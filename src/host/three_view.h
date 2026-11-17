// three_view.h: the click and crosshair sums of inteng's THREE-VIEW layout for track and point editing (P10; Melanikus's ruling 2026-11-15 through Bishop:
// "rebuild inteng's 3-view layout ... copy its quadrant layout, cursor and crosshair drawing, and the click-to-cursor rule exactly from the read").
// Plain numbers, no window, no game: a test can drive it. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// MATCHES inteng's FUN_004f3e30 (AvP-docs\engine\inteng, out\menu-reads-2.txt:9158-9290), its mouse handler for the track and point screens. The screen
// is 640 by 480 and has four quarters of 320 by 240:
//   top left     the TOP view   (x across, y down):   shows the level's X (across) and Y
//   top right    the SIDE view  (z across, y down):   shows Z (across, flipped) and Y
//   bottom left  the FRONT view (x across, z down):   shows X (across) and Z (flipped)
//   bottom right the 3D view
// Each of the three views has, in inteng's view record: its screen origin (+0x2c, +0x30), its scale in world units per pixel (+0x34, +0x38) and its
// world offsets (+0xd0, +0xd4, +0xd8) for the x, y and z of the shared 3D CURSOR. A left click in a quarter sets the cursor's numbers that view shows
// (and leaves the one it doesn't); the three views each draw a crosshair at the cursor. What is NOT read yet, and so not here: how each view is built
// (the origins, the scales, the offsets and what the view draws: FUN_00553ab0's own drawing, the setup of DAT_008f4edc / DAT_008f4ee0 / DAT_008f4ee4);
// they are the caller's numbers. The sums here, including the integer divisions and the places the quarters' edges fall, are the handler's own.

#pragma once

#include <stdint.h>

namespace host
{
	int32_t const kThreeViewScreenWidth = 640;
	int32_t const kThreeViewScreenHeight = 480;

	// One of the three views, as inteng's view record holds it
	struct OrthoView
	{
		int32_t originX;    // +0x2c: where the view's world offset lies on the screen, across
		int32_t originY;    // +0x30: down
		int32_t scaleX;     // +0x34: world units a pixel stands for, across
		int32_t scaleY;     // +0x38: down
		int32_t worldX;     // +0xd0: the world x at the origin
		int32_t worldY;     // +0xd4: the world y at the origin
		int32_t worldZ;     // +0xd8: the world z at the origin
	};

	struct ThreeViews
	{
		OrthoView top;      // DAT_008f4edc
		OrthoView side;     // DAT_008f4ee0
		OrthoView front;    // DAT_008f4ee4
	};

	// Fits the three views to a level whose extent in the world is mins to maxs. WHAT RUN 18 READ (p11-shape-handlers.md 27 and 29; FUN_004f3770, menu-reads-18.txt:2-193):
	// inteng's three views are each a quarter of the screen with their scale = R / 100 world units a pixel, where R is the track's cursor object's size
	// number (+0x100); their offsets are the track's centre object's place, which that setup puts at (0, 0, 0) (the object's own space), with one axis
	// backed off by R + 100000 (the camera's depth, not the picture); and that screen HIDES every object of the level but the two track objects, so inteng's
	// views show the track, not the level. NOT READ and so ours here: R (it is made from the level's extent: the scale is the least whole one that shows the
	// extent in 300 by 220 pixels, which is R / 100 for the R that does), the offsets (the middle of the extent, so the level can be seen: the level's wire
	// drawing is our stand-in until the track's own drawing is read, FUN_004f0da0), and the origin (the middle of each quarter).
	void ThreeView_Fit(const int32_t mins[3], const int32_t maxs[3], ThreeViews *views);

	// Fits the three views to a TRACK in its own space (the centre at 0, 0, 0, as inteng's setup puts it: run 18): the offsets are 0, and the scale the least whole
	// one that shows maxAbs (the largest distance from the centre on each axis) either side of the centre in 300 by 220 pixels. inteng's R (the cursor object's
	// size number; its scale is R / 100) is not read: this scale is ours.
	void ThreeView_FitTrack(const int32_t maxAbs[3], ThreeViews *views);

	// The three views about a centre in the world at a scale of whole world units a pixel (Waypoint's Edit Extents, FUN_005846b0: every view's offsets are the centre, each view's origin the
	// middle of its quarter, as ThreeView_Fit puts them). Vasquez 2026-11-17.
	void ThreeView_Centre(const int64_t centre[3], int32_t scale, ThreeViews *views);

	// A click at (x, y) of the 640 by 480 screen: the cursor's numbers the clicked quarter shows are set (top left: x and y; top right: z and y; bottom
	// left: x and z), the others left as they were. A click on the middle lines, or in the bottom right (the 3D view), sets none. Gives back true when
	// the click was in one of the three views. A scale of 0 is never used (inteng would divide by it): the click is refused, false, nothing changed.
	bool ThreeView_Click(const ThreeViews &views, int32_t x, int32_t y, int32_t cursor[3]);

	// Where the cursor's crosshair goes in each view, in screen pixels, or false when it is outside that view's quarter (the handler's own limits: top
	// 0 < x < 320 and 0 < y < 240; side 320 < x < 640 and 0 < y < 240; front 1 <= x <= 319 and 241 <= y <= 479). A scale of 0 gives false.
	bool ThreeView_CrosshairTop(const ThreeViews &views, const int32_t cursor[3], int32_t *x, int32_t *y);
	bool ThreeView_CrosshairSide(const ThreeViews &views, const int32_t cursor[3], int32_t *x, int32_t *y);
	bool ThreeView_CrosshairFront(const ThreeViews &views, const int32_t cursor[3], int32_t *x, int32_t *y);
}
