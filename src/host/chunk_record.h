// chunk_record.h: turns one chunk of a level (a light, a sound, a generator, anything the field model has a table for) into the field panel
// host's record, read-only. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng (the two-dev sprint: the chunk twin of
// strategy_record.h). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT DOES. The same as BuildStrategyRecord (strategy_record.h) does for a strategy, for any chunk the field model describes
// (field_model.h, ChunkKind): the label, help, units, bit names and choice names come from the tables, the values are read out of the
// payload by the model's readers (ReadChunkField, ReadChunkFieldAxis, ReadChunkFieldObject), and everything is read-only. The chunk is
// found by its 8-letter name in ANY family of tables (FindAnyChunkKind, chunk_families.cpp), so a family added later needs nothing here.
//
// THE RULES, which are the old editor's (inteng is the spec), field by field, and the same as the strategy record's:
// - A field the old editor showed no label for (its label is "") isn't shown, and its bytes are never touched. (A flags field is the
//   exception: it has no label of its own, and each of its named bits is shown by the bit's own label.)
// - A flags field is one Bit for each named bit; a choice is one Choice box with its named values (a choice with none is shown as a
//   number); a field of three numbers is three numbers, x, y and z; an object field is one Text, "none" or its two halves in hex; any
//   other field is a Number with its limits and its unit's words. An orientation (nine numbers) isn't shown yet: orientations come with
//   the edit path.
// - A chunk's NAMES (a sound's file name, a generator's name: the text after its numbers) aren't shown yet: they come with a reader of
//   their own.
// - A value is shown as the file holds it. A number outside its range, a "not a number" and a choice nobody named are all kept, and the
//   panel flags them.
// - Every word is the editor's own, cut to what a descriptor takes and with each letter the font doesn't have made a "?", so the record
//   always passes the panel's own check. A record the check refuses anyway is a mistake in our tables, and is refused whole, with the
//   reason.
//
// The record is the shared one (field_record.h): the same record an object's header and its strategy fill.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "field_model.h"
#include "field_record.h"

namespace host
{
	// Builds the record for the chunk named chunk (its 8-letter name; 8 bytes are read, and no zero is needed after them) whose payload
	// (the bytes after the chunk's 12-byte heading) is length bytes at payload. Gives back false, with *why (if not null) in plain words,
	// and an empty record, for:
	// - a null record, or a null payload;
	// - a chunk the field model has no table for in any family;
	// - a chunk that has ITEMS (a track, alternate locations, an AI chunk: model::FindChunkFamily knows it, model::FindAnyChunkKind
	//   doesn't): it's shown one item at a time, by code of its own, so it's refused here with words that say so, not "unknown";
	// - a payload of a size the game's reader refuses (the reason is the model's own: ChunkSizeCheckWords);
	// - a field of the kind that couldn't be read, more fields than the panel host shows (kFieldPanelHostMostFields), or a record the
	//   panel's own check (ui::CheckFieldList) refuses: those are mistakes in the tables.
	// *why points to a text that's static, or in the record, so it's good as long as the record is.
	bool BuildChunkRecord(char const *chunk, uint8_t const *payload, size_t length, FieldRecord *record, char const **why);

	// The same for a kind and names the caller has found: BuildChunkRecord is this, once the chunk is known and its size is right. It
	// doesn't check the payload's size, but it can't read past it, since every field is checked to fit before it's read. It's here so a
	// test can build a kind of its own, with a field of each storage.
	bool BuildChunkRecordOfKind(model::ChunkKind const &kind, model::ChunkNames const &names, uint8_t const *payload, size_t length,
	                            FieldRecord *record, char const **why);
}
