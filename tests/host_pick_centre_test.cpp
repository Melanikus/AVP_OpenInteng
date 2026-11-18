// The test of the "nearest centre on the screen" pick (pick_centre.h; inteng's FUN_0046ddb0 and the loops that use it: Module Linking's Select With Centres FUN_004c3760, Object Placement's click
// FUN_004d6520, Light Sourcing's light and object clicks FUN_004ab2c0 / FUN_004ab200, light-closeout-frost.md TB-L3 / TB-L4): the projection, the candidates behind the camera, the smallest
// distance, the first on a tie, 0.0 taken or not, and where the best starts. Plain sums: no level. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <math.h>

#include <string>

#include "pick_centre.h"

static int checksFailed = 0;
static int checks = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static host::PickCentre At(int32_t x, int32_t y, int32_t z)
{
	host::PickCentre c;

	c.place[0] = x;
	c.place[1] = y;
	c.place[2] = z;
	return c;
}

int main()
{
	// a camera at the origin looking along +z, no turn, a square projection of 1, a 640 x 480 view: (x, y, z) goes to (320 + 320 x / z, 240 - 240 (-y) / z)
	host::PickCamera camera = {};

	camera.turn[0][0] = camera.turn[1][1] = camera.turn[2][2] = 65536;
	camera.xScale = camera.yScale = 1.0f;
	camera.viewWidth = 640;
	camera.viewHeight = 480;

	double x = 0, y = 0;

	Check(host::PickCentre_Project(camera, At(0, 0, 1000), &x, &y) && fabs(x - 320.0) < 1e-9 && fabs(y - 240.0) < 1e-9, "a centre straight ahead is the view's middle");
	Check(host::PickCentre_Project(camera, At(1000, 0, 1000), &x, &y) && fabs(x - 640.0) < 1e-9, "one as far across as ahead is the view's edge");
	Check(!host::PickCentre_Project(camera, At(0, 0, -1000), &x, &y) && host::PickCentre_Distance(camera, 320, 240, At(0, 0, -1000)) == -1.0, "one behind the camera is not on the screen: -1.0");
	Check(fabs(host::PickCentre_Distance(camera, 330, 240, At(0, 0, 1000)) - 10.0) < 1e-9, "the distance is in pixels: 10 to the side");

	host::PickCentre const two[2] = { At(100, 0, 1000), At(0, 0, 1000) };   // 32 pixels right of the middle, and the middle

	Check(host::PickCentre_Nearest(camera, 330, 240, two, 2, true) == 1, "the nearer on the screen wins");
	Check(host::PickCentre_Nearest(camera, 320, 240, two, 2, false) == 0, "Select With Centres never takes 0.0: the other one");
	Check(host::PickCentre_Nearest(camera, 320, 240, two, 2, true) == 1, "Object Placement's and Light Sourcing's take 0.0");

	host::PickCentre const tie[2] = { At(-100, 0, 1000), At(100, 0, 1000) };

	Check(host::PickCentre_Nearest(camera, 320, 240, tie, 2, true) == 0, "on a tie the first");

	host::PickCentre const behind[2] = { At(0, 0, -5000), At(5000, 0, 1000) };   // one behind, one 1600 pixels off to the right

	Check(host::PickCentre_Nearest(camera, 320, 240, behind, 2, true, host::kPickCentreLightFarthest) == 1, "a centre far off the screen is still taken: the best starts at 1,000,000 pixels");
	Check(host::PickCentre_Nearest(camera, 320, 240, behind, 2, true, 1000.0) == -1, "and none past where the best starts");
	Check(host::PickCentre_Nearest(camera, 320, 240, behind, 1, true) == -1, "only one behind the camera: none");
	Check(host::PickCentre_Nearest(camera, 320, 240, nullptr, 2, true) == -1, "no candidates: none");

	camera.viewWidth = 0;
	Check(host::PickCentre_Nearest(camera, 0, 0, two, 2, true) == -1, "an unusable view: none");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED host_pick_centre_test\n");
		return 1;
	}

	printf("passed host_pick_centre_test\n");
	return 0;
}
