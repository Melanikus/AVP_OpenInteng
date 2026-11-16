// The rules for the chunks the editor edits directly (not strategies): how big each one has to be, and setting a field. Written
// 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A level can come from anyone, so a payload is read as hostile: its size is checked as the game's reader checks it before any
// field is read, and every setter checks that its field fits in the payload before writing.

#include <string.h>

#include "field_bytes.h"
#include "field_model.h"

namespace model
{
	using namespace detail;

	namespace
	{
		// A place no field fits at, for the setters to refuse.
		size_t const NoPlace = (size_t)-1;

		// Whether the field is one of that kind's own, so a field of another kind can't be written at its own offset here.
		bool IsFieldOf(ChunkKind const &kind, Field const &field)
		{
			for (size_t i = 0; i < kind.fieldCount; i++)
			{
				if (&kind.fields[i] == &field)
				{
					return true;
				}
			}

			return false;
		}

		// Where the numbers begin in a payload that starts with a name: after the name's letters, its zero, and the zeros that pad it to a
		// multiple of 4 bytes (the game steps over "the length plus 4, rounded down to a multiple of 4": CHUNK_EXTRACT_STRING and the
		// chunks that read it by hand, chunk_fragments.cpp:48). Gives back false when the name has no zero inside the payload, which the
		// game refuses (CopyNameFromChunk). The place can be past the end of the payload, when the padding runs off it: whatever the game
		// reads from there is refused too, so the callers measure it against the length.
		bool AfterLeadingName(uint8_t const *payload, size_t length, size_t *place)
		{
			uint8_t const *const zero = length ? (uint8_t const *)memchr(payload, 0, length) : 0;

			if (!zero)
			{
				return false;
			}

			// The zero is inside the payload, so the letters before it are fewer than its length and this sum can't wrap.
			*place = ((size_t)(zero - payload) + 4) & ~(size_t)3;
			return true;
		}
	}

	bool ChunkFieldPlace(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, size_t *place)
	{
		// Every chunk field is counted from the start of its payload, or from the end of its leading name, or, after a list, from the
		// list's end (the test checks the tables)
		if (!IsFieldOf(kind, field) || (field.anchor != Anchor::Start && field.anchor != Anchor::AfterFirstList))
		{
			return false;
		}

		size_t origin = 0;

		if (kind.nameFirst && !AfterLeadingName(payload, length, &origin))
		{
			return false;
		}

		if (field.anchor == Anchor::AfterFirstList)
		{
			// The count has to be readable and fit, as the game's ReadListCount insists, before the list's end can be known. It is then
			// inside the payload, because the count was measured against the bytes there.
			size_t count = 0;

			if (kind.listCountAt < 0 || ReadCount(payload, length, origin + (size_t)kind.listCountAt, kind.listItemBytes, &count) != SizeCheck::Right)
			{
				return false;
			}

			origin += (size_t)kind.listCountAt + 4 + count * kind.listItemBytes;
		}

		// Measured without adding past the end, so a place can't wrap around
		if (origin > length || field.offset > length - origin || length - origin - field.offset < StorageBytes(field.storage))
		{
			return false;
		}

		*place = origin + field.offset;
		return true;
	}

	SizeCheck CheckChunkSize(ChunkKind const &kind, uint8_t const *payload, size_t length)
	{
		// The game doesn't read a chunk shorter than its registered smallest size: it keeps it as unknown, and refuses the level. This is
		// the only check of the fixed part as such: for a kind with no list and no self-sized block, leastBytes is the whole fixed part
		// (the test checks the tables), a list's count is checked below, and a self-sized block may be shorter than the editor writes.
		if (length < kind.leastBytes)
		{
			return SizeCheck::TooShort;
		}

		// Where the numbers begin: at the start of the payload, or, for a kind that starts with a name, after the name. The game reads the
		// name, steps over its padding, and then reads each number with ReadFromChunk, which refuses the level when the number isn't
		// there: so the name has to end inside the payload, and the numbers after it fit. A kind with a list has its count checked
		// below, which gives the reasons of a list.
		size_t origin = 0;

		if (kind.nameFirst)
		{
			if (!AfterLeadingName(payload, length, &origin))
			{
				return SizeCheck::NameCutOff;
			}

			if (kind.listCountAt < 0 && (origin > length || length - origin < kind.fixedBytes))
			{
				return SizeCheck::TooShort;
			}
		}

		// A list's count has to be there, not negative, and no more than the bytes after it hold (the game's ReadListCount). The numbers
		// after the last item are read one at a time with ReadFromChunk, which refuses the level when one isn't there.
		if (kind.listCountAt >= 0)
		{
			size_t count = 0;
			size_t const at = origin + (size_t)kind.listCountAt;
			SizeCheck const check = ReadCount(payload, length, at, kind.listItemBytes, &count);

			if (check != SizeCheck::Right)
			{
				return check;
			}

			// The count was measured against the bytes after it, so the end of the items is inside the payload
			return length - (at + 4 + count * kind.listItemBytes) >= kind.trailerBytes ? SizeCheck::Right : SizeCheck::TooShort;
		}

		// A block whose first number is its own size has to fill the rest of the payload exactly (GENEXSET: avp_chunk.cpp:285-290).
		// leastBytes already ends after that number, so it's there to read.
		if (kind.sizeFieldAt >= 0)
		{
			size_t const at = (size_t)kind.sizeFieldAt;
			int64_t const stored = LoadInt32(payload + at);

			return stored == (int64_t)(length - at) ? SizeCheck::Right : SizeCheck::WrongSize;
		}

		// Names one after another, each ended by a zero byte (the game's CopyNameFromChunk and CopyOptionalNameFromChunk). One
		// the game needs has to end inside the payload; one it can do without may be missing, but not cut off.
		size_t at = kind.fixedBytes;

		for (unsigned name = 0; name < kind.namesRequired + kind.namesOptional; name++)
		{
			if (name >= kind.namesRequired && at >= length)
			{
				break;
			}

			uint8_t const *const zero = at < length ? (uint8_t const *)memchr(payload + at, 0, length - at) : 0;

			if (!zero)
			{
				return SizeCheck::NameCutOff;
			}

			at = (size_t)(zero - payload) + 1;
		}

		// A list of names after those, read one after another until an empty one or the end of the chunk (chunk_palette.cpp:610-624)
		while (kind.namesToEmpty && at < length && payload[at] != 0)
		{
			uint8_t const *const listZero = (uint8_t const *)memchr(payload + at, 0, length - at);

			if (!listZero)
			{
				return SizeCheck::NameCutOff;
			}

			at = (size_t)(listZero - payload) + 1;
		}

		return SizeCheck::Right;
	}

