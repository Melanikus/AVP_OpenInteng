// entry_recalc.h: inteng's Recalculate Entry Points (Adjacent Modules, Misc, command 0x47) on the open level: every entry of every module's ADJMDLEP is
// worked out again with entry_points.h (FUN_00402ae0). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// SOURCE: FUN_00403d50 (re-tools\projects\vasquez-inteng\out\menu-reads-2.txt:60-194). As read: for each object of the level (RBOBJECT) that has a
// MODULEDT with an ADJMDLEP, for each entry in it, the object whose number (object +0x98) is the entry's object number (+0x10) is looked up, and
// FUN_00402ae0 works out the entry again from the pair; the entry is replaced by the new flags and point. FUN_00402ae0 gives 0 (the mapper stopped it
// after the "modules don't touch" warning) and the run stops there.
//
// AI GROUPS: a module stands for a list of modules (FUN_004038a0: a slave for its master and all its slaves, a master for itself and its slaves),
// and the two lists of a pair are merged through the modules' ADJMDLEP (FUN_004039d0), as Vasquez read them (runs 31, 33, 36; that the merge
// writes back into the first list is inferred from the two copy loops, not shown by an instruction).
// A pair whose two modules are in one group (the main's list holds the adjacent module) is worked on as the two modules alone (FUN_00402ae0 at
// 0x402c46, menu-reads-4.txt:3647-3727).
// CHECKED AGAINST THE STOCK LEVELS (63 levels, 31,083 entries, groups included): 28,498 come out as stored (91.7%); without the four old levels
// (AvP-docs\director\plans\drake-entry-points-misses.md) 27,989 of 28,740 (97.4%), and of the 7,136 entries with a group 6,634 (93.0%).
// ONE NAMED GAP: the "modules don't touch" question (the mapper's answer stops the run): here the run goes on and the pair is counted
// (notTouching).
//
// THE EDIT. The plan (PlanEntryRecalc) changes nothing. Applying it (ApplyEntryRecalc) puts the new flags and point into each ADJMDLEP's bytes
// through the edit store (the chunk keeps its size), so a save writes them, and nothing of the game folder is touched. The bytes the store holds
// now are what is read as the entry's old flags and point (so a second run after the first sees the first's result); the geometry (places,
// shapes) is the level as loaded.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class File_Chunk;
class Chunk;

namespace host
{
	class PayloadStore;

	// One entry whose flags or point come out different
	struct EntryChange
	{
		Chunk *adjacency;        // the module's ADJMDLEP chunk
		int32_t mainIndex;       // the module (its object number)
		int32_t adjacentIndex;   // the entry's module (its object number)
		size_t entry;            // the entry's number in the chunk (from 0)
		uint32_t flagsBefore;
		uint32_t flagsAfter;
		int32_t pointBefore[3];
		int32_t pointAfter[3];
		bool notTouching;        // the modules don't touch (see the gaps)
		bool failedToPlace;      // inteng's "Failed to placed entrypoint" message
	};

	struct EntryRecalcCounts
	{
		size_t entries;          // all entries of all modules
		size_t recalculated;     // entries worked out again
		size_t inGroups;         // of the entries worked out or not, those where a module of the pair stands for a group
		size_t skippedNoOverlap; // after the merge a list is empty (inteng's assertion): not worked out
		size_t skippedNoObject;  // no object with the entry's number, or a module without a shape
		size_t unchanged;        // worked out again and the same
		size_t notTouching;
		size_t failedToPlace;
		size_t outOfRange;       // the new point is beyond 2^24 from the middle: the game's loader would refuse it, so it is not written
	};

	// Works out what Recalculate Entry Points would change. false when level is null. Nothing is changed. Hostile input: every size is checked,
	// a chunk that is not a whole ADJMDLEP is skipped.
	bool PlanEntryRecalc(File_Chunk *level, PayloadStore const *store, std::vector<EntryChange> *changes, EntryRecalcCounts *counts);

	// Puts the changes into the ADJMDLEP bytes (the store keeps their size). An entry is only written when the chunk still has that entry number
	// with that module number. Gives how many entries were written, and in *refused how many were not (null: not asked).
	size_t ApplyEntryRecalc(PayloadStore *store, std::vector<EntryChange> const &changes, size_t *refused);

	// The object numbers a module's ADJMDLEP entries name, in the entry order, as FUN_004045b0 walks them (reads2-apone-4.txt:3-49), into *out
	// (emptied first); a number that names no object of the level is left out (FUN_004045b0 puts a module on its list only when one has that
	// number). The module is given by its object number (ChunkObject index_num). False when there is no level or store, the module has no
	// MODULEDT, or its ADJMDLEP is not a whole chunk of items; *out is then empty. Changes nothing.
	bool AdjacentModulesOf(File_Chunk *level, PayloadStore const *store, int32_t moduleNumber, std::vector<int32_t> *out);
}
