// Which widget has keyboard focus, and the order Tab moves it in. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES. The panels add their widgets in the order Tab should visit them: the panels in order, and each panel's widgets in order.
// - Tab moves to the next widget that can take focus, and Shift+Tab to the one before. Both go round from the end to the start.
// - A widget that's hidden or disabled can't take focus, and is passed over.
// - While any widget can take focus, one has it. When the one that has it can't any more, focus moves on to the next that can. When
//   none had it and one can, the first that can takes it.
// - A click gives focus only to a widget that can take it.
// Only the widget with focus is given typed letters, so this decides where every letter goes.

#pragma once

#include <stddef.h>

namespace ui
{
	// The most widgets one ring holds. Adding one more is refused, never written past the end.
	size_t const kFocusMostWidgets = 512;

	class FocusRing
	{
	public:
		FocusRing();

		// Adds a widget at the end of the order. Gives back its place in the ring, or -1 when the ring is full.
		int Add(bool canTakeFocus);

		// A widget shown or hidden, enabled or disabled. A place that isn't in the ring is left alone.
		void SetCanTakeFocus(int place, bool canTakeFocus);

		// The place of the widget with focus, or -1 when none can take it
		int Focused() const { return focused; }

		// Tab and Shift+Tab. Each gives back whether focus moved.
		bool Next();
		bool Previous();

		// A click on a widget: it takes focus if it can. Gives back whether it did.
		bool FocusOn(int place);

		int Count() const { return (int)count; }
		bool CanTakeFocus(int place) const { return place >= 0 && (size_t)place < count && can[place]; }

	private:
		// The first place from start, going one way, that can take focus, or -1
		int Find(int start, int step) const;

		bool can[kFocusMostWidgets];
		size_t count;
		int focused;
	};
}
