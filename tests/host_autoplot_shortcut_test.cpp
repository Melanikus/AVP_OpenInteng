// host_autoplot_shortcut_test.cpp: THE FLOOR SHORTCUTS (src/host/autoplot_shortcut.h AutoPlot_CutShortcuts, AutoPlot_ShortcutWords) on made plans, every lane walked by
// the planner's own Lane_Through: a step down a wall that wins over the ceiling (cut, the floor box still steps up); a door box with no link to its exit's box and the way
// through another exit's box (joined, and that exit box made an end); a cut that would break another lane (undone, the list as it was); Lane_Through's real words read
// back; and the hostile inputs (no plan, no boxes, a scale that is not a number, door boxes outside the list). Written 2026-11-20 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_lane.h"
#include "autoplot_shortcut.h"
#include "waypoint_model.h"

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

// heights (the file's units = the world's mm at scale 1, y down): a ceiling box, a floor box, a wall box floor to ceiling
static int32_t const kCeilTop = -3000, kCeilBottom = -2800, kFloorTop = -1700, kFloorBottom = 0;

static host::WaypointRec Box(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1)
{
	host::WaypointRec w;

	w.min[0] = x0, w.min[1] = y0, w.min[2] = z0;
	w.max[0] = x1, w.max[1] = y1, w.max[2] = z1;
	w.centre[0] = (x0 + x1) / 2, w.centre[1] = (y0 + y1) / 2, w.centre[2] = (z0 + z1) / 2;
	return w;
}

static host::WaypointRec Ceil(int32_t x0, int32_t x1, int32_t z0 = 0, int32_t z1 = 1000)
{
	return Box(x0, kCeilTop, z0, x1, kCeilBottom, z1);
}

static host::WaypointRec Floor(int32_t x0, int32_t x1, int32_t z0 = 0, int32_t z1 = 1000)
{
	return Box(x0, kFloorTop, z0, x1, kFloorBottom, z1);
}

static void Join(std::vector<host::WaypointRec> *list, int32_t a, int32_t b, int32_t flags)
{
	host::Waypoints_AddLink(list, a, b, flags);
}

// a's link to b: its flags, -1 when there is none
static int32_t FlagsOf(std::vector<host::WaypointRec> const &list, int32_t a, int32_t b)
{
	for (host::WaypointLinkRec const &l : list[(size_t)a].links)
	{
		if (l.index == b)
		{
			return l.flags;
		}
	}

	return -1;
}

static std::vector<host::WaypointPoint> FileExits(std::vector<std::vector<int32_t> > const &exits)
{
	std::vector<host::WaypointPoint> out;

	for (std::vector<int32_t> const &e : exits)
	{
		host::WaypointPoint p;

		p.place[0] = e[0], p.place[1] = e[1], p.place[2] = e[2];
		out.push_back(p);
	}

	return out;
}

// every exit's doorway with a climb station (or none): rules 1 and 2 are made only where there is one
static std::vector<host::AutoPlotDoor> Stations(std::vector<std::vector<int32_t> > const &exits, bool station = true)
{
	std::vector<host::AutoPlotDoor> doors(exits.size());

	for (host::AutoPlotDoor &d : doors)
	{
		d.stationFound = station;
	}

	return doors;
}

static bool LaneHolds(host::AutoPlotPlan const &plan, std::vector<std::vector<int32_t> > const &exits, size_t i, size_t j)
{
	return host::Lane_Through(plan.list, 1.0, plan.doorBox[i], exits[j].data(), plan.climbing).continuous;
}

