// The small pieces every model rule shares: reading and writing little-endian numbers one byte at a time, and the setters that put
// a checked value at a known place. Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// This header is for src/model's own files. Everything outside the model uses field_model.h.
//
// Reads and writes go one byte at a time, so the result is the same on any machine, and nothing is read through a pointer of the
// wrong type. Every caller has already checked that the bytes are inside the payload.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "field_model.h"

namespace model
{
	namespace detail
	{
		inline uint32_t LoadUInt32(uint8_t const *at)
		{
			return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
		}

		inline int32_t LoadInt32(uint8_t const *at)
		{
			uint32_t const bits = LoadUInt32(at);
			int32_t value;
			memcpy(&value, &bits, sizeof(value));
			return value;
		}

		inline void StoreUInt32(uint8_t *at, uint32_t value)
		{
			at[0] = (uint8_t)(value & 0xFF);
			at[1] = (uint8_t)((value >> 8) & 0xFF);
			at[2] = (uint8_t)((value >> 16) & 0xFF);
			at[3] = (uint8_t)((value >> 24) & 0xFF);
		}

		inline void StoreInt32(uint8_t *at, int32_t value)
		{
			uint32_t bits;
			memcpy(&bits, &value, sizeof(bits));
			StoreUInt32(at, bits);
		}

		// Reads a list's count at payload offset at, as the game's ReadListCount does (3dc/win95/chunk.hpp): the count has to be
		// there, not negative, and no more than the bytes after it could hold at item_bytes each. The game refuses the level otherwise,
		// and this says which of the three it was.
		SizeCheck ReadCount(uint8_t const *payload, size_t length, size_t at, size_t item_bytes, size_t *count);

		// Puts a checked value into a field whose place in the payload is already known (at). Each gives back false, changing
		// nothing, if the value isn't allowed (*why says why), the field doesn't fit at that place, or the field's storage isn't the
		// setter's: SetFieldAt takes every storage but a field of three numbers or an object, which have their own.
		bool SetFieldAt(Field const &field, uint8_t *payload, size_t length, size_t at, double value, char const **why);
		bool SetAxisAt(Field const &field, uint8_t *payload, size_t length, size_t at, int axis, int32_t value, char const **why);
		bool SetObjectAt(Field const &field, uint8_t *payload, size_t length, size_t at, uint32_t first, uint32_t second);

		// Reads a field from a place already known (at): the inverses of the three above, for ReadField and ReadChunkField. Each gives back
		// false, with what it was given to fill left alone, if the payload or the place to put the value isn't there, the field doesn't fit
		// at that place, or its storage isn't the reader's: ReadFieldAt takes every storage but a field of three numbers, an object or an
		// orientation, which it refuses. What's read is the level's, so it's hostile, and it's given back exactly as the file holds it (a
		// fraction may be NaN, and a masked whole number is given with its mask applied, not shifted).
		bool ReadFieldAt(Field const &field, uint8_t const *payload, size_t length, size_t at, double *value);
		bool ReadAxisAt(Field const &field, uint8_t const *payload, size_t length, size_t at, int axis, int32_t *value);
		bool ReadObjectAt(Field const &field, uint8_t const *payload, size_t length, size_t at, uint32_t *first, uint32_t *second);

		// Writes a field's creation value at a known place, for a new chunk: a field of three numbers gets it in each of its numbers,
		// and an object field names no object. Gives back false, writing nothing, if the field doesn't fit at that place.
		bool WriteCreationAt(Field const &field, uint8_t *payload, size_t length, size_t at);
	}
}
