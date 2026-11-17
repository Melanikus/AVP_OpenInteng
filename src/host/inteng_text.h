// inteng_text.h: where inteng's text routine puts the letters of a text: its new lines, tabs and wrapping (view-state-table.md H-1f). Written 2026-11-16 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES (FUN_0057be40, dump-full-4:41545, 0x57bea5-0x57c1aa; every text of inteng ends there, H-1d): from (x, y), a WORD at a time (the letters up to a new line, a tab, a space
// or the end; the stop itself is drawn as one more letter): a word that would reach the screen's right edge and is narrower than half the screen starts a new line first; then letter by letter:
//   '\n'  the next line: back to the first x, down by the letters' height (16 with font16);
//   '\t'  x to the next multiple of 4 x the height (64 pixels): ((x / 64) + 1) x 64, counted from the screen's left;
//   any other letter: when it would reach the right edge, the next line first (and if it would not fit there either, the text ends); drawn at (x, y), x moves on by its width.
// A line whose bottom would pass the screen's bottom ends the text (0x57bf2d, 0x57c18a; the Direct3D path, DAT_00917b70, draws on; ours is the blit's).
// Letters 128 and above are not in inteng's font (it reads its width table out of bounds for them, a signed char): here they are drawn as nothing and take no room.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	typedef int (*IntengLetterWidth)(int letter);

	// A run of letters drawn together, from (x, y): the letters between two jumps (a tab, a new line, a wrap)
	struct IntengTextPiece
	{
		std::string text;
		int32_t x;
		int32_t y;
	};

	// Lays the text out as FUN_0057be40 does, from (x, y), on a screen of screenWidth x screenHeight, with that letter width and height; at most mostPieces pieces (the rest left out).
	// Nothing for no width function, a height below 1 or a screen below 1 x 1.
	std::vector<IntengTextPiece> IntengText_Layout(std::string const &text, int32_t x, int32_t y, int32_t screenWidth, int32_t screenHeight, IntengLetterWidth widthOf, int32_t letterHeight,
		size_t mostPieces);
}
