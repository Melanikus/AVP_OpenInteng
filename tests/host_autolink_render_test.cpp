// The test of AutoLink's pictures on the card (src/host/autolink_render.*), against AvP-docs\engine\inteng\reassess\autolink-pictures-drake.md AP-7: a screen of 320 x 240, centre (160, 120),
// 160 pixels to the unit of depth, y down, the nearer polygon in front, module n drawn in n + 1, the shown[] mask. Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code". It reads no file; it needs a Direct3D 9 device (a hidden window of its own), so it runs only where a display is free.

#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include "autolink_render.h"
#include "autolink_scene.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// a flat square of the module's, facing z, corners at x0..x1, y0..y1, depth z
static void Square(host::AutoLinkScene *scene, int32_t number, int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t z)
{
	std::unique_ptr<host::PortalShape> shape(new host::PortalShape());

	shape->points = { x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z };
	shape->polygons = { { 0, 1, 2, 3 } };

	host::AutoLinkModule one = {};

	one.number = number;
	one.module.shape = shape.get();
	scene->shapes[number] = std::move(shape);
	scene->list.push_back(one);
}

int main()
{
	host::AutoLinkScene scene;

	Square(&scene, 0, -500, 500, -300, 300, 1000);        // module 0: x -500..500 at depth 1000: columns 160 -/+ 80, rows 120 -/+ 48
	Square(&scene, 1, -9000, 9000, -9000, 9000, 4000);    // module 1: a wall behind it, filling the screen

	host::AutoLinkRenderer renderer;
	std::string why;

	// the window at the front before the renderer is made (Bishop: ours must never take the front; checked after Open, Load and the pictures)
	HWND const frontBefore = GetForegroundWindow();

	if (!renderer.Open(&why))
	{
		printf("  FAILED  a Direct3D 9 device: %s\n", why.c_str());
		printf("FAILED host_autolink_render_test (no device)\n");
		return 1;
	}

	size_t leftOut = 9;

	Check(renderer.Load(scene, &leftOut, &why) && leftOut == 0 && renderer.ModuleCount() == 2, "the two squares are loaded, nothing left out");

	host::AutoLinkView view = {};

	view.turn[0] = view.turn[4] = view.turn[8] = 0x10000;   // z, direction +1: the identity (AP-1 local_9c)

	std::vector<uint32_t> pixels;
	std::vector<uint8_t> both(2, 1);

	Check(renderer.Picture(view, both, &pixels, &why) && pixels.size() == 320 * 240, "a picture of 320 x 240 is read back (" + why + ")");

	auto at = [&](int x, int y) { return pixels.size() == 320 * 240 ? pixels[(size_t)y * 320 + (size_t)x] : 0xFFFFFFFFu; };

	Check(at(160, 120) == 1, "the screen's centre (160, 120) shows module 0 (ID 1), the nearer");
	Check(at(10, 10) == 2 && at(310, 230) == 2, "the corners show the wall behind (ID 2)");
	Check(at(238, 120) == 1 && at(242, 120) == 2, "x 500 at depth 1000 lands at column 160 + 160 x 500 / 1000 = 240 (the lens, 160 pixels to the unit)");
	Check(at(160, 166) == 1 && at(160, 170) == 2 && at(160, 74) == 1 && at(160, 70) == 2, "y 300 at depth 1000 lands at row 120 + 48 = 168, below the centre (y is down), -300 at row 72");

	std::vector<uint8_t> wallOnly = { 0, 1 };

	Check(renderer.Picture(view, wallOnly, &pixels, &why) && at(160, 120) == 2, "with module 0 not shown, the wall is seen through where it was");

	std::vector<uint8_t> none;

	Check(renderer.Picture(view, none, &pixels, &why) && at(160, 120) == 0 && at(10, 10) == 0, "nothing shown: every pixel 0");

	host::AutoLinkView behind = view;

	behind.place[2] = 5000;   // past both squares, looking the same way: nothing ahead
	Check(renderer.Picture(behind, both, &pixels, &why) && at(160, 120) == 0, "a camera past both sees nothing (no polygon behind the near plane is drawn)");

	// RECORDS (Dietrich's driver: the modules in file order, then one stand-in per door): the two squares, then a third record, a small square at depth 500 (x -100..100: columns
	// 160 -/+ 32), drawn in 3; shown[] covers it too
	host::AutoLinkScene standIn;

	Square(&standIn, 0, -100, 100, -100, 100, 500);

	std::vector<host::PortalModule> records = { scene.list[0].module, scene.list[1].module, standIn.list[0].module };

	leftOut = 9;
	Check(renderer.Load(records, &leftOut, &why) && leftOut == 0 && renderer.ModuleCount() == 3, "three records are loaded (two modules and a stand-in), nothing left out");

	std::vector<uint8_t> all(3, 1), noStandIn = { 1, 1, 0 };

	Check(renderer.Picture(view, all, &pixels, &why) && at(160, 120) == 3 && at(200, 120) == 1 && at(10, 10) == 2,
	      "record 2 (the stand-in) is drawn in ID 3 in front: the centre 3, column 200 the module 0 (ID 1), the corner the wall (ID 2)");
	Check(renderer.Picture(view, noStandIn, &pixels, &why) && at(160, 120) == 1, "with the stand-in not shown, module 0 is seen where it was");

	// refused: more records than kAutoLinkMostRecords (the IDs are 24-bit; a hostile level could name millions)
	std::vector<host::PortalModule> tooMany(host::kAutoLinkMostRecords + 1, scene.list[0].module);

	why.clear();
	Check(!renderer.Load(tooMany, &leftOut, &why) && why.find("1,000,000") != std::string::npos, "refused: 1,000,001 records are more than kAutoLinkMostRecords (" + why + ")");

	// NEVER THE FRONT: after Open, two Loads and seven pictures the foreground window is the one before (a window a person brings forward in that second would show here too); the renderer's
	// own window is not visible, never activates (WS_EX_NOACTIVATE) and has no taskbar button (WS_EX_TOOLWINDOW)
	HWND const ours = FindWindowW(L"AVP_OpenInteng_AutoLinkPictures", NULL);
	LONG const exStyle = ours ? GetWindowLongW(ours, GWL_EXSTYLE) : 0;

	Check(GetForegroundWindow() == frontBefore, "the foreground window is the same as before Open (no window of ours took the front)");
	Check(ours != NULL && !IsWindowVisible(ours) && GetForegroundWindow() != ours, "the renderer's window is there, not visible, and not the foreground window");
	Check((exStyle & WS_EX_NOACTIVATE) != 0 && (exStyle & WS_EX_TOOLWINDOW) != 0, "the renderer's window never activates (WS_EX_NOACTIVATE) and has no taskbar button (WS_EX_TOOLWINDOW)");

	if (checksFailed)
	{
		printf("FAILED host_autolink_render_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_render_test\n");
	return 0;
}
