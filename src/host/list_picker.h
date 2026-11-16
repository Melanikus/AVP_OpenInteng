// list_picker.h: inteng's LIST CHOOSER dialog (Genfunc.cpp, FUN_00475bf0; FUN_0048c610 is the Hierarchy mode's way in): a window with a title, the choices as a GRID OF BUTTONS laid out in columns, Prev / Next buttons
// when there are more choices than fit one page, and a text field under them. A choice is made by CLICKING its button. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES (read from FUN_00475bf0, dump-all-2.txt:8668-9000, 2026-11-15; Select Hierarchy FUN_0048b8a0 0x48bb2b and Select Sequence FUN_0048bc10 0x48bff3 call it as FUN_0048c610(title, names, numbers)):
// - the width of a column is the widest choice's text + 0x14 (20), at least 100 and at most the screen's width - 0x28;
// - rows in a column: (screen height - 0x78) / 0x14 (rows are 0x14 = 20 pixels apart), at most as many as there are choices, at least 1;
// - columns: the screen's width / (column width + 0x14), at most as many as the choices need, at least 1;
// - the window: (column width + 0x14) x columns wide (at least 300, at least the title's width + 0x28, at most the screen's width - 0x14) and (rows + 5) x 0x14 high;
// - the choices go down the first column, then the next (index = rows x column + row); button (row, column) is at x = window width / columns x column + 10, y = row x 0x14 + 0x1e, as wide as the column minus 0x14 and 0x12
//   high; a page holds rows x columns choices;
// - at the bottom: Prev at (0x32, height - 0x1e) and Next at (0x96, height - 0x1e), each 0x50 x 0x12 (FUN_0057d830 with the words "Prev" and "Next"), and a text field at (0x1e, height - 0x3c), width - 100 wide, 0x12 high,
//   holding up to 100 letters (FUN_0057d8e0).
// What a click on a choice does after that, and what the text field is for, is in the dialog's event method (vtable 0x5ad62c), not read here: the editor answers the question with the clicked choice's place.
//
// NAMED: the colours and fonts are the editor's own. Keys (Up, Down, Left, Right, Page Up, Page Down, Home, End, Enter) move a highlight and pick it, as an addition; inteng's dialog is a mouse dialog.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

namespace host
{
	struct ListPickerBox
	{
		int32_t x, y, width, height;   // in the window's pixels
	};

	struct ListPickerCell
	{
		int32_t choice;        // the place of the choice in the list, from 0
		ListPickerBox box;
	};

	enum class ListPickerHit
	{
		Nothing,
		Choice,   // *choice says which
		Prev,
		Next,
		Text      // the text field
	};

	class ListPicker
	{
	public:
		// The choices, the title, the screen's size and the width of a text (the editor's own letters). The dialog is shown in the middle of the screen (FUN_0057c800 centres every window); the screen is the box from (originX, originY) of that width and height.
		void Open(std::string const &title, std::vector<std::string> const &choices, int32_t screenWidth, int32_t screenHeight, std::function<int32_t(std::string const &)> const &textWidth, int32_t originX = 0, int32_t originY = 0);
		void Close();
		bool IsOpen() const { return open; }

		// What is on the screen now
		ListPickerBox Window() const { return window; }
		ListPickerBox PrevButton() const { return prev; }
		ListPickerBox NextButton() const { return next; }
		ListPickerBox TextField() const { return field; }
		std::string const &Title() const { return title; }
		int32_t Page() const { return page; }
		int32_t Pages() const { return pages; }
		int32_t Rows() const { return rows; }
		int32_t Columns() const { return columns; }
		int32_t Count() const { return (int32_t)choices.size(); }
		std::string const &Choice(int32_t place) const { return choices[(size_t)place]; }

		// The buttons of the choices of the page shown, in the window's pixels
		std::vector<ListPickerCell> Cells() const;

		// What a press at (x, y) (the window's pixels) is on. Prev and Next turn the page by themselves (and answer Prev / Next); a click on a choice gives its place in *choice.
		ListPickerHit Press(int32_t x, int32_t y, int32_t *choice);

		// The choice the pointer is over (-1 for none), to light
		int32_t Over(int32_t x, int32_t y) const;

		// Keys (the editor's own, an addition): the highlight is moved over the choices, a page turned with Page Up / Page Down. Gives back whether the key was the list's.
		enum class Key { Up, Down, Left, Right, PageUp, PageDown, Home, End };
		bool Move(Key key);
		int32_t Highlighted() const { return highlighted; }

		void SetPage(int32_t newPage);

	private:
		bool open = false;
		std::string title;
		std::vector<std::string> choices;
		int32_t rows = 1, columns = 1, page = 0, pages = 1, highlighted = 0;
		int32_t columnWidth = 100;
		ListPickerBox window = { 0, 0, 0, 0 }, prev = { 0, 0, 0, 0 }, next = { 0, 0, 0, 0 }, field = { 0, 0, 0, 0 };
	};
}
