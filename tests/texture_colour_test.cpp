// texture_colour_test.cpp: the Texture Map's Select Colour grid and colour word (src/model/texture_colour.h; inteng FUN_00560620). Written 2026-11-18 by Melanikus (Dietrich) for
// AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

#include "texture_colour.h"

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

int main()
{
	printf("SELECT COLOUR\n");

	// 640 x 480: cells 40 x 30
	Check(model::ColourCellAt(0, 0, 640, 480) == 0 && model::ColourCellAt(39, 29, 640, 480) == 0 && model::ColourCellAt(40, 0, 640, 480) == 1 && model::ColourCellAt(0, 30, 640, 480) == 16,
		"640 x 480: cells 40 by 30, numbered across then down");
	Check(model::ColourCellAt(639, 479, 640, 480) == 255 && model::ColourCellAt(640, 10, 640, 480) == -1 && model::ColourCellAt(10, 480, 640, 480) == -1 && model::ColourCellAt(-1, 10, 640, 480) == -1,
		"the last cell, and outside the grid nothing");

	// 1008 x 693 (the editor's frame less its status line in the harness): cells 63 x 43, the grid 1008 x 688: the bottom 5 rows of pixels are outside it
	Check(model::ColourCellAt(1007, 687, 1008, 693) == 255 && model::ColourCellAt(500, 690, 1008, 693) == -1 && model::ColourCellAt(63 * 5 + 1, 43 * 9 + 1, 1008, 693) == 9 * 16 + 5,
		"1008 x 693: cells 63 by 43; the pixels below the 16th row are outside");
	Check(model::ColourCellAt(5, 5, 15, 480) == -1, "a screen under 16 pixels wide has no grid");

	int32_t x = 0, y = 0, w = 0, h = 0;

	model::ColourCellBox(9 * 16 + 5, 1008, 693, &x, &y, &w, &h);
	Check(x == 315 && y == 387 && w == 63 && h == 43, "cell 149's box");

	// the word: the cell in the top byte, each channel x 4
	Check(model::ColourWord(0, 0, 0, 0) == 0 && model::ColourWord(1, 63, 0, 0) == 0x01FC0000u && model::ColourWord(17, 10, 20, 30) == 0x11285078u && model::ColourWord(255, 63, 63, 63) == 0xFFFCFCFCu,
		"the colour word: ((i << 6 | r) << 16 | g << 8 | b) x 4");
	Check(model::ColourShown(63, 32, 1) == 0xFFFC8004u, "the cell's colour: each channel x 4 (inteng's 24-bit << 3 wrapped 32 to 0: fixed)");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED texture_colour_test\n");
		return 1;
	}

	printf("passed texture_colour_test\n");
	return 0;
}
