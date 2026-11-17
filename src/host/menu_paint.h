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
	uint32_t const kMenuCheckedColour = 0xFF3C6C3Cu;   // (no longer drawn by the menus: an "on" item's words are yellow, kMenuOnColour)

	// INTENG'S MENU STYLE (FUN_004b7950 0x4b7aef-0x4b7b11, view-state-table.md H-5a): BLUE (0, 0, 0xff) the bar and the drop-downs (FUN_004b6800's fill, 0x4b7062), WHITE the words (0x4b7137), ORANGE (0xff, 0x7f, 0)
	// the pointed row's background (FUN_004b7590 / FUN_004b6c50 swap blue and orange in its band, H-5d), YELLOW the words of an item that is "on" (FUN_004b7470 swaps white and yellow, H-5e: no tick mark)
	uint32_t const kMenuBlueColour = 0xFF0000FFu;
	uint32_t const kMenuPointedColour = 0xFFFF7F00u;
	uint32_t const kMenuOnColour = 0xFFFFFF00u;

	// The room between an item's words and its keys, in pixels
	int32_t const kMenuKeysGap = 16;

	// The row under a line's top where an access letter's underline goes (its font's LAST row: FUN_0046ce70 0x46cffb); -1 (the start) for lineHeight - 2, the small font's
	void MenuPaint_SetUnderlineRow(int32_t row);

	// Whether the menus' lines are in inteng's own menu font (its glyph 2 is the sub-menu mark); false (the start): the mark is ">" in the small font
	void MenuPaint_SetMenuGlyphs(bool on);

	// The room a sub-menu's mark takes at the right of its row: glyph 2's cell (16 pixels in font16)
	int32_t const kMenuSubMarkRoom = 16;

	// The height of one item: a line of the font, with the panels' margin above and below it
	int32_t MenuPaint_ItemHeight(int32_t lineHeight);

	// How wide each title and each drop-down is, as the font measures their words, for MenuBar::LayOut: a title is its words with the
	// panels' margin each side; a drop-down is its widest item's words with the margin each side, then, if any item has keys, the gap
	// and its widest keys with the margin each side, as the plan draws the words and the keys in boxes of their own. Each is held to
	// ui::kPanelMostSide. titleWidths and dropWidths have room for count numbers.
	// cascadeWidths (when not null, ui::kMenuMostMenus x ui::kMenuMostItems): each item's cascade as wide as its widest row's words and tag with the margin each side (BAR-11), 0 for an item with none
	void MenuPaint_Measure(const ui::Menu *menus, size_t count, PaintLetterWidth widthOf, int32_t *titleWidths, int32_t *dropWidths, int32_t *cascadeWidths = nullptr);

	// What the bar shows this frame: every title's line, in its box; and with a menu open, the band behind its title, its drop-down's
	// background, the band behind its lit item, and each shown item's words at the left of its row and its keys at the right. Gives back
	// how many lines and boxes it wrote, never more than the room given.
	void MenuPaint_Plan(const ui::MenuBar &bar, const ui::Menu *menus, PaintLetterWidth widthOf, int32_t lineHeight, PaintText *lines,
		size_t mostLines, size_t *lineCount, PaintBox *boxes, size_t mostBoxes, size_t *boxCount);
}
