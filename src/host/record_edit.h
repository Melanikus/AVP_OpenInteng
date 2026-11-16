// record_edit.h: makes a record the field panel shows EDITABLE, and turns a field the mapper kept in the panel into an edit of the bytes it came
// from. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (the edit path, S3c). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT DOES. A record built by strategy_record.h or chunk_record.h is read-only, and remembers, for each descriptor, which field of the
// field model the descriptor came from (field_record.h: FieldSource). Three things are done here, one after the other:
// 1. MakeStrategyRecordEditable / MakeChunkRecordEditable open each descriptor whose field the model can set: not read-only any more, with
//    the field spec's own check inside it (a number is checked by the model's ValueIsAllowed and, for a choice, its named values; an object is
//    checked as text that names an object). So the panel itself refuses a value the field can't hold, with the spec's own reason, before it
//    keeps it.
// 2. EditFromPanel turns one field of a panel that kept a value into a FieldEdit (field_edit.h), the edit the model applies.
// 3. ApplyKeptStrategyField / ApplyKeptChunkField do that, then apply the edit to the bytes the store holds (field_edit.h).
//
// THE RULES.
// - A descriptor with no source (an object's header bit, any title), a list's count, and a field of another kind than the record's, are
//   never made editable and never edited.
// - A flags word is edited one bit at a time, but every bit of the panel holds its OWN copy of the word as it was loaded, so the new word is
//   worked out from the bytes the store holds now (the bit set or cleared in them), never from the panel's copy: two bits changed one after
//   the other both stay changed.
// - What the panel hands over is hostile, like all else: a number is checked to be a whole number that an int32 holds before it becomes
//   an axis (a cast of a number an int32 can't hold is undefined), and the text of an object is parsed by ParseObjectText, which takes
//   "none" or two groups of 8 hex digits and nothing else.
// - Nothing is changed unless the edit is applied whole: ApplyStrategyPayloadEdit and ApplyChunkEdit change nothing when they refuse.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "field_edit.h"
#include "field_model.h"
#include "field_panel.h"
#include "field_record.h"

namespace host
{
	// Opens the descriptors of a record built for that kind of strategy (or chunk) whose field the model can set. A source the model can't
	// set stays read-only: a list's count, and a field whose storage doesn't match what the descriptor shows. Does nothing to a record with
	// no sources. The record is only to be moved after this (a descriptor points to its own source), never copied: it can't be.
	void MakeStrategyRecordEditable(FieldRecord *record, model::StrategyKind const &kind);
	void MakeChunkRecordEditable(FieldRecord *record, model::ChunkKind const &kind);

	// The record for the strategy in payload, editable: BuildStrategyRecord (strategy_record.h), then MakeStrategyRecordEditable for the
	// kind the payload names. The same refusals and the same *why as BuildStrategyRecord.
	bool BuildEditableStrategyRecord(uint8_t const *payload, size_t length, FieldRecord *record, char const **why);

	// The same for the chunk with that id (8 bytes are read): BuildChunkRecord, then MakeChunkRecordEditable.
	bool BuildEditableChunkRecord(char const *chunk, uint8_t const *payload, size_t length, FieldRecord *record, char const **why);

	// PICKING A TARGET BY NAME (inteng's "Select target for switch" list, FUN_0052db10: the target is chosen from the objects by their names): an object field also takes the NAME of an object of the
	// level. The editor gives the lookup (the object list, virtual objects included); with none, only "none" and the hex form are taken. The first object with that name (letters of either case) is the one.
	typedef bool (*ObjectNameResolver)(char const *name, size_t length, uint32_t *first, uint32_t *second);
	void SetObjectNameResolver(ObjectNameResolver resolver);

	// An object's text as the record shows it: "none" for no object (0 and 0), or two groups of 8 hex digits with a colon between, such as
	// "7334ED09:EF81A000" (upper or lower case). Gives back false, changing nothing, for anything else, and for a null text or a null place.
	bool ParseObjectText(unsigned char const *bytes, size_t length, uint32_t *first, uint32_t *second);

