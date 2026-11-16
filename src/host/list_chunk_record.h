// list_chunk_record.h: turns ONE ITEM of a chunk that holds a list (a track's section, one alternate location, one point of a patrol path, one
// waypoint, ...) into the field panel host's record, read-only. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (the records
// for the chunks Drake's registry says have items). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY IT ISN'T BuildChunkRecord (chunk_record.h). That one shows a chunk whose fields are numbers counted from the start of its payload.
// The chunks here are a few numbers of their own and then a LIST, each item with numbers of its own, so the count in the file says how many
// items there are and a place depends on the payload (model::FindChunkFamily names them: Tracks, AlternateLocations, Ai). A list can hold
// thousands of items and the panel shows at most kFieldPanelHostMostFields fields, so the record shows the chunk's own numbers and ONE
// item, as the old editor's dialogs did ("Section n/m", "Location n/m"), and the caller says which item.
//
// WHAT A RECORD HOLDS. The title, then the chunk's own fields (its head), then the chosen item's fields. The title says which item of how
// many: "<what the chunk is>: <its 8-letter name> item <i>/<n>", counting from 1, so item 1/3 is the first of three. A chunk with no
// items to choose (an empty list, or a chunk that has no list) shows its head alone, as item 0, and its title says "(no items)" or nothing
// about items.
//
// THE RULES, which are the other records' (chunk_record.h, strategy_record.h), field by field:
// - A field the old editor showed no label for (its label is "") isn't shown, and its bytes are never touched. A flags field is the
//   exception: it has no label of its own, and each of its named bits is shown by the bit's own words.
// - A flags field is one Bit for each named bit; a field of three numbers is three numbers, x, y and z; any other field is a Number with
//   its limits and its unit's words. A value is shown as the file holds it: a number outside its range is kept, and the panel flags it.
// - Every word is the editor's own, cut to what a descriptor takes, and each letter the font doesn't have is made a "?", so the record
//   always passes the panel's own check. A record the check refuses anyway is a mistake in our tables, and is refused whole, with the reason.
// - Everything read from the payload is hostile (Tao 3). The model checks the payload the way the game's own reader does before it
//   trusts a count, so a count that says more items than the bytes hold is refused whole, and no place is worked out past the payload's end.
//   An item the list doesn't hold is refused whatever number is asked for. Only one item is ever shown, so a count can't make the record
//   longer than its tables are: it stays inside the panel's kFieldPanelHostMostFields whatever a hostile file says.
// - Names (a path's name, a waypoint's module links' names) and the lists inside an item (a waypoint's links) aren't shown yet: they need a
//   reader of their own, and a second index to choose among them (inteng-coverage.md, the card under J).
//
// The record is the shared one (field_record.h): the same record an object's header, its strategy and any other chunk fill.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "field_model.h"
#include "field_record.h"

namespace host
{
	// Builds the record of item number item (the first is 0) of the chunk named chunk (its 8-letter name: 8 bytes are read, and no zero is needed
	// after them, but MODZONE has 7 letters and its zero) whose payload (the bytes after the chunk's 12-byte heading) is length bytes at
	// payload. Gives back false, with *why (if not null) in plain words and an empty record, for:
	// - a null record, a null payload, or no chunk;
	// - a chunk the field model has no table for in any family;
	// - a chunk that has no items to show one by one (a light, a sound, acoustics: BuildChunkRecord shows those whole);
	// - a payload of a size the game's reader refuses (the reason is the model's own);
	// - an item the chunk doesn't hold: any number from the count up, however big. A chunk with no items takes item 0 only;
	// - a field the tables describe that can't be read or shown, more fields than the panel host shows, or a record the panel's own check
	//   refuses: those are mistakes in our tables.
	// *why points to a text that's static, or in the record, so it's good as long as the record is.
	bool BuildChunkItemRecord(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why);

	// How many items the chunk holds, for a caller that steps through them with BuildChunkItemRecord ("Section n of m"): *count is the number,
	// 0 for a chunk whose list is empty or that has no list, and the items are 0 to *count - 1 (an empty list still takes item 0, which shows
	// its head). The same chunk names and payload as BuildChunkItemRecord. Gives back false, with *why (if not null) in plain words and
	// *count left alone, for a null payload or no place for the count, a chunk that is unknown or has no items to show one by one, and a
	// payload of a size the game's reader refuses.
	bool CountChunkItems(char const *chunk, uint8_t const *payload, size_t length, size_t *count, char const **why);

	// ---- What a record is made of ----
	//
	// These are here so a test can build a part of its own, with a field of each storage, which the real tables have few examples of.
	// BuildChunkItemRecord finds the parts of a real chunk and hands them to BuildRecordOfParts.

	// Reads the fields of ONE part (the numbers at a chunk's head, or those of one item) out of a payload, in the way its family finds them.
	class PartReader
	{
	public:
		virtual ~PartReader() {}

		// The field's value as one number, as the file holds it (a masked field with its mask applied, not shifted). False when it can't be read.
		virtual bool Number(model::Field const &field, double *value) const = 0;

		// One number of a field of three numbers: axis 0, 1 or 2. A part with no such field needn't say: by default it's refused.
		virtual bool Axis(model::Field const &field, int axis, int32_t *value) const
		{
			(void)field;
			(void)axis;
			(void)value;
			return false;
		}
	};

	// The words of one bit of a flags field of a part
	struct PartBit
	{
		char const *field;    // the field's name
		uint32_t bit;         // the bit: exactly one
		char const *label;    // in words; a bit with no words ("" or null) isn't shown
	};

	// One part: its fields in the order they're shown, the words of its flags' bits, and what reads them. A part with no fields shows nothing.
	struct PartShown
	{
		model::Field const *fields;
		size_t fieldCount;
		PartBit const *bits;
		size_t bitCount;
		PartReader const *reader;
	};

	// Builds a record from the head part and the item part (either may be null) under that title. Gives back false, with *why and an empty
	// record, for a null record, a field of a part that can't be read or shown (a field of an object, an orientation or a choice: no list
	// chunk has one yet), more fields than kFieldPanelHostMostFields, or a record the panel's own check refuses.
	bool BuildRecordOfParts(std::string const &title, PartShown const *head, PartShown const *item, FieldRecord *record, char const **why);
}
