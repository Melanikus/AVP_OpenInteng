// See number_box.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "number_box.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

// The game's own typed-number reader (avpx/3dc/shoulder_lamp.cpp), so a number is read here exactly as the game reads one
#include "shoulder_lamp.h"

// The editor's one test for NaN and endless values, by their bits. The editor is built /fp:fast, where isnan(), isfinite() and
// comparisons aren't promised to catch either, and a value from a level can be either.
#include "real_numbers.h"

static_assert(ui::kNumberMostLetters == kReadNumberMostLongest + 1, "a typed number is the reader's longest and a minus");

namespace ui
{
	// The letters a number can have. The minus only where the range goes below 0.
	static bool IsNumberLetterWithMinus(unsigned char letter)
	{
		return (letter >= '0' && letter <= '9') || letter == '.' || letter == '-';
	}

	static bool IsNumberLetterNoMinus(unsigned char letter)
	{
		return (letter >= '0' && letter <= '9') || letter == '.';
	}

	static double TenTo(unsigned power)
	{
		double result = 1.0;

		for (unsigned i = 0; i < power; i++)
		{
			result *= 10.0;
		}

		return result;
	}

	NumberRule NumberRuleFor(double lowest, double highest, unsigned decimals)
	{
		NumberRule rule = { lowest, highest, decimals, lowest < 0.0, 0, false };

		if (!IsRealNumber(lowest) || !IsRealNumber(highest) || lowest > highest || decimals > kNumberMostDecimals)
		{
			return rule;
		}

		double const largest = fabs(lowest) > fabs(highest) ? fabs(lowest) : fabs(highest);

		if (largest * TenTo(decimals) >= kNumberLargest)
		{
			return rule;
		}

		// The digits before the dot of the largest number in the range, at least one, then the dot and the decimals
		unsigned digits = 1;

		for (double whole = floor(largest); whole >= 10.0; whole = floor(whole / 10.0))
		{
			digits++;
		}

		rule.longest = digits + (decimals > 0 ? 1 + decimals : 0);
		rule.good = rule.longest <= kReadNumberMostLongest;
		return rule;
	}

	void WriteNumber(double value, unsigned decimals, char *out, size_t outSize)
	{
		if (!out || outSize == 0)
		{
			return;
		}

		// What the value is written as, before it's copied into its room
		char written[48];
		size_t length = 0;

		if (IsNaN(value))
		{
			memcpy(written, "not a number", 12);
			length = 12;
		}
		else if (decimals > kNumberMostDecimals || IsNaNOrEndless(value) || fabs(value) * TenTo(decimals) >= kNumberLargest)
		{
			memcpy(written, "too large", 9);
			length = 9;
		}
		else
		{
			// The value as a whole number of its smallest steps: below kNumberLargest, so a double holds it exactly, and so does 64 bits
			uint64_t steps = (uint64_t)floor(fabs(value) * TenTo(decimals) + 0.5);
			char digits[24];
			size_t count = 0;

			do
			{
				digits[count++] = (char)('0' + steps % 10);
				steps /= 10;
			}
			while (steps > 0 && count < sizeof(digits));

			// Enough digits for a 0 before the dot
			while (count < decimals + 1)
			{
				digits[count++] = '0';
			}

			bool const allZero = floor(fabs(value) * TenTo(decimals) + 0.5) == 0.0;

			if (value < 0.0 && !allZero)
			{
				written[length++] = '-';
			}

			for (size_t i = count; i > 0; i--)
			{
				written[length++] = digits[i - 1];

				if (decimals > 0 && i - 1 == decimals)
				{
					written[length++] = '.';
				}
			}
		}

		size_t const copied = length < outSize - 1 ? length : outSize - 1;

		memcpy(out, written, copied);
		out[copied] = '\0';
	}

	NumberBox::NumberBox() : rule(NumberRuleFor(0.0, 0.0, 0)), value(0.0), refusal(NumberRefusal::None), check(0), checkContext(0), specWhy(0)
	{
		rule.good = false;
	}

	// The text field is loaded with the value written out, so an edit starts from it. A value that doesn't fit, or isn't a number, gives
	// an empty text instead: the mapper types a new one. The value itself is never changed by this.
	void NumberBox::ShowValue()
	{
		char written[48];

		WriteNumber(value, rule.decimals, written, sizeof(written));

		size_t const capacity = rule.longest + (rule.mayBeNegative ? 1 : 0);
		LetterRule const letters = rule.mayBeNegative ? IsNumberLetterWithMinus : IsNumberLetterNoMinus;
		size_t const length = strlen(written);
		bool usable = length <= capacity;

		for (size_t i = 0; usable && i < length; i++)
		{
			usable = letters((unsigned char)written[i]);
		}

		text.Load((const unsigned char *)written, usable ? length : 0, capacity, letters);
	}

	bool NumberBox::Load(double newValue, const NumberRule &newRule)
	{
		return Load(newValue, newRule, 0, 0);
	}

