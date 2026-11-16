// See adjacency_generate.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "adjacency_generate.h"

#include <exception>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "level_file.h"
#include "level_chunks.h"
#include "level_store.h"
#include "entry_recalc.h"
#include "autolink_portal.h"
#include "autolink_scene.h"
#include "object_edit.h"

namespace host
{
	namespace
	{
		// FUN_004c5980: the two modules meet and the opening is at least 50000 units wide on both axes that are not the contact axis
		bool OpenEnough(AutoLinkScene const &scene, int32_t a, int32_t b)
		{
			std::map<int32_t, size_t>::const_iterator ia = scene.indexOf.find(a);
			std::map<int32_t, size_t>::const_iterator ib = scene.indexOf.find(b);

			if (ia == scene.indexOf.end() || ib == scene.indexOf.end())
			{
				return false;
			}

			Portal portal;

			if (!AutoLink_ModulesMeet(scene.list[ia->second].module, scene.list[ib->second].module, &portal))
			{
				return false;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				if (axis != portal.axis && (int64_t)portal.hi[axis] - portal.lo[axis] < 50000)
				{
					return false;
				}
			}

			return true;
		}

		struct State
		{
			bool module;
			bool hasAdjacency;
			std::vector<ModuleAdjacencyEntry> adjacency;
			bool hasLinks;
			std::vector<ModuleLinkItem> links;
			bool touched;
		};

		bool HasEntry(std::vector<ModuleAdjacencyEntry> const &list, int32_t index)
		{
			for (ModuleAdjacencyEntry const &entry : list)
			{
				if (entry.objectIndex == index)
				{
					return true;
				}
			}

			return false;
		}
	}

	char const *AdjacencyWords(AdjacencyResult result)
	{
		switch (result)
		{
		case AdjacencyResult::Done: return "done";
		case AdjacencyResult::NothingOpen: return "no level is open";
		case AdjacencyResult::WriteFailed: return "a module's adjacency could not be written";
		}

		return "the adjacent module data could not be made";
	}

