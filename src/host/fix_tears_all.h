// fix_tears_all.h: Construction > Miscellaneous > Fix tears between Objects (All) (0x5b, FUN_0044b1f0): every pair of the level's non-placed objects welded by the single-pair weld of Fix tears (shape_edit.h
// ShapeEdit_FixTears). Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (p11-shape-handlers.md sections 4, 8, 47, 48; FUN_0044b1f0 dump-all-1.txt): it lists the level's objects, takes out the placed ones (flag 0x400), shows "Fixing tears" / "Right button to
// cancel" with a percentage, and for the pairs it finds welds one shape against the other with the same weld as the single-pair command (a shape used by two objects is given its own copy first) and rebuilds.
// NAMED: the order of inteng's pairs is not read (its nested loops are not summarised). Here every ordered pair (moved = the earlier object, reference = the other; then the other way round) whose
// world boxes, each grown by the tolerance, overlap is welded in the file's order; a weld only ever moves a point onto a point within the tolerance, so a pair whose boxes do not overlap can change
// nothing and is left out (that is only a short cut). A moved shape that other objects use, or a morph, is refused by ShapeEdit_FixTears (the card in unfinished-in-inteng.md) and counted.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	struct FixTearsAllCounts
	{
		size_t objects = 0;       // non-placed objects with a shape
		size_t pairs = 0;         // pairs welded (boxes overlap)
		size_t welded = 0;        // points moved in all
		size_t refused = 0;       // pairs ShapeEdit_FixTears refused
		std::string firstWhy;     // the first refusal's words
	};

	// Welds every pair. false only for no level
	bool FixTearsAll(OpenLevelFile *store, File_Chunk *level, int32_t tolerance, FixTearsAllCounts *counts);
}
