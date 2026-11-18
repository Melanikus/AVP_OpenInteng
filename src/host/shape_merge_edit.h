// shape_merge_edit.h: Merge Objects (inteng command 0x58), the part the shape model can't do: the level's bytes. P11, slice 8. Written
// 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT DOES. It merges the SECOND object's shape into the FIRST object's (model::MergeShapes does the numbers, copied from inteng's
// FUN_00449cf0 / FUN_00448cf0), makes the first shape's chunks again with the new counts, moves the first object's place by the merged shape's
// centre, and takes the second object out of the level. THE TURNS (TB-C8h, Frost 2026-11-17): before that, each object's turn is BAKED into its shape, as
// inteng's FUN_00449970 does (0x449f07 / 0x449f0d; model::TurnShapeByTurn), and the first object's turn becomes (0, 0, 0, 1): the merge itself only moves
// points, so both shapes are unturned by then (the old card in unfinished-in-inteng.md read FUN_00449970 as a list walk). All of it is one
// new image of the level, put in through OpenLevelFile::ReplaceLevel, which reads it with the game's own loader and swaps it in only if the
// loader takes it: nothing changes on any other answer. After a success every Chunk * the editor held is gone (object_edit.h says the same).
//
// REFUSED: a shape with ANIMSEQU, CONSHAPE or SHPMORPH, with words saying why. NAMED DIFFERENCE (Bishop's ruling 2026-11-17, TB-C8e / C8f): inteng merges ANIMSEQU
// (FUN_004497a0 / 004495f0, which corrupts: the frames' points are not moved, an unmatched first sequence is deleted) and CONSHAPE (FUN_0050fd40 / 00519a80 / 00519cc0);
// no placed object of six stock levels has either, and the owner can reopen it. SHPMORPH: inteng refuses it too (TB-C8i).
// THE ANIMATION (TB-C8d, Frost 2026-11-17): as inteng's FUN_004492f0 (called at 0x44a14e), only when the second shape has TEXTANIM records: the first's records,
// then each of the second's with its polygon plus the first's polygon count (0x449524-0x44952a), the groups unchanged; a first without one gets one (0x449490-0x4494a8).
// SHPPCINF (TB-C8g, Frost 2026-11-17): the first shape's FIRST SHPPCINF is left out, as inteng deletes the first entry of its list (0x44a5f0, 0x44a769-0x44a774);
// its others stay; the second's is read and nothing is done with it (0x44a5f3-0x44a601).
// THE MERGE DATA (TB-C8c, Frost 2026-11-17): the merged SHPMRGDT holds the first's entries, then the second's, each -1 kept and any other plus the first's
// polygon count (FUN_00449cf0 0x44a466-0x44a4ba). INTENG BUG, FIXED: inteng reads both shapes' lists without looking (0x44a29f / 0x44a42f only log "Tried
// to get first entry from a 0-length list"); here a shape without one counts as all -1, and the chunk is written when either shape has one.
// TEXTURED SHAPES (TB-C8a, Frost 2026-11-17): the merged SHPUVCRD holds the first's lists, then the second's (FUN_00448cf0 0x448fc6-0x449029), the second's textured
// polygons name their list + the first's count (0x448f57, the model's uvListsInFirst); SHPTEXFN is the first's, as inteng keeps the first's record (FUN_0042fc00).
// THE CENTRE (TB-C8k, Frost 2026-11-17): the merged shape's SHPCENTR is worked out again, as inteng's rebuild does (FUN_0050f570 at 0x44a780): the middle of the merged
// extents (each sum halved toward zero, 0x50fc36-0x50fcd4) and the radius about it; one is made when the first shape had none (0x50fce0-0x50fd05).
// THE SECOND'S SHAPE (TB-C8s, Frost 2026-11-17): it leaves the level with the second object when that object was its only user, as inteng deletes it (FUN_00449cf0:
// taken at 0x449d35, deleted at 0x44a830-0x44a83c). A shape other objects use too stays, less the second's name in its list of users (TB-C8b).
// SHARED SHAPES (TB-C8b, Frost 2026-11-17): a first shape other objects use too is not merged into: as inteng (0x449dba-0x449ddb: FUN_004d1090, FUN_0050e880, FUN_004d0ff0),
// the first object gets a COPY of its own, numbered one past the highest shape number the level names (in a SHPHEAD1 or an OBJHEAD1; the library numbers a new shape
// ++max_id, chunk_shape.cpp prepare_for_output), put after the last chunk (inteng's copy is a new child of the file), and the merge goes into the copy; the old shape stays
// for its other users, less the first's name. TO CHECK: inteng's copy gets a FRESH SHPHEAD1 (0x50e90a-0x50e9ab: flags from the shape's +0x9c, lock and version probably 0);
// ours keeps the old header's numbers but the shape number, the counts and the list.
// ONE SHAPE FOR BOTH (Frost 2026-11-17; an object and its copy, as most placed objects of the stock levels share their shape): the first gets its copy, the shape is
// merged into it as the second's, and the shape goes when the two were its only users (0x44a830), else it stays less both names.
//
// NAMED DEVIATIONS FROM INTENG (each one is security or crash safety, Tao 1; the card is in unfinished-in-inteng.md):
// - The merge is REFUSED when any other object's chunk holds the second object's 8-byte ID (a switch's target, a door, a parent...): inteng
//   removes the object and leaves such references pointing at nothing, which can crash the game. The bytes are searched, as object_edit.h does.
// - A module (an object with a MODULEDT) is never merged, either side: other chunks name modules.
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
