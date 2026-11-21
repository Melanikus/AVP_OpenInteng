// host_dialog_box_test: inteng's text boxes' sizes and places (dialog_box.h InDialog::OpenText). Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng (the Strategy box batch, Bishop's
// Q9c: number boxes 400 x 200 as inteng's). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// ONE field with no label is inteng's FUN_00474170 / FUN_004747f0 / FUN_00474bc0: 400 x 200, the field at 0x1e, 100, 300 x 0x12, Ok at 100, 0x96 and Cancel at 200, 0x96 (0x32 x 0x10).
// Fields with labels are FUN_00474350's: 400 x 250, each field 0x96 wide at 0x1e with its label at 0xbe, the rows 100, 0x7c, 0x94, Ok and Cancel at 0xbe. Both in the middle of the screen.

#include <stdio.h>

#include <string>
#include <vector>

#include "dialog_box.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static bool Is(host::DialogRect const &r, int32_t x, int32_t y, int32_t w, int32_t h)
{
	return r.x == x && r.y == y && r.width == w && r.height == h;
}

static std::string Words(host::DialogRect const &r)
{
	return std::to_string(r.x) + " " + std::to_string(r.y) + " " + std::to_string(r.width) + " " + std::to_string(r.height);
}

int main()
{
	host::DialogRect const screen = { 0, 0, 1008, 729 };
	auto const width = [](std::string const &text) { return (int32_t)text.size() * 8; };

	// one field, no label: inteng's number box
	{
		host::InDialog box;
		host::DialogField field;

		field.text = "0";
		box.OpenText("Select alternate location group number\n(0 means no group)", { field }, screen, width);

		host::DialogRect const w = box.Window();

		Check(w.width == 400 && w.height == 200, "one field with no label: the window is 400 x 200 (0x47419c), it is " + Words(w));
		Check(w.x == (1008 - 400) / 2 && w.y == (729 - 200) / 2, "and in the middle of the screen (FUN_0057c800 0x57c80e), at " + Words(w));
		Check(box.Fields().size() == 1 && Is(box.Fields()[0].edit, w.x + 0x1e, w.y + 100, 300, 0x12), "its field at 0x1e, 100, 300 x 0x12 (0x47420c)" +
			(box.Fields().empty() ? std::string() : ", it is " + Words(box.Fields()[0].edit)));
		Check(box.Buttons().size() == 2 && Is(box.Buttons()[0].box, w.x + 100, w.y + 0x96, 0x32, 0x10) && Is(box.Buttons()[1].box, w.x + 200, w.y + 0x96, 0x32, 0x10),
			"Ok at 100, 0x96 and Cancel at 200, 0x96, 0x32 x 0x10 (0x474248 / 0x474287)" + (box.Buttons().size() < 2 ? std::string() : ", they are " + Words(box.Buttons()[0].box) + " and " + Words(box.Buttons()[1].box)));
		Check(box.TextArea().height == 0x28, "its text area 0x28 high (0x4741d6), it is " + std::to_string(box.TextArea().height));
	}

	// fields with labels: FUN_00474350's box
	{
		host::InDialog box;
		host::DialogField a, b;

		a.label = "x";
		b.label = "y";
		box.OpenText("Two numbers", { a, b }, screen, width);

		host::DialogRect const w = box.Window();

		Check(w.width == 400 && w.height == 0xfa, "fields with labels: the window is 400 x 250 (0x47437c), it is " + Words(w));
		Check(box.Fields().size() == 2 && Is(box.Fields()[0].edit, w.x + 0x1e, w.y + 100, 0x96, 0x12) && Is(box.Fields()[0].labelBox, w.x + 0xbe, w.y + 100, 0x96, 0x12) &&
			Is(box.Fields()[1].edit, w.x + 0x1e, w.y + 0x7c, 0x96, 0x12), "each field 0x96 wide at 0x1e, its label at 0xbe, the rows 100 and 0x7c (0x4743fd / 0x474438 / 0x4744eb)");
		Check(box.Buttons().size() == 2 && box.Buttons()[0].box.y == w.y + 0xbe, "Ok at 0xbe");
	}

	// A LONG COMBO LIST (the owner 11-21, through Bishop: "We can't scroll down to see message 1001..."): the Message window's number list holds the 53 stock messages and his 1001. Opened,
	// every row it draws must lie on the screen, or a person can never press the rows past the screen's foot. The window is the Message window's (strategy_screen.cpp MissionTextWindow:
	// 500 x 400, the number box at 0x1e, 0x24, 300 x 0x12) on a hidden run's 1008 x 729 screen.
	{
		host::InDialog box;
		host::DialogFormSpec spec;
		host::DialogWidget combo;

		spec.width = 500;
		spec.height = 400;
		combo.type = host::DialogWidgetType::Combo;
		combo.box = { 0x1e, 0x24, 300, 0x12 };

		for (int n = 1; n <= 53; n++)
		{
			combo.items.push_back(std::to_string(n));
		}

		combo.items.push_back("1001");
		combo.selected = 12;
		combo.text = "13";
		spec.widgets = { combo };
		box.OpenForm(spec, screen, width);

		host::DialogRect const closed = box.Widgets()[0].box;
		int32_t answer = 0;

		box.Press(closed.x + closed.width - 8, closed.y + closed.height / 2, &answer);   // on its arrow, as a person opens it

		host::DialogWidget const &open = box.Widgets()[0];
		size_t drawn = 0, offScreen = 0;
		std::string firstOff;

		for (size_t row = 0; row < open.items.size(); row++)
		{
			host::DialogRect const r = host::InDialog::ComboRow(open, row);

			if (r.width <= 0 || r.height <= 0)
			{
				continue;   // a row scrolled out of view is not drawn
			}

			drawn++;

			if (r.x < screen.x || r.y < screen.y || r.x + r.width > screen.x + screen.width || r.y + r.height > screen.y + screen.height)
			{
				offScreen++;
				firstOff = firstOff.empty() ? open.items[row] + " at " + Words(r) : firstOff;
			}
		}

		Check(open.listOpen, "a press on the Message number box's arrow opens its list of 54 rows");
		Check(drawn > 0 && offScreen == 0, "every row the open list draws lies on the 1008 x 729 screen, so a person can press it: " + std::to_string(drawn) + " drawn, " +
			std::to_string(offScreen) + " past the screen" + (firstOff.empty() ? std::string() : " (the first: row " + firstOff + ")"));
	}

	// THE LIST THAT SCROLLS (dialog_box.h): the rows a DLL combo's dropped height gives, worked out as Windows does for an 8-point MS Sans Serif dialog
	Check(host::DialogBox_ComboListRows(74) == 7 && host::DialogBox_ComboListRows(81) == 8 && host::DialogBox_ComboListRows(121) == 13,
		"a DLL combo's list shows the rows its resource drops it to: Edit object properties' type list (74 dialog units) 7, its other lists (81) 8, Sound Edit's wav names (121) 13 (they are " +
		std::to_string(host::DialogBox_ComboListRows(74)) + ", " + std::to_string(host::DialogBox_ComboListRows(81)) + ", " + std::to_string(host::DialogBox_ComboListRows(121)) + ")");
	Check(host::DialogBox_ComboListRows(0) == 1 && host::DialogBox_ComboListRows(-5) == 1, "a resource too short for a row still shows one");

	// the Message window's list as strategy_screen.cpp makes it: 54 numbers, 13 rows in view
	{
		host::InDialog box;
		host::DialogFormSpec spec;
		host::DialogWidget combo;
		std::vector<std::string> changes;

		spec.width = 500;
		spec.height = 400;
		combo.type = host::DialogWidgetType::Combo;
		combo.box = { 0x1e, 0x24, 300, 0x12 };
		combo.editable = true;
		combo.listRows = host::DialogBox_ComboListRows(121);

		for (int n = 1; n <= 53; n++)
		{
			combo.items.push_back(std::to_string(n));
		}

		combo.items.push_back("1001");
		combo.selected = 46;   // "47", the number message13 holds
		combo.text = "47";
		spec.widgets = { combo };
		spec.onChange = [&changes](std::vector<host::DialogWidget> &widgets, size_t changed) { changes.push_back(widgets[changed].text); };
		box.OpenForm(spec, screen, width);

		host::DialogRect const closed = box.Widgets()[0].box;
		int32_t answer = 0;
		auto const openIt = [&]() { box.Press(closed.x + closed.width - 8, closed.y + closed.height / 2, &answer); };
		auto const shownRows = [&]()
		{
			std::string words;

			for (size_t row = 0; row < box.Widgets()[0].items.size(); row++)
			{
				if (host::InDialog::ComboRow(box.Widgets()[0], row).width > 0)
				{
					words += (words.empty() ? "" : " ") + box.Widgets()[0].items[row];
				}
			}

			return words;
		};
		auto const rowAt = [&](size_t row) { return host::InDialog::ComboRow(box.Widgets()[0], row); };

		openIt();
		Check(box.ListOpen() && host::InDialog::ComboRowsShown(box.Widgets()[0]) == 13 && host::InDialog::ComboScrolls(box.Widgets()[0]),
			"opened, 13 of the 54 rows are in view, and the list scrolls");
		Check(shownRows() == "42 43 44 45 46 47 48 49 50 51 52 53 1001", "it opens scrolled so the picked row (47) shows, as near the top as the list's end lets it: " + shownRows());
		Check(Is(rowAt(41), closed.x, closed.y + closed.height, 300 - 16, 0x12), "the first row in view sits right under the box, 284 wide: the scroll bar takes the right 16 pixels (it is " +
			Words(rowAt(41)) + ")");

		host::DialogScrollBar const bar = host::InDialog::ComboScrollBar(box.Widgets()[0]);

		Check(Is(bar.column, closed.x + 300 - 16, closed.y + closed.height, 16, 13 * 0x12) && Is(bar.up, bar.column.x, bar.column.y, 16, 0x12) &&
			Is(bar.down, bar.column.x, bar.column.y + 12 * 0x12, 16, 0x12) && bar.thumb.height >= 8 && bar.thumb.y + bar.thumb.height == bar.down.y,
			"the scroll bar: a column beside the 13 rows, an arrow a row high at each end, the thumb at the track's foot (the list is at its end) (column " + Words(bar.column) +
			", thumb " + Words(bar.thumb) + ")");

		// the wheel: 3 rows a notch, toward the top for a delta over 0; it stops at the ends; nothing is picked
		box.Wheel(120);
		Check(rowAt(38).width > 0 && rowAt(51).width == 0 && rowAt(37).width == 0, "a notch of the wheel up scrolls 3 rows: 39 to 51 in view (" + shownRows() + ")");
		box.Wheel(120 * 40);
		Check(rowAt(0).y == closed.y + closed.height && rowAt(13).width == 0, "turned far up, it stops at the first row (" + shownRows() + ")");
		box.Wheel(-60);
		Check(rowAt(0).width == 0 && rowAt(1).width > 0, "half a notch scrolls the one whole row it makes, and keeps the rest (Windows' rule, as Wine's listbox.c writes it) (" + shownRows() + ")");
		box.Wheel(-60);
		Check(rowAt(2).width == 0 && rowAt(3).width > 0, "the other half makes the notch's 3 rows in all (" + shownRows() + ")");
		box.Wheel(-120 * 40);
		Check(rowAt(53).width > 0 && rowAt(53).y + rowAt(53).height <= screen.y + screen.height && box.ListOpen() && box.Widgets()[0].selected == 46,
			"turned far down, it stops with 1001 the last row in view, on the screen; the list stays open and the pick is still 47 (" + shownRows() + ")");

		// a press on a row that was scrolled into view picks THAT row
		host::DialogRect const last = rowAt(53);

		box.Press(last.x + last.width / 2, last.y + last.height / 2, &answer);
		Check(!box.ListOpen() && box.Widgets()[0].selected == 53 && box.Widgets()[0].text == "1001" && changes.size() == 1 && changes[0] == "1001",
			"a press on 1001, scrolled into view, picks it: the box says 1001 and the change hook ran once (it says \"" + box.Widgets()[0].text + "\")");

		// the scroll bar's parts: an arrow a row, the track a page, the list stays open; a press on the thumb does nothing
		openIt();
		Check(rowAt(41).width > 0 && rowAt(53).width > 0, "opened again on 1001: the last 13 rows in view (" + shownRows() + ")");
		host::DialogScrollBar const end = host::InDialog::ComboScrollBar(box.Widgets()[0]);
		box.Press(end.up.x + 8, end.up.y + 8, &answer);
		Check(box.ListOpen() && rowAt(40).width > 0 && rowAt(53).width == 0, "a press on the up arrow scrolls a row, and the list stays open (" + shownRows() + ")");
		host::DialogScrollBar const mid = host::InDialog::ComboScrollBar(box.Widgets()[0]);
		box.Press(mid.column.x + 8, mid.up.y + mid.up.height + 1, &answer);
		Check(box.ListOpen() && rowAt(27).width > 0 && rowAt(26).width == 0, "a press on the track above the thumb scrolls a page, 13 rows (" + shownRows() + ")");
		host::DialogScrollBar const page = host::InDialog::ComboScrollBar(box.Widgets()[0]);
		box.Press(page.thumb.x + 8, page.thumb.y + page.thumb.height / 2, &answer);
		Check(box.ListOpen() && rowAt(27).width > 0 && rowAt(26).width == 0, "a press on the thumb does nothing (it isn't dragged)");
		box.Press(page.down.x + 8, page.down.y + 8, &answer);
		Check(box.ListOpen() && rowAt(28).width > 0 && rowAt(27).width == 0, "a press on the down arrow scrolls a row (" + shownRows() + ")");

		// the keys: Home and End, the arrows, Page Up and Down move the row the keys are on and bring it into view; Return picks it and closes the list only; Escape closes it only
		box.ListKey(host::DialogListKey::Home);
		Check(box.Widgets()[0].listLit == 0 && rowAt(0).y == closed.y + closed.height, "Home: the first row, at the top");
		box.ListKey(host::DialogListKey::PageDown);
		Check(box.Widgets()[0].listLit == 12 && rowAt(0).width > 0, "Page Down: the rows in view less one on (row 12), still in view (" + shownRows() + ")");
		box.ListKey(host::DialogListKey::Down);
		Check(box.Widgets()[0].listLit == 13 && rowAt(13).width > 0 && rowAt(0).width == 0, "Down past the last row in view scrolls a row (" + shownRows() + ")");
		box.ListKey(host::DialogListKey::End);
		Check(box.Widgets()[0].listLit == 53 && rowAt(53).width > 0, "End: 1001, in view");
		box.ListKey(host::DialogListKey::Up);
		box.ListKey(host::DialogListKey::PageUp);
		Check(box.Widgets()[0].listLit == 40 && rowAt(40).width > 0, "Up, then Page Up: row 40, in view (" + shownRows() + ")");
		Check(box.ListReturn() && !box.ListOpen() && box.IsOpen() && box.Widgets()[0].text == "41" && changes.size() == 2,
			"Return picks the row the keys are on (41) and closes the list; the dialog stays open");
		openIt();
		box.ListKey(host::DialogListKey::Home);
		Check(box.ListEscape() && !box.ListOpen() && box.IsOpen() && box.Widgets()[0].text == "41" && changes.size() == 2, "Escape closes the list only: nothing picked, the dialog open");
		Check(!box.ListReturn() && !box.ListEscape() && !box.Wheel(120) && !box.ListKey(host::DialogListKey::Down), "with no list open, none of them takes anything");
	}

	// a short list is as it was: every row, as wide as the box, no scroll bar
	{
		host::InDialog box;
		host::DialogFormSpec spec;
		host::DialogWidget combo;

		combo.type = host::DialogWidgetType::Combo;
		combo.box = { 0x1e, 0x24, 200, 0x12 };
		combo.items = { "Random", "Original", "Marine" };
		combo.listRows = host::DialogBox_ComboListRows(81);
		spec.widgets = { combo };
		box.OpenForm(spec, screen, width);

		int32_t answer = 0;
		host::DialogRect const closed = box.Widgets()[0].box;

		box.Press(closed.x + 4, closed.y + 4, &answer);
		Check(box.ListOpen() && !host::InDialog::ComboScrolls(box.Widgets()[0]) && host::InDialog::ComboScrollBar(box.Widgets()[0]).column.width == 0 &&
			Is(host::InDialog::ComboRow(box.Widgets()[0], 2), closed.x, closed.y + 3 * 0x12, 200, 0x12), "a list of 3 under its 8 rows: all 3 in view, as wide as the box, no scroll bar");
		Check(box.ListOpen() && box.Widgets()[0].listLit == -1, "nothing picked yet: no row lit as it opens");
		box.ListKey(host::DialogListKey::Down);
		Check(box.Widgets()[0].listLit == 0, "Down lights the first row");
	}

	// a list with no resource of its own never runs past the screen's foot: inteng's own 640 x 480, the box near the foot
	{
		host::InDialog box;
		host::DialogFormSpec spec;
		host::DialogWidget combo;
		host::DialogRect const small = { 0, 0, 640, 480 };

		spec.width = 500;
		spec.height = 400;
		combo.type = host::DialogWidgetType::Combo;
		combo.box = { 0x1e, 300, 200, 0x12 };

		for (int n = 0; n < 40; n++)
		{
			combo.items.push_back("row " + std::to_string(n));
		}

		spec.widgets = { combo };
		box.OpenForm(spec, small, width);

		int32_t answer = 0;
		host::DialogRect const closed = box.Widgets()[0].box;
		size_t const fit = (size_t)((480 - (closed.y + closed.height)) / 0x12);

		box.Press(closed.x + 4, closed.y + 4, &answer);
		Check(host::InDialog::ComboRowsShown(box.Widgets()[0]) == fit && host::InDialog::ComboRow(box.Widgets()[0], fit - 1).y + 0x12 <= 480,
			"with no rows of its own, the list shows the " + std::to_string(fit) + " rows between its box and the screen's foot, and scrolls (it shows " +
			std::to_string(host::InDialog::ComboRowsShown(box.Widgets()[0])) + ")");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return checksFailed ? 1 : 0;
}
