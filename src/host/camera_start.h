// camera_start.h: where the editor's camera starts in a level just opened. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng
// (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the camera used to start at the player's display block (Player->ObWorld). The game puts the player's BODY at the level's start
// (InitPlayer: DynPtr->Position = PlayerStartLocation, player.cpp:340, from the level's AVPSTART record, projload.cpp:149-181), and the
// display block only follows the body once the game's own frame moves the player, which the editor never runs. So the display block was
// still at 0,0,0. Invasion drew only because its origin happens to be inside a room; Genshd1's isn't, and its view was black (2026-11-14).
//
// THE RULE: the camera starts at the player's body. If any piece on the way to it is missing (no player, no strategy block, no dynamics
// block), nothing missing is ever read: the camera starts at the level's own start instead, and the reason says which piece was missing.
// This is plain data, so a test tries every case.

#pragma once

struct CameraPlace
{
	int x;
	int y;
	int z;
};

// The place the camera starts at. body is only read when all three pieces are there. why is set to null when the body was used, or else
// to the editor's own words for the missing piece.
CameraPlace CameraStart_Choose(bool hasPlayer, bool hasStrategy, bool hasDynamics, const CameraPlace &body, const CameraPlace &levelStart,
	const char **why);
