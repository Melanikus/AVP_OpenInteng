// texture_area.cpp: the Texture Map's texture area (see texture_area.h). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_area.h"

namespace model
{
	namespace
	{
		int64_t const kMostAreaSide = 1000000000;

		int32_t Held(int64_t side)
		{
			return side > kMostAreaSide ? (int32_t)kMostAreaSide : (int32_t)side;
		}

		void SizeIt(TextureWindow *area)
		{
			area->sizeX = area->right - area->left;
			area->sizeY = area->bottom - area->top;
		}
	}

	TextureWindow AreaDefault(int32_t width, int32_t height, int addOne)
	{
		TextureWindow area = { 0, 0, Held((int64_t)width + addOne), Held((int64_t)height + addOne), 0, 0 };

		SizeIt(&area);

		return area;
	}

	TextureWindow AreaForTexture(TextureWindow const &before, bool hadTexture, bool sameTexture, int32_t oldW, int32_t oldH, int32_t newW, int32_t newH, AreaKeep keep, int addOne)
	{
		if (newW < 1 || newH < 1)
		{
			return before;
		}

		if (!hadTexture || (!sameTexture && keep == AreaKeep::DefaultTo1X1))
		{
			return AreaDefault(newW, newH, addOne);
		}

		TextureWindow area = before;

		if (!sameTexture && keep == AreaKeep::MaintainNumRepeats)
		{
			if (area.left != -kInfiniteArea && oldW > 0)
			{
				area.left = 0;
				area.right = Held((int64_t)newW * (before.right / oldW) + addOne);
			}

			if (area.top != -kInfiniteArea && oldH > 0)
			{
				area.top = 0;
				area.bottom = Held((int64_t)newH * (before.bottom / oldH) + addOne);
			}
		}

		SizeIt(&area);

		return area;
	}

	TextureWindow AreaReset(int32_t width, int32_t height, int addOne)
	{
		return AreaDefault(width, height, addOne);
	}

	TextureWindow AreaFromRepeats(int32_t width, int32_t height, int repeatsX, int repeatsY, int addOne)
	{
		TextureWindow area = {};

		area.left = repeatsX == kRepeatsInfinite ? -kInfiniteArea : 0;
		area.top = repeatsY == kRepeatsInfinite ? -kInfiniteArea : 0;
		area.right = repeatsX == kRepeatsInfinite ? kInfiniteArea : Held((int64_t)width * repeatsX + addOne);
		area.bottom = repeatsY == kRepeatsInfinite ? kInfiniteArea : Held((int64_t)height * repeatsY + addOne);
		SizeIt(&area);

		return area;
	}

	void AreaAddOne(TextureWindow *area, bool on)
	{
		area->right += on ? 1 : -1;
		area->bottom += on ? 1 : -1;
	}
}
