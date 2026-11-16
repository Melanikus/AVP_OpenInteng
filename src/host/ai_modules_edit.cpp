// See ai_modules_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "ai_modules_edit.h"

#include <string.h>

#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "level_file.h"
#include "object_edit.h"
#include "waypoint_model.h"

namespace host
{
	namespace
	{
		ModuleChildEdit RemoveEdit(int32_t index, char const *name)
		{
			ModuleChildEdit edit;

			edit.objectIndex = index;
			memset(edit.name, 0, sizeof(edit.name));
			memcpy(edit.name, name, 8);
			edit.remove = true;
			return edit;
		}

		ModuleChildEdit PutEdit(int32_t index, char const *name, std::vector<uint8_t> const &chunk)
		{
			ModuleChildEdit edit = RemoveEdit(index, name);

			edit.remove = false;
			edit.chunk = chunk;
			return edit;
		}

		// AIMODSLA: its heading (16 bytes in all) and the master's object number; AIMODMAS: its heading only (12 bytes)
		std::vector<uint8_t> SlaveChunk(int32_t master)
		{
			std::vector<uint8_t> chunk(16, 0);
			int32_t const size = 16;

			memcpy(chunk.data(), "AIMODSLA", 8);
			memcpy(chunk.data() + 8, &size, 4);
			memcpy(chunk.data() + 12, &master, 4);
			return chunk;
		}

		std::vector<uint8_t> MasterChunk()
		{
			std::vector<uint8_t> chunk(12, 0);
			int32_t const size = 12;

			memcpy(chunk.data(), "AIMODMAS", 8);
			memcpy(chunk.data() + 8, &size, 4);
			return chunk;
		}

		AiModulesResult Write(OpenLevelFile *open, std::vector<ModuleChildEdit> const &edits, AiModulesCounts *counts, std::string *why)
		{
			std::vector<bool> wrote;
			ObjectEditResult const result = SetModuleChildrenMany(open, edits, &wrote);

			if (result != ObjectEditResult::Done)
			{
				if (why)
				{
					*why = ObjectEditWords(result);
				}

				return AiModulesResult::WriteFailed;
			}

			for (size_t i = 0; i < wrote.size(); i++)
			{
				counts->chunksChanged += wrote[i] ? 1 : 0;
			}

			return AiModulesResult::Done;
		}
	}

	char const *AiModulesWords(AiModulesResult result)
	{
		switch (result)
		{
		case AiModulesResult::Done: return "done";
		case AiModulesResult::NothingOpen: return "no level is open";
		case AiModulesResult::NotAModule: return "that is not a module of the level";
		case AiModulesResult::SameModule: return "the module is the master itself";
		case AiModulesResult::AlreadyThere: return "the module is already in that master's list";
		case AiModulesResult::MasterIsASlave: return "that module is a slave: pick its master";
		case AiModulesResult::NotASlave: return "the module is not in any master's list";
		case AiModulesResult::BadWaypoints: return "a waypoint chunk could not be read";
		case AiModulesResult::WriteFailed: return "the chunks could not be written";
		}

		return "the AI module could not be changed";
	}

