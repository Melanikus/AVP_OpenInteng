// host_autoplot_lane_test.cpp: THE LANES (src/host/autoplot_lane.h, autoplot_plan.h; Bishop 11-19, the owner's "continuous lanes"): the stock route choice copied from
// bh_waypoint.cpp on made-up lists, and two made-up corridors planned with their doorways, a hanging alien carried through the doorway between them by the stock choice.
// Written 2026-11-19 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "autoplot_lane.h"
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

static host::WaypointRec Box(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1)
{
	host::WaypointRec w;

	w.min[0] = x0; w.min[1] = y0; w.min[2] = z0;
	w.max[0] = x1; w.max[1] = y1; w.max[2] = z1;
	w.centre[0] = (x0 + x1) / 2; w.centre[1] = (y0 + y1) / 2; w.centre[2] = (z0 + z1) / 2;
	w.flags = 0;
	w.spare2 = 0;
	return w;
}

static void Link(std::vector<host::WaypointRec> *list, int32_t a, int32_t b, int32_t flags)
{
	host::WaypointLinkRec there, back;

	there.index = b; there.flags = flags;
	back.index = a; back.flags = flags;
	(*list)[(size_t)a].links.push_back(there);
	(*list)[(size_t)b].links.push_back(back);
}

static host::AutoPlotStep Step(int32_t x, int32_t y, int32_t z, AutoPlotSurface surface, bool inside)
{
	host::AutoPlotStep s;

	s.place[0] = x; s.place[1] = y; s.place[2] = z;
	s.surface = surface;
	s.facing[0] = s.facing[2] = 0;
	s.facing[1] = surface == AutoPlotSurface::Floor ? -65536 : 65536;
	s.inside = inside;
	return s;
}

// ONE CORRIDOR (x from x0 to x0 + 6000, z -1000..1000, the floor at 0, the ceiling 3 m up, y is down; open doorways at both ends, as tall as the corridor): an alien way
// along the ceiling from soffit to soffit, a marine's along the floor, the floor cells, and both doorways (the exit points 100 past each end, half way up)
struct Corridor
{
	std::vector<host::AutoPlotWay> ways;
	std::vector<std::vector<int32_t> > exits;
	host::AutoPlotFloor floor;
	std::vector<host::AutoPlotDoor> doors;
};

static Corridor CorridorAt(int32_t x0, int32_t length = 6000)
{
	Corridor c;
	host::AutoPlotWay up, walk;

	up.alien = true;
	walk.alien = false;

	for (int32_t x = x0 - 100; x <= x0 + length + 100; x += length < 1500 ? 100 : 300)
	{
		up.steps.push_back(Step(x, -3000, 0, AutoPlotSurface::Ceiling, x >= x0 && x < x0 + length));
	}

	for (int32_t x = x0 - 100; x <= x0 + length + 100; x += length < 1500 ? 100 : 500)
	{
		walk.steps.push_back(Step(x, 0, 0, AutoPlotSurface::Floor, x >= x0 && x < x0 + length));
	}

	c.ways = { up, walk };

	for (int32_t x = x0 + 150; x < x0 + length; x += 300)
	{
		for (int32_t z = -850; z <= 850; z += 300)
		{
			c.floor.cells.push_back(std::vector<int32_t>{ x, 0, z });
		}
	}

	c.floor.cellEdge = 300;
	c.exits = { { x0 - 100, -1500, 0 }, { x0 + length + 100, -1500, 0 } };

	for (int end = 0; end < 2; end++)
	{
		host::AutoPlotDoor d;
		int32_t const mx = end == 0 ? x0 : x0 + length;

		d.soffitFound = true;
		d.soffit[0] = end == 0 ? x0 - 100 : x0 + length + 100; d.soffit[1] = -3000; d.soffit[2] = 0;
		d.throughFound = true;
		d.middle[0] = mx; d.middle[1] = -1500; d.middle[2] = 0;
		d.through[0] = end == 0 ? -1.0 : 1.0;
		d.through[1] = 0.0;
		c.doors.push_back(d);
	}

	return c;
}

