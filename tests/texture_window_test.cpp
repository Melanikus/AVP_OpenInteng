// texture_window_test.cpp: the Texture Map's texture window as inteng lays it out, and Texture Resize (src/model/texture_window.h; reassess/tm-b-window-layout.md). Written 2026-11-18
// by Melanikus (Dietrich) for AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "texture_window.h"

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

static std::string Words(model::TextureWindowState const &s)
{
	return "view " + std::to_string(s.viewL) + "," + std::to_string(s.viewT) + "-" + std::to_string(s.viewR) + "," + std::to_string(s.viewB) + " window " + std::to_string(s.winL) + "," +
		std::to_string(s.winT) + "-" + std::to_string(s.winR) + "," + std::to_string(s.winB) + " area on the screen " + std::to_string(s.areaSL) + "-" + std::to_string(s.areaSR);
}

int main()
{
	printf("THE TEXTURE WINDOW\n");

	int32_t const W = 640, H = 480;
	model::TextureWindowState s;

	// the start view: s = 270 at 640, the 256 x 256 area holds it to 256
	model::WindowStartView(&s, W, H);
	Check(s.viewL == 0 && s.viewR == 256 && s.viewB == 256 && s.winL == 384 && s.winT == 224 && s.winR == 640 && s.winB == 480 && s.areaSL == 384 && s.areaSR == 640,
		"the start view: the whole 256 x 256 area, the window in the bottom-right corner 256 pixels each way (" + Words(s) + ")");

	// x2 at the view's middle (no mouse): s = 135 texels about 128
	model::WindowSetScale(&s, 2, false, 0, 0, W, H);
	Check(s.scaleN == 2 && s.scaleF == 2.0 && s.viewL == 61 && s.viewR == 196 && s.viewT == 61 && s.viewB == 196 && s.winL == 370 && s.winT == 210,
		"x2: a view of 135 texels about the middle, the window 270 pixels each way (" + Words(s) + ")");
	Check(s.areaSL == 248 && s.areaSR == 760, "x2: the area on the screen, as if the view's right sat at the screen's (" + Words(s) + ")");

	// the places, toward zero
	Check(model::WindowScreenX(s, 61) == 370 && model::WindowScreenX(s, 62.7) == 373 && model::WindowTexelX(s, 373) == 62 && model::WindowTexelY(s, 210) == 61,
		"a texel's place and the texel under a place, both toward zero (FUN_005567b0 / FUN_00556710)");

	// x1/2: the view would be 540 texels, more than the area: the area itself
	model::TextureWindowState half = s;

	model::WindowSetScale(&half, -2, false, 0, 0, W, H);
	Check(half.scaleN == -2 && half.scaleF == 0.5 && half.viewL == 0 && half.viewR == 256 && half.winL == 512, "x1/2: the view the whole area, the window 128 pixels (" + Words(half) + ")");

	// the scale at the mouse: the texel under it is the new middle
	model::TextureWindowState atMouse = s;

	model::WindowSetScale(&atMouse, 3, true, 470, 310, W, H);
	Check(atMouse.viewL == 111 - 45 && atMouse.viewR == 111 - 45 + 90, "x3 at the mouse: the view about the texel under it, 111 (" + Words(atMouse) + ")");
	Check(model::WindowScaleUp(-2) == -1 && model::WindowScaleUp(1) == 2 && model::WindowScaleDown(1) == -2 && model::WindowScaleDown(2) == 1 && model::WindowScaleDown(-2) == -3,
		"9 / 7: one up and one down (-2 up gives -1, which the scale makes 1; 1 down gives -2)");

	model::TextureWindowState clamped = s;

	model::WindowSetScale(&clamped, -1, false, 0, 0, W, H);
	Check(clamped.scaleN == 1 && clamped.scaleF == 1.0, "a scale of -1 is x1");
	Check(model::WindowScalePowerUp(1) == 2 && model::WindowScalePowerUp(3) == 4 && model::WindowScalePowerUp(4) == 8 && model::WindowScalePowerUp(-2) == -1 && model::WindowScalePowerUp(-4) == -2 &&
		model::WindowScalePowerUp(-8) == -4, "0: powers of two up");
	Check(model::WindowScalePowerDown(1) == -2 && model::WindowScalePowerDown(2) == 1 && model::WindowScalePowerDown(3) == 2 && model::WindowScalePowerDown(8) == 4 &&
		model::WindowScalePowerDown(-2) == -4 && model::WindowScalePowerDown(-3) == -4 && model::WindowScalePowerDown(-4) == -8, "6: powers of two down");

	// TEXTURE RESIZE at x2: the left edge 100 pixels left
	model::TextureWindowState edge = s;

	Check(model::WindowResizePress(&edge, 368, 5) == model::kWindowDrag_LeftEdge && model::WindowResizePress(&edge, 500, 208) == model::kWindowDrag_TopEdge &&
		model::WindowResizePress(&edge, 371, 211) == (model::kWindowDrag_LeftEdge | model::kWindowDrag_TopEdge) && model::WindowResizePress(&edge, 500, 300) == model::kWindowDrag_Scroll &&
		model::WindowResizePress(&edge, 100, 100) == 0, "the press: the left edge (x only), the top edge (y only), both at the corner, scroll inside, nothing outside");

	int32_t ndx = 0, ndy = 0;

	model::WindowResizePress(&edge, 370, 300);
	model::WindowResizeDrag(&edge, -100, 0, W, H, &ndx, &ndy);
	Check(edge.winL == 270 && edge.areaSL == 148 && edge.areaSR == 660 && edge.viewL == 61 && edge.viewR == 246, "the left edge 100 pixels out: the window wider, the view to 246 (" + Words(edge) + ")");
	model::WindowResizeDrag(&edge, -400, 0, W, H, &ndx, &ndy);
	Check(edge.winL >= 0 && edge.viewR <= 256, "dragged far out: the window's left at the screen's, the view held in the area (" + Words(edge) + ")");

	// scroll
	model::TextureWindowState scroll = s;

	model::WindowResizePress(&scroll, 500, 300);
	model::WindowResizeDrag(&scroll, 20, 0, W, H, &ndx, &ndy);
	Check(scroll.areaSL == 268 && scroll.viewR == 186 && scroll.viewL == 51 && ndx == 20, "scroll 20 pixels right: the area on the screen moves, the view 10 texels back (" + Words(scroll) + ")");
	model::WindowResizeDrag(&scroll, 1000, 0, W, H, &ndx, &ndy);
	Check(scroll.areaSL == scroll.winL && scroll.viewL == 0, "scroll far right: held where the area's left meets the window's (" + Words(scroll) + ")");
	model::WindowResizeRelease(&scroll, W, H);
	Check(scroll.drag == 0 && scroll.areaSL % 2 == 0, "the release at x2 puts the area on whole texels");

	// the arrow keys: a quarter of the view
	model::TextureWindowState arrows = s;

	model::WindowResizeArrows(&arrows, false, false, false, true, false, W, H);
	Check(arrows.viewL == 61 + 33 && arrows.viewR == 196 + 33, "right: the view a quarter of its size on (" + Words(arrows) + ")");
	model::WindowResizeArrows(&arrows, false, false, false, true, true, W, H);
	Check(arrows.viewR == 256 && arrows.viewL == 256 - 135, "right with the modifier: a whole view, held in the area (" + Words(arrows) + ")");

	// THE DOTTED LINE (FUN_005564b0)
	std::vector<model::WindowDot> dots;

	model::WindowDots(0, 0, 30, 0, 0, &dots);
	Check(dots.size() == 9 && dots[0].x == 0 && dots[1].x == 3 && dots[8].x == 24 && !dots[0].white && dots[1].white && !dots[2].white,
		"30 pixels across, phase 0: 9 dots (30 / 3 - 1), every 3 pixels from 0, black then white");
	model::WindowDots(0, 0, 30, 0, 4, &dots);
	Check(dots.size() == 9 && dots[0].x == 1 && dots[1].x == 4 && dots[0].white && !dots[1].white, "phase 4: from 1 (4 % 3), white first (4 > 2)");
	model::WindowDots(0, 0, 30, 40, 0, &dots);
	Check(dots.size() == 15 && dots[1].x == 1 && dots[1].y == 2 && dots[14].x == 25 && dots[14].y == 33, "30 by 40 (50 long): 15 dots, each place cut toward zero (3 x 30 / 50 = 1, 3 x 40 / 50 = 2)");
	model::WindowDots(-20, 5, -50, 5, 0, &dots);
	Check(dots.size() == 9 && dots[1].x == -23 && dots[8].x == -44, "leftward: the places go the line's way");
	model::WindowDots(7, 7, 7, 7, 0, &dots);
	Check(dots.empty(), "no length: no dot");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED texture_window_test\n");
		return 1;
	}

	printf("passed texture_window_test\n");
	return 0;
}
