// placed_hierarchy_strategy.h: a PLACED HIERARCHY's strategy (Strategy Data's kind 76, Hierarchy Object: Assign, Delete and the view's edits), written into the level's bytes. Written 2026-11-16 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE IT LIVES: a placed hierarchy is a PLACHIER chunk in REBENVDT > SPECLOBJ (special_objects_edit.h); its strategy is the PLACHIER's AVPSTRAT child (avpx hierplace.cpp: the game reads it there and
// refuses a level whose placed hierarchy holds a kind other than 76). inteng's Assign makes the holder when there is none (the wrapper's +0xc, FUN_0053b790: FUN_0051e130 on the PLACHIER; S-K76-W).
// HERE: the strategy put in (a new one or given bytes) replaces the PLACHIER's AVPSTRAT where it stands, or goes in at the end of the PLACHIER; Delete takes it out. The placed hierarchy is the FIRST
// PLACHIER whose PLACHIDT holds the ID (its name, its zero padded to four bytes, the index, the place, the turn, then the 8-byte ID: avpx hierplace.cpp fill_data_block). The game's loader reads the
// result before it is swapped in; after a success every Chunk * the editor held is gone. The bytes are hostile: every size is checked as they are walked (level_image.h); a strategy over 64 KB is refused.
#pragma once

#include <stddef.h>
#include <stdint.h>

class OpenLevelFile;   // src/io/level_file.h

namespace host
{
	enum class PlacedHierarchyStrategyResult
	{
		Done,
		BadCall,         // no level, no ID, or no bytes to put
		NotFound,        // no placed hierarchy has that ID
		BadKind,         // the kind is one the editor can't make (model::MakeNewStrategy)
		TooBig,          // the strategy is over 64 KB
		NoStrategy,      // Delete: it has no strategy
		CantBuild,       // the level's bytes could not be walked or made again
		LoaderRefused,   // the game's loader would not take the changed level: nothing changed
	};

	// These payload bytes (the AVPSTRAT chunk's, after its 12-byte heading) as its strategy
	PlacedHierarchyStrategyResult PlacedHierarchyStrategy_Put(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length);

	// A new strategy of that kind (its creation values) in place of the one it has, if any
	PlacedHierarchyStrategyResult PlacedHierarchyStrategy_Assign(OpenLevelFile *file, uint8_t const *id, int32_t kind);

	// Its strategy taken out
	PlacedHierarchyStrategyResult PlacedHierarchyStrategy_Delete(OpenLevelFile *file, uint8_t const *id);

	char const *PlacedHierarchyStrategyWords(PlacedHierarchyStrategyResult result);
}
