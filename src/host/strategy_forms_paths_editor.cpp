// strategy_forms_paths_editor.cpp: the editor's answers to the paths' form rows (strategy_forms_paths.h): the selected object's species and the level's paths, read where the
// pages read them (strategy_screen.cpp: EnemySpeciesOf, Select Path's list). Only the editor builds this file; the forms' unit test has its own stand-ins. Written 2026-11-24
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_forms_paths.h"

#include "editor_shared.h"    // SelectedObjectId, EnemySpeciesOf
#include "level_objects.h"    // LevelObjects_OpenForEdit
#include "list_order.h"       // ListOrder: a list window's order
#include "path_mode.h"        // PathMode_ReadPaths

namespace host
{
	int32_t StrategyFormsPaths_SelectedSpecies()
	{
		uint8_t id[8];

		return SelectedObjectId(id) ? EnemySpeciesOf(id) : -1;
	}

	// In Select Path's order: inteng's list window sorts its rows, "<number>  <name>", byte by byte (FUN_00475bf0; dialogs.cpp Dialogs_List and list_order.h), so "0  smart01"
	// comes before "1  genpath01" whatever the file's order
	std::vector<std::pair<int32_t, std::string>> StrategyFormsPaths_LevelPaths()
	{
		std::vector<std::pair<int32_t, std::string>> numbered;
		std::vector<std::string> rows;
		std::vector<PathInfo> paths;
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (open && PathMode_ReadPaths(open, &paths))
		{
			for (PathInfo const &one : paths)
			{
				rows.push_back(std::to_string((long long)one.number) + "  " + one.name);
			}

			for (size_t const place : ListOrder(rows))
			{
				numbered.push_back({ paths[place].number, paths[place].name });
			}
		}

		return numbered;
	}
}
