// field_descriptor_from_model.h: the rules that turn a field of the field model (src/model/field_model.h) and a value already read from a
// level into a descriptor and a value of the field panel's record (field_record.h). Written 2026-11-14 by Melanikus (Drake) for
// AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY IT'S ITS OWN FILE. A strategy's record (strategy_record.h) and an object's header record (and any chunk's record) are made the same
// way: the old editor's label cut to what a descriptor takes, the help line, the unit's words, the range a number's box is given, and a
// descriptor kept in the record with its value. These functions are that one way. They take a value that's ALREADY READ, so they work for
// any table (a strategy's or a chunk's); reading the value, and choosing which fields and bits to show, stay with each record's own code.
//
// THE RULES (the old editor's, from inteng):
// - Every descriptor made here is read-only: inspect never changes a level.
// - Every word is cut to what a descriptor takes, reading no further than its first 0, and each byte the font has no letter for becomes a
//   '?', so the panel's own check (ui::CheckFieldList) can't refuse the record for its letters.
// - Everything a descriptor points to (its words, its choices, its value's text) is kept in the record, which has to stay alive while the
//   panel host shows it (field_record.h).
// - A value is kept as it is: a number out of range, "not a number" and an unnamed choice are shown and flagged by the panel, never fixed here.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "choice_box.h"   // ui::Choice
#include "field_descriptor.h"
#include "field_model.h"
#include "field_panel.h"
#include "field_record.h"

namespace host
{
	// The words of a text of the editor's own for a descriptor: no more than most letters, up to the first 0, never measured past that,
	// and each byte that isn't a letter of the font a '?'. A null text is no words.
	std::string SafeWords(char const *text, size_t most);

	// Keeps some words in the record, and gives back a pointer to them that stays good while the record does.
	char const *Keep(FieldRecord *record, std::string const &words);

	// The field's help line: its reason (field.why), cut to ui::kFieldHelpMost and kept in the record, or null when there's none.
	char const *HelpOf(FieldRecord *record, model::Field const &field);

	// The words of a number's unit, or null when it has none. Each is at most ui::kFieldUnitMost letters.
	char const *UnitWords(model::Unit unit);

	// What a number is multiplied by to be shown in the unit inteng shows it in (a light's brightness in percent, its spread in degrees, its range in metres), and 1 for every other unit. The panel shows and takes the
	// shown number; the field holds the stored one: stored = shown / DisplayFactor, rounded to a whole number. The range's factor uses the level's scale (SetDisplayLevelScale), as inteng's memory units do.
	double DisplayFactor(model::Field const &field);
	void SetDisplayLevelScale(double scale);

	// Whether a storage holds a fraction (Float32 or Float64).
	bool IsAFraction(model::Storage storage);

	// The range a number's box is given: the field's own when it has a limit, otherwise everything its storage holds, except that a
	// fraction's range is a billion either way (a box refuses a wider one).
	void RangeOf(model::Field const &field, double *lowest, double *highest);

	// A descriptor of that kind, read-only, with nothing else set.
	ui::FieldDescriptor NewDescriptor(char const *name, char const *help, ui::FieldKind kind);

	// Adds a Number: name and help are already kept in the record (or help is null). The limits, decimals (0, or 3 for a fraction) and
	// unit words come from the field, which the record remembers as the descriptor's source (field_record.h).
	void PushNumber(FieldRecord *record, char const *name, char const *help, model::Field const &field, double number);

	// Adds a Bit for one bit of a flags word: name and help are already kept. word is the whole flags word, as the panel takes it. source is
	// the model's field the word is, or null for a word that isn't one (an object's header): the record then remembers no source for it.
	void PushBit(FieldRecord *record, char const *name, char const *help, uint32_t bit, uint32_t word, model::Field const *source = 0);

	// Adds a Choice: name and help are already kept, and each choice's words are already kept (ui::Choice points to them). The list is
	// copied into the record. stored is the value as the file holds it, named or not. source is the model's field, or null.
	void PushChoice(FieldRecord *record, char const *name, char const *help, std::vector<ui::Choice> const &choices, int32_t stored,
	                model::Field const *source = 0);

	// Whether a choice field's stored number can be a Choice's value, which is an int32: a whole number from -2147483648 to 2147483647. An
	// unsigned field can hold more (a light effect's type is 32 bits, and the game keeps whatever it is), and turning a double above
	// 2147483647 into an int32_t is undefined in C++ ([conv.fpint]). So a builder that gets false shows the field as a Number, exactly as the
	// file holds it, and the panel flags it as outside the field's range. A NaN or endless number is told by its bits (real_numbers.h) and
	// isn't one.
	bool FitsAChoice(double stored);

	// Adds three Numbers for a field of three numbers, named "<label> x", "<label> y" and "<label> z". label is NOT yet kept, and should be
	// cut two letters short of ui::kFieldNameMost (SafeWords(field.label, ui::kFieldNameMost - 2)) so the axis fits. help is already kept.
	// axes is the three values read, x, y, z.
	void PushTriple(FieldRecord *record, std::string const &label, char const *help, model::Field const &field, int32_t const *axes);

	// Adds a Text for an object field: "none" for 0 and 0, otherwise the two halves in hex, "%08X:%08X". name and help are already kept.
	// source is the model's field, or null.
	void PushObject(FieldRecord *record, char const *name, char const *help, uint32_t first, uint32_t second,
	                model::Field const *source = 0);

	// Empties a record (the title, the descriptors, the values and everything they pointed to).
	void EmptyRecord(FieldRecord *record);
}