	bool NumberBox::Load(double newValue, const NumberRule &newRule, NumberCheck newCheck, const void *context)
	{
		rule = newRule;
		value = 0.0;
		refusal = NumberRefusal::None;
		check = newCheck;
		checkContext = context;
		specWhy = 0;
		text = TextField();

		if (!rule.good || rule.longest == 0 || rule.longest > kReadNumberMostLongest)
		{
			rule.good = false;
			return false;
		}

		value = newValue;
		ShowValue();
		return true;
	}

	// A NaN or endless value is asked about by its bits first, as a comparison with one isn't promised to come out false under /fp:fast.
	// There's no take-out row for IsNaNOrEndless here: on this compiler the comparison happens to say "out" anyway, so no test can see the
	// check go, and it stays because /fp:fast doesn't promise that.
	bool NumberBox::OutOfRangeNow() const
	{
		return IsNaNOrEndless(value) || !(value >= rule.lowest && value <= rule.highest);
	}

	NumberAnswer NumberBox::Refuse(NumberRefusal why)
	{
		refusal = why;
		return NumberAnswer::Refused;
	}

	// Reads the typed text with the game's reader, asks the spec's check if there is one, then holds it to the field's decimals and range.
	// The spec is asked first so a refusal is in its words; the box's own decimals and range stay after it, so a spec that says yes to
	// a value outside the field's range still can't get it kept.
	NumberAnswer NumberBox::Keep()
	{
		char typed[kNumberMostLetters + 2];

		specWhy = 0;
		text.Shown(typed, sizeof(typed));

		const void *const end = memchr(typed, 0, sizeof(typed));
		size_t const length = end ? (size_t)((const char *)end - typed) : 0;
		double number = 0.0;
		bool const read = rule.mayBeNegative ? ReadSignedNumber(typed, (unsigned int)length, &number, rule.longest)
			: ReadPlainNumber(typed, (unsigned int)length, &number, rule.longest);

		if (!read)
		{
			return Refuse(NumberRefusal::NotANumber);
		}

		if (check)
		{
			const char *said = 0;

			if (!check(checkContext, number, &said))
			{
				specWhy = said;
				return Refuse(NumberRefusal::SpecRefused);
			}
		}

		const char *const dot = (const char *)memchr(typed, '.', length);
		size_t const afterDot = dot ? length - (size_t)(dot - typed) - 1 : 0;

		if (dot && (rule.decimals == 0 || afterDot > rule.decimals))
		{
			return Refuse(NumberRefusal::TooManyDecimals);
		}

		if (number < rule.lowest || number > rule.highest)
		{
			return Refuse(NumberRefusal::OutOfRange);
		}

		// "-0" is 0
		value = number == 0.0 ? 0.0 : number;
		ShowValue();
		return NumberAnswer::Kept;
	}

	NumberAnswer NumberBox::Handle(const Event &event)
	{
		refusal = NumberRefusal::None;

		if (!Loaded())
		{
			return Refuse(NumberRefusal::NotLoaded);
		}

		if (event.key == Key::Letter && !event.ctrl && event.letter == ',')
		{
			return Refuse(NumberRefusal::UseADot);
		}

		if (event.key == Key::Enter)
		{
			return text.Editing() ? Keep() : NumberAnswer::Nothing;
		}

		switch (text.Handle(event))
		{
			case TextAnswer::Moved: return NumberAnswer::Moved;
			case TextAnswer::Changed: return NumberAnswer::Changed;
			case TextAnswer::GaveUp: return NumberAnswer::GaveUp;
			case TextAnswer::Kept: return Keep();
			case TextAnswer::Refused: return Refuse(text.Refusal() == TextRefusal::Full ? NumberRefusal::TooLong : NumberRefusal::NotANumberLetter);
			default: return NumberAnswer::Nothing;
		}
	}

	NumberAnswer NumberBox::Paste(const unsigned char *bytes, size_t length)
	{
		refusal = NumberRefusal::None;

		if (!Loaded())
		{
			return Refuse(NumberRefusal::NotLoaded);
		}

		if (bytes && length > 0 && length <= kNumberMostLetters && memchr(bytes, ',', length))
		{
			return Refuse(NumberRefusal::UseADot);
		}

		switch (text.Paste(bytes, length))
		{
			case TextAnswer::Changed: return NumberAnswer::Changed;
			case TextAnswer::Refused: return Refuse(NumberRefusal::BadPaste);
			default: return NumberAnswer::Nothing;
		}
	}

	NumberAnswer NumberBox::Leave()
	{
		refusal = NumberRefusal::None;

		if (!text.Editing())
		{
			return NumberAnswer::Nothing;
		}

		if (Keep() == NumberAnswer::Kept)
		{
			return NumberAnswer::Kept;
		}

		// Keep said why in refusal, which stays for the panel to show
		text.Handle(KeyEvent(Key::Escape));
		return NumberAnswer::GaveUp;
	}

	void NumberBox::Shown(char *out, size_t outSize) const
	{
		if (text.Editing())
		{
			text.Shown(out, outSize);
			return;
		}

		WriteNumber(value, rule.decimals, out, outSize);
	}
}
