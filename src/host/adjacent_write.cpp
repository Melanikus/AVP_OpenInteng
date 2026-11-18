// See adjacent_write.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "adjacent_write.h"

#include <map>
#include <set>

#include "entry_recalc.h"
#include "level_file.h"
#include "level_objects.h"
#include "level_store.h"
#include "object_edit.h"

namespace host
{
	char const *AdjacentWriteWords(AdjacentWriteResult result)
	{
		switch (result)
		{
		case AdjacentWriteResult::Done: return "done";
		case AdjacentWriteResult::NothingOpen: return "no level is open";
		case AdjacentWriteResult::NoSuchModule: return "the current module is not a module of the level";
		case AdjacentWriteResult::WriteFailed: return "the game would not take the adjacent modules that way, so nothing was changed";
		}

		return "the adjacent modules could not be written";
	}

	AdjacentWriteResult AdjacentWriteBack(OpenLevelFile *open, int32_t current, std::vector<int32_t> const &members, AdjacentWriteCounts *counts, bool workOutEntryPoints)
	{
		AdjacentWriteCounts none;

		if (!counts)
		{
			counts = &none;
		}

		*counts = AdjacentWriteCounts();

		if (!open || !open->Level())
		{
			return AdjacentWriteResult::NothingOpen;
		}

		std::vector<ModuleSnapshot> snapshots;

		if (ReadModuleSnapshots(open, &snapshots) != ObjectEditResult::Done)
		{
			return AdjacentWriteResult::WriteFailed;
		}

		ModuleSnapshot const *mine = nullptr;

		for (ModuleSnapshot const &one : snapshots)
		{
			if (one.index == current && one.module)
			{
				mine = &one;
				break;
			}
		}

		if (!mine)
		{
			return AdjacentWriteResult::NoSuchModule;
		}

		ModuleAdjacencyFor edit;

		edit.objectIndex = current;
		edit.remove = members.empty();

		if (!edit.remove)
		{
			for (int32_t const member : members)
			{
				ModuleAdjacencyEntry entry = { member, 0, { 0, 0, 0 } };
				bool old = false;

				if (mine->hasAdjacency)
				{
					for (ModuleAdjacencyEntry const &before : mine->adjacency)
					{
						if (before.objectIndex == member)
						{
							entry = before;   // an entry the old chunk has is taken over as it is (its flags, so a hand-set one, and its point)
							old = true;
							break;
						}
					}
				}

				counts->kept += old ? 1 : 0;
				counts->made += old ? 0 : 1;
				edit.entries.push_back(entry);
			}

			counts->entries = edit.entries.size();
		}
		else
		{
			counts->removed = mine->hasAdjacency;

			if (!mine->hasAdjacency)
			{
				return AdjacentWriteResult::Done;   // nothing to take out
			}
		}

		std::vector<ModuleAdjacencyFor> edits(1, edit);
		std::vector<bool> written;

		if (SetModuleAdjacencyMany(open, edits, &written) != ObjectEditResult::Done || written.empty() || !written[0])
		{
			return AdjacentWriteResult::WriteFailed;
		}

		if (edit.remove || !workOutEntryPoints)
		{
			return AdjacentWriteResult::Done;   // (!workOutEntryPoints: the caller works them out, with the questions)
		}

		// the entries of THIS module worked out again (FUN_00402ae0 for each): the plan of Recalculate Entry Points, kept to the module's own
		LevelStore const store(open);
		std::vector<EntryChange> changes;
		EntryRecalcCounts recalcCounts = {};

		if (PlanEntryRecalc(open->Level(), &store, &changes, &recalcCounts))
		{
			std::vector<EntryChange> mineOnly;

			for (EntryChange const &change : changes)
			{
				if (change.mainIndex == current)
				{
					mineOnly.push_back(change);
				}
			}

			LevelStore apply(open);
			size_t refused = 0;

			counts->recalculated = ApplyEntryRecalc(&apply, mineOnly, &refused);
		}

		return AdjacentWriteResult::Done;
	}
	AdjacentWriteResult DeleteAdjacencyEntries(OpenLevelFile *open, std::vector<EntryDelete> const &deletes, size_t *deleted)
	{
		if (deleted)
		{
			*deleted = 0;
		}

		if (!open || !open->Level())
		{
			return AdjacentWriteResult::NothingOpen;
		}

		if (deletes.empty())
		{
			return AdjacentWriteResult::Done;
		}

		// what goes, module by module: its chunk, and each entry number with the module it must name
		std::map<int32_t, std::pair<Chunk *, std::map<size_t, int32_t>>> byModule;

		for (EntryDelete const &drop : deletes)
		{
			std::pair<Chunk *, std::map<size_t, int32_t>> &one = byModule[drop.mainIndex];

			one.first = drop.adjacency;
			one.second[drop.entry] = drop.adjacentIndex;
		}

		LevelStore const store(open);
		std::vector<ModuleAdjacencyFor> edits;
		std::vector<size_t> goneOf;

		for (auto const &module : byModule)
		{
			std::vector<unsigned char> bytes;

			// the entries as the store holds them now (after the recalculated ones were put in): a count, then 20 bytes each
			if (!module.second.first || !store.PayloadOf(module.second.first, &bytes) || bytes.size() < 4)
			{
				continue;
			}

			int32_t const stated = (int32_t)((uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24));

			if (stated < 0 || bytes.size() != 4 + (size_t)stated * 20)
			{
				continue;   // not a whole ADJMDLEP: left as it is
			}

			ModuleAdjacencyFor edit;
			size_t gone = 0;

			edit.objectIndex = module.first;
			edit.remove = false;

			for (size_t entry = 0; entry < (size_t)stated; entry++)
			{
				int32_t numbers[5];

				for (int k = 0; k < 5; k++)
				{
					size_t const at = 4 + entry * 20 + (size_t)k * 4;

					numbers[k] = (int32_t)((uint32_t)bytes[at] | ((uint32_t)bytes[at + 1] << 8) | ((uint32_t)bytes[at + 2] << 16) | ((uint32_t)bytes[at + 3] << 24));
				}

				auto const drop = module.second.second.find(entry);

				if (drop != module.second.second.end() && drop->second == numbers[0])
				{
					gone++;
					continue;
				}

				ModuleAdjacencyEntry kept = { numbers[0], numbers[1], { numbers[2], numbers[3], numbers[4] } };

				edit.entries.push_back(kept);
			}

			if (gone > 0)
			{
				edits.push_back(edit);
				goneOf.push_back(gone);
			}
		}

		if (edits.empty())
		{
			return AdjacentWriteResult::Done;
		}

		std::vector<bool> written;

		if (SetModuleAdjacencyMany(open, edits, &written) != ObjectEditResult::Done)
		{
			return AdjacentWriteResult::WriteFailed;
		}

		for (size_t i = 0; i < edits.size() && i < written.size(); i++)
		{
			if (written[i] && deleted)
			{
				*deleted += goneOf[i];
			}
		}

		return AdjacentWriteResult::Done;
	}
}

