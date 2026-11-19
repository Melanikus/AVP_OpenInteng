// imported_shape.h: whether a level object's shape is IMPORTED, as the editor's guards ask it, for the lister (verify_list_objects), so a scenario does not pick a subject a guard
// refuses (Vasquez 11-18: chair1, an imported shape, cost ~20 min). Written 2026-11-19 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// AS THE EDITOR ASKS IT:
//   - imported: the object's shape holds an outside file (SHPEXTFL, the chunk the game's loader takes an imported shape by); inteng rebuilds only shapes with none
//     (texture_map_screen.cpp), and Texture Map leaves them out.
//   - its own pictures: that SHPEXTFL holds the shape's own list of pictures (BMPLSTST), so its pictures are numbered by that list, not the level's: S + click refuses it
//     (polygon_items_read.cpp PolygonItems_ShapeHasOwnPictureList).
#pragma once

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"

enum class ImportedShape
{
	NoShape,          // the object has no shape (or there is no object)
	No,               // its shape has no outside file
	Yes,              // its shape has an outside file
	YesOwnPictures,   // and that file holds the shape's own list of pictures
};

inline ImportedShape ImportedShapeOf(Object_Chunk *object)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

	if (!shape)
	{
		return ImportedShape::NoShape;
	}

	Chunk *const outside = shape->lookup_single_child("SHPEXTFL");

	if (!outside)
	{
		return ImportedShape::No;
	}

	// a chunk the library keeps as plain bytes (it did not know it) has no children to look in
	Chunk_With_Children *const holder = outside->r_u_miscellaneous() ? nullptr : dynamic_cast<Chunk_With_Children *>(outside);

	return holder && holder->lookup_single_child("BMPLSTST") ? ImportedShape::YesOwnPictures : ImportedShape::Yes;
}

// The lister's word for it
inline char const *ImportedShapeWords(ImportedShape imported)
{
	switch (imported)
	{
	case ImportedShape::No:
		return "no";
	case ImportedShape::Yes:
		return "yes";
	case ImportedShape::YesOwnPictures:
		return "yes, own pictures";
	default:
		return "no shape";
	}
}
