// The scroll list's model: which row is chosen, and which rows show, in a list such as the level's objects. Written 2026-11-14 by
// Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES.
// - It holds no copy of the rows. It only knows how many there are, how many show at once, which is chosen and which shows first. The
//   view asks the panel for each shown row's text when it draws, and a row's text goes through the same safe copy as any other text. So a
//   list of 100,000 objects costs nothing until a row shows.
// - With no rows, none is chosen. Otherwise the chosen row is a real one, and it always shows: a move that takes it out of sight brings
//   the rows it's on into sight.
// - Up and Down move one row, Page Up and Page Down a screenful, and Home and End to the ends. None goes past an end.
// - A click chooses the row clicked, if there is one.

#pragma once

#include <stdint.h>

#include "ui_event.h"

namespace ui
{
	enum class ListAnswer
	{
		Nothing,   // the event means nothing here, or the chosen row didn't change
		Moved      // another row is chosen
	};

	class ScrollList
	{
	public:
		ScrollList();

		// How many rows there are, and how many show at once. A count below 0 is taken as 0, and fewer than one row shown as one. The
		// chosen row stays chosen if it's still there; otherwise the last row is.
		void SetRows(int32_t count, int32_t shown);

		ListAnswer Handle(const Event &event);

		// A click on row (counted from the list's first row, not the first shown). Gives back whether it chose one.
		bool Click(int32_t row);

		int32_t Count() const { return count; }
		int32_t RowsShown() const { return shown; }
		int32_t Chosen() const { return chosen; }    // -1 when there are no rows
		int32_t First() const { return first; }      // the first row shown

	private:
		void Choose(int64_t row);

		int32_t count;
		int32_t shown;
		int32_t chosen;
		int32_t first;
	};
}
