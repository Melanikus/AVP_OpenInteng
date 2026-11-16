// file_pick.h: the FILE-SHAPE PICK, a service next to P1's pick (pick_game.h): a ray through a click, against the polygons of an object's shape as the LEVEL FILE
// holds them (the shape model's polygons of 3 to 5 corners, never split), giving back the file's own polygon number, the hit point and the polygon's plane. Move rel.
// to poly (0x51) needs it, and so will face selection and the Texture Map mode (owner-intent 11). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY NOT THE GAME'S OWN PICK. The game's line of sight (pick_game.h) numbers the polygons of its LOADED shape, where a polygon of more than 4 corners is split and the
// order is the game's; the editor's edits name a polygon by its number in the FILE (SHPPOLYS), which is also what inteng's FUN_005633e0 takes. So the game's pick says
// WHICH OBJECT is under the click, and this says which of the file's polygons of that object.
//
// WHAT IS COPIED (FUN_00473680, run 30; p11-shape-handlers.md 42): the hit point is where the pixel's ray meets the polygon's PLANE: t = (plane . point) / (plane . ray),
// no hit when the denominator is exactly 0.0, else the three coordinates of the point, each truncated toward 0 (__ftol). The polygon's first point and its normal are
// taken into the world by the object's turn (FUN_004b3860 / the quaternion matrix of FUN_00430d70, via model::RotateByTurn) and its place.
// WHAT IS OURS, named: inteng picks "the polygon under the pixel" with its own drawing (FUN_00559750, not read); here it is the NEAREST polygon along the ray whose
// plane the ray meets in front of it and whose edges hold the hit point (a polygon is looked at from either side). The check of "holds the point" is the usual one:
// the point and the corners dropped onto the plane's two widest axes, and the crossing count. Everything is in the level's FILE units (the places and points of the
// level file); the caller turns the game's millimetres into them (divide by the level's scale).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "shape_model.h"

class File_Chunk;
class OpenLevelFile;

namespace host
{
	// A ray in file units: where it starts and its direction (any length but 0)
	struct FilePickRay
	{
		double from[3];
		double direction[3];
	};

	struct FilePickHit
	{
		bool found;
		int32_t polygon;       // the polygon's number in the file's SHPPOLYS, counting from 0
		int32_t point[3];      // where the ray meets the polygon's plane, each coordinate truncated toward 0
		double distance;       // t, along the ray as the caller gave it
		int32_t planePoint[3]; // the polygon's first corner, in the world (a point on the plane)
		double normal[3];      // the polygon's normal in the world, 1 long (from the shape's own normal: from the corners when the shape has none)
		int cornerCount;
	};

	// The nearest polygon of the shape that the ray hits, for an object at place with turn (the four floats of its header). Gives back false, with hit->found false,
	// for a ray with no direction or a shape with no polygons; hit->found is false too when the ray hits none. Plain numbers: no level, no window.
	bool FilePick_Shape(model::ShapeModel const &shape, int32_t const place[3], float const turn[4], FilePickRay const &ray, FilePickHit *hit);

	enum class FilePickResult
	{
		Done,        // *hit is filled in (hit->found says whether anything was hit)
		BadCall,
		NoObject,    // no object of the level has that ID
		NoShape,     // the object has no shape the shape model takes
		BadHeader,   // the object's header is too short for a place and a turn
	};

	// The same for the object whose 8-byte ID is id in the open level: its shape from the shape model (as the edit store has it now), its place and turn from its
	// OBJHEAD1.
	FilePickResult FilePick_Object(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, FilePickRay const &ray, FilePickHit *hit);

	// The object's shape (as the edit store has it now) and WHERE THE GAME PUTS IT: every point of the shape turned by the object's turn and moved by its place, three whole numbers each (inteng's FUN_0055a900: FUN_004b3860 then FUN_004b3800),
	// and for every polygon its normal as three whole numbers in 65536ths, turned the same way (the shape's own normal cut toward 0 after the 65536 scale, as the game loader does; inteng's own conversion not read: ASSUMED; zeros for a
	// polygon with no normal). Both arrays are 3 numbers per point / per polygon.
	FilePickResult FilePick_World(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, model::ShapeModel *shape, std::vector<int32_t> *worldPoints, std::vector<int32_t> *worldNormals);
}
