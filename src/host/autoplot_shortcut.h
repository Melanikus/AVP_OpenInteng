// autoplot_shortcut.h: THE FLOOR SHORTCUTS of THE LANES (the owner's continuous lanes: an alien on a module's ceiling stays up through the doorway and on along the next
// module's ceiling). Drake's whole-level gate on Dietrich's 5627d9c1 (lanes8): 43 lanes cross their doorway and then break ALONG the next AI module, "box N on the way is a
// floor box": the stock route search takes the fewest boxes (every box weighs 5, bh_waypoint.cpp:687-874), and a way down to the floor and across it had fewer than the
// ceiling's. A pass over the FINISHED plan, after Drake's repair (autoplot_repair.h), that changes links only (no box added, moved or taken out). Pure: no level, no game.
// Written 2026-11-20 by Melanikus (Vasquez) for AVP_OpenInteng, the group Dietrich gave (he owns autoplot_plan.cpp and autoplot_lane.cpp). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHAT BREAKS (24 of the 28 such lanes outside the modules whose roof the planner and the polygons disagree on, Dietrich's list):
//   - 12: the target exit's door box has no link to the box that holds the target exit's point, so the ceiling can't reach it at all; in 7 of them the way goes through
//     ANOTHER exit's box (its door box down to its exit box, then across the floor to the target's);
//   - 12: the link is there, but a step down a wall to the floor and across is shorter than the ceiling's way.
// A STEP STOPPED: box V may no longer step to box W (W lists V with One way, V's link to W One way the other direction: inteng's own one-way link, Add Link's "only W to V";
// the stock search never takes it, bh_waypoint.cpp:767). W may still step to V. Stopped both ways, the two are no longer joined (Delete Link).
// THE RULES:
//   (rules 1 and 2 not at a HANGING-BAND exit, Dietrich's: its point less than a hanging alien's 900 under its doorway's top; at Genshd1's start exit 0 -> top, 779
//   under, they made 4 back places)
//   1. each lane exit's door box joined to its exit's target box (the last box that holds the exit point, a floor box), aliens only, both ways, when they are not: the
//      planner's own join at its other exits. Only at an exit whose doorway has a CLIMB STATION: an alien in the exit box then heads for the door box's centre over a
//      wall's top and climbs it; with none it would head straight up and stall (Drake's gate counted 10 more stall and back places in 3 AI modules when joins were made
//      at every exit);
//   2. AN EXIT'S BOX IS AN END (Dietrich's 225d68bb has it for the parts): that floor box stopped from stepping on to any other floor box; it may still step up to its door
//      box, and floor boxes may still step into it. At every exit whose box steps up to its door box (joined before, or by rule 1): it only stops steps and adds no way
//      up, so it makes no new stall (Dietrich 11-20: 11 of the 16 floor-route ends left were the arriving alien stepping down into its own exit's box and on);
//   3. for each lane along the plan that still breaks on a floor box (from an exit's door box toward each other exit, as the planner measures it: Lane_Through), a chain of
//      at most kAutoPlotShortcutChain steps stopped, each the step DOWN from the climbing box into the first floor box on the lane's walk (not the target's): the floor
//      box may still step UP, so an alien standing there still climbs.
// THE LANES MEASURED: from each exit's door box toward each other exit, on link shift 0's route as the planner measures them (Lane_Through); and the ARRIVAL LANES, as
// Drake's gate measures them: the alien carried through each exit's doorway, from the box the stock choice starts from 600 past the doorway's middle, toward each other
// exit, on EVERY link shift's route (Lane_ThroughAll, Dietrich 11-20: the game gives each alien its own link shift, so a tie with a floor way is a break; rule 3's chain
// then stops each floor branch in turn). Door lanes stay on shift 0: walked on every shift they chased ties the gate does not count as lane ends, and the cuts made stall
// and back places (Genshd1 11-20: RAMP-01, office, shft11).
// Rules 1 and 2 are tried for every exit at once, then (when that is not kept) exit by exit; then rule 3 lane by lane, in rounds while one is kept. Each try is KEPT only
// when, over the whole plan: fewer lanes break than before; nothing breaks that did not (a lane, or any climbing box's walk toward an exit); and Frost's checker
// (Waypoints_Check) finds no more problems. Otherwise it is undone. At most kAutoPlotShortcutMostTries tries are judged in an AI module. NOT GUARDED: stall and back
// places (the pass has no walls or modules; Drake's check is to come as a callback), so a kept try may add some: on Genshd1 11-20 this pass took lane ends under a
// ceiling 7 -> 5 for 12 more stall or back places (Dietrich's call, Bishop's ranking: an end, a drop, before a stall, a hesitation).
// It reads where a lane breaks from Lane_Through's and Lane_ThroughAll's own words ("box N on the way is a floor box (the route a b ... N, the target's box T)", and
// "; link shift s"): if those words change, this pass does nothing (and its unit test, which calls the real ones, fails).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autoplot_plan.h"
#include "waypoint_model.h"
#include "waypoint_net_check.h"   // WaypointPoint: an exit point as Frost's checker takes it

namespace host
{
	size_t const kAutoPlotShortcutChain = 4;        // the most changes in one lane's chain
	size_t const kAutoPlotShortcutMostTries = 64;   // the most chains judged in one AI module (each costs one look at all its lanes)
	size_t const kAutoPlotShortcutRounds = 4;

	struct AutoPlotShortcutCounts
	{
		size_t lanes = 0;                   // the lanes along the plan looked at (each exit's door box toward each other exit)
		size_t continuousBefore = 0, continuousAfter = 0;
		size_t joined = 0;                  // door boxes joined to their exit's target box
		size_t madeOneWay = 0;              // steps stopped (a link made one-way, or taken out when stopped both ways)
		size_t tried = 0, kept = 0;         // tries judged, tries kept
	};

	// Where a lane along a module breaks on a floor box, from Lane_Through's words: the route (box numbers, from the start to the floor box) and the target's box. False
	// when the words are not that break
	bool AutoPlot_ShortcutWords(std::string const &why, std::vector<int32_t> *route, int32_t *target);

	// The pass over a finished plan (the command's job, right after AutoPlot_RepairPlan): `exits` the world's mm (as Lane_Through takes them), `exitsFile` the file's units
	// (as Frost's checker takes them), `doors` each exit's doorway (rules 1 and 2 only where it has a climb station), `scale` the level's. `exitEnds`: rules 1 and 2 made
	// too (Dietrich 11-20: rule 3 alone first, "nothing gets worse, which is the bar"; at start exit 0 -> top rules 1 and 2 made 4 back places in Drake's gate).
	// Changes only plan->list's links, and adds a note to plan->notes when it kept anything
	AutoPlotShortcutCounts AutoPlot_CutShortcuts(AutoPlotPlan *plan, std::vector<std::vector<int32_t> > const &exits, std::vector<WaypointPoint> const &exitsFile,
		std::vector<AutoPlotDoor> const &doors, double scale, bool exitEnds = false);
}
