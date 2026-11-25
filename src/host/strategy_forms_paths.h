// strategy_forms_paths.h: the things the paths' form rows (strategy_forms_paths.cpp) ask of the editor, kept apart so the rows build and test without the editor's window
// and level: the selected object's species and track sections, and the level's paths. The editor answers in strategy_forms_paths_editor.cpp; tests/host_strategy_forms_test.cpp
// answers with its own stand-ins (no species, no sections, no paths). Written 2026-11-24 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

namespace host
{
	// The selected object's species (its AVPGENER type: 7 marine, 2 predator ...), -1 when nothing is selected or it is no enemy (EnemySpeciesOf)
	int32_t StrategyFormsPaths_SelectedSpecies();

	// The selected object's track's sections (TrackSectionsOf: OBJTRAK2 +0x28), 0 when it has none: a Track Object's points are 0 to this (TrackAskPoint)
	int32_t StrategyFormsPaths_SelectedTrackSections();

	// The level's paths in Select Path's order (its rows sorted byte by byte, as inteng's list window sorts them): each one's number and name (PathMode_ReadPaths,
	// FUN_004fb6a0); none when no level is open
	std::vector<std::pair<int32_t, std::string>> StrategyFormsPaths_LevelPaths();
}
