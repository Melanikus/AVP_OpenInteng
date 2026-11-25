// The paths' forms (strategy_forms.h): 72 / 80 Track Object, 76 Placed Hierarchy, 69 Enemy, 79 Generator, and the E extra-data pages. Written 2026-11-24 by Melanikus
// (Vasquez, the empty start; Frost, the rows) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each row writes what inteng's own page and windows write (strategy_views.cpp EnemyLines / GeneratorLines, strategy_screen.cpp MissionTypeWindow and SelectPathWindow): the same
// fields, the same values, the same words. Only the look is ours (the owner, 2026-11-24: "our pretty drop downs").

#include "strategy_forms.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "strategy_forms_paths.h"   // the editor's answers: the selected object's species and track sections, the level's paths
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

		// ---- 72 / 80 Track Object, 76 Hierarchy Object (TrackLines / HierarchyLines): the point effects (76: the key frame flag effects, the same list in the file) as a list
		// of lists: a line per effect ("Point 3" / "Flag 3") with its targets under it, each target's object, on / off (request bit 0), direction and Extra data. The record is
		// read and written whole (StrategyView_TrackEffects / StrategyView_TrackWith, the game's ReadPointEffects layout, bounds checked).
		//   "+" asks the number as P does ("Which track point?" 0 to the track's sections; "Which flag?" 0 to 31: TrackAskPoint); the list is kept sorted by the number and a
		//   number already there adds nothing, as P's own (S-K72-2, S-K76-2).
		//   A target added is on, both directions (TrackTargetsEnd: request 1, direction 3); the directions are the page's cycle (Forward only 1, Backward only 2, Opposite
		//   back 7, Both directions 3: TrackChangeTarget).
		bool EffectsRead(std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> *entries)
		{
			std::vector<TrackEffect> effects;

			if (!StrategyView_TrackEffects(payload, &effects))
			{
				return false;
			}

			for (TrackEffect const &effect : effects)
			{
				StrategyFormEntry entry;

				entry.word = (uint32_t)effect.point;

				for (TrackTarget const &target : effect.targets)
				{
					StrategyFormEntry one;

					one.id = target.id;
					one.word = (uint32_t)target.request;
					one.choice = target.direction;
					entry.sub.push_back(one);
				}

				entries->push_back(entry);
			}

			return true;
		}

		bool EffectsWrite(std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> const &entries, std::vector<uint8_t> *bytes)
		{
			std::vector<TrackEffect> had, effects;

			if (!StrategyView_TrackEffects(payload, &had))
			{
				return false;   // a record whose counts don't fit is not changed (as the page's edits)
			}

			for (StrategyFormEntry const &entry : entries)
			{
				int32_t const point = (int32_t)entry.word;
				bool there = false;

				for (TrackEffect const &effect : effects)
				{
					there = there || effect.point == point;
				}

				if (there)
				{
					continue;   // a number already there adds nothing (the first, with its targets, is kept)
				}

				TrackEffect effect;

				effect.point = point;

				for (TrackEffect const &old : had)
				{
					if (old.point == point)
					{
						effect.pad = old.pad;   // its pad word as the record had it (a new one: 0, as P's)
						break;
					}
				}

				for (StrategyFormEntry const &one : entry.sub)
				{
					TrackTarget target;

					target.id = one.id;
					target.request = (int32_t)one.word;
					target.direction = one.choice;
					effect.targets.push_back(target);
				}

				effects.push_back(effect);
			}

			// kept sorted by the number, as P puts a new one in its place (TrackAskPoint)
			std::stable_sort(effects.begin(), effects.end(), [](TrackEffect const &a, TrackEffect const &b) { return a.point < b.point; });
			return StrategyView_TrackWith(payload, effects, bytes);
		}

		StrategyFormRow EffectsRow(int32_t kind)
		{
			bool const flags = kind == 76;
			std::shared_ptr<StrategyFormRow> const targets = std::make_shared<StrategyFormRow>();
			StrategyFormRow row;

			targets->kind = StrategyFormRowKind::List;
			targets->label = "Targets";
			targets->entryChecks = { { "On", 1u } };
			targets->entryChoices = { { "Forward only", 1 }, { "Backward only", 2 }, { "Opposite back", 7 }, { "Both directions", 3 } };
			targets->newWord = 1u;
			targets->newChoice = 3;
			targets->entryExtra = true;   // the page's E on a target line

			row.kind = StrategyFormRowKind::List;
			row.label = flags ? "Key Frame Flag Effects" : "Point Effects";   // 0x5efb0c, 0x5efc44
			row.entryWords = [flags](StrategyFormEntry const &entry)
			{
				return std::string(flags ? "Flag " : "Point ") + std::to_string((long long)(int32_t)entry.word);
			};
			row.newAsk = flags ? "Which flag?" : "Which track point?";   // 0x5efae8, 0x5efb98
			row.newNumberRefused = [flags](std::vector<uint8_t> const &, int32_t number) -> std::string
			{
				// P takes 0 <= n < 32 for a flag, 0 <= n < the track's sections + 1 for a point, and silently nothing else (NAMED: the form says why)
				int32_t const most = flags ? 31 : StrategyFormsPaths_SelectedTrackSections();

				return number >= 0 && number <= most ? std::string() : (flags ? std::string("A flag is 0 to 31") : "A track point is 0 to " + std::to_string((long long)most));
			};
			row.readList = EffectsRead;
			row.writeList = EffectsWrite;
			row.subRow = targets;
			return row;
		}

		// ---- 80 Track Object (Destructable) adds (TrackLines' main page): Integrity, the destruct target's on / off (target_request bit 0) and the Destruct Target
		void DestructRows(std::vector<StrategyFormRow> *rows)
		{
			StrategyFormRow integrity;

			integrity.kind = StrategyFormRowKind::Number;
			integrity.label = "Integrity";
			integrity.field = "integrity";
			integrity.lowest = 0.0;   // NAMED: inteng makes a negative number 0 ("Integrity for object", 0x5efb80); the form refuses it with the range shown
			integrity.highest = 2147483647.0;
			rows->push_back(integrity);

			StrategyFormRow on;

			on.kind = StrategyFormRowKind::Check;
			on.label = "Destruct Target on";
			on.field = "target_request";
			on.mask = 0x1;
			rows->push_back(on);

			StrategyFormRow target;

			target.kind = StrategyFormRowKind::Target;
			target.label = "Destruct Target";
			target.field = "targetID";
			target.pickRow = kPickTarget;
			target.request = "target_request";
			target.pickWords = "Select destruct target";   // 0x5efd08, FUN_005353c0
			target.extra = true;                          // the page's E
			rows->push_back(target);
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
		else if (kind == 72 || kind == 80 || kind == 76)
		{
			built.push_back(EffectsRow(kind));

			if (kind == 80)
			{
				DestructRows(&built);
			}
		}
		else
		{
			return false;
		}

		if (rows)
		{
			*rows = built;
		}

		return true;
	}

	// THE E PAGES as forms: not yet. DEBT (Bishop 2026-11-24, alpha.7: the kinds first, the E pages their own MVP right after): every target's "Extra data..." button runs
	// inteng's own E page (StrategyRequestE: lines that a click changes); its rows per page (placed object, switch, mission, track, message, enemy, placed hierarchy) go here,
	// on the field "request.data" (strategy_forms.h)
	bool StrategyFormRows_Extra(RequestPage, RequestPageState const &, std::vector<StrategyFormRow> *)
	{
		return false;
	}
}
