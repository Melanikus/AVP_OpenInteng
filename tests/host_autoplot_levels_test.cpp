// host_autoplot_levels_test.cpp: THE LEVELS (autoplot_levels.h; Bishop 11-20, the owner's "get our auto-plot waypoint system to work": the AI modules the volume planner
// leaves as "stairs or levels"). Made-up floors on the planner's 600 mm grid: a split level joined by four treads (Genshd1's opscen02 / opscen03 and MAIN03), a pipe run's
// top (a fitting, not a level), a platform with no stairs (its riser climbed, aliens only, as stock's MAIN03 links its levels), two storeys over one another with no stair
// (never linked through the ceiling), and no cells at all. Written 2026-11-20 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_levels.h"
#include "autoplot_volumes.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

#define CHECK(condition, words) \
	do \
	{ \
		bool const checked = (condition); \
		Check(checked, (words)); \
	} while (0)

// floor cells ix0..ix1 x iz0..iz1 at height y (world mm, y down; a cell's middle at ix * edge, iz * edge: 600 the marine grid's, 300 Drake's crawl cells)
static void Cells(host::AutoPlotFloor *floor, int ix0, int ix1, int iz0, int iz1, int y, int edge = 600)
{
	for (int ix = ix0; ix <= ix1; ix++)
	{
		for (int iz = iz0; iz <= iz1; iz++)
		{
			floor->cells.push_back(std::vector<int32_t>{ ix * edge, y, iz * edge });
		}
	}
}

static int Count(host::AutoPlotLevelPlan const &plan, int kind)
{
	int n = 0;

	for (host::AutoPlotLevelBox const &b : plan.boxes)
	{
		n += b.kind == kind ? 1 : 0;
	}

	return n;
}

static bool Linked(host::AutoPlotLevelPlan const &plan, int a, int b)
{
	for (host::AutoPlotLevelLink const &l : plan.links)
	{
		if ((l.from == a && l.to == b) || (l.from == b && l.to == a))
		{
			return true;
		}
	}

	return false;
}

// the floor box (a level or a stair) whose floor is nearest y over the cell ix, iz
static int FloorAt(host::AutoPlotLevelPlan const &plan, int ix, int iz, double y)
{
	int best = -1;
	double bestGap = 1e300;

	for (size_t i = 0; i < plan.boxes.size(); i++)
	{
		host::AutoPlotLevelBox const &b = plan.boxes[i];
		double const x = ix * 600.0, z = iz * 600.0;

		if (b.kind != host::kAutoPlotLevelFloor || x < b.lo[0] || x > b.hi[0] || z < b.lo[2] || z > b.hi[2] || y < b.lo[1] || y > b.hi[1])
		{
			continue;
		}

		double const gap = fabs(b.hi[1] - host::kAutoPlotLevelInto - y);

		if (gap < bestGap)
		{
			bestGap = gap;
			best = (int)i;
		}
	}

	return best;
}

static std::string Notes(host::AutoPlotLevelPlan const &plan)
{
	std::string out;

	for (std::string const &n : plan.notes)
	{
		out += (out.empty() ? "" : "; ") + n;
	}

	return out;
}

