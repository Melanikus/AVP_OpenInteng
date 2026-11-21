// host_autoplot_volumes_test.cpp: THE VOLUME SHAPE (autoplot_volumes.h; the owner 11-20: "one entry module on top on each side, one in the middle to climb the wall/get in and
// another to get out/attack", "So like 5 volumes, maybe 7/8 for corners"). A straight corridor gives 7 volumes (floor, climb-in, get-out, 2 entries, 2 exits) with the spec's
// links and stops, and the stock route choice (autoplot_lane.h Stock_Choose, Rebellion's search) takes the floor up the climb-in, a hanging alien along the ceiling to the
// far doorway, and down by the get-out to a target on the floor. An L gives 2 floor rectangles. With no wall to climb the shape is refused (the caller's old planner).
// Written 2026-11-20 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "autoplot_lane.h"
#include "autoplot_plan.h"
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

using host::AutoPlotSurface;

static bool Holds(host::WaypointRec const &b, int32_t const p[3])
{
	for (int a = 0; a < 3; a++)
	{
		if (p[a] < b.min[a] || p[a] > b.max[a] - 1)
		{
			return false;
		}
	}

	return true;
}

static int32_t FlagsOf(host::WaypointRec const &b, int32_t to)
{
	for (host::WaypointLinkRec const &l : b.links)
	{
		if (l.index == to)
		{
			return l.flags;
		}
	}

	return -1;
}

// a box's numbers, for a check's words
static std::string Said(host::WaypointRec const &b)
{
	return "min " + std::to_string((long long)b.min[0]) + " " + std::to_string((long long)b.min[1]) + " " + std::to_string((long long)b.min[2]) + " max " +
		std::to_string((long long)b.max[0]) + " " + std::to_string((long long)b.max[1]) + " " + std::to_string((long long)b.max[2]);
}

// THE GAME'S OWN BOX (Vasquez 11-20: SweepWaypoints takes the alien's own box to be the HIGHEST-numbered box any corner of its body is in, and it heads on to the route's
// second box only when that own box is the route's start): the boxes numbered higher than box b that an alien's body at b's aim reaches (450 each way across, 900 up and
// down). None, or the alien that reaches b's aim never moves on ("never held")
static std::string HigherHeld(std::vector<host::WaypointRec> const &list, size_t b)
{
	std::string said;

	for (size_t o = b + 1; o < list.size(); o++)
	{
		bool reach = true;

		for (int k = 0; k < 3; k++)
		{
			int32_t const half = k == 1 ? 900 : 450;

			reach = reach && list[b].centre[k] + half >= list[o].min[k] && list[b].centre[k] - half <= list[o].max[k] - 1;
		}

		said += reach ? " " + std::to_string((long long)o) : std::string();
	}

	return said;
}

// every box reaches every other by its links (Frost's checker's rule: each reaches box 0, box 0 reaches each); a record's link with kLinkOneWayBack set is the stopped way
static bool AllReach(std::vector<host::WaypointRec> const &list)
{
	for (bool const forward : { true, false })
	{
		std::vector<bool> seen(list.size(), false);
		std::vector<size_t> todo(1, 0);

		seen[0] = !list.empty();

		while (!todo.empty() && !list.empty())
		{
			size_t const at = todo.back();

			todo.pop_back();

			for (size_t b = 0; b < list.size(); b++)
			{
				int32_t const f = forward ? FlagsOf(list[at], (int32_t)b) : FlagsOf(list[b], (int32_t)at);

				if (!seen[b] && f >= 0 && (f & host::kLinkOneWayBack) == 0)
				{
					seen[b] = true;
					todo.push_back(b);
				}
			}
		}

		for (bool const s : seen)
		{
			if (!s)
			{
				return false;
			}
		}
	}

	return true;
}

// one 300 mm floor cell (Genshd1's grid) at cell (ix, iz), the floor at y 2000, a ceiling at y -1000 over it
static void Cell300(host::AutoPlotFloor *floor, int32_t ix, int32_t iz)
{
	floor->cellEdge = 300;
	floor->cells.push_back(std::vector<int32_t>{ ix * 300, 2000, iz * 300 });
	floor->ceilings.push_back(std::vector<int32_t>{ ix * 300, -1000, iz * 300 });
}

static host::AutoPlotStep Step(int32_t x, int32_t y, int32_t z, AutoPlotSurface surface, int32_t fx, int32_t fy, int32_t fz)
{
	host::AutoPlotStep s;

	s.place[0] = x;
	s.place[1] = y;
	s.place[2] = z;
	s.surface = surface;
	s.facing[0] = fx;
	s.facing[1] = fy;
	s.facing[2] = fz;
	s.inside = true;
	return s;
}

// a corridor's floor cells (600 mm) over x0..x1, z0..z1 (cell centres), floor at y 2000, a ceiling at y -1000 over each
static void Cells(host::AutoPlotFloor *floor, int32_t x0, int32_t x1, int32_t z0, int32_t z1)
{
	floor->cellEdge = 600;

	for (int32_t x = x0; x <= x1; x += 600)
	{
		for (int32_t z = z0; z <= z1; z += 600)
		{
			floor->cells.push_back(std::vector<int32_t>{ x, 2000, z });
			floor->ceilings.push_back(std::vector<int32_t>{ x, -1000, z });
		}
	}
}

static host::AutoPlotDoor Door(int32_t mx, int32_t mz, double tx, double tz)
{
	host::AutoPlotDoor d;

	d.soffitFound = true;
	d.soffit[0] = mx;
	d.soffit[1] = -500;   // a 2.5 m doorway
	d.soffit[2] = mz;
	d.throughFound = true;
	d.middle[0] = mx;
	d.middle[1] = 750;
	d.middle[2] = mz;
	d.through[0] = tx;
	d.through[1] = tz;
	return d;
}

// hand6g's own counts (the owner's gold standard): a climb on every doorway floor (a corner's two), its one get-out
static host::AutoPlotVolumeCounts Hand6g()
{
	host::AutoPlotVolumeCounts c;

	c.climbIns = 2;
	c.getOuts = 1;
	return c;
}

// THE OWNER'S SHED01 (Drake's hand6g): a tube corner on 300 mm cells whose inner and outer corners are DIAGONAL in plan. Arm 1 along x (rows 13..38, the west doorway),
// arm 2 along z (columns 13..38, the south doorway); the outer (north-east) corner cut off, the inner one's diagonal floor west of arm 2. Planned with these counts
static host::AutoPlotPlan Shed01(host::AutoPlotVolumeCounts const &counts)
{
	host::AutoPlotFloor floor;

	for (int32_t iz = 13; iz <= 38; iz++)
	{
		for (int32_t ix = 0; ix <= std::min(38, 24 + (38 - iz)); ix++)
		{
			Cell300(&floor, ix, iz);
		}
	}

	for (int32_t iz = 0; iz <= 12; iz++)
	{
		for (int32_t ix = iz <= 1 ? 13 : 14 - iz; ix <= 38; ix++)
		{
			Cell300(&floor, ix, iz);
		}
	}

	floor.boxFound = true;   // a tube: the AI module's box 1,948 past the outer sides (the doorways' sides at the floor)
	floor.boxLo[0] = -150;
	floor.boxLo[1] = -1000;
	floor.boxLo[2] = -150;
	floor.boxHi[0] = 11550 + 1948;
	floor.boxHi[1] = 2000;
	floor.boxHi[2] = 11550 + 1948;

	std::vector<std::vector<int32_t> > exits = { { -250, 750, 7650 }, { 7650, 750, -250 } };
	std::vector<host::AutoPlotDoor> doors = { Door(-150, 7650, -1.0, 0.0), Door(7650, -150, 0.0, -1.0) };
	return host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0, counts);
}

