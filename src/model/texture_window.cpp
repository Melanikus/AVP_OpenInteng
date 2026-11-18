// texture_window.cpp: the Texture Map's texture window as inteng lays it out, and Texture Resize (see texture_window.h). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng
// (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_window.h"

#include <math.h>

namespace model
{
	int32_t WindowFtol(double value)
	{
		if (value >= 2147483647.0)
		{
			return 2147483647;
		}

		if (value <= -2147483648.0)
		{
			return -2147483647 - 1;
		}

		return (int32_t)value;   // C's conversion cuts toward zero, as the chop mode does
	}

	namespace
	{
		int32_t StartSize(int32_t screenW)
		{
			return (int32_t)((int64_t)screenW * 270 / 640);   // 0x55d2c7: the multiply-by-0x66666667 division, toward zero
		}

		void HoldViewInArea(int32_t *lo, int32_t *hi, int32_t areaLo, int32_t areaHi)
		{
			if (*hi - *lo > areaHi - areaLo)
			{
				*lo = areaLo;
				*hi = areaHi;
			}
			else if (*lo < areaLo)
			{
				*hi += areaLo - *lo;
				*lo = areaLo;
			}
			else if (*hi > areaHi)
			{
				*lo -= *hi - areaHi;
				*hi = areaHi;
			}
		}
	}

	void WindowLayout(TextureWindowState *state, int32_t screenW, int32_t screenH)
	{
		double const f = state->scaleF;

		HoldViewInArea(&state->viewL, &state->viewR, state->areaL, state->areaR);
		HoldViewInArea(&state->viewT, &state->viewB, state->areaT, state->areaB);

		int32_t const left = WindowFtol(screenW - (state->viewR - state->viewL) * f), top = WindowFtol(screenH - (state->viewB - state->viewT) * f);

		state->winL = left < 10 ? 10 : left;
		state->winR = screenW;
		state->winT = top < 10 ? 10 : top;
		state->winB = screenH;
		state->areaSL = WindowFtol(screenW - (state->viewR - state->areaL) * f);
		state->areaSR = WindowFtol(screenW - (state->viewR - state->areaR) * f);
		state->areaST = WindowFtol(screenH - (state->viewB - state->areaT) * f);
		state->areaSB = WindowFtol(screenH - (state->viewB - state->areaB) * f);
	}

	void WindowStartView(TextureWindowState *state, int32_t screenW, int32_t screenH)
	{
		int32_t const s = WindowFtol(StartSize(screenW) / state->scaleF);

		state->viewL = 0;
		state->viewT = 0;
		state->viewR = s < state->areaR ? s : state->areaR;
		state->viewB = s < state->areaB ? s : state->areaB;
		WindowLayout(state, screenW, screenH);
	}

	void WindowSetScale(TextureWindowState *state, int n, bool mouseKnown, int32_t mouseX, int32_t mouseY, int32_t screenW, int32_t screenH)
	{
		int32_t cx, cy;

		if (mouseKnown && mouseX > state->winL && mouseY > state->winT)
		{
			cx = WindowFtol((mouseX - state->winL) / state->scaleF) + state->viewL;
			cy = WindowFtol((mouseY - state->winT) / state->scaleF) + state->viewT;
		}
		else
		{
			cx = (state->viewL + state->viewR) / 2;
			cy = (state->viewT + state->viewB) / 2;
		}

		n = n == 0 || n == -1 ? 1 : n > 128 ? 128 : n < -128 ? -128 : n;
		state->scaleN = n;
		state->scaleF = n >= 1 ? (double)n : -1.0 / n;

		int32_t const s = WindowFtol(StartSize(screenW) / state->scaleF);

		state->viewL = cx - s / 2;
		state->viewR = state->viewL + s;
		state->viewT = cy - s / 2;
		state->viewB = state->viewT + s;
		WindowLayout(state, screenW, screenH);
	}

	int WindowScaleUp(int n)
	{
		int const next = n + 1;

		return next == 0 || next == 1 ? 2 : next;   // FUN_0055d190 (from -2 the -1 the clamp makes 1)
	}

