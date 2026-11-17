// The test for Hierarchy's vertex selection click (host::VertexPick_*, src/host/vertex_pick.cpp; hierarchy-closeout-frost.md TB-2, inteng's FUN_00485290). Written 2026-11-17 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// CHECKS: the click's line crosses a square face in front of the camera and misses beside it; a face behind the camera is not crossed; the nearest point on the screen is taken, strictly (the
// first of two at one place), with every point within 1 of it on each axis (its duplicates) and none 2 away; a point behind the camera, or not beyond the near value, is never taken; nothing
// in front: nothing taken. RED FIRST: ours took the ONE nearest point (no duplicates) wherever the click was.

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "pick_ray.h"
#include "vertex_pick.h"

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		failed++;
	}
}

int main()
{
	using namespace host;

	// a camera at the origin looking along +z, a 640 x 480 view (the same as pick_test's straight camera)
	PickCamera camera;

	camera.place[0] = camera.place[1] = camera.place[2] = 0;

	for (int r = 0; r < 3; r++)
	{
		for (int c = 0; c < 3; c++)
		{
			camera.turn[r][c] = r == c ? 65536 : 0;
		}
	}

	camera.yScale = 1.0f;
	camera.xScale = 1.0f / (640.0f / 480.0f);
	camera.viewWidth = 640;
	camera.viewHeight = 480;

	PickRay middle, aside;

	Check(PickRay_FromClick(camera, 320, 240, &middle) == PickRayMade::Made && PickRay_FromClick(camera, 20, 20, &aside) == PickRayMade::Made, "the clicks make lines");

	// a square 2 m wide, 5 m ahead, round the middle of the view; and the same square behind the camera
	std::vector<int32_t> const square = { -1000, -1000, 5000, 1000, -1000, 5000, 1000, 1000, 5000, -1000, 1000, 5000, -1000, -1000, -5000, 1000, -1000, -5000, 1000, 1000, -5000, -1000, 1000, -5000 };
	std::vector<int32_t> const front = { 0, 1, 2, 3 };
	std::vector<int32_t> const back = { 4, 5, 6, 7 };

	Check(VertexPick_RayHitsFaces(middle, square, front), "a click in the middle is on the square ahead");
	Check(!VertexPick_RayHitsFaces(aside, square, front), "a click in the corner of the view is not");
	Check(!VertexPick_RayHitsFaces(middle, square, back), "a square behind the camera is never under a click");

	// the points: 0 and 1 ahead, 1 nearer the middle; 2 the same place as 1 (a duplicate), 3 one unit off it on each axis (a duplicate too), 4 two units off on x (not); 5 behind the camera
	std::vector<int32_t> const points = { 600, 0, 5000, 100, 50, 5000, 100, 50, 5000, 101, 49, 5001, 102, 50, 5000, 0, 0, -5000 };
	std::vector<int32_t> const taken = VertexPick_Points(camera, 320, 240, points, 3.2);

	Check(taken.size() == 3 && taken[0] == 1 && taken[1] == 2 && taken[2] == 3,
		"the click takes the nearest point (1, the first of two at one place) and its duplicates within 1 (2, 3), not 4 two units off: " + std::to_string(taken.size()) + " taken");

	std::vector<int32_t> const onlyBehind = { 0, 0, -5000, 10, 10, 2 };

	Check(VertexPick_Points(camera, 320, 240, onlyBehind, 3.2).empty(), "a point behind the camera or not beyond the near value is never taken");
	Check(VertexPick_Points(camera, 320, 240, std::vector<int32_t>(), 3.2).empty(), "no points: nothing taken");

	printf("%s\n", failed ? "SOME CHECKS FAILED" : "All checks passed");
	return failed ? 1 : 0;
}
