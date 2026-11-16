// See menu_paint.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "menu_paint.h"

namespace host
{
	// How wide the font draws a text, after it's made safe as it will be shown: at most kPaintMostLetters letters, summed in 64 bits and
	// held to ui::kPanelMostSide
	static int32_t TextWidth(const char *text, PaintLetterWidth widthOf)
	{
		if (!text || !widthOf)
		{
			return 0;
		}

		std::string const safe = PanelPaint_SafeText(text);
		int64_t width = 0;

		for (size_t i = 0; i < safe.size(); i++)
		{
			int const letter = widthOf((unsigned char)safe[i]);

			width += letter > 0 ? letter : 0;
		}

		return width > ui::kPanelMostSide ? ui::kPanelMostSide : (int32_t)width;
	}

	static int32_t HeldToMostSide(int64_t value)
	{
		return value > ui::kPanelMostSide ? ui::kPanelMostSide : (value < 0 ? 0 : (int32_t)value);
	}

	int32_t MenuPaint_ItemHeight(int32_t lineHeight)
	{
		int32_t const line = lineHeight < 1 ? 1 : (lineHeight > kPaintTallestLine ? kPaintTallestLine : lineHeight);

		return line + 2 * kPaintMarginDown;
	}

	void MenuPaint_Measure(const ui::Menu *menus, size_t count, PaintLetterWidth widthOf, int32_t *titleWidths, int32_t *dropWidths)
	{
		if (!menus || !titleWidths || !dropWidths)
		{
			return;
		}

		for (size_t m = 0; m < count; m++)
		{
			int32_t widestName = 0;
			int32_t widestKeys = 0;

			for (size_t i = 0; i < menus[m].count; i++)
			{
				// An item's words are its name and, right after it, its tag (A6)
				int32_t const name = HeldToMostSide((int64_t)TextWidth(menus[m].items[i].name, widthOf) + TextWidth(menus[m].items[i].tag, widthOf));
				int32_t const keys = TextWidth(menus[m].items[i].keys, widthOf);

				widestName = name > widestName ? name : widestName;
				widestKeys = keys > widestKeys ? keys : widestKeys;
			}

			titleWidths[m] = HeldToMostSide((int64_t)TextWidth(menus[m].title, widthOf) + 2 * kPaintMarginAcross);
			// The words and the keys are each drawn in a box of their own, with the margin each side of each, and the gap between
			dropWidths[m] = HeldToMostSide((int64_t)widestName + 2 * kPaintMarginAcross + (widestKeys > 0 ? kMenuKeysGap + widestKeys + 2 * kPaintMarginAcross : 0));
		}
	}

	// One line, fitted to its box, added while there's room
	static void AddLine(const char *text, const ui::Box &box, PaintLetterWidth widthOf, int32_t lineHeight, PaintText *lines, size_t mostLines,
		size_t *lineCount)
	{
		if (*lineCount < mostLines && text && PanelPaint_FitText(text, box, widthOf, lineHeight, &lines[*lineCount]))
		{
			(*lineCount)++;
		}
	}

	static void AddBox(const ui::Box &box, uint32_t colour, PaintBox *boxes, size_t mostBoxes, size_t *boxCount)
	{
		if (*boxCount < mostBoxes && box.width > 0 && box.height > 0)
		{
			PaintBox const paint = { box.x, box.y, box.width, box.height, colour };

			boxes[*boxCount] = paint;
			(*boxCount)++;
		}
	}

	void MenuPaint_Plan(const ui::MenuBar &bar, const ui::Menu *menus, PaintLetterWidth widthOf, int32_t lineHeight, PaintText *lines,
		size_t mostLines, size_t *lineCount, PaintBox *boxes, size_t mostBoxes, size_t *boxCount)
	{
		*lineCount = 0;
		*boxCount = 0;

		if (!menus || !lines || !boxes || !bar.Loaded())
		{
			return;
		}

		int const open = bar.OpenMenu();

		// The open menu's title is drawn on its band, so the band goes first
		if (open >= 0)
		{
			AddBox(bar.TitleBox((size_t)open), kMenuLitColour, boxes, mostBoxes, boxCount);
		}

		for (size_t m = 0; m < ui::kMenuMostMenus && bar.TitleBox(m).height > 0; m++)
		{
			AddLine(menus[m].title, bar.TitleBox(m), widthOf, lineHeight, lines, mostLines, lineCount);
		}

		if (open < 0)
		{
			return;
		}

		ui::Box const drop = bar.DropBox();
		int32_t const itemHeight = MenuPaint_ItemHeight(lineHeight);

		AddBox(drop, kMenuDropColour, boxes, mostBoxes, boxCount);

		for (size_t row = 0; row < bar.ShownItems(); row++)
		{
			size_t const item = bar.FirstShownItem() + row;
			const ui::MenuItem &entry = menus[open].items[item];
			ui::Box const rowBox = { drop.x, (int32_t)((int64_t)drop.y + (int64_t)row * itemHeight), drop.width, itemHeight };

			if ((int)item == bar.LitItem())
			{
				AddBox(rowBox, kMenuLitColour, boxes, mostBoxes, boxCount);
			}
			else if (bar.IsChecked((size_t)open, item))
			{
				AddBox(rowBox, kMenuCheckedColour, boxes, mostBoxes, boxCount);
			}

			// The keys sit at the row's right, and the words have the rest of the row, less the gap
			int32_t const keysWidth = TextWidth(entry.keys, widthOf);
			int64_t const keysRoom = entry.keys ? (int64_t)keysWidth + 2 * kPaintMarginAcross : 0;
			int64_t const nameRoom = (int64_t)drop.width - (entry.keys ? keysRoom + kMenuKeysGap : 0);
			ui::Box const nameBox = { drop.x, rowBox.y, HeldToMostSide(nameRoom), itemHeight };

			// A tag goes right after the name and is never cut: the name gets the room the tag leaves, shortened with the game's own ".." when
			// it doesn't fit, and the tag is drawn right after what was drawn of the name (Melanikus: "[wip] besides them"). (Added 2026-11-15
			// by Melanikus (Frost), A6.)
			int32_t const tagWidth = TextWidth(entry.tag, widthOf);
			ui::Box wordsBox = nameBox;

			wordsBox.width = HeldToMostSide((int64_t)nameBox.width - tagWidth);

			size_t const before = *lineCount;

			AddLine(entry.name, wordsBox, widthOf, lineHeight, lines, mostLines, lineCount);

			if (entry.tag)
			{
				int32_t const drawn = *lineCount > before ? TextWidth(lines[before].text.c_str(), widthOf) : 0;
				ui::Box const tagBox = { (int32_t)((int64_t)nameBox.x + drawn), rowBox.y, HeldToMostSide((int64_t)tagWidth + 2 * kPaintMarginAcross), itemHeight };

				AddLine(entry.tag, tagBox, widthOf, lineHeight, lines, mostLines, lineCount);
			}

			if (entry.keys && nameRoom > 0)
			{
				ui::Box const keysBox = { (int32_t)((int64_t)drop.x + drop.width - keysRoom), rowBox.y, HeldToMostSide(keysRoom), itemHeight };

				AddLine(entry.keys, keysBox, widthOf, lineHeight, lines, mostLines, lineCount);
			}
		}
	}
}