int main()
{
	printf("host_autoplot_lane_test\n");

	// --- the stock choice, on lists made by hand
	std::vector<host::WaypointRec> two = { Box(0, -3000, -1000, 3000, 1, 1000), Box(3000, -3000, -1000, 6000, 1, 1000) };
	int32_t const onFloor[3] = { 1000, 0, 0 }, farEnd[3] = { 5000, -1000, 0 }, nearHere[3] = { 1500, -1000, 0 };

	Link(&two, 0, 1, host::kLinkAlien);
	Check(host::Stock_Choose(two, 1.0, onFloor, farEnd, true, 0).crawls, "two boxes linked aliens only: the first link is aliens only, it crawls");
	Check(!host::Stock_Choose(two, 1.0, onFloor, nearHere, true, 0).crawls && host::Stock_Choose(two, 1.0, onFloor, nearHere, true, 0).routed,
		"in its target's box (start == last): no first link, it does not crawl (bh_waypoint.cpp:154)");
	Check(!host::Stock_Choose(two, 1.0, onFloor, farEnd, false, 0).routed, "a marine skips 0x4: no route");

	std::vector<host::WaypointRec> plain = two;

	plain[0].links[0].flags = 0;
	plain[1].links[0].flags = 0;
	Check(!host::Stock_Choose(plain, 1.0, onFloor, farEnd, true, 0).crawls && host::Stock_Choose(plain, 1.0, onFloor, farEnd, false, 0).routed,
		"a plain link: routed, but gravity on (no crawl)");

	int32_t const lost[3] = { 20000, 0, 0 };

	Check(host::Stock_Choose(two, 1.0, lost, farEnd, true, 0).holding == 0 && !host::Stock_Choose(two, 1.0, lost, farEnd, true, 0).crawls, "in no box: the dropout, no crawl");

	// the corners: a hanging alien (position 900 under the ceiling) is in a box that holds only the ceiling's band, by its upper corners
	std::vector<host::WaypointRec> band = { Box(0, -3200, -1000, 3000, -2549, 1000), Box(3000, -3200, -1000, 6000, -2549, 1000) };
	int32_t const hanging[3] = { 1000, -3000 + host::kStockAlienTop, 0 };

	Link(&band, 0, 1, host::kLinkAlien);
	Check(host::Stock_Choose(band, 1.0, hanging, std::vector<int32_t>{ 5000, -3000, 0 }.data(), true, 0).holding == 1, "a hanging alien: its upper corners in the ceiling's band");

	// a one-way link is never taken
	std::vector<host::WaypointRec> oneWay = two;

	oneWay[0].links[0].flags = host::kLinkAlien | host::kLinkOneWay;
	oneWay[1].links[0].flags = host::kLinkAlien | host::kLinkOneWay;
	Check(!host::Stock_Choose(oneWay, 1.0, onFloor, farEnd, true, 0).routed, "a one-way link is never taken (bh_waypoint.cpp:767)");

	// the fewest boxes: a floor route of 2 and a ceiling route of 3; the best start is the one with the fewest (the floor's), plain: no crawl
	std::vector<host::WaypointRec> choice = { Box(0, -1500, -1000, 2000, 201, 1000), Box(2000, -1500, -1000, 4000, 201, 1000), Box(0, -3200, -1000, 2000, -2549, 1000),
		Box(2000, -3200, -1000, 4000, -2549, 1000), Box(4000, -3200, -1000, 6000, 201, 1000) };

	Link(&choice, 0, 1, 0);
	Link(&choice, 1, 4, 0);
	Link(&choice, 2, 3, host::kLinkAlien);
	Link(&choice, 3, 4, host::kLinkAlien);
	Link(&choice, 0, 2, host::kLinkAlien);

	int32_t const target[3] = { 5000, -1000, 0 }, lowHang[3] = { 1000, -2400 + host::kStockAlienTop, 0 };

	Check(!host::Stock_Choose(choice, 1.0, lowHang, target, true, 0).crawls,
		"a low ceiling (2.4 m): the hanging alien's low corners in the floor box, whose plain route is as short: it drops");

	// --- two corridors planned with their doorways (THE LANES)
	Corridor const a = CorridorAt(0), b = CorridorAt(6000);
	host::AutoPlotPlan const pa = host::AutoPlot_PlanConsolidated(a.ways, a.exits, a.floor, a.doors, 1.0);
	host::AutoPlotPlan const pb = host::AutoPlot_PlanConsolidated(b.ways, b.exits, b.floor, b.doors, 1.0);

	Check(pa.list.size() <= 99 && pb.list.size() <= 99, "each corridor at most 99 boxes (" + std::to_string(pa.list.size()) + ", " + std::to_string(pb.list.size()) + ")");
	Check(pa.doorWords.size() == 2 && pa.doorWords[0] == "lane" && pa.doorWords[1] == "lane", "corridor A: both doorways are lanes (" +
		(pa.doorWords.size() == 2 ? pa.doorWords[0] + " / " + pa.doorWords[1] : std::string("none")) + ")");
	Check(pb.doorWords.size() == 2 && pb.doorWords[0] == "lane" && pb.doorWords[1] == "lane", "corridor B: both doorways are lanes");
	Check(pa.doorBox.size() == 2 && pa.doorBox[0] >= 0 && pa.doorBox[1] >= 0 && pa.exitBox.size() == 2 && pa.exitBox[1] >= 0, "corridor A: door boxes and exit boxes");
	Check(pa.centresShared == 0 && pb.centresShared == 0 && pa.parts == 1 && pb.parts == 1, "each centre its own box's only, one network each");

	if (pa.exitBox.size() == 2 && pa.exitBox[1] >= 0)
	{
		host::WaypointRec const &t = pa.list[(size_t)pa.exitBox[1]];

		Check(t.min[1] >= -3000 + host::kAutoPlotHangClear, "the exit point's box " + std::to_string(host::kAutoPlotHangClear) + " under the soffit at least (its top " +
			std::to_string(t.min[1]) + ")");
	}

	// A PURE TARGET (Drake's gate 11-20, office01's exit 0, 981 under its doorway's top): the corridor's exit points 950 under its 3 m ceiling, between a hanging alien's
	// low corners (900) and kAutoPlotHangClear (1000): both doorways still lanes, each exit's own box topped between those corners and its point
	{
		Corridor high = CorridorAt(0);

		for (std::vector<int32_t> &e : high.exits) e[1] = -3000 + 950;

		host::AutoPlotPlan const planHigh = host::AutoPlot_PlanConsolidated(high.ways, high.exits, high.floor, high.doors, 1.0);
		bool clear = planHigh.exitBox.size() == 2;
		std::string tops;

		for (size_t e = 0; clear && e < 2; e++)
		{
			int32_t const top = planHigh.exitBox[e] >= 0 ? planHigh.list[(size_t)planHigh.exitBox[e]].min[1] : 0;

			clear = planHigh.exitBox[e] >= 0 && top > -3000 + host::kStockAlienTop && top <= -3000 + 950;
			tops += " " + std::to_string(top);
		}

		Check(planHigh.doorWords.size() == 2 && planHigh.doorWords[0] == "lane" && planHigh.doorWords[1] == "lane", "exit points 950 under the ceiling: both doorways still lanes (" +
			(planHigh.doorWords.size() == 2 ? planHigh.doorWords[0] + " / " + planHigh.doorWords[1] : std::string("none")) + ")");
		Check(clear && planHigh.exitsCovered == 2, "each exit's own box topped under a hanging alien's low corners (-2100) and at its point at most (-2050): tops" + tops);
	}

	// A PURE TARGET IN THE HANGING BAND (Drake's gate 11-20: Object11 exit 1, 874 under its doorway's top; BLOCK-10 exit 3, 841): the corridor's exit points 850 under its
	// 3 m ceiling, between a hanging alien's two corner heights (-3000 and -2100): both doorways still lanes, each exit's own box inside that band, clear of both
	{
		Corridor band = CorridorAt(0);

		for (std::vector<int32_t> &e : band.exits) e[1] = -3000 + 850;

		host::AutoPlotPlan const planBand = host::AutoPlot_PlanConsolidated(band.ways, band.exits, band.floor, band.doors, 1.0);
		bool inside = planBand.exitBox.size() == 2;
		std::string spans;

		for (size_t e = 0; inside && e < 2; e++)
		{
			host::WaypointRec const *const t = planBand.exitBox[e] >= 0 ? &planBand.list[(size_t)planBand.exitBox[e]] : nullptr;

			inside = t && t->min[1] > -3000 && t->max[1] - 1 < -3000 + host::kStockAlienTop && t->min[1] <= -2150 && -2150 <= t->max[1] - 1;
			spans += t ? " " + std::to_string(t->min[1]) + ".." + std::to_string(t->max[1] - 1) : std::string(" none");
		}

		Check(planBand.doorWords.size() == 2 && planBand.doorWords[0] == "lane" && planBand.doorWords[1] == "lane", "exit points 850 under the ceiling: both doorways still lanes (" +
			(planBand.doorWords.size() == 2 ? planBand.doorWords[0] + " / " + planBand.doorWords[1] : std::string("none")) + ")");
		Check(inside && planBand.exitsCovered == 2, "each exit's own box between a hanging alien's corners (-3000, -2100), holding its point (-2150):" + spans);
	}

	// AN EXIT TO OPEN SKY STILL ENDS THE LANES THAT COME TO IT (Drake's gate on 0f3b6bcf: ENT-01's exit 0, RAMP-01's exit 2): the corridor's exit 1 with no ceiling over
	// its point nor past its doorway (no soffit, so no lane and no door box): its exit box linked aliens only to the ceiling's box, so the lane along the ceiling reaches it
	{
		Corridor sky = CorridorAt(0);

		for (int32_t x = 150; x < 6000; x += 300) for (int32_t z = -850; z <= 850; z += 300) sky.floor.ceilings.push_back(std::vector<int32_t>{ x, -3000, z });

		sky.doors[1].soffitFound = false;

		host::AutoPlotPlan const planSky = host::AutoPlot_PlanConsolidated(sky.ways, sky.exits, sky.floor, sky.doors, 1.0);
		bool toCeiling = false;
		int32_t const target = planSky.exitBox.size() == 2 ? planSky.exitBox[1] : -1;

		for (size_t i = 0; target >= 0 && i < planSky.list[(size_t)target].links.size(); i++)
		{
			host::WaypointLinkRec const &l = planSky.list[(size_t)target].links[i];

			toCeiling = toCeiling || ((l.flags & host::kLinkAlien) != 0 && planSky.climbing[(size_t)l.index] && planSky.list[(size_t)l.index].centre[1] < -2900);
		}

		Check(planSky.doorWords.size() == 2 && planSky.doorWords[1].rfind("no lane: no ceiling", 0) == 0, "exit 1 open sky: no lane there (" +
			(planSky.doorWords.size() == 2 ? planSky.doorWords[1] : std::string("none")) + ")");
		Check(toCeiling, "its exit box linked aliens only to the ceiling's box, so the lane along the ceiling ends at its doorway");
	}

	// A DOOR MODULE 0.4 M DEEP (Drake's gate on 164672c2, medoor02): its two exit boxes are linked to each other (the marine walks straight across), and the doorway pass's
	// "a floor box linked to the exit box never climbs" took the earlier exit box's link to its own door box away: no lane could reach exit 0. Each exit's box keeps its door
	{
		Corridor thin = CorridorAt(0, 400);

		for (int end = 0; end < 2; end++)
		{
			thin.doors[(size_t)end].stationFound = true;   // each doorway's climb station up the -z side wall to the ceiling (as tiledB's below)
			thin.doors[(size_t)end].stationTop[0] = end == 0 ? 150 : 250;
			thin.doors[(size_t)end].stationTop[1] = -3000;
			thin.doors[(size_t)end].stationTop[2] = -850;
		}

		host::AutoPlotPlan const planThin = host::AutoPlot_PlanConsolidated(thin.ways, thin.exits, thin.floor, thin.doors, 1.0);
		std::string pairs;
		size_t kept = 0;

		for (size_t e = 0; e < 2 && e < planThin.doorBox.size() && e < planThin.exitBox.size(); e++)
		{
			int32_t const door = planThin.doorBox[e], target = planThin.exitBox[e];
			bool linked = false;

			for (size_t i = 0; door >= 0 && target >= 0 && i < planThin.list[(size_t)door].links.size(); i++)
			{
				linked = linked || planThin.list[(size_t)door].links[i].index == target;
			}

			kept += linked ? 1 : 0;
			pairs += " exit " + std::to_string(e) + ": door " + std::to_string(door) + " -> its box " + std::to_string(target) + (linked ? " linked" : " NOT linked");
		}

		Check(kept == 2, "a 0.4 m door module: each door box linked to its own exit's box (" + pairs + ")");
	}

	// the hanging alien carried from A's ceiling through the doorway at x 6000 into B's, by the stock choice
	host::LaneSide sideA, sideB;

	sideA.list = &pa.list;
	sideA.target[0] = 6100; sideA.target[1] = -1500; sideA.target[2] = 0;
	sideB.list = &pb.list;
	sideB.target[0] = 12100; sideB.target[1] = -1500; sideB.target[2] = 0;

	int32_t const soffit[3] = { 6000, -3000, 0 }, middle[3] = { 6000, -1500, 0 };
	double const through[2] = { 1.0, 0.0 };
	host::LaneCrossing const cross = host::Lane_Cross(sideA, sideB, 1.0, soffit, middle, through, 2000, 2000, 50);

	Check(cross.continuous, "A to B: the hanging alien crawls at every place through the doorway (" + (cross.continuous ? std::string("continuous") : cross.why) + ")");

	if (pb.doorBox.size() == 2 && pb.doorBox[0] >= 0)
	{
		host::LaneCrossing const along = host::Lane_Through(pb.list, 1.0, pb.doorBox[0], sideB.target, pb.climbing);

		Check(along.continuous, "B: from its door box at A's doorway along the ceiling to its exit toward C, aliens only (" + (along.continuous ? std::string("yes") : along.why) +
			")");
	}

	// A WALL BOX MADE (Drake's rule, the chain gate 11-20: Ops01's side doorways had no wall within 3 m, their strips walked, the exit box was the start): corridor B has no climb
	// station at its doorway from A, so a wall box is made beside the opening, its centre in the doorway's wall (x 6000 + 100), and the doorway's floor climbs to it
	size_t climbsToMade = 0;

	for (size_t n = 0; pb.doorFloors.size() == 2 && n < pb.doorFloors[0].size(); n++)
	{
		for (host::WaypointLinkRec const &l : pb.list[(size_t)pb.doorFloors[0][n]].links)
		{
			host::WaypointRec const &w = pb.list[(size_t)l.index];

			climbsToMade += l.flags == host::kLinkAlien && pb.climbing[(size_t)l.index] && w.centre[0] < 6000 && w.centre[0] > 5800 ? 1 : 0;
		}
	}

	Check(pb.wallsMade >= 1 && climbsToMade >= 1, "corridor B, no station at A's doorway: a wall box made beside the opening, its centre in the doorway's wall, and the doorway's floor "
		"climbs to it (" + std::to_string(pb.wallsMade) + " made, " + std::to_string(climbsToMade) + " climbs)");

	// A STRAIGHT CORRIDOR'S WAYS AS THE GRID GIVES THEM (its turning points only: the floor way two points 6.2 m apart): no plain link between floor boxes apart (Genshd1's
	// Ops01: its two end tiles linked, a one-box floor route that beat the lane); the exit point's own box the LAST that holds it (the game's target box); A's lane from its
	// door box at x 0 to its exit at x 6100, aliens only all the way
	Corridor sparse = CorridorAt(0);

	sparse.ways[1].steps = { sparse.ways[1].steps.front(), sparse.ways[1].steps.back() };

	host::AutoPlotPlan const ps = host::AutoPlot_PlanConsolidated(sparse.ways, sparse.exits, sparse.floor, sparse.doors, 1.0);
	size_t farPlain = 0;

	for (size_t i = 0; i < ps.list.size(); i++)
	{
		for (host::WaypointLinkRec const &l : ps.list[i].links)
		{
			host::WaypointRec const &o = ps.list[(size_t)l.index];
			bool const apart = ps.list[i].max[0] + 600 < o.min[0] || o.max[0] + 600 < ps.list[i].min[0];
			// an exit box's link past the doorway's first floor box is hand2's rule (c) (its floor route one box longer than door -> exit): not a floor shortcut
			bool const exitsOwn = std::find(ps.exitBox.begin(), ps.exitBox.end(), (int32_t)i) != ps.exitBox.end() ||
				std::find(ps.exitBox.begin(), ps.exitBox.end(), l.index) != ps.exitBox.end();

			farPlain += (l.flags & host::kLinkAlien) == 0 && apart && !exitsOwn ? 1 : 0;
		}
	}

	Check(farPlain == 0, "a two-point floor way: no plain link between boxes more than 0.6 m apart (" + std::to_string(farPlain / 2) + ")");

	int lastHolder = -1;

	for (size_t i = 0; i < ps.list.size(); i++)
	{
		bool in = true;

		for (int a = 0; a < 3; a++) in = in && ps.list[i].min[a] <= sparse.exits[1][(size_t)a] && sparse.exits[1][(size_t)a] <= ps.list[i].max[a] - 1;

		lastHolder = in ? (int)i : lastHolder;
	}

	Check(ps.exitBox.size() == 2 && lastHolder >= 0 && lastHolder == ps.exitBox[1], "the last box holding exit 1's point is its own (" + std::to_string(lastHolder) + ")");

	if (ps.doorBox.size() == 2 && ps.doorBox[0] >= 0)
	{
		host::LaneCrossing const along = host::Lane_Through(ps.list, 1.0, ps.doorBox[0], sparse.exits[1].data(), ps.climbing);

		Check(along.continuous, "A: from its door box at x 0 along the ceiling to its exit at x 6100, aliens only (" + (along.continuous ? std::string("yes") : along.why) + ")");
	}

	// RED FIRST: v1's shape (Colony v1, the planner before the lanes): the ceiling's box reaching 1.5 m down, the exit point's own box 1.5 m up from it, so it reaches into the
	// ceiling's band: the hanging alien is in its target's box before the doorway (start == last: no first link, gravity on)
	std::vector<host::WaypointRec> v1A = { Box(-600, -3200, -600, 6700, -1500, 600), Box(5500, -3000, -600, 6700, -1300, 600), Box(0, -1500, -1000, 6000, 201, 1000) };
	std::vector<host::WaypointRec> v1B = { Box(5300, -3200, -600, 12700, -1500, 600), Box(11500, -3000, -600, 12700, -1300, 600), Box(6000, -1500, -1000, 12000, 201, 1000) };

	Link(&v1A, 0, 1, host::kLinkAlien);
	Link(&v1A, 0, 2, host::kLinkAlien);
	Link(&v1A, 1, 2, 0);
	Link(&v1B, 0, 1, host::kLinkAlien);
	Link(&v1B, 0, 2, host::kLinkAlien);
	Link(&v1B, 1, 2, 0);
	sideA.list = &v1A;
	sideB.list = &v1B;

	host::LaneCrossing const oldCross = host::Lane_Cross(sideA, sideB, 1.0, soffit, middle, through, 2000, 2000, 50);

	Check(!oldCross.continuous && oldCross.why.find("its target's box") != std::string::npos, "v1's shape: the crossing breaks, the alien in its target's box (" +
		oldCross.why + ")");

	// THE DOORWAY START (Drake's hand2, Bishop 11-19 "fold it into v2"; Frost's probe of hand1 and Drake's lanes up on v2d, Genshd1's Ops01 81 and 78, opscen03 72): an
	// alien standing on B's floor just inside the doorway from A, heading for B's far exit. The stock heading (bh_waypoint.cpp after FindBestRoute): held by one box it
	// heads for the route's second box's centre, by two or more for its start's own centre. v2d: the exit box (its centre 1 m past the doorway, in A) was the route's
	// start, so the alien headed back out. hand2: the floor box at the doorway is the start, linked aliens only to the ceiling's box over it; the exit box links the
	// ceiling only through the door box and the floor only past the first floor box
	Corridor tiledB = CorridorAt(6000);

	// its floor cells from 600 in (the grid's last cell short of the doorway's middle, as at Genshd1's doorways): the tiles' face 450 and more inside
	std::vector<std::vector<int32_t> > fromSixHundred;

	for (std::vector<int32_t> const &cell : tiledB.floor.cells) if (cell[0] >= 6600) fromSixHundred.push_back(cell);

	tiledB.floor.cells = fromSixHundred;

	for (std::vector<int32_t> const &cell : tiledB.floor.cells) tiledB.floor.ceilings.push_back(std::vector<int32_t>{ cell[0], -3000, cell[2] });

	// the climb station at the doorway from A (Drake's AutoPlotGrid_ClimbStation): up the -z side wall (z -1000) to the ceiling, its top the ceiling cell beside the wall
	tiledB.doors[0].stationFound = true;
	tiledB.doors[0].stationTop[0] = 6150; tiledB.doors[0].stationTop[1] = -3000; tiledB.doors[0].stationTop[2] = -850;

	// the exit points 310 over the floor, as Genshd1's (Drake 11-19: a floor alien at a doorway is always in an exit box that reaches the floor)
	tiledB.exits[0][1] = -310;
	tiledB.exits[1][1] = -310;

	host::AutoPlotPlan const pt = host::AutoPlot_PlanConsolidated(tiledB.ways, tiledB.exits, tiledB.floor, tiledB.doors, 1.0);
	int32_t const farExit[3] = { 12100, -310, 0 };

	// Bishop's gate on the chain (11-20, Drake's lanes up: 52 stall/back places; Drake's rules): (1) the exit box a pure target: no standing alien's corner (feet, 900 up) is in it, so
	// it is never a route's start (no alien heads back past the doorway); (2) a floor box linked to the exit box never climbs (ceiling -> wall -> floor -> exit tied the lane);
	// (3) a floor box climbs only to a centre in a wall (one over open floor: the alien stands under it, its speed 0)
	size_t inExitBox = 0, climbingNeighbours = 0, openAir = 0;

	for (int32_t x = 6000 - 900; x <= 6000 + 1500; x += 100)
	{
		for (int32_t z = -600; z <= 600; z += 300)
		{
			bool held = false;

			for (int32_t dx : { -450, 450 }) for (int32_t dz : { -450, 450 }) for (int32_t y : { 0, -900 })
			{
				host::WaypointRec const &own = pt.list[(size_t)pt.exitBox[0]];
				int32_t const p[3] = { x + dx, y, z + dz };
				bool in = true;

				for (int a = 0; a < 3; a++) in = in && own.min[a] <= p[a] && p[a] <= own.max[a] - 1;

				held = held || in;
			}

			inExitBox += held ? 1 : 0;
		}
	}

	for (size_t i = 0; i < pt.list.size(); i++)
	{
		bool const nextToExit = !pt.climbing[i] && std::any_of(pt.list[i].links.begin(), pt.list[i].links.end(), [&](host::WaypointLinkRec const &l)
			{ return std::find(pt.exitBox.begin(), pt.exitBox.end(), l.index) != pt.exitBox.end(); });

		for (host::WaypointLinkRec const &l : pt.list[i].links)
		{
			bool const climbsTo = !pt.climbing[i] && pt.climbing[(size_t)l.index] && std::find(pt.doorBox.begin(), pt.doorBox.end(), l.index) == pt.doorBox.end();

			climbingNeighbours += nextToExit && climbsTo ? 1 : 0;
			openAir += climbsTo && abs(pt.list[(size_t)l.index].centre[2]) < 950 && pt.list[(size_t)l.index].centre[1] < -2000 ? 1 : 0;   // a ceiling's centre over the floor
		}
	}

	Check(pt.exitBox.size() == 2 && pt.exitBox[0] >= 0 && inExitBox == 0, "the exit box at the doorway from A holds no standing alien (" + std::to_string(inExitBox) +
		" places from 0.9 m past to 1.5 m in)");
	Check(climbingNeighbours == 0, "no floor box linked to an exit box climbs (" + std::to_string(climbingNeighbours) + ")");
	Check(openAir == 0, "no floor box climbs to a ceiling's centre over the floor (" + std::to_string(openAir) + ")");
	size_t backOut = 0, startsInExit = 0, walks = 0, places = 0;
	std::string firstBad;

	// Drake's review of 1ac57e1b: (a) the floor box at the doorway starts AT its line (a corner reaches 450: a face further in leaves a band only the exit box holds);
	// (b) the ceiling's box it links has its centre IN the side wall at the ceiling's corner (a centre over the floor: the alien stands under it, its speed 0)
	int nearestFace = 1 << 30, cornerZ = 0;

	for (size_t i = 0; i < pt.list.size(); i++)
	{
		for (host::WaypointLinkRec const &l : pt.list[i].links)
		{
			bool const fromFloor = !pt.climbing[i] && (int32_t)i != pt.exitBox[0] && pt.list[i].min[0] <= 6000 + 900 && pt.list[i].max[0] > 6000;

			if (fromFloor && l.flags == host::kLinkAlien && pt.climbing[(size_t)l.index] && (int32_t)l.index != pt.doorBox[0])
			{
				nearestFace = std::min(nearestFace, pt.list[i].min[0] - 6000);
				cornerZ = pt.list[(size_t)l.index].centre[2];
			}
		}
	}

	Check(nearestFace <= 50, "the floor box at the doorway from A starts at its line (its face " + std::to_string(nearestFace) + " mm inside)");
	Check(cornerZ <= -1000 && cornerZ > -1400, "the ceiling's box it climbs to: its centre in the -z side wall at the ceiling's corner (z " + std::to_string(cornerZ) + ")");

	for (int32_t inside = 200; inside <= 800; inside += 300)
	{
		int32_t const standing[3] = { 6000 + inside, 0, 0 };

		for (uint32_t shift = 0; shift < 16; shift++)
		{
			host::StockChoice const c = host::Stock_Choose(pt.list, 1.0, standing, farExit, true, shift);
			int32_t const towards = c.holding == 1 ? c.second : c.start;
			bool const back = towards >= 0 && pt.list[(size_t)towards].centre[0] < 6000;
			bool const inExit = pt.exitBox.size() == 2 && c.start >= 0 && c.start == pt.exitBox[0];

			places++;
			backOut += back ? 1 : 0;
			startsInExit += inExit ? 1 : 0;
			walks += c.crawls ? 0 : 1;

			if ((back || inExit || !c.crawls) && firstBad.empty())
			{
				firstBad = std::to_string(inside) + " mm in, shift " + std::to_string(shift) + ": held by " + std::to_string(c.holding) + ", start " + std::to_string(c.start) +
					", second " + std::to_string(c.second) + (c.crawls ? ", crawls" : ", walks");
			}
		}
	}

	Check(startsInExit == 0, "standing at B's doorway from A: the route never starts in the exit box (" + std::to_string(startsInExit) + " of " + std::to_string(places) +
		(firstBad.empty() ? std::string(")") : "; " + firstBad + ")"));
	Check(backOut == 0, "and it never heads back past the doorway (" + std::to_string(backOut) + " of " + std::to_string(places) + ")");
	Check(walks == 0, "and its first link is aliens only: it climbs (" + std::to_string(walks) + " walk of " + std::to_string(places) + ")");
	Check(pt.doorBox.size() == 2 && pt.doorBox[0] >= 0 && pt.list[(size_t)pt.doorBox[0]].centre[0] < 6000,
		"B's door box at the doorway from A: its centre past the doorway, in A (x " + (pt.doorBox.size() == 2 && pt.doorBox[0] >= 0 ?
		std::to_string(pt.list[(size_t)pt.doorBox[0]].centre[0]) : std::string("none")) + ")");
	Check(pt.centresShared == 0 && pt.parts == 1, "each centre its own box's only, one network");

	// EVERY LINK SHIFT'S ROUTE (Vasquez 11-20, Object16's tie): from climbing box 0, the ceiling route 0-1-2-3 and a floor route 0-4-5-3 have as many boxes, so some link
	// shifts take the floor's. Lane_Through walks shift 0's route only; Lane_ThroughAll walks them all and names the floor box. Without the climb down to 4 it is continuous
	{
		std::vector<host::WaypointRec> tie = { Box(0, -3200, -500, 1000, -2549, 500), Box(1000, -3200, -500, 2000, -2549, 500), Box(2000, -3200, -500, 3000, -2549, 500),
			Box(3000, -1500, -500, 4000, 201, 500), Box(0, -1500, -500, 1500, 201, 500), Box(1500, -1500, -500, 3000, 201, 500) };
		std::vector<bool> const tieClimbing = { true, true, true, false, false, false };
		int32_t const tieTarget[3] = { 3500, -500, 0 };

		Link(&tie, 0, 1, host::kLinkAlien);
		Link(&tie, 1, 2, host::kLinkAlien);
		Link(&tie, 2, 3, host::kLinkAlien);

		std::vector<host::WaypointRec> noDown = tie;   // the ceiling's only

		Link(&tie, 0, 4, host::kLinkAlien);
		Link(&tie, 4, 5, 0);
		Link(&tie, 5, 3, 0);

		host::LaneCrossing const all = host::Lane_ThroughAll(tie, 1.0, 0, tieTarget, tieClimbing);
		host::LaneCrossing const ceilingOnly = host::Lane_ThroughAll(noDown, 1.0, 0, tieTarget, tieClimbing);

		Check(!all.continuous && all.why.rfind("box 4 on the way is a floor box (the route 0 4, the target's box 3); link shift ", 0) == 0,
			"a tie over the floor: every shift's route walked, the floor box named (" + (all.continuous ? std::string("continuous") : all.why) + ")");
		Check(ceilingOnly.continuous, "the ceiling's route alone: continuous (" + (ceilingOnly.continuous ? std::string("yes") : ceilingOnly.why) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_autoplot_lane_test\n");
		return 1;
	}

	printf("passed host_autoplot_lane_test\n");
	return 0;
}
