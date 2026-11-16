// light_intensities_edit.h: Light Sourcing's Save Intensities for Game (0xc) and for Tools (0xd): the lighting calculation (light_scene.h) is run for every module and each lit module's SHPVTINT chunk of the set
// NORMALLT is made again. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-lighting-facts.md.
//
// WHAT inteng DOES (FUN_004aef90): the lights with LOFlag_Changed are made again; for the GAME's save the lights that are "No PreLight" (local flag 0x40) are switched OFF (for the tools' save they are on); then for every
// module that has a shape: its old SHPVTINT chunks are destroyed, and when a light reaches the shape (FUN_004fe730 made it a contribution array) a new SHPVTINT chunk named NORMALLT is made inside the module's object,
// holding for each point red << 16 | green << 8 | blue. Afterwards the lights are put back as they were. The doors are as they are (Doors > Close / Open): the stock levels were saved with them closed.
//
// WHAT THIS DOES: the same, on the bytes of the open level: all the changed objects are rebuilt in ONE new image and read by the game's own loader (object_edit.h SetObjectChildrenMany); if the loader refuses it
// nothing is changed. The new chunk goes at the END of the object's children, as the game's tools put a new chunk. The lights' own chunks are not touched (the on / off state is the editor's: light_state.h).
//
// NOT HERE: a shape whose header flag 0x1000 is set (inteng skips it; no stock shape has it among the modules tried), the placed lights' direction (every stock placed light is Omni or No PreLight).
#pragma once

#include <stddef.h>

#include <vector>

#include "light_scene.h"
#include "light_state.h"

class OpenLevelFile;

namespace host
{
	enum class SaveIntensitiesResult
	{
		Done,
		NothingOpen,     // no level is open
		NoLightSet,      // the level has no light set named NORMALLT
		CantBuild,       // the level could not be put together
		LoaderRefused    // the game's own loader would not take the changed level, so nothing was changed
	};

	struct SaveIntensitiesCounts
	{
		SaveIntensitiesCounts() : modules(0), written(0), removed(0), vertices(0), lightsOff(0) {}

		size_t modules;    // modules with a shape the calculation read
		size_t written;    // SHPVTINT chunks made
		size_t removed;    // modules whose old chunk was taken out and not made again (no light reaches them)
		size_t vertices;   // vertices written
		size_t lightsOff;  // lights that were off (by the state, or No PreLight for the game's save)
	};

	char const *SaveIntensitiesWords(SaveIntensitiesResult result);

	// Save Intensities for Game (toolsSave false) or for Tools (true), with the lights on or off as `state` holds them and the doors as it holds them
	SaveIntensitiesResult SaveIntensities(OpenLevelFile *open, LightState const &state, bool toolsSave, SaveIntensitiesCounts *counts, std::vector<LitObject> *written = nullptr);
}
