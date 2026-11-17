// shape_merge_edit.h: Merge Objects (inteng command 0x58), the part the shape model can't do: the level's bytes. P11, slice 8. Written
// 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT DOES. It merges the SECOND object's shape into the FIRST object's (model::MergeShapes does the numbers, copied from inteng's
// FUN_00449cf0 / FUN_00448cf0), makes the first shape's chunks again with the new counts, moves the first object's place by the merged shape's
// centre WITHOUT rotation (as inteng does: the card in unfinished-in-inteng.md), and takes the second object out of the level. All of it is one
// new image of the level, put in through OpenLevelFile::ReplaceLevel, which reads it with the game's own loader and swaps it in only if the
// loader takes it: nothing changes on any other answer. After a success every Chunk * the editor held is gone (object_edit.h says the same).
//
// A SUBSET, on purpose (AvP-docs\engine\inteng\p11-merge-host-plan.md): the shapes may have none of ANIMSEQU, TEXTANIM, SHPMRGDT, SHPMORPH or
// SHPPCINF; everything else is refused with a plain message, until what inteng does with those chunks is built (construction-closeout-frost.md TB-C8b..g).
// TEXTURED SHAPES (TB-C8a, Frost 2026-11-17): the merged SHPUVCRD holds the first's lists, then the second's (FUN_00448cf0 0x448fc6-0x449029), the second's textured
// polygons name their list + the first's count (0x448f57, the model's uvListsInFirst); SHPTEXFN is the first's, as inteng keeps the first's record (FUN_0042fc00).
//
// NAMED DEVIATIONS FROM INTENG (each one is security or crash safety, Tao 1; the card is in unfinished-in-inteng.md):
// - The merge is REFUSED when any other object's chunk holds the second object's 8-byte ID (a switch's target, a door, a parent...): inteng
//   removes the object and leaves such references pointing at nothing, which can crash the game. The bytes are searched, as object_edit.h does.
// - A module (an object with a MODULEDT) is never merged, either side: other chunks name modules.
// - The first shape must belong to the first object alone (inteng's handling of other users of the shape is not read).
// - The second object's SHAPE is left in the level (inteng's removal of it, if any, is not read).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ShapeMergeEditResult
	{
		Done,
		BadCall,        // no level, no ID, or no room for an answer
		NoObject,       // an ID names no object
		NoShape,        // an object has no shape the editor can read
		SameObject,     // the two IDs are one object, or the two objects share one shape
		IsAModule,      // an object has a MODULEDT
		SharedShape,    // another object uses the first shape too
		Unsupported,    // a shape has a chunk the subset doesn't take (*why names it)
		ModelRefused,   // the numbers wouldn't merge (*why says why)
		Referenced,     // another object's chunk holds the second object's ID
		CantBuild,      // the level couldn't be put together from its bytes
		LoaderRefused,  // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *ShapeMergeEditWords(ShapeMergeEditResult result);

	// Merges the shape of the object whose 8-byte ID is `second` into the shape of the object `first`, and takes `second` out.
	ShapeMergeEditResult ShapeMergeEdit_Objects(OpenLevelFile *file, uint8_t const *first, uint8_t const *second, std::string *why);
}
