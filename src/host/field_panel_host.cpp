// See field_panel_host.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".

#include "field_panel_host.h"

#include <string.h>

#include "online_screens.h"   // OnlineScreens_SafeName: the safe text every name from a level is read through; OnlineScreens_FitToWidth

namespace host
{
	FieldPanelHost::FieldPanelHost() : showing(false), typing(false), lineHeight(0)
	{
		ui::Box const none = { 0, 0, 0, 0 };

		box = none;
	}

	bool FieldPanelHost::Show(const char *newTitle, const ui::FieldDescriptor *list, size_t count, const ui::FieldValue *values,
		ui::FieldListProblem *problem, bool editable)
	{
		Clear();

		if ((count > 0 && (!list || !values)) || count > kFieldPanelHostMostFields)
		{
			return false;
		}

		// The host's own copy. Inspect (editable false) makes every field read-only whatever the adapter said, so it never changes anything;
		// an editable show keeps each field as its record made it (record_edit.h opens only the fields the model can set)
		descriptors.assign(list, list + count);

		for (ui::FieldDescriptor &field : descriptors)
		{
			field.readOnly = field.readOnly || !editable;
		}

		if (!panel.Load(descriptors.empty() ? 0 : descriptors.data(), descriptors.size(), values, problem))
		{
			Clear();
			return false;
		}

		char safe[kFieldPanelHostTitleMost + 1];

		OnlineScreens_SafeName(newTitle ? newTitle : "", kFieldPanelHostTitleMost, safe, sizeof(safe));
		title = safe;
		showing = true;
		return true;
	}

	void FieldPanelHost::Clear()
	{
		// The panel lets go of the list first, so it never points into a copy that's about to go
		panel.Load(0, 0, 0, 0);
		descriptors.clear();
		title.clear();
		showing = false;
		typing = false;
	}

	void FieldPanelHost::LayOut(const ui::Box &newBox, int32_t newLineHeight)
	{
		box = newBox;
		lineHeight = newLineHeight;
	}

	int32_t FieldPanelHost::HeightFor(int32_t forLineHeight) const
	{
		if (!showing || forLineHeight <= 0)
		{
			return 0;
		}

		// The title's line and a line for each field, and the margins: the height LinesThatFit counts the lines of, below
		int64_t const lines = 1 + (int64_t)descriptors.size();
		int64_t const height = lines * forLineHeight + 2 * kPaintMarginDown;

		return height > INT32_MAX ? INT32_MAX : (int32_t)height;
	}

	// How many lines fit in the box, inside the same margins every panel's text keeps (panel_paint.h): none for a line of no height, a box
	// with no room across, or a box too short for one line and its two margins. Counted in 64 bits.
	static size_t LinesThatFit(const ui::Box &box, int32_t lineHeight)
	{
		int64_t const inside = (int64_t)box.height - 2 * kPaintMarginDown;

		return lineHeight > 0 && (int64_t)box.width > 2 * kPaintMarginAcross && inside >= lineHeight ? (size_t)(inside / lineHeight) : 0;
	}

	// One field's line, in the three parts it is drawn from (FitFieldLine, below, puts them together to fit the line's room):
	// - mark: "! " when the panel flags the value (out of range, unknown, unreadable...), else nothing. It goes first and is never cut, so
	//   a strange value from a level, however long, is never shown as if it were fine;
	// - label: the field's name. It is the part that gives way when the line is too wide;
	// - value: ": ", the value as the panel shows it (a bit as on or off), and the unit after a number.
	struct FieldLineParts
	{
		std::string mark;
		std::string label;
		std::string value;
	};

