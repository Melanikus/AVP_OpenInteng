// shape_edit.h: the host side of the shape model: reading an object's shape from the open level, and putting a changed shape's numbers back
// through the edit store. P11, slice 2. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT DOES. An object (an RBOBJECT chunk) names its shape by number, and the game's chunk library pairs the two (Object_Chunk::
// get_assoc_shape). The shape (a REBSHAPE chunk) holds the chunks that model::ReadShapeModel takes (SHPHEAD1, SHPRAWVT, SHPPOLYS, SHPPNORM,
// SHPVNORM). This reads the BYTES of those chunks through the edit store (OpenLevelFile::PayloadOf, so an earlier edit is seen) and hands
// them to the model; and it writes the model's numbers back with OpenLevelFile::SetPayload.
//
// THE RULES.
// - The level's bytes are hostile: the model checks every count and index before anything is sized from it. Nothing is changed when the
//   answer isn't Done.
// - When the library takes "the first" child of a name, so does this (Chunk::lookup_single_child), so a hostile file with two SHPRAWVT gives
//   the one the game uses.
// - A write keeps every chunk's SIZE (SetPayload never changes it): it refuses a model whose point or polygon count differs from the file's.
//   Edits that change a count (Merge Objects, Fix tears, Replace shape) rebuild whole chunks instead, the way object_edit.h's create and delete
//   do (BuildLevel, then OpenLevelFile::ReplaceLevel). That is the next slice; inteng's own shape edits all end in such a rebuild
//   (engine\inteng\p11-shape-handlers.md, section 6).
// - The shape is the object's: a shape that other objects use too is the same bytes for all of them, so a write here changes them all, as it
//   does in inteng. The count of users is given back (ShapeUsers) so a mode can say so.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "shape_model.h"

class File_Chunk;      // the game's chunk library
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ShapeEditResult
	{
		Done,
		BadCall,      // no store, no level, no ID, or no place for the answer
		NoObject,     // no object has that ID
		NoShape,      // the object has no shape, or the shape lacks a header, points or polygons
		NoBytes,      // the edit store has no bytes for one of the shape's chunks
		BadShape,     // the model refused the bytes (*why says why in the model's words)
		Refused,      // a write: the model's counts differ from the file's, or the bytes wouldn't take the change
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *ShapeEditWords(ShapeEditResult result);

	// Reads the shape of the object whose 8-byte ID is id into *shape. *users gets how many objects of the level share this shape (1 = only this
	// one), when it isn't null.
	ShapeEditResult ShapeEdit_Read(File_Chunk *level, OpenLevelFile const *store, uint8_t const *id, model::ShapeModel *shape, size_t *users,
		std::string *why);

	// Puts the shape's points, polygons, both normal arrays and its header's radius and extents back, each into the bytes it came from. All the bytes
	// are prepared and checked before the first is put in. The model's counts have to be the file's.
	ShapeEditResult ShapeEdit_WriteSameCounts(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, model::ShapeModel const &shape,
		std::string *why);

	// Recentre Shape, "Move centre to top" (top true) or "to bottom" (false), for the shape of the object whose 8-byte ID is id. Copies inteng's
	// command code for 0x14a / 0x14b (FUN_004452b0) and FUN_00473c20: AvP-docs\engine\inteng\p11-shape-handlers.md sections 2, 10, 12, 30, 31.
	//  - A shape with a SHPMORPH is REFUSED, as inteng refuses it.
	//  - The target is the middle of the extents on x and z and the top or bottom on y (model::RecentreTarget); every point, the extents and the
	//    radius move by the NEGATED target (model::TranslateShape); the normals don't change.
	//  - The place of EVERY object that uses the shape moves by minus (its own turn applied to that same offset): place = place - M x Q, M from
	//    the object's four turn floats (model::RotateByTurn), so the world stays where it was. A place that would leave 2^24 either way is
	//    refused, as the game's loader refuses it.
	// Everything is prepared and checked before the first byte is put in; the shape's chunks and every user's header keep their sizes. *users
	// (when not null) gets how many objects were moved.
	ShapeEditResult ShapeEdit_Recentre(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, bool top, size_t *users, std::string *why);

	// Texture Map, Recentre Shape (0xd6, FUN_00565bd0 + FUN_00473c20): the same move as above, with the CENTRE OF ONE POLYGON of the shape as the point that goes to the origin. The polygon's distinct
	// corner points are added up and the offset is ftol(sum / -n) per axis (FUN_0042f9b0 divides, cutting toward 0), n the number of corners (inteng counts 3, or 4 for a merged pair; a file polygon holds
	// the 4). Every user's place moves as in ShapeEdit_Recentre. A shape with a SHPMORPH is refused.
	ShapeEditResult ShapeEdit_RecentreOnPolygon(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t polygon, size_t *users, std::string *why);

	// Recalculate Normals and Extents (command 0x25b) for the shape of the object whose 8-byte ID is id. Copies FUN_0044baa0 (model::
	// RecalculateNormalsAndExtents: every polygon normal, every point normal, the extents) and puts the radius about the ORIGIN in the header,
	// because the header's radius is that one (p11-shape-handlers.md section 20). The counts don't change, so it is a same-size write of the
	// shape's chunks. The shape is the object's: other objects that use it get the new numbers too, as in inteng. Nothing changes when the
	// answer isn't Done. *users (when not null) is how many objects of the level use the shape.
	ShapeEditResult ShapeEdit_Recalculate(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, size_t *users, std::string *why);

	// Fix tears (command 0x5a, FUN_0044ad20 -> FUN_0044abd0), the weld of two objects' shapes. Copies the data effect read in p11-shape-handlers.md
	// sections 8, 47 and 48: the shape of the object named by movedId (the FIRST of the two selected) has every point that lies, in the world, within
	// `tolerance` on each axis of a point of the other object's shape (referenceId, the SECOND) moved onto it (model::WeldShapePoints: world place =
	// point + the object's own place, translation only, first match in order); then the moved shape's normals, extents and radius are made again
	// (inteng's rebuild FUN_0050f570 and FUN_0044baa0) and its chunks written back. The reference shape is not changed. *welded (when not null) is
	// how many points were moved. Nothing changes when the answer isn't Done.
	// NAMED DEVIATIONS (a card in unfinished-in-inteng.md): (1) a moved shape that other objects use too is REFUSED (inteng copies it for this object
	// first); (2) a shape with a SHPMORPH on either side is REFUSED (inteng also welds the morph's sub-shapes); (3) the same shape twice is refused
	// (the model's SameShape). NOT CHECKED HERE: inteng wants exactly two selected objects that are not "placed" (kind flag 0x400 clear): that is the
	// mode's job, as is asking for the tolerance (an argument of FUN_0044ad20, typed by the caller).
	ShapeEditResult ShapeEdit_FixTears(OpenLevelFile *store, File_Chunk *level, uint8_t const *movedId, uint8_t const *referenceId, int32_t tolerance,
		size_t *welded, std::string *why);
}
