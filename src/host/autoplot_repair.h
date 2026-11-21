// autoplot_repair.h: THE REPAIR of a planned alien list before it is written (Bishop 11-20: "every refused module is a hole in the owner's whole level"; the auto-plot's
// refusals are Drake's, the lane rules Dietrich's). The auto-plot writes a list only when Frost's checker (waypoint_net_check.h Waypoints_Check) finds nothing; on Genshd1's
// whole level 41 AI modules were refused, for three kinds of problem the planner leaves. This pass takes the checker's own problems and fixes them by rule, until the checker
// finds none of them or nothing more can be done. Pure: a list in the file's units and the module's exit points in, the list changed in place. Written 2026-11-20 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES (each one's case on Genshd1, 69c0f1a0's whole run):
//   1. A PART THAT NO EXIT REACHES IS TAKEN OUT (12 of the 14 "cannot reach" networks: floor patches 6 to 55 m off, a raised platform or a roof no doorway leads to).
//      The parts are the network's boxes joined by links (either way). No route leads into such a part, so no alien that enters the module through a doorway uses it.
//      A part holding an exit is never taken out by this rule (rule 6 is the one for those).
//   2. A PART WITHIN kAutoPlotRepairJoinGap OF ANOTHER IS LINKED to it (186: 0.55 m off): its nearest box and theirs, both ways, aliens only (0x4) when either box is a
//      climbing one (all its links aliens only), else plain.
//   3. TWO BOXES THAT HOLD EACH OTHER'S CENTRES ARE MADE ONE (7 networks: two exits' door boxes at one doorway, 390 / 391 at the very same centre): their union, with the
//      first one's centre, the links of both.
//   4. A CLIMBING BOX'S CENTRE IN ANOTHER BOX: the other box is TRIMMED back on the face nearest that centre, just past it (8 networks: a wall box's centre in a tall floor
//      box), unless that would take the other box's own centre, an exit point it holds, or its room.
//   5. ANY OTHER CENTRE IN ANOTHER BOX IS MOVED within its own box to the nearest place that is in no other box, at its own height (9 networks: a merged floor rectangle's
//      centre inside a wall or station box); a box where no such place is found is trimmed as in 4, else made one with the other as in 3.
//   6. TWO AREAS EACH HOLDING AN EXIT, FURTHER APART THAN THE JOIN GAP (295 shft10: two shaft ends 9.3 m apart; 364 air06: an exit's stretch of duct 8 m off), Bishop's
//      ruling 11-20 ("refused modules are holes in his whole level"; the owner's objective, aliens crawling on): (b) they are JOINED by one aliens-only link (both ways)
//      where a climbable surface actually connects them: the grid's own alien way (floor is lava) between the two parts' nearest boxes (at most kAutoPlotRepairMostWays
//      pairs, nearest first), every point of it outside the list's boxes on a wall or a ceiling of one of this AI module's own game modules. Otherwise (a) the area with
//      fewer exits (then fewer boxes) is TAKEN OUT and the one with more kept; its exit points are then in no box, which the checker refuses ("exit N is in no box"),
//      and that is said, never hidden. Never left as two parts. With no way asker given (the tests, a tool with no grid) no surface is known: (a).
//   Anything else the checker finds (too many boxes, no room, a link outside the list...) is left as it is and said: the planner's to fix, never hidden.
// HOSTILE INPUT: at most kAutoPlotRepairMostRounds rounds, each one problem; a list over the checker's 99 boxes or with links outside it is left as it is.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "autoplot_grid.h"
#include "autoplot_plan.h"
#include "waypoint_model.h"
#include "waypoint_net_check.h"

namespace host
{
	int32_t const kAutoPlotRepairJoinGap = 1000;       // the file's units (millimetres at scale 1): a part this near another is linked to it
	int const kAutoPlotRepairMostRounds = 200;
	size_t const kAutoPlotRepairMostWays = 4;            // rule 6: the pairs of boxes (nearest first) a far area asks the grid a way for

	// THE GRID'S ALIEN WAY, for rule 6: from one place to another (the world's millimetres, y down), the alien search's points (AutoPlotGrid_Way with alien set). False: no
	// way. The editor's: the grid's own; the tests': made ways
	using AutoPlotRepairWay = std::function<bool(int32_t const from[3], int32_t const to[3], std::vector<AutoPlotPoint> *way)>;

	struct AutoPlotRepairCounts
	{
		size_t partsTakenOut = 0, boxesTakenOut = 0;   // rule 1
		size_t partsLinked = 0;                         // rule 2
		size_t madeOne = 0;                             // rules 3 and 5
		size_t trimmed = 0;                             // rules 4 and 5
		size_t centresMoved = 0;                        // rule 5
		size_t areasJoined = 0;                         // rule 6 (b)
		size_t areasTakenOut = 0, exitsLeftOut = 0;     // rule 6 (a): the areas taken out, and the exit points they held
		std::vector<int32_t> leftOutExits;              // rule 6 (a): those exits' numbers (their places in `exits`), in order
		size_t left = 0;                                // the checker's problems still there after the pass
	};

	// The pass: `list` changed in place (boxes taken out or made one renumber the rest, and every link follows). `exits`: the module's exit points, the file's units, as the
	// checker takes them. Gives what it did; *words (when given) gets one note ("repair: ...") when it did anything or left a problem, else stays as it was. *renumber
	// (when given): for each box of the list as it came, its place in the list as it goes (the one it was made one with, or -1 when it was taken out). alienWay and
	// modules (this AI module's game modules, MODULE::m_index): rule 6's grid; none given, rule 6 takes the area with fewer exits out.
	AutoPlotRepairCounts AutoPlot_Repair(std::vector<WaypointRec> *list, std::vector<WaypointPoint> const &exits, double scale, std::string *words = nullptr,
		std::vector<int32_t> *renumber = nullptr, AutoPlotRepairWay const &alienWay = AutoPlotRepairWay(), std::vector<int32_t> const &modules = std::vector<int32_t>());

	// The same on a plan (the auto-plot's hook, on the worker just after the plan is made, so the report's lines and the checker see the list as it will be written:
	// autoplot_command.cpp): plan->list repaired, and every field that numbers its boxes made to follow (climbing, doorBox, exitBox, doorFloors; floorBoxes and parts
	// counted again), the note added to plan->notes
	AutoPlotRepairCounts AutoPlot_RepairPlan(AutoPlotPlan *plan, std::vector<WaypointPoint> const &exits, double scale, AutoPlotRepairWay const &alienWay = AutoPlotRepairWay(),
		std::vector<int32_t> const &modules = std::vector<int32_t>());

	// THE CHECKER'S EXCEPTIONS BY RULE 6 (a) (Bishop 11-20, "YES, write them, on conditions"): an exit rule 6 (a) left out has no alien way to the rest of its AI module, so
	// the stock game handles it as it is (an alien heading for it drops its waypoints: D4), and the module is written all the same. Of `problems` (Frost's checker's, which
	// is not changed), each "exit in no box" for an exit in `leftOutExits` (plan.exitsLeftOut) is moved to *excepted, to be printed with its reason and counted; every
	// other problem stays. Gives how many were moved.
	size_t AutoPlot_RepairExceptions(std::vector<WaypointProblem> *problems, std::vector<int32_t> const &leftOutExits, std::vector<WaypointProblem> *excepted);
}
