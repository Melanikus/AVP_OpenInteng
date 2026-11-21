// host_autoplot_plan_test.cpp: Auto-plot alien ways' generator (src/host/autoplot_plan.h) on made-up ways, no grid and no level: the stock rules each plan must keep (at most 99 boxes,
// every exit inside a box, one network, each centre inside its own box only, links both ways inside the list, 0x4 where a wall or a ceiling is), and the steps that make it.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_plan.h"

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

static host::AutoPlotStep Step(int32_t x, int32_t y, int32_t z, AutoPlotSurface surface, bool inside = true)
{
	host::AutoPlotStep s;

	s.place[0] = x; s.place[1] = y; s.place[2] = z;
	s.surface = surface;
	s.facing[0] = s.facing[1] = s.facing[2] = 0;

	if (surface == AutoPlotSurface::Floor)
	{
		s.facing[1] = -65536;   // out of a floor is up, -y
	}
	else if (surface == AutoPlotSurface::Ceiling)
	{
		s.facing[1] = 65536;
	}
	else
	{
		s.facing[0] = -65536;   // the tests' walls face -x
	}

	s.inside = inside;
	return s;
}

static host::AutoPlotWay FloorWay(int32_t x0, int32_t x1, int32_t z, int32_t step = 500, bool alien = false)
{
	host::AutoPlotWay way;

	way.alien = alien;

	for (int32_t x = x0; x0 <= x1 ? x <= x1 : x >= x1; x += x0 <= x1 ? step : -step)
	{
		way.steps.push_back(Step(x, 0, z, AutoPlotSurface::Floor));
	}

	return way;
}

// The game's own test of a place in a box (WaypointContainsPoint, as Frost's checker does it): each number times the level's scale with what's after the point cut off,
// inside = lowest <= place <= highest - 1
static bool GameHolds(host::WaypointRec const &box, int32_t const place[3], double scale)
{
	for (int a = 0; a < 3; a++)
	{
		double const p = trunc(place[a] * scale);

		if (p < trunc(box.min[a] * scale) || p > trunc(box.max[a] * scale) - 1.0)
		{
			return false;
		}
	}

	return true;
}

// every rule the plan must keep, whatever its notes say (each centre in its own box only among them); how many centres sit in another box
static size_t RulesKept(host::AutoPlotPlan const &plan, std::string const &what, double scale = 1.0)
{
	bool linksOk = true, roomOk = true;
	size_t shared = 0;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		host::WaypointRec const &wp = plan.list[i];

		for (int a = 0; a < 3; a++)
		{
			roomOk = roomOk && wp.max[a] - 1 > wp.min[a];
		}

		roomOk = roomOk && GameHolds(wp, wp.centre, scale);

		for (host::WaypointLinkRec const &l : wp.links)
		{
			bool back = false;

			linksOk = linksOk && l.index >= 0 && (size_t)l.index < plan.list.size() && (size_t)l.index != i && (l.flags & ~host::kLinkAlien) == 0;

			if (l.index >= 0 && (size_t)l.index < plan.list.size())
			{
				for (host::WaypointLinkRec const &r : plan.list[(size_t)l.index].links)
				{
					back = back || (r.index == (int32_t)i && r.flags == l.flags);
				}
			}

			linksOk = linksOk && back;
		}

		bool inOther = false;

		for (size_t j = 0; j < plan.list.size(); j++)
		{
			host::WaypointRec const &o = plan.list[j];

			inOther = inOther || (j != i && GameHolds(o, wp.centre, scale));
		}

		shared += inOther ? 1 : 0;
	}

	Check(plan.list.size() <= 99, what + ": at most 99 boxes (" + std::to_string(plan.list.size()) + ")");
	Check(roomOk, what + ": every box has room on every axis, its centre inside it");
	Check(linksOk, what + ": every link inside the list, both ways with the same flags, only 0 or 0x4");
	Check(shared == 0, what + ": no centre in another box, by the game's own test (" + std::to_string(shared) + " are)");
	return shared;
}

