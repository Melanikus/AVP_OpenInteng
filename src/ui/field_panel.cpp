// See field_panel.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "field_panel.h"

#include <string.h>

// AvPx's own safe text, which every word the panel shows passes through (avpx/3dc/avp/win95/frontend)
#include "online_screens.h"

namespace ui
{
	const char kPanelSpecGaveNoReason[] = "the field's rules don't allow this value";
	const char kPanelReadOnly[] = "this field is shown, but can't be changed here";
	const char kPanelNotLoaded[] = "this value couldn't be read from the level, so it's kept as it is and can't be changed";
	const char kPanelPrefixNotLoaded[] = "the name the game puts in front of this one couldn't be read, so this one can't be checked";
	const char kPanelFull[] = "this field is full";
	const char kPanelUseADot[] = "use a dot: 0.5";
	const char kPanelNotANumberLetter[] = "a number has only digits, one dot, and a minus in front where it can go below 0";
	const char kPanelTooLong[] = "that's as long as this field's numbers can be";
	const char kPanelNotANumber[] = "that isn't a number";
	const char kPanelTooManyDecimals[] = "that has too many digits after the dot";
	const char kPanelOutOfRange[] = "that's outside this field's range";
	const char kPanelNotAFontLetter[] = "the editor's font has no such letter";
	const char kPanelNotAllowedHere[] = "this field doesn't take that letter";
	const char kPanelBadPaste[] = "that can't be pasted here: it's too long, or has a letter this field doesn't take";
	const char kPanelPasteOneLine[] = "paste one line";
	const char kPanelHasABadLetter[] = "the name still has a letter this field doesn't take";
	const char kPanelCantTypeSome[] = "this name has characters the editor can't type; your edit replaces them";

	FieldValue EmptyFieldValue()
	{
		FieldValue value;

		value.number = 0.0;
		value.word = 0;
		value.choice = 0;
		value.bytes = 0;
		value.length = 0;
		return value;
	}

