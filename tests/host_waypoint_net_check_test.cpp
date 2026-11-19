// host_waypoint_net_check_test.cpp: the alien waypoint CHECKER (src/host/waypoint_net_check.h) on networks made here, one rule at a time: a good network gives no problem, and each mistake
// gives its own problem and no other; the edges of a box are the game's own (min <= p <= max - 1, after the loader's scale). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_waypoint_net_check_test.args builds it; it takes no file. A non-zero result means a check failed.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "waypoint_net_check.h"

using namespace host;

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

// A box from (x0 y0 z0) to (x1 y1 z1), its centre in the middle
static WaypointRec BoxOf(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1)
{
	WaypointRec box;

	box.min[0] = x0; box.min[1] = y0; box.min[2] = z0;
	box.max[0] = x1; box.max[1] = y1; box.max[2] = z1;
	box.centre[0] = (x0 + x1) / 2; box.centre[1] = (y0 + y1) / 2; box.centre[2] = (z0 + z1) / 2;
	box.flags = 0;
	box.spare2 = 0;
	return box;
}

static void Link(std::vector<WaypointRec> &list, int32_t a, int32_t b, int32_t flags = 0)
{
	WaypointLinkRec link;

	link.index = b;
	link.flags = flags;
	list[(size_t)a].links.push_back(link);
}

// Three boxes along x, each 2 m, linked both ways in a chain
static std::vector<WaypointRec> Chain()
{
	std::vector<WaypointRec> list;

	list.push_back(BoxOf(0, 0, 0, 2000, 2000, 2000));
	list.push_back(BoxOf(2000, 0, 0, 4000, 2000, 2000));
	list.push_back(BoxOf(4000, 0, 0, 6000, 2000, 2000));
	Link(list, 0, 1);
	Link(list, 1, 0);
	Link(list, 1, 2);
	Link(list, 2, 1);
	return list;
}

static WaypointPoint Point(int32_t x, int32_t y, int32_t z)
{
	WaypointPoint p = { { x, y, z } };
	return p;
}

static size_t CountOf(std::vector<WaypointProblem> const &problems, WaypointProblemKind kind)
{
	size_t n = 0;

	for (WaypointProblem const &p : problems)
	{
		n += p.kind == kind ? 1 : 0;
	}

	return n;
}

// The problems in words, for a check's line
static std::string Words(std::vector<WaypointProblem> const &problems)
{
	std::string out;

	for (WaypointProblem const &p : problems)
	{
		out += (out.empty() ? "" : "; ") + p.words;
	}

	return out.empty() ? "none" : out;
}

static bool Only(std::vector<WaypointProblem> const &problems, WaypointProblemKind kind, size_t count)
{
	return problems.size() == count && CountOf(problems, kind) == count;
}

