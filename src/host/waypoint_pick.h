// waypoint_pick.h: WAYPOINT MODE BY MOUSE (the owner's addition 11-18; Hicks's plan .claude/plans/streamed-twirling-spindle.md section 6, Bishop's order P4; the ALIEN list only: the game
// loads only it, projload.cpp:2614-2651): the pure sums of its clicks, a test can drive. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE WORLD: the game's millimetres, y DOWN; a waypoint's numbers are the file's, the world's being the file's times the level's scale (waypoint_view.cpp World: toward zero), and the
// 3D view draws each box from its min and max (a minimised one, not the current, as a cube 200 each way round its aim point, kWaypointMinimisedHalf).
// THE CLICK (the readings Bishop agreed 11-18): the box the line from the camera meets first (its way in nearest; not one the camera is inside; not behind the wall the game's line of
// sight met) becomes the current waypoint; a FLOOR (a face whose normal is within 45 degrees of straight up) of the AI module gets a new box; anything else lets the current go.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "waypoint_model.h"

namespace host
{
	// A new box on a floor: 2 m across each way (1 m from the click), 2 m up from the floor; the aim point in its middle (the world's millimetres)
	int32_t const kWaypointNewHalfWidth = 1000;
	int32_t const kWaypointNewHeight = 2000;

	// A minimised box's half side round its aim point, as the 3D view draws it (waypoint_view.h kWaypointMinimisedHalf: 20,000 of inteng's units)
	int32_t const kWaypointPickMinimisedHalf = 200;

	// How far behind the wall the game's line met a box may begin and still be taken (a box on a floor or against a wall starts exactly at it)
	double const kWaypointPickPastWall = 2.0;

	// Waypoint wp's box in the world, as the 3D view draws it (minimisedCube: the minimised cube round its aim point)
	void WaypointPick_WorldBox(WaypointRec const &wp, double levelScale, bool minimisedCube, double lo[3], double hi[3]);

	// Where the line from `from` along `direction` (any length above 0) comes INTO the box, in the world's millimetres along it; false when it misses it, the box is behind, the line
	// starts inside it, or the direction has no length
	bool WaypointPick_RayEnters(double const from[3], double const direction[3], double const lo[3], double const hi[3], double *distance);

	// The waypoint of the list the line meets first; -1 for none. wallAt: how far along the line the game's line of sight met a surface (mm), or below 0 when it met none; a box whose
	// way in is past it (by more than kWaypointPickPastWall) is behind that surface. minimised and current as the 3D view draws them.
	int32_t WaypointPick_BoxOnRay(std::vector<WaypointRec> const &list, double levelScale, bool minimised, int32_t current, double const from[3], double const direction[3], double wallAt);

	// A floor: the face's normal (65536 long in the game, any length taken) within 45 degrees of straight up (-y); false for no length
	bool WaypointPick_IsFloor(int32_t const normal[3]);

	// ITEM 3b, A LINK: one line between two aim points, as the 3D view draws it (waypoint_view.cpp: one line a pair). A click takes it when it is within kWaypointPickLinkPixels of the line
	// on the screen and at least kWaypointPickLinkEnds from both ends (nearer an end the click is the box's).
	double const kWaypointPickLinkPixels = 4.0;
	double const kWaypointPickLinkEnds = 12.0;

	// The aim point of a waypoint in the world, as the view draws a link's end (the file's number times the scale, toward zero)
	void WaypointPick_AimWorld(WaypointRec const &wp, double levelScale, int32_t out[3]);

	// The pixels from (px, py) to the line from (ax, ay) to (bx, by) on the screen (to its nearest end past either end)
	double WaypointPick_SegmentPixels(double ax, double ay, double bx, double by, double px, double py);

	// The link rule above: true with *pixels when (px, py) takes the line from (ax, ay) to (bx, by)
	bool WaypointPick_NearLink(double ax, double ay, double bx, double by, double px, double py, double *pixels);

	// The flags a click on the SELECTED link gives it next, seen from its base: both ways, one way base to current, one way current to base; then those three Aliens only; then those
	// three Deviation allowed: 0 1 2 4 5 6 8 9 10, then 0 again. Flags out of that cycle (12: both bits) go to 0.
	int32_t WaypointPick_NextLinkFlags(int32_t flags);

	// The new waypoint for a floor met at `hit` (the world's mm): its box and aim point in the file's units (the world's over the scale, toward minus: waypoint_view.cpp FileUnits),
	// flags 0, no links
	WaypointRec WaypointPick_NewBox(int32_t const hit[3], double levelScale);
}
