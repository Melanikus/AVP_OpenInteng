// texture_area.h: the Texture Map's texture area, inteng's texture screen state +8 .. +0x14 (left, top, right, bottom) and +0x48 / +0x4c (its size), as each of inteng's
// writers makes it (reassess/tm-b-texture-area.md). Pure maths on model::TextureWindow (uv_model.h); the host keeps the area. Written 2026-11-18 by Melanikus (Dietrich) for
// AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include "uv_model.h"

namespace model
{
	int32_t const kInfiniteArea = 100000000;   // an "inf" side of the area runs from -100,000,000 to 100,000,000 (FUN_00557c00 0x557d6e)
	int const kRepeatsInfinite = 11;           // the chooser's 11th column or row

	// Repeating Texture Options (0x110 / 0x111 / 0x112, DAT_00914db8): what the area does when the current texture changes
	enum class AreaKeep
	{
		DefaultTo1X1 = 0,
		MaintainUsedArea = 1,
		MaintainNumRepeats = 2,
	};

	// (0, 0, w + addOne, h + addOne), its size right - left and bottom - top: the area of a texture with nothing before (FUN_00555dd0 0x555e2c)
	TextureWindow AreaDefault(int32_t width, int32_t height, int addOne);

	// FUN_00555dd0, the current texture set to one whose picture is newW x newH. hadTexture false: AreaDefault. The same texture: the area as it was. Another texture:
	// DefaultTo1X1: AreaDefault; MaintainUsedArea: as it was; MaintainNumRepeats: each side that is not inf becomes 0 .. newW x (right / oldW) + addOne (the bottom the same with the
	// heights). The size is right - left, bottom - top in every case. INTENG BUG, FIXED: inteng divides the old right by the NEW picture's width (0x555e79), so the repeats change
	// when the two pictures differ in size; here the old width divides it, so the repeats are kept. A width or height below 1 leaves the area as it was.
	TextureWindow AreaForTexture(TextureWindow const &before, bool hadTexture, bool sameTexture, int32_t oldW, int32_t oldH, int32_t newW, int32_t newH, AreaKeep keep, int addOne);

	// Reset Source Rect (0x9, FUN_0055d290): the whole picture, 0, 0, w, h. INTENG BUG, FIXED: inteng leaves out Add 1, which every other writer adds, so with Add 1 on the area was
	// one smaller than its tick says; here it is AreaDefault(w, h, addOne).
	TextureWindow AreaReset(int32_t width, int32_t height, int addOne);

	// Select Number of Repeats (0x10f, FUN_00557c00 0x557d5f-0x557e32): repeatsX across and repeatsY down (1 to 10, or kRepeatsInfinite): 0 .. w x repeats + addOne, an inf side
	// -kInfiniteArea .. kInfiniteArea; the size right - left, bottom - top. A right or bottom beyond 1,000,000,000 is held there.
	TextureWindow AreaFromRepeats(int32_t width, int32_t height, int repeatsX, int repeatsY, int addOne);

	// Add 1 to texture area (0x14, 0x54f0eb-0x54f15d): right and bottom one more (on) or one less (off); the size is left as it was
	void AreaAddOne(TextureWindow *area, bool on);
}
