// See focus_ring.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "focus_ring.h"

namespace ui
{
	FocusRing::FocusRing() : count(0), focused(-1)
	{
		for (size_t i = 0; i < kFocusMostWidgets; i++)
		{
			can[i] = false;
		}
	}

	// Looks at every place once, starting at start and going round, so it always ends
	int FocusRing::Find(int start, int step) const
	{
		if (count == 0)
		{
			return -1;
		}

		int const n = (int)count;
		int place = ((start % n) + n) % n;

		for (int looked = 0; looked < n; looked++)
		{
			if (can[place])
			{
				return place;
			}

			place = (place + step + n) % n;
		}

		return -1;
	}

	int FocusRing::Add(bool canTakeFocus)
	{
		if (count >= kFocusMostWidgets)
		{
			return -1;
		}

		int const place = (int)count;

		can[count] = canTakeFocus;
		count++;

		if (focused < 0 && canTakeFocus)
		{
			focused = place;
		}

		return place;
	}

	void FocusRing::SetCanTakeFocus(int place, bool canTakeFocus)
	{
		if (place < 0 || (size_t)place >= count)
		{
			return;
		}

		can[place] = canTakeFocus;

		if (place == focused && !canTakeFocus)
		{
			focused = Find(place + 1, 1);
		}
		else if (focused < 0 && canTakeFocus)
		{
			focused = Find(0, 1);
		}
	}

	bool FocusRing::Next()
	{
		int const before = focused;
		int const next = Find(focused < 0 ? 0 : focused + 1, 1);

		focused = next;
		return focused != before;
	}

	bool FocusRing::Previous()
	{
		int const before = focused;
		int const previous = Find(focused < 0 ? (int)count - 1 : focused - 1, -1);

		focused = previous;
		return focused != before;
	}

	bool FocusRing::FocusOn(int place)
	{
		if (!CanTakeFocus(place))
		{
			return false;
		}

		focused = place;
		return true;
	}
}