	AiModulesResult AiModules_Add(OpenLevelFile *open, int32_t masterIndex, int32_t moduleIndex, AiModulesCounts *counts, std::string *why)
	{
		AiModulesCounts none;
		AiModulesCounts &total = counts ? *counts : none;

		total.slavesMoved = total.waypointsMoved = total.chunksChanged = 0;

		if (!open || !open->Level())
		{
			return AiModulesResult::NothingOpen;
		}

		try
		{
			std::vector<ModuleSnapshot> snapshots;

			if (ReadModuleSnapshots(open, &snapshots) != ObjectEditResult::Done)
			{
				if (why)
				{
					*why = "the level could not be read";
				}

				return AiModulesResult::WriteFailed;
			}

			std::map<int32_t, size_t> at;

			for (size_t i = 0; i < snapshots.size(); i++)
			{
				if (snapshots[i].module && !at.count(snapshots[i].index))
				{
					at[snapshots[i].index] = i;
				}
			}

			if (!at.count(masterIndex) || !at.count(moduleIndex))
			{
				return AiModulesResult::NotAModule;
			}

			if (masterIndex == moduleIndex)
			{
				return AiModulesResult::SameModule;
			}

			ModuleSnapshot const &master = snapshots[at[masterIndex]];
			ModuleSnapshot const &module = snapshots[at[moduleIndex]];

			if (master.masterIndex >= 0)
			{
				return AiModulesResult::MasterIsASlave;
			}

			if (module.masterIndex == masterIndex)
			{
				return AiModulesResult::AlreadyThere;
			}

			std::vector<ModuleChildEdit> edits;

			if (!master.isMaster)
			{
				edits.push_back(PutEdit(masterIndex, "AIMODMAS", MasterChunk()));
			}

			// x is a master: every module of its list joins the new master (unless it is there already or is the master), then x's AIMODMAS goes
			if (module.isMaster)
			{
				for (ModuleSnapshot const &other : snapshots)
				{
					if (other.module && other.masterIndex == moduleIndex && other.index != masterIndex && other.index != moduleIndex)
					{
						edits.push_back(PutEdit(other.index, "AIMODSLA", SlaveChunk(masterIndex)));
						total.slavesMoved++;
					}
				}

				edits.push_back(RemoveEdit(moduleIndex, "AIMODMAS"));
			}

			edits.push_back(PutEdit(moduleIndex, "AIMODSLA", SlaveChunk(masterIndex)));

			// the waypoints: x's chunk is merged into the master's (made when it has none); x's chunk goes when it had waypoints
			if (module.hasWaypoint)
			{
				WaypointSet from, into;
				std::string reason;

				if (!Waypoints_Parse(module.waypoint.data(), module.waypoint.size(), &from, &reason))
				{
					if (why)
					{
						*why = "the module's waypoints: " + reason;
					}

					return AiModulesResult::BadWaypoints;
				}

				if (master.hasWaypoint && !Waypoints_Parse(master.waypoint.data(), master.waypoint.size(), &into, &reason))
				{
					if (why)
					{
						*why = "the master's waypoints: " + reason;
					}

					return AiModulesResult::BadWaypoints;
				}

				total.waypointsMoved = from.alien.size() + from.ground.size();

				if (Waypoints_Transfer(&into, from))
				{
					edits.push_back(PutEdit(masterIndex, "WAYPOINT", Waypoints_Serialize(into)));
					edits.push_back(RemoveEdit(moduleIndex, "WAYPOINT"));
				}
				else if (!master.hasWaypoint)
				{
					edits.push_back(PutEdit(masterIndex, "WAYPOINT", Waypoints_Serialize(into)));   // the master's new, empty chunk (the game's tools make it before the transfer)
				}
			}

			return Write(open, edits, &total, why);
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return AiModulesResult::WriteFailed;
		}
	}

	AiModulesResult AiModules_Remove(OpenLevelFile *open, int32_t moduleIndex, AiModulesCounts *counts, std::string *why)
	{
		AiModulesCounts none;
		AiModulesCounts &total = counts ? *counts : none;

		total.slavesMoved = total.waypointsMoved = total.chunksChanged = 0;

		if (!open || !open->Level())
		{
			return AiModulesResult::NothingOpen;
		}

		try
		{
			std::vector<ModuleSnapshot> snapshots;

			if (ReadModuleSnapshots(open, &snapshots) != ObjectEditResult::Done)
			{
				return AiModulesResult::WriteFailed;
			}

			for (ModuleSnapshot const &snapshot : snapshots)
			{
				if (snapshot.module && snapshot.index == moduleIndex)
				{
					if (snapshot.masterIndex < 0)
					{
						return AiModulesResult::NotASlave;
					}

					std::vector<ModuleChildEdit> edits;

					edits.push_back(RemoveEdit(moduleIndex, "AIMODSLA"));
					return Write(open, edits, &total, why);
				}
			}

			return AiModulesResult::NotAModule;
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return AiModulesResult::WriteFailed;
		}
	}

