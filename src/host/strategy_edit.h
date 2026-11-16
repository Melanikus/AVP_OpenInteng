// strategy_edit.h: from an object's ID to its strategy's bytes in the store, and an edit of one field of them. Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng (the edit path, S3b). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The selection names an object (selection.h); strategy_lookup.h finds its AVPSTRAT chunk in the open level; field_edit.h edits the bytes the
// store holds for a chunk. This joins them: ApplyStrategyEdit looks the chunk up by the object's ID and edits its bytes, and
// LookUpStrategyBytes gives the strategy's bytes AS THE STORE HOLDS THEM NOW, for the panel's record: after an edit they're the edited bytes,
// where the game's class for the chunk (which FindObjectStrategy copies from) still says what the file held before.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "field_edit.h"
#include "strategy_lookup.h"

class File_Chunk;

namespace host
{
	// Edits one field of the strategy of the object whose ID is the 8 bytes at id, in level, through store. kind is the number of the kind the
	// edit was made for (the record's own). *lookup (if not null) is what the lookup said; anything but Found gives NoPayload, with nothing
	// changed. Every other answer is field_edit.h's.
	EditAnswer ApplyStrategyEdit(File_Chunk *level, uint8_t const *id, int32_t kind, FieldEdit const &edit, PayloadStore *store, char const **why,
	                             StrategyFound *lookup);

	// The object's strategy, as FindObjectStrategy gives it (the chunk, for the edit path to name), but with the payload the STORE holds now.
	// Anything but Found leaves *out empty, as FindObjectStrategy does; a chunk the store has no bytes for gives NoStrategyBytes.
	StrategyFound LookUpStrategyBytes(File_Chunk *level, PayloadStore const &store, uint8_t const *id, ObjectStrategy *out);
}
