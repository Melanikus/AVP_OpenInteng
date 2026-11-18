// adjacent_write.h: Adjacent Modules' WRITE-BACK (FUN_00404040): the list of the current module's adjacent modules is put into the module's ADJMDLEP chunk. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-autolink-facts.md section 11.
//
// WHAT inteng DOES (FUN_00404040, dump-all-1.txt:2900-3300; called at Exit, at Find Not Done (0x68) and, for the module that WAS current, at Set Current (FUN_004025d0)): when there is a current module, the list
// has been changed (DAT_0060677c) and the module's object has its chunk record, then
//  - an empty list (DAT_0060676c == 0): the module's ADJMDLEP is DELETED (when it has one);
//  - else a NEW ADJMDLEP is made from the list, in the list's order: a module the OLD chunk already has an entry for keeps that entry (its flags, so a hand-set one, and its point), any other module gets an entry with
//    flags 0 and point 0; the old chunk is deleted and the new one goes at the end of the module's MODULEDT (a MODULEDT is made when there is none);
//  - then for every entry of the new chunk, the module it names is found and FUN_00402ae0 works the entry's flags and point out again (the same step as Recalculate Entry Points; an entry whose flags have the
//    hand-set bit keeps its point; an entry naming no object is deleted without a call, 0x404393). FUN_00402ae0 asks "Warning adjacent modules don't touch" for a pair that doesn't touch; a Yes gives 0, which
//    sets a flag (EBP, 0x404465) cleared only ONCE before the loop (0x404366), so that entry AND EVERY LATER ONE is deleted (FUN_0044d280, 0x40448f-0x4044a6), the later ones still worked out and asked;
//  - on both the empty and the non-empty path: FUN_004bd8e0(obj, file, 1) (marks the chunk updated; writes nothing) and the level FILE is written (FUN_004719e0(DAT_0060801c, 5) at 0x404511).
//    (Corrected 2026-11-17 by Melanikus (Dietrich) from the ad6-verify workflow, reassess/adjacent-closeout-dietrich.md AD-6: the rest IS worked out, then taken out.)
//
// WHAT THIS DOES: the chunk is made in one new image and read by the game's own loader (object_edit.h SetModuleAdjacencyMany), then the entry points of THAT module's entries are worked out with the plan of
// Recalculate Entry Points (entry_recalc.h) restricted to the module: no other module's entry is touched. A module with no MODULEDT is not made one (a current module is a module). With
// workOutEntryPoints false the entry points are left to the caller: the editor's StartEntryRecalc(..., current) asks the questions and takes the Yes entries out (AD-6), and the editor saves
// (0x404511). INTENG BUG, FIXED (NAMED): only the entry answered Yes goes, not every later one of the list (the flag is cleared once per run in inteng, once per entry in FUN_00403d50 0x403e34).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "entry_recalc.h"   // EntryDelete

class OpenLevelFile;

namespace host
{
	enum class AdjacentWriteResult
	{
		Done,
		NothingOpen,
		NoSuchModule,     // the current module is not a module of the level
		WriteFailed,      // the game's loader would not take the new chunk, so nothing was changed
	};

	struct AdjacentWriteCounts
	{
		AdjacentWriteCounts() : entries(0), kept(0), made(0), recalculated(0), removed(false) {}

		size_t entries;        // entries in the new chunk
		size_t kept;           // of them taken over from the old chunk
		size_t made;           // new ones (flags 0, point 0 before they are worked out)
		size_t recalculated;   // entries whose flags or point came out different
		bool removed;          // the list was empty: the chunk was taken out
	};

	char const *AdjacentWriteWords(AdjacentWriteResult result);

	// Writes the list of module `current` (its object index) as inteng's write-back does. members are the object indexes of the adjacent modules in the list's order.
	AdjacentWriteResult AdjacentWriteBack(OpenLevelFile *open, int32_t current, std::vector<int32_t> const &members, AdjacentWriteCounts *counts, bool workOutEntryPoints = true);

	// RECALCULATE ENTRY POINTS' DELETIONS (AD-2 / AD-5 of reassess/adjacent-closeout-dietrich.md): each module's ADJMDLEP again without the entries given (an entry
	// naming no object; a Yes to "Warning adjacent modules don't touch ... delete the adjacency?"), every module in ONE new image read by the game's loader. An entry
	// goes only when the chunk still has that entry number naming that module. inteng takes the node out of the chunk's list where it is (FUN_00429820); ours makes the
	// shortened chunk again at the end of its MODULEDT (object_edit.h SetModuleAdjacencyMany), as inteng's own write of a list does (FUN_00404040): NAMED, the game reads
	// both the same. A module whose list is left empty keeps an empty ADJMDLEP, as inteng's would. *deleted counts the entries taken out.
	AdjacentWriteResult DeleteAdjacencyEntries(OpenLevelFile *open, std::vector<EntryDelete> const &deletes, size_t *deleted);
}