	AdjacencyResult Adjacency_Generate(OpenLevelFile *open, bool newOnly, double levelScale, AdjacencyCounts *counts, std::string *why)
	{
		AdjacencyCounts none;
		AdjacencyCounts &total = counts ? *counts : none;

		total.listed = total.noModuleData = total.renewed = total.made = total.noneFound = total.entries = total.reverseAdded = total.keptByHand = total.recalculated = total.written = 0;

		if (!open || !open->Level())
		{
			return AdjacencyResult::NothingOpen;
		}

		try
		{
			std::vector<ModuleSnapshot> snapshots;
			ObjectEditResult const read = ReadModuleSnapshots(open, &snapshots);

			if (read != ObjectEditResult::Done)
			{
				if (why)
				{
					*why = ObjectEditWords(read);
				}

				return AdjacencyResult::WriteFailed;
			}

			AutoLinkScene scene;

			AutoLinkScene_Build(open, levelScale > 0.0 ? levelScale : 1.0, &scene);

			// the state of every object, by its number (the first object with a number is the one the game finds)
			std::map<int32_t, State> state;
			std::vector<int32_t> order;

			for (ModuleSnapshot const &snapshot : snapshots)
			{
				if (state.count(snapshot.index))
				{
					continue;
				}

				State one;

				one.module = snapshot.module;
				one.hasAdjacency = snapshot.hasAdjacency;
				one.adjacency = snapshot.adjacency;
				one.hasLinks = snapshot.hasLinks;
				one.links = snapshot.links;
				one.touched = false;
				state[snapshot.index] = one;
				order.push_back(snapshot.index);
			}

			// the list: every object, or (newOnly) those that do not have both a MODULEDT and an ADJMDLEP
			std::vector<int32_t> list;

			for (int32_t const index : order)
			{
				State const &one = state[index];

				if (newOnly && one.module && one.hasAdjacency)
				{
					continue;
				}

				list.push_back(index);
			}

			total.listed = list.size();

			for (int32_t const a : list)
			{
				State &self = state[a];

				if (!self.module)
				{
					total.noModuleData++;
					continue;
				}

				std::vector<ModuleAdjacencyEntry> keep;

				if (self.hasAdjacency)
				{
					for (ModuleAdjacencyEntry const &entry : self.adjacency)
					{
						if ((entry.flags & 1) != 0)
						{
							keep.push_back(entry);
							total.keptByHand++;
						}
					}

					self.hasAdjacency = false;
					self.adjacency.clear();
					self.touched = true;
					total.renewed++;
				}

				if (!self.hasLinks)
				{
					total.noModuleData++;
					continue;
				}

				for (ModuleLinkItem const &item : self.links)
				{
					int32_t const b = item.objectIndex;
					std::map<int32_t, State>::iterator other = state.find(b);

					if (other == state.end() || !OpenEnough(scene, a, b))
					{
						continue;
					}

					if (!HasEntry(keep, b))
					{
						ModuleAdjacencyEntry entry;

						entry.objectIndex = b;
						entry.flags = 0;
						entry.point[0] = entry.point[1] = entry.point[2] = 0;
						keep.push_back(entry);
					}

					if (other->first != a && other->second.module && other->second.hasAdjacency && !HasEntry(other->second.adjacency, a))
					{
						ModuleAdjacencyEntry entry;

						entry.objectIndex = a;
						entry.flags = 0;
						entry.point[0] = entry.point[1] = entry.point[2] = 0;
						other->second.adjacency.push_back(entry);
						other->second.touched = true;
						total.reverseAdded++;
					}
				}

				if (keep.empty())
				{
					total.noneFound++;
				}
				else
				{
					self.adjacency = keep;
					self.hasAdjacency = true;
					self.touched = true;
					total.made++;
					total.entries += keep.size();
				}
			}

			// the write: every module whose chunk was touched, in one image
			std::vector<ModuleAdjacencyFor> edits;

			for (int32_t const index : order)
			{
				State const &one = state[index];

				if (!one.touched)
				{
					continue;
				}

				ModuleAdjacencyFor edit;

				edit.objectIndex = index;
				edit.remove = !one.hasAdjacency;
				edit.entries = one.adjacency;
				edits.push_back(edit);
			}

			if (!edits.empty())
			{
				std::vector<bool> wrote;
				ObjectEditResult const result = SetModuleAdjacencyMany(open, edits, &wrote);

				if (result != ObjectEditResult::Done)
				{
					if (why)
					{
						*why = ObjectEditWords(result);
					}

					return AdjacencyResult::WriteFailed;
				}

				for (size_t at = 0; at < wrote.size(); at++)
				{
					total.written += wrote[at] ? 1 : 0;
				}
			}

			// FUN_00403d50: every entry's flags and point worked out again (entry_recalc.h)
			LevelStore store(open);
			std::vector<EntryChange> changes;
			EntryRecalcCounts recalc;

			if (PlanEntryRecalc(open->Level(), &store, &changes, &recalc))
			{
				size_t refused = 0;

				ApplyEntryRecalc(&store, changes, &refused);
				total.recalculated = recalc.recalculated;
			}

			return AdjacencyResult::Done;
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return AdjacencyResult::WriteFailed;
		}
	}

	int64_t Adjacency_RecalculateEntryPoints(OpenLevelFile *open)
	{
		if (!open || !open->Level())
		{
			return -1;
		}

		try
		{
			LevelStore store(open);
			std::vector<EntryChange> changes;
			EntryRecalcCounts recalc;

			if (!PlanEntryRecalc(open->Level(), &store, &changes, &recalc))
			{
				return -1;
			}

			size_t refused = 0;

			ApplyEntryRecalc(&store, changes, &refused);
			return (int64_t)recalc.recalculated;
		}
		catch (std::exception const &)
		{
			return -1;
		}
	}
}
