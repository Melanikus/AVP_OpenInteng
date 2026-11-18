// texture_colour.cpp: see texture_colour.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_colour.h"

namespace model
{
	int ColourCellAt(int32_t x, int32_t y, int32_t screenW, int32_t screenH)
	{
		int32_t const cellW = screenW / 16, cellH = screenH / 16;

		if (cellW < 1 || cellH < 1 || x < 0 || y < 0 || x >= cellW * 16 || y >= cellH * 16)
		{
			return -1;
		}

		int const cell = (int)(x / cellW + (y / cellH) * 16);

		return cell > 255 ? 255 : cell;
	}

	void ColourCellBox(int cell, int32_t screenW, int32_t screenH, int32_t *x, int32_t *y, int32_t *width, int32_t *height)
	{
		int32_t const cellW = screenW / 16, cellH = screenH / 16;

		*x = (cell % 16) * cellW;
		*y = (cell / 16) * cellH;
		*width = cellW;
		*height = cellH;
	}

	uint32_t ColourWord(int cell, uint8_t r, uint8_t g, uint8_t b)
	{
		uint32_t const word = (((uint32_t)cell << 6 | r) << 8 | g) << 8 | b;

		return word * 4u;
	}

	uint32_t ColourShown(uint8_t r, uint8_t g, uint8_t b)
	{
		uint32_t const rr = (uint32_t)r * 4 > 255 ? 255 : (uint32_t)r * 4, gg = (uint32_t)g * 4 > 255 ? 255 : (uint32_t)g * 4, bb = (uint32_t)b * 4 > 255 ? 255 : (uint32_t)b * 4;

		return 0xFF000000u | rr << 16 | gg << 8 | bb;
	}
}
