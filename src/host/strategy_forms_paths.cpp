// The paths' forms (strategy_forms.h): 72 / 80 Track Object, 76 Placed Hierarchy, 69 Enemy, 79 Generator, and the E extra-data pages. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Frost, the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each row writes what inteng's own page and windows write (strategy_views.cpp EnemyLines / GeneratorLines, strategy_screen.cpp MissionTypeWindow and SelectPathWindow): the same
// fields, the same values, the same words. Only the look is ours (the owner, 2026-11-24: "our pretty drop downs").

#include "strategy_forms.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "strategy_forms_paths.h"   // the editor's answers: the selected object's species, the level's paths
#include "strategy_views.h"         // StrategyView_FieldNumber, kPickTarget

namespace host
{
	namespace
	{
		int32_t const kMarine = 7;     // an enemy's species (its AVPGENER type): a marine has six missions
		int32_t const kPredator = 2;   // a predator two; any other species one, and no Mission line on its page

		// The selected object's species, -1 when nothing is selected or it is no enemy. DEBT: the rows are built from the kind and the payload only (StrategyForm_Has must answer
		// without a record), so the species comes from the selection, as the page's own click reads it (strategy_screen.cpp, kWindowEnemyMission)
		int32_t SelectedSpecies()
		{
			return StrategyFormsPaths_SelectedSpecies();
		}

		int32_t FieldOf(int32_t kind, std::vector<uint8_t> const &payload, char const *field)
		{
			int32_t value = 0;

			return StrategyView_FieldNumber(kind, payload, field, &value) ? value : 0;
		}

		// An enemy's path follower: a marine's mission 5, a predator's 1 (strategy_views.cpp EnemyFollowsAPath)
		bool EnemyFollowsAPath(int32_t species, int32_t mission)
		{
			return (species == kMarine && mission == 5) || (species == kPredator && mission == 1);
		}

		// Select Path's rows: one per path of the level, "<number>  <name>" as inteng's list (FUN_004fb6a0). Each row's value is the WHOLE ExtraMissionData that inteng's pick
		// writes, (old & 0x0ffff000) | the path's number (SelectPathWindow, 0x531a63 / 0x531ef2), so picking one writes exactly that.
		// DEBT: the values are made as the form opens. A mission changed away and back in one form keeps the old bits 12..27 where inteng's 0 would have cleared them; the
		// view's end (EnemyPathFixAtEnd) rewrites bits 16 and up, so only an old path number of 4096 or more could come out different
		std::vector<StrategyFormChoice> PathChoices(int32_t kind, std::vector<uint8_t> const &payload)
		{
			std::vector<StrategyFormChoice> choices;
			int32_t extra = 0;

			if (!StrategyView_FieldNumber(kind, payload, "ExtraMissionData", &extra))
			{
				return choices;
			}

			for (std::pair<int32_t, std::string> const &one : StrategyFormsPaths_LevelPaths())
			{
				StrategyFormChoice choice;

				choice.words = std::to_string((long long)one.first) + "  " + one.second;
				choice.value = (int32_t)(((uint32_t)extra & 0x0ffff000u) | (uint32_t)one.first);
				choices.push_back(choice);
			}

			return choices;
		}

		// Select Mission Type's buttons, by species (strategy_screen.cpp kWindowEnemyMission: FUN_00531fb0 a marine's, FUN_00532230 a predator's)
		std::vector<StrategyFormChoice> EnemyMissions(int32_t, std::vector<uint8_t> const &)
		{
			if (SelectedSpecies() == kMarine)
			{
				return { { "Wait then wander", 0 }, { "Wander", 1 }, { "Guard", 2 }, { "Local Guard", 4 }, { "Noncombatant", 3 }, { "Path follower", 5 } };
			}

			return { { "Wait then wander", 0 }, { "Path follower", 1 } };
		}

		// The Mission row: a changed mission also sets ExtraMissionData to 0, as the window does (MissionTypeWindow)
		StrategyFormRow MissionRow()
		{
			StrategyFormRow row;

			row.kind = StrategyFormRowKind::Choice;
			row.label = "Mission";
			row.field = "MissionType";
			row.alsoOnChange = { { "ExtraMissionData", 0.0 } };
			return row;
		}

		StrategyFormRow PathRow()
		{
			StrategyFormRow row;

			row.kind = StrategyFormRowKind::Choice;
			row.label = "Path";
			row.field = "ExtraMissionData";
			row.choicesFrom = PathChoices;
			return row;
		}

		// ---- 69 Enemy (EnemyLines): Mission (a marine's or a predator's only), Path (a path follower's), the Death Target's on / off (request bit 0) and the Death Target
		void EnemyRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow mission = MissionRow();

			mission.choicesFrom = EnemyMissions;
			mission.shown = [](int32_t, std::vector<uint8_t> const &)
			{
				int32_t const species = SelectedSpecies();

				return species == kMarine || species == kPredator;
			};
			rows->push_back(mission);

			StrategyFormRow path = PathRow();

			path.shown = [](int32_t kind, std::vector<uint8_t> const &payload)
			{
				return EnemyFollowsAPath(SelectedSpecies(), FieldOf(kind, payload, "MissionType"));
			};
			rows->push_back(path);

			StrategyFormRow on;

			on.kind = StrategyFormRowKind::Check;
			on.label = "Death Target on";
			on.field = "request";
			on.mask = 0x1;
			rows->push_back(on);

			StrategyFormRow target;

			target.kind = StrategyFormRowKind::Target;
			target.label = "Death Target";
			target.field = "DeathTarget";
			target.pickRow = kPickTarget;
			target.request = "request";
			target.pickWords = "Select target";   // the page's pick (EnemyClick: 0x531a38, 0x5e4048)
			target.extra = true;                  // the page's E: "Edit extra message data for target"
			rows->push_back(target);
		}

		// ---- 79 Generator (GeneratorLines): Mission (Hunt, Path follower) and the Path (a path follower's)
		void GeneratorRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow mission = MissionRow();

			mission.choices = { { "Hunt", 0 }, { "Path follower", 1 } };
			rows->push_back(mission);

			StrategyFormRow path = PathRow();

			path.shown = [](int32_t kind, std::vector<uint8_t> const &payload)
			{
				return FieldOf(kind, payload, "MissionType") == 1;
			};
			rows->push_back(path);
		}
	}

	bool StrategyFormRows_Paths(int32_t kind, std::vector<StrategyFormRow> *rows)
	{
		std::vector<StrategyFormRow> built;

		if (kind == 69)
		{
			EnemyRows(&built);
		}
		else if (kind == 79)
		{
			GeneratorRows(&built);
		}
		else
		{
			return false;   // 72, 80, 76 keep inteng's page until their list rows are done
		}

		if (rows)
		{
			*rows = built;
		}

		return true;
	}

	// THE E PAGES as forms: not yet; every target's Extra data keeps inteng's page (strategy_forms.h)
	bool StrategyFormRows_Extra(RequestPage, RequestPageState const &, std::vector<StrategyFormRow> *)
	{
		return false;
	}
}
