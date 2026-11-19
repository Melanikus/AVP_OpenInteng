// surface_place.h: PLACEMENT MODE's pose (the owner's addition 11-18, Hicks's plan section 5; inteng had none): an object dragged by the gizmo's centre ball slides over walls and floors,
// its base flat on the surface under the pointer. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Pure maths, no level and no window (the editor casts the line of sight each frame, PickGame_AtClickLeavingOut, and hands the hit here). The world is the game's: y DOWN, so "up" is -y and an
// object's BASE is its local +y side. Places are in one unit throughout, the hit, the box and the answer alike (the editor's live loops use the file's units: the game's millimetres divided by
// the level's scale).
//
// THE TURN is the file's, x y z w, as OBJHEAD1 holds it and the snap's and the fly's live state keep it: a point of the shape at local p is drawn at the object's place plus R p, R the turn's
// own rotation matrix (row 1: 1 - 2(y^2 + z^2), 2(xy - zw), 2(xz + yw)). That is model::TurnMatrix / RotateByTurn (inteng's FUN_00430d70 / FUN_00430e80), and it is how the game draws it: the
// loader takes QuatToMat of (-x, -y, -z, w) (object_setup.cpp add_default_object), and _RotateVector multiplies by that matrix turned over (maths.cpp). (MoveRelPolyEdit_TurnToMatrix gives
// inteng's matrix in the game's form, R turned over: not this.)
//
// THE POSE (the owner's decision 11-18: placement aligns the object to the surface):
// - the turn: the SMALLEST turn that brings the object's up (R (0, -1, 0)) onto the surface's normal (the way out of the surface), applied after the object's own, so its heading about the
//   normal is kept as far as it can be: on a floor an upright object keeps its turn (to length 1). Up exactly opposite the normal (an upright object onto a ceiling): half a turn about the
//   object's own x axis (R (1, 0, 0)).
// - the place: the hit point, pushed out along the normal by the box's base extent (the local box's largest y, as y is down), so the base rests ON the surface, not half inside it.
// - a kind that holds no turn (a sound, a generator, a bad guy, a virtual object): the hit point alone, its turn copied as it came.
#pragma once

#include <stdint.h>

namespace host
{
	struct SurfacePlaceInput
	{
		int32_t hitPoint[3];    // where the line of sight met the surface
		int32_t hitNormal[3];   // that face's normal, 65536 = length 1 (the game's ONE_FIXED), pointing OUT of the surface (towards the camera that saw it); any length but 0
		int32_t boxMin[3];      // the object's own box, in its local frame (its shape's extents), y down
		int32_t boxMax[3];
		float turn[4];          // the object's turn now, the file's x y z w (see above); any length but 0
		bool canTurn;           // false: a move-only kind takes the hit point only
	};

	struct SurfacePlacePose
	{
		int32_t place[3];       // the object's new place
		float turn[4];          // its new turn, the file's x y z w, of length 1; for a kind that cannot turn, the old one copied as it came
	};

	// The pose for one hit. False, with *out untouched, for a normal of no length, a turn of no length or not a number, or a place that would be past 2^24 on an axis.
	bool SurfacePlace_Pose(SurfacePlaceInput const &in, SurfacePlacePose *out);

	// ONE FRAME OF THE LIVE DRAG (the editor's Snap To Vertex live loop, in placement mode): the game's numbers made into the file's. The pick's hit point and normal and the camera's place
	// are in the game's world units (the file's times the level's scale); the normal is turned round to face the camera when it faces away (a face seen from behind); the object's base below
	// its place is in world units (LevelView_ObjectBox's largest y less the object's place).
	struct SurfaceLiveInput
	{
		int32_t hitWorld[3];      // PickReport::hitPoint
		int32_t hitNormal[3];     // PickReport::hitNormal, 65536 = length 1, either way round
		int32_t cameraWorld[3];   // where the line of sight came from
		double scale;             // the level's scale (the game's local_scale): a real number from 1/1000 to 1000
		int32_t baseWorld;        // the object's base below its place, world units, y down
		float turn[4];            // the turn the drag started from, the file's x y z w
		bool canTurn;             // false: a move-only kind takes the hit point only
	};

	// The pose for one frame: its place in the FILE's units (the world's divided by the scale, the nearest), its turn the file's. False, with *out untouched, for a scale out of its bounds or
	// not a number, a normal that can't be turned round (a part of -2^31), or SurfacePlace_Pose's refusals.
	bool SurfacePlace_Live(SurfaceLiveInput const &in, SurfacePlacePose *out);
}
