// See field_descriptor.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "field_descriptor.h"

#include "number_box.h"

namespace ui
{
	// How many bytes a text has before its zero, reading no more than most + 1 of them: an answer of most + 1 means it's longer than most.
	// It stops at the zero, so it never reads past the end of a text that's shorter.
	static size_t BoundedLength(const char *text, size_t most)
	{
		size_t length = 0;

		while (length <= most && text[length] != 0)
		{
			length++;
		}

		return length;
	}

	// Whether a text of the editor's own is there, and is 1 to most font letters
	static bool IsWords(const char *text, size_t most)
	{
		if (!text)
		{
			return false;
		}

		size_t const length = BoundedLength(text, most);

		if (length == 0 || length > most)
		{
			return false;
		}

		for (size_t i = 0; i < length; i++)
		{
			if (!IsFontLetter((unsigned char)text[i]))
			{
				return false;
			}
		}

		return true;
	}

	// One field of a list, with the rest of the list for its prefix
	static FieldListRefusal CheckOne(const FieldDescriptor *list, size_t count, size_t place)
	{
		const FieldDescriptor &field = list[place];
		bool const number = field.kind == FieldKind::Number;
		bool const bit = field.kind == FieldKind::Bit;
		bool const choice = field.kind == FieldKind::Choice;
		bool const text = field.kind == FieldKind::Text;

		if (!IsWords(field.name, kFieldNameMost))
		{
			return FieldListRefusal::BadName;
		}

		if (field.help && !IsWords(field.help, kFieldHelpMost))
		{
			return FieldListRefusal::BadHelp;
		}

		if (!number && !bit && !choice && !text)
		{
			return FieldListRefusal::NotAKind;
		}

		// Nothing that only another kind has, so a field filled for the wrong kind can't pass for a sound one. (A number's range and
		// decimals are only read for a number, so they're not asked about here.)
		bool const numberParts = field.unit || field.numberCheck;
		bool const bitParts = field.bit != 0;
		bool const choiceParts = field.choices || field.choiceCount != 0;
		bool const textParts = field.capacity != 0 || field.fixed || field.capacityWhy || field.required || field.requiredWhy || field.letters
			|| field.textCheck || field.prefixField != -1;

		if ((!number && numberParts) || (!bit && bitParts) || (!choice && choiceParts) || (!text && textParts))
		{
			return FieldListRefusal::NotItsKind;
		}

		if (number)
		{
			if (!NumberRuleFor(field.lowest, field.highest, field.decimals).good || (field.unit && !IsWords(field.unit, kFieldUnitMost)))
			{
				return FieldListRefusal::BadNumber;
			}
		}

		// Exactly one bit: a word with one bit set has none left when that bit is taken away
		if (bit && (field.bit == 0 || (field.bit & (field.bit - 1)) != 0))
		{
			return FieldListRefusal::BadBit;
		}

		if (choice && !ChoicesAreSound(field.choices, field.choiceCount))
		{
			return FieldListRefusal::BadChoices;
		}

		if (text)
		{
			if (field.capacity == 0 || field.capacity > kTextMostBytes || (field.capacityWhy && !IsWords(field.capacityWhy, kFieldHelpMost)))
			{
				return FieldListRefusal::BadText;
			}

			if (field.required != (field.requiredWhy != 0) || (field.required && !IsWords(field.requiredWhy, kFieldHelpMost)))
			{
				return FieldListRefusal::BadRequired;
			}

			// The game puts one name in front, never a chain of them, and only the spec's check works out whether the two fit together.
			// - A place below 0 turns into a huge number as a size_t, so the one comparison with count refuses it as well as a place
			//   past the end.
			// - A field that names itself has a prefix of its own, so the chain rule refuses it, and so it does two fields naming each
			//   other.
			if (field.prefixField != -1)
			{
				if ((size_t)field.prefixField >= count || !field.textCheck)
				{
					return FieldListRefusal::BadPrefix;
				}

				const FieldDescriptor &prefix = list[field.prefixField];

				if (prefix.kind != FieldKind::Text || prefix.prefixField != -1)
				{
					return FieldListRefusal::BadPrefix;
				}
			}
		}

		return FieldListRefusal::None;
	}

	bool CheckFieldList(const FieldDescriptor *list, size_t count, FieldListProblem *problem)
	{
		FieldListProblem found = { 0, FieldListRefusal::None };

		// The count first, so a huge one is refused before the list is read at all
		if (count > kFieldPanelMost)
		{
			found.why = FieldListRefusal::TooMany;
		}
		else if (count > 0 && !list)
		{
			found.why = FieldListRefusal::NoList;
		}
		else
		{
			for (size_t i = 0; i < count && found.why == FieldListRefusal::None; i++)
			{
				found.field = i;
				found.why = CheckOne(list, count, i);
			}

			if (found.why == FieldListRefusal::None)
			{
				found.field = 0;
			}
		}

		if (problem)
		{
			*problem = found;
		}

		return found.why == FieldListRefusal::None;
	}

	FieldDescriptor EmptyFieldDescriptor()
	{
		FieldDescriptor field;

		field.name = 0;
		field.help = 0;
		field.kind = FieldKind::Number;
		field.readOnly = false;
		field.lowest = 0.0;
		field.highest = 0.0;
		field.decimals = 0;
		field.unit = 0;
		field.numberCheck = 0;
		field.bit = 0;
		field.choices = 0;
		field.choiceCount = 0;
		field.capacity = 0;
		field.fixed = false;
		field.capacityWhy = 0;
		field.required = false;
		field.requiredWhy = 0;
		field.letters = 0;
		field.textCheck = 0;
		field.prefixField = -1;
		field.checkContext = 0;
		return field;
	}
}
