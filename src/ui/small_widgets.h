// The checkbox and the button: the two widgets with the least in them. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE CHECKBOX is one bit of a flags word from a level, such as "starts open" in a door's flags. It changes that one bit and no other: a
// word from a level can hold bits the editor has no name for, or a later game version's, and those are kept exactly as they were.
//
// THE BUTTON does one thing when it's pressed, and nothing while it's disabled.

#pragma once

#include <stdint.h>

#include "ui_event.h"

namespace ui
{
	// What one event did to a checkbox or a button
	enum class SmallAnswer
	{
		Nothing,    // the event means nothing here
		Changed,    // the checkbox's bit was turned over
		Pressed,    // the button was pressed: the panel does what it's for
		Refused     // the widget is disabled, or has no value, and nothing changed
	};

	class BitBox
	{
	public:
		BitBox();

		// Loads a flags word from a level, and which bit of it this box shows. Gives back false, and leaves the box with no value, unless
		// bit has exactly one bit set: a box is never two bits, or none.
		bool Load(uint32_t word, uint32_t bit);

		// Space turns the bit over, as a click does (Click). Every other event means nothing to it.
		SmallAnswer Handle(const Event &event);
		SmallAnswer Click();

		void SetEnabled(bool on) { enabled = on; }

		bool Loaded() const { return bit != 0; }
		bool Checked() const { return (word & bit) != 0; }

		// The whole word as it will be saved: the level's own, with only this box's bit as the box says
		uint32_t Word() const { return word; }

	private:
		SmallAnswer TurnOver();

		uint32_t word;
		uint32_t bit;    // 0 when there's no value
		bool enabled;
	};

	class Button
	{
	public:
		Button() : enabled(true) {}

		// Enter or Space presses it, as a click does. Every other event means nothing to it.
		SmallAnswer Handle(const Event &event);
		SmallAnswer Click() { return enabled ? SmallAnswer::Pressed : SmallAnswer::Refused; }

		void SetEnabled(bool on) { enabled = on; }
		bool Enabled() const { return enabled; }

	private:
		bool enabled;
	};
}
