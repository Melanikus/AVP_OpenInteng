// See text_field.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "text_field.h"

#include <string.h>

// AvPx's own safe text: every byte the game's font has no letter for becomes a question mark (avpx/3dc/avp/win95/frontend). The editor
// uses the game's, so there's one rule for safe text, not two.
#include "online_screens.h"

namespace ui
{
	bool IsFontLetter(unsigned char letter)
	{
		return letter >= 32 && letter <= 126;
	}

	TextField::TextField()
		: valueLength(0), editLength(0), capacity(0), cursor(0), anchor(0), editing(false), valueHasLettersItCantType(false), allowed(0),
		refusal(TextRefusal::None), keepCheck(0), keepContext(0), specWhy(0)
	{
		memset(value, 0, sizeof(value));
		memset(edit, 0, sizeof(edit));
	}

	bool TextField::TakesLetter(unsigned char letter) const
	{
		return IsFontLetter(letter) && (!allowed || allowed(letter));
	}

	bool TextField::Load(const unsigned char *bytes, size_t length, size_t newCapacity, LetterRule rule)
	{
		return Load(bytes, length, newCapacity, rule, 0, 0);
	}

	bool TextField::Load(const unsigned char *bytes, size_t length, size_t newCapacity, LetterRule rule, TextKeepCheck check, const void *context)
	{
		// Whatever was there goes first, so a value that can't be loaded leaves nothing behind that could be saved
		valueLength = 0;
		editLength = 0;
		capacity = 0;
		cursor = 0;
		anchor = 0;
		editing = false;
		valueHasLettersItCantType = false;
		allowed = rule;
		refusal = TextRefusal::None;
		keepCheck = check;
		keepContext = context;
		specWhy = 0;

		if (newCapacity == 0 || newCapacity > kTextMostBytes || (!bytes && length > 0))
		{
			return false;
		}

		// The name is the bytes before its first zero, or all of them. It's found within length, never past it.
		const void *const zero = bytes ? memchr(bytes, 0, length) : 0;
		size_t const nameLength = zero ? (size_t)((const unsigned char *)zero - bytes) : length;

		if (nameLength > newCapacity)
		{
			return false;
		}

		if (nameLength > 0)
		{
			memcpy(value, bytes, nameLength);
		}

		valueLength = nameLength;
		capacity = newCapacity;

		for (size_t i = 0; i < valueLength; i++)
		{
			valueHasLettersItCantType = valueHasLettersItCantType || !TakesLetter(value[i]);
		}

		return true;
	}

	// An edit starts from the value as it's shown: a letter this field can't take becomes a question mark, which the mapper sees and
	// can type over. Esc gives the level's own bytes back.
	void TextField::BeginEditing()
	{
		for (size_t i = 0; i < valueLength; i++)
		{
			edit[i] = TakesLetter(value[i]) ? value[i] : (unsigned char)'?';
		}

		editLength = valueLength;
		cursor = editLength;
		anchor = editLength;
		editing = true;
	}

	void TextField::DeleteSelection()
	{
		size_t const start = SelectionStart();
		size_t const end = SelectionEnd();

		if (start == end)
		{
			return;
		}

		memmove(edit + start, edit + end, editLength - end);
		editLength -= end - start;
		cursor = start;
		anchor = start;
	}

	TextAnswer TextField::Refuse(TextRefusal why)
	{
		refusal = why;
		return TextAnswer::Refused;
	}

	// Keeps the edit as the value. Every letter in it has to be one this field takes: a question mark left where a level's own letter
	// was is kept only if the field takes question marks. Then the spec's check, if the field has one, has the last word: when it says
	// no, nothing is kept, so there's never a value kept and then taken back.
	TextAnswer TextField::Keep()
	{
		specWhy = 0;

		for (size_t i = 0; i < editLength; i++)
		{
			if (!TakesLetter(edit[i]))
			{
				return Refuse(TextRefusal::NotKeptHasABadLetter);
			}
		}

		if (keepCheck)
		{
			const char *said = 0;

			if (!keepCheck(keepContext, edit, editLength, &said))
			{
				specWhy = said;
				return Refuse(TextRefusal::SpecRefused);
			}
		}

		if (editLength > 0)
		{
			memcpy(value, edit, editLength);
		}

		valueLength = editLength;
		valueHasLettersItCantType = false;
		editing = false;
		return TextAnswer::Kept;
	}

