// host_autoplot_plan_test.cpp: Auto-plot alien ways' generator (src/host/autoplot_plan.h) on made-up ways, no grid and no level: the stock rules each plan must keep (at most 99 boxes,
// every exit inside a box, one network, each centre inside its own box only, links both ways inside the list, 0x4 where a wall or a ceiling is), and the steps that make it.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
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

// every rule the plan must keep, whatever its notes say; how many centres sit in another box
static size_t RulesKept(host::AutoPlotPlan const &plan, std::string const &what)
{
	bool linksOk = true, roomOk = true;
	size_t shared = 0;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		host::WaypointRec const &wp = plan.list[i];

		for (int a = 0; a < 3; a++)
		{
			roomOk = roomOk && wp.max[a] - 1 > wp.min[a] && wp.centre[a] >= wp.min[a] && wp.centre[a] <= wp.max[a] - 1;   // inside: min <= p <= max - 1 (the loader)
		}

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

			inOther = inOther || (j != i && wp.centre[0] >= o.min[0] && wp.centre[0] <= o.max[0] - 1 && wp.centre[1] >= o.min[1] && wp.centre[1] <= o.max[1] - 1 && wp.centre[2] >= o.min[2] &&
				wp.centre[2] <= o.max[2] - 1);
		}

		shared += inOther ? 1 : 0;
	}

	Check(plan.list.size() <= 99, what + ": at most 99 boxes (" + std::to_string(plan.list.size()) + ")");
	Check(roomOk, what + ": every box has room on every axis, its centre inside it");
	Check(linksOk, what + ": every link inside the list, both ways with the same flags, only 0 or 0x4");
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

	printf("AN ALIEN WAY: FLOOR, UP A WALL, ALONG THE CEILING\n");

	host::AutoPlotWay climb;

	climb.alien = true;

	for (int32_t x = 0; x <= 2000; x += 500) climb.steps.push_back(Step(x, 0, 0, AutoPlotSurface::Floor));
	for (int32_t y = -500; y >= -3000; y -= 500) climb.steps.push_back(Step(2000, y, 0, AutoPlotSurface::Wall));
	for (int32_t x = 1500; x >= 0; x -= 500) climb.steps.push_back(Step(x, -3000, 0, AutoPlotSurface::Ceiling));

	// the turn: the last floor point starts the wall
	climb.steps[4].surface = AutoPlotSurface::Wall;
	climb.steps[4].facing[0] = -65536;
	climb.steps[4].facing[1] = 0;
	climb.steps[10].surface = AutoPlotSurface::Ceiling;
	climb.steps[10].facing[0] = 0;
	climb.steps[10].facing[1] = 65536;
	plan = host::AutoPlot_Plan({ climb }, {}, 1.0);
	RulesKept(plan, "climb");
	Check(plan.list.size() == 3, "three surfaces: three boxes (" + std::to_string(plan.list.size()) + ")");
	Check(plan.list.size() == 3 && FlagOf(plan, 0, 1) == host::kLinkAlien && FlagOf(plan, 1, 2) == host::kLinkAlien, "floor to wall and wall to ceiling: aliens only (0x4)");
	Check(plan.list.size() == 3 && plan.list[1].max[0] == 2000 + 200 + 1 && plan.list[1].min[0] == 2000 - 1500, "the wall's box: 1.5 m off it (its facing -x), 0.2 m into it");
	Check(plan.list.size() == 3 && plan.list[2].min[1] == -3000 - 200 && plan.list[2].max[1] == -3000 + 1500 + 1, "the ceiling's box: hanging 1.5 m below it (+y), 0.2 m into it");
	Check(plan.parts == 1, "one network");

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

	printf("A FLOOR ROUTE BESIDE AN ALIEN ROUTE ON THE SAME LINE\n");

	host::AutoPlotWay alienFloor = FloorWay(0, 4000, 0, 500, true), mixed = FloorWay(0, 4000, 0);

	for (host::AutoPlotStep &s : alienFloor.steps) { s.surface = AutoPlotSurface::Wall; s.facing[0] = 0; s.facing[1] = -65536; }
	plan = host::AutoPlot_Plan({ alienFloor, mixed }, {}, 1.0);
	RulesKept(plan, "shared");

	bool anyPlain = false, anyAlien = false;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		for (host::WaypointLinkRec const &l : plan.list[i].links)
		{
			anyPlain = anyPlain || l.flags == 0;
			anyAlien = anyAlien || l.flags == host::kLinkAlien;
		}
	}

	Check(anyPlain, "the floor route's own links stay plain (a marine may go)");
	Check(anyAlien && plan.parts == 1, "and it is joined to the wall route aliens only, where their boxes touch: one network");

	printf("SCALE AND HOSTILE PLACES\n");

	host::AutoPlotPlan const halved = host::AutoPlot_Plan({ FloorWay(0, 2000, 0) }, {}, 2.0);

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

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_autoplot_plan_test\n");
		return 1;
	}

	printf("passed host_autoplot_plan_test\n");
	return 0;
}
