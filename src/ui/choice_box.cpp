// See choice_box.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "choice_box.h"

#include <stdio.h>

// AvPx's own safe text, which the words pass through like any other text shown (avpx/3dc/avp/win95/frontend)
#include "online_screens.h"

namespace ui
{
	bool ChoicesAreSound(const Choice *choices, size_t count)
	{
		if (!choices || count == 0 || count > kChoiceMost)
		{
			return false;
		}

		for (size_t i = 0; i < count; i++)
		{
			if (!choices[i].words)
			{
				return false;
			}

			for (size_t j = i + 1; j < count; j++)
			{
				if (choices[i].value == choices[j].value)
				{
					return false;
				}
			}
		}

		return true;
	}

	ChoiceBox::ChoiceBox() : choices(0), count(0), value(0), chosen(-1), lit(0), open(false)
	{
	}

	bool ChoiceBox::Load(const Choice *newChoices, size_t newCount, int32_t stored)
	{
		choices = 0;
		count = 0;
		value = 0;
		chosen = -1;
		lit = 0;
		open = false;

		if (!ChoicesAreSound(newChoices, newCount))
		{
			return false;
		}

		choices = newChoices;
		count = newCount;
		value = stored;

		for (size_t i = 0; i < count; i++)
		{
			if (choices[i].value == stored)
			{
				chosen = (int)i;
			}
		}

		return true;
	}

	ChoiceAnswer ChoiceBox::Handle(const Event &event)
	{
		if (!Loaded())
		{
			return ChoiceAnswer::Refused;
		}

		bool const picks = event.key == Key::Enter || event.key == Key::Space || (event.key == Key::Letter && event.letter == ' ' && !event.ctrl);

		if (!open)
		{
			if (!picks)
			{
				return ChoiceAnswer::Nothing;
			}

			open = true;
			lit = chosen >= 0 ? chosen : 0;
			return ChoiceAnswer::Opened;
		}

		if (picks)
		{
			value = choices[lit].value;
			chosen = lit;
			open = false;
			return ChoiceAnswer::Picked;
		}

		if (event.key == Key::Escape)
		{
			open = false;
			return ChoiceAnswer::Closed;
		}

		int const last = (int)count - 1;
		int to = lit;

		switch (event.key)
		{
			case Key::Up: to = lit - 1; break;
			case Key::Down: to = lit + 1; break;
			case Key::PageUp: to = lit - 10; break;
			case Key::PageDown: to = lit + 10; break;
			case Key::Home: to = 0; break;
			case Key::End: to = last; break;
			default: return ChoiceAnswer::Nothing;
		}

		to = to < 0 ? 0 : (to > last ? last : to);

		if (to == lit)
		{
			return ChoiceAnswer::Nothing;
		}

		lit = to;
		return ChoiceAnswer::Moved;
	}

	ChoiceAnswer ChoiceBox::Leave()
	{
		if (!open)
		{
			return ChoiceAnswer::Nothing;
		}

		open = false;
		return ChoiceAnswer::Closed;
	}

	void ChoiceBox::Shown(char *out, size_t outSize) const
	{
		if (!out || outSize == 0)
		{
			return;
		}

		if (chosen >= 0)
		{
			// The words are the field spec's, but they go through the same safe copy as a level's text, so nothing shown can reach past its room
			OnlineScreens_SafeName(choices[chosen].words, outSize, out, outSize);
			return;
		}

		// The format is the editor's own, fixed here, and the value goes in as a number. Nothing from a level is ever a format.
		snprintf(out, outSize, "unknown (%ld)", (long)value);
	}
}