// THE DESCENT: a ceiling lane of six boxes from exit 0's door box to exit 1's (0 1 2 3 4 5, then 5 down to exit 1's box 6), and a wall down from box 2 (7) to a floor (8)
// joined to exit 1's box: the floor way (0 1 2 7 8 6) has a box fewer, so the stock search takes it. Exit 2's point, when asked, lies on that floor (box 8)
static host::AutoPlotPlan Descent(std::vector<std::vector<int32_t> > *exits, bool exitOnTheFloor)
{
	host::AutoPlotPlan plan;
	std::vector<host::WaypointRec> &l = plan.list;

	l = { Ceil(0, 1000), Ceil(1000, 2000), Ceil(2000, 3000), Ceil(3000, 4000), Ceil(4000, 5000), Ceil(5000, 6000), Floor(5000, 6000),
		Box(2000, kCeilTop, 0, 2100, kFloorBottom, 1000), Floor(2100, 5000), Floor(0, 1000) };
	plan.climbing = { true, true, true, true, true, true, false, true, false, false };

	int32_t const a = host::kLinkAlien;

	Join(&l, 0, 1, a), Join(&l, 1, 2, a), Join(&l, 2, 3, a), Join(&l, 3, 4, a), Join(&l, 4, 5, a), Join(&l, 5, 6, a), Join(&l, 0, 9, a);
	Join(&l, 2, 7, a), Join(&l, 7, 8, a), Join(&l, 8, 6, 0);
	*exits = { { 500, -500, 500 }, { 5500, -500, 500 } };
	plan.doorBox = { 0, 5 };
	plan.exitBox = { 9, 6 };
	plan.doorFloors = { { -1, 999 }, {} };   // out of the list: left alone

	if (exitOnTheFloor)
	{
		exits->push_back({ 3000, -500, 500 });
		plan.doorBox.push_back(-1);   // no lane at exit 2, its point in the floor box 8
		plan.exitBox.push_back(-1);
		plan.doorFloors.push_back({});
	}

	return plan;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// THE DESCENT, CUT: the step down from the wall (7) into the floor (8) stopped; the floor still steps up to the wall
	{
		std::vector<std::vector<int32_t> > exits;
		host::AutoPlotPlan plan = Descent(&exits, false);
		host::LaneCrossing const before = host::Lane_Through(plan.list, 1.0, 0, exits[1].data(), plan.climbing);
		std::vector<int32_t> route;
		int32_t target = -1;

		Check(!before.continuous && before.why.find("box 8 on the way is a floor box") == 0, "the descent: the lane 0 -> exit 1 breaks on the floor box 8 (" + before.why + ")");
		Check(host::AutoPlot_ShortcutWords(before.why, &route, &target) && route == std::vector<int32_t>({ 0, 1, 2, 7, 8 }) && target == 6,
			"Lane_Through's own words read back: the route 0 1 2 7 8, the target's box 6");

		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0, true);

		Check(LaneHolds(plan, exits, 0, 1) && LaneHolds(plan, exits, 1, 0), "after the pass both lanes hold (0 -> exit 1 along the ceiling, 5 -> exit 0)");
		Check((FlagsOf(plan.list, 8, 7) & host::kLinkOneWay) != 0 && (FlagsOf(plan.list, 7, 8) & host::kLinkOneWayBack) != 0,
			"the step 7 -> 8 stopped as inteng makes a one-way link (8 lists 7 One way, 7 lists 8 One way the other direction)");
		Check((FlagsOf(plan.list, 7, 8) & host::kLinkOneWay) == 0 && (FlagsOf(plan.list, 8, 7) & host::kLinkAlien) != 0,
			"the floor box still steps up to the wall (7 does not list 8 One way), and the link stays aliens only");
		Check(c.lanes == 2 && c.continuousBefore == 1 && c.continuousAfter == 2 && c.kept == 1 && c.madeOneWay == 1 && c.joined == 0,
			"counts: 2 lanes, 1 -> 2 continuous, 1 kept, 1 step stopped, none joined (tried " + std::to_string(c.tried) + ")");
		Check(!plan.notes.empty() && plan.notes.back().find("shortcuts: lanes along 1 -> 2 of 2 continuous") == 0, "the plan's note says so");
		Check(FlagsOf(plan.list, 8, 6) == 0 && FlagsOf(plan.list, 6, 8) == 0,
			"the exit-end try (exit 1's box stopped from stepping on to the floor) made no lane hold alone, so it was undone: 6 - 8 plain both ways, as it was");
	}

	// THE WAY THROUGH ANOTHER EXIT'S BOX: exit 1's door box (3) has no link to its exit's box (4), so the only way to it is from exit 2's door box (5) down into exit 2's box
	// (6) and across the floor (6 - 4). Rule 1 joins 3 and 4; rule 2 makes both exit boxes ends: 4 and 6 stopped both ways, no longer joined
	{
		host::AutoPlotPlan plan;
		std::vector<host::WaypointRec> &l = plan.list;
		int32_t const a = host::kLinkAlien;

		l = { Ceil(0, 6000), Ceil(-1000, 0), Floor(-1000, 0), Ceil(6000, 7000), Floor(6000, 7000), Ceil(5000, 6000, 1000, 2000), Floor(5000, 6000, 1000, 2000) };
		plan.climbing = { true, true, false, true, false, true, false };
		Join(&l, 0, 1, a), Join(&l, 0, 3, a), Join(&l, 0, 5, a), Join(&l, 1, 2, a), Join(&l, 5, 6, a), Join(&l, 6, 4, 0);
		plan.doorBox = { 1, 3, 5 };
		plan.exitBox = { 2, 4, 6 };
		plan.doorFloors = { {}, {}, {} };

		std::vector<std::vector<int32_t> > const exits = { { -500, -500, 500 }, { 6500, -500, 500 }, { 5500, -500, 1500 } };

		Check(!LaneHolds(plan, exits, 0, 1) && !LaneHolds(plan, exits, 2, 1) && LaneHolds(plan, exits, 1, 2), "through another exit's box: the lanes toward exit 1 break, 1 -> 2 holds");

		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0, true);
		bool all = true;

		for (size_t i = 0; i < 3; i++)
		{
			for (size_t j = 0; j < 3; j++)
			{
				all = all && (i == j || LaneHolds(plan, exits, i, j));
			}
		}

		Check(all, "after the pass all six lanes hold");
		Check(FlagsOf(plan.list, 3, 4) == host::kLinkAlien && FlagsOf(plan.list, 4, 3) == host::kLinkAlien, "exit 1's door box joined to its exit's box, aliens only, both ways");
		Check(FlagsOf(plan.list, 4, 6) < 0 && FlagsOf(plan.list, 6, 4) < 0, "the two exit boxes no longer joined (each an end: stopped both ways)");
		Check(c.joined == 1 && c.madeOneWay == 2 && c.kept == 1 && c.continuousBefore == 4 && c.continuousAfter == 6,
			"counts: 1 joined, 2 steps stopped, one try kept, 4 -> 6 of 6 (" + std::to_string(c.continuousBefore) + " -> " + std::to_string(c.continuousAfter) + ")");
	}

	// THE SAME WITH NO CLIMB STATION at any doorway: rules 1 and 2 are not made (an alien in an exit box would head straight up and stall), and rule 3's only cut (5 -> 6)
	// would break the lanes toward exit 2, so nothing is kept
	{
		host::AutoPlotPlan plan;
		std::vector<host::WaypointRec> &l = plan.list;
		int32_t const a = host::kLinkAlien;

		l = { Ceil(0, 6000), Ceil(-1000, 0), Floor(-1000, 0), Ceil(6000, 7000), Floor(6000, 7000), Ceil(5000, 6000, 1000, 2000), Floor(5000, 6000, 1000, 2000) };
		plan.climbing = { true, true, false, true, false, true, false };
		Join(&l, 0, 1, a), Join(&l, 0, 3, a), Join(&l, 0, 5, a), Join(&l, 1, 2, a), Join(&l, 5, 6, a), Join(&l, 6, 4, 0);
		plan.doorBox = { 1, 3, 5 };
		plan.exitBox = { 2, 4, 6 };
		plan.doorFloors = { {}, {}, {} };

		std::vector<std::vector<int32_t> > const exits = { { -500, -500, 500 }, { 6500, -500, 500 }, { 5500, -500, 1500 } };
		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits, false), 1.0, true);

		Check(FlagsOf(plan.list, 3, 4) < 0 && FlagsOf(plan.list, 6, 4) == 0 && c.joined == 0 && c.kept == 0,
			"no climb station: exit 1's door box not joined, the exit boxes as they were, nothing kept (" + std::to_string(c.tried) + " tried)");

		// and with stations, but rules 1 and 2 not asked for (the command's call, Dietrich 11-20: rule 3 alone): the same, nothing joined or made an end
		host::AutoPlotShortcutCounts const d = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0);

		Check(FlagsOf(plan.list, 3, 4) < 0 && FlagsOf(plan.list, 6, 4) == 0 && d.joined == 0 && d.kept == 0, "rules 1 and 2 not asked for: nothing joined, nothing made an end");

		// and asked for, with stations, but exit 1 a HANGING-BAND exit (its point 700 under its doorway's soffit, less than a hanging alien's 900): no end made at exit 1, so
		// its door box is not joined (exit 2's end alone makes no lane hold: not kept)
		std::vector<host::AutoPlotDoor> band = Stations(exits);

		band[1].soffitFound = true;
		band[1].soffit[0] = 6500, band[1].soffit[1] = -1200, band[1].soffit[2] = 500;

		host::AutoPlotShortcutCounts const e = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), band, 1.0, true);

		Check(FlagsOf(plan.list, 3, 4) < 0 && e.joined == 0 && e.kept == 0, "a hanging-band exit (700 under its soffit): no end made there, its door box not joined");
	}

	// RULE 2 WITH NO CLIMB STATION (Dietrich 11-20, his 11 ends: the arriving alien held by its doorway's door box steps DOWN into that same exit's own box and on across the
	// floor): door boxes 2 and 4 each joined to their own exit's box (3, 5), a floor (6) between the two exit boxes, and a ceiling of two boxes (0, 1) one box longer than the
	// floor way. Each exit box made an end (no step on to the floor): both lanes hold, nothing joined (no station), the floor still steps into each exit box
	{
		host::AutoPlotPlan plan;
		std::vector<host::WaypointRec> &l = plan.list;
		int32_t const a = host::kLinkAlien;

		// (and a wall (8) down to the floor from a ceiling stub (7) off box 1: the floor's own way in, longer than the ceiling's, so with the exit boxes made ends the floor
		// is still reached: else Frost's checker finds a box nothing steps into, and the try is rightly undone)
		l = { Ceil(0, 3000), Ceil(3000, 6000), Ceil(-1000, 0), Floor(-1000, 0), Ceil(6000, 7000), Floor(6000, 7000), Floor(0, 6000), Ceil(3000, 4000, 1000, 2000),
			Box(3000, kCeilTop, 1000, 3100, kFloorBottom, 1100) };
		plan.climbing = { true, true, true, false, true, false, false, true, true };
		Join(&l, 0, 1, a), Join(&l, 0, 2, a), Join(&l, 1, 4, a), Join(&l, 2, 3, a), Join(&l, 4, 5, a), Join(&l, 3, 6, 0), Join(&l, 6, 5, 0), Join(&l, 1, 7, a), Join(&l, 7, 8, a),
			Join(&l, 8, 6, a);
		plan.doorBox = { 2, 4 };
		plan.exitBox = { 3, 5 };
		plan.doorFloors = { {}, {} };

		std::vector<std::vector<int32_t> > const exits = { { -500, -500, 500 }, { 6500, -500, 500 } };
		host::LaneCrossing const before = host::Lane_Through(plan.list, 1.0, 2, exits[1].data(), plan.climbing);

		Check(!before.continuous && before.why.find("box 3 on the way is a floor box (the route 2 3,") == 0, "own exit box on the way: door 2 steps down into exit 0's box 3 (" + before.why + ")");

		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits, false), 1.0, true);

		Check(LaneHolds(plan, exits, 0, 1) && LaneHolds(plan, exits, 1, 0), "no station, rule 2: both lanes hold along the ceiling");
		Check((FlagsOf(plan.list, 6, 3) & host::kLinkOneWay) != 0 && (FlagsOf(plan.list, 3, 6) & host::kLinkOneWay) == 0 &&
			(FlagsOf(plan.list, 6, 5) & host::kLinkOneWay) != 0 && (FlagsOf(plan.list, 5, 6) & host::kLinkOneWay) == 0,
			"each exit box stopped from stepping on to the floor; the floor still steps into each (3 and 5 do not list 6 One way)");
		Check(c.joined == 0 && c.madeOneWay == 2 && c.kept == 1, "counts: nothing joined (no station), 2 steps stopped, one try kept");
	}

	// A CUT THAT WOULD BREAK ANOTHER LANE IS UNDONE: as the descent, but exit 2's point lies on the floor box 8 (no lane there), reached only through the wall: stopping 7 -> 8
	// would leave the lane 0 -> exit 2 no way, so that cut is not kept
	{
		std::vector<std::vector<int32_t> > exits;
		host::AutoPlotPlan plan = Descent(&exits, true);

		Check(LaneHolds(plan, exits, 0, 2) && !LaneHolds(plan, exits, 0, 1) && !LaneHolds(plan, exits, 1, 2), "exit 2 on the floor: 0 -> exit 2 holds, 0 -> exit 1 and 5 -> exit 2 break");

		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0, true);

		Check(LaneHolds(plan, exits, 0, 2) && LaneHolds(plan, exits, 1, 2), "after: 0 -> exit 2 still holds, 5 -> exit 2 holds (exit 1's box an end: no way on to the floor)");
		Check(!LaneHolds(plan, exits, 0, 1), "0 -> exit 1 still breaks: its only cut (7 -> 8) would break 0 -> exit 2");
		Check(FlagsOf(plan.list, 8, 7) == host::kLinkAlien && FlagsOf(plan.list, 7, 8) == host::kLinkAlien, "the wall's link to the floor as it was (the cut undone)");
		Check(c.kept == 1 && c.madeOneWay == 1 && c.tried >= 2 && c.continuousAfter == c.continuousBefore + 1,
			"counts: the exit end kept, the cut tried and not kept (" + std::to_string(c.tried) + " tries, " + std::to_string(c.continuousBefore) + " -> " +
			std::to_string(c.continuousAfter) + ")");
	}

	// FEWER BREAK, BUT ONE THAT HELD WOULD BREAK: undone all the same. As the descent, with lanes only from exit 0's door box: exit 1's box at the ceiling's end, exit 2's
	// point on the floor (box 8), exit 4's box (10) beside the floor, reached down from the ceiling's end (11) or across the floor. Stopping 7 -> 8 makes 0 -> exit 1 and
	// 0 -> exit 4 hold (two fewer break) but 0 -> exit 2, which held, breaks
	{
		std::vector<std::vector<int32_t> > exits;
		host::AutoPlotPlan plan = Descent(&exits, true);
		int32_t const a = host::kLinkAlien;

		plan.list.push_back(Floor(3000, 4000, 1000, 2000));   // 10: exit 4's box
		plan.list.push_back(Ceil(4000, 5000, 1000, 2000));    // 11: over it, from the ceiling's box 4
		plan.climbing.push_back(false);
		plan.climbing.push_back(true);
		Join(&plan.list, 8, 10, 0), Join(&plan.list, 4, 11, a), Join(&plan.list, 11, 10, a);
		exits.push_back({ 0, 0, 0 });   // (exit 3: a point no box holds)
		exits.push_back({ 3500, -500, 1500 });
		plan.doorBox = { 0, -1, -1, -1, -1 };   // lanes from exit 0's door box only
		plan.exitBox = { 9, 6, -1, -1, 10 };
		plan.doorFloors = { {}, {}, {}, {}, {} };

		Check(LaneHolds(plan, exits, 0, 2) && !LaneHolds(plan, exits, 0, 1) && !LaneHolds(plan, exits, 0, 4), "before: 0 -> exit 2 holds, 0 -> exit 1 and 0 -> exit 4 break");

		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0, true);

		Check(LaneHolds(plan, exits, 0, 2) && (FlagsOf(plan.list, 8, 7) & host::kLinkOneWay) == 0, "after: 0 -> exit 2 still holds, 7 -> 8 not stopped (the cut undone)");
		Check(c.kept == 0 && c.tried >= 1, "nothing kept (" + std::to_string(c.tried) + " tried)");
	}

	// A PLAN WHOSE LANES ALL HOLD: nothing changed, no note
	{
		std::vector<std::vector<int32_t> > exits;
		host::AutoPlotPlan plan = Descent(&exits, false);

		host::Waypoints_DeleteLink(&plan.list, 7, 8);   // no floor way: the ceiling's is the only one
		std::vector<host::WaypointRec> const was = plan.list;
		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0, true);
		bool same = was.size() == plan.list.size();

		for (size_t i = 0; same && i < was.size(); i++)
		{
			same = was[i].links.size() == plan.list[i].links.size();

			for (size_t k = 0; same && k < was[i].links.size(); k++)
			{
				same = was[i].links[k].index == plan.list[i].links[k].index && was[i].links[k].flags == plan.list[i].links[k].flags;
			}
		}

		Check(same && c.tried == 0 && c.kept == 0 && plan.notes.empty() && c.continuousAfter == 2, "every lane holds: nothing tried, nothing changed, no note");
	}

	// THE WORDS: only Lane_Through's floor-box break is read
	{
		std::vector<int32_t> route;
		int32_t target = -1;

		Check(!host::AutoPlot_ShortcutWords("", &route, &target) && !host::AutoPlot_ShortcutWords("no route from box 3", &route, &target) &&
			!host::AutoPlot_ShortcutWords("the link from box 3 to box 4 is plain (link shift 2)", &route, &target), "other breaks' words: not read");
		Check(!host::AutoPlot_ShortcutWords("box 8 on the way is a floor box (the route 0 1 2 7, the target's box 6)", &route, &target),
			"a route that does not end at the floor box: not read");
		Check(!host::AutoPlot_ShortcutWords("box 8 on the way is a floor box (the route 0 1 2 7 8, the target's box 6", &route, &target) &&
			!host::AutoPlot_ShortcutWords("box 8 on the way is a floor box (the route 0 1 2 7 8, the target's box 6) and more", &route, &target),
			"cut short, or more after the close: not read");
		Check(!host::AutoPlot_ShortcutWords("box 12345678901 on the way is a floor box (the route 12345678901, the target's box 6)", &route, &target),
			"a number of more than 9 digits: not read");
		Check(host::AutoPlot_ShortcutWords("box 4 on the way is a floor box (the route 4, the target's box 0)", &route, &target) && route.size() == 1 && target == 0,
			"a route of one box: read (no step down to stop)");
		Check(host::AutoPlot_ShortcutWords("box 5 on the way is a floor box (the route 1 5, the target's box 13); link shift 7", &route, &target) &&
			route == std::vector<int32_t>({ 1, 5 }) && target == 13, "Lane_ThroughAll's words, with the branch's link shift: read");
		Check(!host::AutoPlot_ShortcutWords("box 5 on the way is a floor box (the route 1 5, the target's box 13); link shift", &route, &target) &&
			!host::AutoPlot_ShortcutWords("box 5 on the way is a floor box (the route 1 5, the target's box 13); link shift 7 more", &route, &target),
			"a link shift with no number, or more after it: not read");
	}

	// A TIE (Dietrich's Object16, 11-20): from exit 0's door box D0 (0) to exit 1's box T (3) the ceiling way D0 C2 D1 T and two floor ways, D0 F1 F3 T and D0 F2 F3 T,
	// each of 3 steps (F1 and F2 climb straight into D0, aliens only). The game gives each alien its own link shift, so some take a floor way: Lane_ThroughAll, which
	// walks every shift's route, breaks. The pass walks the ARRIVAL lanes so (the alien carried through a doorway, from the box that holds it 600 back along the way through
	// and 900 under the soffit, as Drake's gate): the one through exit 0's doorway, held by D0 alone, toward exit 1 is this tie. The door lanes stay on shift 0's walk,
	// where the tie reads continuous. The pass stops D0 -> F1 and D0 -> F2 in one chain, then every branch holds
	{
		host::AutoPlotPlan plan;
		std::vector<host::WaypointRec> &l = plan.list;
		int32_t const a = host::kLinkAlien;

		// 0 D0, 1 C2, 2 D1, 3 T (exit 1's box), 4 F1, 5 F2, 6 F3, 7 T0 (exit 0's box)
		l = { Ceil(-1000, 1000), Ceil(1000, 2000), Ceil(2000, 3000), Floor(2000, 3000), Floor(0, 1000, 1000, 2000), Floor(0, 1000, -1000, 0), Floor(1000, 2000), Floor(0, 1000) };
		plan.climbing = { true, true, true, false, false, false, false, false };
		Join(&l, 0, 1, a), Join(&l, 1, 2, a), Join(&l, 2, 3, a), Join(&l, 0, 7, a);
		Join(&l, 0, 4, a), Join(&l, 0, 5, a), Join(&l, 4, 6, 0), Join(&l, 5, 6, 0), Join(&l, 6, 3, 0);
		plan.doorBox = { 0, 2 };
		plan.exitBox = { 7, 3 };
		plan.doorFloors = { {}, {} };

		std::vector<std::vector<int32_t> > const exits = { { 500, -500, 500 }, { 2500, -500, 500 } };
		std::vector<host::AutoPlotDoor> doors = Stations(exits, false);

		doors[0].throughFound = doors[0].soffitFound = true;   // exit 0's doorway at D0's far end, out along -x: the alien arriving there is held by D0 alone
		doors[0].middle[0] = -1000, doors[0].middle[1] = -500, doors[0].middle[2] = 500;
		doors[0].through[0] = -1.0, doors[0].through[1] = 0.0;
		doors[0].soffit[0] = -1000, doors[0].soffit[1] = kCeilTop, doors[0].soffit[2] = 500;
		doors[1].throughFound = doors[1].soffitFound = true;   // exit 1's doorway at D1's far end, out along +x
		doors[1].middle[0] = 3000, doors[1].middle[1] = -500, doors[1].middle[2] = 500;
		doors[1].through[0] = 1.0, doors[1].through[1] = 0.0;
		doors[1].soffit[0] = 3000, doors[1].soffit[1] = kCeilTop, doors[1].soffit[2] = 500;

		bool const shiftZero = host::Lane_Through(plan.list, 1.0, 0, exits[1].data(), plan.climbing).continuous;
		host::LaneCrossing const all = host::Lane_ThroughAll(plan.list, 1.0, 0, exits[1].data(), plan.climbing);

		Check(!all.continuous && all.why.find("on the way is a floor box") != std::string::npos,
			"the tie: on every link shift D0 -> exit 1 breaks on a floor box (" + all.why + "; shift 0's walk alone: " + (shiftZero ? "continuous" : "breaks") + ")");

		host::AutoPlotShortcutCounts const c = host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), doors, 1.0, true);

		Check(host::Lane_ThroughAll(plan.list, 1.0, 0, exits[1].data(), plan.climbing).continuous, "after: D0 -> exit 1 holds on every link shift");
		Check((FlagsOf(plan.list, 4, 0) & host::kLinkOneWay) != 0 && (FlagsOf(plan.list, 5, 0) & host::kLinkOneWay) != 0 &&
			(FlagsOf(plan.list, 0, 4) & host::kLinkOneWay) == 0 && (FlagsOf(plan.list, 0, 5) & host::kLinkOneWay) == 0,
			"both floor branches' steps down stopped (D0 -> F1, D0 -> F2); F1 and F2 still climb into D0");
		Check(c.lanes == 4 && c.continuousBefore == 3 && c.continuousAfter == 4, "4 lanes counted (the door lanes D0 -> exit 1 and D1 -> exit 0, the arrivals through each "
			"doorway); only the arrival through exit 0's breaks before, all hold after (" + std::to_string(c.lanes) + " lanes, " + std::to_string(c.continuousBefore) + " -> " +
			std::to_string(c.continuousAfter) + ")");
	}

	// HOSTILE: no plan, no boxes, a scale that is not a number or not above 0, door boxes outside the list
	{
		std::vector<std::vector<int32_t> > exits;
		host::AutoPlotPlan plan = Descent(&exits, false);
		host::AutoPlotPlan empty;

		Check(host::AutoPlot_CutShortcuts(nullptr, exits, FileExits(exits), Stations(exits), 1.0, true).lanes == 0, "no plan: nothing");
		Check(host::AutoPlot_CutShortcuts(&empty, exits, FileExits(exits), Stations(exits), 1.0, true).lanes == 0 && empty.list.empty(), "no boxes: nothing");
		Check(host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), nan("")).lanes == 0 && host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 0.0).lanes == 0 &&
			plan.notes.empty(), "a scale that is not a number, or 0: nothing");

		plan.doorBox = { 99, -5 };
		Check(host::AutoPlot_CutShortcuts(&plan, exits, FileExits(exits), Stations(exits), 1.0, true).lanes == 0, "door boxes outside the list: no lane to walk");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
