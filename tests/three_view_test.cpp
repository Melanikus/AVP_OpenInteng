// The test for the three-view sums (three_view.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P10). AVP_OpenInteng's own code: see
// License.txt. No window, none of the game.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "three_view.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	puts("The three views' click and crosshair sums:");

	// Each view: origin (160,120) on its quarter's middle, 10 world units a pixel, world offsets 1000, 2000, 3000
	ThreeViews views;
	views.top = OrthoView{ 160, 120, 10, 10, 1000, 2000, 3000 };
	views.side = OrthoView{ 480, 120, 10, 10, 1000, 2000, 3000 };
	views.front = OrthoView{ 160, 360, 10, 10, 1000, 2000, 3000 };

	int32_t cursor[3] = { -1, -2, -3 };

	Check(ThreeView_Click(views, 170, 130, cursor) && cursor[0] == 1100 && cursor[1] == 2100 && cursor[2] == -3, "top left sets x and y, and leaves z");
	cursor[0] = cursor[1] = cursor[2] = -1;
	Check(ThreeView_Click(views, 470, 110, cursor) && cursor[2] == 3100 && cursor[1] == 1900 && cursor[0] == -1, "top right sets z (flipped: right of the origin is less z) and y");
	cursor[0] = cursor[1] = cursor[2] = -1;
	Check(ThreeView_Click(views, 150, 350, cursor) && cursor[0] == 900 && cursor[2] == 3100 && cursor[1] == -1, "bottom left sets x and z (z flipped: higher on the screen is more z)");

	cursor[0] = 5; cursor[1] = 6; cursor[2] = 7;
	Check(!ThreeView_Click(views, 500, 400, cursor) && cursor[0] == 5 && cursor[1] == 6 && cursor[2] == 7, "the 3D view (bottom right) sets nothing");
	Check(!ThreeView_Click(views, 320, 100, cursor) && !ThreeView_Click(views, 100, 240, cursor) && !ThreeView_Click(views, 320, 240, cursor),
		"the middle lines (x = 320, y = 240) belong to no view, as the handler's tests leave them");

	ThreeViews flat = views;
	flat.top.scaleY = 0;
	Check(!ThreeView_Click(flat, 10, 10, cursor) && cursor[0] == 5, "a scale of 0 refuses the click, nothing changed");

	Check(ThreeView_Click(views, 0, 0, cursor) && cursor[0] == 1000 - 1600 && cursor[1] == 2000 - 1200, "the corner (0, 0) is in the top view");

	// crosshairs: the cursor of the first click, back on the screen
	int32_t at[3] = { 1100, 2100, 3100 };
	int32_t x = 0, y = 0;
	Check(ThreeView_CrosshairTop(views, at, &x, &y) && x == 170 && y == 130, "the top view's crosshair is where the click was");
	Check(ThreeView_CrosshairSide(views, at, &x, &y) && x == 470 && y == 130, "the side view's, with z flipped");
	Check(ThreeView_CrosshairFront(views, at, &x, &y) && x == 170 && y == 350, "the front view's, with z flipped");

	int32_t far[3] = { 100000, 2100, 3100 };
	Check(!ThreeView_CrosshairTop(views, far, &x, &y) && !ThreeView_CrosshairFront(views, far, &x, &y), "outside its quarter, a view draws no crosshair");
	int32_t edge[3] = { 1000 - 1600, 2000 - 1200, 3000 };
	Check(!ThreeView_CrosshairTop(views, edge, &x, &y), "the top view's limits are exclusive: x = 0 is outside");
	int32_t negative[3] = { 1005, 2000, 3000 };
	Check(ThreeView_CrosshairTop(views, negative, &x, &y) && x == 160, "a division rounds towards 0, as the handler's does");

	// the fit (ours): a level 30000 across, 22000 down, 9000 deep
	int32_t const mins[3] = { -15000, -11000, -4500 };
	int32_t const maxs[3] = { 15000, 11000, 4500 };
	ThreeViews fitted;
	ThreeView_Fit(mins, maxs, &fitted);
	Check(fitted.top.scaleX == 100 && fitted.top.scaleY == 100 && fitted.top.originX == 160 && fitted.top.originY == 120 && fitted.top.worldX == 0 && fitted.top.worldY == 0,
		"the fit puts the extent in 300 by 220 pixels in the middle of the top view");
	Check(fitted.side.originX == 480 && fitted.front.originY == 360 && fitted.side.scaleX == 100 && fitted.front.scaleX == 100, "and the other two in theirs");
	int32_t const tiny[3] = { 5, 5, 5 };
	ThreeView_Fit(tiny, tiny, &fitted);
	Check(fitted.top.scaleX == 1, "a level with no extent still has a scale of 1, never 0");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
