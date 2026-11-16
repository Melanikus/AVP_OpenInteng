// menu_paint.h: what the menu bar shows, and where, worked out as plain sums: its titles along the toolbar, and the open menu's
// drop-down. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// The menu bar's model (src/ui/menu_bar.h) says where things are; this turns that into what the panel pass draws (panel_pass.h): plain
// boxes, for the drop-down's background and the lit item's band, and lines of text. Every line goes through PanelPaint_FitText, so it's
// made safe and cut to its box by the one rule the panels use (panel_paint.h). Nothing here draws, so a test can check every place.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "menu_bar.h"
#include "panel_paint.h"

namespace host
{
	// The colours, as the game's ARGB numbers: the drop-down's background, and the band behind the lit item and the open title
	uint32_t const kMenuDropColour = 0xFF30363Cu;
	uint32_t const kMenuLitColour = 0xFF4A6A8Au;
	uint32_t const kMenuCheckedColour = 0xFF3C6C3Cu;   // the band behind an item that is checked (its state is on): the draw mode in use, a switch that is on

	// The room between an item's words and its keys, in pixels
	int32_t const kMenuKeysGap = 16;

	// The height of one item: a line of the font, with the panels' margin above and below it
	int32_t MenuPaint_ItemHeight(int32_t lineHeight);

	// How wide each title and each drop-down is, as the font measures their words, for MenuBar::LayOut: a title is its words with the
	// panels' margin each side; a drop-down is its widest item's words with the margin each side, then, if any item has keys, the gap
	// and its widest keys with the margin each side, as the plan draws the words and the keys in boxes of their own. Each is held to
	// ui::kPanelMostSide. titleWidths and dropWidths have room for count numbers.
	void MenuPaint_Measure(const ui::Menu *menus, size_t count, PaintLetterWidth widthOf, int32_t *titleWidths, int32_t *dropWidths);

	// What the bar shows this frame: every title's line, in its box; and with a menu open, the band behind its title, its drop-down's
	// background, the band behind its lit item, and each shown item's words at the left of its row and its keys at the right. Gives back
	// how many lines and boxes it wrote, never more than the room given.
	void MenuPaint_Plan(const ui::MenuBar &bar, const ui::Menu *menus, PaintLetterWidth widthOf, int32_t lineHeight, PaintText *lines,
		size_t mostLines, size_t *lineCount, PaintBox *boxes, size_t mostBoxes, size_t *boxCount);
}
