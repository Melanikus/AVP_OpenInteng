// The enum dropdown's model: a field that takes one of a fixed list of values, such as a light's type or a sound's kind. Written
// 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES.
// - The list and its words come from the field spec, written in the editor's own code, never from a level.
// - A level can hold a value the list doesn't have: a later version's, or a damaged file's. That value is kept exactly as it was, and
//   shown as "unknown (N)", until the mapper picks another. The box never changes a value by itself.
// - Enter or Space opens the list. While it's open, Up, Down, Home, End, Page Up and Page Down move which choice is lit, never past either
//   end; Enter or Space picks it, and Esc closes the list with nothing changed. While it's closed, Up and Down change nothing, so moving
//   through the panel can't change a value by chance.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ui_event.h"

namespace ui
{
	// The most choices one box holds
	size_t const kChoiceMost = 256;

	// One choice: the value stored in the level, and the words shown for it
	struct Choice
	{
		int32_t value;
		const char *words;
	};

	// Whether a list of choices can be a box's: there is one, it has 1 to kChoiceMost choices, each has words, and no two have the same
	// value (a pick would be ambiguous). ChoiceBox::Load and the field list's check (field_descriptor.h) both ask this, so there's one rule.
	bool ChoicesAreSound(const Choice *choices, size_t count);

	enum class ChoiceAnswer
	{
		Nothing,   // the event means nothing here
		Opened,    // the list opened
		Moved,     // a different choice is lit
		Picked,    // the lit choice is now the value, and the list closed
		Closed,    // the list closed, with nothing changed
		Refused    // the box has no value, and nothing changed
	};

	class ChoiceBox
	{
	public:
		ChoiceBox();

		// Loads the field's list and the value the level holds. Gives back false, and leaves the box with no value, for a list
		// ChoicesAreSound refuses.
		// choices has to stay there while the box is used: it's the field spec's own table.
		bool Load(const Choice *choices, size_t count, int32_t stored);

		ChoiceAnswer Handle(const Event &event);

		// Focus has moved away: an open list closes, with nothing changed
		ChoiceAnswer Leave();

		bool Loaded() const { return choices != 0; }
		bool Open() const { return open; }

		// The value as it will be saved: the level's own, until a pick
		int32_t Value() const { return value; }

		// Which choice the value is, or -1 when the list doesn't have it
		int Chosen() const { return chosen; }

		// Which choice is lit while the list is open
		int Lit() const { return lit; }

		// The words for the value, or "unknown (N)". out always ends with a zero. An outSize of 0 writes nothing.
		void Shown(char *out, size_t outSize) const;

	private:
		const Choice *choices;   // 0 when there's no value
		size_t count;
		int32_t value;
		int chosen;
		int lit;
		bool open;
	};
}