static int32_t FlagOf(host::AutoPlotPlan const &plan, size_t a, size_t b)
{
	for (host::WaypointLinkRec const &l : plan.list[a].links)
	{
		if (l.index == (int32_t)b)
		{
			return l.flags;
		}
	}

	return -1;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("A STRAIGHT FLOOR WAY, 10 M, TWO EXITS AT ITS ENDS\n");

	std::vector<std::vector<int32_t> > const twoExits = { { 0, -100, 0 }, { 10000, -100, 0 } };
	host::AutoPlotPlan plan = host::AutoPlot_Plan({ FloorWay(0, 10000, 0) }, twoExits, 1.0);

	Check(plan.list.size() == 4, "10 m in stretches of 3 m: 4 boxes (" + std::to_string(plan.list.size()) + ")");
	Check(RulesKept(plan, "straight") == 0 && plan.centresShared == 0, "no centre in another box");
	Check(plan.exits == 2 && plan.exitsCovered == 2 && plan.parts == 1, "both exits inside a box; one network");
	Check(!plan.list.empty() && plan.list[0].min[1] == -1500 && plan.list[0].max[1] == 201, "a floor box: 1.5 m up off the floor, 0.2 m below it (y down)");
	Check(!plan.list.empty() && plan.list[0].min[2] == -600 && plan.list[0].max[2] == 601, "0.6 m to each side of the way");
	Check(plan.list.size() == 4 && FlagOf(plan, 0, 1) == 0 && FlagOf(plan, 1, 2) == 0 && FlagOf(plan, 2, 3) == 0 && FlagOf(plan, 0, 2) == -1, "the floor's links plain, box to the next only");
	Check(host::AutoPlot_Words(plan) == "4 boxes, 3 links (0 aliens only); exits covered 2 of 2", "its words: " + host::AutoPlot_Words(plan));

	printf("AN ALIEN WAY: FLOOR, UP A 6 M WALL, ALONG THE CEILING\n");

	auto const climbOf = [](int32_t height)
	{
		host::AutoPlotWay climb;

		climb.alien = true;

		for (int32_t x = 0; x <= 2000; x += 500) climb.steps.push_back(Step(x, 0, 0, AutoPlotSurface::Floor));
		for (int32_t y = -500; y >= -height; y -= 500) climb.steps.push_back(Step(2000, y, 0, AutoPlotSurface::Wall));

		size_t const top = climb.steps.size() - 1;

		for (int32_t x = 1500; x >= 0; x -= 500) climb.steps.push_back(Step(x, -height, 0, AutoPlotSurface::Ceiling));

		// the turns: the last floor point starts the wall, the wall's top the ceiling
		climb.steps[4].surface = AutoPlotSurface::Wall;
		climb.steps[4].facing[0] = -65536;
		climb.steps[4].facing[1] = 0;
		climb.steps[top].surface = AutoPlotSurface::Ceiling;
		climb.steps[top].facing[0] = 0;
		climb.steps[top].facing[1] = 65536;
		return climb;
	};

	// the floor's and the ceiling's boxes reach 1.5 m each, so a 6 m wall has room of its own: one highway box (6 m at most), standing on the floor's box
	plan = host::AutoPlot_Plan({ climbOf(6000) }, {}, 1.0);
	RulesKept(plan, "climb");
	Check(plan.list.size() == 3, "three surfaces, the wall one highway: three boxes (" + std::to_string(plan.list.size()) + ")");
	Check(plan.list.size() == 3 && FlagOf(plan, 0, 1) == host::kLinkAlien && FlagOf(plan, 1, 2) == host::kLinkAlien, "floor to wall and wall to ceiling: aliens only (0x4)");
	Check(plan.list.size() == 3 && plan.list[1].max[0] == 2000 + 200 + 1 && plan.list[1].min[0] == 2000 - host::kAutoPlotClimbReach,
		"the wall's box: 0.45 m off it (its facing -x), 0.2 m into it: a thin climbing box (THE LANES)");
	Check(plan.list.size() == 3 && plan.list[1].max[1] == -1500 + 1, "and cut off at the floor's box's top (y -1500): it stands on the floor, never over it");
	Check(plan.list.size() == 3 && plan.list[2].min[1] == -6000 - 200 && plan.list[2].max[1] == -6000 + host::kAutoPlotClimbReach + 1,
		"the ceiling's box: 0.45 m below it (+y), 0.2 m into it: a thin climbing box");
	Check(plan.list.size() == 3 && plan.list[1].centre[0] > 2000 && plan.list[2].centre[1] < -6000,
		"the wall's centre inside the wall (x " + (plan.list.size() == 3 ? std::to_string(plan.list[1].centre[0]) : std::string()) + "), the ceiling's inside the ceiling (y " +
		(plan.list.size() == 3 ? std::to_string(plan.list[2].centre[1]) : std::string()) + "): the inteng manual's rule");
	Check(plan.list.size() == 3 && plan.list[0].centre[1] < 0, "the floor's centre in the air over the floor");
	Check(plan.parts == 1, "one network");

	printf("A 3 M WALL: ITS OWN BOX (THE CEILING'S IS THIN)\n");

	plan = host::AutoPlot_Plan({ climbOf(3000) }, {}, 1.0);
	RulesKept(plan, "low climb");
	Check(plan.list.size() == 3 && FlagOf(plan, 0, 1) == host::kLinkAlien && FlagOf(plan, 1, 2) == host::kLinkAlien && plan.parts == 1,
		"the wall keeps a box of its own (the ceiling's reaches only 0.45 m down): 3 boxes, floor to wall to ceiling aliens only (" + std::to_string(plan.list.size()) + ")");

	// a climbing box cut back to another climbing box's face still shares that face's unit in the file (its max is one past it, the game's inside is min <= p <= max - 1):
	// a centre on that unit is in both, the cut changed nothing, and the picking went round 4000 times and gave up (Genshd1 v2c, 11-19: opscen03's box 6 at y -19683, the
	// face of box 0; 120 AI modules refused)
	printf("WALLS FROM 1.6 TO 4 M UNDER THEIR CEILING, AND A CEILING OVER A WALL ALONG A CORRIDOR: EACH CENTRE ITS OWN BOX'S ONLY\n");

	size_t sharedAtAnyHeight = 0;
	std::string sharedHeights;

	for (int32_t height = 1600; height <= 4000; height += 100)
	{
		host::AutoPlotPlan const low = host::AutoPlot_Plan({ climbOf(height) }, {}, 1.0);
		size_t shared = 0;

		for (size_t i = 0; i < low.list.size(); i++)
		{
			for (size_t j = 0; j < low.list.size(); j++)
			{
				shared += j != i && GameHolds(low.list[j], low.list[i].centre, 1.0) ? 1 : 0;
			}
		}

		sharedAtAnyHeight += shared + low.centresShared;
		sharedHeights += shared + low.centresShared ? " " + std::to_string(height) : std::string();
	}

	Check(sharedAtAnyHeight == 0, "a wall of every height: no centre in another box (shared at" + (sharedHeights.empty() ? std::string(" none") : sharedHeights) + ")");

	host::AutoPlotWay along;   // a corridor's ceiling at y -3000 for 8 m, and its side wall (z 1000, facing -z) climbed beside it the whole way
	host::AutoPlotWay side;

	along.alien = side.alien = true;

	for (int32_t x = 0; x <= 8000; x += 500)
	{
		along.steps.push_back(Step(x, -3000, 0, AutoPlotSurface::Ceiling));
		along.steps.back().place[2] = 700;

		host::AutoPlotStep s = Step(x, -2500, 1000, AutoPlotSurface::Wall);

		s.facing[0] = 0;
		s.facing[2] = -65536;
		side.steps.push_back(s);
	}

	plan = host::AutoPlot_Plan({ along, side }, {}, 1.0);
	RulesKept(plan, "a ceiling over a wall along a corridor");
	Check(plan.parts == 1, "the corridor's ceiling and wall: one network after the cuts");

	printf("A WAY CUT TO THE AI MODULE, ONE POINT PAST EACH END\n");

	host::AutoPlotWay cut;

	cut.alien = false;
	cut.steps.push_back(Step(-1000, 0, 0, AutoPlotSurface::Floor, false));
	cut.steps.push_back(Step(-500, 0, 0, AutoPlotSurface::Floor, false));
	for (int32_t x = 0; x <= 2500; x += 500) cut.steps.push_back(Step(x, 0, 0, AutoPlotSurface::Floor));
	cut.steps.push_back(Step(3000, 0, 0, AutoPlotSurface::Floor, false));
	cut.steps.push_back(Step(3500, 0, 0, AutoPlotSurface::Floor, false));
	plan = host::AutoPlot_Plan({ cut }, {}, 1.0);
	RulesKept(plan, "cut");

	int32_t lowest = 1 << 30, highest = -(1 << 30);

	for (host::WaypointRec const &wp : plan.list)
	{
		lowest = wp.min[0] < lowest ? wp.min[0] : lowest;
		highest = wp.max[0] > highest ? wp.max[0] : highest;
	}

	Check(lowest == -500 - 600 && highest == 3000 + 600 + 1, "from the point before (-500) to the point after (3000), not further: x " + std::to_string(lowest) + " to " + std::to_string(highest));
	Check(host::AutoPlot_Plan({ FloorWay(0, 3000, 0) }, {}, 1.0).list.size() > 0 && host::AutoPlot_Plan({ []() { host::AutoPlotWay w; w.alien = false;
		w.steps.push_back(Step(0, 0, 0, AutoPlotSurface::Floor, false)); w.steps.push_back(Step(500, 0, 0, AutoPlotSurface::Floor, false)); return w; }() }, {}, 1.0).list.empty(),
		"a way wholly outside the AI module: no box");

	printf("A LONG WAY: DOWN TO 99 BOXES\n");

	std::vector<host::AutoPlotWay> snake;

	snake.push_back(FloorWay(0, 400000, 0));
	plan = host::AutoPlot_Plan(snake, { { 0, -100, 0 }, { 400000, -100, 0 } }, 1.0);
	RulesKept(plan, "snake");
	Check(plan.list.size() == 99, "400 m of floor in 134 stretches merged down to 99 (" + std::to_string(plan.list.size()) + ")");
	Check(plan.parts == 1 && plan.exitsCovered == 2, "still one network, both exits held");

	printf("AN EXIT NO WAY REACHES\n");

	plan = host::AutoPlot_Plan({ FloorWay(0, 3000, 0) }, { { 0, -100, 0 }, { 9000, -100, 0 } }, 1.0);
	RulesKept(plan, "far exit");
	Check(plan.exitsCovered == 2 && plan.parts == 1, "the far exit gets a box of its own, linked: both held, one network");
	Check(!plan.notes.empty() && plan.notes[0].find("exit 1 (9000 -100 0) was on no way") == 0, "and a note says so: " + (plan.notes.empty() ? std::string() : plan.notes[0]));

	printf("WAYS THAT CROSS, WAYS FAR APART\n");

	host::AutoPlotWay across;

	across.alien = false;
	for (int32_t z = -3000; z <= 3000; z += 500) across.steps.push_back(Step(5000, 0, z, AutoPlotSurface::Floor));
	plan = host::AutoPlot_Plan({ FloorWay(0, 10000, 0), across }, {}, 1.0);
	RulesKept(plan, "crossing");
	Check(plan.parts == 1, "two ways that cross: joined where their boxes touch, one network");

	plan = host::AutoPlot_Plan({ FloorWay(0, 3000, 0), FloorWay(0, 3000, 20000) }, {}, 1.0);
	Check(plan.parts == 2 && !plan.notes.empty() && plan.notes.back().find("2 parts don't reach each other") == 0, "20 m apart: 2 parts, said: " + (plan.notes.empty() ? std::string() : plan.notes.back()));
	Check(host::AutoPlot_Words(plan).find("; 2 parts apart") != std::string::npos, "and in the words");

	printf("AN ALIEN ROUTE ON A FLOOR ROUTE'S LINE, AND ONE BESIDE IT\n");

	bool anyPlain = false, anyAlien = false;
	auto const flagsSeen = [&anyPlain, &anyAlien](host::AutoPlotPlan const &seen)
	{
		anyPlain = anyAlien = false;

		for (size_t i = 0; i < seen.list.size(); i++)
		{
			for (host::WaypointLinkRec const &l : seen.list[i].links)
			{
				anyPlain = anyPlain || l.flags == 0;
				anyAlien = anyAlien || l.flags == host::kLinkAlien;
			}
		}
	};

	// on the same line, the alien route's boxes are the floor route's: theirs go (the wall and ceiling boxes are looked at first), the floor route's stay
	host::AutoPlotWay alienFloor = FloorWay(0, 4000, 0, 500, true), mixed = FloorWay(0, 4000, 0);

	for (host::AutoPlotStep &s : alienFloor.steps) { s.surface = AutoPlotSurface::Wall; s.facing[0] = 0; s.facing[1] = -65536; }
	plan = host::AutoPlot_Plan({ alienFloor, mixed }, {}, 1.0);
	RulesKept(plan, "shared");
	flagsSeen(plan);
	Check(plan.list.size() == 2 && plan.parts == 1 && anyPlain && !anyAlien, "on the same line: the floor route's 2 boxes kept, their link plain (a marine may go), one network (" +
		std::to_string(plan.list.size()) + " boxes)");

	// 1 m over, the boxes overlap without covering: both routes kept, joined aliens only where their boxes touch
	host::AutoPlotWay alienBeside = FloorWay(0, 4000, 1000, 500, true);

	for (host::AutoPlotStep &s : alienBeside.steps) { s.surface = AutoPlotSurface::Wall; s.facing[0] = 0; s.facing[1] = -65536; }
	plan = host::AutoPlot_Plan({ alienBeside, mixed }, {}, 1.0);
	RulesKept(plan, "beside");
	flagsSeen(plan);
	Check(plan.list.size() == 3 && anyPlain, "1 m over: both routes kept (the alien's one highway box, the floor's two), the floor route's own links plain (" +
		std::to_string(plan.list.size()) + " boxes)");
	Check(anyAlien && plan.parts == 1, "and it is joined to the wall route aliens only, where their boxes touch: one network");

	printf("A BOX THE OTHERS COVER: MADE ONE WITH A BOX THAT COVERS IT\n");

	// three floor ways on one line, none 80% over another, so none merged for overlap: the third's box (x 400 to 4100) lies wholly inside the first's (-600 to 3100) and the
	// second's (1400 to 5100), so no place in it is outside both (on Genshd1's genent01 the checker said: "box 1: its centre is in box 3 too")
	plan = host::AutoPlot_Plan({ FloorWay(0, 2500, 0), FloorWay(2000, 4500, 0), FloorWay(1000, 3500, 0) }, { { 0, -100, 0 }, { 4900, -100, 0 } }, 1.0);
	RulesKept(plan, "covered");
	Check(plan.list.size() == 2, "the covered box taken out: 2 boxes (" + std::to_string(plan.list.size()) + ")");
	Check(plan.exitsCovered == 2 && plan.parts == 1, "both exits still held, one network");
	Check(!plan.notes.empty() && plan.notes[0] == "1 box the others covered taken out (each centre must be in its own box only)",
		"and a note says so: " + (plan.notes.empty() ? std::string() : plan.notes[0]));

	// the same at a scale under 1, where the loader's sum (each corner scaled, then 1 off the highest) is not the file's max - 1
	plan = host::AutoPlot_Plan({ FloorWay(0, 2500, 0), FloorWay(2000, 4500, 0), FloorWay(1000, 3500, 0) }, { { 0, -100, 0 }, { 4900, -100, 0 } }, 0.3);
	RulesKept(plan, "covered at scale 0.3", 0.3);

	printf("THE FLOOR TILED FROM THE GRID'S CELLS (the stock price, the floor under the player covered)\n");

	// every cell of a floor, 250 mm apart, at y (the world's mm)
	auto const floorOf = [](int32_t x0, int32_t x1, int32_t z0, int32_t z1, int32_t y)
	{
		host::AutoPlotFloor f;

		f.cellEdge = 250;
		f.mostSide = 0.0;   // the tiling itself, not merged (CONSOLIDATED, below, merges)

		for (int32_t x = x0; x <= x1; x += 250) for (int32_t z = z0; z <= z1; z += 250) f.cells.push_back(std::vector<int32_t>{ x, y, z });

		return f;
	};

	// every cell inside a floor box (the game's test), the floor boxes all reach each other by plain links
	auto const covered = [](host::AutoPlotPlan const &seen, host::AutoPlotFloor const &f, double scale)
	{
		size_t out = 0;

		for (std::vector<int32_t> const &c : f.cells)
		{
			int32_t const place[3] = { (int32_t)floor(c[0] / scale), (int32_t)floor(c[1] / scale), (int32_t)floor(c[2] / scale) };
			bool held = false;

			for (host::WaypointRec const &wp : seen.list) held = held || GameHolds(wp, place, scale);

			out += held ? 0 : 1;
		}

		return out;
	};

	host::AutoPlotFloor const room = floorOf(0, 6000, 0, 4000, 0);

	plan = host::AutoPlot_Plan({}, { { 0, -100, 2000 }, { 6000, -100, 2000 } }, room, 1.0);
	RulesKept(plan, "tiled room");
	Check(plan.floorTile == 600.0 && plan.floorBoxes == plan.list.size() && plan.list.size() == 11 * 7, "a 6 x 4 m room: tiles 0.6 m across, 11 x 7 of them, all floor (" +
		std::to_string(plan.list.size()) + " boxes, tile " + std::to_string((int)plan.floorTile) + ")");
	Check(covered(plan, room, 1.0) == 0, "every cell of the floor inside a box: no NPC on it or target there is in no box (the stock dropout)");
	Check(plan.parts == 1 && plan.exitsCovered == 2, "the tiles all reach each other, both exits in them");

	bool allPlain = true;

	for (host::WaypointRec const &wp : plan.list) for (host::WaypointLinkRec const &l : wp.links) allPlain = allPlain && l.flags == 0;

	Check(allPlain, "the tiles linked plain: marines, predators and praetorians go");

	host::AutoPlotFloor const hall = floorOf(0, 30000, 0, 30000, 0);

	plan = host::AutoPlot_Plan({}, {}, hall, 1.0);
	RulesKept(plan, "big hall");
	Check(plan.list.size() <= 99 && plan.floorTile > 600.0 && covered(plan, hall, 1.0) == 0 && plan.parts == 1, "a 30 x 30 m hall: the tiles grow (" +
		std::to_string((int)plan.floorTile) + " mm) to stay under 99 (" + std::to_string(plan.list.size()) + "), every cell still covered, one network");

	host::AutoPlotFloor twoFloors = floorOf(0, 3000, 0, 3000, 0), upper = floorOf(0, 3000, 0, 3000, -4000);

	twoFloors.cells.insert(twoFloors.cells.end(), upper.cells.begin(), upper.cells.end());
	plan = host::AutoPlot_Plan({}, {}, twoFloors, 1.0);
	RulesKept(plan, "two floors");
	size_t between = 0, betweenPlain = 0;

	for (host::WaypointRec const &wp : plan.list) for (host::WaypointLinkRec const &l : wp.links) if ((wp.centre[1] < -2000) != (plan.list[(size_t)l.index].centre[1] < -2000)) { between++; betweenPlain += l.flags == 0 ? 1 : 0; }

	Check(covered(plan, twoFloors, 1.0) == 0 && plan.parts == 1 && between > 0 && betweenPlain == 0,
		"a floor 4 m over a floor: boxes of their own, both covered, joined aliens only (an alien climbs there; the floor route does not: " + std::to_string(between / 2) + " links between them, none plain)");

	// a wall beside the room: its climbing way (up 4 m from the floor at x 6000) a highway box standing on the tiles, linked to them aliens only
	host::AutoPlotWay up;

	up.alien = true;
	for (int32_t x = 4000; x <= 6000; x += 500) up.steps.push_back(Step(x, 0, 2000, AutoPlotSurface::Floor));
	for (int32_t y = -500; y >= -4000; y -= 500) up.steps.push_back(Step(6000, y, 2000, AutoPlotSurface::Wall));
	up.steps[4].surface = AutoPlotSurface::Wall;
	up.steps[4].facing[0] = -65536;
	up.steps[4].facing[1] = 0;
	plan = host::AutoPlot_Plan({ up }, {}, room, 1.0);
	RulesKept(plan, "wall on tiles");

	size_t highway = plan.list.size(), wallLinks = 0;

	for (size_t i = 0; i < plan.list.size(); i++) if (plan.list[i].min[0] == 6000 - host::kAutoPlotClimbReach) highway = i;

	if (highway < plan.list.size()) for (host::WaypointLinkRec const &l : plan.list[highway].links) wallLinks += l.flags == host::kLinkAlien ? 1 : 0;

	Check(highway < plan.list.size() && plan.list[highway].max[1] == -1500 + 1 && plan.list[highway].centre[0] > 6000,
		"the wall's highway box stands on the tiles (its bottom at their top, y -1500), its centre inside the wall");
	Check(highway < plan.list.size() && wallLinks > 0 && wallLinks == plan.list[highway].links.size() && covered(plan, room, 1.0) == 0 && plan.parts == 1,
		"linked aliens only to the tiles it touches (" + std::to_string(wallLinks) + "), the floor still all covered, one network");
	Check(plan.floorBoxes + 1 == plan.list.size(), "the way's floor stretch made no box of its own: the tiles are the floor");

	// the price along one route: 6 m of floor in 0.6 m tiles, 6 m of ceiling in one highway box
	host::AutoPlotFloor const corridor = floorOf(0, 6000, 0, 0, 0);
	host::AutoPlotWay over;

	over.alien = true;
	for (int32_t x = 0; x <= 6000; x += 500) over.steps.push_back(Step(x, -3000, 0, AutoPlotSurface::Ceiling));
	plan = host::AutoPlot_Plan({ over }, {}, corridor, 1.0);
	RulesKept(plan, "price");
	Check(plan.floorBoxes >= 10 && plan.list.size() - plan.floorBoxes == 1, "6 m of corridor: " + std::to_string(plan.floorBoxes) + " floor tiles under 1 ceiling highway box (the floor price, " +
		std::to_string(host::kAutoPlotFloorPrice) + ")");

	// THE CEILING TILED (Drake's rule from his hand-built Ops01, 11-19): the route with the fewest boxes wins, so the ceiling a few long boxes edge to edge, the floor many.
	// The ways' ceiling covered only where an alien way went (Genshd1 v2d: Ops01's door box 0 an island 2 m past the ceiling's last box); the grid's ceiling over every floor
	// cell covers it all
	printf("THE CEILING TILED FROM THE CEILING OVER THE FLOOR CELLS\n");

	host::AutoPlotFloor hallway = floorOf(0, 24000, 0, 4000, 0);

	hallway.mostSide = -1.0;   // merged as the command plans (kAutoPlotFloorMost)

	for (std::vector<int32_t> const &c : hallway.cells) hallway.ceilings.push_back(std::vector<int32_t>{ c[0], -4000, c[2] });

	host::AutoPlotWay part;   // an alien way along 6 m of the ceiling: its stretch is no box of its own (the tiles are the ceiling)

	part.alien = true;
	for (int32_t x = 9000; x <= 15000; x += 500) part.steps.push_back(Step(x, -4000, 2000, AutoPlotSurface::Ceiling));
	plan = host::AutoPlot_Plan({ part }, { { 0, -100, 2000 }, { 24000, -100, 2000 } }, hallway, 1.0);
	RulesKept(plan, "ceiling tiled");

	size_t hung = 0, climbingBoxes = 0, climbingParts = 0;

	for (std::vector<int32_t> const &c : hallway.ceilings)
	{
		int32_t const top[3] = { c[0], c[1] + 100, c[2] };   // a hanging alien's top corners, just under the ceiling
		bool in = false;

		for (size_t i = 0; i < plan.list.size(); i++) in = in || (plan.climbing[i] && GameHolds(plan.list[i], top, 1.0));

		hung += in ? 1 : 0;
	}

	std::vector<int> seenPart(plan.list.size(), -1);

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		climbingBoxes += plan.climbing[i] ? 1 : 0;

		if (!plan.climbing[i] || seenPart[i] >= 0)
		{
			continue;
		}

		std::vector<size_t> todo(1, i);

		seenPart[i] = (int)climbingParts;

		while (!todo.empty())
		{
			size_t const k = todo.back();

			todo.pop_back();

			for (host::WaypointLinkRec const &l : plan.list[k].links)
			{
				if (l.flags == host::kLinkAlien && plan.climbing[(size_t)l.index] && seenPart[(size_t)l.index] < 0)
				{
					seenPart[(size_t)l.index] = (int)climbingParts;
					todo.push_back((size_t)l.index);
				}
			}
		}

		climbingParts++;
	}

	Check(hung == hallway.ceilings.size(), "every place under the ceiling in a climbing box (" + std::to_string(hung) + " of " + std::to_string(hallway.ceilings.size()) + ")");
	Check(climbingBoxes >= 1 && climbingBoxes <= 4 && plan.ceilingBoxes == climbingBoxes,
		"24 m of ceiling in at most 4 boxes, all of them tiles (" + std::to_string(climbingBoxes) + " climbing, " + std::to_string(plan.ceilingBoxes) + " tiles)");
	Check(climbingParts == 1, "the ceiling's boxes reach each other by aliens-only links (" + std::to_string(climbingParts) + " parts)");
	Check(plan.ceilingTile >= host::kAutoPlotCeilingTile && plan.floorTile > 0.0 && plan.floorTile * 2.0 <= plan.ceilingTile,
		"the floor finer than the ceiling (tiles " + std::to_string((long long)plan.floorTile) + " mm under " + std::to_string((long long)plan.ceilingTile) + " mm)");

	// CONSOLIDATED (the owner 11-20 after playing v2d: "the game is not dealing well with discreet squares", "we need to consolidate"): the floor tiles merged into rectangles of at
	// most kAutoPlotFloorMost a side (still finer than the ceiling's lanes of up to kAutoPlotCeilingTile, so the route with the fewest boxes stays the ceiling's), the ceiling in lanes
	printf("CONSOLIDATED: 24 M BY 4 M, ITS FLOOR IN RECTANGLES OF AT MOST 4 M A SIDE, ITS CEILING IN LANES OF UP TO 12 M\n");

	int32_t widest = 0;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		if (!plan.climbing[i])
		{
			widest = std::max(widest, std::max(plan.list[i].max[0] - plan.list[i].min[0], plan.list[i].max[2] - plan.list[i].min[2]));
		}
	}

	Check(plan.floorBoxes <= 16 && plan.floorMost == host::kAutoPlotFloorMost, "the floor in at most 16 boxes, from 84: rectangles of whole 600 mm squares, 6 a side (3.6 m), 7 by 2 of them, "
		"and the two exits' own (" + std::to_string(plan.floorBoxes) + ")");
	Check(widest <= host::kAutoPlotFloorMost + 300, "each floor box at most 4 m a side (the widest " + std::to_string(widest) + ")");
	Check(plan.ceilingBoxes == 1, "THE LONG PATHS: the flat ceiling one lane its whole 24 m (" + std::to_string(plan.ceilingBoxes) + "; 2 in 12 m pieces on e4d4f0e3)");
	Check(covered(plan, hallway, 1.0) == 0, "the floor still all covered (" + std::to_string(covered(plan, hallway, 1.0)) + " cells in no box)");

	printf("SCALE AND HOSTILE PLACES\n");

	host::AutoPlotPlan const halved = host::AutoPlot_Plan({ FloorWay(0, 2000, 0) }, {}, 2.0);

	RulesKept(halved, "scale 2", 2.0);

	Check(halved.list.size() == 1 && halved.list[0].min[0] == -300 && halved.list[0].max[0] == 1301 && halved.list[0].min[1] == -750 && halved.list[0].max[1] == 101,
		"scale 2: the file's numbers half the world's");

	host::AutoPlotWay wild;

	wild.alien = true;
	wild.steps.push_back(Step(2147483000, 0, 0, AutoPlotSurface::Floor));
	wild.steps.push_back(Step(-2147483000, 2147483000, 0, AutoPlotSurface::Floor));
	plan = host::AutoPlot_Plan({ wild }, { { 2147483000, 2147483000, 2147483000 }, { 1, 2 } }, 1.0);
	RulesKept(plan, "wild");

	bool inside = true;

	for (host::WaypointRec const &wp : plan.list) for (int a = 0; a < 3; a++) inside = inside && wp.min[a] >= -host::kAutoPlotMostPlace && wp.max[a] <= host::kAutoPlotMostPlace;
	Check(inside, "places past 2^24 held inside it (the loader refuses past it)");
	Check(plan.exits == 2, "an exit of two numbers is counted, never read");

	Check(host::AutoPlot_Plan({}, {}, 1.0).list.empty() && host::AutoPlot_Words(host::AutoPlot_Plan({}, {}, 1.0)) == "0 boxes, 0 links (0 aliens only); exits covered 0 of 0", "no ways: nothing");

	// Drake's gate 11-20, BLOCK-10: its roof and its ground were joined only by exit 3's box (the nearest pair of the two parts), so an alien on the roof going to exit 0 headed
	// for that box's centre past the doorway, into EXT-05. A ground 4 m under a roof, no way between them, and an exit 2 m up that no way's box holds: its own box stands on
	// the ground and its top is 0.3 m under the roof's box, nearer it than the ground's 2.3 m. The parts are joined by the roof's box and the ground's, never through the exit's
	printf("AN EXIT BOX IS AN END, NEVER A WAY THROUGH: A ROOF AND A GROUND NO WAY JOINS, AN EXIT'S BOX NEARER THE ROOF THAN THE GROUND IS\n");

	host::AutoPlotWay roof = FloorWay(0, 4000, 0);

	for (host::AutoPlotStep &s : roof.steps) s.place[1] = -4000;

	int32_t const midway[3] = { 2000, -2000, 0 };

	plan = host::AutoPlot_Plan({ FloorWay(0, 4000, 0), roof }, { { midway[0], midway[1], midway[2] } }, 1.0);
	RulesKept(plan, "exit between");

	int exitBox = -1;
	bool groundToRoof = false, exitToRoof = false;

	for (size_t i = 0; i < plan.list.size(); i++) exitBox = exitBox < 0 && GameHolds(plan.list[i], midway, 1.0) ? (int)i : exitBox;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		for (size_t j = 0; j < plan.list.size(); j++)
		{
			bool const iGround = plan.list[i].centre[1] > -2000 && (int)i != exitBox, jRoof = plan.list[j].centre[1] < -3000 && (int)j != exitBox;

			groundToRoof = groundToRoof || (iGround && jRoof && FlagOf(plan, i, j) >= 0);
			exitToRoof = exitToRoof || ((int)i == exitBox && jRoof && FlagOf(plan, i, j) >= 0);
		}
	}

	Check(exitBox >= 0 && plan.parts == 1, "the exit held by a box of its own; one network");
	Check(groundToRoof && !exitToRoof, std::string("the ground's box linked to the roof's (") + (groundToRoof ? "yes" : "no") + "), the roof's not to the exit's box (" + (exitToRoof ? "it is" : "no") +
		"): the exit's box is an end, never the way between two parts");

	// Drake's gate on b99de13d, med02 (36 S): a raised floor no way joins (boxes 29, 30) was linked aliens only to the ceiling's band 0.55 m over it, its centre 10 m off,
	// where floor boxes were 1.2 m beside it; heading for that centre the alien walks under it and stands. A hall (its floor, up its wall at x 2000, its ceiling 3 m up
	// back to x -6000) and a ledge 0.8 m up under that ceiling, 2.2 m from the hall's floor, that no way joins: the ledge joins the floor, not the ceiling's box over it
	printf("A FLOOR ISLAND JOINS A FLOOR OR A WALL FIRST, NOT A CEILING'S BOX OVER IT\n");

	host::AutoPlotWay ledgeHall;

	ledgeHall.alien = true;

	for (int32_t x = 0; x <= 2000; x += 500) ledgeHall.steps.push_back(Step(x, 0, 0, AutoPlotSurface::Floor));
	for (int32_t y = -500; y >= -3000; y -= 500) ledgeHall.steps.push_back(Step(2000, y, 0, AutoPlotSurface::Wall));

	size_t const hallTop = ledgeHall.steps.size() - 1;

	for (int32_t x = 1500; x >= -6000; x -= 500) ledgeHall.steps.push_back(Step(x, -3000, 0, AutoPlotSurface::Ceiling));

	ledgeHall.steps[4].surface = AutoPlotSurface::Wall;
	ledgeHall.steps[4].facing[0] = -65536;
	ledgeHall.steps[4].facing[1] = 0;
	ledgeHall.steps[hallTop].surface = AutoPlotSurface::Ceiling;
	ledgeHall.steps[hallTop].facing[0] = 0;
	ledgeHall.steps[hallTop].facing[1] = 65536;

	host::AutoPlotWay ledge = FloorWay(-5000, -4000, 0);

	for (host::AutoPlotStep &s : ledge.steps) s.place[1] = -800;

	plan = host::AutoPlot_Plan({ ledgeHall, ledge }, {}, 1.0);
	RulesKept(plan, "ledge");

	bool ledgeToCeiling = false, ledgeToFloor = false;
	size_t ledgeBoxes = 0;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		bool const isLedge = plan.list[i].centre[0] < -3000 && plan.list[i].centre[1] > -2400 && plan.list[i].centre[1] < -500;

		ledgeBoxes += isLedge ? 1 : 0;

		for (size_t j = 0; isLedge && j < plan.list.size(); j++)
		{
			bool const ceiling = plan.list[j].centre[1] < -2900, hallFloor = plan.list[j].centre[0] > -1000 && plan.list[j].centre[1] > -1600;

			ledgeToCeiling = ledgeToCeiling || (ceiling && FlagOf(plan, i, j) >= 0);
			ledgeToFloor = ledgeToFloor || (hallFloor && FlagOf(plan, i, j) >= 0);
		}
	}

	Check(ledgeBoxes > 0 && plan.parts == 1, "the ledge has its box (" + std::to_string(ledgeBoxes) + "); one network");
	Check(ledgeToFloor && !ledgeToCeiling, std::string("the ledge linked to the hall's floor (") + (ledgeToFloor ? "yes" : "no") + "), not to the ceiling's box over it (" +
		(ledgeToCeiling ? "it is" : "no") + "): a floor box climbs only into a wall");

	// Drake's gate on e0a30bd6, Object11 (74 stall places): one 24 m ceiling tile over a ceiling sloping from -2377 down to -1500 held the head corners of the aliens
	// standing on a platform under its high end (another module's floor), and the cut over their heads was refused by the tile's lowest place. A room 12 m long whose
	// ceiling slopes from 3.0 m to 2.2 m up, and a platform 1.3 m up under the high end that is not the room's own floor: no ceiling box holds a head on the platform,
	// and every place under the ceiling is still hung
	printf("A PLATFORM UNDER A SLOPING CEILING: THE TILES CUT AT ITS HEADS\n");

	host::AutoPlotFloor sloped = floorOf(0, 12000, 0, 4000, 0);

	for (std::vector<int32_t> const &c : sloped.cells) sloped.ceilings.push_back(std::vector<int32_t>{ c[0], -3000 + c[0] * 800 / 12000, c[2] });
	for (int32_t x = 1000; x <= 3000; x += 250) for (int32_t z = 1000; z <= 3000; z += 250) sloped.standFloors.push_back(std::vector<int32_t>{ x, -1300, z });

	plan = host::AutoPlot_Plan({}, { { 0, -100, 2000 }, { 12000, -100, 2000 } }, sloped, 1.0);
	RulesKept(plan, "platform");

	size_t headsHeld = 0, slopedHung = 0;

	for (std::vector<int32_t> const &c : sloped.standFloors)
	{
		int32_t const head[3] = { c[0], c[1] - host::kAutoPlotCornerHeight, c[2] };   // a standing alien's head corner
		bool held = false;

		for (size_t i = 0; i < plan.list.size(); i++) held = held || (plan.climbing[i] && GameHolds(plan.list[i], head, 1.0));

		headsHeld += held ? 1 : 0;
	}

	for (std::vector<int32_t> const &c : sloped.ceilings)
	{
		int32_t const top[3] = { c[0], c[1] + 100, c[2] };
		bool in = false;

		for (size_t i = 0; i < plan.list.size(); i++) in = in || (plan.climbing[i] && GameHolds(plan.list[i], top, 1.0));

		slopedHung += in ? 1 : 0;
	}

	// Vasquez 11-20, module 05: its ceiling in two parts joined only through the floor (a wall strip over two doorways overlapping the main ceiling's box with no link: the
	// parts are joined where boxes touch only while the network is apart). Two halls side by side (floor, up a 3 m wall, the ceiling back), 1.1 m apart in z so their
	// ceilings' boxes touch, and a marine's walk across their floors: the two ceilings' boxes are linked, aliens only
	printf("CLIMBING BOXES THAT TOUCH ARE LINKED, THOUGH THE FLOOR JOINS THEIR NETWORK ALREADY\n");

	host::AutoPlotWay nearHall = climbOf(3000), farHall = climbOf(3000), walkAcross;

	for (host::AutoPlotStep &s : farHall.steps) s.place[2] += 1100;

	walkAcross.alien = false;

	for (int32_t z = 0; z <= 1100; z += 100) walkAcross.steps.push_back(Step(1000, 0, z, AutoPlotSurface::Floor));

	plan = host::AutoPlot_Plan({ nearHall, farHall, walkAcross }, {}, 1.0);
	RulesKept(plan, "two halls");

	bool ceilingsLinked = false;
	size_t ceilingsSeen = 0;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		bool const nearCeiling = plan.list[i].centre[1] < -2900 && plan.list[i].centre[2] < 550;

		ceilingsSeen += plan.list[i].centre[1] < -2900 ? 1 : 0;

		for (size_t j = 0; nearCeiling && j < plan.list.size(); j++)
		{
			ceilingsLinked = ceilingsLinked || (plan.list[j].centre[1] < -2900 && plan.list[j].centre[2] > 550 && FlagOf(plan, i, j) == host::kLinkAlien);
		}
	}

	Check(ceilingsSeen >= 2 && ceilingsLinked, "the two ceilings' boxes linked aliens only (" + std::to_string(ceilingsSeen) + " ceiling boxes, " + (ceilingsLinked ? "linked" : "NOT linked") + ")");

	Check(headsHeld == 0, "no ceiling box holds a head on the platform (" + std::to_string(headsHeld) + " of " + std::to_string(sloped.standFloors.size()) + " held)");
	Check(slopedHung == sloped.ceilings.size(), "every place under the sloping ceiling still hung (" + std::to_string(slopedHung) + " of " + std::to_string(sloped.ceilings.size()) + ")");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_autoplot_plan_test\n");
		return 1;
	}

	printf("passed host_autoplot_plan_test\n");
	return 0;
}