	static FieldLineParts FieldLine(const ui::FieldPanel &panel, const ui::FieldDescriptor &field, size_t place, bool marked)
	{
		char shown[ui::kTextMostBytes + 1];

		panel.Shown(place, shown, sizeof(shown));

		FieldLineParts parts;

		parts.mark = panel.Flag(place) != ui::FieldFlag::None ? "! " : "";
		parts.label = field.name ? field.name : "";

		// The field the keyboard is in is marked, so a mapper can see where typing goes (FieldPanelHost::SetTyping)
		if (marked)
		{
			parts.mark += "> ";
		}

		parts.value = ": ";

		if (field.kind == ui::FieldKind::Bit)
		{
			parts.value += panel.BitChecked(place) ? "on" : "off";
		}
		else
		{
			parts.value += shown;
		}

		if (field.kind == ui::FieldKind::Number && field.unit)
		{
			parts.value += std::string(" ") + field.unit;
		}

		return parts;
	}

	// How wide a safe text is drawn, counted the way the game's cutter counts it (OnlineScreens_FitToWidth holds each letter's width to 0
	// to 1000 pixels), in 64 bits. A safe text is at most kPaintMostLetters letters, so the sum can't overflow.
	static int64_t DrawnWidth(const std::string &safe, PaintLetterWidth widthOf)
	{
		int64_t width = 0;

		for (char const letter : safe)
		{
			int const one = widthOf((unsigned char)letter);

			width += one < 0 ? 0 : one > 1000 ? 1000 : one;
		}

		return width;
	}

	// A safe text cut to room pixels and to at most mostLetters letters. When it's cut, it ends with the game's "..", so a cut is always
	// seen. Too many letters are cut here first, the same way the game's cutter cuts a text too wide (OnlineScreens_FitToWidth).
	static std::string Cut(const std::string &safe, int64_t room, size_t mostLetters, PaintLetterWidth widthOf)
	{
		if (mostLetters < 2)
		{
			return "";
		}

		char letters[kPaintMostLetters + 1];
		size_t const length = safe.size() < kPaintMostLetters ? safe.size() : kPaintMostLetters;

		memcpy(letters, safe.data(), length);
		letters[length] = '\0';

		if (length > mostLetters)
		{
			letters[mostLetters - 2] = '.';
			letters[mostLetters - 1] = '.';
			letters[mostLetters] = '\0';
		}

		OnlineScreens_FitToWidth(letters, room < 0 ? 0 : room > INT32_MAX ? INT32_MAX : (int)room, widthOf);
		return letters;
	}

	// The line a field shows in room pixels across. Its value is never cut while it can be shown whole (Melanikus's rule, through Bishop,
	// 2026-11-15: "the value is never cut". On a panel 150 pixels wide a door's times had been cut away: "Time take to open (0.1 - 25..").
	// 1. The whole line fits, in width and in letters (a line holds at most kPaintMostLetters): it's shown as it is.
	// 2. It doesn't, but the mark and the value do: the label is cut with the game's ".." (or left out, when not even ".." fits), and the
	//    mark and the value stay whole.
	// 3. Not even the mark and the value fit: the label is ".." alone, and the value is cut with "..", so it's seen to be cut.
	// Each part is made safe first, as PanelPaint_FitText makes the line, so what is measured is what is drawn. The letters are counted
	// before that, because the safe text keeps at most kPaintMostLetters and drops the rest unseen. The line given back fits, so
	// PanelPaint_FitText, which still cuts every line to its box, has nothing to cut but in a room too small for the mark and "..".
	static std::string FitFieldLine(const FieldLineParts &parts, int64_t room, PaintLetterWidth widthOf)
	{
		std::string const mark = PanelPaint_SafeText(parts.mark);
		std::string const label = PanelPaint_SafeText(parts.label);
		std::string const value = PanelPaint_SafeText(parts.value);

		if (!widthOf)
		{
			return mark + label + value;   // PanelPaint_FitText refuses a line with no letter widths to count
		}

		int64_t const markWide = DrawnWidth(mark, widthOf);
		int64_t const valueWide = DrawnWidth(value, widthOf);
		int64_t const dotsWide = DrawnWidth("..", widthOf);

		if (markWide + DrawnWidth(label, widthOf) + valueWide <= room && parts.mark.size() + parts.label.size() + parts.value.size() <= kPaintMostLetters)
		{
			return mark + label + value;
		}

		if (markWide + valueWide <= room && parts.mark.size() + parts.value.size() <= kPaintMostLetters)
		{
			return mark + Cut(label, room - markWide - valueWide, kPaintMostLetters - parts.mark.size() - parts.value.size(), widthOf) + value;
		}

		return mark + ".." + Cut(value, room - markWide - dotsWide, kPaintMostLetters - parts.mark.size() - 2, widthOf);
	}

