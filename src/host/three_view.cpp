// See three_view.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P10). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "three_view.h"

namespace host
{
	namespace
	{
		int32_t const kHalfWidth = 0x140;    // 320
		int32_t const kHalfHeight = 0xf0;    // 240

		bool Usable(const OrthoView &view)
		{
			return view.scaleX != 0 && view.scaleY != 0;
		}

		// The handler's sums, done in 64 bits so no number can wrap, and held to an int when they are given back
		int32_t Held(int64_t value)
		{
			return value > INT32_MAX ? INT32_MAX : (value < INT32_MIN ? INT32_MIN : (int32_t)value);
		}
	}

	void ThreeView_Fit(const int32_t mins[3], const int32_t maxs[3], ThreeViews *views)
	{
		int64_t extent[3];
		int64_t middle[3];

		for (int axis = 0; axis < 3; axis++)
		{
			extent[axis] = (int64_t)maxs[axis] - mins[axis];
			middle[axis] = ((int64_t)maxs[axis] + mins[axis]) / 2;
		}

		// the least whole scale that fits a world extent across and another down into 300 by 220 pixels
		struct Fit
		{
			static int32_t Scale(int64_t across, int64_t down)
			{
				int64_t const byAcross = (across + 299) / 300;
				int64_t const byDown = (down + 219) / 220;
				int64_t const most = byAcross > byDown ? byAcross : byDown;

				return most < 1 ? 1 : (most > INT32_MAX ? INT32_MAX : (int32_t)most);
			}
		};

		int32_t const topScale = Fit::Scale(extent[0], extent[1]);
		int32_t const sideScale = Fit::Scale(extent[2], extent[1]);
		int32_t const frontScale = Fit::Scale(extent[0], extent[2]);

		views->top = OrthoView{ 160, 120, topScale, topScale, Held(middle[0]), Held(middle[1]), Held(middle[2]) };
		views->side = OrthoView{ 480, 120, sideScale, sideScale, Held(middle[0]), Held(middle[1]), Held(middle[2]) };
		views->front = OrthoView{ 160, 360, frontScale, frontScale, Held(middle[0]), Held(middle[1]), Held(middle[2]) };
	}

	void ThreeView_FitTrack(const int32_t maxAbs[3], ThreeViews *views)
	{
		int32_t mins[3];
		int32_t maxs[3];

		for (int axis = 0; axis < 3; axis++)
		{
			maxs[axis] = maxAbs[axis] < 0 ? 0 : maxAbs[axis];
			mins[axis] = -maxs[axis];
		}

		ThreeView_Fit(mins, maxs, views);   // an extent either side of 0: its middle is 0
	}

	bool ThreeView_Click(const ThreeViews &views, int32_t x, int32_t y, int32_t cursor[3])
	{
		if (x < kHalfWidth && y < kHalfHeight)
		{
			// top left: x and y, from the top view (FUN_004f3e30:4f3e9b-4f3eb4)
			if (!Usable(views.top))
			{
				return false;
			}

			cursor[0] = Held(((int64_t)x - views.top.originX) * views.top.scaleX + views.top.worldX);
			cursor[1] = Held(((int64_t)y - views.top.originY) * views.top.scaleY + views.top.worldY);
			return true;
		}

		if (kHalfWidth < x && y < kHalfHeight)
		{
			// top right: z and y, from the side view (:4f3edf-4f3eb4)
			if (!Usable(views.side))
			{
				return false;
			}

			cursor[2] = Held(((int64_t)views.side.originX - x) * views.side.scaleX + views.side.worldZ);
			cursor[1] = Held(((int64_t)y - views.side.originY) * views.side.scaleY + views.side.worldY);
			return true;
		}

		if (x < kHalfWidth && kHalfHeight < y)
		{
			// bottom left: x and z, from the front view (:4f3f0d-4f3f28)
			if (!Usable(views.front))
			{
				return false;
			}

			cursor[0] = Held(((int64_t)x - views.front.originX) * views.front.scaleX + views.front.worldX);
			cursor[2] = Held(((int64_t)views.front.originY - y) * views.front.scaleY + views.front.worldZ);
			return true;
		}

		return false;
	}

	bool ThreeView_CrosshairTop(const ThreeViews &views, const int32_t cursor[3], int32_t *x, int32_t *y)
	{
		if (!Usable(views.top))
		{
			return false;
		}

		// (:4f3f62-4f3f8b)
		int64_t const across = ((int64_t)cursor[0] - views.top.worldX) / views.top.scaleX + views.top.originX;
		int64_t const down = ((int64_t)cursor[1] - views.top.worldY) / views.top.scaleY + views.top.originY;

		if (!(0 < across && across < kHalfWidth && 0 < down && down < kHalfHeight))
		{
			return false;
		}

		*x = (int32_t)across;
		*y = (int32_t)down;
		return true;
	}

	bool ThreeView_CrosshairSide(const ThreeViews &views, const int32_t cursor[3], int32_t *x, int32_t *y)
	{
		if (!Usable(views.side))
		{
			return false;
		}

		// (:4f3fb7-4f3fe6)
		int64_t const across = ((int64_t)views.side.worldZ - cursor[2]) / views.side.scaleX + views.side.originX;
		int64_t const down = ((int64_t)cursor[1] - views.side.worldY) / views.side.scaleY + views.side.originY;

		if (!(kHalfWidth < across && across < 2 * kHalfWidth && 0 < down && down < kHalfHeight))
		{
			return false;
		}

		*x = (int32_t)across;
		*y = (int32_t)down;
		return true;
	}

	bool ThreeView_CrosshairFront(const ThreeViews &views, const int32_t cursor[3], int32_t *x, int32_t *y)
	{
		if (!Usable(views.front))
		{
			return false;
		}

		// (:4f4011-4f403b): inside only when 1 <= across <= 0x13f and 0xf1 <= down <= 0x1df
		int64_t const across = ((int64_t)cursor[0] - views.front.worldX) / views.front.scaleX + views.front.originX;
		int64_t const down = ((int64_t)views.front.worldZ - cursor[2]) / views.front.scaleY + views.front.originY;

		if (across < 1 || across > 0x13f || down < 0xf1 || down > 0x1df)
		{
			return false;
		}

		*x = (int32_t)across;
		*y = (int32_t)down;
		return true;
	}
}
