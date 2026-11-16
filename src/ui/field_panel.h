// The field panel's model: the column of fields for one record, such as one light or one sound, each in the box its kind needs, with
// the focus ring that moves between them, the help row at the panel's foot and the status line. Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES.
// - The list of fields is the editor's own table (field_descriptor.h), checked as a whole by CheckFieldList when it's loaded. A list
//   that check refuses loads nothing, and a panel with nothing loaded refuses every event.
// - The values come from a level and are hostile. Each goes into its own box, which keeps it exactly as it was until the mapper keeps an
//   edit. A value that's odd but readable (a number outside its range, a choice its list doesn't have, an empty required name, a name
//   the spec refuses as it is) is kept, shown, and flagged (Flag), never changed by the panel. A value that can't go in its box at all
//   (a name longer than its capacity) is shown as unreadable, refuses every edit, and is never reported as changed, so it's never saved
//   over.
// - Every edit passes the field spec's own check before it's kept, inside the box, so nothing is ever kept and then taken back. When
//   the spec says no, nothing is kept, whatever reason it gives or doesn't give: a check that refuses with no reason still refuses.
// - A required text can't be emptied by an edit: the spec's own reason (requiredWhy) is shown.
// - A text whose file the game opens with another name in front (prefixField) is checked with that name as the panel holds it now. If
//   that name couldn't be read from the level, the edit can't be checked, so it's refused.
// - A read-only field takes focus, so it can be read, but refuses every edit.
// - The status line shows the spec's own words for a refusal of a value, and the panel's own words only for typing (a comma for the
//   dot, a letter the font doesn't have) or for a reason the spec can't give (a read-only field, a value that couldn't be read, a spec
//   that gave no reason). Everything shown goes through AvPx's OnlineScreens_SafeName, measured to at most kFieldHelpMost bytes.
// - Tab moves to the next field and Shift+Tab to the one before, round from the end to the start. Focus leaving a field keeps its edit if
//   it can be kept and gives it up if it can't, saying why, so no field is ever left half edited.
//
// A panel holds the descriptors' list by pointer, and that list has to stay where it is while the panel uses it: it's the editor's own
// constant table. The level's values are copied into the boxes, so they needn't stay.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "choice_box.h"
#include "field_descriptor.h"
#include "focus_ring.h"
#include "number_box.h"
#include "small_widgets.h"
#include "text_field.h"
#include "ui_event.h"

namespace ui
{
	// One field's value from a level, hostile. Each kind reads only its own part.
	struct FieldValue
	{
		double number;               // a number's value, as stored
		uint32_t word;               // a bit's whole flags word, with every bit the editor has no name for
		int32_t choice;              // a choice's value
		const unsigned char *bytes;  // a text's bytes, length of them: the name is those before the first zero, or all of them. A full
		size_t length;               // fixed text has no zero at all, so the length is what ends it.
	};

	// A value with every part 0
	FieldValue EmptyFieldValue();

	// What's odd about a value as it is now. The value is kept as it is whatever its flag.
	enum class FieldFlag
	{
		None,
		NotLoaded,         // the level's value couldn't go in its box, so it's unreadable and can't be changed
		OutOfRange,        // a number outside its range, or not a number at all
		UnknownChoice,     // a value the choice list doesn't have
		EmptyButRequired,  // empty, though the game can't use it so (FlagWhy: the spec's requiredWhy)
		SpecRefusesIt,     // the spec's check refuses it as it is now, such as a wav too long with the sound folder in front (FlagWhy:
		                   // the spec's words, or null when it gave none)
		PrefixNotLoaded    // the name the game puts in front of it couldn't be read, so it can't be checked
	};

	// What one event did
	enum class PanelAnswer
	{
		Nothing,     // the event meant nothing here
		FocusMoved,  // focus went to another field (the field it left kept or gave up its edit, and the status line says why if it gave up)
		Moved,       // a cursor, a selection or a lit choice moved, or a list opened or closed
		Changed,     // the text being edited changed
		Kept,        // the field's value is now the edit, or a bit was turned over, or a choice picked
		GaveUp,      // the edit was given up, and the value is as it was
		Refused      // the event was refused, and the status line says why
	};

	// The panel's own words, for typing and for what the spec can't say. Each is in plain words for a mapper.
	extern const char kPanelSpecGaveNoReason[];
	extern const char kPanelReadOnly[];
	extern const char kPanelNotLoaded[];
	extern const char kPanelPrefixNotLoaded[];
	extern const char kPanelFull[];
	extern const char kPanelUseADot[];
	extern const char kPanelNotANumberLetter[];
	extern const char kPanelTooLong[];
	extern const char kPanelNotANumber[];
	extern const char kPanelTooManyDecimals[];
	extern const char kPanelOutOfRange[];
	extern const char kPanelNotAFontLetter[];
	extern const char kPanelNotAllowedHere[];
	extern const char kPanelBadPaste[];
	extern const char kPanelPasteOneLine[];
	extern const char kPanelHasABadLetter[];
	extern const char kPanelCantTypeSome[];

