// autoplot_volume_repair.h: THE VOLUME REPAIR, after the volume planner (autoplot_volumes.h) and before Frost's checker (waypoint_net_check.h). Written 2026-11-20 by
// Melanikus (Drake) for AVP_OpenInteng, on the seam agreed with Dietrich (the planner is his; the refusals are mine: Bishop 11-20, "the 23 refusals"). AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY. The auto-plot writes an AI module's list only when the checker finds nothing wrong with it. On Genshd1's whole level (Dietrich's a8ed1781) 43 modules were refused,
// 34 of them because a box's centre lies inside another box too ("box 8: its centre is in box 6 too"). The game's rule behind that check: an alien heading for a box aims
// at its centre, and it counts as "arrived" once its own box holds the centre (bh_waypt.c 158-207); a centre two boxes hold is a place where the route can't tell which box
// the alien is in. Where a box has room for its centre outside every other box, putting it there costs nothing the planner meant: the box stays as it is.
//
// THE RULES, in this order, each named in the plan's notes (a note that starts "repair:", which the refusal line shows):
//   V1 A BOX WITH NO LINKS THAT HOLDS NO EXIT POINT IS TAKEN OUT. No alien can enter it or leave it, so it is a scrap (the checker would refuse the module: "cannot
//      reach"). The planner should not make one (Dietrich removes them at the source); this is the safety net. The links after it are renumbered, and the plan's lists that
//      run beside the boxes (climbing, volumeRoles) or name boxes (doorBox, exitBox, doorFloors, floorBoxes) follow.
//   V2 A CENTRE IN ANOTHER BOX IS MOVED FLAT (x and z only: its height is the planner's choice, a climb's top or a doorway's middle) inside its own box, to the
//      nearest place no other box holds: along the box's long side first, then across it, then both. A LOW centre (a sloped side's climb: it sits on its face, Drake's
//      hand6g) is never moved, only named; a HIGH's is V3's. A centre with no such place is left where it is and named; the checker then refuses the module, as before.
//   V3 A HIGH'S CENTRE IN ANOTHER BOX (an entry, nearly always: Bishop 11-20, "the entry stops reaching over the HIGH"; seam agreed with Dietrich). The planner puts a
//      HIGH's centre 1 past the floor's edge and kAutoPlotInto over the ceiling, so an alien on the upper face is pulled up onto the ceiling (hand6g). When an entry
//      reaches out past that edge over it: (a) the centre SLIDES out along its face past the entry's end, as far down as the face drops there (the slope read from the
//      box the planner made: from its top + kAutoPlotInto, the ceiling, down to its bottom, the vertex, over the face's reach), so it stays kAutoPlotInto over the face,
//      when that place is in its box and in no other; (b) else a JUMP CATCHER (one link, to that entry) is taken out, as V1 takes a box out: the entry, reaching past
//      its whole face, is the catcher there; (c) else (a HIGH on a climb, LOW -> HIGH -> entry) it is left and named: no wall on that side is the planner's to fix.
//      Cutting the entry back instead was measured and refused: med04's doorway lies 359 mm past its floor's edge, so a ceiling alien arriving there would be in no box.
//   V4 AN EXIT POINT NO BOX HOLDS gets a box round it, as the planner's exit rule makes one (600 each side, from 1000 over the point down to the foot of the floor box
//      nearest it), its centre the point, linked both ways with that floor box. The levels' floor walk, which a room with no wall to climb falls to, leaves out an exit
//      more than its reach over the floor (doormp06 and office01 on the owner's 2nd stretch, e5f6f361: "exit 0 lies in no box").
//   V5 EVERY BOX REACHES BOX 0 AND BOX 0 EVERY BOX (the checker's rule). A part no link joins to box 0's is joined: the nearest two ceiling entries, one each side,
//      overlapping across and in height and at most 1500 mm apart, by the second stretched to meet the first, else any two boxes at most 600 mm apart; linked both ways.
//      Then a box that can't reach box 0, or that box 0 can't reach, gets the links of its shortest way (by links either way) to one that can made both ways: a dead-end
//      floor gets its way back (MED01, the owner's start: two parts, its ceiling entries 1.1 m apart, its north floor reached only one way from an arrival's exit box).
//      Joining changes one box's size and some links' one-way flags: the planner's own shape is kept wherever the checker already accepts it.
// The order: V1, V4, V5 (boxes and links), then V2 and V3 (the centres, so a stretched box can't keep another's centre).
// The test of "holds" is the checker's own (Waypoint_Holds: the game's WaypointContainsPoint on the loaded, scaled numbers), so what this pass fixes the checker accepts.
// A note's box numbers are the list's when that rule starts (V1's before any is taken out; V3's before its catchers go).
//
// What it does NOT do: change a box's size (but V5's stretch) or a height (but V3's slide); take a link out; touch a list the old planner made (plan.volumes false). Pure: no level, no screen.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "autoplot_plan.h"
#include "waypoint_net_check.h"

namespace host
{
	int32_t const kAutoPlotVolumeRepairExitSide = 600;    // V4's exit box: this each side of the exit point (the planner's kAutoPlotVolumeSide)
	int32_t const kAutoPlotVolumeRepairExitOver = 1000;   // V4's exit box: from this over the point (the levels' stand-in's)
	long long const kAutoPlotVolumeRepairJoinGap = 1500;  // V5: two ceiling entries at most this far apart are stretched to meet (MED01's 1121)
	long long const kAutoPlotVolumeRepairTouchGap = 600;  // V5: else two boxes at most this far apart are linked as they are

	struct AutoPlotVolumeRepair
	{
		size_t removed = 0;   // V1 and V3: boxes taken out
		size_t moved = 0;     // V2 and V3: centres moved
		size_t left = 0;      // V2, V3 and V5: centres still in another box (no place, a LOW's, a HIGH's on a climb), a part or box V5 couldn't join
		size_t made = 0;      // V4: exit boxes made
		size_t joined = 0;    // V5: joins and ways made both ways
	};

	// V1, V4, V5, V2, V3 on the plan's list (the file's units). exits: the AI module's exit points (the file's units); scale: the level's (local_scale, 1 for most).
	// A plan the volume planner didn't make is left as it is. What was done goes in plan->notes as one "repair: ..." line (none when nothing was done).
	AutoPlotVolumeRepair AutoPlot_RepairVolumes(AutoPlotPlan *plan, std::vector<WaypointPoint> const &exits, double scale);
}
