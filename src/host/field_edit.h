// field_edit.h: a value changed in the field panel, checked by the field model and put into the bytes of the chunk it belongs to. Written
// 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (the edit path, S3b). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own
// code".
//
// HOW AN EDIT WORKS. The editor never changes the game's class for a chunk (it has none that takes bytes). It holds the chunk's BYTES, in a
// PayloadStore (OpenLevelFile is one: level_file.h, PayloadOf and SetPayload). An edit takes a copy of the bytes the chunk holds now, lets the
// field model put the new value into the copy (SetField, SetFieldAxis, SetFieldObject, and the chunk setters), each of which refuses a value
// the field can't hold with the field's own reason, and only then gives the whole copy back to the store. So an edit that's refused changes
// nothing, and an edit that's taken changes exactly the bytes of one field.
//
// THE RULES.
// - What the edit names (a field's name) comes from outside, so it's read to at most kFieldEditNameMost bytes and looked up in the model's own
//   tables: a name that's no field of the kind gives NoSuchField, and the bytes are left alone.
// - A strategy edit names the kind it was made for. If the strategy in the store isn't that kind any more, nothing is changed (KindChanged).
// - Whatever the store holds is checked as the game's loader would check its size before a field is changed: a payload of a size the game
//   refuses is never edited (PayloadRefused).
// - The store is asked to keep the new bytes only when the model took the value, and it may say no (NotStored): then nothing has changed.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "field_model.h"

class Chunk;   // the game's chunk library (chunk.hpp): only ever handed on to the store, never read here

namespace host
{
	// Where the bytes of the chunks are kept. OpenLevelFile has these two (level_file.h); a test can have a small one of its own.
	class PayloadStore
	{
	public:
		virtual ~PayloadStore() {}

		// The bytes the chunk holds now, after its 12-byte heading. False, with *payload left alone, for a chunk the store doesn't know.
		virtual bool PayloadOf(Chunk *chunk, std::vector<unsigned char> *payload) const = 0;

		// Replaces the bytes, which are as many as there are now. False, changing nothing, if the store won't.
		virtual bool SetPayload(Chunk *chunk, std::vector<unsigned char> const &payload) = 0;
	};

	// The most bytes of a field's name that are read
	size_t const kFieldEditNameMost = 63;

	// What kind of value an edit puts in
	enum class EditWhat
	{
		Number,   // one number (a number, a flags word, a choice's value, a fraction), as the field stores it, after its mask
		Axis,     // one of a three-number field's numbers: axis 0, 1 or 2
		Object,   // an object field's two halves
	};

	struct FieldEdit
	{
		FieldEdit() : field(0), what(EditWhat::Number), axis(0), number(0), axisValue(0), first(0), second(0) {}

		char const *field;     // the field's name in the model's table, as in field_model.h ("TimeToOpen"): its own name, never its label
		EditWhat what;
		int axis;              // for Axis
		double number;         // for Number: as SetField takes it
		int32_t axisValue;     // for Axis
		uint32_t first;        // for Object: the two halves, as the file stores them
		uint32_t second;
	};

	enum class EditAnswer
	{
		Applied,         // the value is in the bytes, and the store has them
		BadCall,         // no store, no chunk, or no field named
		NoPayload,       // the store has no bytes for that chunk
		PayloadRefused,  // the bytes the store holds aren't a size the game reads, so nothing is edited
		KindChanged,     // the strategy isn't the kind the edit was made for
		UnknownKind,     // the editor has no table for the strategy's kind
		NoSuchField,     // the kind has no field of that name
		ValueRefused,    // the field can't hold that value (*why says why, in the model's own words)
		NotStored,       // the model took the value, and the store wouldn't keep the bytes
		NoMemory,        // there wasn't memory to make the copy
	};

	// What an answer means, in plain words: static text, never null
	char const *EditAnswerWords(EditAnswer answer);

	// Edits one field of a strategy, whose payload (the AVPSTRAT chunk's bytes after its heading: the index and spare number, then the
	// strategy) is what the store holds for chunk. kind is the number of the kind the edit was made for (the int32 at payload +8).
	// *why (if not null) is set to the model's reason for a ValueRefused, and to nothing else.
	EditAnswer ApplyStrategyPayloadEdit(PayloadStore *store, Chunk *chunk, int32_t kind, FieldEdit const &edit, char const **why);

	// The same for a chunk the field model has a table for (field_model.h, ChunkKind): its payload is the whole of what the store holds.
	EditAnswer ApplyChunkEdit(PayloadStore *store, Chunk *chunk, model::ChunkKind const &kind, FieldEdit const &edit, char const **why);
}
