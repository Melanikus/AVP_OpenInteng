// host_autoplot_repair_test.cpp: THE REPAIR of a planned alien list (src/host/autoplot_repair.h AutoPlot_Repair) on made lists, one for each of Genshd1's three refusal
// kinds (69c0f1a0's whole run: 24 overlapping centres in 3 shapes, 14 islands), each checked by Frost's own checker (Waypoints_Check) after the pass; then what it must never
// do (take out or trim away an exit, touch a clean list) and the hostile lists. Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_repair.h"

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

using host::WaypointRec;
using host::WaypointPoint;

// a box (the file's units, y down) and its centre
static WaypointRec Box(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1, int32_t cx, int32_t cy, int32_t cz)
{
	WaypointRec r = {};

	r.min[0] = x0, r.min[1] = y0, r.min[2] = z0;
	r.max[0] = x1, r.max[1] = y1, r.max[2] = z1;
	r.centre[0] = cx, r.centre[1] = cy, r.centre[2] = cz;
	return r;
}

static void Link(std::vector<WaypointRec> *list, int a, int b, int32_t flags)
{
	(*list)[(size_t)a].links.push_back({ b, flags });
	(*list)[(size_t)b].links.push_back({ a, flags });
}

static WaypointPoint Point(int32_t x, int32_t y, int32_t z)
{
	WaypointPoint p = {};

	p.place[0] = x, p.place[1] = y, p.place[2] = z;
	return p;
}

static bool Clean(std::vector<WaypointRec> const &list, std::vector<WaypointPoint> const &exits)
{
	return host::Waypoints_Check(list, exits, 1.0).empty();
}

static std::string FirstProblem(std::vector<WaypointRec> const &list, std::vector<WaypointPoint> const &exits)
{
	std::vector<host::WaypointProblem> const p = host::Waypoints_Check(list, exits, 1.0);

	return p.empty() ? std::string("none") : p[0].words;
}

// a room's floor box (x 0..6000, z 0..6000, 2 m tall, its centre given) and an exit box past its +x side holding the exit point at (5900, -100, 3000)
static void Room(std::vector<WaypointRec> *list, std::vector<WaypointPoint> *exits, int32_t cx, int32_t cy, int32_t cz)
{
	list->push_back(Box(0, -2000, 0, 6000, 0, 6000, cx, cy, cz));
	list->push_back(Box(5800, -2000, 2500, 6400, 0, 3500, 6200, -500, 3000));
	Link(list, (int)list->size() - 2, (int)list->size() - 1, 0);
	exits->push_back(Point(5900, -100, 3000));
}

