// shape_replace_edit.h: Replace shape (Object Placement, Object, Replace shape; command 0x1d) on the open level: a placed object is given another of
// the level's shapes. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_004e2e10, out\menu-reads-18.txt:4568-4629; p11-shape-handlers.md sections 25, 26): the object's shape number and index
// are set to the chosen shape's, its module reference (modules only) is repointed, and its BACKUP (a copy of the shape's points and extents
// that Rescale works from) is made again from the new shape. The object's size number is NOT reset. The shape's own points are not changed.
//
// WHAT THAT IS IN THE LEVEL'S BYTES. The backup, the size, the shape table index and the module reference are inteng's memory only: the file
// holds the pairing and nothing else. The pairing is two things the library writes together (Object_Header_Chunk, Shape_Header_Chunk::
// prepare_for_output, avpx 3dc/win95/chunk_shape.cpp:1655-1680): the object's OBJHEAD1 holds the shape's number (shape_id_no), and each
// shape's SHPHEAD1 holds the names of the objects that use it. So this changes exactly those three chunks: the object's shape number, the old
// shape's list without the object's name, and the new shape's list with it at the end (the library appends it, deassoc_with_shape then
// assoc_with_shape: chunk_object.cpp:103-141). Every other byte of the level is as it was.
//
// THE RULES (a level's bytes are hostile; Tao 1 and 3):
// - Only a PLACED object is given another shape (inteng: only an object of kind 0 shows the chooser, FUN_004d9af0 at 0x4d94e0); a module (an
//   object with a MODULEDT) is refused, as other chunks name modules.
// - The chosen shape must be one the level holds. It is chosen by its outside file's name (as Quick-create chooses), a name with an @ is not
//   taken; see the deviation.
// - Nothing is changed unless the game's own loader takes the changed level (OpenLevelFile::ReplaceLevel); after a success every Chunk * the
//   editor held is gone (object_edit.h says the same).
//
// NAMED DEVIATION (drake-deviations.md): inteng shows its shape chooser SCREEN for the choice; here the shape is typed by name. The effect on
// the level's data is inteng's own.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ShapeReplaceResult
	{
		Done,
		BadCall,        // no level, no ID, or no name
		NoObject,       // the ID names no object
		NotPlaced,      // the object is a module, or not a placed object
		NoSuchShape,    // the level has no shape by that name
		SameShape,      // the object already has that shape
		CantBuild,      // the level couldn't be put together from its bytes
		LoaderRefused,  // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *ShapeReplaceWords(ShapeReplaceResult result);

	// Gives the object whose 8-byte ID is id the shape that came from the outside file called shapeName (its name without the extension, capitals
	// not mattering). *newShapeNumber (when not null) gets the shape's number.
	ShapeReplaceResult ShapeReplace_ByName(OpenLevelFile *file, uint8_t const *id, char const *shapeName, int32_t *newShapeNumber);

	// The same, the shape given by its number (the number the object's OBJHEAD1 and the shape's SHPHEAD1 hold)
	ShapeReplaceResult ShapeReplace_ByNumber(OpenLevelFile *file, uint8_t const *id, int32_t shapeNumber);
}