	int WindowScaleDown(int n)
	{
		int const next = n - 1;

		return next == 0 || next == -1 ? -2 : next;   // FUN_0055d1b0
	}

	int WindowScalePowerUp(int n)
	{
		if (n == 1)
		{
			return 2;
		}

		if (n > 1)
		{
			int p = 1;

			while (p <= n)
			{
				p *= 2;
			}

			return p;
		}

		int p = -1;

		while (p > n)
		{
			p *= 2;
		}

		return p / 2;   // FUN_0055d1d0: -2 -> -1 (the clamp makes 1), -3 / -4 -> -2
	}

	int WindowScalePowerDown(int n)
	{
		if (n == 1)
		{
			return -2;
		}

		if (n > 1)
		{
			int p = 1;

			while (p < n)
			{
				p *= 2;
			}

			return p / 2;
		}

		int p = -1;

		while (p >= n)
		{
			p *= 2;
		}

		return p;   // FUN_0055d230: -2 / -3 -> -4, -4 -> -8
	}

	int32_t WindowScreenX(TextureWindowState const &state, double u) { return WindowFtol((u - state.viewL) * state.scaleF) + state.winL; }
	int32_t WindowScreenY(TextureWindowState const &state, double v) { return WindowFtol((v - state.viewT) * state.scaleF) + state.winT; }
	int32_t WindowTexelX(TextureWindowState const &state, int32_t x) { return WindowFtol((x - state.winL) / state.scaleF) + state.viewL; }
	int32_t WindowTexelY(TextureWindowState const &state, int32_t y) { return WindowFtol((y - state.winT) / state.scaleF) + state.viewT; }

	int WindowResizePress(TextureWindowState *state, int32_t x, int32_t y)
	{
		state->drag = 0;

		if (x >= state->winL - 3 && x <= state->winL + 2)
		{
			state->drag |= kWindowDrag_LeftEdge;
		}

		if (y >= state->winT - 3 && y <= state->winT + 2)
		{
			state->drag |= kWindowDrag_TopEdge;
		}

		if (state->drag == 0 && x > state->winL && y > state->winT)
		{
			state->drag = kWindowDrag_Scroll;
		}

		return state->drag;
	}

	namespace
	{
		// One edge of the window dragged by d (left / top): the window's edge and the area on the screen move together, held, then the view is as wide as the window shows
		void DragEdge(int32_t d, int32_t *win, int32_t winFar, int32_t *areaLo, int32_t *areaHi, int32_t *viewLo, int32_t *viewHi, int32_t areaTexelHi, double f)
		{
			*win += d;
			*areaLo += d;
			*areaHi += d;

			if (*win < 0)
			{
				*areaLo -= *win;
				*areaHi -= *win;
				*win = 0;
			}

			if (*win > winFar - 10)
			{
				int32_t const back = winFar - 10 - *win;

				*win += back;
				*areaLo += back;
				*areaHi += back;
			}

			if (*areaHi < winFar)
			{
				int32_t const on = winFar - *areaHi;

				*areaHi += on;
				*areaLo += on;
			}

			if (*areaLo > *win)
			{
				*win = *areaLo;
			}

			*viewHi = WindowFtol((winFar - *win) / f) + *viewLo;

			if (*viewHi > areaTexelHi)
			{
				*viewLo -= *viewHi - areaTexelHi;
				*viewHi = areaTexelHi;
			}
		}
	}

