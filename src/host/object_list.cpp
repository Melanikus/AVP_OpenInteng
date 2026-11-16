// See object_list.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "object_list.h"

#include <string.h>

// AvPx's own safe text (avpx/3dc/avp/win95/frontend): one rule for every text from outside
#include "online_screens.h"

namespace host
{
	static std::string const kNoRow;

	ObjectList::ObjectList() : cut(false), rowHeight(0)
	{
		ui::Box const none = { 0, 0, 0, 0 };

		box = none;
	}

	void ObjectList::Load(const LevelObject *objects, size_t count)
	{
		rows.clear();
		cut = objects && count > kObjectListMost;

		size_t const kept = objects ? (cut ? kObjectListMost : count) : 0;

		rows.reserve(kept);

		for (size_t i = 0; i < kept; i++)
		{
			// Read to its first 0, and to kObjectNameMost bytes at the most, every byte outside the font's letters a '?'
			char safe[kObjectNameMost + 1];
			Row row;

			if (objects[i].name)
			{
				OnlineScreens_SafeName(objects[i].name, kObjectNameMost, safe, sizeof(safe));
			}
			else
			{
				safe[0] = 0;
			}

			row.name = safe;
			memcpy(row.id, objects[i].id, kObjectIdBytes);
			rows.push_back(row);
		}

		// The list starts again: its first row chosen and shown
		list = ui::ScrollList();
		list.SetRows((int32_t)rows.size(), Shown() > 0 ? Shown() : 1);
	}

	const std::string &ObjectList::Name(size_t row) const
	{
		return row < rows.size() ? rows[row].name : kNoRow;
	}

	const uint8_t *ObjectList::Id(size_t row) const
	{
		return row < rows.size() ? rows[row].id : 0;
	}

	int32_t ObjectList::FindById(const uint8_t *id) const
	{
		if (!id)
		{
			return -1;
		}

		for (size_t r = 0; r < rows.size(); r++)
		{
			if (memcmp(rows[r].id, id, kObjectIdBytes) == 0)
			{
				return (int32_t)r;
			}
		}

		return -1;
	}

	// How many whole rows fit in the box: none when it has no room, or rows have no height
	static int32_t RowsThatFit(const ui::Box &box, int32_t rowHeight)
	{
		return rowHeight > 0 && box.height > 0 && box.width > 0 ? box.height / rowHeight : 0;
	}

	void ObjectList::LayOut(const ui::Box &newBox, int32_t newRowHeight)
	{
		box = newBox;
		rowHeight = newRowHeight;

		// The scroll list shows at least one row; the plan draws only the rows that truly fit
		int32_t const fit = RowsThatFit(box, rowHeight);

		list.SetRows((int32_t)rows.size(), fit > 0 ? fit : 1);
	}

	int32_t ObjectList::Shown() const
	{
		int32_t const fit = RowsThatFit(box, rowHeight);
		int64_t const left = (int64_t)rows.size() - list.First();
		int64_t const shown = fit < left ? fit : left;

		return shown > 0 ? (int32_t)shown : 0;
	}

	bool ObjectList::Handle(const ui::Event &event)
	{
		return list.Handle(event) == ui::ListAnswer::Moved;
	}

	bool ObjectList::Press(int32_t x, int32_t y, bool *onRow)
	{
		*onRow = false;

		if (!ui::BoxHolds(box, x, y))
		{
			return false;
		}

		// Which shown row the press is on, by whole rows from the box's top; a press below the last row chooses nothing
		int64_t const row = rowHeight > 0 ? ((int64_t)y - box.y) / rowHeight : 0;

		if (rowHeight > 0 && row < Shown())
		{
			list.Click((int32_t)(list.First() + row));
			*onRow = true;
		}

		return true;
	}

	void ObjectList::Choose(int32_t row)
	{
		if (row >= 0 && (size_t)row < rows.size())
		{
			list.Click(row);
		}
	}

	void ObjectList::Plan(PaintLetterWidth widthOf, int32_t lineHeight, uint32_t chosenColour, PaintText *lines, size_t mostLines,
		size_t *lineCount, PaintBox *boxes, size_t mostBoxes, size_t *boxCount) const
	{
		*lineCount = 0;
		*boxCount = 0;

		if (!lines || !boxes)
		{
			return;
		}

		int32_t const shown = Shown();

		for (int32_t r = 0; r < shown; r++)
		{
			int32_t const row = list.First() + r;
			ui::Box const rowBox = { box.x, (int32_t)((int64_t)box.y + (int64_t)r * rowHeight), box.width, rowHeight };

			if (row == list.Chosen() && *boxCount < mostBoxes)
			{
				PaintBox const band = { rowBox.x, rowBox.y, rowBox.width, rowBox.height, chosenColour };

				boxes[*boxCount] = band;
				(*boxCount)++;
			}

			if (*lineCount < mostLines && PanelPaint_FitText(rows[(size_t)row].name, rowBox, widthOf, lineHeight, &lines[*lineCount]))
			{
				(*lineCount)++;
			}
		}
	}
}
