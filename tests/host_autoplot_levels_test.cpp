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

// which box's centre lies in which other box, for a check's words
static std::string Shared(host::AutoPlotPlan const &plan)
{
	std::string where;

	for (size_t b = 0; b < plan.list.size(); b++)
	{
		for (size_t o = 0; o < plan.list.size(); o++)
		{
			host::WaypointRec const &w = plan.list[o];
			int32_t const *const c = plan.list[b].centre;

			if (o != b && w.min[0] <= c[0] && c[0] < w.max[0] && w.min[1] <= c[1] && c[1] < w.max[1] && w.min[2] <= c[2] && c[2] < w.max[2])
			{
				where += "; box " + std::to_string((long long)b) + "'s centre in box " + std::to_string((long long)o);
			}
		}
	}

	return where;
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

	// ---- A TRENCH (trench01, Genshd1 AI 371, in whole-b90f37bb: refused, "box 0: its centre is in box 1 too", Drake's V2 "no place in it outside the other boxes"): a flat
	// trench floor (a level) between two sloping banks, the banks joined by a narrow ledge along its east side, so the banks and the ledge are ONE stair whose box (the
	// bounding box of its cells) holds the whole level. A box inside another merges into it (Frost's rule 11-20): no centre in another box, both exits held, one network
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 39, 16, 40, 2725, 300);   // the trench floor

		for (int row = 0; row < 10; row++)   // the banks: 300 up each row away from the floor (a step the grid walks), to 3000 over it
		{
			Cells(&floor, 0, 39, 15 - row, 15 - row, 2425 - 300 * row, 300);
			Cells(&floor, 0, 39, 41 + row, 41 + row, 2425 - 300 * row, 300);
		}

		Cells(&floor, 40, 41, 15, 41, 2425, 300);   // the ledge along the east side, 300 over the floor, from bank to bank (too narrow to be a level)

		std::vector<std::vector<int32_t> > const exits = { { -300, 1975, 8400 }, { 6000, -1025, 15300 } };
		std::vector<host::AutoPlotDoor> const doors(2);
		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");

		Check(lv.volumes && lv.centresShared == 0 && lv.exitsCovered == 2 && lv.parts == 1, "A TRENCH: the stair round both banks holds the trench's level whole, so the level "
			"merges into it: no centre in another box, both exits held, one network (" + std::to_string((long long)lv.floorBoxes) + " floors, shared " +
			std::to_string((long long)lv.centresShared) + ", exits " + std::to_string((long long)lv.exitsCovered) + ", parts " + std::to_string((long long)lv.parts) + ")");
	}

	// ---- EXITS AT ONE POINT (yard04, Genshd1 AI 108, in whole-b90f37bb: refused, "box 1: its centre is in box 3 too": exits 0 and 2 stand at one point, so do 1 and 3, and
	// each had a box, two the same). A yard floor with its four exits at two points: one exit box a point, both exits held by it, no centre in another box
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 19, 0, 19, 2000, 300);

		std::vector<std::vector<int32_t> > const exits = { { -300, 1250, 3000 }, { 6000, 1250, 3000 }, { -300, 1250, 3000 }, { 6000, 1250, 3000 } };
		std::vector<host::AutoPlotDoor> const doors(4);
		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");

		Check(lv.volumes && lv.centresShared == 0 && lv.exitsCovered == 4 && lv.exitBox.size() == 4 && lv.exitBox[0] >= 0 && lv.exitBox[0] == lv.exitBox[2] &&
			lv.exitBox[1] >= 0 && lv.exitBox[1] == lv.exitBox[3] && lv.exitBox[0] != lv.exitBox[1], "EXITS AT ONE POINT: one exit box a point, both its exits held by it, no "
			"centre in another box (shared " + std::to_string((long long)lv.centresShared) + ", exits " + std::to_string((long long)lv.exitsCovered) + ", boxes " +
			std::to_string((long long)lv.list.size()) + ")");
	}

	// ---- A DOORWAY INSIDE THE FLOOR'S FOOTPRINT (BLOCK-01 and OPEN-16, on Hudson's route, in the whole-level plot on 5e58207f: refused "box 2: its centre is in box 0
	// too": an open-sky floor runs on 1.9 m past a doorway's middle, so the exit box's centre, 1000 past it, is in the floor box and V2 has nowhere to move it). A yard
	// whose floor runs 1350 past its east doorway's middle: the exit's centre goes on along the way through until no floor box holds it
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 19, 0, 9, 2000, 300);

		std::vector<std::vector<int32_t> > const exits = { { -300, 1250, 1350 }, { 4500, 1250, 1350 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[1].throughFound = true, doors[1].middle[0] = 4500, doors[1].middle[1] = 1250, doors[1].middle[2] = 1350, doors[1].through[0] = 1.0;
		doors[1].soffitFound = true, doors[1].soffit[0] = 4500, doors[1].soffit[1] = -500, doors[1].soffit[2] = 1350;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");

		Check(lv.volumes && lv.centresShared == 0 && lv.exitsCovered == 2, "A DOORWAY INSIDE THE FLOOR'S FOOTPRINT: the exit's centre on along the way through, out of the "
			"floor box: no centre in another box, both exits held (shared " + std::to_string((long long)lv.centresShared) + ", exits " + std::to_string((long long)lv.exitsCovered) + ")");
	}

	// ---- AN EXIT POINT IN ITS OWN FLOOR BOX (SHED04, AI 225, in Hudson's whole-map runs 220016 and 221129, Vasquez: the levels' floor box ran 107 past the doorway's middle
	// and held the entry point, so the route was 0>0..0, one box with no aliens-only link: AlienShouldBeCrawling said no and the hunter stood, why=HUNT-STANDING). A floor that
	// runs 150 past its east doorway's middle, the exit point there 750 over the floor. (B), Bishop 11-21: the cut along the way through is taken out; on the fully covered base it uncovered floor (MOD-01, Genshd1 AI 70: 34 m2, 8,362 standing bodies in no
	// box), and bishop-int never had it. SHED04's case is queued as "a cut that never uncovers a floor cell". So the floor box keeps its
	// floor to its end and holds the point too, every floor cell held, both exits held, no centre shared (as bishop-int plots it today)
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 19, 0, 9, 2000, 300);

		std::vector<std::vector<int32_t> > const exits = { { -300, 1250, 1350 }, { 5700, 1250, 1350 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[1].throughFound = true, doors[1].middle[0] = 5700, doors[1].middle[1] = 1250, doors[1].middle[2] = 1350, doors[1].through[0] = 1.0;
		doors[1].soffitFound = true, doors[1].soffit[0] = 5700, doors[1].soffit[1] = -500, doors[1].soffit[2] = 1350;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "corridor, floor only");
		int floorsHolding = 0;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			host::WaypointRec const &w = lv.list[b];
			bool const isExit = lv.exitBox.size() == 2 && (int)b == lv.exitBox[1];

			floorsHolding += !isExit && w.min[0] <= 5700 && w.max[0] > 5700 && w.min[1] <= 1250 && w.max[1] > 1250 && w.min[2] <= 1350 && w.max[2] > 1350 ? 1 : 0;
		}

		Check(lv.volumes && floorsHolding == 1 && lv.cellsCovered == lv.cells && lv.exitsCovered == 2 && lv.centresShared == 0, "AN EXIT POINT IN ITS OWN FLOOR BOX: "
			"no cut along the way through (B): the floor box keeps its floor and holds the point too, every floor cell held, both exits held, no centre shared (" +
			std::to_string((long long)floorsHolding) + " other boxes hold it, cells " + std::to_string((unsigned long long)lv.cellsCovered) + " of " +
			std::to_string((unsigned long long)lv.cells) + ", shared " + std::to_string((long long)lv.centresShared) + ")");
	}

	// ---- AN EXIT INTO A PIT (corr10 into lift03, liftplt's pit, in Hudson's run 220016, Vasquez: its entry point lies on the pit floor 768 under corr10's floor, and on
	// the aliens-only lane the alien crawled with gravity off and flipped at the lip 900 times instead of dropping in). A floor with its west exit level and its east exit
	// 768 under it: the east exit's link to its floor is a GROUND link (no kLinkAlien: the alien walks with gravity and drops in), the west one stays aliens only
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 19, 0, 9, 2000, 300);

		std::vector<std::vector<int32_t> > const exits = { { -300, 1250, 1350 }, { 6000, 2768, 1350 } };
		std::vector<host::AutoPlotDoor> const doors(2);
		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "corridor, floor only");
		int pitFlags = -1, levelFlags = -1;

		for (int k = 0; k < 2 && lv.exitBox.size() == 2; k++)
		{
			int const xb = lv.exitBox[(size_t)k];

			for (size_t b = 0; xb >= 0 && b < lv.list.size(); b++)
			{
				for (host::WaypointLinkRec const &l : lv.list[b].links)
				{
					if ((int)b != xb && l.index == xb)
					{
						(k == 1 ? pitFlags : levelFlags) = l.flags;
					}
				}
			}
		}

		Check(lv.volumes && pitFlags >= 0 && (pitFlags & host::kLinkAlien) == 0 && levelFlags >= 0 && (levelFlags & host::kLinkAlien) != 0, "AN EXIT INTO A PIT (768 under the "
			"floor): its link from the floor is a ground link, the level exit's stays aliens only (pit " + std::to_string((long long)pitFlags) + ", level " +
			std::to_string((long long)levelFlags) + ")");
	}

	// ---- AN EXIT IN MID-AIR OVER A TRENCH (trench01, Genshd1 AI 371, exit 1 to air06, in whole-0a1abdea: its point 4.6 m over the trench floor, over its middle; the rule that
	// stops a floor box short of an exit point cut the box along the doorway's way through, so 12.4 m of the trench was in no box, 1572 of 3349 floor cells held; the same in
	// procpath01, 2154 of 4800). The trench of A TRENCH with banks 4.5 m high and an exit 4.6 m over the floor's middle, its way through along -x: the floor's west end is
	// still held by a floor box, the exit point only by its exit box, every exit held, no centre shared
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 39, 16, 40, 2725, 300);   // the trench floor

		for (int row = 0; row < 15; row++)   // the banks: 300 up each row away from the floor, to 4.5 m over it
		{
			Cells(&floor, 0, 39, 15 - row, 15 - row, 2425 - 300 * row, 300);
			Cells(&floor, 0, 39, 41 + row, 41 + row, 2425 - 300 * row, 300);
		}

		Cells(&floor, 40, 41, 15, 41, 2425, 300);   // the ledge along the east side, from bank to bank

		// (exit 1 just past the banks' top row, 750 over it, as in A TRENCH)
		std::vector<std::vector<int32_t> > const exits = { { -300, 1975, 8400 }, { 6000, -2525, 16800 }, { 6000, -1900, 8400 } };
		std::vector<host::AutoPlotDoor> doors(3);

		doors[2].throughFound = true, doors[2].middle[0] = 6000, doors[2].middle[1] = -1900, doors[2].middle[2] = 8400, doors[2].through[0] = -1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");
		auto const holds = [](host::WaypointRec const &w, int32_t x, int32_t y, int32_t z) { return w.min[0] <= x && x < w.max[0] && w.min[1] <= y && y < w.max[1] && w.min[2] <= z && z < w.max[2]; };
		int westHeld = 0, pointHeld = 0;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			bool const isExit = lv.exitBox.size() == 3 && ((int)b == lv.exitBox[0] || (int)b == lv.exitBox[1] || (int)b == lv.exitBox[2]);

			westHeld += !isExit && holds(lv.list[b], 1500, 2725, 8400) ? 1 : 0;   // (a floor cell 1.5 m from the trench's west end, on its middle line)
			pointHeld += lv.exitBox.size() == 3 && (int)b != lv.exitBox[2] && holds(lv.list[b], 6000, -1900, 8400) ? 1 : 0;
		}

		Check(lv.volumes && westHeld >= 1 && pointHeld == 0 && lv.exitsCovered == 3 && lv.centresShared == 0, "AN EXIT IN MID-AIR OVER A TRENCH: the trench's west end "
			"still in a floor box, the point only in its exit box, every exit held, no centre shared (west held by " + std::to_string((long long)westHeld) + ", the point by " +
			std::to_string((long long)pointHeld) + " other boxes, exits " + std::to_string((long long)lv.exitsCovered) + ", shared " + std::to_string((long long)lv.centresShared) + Shared(lv) + ")");
	}

	// ---- AN EXIT ON A STAIR'S SIDE (PROC04, Genshd1 AI 349, in whole-adc1d8d6: one stair 9.5 m high, its exit to AI 106 on its side at mid-height; taken for an exit in mid-air,
	// the box's top came down under the point and 147 of its 164 floor cells were in no box; PROC05 the same). A bottom level and a stair rising 300 a row to 8.7 m over it, an
	// exit on the stair's -z side 750 over its tread there, its way through -z: the point stands on the stair, so the box keeps its height: the top tread still held,
	// both exits held, no centre shared. (B), Bishop 11-21: the cut along the way through is taken out; on the fully covered base it uncovered floor (MOD-01, Genshd1 AI 70: 34 m2, 8,362 standing bodies in no
	// box), and bishop-int never had it. SHED04's case is queued as "a cut that never uncovers a floor cell". So the stair
	// box holds the point too
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, -10, -1, 0, 9, 2000, 300);   // the bottom level

		for (int row = 0; row < 30; row++)   // the stair: 300 up a row, a step the grid walks
		{
			Cells(&floor, row, row, 0, 9, 2000 - 300 * row, 300);
		}

		std::vector<std::vector<int32_t> > const exits = { { -3300, 1250, 1350 }, { 4500, -3250, 100 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[1].throughFound = true, doors[1].middle[0] = 4500, doors[1].middle[1] = -3250, doors[1].middle[2] = 100, doors[1].through[1] = -1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "stairs or levels");
		auto const holds = [](host::WaypointRec const &w, int32_t x, int32_t y, int32_t z) { return w.min[0] <= x && x < w.max[0] && w.min[1] <= y && y < w.max[1] && w.min[2] <= z && z < w.max[2]; };
		int topHeld = 0, pointHeld = 0;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			bool const isExit = lv.exitBox.size() == 2 && ((int)b == lv.exitBox[0] || (int)b == lv.exitBox[1]);

			topHeld += !isExit && holds(lv.list[b], 8700, -6700, 1350) ? 1 : 0;   // (the top tread's middle)
			pointHeld += lv.exitBox.size() == 2 && (int)b != lv.exitBox[1] && holds(lv.list[b], 4500, -3250, 100) ? 1 : 0;
		}

		Check(lv.volumes && topHeld >= 1 && pointHeld == 1 && lv.exitsCovered == 2 && lv.centresShared == 0, "AN EXIT ON A STAIR'S SIDE: the stair box keeps its height (the top "
			"tread held) and holds the point too (no cut along the way through: (B)), both exits held, no centre shared (top held by " + std::to_string((long long)topHeld) + ", the point by " +
			std::to_string((long long)pointHeld) + " other boxes, exits " + std::to_string((long long)lv.exitsCovered) + ", shared " + std::to_string((long long)lv.centresShared) + ")");
	}

	// ---- AN EXIT POINT PAST THE STAIR'S CELLS (PROC04 as it is, the debug dump of whole-05e764b6: its exit point lies inside the stair's bounding box with no floor cell
	// within 1.2 m of it in plan, the nearest 2.2 m away and 3.4 m over it; taken for an exit in mid-air, the box's top came down under the point and 147 of its 164 cells
	// were in no box). The stair of AN EXIT ON A STAIR'S SIDE with a low wing off its +z side along its foot, so its bounding box runs 6 m along z over the whole stair; the
	// exit point in that box over the stair's middle, 1.8 m past its cells, its way through +z: no floor under it, so it is no exit in mid-air: the box keeps its height
	// (the top tread held), both exits held, no centre shared. (B), Bishop 11-21: the cut along the way through is taken out; on the fully covered base it uncovered floor (MOD-01, Genshd1 AI 70: 34 m2, 8,362 standing bodies in no
	// box), and bishop-int never had it. SHED04's case is queued as "a cut that never uncovers a floor cell". So
	// the stair box holds the point too
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, -10, -1, 0, 9, 2000, 300);   // the bottom level

		for (int row = 0; row < 30; row++)   // the stair: 300 up a row
		{
			Cells(&floor, row, row, 0, row < 6 ? 20 : 9, 2000 - 300 * row, 300);   // (its first 6 rows run on to z 6000: the wing)
		}

		std::vector<std::vector<int32_t> > const exits = { { -3300, 1250, 1350 }, { 4500, -3250, 4500 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[1].throughFound = true, doors[1].middle[0] = 4500, doors[1].middle[1] = -3250, doors[1].middle[2] = 4500, doors[1].through[1] = 1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "stairs or levels");
		auto const holds = [](host::WaypointRec const &w, int32_t x, int32_t y, int32_t z) { return w.min[0] <= x && x < w.max[0] && w.min[1] <= y && y < w.max[1] && w.min[2] <= z && z < w.max[2]; };
		int topHeld = 0, pointHeld = 0;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			bool const isExit = lv.exitBox.size() == 2 && ((int)b == lv.exitBox[0] || (int)b == lv.exitBox[1]);

			topHeld += !isExit && holds(lv.list[b], 8700, -6700, 1350) ? 1 : 0;
			pointHeld += lv.exitBox.size() == 2 && (int)b != lv.exitBox[1] && holds(lv.list[b], 4500, -3250, 4500) ? 1 : 0;
		}

		Check(lv.volumes && topHeld >= 1 && pointHeld == 1 && lv.exitsCovered == 2 && lv.centresShared == 0, "AN EXIT POINT PAST THE STAIR'S CELLS: the stair box keeps its "
			"height (the top tread held) and holds the point too (no cut along the way through: (B)), both exits held, no centre shared (top held by " + std::to_string((long long)topHeld) + ", the point by " +
			std::to_string((long long)pointHeld) + " other boxes, exits " + std::to_string((long long)lv.exitsCovered) + ", shared " + std::to_string((long long)lv.centresShared) + Shared(lv) + ")");
	}

	// ---- A POINT JUST PAST THE REACH (Frost's review of the mid-air rule 11-21: the floor 1.6 m under the point, just past kAutoPlotLevelExitReach, so the box's top comes down
	// to 300 under the point; is the box still tall enough?). Only a stair's box reaches that high (a level's reaches kAutoPlotLevelReach over its floor), so: the trench of
	// AN EXIT IN MID-AIR OVER A TRENCH with the point 1.6 m over its floor's middle. The box under it at least the game's 900 mm arrival box tall (by the rule at least
	// kAutoPlotLevelExitReach - kAutoPlotLevelExitClear + kAutoPlotLevelInto), the west end held, the point only in its exit box, every exit held
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 39, 16, 40, 2725, 300);   // the trench floor

		for (int row = 0; row < 15; row++)   // the banks, to 4.5 m over the floor
		{
			Cells(&floor, 0, 39, 15 - row, 15 - row, 2425 - 300 * row, 300);
			Cells(&floor, 0, 39, 41 + row, 41 + row, 2425 - 300 * row, 300);
		}

		Cells(&floor, 40, 41, 15, 41, 2425, 300);   // the ledge along the east side

		std::vector<std::vector<int32_t> > const exits = { { -300, 1975, 8400 }, { 6000, -2525, 16800 }, { 6000, 1125, 8400 } };
		std::vector<host::AutoPlotDoor> doors(3);

		doors[2].throughFound = true, doors[2].middle[0] = 6000, doors[2].middle[1] = 1125, doors[2].middle[2] = 8400, doors[2].through[0] = -1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");
		auto const holds = [](host::WaypointRec const &w, int32_t x, int32_t y, int32_t z) { return w.min[0] <= x && x < w.max[0] && w.min[1] <= y && y < w.max[1] && w.min[2] <= z && z < w.max[2]; };
		int shortest = 1 << 30, westHeld = 0, pointHeld = 0;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			bool const isExit = lv.exitBox.size() == 3 && ((int)b == lv.exitBox[0] || (int)b == lv.exitBox[1] || (int)b == lv.exitBox[2]);

			if (!isExit && holds(lv.list[b], 6000, 2724, 8400))   // (the floor right under the point)
			{
				shortest = std::min(shortest, (int)(lv.list[b].max[1] - lv.list[b].min[1]));
			}

			westHeld += !isExit && holds(lv.list[b], 1500, 2724, 8400) ? 1 : 0;
			pointHeld += lv.exitBox.size() == 3 && (int)b != lv.exitBox[2] && holds(lv.list[b], 6000, 1125, 8400) ? 1 : 0;
		}

		Check(lv.volumes && shortest >= 900 && shortest < (1 << 30) && westHeld >= 1 && pointHeld == 0 && lv.exitsCovered == 3, "A POINT JUST PAST THE REACH: the box under "
			"it at least the 900 mm arrival box tall, the west end held, the point only in its exit box, every exit held (" + std::to_string((long long)shortest) + " mm, west " +
			std::to_string((long long)westHeld) + ", the point by " + std::to_string((long long)pointHeld) + " other boxes, exits " + std::to_string((long long)lv.exitsCovered) +
			", shared " + std::to_string((long long)lv.centresShared) + Shared(lv) + ")");
	}

	// ---- A BANK BESIDE AN EXIT OVER THE FLOOR (Frost's review 11-21: the trench floor 4.6 m under the point but a bank 0.5 m under it within reach in plan, so it is no exit in
	// mid-air). The trench of AN EXIT IN MID-AIR OVER A TRENCH with the exit point beside the -z bank, 0.5 m over its row there: the stair box keeps its height (the bank's
	// top row held). (B), Bishop 11-21: the cut along the way through is taken out; on the fully covered base it uncovered floor (MOD-01, Genshd1 AI 70: 34 m2, 8,362 standing bodies in no
	// box), and bishop-int never had it. SHED04's case is queued as "a cut that never uncovers a floor cell". So the stair box
	// holds the point too, and the exit box's centre is then in the stair box: 1 centre shared, NAMED (the repair's V2 moves it or the checker refuses the module: the
	// queued item's case)
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 39, 16, 40, 2725, 300);   // the trench floor

		for (int row = 0; row < 15; row++)   // the banks, to 4.5 m over the floor
		{
			Cells(&floor, 0, 39, 15 - row, 15 - row, 2425 - 300 * row, 300);
			Cells(&floor, 0, 39, 41 + row, 41 + row, 2425 - 300 * row, 300);
		}

		Cells(&floor, 40, 41, 15, 41, 2425, 300);   // the ledge along the east side

		// (the point over the trench floor's first row, z 4800, 600 from the bank's row at z 4200, 2425 - 300 * 1 = 2125 there: 500 under the point)
		std::vector<std::vector<int32_t> > const exits = { { -300, 1975, 8400 }, { 6000, -2525, 16800 }, { 6000, 1625, 4800 } };
		std::vector<host::AutoPlotDoor> doors(3);

		doors[2].throughFound = true, doors[2].middle[0] = 6000, doors[2].middle[1] = 1625, doors[2].middle[2] = 4800, doors[2].through[0] = -1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");
		auto const holds = [](host::WaypointRec const &w, int32_t x, int32_t y, int32_t z) { return w.min[0] <= x && x < w.max[0] && w.min[1] <= y && y < w.max[1] && w.min[2] <= z && z < w.max[2]; };
		int topHeld = 0, pointHeld = 0;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			bool const isExit = lv.exitBox.size() == 3 && ((int)b == lv.exitBox[0] || (int)b == lv.exitBox[1] || (int)b == lv.exitBox[2]);

			topHeld += !isExit && holds(lv.list[b], 9000, -1775, 300) ? 1 : 0;   // (the -z bank's top row, z 300, at 4.5 m, east of the point)
			pointHeld += lv.exitBox.size() == 3 && (int)b != lv.exitBox[2] && holds(lv.list[b], 6000, 1625, 4800) ? 1 : 0;
		}

		Check(lv.volumes && topHeld >= 1 && pointHeld == 1 && lv.exitsCovered == 3 && lv.centresShared == 1, "A BANK BESIDE AN EXIT OVER THE FLOOR: no exit in mid-air, the "
			"stair box keeps its height (the bank's top row held) and holds the point too (no cut along the way through: (B)), its exit box's centre in it: 1 shared, "
			"named (top held by " + std::to_string((long long)topHeld) + ", the point by " +
			std::to_string((long long)pointHeld) + " other boxes, exits " + std::to_string((long long)lv.exitsCovered) + ", shared " + std::to_string((long long)lv.centresShared) + Shared(lv) + ")");
	}

	// ---- THE CUT NEVER LEAVES A BOX NO PLACE OF ITS OWN (Bishop's (B) 11-21: there is no cut along the way through any more, so this pins that no box is left without a
	// place of its own. The case: trench07, Genshd1 AI 375, in whole-48ef59c2: refused, "box 6: its centre is in box 0 too", Drake's V2 "no place in it
	// outside the other boxes": its floor box's only place outside the stair boxes over it was the strip by exit 1, and the cut along exit 1's way through took it; in whole4,
	// with no cut, V2 put the centre there). The trench of A TRENCH with its banks joined at the WEST end, so the stair's box covers the floor, and the floor running 1.8 m on
	// east past the banks; an exit point near the floor's east end, its way through +x: every box but the exits keeps a place outside all the others (V2's test)
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;
		Cells(&floor, 0, 45, 16, 40, 2725, 300);   // the trench floor, 6 rows past the banks' east end

		for (int row = 0; row < 10; row++)   // the banks, to 3 m over the floor
		{
			Cells(&floor, 0, 39, 15 - row, 15 - row, 2425 - 300 * row, 300);
			Cells(&floor, 0, 39, 41 + row, 41 + row, 2425 - 300 * row, 300);
		}

		Cells(&floor, -2, -1, 15, 41, 2425, 300);   // the ledge joining the banks at the west end

		std::vector<std::vector<int32_t> > const exits = { { 12000, 1975, 8400 }, { 6000, -1025, 15300 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].throughFound = true, doors[0].middle[0] = 12000, doors[0].middle[1] = 1975, doors[0].middle[2] = 8400, doors[0].through[0] = 1.0;

		host::AutoPlotPlan const lv = host::AutoPlot_LevelsVolumes(floor, exits, doors, 1.0, "open sky");
		std::string covered;

		for (size_t b = 0; b < lv.list.size(); b++)
		{
			bool const isExit = lv.exitBox.size() == 2 && ((int)b == lv.exitBox[0] || (int)b == lv.exitBox[1]);
			host::WaypointRec const &w = lv.list[b];
			bool place = false;

			for (int32_t x = w.min[0]; !isExit && !place && x < w.max[0]; x += 150)   // (at its centre's height, as V2 looks)
			{
				for (int32_t z = w.min[2]; !place && z < w.max[2]; z += 150)
				{
					bool inOther = false;

					for (size_t o = 0; o < lv.list.size() && !inOther; o++)
					{
						host::WaypointRec const &v = lv.list[o];

						inOther = o != b && v.min[0] <= x && x < v.max[0] && v.min[1] <= w.centre[1] && w.centre[1] < v.max[1] && v.min[2] <= z && z < v.max[2];
					}

					place = !inOther;
				}
			}

			covered += !isExit && !place ? " box " + std::to_string((long long)b) : std::string();
		}

		Check(lv.volumes && covered.empty() && lv.exitsCovered == 2, "THE CUT NEVER LEAVES A BOX NO PLACE OF ITS OWN: every box but the exits has a place outside all "
			"the others, both exits held (with no place:" + (covered.empty() ? std::string(" none") : covered) + "; exits " + std::to_string((long long)lv.exitsCovered) + Shared(lv) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
