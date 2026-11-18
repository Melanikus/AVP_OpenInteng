// texture_window.h: the Texture Map's texture window as inteng lays it out - its view of the picture, its scale, where it sits on the screen, where a node is - and the Texture
// Resize mode's drags (T, 0x103), as pure maths on the window's state (reassess/tm-b-window-layout.md: FUN_00555ad0, FUN_0055d290, FUN_0055d350, FUN_005567b0, FUN_00556710,
// FUN_00553be0, FUN_005546b0, FUN_00554ca0, FUN_00555070). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <vector>

namespace model
{
	// The window's state, the fields of inteng's DAT_00914d38 the layout uses (byte offsets)
	struct TextureWindowState
	{
		int32_t areaL = 0, areaT = 0, areaR = 256, areaB = 256;          // +08..+14: the texture area in texels
		int32_t areaSL = 0, areaST = 0, areaSR = 0, areaSB = 0;          // +18..+24: the area on the screen (the scroll and node-drag clamp)
		int32_t viewL = 0, viewT = 0, viewR = 256, viewB = 256;          // +28..+34: the view, the part of the picture the window shows, texels
		int32_t winL = 0, winT = 0, winR = 0, winB = 0;                  // +38..+44: the window on the screen (right and bottom not in it)
		int scaleN = 1;                                                  // +54: the scale's whole number (x n, or x 1/-n when below 1)
		double scaleF = 1.0;                                             // +50: the factor every place uses
		int drag = 0;                                                    // +74's resize flags: 2 the left edge, 4 the top edge, 8 scroll
		bool resizing = false;                                           // +90: Texture Resize (T)
	};

	int32_t const kWindowDrag_LeftEdge = 2, kWindowDrag_TopEdge = 4, kWindowDrag_Scroll = 8;

	// __ftol: toward zero (0x59008c sets the FPU to chop)
	int32_t WindowFtol(double value);

	// FUN_00555ad0 (the screen wider than 320): the view held inside the area (the area itself when wider; else moved back in, its size kept), then the window in the screen's
	// bottom-right corner: left = max(10, ftol(screenW - view width x f)), top the same with the height, right = screenW, bottom = screenH; and the area on the screen as if the view's
	// right / bottom sat at the screen's (+18 = ftol(screenW - (viewR - areaL) x f), +20 = ftol(screenW - (viewR - areaR) x f), the same down).
	void WindowLayout(TextureWindowState *state, int32_t screenW, int32_t screenH);

	// The view a texture change or Reset Source Rect starts (FUN_0055d290 / FUN_00555dd0): s = trunc(screenW x 270 / 640), the view (0, 0, min(ftol(s / f), areaR), min(ftol(s / f),
	// areaB)): square, both from the width. Then the layout.
	void WindowStartView(TextureWindowState *state, int32_t screenW, int32_t screenH);

	// FUN_0055d350(n, use the mouse): the centre, with the old f: the texel under the mouse when it is right of and below the window's top left (mouseX > winL, mouseY > winT, no
	// upper bound: 0x55d36c), else the view's middle; n held (0 and -1 are 1; -128..128), f = n or -1 / n; the view s = ftol(screenW x 270 / 640 / f) texels each way about the
	// centre; then the layout.
	void WindowSetScale(TextureWindowState *state, int n, bool mouseKnown, int32_t mouseX, int32_t mouseY, int32_t screenW, int32_t screenH);

	// The keys 9 / 7 / 0 / 6 (0xF1 / 0xF3 / 0xF2 / 0xF4: FUN_0055d190 / 0055d1b0 / 0055d1d0 / 0055d230): the next scale up / down by one, or by powers of two
	int WindowScaleUp(int n);
	int WindowScaleDown(int n);
	int WindowScalePowerUp(int n);
	int WindowScalePowerDown(int n);

	// FUN_005567b0 / FUN_00556710: a texel's place on the screen and the texel under a screen place (both toward zero)
	int32_t WindowScreenX(TextureWindowState const &state, double u);
	int32_t WindowScreenY(TextureWindowState const &state, double v);
	int32_t WindowTexelX(TextureWindowState const &state, int32_t x);
	int32_t WindowTexelY(TextureWindowState const &state, int32_t y);

	// TEXTURE RESIZE. The press (FUN_00553be0 0x553bf3): x within winL - 3 .. winL + 2 grabs the left edge, y within winT - 3 .. winT + 2 the top edge (each on its own axis: the whole
	// line grabs, bug candidate G-5, kept); neither and inside the window: scroll. Gives the flags (0: nothing).
	int WindowResizePress(TextureWindowState *state, int32_t x, int32_t y);

	// The drag (FUN_005546b0) by (dx, dy) from the last mouse place. The left edge: winL, areaSL and areaSR move by dx, held at 0..winR - 10, the area's right not left of the window's,
	// winL not left of areaSL; then viewR = ftol((winR - winL) / f) + viewL, held at areaR (viewL moving back). The top the same. Scroll: dx held to winL - areaSL (right) and winR -
	// areaSR (left), the area on the screen moves, viewR = ftol((screenW - areaSL) / f + areaL), viewL = viewR less the old width (the same down). Gives how far the nodes move on the
	// screen (scroll only; the edges make the places again).
	void WindowResizeDrag(TextureWindowState *state, int32_t dx, int32_t dy, int32_t screenW, int32_t screenH, int32_t *nodeDx, int32_t *nodeDy);

	// The release (FUN_00554ca0): scroll with f above 1 puts the area on whole texels (areaSL and areaSR back by areaSL % ftol(f), the same down) and makes the view again; an edge with
	// f other than 1 lays the window out again. The flags are let go.
	void WindowResizeRelease(TextureWindowState *state, int32_t screenW, int32_t screenH);

	// The arrow keys in Texture Resize (FUN_00555070 0x55508d): the view a quarter of its size each way the keys say (the whole size with the modifier), then the layout. up / down /
	// left / right: the keys held.
	void WindowResizeArrows(TextureWindowState *state, bool up, bool down, bool left, bool right, bool whole, int32_t screenW, int32_t screenH);

	// THE WINDOW'S DOTTED LINE (FUN_005564b0: the outline from node to node, and the multiple node mode's links): L = ftol(the line's length), at least 1; dot k (k = 0 .. L / 3 - 2) at
	// (x0 + (o + 3k) dx / L, y0 + (o + 3k) dy / L), each division toward zero, o = phase % 3; one pixel, white when (k + (phase > 2)) is odd, else black. The phase (DAT_00914d14) goes
	// round 0..5, one step a frame (0x552062 / 0x552c8c), so the line crawls. The caller draws only the dots inside the window (0x55658c).
	struct WindowDot
	{
		int32_t x, y;
		bool white;
	};

	void WindowDots(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int phase, std::vector<WindowDot> *out);
}
