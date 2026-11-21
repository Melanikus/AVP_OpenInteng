// autoplot_stairs.h: THE STAIR BETWEEN TWO FLOORS, for the volume planner (autoplot_volumes.h). Pure: no level, no game. Written 2026-11-21 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: Genshd1's opscen03 and opscen02, the owner's own ceiling-lane POC (chain A: opscen03 - Ops01 - opscen02), were refused by the auto-plot, "box 0 cannot reach box 1".
// Each is a room on two floors 1.95 m apart, one doorway on each floor, joined by four treads 390 mm high and 780 mm deep. The volume planner makes one floor box for each
// flat patch big enough to be a floor; a tread is a flat patch of its own, too small (2 rows of cells: a "fitting") or too narrow (3 rows, 900 mm: a "sliver") to be one,
// so its cells got no box, and the plan was two networks that never meet, one at each doorway. Drake's hand-built chain (hand4, make-spec4.js; the owner's in-game PASS)
// joins the two halves with floor boxes over the treads, linked to both floors, and one ceiling box over them, linked to the upper ceiling and the lower one.
//
// THE RULE: the floor cells no floor rectangle holds are grouped, neighbours (4 ways) whose floors step at most kAutoPlotLevelStep (450, a tread the grid walks: the levels'
// rule's own step, autoplot_levels.h). A group that meets two floor rectangles whose floors are more than kAutoPlotLevelStep apart (a cell of the group beside a cell of each
// rectangle, within that step of it) is a STAIR between them. Every other group (a pipe's top, a sill, a ledge that meets one floor) is left as it was.
// Each stair gives what the volume planner needs for its boxes: the cells' range, the highest tread's floor and the lowest's, and the ceiling over the treads. A place's
// "ceiling" less than kAutoPlotStairRoofLeast over its floor is not counted: at a tread's edge the grid gives two cells, and the lower one's "ceiling" is the tread over it.

#pragma once

#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

namespace host
{
	double const kAutoPlotStairRoofLeast = 1000.0;     // mm: a ceiling nearer its place's floor than this is a tread's edge over it, not a ceiling
	double const kAutoPlotStairUnderCeiling = 1000.0;  // mm: a stair's floor box keeps its top this far under the lowest ceiling over it (a hanging alien never touches it)
	double const kAutoPlotStairBody = 900.0;           // mm: a stair's floor box reaches at least this over its highest tread (an alien on it is inside), or there is no box

	// a floor rectangle as the volume planner has it: its cells' range (the planner's grid indices) and its floor's height (the highest, y down)
	struct AutoPlotStairFloor
	{
		int ix0 = 0, ix1 = 0, iz0 = 0, iz1 = 0;
		double floorTop = 0.0;
	};

	struct AutoPlotStair
	{
		int ix0 = 0, ix1 = 0, iz0 = 0, iz1 = 0;   // the treads' cells: the range that holds them all
		double floorTop = 0.0, floorLow = 0.0;    // the highest tread's floor and the lowest's (y down: floorTop <= floorLow)
		bool ceiled = false;                      // a ceiling over at least one of its cells (kAutoPlotStairRoofLeast or more over the floor there)
		double ceilTop = 0.0, ceilLow = 0.0;      // the highest of those ceilings and the lowest (y down)
		std::vector<int> joins;                   // the floor rectangles it meets, by their place in the list given, in that order
	};

	// The stairs between the floor rectangles. cells: each cell's floor (the higher where there are two), by its grid indices; ceilings: the ceiling over each (the lower
	// where there are two), by the same indices, none where there is none
	std::vector<AutoPlotStair> AutoPlot_Stairs(std::map<std::pair<int, int>, double> const &cells, std::map<std::pair<int, int>, double> const &ceilings,
		std::vector<AutoPlotStairFloor> const &floors);
}
