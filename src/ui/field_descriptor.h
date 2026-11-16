// What the field panel needs to know about each field it shows: its label, its help, what kind of box edits it, and that box's limits.
// Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A descriptor is the editor's own words and the game's own limits, never anything read from a level, so a list of them is a constant
// table. A thin adapter in src/panels/ fills these from F6's tables (src/model/field_model.h and name_model.h), so this kit never needs
// to know F6's shapes. The values a level holds are loaded into the boxes afterwards, and are hostile: that's the boxes' job
// (text_field.h, number_box.h, choice_box.h, small_widgets.h).
//
// A list of descriptors is checked as a whole by CheckFieldList before a panel uses it, and refused as a whole if anything in it is
// wrong, so a mistake in the table shows at once instead of as a field that's quietly missing or cut short.
//
// THE FIELD SPEC'S OWN WORDS. When a value is refused, the mapper is told the field spec's reason, never one the panel makes up
// (Hicks, 2026-11-14). So a field can carry the spec's own check (a NumberCheck or a TextCheck), run on a value before it's kept, and
// the reasons the spec gives for its limits (help, capacityWhy, requiredWhy). The boxes' own refusals are only about typing: a comma
// for the dot, a letter that's never in a number, a letter the font doesn't have.
//
// Every text here is measured by a loop that stops at its zero or one past its most, whichever comes first, never by strlen, even
// though they're the editor's own words: one rule everywhere is easier to keep.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "choice_box.h"
#include "focus_ring.h"
#include "number_box.h"
#include "text_field.h"

namespace ui
{
	// The most fields one panel shows: as many as its focus ring holds, so every field can take focus. The largest record the game has
	// is far smaller; a longer list is a mistake in the table.
	size_t const kFieldPanelMost = kFocusMostWidgets;

	// The longest label, help and unit, in bytes. The label sits in a column of its own, the help in the field panel's help row at its
	// foot, and the unit beside its number. The field spec's longest reason is 118 bytes (Vasquez, 2026-11-14).
	size_t const kFieldNameMost = 48;
	size_t const kFieldHelpMost = 200;
	size_t const kFieldUnitMost = 24;

	enum class FieldKind
	{
		Number,   // a NumberBox: lowest, highest, decimals, and maybe a unit
		Bit,      // a BitBox: one bit of a flags word
		Choice,   // a ChoiceBox: a list of values with words
		Text      // a TextField: capacity, letters, and maybe the name the game puts in front of it
	};

	// The field spec's own check of a number before it's kept is a NumberCheck (number_box.h): the box asks it itself, with the
	// descriptor's checkContext.
	//
	// The spec's check of a text: its bytes and their length, and the text of the field the game puts in front of it when it opens the
	// file (prefixField), or null and 0 when there's none. The spec works out whether the two fit together, so there's one rule for it.
	// Gives back true when the text may be kept; when it may not, false, with *why set to the spec's reason or left null. Only the answer
	// counts. A full fixed text has no zero byte: it's given as its bytes and their length, never as a text to be measured.
	typedef bool (*TextCheck)(const void *context, const unsigned char *bytes, size_t length, const unsigned char *prefix,
		size_t prefixLength, const char **why);

	struct FieldDescriptor
	{
		const char *name;          // the label: the editor's own words, 1 to kFieldNameMost font letters
		const char *help;          // one line of help, 1 to kFieldHelpMost font letters, or null for none
		FieldKind kind;
		bool readOnly;             // shown, and can take focus to be read, but never changed

		// Number. The number is shown as it's stored, never converted: unit says what it counts.
		double lowest;
		double highest;
		unsigned decimals;         // 0 for every whole-number storage
		const char *unit;          // the unit's words, 1 to kFieldUnitMost font letters, such as "mm", or null
		NumberCheck numberCheck;   // the spec's own check, or null

		// Bit
		uint32_t bit;              // exactly one bit set

		// Choice
		const Choice *choices;     // as ChoiceBox::Load takes them (ChoicesAreSound): 1 to kChoiceMost, each value once
		size_t choiceCount;

		// Text
		size_t capacity;           // the game's limit in bytes, not counting the zero: 63, 255, or a fixed 8, 16, 20 or 100
		bool fixed;                // the game keeps exactly capacity bytes, with no zero at all when it's full
		const char *capacityWhy;   // the spec's reason for that limit, shown when a letter won't fit, or null
		bool required;             // the game can't use it empty: an edit that empties it is refused, and an empty value a level
		                           // already holds is kept as it is and flagged, never changed
		const char *requiredWhy;   // the spec's reason, when required; null when it isn't
		LetterRule letters;        // which letters it takes, or null for every font letter
		TextCheck textCheck;       // the spec's own check, or null
		int32_t prefixField;       // the place in this list of the text field whose text the game puts in front of this one when it
		                           // opens the file (such as the level's sound folder, shown read-only), or -1 for none

		const void *checkContext;  // given back to numberCheck or textCheck as it was
	};

	// Why CheckFieldList refused a list
	enum class FieldListRefusal
	{
		None,
		TooMany,      // more than kFieldPanelMost fields
		NoList,       // fields counted, but no list
		BadName,      // no name, an empty one, one longer than kFieldNameMost, or one with a byte that isn't a font letter
		BadHelp,      // help that's empty, too long, or has a byte that isn't a font letter
		NotAKind,     // a kind the panel has no box for
		NotItsKind,   // something set that only another kind has, such as a capacity on a number, or a unit on a text
		BadNumber,    // a range or decimals NumberRuleFor refuses, or a unit that's empty, too long, or not font letters
		BadBit,       // not exactly one bit
		BadChoices,   // a list ChoicesAreSound refuses
		BadText,      // a capacity of 0 or above kTextMostBytes, or a capacityWhy that's empty, too long or not font letters
		BadRequired,  // required with no reason, a reason when it isn't required, or a reason that's empty, too long or not font letters
		BadPrefix     // a prefix field that isn't another text field of this list, has a prefix of its own, or has no check to use it
	};

	struct FieldListProblem
	{
		size_t field;           // the first field refused, or 0 when the list as a whole was (TooMany, NoList)
		FieldListRefusal why;
	};

	// Checks a list of count descriptors as a whole. Gives back true when a panel can use every one of them. When it can't, it gives
	// back false and says in *problem (if not null) which field was refused first and why. An empty list is a sound one: a record with
	// nothing to edit shows an empty panel. A count above kFieldPanelMost is refused before anything in the list is read.
	bool CheckFieldList(const FieldDescriptor *list, size_t count, FieldListProblem *problem);

	// A descriptor with nothing set, for a table to fill: no help, no unit, no check, no prefix (-1), and every limit 0
	FieldDescriptor EmptyFieldDescriptor();
}
