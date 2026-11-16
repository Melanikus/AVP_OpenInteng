// See field_descriptor_from_model.h. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "field_descriptor_from_model.h"

#include <stdio.h>

#include "real_numbers.h"   // IsNaNOrEndless, by the bits: the editor is built with /fp:fast
#include "text_field.h"   // ui::IsFontLetter

namespace host
{
	std::string SafeWords(char const *text, size_t most)
	{
		std::string words;

		if (!text)
		{
			return words;
		}

		for (size_t i = 0; i < most && text[i] != 0; i++)
		{
			unsigned char const letter = (unsigned char)text[i];
			words += ui::IsFontLetter(letter) ? (char)letter : '?';
		}

		return words;
	}

	// A deque never moves what it holds when it grows, so the pointer stays good while the record does.
	char const *Keep(FieldRecord *record, std::string const &words)
	{
		record->text.push_back(words);
		return record->text.back().c_str();
	}

	char const *HelpOf(FieldRecord *record, model::Field const &field)
	{
		std::string const help = SafeWords(field.why, ui::kFieldHelpMost);
		return help.empty() ? 0 : Keep(record, help);
	}

	char const *UnitWords(model::Unit unit)
	{
		switch (unit)
		{
		case model::Unit::GameUnits: return "mm";
		case model::Unit::TenthsOfASecond: return "tenths s";
		case model::Unit::Seconds: return "s";
		case model::Unit::Fixed16Seconds: return "s/65536";
		case model::Unit::Percent: return "%";
		case model::Unit::HundredGrams: return "x100 g";
		case model::Unit::TextNumber: return "message";
		case model::Unit::Fixed16: return "/65536";
		case model::Unit::Angle4096: return "/4096 turn";
		case model::Unit::LightBrightness: return "%";
		case model::Unit::LightSpread: return "deg";
		case model::Unit::LightRange: return "m";
		case model::Unit::Milliseconds: return "ms";
		case model::Unit::Pitch128: return "/128 semitone";
		default: return 0;   // a count, a kind, flags, a choice or a list's count: no unit words
		}
	}

	static double displayLevelScale = 1.0;

	void SetDisplayLevelScale(double scale)
	{
		displayLevelScale = (scale > 0.0 && scale <= 1.0e6) ? scale : 1.0;
	}

	double DisplayFactor(model::Field const &field)
	{
		switch (field.unit)
		{
		case model::Unit::LightBrightness: return 100.0 / 65536.0;
		case model::Unit::LightSpread: return 360.0 / 4096.0;
		case model::Unit::LightRange: return displayLevelScale / 1000.0;
		default: return 1.0;
		}
	}

	bool IsAFraction(model::Storage storage)
	{
		return storage == model::Storage::Float32 || storage == model::Storage::Float64;
	}

	// A fraction's whole range can't be a box's (ui::NumberRuleFor refuses a range times ten to its decimals that reaches 1e15), so its
	// range is a billion either way: a fraction beyond that is still read and shown, and flagged, as any number outside its range is.
	void RangeOf(model::Field const &field, double *lowest, double *highest)
	{
		if (field.limit != model::Limit::None)
		{
			*lowest = field.lowest;
			*highest = field.highest;
			return;
		}

		model::StorageRange(field.storage, lowest, highest);

		if (IsAFraction(field.storage))
		{
			*lowest = -1e9;
			*highest = 1e9;
		}
	}

	ui::FieldDescriptor NewDescriptor(char const *name, char const *help, ui::FieldKind kind)
	{
		ui::FieldDescriptor descriptor = ui::EmptyFieldDescriptor();
		descriptor.name = name;
		descriptor.help = help;
		descriptor.kind = kind;
		descriptor.readOnly = true;
		return descriptor;
	}

	// Remembers where the descriptor just added to the record comes from. The descriptors before it that nobody gave a source for get
	// none, so a source is always at its descriptor's own place. A null field is no source at all.
	static void RememberSource(FieldRecord *record, SourceKind what, model::Field const *field, int axis, uint32_t bit)
	{
		if (!field)
		{
			return;
		}

		while (record->sources.size() + 1 < record->list.size())
		{
			record->sources.push_back(FieldSource());
		}

		FieldSource source;
		source.what = what;
		source.field = field;
		source.axis = axis;
		source.bit = bit;
		record->sources.push_back(source);
	}

