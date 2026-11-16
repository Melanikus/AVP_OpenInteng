// place_at_view.h: P3 of plans/sub-modes.md, the ONE function that works out where a new thing goes: the camera's place, or a place a distance
// from it along one axis (the 7 variants of Point Create and Virtual Object: On Camera, In Front, Behind, Above, Under, Left, Right). Every Create
// item differs only in WHAT it makes; WHERE is this. Plain numbers, no window, no game: a test can drive it. Written 2026-11-15 by Melanikus
// (Dietrich) for AVP_OpenInteng (P3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// What is NOT here yet, and why: how far each of inteng's items goes (a distance per item) and whether its Above and Under follow the camera's own up
// or the level's: the handlers of Point Create (0x46-0x4c) and Virtual Object Create are not read ("not found yet" in plans/wip-items.md). So the
// distance is the caller's, and Above and Under are the level's own up and down here (ours until read; the game's y points down, so up is less y,
// as the R and F keys move). An item that is read gives its own distance and flips itself, citing the function.

#pragma once

#include <stdint.h>

namespace host
{
	enum class ViewOffset
	{
		OnCamera,   // the camera's own place; the distance is not used
		InFront,    // along the way the camera faces (the look up or down included)
		Behind,
		Above,      // straight up in the level
		Under,      // straight down in the level
		Left,       // along the camera's own left
		Right,
	};

	// The way the camera faces and its right, as the game's last frame drew them (LevelView_CameraAxes): each 65536 = length 1
	struct ViewAxes
	{
		int32_t forward[3];
		int32_t right[3];
	};

	// The farthest a place may be from the level's own centre on any axis, in millimetres: 1 << 24, the editor's own limit for a place (the level
	// file's places are held to it too)
	int32_t const kPlaceAtViewFarthest = 1 << 24;

	// Works out the place for where, distance millimetres from camera (its place, in the level's millimetres). Gives back false, with place
	// untouched, when the distance is below 0 or above kPlaceAtViewFarthest, or the result would lie farther than kPlaceAtViewFarthest from the
	// level's centre on any axis, or where is not one of the seven.
	bool PlaceAtView_Make(const int32_t camera[3], const ViewAxes &axes, ViewOffset where, int32_t distance, int32_t place[3]);
}