int main()
{
	host::AutoPlotLevelPlan plan;

	// 1. A SPLIT LEVEL (opscen02 / opscen03, MAIN03): the lower floor 8 x 4 cells at y 0 (ix 0..7), four treads one cell wide stepping 400 up (ix 8..11), the upper floor
	// 4 x 4 cells at y -2000 (ix 12..15); a doorway at each end
	{
		puts("a split level joined by four treads:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 600;
		Cells(&floor, 0, 7, 0, 3, 0);

		for (int t = 0; t < 4; t++)
		{
			Cells(&floor, 8 + t, 8 + t, 0, 3, -400 * (t + 1));
		}

		Cells(&floor, 12, 15, 0, 3, -2000);

		std::vector<std::vector<int32_t> > const exits = { { -400, -10, 900 }, { 9400, -2010, 900 } };
		std::vector<host::AutoPlotDoor> doors(2);

		plan = host::AutoPlot_PlanLevels(floor, exits, doors);
		CHECK(plan.shaped, "shaped (" + Notes(plan) + ")");
		CHECK(Count(plan, host::kAutoPlotLevelFloor) == 3, "3 floors: the lower level, the stair and the upper level (" + std::to_string(Count(plan, host::kAutoPlotLevelFloor)) + ")");
		CHECK(Count(plan, host::kAutoPlotLevelExit) == 2, "an exit a doorway (" + std::to_string(Count(plan, host::kAutoPlotLevelExit)) + ")");

		int const lower = FloorAt(plan, 3, 1, 0), stair = FloorAt(plan, 9, 1, -800), upper = FloorAt(plan, 13, 1, -2000);

		CHECK(lower >= 0 && upper >= 0 && stair >= 0 && lower != stair && stair != upper && lower != upper, "the lower level, the treads and the upper level are three boxes");
		CHECK(stair >= 0 && plan.boxes[(size_t)stair].lo[1] <= -1600 - 1500 + 1 && plan.boxes[(size_t)stair].hi[1] >= -400 + 200 - 1,
			"the stair reaches 1500 over its top tread and 200 under its lowest");
		CHECK(lower >= 0 && stair >= 0 && Linked(plan, lower, stair) && stair >= 0 && upper >= 0 && Linked(plan, stair, upper), "the lower level, the stair and the upper level are linked in a row");
		CHECK(!(lower >= 0 && upper >= 0 && Linked(plan, lower, upper)), "the lower and upper levels go by the stair, not straight");

		bool allAlien = !plan.links.empty();

		for (host::AutoPlotLevelLink const &l : plan.links)
		{
			allAlien = allAlien && (l.flagsAtFrom & 4) && (l.flagsAtTo & 4);
		}

		CHECK(allAlien, "every link aliens only, both ways (the volume shape's rule)");

		int exitLower = -1, exitUpper = -1;

		for (size_t i = 0; i < plan.boxes.size(); i++)
		{
			exitLower = plan.boxes[i].kind == host::kAutoPlotLevelExit && plan.boxes[i].exit == 0 ? (int)i : exitLower;
			exitUpper = plan.boxes[i].kind == host::kAutoPlotLevelExit && plan.boxes[i].exit == 1 ? (int)i : exitUpper;
		}

		CHECK(exitLower >= 0 && plan.boxes[(size_t)exitLower].floor == lower && Linked(plan, exitLower, lower), "exit 0 stands on the lower level, linked to it");
		CHECK(exitUpper >= 0 && plan.boxes[(size_t)exitUpper].floor == upper && Linked(plan, exitUpper, upper), "exit 1 on the upper level, linked to it");

		// THE LEVEL ROOMS (the second cut's seam, Bishop 11-20: the volume planner shapes each room as it shapes a module, its climb-ins and entries; the stairs link them):
		// one a flat level, its floor boxes and the exits standing on them; a stair is in no room
		CHECK(plan.rooms.size() == 2, "two level rooms, the lower and the upper (" + std::to_string(plan.rooms.size()) + ")");

		int lowerRoom = -1, upperRoom = -1;

		for (size_t r = 0; r < plan.rooms.size(); r++)
		{
			for (int const f : plan.rooms[r].floors)
			{
				lowerRoom = f == lower ? (int)r : lowerRoom;
				upperRoom = f == upper ? (int)r : upperRoom;
				CHECK(f != stair, "the stair is in no room");
			}
		}

		CHECK(lowerRoom >= 0 && upperRoom >= 0 && lowerRoom != upperRoom, "the lower and upper levels are rooms of their own");
		CHECK(lowerRoom >= 0 && plan.rooms[(size_t)lowerRoom].exits == std::vector<int>{ 0 } && upperRoom >= 0 && plan.rooms[(size_t)upperRoom].exits == std::vector<int>{ 1 },
			"each room has the exit standing on it");
		CHECK(lowerRoom >= 0 && fabs(plan.rooms[(size_t)lowerRoom].floorY - 0.0) < 1.0 && upperRoom >= 0 && fabs(plan.rooms[(size_t)upperRoom].floorY + 2000.0) < 1.0,
			"each room's floor height");
	}

	// 2. A FITTING: a floor 8 x 8 cells at y 0 and a pipe run's top, a row one cell wide 1169 over it, joining nothing
	{
		puts("a pipe run's top is a fitting:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 600;
		Cells(&floor, 0, 7, 0, 7, 0);
		Cells(&floor, 9, 9, 1, 6, -1169);

		std::vector<std::vector<int32_t> > const exits = { { -400, -10, 2100 } };
		std::vector<host::AutoPlotDoor> doors(1);

		plan = host::AutoPlot_PlanLevels(floor, exits, doors);
		CHECK(plan.shaped && Count(plan, host::kAutoPlotLevelFloor) == 1, "one floor: the pipe's top is no level (" + std::to_string(Count(plan, host::kAutoPlotLevelFloor)) + "; " + Notes(plan) + ")");
	}

	// 3. A PLATFORM with no stairs: a floor 6 x 6 at y 0 and beside it a platform 4 x 4 at y -1200: its riser climbed (aliens only), as stock links MAIN03's levels
	{
		puts("a platform with no stairs:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 600;
		Cells(&floor, 0, 5, 0, 5, 0);
		Cells(&floor, 6, 9, 1, 4, -1200);

		std::vector<std::vector<int32_t> > const exits = { { -400, -10, 1800 } };
		std::vector<host::AutoPlotDoor> doors(1);

		plan = host::AutoPlot_PlanLevels(floor, exits, doors);

		int const low = FloorAt(plan, 2, 2, 0), high = FloorAt(plan, 7, 2, -1200);

		CHECK(plan.shaped && Count(plan, host::kAutoPlotLevelFloor) == 2, "two floors (" + std::to_string(Count(plan, host::kAutoPlotLevelFloor)) + "; " + Notes(plan) + ")");
		CHECK(low >= 0 && high >= 0 && low != high && Linked(plan, low, high), "linked across the riser");
	}

	// 4. TWO STOREYS over one another with no stair: never linked through the ceiling between them
	{
		puts("two storeys with no stair:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 600;
		Cells(&floor, 0, 5, 0, 5, 0);
		Cells(&floor, 0, 5, 0, 5, -4000);

		std::vector<std::vector<int32_t> > const exits = { { -400, -10, 1800 }, { -400, -4010, 1800 } };
		std::vector<host::AutoPlotDoor> doors(2);

		plan = host::AutoPlot_PlanLevels(floor, exits, doors);

		int const ground = FloorAt(plan, 2, 2, 0), above = FloorAt(plan, 2, 2, -4000);

		CHECK(ground >= 0 && above >= 0 && ground != above, "both storeys have a floor (" + Notes(plan) + ")");
		CHECK(!(ground >= 0 && above >= 0 && Linked(plan, ground, above)), "and they are not linked");
	}

	// 6. A DUCT on Drake's crawl cells (AutoPlotGrid_CrawlCells, 300 mm: Genshd1's shft10 / 12 / 18 are 1.9 m tall, no cell a marine stands on): an L, 1.8 m wide, one
	// arm 7.8 m along x, the other 6 m along z, a doorway at each end; floors that make the L, linked, and both exits on them
	{
		puts("a duct on crawl cells:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 300;
		Cells(&floor, 0, 25, 0, 5, 0, 300);
		Cells(&floor, 20, 25, 6, 25, 0, 300);
		floor.crawl = true;   // Drake's crawl cells: every rectangle a level, however narrow (a duct is 1.8 m wide)

		std::vector<std::vector<int32_t> > const exits = { { -200, -10, 750 }, { 6750, -10, 7700 } };
		std::vector<host::AutoPlotDoor> doors(2);

		plan = host::AutoPlot_PlanLevels(floor, exits, doors);

		int const west = FloorAt(plan, 0, 1, 0) >= 0 ? 0 : -1;
		CHECK(plan.shaped && Count(plan, host::kAutoPlotLevelExit) == 2, "shaped, both exits placed (" + Notes(plan) + ")");

		// every floor box reached from exit 0's floor: the duct is one walk end to end
		std::vector<bool> seen(plan.boxes.size(), false);
		std::vector<int> todo;

		for (size_t i = 0; i < plan.boxes.size(); i++)
		{
			if (plan.boxes[i].kind == host::kAutoPlotLevelExit && plan.boxes[i].exit == 0)
			{
				seen[i] = true;
				todo.push_back((int)i);
			}
		}

		while (!todo.empty())
		{
			int const n = todo.back();

			todo.pop_back();

			for (host::AutoPlotLevelLink const &l : plan.links)
			{
				int const o = l.from == n ? l.to : l.to == n ? l.from : -1;

				if (o >= 0 && !seen[(size_t)o])
				{
					seen[(size_t)o] = true;
					todo.push_back(o);
				}
			}
		}

		bool all = !plan.boxes.empty();

		for (bool const s : seen)
		{
			all = all && s;
		}

		CHECK(west == 0 && all, "from exit 0 every box is reached, the other exit too (" + std::to_string(Count(plan, host::kAutoPlotLevelFloor)) + " floors)");
		CHECK(Count(plan, host::kAutoPlotLevelFloor) <= 3, "a few room-sized floors, not many small ones (" + std::to_string(Count(plan, host::kAutoPlotLevelFloor)) + ")");
	}

	// 7. THROUGH THE VOLUME PLANNER (Dietrich's seam, 11-20): a split level whose treads are 1.2 x 2.4 m (Genshd1's grid is 300 mm: real treads are flat patches of
	// their own, as STEPS04's 6 m2 ones), ceilings 3.9 m over every cell, a doorway at each end: the volume planner finds "stairs or levels" and gives it the levels'
	// plan: 3 floors (the lower level, the treads as ONE stair, the upper level) and 2 exit volumes by its EXIT rule, one network, both exit points held
	{
		puts("a split level through the volume planner:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 600;
		Cells(&floor, 0, 7, 0, 3, 0);

		for (int t = 0; t < 4; t++)
		{
			Cells(&floor, 8 + 2 * t, 9 + 2 * t, 0, 3, -400 * (t + 1));
		}

		Cells(&floor, 16, 19, 0, 3, -2000);

		for (std::vector<int32_t> const &c : floor.cells)
		{
			floor.ceilings.push_back(std::vector<int32_t>{ c[0], c[1] - 3900, c[2] });
		}

		std::vector<std::vector<int32_t> > const exits = { { -400, -10, 900 }, { 11800, -2010, 900 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].throughFound = true, doors[0].middle[0] = -300, doors[0].middle[1] = -10, doors[0].middle[2] = 900, doors[0].through[0] = -1.0;
		doors[1].throughFound = true, doors[1].middle[0] = 11700, doors[1].middle[1] = -2010, doors[1].middle[2] = 900, doors[1].through[0] = 1.0;

		host::AutoPlotPlan const vp = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		std::string notes;

		for (std::string const &n : vp.notes)
		{
			notes += (notes.empty() ? "" : "; ") + n;
		}

		CHECK(vp.volumes && vp.volumeKind == "stairs or levels", "shaped as stairs or levels (" + vp.volumeKind + ": " + notes + ")");
		CHECK(vp.floorBoxes == 3 && vp.list.size() == 5, "3 floors and 2 exit volumes (" + std::to_string((long long)vp.floorBoxes) + " floors, " + std::to_string((long long)vp.list.size()) +
			" volumes)");
		CHECK(vp.exitsCovered == 2 && vp.exitBox.size() == 2 && vp.exitBox[0] >= 0 && vp.exitBox[1] >= 0, "both exit points held by their exit volumes");
		CHECK(vp.parts == 1, "one network (" + std::to_string((long long)vp.parts) + " parts)");

		bool linkedBothWays = true;

		for (size_t i = 0; i < vp.list.size(); i++)
		{
			for (host::WaypointLinkRec const &l : vp.list[i].links)
			{
				bool back = false;

				for (host::WaypointLinkRec const &m : vp.list[(size_t)l.index].links)
				{
					back = back || m.index == (int32_t)i;
				}

				linkedBothWays = linkedBothWays && back && (l.flags & 4);
			}
		}

		CHECK(linkedBothWays, "every link listed on both sides, aliens only");
	}

	// 8. A DUCT THROUGH THE VOLUME PLANNER: crawl cells (floor.crawl) go to the levels' rule as the kind "duct"
	{
		puts("a duct through the volume planner:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 300;
		floor.crawl = true;
		Cells(&floor, 0, 25, 0, 5, 0, 300);

		std::vector<std::vector<int32_t> > const exits = { { -200, -10, 750 }, { 7700, -10, 750 } };
		std::vector<host::AutoPlotDoor> doors(2);
		host::AutoPlotPlan const vp = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		CHECK(vp.volumes && vp.volumeKind == "duct" && vp.exitsCovered == 2 && vp.parts == 1, "a duct, shaped, both exits held, one network (" + vp.volumeKind + ")");
	}

	// 9. STEPS04 (Genshd1's AI module 215, Frost 11-20; Dietrich's 544831f0 left it "NOT SHAPED: a corridor with no side to climb", both exits D4 FAILS): ONE STRAIGHT FLIGHT
	// and nothing else, as levels.exe reads the stock file: 10 treads 7.8 m wide (x 18807..26601) and 780 mm deep, each 390 mm over the one before, from y 7403 at z 7793 to
	// y 3895 at z 15587; ceilings at y 2900 over the lower half and y 1000 over the upper; a doorway at each end. No flat rectangle reaches a level's 2400, so the levels'
	// rule has no level: the flight must still come back as a floor-walk network, a stair joining both exits
	{
		puts("STEPS04, one straight flight through the volume planner:");

		// the cells as the planner's own map of it drew them (544831f0 editor-output.txt, "volume-map: cells 707, edge 300, origin 19050 15460, the first floor at y
		// 6234"): 27 rows (row 0 the top, at z 15460, down to row 26 at z 7660) by 27 columns (x 18750 to 26550); columns 1..25 in every row, the two side columns
		// in some (sideW, sideE: '1' where the map has a cell); each cell at the height of the tread 150 mm up the flight from its middle, as the map has them (rows
		// 16..18 the first floor's 6234, the six treads over it in rows 0..15, the three under it in rows 19..26)
		char const *const sideW = "001001010010110101101001010";
		char const *const sideE = "011011110110111101111011011";
		host::AutoPlotFloor floor;
		floor.cellEdge = 300;

		for (int row = 0; row < 27; row++)
		{
			int const z = 15460 - 300 * row;
			int const under = (int)((double)(z + 150 - 7793) / 779.4);   // z + 150 is over 7793 in every row
			int const tread = under > 9 ? 9 : (under < 0 ? 0 : under);
			int const y = (int)(7403.0 - 389.8 * tread + 0.5);

			for (int col = 0; col < 27; col++)
			{
				if ((col == 0 && sideW[row] != '1') || (col == 26 && sideE[row] != '1'))
				{
					continue;
				}

				int const x = 18750 + 300 * col;

				floor.cells.push_back(std::vector<int32_t>{ x, y, z });
				floor.ceilings.push_back(std::vector<int32_t>{ x, z < 11690 ? 2900 : 1000, z });
			}
		}

		CHECK(floor.cells.size() == 707, "the map's 707 cells (" + std::to_string((long long)floor.cells.size()) + ")");

		// its exits as the auto-plot takes them (its ADJMDLEP entry points, levels.exe on the stock file): to shedent01 at the foot, to genstrs at the top; each doorway's
		// middle half way to the neighbour's entry back (shedent01's at z 7893, genstrs's at z 15486), the way through out of this module (autoplot_command.cpp)
		std::vector<std::vector<int32_t> > const exits = { { 22704, 7783, 7692 }, { 22704, 3497, 15687 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].throughFound = true, doors[0].middle[0] = 22704, doors[0].middle[1] = 7588, doors[0].middle[2] = 7792, doors[0].through[1] = -1.0;
		doors[1].throughFound = true, doors[1].middle[0] = 22704, doors[1].middle[1] = 3690, doors[1].middle[2] = 15586, doors[1].through[1] = 1.0;

		host::AutoPlotPlan const vp = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		std::string notes;

		for (std::string const &n : vp.notes)
		{
			notes += (notes.empty() ? "" : "; ") + n;
		}

		CHECK(vp.volumes && vp.floorBoxes >= 1, "shaped, a floor-walk network (" + vp.volumeKind + ": " + notes + ")");
		CHECK(vp.exitsCovered == 2 && vp.exitBox.size() == 2 && vp.exitBox[0] >= 0 && vp.exitBox[1] >= 0, "both exit points held by their exit volumes (" +
			std::to_string((long long)vp.exitsCovered) + ")");
		CHECK(vp.parts == 1, "one network (" + std::to_string((long long)vp.parts) + " parts)");
	}

	// 10. OPSCEN03 (Genshd1's AI module 153; Drake for Dietrich 11-20: "does AutoPlot_LevelsVolumes handle a room with only TWO floors at two heights?"), as levels.exe reads
	// the stock file: an upper floor at y -15586 (z 81050..85726, with the first tread at its height), four treads 780 mm deep, 390 mm each, down to the lower floor at
	// y -13638 (z 88844..97416), 6 m across (20 cells of 300); its exits: Ops02 at the upper end (-60684 -15596 80950), Ops01 at the lower floor's side (-57076 -13648 93520)
	for (int withTreads = 1; withTreads >= 0; withTreads--)
	{
		puts(withTreads ? "opscen03, two floors and the treads between them:" : "opscen03, the two floors only (no cell on the treads):");

		host::AutoPlotFloor floor;
		floor.cellEdge = 300;

		for (int row = 0; row < 55; row++)
		{
			int const z = 81200 + 300 * row;
			int const y = z < 85726 ? -15586 : z < 86506 ? -15197 : z < 87285 ? -14807 : z < 88064 ? -14417 : z < 88844 ? -14028 : -13638;

			if (!withTreads && z >= 85726 && z < 88844)
			{
				continue;
			}

			for (int col = 0; col < 20; col++)
			{
				int const x = -63591 + 300 * col;

				floor.cells.push_back(std::vector<int32_t>{ x, y, z });
				floor.ceilings.push_back(std::vector<int32_t>{ x, y - 3900, z });
			}
		}

		std::vector<std::vector<int32_t> > const exits = { { -60684, -15596, 80950 }, { -57076, -13648, 93520 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].throughFound = true, doors[0].middle[0] = -60684, doors[0].middle[1] = -15596, doors[0].middle[2] = 81050, doors[0].through[1] = -1.0;
		doors[1].throughFound = true, doors[1].middle[0] = -57176, doors[1].middle[1] = -13648, doors[1].middle[2] = 93520, doors[1].through[0] = 1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "stairs or levels");
		std::string notes;

		for (std::string const &n : lv.notes)
		{
			notes += (notes.empty() ? "" : "; ") + n;
		}

		if (withTreads)
		{
			CHECK(lv.volumes && lv.floorBoxes >= 2 && lv.exitsCovered == 2 && lv.parts == 1, "the levels' rule: both floors, both exits, one network (" + std::to_string((long long)lv.floorBoxes) +
				" floors, " + std::to_string((long long)lv.exitsCovered) + " exits, " + std::to_string((long long)lv.parts) + " parts: " + notes + ")");
		}
		else
		{
			// with no cell between them the two floors don't touch: the rule links levels only through neighbouring cells (and a riser of up to 3900 between them), so this
			// is two networks, and the notes say why. Said here so the answer to Drake's question is the rule's own
			CHECK(!lv.volumes && notes.find("NOT SHAPED") != std::string::npos && notes.find("not joined") != std::string::npos,
				"with no cell on the treads the two floors are not joined: NOT SHAPED, said, no network of two parts written (" + std::to_string((long long)lv.parts) + " parts: " + notes + ")");
		}
	}

	// 5. NO CELLS
	{
		puts("no cells:");

		host::AutoPlotFloor floor;
		floor.cellEdge = 600;
		plan = host::AutoPlot_PlanLevels(floor, std::vector<std::vector<int32_t> >(), std::vector<host::AutoPlotDoor>());
		CHECK(!plan.shaped && !plan.notes.empty(), "not shaped, and said (" + Notes(plan) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
