// adjacent_write.h: Adjacent Modules' WRITE-BACK (FUN_00404040): the list of the current module's adjacent modules is put into the module's ADJMDLEP chunk. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-autolink-facts.md section 11.
//
// WHAT inteng DOES (FUN_00404040, dump-all-1.txt:2900-3300; called at Exit, at Find Not Done (0x68) and, for the module that WAS current, at Set Current (FUN_004025d0)): when there is a current module, the list
// has been changed (DAT_0060677c) and the module's object has its chunk record, then
//  - an empty list (DAT_0060676c == 0): the module's ADJMDLEP is DELETED (when it has one);
//  - else a NEW ADJMDLEP is made from the list, in the list's order: a module the OLD chunk already has an entry for keeps that entry (its flags, so a hand-set one, and its point), any other module gets an entry with
//    flags 0 and point 0; the old chunk is deleted and the new one goes at the end of the module's MODULEDT (a MODULEDT is made when there is none);
//  - then for every entry of the new chunk, the module it names is found and FUN_00402ae0 works the entry's flags and point out again (the same step as Recalculate Entry Points; an entry whose flags have the
//    hand-set bit keeps its point); when it gives 0 (the mapper stopped it after the "modules don't touch" warning) the rest is not worked out;
//  - the level is written (FUN_004bd8e0, FUN_004719e0).
//
// WHAT THIS DOES: the chunk is made in one new image and read by the game's own loader (object_edit.h SetModuleAdjacencyMany), then the entry points of THAT module's entries are worked out with the plan of
// Recalculate Entry Points (entry_recalc.h) restricted to the module: no other module's entry is touched. NAMED: the level is not written to a file (Save a copy does it); a module with no MODULEDT is not made one
// (a current module is a module); the "modules don't touch" question is not asked (as in entry_recalc.h).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

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
	AdjacentWriteResult AdjacentWriteBack(OpenLevelFile *open, int32_t current, std::vector<int32_t> const &members, AdjacentWriteCounts *counts);
}
