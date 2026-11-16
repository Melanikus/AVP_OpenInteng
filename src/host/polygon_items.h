// polygon_items.h: which of the game's ITEMS a polygon of the level FILE is, and the other way round. The Texture Map mode selects polygons by the file's own numbers (file_pick.h,
// uv_edit.h) but the game draws its loaded shape, whose polygons are its items. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OPENINTENG. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHERE THE NUMBERS PART. A shape that has a SHPMRGDT chunk is stored as TRIANGLES, and the loader (avpx chunk_load.cpp merge_polygons_in_chunkshape) joins the pairs the chunk names
// into quads when they lie flat enough and their pictures fit (tex_merge_polys); a pair that cannot be joined stays two triangles. The merged list is the game's items, in the same
// order. Without a SHPMRGDT chunk the items are the file's polygons one for one. So the two numberings agree until the first pair, and a joined pair is ONE item for TWO file polygons.
// Which pairs were joined is not in the file (the loader decides), but the game's own item says it: a joined quad has a fourth corner, a triangle left apart does not. This is the loader's
// own walk (chunk_load.cpp:2372-2412, the PolyConv of the animation code, with its comment on why it cannot run off the end), over the file's merge data and the game's item corners.
//
// THE RULES (a level's bytes are hostile, Tao 1 and 3): the merge data must have one entry per file polygon; an entry is -1 or the number of a partner that is another polygon of the
// shape and names this one back; the walk never reads an item the shape does not have; every file polygon gets an item and every item is used. Anything else is refused whole.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	size_t const kMostPolygonItems = 1u << 20;   // the shape model's own bound on polygons, so no vector here is sized from a number past it

	enum class PolygonItemResult
	{
		Ok,
		BadCall,           // a null pointer for a size above 0, or a count past kMostPolygonItems
		MergeSizeWrong,    // the merge data does not have one number per file polygon
		BadPartner,        // an entry that is not -1, a polygon of the shape that names this one back, and not itself
		ItemsRanOut,       // the walk needed an item the shape does not have
		ItemsLeftOver,     // the walk ended with items nobody maps to
		ItemCornersWrong,  // an item with fewer than 3 or more than 5 corners
	};

	// What an answer means, in plain words: static text, never null
	char const *PolygonItemWords(PolygonItemResult result);

	// The loader's own walk. mergeData has mergeCount ints, which must be one per file polygon (filePolygons of them), or is null when the shape has no SHPMRGDT chunk (then the items are the file's polygons one
	// for one and itemCorners must have filePolygons entries). itemCorners[j] is how many corners the game's item j has, 3 to 5. On Ok, (*fileToItem)[p] is the item that file polygon
	// p is part of, and (*itemFirstFile)[j] the first file polygon of item j (a joined pair: the lower number).
	PolygonItemResult PolygonItems_Map(int32_t const *mergeData, size_t mergeCount, size_t filePolygons, uint8_t const *itemCorners, size_t items,
		std::vector<int32_t> *fileToItem, std::vector<int32_t> *itemFirstFile);

	// The shape's SHPMRGDT numbers for the object whose 8-byte ID is id, read through the edit store: *hasMergeData false (and *numbers empty) for a shape without the chunk. False when
	// there is no such object or shape, or the chunk's bytes are not a whole number of ints.
	bool PolygonItems_ReadMergeData(OpenLevelFile const *store, File_Chunk *level, uint8_t const *id, std::vector<int32_t> *numbers, bool *hasMergeData);

	// Whether the shape of the object with the 8-byte ID has its OWN list of pictures (a SHPEXTFL holding a BMPLSTST: the game's setup_tex_conv_array then numbers the picture of every polygon
	// by THAT list, not by the level's list of pictures). False when the shape has none, or there is no such object.
	bool PolygonItems_ShapeHasOwnPictureList(File_Chunk *level, uint8_t const *id);

	// The object of the level file that a module of the game is. The game keeps the object's name (MODULE.name) and its world place (MODULE.m_world, the file place times the level's scale),
	// but names repeat (two rooms called the same), so: the non-placed object with that name whose place gives that world place wins; failing that, a name that only ONE non-placed object has
	// is taken whatever its place. A placed object (a door) is never a module (projload.cpp makes a module of every object without OBJECT_FLAG_PLACED_OBJECT). False when none is chosen.
	bool PolygonItems_ModuleObject(File_Chunk *level, char const *name, int32_t const world[3], double scale, uint8_t id[8]);
}