int main()
{
	printf("RULE 3: TWO BOXES HOLDING EACH OTHER'S CENTRES ARE MADE ONE (two exits' door boxes at one doorway: 162, 219, 224, 287, 290, 390, 391)\n");
	{
		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;

		Room(&list, &exits, 3000, -500, 3000);
		list.push_back(Box(5500, -3500, 1000, 6500, -3000, 3000, 6200, -3200, 2000));   // door box of one exit
		list.push_back(Box(5600, -3500, 1200, 6600, -3000, 3200, 6300, -3200, 2100));   // the other exit's, at the same doorway
		Link(&list, 1, 2, 4);
		Link(&list, 1, 3, 4);
		Check(FirstProblem(list, exits).find("centre is in box") != std::string::npos, "before: a door box's centre is in the other (" + FirstProblem(list, exits) + ")");

		std::string words;
		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&list, exits, 1.0, &words);

		Check(Clean(list, exits), "after: the checker finds nothing (" + FirstProblem(list, exits) + ")");
		Check(c.madeOne == 1 && list.size() == 3, "the two door boxes made one (3 boxes, was 4)");
		Check(!list.empty() && list.back().min[0] == 5500 && list.back().max[0] == 6600 && list.back().max[2] == 3200, "the one box is the union of the two");
		Check(words.find("repair") != std::string::npos, "it says what it did (" + words + ")");
	}

	printf("RULE 4: A CLIMBING BOX'S CENTRE IN ANOTHER BOX: THAT ONE IS TRIMMED (a wall box's centre in a tall floor box: 89, 168, 174, 203, 275, 289, 312, 313)\n");
	{
		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;

		Room(&list, &exits, 3000, -500, 3000);
		list.push_back(Box(-200, -3000, 2000, 400, 0, 4000, 200, -1500, 3000));   // a wall box at x 0: its centre 200 into the room, in the floor box
		Link(&list, 0, 2, 4);
		Check(FirstProblem(list, exits).find("centre is in box") != std::string::npos, "before: the wall box's centre is in the floor box (" + FirstProblem(list, exits) + ")");

		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&list, exits, 1.0);

		Check(Clean(list, exits), "after: the checker finds nothing (" + FirstProblem(list, exits) + ")");
		Check(c.trimmed == 1 && list.size() == 3 && list[0].min[0] > 200 && list[0].max[0] == 6000, "the floor box trimmed on its -x face, just past the wall box's centre");
		Check(list[2].min[0] == -200 && list[2].centre[0] == 200, "the wall box as it was (its centre where the planner put it)");
	}

	printf("RULE 5: ANY OTHER CENTRE IN ANOTHER BOX IS MOVED (a merged floor rectangle's centre in a wall or station box: 43, 84, 87, 216, 242, 305, 371, 373, 379)\n");
	{
		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;

		Room(&list, &exits, 300, -500, 3000);                                        // the floor box's centre 300 from the wall
		list.push_back(Box(-200, -3000, 2000, 800, 0, 4000, -100, -1500, 3000));   // a wall box reaching 800 into the room
		Link(&list, 0, 2, 4);
		Check(FirstProblem(list, exits).find("centre is in box") != std::string::npos, "before: the floor box's centre is in the wall box (" + FirstProblem(list, exits) + ")");

		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&list, exits, 1.0);

		Check(Clean(list, exits), "after: the checker finds nothing (" + FirstProblem(list, exits) + ")");
		Check(c.centresMoved == 1 && list.size() == 3, "the floor box's centre moved");
		Check(list[0].centre[1] == -500 && list[0].min[0] == 0 && list[0].max[0] == 6000, "at its own height, the box as it was");
		Check(list[0].centre[0] >= 800 && list[0].centre[0] <= 1600, "to the nearest place out of the wall box (x " + std::to_string(list[0].centre[0]) + ")");
	}

	printf("RULE 1: A PART THAT NO EXIT REACHES IS TAKEN OUT (12 of the 14 islands: 56, 72, 102, 105, 116, 124, 138, 368, 377, 381, 389...)\n");
	{
		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;

		list.push_back(Box(20000, -2000, 0, 22000, 0, 2000, 21000, -500, 1000));   // the island first, so the rest is numbered again
		list.push_back(Box(22000, -2000, 0, 24000, 0, 2000, 23000, -500, 1000));
		Link(&list, 0, 1, 0);
		Room(&list, &exits, 3000, -500, 3000);
		Check(FirstProblem(list, exits).find("cannot reach") != std::string::npos, "before: a part cannot reach the rest (" + FirstProblem(list, exits) + ")");

		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&list, exits, 1.0);

		Check(Clean(list, exits), "after: the checker finds nothing (" + FirstProblem(list, exits) + ")");
		Check(c.partsTakenOut == 1 && c.boxesTakenOut == 2 && list.size() == 2, "the island's 2 boxes taken out");
		Check(list.size() == 2 && list[0].min[0] == 0 && list[0].links.size() == 1 && list[0].links[0].index == 1 && list[1].links[0].index == 0,
			"the rest numbered again, its links with it");
	}

	printf("RULE 2: A PART WITHIN THE JOIN GAP IS LINKED (186: 0.55 m off)\n");
	{
		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;

		Room(&list, &exits, 3000, -500, 3000);
		list.push_back(Box(-2500, -2000, 0, -500, 0, 2000, -1500, -500, 1000));   // 500 off the room's -x side, no link
		Check(FirstProblem(list, exits).find("cannot reach") != std::string::npos, "before: it cannot reach the rest (" + FirstProblem(list, exits) + ")");

		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&list, exits, 1.0);

		Check(Clean(list, exits), "after: the checker finds nothing (" + FirstProblem(list, exits) + ")");
		Check(c.partsLinked == 1 && list.size() == 3 && !list[2].links.empty() && list[2].links[0].index == 0 && list[2].links[0].flags == 0, "linked to the room's box, plain");
	}

	printf("RULE 6: TWO AREAS EACH WITH AN EXIT, FAR APART (295 shft10, 364 air06), Bishop 11-20: (b) joined aliens-only where the grid climbs between them, else (a)\n");
	{
		// the room (2 boxes, 1 exit) and a far area: one box 14 m off holding its own exit point. Its nearest box to the room's: box 1 (the room's exit box)
		auto const twoAreas = [](std::vector<WaypointRec> *list, std::vector<WaypointPoint> *exits) {
			Room(list, exits, 3000, -500, 3000);
			list->push_back(Box(20000, -2000, 0, 22000, 0, 2000, 21000, -500, 1000));
			exits->push_back(Point(21500, -100, 1000));
		};
		int32_t asked[2][3] = {};
		int calls = 0;

		// the grid's alien way: off the far box's floor, up its wall, along the ceiling, down into the room's exit box (every point outside a box on a wall or a ceiling of module 7)
		auto const climbs = [&](int32_t const from[3], int32_t const to[3], std::vector<host::AutoPlotPoint> *way) {
			calls++;
			for (int i = 0; i < 3; i++)
			{
				asked[0][i] = from[i], asked[1][i] = to[i];
			}
			int32_t const places[4][3] = { { 21000, -500, 1000 }, { 19000, -1500, 1000 }, { 12000, -2500, 2000 }, { 6200, -500, 3000 } };
			host::AutoPlotPoint::Surface const surfaces[4] = { host::AutoPlotPoint::Surface::Floor, host::AutoPlotPoint::Surface::Wall, host::AutoPlotPoint::Surface::Ceiling,
				host::AutoPlotPoint::Surface::Floor };
			way->clear();
			for (int n = 0; n < 4; n++)
			{
				host::AutoPlotPoint p;
				p.place[0] = places[n][0], p.place[1] = places[n][1], p.place[2] = places[n][2];
				p.surface = surfaces[n];
				p.module = 7;
				way->push_back(p);
			}
			return true;
		};
		std::vector<int32_t> const modules(1, 7);

		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;
		std::string words;

		twoAreas(&list, &exits);
		Check(FirstProblem(list, exits).find("cannot reach") != std::string::npos, "before: the far area cannot reach the room (" + FirstProblem(list, exits) + ")");

		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&list, exits, 1.0, &words, nullptr, climbs, modules);

		Check(Clean(list, exits), "(b) after: the checker finds nothing (" + FirstProblem(list, exits) + ")");
		Check(c.areasJoined == 1 && c.boxesTakenOut == 0 && list.size() == 3, "(b) the two areas joined, nothing taken out");
		Check(list.size() == 3 && list[2].links.size() == 1 && list[2].links[0].index == 1 && list[2].links[0].flags == host::kLinkAlien && list[1].links.size() == 2 &&
			list[1].links[1].index == 2 && list[1].links[1].flags == host::kLinkAlien, "(b) by one link both ways, aliens only, between the nearest boxes (the far box and the room's exit box)");
		Check(asked[0][0] == 21000 && asked[0][2] == 1000 && asked[1][0] == 6200 && asked[1][2] == 3000, "(b) the grid asked from the far box's centre to the exit box's (the world's mm)");
		Check(words.find("joined") != std::string::npos, "(b) said (" + words + ")");

		// the same way, but a stretch of it on a floor no box covers: not a climbable surface
		auto const overFloor = [&](int32_t const from[3], int32_t const to[3], std::vector<host::AutoPlotPoint> *way) {
			climbs(from, to, way);
			(*way)[2].surface = host::AutoPlotPoint::Surface::Floor;
			return true;
		};
		// the same way, but through another module's space (module 9)
		auto const elsewhere = [&](int32_t const from[3], int32_t const to[3], std::vector<host::AutoPlotPoint> *way) {
			climbs(from, to, way);
			(*way)[1].module = 9;
			return true;
		};
		auto const noWay = [&](int32_t const *, int32_t const *, std::vector<host::AutoPlotPoint> *way) {
			calls++;
			way->clear();
			return false;
		};
		struct Case
		{
			char const *what;
			host::AutoPlotRepairWay way;
		};
		Case const cases[] = { { "a floor stretch no box covers", overFloor }, { "a way through another module", elsewhere }, { "no way at all", noWay },
			{ "no grid given", host::AutoPlotRepairWay() } };

		for (Case const &one : cases)
		{
			std::vector<WaypointRec> two;
			std::vector<WaypointPoint> twoExits;
			std::string said;

			twoAreas(&two, &twoExits);
			calls = 0;
			host::AutoPlotRepairCounts const a = host::AutoPlot_Repair(&two, twoExits, 1.0, &said, nullptr, one.way, modules);

			Check(a.areasJoined == 0 && a.areasTakenOut == 1 && a.exitsLeftOut == 1 && two.size() == 2 && two[0].min[0] == 0,
				std::string("(a) ") + one.what + ": not joined; the area with fewer exits (then fewer boxes: the far one) taken out, the room kept");
			Check(FirstProblem(two, twoExits).find("exit 1") != std::string::npos && said.find("taken out") != std::string::npos && said.find("left") != std::string::npos,
				std::string("(a) ") + one.what + ": its exit point now in no box, which the checker refuses, and that is said (" + said + ")");
			Check(calls <= (int)host::kAutoPlotRepairMostWays, std::string("(a) ") + one.what + ": the grid asked at most " + std::to_string(host::kAutoPlotRepairMostWays) +
				" times (" + std::to_string(calls) + ")");
			Check(a.leftOutExits == std::vector<int32_t>({ 1 }) && said.find("exit 1 left out, rule 6(a): no alien way to the rest") != std::string::npos,
				std::string("(a) ") + one.what + ": the exit left out named (Bishop: \"exit N left out, rule 6(a): no alien way to the rest\")");
		}

		// AFTER (a), THE PASS GOES ON (apcend on d4798b45: refused, "box 13: its centre is in box 46 too", the repair stopped at the exit it had itself left out): the room's
		// two door boxes holding each other's centres (rule 3) are still made one, and only the excepted exit is left
		{
			std::vector<WaypointRec> list;
			std::vector<WaypointPoint> exits;
			std::string said;

			twoAreas(&list, &exits);
			list.push_back(Box(5500, -3500, 1000, 6500, -3000, 3000, 6200, -3200, 2000));
			list.push_back(Box(5600, -3500, 1200, 6600, -3000, 3200, 6300, -3200, 2100));
			Link(&list, 1, 3, 4);
			Link(&list, 1, 4, 4);
			host::AutoPlotRepairCounts const after = host::AutoPlot_Repair(&list, exits, 1.0, &said, nullptr, noWay, modules);
			std::vector<host::WaypointProblem> left = host::Waypoints_Check(list, exits, 1.0), excepted;
			host::AutoPlot_RepairExceptions(&left, after.leftOutExits, &excepted);
			Check(after.areasTakenOut == 1 && after.madeOne == 1 && left.empty() && excepted.size() == 1,
				"after (a) the pass goes on: the two door boxes made one, only the excepted exit left (" + (left.empty() ? std::string("nothing else") : left[0].words) + "; " + said + ")");
		}

		// THE CHECKER'S EXCEPTIONS (Bishop 11-20: write them, the finding still printed with its reason; Frost's checker unchanged)
		host::AutoPlotPlan split;
		std::vector<WaypointPoint> splitExits;

		twoAreas(&split.list, &splitExits);
		split.climbing.assign(split.list.size(), false);
		host::AutoPlot_RepairPlan(&split, splitExits, 1.0, noWay, modules);
		Check(split.exitsLeftOut == std::vector<int32_t>({ 1 }), "the plan: exitsLeftOut names the exit rule 6 (a) left out");

		std::vector<host::WaypointProblem> problems = host::Waypoints_Check(split.list, splitExits, 1.0), excepted;
		size_t const found = problems.size();
		size_t const moved = host::AutoPlot_RepairExceptions(&problems, split.exitsLeftOut, &excepted);
		Check(found == 1 && moved == 1 && problems.empty() && excepted.size() == 1 && excepted[0].kind == host::WaypointProblemKind::ExitOutside && excepted[0].other == 1,
			"the checker's \"exit 1 in no box\" moved to the exceptions, nothing left: written (" + std::to_string(found) + " found)");
		Check(Clean(split.list, std::vector<WaypointPoint>(1, splitExits[0])), "and the rest is clean by the checker");

		std::vector<host::WaypointProblem> other = host::Waypoints_Check(split.list, splitExits, 1.0), none;
		Check(host::AutoPlot_RepairExceptions(&other, std::vector<int32_t>({ 0 }), &none) == 0 && other.size() == 1 && none.empty(), "an exit NOT left out by rule 6 (a): stays a problem");

		std::vector<host::WaypointProblem> mixed = host::Waypoints_Check(split.list, splitExits, 1.0);
		host::WaypointProblem reach;
		reach.kind = host::WaypointProblemKind::CannotReach, reach.box = 0, reach.other = 1, reach.words = "box 0 cannot reach box 1";
		mixed.push_back(reach);
		std::vector<host::WaypointProblem> out;
		host::AutoPlot_RepairExceptions(&mixed, split.exitsLeftOut, &out);
		Check(mixed.size() == 1 && mixed[0].kind == host::WaypointProblemKind::CannotReach && out.size() == 1, "any other problem stays (a box that cannot reach another: refused)");
		Check(host::AutoPlot_RepairExceptions(nullptr, split.exitsLeftOut, nullptr) == 0, "hostile: no lists: nothing moved");

		// (a) keeps the area with MORE exits: a far area of two boxes holding two exit points, the room one
		std::vector<WaypointRec> more;
		std::vector<WaypointPoint> moreExits;

		Room(&more, &moreExits, 3000, -500, 3000);
		more.push_back(Box(20000, -2000, 0, 22000, 0, 2000, 21000, -500, 1000));
		more.push_back(Box(22000, -2000, 0, 24000, 0, 2000, 23000, -500, 1000));
		Link(&more, 2, 3, 0);
		moreExits.push_back(Point(21500, -100, 1000));
		moreExits.push_back(Point(23500, -100, 1000));
		host::AutoPlotRepairCounts const m = host::AutoPlot_Repair(&more, moreExits, 1.0, nullptr, nullptr, noWay, modules);
		Check(m.areasTakenOut == 1 && more.size() == 2 && more[0].min[0] == 20000 && more[1].min[0] == 22000, "(a) the area with more exits kept (the far one, 2 exits), the room taken out");
	}

	printf("THE RENUMBERING AND THE PLAN (Dietrich: climbing, doorBox, exitBox and doorFloors index the list; the lane table reads them after)\n");
	{
		// rule 3's list: the two door boxes 2 and 3 made one
		std::vector<WaypointRec> list;
		std::vector<WaypointPoint> exits;

		Room(&list, &exits, 3000, -500, 3000);
		list.push_back(Box(5500, -3500, 1000, 6500, -3000, 3000, 6200, -3200, 2000));
		list.push_back(Box(5600, -3500, 1200, 6600, -3000, 3200, 6300, -3200, 2100));
		Link(&list, 1, 2, 4);
		Link(&list, 1, 3, 4);
		std::vector<int32_t> renumber;
		host::AutoPlot_Repair(&list, exits, 1.0, nullptr, &renumber);
		Check(renumber == std::vector<int32_t>({ 0, 1, 2, 2 }), "made one: both door boxes are now box 2");

		// rule 1's: the island (boxes 1 and 2) taken out from between the room's floor box (0) and its exit box (3)
		host::AutoPlotPlan plan;

		plan.list.push_back(Box(0, -2000, 0, 6000, 0, 6000, 3000, -500, 3000));
		plan.list.push_back(Box(20000, -2000, 0, 22000, 0, 2000, 21000, -500, 1000));
		plan.list.push_back(Box(22000, -2000, 0, 24000, 0, 2000, 23000, -500, 1000));
		plan.list.push_back(Box(5800, -2000, 2500, 6400, 0, 3500, 6200, -500, 3000));
		Link(&plan.list, 1, 2, 4);
		Link(&plan.list, 0, 3, 0);
		std::vector<WaypointPoint> planExits(1, Point(5900, -100, 3000));
		plan.climbing = { false, true, true, false };
		plan.doorBox = { 3, -1 };
		plan.exitBox = { 3 };
		plan.doorFloors = { { 0, 2, 3 } };
		plan.floorBoxes = 2;
		plan.parts = 2;
		plan.notes.push_back("the planner's own note");

		host::AutoPlot_RepairPlan(&plan, planExits, 1.0);
		Check(plan.list.size() == 2 && plan.climbing == std::vector<bool>({ false, false }), "the plan: the island's 2 boxes out, climbing follows (2 floor boxes)");
		Check(plan.doorBox == std::vector<int32_t>({ 1, -1 }) && plan.exitBox == std::vector<int32_t>({ 1 }), "doorBox and exitBox follow (3 is now 1; none stays none)");
		Check(plan.doorFloors.size() == 1 && plan.doorFloors[0] == std::vector<int32_t>({ 0, 1 }), "doorFloors follow, the box taken out left out");
		Check(plan.floorBoxes == 2 && plan.parts == 1, "floorBoxes and parts counted again");
		Check(plan.notes.size() == 2 && plan.notes[0] == "the planner's own note" && plan.notes[1].find("repair") == 0, "the note added after the planner's (" +
			(plan.notes.size() > 1 ? plan.notes[1] : std::string("none")) + ")");

		host::AutoPlotPlan clean;
		Room(&clean.list, &planExits, 3000, -500, 3000);
		planExits.resize(1);
		clean.climbing = { false, false };
		clean.doorBox = { 1 };
		host::AutoPlot_RepairPlan(&clean, planExits, 1.0);
		Check(clean.list.size() == 2 && clean.doorBox == std::vector<int32_t>({ 1 }) && clean.notes.empty(), "a clean plan: as it was, no note");

		host::AutoPlotPlan shortKinds = plan;
		shortKinds.climbing.clear();
		shortKinds.doorBox = { 77, -5 };
		host::AutoPlot_RepairPlan(&shortKinds, planExits, 1.0);
		Check(shortKinds.climbing.size() == shortKinds.list.size() && shortKinds.doorBox == std::vector<int32_t>({ -1, -1 }), "hostile: kinds missing, places outside the list: none");
		host::AutoPlot_RepairPlan(nullptr, planExits, 1.0);
		Check(true, "no plan: nothing done");
	}

	printf("LEFT AS THEY ARE: what is not the repair's\n");
	{
		std::vector<WaypointRec> clean;
		std::vector<WaypointPoint> cleanExits;

		Room(&clean, &cleanExits, 3000, -500, 3000);
		std::string untouched = "as it was";
		host::AutoPlotRepairCounts const none = host::AutoPlot_Repair(&clean, cleanExits, 1.0, &untouched);
		Check(none.madeOne + none.trimmed + none.centresMoved + none.boxesTakenOut + none.partsLinked == 0 && untouched == "as it was", "a clean list: nothing done, nothing said");

		std::vector<WaypointRec> trimExit;
		std::vector<WaypointPoint> trimExits;

		Room(&trimExit, &trimExits, 3000, -500, 3000);
		trimExit.push_back(Box(5700, -3000, 2000, 6300, 0, 4000, 6000, -1500, 3000));   // a wall box whose centre is in the floor box next to its exit point
		Link(&trimExit, 0, 2, 4);
		trimExits.push_back(Point(5950, -100, 2900));   // an exit point only the floor box and the exit box hold
		host::AutoPlot_Repair(&trimExit, trimExits, 1.0);
		Check(host::Waypoints_Check(trimExit, trimExits, 1.0).empty() || trimExit[0].max[0] > 5950, "a trim never takes an exit point out of the box holding it");
	}

	printf("HOSTILE\n");
	{
		std::vector<WaypointRec> empty;
		std::vector<WaypointPoint> none;

		host::AutoPlot_Repair(&empty, none, 1.0);
		Check(empty.empty(), "an empty list stays empty");
		host::AutoPlot_Repair(nullptr, none, 1.0);
		Check(true, "no list: nothing done");

		std::vector<WaypointRec> outside;
		std::vector<WaypointPoint> exits;

		Room(&outside, &exits, 3000, -500, 3000);
		outside[0].links.push_back({ 77, 0 });   // a link outside the list
		std::vector<WaypointRec> const before = outside;
		host::AutoPlot_Repair(&outside, exits, 1.0);
		Check(outside.size() == before.size() && outside[0].links.size() == before[0].links.size(), "a link outside the list: left as it is (the planner's)");

		std::vector<WaypointRec> many;
		std::vector<WaypointPoint> manyExits;

		for (int i = 0; i < 120; i++)
		{
			many.push_back(Box(i * 1000, -2000, 0, i * 1000 + 1500, 0, 1000, i * 1000 + 100, -500, 500));   // every centre in the box before's too
		}
		host::AutoPlotRepairCounts const c = host::AutoPlot_Repair(&many, manyExits, 1.0);
		Check(many.size() == 120 && c.left > 0, "over the checker's 99 boxes: left as it is");

		std::vector<WaypointRec> chain;
		std::vector<WaypointPoint> chainExits;

		for (int i = 0; i < 60; i++)
		{
			chain.push_back(Box(i * 100, -2000, 0, i * 100 + 3000, 0, 3000, i * 100 + 50, -500, 1500));   // 60 boxes, every centre in many others
			if (i > 0)
			{
				Link(&chain, i - 1, i, 0);
			}
		}
		chainExits.push_back(Point(100, -100, 1500));
		host::AutoPlot_Repair(&chain, chainExits, 1.0);
		Check(chain.size() <= 60, "60 boxes each over many centres: it ends (" + std::to_string(chain.size()) + " boxes, first problem: " + FirstProblem(chain, chainExits) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