	class FieldPanel
	{
	public:
		FieldPanel();

		// Loads a record: count descriptors, and count values from the level, one for each. Gives back false, and leaves the panel with
		// nothing, for a list CheckFieldList refuses (problem, if not null, says why) or for no values where some are counted. A value
		// that's odd, or can't go in its box, doesn't stop the load: that field is flagged.
		bool Load(const FieldDescriptor *list, size_t count, const FieldValue *values, FieldListProblem *problem);

		// One event, for the field with focus, or Tab and Shift+Tab for the ring
		PanelAnswer Handle(const Event &event);

		// Text from the clipboard, for the field with focus
		PanelAnswer Paste(const unsigned char *bytes, size_t length);

		// A click on a field: it takes focus, and the field it left keeps or gives up its edit
		PanelAnswer ClickField(size_t field);

		// Whether a list was loaded. An empty one is: a record with nothing to edit shows an empty panel.
		bool Loaded() const { return loaded; }
		size_t Count() const { return count; }

		// The field with focus, or -1 when there's none (an empty panel)
		int Focused() const { return ring.Focused(); }

		// What's odd about a field's value as it is now, and the words to show for it: the spec's (help for a number out of its range,
		// requiredWhy, the check's own reason), the panel's for what the spec can't say (kPanelNotLoaded, kPanelPrefixNotLoaded), or null
		// when there are none (a choice the list doesn't have shows "unknown (N)" in the box itself)
		FieldFlag Flag(size_t field) const;
		const char *FlagWhy(size_t field) const;

		// Whether a field's value is no longer the one the level held: only those are saved. A field that couldn't be read never is.
		bool Changed(size_t field) const;

		// The values as they'll be saved. A field of another kind, or a place past the end, gives 0.
		double NumberValue(size_t field) const;
		uint32_t BitWord(size_t field) const;
		bool BitChecked(size_t field) const;
		int32_t ChoiceValue(size_t field) const;
		const unsigned char *TextValue(size_t field, size_t *length) const;

		// What a number, choice or text field shows. out always ends with a zero. An outSize of 0 writes nothing.
		void Shown(size_t field, char *out, size_t outSize) const;

		// The help row: the focused field's help, or nothing. The status line: the words for the last event, or nothing.
		void HelpRow(char *out, size_t outSize) const;
		void StatusLine(char *out, size_t outSize) const;

		// The words the status line shows, as they are, and whether they're the spec's own. Null when it shows nothing.
		const char *StatusWords() const { return status; }
		bool StatusIsTheSpecs() const { return statusIsTheSpecs; }

	private:
		// One field: the box its kind needs, and the value the level held, for Changed
		struct Slot
		{
			const FieldPanel *panel;
			size_t place;
			bool loaded;
			NumberBox number;
			BitBox bit;
			ChoiceBox choice;
			TextField text;
			uint64_t levelNumberBits;
			uint32_t levelWord;
			int32_t levelChoice;
			unsigned char levelText[kTextMostBytes];
			size_t levelTextLength;
		};

		FieldPanel(const FieldPanel &) = delete;
		FieldPanel &operator=(const FieldPanel &) = delete;

		void Unload();
		void LoadSlot(Slot &slot, const FieldValue &value);
		void LeaveFocused();
		void Quiet();
		PanelAnswer Say(const char *words, bool theSpecs, PanelAnswer answer);
		PanelAnswer SpecSays(const char *why, PanelAnswer answer);
		PanelAnswer FromNumber(const FieldDescriptor &field, const Slot &slot, NumberAnswer answer);
		PanelAnswer FromText(const FieldDescriptor &field, const Slot &slot, TextAnswer answer);
		FieldFlag FlagAndWhy(size_t field, const char **why) const;

		// Whether a text may be kept in a field: the required rule, then the spec's check with the name in front as the panel holds it
		// now. When it may not, *why is the spec's reason, kPanelPrefixNotLoaded, or null when the spec gave none.
		bool TextAllowed(const FieldDescriptor &field, const unsigned char *bytes, size_t length, const char **why) const;

		// The text box's keep check: the slot is its context
		static bool TextKeep(const void *context, const unsigned char *bytes, size_t length, const char **why);

		bool loaded;
		const FieldDescriptor *list;   // null when nothing is loaded, or an empty list was
		size_t count;
		std::vector<Slot> slots;       // count of them, made once by Load, so each slot stays where it is while it's used
		FocusRing ring;
		const char *status;
		bool statusIsTheSpecs;
	};
}
