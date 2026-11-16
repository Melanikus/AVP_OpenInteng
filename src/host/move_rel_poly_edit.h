// move_rel_poly_edit.h: Move rel. to poly (Object Placement and Track Editing, command 0x51) done on the open level: the moving object's place and turn are set
// from one polygon of another object's shape, through the edit store. The sums are move_rel_poly.h's (inteng's FUN_005633e0); this adds what puts them on the level's
// own numbers. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// UNITS (p11-shape-handlers.md 32, run 21): inteng's own place unit is 1e-5 m, which is the file's place times 100 times the level's scale (the game's local_scale,
// ENVSDSCL; 1 when the level has none). Inteng holds a shape's points in the same unit as its places (the sums add the two), so here every point, place and click
// point is made into inteng's unit with that factor (rounded to the nearest whole number: ours, named), the sums are done, and the place comes back to file units
// by cutting toward 0 (FUN_004dfc70 saves a place with __ftol).
// THE TURNS (run 37, p11 section 46 and menu-reads-12.txt:2592):
//   - The target's turn in the file is four floats (x, y, z, w). Inteng holds it as a 3x3 of 16.16 ints made by FUN_004b3c80 from an int quaternion in the order
//     (w, x, y, z). It is the CONJUGATE of the file's: inteng turns a point by x' = m0 x + m3 y + m6 z (FUN_004b3860), which is the transpose of the game's own
//     matrix (FUN_00430d70, x' = m0 x + m1 y + m2 z), and the same turn comes out of the conjugate quaternion. ASSUMED (inteng's loader of the turn is not read; the
//     test checks that the matrix turns a point exactly as the game's rotation of the file's floats does, which is what shows on screen). Each part is times 65536
//     and rounded to nearest, ties to even.
//   - The moving object's new matrix becomes the file's four floats by FUN_004de070 (the usual matrix to quaternion by the largest diagonal term).
// NAMED, besides move_rel_poly.h's: a file polygon of 3 to 5 corners gives the corner list as its own corners (inteng's list is the polygon's first 3 corners plus the
// merged-from polygon's others, which for a merged quad is the same four); the polygon's normal is the shape's float normal times 65536, cut toward 0 (the game
// loader's own conversion, chunk_load.cpp:2511; inteng's is not read).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	// FUN_004b3c80 on the int quaternion made from a file turn (x, y, z, w floats): the nine ints m0..m8 in inteng's order
	void MoveRelPolyEdit_TurnToMatrix(float const turn[4], int32_t m[9]);

	// FUN_004de070: the nine ints (16.16) to the file's four floats (x, y, z, w)
	void MoveRelPolyEdit_MatrixToTurn(int32_t const m[9], float turn[4]);

	enum class MoveRelPolyEditResult
	{
		Done,
		BadCall,
		SameObject,   // the polygon is the moving object's own: inteng refuses it
		NoObject,     // the moving or the target object isn't in the level
		NoShape,      // the target has no shape the shape model takes, or no header of a place and a turn
		BadPolygon,   // the polygon number isn't one of the target's shape's
		TooFar,       // a number that doesn't fit inteng's ints, or the new place leaves the game's 2^24 either way
		Refused,      // the edit store wouldn't take the new header
	};

	char const *MoveRelPolyEdit_Words(MoveRelPolyEditResult result);

	// The ID of the object whose name is exactly name (capitals or not), the first in the level's order: a module the game's pick hit is named, and its object is the one
	// to take the polygon from. False when none has that name.
	bool MoveRelPolyEdit_ObjectNamed(File_Chunk *level, char const *name, uint8_t id[8]);

	// Sets the place and turn of the object whose 8-byte ID is movingId so that it lies on polygon `polygon` (the file's number, from 0) of the object targetId's shape,
	// `distance` (inteng's unit, 1e-5 m) back along the polygon's normal, its +z axis along the negated normal (move_rel_poly.h). hitFile is the click's point on the
	// polygon's plane in FILE units (FilePickHit.point) or null for the polygon's centre. scale is the level's scale (the game's local_scale).
	// Nothing is changed unless the answer is Done.
	MoveRelPolyEditResult MoveRelPolyEdit_Do(OpenLevelFile *store, File_Chunk *level, uint8_t const *movingId, uint8_t const *targetId, int32_t polygon, int32_t distance,
		int32_t const *hitFile, double scale, std::string *why);

	// SNAP TO VERTEX (Object Placement, 0x4e2; FUN_004f7000, read by Vasquez 2026-11-15, AvP-docs/engine/inteng/object-placement-reads.md): the vertex of the object targetId's shape
	// (in the world: its points turned by its turn and moved by its place) NEAREST to point (file units) is the target T. The moving object's own vertex nearest T is found the same way,
	// and the object is moved by T minus that vertex. Gives T in *vertex (file units). Nothing is changed unless the answer is Done. NAMED: inteng does this each frame under the mouse and
	// keeps the result on a left click (Esc puts the object back); here the click names the object and the point, once. inteng also turns the object to the polygon's normal when a fourth
	// argument is set; what sets it is not read (the call pushes three), so the turn is left alone.
	MoveRelPolyEditResult SnapToVertex_Target(OpenLevelFile *store, File_Chunk *level, uint8_t const *targetId, double const point[3], int32_t vertex[3]);
	MoveRelPolyEditResult SnapToVertex_MoveObject(OpenLevelFile *store, File_Chunk *level, uint8_t const *movingId, int32_t const vertex[3]);

	// An object's place as its header holds it (file units), and set again: for Move relative (Object Placement 0x1c, FUN_004e1d70: the placed shape object's place typed as an offset from the module it is in).
	// Nothing changes unless the answer is Done.
	MoveRelPolyEditResult ObjectPlace_Get(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t place[3]);
	MoveRelPolyEditResult ObjectPlace_Set(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int32_t const place[3]);
}
