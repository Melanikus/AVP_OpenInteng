// texture_colour.h: the Texture Map's Select Colour (Shift+K, 0x40, inteng FUN_00560620) as pure maths: the 16 x 16 grid of inteng's 256 colours over the whole screen, which cell a press
// is in, the colour word a pick gives a plain polygon, and the colour a cell is drawn in. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B). AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// Inteng's colour table (0x5d77c8, 256 x 3 bytes, each 0..63: read from the user's inteng.exe at run time, never copied here) is drawn as a grid of cells screenW / 16 by screenH / 16
// (toward zero), cell i at column i % 16, row i / 16 (0x56062c-0x5607ec); a left press inside the grid takes cell x / cellW + (y / cellH) x 16, held at 255 (0x560879-0x5608c6); the word
// is ((i << 6 | r) << 16 | g << 8 | b) x 4: the cell's number in the top byte, each channel times 4 below it (0x5608d5-0x5608fd).
#pragma once

#include <stdint.h>

namespace model
{
	int const kColourCells = 256;

	// The cell under (x, y) on a screen screenW x screenH; -1 when outside the grid (at or past 16 cells each way, or left of / above it) or when the screen is under 16 pixels a side
	int ColourCellAt(int32_t x, int32_t y, int32_t screenW, int32_t screenH);

	// The cell's place on the screen: left, top, width, height
	void ColourCellBox(int cell, int32_t screenW, int32_t screenH, int32_t *x, int32_t *y, int32_t *width, int32_t *height);

	// The colour word of cell `cell` whose table entry is (r, g, b), each 0..63 (values above 63 are taken as they are: the table is inteng's)
	uint32_t ColourWord(int cell, uint8_t r, uint8_t g, uint8_t b);

	// The colour the cell is drawn in, 0xAARRGGBB, opaque: each channel times 4, as the word and inteng's 16-bit grid have it. INTENG BUG, FIXED: inteng's 24-bit grid shifts each
	// channel left 3 in a byte (0x5606da-0x5606f4), which wraps for 32 and up, so its brighter colours were drawn dark
	uint32_t ColourShown(uint8_t r, uint8_t g, uint8_t b);
}
