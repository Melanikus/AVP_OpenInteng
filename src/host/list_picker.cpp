// list_picker.cpp: see list_picker.h (inteng's list chooser dialog, FUN_00475bf0).
#include "list_picker.h"

#include <algorithm>

namespace host
{
	namespace
	{
		int32_t const kRowHeight = 0x14;     // 20 pixels between rows
		int32_t const kButtonHeight = 0x12;

		// THE PAGE (Bishop's ruling 2026-11-18, named): inteng works its page out from its screen (FUN_00475bf0: rows (height - 0x78) / 0x14 at 0x475d2f, columns
		// width / (column + 0x14) at 0x475d60), and its screen was 640 x 480. Ours works it out on at most 640 x 480, so a bigger window keeps inteng's page (18 rows, as many
		// columns as 640 holds) and a page always fits in one frame: on a 2560 x 1440 window Object Placement's Select Object had 302 cells, more boxes than a frame draws, and
		// the window was not drawn at all (Drake 11-18). The window stays in the middle of the real screen; Prev / Next page through the rest, as before.
		int32_t const kPageScreenWidth = 640;
		int32_t const kPageScreenHeight = 480;
		size_t const kMostChoices = 100000;  // a bound the editor puts

		bool Inside(ListPickerBox const &box, int32_t x, int32_t y)
		{
			return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
		}
	}

	void ListPicker::Open(std::string const &newTitle, std::vector<std::string> const &list, int32_t screenWidth, int32_t screenHeight, std::function<int32_t(std::string const &)> const &textWidth, int32_t originX, int32_t originY)
	{
		title = newTitle;
		choices = list;

		if (choices.size() > kMostChoices)
		{
			choices.resize(kMostChoices);
		}

		int32_t const count = (int32_t)choices.size();

		// the screen the page is worked out on: the real one, at most inteng's 640 x 480 (kPageScreenWidth)
		int32_t const pageWidth = screenWidth < kPageScreenWidth ? screenWidth : kPageScreenWidth;
		int32_t const pageHeight = screenHeight < kPageScreenHeight ? screenHeight : kPageScreenHeight;

		// the column's width: the widest choice + 0x14, at least 100, at most the screen's width - 0x28
		columnWidth = 100;

		for (std::string const &one : choices)
		{
			int32_t const w = textWidth(one);

			if (columnWidth <= w + 0x14)
			{
				columnWidth = w + 0x14;
			}
		}

		if (columnWidth > pageWidth - 0x28 && pageWidth - 0x28 >= 100)
		{
			columnWidth = pageWidth - 0x28;
		}

		// rows in a column, columns on the screen
		rows = (pageHeight - 0x78) / kRowHeight;

		if (rows > count)
		{
			rows = count;
		}

		if (rows < 1)
		{
			rows = 1;
		}

		columns = pageWidth / (columnWidth + 0x14);

		if ((count - 1 + rows) / rows <= columns)
		{
			columns = (count - 1 + rows) / rows;
		}

		if (columns < 1)
		{
			columns = 1;
		}

		// the window
		int32_t width = (columnWidth + 0x14) * columns;

		if (width < 300)
		{
			width = 300;
		}

		int32_t const titleWidth = textWidth(title) + 0x28;

		if (width < titleWidth)
		{
			width = titleWidth;
		}

		if (width > pageWidth - 0x14 && pageWidth - 0x14 >= 300)
		{
			width = pageWidth - 0x14;
		}

		int32_t const height = (rows + 5) * kRowHeight;

		window.width = width;
		window.height = height;
		// FUN_0057c800 puts every window in the MIDDLE of the screen (0x57c80e-0x57c82d: ((screen width - width) / 2, (screen height - height) / 2); the "(10, 10)" the callers show is not an argument)
		window.x = originX + (screenWidth - width) / 2;
		window.y = originY + (screenHeight - height) / 2;

		prev = { window.x + 0x32, window.y + height - 0x1e, 0x50, kButtonHeight };
		next = { window.x + 0x96, window.y + height - 0x1e, 0x50, kButtonHeight };
		field = { window.x + 0x1e, window.y + height - 0x3c, width - 100, kButtonHeight };

		int32_t const perPage = rows * columns;

		pages = perPage > 0 ? (count + perPage - 1) / perPage : 1;

		if (pages < 1)
		{
			pages = 1;
		}

		page = 0;
		highlighted = 0;
		keyed = false;
		open = true;
	}

	void ListPicker::Close()
	{
		open = false;
		choices.clear();
	}

	std::vector<ListPickerCell> ListPicker::Cells() const
	{
		std::vector<ListPickerCell> cells;

		if (!open)
		{
			return cells;
		}

		int32_t const perPage = rows * columns;
		int32_t const cellWidth = window.width / columns - 0x14;

		for (int32_t column = 0; column < columns; column++)
		{
			for (int32_t row = 0; row < rows; row++)
			{
				int32_t const choice = page * perPage + rows * column + row;

				if (choice >= (int32_t)choices.size())
				{
					continue;
				}

				ListPickerCell cell;

				cell.choice = choice;
				cell.box = { window.x + (window.width / columns) * column + 10, window.y + row * kRowHeight + 0x1e, cellWidth, kButtonHeight };
				cells.push_back(cell);
			}
		}

		return cells;
	}

	void ListPicker::SetPage(int32_t newPage)
	{
		page = std::max(0, std::min(newPage, pages - 1));

		int32_t const perPage = rows * columns;

		if (highlighted / perPage != page)
		{
			highlighted = std::min(page * perPage, (int32_t)choices.size() - 1);
		}
	}

	ListPickerHit ListPicker::Press(int32_t x, int32_t y, int32_t *choice)
	{
		if (!open)
		{
			return ListPickerHit::Nothing;
		}

		if (Inside(prev, x, y))
		{
			SetPage(page - 1);
			return ListPickerHit::Prev;
		}

		if (Inside(next, x, y))
		{
			SetPage(page + 1);
			return ListPickerHit::Next;
		}

		if (Inside(field, x, y))
		{
			return ListPickerHit::Text;
		}

		int32_t const over = Over(x, y);

		if (over >= 0)
		{
			if (choice)
			{
				*choice = over;
			}

			highlighted = over;
			return ListPickerHit::Choice;
		}

		return ListPickerHit::Nothing;
	}

	int32_t ListPicker::Over(int32_t x, int32_t y) const
	{
		for (ListPickerCell const &cell : Cells())
		{
			if (Inside(cell.box, x, y))
			{
				return cell.choice;
			}
		}

		return -1;
	}

	bool ListPicker::Move(Key key)
	{
		if (!open || choices.empty())
		{
			return false;
		}

		int32_t const count = (int32_t)choices.size();
		int32_t const perPage = rows * columns;
		int32_t at = highlighted;

		switch (key)
		{
		case Key::Up: at = at - 1; break;
		case Key::Down: at = at + 1; break;
		case Key::Left: at = at - rows; break;
		case Key::Right: at = at + rows; break;
		case Key::PageUp: at = at - perPage; break;
		case Key::PageDown: at = at + perPage; break;
		case Key::Home: at = 0; break;
		case Key::End: at = count - 1; break;
		}

		highlighted = std::max(0, std::min(at, count - 1));
		page = highlighted / perPage;
		keyed = true;
		return true;
	}
}
