// module_poly_count.h: inteng's "how many polygons does this module draw, with every module it sees" (FUN_004d5e60), which Module Linking's Show All (0xca) paints by. Written 2026-11-15 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULE (Vasquez, module-linking-reads.md section 1; nothing in the level changes): count(shape) = the shape's polygon count when it has no SHPMRGDT; else, for each file polygon i with merge[i] < i (a
// polygon that is not merged, or the SECOND half of a merged pair) 1, or 2 when the polygon's flag word has bit 31 (0x80000000). poly_count(module) = count(its shape) + count of the shape of every other module
// that the FIRST VMDARRAY of its first MODULEDT lists (each item's object_index matched to the first not yet counted object with that index_num; no object with that index adds nothing), each counted once.
// A module with no MODULEDT or no VMDARRAY has no figure (Show All paints it white).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	// The figure for the module whose object has this 8-byte ID. *hasFigure false (count 0) for a module with no MODULEDT or no VMDARRAY. False when there is no such object or its shape can't be read.
	bool ModulePolyCount_Of(OpenLevelFile const *store, File_Chunk *level, uint8_t const *objectId, int32_t *count, bool *hasFigure);

	// The colour Show All paints for a figure: green below 1200, blue 1200 to 1499, orange 1500 to 1999, red 2000 and up (ARGB); white (0xFFFFFFFF) for none
	uint32_t ModulePolyCount_Colour(bool hasFigure, int32_t count);

	// count of one shape by the merge rule above, from the shape's flag words and its merge data (null: no SHPMRGDT). Pure.
	int32_t ModulePolyCount_OfShape(size_t polygons, int32_t const *polygonFlags, int32_t const *mergeData, size_t mergeCount);
}