	// A Number, and the part of the field it is: the whole field, or one axis of it
	static void PushNumberOf(FieldRecord *record, char const *name, char const *help, model::Field const &field, double number,
	                         SourceKind what, int axis)
	{
		ui::FieldDescriptor descriptor = NewDescriptor(name, help, ui::FieldKind::Number);
		RangeOf(field, &descriptor.lowest, &descriptor.highest);
		descriptor.decimals = IsAFraction(field.storage) ? 3 : 0;
		descriptor.unit = UnitWords(field.unit);

		// a unit inteng shows in other terms (a light's brightness, spread and range): the box has the shown number and its range, with two decimals
		double const factor = DisplayFactor(field);

		if (factor != 1.0)
		{
			descriptor.lowest *= factor;
			descriptor.highest *= factor;
			descriptor.decimals = 2;
		}

		ui::FieldValue value = {};
		value.number = number * factor;
		record->list.push_back(descriptor);
		record->values.push_back(value);
		RememberSource(record, what, &field, axis, 0);
	}

	void PushNumber(FieldRecord *record, char const *name, char const *help, model::Field const &field, double number)
	{
		PushNumberOf(record, name, help, field, number, SourceKind::Number, 0);
	}

	void PushBit(FieldRecord *record, char const *name, char const *help, uint32_t bit, uint32_t word, model::Field const *source)
	{
		ui::FieldDescriptor descriptor = NewDescriptor(name, help, ui::FieldKind::Bit);
		descriptor.bit = bit;

		ui::FieldValue value = {};
		value.word = word;
		record->list.push_back(descriptor);
		record->values.push_back(value);
		RememberSource(record, SourceKind::Bit, source, 0, bit);
	}

	void PushChoice(FieldRecord *record, char const *name, char const *help, std::vector<ui::Choice> const &choices, int32_t stored,
	                model::Field const *source)
	{
		record->choiceLists.push_back(choices);
		std::vector<ui::Choice> const &kept = record->choiceLists.back();

		ui::FieldDescriptor descriptor = NewDescriptor(name, help, ui::FieldKind::Choice);
		descriptor.choices = kept.data();
		descriptor.choiceCount = kept.size();

		ui::FieldValue value = {};
		value.choice = stored;
		record->list.push_back(descriptor);
		record->values.push_back(value);
		RememberSource(record, SourceKind::Choice, source, 0, 0);
	}

	bool FitsAChoice(double stored)
	{
		return !IsNaNOrEndless(stored) && stored >= -2147483648.0 && stored <= 2147483647.0;
	}

	void PushTriple(FieldRecord *record, std::string const &label, char const *help, model::Field const &field, int32_t const *axes)
	{
		char const names[3] = { 'x', 'y', 'z' };

		for (int axis = 0; axis < 3; axis++)
		{
			PushNumberOf(record, Keep(record, label + " " + names[axis]), help, field, axes[axis], SourceKind::Axis, axis);
		}
	}

	void PushObject(FieldRecord *record, char const *name, char const *help, uint32_t first, uint32_t second, model::Field const *source)
	{
		char shown[24];

		if (first == 0 && second == 0)
		{
			snprintf(shown, sizeof(shown), "none");
		}
		else
		{
			snprintf(shown, sizeof(shown), "%08X:%08X", (unsigned)first, (unsigned)second);
		}

		record->valueText.push_back(shown);
		std::string const &kept = record->valueText.back();

		ui::FieldDescriptor descriptor = NewDescriptor(name, help, ui::FieldKind::Text);
		descriptor.capacity = 63;   // "XXXXXXXX:XXXXXXXX", or the name of an object (an object name is at most 63 letters)
		descriptor.fixed = false;

		ui::FieldValue value = {};
		value.bytes = (unsigned char const *)kept.c_str();
		value.length = kept.size();
		record->list.push_back(descriptor);
		record->values.push_back(value);
		RememberSource(record, SourceKind::Object, source, 0, 0);
	}

	void EmptyRecord(FieldRecord *record)
	{
		record->title.clear();
		record->list.clear();
		record->values.clear();
		record->text.clear();
		record->choiceLists.clear();
		record->valueText.clear();
		record->sources.clear();
	}
}
