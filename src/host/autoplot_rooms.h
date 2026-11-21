// autoplot_rooms.h: THE ROOMS, the volume planner's rule for a room (Bishop 11-20, for the owner's "the whole genshd mapped": the kinds "junction or room" and "room with 2
// doorways", 36 AI modules of Genshd1 the planner left NOT SHAPED). Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng, on the seam agreed with Dietrich (the
// planner is his, autoplot_volumes.cpp; a room goes through his rules as they are, and this file holds what a room needs more). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// A room has the gold standard's shape (Drake's hand6g): one entry on the ceiling per doorway (the ceiling places nearest it, the planner's own), linked where they meet;
// its floor rectangles; a climb-in on a WALL and a get-out (the owner's counts); Airduct on; aims a hanging alien's box holds.
//
// THE WALL TEST. A corridor's floor fills it wall to wall, so a side of its floor rectangle is a wall. A room's floor rectangles often stop short of the wall: a desk, a
// pit or a raised band the flat floor leaves out, and the ceiling goes on over it (med16 on 6342c5de: its ceiling 3.9 m past the floor's south edge; office01). A climb
// put on such a side leads up into the open, and its HIGH lies under the entry that covers that ceiling (the checker: "its centre is in box N too"). So, for a room, a
// side is a wall only where the module's flat ceiling does NOT go on past it: AutoPlot_CeilingBeyond measures how much of the side it goes on past, and the planner
// takes no climb on a side where that is more than a quarter of the side (as its floorBeyond does for the floor).
#pragma once

#include <map>
#include <utility>

namespace host
{
	// How far outward a room's ceiling is looked for past a side (mm): two cells of 300; a wall stands at the floor's edge or within a hanging alien's reach of it
	double const kAutoPlotRoomWallLook = 600.0;

	// The mm of one side of a floor rectangle along which the module's FLAT ceiling goes on past it. The rectangle is its cell range ix0..ix1 x iz0..iz1 (the planner's
	// cells, cellEdge mm across); the side is the one facing along axis (0 x, 2 z) on sign's end (+1 the high end). ceilings: each cell's ceiling height (the world's mm,
	// y down), as the planner keeps them; ceilTop: the room's ceiling (the highest, the smallest y); flat: how far under ceilTop a ceiling still counts as the room's flat
	// ceiling. A cell along the side counts when any cell in the kAutoPlotRoomWallLook past it holds such a ceiling.
	double AutoPlot_CeilingBeyond(std::map<std::pair<int, int>, double> const &ceilings, int ix0, int ix1, int iz0, int iz1, int axis, int sign, double ceilTop, double flat,
		double cellEdge);
}