	char const *ChunkSizeCheckWords(SizeCheck check)
	{
		switch (check)
		{
		case SizeCheck::Right: return "the chunk is a size the game reads";
		case SizeCheck::TooShort: return "the chunk is shorter than the game reads";
		case SizeCheck::CountCutOff: return "the chunk ends before its list's count";
		case SizeCheck::CountNegative: return "the chunk's list has a count below zero";
		case SizeCheck::CountTooBig: return "the chunk's list counts more items than the chunk holds";
		case SizeCheck::WrongSize: return "the chunk isn't the size its block says it is";
		case SizeCheck::NameCutOff: return "a name at the end of the chunk has no ending zero inside it";
		}

		return "the chunk's size can't be checked";
	}

	bool SetChunkField(ChunkKind const &kind, Field const &field, uint8_t *payload, size_t length, double value, char const **why)
	{
		size_t at = NoPlace;

		// A null payload is checked first: finding a place can read the payload (a leading name, a list's count), and storing would write
		// through it. A field with no place is still given its value's reason when the value is wrong, so the caller can show it
		if (!payload || !ChunkFieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return SetFieldAt(field, payload, length, at, value, why);
	}

	bool SetChunkFieldAxis(ChunkKind const &kind, Field const &field, uint8_t *payload, size_t length, int axis, int32_t value,
	                       char const **why)
	{
		size_t at = NoPlace;

		// A null payload is checked first, as in SetChunkField
		if (!payload || !ChunkFieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return SetAxisAt(field, payload, length, at, axis, value, why);
	}

	// The readers: the setters' inverses, with the same rules as the strategy's (see field_model.h). A field that isn't one of the kind's
	// has no place (NoPlace), and a place that's too far on never fits, so every refusal is the one test in ReadFieldAt.
	bool ReadChunkField(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, double *value, char const **why)
	{
		(void)why;   // never set: see field_model.h
		size_t at = NoPlace;

		// A field with no place (not the kind's, or one that doesn't fit this payload) is read from nowhere, which ReadFieldAt refuses. A null
		// payload is checked first: finding a place can read the payload (a leading name, a list's count)
		if (!payload || !ChunkFieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return ReadFieldAt(field, payload, length, at, value);
	}

	bool ReadChunkFieldAxis(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, int axis, int32_t *value,
	                        char const **why)
	{
		(void)why;   // never set: see field_model.h
		size_t at = NoPlace;

		if (!payload || !ChunkFieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return ReadAxisAt(field, payload, length, at, axis, value);
	}

	bool ReadChunkFieldObject(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, uint32_t *first,
	                          uint32_t *second)
	{
		size_t at = NoPlace;

		if (!payload || !ChunkFieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return ReadObjectAt(field, payload, length, at, first, second);
	}

	// The setter for an object field: the same rules, the other way (SetObjectAt checks the field is an object field and fits). A null
	// payload is checked first, as the readers' are: finding a place can read the payload
	bool SetChunkFieldObject(ChunkKind const &kind, Field const &field, uint8_t *payload, size_t length, uint32_t first, uint32_t second)
	{
		size_t place = NoPlace;

		if (!payload || !ChunkFieldPlace(kind, field, payload, length, &place))
		{
			place = NoPlace;
		}

		return SetObjectAt(field, payload, length, place, first, second);
	}
}