	namespace
	{
		// The module and every module of its AI module: its master and the master's slaves (FUN_0058a530)
		std::set<int32_t> GroupOf(std::map<int32_t, ModuleSnapshot> const &modules, int32_t index)
		{
			std::set<int32_t> group;
			std::map<int32_t, ModuleSnapshot>::const_iterator found = modules.find(index);

			if (found == modules.end())
			{
				return group;
			}

			int32_t const master = found->second.masterIndex >= 0 ? found->second.masterIndex : index;

			group.insert(master);

			for (std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.begin(); it != modules.end(); ++it)
			{
				if (it->second.masterIndex == master)
				{
					group.insert(it->first);
				}
			}

			return group;
		}

		// An ADJMDLEP chunk of these entries, an empty list too (inteng takes entries out of the list and leaves the chunk)
		std::vector<uint8_t> AdjacencyChunk(std::vector<ModuleAdjacencyEntry> const &entries)
		{
			std::vector<uint8_t> chunk(16 + 20 * entries.size(), 0);
			int32_t const size = (int32_t)chunk.size();
			int32_t const count = (int32_t)entries.size();

			memcpy(chunk.data(), "ADJMDLEP", 8);
			memcpy(chunk.data() + 8, &size, 4);
			memcpy(chunk.data() + 12, &count, 4);

			for (size_t i = 0; i < entries.size(); i++)
			{
				memcpy(chunk.data() + 16 + 20 * i, &entries[i].objectIndex, 4);
				memcpy(chunk.data() + 20 + 20 * i, &entries[i].flags, 4);
				memcpy(chunk.data() + 24 + 20 * i, entries[i].point, 12);
			}

			return chunk;
		}
	}

	AiModulesResult AiModules_DeleteEntryPoints(OpenLevelFile *open, int32_t a, int32_t b, size_t *removed, std::string *why)
	{
		if (removed)
		{
			*removed = 0;
		}

		if (!open || !open->Level())
		{
			return AiModulesResult::NothingOpen;
		}

		try
		{
			std::vector<ModuleSnapshot> all;

			if (ReadModuleSnapshots(open, &all) != ObjectEditResult::Done)
			{
				return AiModulesResult::WriteFailed;
			}

			std::map<int32_t, ModuleSnapshot> modules;

			for (ModuleSnapshot const &snapshot : all)
			{
				if (snapshot.module && !modules.count(snapshot.index))
				{
					modules[snapshot.index] = snapshot;
				}
			}

			if (!modules.count(a) || !modules.count(b))
			{
				return AiModulesResult::NotAModule;
			}

			std::set<int32_t> const sideA = GroupOf(modules, a);
			std::set<int32_t> const sideB = GroupOf(modules, b);
			std::vector<ModuleChildEdit> edits;
			size_t gone = 0;

			for (std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.begin(); it != modules.end(); ++it)
			{
				bool const inA = sideA.count(it->first) != 0;
				bool const inB = sideB.count(it->first) != 0;

				if ((!inA && !inB) || !it->second.hasAdjacency)
				{
					continue;
				}

				std::vector<ModuleAdjacencyEntry> kept;

				for (ModuleAdjacencyEntry const &entry : it->second.adjacency)
				{
					bool const goes = (inA && sideB.count(entry.objectIndex)) || (inB && sideA.count(entry.objectIndex));

					if (goes)
					{
						gone++;
					}
					else
					{
						kept.push_back(entry);
					}
				}

				if (kept.size() != it->second.adjacency.size())
				{
					edits.push_back(PutEdit(it->first, "ADJMDLEP", AdjacencyChunk(kept)));
				}
			}

			if (edits.empty())
			{
				return AiModulesResult::Done;
			}

			AiModulesCounts counts;
			AiModulesResult const result = Write(open, edits, &counts, why);

			if (result == AiModulesResult::Done && removed)
			{
				*removed = gone;
			}

			return result;
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return AiModulesResult::WriteFailed;
		}
	}
}
