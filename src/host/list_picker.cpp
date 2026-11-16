// list_picker.cpp: see list_picker.h (inteng's list chooser dialog, FUN_00475bf0).
#include "list_picker.h"

#include <algorithm>

namespace host
{
	namespace
	{
		int32_t const kRowHeight = 0x14;     // 20 pixels between rows
		int32_t const kButtonHeight = 0x12;
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

		if (columnWidth > screenWidth - 0x28 && screenWidth - 0x28 >= 100)
		{
			columnWidth = screenWidth - 0x28;
		}

		// rows in a column, columns on the screen
		rows = (screenHeight - 0x78) / kRowHeight;

		if (rows > count)
		{
			rows = count;
		}

		// NAMED: the panel pass holds 64 boxes and lines in all, so a page has at most 22 rows (inteng's own rule gives up to 30 on a 480 pixel screen)
		if (rows > 22)
		{
			rows = 22;
		}

		if (rows < 1)
		{
			rows = 1;
		}

		columns = screenWidth / (columnWidth + 0x14);

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

		if (width > screenWidth - 0x14 && screenWidth - 0x14 >= 300)
		{
			width = screenWidth - 0x14;
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
