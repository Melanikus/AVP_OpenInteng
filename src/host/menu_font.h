// menu_font.h: inteng's own menu font (its Tools\font16.fnt and font8.fnt), read from the file at run time and drawn by the editor's menus. Written 2026-11-16 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES. Its menu object (FUN_004b7950 0x4b79de / 0x4b7a0d) loads two fonts with FUN_0046cb50: font8.fnt and font16.fnt, from its Tools folder; FUN_0046ce70 draws a
// string with one, every set bit of a letter one pixel in the colour asked (view-state-table.md H-5b, H-5c). The file: a 0x4c-byte header; the word at +0x1c is the letters'
// width when they all have the same one, 0 when each has its own (then 128 16-bit widths follow the header); +0x1e the letters' height; the byte at +0x24 the bytes of one row
// of a letter; then the 128 letters' rows, one bit a pixel, the leftmost the highest bit. Glyph 1 is a tick and glyph 2 a right-pointing triangle (a sub-menu's mark).
//
// THE FILE IS HOSTILE INPUT: Rebellion's font is never carried in our source (Bishop's ruling 11-16: it is read from the user's own inteng Tools folder), so any file of that name
// is read with every size checked before it is used, and refused with its reason (MenuFont_Read). Nothing is read past the bytes given.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	size_t const kMenuFontHeaderBytes = 0x4c;
	size_t const kMenuFontLetters = 128;
	int32_t const kMenuFontMostHeight = 32;      // inteng's are 8 and 16
	int32_t const kMenuFontMostRowBytes = 4;     // a letter at most 32 pixels wide (inteng's: 1 and 2 bytes)
	size_t const kMenuFontMostBytes = 64 * 1024; // the largest such file: header, widths, 128 letters of 32 rows of 4 bytes is under 17 KB

	struct MenuFont
	{
		int32_t height = 0;          // the letters' rows
		int32_t rowBytes = 0;        // bytes of one row
		int32_t widths[kMenuFontLetters] = {};   // each letter's width in pixels (the same for all in a fixed font)
		std::vector<uint8_t> bits;   // kMenuFontLetters x height x rowBytes bytes, letter by letter, row by row
	};

	enum class MenuFontRefusal
	{
		None,
		TooShort,          // fewer bytes than the header
		TooLong,           // more than kMenuFontMostBytes
		BadHeight,         // the height is 0 or above kMenuFontMostHeight
		BadRowBytes,       // the bytes of a row are 0 or above kMenuFontMostRowBytes
		BadFixedWidth,     // a fixed width wider than a row's bits
		ShortOfWidths,     // a proportional font whose 128 widths are not all there
		BadWidth,          // a width wider than a row's bits
		ShortOfLetters,    // the letters' rows are not all there
	};

	// Reads a font file's bytes: true and *font filled, or false with *refusal saying why (nothing else is changed then)
	bool MenuFont_Read(uint8_t const *bytes, size_t size, MenuFont *font, MenuFontRefusal *refusal);

	// The refusal in plain words, for the status line
	char const *MenuFont_Why(MenuFontRefusal refusal);

	// Whether the pixel (x, y) of a letter is set; false outside the letter
	bool MenuFont_Pixel(MenuFont const &font, unsigned letter, int32_t x, int32_t y);
}