	TextAnswer TextField::Handle(const Event &event)
	{
		refusal = TextRefusal::None;

		if (!Loaded())
		{
			return Refuse(TextRefusal::NotLoaded);
		}

		// Keys that belong to others: Tab moves focus, and the lists use the rest
		if (event.key == Key::Tab || event.key == Key::Up || event.key == Key::Down || event.key == Key::PageUp || event.key == Key::PageDown ||
			event.key == Key::None)
		{
			return TextAnswer::Nothing;
		}

		if (event.key == Key::Escape)
		{
			if (!editing)
			{
				return TextAnswer::Nothing;
			}

			editing = false;
			return TextAnswer::GaveUp;
		}

		// A shortcut, never a letter: only Ctrl+A means something here, and it selects everything
		if (event.key == Key::Letter && event.ctrl)
		{
			if (event.letter != 'a' && event.letter != 'A')
			{
				return TextAnswer::Nothing;
			}

			if (!editing)
			{
				BeginEditing();
			}

			anchor = 0;
			cursor = editLength;
			return TextAnswer::Moved;
		}

		if (event.key == Key::Enter)
		{
			return editing ? Keep() : TextAnswer::Nothing;
		}

		if (!editing)
		{
			BeginEditing();
		}

		switch (event.key)
		{
			case Key::Letter:
			case Key::Space:
			{
				unsigned char const letter = event.key == Key::Space ? (unsigned char)' ' : event.letter;

				if (!IsFontLetter(letter))
				{
					return Refuse(TextRefusal::NotAFontLetter);
				}

				if (!TakesLetter(letter))
				{
					return Refuse(TextRefusal::NotAllowedHere);
				}

				// It replaces the selection, so it fits if the text without the selection has room for one more
				if (editLength - (SelectionEnd() - SelectionStart()) >= capacity)
				{
					return Refuse(TextRefusal::Full);
				}

				DeleteSelection();
				memmove(edit + cursor + 1, edit + cursor, editLength - cursor);
				edit[cursor] = letter;
				editLength++;
				cursor++;
				anchor = cursor;
				return TextAnswer::Changed;
			}

			case Key::Backspace:
			case Key::Delete:
			{
				if (SelectionStart() != SelectionEnd())
				{
					DeleteSelection();
					return TextAnswer::Changed;
				}

				bool const back = event.key == Key::Backspace;

				if ((back && cursor == 0) || (!back && cursor == editLength))
				{
					return TextAnswer::Nothing;
				}

				size_t const gone = back ? cursor - 1 : cursor;

				memmove(edit + gone, edit + gone + 1, editLength - gone - 1);
				editLength--;
				cursor = gone;
				anchor = cursor;
				return TextAnswer::Changed;
			}

			case Key::Left:
			case Key::Right:
			case Key::Home:
			case Key::End:
			{
				size_t to = cursor;

				// With a selection and no Shift, Left and Right go to its own end, as most programs do
				if (!event.shift && SelectionStart() != SelectionEnd() && (event.key == Key::Left || event.key == Key::Right))
				{
					to = event.key == Key::Left ? SelectionStart() : SelectionEnd();
				}
				else if (event.key == Key::Left)
				{
					to = cursor > 0 ? cursor - 1 : 0;
				}
				else if (event.key == Key::Right)
				{
					to = cursor < editLength ? cursor + 1 : editLength;
				}
				else
				{
					to = event.key == Key::Home ? 0 : editLength;
				}

				cursor = to;

				if (!event.shift)
				{
					anchor = cursor;
				}

				return TextAnswer::Moved;
			}

			default:
				return TextAnswer::Nothing;
		}
	}

	TextAnswer TextField::Paste(const unsigned char *bytes, size_t length)
	{
		refusal = TextRefusal::None;

		if (!Loaded())
		{
			return Refuse(TextRefusal::NotLoaded);
		}

		if (!bytes || length == 0)
		{
			return TextAnswer::Nothing;
		}

		if (!editing)
		{
			BeginEditing();
		}

		// Whether it fits is asked first, so text far too long is refused before anything reads all of it
		size_t const kept = editLength - (SelectionEnd() - SelectionStart());

		if (length > capacity - kept)
		{
			return Refuse(TextRefusal::PasteTooLong);
		}

		for (size_t i = 0; i < length; i++)
		{
			if (bytes[i] == '\r' || bytes[i] == '\n')
			{
				return Refuse(TextRefusal::PasteHasALineBreak);
			}
		}

		for (size_t i = 0; i < length; i++)
		{
			if (!TakesLetter(bytes[i]))
			{
				return Refuse(TextRefusal::PasteHasANonLetter);
			}
		}

		DeleteSelection();
		memmove(edit + cursor + length, edit + cursor, editLength - cursor);
		memcpy(edit + cursor, bytes, length);
		editLength += length;
		cursor += length;
		anchor = cursor;
		return TextAnswer::Changed;
	}

	TextAnswer TextField::Leave()
	{
		refusal = TextRefusal::None;

		if (!editing)
		{
			return TextAnswer::Nothing;
		}

		if (Keep() == TextAnswer::Kept)
		{
			return TextAnswer::Kept;
		}

		// Keep said why in refusal, which stays for the panel to show
		editing = false;
		return TextAnswer::GaveUp;
	}

	size_t TextField::CopySelection(unsigned char *out, size_t outSize) const
	{
		size_t const start = SelectionStart();
		size_t const end = SelectionEnd();
		size_t const count = end - start < outSize ? end - start : outSize;

		if (!out || count == 0)
		{
			return 0;
		}

		memcpy(out, edit + start, count);
		return count;
	}

	void TextField::Shown(char *out, size_t outSize) const
	{
		const unsigned char *const from = editing ? edit : value;

		OnlineScreens_SafeName((const char *)from, Length(), out, outSize);
	}
}
