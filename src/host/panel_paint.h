// What the editor's panels show, and where: each panel's text, made safe and cut to fit its box, worked out as plain sums. Written
// 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The panels are drawn once a frame, after the level, into their own swap chain (view_surface.h). This file works out the text that goes
// on them: for each panel, its line of text at its top left, inside a margin, cut to fit its box. The drawing itself is the game's own
// menu text, done by the panel pass with what this gives back. Nothing here draws, so a test can check every place.
//
// TEXT IS HOSTILE. A panel's text can come from a level (an object's name, say), and anything from a level is hostile. So every text goes
// through the game's own rules for a name from outside, in AvPx's online_screens.cpp, the same ones the game's online pages and the
// editor's fields use, so there's one rule for safe text:
// - OnlineScreens_SafeName: at most kPaintMostLetters letters, ending at the first 0, and every letter outside the font's printable ones
//   (the space to the tilde, 32 to 126) a '?', so no letter outside the font's picture is ever looked up, and nothing moves the text;
// - OnlineScreens_FitToWidth: cut until it fits its box's width, by the font's own letter widths, ending with two dots when it was cut.
// Nothing in the text is ever read as a format: "%s" or "%n" is just the letters it is.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "panel_layout.h"

namespace host
{
	// The most letters one panel's text keeps
	size_t const kPaintMostLetters = 255;

	// The room left inside a panel's box, in pixels: at its left and right, and at its top and bottom. The status line is 20 pixels tall
	// as built, and the game's small menu font's lines are 15 (HUD_FONT_HEIGHT, hud_layout.h), so its top and bottom can spare 2 each.
	int32_t const kPaintMarginAcross = 4;
	int32_t const kPaintMarginDown = 2;

	// The tallest a line of text is counted as, whatever the font says
	int32_t const kPaintTallestLine = 256;

	// The status line's words when the window is too small for every panel to have the room it asks for
	char const *const kPaintTooSmall = "The window is too small for every panel";

	// How wide the font draws a letter, given as a number from 0 to 255, in pixels: the game's own convention (OnlineScreens_FitToWidth)
	typedef int (*PaintLetterWidth)(int letter);

	// One line of text, ready to draw: its safe, cut letters, and where its top left goes, in the window's pixels
	struct PaintText
	{
		std::string text;
		int32_t x;
		int32_t y;
	};

	// A plain box of one colour on the panels, such as an open menu's background: its top left and size in the window's pixels, and its
	// colour as ARGB
	struct PaintBox
	{
		int32_t x;
		int32_t y;
		int32_t width;
		int32_t height;
		uint32_t colour;
	};

	// The part of box inside a window of width by height pixels, in *out. Gives back false, with nothing in *out, when no part of it is
	// inside: a box of no size or of a size below 0, one wholly outside, or a window of no size. Every sum is done in 64 bits.
	bool PanelPaint_CutBox(const PaintBox &box, int32_t width, int32_t height, PaintBox *out);

	// The text each panel shows
	struct PanelTexts
	{
		std::string toolbar;
		std::string list;
		std::string fields;
		std::string status;
	};

	// The letters of text as they'll be shown, by OnlineScreens_SafeName: at most kPaintMostLetters, up to its first 0, each letter outside
	// 32..126 a '?'
	std::string PanelPaint_SafeText(const std::string &text);

	// text, made safe and cut to fit inside box, at its top left inside the margin, for a font whose letters are widthOf wide and whose
	// lines are lineHeight tall. Gives back false, with nothing in *out, when the box has no room for one line, there's no font measure,
	// or nothing of the text is left to show.
	bool PanelPaint_FitText(const std::string &text, const ui::Box &box, PaintLetterWidth widthOf, int32_t lineHeight, PaintText *out);

	// Every line the panels show this frame, in this order: the toolbar's, the list's, the field panel's, then the status line's, which
	// says kPaintTooSmall instead when the layout isn't roomy. The level's view shows none: the level is drawn there. Gives back how many
	// it put in items, never more than mostItems.
	size_t PanelPaint_Plan(const ui::PanelLayout &layout, const PanelTexts &texts, PaintLetterWidth widthOf, int32_t lineHeight,
		PaintText *items, size_t mostItems);
}
