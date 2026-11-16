// See small_widgets.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "small_widgets.h"

namespace ui
{
	BitBox::BitBox() : word(0), bit(0), enabled(true)
	{
	}

	bool BitBox::Load(uint32_t newWord, uint32_t newBit)
	{
		word = 0;
		bit = 0;

		// Exactly one bit is set when the number isn't 0 and taking 1 from it clears that bit and leaves nothing
		if (newBit == 0 || (newBit & (newBit - 1)) != 0)
		{
			return false;
		}

		word = newWord;
		bit = newBit;
		return true;
	}

	SmallAnswer BitBox::TurnOver()
	{
		if (!Loaded() || !enabled)
		{
			return SmallAnswer::Refused;
		}

		word ^= bit;
		return SmallAnswer::Changed;
	}

	SmallAnswer BitBox::Handle(const Event &event)
	{
		bool const space = event.key == Key::Space || (event.key == Key::Letter && event.letter == ' ' && !event.ctrl);

		return space ? TurnOver() : SmallAnswer::Nothing;
	}

	SmallAnswer BitBox::Click()
	{
		return TurnOver();
	}

	SmallAnswer Button::Handle(const Event &event)
	{
		bool const presses = event.key == Key::Enter || event.key == Key::Space || (event.key == Key::Letter && event.letter == ' ' && !event.ctrl);

		return presses ? Click() : SmallAnswer::Nothing;
	}
}