	// What the editable record's checks say when the text is not an object, in plain words (static)
	extern char const NotAnObjectReason[];

	enum class PanelEditAnswer
	{
		Made,         // *edit is the edit for the field
		BadCall,      // a null place, or a field past the record's or the panel's
		NotEditable,  // the descriptor has no source the model can set, or is read-only
		ValueRefused  // what the panel holds isn't a value the field can take (*why says why)
	};

	// The edit for what the panel holds in a field: the field the descriptor came from, and the value now in the panel (a number, a choice,
	// a bit, an axis, or an object's text). wordNow is the flags word as the bytes hold it now, used only for a bit: the bit is set or cleared
	// in it, as the panel's box says. *why (if not null) is set to the reason for a ValueRefused, and to nothing else.
	PanelEditAnswer EditFromPanel(FieldRecord const &record, ui::FieldPanel const &panel, size_t field, uint32_t wordNow, FieldEdit *edit,
	                               char const **why);

	// WHAT inteng DOES WHEN A FLAG LINE IS CLICKED IN ITS EDIT SCREEN, where one flag depends on another (inteng-edit-rules.md): the flags word a panel bit edit makes, corrected the way
	// inteng's click leaves it. DOOR (kind 50, DoorFlags; FUN_0052cbb0 0x52cdff-0x52ce0a): "Proximity door" (0x2) and "Switch activated door" (0x8) are one choice: turning one on turns the other
	// off, and turning one off turns the other on, so exactly one of the two is set after any click of either. Every other flag word is as the panel made it.
	uint32_t InteingFlagRules(int32_t kind, char const *field, uint32_t bit, uint32_t newWord);

	enum class KeptAnswer
	{
		Applied,       // the value is in the bytes and the store has them
		BadCall,       // a null place, a field past the record, or no chunk
		NotEditable,   // the field has no source the model can set, or isn't a field of this kind
		NoPayload,     // the store holds no bytes for the chunk, or a flags field can't be read from them
		ValueRefused,  // the panel held no value the field can take (*why says why)
		EditRefused    // the edit was made and the model or the store refused it: *editAnswer says which, and *why the model's reason
	};

	// A field of a strategy the panel kept: makes the edit (EditFromPanel, with the flags word read from the store's bytes) and applies it
	// (ApplyStrategyPayloadEdit). record is the record the panel was loaded from, kind the strategy's kind. *editAnswer (if not null) is the
	// apply's own answer when it gives EditRefused or Applied; *why as above. Changes nothing unless it gives Applied.
	KeptAnswer ApplyKeptStrategyField(PayloadStore *store, Chunk *chunk, model::StrategyKind const &kind, FieldRecord const &record,
	                                  ui::FieldPanel const &panel, size_t field, EditAnswer *editAnswer, char const **why);

	// The same for a chunk the model has a table for.
	KeptAnswer ApplyKeptChunkField(PayloadStore *store, Chunk *chunk, model::ChunkKind const &kind, FieldRecord const &record,
	                               ui::FieldPanel const &panel, size_t field, EditAnswer *editAnswer, char const **why);

	enum class ApplyLightCouplingAnswer
	{
		NotCoupled,   // the field just changed has no coupled change in inteng (or none is needed now): nothing was done
		Applied,      // the engine word got the Omni bit, as inteng's No PreLight (on) leaves it when saved
		Refused       // the bytes couldn't be read or the edit was refused: nothing was done
	};

	// After a STDLIGHT's field was changed from the panel: the change inteng makes with it (No PreLight on also sets the engine word's Omni bit in what it saves;
	// FUN_004acf30 / FUN_004ad630, F-LIGHT-COUPLING). Call it after ApplyKeptChunkField gave Applied; field is the panel's field.
	ApplyLightCouplingAnswer ApplyLightCoupling(PayloadStore *store, Chunk *chunk, model::ChunkKind const &kind, FieldRecord const &record, size_t field);
}
