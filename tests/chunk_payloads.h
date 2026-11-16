// chunk_payloads.h: a payload the game's reader accepts, for any kind of chunk the field model has a table for. For the tests of the chunk
// readers and the chunk record, which each used to keep a copy of this. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT THE PAYLOAD IS. Every number is 0, and the payload is as long as the kind's reader needs and no longer (field_model.h, ChunkKind):
// - a kind that starts with a name (nameFirst) has one: a letter, its zero, and zeros up to the next multiple of 4 bytes, so 4 bytes;
// - then the kind's fixed part (its fields, and a list's count when it has one: the list is empty, so no item follows the count);
// - then the numbers the game reads after the list's last item (trailerBytes);
// - then the names the game needs, each just an ending zero (a list of names that ends at an empty name needs no more than that);
// - and a block's own size, put in where the kind has one (sizeFieldAt).
// Every test that builds a payload this way is then in step with ChunkKind's members, in this one place.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "field_model.h"

inline std::vector<uint8_t> ValidChunkPayload(model::ChunkKind const &kind)
{
	size_t const nameBytes = kind.nameFirst ? 4 : 0;
	size_t size = nameBytes + (kind.fixedBytes > kind.leastBytes ? kind.fixedBytes : kind.leastBytes);

	size += kind.trailerBytes;
	size += kind.namesRequired;

	std::vector<uint8_t> bytes(size, 0);

	if (kind.nameFirst)
	{
		bytes[0] = 'a';
	}

	if (kind.sizeFieldAt >= 0)
	{
		int32_t const stored = (int32_t)(bytes.size() - (size_t)kind.sizeFieldAt);

		memcpy(bytes.data() + kind.sizeFieldAt, &stored, sizeof(stored));
	}

	return bytes;
}