	void FieldPanelHost::Plan(PaintLetterWidth widthOf, PaintText *lines, size_t mostLines, size_t *lineCount) const
	{
		*lineCount = 0;

		size_t const fit = LinesThatFit(box, lineHeight);

		if (!showing || !lines || fit == 0)
		{
			return;
		}

		size_t const fields = descriptors.size();
		// The title takes the first line. When the fields don't all fit in the rest, the last line says how many more there are.
		size_t const room = fit - 1;
		size_t const shownFields = fields <= room ? fields : (room > 0 ? room - 1 : 0);
		bool const moreLine = fields > shownFields && room > 0;
		size_t const total = 1 + shownFields + (moreLine ? 1 : 0);

		for (size_t i = 0; i < total && *lineCount < mostLines; i++)
		{
			// Where this line's box starts, in 64 bits. Past the end of the numbers there is no place left, so the lines stop there:
			// a place is never wrapped round to the other end.
			int64_t const top = (int64_t)box.y + (int64_t)i * lineHeight;

			if (top > INT32_MAX)
			{
				break;
			}

			std::string text;

			if (i == 0)
			{
				text = title;
			}
			else if (i - 1 < shownFields)
			{
				// Fitted so its value is never cut (FitFieldLine), in the room PanelPaint_FitText gives a line: its box inside the margins
				text = FitFieldLine(FieldLine(panel, descriptors[i - 1], i - 1, typing && panel.Focused() == (int)(i - 1)), (int64_t)box.width - 2 * kPaintMarginAcross, widthOf);
			}
			else
			{
				text = "... and " + std::to_string(fields - shownFields) + " more";
			}

			// A line's own box is one line and its two margins tall: PanelPaint_FitText takes the margins off, and wants what's left to
			// hold the line. The next line's box starts one line height lower, so the lines go down the box one line height apart.
			// (LinesThatFit saw that lineHeight and the margins fit in a box's height, so this sum can't pass the end of the numbers.)
			ui::Box const lineBox = { box.x, (int32_t)top, box.width, lineHeight + 2 * kPaintMarginDown };

			if (PanelPaint_FitText(text, lineBox, widthOf, lineHeight, &lines[*lineCount]))
			{
				(*lineCount)++;
			}
		}
	}

	ui::PanelAnswer FieldPanelHost::Handle(const ui::Event &event)
	{
		return showing ? panel.Handle(event) : ui::PanelAnswer::Nothing;
	}

	ui::PanelAnswer FieldPanelHost::ClickField(size_t field)
	{
		return showing ? panel.ClickField(field) : ui::PanelAnswer::Nothing;
	}

	int32_t FieldPanelHost::FieldAt(int32_t x, int32_t y) const
	{
		size_t const fit = LinesThatFit(box, lineHeight);

		if (!showing || fit == 0 || !ui::BoxHolds(box, x, y))
		{
			return -1;
		}

		size_t const fields = descriptors.size();
		size_t const room = fit - 1;
		size_t const shownFields = fields <= room ? fields : (room > 0 ? room - 1 : 0);
		// The title's line starts below the top margin, so a place in the margin is above every line
		int64_t const below = (int64_t)y - box.y - kPaintMarginDown;
		int64_t const row = below / lineHeight;

		return below >= 0 && row >= 1 && (size_t)(row - 1) < shownFields ? (int32_t)(row - 1) : -1;
	}
}