	void WindowResizeDrag(TextureWindowState *state, int32_t dx, int32_t dy, int32_t screenW, int32_t screenH, int32_t *nodeDx, int32_t *nodeDy)
	{
		*nodeDx = 0;
		*nodeDy = 0;

		if (state->drag & kWindowDrag_LeftEdge)
		{
			DragEdge(dx, &state->winL, state->winR, &state->areaSL, &state->areaSR, &state->viewL, &state->viewR, state->areaR, state->scaleF);
		}

		if (state->drag & kWindowDrag_TopEdge)
		{
			DragEdge(dy, &state->winT, state->winB, &state->areaST, &state->areaSB, &state->viewT, &state->viewB, state->areaB, state->scaleF);
		}

		if (state->drag & kWindowDrag_Scroll)
		{
			if (dx > 0 && dx > state->winL - state->areaSL)
			{
				dx = state->winL - state->areaSL;
			}
			else if (dx < 0 && dx < state->winR - state->areaSR)
			{
				dx = state->winR - state->areaSR;
			}

			if (dy > 0 && dy > state->winT - state->areaST)
			{
				dy = state->winT - state->areaST;
			}
			else if (dy < 0 && dy < state->winB - state->areaSB)
			{
				dy = state->winB - state->areaSB;
			}

			int32_t const width = state->viewR - state->viewL, height = state->viewB - state->viewT;

			state->areaSL += dx;
			state->areaSR += dx;
			state->areaST += dy;
			state->areaSB += dy;
			state->viewR = WindowFtol((screenW - state->areaSL) / state->scaleF + state->areaL);
			state->viewB = WindowFtol((screenH - state->areaST) / state->scaleF + state->areaT);
			state->viewL = state->viewR - width;
			state->viewT = state->viewB - height;
			*nodeDx = dx;
			*nodeDy = dy;
		}
	}

	void WindowResizeRelease(TextureWindowState *state, int32_t screenW, int32_t screenH)
	{
		if ((state->drag & kWindowDrag_Scroll) && state->scaleF > 1.0)
		{
			int32_t const k = WindowFtol(state->scaleF);
			int32_t const rx = state->areaSL % k, ry = state->areaST % k;
			int32_t const width = state->viewR - state->viewL, height = state->viewB - state->viewT;

			state->areaSL -= rx;
			state->areaSR -= rx;
			state->areaST -= ry;
			state->areaSB -= ry;
			state->viewR = WindowFtol((screenW - state->areaSL) / state->scaleF + state->areaL);
			state->viewB = WindowFtol((screenH - state->areaST) / state->scaleF + state->areaT);
			state->viewL = state->viewR - width;
			state->viewT = state->viewB - height;
		}

		if ((state->drag & (kWindowDrag_LeftEdge | kWindowDrag_TopEdge)) && state->scaleF != 1.0)
		{
			WindowLayout(state, screenW, screenH);
		}

		state->drag = 0;
	}

	void WindowResizeArrows(TextureWindowState *state, bool up, bool down, bool left, bool right, bool whole, int32_t screenW, int32_t screenH)
	{
		int32_t dx = 0, dy = 0;

		if (up)
		{
			dy = state->viewT - state->viewB;
		}

		if (down)
		{
			dy += state->viewB - state->viewT;
		}

		if (left)
		{
			dx = state->viewL - state->viewR;
		}

		if (right)
		{
			dx += state->viewR - state->viewL;
		}

		if (!whole)
		{
			dx /= 4;   // toward zero
			dy /= 4;
		}

		if (dx == 0 && dy == 0)
		{
			return;
		}

		state->viewL += dx;
		state->viewR += dx;
		state->viewT += dy;
		state->viewB += dy;
		WindowLayout(state, screenW, screenH);
	}

	void WindowDots(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int phase, std::vector<WindowDot> *out)
	{
		out->clear();

		int64_t const dx = (int64_t)x1 - x0, dy = (int64_t)y1 - y0;
		double const length = (double)(float)sqrt((double)(dx * dx + dy * dy));   // the x87's sqrt stored as a float (0x5564f6), then __ftol
		int64_t const whole = length >= 2147483647.0 ? 2147483647 : (int64_t)length;
		int64_t const l = whole < 1 ? 1 : whole;
		int const p = ((phase % 6) + 6) % 6;
		int64_t const offset = p % 3;
		int const startWhite = p > 2 ? 1 : 0;
		int64_t const count = l / 3 - 1;

		for (int64_t k = 0; k < count && k < 100000; k++)
		{
			int64_t const t = offset + 3 * k;
			WindowDot dot;

			dot.x = (int32_t)(t * dx / l + x0);
			dot.y = (int32_t)(t * dy / l + y0);
			dot.white = ((k + startWhite) & 1) != 0;
			out->push_back(dot);
		}
	}
}
