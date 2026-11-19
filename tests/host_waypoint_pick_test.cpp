// host_waypoint_pick_test.cpp: Waypoint mode by mouse's sums (src/host/waypoint_pick.h): the line into a box, the box a click takes, a floor, the new box on a floor.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "waypoint_pick.h"

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

static host::WaypointRec Box(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1)
{
	host::WaypointRec wp;

	wp.min[0] = x0; wp.min[1] = y0; wp.min[2] = z0;
	wp.max[0] = x1; wp.max[1] = y1; wp.max[2] = z1;

	for (int i = 0; i < 3; i++)
	{
		wp.centre[i] = (wp.min[i] + wp.max[i]) / 2;
	}

	wp.flags = 0;
	wp.spare2 = 0;
	return wp;
}

int main()
{
	printf("THE LINE INTO A BOX\n");

	double const lo[3] = { 0, 0, 0 }, hi[3] = { 10, 10, 10 };
	double distance = -1.0;
	double const fromLeft[3] = { -5, 5, 5 }, alongX[3] = { 1, 0, 0 }, longX[3] = { 65536, 0, 0 };

	Check(host::WaypointPick_RayEnters(fromLeft, alongX, lo, hi, &distance) && fabs(distance - 5.0) < 1e-9, "from 5 to its left: in at 5");
	Check(host::WaypointPick_RayEnters(fromLeft, longX, lo, hi, &distance) && fabs(distance - 5.0) < 1e-9, "a direction 65536 long (the game's): the same 5 millimetres");

	double const inside[3] = { 5, 5, 5 }, behind[3] = { 20, 5, 5 }, above[3] = { -5, 20, 5 }, none[3] = { 0, 0, 0 }, slant[3] = { 1, 1, 0 };

	Check(!host::WaypointPick_RayEnters(inside, alongX, lo, hi, &distance), "a line starting inside: no way in");
	Check(!host::WaypointPick_RayEnters(behind, alongX, lo, hi, &distance), "a box behind the line: missed");
	Check(!host::WaypointPick_RayEnters(above, alongX, lo, hi, &distance), "a line passing above (parallel to a side, outside it): missed");
	Check(!host::WaypointPick_RayEnters(fromLeft, none, lo, hi, &distance), "a direction of no length: nothing");
	Check(host::WaypointPick_RayEnters(fromLeft, slant, lo, hi, &distance) && fabs(distance - 5.0 * sqrt(2.0)) < 1e-9, "a slanted line: in at 5 x root 2 along it");

	printf("THE BOX A CLICK TAKES\n");

	std::vector<host::WaypointRec> list = { Box(30, 0, 0, 40, 10, 10), Box(10, 0, 0, 20, 10, 10) };
	double const camera[3] = { 0, 5, 5 };

	Check(host::WaypointPick_BoxOnRay(list, 1.0, false, -1, camera, alongX, -1.0) == 1, "two boxes on the line: the nearer one (1), whatever the list's order");
	Check(host::WaypointPick_BoxOnRay(list, 1.0, false, -1, camera, alongX, 15.0) == 1, "a wall at 15: box 1 (in at 10) is in front of it");
	Check(host::WaypointPick_BoxOnRay(list, 1.0, false, -1, camera, alongX, 11.5) == 1, "a wall at 11.5: box 1 still (in at 10)");
	Check(host::WaypointPick_BoxOnRay(list, 1.0, false, -1, camera, alongX, 7.0) == -1, "a wall at 7: both boxes behind it, none taken");
	Check(host::WaypointPick_BoxOnRay(list, 1.0, false, -1, camera, alongX, 9.0) == 1, "a wall at 9: box 1 starts 1 mm behind it (on the wall): taken");

	double const insideOne[3] = { 15, 5, 5 };

	Check(host::WaypointPick_BoxOnRay(list, 1.0, false, -1, insideOne, alongX, -1.0) == 0, "the camera inside box 1: the line takes box 0 beyond it");
	Check(host::WaypointPick_BoxOnRay({}, 1.0, false, -1, camera, alongX, -1.0) == -1, "no boxes: none");

	// scale 2: the file's numbers doubled in the world
	std::vector<host::WaypointRec> scaled = { Box(5, 0, 0, 10, 5, 5) };
	double const camera2[3] = { 0, 2, 2 };

	Check(host::WaypointPick_BoxOnRay(scaled, 2.0, false, -1, camera2, alongX, -1.0) == 0, "scale 2: a file box 5..10 is the world's 10..20 on the line");

	double const atY8[3] = { 0, 8, 2 };

	Check(host::WaypointPick_BoxOnRay(scaled, 2.0, false, -1, atY8, alongX, -1.0) == 0 && host::WaypointPick_BoxOnRay(scaled, 1.0, false, -1, atY8, alongX, -1.0) == -1,
		"y 8 is inside the world box at scale 2 (0..10), outside it at scale 1 (0..5)");

	// minimised: every box but the current is the 200 cube round its aim point
	std::vector<host::WaypointRec> big = { Box(1000, -5000, -5000, 9000, 5000, 5000) };
	double const offCentre[3] = { 0, 3000, 0 }, atCentre[3] = { 0, 0, 0 };

	Check(host::WaypointPick_BoxOnRay(big, 1.0, false, -1, offCentre, alongX, -1.0) == 0, "a big box met 3 m from its aim point");
	Check(host::WaypointPick_BoxOnRay(big, 1.0, true, -1, offCentre, alongX, -1.0) == -1, "minimised: the same line misses its 200 cube");
	Check(host::WaypointPick_BoxOnRay(big, 1.0, true, -1, atCentre, alongX, -1.0) == 0, "minimised: a line through its aim point meets the cube");
	Check(host::WaypointPick_BoxOnRay(big, 1.0, true, 0, offCentre, alongX, -1.0) == 0, "minimised but the CURRENT one: its whole box, as drawn");

	printf("A FLOOR\n");

	int32_t const up[3] = { 0, -65536, 0 }, down[3] = { 0, 65536, 0 }, side[3] = { 65536, 0, 0 }, zero[3] = { 0, 0, 0 };
	double const r = 65536.0;
	int32_t const at44[3] = { (int32_t)(r * sin(44.0 * 3.14159265358979 / 180.0)), (int32_t)(-r * cos(44.0 * 3.14159265358979 / 180.0)), 0 };
	int32_t const at46[3] = { (int32_t)(r * sin(46.0 * 3.14159265358979 / 180.0)), (int32_t)(-r * cos(46.0 * 3.14159265358979 / 180.0)), 0 };

	Check(host::WaypointPick_IsFloor(up), "facing straight up (-y, the world is y down): a floor");
	Check(!host::WaypointPick_IsFloor(down), "facing down: a ceiling, not a floor");
	Check(!host::WaypointPick_IsFloor(side), "facing across: a wall");
	Check(host::WaypointPick_IsFloor(at44) && !host::WaypointPick_IsFloor(at46), "44 degrees from up: a floor; 46: not");
	Check(!host::WaypointPick_IsFloor(zero), "no normal: not a floor");

	printf("THE NEW BOX ON A FLOOR\n");

	int32_t const hit[3] = { 100, 500, -300 };
	host::WaypointRec const made = host::WaypointPick_NewBox(hit, 1.0);

	Check(made.min[0] == -900 && made.max[0] == 1100 && made.min[2] == -1300 && made.max[2] == 700, "1 m each way across from the click");
	Check(made.min[1] == -1500 && made.max[1] == 500, "from the floor (y 500) 2 m up (y down: to -1500)");
	Check(made.centre[0] == 100 && made.centre[1] == -500 && made.centre[2] == -300, "its aim point in the middle");
	Check(made.flags == 0 && made.links.empty() && made.moduleLinks.empty(), "no flags, no links");

	host::WaypointRec const half = host::WaypointPick_NewBox(hit, 2.0);

	Check(half.min[0] == -450 && half.max[0] == 550 && half.min[1] == -750 && half.max[1] == 250, "scale 2: the file's numbers halved");

	int32_t const odd[3] = { -1, 0, 0 };
	host::WaypointRec const down2 = host::WaypointPick_NewBox(odd, 2.0);

	Check(down2.min[0] == -501 && down2.max[0] == 499, "halves taken toward minus (the view's FileUnits): -1001 / 2 is -501");

	printf("A LINK\n");

	Check(fabs(host::WaypointPick_SegmentPixels(0, 0, 100, 0, 50, 3) - 3.0) < 1e-9, "3 pixels above the middle of a line: 3");
	Check(fabs(host::WaypointPick_SegmentPixels(0, 0, 100, 0, 110, 0) - 10.0) < 1e-9, "past its end: from the end, 10");
	Check(fabs(host::WaypointPick_SegmentPixels(5, 5, 5, 5, 8, 9) - 5.0) < 1e-9, "a line of no length: from its point, 5");

	double pixels = -1.0;

	Check(host::WaypointPick_NearLink(0, 0, 100, 0, 50, 3, &pixels) && fabs(pixels - 3.0) < 1e-9, "3 px from the line, 50 from both ends: the link");
	Check(!host::WaypointPick_NearLink(0, 0, 100, 0, 50, 5, &pixels), "5 px from it: not the link");
	Check(!host::WaypointPick_NearLink(0, 0, 100, 0, 10, 1, &pixels), "10 px from an end (its box's place): not the link");
	Check(host::WaypointPick_NearLink(0, 0, 100, 0, 13, 1, &pixels), "13 px from it: the link");

	host::WaypointRec aim = Box(0, 0, 0, 10, 10, 10);
	int32_t aimWorld[3];

	aim.centre[0] = 7; aim.centre[1] = -3; aim.centre[2] = 5;
	host::WaypointPick_AimWorld(aim, 2.5, aimWorld);
	Check(aimWorld[0] == 17 && aimWorld[1] == -7 && aimWorld[2] == 12, "its end, the aim point times the scale toward zero: 17, -7, 12");

	int32_t flags = 0;
	std::string order;

	for (int k = 0; k < 10; k++)
	{
		order += (k ? " " : "") + std::to_string(flags);
		flags = host::WaypointPick_NextLinkFlags(flags);
	}

	Check(order == "0 1 2 4 5 6 8 9 10 0", "the click cycle: " + order);
	Check(host::WaypointPick_NextLinkFlags(12) == 0 && host::WaypointPick_NextLinkFlags(3) == 0, "out of the cycle (12, 3): back to 0");

	printf("SET LINK FLAGS (ours: Add Link only ORs bits in)\n");

	std::vector<host::WaypointRec> pair = { Box(0, 0, 0, 1, 1, 1), Box(2, 0, 0, 3, 1, 1), Box(4, 0, 0, 5, 1, 1) };
	auto const flagOf = [&pair](int32_t a, int32_t b) { for (host::WaypointLinkRec const &l : pair[(size_t)a].links) { if (l.index == b) return l.flags; } return -1; };

	Check(host::Waypoints_SetLinkFlags(&pair, 0, 1, 5) && flagOf(0, 1) == 5 && flagOf(1, 0) == 6, "0 to 1 set to 5 (made): one way + aliens; 1 to 0 its mirror 6");
	Check(host::Waypoints_SetLinkFlags(&pair, 0, 1, 8) && flagOf(0, 1) == 8 && flagOf(1, 0) == 8 && pair[0].links.size() == 1 && pair[1].links.size() == 1,
		"set again to 8: EXACTLY 8 both ways (the old bits gone), still one link each");
	Check(host::Waypoints_SetLinkFlags(&pair, 1, 0, 2) && flagOf(1, 0) == 2 && flagOf(0, 1) == 1, "from 1's side, 2 (one way back): 0 to 1 is then 1");
	Check(host::Waypoints_SetLinkFlags(&pair, 0, 1, 7) && flagOf(0, 1) == 4 && flagOf(1, 0) == 4, "1 and 2 together clear both, as Add Link: 7 is 4");
	Check(flagOf(0, 2) == -1 && flagOf(2, 0) == -1, "waypoint 2 untouched");
	Check(!host::Waypoints_SetLinkFlags(&pair, 0, 0, 1) && !host::Waypoints_SetLinkFlags(&pair, 0, 3, 1) && !host::Waypoints_SetLinkFlags(&pair, -1, 1, 1) && !host::Waypoints_SetLinkFlags(nullptr, 0, 1, 1),
		"the same waypoint, one past the list, -1 or no list: refused");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_waypoint_pick_test\n");
		return 1;
	}

	printf("passed host_waypoint_pick_test\n");
	return 0;
}
