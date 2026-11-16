// asc_level.h: from a level (the game's chunk library) to the numbers of inteng's Write Asc File (asc_export.h): the walk over the objects and their
// shapes. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P7). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The walk is FUN_00506f90's (menu-reads-2.txt:11242-11623): every RBOBJECT of the level in the level's own order; an object's shape is the one it is
// tied to (FUN_004d0fd0: Object_Chunk::get_assoc_shape); a shape whose header (SHPHEAD1) has flag 0x1000 set is left out; with "doors closed"
// (param_3) the shape written is the first SUBSHAPE of the shape's SHPMORPH when it has one. This reads the level and changes nothing.
//
// STILL OPEN (said in the commit and on the board, not guessed): the text of each line (which string each sprintf pushes), the scale, the name
// "Named object" prints for an object (its own name or its shape's), and whether the doors-closed choice takes the FIRST sub-shape as read here
// (the decompile's list code around 0x5071b6-0x5073ff is inlined list handling that hides it).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "asc_export.h"

class File_Chunk;

namespace host
{
	struct AscLevelObject
	{
		std::string objectName;   // the object's name, to its first 0 and within the game's 64 bytes
		std::string shapeName;    // its shape's name, the same way (empty when the shape has none)
		AscObject numbers;        // its vertices and faces (asc_export.h)
	};

	// The objects that are written, in order, into *out (emptied first), and the offset over ALL objects' shapes into *offset. scale is the
	// writer's. Gives the number of objects of the level, 0 for no level. An object with no shape, and one whose shape is flagged out, is not
	// in *out; a count or index that doesn't fit is refused by asc_export.h's rules (nothing is read past a shape).
	size_t AscWalkLevel(File_Chunk *level, bool doorsClosed, double scale, std::vector<AscLevelObject> *out, AscOffset *offset);
}
