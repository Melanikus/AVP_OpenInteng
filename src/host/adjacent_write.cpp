// See adjacent_write.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "adjacent_write.h"

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

	AdjacentWriteResult AdjacentWriteBack(OpenLevelFile *open, int32_t current, std::vector<int32_t> const &members, AdjacentWriteCounts *counts)
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

		if (edit.remove)
		{
			return AdjacentWriteResult::Done;
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
}
