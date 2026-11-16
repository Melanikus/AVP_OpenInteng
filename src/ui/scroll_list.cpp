// See scroll_list.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "scroll_list.h"

namespace ui
{
	ScrollList::ScrollList() : count(0), shown(1), chosen(-1), first(0)
	{
	}

	// Chooses a row, brought inside the list, and brings it into sight. The sums are done in 64 bits, so a move of a screenful from near
	// the largest count can't wrap round.
	void ScrollList::Choose(int64_t row)
	{
		if (count == 0)
		{
			chosen = -1;
			first = 0;
			return;
		}

		int64_t const last = (int64_t)count - 1;
		int64_t const to = row < 0 ? 0 : (row > last ? last : row);

		chosen = (int32_t)to;

		// The first row shown is never so late that the last screenful has empty rows, nor so early or late that the chosen one is out of sight
		int64_t const latestFirst = (int64_t)count > shown ? (int64_t)count - shown : 0;
		int64_t start = first;

		if (to < start)
		{
			start = to;
		}
		else if (to >= start + shown)
		{
			start = to - shown + 1;
		}

		start = start < 0 ? 0 : (start > latestFirst ? latestFirst : start);
		first = (int32_t)start;
	}

	void ScrollList::SetRows(int32_t newCount, int32_t newShown)
	{
		count = newCount < 0 ? 0 : newCount;
		shown = newShown < 1 ? 1 : newShown;

		if (count == 0)
		{
			chosen = -1;
			first = 0;
			return;
		}

		Choose(chosen < 0 ? 0 : chosen);
	}

	ListAnswer ScrollList::Handle(const Event &event)
	{
		if (count == 0)
		{
			return ListAnswer::Nothing;
		}

		int64_t to = chosen;

		switch (event.key)
		{
			case Key::Up: to = (int64_t)chosen - 1; break;
			case Key::Down: to = (int64_t)chosen + 1; break;
			case Key::PageUp: to = (int64_t)chosen - shown; break;
			case Key::PageDown: to = (int64_t)chosen + shown; break;
			case Key::Home: to = 0; break;
			case Key::End: to = (int64_t)count - 1; break;
			default: return ListAnswer::Nothing;
		}

		int32_t const before = chosen;

		Choose(to);
		return chosen != before ? ListAnswer::Moved : ListAnswer::Nothing;
	}

	bool ScrollList::Click(int32_t row)
	{
		if (row < 0 || row >= count)
		{
			return false;
		}

		Choose(row);
		return true;
	}
}
