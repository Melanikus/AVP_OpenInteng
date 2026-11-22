// autoplot_closers.h: THE GAP CLOSERS, boxes that close the wall band the volume planner leaves open (autoplot_volumes.h). Written 2026-11-21 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY (the owner 11-21: "Gaps first", "fitter boxes as large as possible within the space we have and 'tetris together' to not leave gaps in the module", "no smaller than .9
// right, need to hold the alien"). The game counts an alien IN a box when any of its body's 8 corners is inside it (bh_waypoint.cpp WaypointContainsNPC); in no box its route
// is dropped and it heads straight at its target ("Waypoint dropout"). On Hudson's whole-2b runs (Vasquez's gaps.js, AvP-docs/tools/vasquez/seams) 3,073 frames of aliens
// stuck on walls or ceilings had their body in no box, and 1,507 of them (49%) were on a WALL just above the floor box's top (73-111 mm over it) or just under an entry,
// away from the 2.4 m climb-in: the floor box stops 1.5 m over the floor and the entries 450 mm under the ceiling, and between them only the climb-in stands. Rebellion's
// own boxes reach floor to ceiling (129 of Genshd1's 136 stock boxes are 2.5 m or taller), so its walls have no such band.
//
// WHAT (autoplot_volumes.cpp makes them, when the switch is on): along each upright wall side of a shaped floor (no other floor along it, not a tube's sloped side; a side
// with a doorway less each doorway's span, below), over the band from the floor box's top up to the bottom of the entries over it, the fewest, largest boxes that fit
// between the boxes already there:
//   - each at most kAutoPlotCloserMost and at least kAutoPlotCloserLeast along the wall, the free stretch split evenly (a 5.1 m stretch: 3 x 1.7 m);
//   - the band whole as one row when it is at most kAutoPlotCloserMost tall (Genshd1's 3.9 m rooms: 1,945 mm, one row), else the fewest rows split evenly; a band under
//     kAutoPlotCloserLeast is left (a body is taller than it, so it is always in the floor box or an entry);
//   - kAutoPlotCloserDepth out from the wall's face (and kAutoPlotInto into it, as the climb-in): an alien on a wall stands about 450 off the face (medent06's frame:
//     453), so its body spans the 900 in front of the face; the closer's aim point 450 out, so the body of an alien on the wall holds it;
//   - touching every box round it, never overlapping one, holding no other box's aim point;
//   - linked as the climb-in is (aliens only): the floor -> the closer (one way), the closer -> the entries over it (one way), closer <-> closer (both ways).
// A stretch under kAutoPlotCloserLeast next to the climb-in is taken by the climb-in, stretched along the wall (its aim point stays); elsewhere it is left and named.
// THE DOORWAY SIDES (Drake 11-21; PROC11's alien on its shut lift door's face, 897 mm beside the exit box, in no box): a doorway's span along its side is kept clear, its
// exit box's (600 each side of the doorway's middle) and 451 more each side: an alien centred in the exit box is held by it alone (the body's corners are exactly 450 out),
// and the 451 left is under a body's width, so a body beside the doorway is always in one box or the other. No closer holds an exit's point either.
#pragma once

#include <stdint.h>

#include <utility>
#include <vector>

namespace host
{
	struct AutoPlotPlan;   // autoplot_plan.h

	int32_t const kAutoPlotCloserMost = 2400;    // a closer's most, along the wall and in height (the climb-in's own length: well over the 900 body, never near it)
	int32_t const kAutoPlotCloserLeast = 900;    // a closer's least, every way (the alien's body: a smaller box could sit wholly inside a body)
	int32_t const kAutoPlotCloserDepth = 900;    // a closer reaches this far out from the wall's face (a wall-hanging alien's body)

	// THE SWITCH ([Editor] AutoPlotClosers=1 in the ini, read at start; anything else is off): the closers are a scored candidate, off until a batch says otherwise
	bool AutoPlotClosers_On();
	void AutoPlotClosers_SetOn(bool on);
	void AutoPlotClosers_Load(char const *settingsFile);

	// The fewest pieces of at most "most" that fill from..to, split evenly; none when the stretch is under "least" (the dialog may pass its own; the planner clamps them:
	// least never under kAutoPlotCloserLeast, the body, and most never under least)
	std::vector<std::pair<double, double> > AutoPlot_CloserPieces(double from, double to, double most = kAutoPlotCloserMost, double least = kAutoPlotCloserLeast);

	// The stretches of from..to left free by the blocked ones (each a from, to pair, in any order, overlapping or not), in order
	std::vector<std::pair<double, double> > AutoPlot_CloserFree(double from, double to, std::vector<std::pair<double, double> > const &blocked);

	// THE BODY AT ANOTHER BOX'S POINT (the closers' rounds on Hudson's runs, Vasquez 11-21; Bishop: "a closer must not reach into the body of an alien standing at ANY
	// other box's point within reach, above it (an entry's aim) or below it (a floor's point), whatever that box's role"). An alien whose body is in two boxes is sent by
	// the game to its route's start box's point (bh_waypoint.cpp 169-189). Where it ends up there, its body is in that box; if a closer reaches into that body too, it is
	// held by two again and sent to the same point, so it never moves on. Two places it ends up:
	//   AT THE POINT: its body 450 each way across, from a body over the point (standing) to a body under it (hanging: on a ceiling its position is 900 under it).
	//     opscen01's alien a12 hung 398 frames at entry 4's point, the entry 651 mm deep with a closer right under it (rounds 1-6: 848 such frames);
	//   AT THE POINT'S FOOT on the closer's wall: an alien on the wall crawls to the place straight across from the point (the point's height, its place along the wall)
	//     and stops there, the way to the point being straight out from the wall. med18's alien a2 stood 88 s across from the floor's point, its body there 151 mm into
	//     the two closers over the floor box (batch 18). Counted when the body at the foot is in that box too: only then can the game send an alien there to its point.
	// So, for every other box's FINAL point but another closer's (the planner keeps every centre in its own box alone, and closers along a wall meet end to end), a
	// closer that reaches into either body, or the kAutoPlotCloserClear round it, has its top lowered under it (the point over its middle) or its bottom raised over it
	// (under its middle). The slot a move leaves, between the closer and the box it met, stays shorter than a body (a body's corners could straddle a taller one, in
	// neither box): the margin is cut to keep it so, and the side is still done when the body itself is clear; else, and when the closer would be left under a body's
	// height (the 0.9 m least: both sides, else the top alone, else the bottom alone), it is left as it is and named. The wall's way is read from the closer itself (its
	// side kAutoPlotCloserDepth + kAutoPlotInto across; the plan keeps no wall); a closer with no such side has no foot looked at, and is counted in the note.
	// It runs on the plan as written, after Drake's repair, which may move a centre (autoplot_volume_repair.h, V2): the planner can't know the final points. Says it in the
	// plan's notes; gives back how many closers it moved. Only "closer" boxes change, only up and down (with the switch off there are none: the plot is as before)
	int32_t const kAutoPlotCloserClear = 100;    // the room a closer leaves round the body at another box's point (Bishop 11-21: the alien is never exactly at the point)
	int AutoPlot_CloserClearAims(AutoPlotPlan *plan, double scale);
}
