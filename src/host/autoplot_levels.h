// autoplot_levels.h: THE LEVELS, the auto-plot's rule for the AI modules the volume planner (autoplot_volumes.h) finds to be "stairs or levels" (Bishop 11-20, the owner's
// "we need to get our auto-plot waypoint system to work"; the seam agreed with Dietrich 11-20: plain boxes and their kinds, and the links as triples; his planner builds
// its volumes from them, and his EXIT rule makes each exit's box). Pure: no level, no game. Written 2026-11-20 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULE, from Rebellion's own networks of such modules (Genshd1's MAIN03, AI module 273: a floor and a ring 2 m over it, 20 room-sized boxes, each floor to ceiling,
// PLAIN within a level and ALIENS ONLY between levels; strwll01, 149: storeys over one another, joined by climbing boxes), in the owner's volume shape:
//   - the floor cells (the planner's 600 mm grid, each with its floor's height; several over one place for storeys) made into FLAT PATCHES: neighbours (4 ways) within
//     kAutoPlotLevelFlat of each other in height;
//   - a LEVEL: a rectangle of a patch at least 2 x 2 cells (the biggest first, as many as kAutoPlotLevelsMost); one FLOOR box each, 200 under its floor to 1500 over it
//     (the volume shape's FLOOR);
//   - the cells left (patches one cell wide, the leftovers of patches) joined where neighbours step at most kAutoPlotLevelStep (the grid's own step): a group that meets
//     levels of two heights, or rises more than kAutoPlotLevelRiseAlone by itself, is a STAIR: ONE floor box over all its treads, 200 under the lowest to 1500 over the
//     highest (they are taken together here, before the volume planner's sliver rule would drop them one row at a time); any other group is a FITTING (a pipe run's top,
//     a sill: Genshd1's tubes have them 800 and 1500 over the floor) and gets no box: an alien on it is in the level's FLOOR, which reaches 1500 up;
//   - LINKS, aliens only both ways (the volume shape's rule): two floor boxes whose cells are neighbours, stepping at most kAutoPlotLevelStep there; or more, up to
//     kAutoPlotLevelRiserMost (a riser an alien climbs, as stock's MAIN03): then the lower box reaches up to the upper's so the two overlap. Storeys over one another are
//     never neighbours, so never linked through the ceiling between them;
//   - an EXIT a doorway: its box is a stand-in round the exit point (the volume planner's EXIT rule replaces it), on the floor box under the exit point (the nearest cell
//     within kAutoPlotLevelExitReach of its height), linked both ways;
//   - a floor no exit reaches is left out (the stock dropout).
// NOT YET (the first cut, said in the notes): no CLIMB-IN and no ENTRY, so no ceiling lane through these modules yet; the floor walks them.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autoplot_plan.h"

namespace host
{
	int32_t const kAutoPlotLevelFlat = 100;          // mm: neighbouring cells this near in height are one flat patch
	int32_t const kAutoPlotLevelStep = 450;          // mm: a tread steps at most this from the next (the grid's own step)
	int32_t const kAutoPlotLevelRiseAlone = 900;     // mm: a group of treads rising more than this by itself is a stair even when it meets only one level
	int32_t const kAutoPlotLevelRiserMost = 3900;    // mm: levels whose cells are neighbours this far apart in height are linked over the riser (aliens climb it)
	int32_t const kAutoPlotLevelExitReach = 1500;    // mm: an exit stands on a floor cell this near its height
	int32_t const kAutoPlotLevelExitFar = 2800;      // mm: an exit box's centre at most this far past its doorway's middle (the floor running on past it: BLOCK-01)
	int32_t const kAutoPlotLevelInto = kAutoPlotInto;    // a floor box reaches this far under its floor
	int32_t const kAutoPlotLevelReach = kAutoPlotReach;  // and this far over it
	int32_t const kAutoPlotLevelMinSide = 1200;      // mm: a level's rectangle is at least this across both ways (2 cells at 600, 4 at 300)
	int32_t const kAutoPlotLevelRoomSide = 2400;     // mm: a rectangle under this across either way is a TREAD (its cells for the stairs), not a level; on a crawl floor
	                                                 // (a duct) every rectangle is a level
	size_t const kAutoPlotLevelsMost = 12;           // levels at most (the rest of the cells count as left over)
	size_t const kAutoPlotLevelMostCells = 200000;   // floor cells at most (a hostile level can't make the rule work for ever)

	int const kAutoPlotLevelFloor = 0;   // the box kinds (Dietrich's numbering)
	int const kAutoPlotLevelClimb = 1;
	int const kAutoPlotLevelEntry = 2;
	int const kAutoPlotLevelExit = 3;

	struct AutoPlotLevelBox
	{
		int kind = kAutoPlotLevelFloor;
		double lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };   // world mm, y down
		std::vector<std::vector<double> > centres;          // candidates for its centre, the first preferred
		int exit = -1;                                      // an exit's: which exit
		int floor = -1;                                     // an exit's: the floor box it stands on
	};

	struct AutoPlotLevelLink
	{
		int from = -1, to = -1;     // box numbers in the plan
		int32_t flagsAtFrom = 0;    // the link as the box `from` lists it (kLinkAlien = 4, ...)
		int32_t flagsAtTo = 0;      // and as `to` lists it
	};

	// A LEVEL ROOM (the second cut's seam, Bishop 11-20): one flat level of the module, which the volume planner shapes as it shapes a module (its climb-ins and entries);
	// the stairs and risers link the rooms. A stair is in no room
	struct AutoPlotLevelRoom
	{
		std::vector<int> floors;   // its floor boxes (numbers in the plan)
		std::vector<int> exits;    // the exits standing on them (exit numbers)
		double floorY = 0;         // its floor's height (world mm, y down)
	};

	struct AutoPlotLevelPlan
	{
		bool shaped = false;
		std::vector<AutoPlotLevelBox> boxes;
		std::vector<AutoPlotLevelLink> links;
		std::vector<AutoPlotLevelRoom> rooms;
		std::vector<std::string> notes;
	};

	// The plan for one AI module, from the volume planner's inputs (the floor cells as the grid gives them, before any rule drops a row; the exits and their doorways)
	AutoPlotLevelPlan AutoPlot_PlanLevels(AutoPlotFloor const &floor, std::vector<std::vector<int32_t> > const &exits, std::vector<AutoPlotDoor> const &doors);

	// THE LEVELS AS THE VOLUME PLANNER'S PLAN (Dietrich's seam, 11-20): the levels' boxes and links as AutoPlot_PlanVolumes gives its own (the file's numbers, each
	// centre held by its own box, each exit's box by the volume planner's EXIT rule, the counts); plan.volumes false (with the notes) when the levels' rule can't shape it.
	// kind: the volume planner's kind ("stairs or levels", or "duct" for crawl cells)
	AutoPlotPlan AutoPlot_LevelsVolumes(AutoPlotFloor const &floor, std::vector<std::vector<int32_t> > const &exits, std::vector<AutoPlotDoor> const &doors, double levelScale,
		std::string const &kind);
}
