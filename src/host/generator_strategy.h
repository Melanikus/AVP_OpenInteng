// generator_strategy.h: an ENEMY's or a GENERATOR's strategy (Strategy Data's Assign, Delete and the views' list edits on kinds 69 Enemy and 79 Generator), written into the level's bytes.
// Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE IT LIVES: an enemy or a generator is an AVPGENER chunk in REBENVDT > SPECLOBJ (special_objects_edit.h). Its strategy is not inside it: it is the AVPSTRAT child of an AVPGENEX
// chunk, also in SPECLOBJ, whose AVPGENNM child holds the same name (avpx avp_chunk.cpp AVP_Generator_Chunk::get_extra_data_chunk: the FIRST AVPGENEX with that name; the game reads it so,
// object_setup.cpp, and checks the strategy's kind before it uses it). inteng's Assign makes the holder when there is none (the enemy wrapper's +0xc, FUN_0053a2c0; S-K69-0, S-K79-0).
// HERE: the strategy put in (a new one or given bytes) replaces the AVPSTRAT of that AVPGENEX, or goes in as its strategy; when no AVPGENEX has the name, one is made at the end of SPECLOBJ
// with an AVPGENNM (the name, its zero, padded to four bytes as the game writes it) and the AVPSTRAT. Delete takes the AVPSTRAT out and leaves the AVPGENEX (its other children, such as
// GENEXSET, belong to the generator). The game's loader reads the result before it is swapped in; after a success every Chunk * the editor held is gone, and the generator's ID is the same.
// The bytes are hostile: every size is checked as they are walked (level_image.h), a name over 255 letters and a strategy over 64 KB are refused.
#pragma once

#include <stddef.h>
#include <stdint.h>

class OpenLevelFile;   // src/io/level_file.h

namespace host
{
	enum class GeneratorStrategyResult
	{
		Done,
		BadCall,         // no level, no ID, or no bytes to put
		NotFound,        // no enemy or generator has that ID
		BadKind,         // the kind is one the editor can't make (model::MakeNewStrategy)
		TooBig,          // the strategy is over 64 KB, or the name over 255 letters
		NoStrategy,      // Delete: the enemy or generator has no strategy
		CantBuild,       // the level's bytes could not be walked or made again
		LoaderRefused,   // the game's loader would not take the changed level: nothing changed
	};

	// A new strategy of that kind (its creation values) in place of the one it has, if any
	GeneratorStrategyResult GeneratorStrategy_Assign(OpenLevelFile *file, uint8_t const *id, int32_t kind);

	// These payload bytes (the AVPSTRAT chunk's, after its 12-byte heading) as its strategy
	GeneratorStrategyResult GeneratorStrategy_Put(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length);

	// Its strategy taken out
	GeneratorStrategyResult GeneratorStrategy_Delete(OpenLevelFile *file, uint8_t const *id);

	char const *GeneratorStrategyWords(GeneratorStrategyResult result);
}
