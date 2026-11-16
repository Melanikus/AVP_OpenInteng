// The number box's model: a number a mapper types, such as a light's range or a door's time to open. Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES.
// - The text is read by the game's own reader, ReadPlainNumber (avpx/3dc/shoulder_lamp.cpp), so the editor and the game read a typed
//   number the same way: digits, one dot at most, and a minus in front only where the field's range goes below 0. There is one reader,
//   not two (R-INT-4). Each field gives the reader the longest text its range can need, and longer text is refused before it's read.
// - A dot is the decimal mark, decided here and never by the computer's language: a comma is refused with "use a dot: 0.5". Numbers are
//   written out by this file's own sums, not by printf, so no language setting can change how one is shown either.
// - A value is kept only when it's a number, has no more decimals than the field allows, and is inside the field's range. Anything
//   else is refused, the reason is given, and the value stays as it was. The range and why it's that range come from the field spec.
// - A level can hold a value outside its field's range, or a float that isn't a number at all. It's kept exactly as it was, and shown
//   as it is (or as "not a number"), until the mapper types a value that's kept. The box never changes a value by itself.
// - Editing is the text field's: the same bounded storage, the same keys, the same paste rules. This only adds the number's rules.

#pragma once

#include <stddef.h>

#include "text_field.h"
#include "ui_event.h"

namespace ui
{
	// What one field's numbers may be, worked out once from its range by NumberRuleFor
	struct NumberRule
	{
		double lowest;           // the smallest value kept, as shown
		double highest;          // the largest
		unsigned decimals;       // the most digits after the dot. 0 means whole numbers only
		bool mayBeNegative;      // whether a minus can be typed: only when lowest is below 0
		unsigned longest;        // the longest number the reader takes, not counting a minus
		bool good;               // false when the range can't be a field's (see NumberRuleFor)
	};

	// The most decimals a field can have, and the largest size a value can be and still be written out exactly
	unsigned const kNumberMostDecimals = 6;
	double const kNumberLargest = 1e15;

	// The most letters a typed number can be: the reader's longest (kReadNumberMostLongest, 24), and a minus. number_box.cpp checks that the
	// two agree when it's compiled.
	size_t const kNumberMostLetters = 25;

	// Works out a field's rule from its range and its decimals. The rule isn't good, and every box given it refuses to load, when either
	// end isn't a number, lowest is above highest, decimals is above kNumberMostDecimals, or the range times ten to its decimals reaches
	// kNumberLargest, beyond which a double can't count every step exactly.
	NumberRule NumberRuleFor(double lowest, double highest, unsigned decimals);

	// The field spec's own check of a number before it's kept (the field panel gives it, field_descriptor.h). Gives back true when it may
	// be kept. When it may not, it gives back false and sets *why to the spec's reason, in the spec's words, or leaves it null when the
	// spec gave none. Only the answer counts: false refuses the number whatever *why says. context is given back as it was: the adapter's
	// own pointer to the spec's field.
	typedef bool (*NumberCheck)(const void *context, double value, const char **why);

	enum class NumberAnswer
	{
		Nothing,    // the event means nothing here
		Moved,      // the cursor or the selection moved
		Changed,    // the text being typed changed
		Kept,       // the typed number is now the value
		GaveUp,     // the edit was given up, and the value is as it was
		Refused     // the event was refused, for the reason Refusal says, and nothing changed
	};

	enum class NumberRefusal
	{
		None,
		UseADot,           // a comma: the decimal mark is a dot ("use a dot: 0.5")
		NotANumberLetter,  // a letter that's never part of a number, or a minus where the range doesn't go below 0
		TooLong,           // the text is as long as the field's numbers can be
		NotANumber,        // the reader refused it: empty, a minus alone, two dots, a minus that isn't in front
		TooManyDecimals,   // more digits after the dot than the field has (for a whole number, any dot at all)
		OutOfRange,        // below the field's lowest or above its highest: the panel says the range and why
		BadPaste,          // a paste refused by the text field's rules (too long, more than one line, a letter it doesn't take)
		NotLoaded,         // the box has no value: its rule wasn't good
		SpecRefused        // the field spec's own check refused the number, for the reason SpecWhy gives
	};

	class NumberBox
	{
	public:
		NumberBox();

		// Loads a value read from a level, and the field's rule. Gives back false, and leaves the box with no value, for a rule that
		// isn't good. A value outside the range, or one that isn't a number, loads and is kept as it is (see OutOfRangeNow).
		bool Load(double value, const NumberRule &rule);

		// The same, with the spec's check, which every typed number has to pass before it's kept. It's asked as soon as the text reads as
		// a number, before the box's own decimals and range, so the reason shown is the spec's. The check belongs to the load, so a box
		// loaded again never keeps an old check, and a load without one has none.
		bool Load(double value, const NumberRule &rule, NumberCheck check, const void *context);

		NumberAnswer Handle(const Event &event);
		NumberAnswer Paste(const unsigned char *bytes, size_t length);

		// Focus has moved away: a number that can be kept is kept, and one that can't is given up, with Refusal saying why
		NumberAnswer Leave();

		// What to show: the text being typed, or the value written out with the field's decimals. out always ends with a zero.
		void Shown(char *out, size_t outSize) const;

		bool Loaded() const { return rule.good; }
		bool Editing() const { return text.Editing(); }
		double Value() const { return value; }

		// True when the value from the level is outside the field's range, or isn't a number, so the panel can say so. It's still kept.
		bool OutOfRangeNow() const;

		NumberRefusal Refusal() const { return refusal; }
		const NumberRule &Rule() const { return rule; }

		// The spec's reason when its check refused the last number (Refusal is SpecRefused), or null when it gave none or that wasn't the
		// refusal
		const char *SpecWhy() const { return refusal == NumberRefusal::SpecRefused ? specWhy : 0; }

	private:
		NumberAnswer Refuse(NumberRefusal why);
		NumberAnswer Keep();
		void ShowValue();

		TextField text;          // the typing, with its bounded storage and its rules
		NumberRule rule;
		double value;
		NumberRefusal refusal;
		NumberCheck check;       // the spec's check, or null for none
		const void *checkContext;
		const char *specWhy;     // the spec's reason for the last refusal it made, or null
	};

	// Writes value out with exactly decimals digits after the dot, by sums, never by printf, so no language setting can change it. A
	// value that isn't a number is "not a number", and one too large to write out exactly is "too large". out always ends with a zero.
	void WriteNumber(double value, unsigned decimals, char *out, size_t outSize);
}