int main()
{
	std::vector<WaypointPoint> const none;

	puts("a good network");
	{
		std::vector<WaypointRec> const list = Chain();
		std::vector<WaypointProblem> const p = Waypoints_Check(list, { Point(5000, 1000, 1000) }, 1.0);

		Check(p.empty(), "three boxes linked both ways, centres inside, an exit inside: no problem (" + Words(p) + ")");
	}

	puts("too many boxes");
	{
		std::vector<WaypointRec> list;

		for (int i = 0; i < 100; i++)
		{
			list.push_back(BoxOf(i * 1000, 0, 0, i * 1000 + 1000, 1000, 1000));

			if (i > 0)
			{
				Link(list, i, i - 1);
				Link(list, i - 1, i);
			}
		}

		std::vector<WaypointProblem> const p = Waypoints_Check(list, none, 1.0);

		Check(Only(p, WaypointProblemKind::TooMany, 1), "100 boxes: too many, and nothing else (" + Words(p) + ")");
		list.pop_back();
		list.back().links.pop_back();   // box 98's link to the box taken away
		Check(Waypoints_Check(list, none, 1.0).empty(), "99 boxes: no problem");
	}

	puts("no room");
	{
		std::vector<WaypointRec> list = Chain();

		list[1].min[2] = 2500;   // past its highest (2000) on z
		list[1].centre[2] = 2200;

		std::vector<WaypointProblem> p = Waypoints_Check(list, none, 1.0);

		Check(CountOf(p, WaypointProblemKind::NoRoom) == 1 && p[0].box == 1 && p[0].axis == 2, "a lowest corner past the highest: no room on z (" + Words(p) + ")");

		// the stock room: three quarters of (lowest - centre) less three quarters of (highest - centre - 1): a box 1 mm high at its centre has a room of 0
		list = Chain();
		list[2].min[1] = 1000;
		list[2].max[1] = 1001;
		list[2].centre[1] = 1000;
		p = Waypoints_Check(list, none, 1.0);
		Check(CountOf(p, WaypointProblemKind::NoRoom) == 1 && p[0].box == 2 && p[0].axis == 1, "a box 1 mm high: room 0 on y (" + Words(p) + ")");

		// 8 mm high at its middle: (3 * 3) >> 2 = 2, less (-4 * 3) >> 2 = -3: room 5
		list[2].min[1] = 996;
		list[2].max[1] = 1004;
		list[2].centre[1] = 1000;
		Check(CountOf(Waypoints_Check(list, none, 1.0), WaypointProblemKind::NoRoom) == 0, "8 mm high: room 5, no room problem");
	}

	puts("a large-target link into a box with no room");
	{
		std::vector<WaypointRec> list = Chain();

		list[1].min[1] = 1000;
		list[1].max[1] = 1001;
		list[1].centre[1] = 1000;
		list[1].links[0].flags = kWaypointLinkLargeTarget;   // in box 1's list: the step INTO box 1 aims at a random place in it

		std::vector<WaypointProblem> const p = Waypoints_Check(list, none, 1.0);

		Check(CountOf(p, WaypointProblemKind::LargeTargetSmallBox) == 1 && CountOf(p, WaypointProblemKind::NoRoom) == 1, "0x8 in a box with room 0: the crash named, with its no room (" + Words(p) + ")");

		list = Chain();
		list[0].links[0].flags = kWaypointLinkLargeTarget;
		Check(Waypoints_Check(list, none, 1.0).empty(), "0x8 in a box with room: no problem");
	}

	puts("a link outside the list");
	{
		std::vector<WaypointRec> list = Chain();

		Link(list, 2, 3);
		Link(list, 0, -1);

		std::vector<WaypointProblem> const p = Waypoints_Check(list, none, 1.0);

		Check(Only(p, WaypointProblemKind::LinkOutside, 2) && p[0].box == 0 && p[0].other == -1 && p[1].box == 2 && p[1].other == 3,
			"links to -1 and 3 in a list of 3: both named, nothing else (" + Words(p) + ")");
	}

	puts("every box reaches every other");
	{
		std::vector<WaypointRec> list = Chain();

		// box 2 lists box 1 only one way (0x1): nothing steps from 1 into 2, so box 0 cannot reach box 2 (box 2 still reaches box 0: box 1 lists it)
		list[2].links[0].flags = kWaypointLinkOneWay;

		std::vector<WaypointProblem> p = Waypoints_Check(list, none, 1.0);

		Check(Only(p, WaypointProblemKind::CannotReach, 1) && p[0].box == 0 && p[0].other == 2, "a one-way link: box 0 cannot reach box 2, and that only (" + Words(p) + ")");

		// aliens take the aliens-only links (0x4)
		list = Chain();
		list[1].links[1].flags = 4;
		list[2].links[0].flags = 4;
		Check(Waypoints_Check(list, none, 1.0).empty(), "aliens-only links (0x4): aliens take them, no problem");

		// a box with no link at all
		list = Chain();
		list.push_back(BoxOf(8000, 0, 0, 9000, 1000, 1000));
		p = Waypoints_Check(list, none, 1.0);
		Check(Only(p, WaypointProblemKind::CannotReach, 2), "a box with no link: it cannot be reached, nor reach (" + Words(p) + ")");
	}

	puts("every exit in a box (the game's edges: min <= p <= max - 1)");
	{
		std::vector<WaypointRec> const list = Chain();

		Check(Waypoints_Check(list, { Point(5999, 1999, 1999), Point(0, 0, 0) }, 1.0).empty(), "an exit at max - 1, and one at min: inside");

		std::vector<WaypointProblem> const p = Waypoints_Check(list, { Point(6000, 1000, 1000), Point(3000, 2000, 1000), Point(-1, 5, 5) }, 1.0);

		Check(Only(p, WaypointProblemKind::ExitOutside, 3) && p[0].other == 0 && p[1].other == 1 && p[2].other == 2, "exits at max on x, at max on y, below min: all three outside (" + Words(p) + ")");
	}

	puts("centres");
	{
		std::vector<WaypointRec> list = Chain();

		list[0].centre[0] = 2500;   // in box 1, not its own

		std::vector<WaypointProblem> p = Waypoints_Check(list, none, 1.0);

		Check(p.size() == 2 && CountOf(p, WaypointProblemKind::CentreOutside) == 1 && CountOf(p, WaypointProblemKind::CentreInAnother) == 1,
			"a centre in the next box: outside its own, and in another (" + Words(p) + ")");

		// two boxes overlapping (box 1 from 1500, over box 0's last half metre): box 1's centre in the overlap is in both; box 0's (1000) is not in box 1
		list = Chain();
		list[1].min[0] = 1500;
		list[1].centre[0] = 1800;
		p = Waypoints_Check(list, none, 1.0);
		Check(Only(p, WaypointProblemKind::CentreInAnother, 1) && p[0].box == 1 && p[0].other == 0, "box 1 over box 0, its centre in the overlap: in another (" + Words(p) + ")");
	}

	puts("the scale");
	{
		std::vector<WaypointRec> const list = Chain();

		// at scale 2 the exit's place is scaled too: (3000, ...) * 2 = 6000, inside box 1 (4000 .. 7999)
		Check(Waypoints_Check(list, { Point(3000, 1000, 1000) }, 2.0).empty(), "scale 2: the boxes and the exit scaled alike");
		Check(Only(Waypoints_Check(list, none, nan("")), WaypointProblemKind::BadScale, 1), "a scale that is not a number: bad numbers, and nothing else");
		Check(Only(Waypoints_Check(list, none, 0.0), WaypointProblemKind::BadScale, 1), "a scale of 0: bad numbers");
	}

	puts("the most kept");
	{
		std::vector<WaypointRec> list;

		for (int i = 0; i < 20; i++)
		{
			list.push_back(BoxOf(0, 0, 0, 1000, 1000, 1000));   // twenty boxes in one place: each centre in 19 others
		}

		size_t more = 0;
		std::vector<WaypointProblem> const p = Waypoints_Check(list, none, 1.0, 10, &more);

		Check(p.size() == 10 && more > 0, "at most 10 kept, the rest counted (" + std::to_string(more) + " more)");
		Check(WaypointProblemName(WaypointProblemKind::CentreInAnother) != nullptr && WaypointProblemName((WaypointProblemKind)99) != nullptr, "every kind has words");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
