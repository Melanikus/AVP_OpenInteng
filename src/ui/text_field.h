// The short text field's model: a name or a short text a mapper edits, such as an object's name. Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// TEXT FROM A LEVEL IS HOSTILE (the coding standard's rule 1). A level can come from anyone, and its names are bytes, not checked words.
// So this field:
// - holds at most kTextMostBytes bytes, in storage of its own whose size is fixed when it's compiled. A field's own capacity is the game's
//   limit for that field, from the field spec, and never more than that.
// - never measures a level's text by looking for its end: it's given the length, and a name ends at its first zero byte or at that length.
// - shows a level's text only through AvPx's own OnlineScreens_SafeName, which turns every byte the game's font has no letter for into a
//   question mark. Nothing a level holds is ever used as a format.
// - keeps a level's bytes exactly as they were until the mapper changes the field and the change is kept. Showing a question mark never
//   changes what's saved.
// - takes only the font's letters (the space to the tilde) from the keyboard and from the clipboard. A letter that won't fit is refused,
//   and so is a paste that won't fit: nothing already there is ever cut to make room.
// - keeps the cursor and the selection inside the text after every event.
//
// HOW IT'S USED. The panel loads the field's value from the level, gives the field each event while it has focus, and calls Leave when
// focus moves away. Value is what's saved. While a value is being edited, Editing says so, and Value stays the last value kept.

#pragma once

#include <stddef.h>

#include "ui_event.h"

namespace ui
{
	// The most bytes any text field holds. Each field's own capacity, from the field spec, is this or less.
	size_t const kTextMostBytes = 256;

	// Whether a byte is one of the letters the game's font has, from the space (32) to the tilde (126). Those are the only ones a person
	// can type here, and the only ones shown as they are.
	bool IsFontLetter(unsigned char letter);

	// Which letters a field takes, from the field spec: for example, no space in a name that can't have one. Every letter a field takes
	// has to be a font letter as well, which the field checks itself. A field with no rule of its own takes every font letter.
	typedef bool (*LetterRule)(unsigned char letter);

	// A check run on an edit before it's kept, from the field spec (the field panel gives it, field_descriptor.h). Gives back true when
	// the edit may be kept. When it may not, it gives back false and sets *why to the spec's reason, or leaves it null when the spec gave
	// none. Only the answer counts: false refuses the edit whatever *why says. context is given back as it was.
	typedef bool (*TextKeepCheck)(const void *context, const unsigned char *bytes, size_t length, const char **why);

	// What one event did, for the panel to show
	enum class TextAnswer
	{
		Nothing,      // the event means nothing to this field (Tab, for one, is the focus ring's)
		Moved,        // the cursor or the selection moved
		Changed,      // the text being edited changed
		Kept,         // the edit is now the field's value
		GaveUp,       // the edit was given up, and the field's value is as it was
		Refused       // the event was refused, for the reason Refusal says, and nothing changed
	};

	// Why the last event was refused, so the panel can say it in words
	enum class TextRefusal
	{
		None,
		NotAFontLetter,       // a letter the font doesn't have: a control character, or a byte from 128 to 255
		NotAllowedHere,       // a font letter this field doesn't take
		Full,                 // the field holds as many bytes as it can
		PasteTooLong,         // the text pasted wouldn't fit, so none of it went in
		PasteHasALineBreak,   // the text pasted is more than one line ("paste one line")
		PasteHasANonLetter,   // the text pasted has a byte that's not a letter this field takes
		NotKeptHasABadLetter, // the edit still has a letter this field doesn't take, such as a question mark left where a level's own letter was
		NotLoaded,            // the field has no value: its value from the level couldn't be loaded
		SpecRefused           // the field spec's own check refused the edit, for the reason SpecWhy gives
	};

	class TextField
	{
	public:
		TextField();

		// Loads a value read from a level: length bytes, of which the name is those before the first zero. capacity is the game's
		// limit for this field, from the field spec. Gives back false, and leaves the field with no value, for a capacity of 0 or more
		// than kTextMostBytes, or a name longer than its capacity: a value that won't fit is never cut. A field with no value refuses
		// every event, so it can never be saved over.
		bool Load(const unsigned char *bytes, size_t length, size_t capacity, LetterRule allowed);

		// The same, with the spec's check, which every edit has to pass before it's kept. The check belongs to the load, so a field
		// loaded again for another value never keeps an old check, and a load without one has none.
		bool Load(const unsigned char *bytes, size_t length, size_t capacity, LetterRule allowed, TextKeepCheck check, const void *context);

		// Gives the field one event. It has focus while it's given them.
		TextAnswer Handle(const Event &event);

		// Text from the clipboard, as the window read it: length bytes, never measured here. All of it goes in, or none of it does.
		TextAnswer Paste(const unsigned char *bytes, size_t length);

		// Focus has moved away. An edit that can be kept is kept, and one that can't is given up, so the field is never left half
		// edited. The answer says which, and Refusal says why one wasn't kept.
		TextAnswer Leave();

		// The selected bytes of the edit, for the clipboard. Gives back how many it wrote, at most outSize.
		size_t CopySelection(unsigned char *out, size_t outSize) const;

		// What to show: the edit, or the value when it isn't being edited, with a question mark for every byte that isn't a font letter.
		// out always ends with a zero. An outSize of 0 writes nothing.
		void Shown(char *out, size_t outSize) const;

		// The value as it will be saved: the level's own bytes, until an edit is kept.
		const unsigned char *Value() const { return value; }
		size_t ValueLength() const { return valueLength; }

		bool Loaded() const { return capacity > 0; }
		bool Editing() const { return editing; }
		size_t Capacity() const { return capacity; }

		// The text being shown, its length, and where the cursor and the selection are in it. The selection is from SelectionStart up to
		// SelectionEnd, and it's empty when they're the same.
		size_t Length() const { return editing ? editLength : valueLength; }
		size_t Cursor() const { return editing ? cursor : valueLength; }
		size_t SelectionStart() const { return editing ? (anchor < cursor ? anchor : cursor) : 0; }
		size_t SelectionEnd() const { return editing ? (anchor < cursor ? cursor : anchor) : 0; }

		// True while an edit is in progress of a value that holds letters this field can't type. The panel then says: "this name has
		// characters the editor can't type; your edit replaces them". Esc gives the original back.
		bool ShowsCantTypeNotice() const { return editing && valueHasLettersItCantType; }

		// Why the last refused event, or the last edit not kept, was refused
		TextRefusal Refusal() const { return refusal; }

		// The spec's reason when its check refused the last edit (Refusal is SpecRefused), or null when it gave none or that wasn't the
		// refusal
		const char *SpecWhy() const { return refusal == TextRefusal::SpecRefused ? specWhy : 0; }

	private:
		bool TakesLetter(unsigned char letter) const;
		void BeginEditing();
		void DeleteSelection();
		TextAnswer Refuse(TextRefusal why);
		TextAnswer Keep();

		unsigned char value[kTextMostBytes];   // the field's value, as it will be saved
		size_t valueLength;
		unsigned char edit[kTextMostBytes];    // the text being edited: only letters this field takes, and question marks where it couldn't
		size_t editLength;
		size_t capacity;                       // 0 when there's no value
		size_t cursor;                         // where the next letter goes, from 0 to editLength
		size_t anchor;                         // the other end of the selection, from 0 to editLength
		bool editing;
		bool valueHasLettersItCantType;
		LetterRule allowed;
		TextRefusal refusal;
		TextKeepCheck keepCheck;               // the spec's check, or null for none
		const void *keepContext;
		const char *specWhy;                   // the spec's reason for the last refusal it made, or null
	};
}