	// A double's bits, so a value is compared exactly, a NaN included, never by a comparison /fp:fast doesn't promise
	static uint64_t BitsOf(double value)
	{
		uint64_t bits;

		memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	// Words shown through AvPx's safe text, read to at most kFieldHelpMost bytes. No words write an empty line.
	static void ShowWords(const char *words, char *out, size_t outSize)
	{
		if (outSize == 0)
		{
			return;
		}

		if (!words)
		{
			out[0] = 0;
			return;
		}

		OnlineScreens_SafeName(words, kFieldHelpMost, out, outSize);
	}

	FieldPanel::FieldPanel() : loaded(false), list(0), count(0), status(0), statusIsTheSpecs(false)
	{
	}

	void FieldPanel::Unload()
	{
		loaded = false;
		list = 0;
		count = 0;
		slots.clear();
		ring = FocusRing();
		Quiet();
	}

	bool FieldPanel::Load(const FieldDescriptor *newList, size_t newCount, const FieldValue *values, FieldListProblem *problem)
	{
		Unload();

		if (!CheckFieldList(newList, newCount, problem) || (newCount > 0 && !values))
		{
			return false;
		}

		// Made once, here, and never resized while the panel is used, so each slot stays where it is: a text box's check is given its
		// slot's place in memory
		slots.resize(newCount);
		list = newCount > 0 ? newList : 0;
		count = newCount;
		loaded = true;

		for (size_t i = 0; i < count; i++)
		{
			slots[i].panel = this;
			slots[i].place = i;
			LoadSlot(slots[i], values[i]);
			ring.Add(true);
		}

		return true;
	}

	void FieldPanel::LoadSlot(Slot &slot, const FieldValue &value)
	{
		const FieldDescriptor &field = list[slot.place];

		slot.loaded = false;
		slot.levelNumberBits = 0;
		slot.levelWord = 0;
		slot.levelChoice = 0;
		slot.levelTextLength = 0;

		switch (field.kind)
		{
			case FieldKind::Number:
				slot.loaded = slot.number.Load(value.number, NumberRuleFor(field.lowest, field.highest, field.decimals), field.numberCheck,
					field.checkContext);
				slot.levelNumberBits = BitsOf(value.number);
				break;

			case FieldKind::Bit:
				slot.loaded = slot.bit.Load(value.word, field.bit);
				slot.levelWord = value.word;
				break;

			case FieldKind::Choice:
				slot.loaded = slot.choice.Load(field.choices, field.choiceCount, value.choice);
				slot.levelChoice = value.choice;
				break;

			case FieldKind::Text:
				slot.loaded = slot.text.Load(value.bytes, value.length, field.capacity, field.letters, TextKeep, &slot);

				if (slot.loaded)
				{
					slot.levelTextLength = slot.text.ValueLength();
					memcpy(slot.levelText, slot.text.Value(), slot.levelTextLength);
				}
				break;
		}
	}

	bool FieldPanel::TextAllowed(const FieldDescriptor &field, const unsigned char *bytes, size_t length, const char **why) const
	{
		*why = 0;

		if (field.required && length == 0)
		{
			*why = field.requiredWhy;
			return false;
		}

		if (!field.textCheck)
		{
			return true;
		}

		const unsigned char *prefix = 0;
		size_t prefixLength = 0;

		if (field.prefixField != -1)
		{
			const Slot &before = slots[(size_t)field.prefixField];

			if (!before.loaded)
			{
				*why = kPanelPrefixNotLoaded;
				return false;
			}

			prefix = before.text.Value();
			prefixLength = before.text.ValueLength();
		}

		const char *said = 0;

		if (!field.textCheck(field.checkContext, bytes, length, prefix, prefixLength, &said))
		{
			*why = said;
			return false;
		}

		return true;
	}

	bool FieldPanel::TextKeep(const void *context, const unsigned char *bytes, size_t length, const char **why)
	{
		const Slot &slot = *(const Slot *)context;

		return slot.panel->TextAllowed(slot.panel->list[slot.place], bytes, length, why);
	}

	// The status line says nothing until the next refusal or notice
	void FieldPanel::Quiet()
	{
		status = 0;
		statusIsTheSpecs = false;
	}

	PanelAnswer FieldPanel::Say(const char *words, bool theSpecs, PanelAnswer answer)
	{
		status = words;
		statusIsTheSpecs = theSpecs;
		return answer;
	}

	// The spec's own words, or the panel's when it gave none: a refusal is shown either way
	PanelAnswer FieldPanel::SpecSays(const char *why, PanelAnswer answer)
	{
		return why ? Say(why, true, answer) : Say(kPanelSpecGaveNoReason, false, answer);
	}

	PanelAnswer FieldPanel::FromNumber(const FieldDescriptor &field, const Slot &slot, NumberAnswer answer)
	{
		switch (answer)
		{
			case NumberAnswer::Nothing: return PanelAnswer::Nothing;
			case NumberAnswer::Moved: return PanelAnswer::Moved;
			case NumberAnswer::Changed: return PanelAnswer::Changed;
			case NumberAnswer::Kept: return PanelAnswer::Kept;
			default: break;
		}

		// Refused, or given up: the reason, if there's one (Esc gives up with none)
		PanelAnswer const result = answer == NumberAnswer::GaveUp ? PanelAnswer::GaveUp : PanelAnswer::Refused;

		switch (slot.number.Refusal())
		{
			case NumberRefusal::SpecRefused: return SpecSays(slot.number.SpecWhy(), result);
			case NumberRefusal::OutOfRange: return field.help ? Say(field.help, true, result) : Say(kPanelOutOfRange, false, result);
			case NumberRefusal::UseADot: return Say(kPanelUseADot, false, result);
			case NumberRefusal::NotANumberLetter: return Say(kPanelNotANumberLetter, false, result);
			case NumberRefusal::TooLong: return Say(kPanelTooLong, false, result);
			case NumberRefusal::NotANumber: return Say(kPanelNotANumber, false, result);
			case NumberRefusal::TooManyDecimals: return Say(kPanelTooManyDecimals, false, result);
			case NumberRefusal::BadPaste: return Say(kPanelBadPaste, false, result);
			default: return result;   // a number box is always loaded here: CheckFieldList refused any range it couldn't take
		}
	}

	PanelAnswer FieldPanel::FromText(const FieldDescriptor &field, const Slot &slot, TextAnswer answer)
	{
		switch (answer)
		{
			case TextAnswer::Nothing: return PanelAnswer::Nothing;
			case TextAnswer::Moved: return PanelAnswer::Moved;
			case TextAnswer::Kept: return PanelAnswer::Kept;
			case TextAnswer::Changed:
				// Rule 6: the moment an edit starts on a name with letters the editor can't type, the mapper is told
				return slot.text.ShowsCantTypeNotice() ? Say(kPanelCantTypeSome, false, PanelAnswer::Changed) : PanelAnswer::Changed;
			default: break;
		}

		PanelAnswer const result = answer == TextAnswer::GaveUp ? PanelAnswer::GaveUp : PanelAnswer::Refused;
		const char *const capacityWords = field.capacityWhy ? field.capacityWhy : kPanelFull;

		switch (slot.text.Refusal())
		{
			case TextRefusal::SpecRefused:
				// The words in front can't be the spec's when the name in front couldn't be read
				return slot.text.SpecWhy() == kPanelPrefixNotLoaded ? Say(kPanelPrefixNotLoaded, false, result) : SpecSays(slot.text.SpecWhy(), result);
			case TextRefusal::Full: return Say(capacityWords, field.capacityWhy != 0, result);
			case TextRefusal::PasteTooLong: return field.capacityWhy ? Say(field.capacityWhy, true, result) : Say(kPanelBadPaste, false, result);
			case TextRefusal::NotAFontLetter: return Say(kPanelNotAFontLetter, false, result);
			case TextRefusal::NotAllowedHere: return Say(kPanelNotAllowedHere, false, result);
			case TextRefusal::PasteHasALineBreak: return Say(kPanelPasteOneLine, false, result);
			case TextRefusal::PasteHasANonLetter: return Say(kPanelBadPaste, false, result);
			case TextRefusal::NotKeptHasABadLetter: return Say(kPanelHasABadLetter, false, result);
			case TextRefusal::NotLoaded: return Say(kPanelNotLoaded, false, result);
			default: return result;
		}
	}

	// The field with focus keeps its edit if it can, and gives it up if it can't, with the reason on the status line. A read-only field,
	// or one that couldn't be loaded, is never being edited, so its box has nothing to keep.
	void FieldPanel::LeaveFocused()
	{
		int const focused = ring.Focused();

		if (focused < 0)
		{
			return;
		}

		Slot &slot = slots[(size_t)focused];
		const FieldDescriptor &field = list[(size_t)focused];

		switch (field.kind)
		{
			case FieldKind::Number: FromNumber(field, slot, slot.number.Leave()); break;
			case FieldKind::Text: FromText(field, slot, slot.text.Leave()); break;
			case FieldKind::Choice: slot.choice.Leave(); break;
			default: break;
		}
	}

	PanelAnswer FieldPanel::Handle(const Event &event)
	{
		Quiet();

		if (!loaded)
		{
			return PanelAnswer::Refused;
		}

		if (event.key == Key::Tab && !event.ctrl)
		{
			int const before = ring.Focused();

			LeaveFocused();

			bool const moved = event.shift ? ring.Previous() : ring.Next();

			return moved && ring.Focused() != before ? PanelAnswer::FocusMoved : PanelAnswer::Nothing;
		}

		int const focused = ring.Focused();

		if (focused < 0)
		{
			return PanelAnswer::Nothing;
		}

		Slot &slot = slots[(size_t)focused];
		const FieldDescriptor &field = list[(size_t)focused];

		// A field that couldn't be loaded needs no check here: its box refuses every event itself, saying so
		if (field.readOnly)
		{
			return Say(kPanelReadOnly, false, PanelAnswer::Refused);
		}

		switch (field.kind)
		{
			case FieldKind::Number:
				return FromNumber(field, slot, slot.number.Handle(event));

			case FieldKind::Text:
				return FromText(field, slot, slot.text.Handle(event));

			case FieldKind::Bit:
				switch (slot.bit.Handle(event))
				{
					case SmallAnswer::Changed: return PanelAnswer::Kept;
					case SmallAnswer::Refused: return PanelAnswer::Refused;
					default: return PanelAnswer::Nothing;
				}

			case FieldKind::Choice:
				switch (slot.choice.Handle(event))
				{
					case ChoiceAnswer::Opened: return PanelAnswer::Moved;
					case ChoiceAnswer::Moved: return PanelAnswer::Moved;
					case ChoiceAnswer::Closed: return PanelAnswer::Moved;
					case ChoiceAnswer::Picked: return PanelAnswer::Kept;
					case ChoiceAnswer::Refused: return PanelAnswer::Refused;
					default: return PanelAnswer::Nothing;
				}
		}

		return PanelAnswer::Nothing;
	}

	PanelAnswer FieldPanel::Paste(const unsigned char *bytes, size_t length)
	{
		Quiet();

		if (!loaded)
		{
			return PanelAnswer::Refused;
		}

		int const focused = ring.Focused();

		if (focused < 0)
		{
			return PanelAnswer::Nothing;
		}

		Slot &slot = slots[(size_t)focused];
		const FieldDescriptor &field = list[(size_t)focused];

		if (field.readOnly)
		{
			return Say(kPanelReadOnly, false, PanelAnswer::Refused);
		}

		switch (field.kind)
		{
			case FieldKind::Number: return FromNumber(field, slot, slot.number.Paste(bytes, length));
			case FieldKind::Text: return FromText(field, slot, slot.text.Paste(bytes, length));
			default: return PanelAnswer::Nothing;
		}
	}

	PanelAnswer FieldPanel::ClickField(size_t place)
	{
		Quiet();

		// A click past the end, or on the field that has focus, changes nothing: the edit there goes on. (Nothing loaded counts 0 fields.)
		if (place >= count || (int)place == ring.Focused())
		{
			return PanelAnswer::Nothing;
		}

		LeaveFocused();
		return ring.FocusOn((int)place) ? PanelAnswer::FocusMoved : PanelAnswer::Nothing;
	}

	FieldFlag FieldPanel::FlagAndWhy(size_t place, const char **why) const
	{
		*why = 0;

		if (place >= count)
		{
			return FieldFlag::None;
		}

		const Slot &slot = slots[place];
		const FieldDescriptor &field = list[place];

		if (!slot.loaded)
		{
			*why = kPanelNotLoaded;
			return FieldFlag::NotLoaded;
		}

		switch (field.kind)
		{
			case FieldKind::Number:
			{
				if (slot.number.OutOfRangeNow())
				{
					*why = field.help;
					return FieldFlag::OutOfRange;
				}

				const char *said = 0;

				if (field.numberCheck && !field.numberCheck(field.checkContext, slot.number.Value(), &said))
				{
					*why = said;
					return FieldFlag::SpecRefusesIt;
				}

				return FieldFlag::None;
			}

			case FieldKind::Choice:
				return slot.choice.Chosen() < 0 ? FieldFlag::UnknownChoice : FieldFlag::None;

			case FieldKind::Text:
			{
				const char *said = 0;

				if (TextAllowed(field, slot.text.Value(), slot.text.ValueLength(), &said))
				{
					return FieldFlag::None;
				}

				*why = said;

				if (said == kPanelPrefixNotLoaded)
				{
					return FieldFlag::PrefixNotLoaded;
				}

				return field.required && slot.text.ValueLength() == 0 ? FieldFlag::EmptyButRequired : FieldFlag::SpecRefusesIt;
			}

			default:
				return FieldFlag::None;
		}
	}

	FieldFlag FieldPanel::Flag(size_t place) const
	{
		const char *why = 0;

		return FlagAndWhy(place, &why);
	}

	const char *FieldPanel::FlagWhy(size_t place) const
	{
		const char *why = 0;

		FlagAndWhy(place, &why);
		return why;
	}

	// A field that couldn't be loaded holds nothing, and so does its copy of the level's value, so it's never changed
	bool FieldPanel::Changed(size_t place) const
	{
		if (place >= count)
		{
			return false;
		}

		const Slot &slot = slots[place];

		switch (list[place].kind)
		{
			case FieldKind::Number: return BitsOf(slot.number.Value()) != slot.levelNumberBits;
			case FieldKind::Bit: return slot.bit.Word() != slot.levelWord;
			case FieldKind::Choice: return slot.choice.Value() != slot.levelChoice;
			case FieldKind::Text:
				return slot.text.ValueLength() != slot.levelTextLength || memcmp(slot.text.Value(), slot.levelText, slot.levelTextLength) != 0;
			default: return false;
		}
	}

	// Each slot has a box of every kind, and only its own kind's is ever loaded. The others hold nothing, 0, so asking a field for another
	// kind's number, word or choice gives 0 without asking its kind. A text box's value is storage, never null, so a text is asked for by
	// its kind, and a text that couldn't be loaded gives none.
	double FieldPanel::NumberValue(size_t place) const
	{
		return place < count ? slots[place].number.Value() : 0.0;
	}

	uint32_t FieldPanel::BitWord(size_t place) const
	{
		return place < count ? slots[place].bit.Word() : 0;
	}

	bool FieldPanel::BitChecked(size_t place) const
	{
		return place < count && slots[place].bit.Checked();
	}

	int32_t FieldPanel::ChoiceValue(size_t place) const
	{
		return place < count ? slots[place].choice.Value() : 0;
	}

	const unsigned char *FieldPanel::TextValue(size_t place, size_t *length) const
	{
		bool const there = place < count && list[place].kind == FieldKind::Text && slots[place].loaded;

		if (length)
		{
			*length = there ? slots[place].text.ValueLength() : 0;
		}

		return there ? slots[place].text.Value() : 0;
	}

	void FieldPanel::Shown(size_t place, char *out, size_t outSize) const
	{
		if (outSize == 0)
		{
			return;
		}

		// A bit shows no text (the view draws its box), and a box that couldn't be loaded shows nothing
		out[0] = 0;

		if (place >= count)
		{
			return;
		}

		switch (list[place].kind)
		{
			case FieldKind::Number: slots[place].number.Shown(out, outSize); break;
			case FieldKind::Choice: slots[place].choice.Shown(out, outSize); break;
			case FieldKind::Text: slots[place].text.Shown(out, outSize); break;
			default: break;
		}
	}

	void FieldPanel::HelpRow(char *out, size_t outSize) const
	{
		int const focused = ring.Focused();

		ShowWords(focused >= 0 ? list[(size_t)focused].help : 0, out, outSize);
	}

	void FieldPanel::StatusLine(char *out, size_t outSize) const
	{
		ShowWords(status, out, outSize);
	}
}
