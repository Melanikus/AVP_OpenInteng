// See autolink_show.h. Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_show.h"

#include <stdio.h>

namespace host
{
	namespace
	{
		int32_t Clamp(int32_t value, int32_t least, int32_t most)
		{
			return value < least ? least : (value > most ? most : value);
		}
	}

	AutoLinkGrid AutoLinkShow_Grid(int32_t viewWidth, int32_t viewHeight)
	{
		int32_t const width = viewWidth < 1 ? 1 : viewWidth;
		int32_t const height = viewHeight < 1 ? 1 : viewHeight;
		AutoLinkGrid grid;

		grid.columns = Clamp(width / 640, 2, kAutoLinkMostColumns);
		grid.rows = Clamp(height / 480, 2, kAutoLinkMostRows);
		return grid;
	}

	bool AutoLinkShow_TilePlace(int32_t viewWidth, int32_t viewHeight, int64_t picture, int32_t *slot, int32_t *x, int32_t *y)
	{
		AutoLinkGrid const grid = AutoLinkShow_Grid(viewWidth, viewHeight);
		int64_t const tiles = (int64_t)grid.columns * grid.rows;
		int64_t const n = picture < 0 ? 0 : picture;
		int32_t const at = (int32_t)(n % tiles);
		int32_t const fromRight = at % grid.columns;
		int32_t const fromBottom = at / grid.columns;
		int64_t const left = (int64_t)viewWidth - (int64_t)(fromRight + 1) * kAutoLinkTileWidth;
		int64_t const top = (int64_t)viewHeight - (int64_t)(fromBottom + 1) * kAutoLinkTileHeight;

		if (slot)
		{
			*slot = at;
		}

		if (x)
		{
			*x = (int32_t)(left < INT32_MIN ? INT32_MIN : left);
		}

		if (y)
		{
			*y = (int32_t)(top < INT32_MIN ? INT32_MIN : top);
		}

		return left >= 0 && top >= 0;
	}

	uint32_t AutoLinkShow_IdColour(uint32_t id)
	{
		if (id == 0)
		{
			return 0xFF000000u;
		}

		// twelve bright colours, the next ID always a different one; the ID's higher bits shift the twelve a little so ID 1 and ID 13 are told apart too
		static uint32_t const kColours[12] = { 0xFF3070FF, 0xFFFF5040, 0xFF40E040, 0xFFFFD030, 0xFFD050FF, 0xFF30E0E0, 0xFFFF9020, 0xFF90A0FF, 0xFFFF70B0, 0xFFA0FF60, 0xFF20A0A0, 0xFFE0E0E0 };
		uint32_t const base = kColours[(id - 1) % 12];
		uint32_t const shade = ((id - 1) / 12) % 4;   // 0..3: each channel darkened by an eighth that many times
		uint32_t const red = ((base >> 16) & 0xFF) * (8 - shade) / 8;
		uint32_t const green = ((base >> 8) & 0xFF) * (8 - shade) / 8;
		uint32_t const blue = (base & 0xFF) * (8 - shade) / 8;

		return 0xFF000000u | (red << 16) | (green << 8) | blue;
	}

	std::string AutoLinkShow_CounterWords(int32_t number)
	{
		char words[16];

		snprintf(words, sizeof(words), "%03d", number < 0 ? 0 : (int)number);
		return std::string(words);
	}
}
