// adjacency_generate.h: Module Linking's Mode > Exit + New Adjacencies (0x66) and Exit + All Adjacencies (0x65): inteng's FUN_004c5a30 on the open level. Written 2026-11-15 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Facts: AvP-docs\engine\inteng\drake-autolink-facts.md section 9.
//
// WHAT inteng DOES (FUN_004c5a30(newOnly), listing-all-2.txt:113288-113790, dump-all-2.txt:61984): the list is every object of the level (RBOBJECT), in the file's order; with newOnly (0x66) the
// objects whose MODULEDT already has an ADJMDLEP are taken out of it. For each object left:
//  - no MODULEDT: the message "Object '%s' has no VMODULE data" and on to the next;
//  - an ADJMDLEP it already has ("Object '%s' already has data, renewing"): the entries whose flags have bit 0 (EntryPointSetByHand) are kept in a list, and the chunk is DELETED;
//  - no VMDARRAY (its links): the same message, and on to the next (the old ADJMDLEP is gone);
//  - for each item of the VMDARRAY (every item, branches too), the module it names: when the two modules' boxes MEET (FUN_004c5980 = FUN_004c4750, autolink_portal.h) and the opening is
//    at least 50000 units wide on both axes that are not the contact axis, the neighbour is put in the kept list as a new entry (flags 0, point 0) unless the list holds it, and, when the
//    neighbour has an ADJMDLEP now, this module is put in THAT chunk's list too (flags 0) unless it is there;
//  - the kept list becomes a NEW ADJMDLEP chunk at the end of the module's MODULEDT; an empty list gives "Warning! Object '%s' found no adjacent modules from VMODULE data" and no chunk.
// After the last object FUN_00403d50 (Recalculate Entry Points, entry_recalc.h) works out every entry's flags and point.
//
// WHAT THIS DOES: the same, on the image's bytes (object_edit.h ReadModuleSnapshots, SetModuleAdjacencyMany: one new image, one loader read), then the same entry points step. NAMED: (1) a module
// whose old chunk had hand-set entries but which has no VMDARRAY loses its chunk, as in inteng, but the kept list does not carry over to the next module (inteng's list is not emptied there, so
// the next module would get another module's hand-set entries: a bug not copied); (2) the meet test is OUR port of FUN_004c4750 (93.9 to 99.3% of the stock levels' adjacent pairs meet, drake-autolink-
// facts.md section 7), so a pair inteng's call would find may be missed; (3) the objects are taken in the order the level's objects are written; (4) the messages inteng prints are counted; (5) "All" asks
// first in inteng ("Do you want to regenerate adjacent module data? This will mess up any one-way adjacencies that have been set up."), here a typed yes after the screen was left.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class OpenLevelFile;

namespace host
{
	enum class AdjacencyResult
	{
		Done,
		NothingOpen,
		WriteFailed,
	};

	struct AdjacencyCounts
	{
		size_t listed;          // objects on the list (all, or only those with no ADJMDLEP)
		size_t noModuleData;    // "has no VMODULE data": no MODULEDT, or no VMDARRAY
		size_t renewed;         // an old ADJMDLEP was deleted
		size_t made;            // modules given a new ADJMDLEP
		size_t noneFound;       // modules whose links gave no adjacent module
		size_t entries;         // entries in the new chunks
		size_t reverseAdded;    // entries put into a neighbour's chunk
		size_t keptByHand;      // entries kept because they were set by hand
		size_t recalculated;    // entry points worked out afterwards
		size_t written;         // modules whose chunk was written (put in or taken out)
	};

	char const *AdjacencyWords(AdjacencyResult result);

	// newOnly: Exit + New Adjacencies (only the modules with no ADJMDLEP); false: Exit + All Adjacencies. levelScale: the level's scale (ENVSDSCL).
	AdjacencyResult Adjacency_Generate(OpenLevelFile *open, bool newOnly, double levelScale, AdjacencyCounts *counts, std::string *why);

	// Recalculate Entry Points over every module (FUN_00403d50; entry_recalc.h): gives how many entry points were worked out, or -1 when the level could not be read. The level's bytes change only for entries that come out different.
	int64_t Adjacency_RecalculateEntryPoints(OpenLevelFile *open);
}
