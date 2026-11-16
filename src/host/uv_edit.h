// uv_edit.h: the host side of the Texture Map mode's data: reading one polygon's picture, texture coordinates and flag word from the open level, and
// putting a changed one back through the edit store. P11. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT DOES. An object (an RBOBJECT chunk) names its shape; the shape (a REBSHAPE chunk) holds SHPPOLYS (the polygons), SHPUVCRD (the lists of
// texture coordinates) and SHPTEXFN (the names of its pictures). This reads those chunks' BYTES through the edit store (OpenLevelFile::PayloadOf, so
// an earlier edit is seen), hands them to model::ReadShapeModel and model::ReadUvLists, and gives back one polygon as a model::PolygonUv (uv_model.h).
// A write does the reverse with OpenLevelFile::SetPayload. The polygon number is the FILE's polygon number (quads unsplit), the one FilePick_Object
// (file_pick.h) gives.
//
// THE RULES (a level's bytes are hostile; Tao 1 and 3).
// - Every count and index is checked by the model before anything is sized from it; nothing is changed unless the answer is Done.
// - A write keeps every chunk's SIZE (SetPayload never changes it): the polygon's list keeps its count of corners, and the polygon's own corner count,
//   engine type and list number stay.
// - Everything is made and checked BEFORE the first byte is put in.
// - The shape is the object's: a shape that other objects use too is the same bytes for all of them, as in inteng.
//
// NAMED DEVIATIONS (cards in AvP-docs\engine\inteng\unfinished-in-inteng.md, written with this slice):
// 1. A polygon's PICTURE cannot be changed here yet: inteng's "set this texture" (FUN_00555dd0, UNREAD) may add a name to the shape's SHPTEXFN, which
//    changes the chunk's size. A write whose pictureNumber differs from the file's is Refused with a plain message.
// 2. A list that other polygons share is Refused when its coordinates change (inteng's behaviour for a shared list is not read); the count of polygons
//    that use it is given back on a read so a mode can say so.
// 3. A polygon that is not textured is read with no coordinates and cannot be written (giving it a texture is the picture change above).
// 4. The flag word may not get bit 0x40000000 (the game's loader refuses a level with it: chunk_shape.cpp:1051).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "uv_model.h"

class File_Chunk;      // the game's chunk library
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class UvEditResult
	{
		Done,
		BadCall,     // no store, no level, no ID, or no place for the answer
		NoObject,    // no object has that ID
		NoShape,     // the object has no shape, or the shape lacks its points, polygons or header
		NoBytes,     // the edit store has no bytes for one of the shape's chunks
		BadShape,    // the shape model refused the shape's bytes (*why says why)
		BadUv,       // the texture model refused the coordinates, names or the polygon (*why says why)
		NoPolygon,   // the shape has no polygon with that number
		Shared,      // a write: the polygon's list is used by other polygons too (deviation 2)
		Refused,     // a write: the change is one the editor does not make or the bytes wouldn't take it (*why says why)
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *UvEditWords(UvEditResult result);

	// Reads polygon number polygonNumber of the shape of the object whose 8-byte ID is objectId. *out gets the polygon, with pictureName filled from the
	// shape's SHPTEXFN (empty when the shape has no such name). *listUsers (when not null) gets how many polygons of the shape use the polygon's list
	// (1 = only this one; 0 for a polygon that is not textured).
	UvEditResult UvEdit_ReadPolygon(OpenLevelFile const *store, File_Chunk *level, uint8_t const *objectId, int32_t polygonNumber, model::PolygonUv *out,
		std::string *why, size_t *listUsers = nullptr);

	// Puts a changed polygon back: its texture coordinates (the same count of corners as the file's list) and its flag word. The polygon is named by
	// polygon.polygonNumber; its engineType, cornerCount, uvListNumber and pictureNumber must be the file's (deviation 1: a different picture is
	// Refused). Nothing changes when the answer isn't Done.
	UvEditResult UvEdit_WritePolygon(OpenLevelFile *store, File_Chunk *level, uint8_t const *objectId, model::PolygonUv const &polygon, std::string *why);

	// The same for MANY polygons at once (a selection): the shape is read once, each polygon is checked against what the file holds, and the chunks are
	// written once at the end, so either every polygon's change goes in or none does. A polygon number may be named only once. A texture list that other
	// polygons use is changed only when every user of it is in the same write with the same coordinates (so a shared list stays one list). Same rules and
	// named deviations as UvEdit_WritePolygon.
	UvEditResult UvEdit_WritePolygons(OpenLevelFile *store, File_Chunk *level, uint8_t const *objectId, std::vector<model::PolygonUv> const &polygons,
		std::string *why);
}
