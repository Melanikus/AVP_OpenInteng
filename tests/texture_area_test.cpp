// texture_area_test.cpp: the Texture Map's texture area (src/model/texture_area.h), inteng's texture screen state +8 .. +0x14 as each of its writers makes it
// (reassess/tm-b-texture-area.md). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

#include "texture_area.h"

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

static bool Is(model::TextureWindow const &area, int32_t left, int32_t top, int32_t right, int32_t bottom, int32_t sizeX, int32_t sizeY)
{
	return area.left == left && area.top == top && area.right == right && area.bottom == bottom && area.sizeX == sizeX && area.sizeY == sizeY;
}

static std::string Words(model::TextureWindow const &area)
{
	return std::to_string(area.left) + "," + std::to_string(area.top) + " to " + std::to_string(area.right) + "," + std::to_string(area.bottom) + " size " +
		std::to_string(area.sizeX) + "x" + std::to_string(area.sizeY);
}

int main()
{
	printf("THE TEXTURE AREA\n");

	model::TextureWindow const none = {};

	// FUN_00555dd0 with no texture before (0x555e2c): 0, 0, w + Add 1, h + Add 1
	model::TextureWindow const first = model::AreaForTexture(none, false, false, 0, 0, 128, 64, model::AreaKeep::MaintainUsedArea, 0);
	model::TextureWindow const firstPlus = model::AreaForTexture(none, false, false, 0, 0, 128, 64, model::AreaKeep::MaintainNumRepeats, 1);

	Check(Is(first, 0, 0, 128, 64, 128, 64), "no texture before: the whole picture whatever the option (" + Words(first) + ")");
	Check(Is(firstPlus, 0, 0, 129, 65, 129, 65), "no texture before, Add 1 on: one larger each way (" + Words(firstPlus) + ")");

	// another texture
	model::TextureWindow const twoByOne = model::AreaFromRepeats(64, 64, 2, 1, 0);

	Check(Is(twoByOne, 0, 0, 128, 64, 128, 64), "Select Number of Repeats 2X1 over 64 x 64: 0,0 to 128,64 (" + Words(twoByOne) + ")");

	model::TextureWindow const defaulted = model::AreaForTexture(twoByOne, true, false, 64, 64, 256, 32, model::AreaKeep::DefaultTo1X1, 0);
	model::TextureWindow const kept = model::AreaForTexture(twoByOne, true, false, 64, 64, 256, 32, model::AreaKeep::MaintainUsedArea, 0);
	model::TextureWindow const repeats = model::AreaForTexture(twoByOne, true, false, 64, 64, 256, 32, model::AreaKeep::MaintainNumRepeats, 0);
	model::TextureWindow const same = model::AreaForTexture(twoByOne, true, true, 64, 64, 64, 64, model::AreaKeep::DefaultTo1X1, 0);

	Check(Is(defaulted, 0, 0, 256, 32, 256, 32), "another texture, Default to 1X1: the new whole picture (" + Words(defaulted) + ")");
	Check(Is(kept, 0, 0, 128, 64, 128, 64), "another texture, Maintain Used Area: the area as it was (" + Words(kept) + ")");
	Check(Is(repeats, 0, 0, 512, 32, 512, 32),
		"INTENG BUG, FIXED: another texture, Maintain Num Repeats: 2X1 stays 2X1 over the new 256 x 32 (inteng divides by the new width: 256 x (128 / 256) = 0) (" + Words(repeats) + ")");
	Check(Is(same, 0, 0, 128, 64, 128, 64), "the same texture: the area as it was, whatever the option (" + Words(same) + ")");

	model::TextureWindow const repeatsPlus = model::AreaForTexture(model::AreaFromRepeats(64, 64, 3, 2, 1), true, false, 64, 64, 32, 128, model::AreaKeep::MaintainNumRepeats, 1);

	Check(Is(repeatsPlus, 0, 0, 97, 257, 97, 257), "Maintain Num Repeats with Add 1: 3X2 over 32 x 128 is 0,0 to 97,257 (" + Words(repeatsPlus) + ")");

	model::TextureWindow const infinite = model::AreaFromRepeats(64, 64, model::kRepeatsInfinite, 2, 0);
	model::TextureWindow const infiniteKept = model::AreaForTexture(infinite, true, false, 64, 64, 128, 128, model::AreaKeep::MaintainNumRepeats, 0);

	Check(Is(infinite, -100000000, 0, 100000000, 128, 200000000, 128), "infX2: the left and right without end (" + Words(infinite) + ")");
	Check(Is(infiniteKept, -100000000, 0, 100000000, 256, 200000000, 256), "Maintain Num Repeats keeps an inf side as it is (" + Words(infiniteKept) + ")");

	model::TextureWindow const bad = model::AreaForTexture(twoByOne, true, false, 64, 64, 0, 32, model::AreaKeep::DefaultTo1X1, 0);

	Check(Is(bad, 0, 0, 128, 64, 128, 64), "a picture with no width leaves the area as it was");

	// Reset Source Rect (FUN_0055d290)
	model::TextureWindow const reset = model::AreaReset(128, 64, 0);
	model::TextureWindow const resetPlus = model::AreaReset(128, 64, 1);

	Check(Is(reset, 0, 0, 128, 64, 128, 64), "Reset Source Rect: the whole picture (" + Words(reset) + ")");
	Check(Is(resetPlus, 0, 0, 129, 65, 129, 65), "INTENG BUG, FIXED: Reset Source Rect with Add 1 on is one larger each way, as its tick says (inteng: 0,0 to 128,64) (" + Words(resetPlus) + ")");

	// Add 1 to texture area (0x14)
	model::TextureWindow added = reset;

	model::AreaAddOne(&added, true);
	Check(Is(added, 0, 0, 129, 65, 128, 64), "Add 1 on: the right and bottom one more, the size left as it was (" + Words(added) + ")");
	model::AreaAddOne(&added, false);
	Check(Is(added, 0, 0, 128, 64, 128, 64), "Add 1 off: back (" + Words(added) + ")");

	model::TextureWindow const huge = model::AreaFromRepeats(65536, 65536, 10, 10, 1);

	Check(huge.right == 655361 && huge.bottom == 655361, "10X10 over the largest picture fits");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED texture_area_test\n");
		return 1;
	}

	printf("passed texture_area_test\n");
	return 0;
}