int main()
{
	printf("host_autoplot_volumes_test\n");

	// ---- A STRAIGHT CORRIDOR along x: cells x 0..12000, z 0..3000 (12.6 by 3.6 m), walls at z -300 and 3300, doorways at x -300 (exit 0) and 12300 (exit 1)
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);

		host::AutoPlotWay way;

		way.alien = true;
		way.steps.push_back(Step(6000, 1000, -300, AutoPlotSurface::Wall, 0, 0, 65536));   // the z -300 wall, facing +z
		way.steps.push_back(Step(6000, 1000, 3300, AutoPlotSurface::Wall, 0, 0, -65536));

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 12400, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({ way }, exits, floor, doors, 1.0);

		Check(plan.volumes, "THE CORRIDOR is shaped as volumes");
		Check(plan.list.size() == 7, "7 volumes: floor, climb-in, get-out, 2 entries, 2 exits (" + std::to_string((long long)plan.list.size()) + ")");

		if (plan.list.size() == 7)
		{
			host::WaypointRec const &fl = plan.list[0], &climb = plan.list[1], &getOut = plan.list[2], &e0 = plan.list[3], &e1 = plan.list[4], &x0 = plan.list[5], &x1 = plan.list[6];

			Check(!plan.climbing[0] && plan.climbing[1] && plan.climbing[2] && plan.climbing[3] && plan.climbing[4] && !plan.climbing[5] && !plan.climbing[6], "the kinds in the list's order");
			Check(fl.min[0] <= -300 && fl.max[0] >= 12300 && fl.min[2] <= -300 && fl.max[2] >= 3300, "THE FLOOR is one box wall to wall");
			Check(fl.min[1] == 500 && fl.max[1] == 2201, "the floor's box: 1500 over the floor to 200 under it");
			Check(plan.cellsCovered == plan.cells && plan.cells == 21 * 6, "every floor cell is in it (" + std::to_string((long long)plan.cellsCovered) + " of " + std::to_string((long long)plan.cells) + ")");
			Check(climb.min[2] == 3200 && climb.max[2] == 3501, "THE CLIMB-IN on the + side (z 3300, as Drake's corridors): 100 into the room, to 200 past the side (no box given: reach 0)");
			Check(climb.min[0] == 4800 && climb.max[0] == 7201, "the climb-in is 2.4 m along the side at its middle");
			Check(climb.centre[2] == 3400 && climb.centre[1] == -700, "its centre 100 beyond the side's face, 300 under the ceiling (Drake's hand5b) (" +
				std::to_string((long long)climb.centre[2]) + ", " + std::to_string((long long)climb.centre[1]) + ")");
			Check(e0.min[0] == -900 && e0.max[0] == 6001 && e1.min[0] == 6000 && e1.max[0] == 12901, "THE ENTRIES: the corridor's halves, split at its middle, each 600 past its doorway");
			Check(e0.min[1] == -1200 && e0.max[1] == -49, "an entry: 200 into the ceiling down to 450 under the soffit");
			// (Bishop 11-20, the loop the owner sat through at gencor01's climb, Frost's run 153834: an aim 100 INTO the ceiling no hanging alien's box ever holds, and at the
			// climb's top the flattened move pushed into the upper chamfer: NPC_IsObstructed, then Avoidance down the tube. hand6g has it too: a named difference)
			Check(e0.centre[1] == -900 && e1.centre[1] == -900, "the entries' centres 100 UNDER the ceiling face: holdable by a hanging alien (" +
				std::to_string((long long)e0.centre[1]) + ")");
			Check(e0.centre[2] == 2600 && e1.centre[2] == 2600, "and 700 off the climb side");
			Check(e0.centre[0] == 4800 && e1.centre[0] == 7200, "each at the climb-in's end nearer its own doorway");
			Check(x0.min[1] == 551 && x1.min[1] == 551, "THE EXITS top out 1051 under the soffit (a hanging alien never touches its target: Drake's D3 wants MORE than 1000 under the REAL "
				"ceiling, and the grid's soffit reads a few mm high: 998 on 1d4b78e7's path) (" + std::to_string((long long)x0.min[1]) + ")");
			Check(x0.centre[0] == -1300 && x1.centre[0] == 13300 && x0.centre[1] == 750, "their centres 1000 past the doorway's middle, at mid-door height");
			Check(getOut.min[1] == 1 && getOut.max[1] == 501, "THE GET-OUT: 1001 under the ceiling (no lane touches it) to 1500 over the floor");
			Check(getOut.min[0] == 4800 && getOut.max[0] == 7201 && getOut.min[2] == -300 && getOut.max[2] == 2851, "the middle 2.4 m, less 450 on the climb side (Drake's hand5b)");
			Check(plan.volumeKind == "corridor", "its kind: a corridor (" + plan.volumeKind + ")");
			Check(plan.exitsCovered == 2 && plan.exitBox[0] == 5 && plan.exitBox[1] == 6 && plan.doorBox[0] == 3 && plan.doorBox[1] == 4, "both exits covered, each its own box and entry");
			Check(plan.doorWords[0] == "lane" && plan.doorWords[1] == "lane", "both doorways a lane");
			Check(plan.parts == 1 && plan.centresShared == 0, "one network, no centre shared (parts " + std::to_string((long long)plan.parts) + ", shared " + std::to_string((long long)plan.centresShared) + ")");

			for (int e = 0; e < 2; e++)
			{
				int32_t const p[3] = { exits[(size_t)e][0], exits[(size_t)e][1], exits[(size_t)e][2] };
				int last = -1;

				for (size_t b = 0; b < plan.list.size(); b++)
				{
					last = Holds(plan.list[b], p) ? (int)b : last;
				}

				Check(last == 5 + e, "exit " + std::to_string((long long)e) + "'s point: its exit box is the last that holds it (the game's target box)");
			}

			bool centresOwn = true;

			for (size_t b = 0; b < plan.list.size(); b++)
			{
				for (size_t o = 0; o < plan.list.size(); o++)
				{
					centresOwn = centresOwn && (Holds(plan.list[o], plan.list[b].centre) == (o == b));
				}
			}

			std::string where;

			for (size_t b = 0; b < plan.list.size(); b++)
			{
				for (size_t o = 0; o < plan.list.size(); o++)
				{
					where += (o != b && Holds(plan.list[o], plan.list[b].centre)) ? " box " + std::to_string((long long)b) + "'s in box " + std::to_string((long long)o) : std::string();
				}
			}

			Check(centresOwn, "every centre in its own box and no other" + where);

			// THE LINKS: 4 both ways; a -> b only: a lists b 5, b lists a 6
			Check(FlagsOf(fl, 1) == 5 && FlagsOf(climb, 0) == 6, "floor -> climb-in only");
			Check(FlagsOf(climb, 3) == 5 && FlagsOf(climb, 4) == 5 && FlagsOf(e0, 1) == 6 && FlagsOf(e1, 1) == 6, "climb-in -> both entries only");
			Check(FlagsOf(e0, 4) == 4 && FlagsOf(e1, 3) == 4, "entry <-> entry");
			Check(FlagsOf(e0, 5) == 5 && FlagsOf(e1, 6) == 5 && FlagsOf(x0, 3) == 6, "entry -> its exit only");
			Check(FlagsOf(x0, 0) == 5 && FlagsOf(x1, 0) == 5 && FlagsOf(fl, 5) == 6, "exit -> floor only");
			Check(FlagsOf(e0, 2) == 5 && FlagsOf(e1, 2) == 5 && FlagsOf(getOut, 0) == 5, "entries -> get-out -> floor only (the landing column)");
			Check(FlagsOf(e0, 0) == -1 && FlagsOf(e1, 0) == -1, "no entry <-> floor link (Drake's hand5b: down by the get-out only)");
			Check(FlagsOf(fl, 2) == 6, "the floor never steps up into the get-out");

			// THE ROUTES, by the stock search (every link shift)
			int32_t const standing[3] = { 3000, 2000, 1500 }, hanging[3] = { 2000, -100, 1500 }, him[3] = { 6000, 1000, 2500 };
			int32_t const far1[3] = { exits[1][0], exits[1][1], exits[1][2] };
			bool up = true, along = true, down = true;

			for (uint32_t s = 0; s < 16; s++)
			{
				host::StockChoice const a = host::Stock_Choose(plan.list, 1.0, standing, far1, true, s);
				host::StockChoice const b = host::Stock_Choose(plan.list, 1.0, hanging, far1, true, s);
				host::StockChoice const c = host::Stock_Choose(plan.list, 1.0, hanging, him, true, s);

				up = up && a.routed && a.start == 0 && a.second == 1 && a.crawls;
				along = along && b.routed && b.start == 3 && b.second == 4 && b.crawls;
				down = down && c.routed && c.start == 3 && (c.second == 2 || c.second == 5) && c.crawls;
			}

			Check(up, "THE FLOOR GOES UP: standing in the corridor, heading for the far doorway, every alien's route is the climb-in next, crawling");
			Check(along, "A LANE: hanging at the near end, heading for the far doorway, entry then entry, crawling (no drop)");
			Check(down, "THE ATTACK: hanging, its target on the floor, the get-out next (or its doorway's exit: the tie Drake's proven file has, named), crawling");
		}
	}

	// ---- AN L: arm 1 along x (x 0..9000, z 0..3000), arm 2 along z (x 6000..9000, z 3600..12000); doorways at x -300 and z 12300
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 9000, 0, 3000);
		Cells(&floor, 6000, 9000, 3600, 12000);

		host::AutoPlotWay way;

		way.alien = true;
		way.steps.push_back(Step(4000, 1000, -300, AutoPlotSurface::Wall, 0, 0, 65536));
		way.steps.push_back(Step(9300, 1000, 8000, AutoPlotSurface::Wall, -65536, 0, 0));

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 7500, 750, 12400 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(7500, 12300, 0.0, 1.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({ way }, exits, floor, doors, 1.0, Hand6g());   // (hand5b's: a climb on each arm)
		size_t floors = 0;

		for (size_t b = 0; b < plan.climbing.size(); b++)
		{
			floors += (!plan.climbing[b] && (int)b != plan.exitBox[0] && (int)b != plan.exitBox[1]) ? 1 : 0;
		}

		Check(plan.volumes && plan.volumeKind == "corner", "THE L is shaped as volumes, a corner (" + plan.volumeKind + ")");
		Check(floors == 2, "2 floor rectangles (" + std::to_string((long long)floors) + ")");

		// its climb-ins on the L's OUTER walls (Drake's hand5b): arm 2's east side (x 9300) and arm 1's south side (z -300)
		bool east = false, south = false;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.max[1] - w.min[1] > 2000)   // a climb-in: floor to ceiling
			{
				east = east || (w.min[0] == 9200 && w.max[0] == 9501);
				south = south || (w.min[2] == -500 && w.max[2] == -199);
			}
		}

		Check(east && south, "its climb-ins on the L's outer walls: arm 2's east side and arm 1's south side");
		Check(plan.list.size() >= 7 && plan.list.size() <= 9, "7 to 9 volumes (" + std::to_string((long long)plan.list.size()) + ")");
		std::string where;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			for (size_t o = 0; o < plan.list.size(); o++)
			{
				where += (o != b && Holds(plan.list[o], plan.list[b].centre)) ? " box " + std::to_string((long long)b) + "'s in box " + std::to_string((long long)o) : std::string();
			}
		}

		Check(plan.parts == 1 && plan.exitsCovered == 2 && plan.centresShared == 0, "one network, both exits covered, no centre shared (parts " + std::to_string((long long)plan.parts) +
			", covered " + std::to_string((long long)plan.exitsCovered) + ", shared " + std::to_string((long long)plan.centresShared) + where + ")");
		Check(plan.cellsCovered == plan.cells, "every floor cell in a floor box");

		int32_t const standing[3] = { 7500, 2000, 10000 }, near0[3] = { exits[0][0], exits[0][1], exits[0][2] };
		bool climbs = true;

		for (uint32_t s = 0; s < 16; s++)
		{
			host::StockChoice const a = host::Stock_Choose(plan.list, 1.0, standing, near0, true, s);

			climbs = climbs && a.routed && a.crawls;
		}

		Check(climbs, "standing at arm 2's far end, heading for arm 1's doorway: every route's first link is aliens only");
	}

	// ---- A TUBE'S FLOOR CELLS (the owner's gencor01-05, SHED01/05/10 came out "stairs or levels" in b93652ad, 11-20): the grid gives floor cells on the 45-degree lower
	// faces too, a row every 600 mm, each 600 higher than the last, so each row was a level of its own and a rectangle 1 cell wide. Rows 1 cell wide after the first
	// rectangle are a side's face, not a floor: left out, and the tube is a corridor
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);

		for (int32_t row = 1; row <= 3; row++)
		{
			for (int32_t x = 0; x <= 12000; x += 600)
			{
				floor.cells.push_back(std::vector<int32_t>{ x, 2000 - 600 * row, 3000 + 600 * row });   // the north side's lower face
				floor.cells.push_back(std::vector<int32_t>{ x, 2000 - 600 * row, 0 - 600 * row });      // the south side's
			}
		}

		floor.boxFound = true;
		floor.boxLo[0] = -300;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -300 - 1948;
		floor.boxHi[0] = 12300;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 3300 + 1948;

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 12400, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "corridor", "A TUBE WITH FLOOR CELLS ON ITS SLOPES is a corridor, shaped (" + plan.volumeKind + ")");
		Check(plan.floorBoxes == 1 && plan.list.size() == 11, "one floor (the flat band), 11 volumes (" + std::to_string((long long)plan.floorBoxes) + " floors, " +
			std::to_string((long long)plan.list.size()) + " volumes)");
	}

	// ---- GENSHD1'S CELLS ARE 300 MM (Drake's gold compare on 78e35f23, 11-20): a 45-degree face's rows are 300 apart and 300 higher each, inside the 450 step, so they
	// joined the floor: the band took in 628 mm of slope, its floor top read 555 high (no room for the get-out), and the tubes read as stairs. A level is a FLAT patch
	// (within 100 mm): the band alone is the floor, its edge where the flat cells end
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;

		for (int32_t x = 0; x <= 12000; x += 300)
		{
			for (int32_t z = 0; z <= 3000; z += 300)
			{
				floor.cells.push_back(std::vector<int32_t>{ x, 2000, z });
				floor.ceilings.push_back(std::vector<int32_t>{ x, -1000, z });
			}

			for (int32_t row = 1; row <= 5; row++)
			{
				floor.cells.push_back(std::vector<int32_t>{ x, 2000 - 300 * row, 3000 + 300 * row });   // the north side's lower face
				floor.cells.push_back(std::vector<int32_t>{ x, 2000 - 300 * row, 0 - 300 * row });      // the south side's
			}
		}

		std::vector<std::vector<int32_t> > exits = { { -250, 750, 1500 }, { 12250, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-150, 1500, -1.0, 0.0), Door(12150, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "corridor" && plan.floorBoxes == 1, "300 MM CELLS ON A 45-DEGREE FACE: a corridor, one floor (" + plan.volumeKind + ", " +
			std::to_string((long long)plan.floorBoxes) + " floors)");
		Check(!plan.list.empty() && plan.list[0].max[2] == 3151 && plan.list[0].min[2] == -150 && plan.list[0].min[1] == 500, "the floor box ends where the flat cells end (z -150 .. 3150), its top 1500 over the floor (" +
			(plan.list.empty() ? std::string() : std::to_string((long long)plan.list[0].min[2]) + ".." + std::to_string((long long)plan.list[0].max[2]) + ", top " + std::to_string((long long)plan.list[0].min[1])) + ")");
	}

	// ---- A GAP ROW AND DOORWAY SILLS (gencor05 on 51fad1b1, 11-20: its band came out 27x12 + 27x12 + two 12x2 strips, a "room with 2 doorways"): a row of missing cells
	// across the band (under a pipe run) and a narrow sill at each doorway. Holes up to 3 cells across between flat cells of one height are floor; a strip under 700 mm
	// across after the first floor is a sliver. One floor, a corridor
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;

		for (int32_t x = 0; x <= 12000; x += 300)
		{
			for (int32_t z = 0; z <= 3000; z += 300)
			{
				if (x != 6000 && x != 6300)   // the gap: two rows of cells missing across the band
				{
					floor.cells.push_back(std::vector<int32_t>{ x, 2000, z });
				}
				else if (z >= 1500 && z < 2400)
				{
					floor.cells.push_back(std::vector<int32_t>{ x, 1550, z });   // and on part of it a pipe lying across the floor, its top 450 up
				}
				else if (z >= 2400)
				{
					floor.cells.push_back(std::vector<int32_t>{ x, -200, z });   // and a column, 2.2 m tall (gencor05's and SHED10's mid-floor block, the cell map on a8ed1781)
				}

				floor.ceilings.push_back(std::vector<int32_t>{ x, -1000, z });
			}
		}

		for (int32_t z = 600; z <= 2400; z += 300)
		{
			floor.cells.push_back(std::vector<int32_t>{ -300, 2000, z });     // the sills, 1 cell deep, in each doorway
			floor.cells.push_back(std::vector<int32_t>{ 12300, 2000, z });
		}

		std::vector<std::vector<int32_t> > exits = { { -550, 750, 1500 }, { 12550, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-450, 1500, -1.0, 0.0), Door(12450, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "corridor" && plan.floorBoxes == 1, "A GAP ROW AND DOORWAY SILLS: a corridor, one floor (" + plan.volumeKind + ", " +
			std::to_string((long long)plan.floorBoxes) + " floors" + (plan.notes.empty() ? std::string() : "; " + plan.notes[0]) + ")");
	}

	// ---- FITTINGS (Frost's levels tool on stock 11-20: SHED01 and gencor05 have pipe-run tops at y 800, 1.1 m over the floor, 1.8 and 6.1 m2): a level that small, after the
	// main floor, is a fitting, not a floor (the floor box, 1500 up, holds an alien standing on it). A corridor with two 2 x 2 cell pipe tops 1100 up is a corridor
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);

		for (int32_t const x0 : { 2400, 8400 })
		{
			for (int32_t x = x0; x <= x0 + 600; x += 600)
			{
				for (int32_t z = 3600; z <= 4200; z += 600)
				{
					floor.cells.push_back(std::vector<int32_t>{ x, 900, z });   // a pipe's top beside the floor, 1100 over it
				}
			}
		}

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 12400, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "corridor" && plan.floorBoxes == 1, "FITTINGS (two 2 x 2 pipe tops 1.1 m up) left out: a corridor, one floor (" + plan.volumeKind + ", " +
			std::to_string((long long)plan.floorBoxes) + " floors)");
	}

	// ---- A TUBE (Genshd1's corridors: each side two 45-degree faces meeting 1,948 out from the floor's edge; Frost's probe on hand5b, Drake's hand6g): the same corridor,
	// the AI module's box 1,948 past both long sides. The climb in LOW (the lower face) and HIGH pieces (the upper face, one an entry, both sides): 11 volumes
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);
		floor.boxFound = true;
		floor.boxLo[0] = -300;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -300 - 1948;
		floor.boxHi[0] = 12300;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 3300 + 1948;

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 12400, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.list.size() == 11, "THE TUBE: 11 volumes (floor, LOW, get-out, 2 entries, 2 HIGH a side, 2 exits) (" + std::to_string((long long)plan.list.size()) + ")");

		if (plan.list.size() == 11)
		{
			host::WaypointRec const &fl = plan.list[0], &low = plan.list[1], &e0 = plan.list[3], &e1 = plan.list[4], &hA = plan.list[5], &hB = plan.list[6], &sA = plan.list[7], &sB = plan.list[8];

			// (the box goes 1,948 out, but two 45-degree faces in a 3 m room meet 1,500 out: the reach is capped there)
			Check(low.min[2] == 3200 && low.max[2] == 5001 && low.min[1] == 100 && low.max[1] == 2201, "LOW: 100 into the room to 200 past the vertex (half the room's height out), 400 over the vertex's "
				"height (clear of the climb-side HIGH's aim) down to 200 under the floor (" + Said(low) + ")");
			Check(low.centre[0] == 6000 && low.centre[1] == 500 && low.centre[2] == 4900, "LOW's centre 100 past the vertex, at its height (" + std::to_string((long long)low.centre[1]) + ", " +
				std::to_string((long long)low.centre[2]) + "): from the foot out and up, INTO the lower face");
			Check(hA.min[0] == -300 && hA.max[0] == 6001 && hB.min[0] == 6000 && hB.max[0] == 12301 && hA.min[1] == -1200 && hA.max[1] == 500, "HIGH on the climb side, the side's whole length split at the entries' line, ceiling to the vertex");
			// (Vasquez's rule 2, 11-20, from the game's own frames at gencor01's climb, alien a1 f85-f108, with Rebellion's NPC_IsObstructed: the climber lands on the upper face with its
			// up axis 60-80 degrees behind it, so an aim far up the face is turned INTO it, 11 frames, Avoidance; an aim 1000 up the face from the crease and 300 under it, 400 along
			// the tube, gives 1. Here: the crease 1,500 out at y 500, the face 45 degrees: 1000 up it is 707 in and up, 300 under it 212 back out and down)
			// (along: Vasquez 11-20 on Frost's run 174100, two arrivals replayed: a climber that JUMPED into the climb wedges in the crease, its y row behind, and an aim 600 or
			// 1500 along gets Avoidance; 2500 along, the move runs along the crease while the y row turns, and both arrivals pass the entry)
			Check(hA.centre[0] == 3500 && hB.centre[0] == 8500 && hA.centre[2] == 3880 && hA.centre[1] == 5, "their centres 2500 either side of the climb's middle, 1000 up the upper face from the crease "
				"and 300 under it (Vasquez's rule 2) (" + std::to_string((long long)hA.centre[0]) + ", " + std::to_string((long long)hA.centre[1]) + ", " + std::to_string((long long)hA.centre[2]) + ")");
			Check(HigherHeld(plan.list, 5).empty() && HigherHeld(plan.list, 6).empty(), "at each climb-side HIGH's aim the alien's body reaches no higher-numbered box (the game's own "
				"box: at 400 along, the other half; Vasquez on 5444c5b4, \"never held\") (" + HigherHeld(plan.list, 5) + " /" + HigherHeld(plan.list, 6) + ")");
			Check(sA.min[2] == -2000 && sA.max[2] == -199 && sA.centre[2] == -301, "the other side's HIGH (the jump catchers), its centre just past the floor's edge");
			Check(FlagsOf(fl, 1) == 5 && FlagsOf(low, 0) == 6, "floor -> LOW only");
			Check(FlagsOf(low, 5) == 5 && FlagsOf(low, 6) == 5 && FlagsOf(low, 3) == -1 && FlagsOf(low, 4) == -1, "LOW -> both climb-side HIGH pieces, never straight to an entry");
			Check(FlagsOf(hA, 3) == 5 && FlagsOf(hB, 4) == 5 && FlagsOf(hA, 4) == -1 && FlagsOf(hB, 3) == -1, "each climb-side piece -> its own entry only");
			Check(FlagsOf(sA, 3) == 4 && FlagsOf(sB, 4) == 4 && FlagsOf(sA, 4) == -1, "each other-side piece <-> its own entry (one link, both ways)");
			std::string where;

			for (size_t b = 0; b < plan.list.size(); b++)
			{
				for (size_t o = 0; o < plan.list.size(); o++)
				{
					where += (o != b && Holds(plan.list[o], plan.list[b].centre)) ? " box " + std::to_string((long long)b) + "'s in box " + std::to_string((long long)o) : std::string();
				}
			}

			Check(plan.parts == 1 && plan.centresShared == 0, "one network, no centre shared (" + std::to_string((long long)plan.centresShared) + where + ")");

			int32_t const foot[3] = { 6000, 2000, 3000 }, middle[3] = { 3000, 2000, 1500 }, top[3] = { 6000, -100, 3100 };
			int32_t const far1[3] = { exits[1][0], exits[1][1], exits[1][2] }, far0[3] = { exits[0][0], exits[0][1], exits[0][2] };
			bool atFoot = true, fromMiddle = true, atTop = true;

			for (uint32_t s = 0; s < 16; s++)
			{
				host::StockChoice const a = host::Stock_Choose(plan.list, 1.0, foot, far1, true, s);
				host::StockChoice const b = host::Stock_Choose(plan.list, 1.0, middle, far1, true, s);
				host::StockChoice const c = host::Stock_Choose(plan.list, 1.0, top, far0, true, s);

				atFoot = atFoot && a.routed && a.start == 1 && a.crawls;   // held by the floor and LOW: it heads for LOW's centre (out and up)
				fromMiddle = fromMiddle && b.routed && b.second == 1 && b.crawls;
				atTop = atTop && c.routed && c.start == 3 && c.crawls;   // at the climb's top, held by HIGH and the entries: the start is an entry, never a piece
			}

			Check(atFoot, "AT THE FOOT (held by the floor and LOW), every alien starts at LOW: it heads for LOW's centre, out and up into the face, crawling");
			Check(fromMiddle, "from the corridor's middle, every alien's route is LOW next, crawling");
			Check(atTop, "AT THE CLIMB'S TOP, heading back for door A: every alien starts at ENTRY A, not a HIGH piece (no tie)");
		}
	}

	// ---- A T (Drake's hand6e SHED10): a bar along x (x 0..12000, z 0..3000), doorways W (x -300) and E (x 12300); a funnel off its south side (x 4800..7200, z -3600..-600)
	// to a doorway S (z -3900). 10 volumes: 2 floors, 1 climb-in (the bar's north side; the funnel none), the get-out over the junction, 3 entries, 3 exits; the funnel's
	// entry linked to ONE bar entry (W's, the lower along the bar), not to E's (the tie at the climb's top)
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);
		Cells(&floor, 4800, 7200, -3600, -600);

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 12400, 750, 1500 }, { 6000, 750, -4000 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0), Door(6000, -3900, 0.0, -1.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "junction (T)", "THE T is shaped, a junction (T) (" + plan.volumeKind + (plan.notes.empty() ? std::string() : ": " + plan.notes[0]) + ")");
		Check(plan.list.size() == 10, "10 volumes (" + std::to_string((long long)plan.list.size()) + ")");

		if (plan.list.size() == 10 && plan.doorBox.size() == 3)
		{
			int const eW = plan.doorBox[0], eE = plan.doorBox[1], eS = plan.doorBox[2];
			size_t climbs = 0;

			for (size_t b = 0; b < plan.list.size(); b++)
			{
				climbs += plan.climbing[b] && plan.list[b].max[1] - plan.list[b].min[1] > 2000 && plan.list[b].max[0] - plan.list[b].min[0] == 2401 ? 1 : 0;
			}

			Check(climbs == 1, "one climb-in (the funnel's floor walks into the bar's)");
			Check(eW >= 0 && eE >= 0 && eS >= 0 && FlagsOf(plan.list[(size_t)eS], eW) == 4 && FlagsOf(plan.list[(size_t)eS], eE) == -1,
				"the funnel's entry linked to W's only, both ways (no E <-> S: Drake's hand6e)");
			Check(eS >= 0 && eW >= 0 && plan.list[(size_t)eS].centre[0] == plan.list[(size_t)eW].centre[0] && plan.list[(size_t)eS].centre[2] == -2700,
				"its centre straight across from W's, 2.4 m into the funnel (" + std::to_string((long long)plan.list[(size_t)eS].centre[0]) + ", " +
				std::to_string((long long)plan.list[(size_t)eS].centre[2]) + ")");
			Check(plan.parts == 1 && plan.exitsCovered == 3 && plan.centresShared == 0, "one network, 3 exits covered, no centre shared");

			int32_t const hangE[3] = { 10000, -100, 1500 }, toS[3] = { exits[2][0], exits[2][1], exits[2][2] };
			bool viaW = true;

			for (uint32_t s = 0; s < 16; s++)
			{
				host::StockChoice const c = host::Stock_Choose(plan.list, 1.0, hangE, toS, true, s);

				viaW = viaW && c.routed && c.second == eW && c.crawls;
			}

			Check(viaW, "hanging in E's half, heading for S: every alien goes by W's entry, crawling (no tie)");
		}
	}

	// ---- THE OWNER'S SHED01 (Drake's hand6g, the owner's gold standard 11-20): a tube corner on 300 mm cells whose inner and outer corners are DIAGONAL in plan (the
	// cells a staircase; the cell map on 9a4a5b7c cut them into 4 rectangles). Arm 1 along x (rows 13..38, the west doorway), arm 2 along z (columns 13..38, the south
	// doorway); the outer (north-east) corner cut off, the inner one's diagonal floor west of arm 2. hand6g: FLOOR 1 arm 1 with the corner square, FLOOR 2 arm 2's own part
	// at its doorway's band (a box can't follow a diagonal)
	{
		host::AutoPlotPlan const plan = Shed01(Hand6g());   // hand6g's own counts: a climb on each arm

		Check(plan.volumes && plan.volumeKind == "corner", "THE OWNER'S SHED01 (a tube corner, diagonal in plan) is shaped, a corner (" + plan.volumeKind +
			(plan.notes.empty() ? std::string() : ": " + plan.notes[0]) + ")");
		Check(plan.floorBoxes == 2, "2 floors, hand6g's two arms (" + std::to_string((long long)plan.floorBoxes) + ")");

		if (plan.list.size() >= 2)
		{
			Check(plan.list[0].min[0] == -150 && plan.list[0].max[0] == 11551 && plan.list[0].min[2] == 3750 && plan.list[0].max[2] == 11551,
				"FLOOR 1: arm 1 with the corner square, over the outer corner's diagonal too (x -150..11550, z 3750..11550) (" + Said(plan.list[0]) + ")");
			Check(plan.list[1].min[0] == 3750 && plan.list[1].max[0] == 11551 && plan.list[1].min[2] == -150 && plan.list[1].max[2] == 3751,
				"FLOOR 2: arm 2's own part, its doorway's band (x 3750..11550, z -150..3750), the inner diagonal's floor left out (" + Said(plan.list[1]) + ")");
		}

		// hand6g's catchers: the north side split at arm 2's west edge (hNA over arm 1's own part, both ways with ENTRY A; hNB over the corner square, its centre over the
		// climb, up into ENTRY B), ONE east piece over the whole side (both floors) up into ENTRY B, its centre over the east climb; none on the inner diagonal sides
		Check(plan.list.size() == 12, "12 volumes: 2 floors, 2 LOW, 3 HIGH, the get-out, 2 entries, 2 exits (" + std::to_string((long long)plan.list.size()) + ")");

		int hNA = -1, hNB = -1, hE = -1, lN = -1, lE = -1, highs = 0;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.min[1] == -1200 && w.max[1] == 500)
			{
				highs++;
				hNA = w.min[2] == 11450 && w.min[0] == -150 && w.max[0] == 3751 ? (int)b : hNA;
				hNB = w.min[2] == 11450 && w.min[0] == 3750 && w.max[0] == 11551 ? (int)b : hNB;
				hE = w.min[0] == 11450 && w.min[2] == -150 && w.max[2] == 11551 ? (int)b : hE;
			}

			if (plan.climbing[b] && w.min[1] == 100 && w.max[1] == 2201)
			{
				lN = w.min[2] == 11450 ? (int)b : lN;
				lE = w.min[0] == 11450 ? (int)b : lE;
			}
		}

		Check(highs == 3 && hNA >= 0 && hNB >= 0 && hE >= 0, "3 HIGH: the north side's two (split at x 3750) and one east over the whole side (" + std::to_string((long long)highs) +
			" HIGH; north A " + std::to_string((long long)hNA) + ", north B " + std::to_string((long long)hNB) + ", east " + std::to_string((long long)hE) + ")");

		int const cA = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, cB = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

		if (hNA >= 0 && hNB >= 0 && hE >= 0 && lN >= 0 && lE >= 0 && cA >= 0 && cB >= 0)
		{
			Check(FlagsOf(plan.list[(size_t)hNA], cA) == 4 && FlagsOf(plan.list[(size_t)cA], hNA) == 4 && FlagsOf(plan.list[(size_t)hNA], cB) == -1,
				"arm 1's north piece <-> ENTRY A only (a jump catcher)");
			Check(FlagsOf(plan.list[(size_t)hNB], cB) == 5 && FlagsOf(plan.list[(size_t)hNB], cA) == -1 && FlagsOf(plan.list[(size_t)lN], hNB) == 5,
				"the corner square's north piece: the north LOW -> it -> ENTRY B only (hand6g: one entry each, no tie)");
			Check(FlagsOf(plan.list[(size_t)hE], cB) == 5 && FlagsOf(plan.list[(size_t)hE], cA) == -1 && FlagsOf(plan.list[(size_t)lE], hE) == 5,
				"the east piece: the east LOW -> it -> ENTRY B only");
			Check(plan.list[(size_t)hNB].centre[0] == 5700 && plan.list[(size_t)hE].centre[2] == 1800 && plan.list[(size_t)hNA].centre[0] == 1800,
				"centres: the corner square's north piece and the east one straight over their climbs (x 5700, z 1800), arm 1's at its middle (x 1800) (" +
				std::to_string((long long)plan.list[(size_t)hNB].centre[0]) + ", " + std::to_string((long long)plan.list[(size_t)hE].centre[2]) + ", " +
				std::to_string((long long)plan.list[(size_t)hNA].centre[0]) + ")");
		}
		else
		{
			Check(false, "hand6g's corner pieces all found (north A " + std::to_string((long long)hNA) + ", north B " + std::to_string((long long)hNB) + ", east " + std::to_string((long long)hE) +
				", LOW north " + std::to_string((long long)lN) + ", LOW east " + std::to_string((long long)lE) + ")");
		}

		std::string where;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			for (size_t o = 0; o < plan.list.size(); o++)
			{
				where += (o != b && Holds(plan.list[o], plan.list[b].centre)) ? " box " + std::to_string((long long)b) + "'s in box " + std::to_string((long long)o) : std::string();
			}
		}

		Check(plan.parts == 1 && plan.exitsCovered == 2 && plan.centresShared == 0, "one network, both exits covered, no centre shared (" + std::to_string((long long)plan.centresShared) +
			where + ")");
	}

	// ---- THE OWNER'S COUNTS (11-20 on hand6g: "too many climb points"; "tunable"): the defaults keep ONE climb an AI module. SHED01 at the defaults: the bigger arm's climb
	// (the north one over the corner square), the east one goes and its HIGH stays as a jump catcher with ENTRY B; 11 volumes, every box reaching every other
	{
		host::AutoPlotPlan const plan = Shed01(host::AutoPlotVolumeCounts());
		int lows = 0, lN = -1, hE = -1;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.min[1] == 100 && w.max[1] == 2201)
			{
				lows++;
				lN = w.min[2] == 11450 ? (int)b : lN;
			}

			hE = plan.climbing[b] && w.min[1] == -1200 && w.max[1] == 500 && w.min[0] == 11450 && w.min[2] == -150 && w.max[2] == 11551 ? (int)b : hE;
		}

		Check(plan.volumes && plan.list.size() == 11 && lows == 1 && lN >= 0, "THE DEFAULT COUNTS on SHED01: 11 volumes, ONE climb, the north one on the bigger arm (" +
			std::to_string((long long)plan.list.size()) + " volumes, " + std::to_string((long long)lows) + " LOW)");

		int const cB = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

		Check(hE >= 0 && cB >= 0 && FlagsOf(plan.list[(size_t)hE], cB) == 4 && FlagsOf(plan.list[(size_t)cB], hE) == 4,
			"the east HIGH stays, a jump catcher both ways with ENTRY B");
		Check(AllReach(plan.list) && plan.parts == 1 && plan.centresShared == 0, "every box reaches every other, one network, no centre shared");
	}

	// ---- THE COUNTS AT 0 on the tube: climb-ins 0 (no climb: each floor goes up straight into the entry over it, so every box still reaches every other), and get-outs 0
	// (no get-out: the entries drop to the floor under them)
	for (int const which : { 0, 1 })
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);
		floor.boxFound = true;
		floor.boxLo[0] = -300;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -300 - 1948;
		floor.boxHi[0] = 12300;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 3300 + 1948;

		host::AutoPlotVolumeCounts counts;

		(which == 0 ? counts.climbIns : counts.getOuts) = 0;

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 12400, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0, counts);
		int lows = 0, getOuts = 0;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			lows += plan.climbing[b] && w.min[1] == 100 && w.max[1] == 2201 ? 1 : 0;
			getOuts += plan.climbing[b] && w.min[1] == 1 && w.max[1] == 501 ? 1 : 0;
		}

		int const e0 = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, e1 = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

		if (which == 0)
		{
			Check(plan.volumes && plan.list.size() == 10 && lows == 0 && getOuts == 1, "CLIMB-INS 0 on the tube: 10 volumes, no LOW, the get-out kept (" +
				std::to_string((long long)plan.list.size()) + ", " + std::to_string((long long)lows) + " LOW)");
			Check(e0 >= 0 && e1 >= 0 && (FlagsOf(plan.list[0], e0) == 5) != (FlagsOf(plan.list[0], e1) == 5), "the floor -> the one entry over it most, one way");
		}
		else
		{
			Check(plan.volumes && plan.list.size() == 10 && lows == 1 && getOuts == 0, "GET-OUTS 0 on the tube: 10 volumes, no get-out, the climb kept (" +
				std::to_string((long long)plan.list.size()) + ", " + std::to_string((long long)getOuts) + " get-outs)");
			Check(e0 >= 0 && e1 >= 0 && FlagsOf(plan.list[(size_t)e0], 0) == 5 && FlagsOf(plan.list[(size_t)e1], 0) == 5, "each entry drops to the floor under it, one way");
		}

		Check(AllReach(plan.list) && plan.parts == 1 && plan.centresShared == 0, std::string(which == 0 ? "climb-ins 0" : "get-outs 0") + ": every box reaches every other, one network, no centre shared");
	}

	// ---- THE OWNER'S SHED10 (Drake's hand6g, the gold standard): a tube T on 300 mm cells. The bar along x (rows 13..38, columns 0..51; doorways W and E), its north row
	// with a NOTCH (no floor cells 2.4 m along under the north wall's middle: the cell map on 9a4a5b7c); the funnel south (columns 13..38 at its doorway S), its sides
	// DIAGONAL in plan, widening into the bar. hand6g: FLOOR M the whole bar (to the north row), FLOOR S the funnel at its doorway's band
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 13; iz <= 38; iz++)
		{
			for (int32_t ix = 0; ix <= 51; ix++)
			{
				if (iz != 38 || ix < 15 || ix > 22)
				{
					Cell300(&floor, ix, iz);
				}
			}
		}

		for (int32_t iz = 0; iz <= 12; iz++)
		{
			for (int32_t ix = iz <= 1 ? 13 : 14 - iz; ix <= (iz <= 1 ? 38 : 37 + iz); ix++)
			{
				Cell300(&floor, ix, iz);
			}
		}

		floor.boxFound = true;   // a tube: the AI module's box 1,948 past the bar's north side and the funnel's (the doorways' sides at the floor)
		floor.boxLo[0] = -150;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -150;
		floor.boxHi[0] = 15450;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 11550 + 1948;

		std::vector<std::vector<int32_t> > exits = { { -250, 750, 7650 }, { 15550, 750, 7650 }, { 7650, 750, -250 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-150, 7650, -1.0, 0.0), Door(15450, 7650, 1.0, 0.0), Door(7650, -150, 0.0, -1.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "junction (T)", "THE OWNER'S SHED10 (a tube T, its funnel diagonal in plan) is shaped, a junction (T) (" + plan.volumeKind +
			(plan.notes.empty() ? std::string() : ": " + plan.notes[0]) + ")");
		Check(plan.floorBoxes == 2, "2 floors, the bar and the funnel (" + std::to_string((long long)plan.floorBoxes) + ")");

		if (plan.list.size() >= 2)
		{
			Check(plan.list[0].min[0] == -150 && plan.list[0].max[0] == 15451 && plan.list[0].min[2] == 3750 && plan.list[0].max[2] == 11551,
				"FLOOR M: the whole bar, its notched north row too (x -150..15450, z 3750..11550) (" + Said(plan.list[0]) + ")");
			Check(plan.list[1].min[0] == 3750 && plan.list[1].max[0] == 11551 && plan.list[1].min[2] == -150 && plan.list[1].max[2] == 3751,
				"FLOOR S: the funnel at its doorway's band (x 3750..11550, z -150..3750), never a diagonal side's line (" + Said(plan.list[1]) + ")");
		}

		// hand6g's catchers: the north side's two halves only, each up into its own entry; NONE on the bar's south side (the funnel opens off it: the 9a4a5b7c run's
		// south catchers over the funnel held the funnel entry's centre, refused)
		Check(plan.list.size() == 12, "12 volumes: 2 floors, LOW, 2 HIGH, the get-out, 3 entries, 3 exits (" + std::to_string((long long)plan.list.size()) + ")");

		int hNW = -1, hNE = -1, highs = 0, south = 0;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.min[1] == -1200 && w.max[1] == 500)
			{
				highs++;
				south += w.max[2] <= 3751 + 200 ? 1 : 0;
				hNW = w.min[2] == 11450 && w.min[0] == -150 && w.max[0] == 7651 ? (int)b : hNW;
				hNE = w.min[2] == 11450 && w.min[0] == 7650 && w.max[0] == 15451 ? (int)b : hNE;
			}
		}

		Check(hNW >= 0 && hNE >= 0 && HigherHeld(plan.list, (size_t)hNW).empty() && HigherHeld(plan.list, (size_t)hNE).empty(), "at each half's aim the alien's body reaches no "
			"higher-numbered box (Vasquez on 5444c5b4: SHED10 held by 2, 7 and 8, never moved on) (" + (hNW >= 0 ? HigherHeld(plan.list, (size_t)hNW) : std::string("-")) + " /" +
			(hNE >= 0 ? HigherHeld(plan.list, (size_t)hNE) : std::string("-")) + ")");
		Check(highs == 2 && hNW >= 0 && hNE >= 0 && south == 0, "2 HIGH, the north side's halves; none on the bar's south side (" + std::to_string((long long)highs) + " HIGH, " +
			std::to_string((long long)south) + " south)");

		if (hNW >= 0 && hNE >= 0 && plan.doorBox.size() == 3)
		{
			Check(FlagsOf(plan.list[(size_t)hNW], plan.doorBox[0]) == 5 && FlagsOf(plan.list[(size_t)hNE], plan.doorBox[1]) == 5, "each half up into its own entry (W's, E's)");
		}

		// the get-out (1001 under the ceiling to 1500 over the floor) is over the bar: W's and E's entries lie over it and go down into it; the funnel's only touches it
		// along a line (hand6g: no cS -> g1; d060b295's gold compare: EXTRA g1>cS, cS>g1)
		int getOut = -1;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			getOut = plan.climbing[b] && plan.list[b].min[1] == 1 && plan.list[b].max[1] == 501 ? (int)b : getOut;
		}

		if (getOut >= 0 && plan.doorBox.size() == 3 && plan.doorBox[0] >= 0 && plan.doorBox[1] >= 0 && plan.doorBox[2] >= 0)
		{
			Check(FlagsOf(plan.list[(size_t)plan.doorBox[0]], getOut) == 5 && FlagsOf(plan.list[(size_t)plan.doorBox[1]], getOut) == 5 &&
				FlagsOf(plan.list[(size_t)plan.doorBox[2]], getOut) == -1, "W's and E's entries -> the get-out; the funnel's entry not linked to it (" +
				std::to_string((long long)FlagsOf(plan.list[(size_t)plan.doorBox[2]], getOut)) + ")");
		}
		else
		{
			Check(false, "the get-out and the 3 entries are there");
		}

		Check(plan.parts == 1 && plan.exitsCovered == 3 && plan.centresShared == 0, "one network, 3 exits covered, no centre shared (" + std::to_string((long long)plan.centresShared) + ")");
	}

	// ---- THE OWNER'S GENCOR02 (d060b295's run: "box 1 cannot reach box 0"): a tube corridor on 300 mm cells (rows 0..25, columns 0..17) whose west doorway lies 1.5 m
	// past the band's end, past a SILL block of floor (columns -5..-1, rows 4..21) the rectangles leave out (a sliver). The west side is the doorway's: no climb there
	// (the climb went on it, its LOW up into nothing); the climb on a long side
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 0; iz <= 25; iz++)
		{
			for (int32_t ix = (iz >= 4 && iz <= 21) ? -5 : 0; ix <= 17; ix++)
			{
				Cell300(&floor, ix, iz);
			}
		}

		floor.boxFound = true;   // a tube: the box 1,948 past both long sides
		floor.boxLo[0] = -1650;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -150 - 1948;
		floor.boxHi[0] = 5250;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 7650 + 1948;

		std::vector<std::vector<int32_t> > exits = { { -1750, 750, 3750 }, { 5350, 750, 3750 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-1650, 3750, -1.0, 0.0), Door(5250, 3750, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "corridor", "THE OWNER'S GENCOR02 (a doorway past a sill) is shaped, a corridor (" + plan.volumeKind + ")");

		int lows = 0, westLows = 0;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.min[1] == 100 && w.max[1] == 2201)
			{
				lows++;
				westLows += w.max[0] <= 0 + 200 ? 1 : 0;
			}
		}

		Check(lows == 1 && westLows == 0, "one LOW, on a long side, none on the doorway's side past which the floor goes on (" + std::to_string((long long)lows) + " LOW, " +
			std::to_string((long long)westLows) + " west)");
		Check(plan.list.size() == 11 && plan.parts == 1 && plan.centresShared == 0, "11 volumes as a tube corridor, one network, no centre shared (" +
			std::to_string((long long)plan.list.size()) + ", parts " + std::to_string((long long)plan.parts) + ")");
	}

	// ---- A DOORWAY FAR PAST ITS FLOOR (Drake's count on d523d3b3: entries 2 to 20 m past a floor's edge, out to its doorway, IIroom, EXT-01): the same band with NO sill,
	// its west doorway 3 m past the band's end (a ramp or a passage with no floor cells). Its west side is the longest, but it is the way on to the doorway: no climb
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 0; iz <= 25; iz++)
		{
			for (int32_t ix = 0; ix <= 17; ix++)
			{
				Cell300(&floor, ix, iz);
			}
		}

		floor.boxFound = true;
		floor.boxLo[0] = -3150;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -150 - 1948;
		floor.boxHi[0] = 5250;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 7650 + 1948;

		std::vector<std::vector<int32_t> > exits = { { -3250, 750, 3750 }, { 5350, 750, 3750 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-3150, 3750, -1.0, 0.0), Door(5250, 3750, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		int lows = 0, westLows = 0;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.min[1] == 100 && w.max[1] == 2201)
			{
				lows++;
				westLows += w.max[0] <= 0 + 200 ? 1 : 0;
			}
		}

		Check(plan.volumes && lows == 1 && westLows == 0, "A DOORWAY 3 M PAST ITS FLOOR: one LOW, on a long side, none on the side facing the doorway (" + std::to_string((long long)lows) +
			" LOW, " + std::to_string((long long)westLows) + " west)");
	}

	// ---- A DOORWAY PAST THE FLOOR'S CORNER (Drake on 544831f0: EXT-01, yard19; EXT-01's exit x -31844 z 47759, its floor x -26100..-22500 z 41400..46200): the band's west
	// doorway 5 m out past its west side and 1.5 m past its north side, outside both sides' spans, so neither faced it and the LOW climbed west toward it, under the
	// entry that reaches out to it. The doorway is on the side it lies farthest out past (west): no climb there
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 0; iz <= 25; iz++)
		{
			for (int32_t ix = 0; ix <= 17; ix++)
			{
				Cell300(&floor, ix, iz);
			}
		}

		floor.boxFound = true;
		floor.boxLo[0] = -5150;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -150 - 1948;
		floor.boxHi[0] = 5250;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 9150 + 1948;

		std::vector<std::vector<int32_t> > exits = { { -5250, 750, 9150 }, { 5350, 750, 3750 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-5150, 9150, -1.0, 0.0), Door(5250, 3750, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		int lows = 0, westLows = 0;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			host::WaypointRec const &w = plan.list[b];

			if (plan.climbing[b] && w.min[1] == 100 && w.max[1] == 2201)
			{
				lows++;
				westLows += w.max[0] <= 0 + 200 ? 1 : 0;
			}
		}

		Check(plan.volumes && lows == 1 && westLows == 0, "A DOORWAY PAST THE FLOOR'S CORNER: one LOW, none on the side it lies farthest out past (" + std::to_string((long long)lows) +
			" LOW, " + std::to_string((long long)westLows) + " west; " + plan.volumeKind + ")");
	}

	// ---- OPEN SKY (Bishop 11-20 ~21:40, for Hudson's full Genshd1 run: the whole-level plot ac998c8d left 60 open-sky modules NOT SHAPED, "no rule for it yet", so they kept
	// stock's networks: OPEN-*, yard*, STREETB-*, PROC*, trench*, the streets and yards aliens walk). A floor with NO ceiling over it, 6 x 3 m, a doorway at each end: the
	// levels' rule's floor walk (floor boxes, an exit box at each doorway, floor <-> exit; no climb, no ceiling lane), every box reaching every other
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 300;

		for (int32_t iz = 0; iz <= 9; iz++)
		{
			for (int32_t ix = 0; ix <= 19; ix++)
			{
				floor.cells.push_back(std::vector<int32_t>{ ix * 300, 2000, iz * 300 });   // (no ceiling over any)
			}
		}

		std::vector<std::vector<int32_t> > exits = { { -250, 750, 1350 }, { 5950, 750, 1350 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-150, 1350, -1.0, 0.0), Door(5850, 1350, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		int const floors = (int)plan.floorBoxes;   // (the levels' rule counts its floor boxes; it names no volume roles)
		int climbing = 0;

		for (size_t b = 0; b < plan.list.size() && b < plan.climbing.size(); b++)
		{
			climbing += plan.climbing[b] ? 1 : 0;
		}

		Check(plan.volumes && plan.volumeKind.find("open sky") != std::string::npos && floors >= 1 && climbing == 0, "OPEN SKY: shaped by the levels' floor walk (" + plan.volumeKind +
			", " + std::to_string((long long)floors) + " floor boxes, " + std::to_string((long long)climbing) + " climbing" + (plan.notes.empty() ? std::string() : "; " + plan.notes[0]) + ")");
		Check(plan.exitsCovered == 2 && plan.exitBox.size() == 2 && plan.exitBox[0] >= 0 && plan.exitBox[1] >= 0, "both doorways' exits covered, each by its own box");
		Check(!plan.list.empty() && AllReach(plan.list) && plan.parts == 1 && plan.centresShared == 0, "every box reaches every other, one network, no centre shared");
	}

	// ---- A DOOR SLAB (Drake's count on d523d3b3, 8 modules "box 1 cannot reach box 0": medoor01, medoor02, shdr01, door01, MNDR01, opsdr01, opsdr02): a door frame's
	// module, its floor one 300 mm cell deep along the passage (x) and 6.9 m across (z), a doorway on each face. As a corridor its climb went on a 300 mm end and led up
	// into nothing. THE DOORWAY KIND: the floor, ONE entry over the whole slab past both doorways, the two exits; no climb, no get-out. Floor <-> entry (the way up, so
	// every box reaches every other), entry -> each exit (a hanging alien goes on along the ceiling), each exit <-> floor (walked through)
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 0; iz <= 22; iz++)
		{
			Cell300(&floor, 0, iz);
		}

		std::vector<std::vector<int32_t> > exits = { { 250, 750, 3300 }, { -250, 750, 3300 } };
		std::vector<host::AutoPlotDoor> doors = { Door(150, 3300, 1.0, 0.0), Door(-150, 3300, -1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && plan.volumeKind == "doorway", "A DOOR SLAB (300 mm deep) is shaped, a doorway (" + plan.volumeKind + (plan.notes.empty() ? std::string() : ": " + plan.notes[0]) + ")");
		Check(plan.list.size() == 4, "4 volumes: the floor, one entry, 2 exits (" + std::to_string((long long)plan.list.size()) + ")");

		if (plan.list.size() == 4 && plan.doorBox.size() == 2 && plan.exitBox.size() == 2 && plan.doorBox[0] >= 0 && plan.exitBox[0] >= 0 && plan.exitBox[1] >= 0)
		{
			int const en = plan.doorBox[0], x0 = plan.exitBox[0], x1 = plan.exitBox[1];
			host::WaypointRec const &fl = plan.list[0], &entry = plan.list[(size_t)en];

			Check(plan.doorBox[1] == en, "both doorways' entry is the one entry");
			Check(entry.min[0] <= 150 - 600 && entry.max[0] >= -150 + 600 + 1 && entry.min[2] == -150 && entry.max[2] == 6751 && entry.min[1] == -1200,
				"the entry: the whole slab, 600 past each doorway, 200 into the ceiling (" + Said(entry) + ")");
			Check(entry.centre[1] == -900 && entry.centre[2] == 3300, "its centre in the middle, 100 under the ceiling face (" + std::to_string((long long)entry.centre[1]) + ")");
			Check(FlagsOf(fl, en) == 4 && FlagsOf(entry, x0) == 5 && FlagsOf(entry, x1) == 5 && FlagsOf(plan.list[(size_t)x0], 0) == 4 && FlagsOf(plan.list[(size_t)x1], 0) == 4,
				"floor <-> entry, entry -> each exit, each exit <-> floor");
			Check(AllReach(plan.list), "every box reaches every other (Frost's checker)");
			Check(plan.parts == 1 && plan.exitsCovered == 2 && plan.centresShared == 0, "one network, both exits covered, no centre shared");
		}
	}

	// ---- A CLIMB ON A SEAM (the owner's doormp09 and newcol03 on 9f781c00, "box 2 cannot reach box 0"): a long shallow floor (10.8 x 1.5 m) with its doorway on its north side
	// and a second floor (4.2 m of it) off its south side, the other doorway past that. The south side was the long side left, its seam under half its length, so its LOW
	// went there; d060b295 gives a side another floor starts at no catchers, and the LOW led up into nothing. A side another floor starts at is no climb side either
	for (int const which : { 0, 1 })
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 5; iz <= 9; iz++)
		{
			for (int32_t ix = 0; ix <= 35; ix++)
			{
				Cell300(&floor, ix, iz);
			}
		}

		for (int32_t iz = 0; iz <= 4; iz++)   // 200 mm higher: a level of its own (a tread, kept: doormp09's second floor is 220 higher)
		{
			for (int32_t ix = 11; ix <= 24; ix++)
			{
				floor.cells.push_back(std::vector<int32_t>{ ix * 300, 1800, iz * 300 });
				floor.ceilings.push_back(std::vector<int32_t>{ ix * 300, -1000, iz * 300 });
			}
		}

		floor.boxFound = true;   // the AI module's box over both floors
		floor.boxLo[0] = -150;
		floor.boxLo[1] = -1000;
		floor.boxLo[2] = -150;
		floor.boxHi[0] = 10650;
		floor.boxHi[1] = 2000;
		floor.boxHi[2] = 2850;

		std::vector<std::vector<int32_t> > exits = { { 5100, 750, 2950 }, { 5250, 750, -250 } };
		std::vector<host::AutoPlotDoor> doors = { Door(5100, 2850, 0.0, 1.0), Door(5250, -150, 0.0, -1.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0, which == 0 ? host::AutoPlotVolumeCounts() : Hand6g());

		Check(plan.volumes && AllReach(plan.list), std::string("A CLIMB ON A SEAM (") + (which == 0 ? "the default counts" : "hand6g's counts") +
			"): every box reaches every other, no LOW up into nothing (" + plan.volumeKind + ", " + std::to_string((long long)plan.list.size()) + " volumes)");
	}

	// ---- NOT A DOOR SLAB: the same shallow floor, its doorways 3 m apart in height (the owner's STEPS04 on 9f781c00, a staircase's landing 900 mm deep: its exits at y 5844
	// and 2101, read as a doorway with one entry 4.6 m tall). A door slab is flat: its doorways at one height, within kAutoPlotLevelStep (Frost's stairs rule takes the rest)
	{
		host::AutoPlotFloor floor;

		for (int32_t iz = 0; iz <= 22; iz++)
		{
			Cell300(&floor, 0, iz);
		}

		std::vector<std::vector<int32_t> > exits = { { 250, 750, 3300 }, { -250, -2250, 3300 } };
		std::vector<host::AutoPlotDoor> doors = { Door(150, 3300, 1.0, 0.0), Door(-150, 3300, -1.0, 0.0) };

		doors[1].middle[1] = 750 - 3000;   // the far doorway 3 m up a stair
		doors[1].soffit[1] = -500 - 3000;

		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumeKind != "doorway", "DOORWAYS 3 M APART IN HEIGHT are no door slab (" + plan.volumeKind + ")");
	}

	// ---- TWO EXITS THROUGH ONE DOORWAY (the whole Genshd1 run on 9f781c00: tolift, "box 0 cannot reach box 4": two neighbouring AI modules through one doorway, so the bar of
	// its T had two "doorways" at one place, both entries the same half, and the funnel's entry touched neither). The later exit is a TWIN of the first: one doorway, one
	// entry, its point in the first one's exit box. A bar (x 0..12000, z 0..3000) with exits 1 and 2 at its east doorway, a funnel (x 4800..7200, z -3600..-600) with exit 0
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);
		Cells(&floor, 4800, 7200, -3600, -600);

		std::vector<std::vector<int32_t> > exits = { { 6000, 750, -4000 }, { 12400, 750, 1500 }, { 12400, 750, 1700 } };
		std::vector<host::AutoPlotDoor> doors = { Door(6000, -3900, 0.0, -1.0), Door(12300, 1500, 1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && AllReach(plan.list) && plan.parts == 1 && plan.centresShared == 0, "TWO EXITS THROUGH ONE DOORWAY: shaped, every box reaching every other, one network, no "
			"centre shared (" + plan.volumeKind + ", " + std::to_string((long long)plan.list.size()) + " volumes, shared " + std::to_string((long long)plan.centresShared) + ")");
		Check(plan.exitBox.size() == 3 && plan.exitBox[1] >= 0 && plan.exitBox[1] == plan.exitBox[2] && plan.doorBox[1] == plan.doorBox[2] && plan.exitsCovered == 3,
			"the twin's exit point is in the first one's exit box, and it has the same entry; all 3 exits covered");
	}

	// ---- TWO EXITS THROUGH ONE DOORWAY AT TWO HEIGHTS (the whole-level plot on 242d5fc8: start and tolift, on Hudson's route, refused "box 3: its centre is in box 4
	// too": two exits through one doorway with their points 780 mm apart in height, start's 14417 and 15196, so the twin test's 450 missed them and each had an entry
	// of its own, the same box). The bar above with its twin's middle and point 780 higher: still one doorway, one entry, no centre shared
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 12000, 0, 3000);
		Cells(&floor, 4800, 7200, -3600, -600);

		std::vector<std::vector<int32_t> > exits = { { 6000, 750, -4000 }, { 12400, 750, 1500 }, { 12400, -30, 1700 } };
		std::vector<host::AutoPlotDoor> doors = { Door(6000, -3900, 0.0, -1.0), Door(12300, 1500, 1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };

		doors[2].middle[1] = -30;   // (780 over the first one's)

		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		Check(plan.volumes && !plan.list.empty() && AllReach(plan.list) && plan.parts == 1 && plan.centresShared == 0, "TWO EXITS THROUGH ONE DOORWAY AT TWO HEIGHTS (780 "
			"apart): one doorway, every box reaching every other, no centre shared (" + std::to_string((long long)plan.list.size()) + " volumes, shared " +
			std::to_string((long long)plan.centresShared) + ")");
		Check(plan.exitBox.size() == 3 && plan.exitBox[1] >= 0 && plan.exitBox[1] == plan.exitBox[2] && plan.doorBox[1] == plan.doorBox[2] && plan.exitsCovered == 3,
			"the twin's exit point is in the first one's exit box, and it has the same entry; all 3 exits covered");
	}

	// ---- A RAMP WEST OF THE FIRST CELL (Genshd1's MOD-01, 11-20: the first build never came back from it). The cells' grid starts at the first cell given, so cells west of
	// it have NEGATIVE indices; a rectangle whose floor rises more than kAutoPlotFloorRise is halved, and (ix0 + ix1) / 2 rounds toward zero: -5 and -4 gave -4, the same
	// rectangle for ever. 12 m of floor rising 2 m eastward (y down), the first cell its east end; walls along z -300 and 3300
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 600;

		for (int32_t x = 12000; x >= 0; x -= 600)
		{
			for (int32_t z = 0; z <= 3000; z += 600)
			{
				floor.cells.push_back(std::vector<int32_t>{ x, 2000 - x / 6, z });   // 100 mm higher each cell east
				floor.ceilings.push_back(std::vector<int32_t>{ x, -2000, z });
			}
		}

		host::AutoPlotWay way;

		way.alien = true;
		way.steps.push_back(Step(6000, 0, -300, AutoPlotSurface::Wall, 0, 0, 65536));
		way.steps.push_back(Step(6000, 0, 3300, AutoPlotSurface::Wall, 0, 0, -65536));

		std::vector<std::vector<int32_t> > exits = { { -400, 1200, 1500 }, { 12400, 0, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(12300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({ way }, exits, floor, doors, 1.0);
		bool flat = true;
		size_t floors = 0;

		for (size_t b = 0; b < plan.climbing.size(); b++)
		{
			if (!plan.climbing[b] && (int)b != plan.exitBox[0] && (int)b != plan.exitBox[1])
			{
				floors++;
				// a floor box is 1500 over its highest floor to 200 under its lowest: its height less 1700 is its floor's spread
				flat = flat && plan.list[b].max[1] - 1 - plan.list[b].min[1] - (host::kAutoPlotReach + host::kAutoPlotInto) <= host::kAutoPlotFloorRise;
			}
		}

		(void)flat;
		// (Frost 11-20: the levels' rule now shapes a stairs or levels, so it comes back shaped; the point of the case is that it comes back at all)
		Check(plan.volumeKind == "stairs or levels", "A RAMP WEST OF THE FIRST CELL comes back at all, named a stairs or levels (" + plan.volumeKind + (plan.volumes ? ", the levels' rule shaped it)" : ", not shaped)"));
	}

	// ---- A 1 M STEP between two columns of cells, the first cell given in the EAST column (so the west column's index is -1): the halving reaches the 2-cell-wide row
	// [-1, 0] and (-1 + 0) / 2 rounds to 0, the same rectangle; the fix halves as ix0 + (ix1 - ix0) / 2. Each column its own floor box
	{
		host::AutoPlotFloor floor;

		floor.cellEdge = 600;

		for (int32_t const x : { 600, 0 })
		{
			for (int32_t z = 0; z <= 3000; z += 600)
			{
				floor.cells.push_back(std::vector<int32_t>{ x, x == 0 ? 2000 : 1000, z });
				floor.ceilings.push_back(std::vector<int32_t>{ x, -2000, z });
			}
		}

		host::AutoPlotWay way;

		way.alien = true;
		way.steps.push_back(Step(300, 0, -300, AutoPlotSurface::Wall, 0, 0, 65536));

		std::vector<std::vector<int32_t> > exits = { { -400, 1200, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({ way }, exits, floor, doors, 1.0);

		Check(plan.volumes, "A 1 M STEP WEST OF THE FIRST CELL is shaped (it comes back at all)");
		// (its second column, ONE cell wide, now reads as a side's face, like a tube's lower face rows: left out, the stock dropout; the first column is the floor)
		Check(plan.cellsCovered == 6 && plan.cells == 12, "the first column's cells in its floor box, the one-cell row left out (" + std::to_string((long long)plan.cellsCovered) + " of " + std::to_string((long long)plan.cells) + ")");
	}

	// ---- OPEN SKY (no ceiling over the floor): never the old small boxes (Bishop's ruling 11-20); since Hudson's full-level run (Bishop 11-20 ~21:40) the levels' floor walk
	// shapes it: a few room-sized boxes (its floor and an exit box at each doorway), no climbing box
	{
		host::AutoPlotFloor floor;

		Cells(&floor, 0, 6000, 0, 3000);
		floor.ceilings.clear();

		std::vector<std::vector<int32_t> > exits = { { -400, 750, 1500 }, { 6400, 750, 1500 } };
		std::vector<host::AutoPlotDoor> doors = { Door(-300, 1500, -1.0, 0.0), Door(6300, 1500, 1.0, 0.0) };
		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		bool noClimb = true;

		for (size_t b = 0; b < plan.climbing.size(); b++)
		{
			noClimb = noClimb && !plan.climbing[b];
		}

		Check(plan.volumes && plan.volumeKind.find("open sky") != std::string::npos && plan.list.size() <= 6 && noClimb, "OPEN SKY: the levels' floor walk, a few room-sized "
			"boxes, never the old small ones, no climbing box (" + plan.volumeKind + ", " + std::to_string((long long)plan.list.size()) + " boxes" + (plan.notes.empty() ? std::string() :
			": " + plan.notes[0]) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_autoplot_volumes_test\n");
		return 1;
	}

	printf("passed host_autoplot_volumes_test\n");
	return 0;
}
