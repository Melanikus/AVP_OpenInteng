// editor_proofs.cpp: THE PROOF RUNS: -allkindsproof, -strategyproof, -trackproof, -modelproof, -sclickproof, -exitproof and -movementproof, each a step a frame. Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
// within what Snyk Code reads (editor_shared.h says why); nothing in it changed. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "3dc.h"
#include "inline.h"
#include "gameplat.h"
#include "platform.h"
#include "renderer.h"
#include "d3_func.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>   // SHGetFolderPathA, CSIDL_PERSONAL: the user's Documents folder, the folder for copies (save_copy.h)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <map>
#include <array>
#include <functional>
#include <math.h>
#include <string>

#include "logString.h"
#include <ctype.h>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "inteng_menus.h"
#include "level_file.h"
#include "field_descriptor_from_model.h"
#include "light_edit.h"
#include "light_paste.h"
#include "light_intensities_edit.h"
#include "light_state.h"
#include "light_pick.h"
#include "level_objects.h"
#include "level_view.h"
#include "uv_project.h"
#include "hierarchy_player.h"
#include "hierarchy_view.h"
#include "object_edit.h"
#include "level_chunks.h"
#include "asc_text.h"
#include "module_links.h"
#include "object_place_edit.h"
#include "object_edit.h"
#include "object_make.h"
#include "object_base.h"
#include "object_type_edit.h"
#include "strategy_mode.h"
#include "strategy_views.h"
#include "small_modules.h"
#include "alt_locations_edit.h"
#include "quick_create.h"
#include "level_store.h"
#include "entry_recalc.h"
#include "record_edit.h"
#include "save_copy.h"
#include "level_save.h"
#include "nobody_watching.h"
#include "key_reader.h"
#include "one_editor.h"
#include "utilities.h"     // SaveFolder_UseThisFolder (SAVE-FOLDER-1)
#include "save_folder.h"   // SaveFolder_WhyNotUsable, for DataFolder too
#include "editor_actions.h"
#include "hidden_objects.h"
#include "poly_counts.h"
#include "path_mode.h"
#include "virtual_object.h"
#include "strategy_edit.h"
#include "strategy_lookup.h"
#include "pick_centre.h"
#include "vertex_pick.h"
#include "editor_bar.h"
#include "inteng_help.h"
#include "click_mode.h"
#include "screenshot_name.h"
#include "place_at_view.h"
#include "tool_buttons.h"
#include "file_pick.h"
#include "move_rel_poly.h"
#include "move_rel_poly_edit.h"
#include "shape_edit.h"
#include "uv_edit.h"
#include "anim_edit.h"
#include "texture_keys.h"
#include "topology_match.h"
#include "texture_anim.h"
#include "polygon_items.h"
#include "module_poly_count.h"
#include "hierarchy_mode.h"
#include "list_picker.h"
#include "dialog_box.h"
#include "dialogs.h"
#include "camera_file.h"
#include "strategy_repair.h"
#include "special_mode.h"
#include "generator_settings.h"
#include "bad_guy_edit.h"
#include "particle_edit.h"
#include "simple_object_edit.h"
#include "editor_icons.h"
#include "inteng_exe.h"
#include "inteng_shapes.h"
#include "virtual_object.h"
#include "generator_strategy.h"
#include "placed_hierarchy_strategy.h"
#include "fix_tears_all.h"
#include "shape_recalc_edit.h"
#include "shape_replace_edit.h"
#include "adjacent_list.h"
#include "adjacent_write.h"
#include "screen_keys.h"
#include "entry_markers.h"
#include "light_keys.h"
#include "reverse_links.h"
#include "autolink_edit.h"
#include "autolink_runner.h"
#include "autolink_scene.h"
#include "module_links_clean.h"
#include "autolink_mlf.h"
#include "adjacency_generate.h"
#include "waypoint_mode.h"
#include "waypoint_view.h"
#include "track_pages.h"
#include "editor_options.h"
#include "shape_chooser.h"
#include "shape_rescale_edit.h"
#include "module_site.h"
#include "shape_soften_edit.h"
#include "module_data_edit.h"
#include "acoustic_mode.h"
#include "shape_merge_edit.h"
#include "poly_edit.h"
#include "module_overlaps.h"
#include "track_edit.h"
#include "sound_edit.h"
#include "placed_sounds_game.h"
#include "track_editor.h"
#include "track_play_game.h"
#include "three_view.h"
#include "field_adapters.h"
#include "field_panel_host.h"
#include "harness_select.h"
#include "key_layout.h"
#include "host_frame.h"
#include "menu_bar.h"
#include "menu_paint.h"
#include "menu_font.h"
#include "object_list.h"
#include "panel_layout.h"
#include "panel_pass.h"
#include "autolink_show.h"
#include "autolink_show_draw.h"   // AutoLink's display: the counter, "Right click to cancel", the owner's grid (reassess/autolink-display-frost.md)
#include "inteng_text.h"          // FUN_0057be40's new lines, tabs and wrapping (view-state-table.md H-1f)
#include "inteng_options.h"       // the start size: -r, IntengOptions.ini, the monitor (startup-screen-frost.md SS-1..SS-13)
#include "texture_window_paint.h"
#include "screen_picture.h"
#include "pick_game.h"
#include "selection.h"
#include "view_surface.h"
#include "view_toggles.h"
#include "view_toggles_game.h"
#include "window_events.h"
#include "open_with.h"

#include "editor_shared.h"

// -strategyproof (a hidden -picture run, nobody at the screen; Strategy mode's MAIN JOB in the running editor): from the fifth frame, a step every second frame, through the editor's own
// functions and the prompts' own answers: pick a placed object (A) -> Assign Strategy (kind 1 of the list) -> Edit a field of its strategy (the field-edit path the panel uses) -> pick a second
// placed object (B) and Copy Strategy from A -> Save a copy -> open the copy through the game's loader and compare both objects' strategies. A picture is asked at each step
// (picture-1..5.png, panels-1..5.png beside the program). One line per step is printed ("AVP_OpenInteng: strategy proof: ..."), and the last says whether the main job works.
// A candidate that refuses a step (a module, an object other objects name) is passed over for the next one. (Added 2026-11-15 by Melanikus (Frost).)
// Presses the bar item of the screen now shown that has these words, the way a click on it does (RunAction with the item's own number): false when the screen has no such item
bool PressTheBarItem(char const *words)
{
	if (barScreen >= host::kIntengScreenCount)
	{
		return false;
	}

	// the item on the bars as they are now (every physical row: the code-built screens keep their second row's items there, and the rows wrap to the window), found by its words and done as a click on it is
	for (size_t row = 0; row < barRowCount; row++)
	{
		for (size_t m = 0; m < barRows.menuCount[row]; m++)
		{
			ui::Menu const &menu = barRows.menus[row][m];

			for (size_t i = 0; i < menu.count; i++)
			{
				if (menu.items[i].name && strcmp(menu.items[i].name, words) == 0)
				{
					RunAction(menu.items[i].action);
					return true;
				}
			}
		}
	}

	// "Menu: Item": an item named by the menu it is in (the code-built pages carry plain item words, so two menus can each have a "Section" or a "Track")
	char const *const colon = strstr(words, ": ");

	if (colon)
	{
		std::string const menuWords(words, (size_t)(colon - words));

		// "Item: Row": a cascade's row (BAR-10), named by the item that opens its cascade ("Strategy Data: Normal", "Create Virtual Object: Here")
		for (size_t row = 0; row < barRowCount; row++)
		{
			for (size_t m = 0; m < barRows.menuCount[row]; m++)
			{
				ui::Menu const &menu = barRows.menus[row][m];

				for (size_t i = 0; i < menu.count; i++)
				{
					ui::MenuItem const &entry = menu.items[i];

					if (!entry.name || menuWords != entry.name)
					{
						continue;
					}

					for (size_t k = 0; k < entry.subCount; k++)
					{
						if (entry.subItems[k].name && strcmp(entry.subItems[k].name, colon + 2) == 0)
						{
							RunAction(entry.subItems[k].action);
							return true;
						}
					}
				}
			}
		}

		for (size_t row = 0; row < barRowCount; row++)
		{
			for (size_t m = 0; m < barRows.menuCount[row]; m++)
			{
				ui::Menu const &menu = barRows.menus[row][m];

				if (!menu.title || menuWords != menu.title)
				{
					continue;
				}

				for (size_t i = 0; i < menu.count; i++)
				{
					if (menu.items[i].name && strcmp(menu.items[i].name, colon + 2) == 0)
					{
						RunAction(menu.items[i].action);
						return true;
					}
				}
			}
		}
	}

	// The right-click pop-up's tables (inteng's +0x14 tables, the only place a page's items are now): an entry at the top level, or "Group: Item" / a plain item word in a group the top level opens
	{
		static EditorBarRoom topRoom, levelRoom;

		if (EditorBar_MakePopup(barScreen, &topRoom) && topRoom.menuCount >= 1)
		{
			std::string const menuWords = colon ? std::string(words, (size_t)(colon - words)) : std::string();
			char const *const itemWords = colon ? colon + 2 : words;

			for (size_t i = 0; i < topRoom.menus[0].count; i++)
			{
				ui::MenuItem const &entry = topRoom.menus[0].items[i];

				if (entry.action < kEditorPopupGroupBase)
				{
					if (entry.name && strcmp(entry.name, words) == 0)
					{
						RunAction(entry.action);
						return true;
					}

					continue;
				}

				if ((colon && (!entry.name || (menuWords != entry.name && menuWords + " >" != entry.name))) || !EditorBar_MakePopupLevel(barScreen, entry.action - kEditorPopupGroupBase, &levelRoom) || levelRoom.menuCount < 1)
				{
					continue;
				}

				for (size_t k = 0; k < levelRoom.menus[0].count; k++)
				{
					if (levelRoom.menus[0].items[k].name && strcmp(levelRoom.menus[0].items[k].name, itemWords) == 0)
					{
						RunAction(levelRoom.menus[0].items[k].action);
						return true;
					}
				}
			}
		}
	}

	return false;
}

void RunTheStrategyProof(long frame)
{
	static int step = 0;
	static std::vector<std::vector<uint8_t>> candidates;
	static size_t candA = 0, candB = 0;
	static uint8_t idA[8], idB[8];
	static int32_t kindMade = 0;
	static std::string copyPath;
	static long last = 0;

	if (frame < 5 || frame < last + 2 || step == 6)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: strategy proof: %s\n", words.c_str());
		fflush(stdout);
	};

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		say("no level is open: the main job does NOT work (nothing to run on)");
		step = 6;
		return;
	}

	if (step == 0)
	{
		// the user's way: Construction's Mode menu, Strategy Data: the Strategy bar is the one in use from here
		bool const entered = PressTheBarItem("Strategy Data: Normal") && barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0;

		say(entered ? "entered Strategy mode through Construction > Mode > Strategy Data (the bar is now the Strategy bar)" : "could NOT enter Strategy mode through the menu");

		if (!entered)
		{
			say("Strategy main job: DOES NOT WORK: Strategy mode could not be entered");
			step = 6;
			return;
		}

		step = 7;
		return;
	}

	if (step == 7)
	{
		std::vector<host::LevelObject> found;

		LevelObjects_Read(&found);

		for (host::LevelObject const &one : found)
		{
			host::ObjectHeader header;

			if (LevelObjects_FindHeader(one.id, &header) && (header.flags & 0x400) != 0 && (header.flags & 0x200) == 0)
			{
				candidates.push_back(std::vector<uint8_t>(one.id, one.id + 8));
			}
		}

		say(std::to_string((unsigned long long)candidates.size()) + " placed objects to try");
		harnessPictureAsked = 6;   // the Strategy bar, before anything is picked
		step = 1;
	}

	if (step == 1)
	{
		// A: the first candidate that takes the strategy through Assign Strategy's own prompts
		if (candA >= candidates.size() || candA > 60)
		{
			say("Strategy main job: DOES NOT WORK: no placed object took a strategy (Assign)");
			step = 6;
			return;
		}

		memcpy(idA, candidates[candA].data(), 8);

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, idA, 8);
		host::Selection_Set(picked);
		ShowTheSelected(host::Selection_Get());

		// the kind: a Fan when the object can take one (a number to edit), else the first the object can take
		{
			int32_t canTake[16];
			size_t const canCount = KindsTheObjectCanTake(idA, canTake);

			kindMade = 0;

			for (size_t k = 0; k < canCount; k++)
			{
				kindMade = (kindMade == 0 || canTake[k] == 75) ? canTake[k] : kindMade;
			}

			if (kindMade == 0)
			{
				say("Assign refused for candidate " + std::to_string((unsigned long long)candA) + ": the object takes no kind");
				candA++;
				return;
			}
		}

		if (!PressTheBarItem("Assign Strategy"))   // the Strategy bar's Assign Strategy: the list opens under the prompt
		{
			say("Strategy main job: DOES NOT WORK: the Strategy bar has no Assign Strategy");
			step = 6;
			return;
		}

		promptText = std::to_string((long long)kindMade);
		RunThePrompt();                                  // the number typed

		if (promptFor == kPromptAssignReplace)
		{
			promptText = "yes";
			RunThePrompt();
		}

		if (statusWords && strstr(statusWords, "has a new strategy"))
		{
			say("Assign: object " + std::to_string((unsigned long long)candA + 1) + " of the list took strategy kind " + std::to_string((long long)kindMade) + ": " + statusWords);
			harnessPictureAsked = 1;
			step = 2;
		}
		else
		{
			say(std::string("Assign refused for candidate ") + std::to_string((unsigned long long)candA) + ": " + (statusWords ? statusWords : ""));
			candA++;
		}

		return;
	}

	if (step == 2)
	{
		// Edit: the first field of the kind whose number the model takes changed (the field-edit path of the panel)
		model::StrategyKind const *const kind = model::FindStrategyKind(kindMade);
		host::LevelStore store(open);
		host::ObjectStrategy s;
		bool edited = false;
		std::string fieldName;

		if (kind && host::LookUpStrategyBytes(LevelObjects_Level(), store, idA, &s) == host::StrategyFound::Found)
		{
			for (size_t i = 0; i < kind->fieldCount && !edited; i++)
			{
				model::Field const &field = kind->fields[i];
				double now = 0;
				char const *why = nullptr;

				if (!model::ReadField(*kind, field, s.payload.data(), s.payload.size(), &now, &why))
				{
					continue;
				}

				double const tries[5] = { now + 1, now - 1, now + 65536, now - 65536, 0 };

				for (double const value : tries)
				{
					host::FieldEdit e;

					e.field = field.name;
					e.what = host::EditWhat::Number;
					e.number = value;

					host::StrategyFound lookup;

					if (host::ApplyStrategyEdit(LevelObjects_Level(), idA, kindMade, e, &store, &why, &lookup) == host::EditAnswer::Applied)
					{
						edited = true;
						fieldName = std::string(field.name) + ": " + std::to_string(now) + " -> " + std::to_string(value);
						break;
					}
				}
			}
		}

		if (!edited)
		{
			say("Edit: no field of kind " + std::to_string((long long)kindMade) + " took a new number");
			step = 6;
			return;
		}

		ShowTheFieldsAgain();
		say("Edit: " + fieldName + " (the panel shows the edited record)");
		harnessPictureAsked = 2;
		step = 3;
		return;
	}

	if (step == 3)
	{
		// Copy: B is the first other placed object the copy goes to, from A, through Copy Strategy's own item
		while (candB < candidates.size() && memcmp(candidates[candB].data(), idA, 8) == 0)
		{
			candB++;
		}

		if (candB >= candidates.size() || candB > 60)
		{
			say("Strategy main job: DOES NOT WORK: no second placed object took the copy");
			step = 6;
			return;
		}

		memcpy(idB, candidates[candB].data(), 8);

		host::Selected a = {}, b = {};

		a.kind = b.kind = host::SelectedKind::Object;
		memcpy(a.objectId, idA, 8);
		memcpy(b.objectId, idB, 8);
		host::Selection_Set(a);
		host::Selection_Toggle(b);
		PressTheBarItem("Copy Strategy");

		if (statusWords && strstr(statusWords, "1 object(s) got the strategy"))
		{
			say(std::string("Copy: ") + statusWords);
			harnessPictureAsked = 3;
			step = 4;
		}
		else
		{
			say(std::string("Copy refused for candidate ") + std::to_string((unsigned long long)candB) + ": " + (statusWords ? statusWords : ""));
			candB++;
		}

		return;
	}

	if (step == 4)
	{
		SaveTheCopy();

		if (savedWords.compare(0, 14, "Saved a copy: ") != 0)
		{
			say("Strategy main job: DOES NOT WORK: " + savedWords);
			step = 6;
			return;
		}

		copyPath = savedWords.substr(14);
		say("Save a copy: " + copyPath);
		harnessPictureAsked = 4;
		step = 5;
		return;
	}

	if (step == 5)
	{
		// the loader: the copy is opened as the game opens a level, and both objects hold the strategy A was given, the same kind
		OpenLevelFile again;
		bool good = false;

		if (again.Open(copyPath.c_str()) == LevelOpenResult::Opened)
		{
			host::ObjectStrategy ra, rb;
			bool const both = host::FindObjectStrategy(again.Level(), idA, &ra) == host::StrategyFound::Found && host::FindObjectStrategy(again.Level(), idB, &rb) == host::StrategyFound::Found;

			good = both && ra.payload.size() >= 12 && ra.payload == rb.payload && *(int32_t const *)(ra.payload.data() + 8) == kindMade;
		}

		say(std::string("the game's loader on the copy: ") + (good ? "takes it, and both objects hold the strategy (same bytes, kind " + std::to_string((long long)kindMade) + ")" : "REFUSED it or the strategies differ"));
		say(good ? "Strategy main job: WORKS in the running editor" : "Strategy main job: DOES NOT WORK in the running editor");
		harnessPictureAsked = 5;
		step = 6;
	}
}

// -strategyallkinds (a hidden -picture run): ONE run through the user's way that edits a field of EVERY kind the Strategy bar's Assign Strategy can give: Construction > Mode > Strategy Data, then for each
// kind (modules: Door 50, Lift 51, Airlock 57, Switch Door 59, Lighting 64; placed objects: Platform Lift 54, Linked Switch 61, Multi Switch 66, Track 72, Track (Destructable) 80, Fan 75, Placed Object 62)
// an object that can take it is picked, the bar's Assign Strategy is pressed with the kind's number typed, a field of the new strategy is edited (the first the model takes a new value for) and the
// record read back from the level's bytes. At the end Save a copy and the game's loader on the copy: every object keeps its kind and the edited value. One line per kind; the last says whether it works.
// (Added 2026-11-15 by Melanikus (Frost).)
int proofViewWidth = 0, proofViewHeight = 0;   // the 3D view's size, for the proof's clicks

void RunTheAllKindsProof(long frame)
{
	static int step = 0;
	static size_t at = 0;                  // the kind being done
	static size_t candidate = 0;           // the candidate object being tried for it
	static std::vector<std::vector<uint8_t>> modules, placed, virtuals;
	struct Done { int32_t kind; std::vector<uint8_t> id; double field; std::string name; double value; bool edited; };
	static std::vector<Done> done;
	static std::string copyPath;
	static long last = 0;
	// pool 0: a module, 1: a placed object, 2: a virtual object made through the Strategy bar's Create Virtual Object
	struct KindPool { int32_t kind; int pool; };
	static KindPool const kinds[] = { { 50, 0 }, { 51, 0 }, { 57, 0 }, { 59, 0 }, { 64, 0 }, { 54, 1 }, { 61, 1 }, { 66, 1 }, { 72, 1 }, { 80, 1 }, { 75, 1 }, { 62, 1 },
	                                  { 66, 2 }, { 61, 2 }, { 68, 2 }, { 70, 2 }, { 73, 2 }, { 77, 2 }, { 78, 2 } };
	static size_t const kindCount = sizeof(kinds) / sizeof(kinds[0]);

	if (frame < 5 || frame < last + 2 || step == 9)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: strategy all kinds: %s\n", words.c_str());
		fflush(stdout);
	};

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		say("Strategy main job: DOES NOT WORK: no level is open");
		step = 9;
		return;
	}

	if (step == 0)
	{
		bool const entered = PressTheBarItem("Strategy Data: Normal") && barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0;

		say(entered ? "entered Strategy mode through Construction > Mode > Strategy Data" : "could NOT enter Strategy mode");
		step = entered ? 1 : 9;
		return;
	}

	if (step == 1)
	{
		std::vector<host::LevelObject> found;

		LevelObjects_Read(&found);

		for (host::LevelObject const &one : found)
		{
			host::ObjectHeader header;

			if (!LevelObjects_FindHeader(one.id, &header))
			{
				continue;
			}

			(header.flags & 0x400 ? placed : modules).push_back(std::vector<uint8_t>(one.id, one.id + 8));
		}

		// the virtual objects: one made through the bar for each kind a virtual object takes (Create Virtual Object: Here, the name typed)
		for (size_t k = 0; k < kindCount; k++)
		{
			if (kinds[k].pool != 2)
			{
				continue;
			}

			std::string const name = "provo" + std::to_string((long long)kinds[k].kind);

			if (PressTheBarItem("Create Virtual Object: Here"))
			{
				promptText = name;
				RunThePrompt();

				std::vector<uint8_t> vid(8);

				host::VirtualObject_Id(name.c_str(), vid.data());
				virtuals.push_back(vid);
			}
		}

		say(std::to_string((unsigned long long)modules.size()) + " modules, " + std::to_string((unsigned long long)placed.size()) + " placed objects and " + std::to_string((unsigned long long)virtuals.size()) + " new virtual objects to choose from");
		step = 2;
		return;
	}

	if (step == 2)
	{
		if (at >= kindCount)
		{
			step = 5;
			return;
		}

		int32_t const kind = kinds[at].kind;
		std::vector<std::vector<uint8_t>> const &pool = kinds[at].pool == 0 ? modules : (kinds[at].pool == 1 ? placed : virtuals);

		if (candidate >= pool.size() || candidate > 80)
		{
			say("kind " + std::to_string((long long)kind) + (kinds[at].pool == 2 ? " (virtual object)" : "") + ": NO OBJECT took it");
			done.push_back({ kind, std::vector<uint8_t>(), 0, "", 0, false });   // no object took it
			at++;
			candidate = 0;
			return;
		}

		// an object not already used for another kind in this run
		std::vector<uint8_t> const id = pool[candidate++];

		for (Done const &d : done)
		{
			if (d.id == id)
			{
				last = frame - 2;
				RunTheAllKindsProof(frame);
				return;
			}
		}

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, id.data(), 8);
		host::Selection_Set(picked);
		ShowTheSelected(host::Selection_Get());

		statusWords = nullptr;
		if (!PressTheBarItem("Assign Strategy"))
		{
			say("Strategy main job: DOES NOT WORK: no Assign Strategy item on the Strategy bar");
			step = 9;
			return;
		}

		promptText = std::to_string((long long)kind);
		RunThePrompt();

		if (promptFor == kPromptAssignReplace)
		{
			promptText = "yes";
			RunThePrompt();
		}

		if (!(statusWords && strstr(statusWords, "has a new strategy")))
		{
			last = frame - 2;
			RunTheAllKindsProof(frame);   // the next candidate at once
			return;
		}

		// Edit: the first field of the new strategy whose number (or flags word) the model takes changed
		model::StrategyKind const *const asked = model::FindStrategyKind(kind);
		int32_t const editKind = asked && asked->writtenAs ? asked->writtenAs : kind;   // an Airlock is written as a Lift with a flag
		model::StrategyKind const *const model = model::FindStrategyKind(editKind);
		host::LevelStore store(open);
		host::ObjectStrategy s;
		Done made = { kind, id, 0, "", 0, false };

		if (model && host::LookUpStrategyBytes(LevelObjects_Level(), store, id.data(), &s) == host::StrategyFound::Found)
		{
			for (size_t i = 0; i < model->fieldCount && !made.edited; i++)
			{
				model::Field const &field = model->fields[i];
				double now = 0;
				char const *why = nullptr;

				if (!model::ReadField(*model, field, s.payload.data(), s.payload.size(), &now, &why))
				{
					continue;
				}

				double const tries[9] = { now + 1, now - 1, now + 65536, now - 65536, 0, now + 2, now + 4, now + 8, 5 };

				for (double const value : tries)
				{
					host::FieldEdit e;

					e.field = field.name;
					e.what = host::EditWhat::Number;
					e.number = value;

					host::StrategyFound lookup;

					if (value != now && host::ApplyStrategyEdit(LevelObjects_Level(), id.data(), editKind, e, &store, &why, &lookup) == host::EditAnswer::Applied)
					{
						made.edited = true;
						made.name = field.name;
						made.field = now;
						made.value = value;
						break;
					}
				}
			}
		}

		ShowTheFieldsAgain();
		harnessPictureAsked = (int)at + 1;
		say("kind " + std::to_string((long long)kind) + " (" + (model ? model->name : "?") + (kinds[at].pool == 2 ? ", on a virtual object" : "") + "): assigned to candidate " + std::to_string((unsigned long long)candidate) + ", " +
			(made.edited ? "edited " + made.name + ": " + std::to_string(made.field) + " -> " + std::to_string(made.value) : std::string("NO field took a new value")));
		done.push_back(made);
		at++;
		candidate = 0;
		return;
	}

	// THE CLICK PICKS (T: Change Target, V: Edit Volume): on the virtual Linked Switch a click at the middle of the view picks its target; on the virtual Volume of Death two clicks (the middle and a third of
	// the way across) place the box's corners. Each is checked in the strategy's bytes.
	static bool picksOk = true;
	static std::vector<uint8_t> targetBefore;

	if (step >= 5 && step <= 8)
	{
		uint8_t const *linked = nullptr, *volume = nullptr;

		for (Done const &d : done)
		{
			if (d.id.empty())
			{
				continue;
			}

			linked = d.kind == 61 ? d.id.data() : linked;
			volume = d.kind == 77 ? d.id.data() : volume;
		}

		auto const selectIt = [&](uint8_t const *id)
		{
			host::Selected picked = {};

			picked.kind = host::SelectedKind::Object;
			memcpy(picked.objectId, id, 8);
			host::Selection_Set(picked);
			ShowTheSelected(host::Selection_Get());
		};

		auto const click = [&](int x, int y)
		{
			pickX = x;
			pickY = y;
			pickInBox = false;
			pickShift = false;
			pickWithS = false;
			pickAsked = true;
		};

		auto const wordsHave = [&](char const *text)
		{
			return statusWords && strstr(statusWords, text) != nullptr;
		};

		if (step == 5)
		{
			if (!linked || !volume || proofViewWidth < 64)
			{
				say("click picks: no virtual Linked Switch or Volume of Death to work on (or no view)");
				picksOk = false;
				step = 3;
				return;
			}

			selectIt(linked);

			host::ObjectStrategy s;
			host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), linked, &s);
			targetBefore.assign(s.payload.begin(), s.payload.end());
			statusWords = nullptr;
			PressTheBarItem("T: Change Target (click)");
			click(proofViewWidth / 2, proofViewHeight / 2);
			step = 6;
			return;
		}

		if (step == 6)
		{
			host::ObjectStrategy s;
			host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), linked, &s);
			bool const changed = s.payload != targetBefore && wordsHave("The target of");
			static int tries = 0;
			static int const spotX[] = { 25, 75, 50, 50, 33, 66, 20, 80 }, spotY[] = { 50, 50, 25, 75, 33, 66, 80, 20 };   // other places of the view, in percent, when the middle found no object

			if (!changed && tries < 8 && strategyPick == 1)
			{
				click(proofViewWidth * spotX[tries] / 100, proofViewHeight * spotY[tries] / 100);
				tries++;
				return;
			}


			say(std::string("T: Change Target: a click at the middle of the view ") + (changed ? "set the Linked Switch's target (" + std::string(statusWords ? statusWords : "") + ")" : "did NOT set a target (" + std::string(statusWords ? statusWords : "") + ")"));
			picksOk = picksOk && changed;
			selectIt(volume);

			host::ObjectStrategy v;
			host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), volume, &v);
			targetBefore.assign(v.payload.begin(), v.payload.end());
			statusWords = nullptr;
			PressTheBarItem("V: Edit Volume (click)");
			click(proofViewWidth / 2, proofViewHeight / 2);
			step = 7;
			return;
		}

		if (step == 7)
		{
			click(proofViewWidth / 3, proofViewHeight / 2);
			step = 8;
			return;
		}

		if (step == 8)
		{
			host::ObjectStrategy v;
			host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), volume, &v);

			bool ordered = v.payload.size() >= 44;

			for (int axis = 0; axis < 3 && ordered; axis++)
			{
				ordered = *(int32_t const *)(v.payload.data() + 20 + 4 * axis) <= *(int32_t const *)(v.payload.data() + 32 + 4 * axis);
			}

			bool const changed = v.payload != targetBefore && ordered && wordsHave("The volume's box");

			say(std::string("V: Edit Volume: two clicks ") + (changed ? "set the box's corners (smaller then larger on each axis)" : "did NOT set the box (" + std::string(statusWords ? statusWords : "") + ")"));
			picksOk = picksOk && changed;
			harnessPictureAsked = (int)kindCount + 2;
			step = 3;
			return;
		}
	}

	if (step == 3)
	{
		SaveTheCopy();

		if (savedWords.compare(0, 14, "Saved a copy: ") != 0)
		{
			say("Strategy main job: DOES NOT WORK: " + savedWords);
			step = 9;
			return;
		}

		copyPath = savedWords.substr(14);
		say("Save a copy: " + copyPath);
		step = 4;
		return;
	}

	if (step == 4)
	{
		OpenLevelFile again;
		size_t good = 0, edited = 0, missing = 0;

		if (again.Open(copyPath.c_str()) != LevelOpenResult::Opened)
		{
			say("Strategy main job: DOES NOT WORK: the game's loader refused the copy");
			step = 9;
			return;
		}

		for (Done const &d : done)
		{
			if (d.id.empty())
			{
				missing++;
				continue;
			}

			host::ObjectStrategy r;
			model::StrategyKind const *const asked = model::FindStrategyKind(d.kind);
			model::StrategyKind const *const model = model::FindStrategyKind(asked && asked->writtenAs ? asked->writtenAs : d.kind);
			double value = -1;
			char const *why = nullptr;
			bool const found = host::FindObjectStrategy(again.Level(), d.id.data(), &r) == host::StrategyFound::Found && r.payload.size() >= 12;
			int32_t const stored = found ? *(int32_t const *)(r.payload.data() + 8) : -1;
			int32_t const wanted = model ? model->number : d.kind;

			if (found && stored == wanted)
			{
				good++;

				if (d.edited)
				{
					for (size_t i = 0; model && i < model->fieldCount; i++)
					{
						if (d.name == model->fields[i].name && model::ReadField(*model, model->fields[i], r.payload.data(), r.payload.size(), &value, &why) && value == d.value)
						{
							edited++;
						}
					}
				}
			}
		}

		size_t wantedEdits = 0;

		for (Done const &d : done)
		{
			wantedEdits += d.edited ? 1 : 0;
		}

		say("the game's loader on the copy: takes it; " + std::to_string((unsigned long long)good) + " of " + std::to_string((unsigned long long)done.size()) + " objects hold their kind, " + std::to_string((unsigned long long)edited) + " of " + std::to_string((unsigned long long)wantedEdits) + " edited values read back; " + std::to_string((unsigned long long)missing) + " kinds found no object");
		// a Track (72) has no field of its own to type into (only padding and its point effects, a list): it is assigned and kept, and counted apart
		size_t listOnly = 0;

		for (Done const &d : done)
		{
			listOnly += (!d.edited && d.kind == 72 && !d.id.empty()) ? 1 : 0;
		}

		say(std::to_string((unsigned long long)listOnly) + " kind(s) have only a list (Track 72: point effects) and nothing scalar to edit");
		say(picksOk && good == done.size() && edited == wantedEdits && wantedEdits + listOnly == done.size() ? "Strategy main job: WORKS for every kind that has a field to edit (virtual objects included), in the running editor" : "Strategy main job: DOES NOT WORK for every kind (see the lines above)");
		harnessPictureAsked = (int)kindCount + 1;
		step = 9;
	}
}

// -trackproof (a hidden -picture run, nobody at the screen; Track Editing's MAIN JOB in the running editor): the user's way, through the bar and the editor's own functions: Construction > Object Placement >
// Edit Track on an object that has a track -> Add Section (the track grows by one section) -> a click in each of the three views places its end -> Finish End Pos Edit -> Properties > Section (the time)
// -> Sound > Edit (a name and numbers) -> Play Track (the game's own track code moves the object: the pose must change) -> Stop track play -> Save a copy -> the game's loader on the copy:
// the object's track has the new section, with its end where the cursor was, its time and its sound -> Delete > Section -> Stop Track Edit (back on Object Placement). One line per step is printed
// ("AVP_OpenInteng: track proof: ..."), and the last says whether the main job works. (Added 2026-11-15 by Melanikus (Frost).)
// -autolinkshowfeed (a hidden run): STANDS IN FOR THE AUTOLINK DRIVER (Dietrich's, not in yet), feeding the display the way it will (AvP-docs/director/autolink-interfaces-2026-11-16.md
// section 3): on the fifth frame Begin (the words at (200, 220)), the counter at record 7, and five pictures of stripes of the IDs 1 to 6 (picture k: the stripe 40 pixels wide at x has the ID
// 1 + (x / 40 + k) % 6), one more than a 2 x 2 grid holds, so the fifth goes round into the first tile; then, as the driver asks before each step, a right press (AutoLinkShow_Cancelled)
// ends the run and the display goes. The status line says each, for the harness (acceptance-autolink-show.txt).
void FeedTheAutoLinkShow(int frames)
{
	static bool begun = false;
	static bool ended = false;
	static std::string words;

	if (!begun && frames >= 5)
	{
		begun = true;
		AutoLinkShow_Begin(host::kAutoLinkWordsY);
		AutoLinkShow_Counter(7);

		int shown = 0;

		for (int k = 0; k < 5; k++)
		{
			std::vector<uint32_t> picture(host::kAutoLinkPicturePixels);

			for (int32_t y = 0; y < host::kAutoLinkTileHeight; y++)
			{
				for (int32_t x = 0; x < host::kAutoLinkTileWidth; x++)
				{
					picture[(size_t)y * host::kAutoLinkTileWidth + x] = 1u + (uint32_t)((x / 40 + k) % 6);
				}
			}

			shown += AutoLinkShow_Picture(picture) ? 1 : 0;
		}

		// a picture that is not 320 x 240 is refused, and nothing of it is shown
		std::vector<uint32_t> const wrong(100, 3u);
		bool const wrongShown = AutoLinkShow_Picture(wrong);

		words = "AutoLink display: " + std::to_string((long long)shown) + " pictures, counter " + host::AutoLinkShow_CounterWords(7) + (wrongShown ? "; a picture of 100 pixels SHOWN" : "; a picture of 100 pixels refused");
		statusWords = words.c_str();
		printf("AVP_OpenInteng: %s\n", words.c_str());
		fflush(stdout);
		return;
	}

	if (begun && !ended && AutoLinkShow_Cancelled())
	{
		ended = true;
		AutoLinkShow_End();
		words = std::string("AutoLink display: ended by a right press") + (AutoLinkShow_On() ? " (STILL ON)" : "");
		statusWords = words.c_str();
		printf("AVP_OpenInteng: %s\n", words.c_str());
		fflush(stdout);
	}
}

bool trackSoundOnly = false;   // -tracksoundproof: Edit Track on an object whose track has a sound, Play Track: the game's sound code plays it
bool trackPoseOnly = false;   // -trackposeproof: Edit Track, a change of the first section's pivot, Stop Track Edit: the object's record is the track's pose at its timer start

void RunTheTrackProof(long frame)
{
	static int step = 0;
	static long last = 0;
	static uint8_t id[8];
	static size_t sectionsBefore = 0;
	static int32_t cursorSet[3];
	static std::string copyPath;
	static host::TrackPose startPose = {};
	static ULONGLONG playStarted = 0;
	static bool moved = false;

	if (frame < 5 || frame < last + 2 || step == 99)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: track proof: %s\n", words.c_str());
		fflush(stdout);
	};

	auto const fail = [&](std::string const &words)
	{
		say("Track Editing main job: DOES NOT WORK: " + words);
		step = 99;
	};

	auto const words = [&]() { return std::string(statusWords ? statusWords : ""); };

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		fail("no level is open (nothing to run on)");
		return;
	}

	if (step == 0)
	{
		if (!PressTheBarItem("Object Placement") || strcmp(host::kIntengScreens[barScreen].name, "Object Placement") != 0)
		{
			fail("Construction has no Object Placement item that shows its screen");
			return;
		}

		std::vector<host::LevelObject> found;

		LevelObjects_Read(&found);

		bool have = false;
		size_t best = 0;

		for (host::LevelObject const &one : found)
		{
			host::TrackLoaded loaded;

			bool travels = false;

			if (host::TrackLoad(open, one.id, &loaded) == host::TrackEditResult::Done && loaded.had)
			{
				for (host::TrackSection const &s : loaded.track.sections)
				{
					travels = travels || s.pivotStart[0] != s.pivotEnd[0] || s.pivotStart[1] != s.pivotEnd[1] || s.pivotStart[2] != s.pivotEnd[2];
				}
			}

			if (trackSoundOnly)
			{
				bool hasSound = false;

				for (host::TrackSound const &sound : loaded.sounds)
				{
					hasSound = hasSound || (!sound.wav.empty() && sound.wav != "Not Set");
				}

				travels = travels && hasSound;
			}

			if (travels && loaded.track.sections.size() >= best)
			{
				best = loaded.track.sections.size();
				memcpy(id, one.id, 8);
				sectionsBefore = best;
				have = true;

				if (best >= 2)
				{
					break;
				}
			}
		}

		for (host::LevelObject const &one : found)
		{
			host::TrackLoaded loaded;

			if (!have && host::TrackLoad(open, one.id, &loaded) == host::TrackEditResult::Done)
			{
				memcpy(id, one.id, 8);
				sectionsBefore = loaded.track.sections.size();
				have = true;
				say("no object of this level has a track that travels: the first object is used, which starts with inteng's one-section default");
			}
		}

		if (!have)
		{
			fail("this level has no object to edit a track of");
			return;
		}

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, id, 8);
		host::Selection_Set(picked);
		ShowTheSelected(host::Selection_Get());
		if (trackPoseOnly)
		{
			// the object is moved 1000 away (the proof's own arrangement, as if a user had moved it with Object Placement's keys): Edit Track brings the track along (inteng FUN_004f0490 on load), so Stop Track Edit leaves the object THERE
			int32_t place[3];
			float quat[4];

			if (!host::TrackObjectPlace(open, id, place, quat))
			{
				fail("the object's record could not be read");
				return;
			}

			place[0] += 1000;

			if (host::TrackSetObjectPose(open, id, place, quat) != host::TrackEditResult::Done)
			{
				fail("the object could not be moved for the proof");
				return;
			}
		}

		say("on Object Placement; the object chosen has a track of " + std::to_string((unsigned long long)sectionsBefore) + " section(s)");
		step = 1;
		return;
	}

	if (step == 1)
	{
		bool const there = PressTheBarItem("Edit Track") && OnTheTrackScreen() && trackEd.state == host::TrackState::Loaded;

		if (!there)
		{
			fail("Edit Track did not show the Track Edit screen: " + words());
			return;
		}

		say("Edit Track: " + words());
		harnessPictureAsked = 1;
		step = trackSoundOnly ? 40 : (trackPoseOnly ? 30 : 2);
		return;
	}

	if (step >= 40 && step <= 43)
	{
		// Play Track: the track's start, middle and end sounds are loaded and played by the game's sound code (Start_Track_Sound, Update_Track_Sound of track.cpp)
		static long playedAt = 0;
		static float loudest = -1.0f;   // the most output volume (OpenAL's listener gain) on any frame since Play
		static int mostSounding = 0;    // the most of the track's sounds sounding at once on any frame since Play
		static long mostAt = 0;         // the frame after Play it was first seen on
		host::TrackLoaded now;

		if (step == 40)
		{
			if (host::TrackLoad(open, id, &now) != host::TrackEditResult::Done || now.sounds.empty())
			{
				fail("this track has no sound");
				return;
			}

			std::string names;

			for (host::TrackSound const &sound : now.sounds)
			{
				names += std::to_string((long long)sound.index) + "=" + sound.wav + (sound.flags & 1 ? " (loops)" : "") + " ";
			}

			say("the track's sounds: " + names);

			// a sound that loops is heard only within its outer range of the player (Start_Track_Sound, track.cpp:140-146): the camera goes to the track's start, as a listener would stand there
			{
				int32_t at[3];
				float quat[4];
				extern double local_scale;

				if (host::TrackPlay_PoseAtStartAsFile(now.track, at, quat))
				{
					theCamera->x = (int)(at[0] * (local_scale > 0.0 ? local_scale : 1.0));
					theCamera->y = (int)(at[1] * (local_scale > 0.0 ? local_scale : 1.0));
					theCamera->z = (int)(at[2] * (local_scale > 0.0 ? local_scale : 1.0));
					FlyCamera_Hold(theCamera);
				}
			}

			if (!PressTheBarItem("Play Track") || trackEd.state != host::TrackState::Playing)
			{
				fail("Play Track did not start: " + words());
				return;
			}

			playedAt = frame;
			loudest = host::TrackPlay_OutputGain();
			mostSounding = 0;
			mostAt = 0;
			step = 41;
			return;
		}

		if (step == 41)
		{
			// the output volume read on EVERY frame from Play on: a hidden run must be silent on all of them, not only when it was set
			float const gainNow = host::TrackPlay_OutputGain();

			loudest = gainNow > loudest ? gainNow : loudest;

			// the sounds counted on every frame too, the most kept: the start sound is short and the loop waits for it (track.cpp Update_Track_Controller), so on slow frames (a
			// busy computer) the 20th frame can come after the game let the start sound go and before the loop is heard: one count there said "none sounds" of a track that played
			int const soundingNow = host::TrackPlay_SoundsSounding();

			if (soundingNow > mostSounding)
			{
				mostSounding = soundingNow;
				mostAt = frame - playedAt;
			}

			if (frame < playedAt + 20)
			{
				last = frame - 1;
				return;
			}

			bool const device = host::TrackPlay_SoundsAvailable();

			say(std::string("sound system: ") + (device ? "started (a sound device is there)" : "NOT started (no sound device in this run)") + "; sounds the game loaded for the track: " + std::to_string((long long)host::TrackPlay_SoundsLoaded()) + "; sounding now: " + std::to_string((long long)soundingNow) + "; the most at once since Play: " + std::to_string((long long)mostSounding) + (mostSounding > 0 ? " (first on frame " + std::to_string((long long)mostAt) + ")" : std::string()));

			if (!device)
			{
				say("Track sounds: NOT PROVEN here: this run has no sound device, the track played silently");
				step = 99;
				return;
			}

			{
				char gainWords[160];

				// 0.00001 is the game's volume 0 (VOLUME_MIN, openal_audio.cpp vol_to_gain_table[0]): nothing heard
				snprintf(gainWords, sizeof(gainWords), "the output volume (OpenAL's listener gain) at most %.6f on the %ld frames since Play: ", (double)loudest, (long)(frame - playedAt));
				say(std::string(gainWords) + (!editorHiddenRun ? "a shown run, heard" : (loudest >= 0.0f && loudest <= 0.00001f ? "SILENT on every frame (a hidden run)" : "HEARD: a hidden run is NOT silent")));
			}

			bool const loaded = host::TrackPlay_SoundsLoaded() > 0;

			say(loaded && mostSounding > 0 ? "Track sounds: WORKS in the running editor (the game's sound code is playing the track's sound)" : (loaded ? "Track sounds: DOES NOT WORK: the sounds were loaded but none sounds" : "Track sounds: DOES NOT WORK: the game found none of the track's sounds"));
			PressTheBarItem("Stop track play");
			step = 42;
			return;
		}

		if (step == 42)
		{
			say(std::string("Stop track play: the sounds are let go: ") + (host::TrackPlay_SoundsLoaded() == 0 && host::TrackPlay_SoundsSounding() == 0 ? "yes" : "NO"));
			step = 99;
			return;
		}
	}

	if (step >= 30 && step <= 36)
	{
		// Stop Track Edit gives the object the track's pose at its timer start (FUN_004eff90 0x4effba): a change of section 1's object offset (Edit Pivot Point, Move Pivot To Centre) first, so the pose is not the one the object had
		static int32_t recordBefore[3];
		static float quatBefore[4];
		static size_t sectionsAtEnter = 0;
		static int32_t markerBefore[3];
		static double markerTurnSeen = 0.0;
		static bool markerLive = false;
		static size_t newSection = 0;   // where Add Section put the new section (after the current one)

		if (step == 30)
		{
			host::TrackLoaded atEnter;

			if (!host::TrackObjectPlace(open, id, recordBefore, quatBefore) || host::TrackLoad(open, id, &atEnter) != host::TrackEditResult::Done)
			{
				fail("the object's record or track could not be read");
				return;
			}

			sectionsAtEnter = atEnter.track.sections.size();
			placementSplit = true;

			if (!PressTheBarItem("Edit Section End Position") || trackEd.state != host::TrackState::EndEdit || !PressTheBarItem("Add Section") || trackEd.state != host::TrackState::EndEdit || !trackEd.working)
			{
				fail("Add Section could not be reached: " + words());
				return;
			}

			// the end marker flies (F) and is turned with the keys: the camera is turned a quarter of a turn (what holding the turn key does), and the marker turns with it
			newSection = trackEd.current;
			host::TrackData_MarkerAt(trackEd.working->track, trackEd.current, true, host::TrackPlay_ObjectPlace, markerBefore);
			{
				// the camera stands 2.5 m back from the marker, looking at it (so a picture shows the object itself, flown live)
				double const sc = local_scale > 0.0 ? local_scale : 1.0;

				theCamera->x = (int)(markerBefore[0] * sc);
				theCamera->y = (int)(markerBefore[1] * sc);
				theCamera->z = (int)(markerBefore[2] * sc) - 2500;
				theCamera->turn = 0;
				theCamera->look = 0;
				FlyCamera_Hold(theCamera);
			}

			SendMessage(editorWindow, WM_KEYDOWN, 'F', 0);
			step = 33;
			return;
		}

		if (step == 33)
		{
			if (!trackFly)
			{
				fail("F did not let the end marker fly: " + words());
				return;
			}

			markerLive = LevelView_FlyActive();
			harnessPictureAsked = 8;
			theCamera->turn = (theCamera->turn + 1024) & 4095;
			step = 34;
			return;
		}

		if (step == 34)
		{
			markerTurnSeen = trackEd.markerTurn[0];

			if (fabs(markerTurnSeen - 90.0) > 1.0 || !PressTheBarItem("Edit Pivot Point") || trackEd.state != host::TrackState::PivotEdit)
			{
				fail("the marker did not turn 90 degrees with the camera, or Edit Pivot Point could not be reached: marker turn " + std::to_string(markerTurnSeen) + ", " + words());
				return;
			}

			step = 31;
			return;
		}

		if (step == 31)
		{
			if (!PressTheBarItem("Move Pivot To Centre") || !PressTheBarItem("Stop Pivot Edit") || trackEd.state != host::TrackState::EndEdit)
			{
				fail("Move Pivot To Centre / Stop Pivot Edit did not work: " + words());
				return;
			}

			step = 32;
			return;
		}

		if (step == 32)
		{
			if (!PressTheBarItem("Finish End Pos Edit") || trackEd.state != host::TrackState::Loaded)
			{
				fail("Finish End Pos Edit did not work: " + words());
				return;
			}

			// inteng keeps the track in memory while it is edited (FUN_004eff90 loads it, FUN_004f1300 writes the chunks only when it is let go): BEFORE Stop Track Edit the level still holds the track it had
			host::TrackLoaded inLevel;
			bool const untouched = host::TrackLoad(open, id, &inLevel) == host::TrackEditResult::Done && inLevel.track.sections.size() == sectionsAtEnter;

			if (!PressTheBarItem("Stop Track Edit") || OnTheTrackScreen())
			{
				fail("Stop Track Edit did not work: " + words());
				return;
			}

			host::TrackLoaded stopped;
			int32_t recordPlace[3], posePlace[3];
			float recordQuat[4], poseQuat[4];

			bool const read = host::TrackLoad(open, id, &stopped) == host::TrackEditResult::Done && stopped.had && host::TrackObjectPlace(open, id, recordPlace, recordQuat) && host::TrackPlay_PoseAtStartAsFile(stopped.track, posePlace, poseQuat);
			double const dot = read ? fabs((double)recordQuat[0] * poseQuat[0] + (double)recordQuat[1] * poseQuat[1] + (double)recordQuat[2] * poseQuat[2] + (double)recordQuat[3] * poseQuat[3]) : 0.0;
			bool const same = read && recordPlace[0] == posePlace[0] && recordPlace[1] == posePlace[1] && recordPlace[2] == posePlace[2] && dot > 0.9999;
			bool const moved = read && (abs(recordPlace[0] - recordBefore[0]) > 2 || abs(recordPlace[1] - recordBefore[1]) > 2 || abs(recordPlace[2] - recordBefore[2]) > 2);   // 1 or 2 of rounding in the pose is the game's 16.16 maths, as inteng's
			bool const written = read && stopped.track.sections.size() == sectionsAtEnter + 1;

			// the marker was turned a quarter of a turn: the new section's end is turned 90 degrees from its start, and the object stays at the end place the marker had
			double turnedDegrees = -1.0;
			bool markerStays = false;

			if (written)
			{
				int32_t startM[9], endM[9], markerAfter[3];

				if (host::TrackPlay_OrientationAt(stopped.track, newSection, false, startM) && host::TrackPlay_OrientationAt(stopped.track, newSection, true, endM))
				{
					double trace = 0;

					for (int i = 0; i < 9; i++)
					{
						trace += (double)endM[i] * (double)startM[i] / 65536.0 / 65536.0;
					}

					double const cosine = (trace - 1.0) / 2.0;

					turnedDegrees = acos(cosine > 1.0 ? 1.0 : (cosine < -1.0 ? -1.0 : cosine)) * 180.0 / 3.14159265358979;
				}

				host::TrackData_MarkerAt(stopped.track, newSection, true, host::TrackPlay_ObjectPlace, markerAfter);
				markerStays = std::abs(markerAfter[0] - markerBefore[0]) <= 4 && std::abs(markerAfter[1] - markerBefore[1]) <= 4 && std::abs(markerAfter[2] - markerBefore[2]) <= 4;
			}

			say(std::string("the end marker is the real object drawn live: ") + (markerLive ? "yes" : "NO"));
			say("the end marker was flown (F) and turned with the camera by " + std::to_string(markerTurnSeen) + " degrees: the new section's end is turned " + std::to_string(turnedDegrees) + " degrees from its start; the object stays at the marker's place: " + (markerStays ? "yes" : "NO"));

			say(std::string("before Stop Track Edit the level still held the track it had: ") + (untouched ? "yes" : "NO") + " (" + std::to_string((unsigned long long)(untouched ? sectionsAtEnter : inLevel.track.sections.size())) + " sections); after it the level holds the edited track: " + (written ? "yes" : "NO") + " (" + std::to_string((unsigned long long)(read ? stopped.track.sections.size() : 0)) + " sections)");
			say(std::string("Stop Track Edit: the object's record is ") + (same ? "the track's pose at its timer start" : "NOT the track's pose at its timer start") + "; the object was at " + std::to_string((long long)recordBefore[0]) + " " + std::to_string((long long)recordBefore[1]) + " " + std::to_string((long long)recordBefore[2]) + " before and is at " + (read ? std::to_string((long long)recordPlace[0]) + " " + std::to_string((long long)recordPlace[1]) + " " + std::to_string((long long)recordPlace[2]) : std::string("?")) + " now (" + (moved ? "it MOVED" : "it stayed where it was put") + "), the pose is " + (read ? std::to_string((long long)posePlace[0]) + " " + std::to_string((long long)posePlace[1]) + " " + std::to_string((long long)posePlace[2]) : std::string("?")) + ", turns agree to " + std::to_string(dot));
			say(same && !moved && untouched && written && turnedDegrees > 88.0 && turnedDegrees < 92.0 && markerStays && markerLive ? "Track Editing main job: WORKS in the running editor (pose: the track followed the moved object, Stop Track Edit left the object where it was put, its record is the track's pose at the timer start; the track was written at Stop Track Edit, not before; the end marker turned with the camera and became the section's end turn)" : "Track Editing main job: DOES NOT WORK: the object's record is not the track's pose at its timer start, or Edit Track + Stop moved it, or the level's track changed before Stop Track Edit, or was not written by it");
			step = 99;
			return;
		}
	}

	if (step == 2)
	{
		// the last section is the current one (Previous Section from the first wraps round), then Add Section after it
		bool const pressed = PressTheBarItem("Previous Section");

		if (!pressed || trackEd.current != sectionsBefore - 1)
		{
			fail(std::string("Previous Section ") + (pressed ? "was pressed but" : "was NOT found on the bar and") + " did not go to the last section (current " + std::to_string((long long)trackEd.current) + " of " + std::to_string((unsigned long long)sectionsBefore) + ", state " + std::to_string((int)trackEd.state) + "): " + words() + " [bar " + std::to_string((unsigned long long)barScreen) + " rows " + std::to_string((unsigned long long)barRowCount) + ": " + [&]() { std::string names; for (size_t row = 0; row < barRowCount; row++) for (size_t m = 0; m < barRows.menuCount[row]; m++) { ui::Menu const &menu = barRows.menus[row][m]; names += std::string(menu.title ? menu.title : "?") + "{"; for (size_t i = 0; i < menu.count; i++) names += std::string(menu.items[i].name ? menu.items[i].name : "?") + ","; names += "} "; } return names; }() + "]");
			return;
		}

		host::TrackLoaded now;

		if (!PressTheBarItem("Add Section") || trackEd.state != host::TrackState::EndEdit || host::TrackLoad(open, id, &now) != host::TrackEditResult::Done || now.track.sections.size() != sectionsBefore + 1)
		{
			fail("Add Section did not add a section: " + words());
			return;
		}

		say("Add Section: " + words());
		step = 13;
		return;
	}

	if (step == 13)
	{
		// inteng's keys on the Track Edit screen, through the window as a person's keys: O shows / hides the section's start position (the view's start cross), P switches the turn between about the centre and about the pivot
		SendMessage(editorWindow, WM_KEYDOWN, 'O', 0);

		bool const startHidden = !trackShowStart;

		SendMessage(editorWindow, WM_KEYDOWN, 'O', 0);
		SendMessage(editorWindow, WM_KEYDOWN, 'P', 0);

		bool const aboutPivot = trackTurnAboutPivot;

		SendMessage(editorWindow, WM_KEYDOWN, 'P', 0);
		say(std::string("keys on the Track Edit screen: O hides the start position (") + (startHidden ? "yes" : "NO") + ") and shows it again (" + (trackShowStart ? "yes" : "NO") + "); P switches to rotation about the pivot (" + (aboutPivot ? "yes" : "NO") +
			") and back (" + (!trackTurnAboutPivot ? "yes" : "NO") + ")");

		if (!(startHidden && trackShowStart && aboutPivot && !trackTurnAboutPivot))
		{
			fail("the O or P key did not toggle");
			return;
		}

		step = 3;
		return;
	}

	if (step == 3)
	{
		// the three views are Pivot Edit's alone now: in the end edit the normal 3D view is shown, with the track drawn in the world; the end is placed by flying the marker (F), as inteng does
		if (ThreeViewsOn() || trackEd.state != host::TrackState::EndEdit)
		{
			fail("the end edit shows the three views (or is not the end edit): they are Pivot Edit's alone");
			return;
		}

		memcpy(cursorSet, threeCursor, sizeof(cursorSet));
		say("the end edit shows the normal 3D view, not the three views (as FUN_004f3770 only makes them for Pivot Edit); the marker is at " + std::to_string((long long)cursorSet[0]) + " " + std::to_string((long long)cursorSet[1]) + " " + std::to_string((long long)cursorSet[2]));
		harnessPictureAsked = 2;
		step = 20;
		return;
	}

	if (step == 20)
	{
		// inteng's way of placing the end: F flies the marker (the cursor takes the camera's place plus its offset), X and B switch Move Rel to Axes and Inc Bounding Box, F lets it go there
		SendMessage(editorWindow, WM_KEYDOWN, 'F', 0);

		bool const flying = trackFly && statusWords && strstr(statusWords, "Fly end marker: on") != nullptr;

		SendMessage(editorWindow, WM_KEYDOWN, 'X', 0);

		bool const xOn = flyRelAxes && statusWords && strstr(statusWords, "Move Rel to Axes: on") != nullptr;

		SendMessage(editorWindow, WM_KEYDOWN, 'B', 0);

		bool const bOn = flyIncBox && statusWords && strstr(statusWords, "Inc Bounding Box: on") != nullptr;

		say(std::string("F in the end edit: the marker flies (") + (flying ? "yes" : "NO") + "); X: Move Rel to Axes on (" + (xOn ? "yes" : "NO") + "); B: Inc Bounding Box on (" + (bOn ? "yes" : "NO") + ")");

		if (!flying || !xOn || !bOn || !theCamera)
		{
			fail("F / X / B did not work in the end edit: " + words());
			return;
		}

		memcpy(cursorSet, threeCursor, sizeof(cursorSet));
		theCamera->x += 20000;
		theCamera->z -= 10000;
		step = 21;
		return;
	}

	if (step == 21)
	{
		// the marker followed the camera by exactly what the camera moved
		double const unit = local_scale > 0.0 ? local_scale : 1.0;   // the cursor is in the file's units, the camera in the world's
		bool const followed = std::abs(threeCursor[0] - (cursorSet[0] + (int32_t)floor(20000.0 / unit + 0.5))) <= 1 && std::abs(threeCursor[1] - cursorSet[1]) <= 1 && std::abs(threeCursor[2] - (cursorSet[2] - (int32_t)floor(10000.0 / unit + 0.5))) <= 1;

		say(std::string("the camera moved by 20000 0 -10000: the marker moved to ") + std::to_string((long long)threeCursor[0]) + " " + std::to_string((long long)threeCursor[1]) + " " + std::to_string((long long)threeCursor[2]) + (followed ? " = it followed" : " (NOT following)"));

		SendMessage(editorWindow, WM_KEYDOWN, 'F', 0);

		bool const let = !trackFly && !flyRelAxes && !flyIncBox && statusWords && strstr(statusWords, "Fly end marker: off") != nullptr;

		say(std::string("F again: the marker is let go, X and B back to off (") + (let ? "yes" : "NO") + ")");

		if (!followed || !let)
		{
			fail("the flying marker did not follow the camera or was not let go");
			return;
		}

		memcpy(cursorSet, threeCursor, sizeof(cursorSet));   // Finish End Pos Edit must take the place it was let go at
		harnessPictureAsked = 8;
		step = 4;
		return;
	}

	if (step == 4)
	{
		host::TrackLoaded now;
		int32_t got[3] = { 0, 0, 0 };

		if (!PressTheBarItem("Finish End Pos Edit") || trackEd.state != host::TrackState::Loaded || host::TrackLoad(open, id, &now) != host::TrackEditResult::Done || now.track.sections.size() != sectionsBefore + 1)
		{
			fail("Finish End Pos Edit did not finish: " + words());
			return;
		}

		host::TrackData_MarkerAt(now.track, sectionsBefore, true, host::TrackPlay_ObjectPlace, got);

		bool const there = std::abs(got[0] - cursorSet[0]) <= 4 && std::abs(got[1] - cursorSet[1]) <= 4 && std::abs(got[2] - cursorSet[2]) <= 4;

		say(std::string("Finish End Pos Edit: the new section's end, by the game's own track code, is at ") + std::to_string((long long)got[0]) + " " + std::to_string((long long)got[1]) + " " + std::to_string((long long)got[2]) +
			(there ? " = the cursor" : " (NOT the cursor)"));

		if (!there)
		{
			fail("the end is not where the cursor was");
			return;
		}

		step = 5;
		return;
	}

	if (step == 5)
	{
		// the time of the section (a prompt), then Properties > Track opens inteng's dialog (four boxes, OK and Cancel)
		bool pressed1 = PressTheBarItem("Properties: Section");
		bool ok = pressed1 && promptFor == kPromptTrackTime;

		if (ok)
		{
			promptText = "2.5";
			RunThePrompt();
		}

		bool pressed2 = ok && PressTheBarItem("Properties: Track");

		say("step 5: Properties: Section pressed " + std::string(pressed1 ? "yes" : "NO") + ", its prompt " + std::to_string((long long)promptFor) + " (time prompt is " + std::to_string((long long)kPromptTrackTime) + "); Properties: Track pressed " + (pressed2 ? "yes" : "NO") + ", prompt now " + std::to_string((long long)promptFor) + " (flags prompt is " + std::to_string((long long)kPromptTrackFlags) + "), form rows " + std::to_string((long long)formRows.size()));

		ok = ok && pressed2 && promptFor == kPromptTrackFlags && formRows.size() == 4;

		if (!ok)
		{
			std::string names;
			static EditorBarRoom topRoom, levelRoom;

			if (EditorBar_MakePopup(barScreen, &topRoom) && topRoom.menuCount >= 1)
			{
				for (size_t i = 0; i < topRoom.menus[0].count; i++)
				{
					names += std::string(topRoom.menus[0].items[i].name ? topRoom.menus[0].items[i].name : "?") + "{";

					if (topRoom.menus[0].items[i].action >= kEditorPopupGroupBase && EditorBar_MakePopupLevel(barScreen, topRoom.menus[0].items[i].action - kEditorPopupGroupBase, &levelRoom) && levelRoom.menuCount >= 1)
					{
						for (size_t k = 0; k < levelRoom.menus[0].count; k++)
						{
							names += std::string(levelRoom.menus[0].items[k].name ? levelRoom.menus[0].items[k].name : "?") + ",";
						}
					}

					names += "} ";
				}
			}

			fail("the section's time did not take, or Properties > Track did not open its dialog: " + words() + " [pop-up: " + names + "]");
			return;
		}

		say("Properties > Section: the new section takes 2.5 seconds; Properties > Track: the dialog is open with its four boxes (Loop Track, Loop back and forth, Playing at start, Use Track Smoothing)");
		harnessPictureAsked = 7;
		step = 14;
		return;
	}

	if (step == 14)
	{
		// in the dialog: each box is ticked or cleared with Space so that Loop Track and Playing at start are on and the others off (the track may have had any flags), Down moves, Enter is OK
		static bool const want[4] = { true, false, true, false };

		for (size_t i = 0; i < 4 && formRows.size() == 4; i++)
		{
			if (formRows[i].checked != want[i])
			{
				FormKey(WM_CHAR, ' ');
			}

			FormKey(WM_KEYDOWN, VK_DOWN);
		}

		FormKey(WM_KEYDOWN, VK_RETURN);

		host::TrackLoaded now;
		bool ok = promptFor == kPromptNone && host::TrackLoad(open, id, &now) == host::TrackEditResult::Done && (now.track.flags & 0xf) == 3;

		if (!ok)
		{
			fail("the Track Properties dialog did not set Loop Track and Playing at start: " + words());
			return;
		}

		say("Track Properties dialog: Loop Track and Playing at start ticked, OK: the track's flags are 3");

		// Start Sound > Edit opens the sound dialog; its name is typed into the first row, the numbers into the next rows
		ok = PressTheBarItem("Start Sound: Edit") && promptFor == kPromptTrackSound && formRows.size() == 6;

		if (!ok)
		{
			fail("Start Sound > Edit did not open the sound dialog: " + words());
			return;
		}

		harnessPictureAsked = 8;
		step = 15;
		return;
	}

	if (step == 15)
	{
		auto const typeInto = [](char const *text)
		{
			for (int i = 0; i < 80; i++)
			{
				FormKey(WM_KEYDOWN, VK_BACK);
			}

			for (char const *p = text; *p; p++)
			{
				FormKey(WM_CHAR, (WPARAM)*p);
			}

			FormKey(WM_KEYDOWN, VK_DOWN);
		};

		typeInto("door_open.wav");
		typeInto("100000");
		typeInto("200000");
		typeInto("100");
		FormKey(WM_KEYDOWN, VK_RETURN);

		host::TrackLoaded now;
		bool ok = promptFor == kPromptNone && host::TrackLoad(open, id, &now) == host::TrackEditResult::Done && now.track.sections.size() == sectionsBefore + 1 && now.track.sections[sectionsBefore].time == 163840;
		bool hasSound = false;

		for (host::TrackSound const &sound : now.sounds)
		{
			hasSound = hasSound || (sound.index == 1 && sound.wav == "door_open.wav" && sound.innerRange == 100000 && sound.outerRange == 200000 && sound.maxVolume == 100);
		}

		if (!ok || !hasSound)
		{
			fail("the sound dialog did not set the start sound: " + words());
			return;
		}

		say("Start Sound > Edit dialog: door_open.wav, inner range 100000, outer range 200000, volume 100, OK: the start sound is in the track");
		step = 11;
		return;
	}

	if (step == 11)
	{
		// the pivot point: Edit Section End Position, Edit Pivot Point, Move Pivot To Centre, Stop Pivot Edit (the object's offset becomes 0 and the object stays at both places: the game's own track code
		// says where it is), then Finish End Pos Edit
		host::TrackLoaded before, after;
		int32_t startBefore[3], endBefore[3], startAfter[3], endAfter[3];

		bool ok = host::TrackLoad(open, id, &before) == host::TrackEditResult::Done && before.track.sections.size() > trackEd.current;

		if (ok)
		{
			host::TrackData_MarkerAt(before.track, trackEd.current, false, host::TrackPlay_ObjectPlace, startBefore);
			host::TrackData_MarkerAt(before.track, trackEd.current, true, host::TrackPlay_ObjectPlace, endBefore);
		}

		static bool pivotOpen = false;

		if (!pivotOpen)
		{
			// the split screen is on first: Edit Pivot Point turns it off (FUN_004f3770 0x4f37aa) and shows the three views; a frame later the rest
			placementSplit = true;
			ok = ok && PressTheBarItem("Edit Section End Position") && trackEd.state == host::TrackState::EndEdit && PressTheBarItem("Edit Pivot Point") && trackEd.state == host::TrackState::PivotEdit;

			bool const views = ThreeViewsOn();

			say(std::string("Edit Pivot Point with the split screen on: the split screen is off now (") + (!placementSplit ? "yes" : "NO") + "), the three views are shown (" + (views ? "yes" : "NO") + ")");

			if (!ok || placementSplit || !views)
			{
				fail("Edit Pivot Point did not turn the split screen off and show the three views");
				return;
			}

			harnessPictureAsked = 9;
			pivotOpen = true;
			return;
		}

		ok = ok && PressTheBarItem("Move Pivot To Centre") && threeCursor[0] == 0 && threeCursor[1] == 0 && threeCursor[2] == 0;
		ok = ok && PressTheBarItem("Stop Pivot Edit") && trackEd.state == host::TrackState::EndEdit && host::TrackLoad(open, id, &after) == host::TrackEditResult::Done && after.track.sections.size() > trackEd.current;

		if (ok)
		{
			host::TrackSection const &s = after.track.sections[trackEd.current];

			host::TrackData_MarkerAt(after.track, trackEd.current, false, host::TrackPlay_ObjectPlace, startAfter);
			host::TrackData_MarkerAt(after.track, trackEd.current, true, host::TrackPlay_ObjectPlace, endAfter);

			bool const stays = true;
			bool nearStart = true, nearEnd = true;

			for (int axis = 0; axis < 3; axis++)
			{
				nearStart = nearStart && std::abs(startAfter[axis] - startBefore[axis]) <= 4;
				nearEnd = nearEnd && std::abs(endAfter[axis] - endBefore[axis]) <= 4;
			}

			ok = stays && s.offset[0] == 0 && s.offset[1] == 0 && s.offset[2] == 0 && nearStart && nearEnd;
			say(std::string("Edit Pivot Point, Move Pivot To Centre, Stop Pivot Edit: the section's object offset is 0 and the object stays at its start and end places (by the game's own code) ") + (ok ? "= yes" : "= NO"));
		}

		ok = ok && PressTheBarItem("Finish End Pos Edit") && trackEd.state == host::TrackState::Loaded;

		if (!ok)
		{
			fail("the pivot point edit did not keep the object where it was: " + words());
			return;
		}

		step = 6;
		return;
	}

	if (step == 6)
	{
		// PLAY: the game's own track code moves the object (a cross in the three views); the pose has to change
		if (!PressTheBarItem("Play Track") || trackEd.state != host::TrackState::Playing || !trackPlaying)
		{
			fail("Play Track did not start the game's track code: " + words());
			return;
		}

		startPose = trackPose;
		playStarted = GetTickCount64();
		moved = false;
		say("Play Track: " + words() + " (starts at " + std::to_string((long long)startPose.place[0]) + " " + std::to_string((long long)startPose.place[1]) + " " + std::to_string((long long)startPose.place[2]) + ")");
		step = 7;
		return;
	}

	if (step == 7)
	{
		moved = moved || trackPose.place[0] != startPose.place[0] || trackPose.place[1] != startPose.place[1] || trackPose.place[2] != startPose.place[2];

		if (!moved && GetTickCount64() - playStarted < 30000 && trackPlaying)
		{
			return;
		}

		if (!moved)
		{
			fail("the game's track code did not move the object in 30 seconds of play");
			return;
		}

		say("Play Track: the object moved to " + std::to_string((long long)trackPose.place[0]) + " " + std::to_string((long long)trackPose.place[1]) + " " + std::to_string((long long)trackPose.place[2]) + ", section " + std::to_string((long long)trackPose.section + 1));
		harnessPictureAsked = 3;
		step = 8;
		return;
	}

	if (step == 8)
	{
		if (trackPlaying && (!PressTheBarItem("Stop track play") || trackPlaying || trackEd.state != host::TrackState::Loaded))
		{
			fail("Stop track play did not stop: " + words());
			return;
		}

		SaveTheCopy();

		if (savedWords.compare(0, 14, "Saved a copy: ") != 0)
		{
			fail(savedWords);
			return;
		}

		copyPath = savedWords.substr(14);
		say("Stop track play, Save a copy: " + copyPath);
		harnessPictureAsked = 4;
		step = 9;
		return;
	}

	if (step == 9)
	{
		// the loader: the copy is opened as the game opens a level
		OpenLevelFile again;
		bool good = false;
		int32_t got[3] = { 0, 0, 0 };

		if (again.Open(copyPath.c_str()) == LevelOpenResult::Opened)
		{
			host::TrackLoaded copy;

			if (host::TrackLoad(&again, id, &copy) == host::TrackEditResult::Done && copy.had && copy.track.sections.size() == sectionsBefore + 1)
			{
				host::TrackData_MarkerAt(copy.track, sectionsBefore, true, host::TrackPlay_ObjectPlace, got);
				good = std::abs(got[0] - cursorSet[0]) <= 4 && std::abs(got[1] - cursorSet[1]) <= 4 && std::abs(got[2] - cursorSet[2]) <= 4 && copy.track.sections[sectionsBefore].time == 163840;

				bool hasSound = false;

				for (host::TrackSound const &sound : copy.sounds)
				{
					hasSound = hasSound || (sound.index == 1 && sound.wav == "door_open.wav");
				}

				good = good && hasSound;
			}
		}

		say(std::string("the game's loader on the copy: ") + (good ? "takes it; the object's track has the new section ending at the cursor, taking 2.5 seconds, with the start sound" : "REFUSED it or the track is not as edited"));

		if (!good)
		{
			fail("the saved copy does not hold the edit");
			return;
		}

		step = 10;
		return;
	}

	if (step == 10)
	{
		// Delete > Section takes the new section out again; Stop Track Edit goes back to Object Placement
		host::TrackLoaded now;
		bool const deleted = PressTheBarItem("Delete: Section") && host::TrackLoad(open, id, &now) == host::TrackEditResult::Done && now.track.sections.size() == sectionsBefore;
		bool const left = deleted && PressTheBarItem("Stop Track Edit") && !OnTheTrackScreen() && trackEd.state == host::TrackState::Off && strcmp(host::kIntengScreens[barScreen].name, "Object Placement") == 0;

		say(std::string("Delete > Section: ") + (deleted ? "back to " + std::to_string((unsigned long long)sectionsBefore) + " section(s)" : "did NOT delete") + "; Stop Track Edit: " + (left ? "back on Object Placement" : "did NOT leave the Track Edit screen"));
		harnessPictureAsked = 5;

		if (!(deleted && left))
		{
			fail("Delete > Section or Stop Track Edit did not work");
			return;
		}

		// Stop Track Edit gives the OBJECT the pose its track has at the timer start (inteng FUN_004eff90 0x4effba): the object's record in the level is that pose
		{
			host::TrackLoaded stopped;
			int32_t recordPlace[3], posePlace[3];
			float recordQuat[4], poseQuat[4];

			bool const read = host::TrackLoad(open, id, &stopped) == host::TrackEditResult::Done && stopped.had && host::TrackObjectPlace(open, id, recordPlace, recordQuat) && host::TrackPlay_PoseAtStartAsFile(stopped.track, posePlace, poseQuat);
			double const dot = read ? fabs((double)recordQuat[0] * poseQuat[0] + (double)recordQuat[1] * poseQuat[1] + (double)recordQuat[2] * poseQuat[2] + (double)recordQuat[3] * poseQuat[3]) : 0.0;
			bool const same = read && recordPlace[0] == posePlace[0] && recordPlace[1] == posePlace[1] && recordPlace[2] == posePlace[2] && dot > 0.9999;

			say(std::string("Stop Track Edit: the object's record is ") + (same ? "the track's pose at its timer start" : "NOT the track's pose at its timer start") + (read ? ": record " + std::to_string((long long)recordPlace[0]) + " " + std::to_string((long long)recordPlace[1]) + " " + std::to_string((long long)recordPlace[2]) + ", pose " + std::to_string((long long)posePlace[0]) + " " + std::to_string((long long)posePlace[1]) + " " + std::to_string((long long)posePlace[2]) + ", turns agree to " + std::to_string(dot) : std::string(": the track or the record could not be read")));

			if (!same)
			{
				fail("Stop Track Edit left the object's record at a pose that is not the track's pose at its timer start");
				return;
			}
		}

		step = 12;
		return;
	}

	if (step == 12)
	{
		// PASTE TRACK RELATIVE: Copy Track on the object, another object is selected, Paste Track Relative: the Track Edit screen is shown with the pasted track, which is turned and moved so that
		// its pose at the timer start (by the game's own code) is that object's place and turn (the shape of the track is kept)
		std::vector<host::LevelObject> found;

		LevelObjects_Read(&found);

		uint8_t other[8];
		bool haveOther = false;

		for (host::LevelObject const &one : found)
		{
			host::Selected const now = host::Selection_Get();

			if (!haveOther && memcmp(one.id, id, 8) != 0)
			{
				int32_t place[3];
				float quat[4];

				if (host::TrackObjectPlace(open, one.id, place, quat) && (place[0] != 0 || place[1] != 0 || place[2] != 0) && host::TrackExists(open, one.id))
				{
					memcpy(other, one.id, 8);
					haveOther = true;
				}
			}

			(void)now;
		}

		if (!haveOther || !PressTheBarItem("Copy Track") || !statusWords || strstr(statusWords, "Track copied") == nullptr)
		{
			fail("Copy Track (or a second object to paste on) was not there: " + words());
			return;
		}

		host::TrackLoaded source;

		host::TrackLoad(open, id, &source);

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, other, 8);
		host::Selection_Set(picked);
		ShowTheSelected(host::Selection_Get());

		if (!PressTheBarItem("Paste Track Relative") || !OnTheTrackScreen() || trackEd.state != host::TrackState::Loaded)
		{
			fail("Paste Track Relative did not show the pasted track in the Track Edit screen: " + words());
			return;
		}

		host::TrackLoaded pasted;
		int32_t objectPlace[3];
		float objectQuat[4];
		int32_t pose[3];
		int32_t poseMatrix[9];

		bool ok = host::TrackLoad(open, other, &pasted) == host::TrackEditResult::Done && pasted.had && pasted.track.sections.size() == source.track.sections.size() &&
			host::TrackObjectPlace(open, other, objectPlace, objectQuat) && host::TrackPlay_PoseAtStart(pasted.track, pose, poseMatrix);

		bool placeOk = false, turnOk = false, shapeOk = false;

		if (ok)
		{
			placeOk = std::abs(pose[0] - objectPlace[0]) <= 4 && std::abs(pose[1] - objectPlace[1]) <= 4 && std::abs(pose[2] - objectPlace[2]) <= 4;

			QUAT q;
			MATRIXCH m;

			q.quatx = (int)(-objectQuat[0] * 65536.0f);
			q.quaty = (int)(-objectQuat[1] * 65536.0f);
			q.quatz = (int)(-objectQuat[2] * 65536.0f);
			q.quatw = (int)(objectQuat[3] * 65536.0f);
			QuatToMat(&q, &m);

			int32_t const want[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

			turnOk = true;

			for (int k = 0; k < 9; k++)
			{
				turnOk = turnOk && std::abs(poseMatrix[k] - want[k]) <= 400;
			}

			// a rigid move: every section's length from start to end is as it was
			shapeOk = true;

			for (size_t i = 0; i < pasted.track.sections.size(); i++)
			{
				double before = 0, after = 0;

				for (int axis = 0; axis < 3; axis++)
				{
					double const a = (double)source.track.sections[i].pivotEnd[axis] - source.track.sections[i].pivotStart[axis];
					double const b = (double)pasted.track.sections[i].pivotEnd[axis] - pasted.track.sections[i].pivotStart[axis];

					before += a * a;
					after += b * b;
				}

				shapeOk = shapeOk && fabs(sqrt(before) - sqrt(after)) <= 8.0 + 0.002 * sqrt(before);
			}
		}

		say(std::string("Copy Track, then Paste Track Relative on another object: the Track Edit screen shows it (") + words() + "); the track's pose at its timer start is the object's place " + (placeOk ? "= yes" : "= NO") +
			" and turn " + (turnOk ? "= yes" : "= NO") + "; every section keeps its length " + (shapeOk ? "= yes" : "= NO"));
		harnessPictureAsked = 6;

		if (!(ok && placeOk && turnOk && shapeOk))
		{
			fail("Paste Track Relative did not put the track onto the object");
			return;
		}

		bool const left = PressTheBarItem("Stop Track Edit") && !OnTheTrackScreen();

		say(left ? "Track Editing main job: WORKS in the running editor" : "Track Editing main job: DOES NOT WORK in the running editor");
		step = 99;
	}
}

// -exitproof (a hidden -picture run): the Strategy mode's leftovers and the Exit sweep through the bar, the user's way: Construction > Mode > Strategy Data -> View Flag (Airduct) colours the modules that
// hold it (a picture) and Stop Flag View clears them -> Help shows inteng's 829 letters -> Assign Strategy on an object (the level changes) -> Exit asks "Would you like to save first?", yes saves a copy
// (the game's loader takes it and the object keeps the strategy), "Camera position saved", the hub is shown -> Strategy Data again, Exit with nothing changed: no question -> Object Placement:
// a change (Copy Track, Paste Track Absolute, Stop Track Edit), Exit saves a copy without asking and shows the hub. (Added 2026-11-15 by Melanikus (Frost).)
// -sclickproof (a hidden -picture run): Strategy Data's S + click through the window's own pick path: Construction > Mode > Strategy Data, then S held (the Classic layout) and a press at places of the view until one finds a
// textured polygon. Said and checked: the selection grew; every module in it uses the clicked polygon's picture (by NAME, read a second way, UvEdit_ReadPolygon) for more than half its polygons; and a sample of the
// modules left out do not. A second S + click adds without letting go of the first, and a click without S still picks the one object. (Added 2026-11-15 by Melanikus (Frost).)
// -modelproof (a hidden -picture run on a file of objects with no modules: marwep, MelPyth_MarWep ...): the user's way through the window: the objects are drawn (ModelObjects_Count > 0 and a picture), a click on one picks it
// (a place of the view found by trying a grid; the camera flown back a little from the origin as a user would), the Hierarchy screen is entered from the Construction bar, and the Hierarchy mode is open on THAT object's
// hierarchy with its first sequence selected and playing (the view's position moves). Said and checked: each step. (Added 2026-11-15 by Melanikus (Frost), the marwep fix.)
void RunTheModelProof(long frame)
{
	static int step = 0;
	static long last = 0;
	static int tries = 0;
	static int32_t position = -1;
	static long playedFrom = 0;

	if (frame < 5 || frame < last + 3 || step == 99)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: model proof: %s\n", words.c_str());
		fflush(stdout);
	};

	auto const fail = [&](std::string const &words)
	{
		say("Model file: DOES NOT WORK: " + words);
		step = 99;
	};

	if (proofViewWidth < 64 || !theCamera)
	{
		return;
	}

	if (step == 0)
	{
		size_t const count = ModelObjects_Count();

		say("the file's objects drawn: " + std::to_string((unsigned long long)count));

		if (count == 0)
		{
			fail("no object of the file was drawn");
			return;
		}

		// a user flies back a little from the origin to see them all (the keys do that; here the camera is put there)
		int32_t mins[3], maxs[3];

		if (ModelObjects_Bounds(mins, maxs))
		{
			theCamera->x = (int)(((int64_t)mins[0] + maxs[0]) / 2);
			theCamera->y = (int)(((int64_t)mins[1] + maxs[1]) / 2);
			// back from the middle by 1.5 times the file's widest extent (4.5 m at the most): a small gun fills the view as a big one does
			int64_t widest = 0;

			for (int axis = 0; axis < 3; axis++)
			{
				widest = (std::max)(widest, (int64_t)maxs[axis] - mins[axis]);
			}

			theCamera->z = (int)(((int64_t)mins[2] + maxs[2]) / 2 - (std::min<int64_t>)(4500, (std::max<int64_t>)(1200, widest * 3 / 2)));
			theCamera->turn = 0;
			theCamera->look = 0;
			FlyCamera_Hold(theCamera);
		}

		host::Selection_Clear();
		harnessPictureAsked = 1;
		step = 1;
		return;
	}

	if (step == 1)
	{

		if (host::Selection_Get().kind == host::SelectedKind::Object)
		{
			uint8_t picked[8];

			if (SelectedObjectId(picked))
			{
				std::string objectName;

				if (!HierarchyOfObject(picked, &objectName).empty())
				{
					step = 2;
					return;
				}

				say("press " + std::to_string((long long)tries) + " picked " + objectName + ", which is no part of a hierarchy: the next place");
				host::Selection_Clear();
			}
		}

		if (tries >= 63)
		{
			// no click at these places hit a part of a hierarchy: one is selected by its ID (what a click on it does), and the proof says so
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			std::vector<std::string> hierarchies, parts;
			std::vector<host::LevelObject> rows;

			LevelObjects_Read(&rows);

			if (open && host::HierarchyEdit_Names(open, &hierarchies) == host::HierarchyEditResult::Done && !hierarchies.empty() &&
				host::HierarchyEdit_SectionObjects(open, hierarchies[0], &parts) == host::HierarchyEditResult::Done && !parts.empty())
			{
				// the first section that has an object in the file (a section like "Root" may have none)
				for (std::string const &part : parts)
				{
					for (host::LevelObject const &row : rows)
					{
						if (row.name && part == row.name)
						{
							host::Selected one = {};

							one.kind = host::SelectedKind::Object;
							memcpy(one.objectId, row.id, 8);
							host::Selection_Set(one);
							say("no click at 63 places hit a part of a hierarchy: " + part + " is selected by its ID instead");
							step = 2;
							return;
						}
					}
				}
			}

			{
				std::string detail = "the file lists " + std::to_string((unsigned long long)hierarchies.size()) + " hierarchies";

				for (size_t i = 0; i < hierarchies.size() && i < 6; i++)
				{
					std::vector<std::string> objects;

					host::HierarchyEdit_SectionObjects(open, hierarchies[i], &objects);
					detail += "; " + hierarchies[i] + ": " + std::to_string((unsigned long long)objects.size()) + " section objects" + (objects.empty() ? "" : " (first " + objects[0] + ")");
					for (size_t k = 0; k < objects.size() && k < 8; k++) detail += " [" + objects[k] + "]";
					detail += " rows:";
					for (size_t k = 0; k < rows.size() && k < 8; k++) detail += std::string(" <") + (rows[k].name ? rows[k].name : "?") + ">";
				}

				fail("a click at 63 places of the view picked no part of a hierarchy (" + detail + ")");
			}
			return;
		}

		pickX = proofViewWidth * (10 + (tries % 9) * 10) / 100;          // a grid of 9 by 7 places over the view
		pickY = proofViewHeight * (30 + ((tries / 9) % 7) * 10) / 100;
		pickInBox = false;
		pickShift = false;
		pickAsked = true;
		tries++;
		return;
	}

	if (step == 2)
	{
		std::vector<host::LevelObject> rows;
		std::string name = "?";
		uint8_t id[8];

		LevelObjects_Read(&rows);

		if (SelectedObjectId(id))
		{
			for (host::LevelObject const &row : rows)
			{
				if (memcmp(row.id, id, 8) == 0 && row.name)
				{
					name = row.name;
				}
			}
		}

		say("a click picked the object \"" + name + "\" (press " + std::to_string((long long)tries) + ")");

		// the camera stays where the user left it across modes (inteng saves it to slot 10 on leaving a mode and restores it on entering the next: FUN_004704d0 / FUN_00470550), and the hierarchy is at the world's origin
		// (reassess/hierarchy-view-trace.md): the user flies back to the origin to see it, here the camera is put there
		theCamera->x = 0;
		theCamera->y = 0;
		theCamera->z = 0;
		theCamera->turn = 0;
		theCamera->look = 0;
		FlyCamera_Hold(theCamera);

		if (!PressTheBarItem("Hierarchy") || !onHierarchy)
		{
			fail("the Hierarchy screen could not be entered from the Construction bar");
			return;
		}

		say(std::string("the Hierarchy screen is entered: the model is open (") + (hierarchyModelOpen ? "yes" : "NO") + "), the hierarchy is \"" + hierarchyMode.hierarchy + "\", a sequence is selected (" +
			(hierarchyMode.sequenceSelected ? "yes" : "NO") + "), the view shows it (" + (HierarchyView_IsShown() ? "yes" : "NO") + ")");

		if (!hierarchyModelOpen || hierarchyMode.hierarchy.empty() || !hierarchyMode.sequenceSelected)
		{
			fail("the Hierarchy mode did not open on the picked object's hierarchy: " + std::string(statusWords ? statusWords : ""));
			return;
		}

		harnessPictureAsked = 2;
		step = 3;
		return;
	}

	if (step == 3)
	{
		if (!HierarchyView_IsShown())
		{
			fail("the view shows no hierarchy: " + std::string(statusWords ? statusWords : ""));
			return;
		}

		HierarchyView_Play(true, true);
		position = HierarchyView_Position();
		playedFrom = frame;

		{
			std::vector<HierarchyViewSection> sections;

			if (HierarchyView_Sections(&sections) && !sections.empty())
			{
				say("the shown hierarchy's first section " + sections[0].name + " is at " + std::to_string((long long)sections[0].world[0]) + " " + std::to_string((long long)sections[0].world[1]) + " " + std::to_string((long long)sections[0].world[2]) +
					" (radius " + std::to_string((long long)sections[0].radius) + "), " + std::to_string((unsigned long long)sections.size()) + " sections; the camera is at " + std::to_string((long long)theCamera->x) + " " + std::to_string((long long)theCamera->y) + " " + std::to_string((long long)theCamera->z) +
					", turn " + std::to_string((long long)theCamera->turn) + " look " + std::to_string((long long)theCamera->look));

				std::string list = "sections (name < parent @ place):";

				for (size_t i = 0; i < sections.size() && i < 24; i++)
				{
					list += " " + sections[i].name + "<" + std::to_string((long long)sections[i].parent) + "@" + std::to_string((long long)sections[i].world[0]) + "," + std::to_string((long long)sections[i].world[1]) + "," + std::to_string((long long)sections[i].world[2]);
				}

				say(list);
			}
		}

		step = 4;
		return;
	}

	if (step == 4)
	{
		if (frame < playedFrom + 20)
		{
			return;
		}

		int32_t const now = HierarchyView_Position();

		say("the sequence plays: its position went from " + std::to_string((long long)position) + " to " + std::to_string((long long)now) + " of 65535");
		harnessPictureAsked = 3;
		say(now != position && HierarchyView_IsPlaying() ? "Model file: WORKS in the running editor (objects drawn, one picked, Hierarchy opened on its hierarchy, its sequence playing)" : "Model file: DOES NOT WORK: the sequence did not move");
		step = 5;
		return;
	}

	if (step == 5)
	{
		// held at the first frame, so every picture below shows the same pose (the sequence plays on its own clock otherwise)
		HierarchyView_Play(false, false);
		HierarchyView_SetPosition(0);
		// C: Use Camera Root (command 0x37): the camera goes to the hierarchy's Camera Root section and looks level
		SendMessage(editorWindow, WM_KEYDOWN, 'C', 0);
		last = frame;
		step = 6;
		return;
	}

	if (step == 6)
	{
		std::vector<HierarchyViewSection> sections;
		bool found = false;
		int32_t at[3] = { 0, 0, 0 };

		if (HierarchyView_Sections(&sections))
		{
			for (HierarchyViewSection const &section : sections)
			{
				if (section.name == "Camera Root")
				{
					found = true;
					memcpy(at, section.world, sizeof(at));
				}
			}
		}

		bool const there = found && std::abs(theCamera->x - at[0]) <= 2 && std::abs(theCamera->y - at[1]) <= 2 && std::abs(theCamera->z - at[2]) <= 2;

		say(std::string("C (Use Camera Root): ") + (hierarchyCameraRoot ? "on" : "NOT on") + "; the hierarchy has a Camera Root section (" + (found ? "yes" : "no") + ") at " + std::to_string((long long)at[0]) + " " + std::to_string((long long)at[1]) + " " + std::to_string((long long)at[2]) +
			"; the camera is at " + std::to_string((long long)theCamera->x) + " " + std::to_string((long long)theCamera->y) + " " + std::to_string((long long)theCamera->z) + ", turn " + std::to_string((long long)theCamera->turn) + " look " + std::to_string((long long)theCamera->look) +
			(there ? " = at the section" : (found ? " (NOT at the section)" : " (no section to go to: only the level facing is set)")));
		harnessPictureAsked = 4;
		say(hierarchyCameraRoot && theCamera->turn == 0 && theCamera->look == 0 && (there || !found) ? "Camera root: WORKS in the running editor" : "Camera root: DOES NOT WORK");

		step = 7;
		return;
	}

	if (step == 7)
	{
		// C again (off), then the camera 2 m behind the hierarchy's extent, looking at it: how the same hierarchy looks from outside (picture 5)
		SendMessage(editorWindow, WM_KEYDOWN, 'C', 0);
		last = frame;
		step = 8;
		return;
	}

	if (step == 8)
	{
		ModelObjects_Close();   // only the hierarchy (the file's own objects removed) in the pictures that follow
		int32_t mins[3], maxs[3];

		if (HierarchyView_Extent(mins, maxs))
		{
			theCamera->x = (int)(((int64_t)mins[0] + maxs[0]) / 2);
			theCamera->y = (int)(((int64_t)mins[1] + maxs[1]) / 2);
			theCamera->z = mins[2] - 2500;
			theCamera->turn = 0;
			theCamera->look = 0;
			FlyCamera_Hold(theCamera);
			say("camera back at " + std::to_string((long long)theCamera->x) + " " + std::to_string((long long)theCamera->y) + " " + std::to_string((long long)theCamera->z) + " (hierarchy extent " + std::to_string((long long)mins[0]) + " " + std::to_string((long long)mins[1]) + " " + std::to_string((long long)mins[2]) + " to " + std::to_string((long long)maxs[0]) + " " + std::to_string((long long)maxs[1]) + " " + std::to_string((long long)maxs[2]) + ")");
		}

		{
			extern float CameraZoomScale;

			extern double local_scale;

			say("view: field of view " + std::to_string((double)d3d.fieldOfView) + ", zoom scale " + std::to_string((double)CameraZoomScale) + ", file scale " + std::to_string(local_scale));
		}

		harnessPictureAsked = 5;
		step = 10;
		return;
	}

	if (step == 10 || step == 11)
	{
		// the camera at the Camera Root section's place, then 400 and 1000 mm back from it, looking level (pictures 6 and 7): where the HUD gun comes into view
		std::vector<HierarchyViewSection> sections;
		int32_t at[3] = { 0, 0, 0 };

		if (HierarchyView_Sections(&sections))
		{
			for (HierarchyViewSection const &section : sections)
			{
				if (section.name == "Camera Root")
				{
					memcpy(at, section.world, sizeof(at));
				}
			}
		}

		int const back = step == 10 ? 400 : 1000;

		theCamera->x = at[0];
		theCamera->y = at[1];
		theCamera->z = at[2] - back;
		theCamera->turn = 0;
		theCamera->look = 0;
		FlyCamera_Hold(theCamera);
		say("camera at the Camera Root minus " + std::to_string((long long)back) + " mm: " + std::to_string((long long)theCamera->x) + " " + std::to_string((long long)theCamera->y) + " " + std::to_string((long long)theCamera->z));
		// the section named Barrels (or the first with a shape) is selected: it is drawn yellow, and where the listing says it is (the pinhole formula, 75 degrees from top to bottom) is printed beside
		{
			int pick = -1;

			for (size_t i = 0; i < sections.size() && pick < 0; i++)
			{
				if (sections[i].hasShape && (sections[i].name.find("Barrel") != std::string::npos || sections[i].name.find("blade") != std::string::npos))
				{
					pick = (int)i;
				}
			}

			if (pick >= 0)
			{
				HierarchyView_SelectSection(pick);

				double const relX = sections[(size_t)pick].world[0] - theCamera->x, relY = sections[(size_t)pick].world[1] - theCamera->y, relZ = sections[(size_t)pick].world[2] - theCamera->z;
				double const focal = 729.0 / 2.0 / tan(37.5 * 3.14159265358979 / 180.0);

				say("yellow section " + sections[(size_t)pick].name + " (the draw put it " + std::to_string((long long)sections[(size_t)pick].view[0]) + " " + std::to_string((long long)sections[(size_t)pick].view[1]) + " " + std::to_string((long long)sections[(size_t)pick].view[2]) + " from the camera; camera is at " + std::to_string((long long)theCamera->x) + " " + std::to_string((long long)theCamera->y) + " " + std::to_string((long long)theCamera->z) + ") is at " + std::to_string((long long)sections[(size_t)pick].world[0]) + " " + std::to_string((long long)sections[(size_t)pick].world[1]) + " " + std::to_string((long long)sections[(size_t)pick].world[2]) + ", so should show at pixel " + std::to_string(relZ > 0 ? (long long)(504.0 + relX / relZ * focal) : -1LL) + "," + std::to_string(relZ > 0 ? (long long)(364.0 + relY / relZ * focal) : -1LL));
			}
		}

		{
			int32_t eye[3] = { 0, 0, 0 };

			LevelView_EyePlace(eye);
			say("the game's eyes were last at " + std::to_string((long long)eye[0]) + " " + std::to_string((long long)eye[1]) + " " + std::to_string((long long)eye[2]) + " (the camera I set before that frame is the previous one)");
		}
		harnessPictureAsked = step == 10 ? 6 : 7;
		step = step == 10 ? 11 : 9;
		return;
	}

	if (step == 9)
	{
		// the Camera Root section of EVERY hierarchy of the file (each is shown once, sequence 0, and its section places read): the proof's note for the HUD files
		{
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			std::vector<std::string> names;

			if (open && host::HierarchyEdit_Names(open, &names) == host::HierarchyEditResult::Done)
			{
				for (std::string const &hierarchy : names)
				{
					int32_t place[3];
					std::string why;
					std::vector<HierarchyViewSection> each;
					std::vector<host::SequenceInfo> sequences;

					host::HierarchyEdit_Sequences(open, hierarchy, &sequences);

					if (sequences.empty() || !ShowTheModelInFront(hierarchyMode.modelName.c_str(), hierarchy.c_str(), sequences[0].number, sequences[0].subNumber, place, &why) || !HierarchyView_Sections(&each))
					{
						say("hierarchy " + hierarchy + ": could not be shown (" + why + ")");
						continue;
					}

					std::string line = "hierarchy " + hierarchy + ": " + std::to_string((unsigned long long)each.size()) + " sections, Camera Root: ";
					bool has = false;

					for (HierarchyViewSection const &section : each)
					{
						if (section.name == "Camera Root")
						{
							has = true;
							line += "yes, at " + std::to_string((long long)section.world[0]) + " " + std::to_string((long long)section.world[1]) + " " + std::to_string((long long)section.world[2]);
						}
					}

					int32_t mins[3] = { 0, 0, 0 }, maxs[3] = { 0, 0, 0 };

					if (HierarchyView_Extent(mins, maxs))
					{
						line += std::string(has ? "" : "NONE") + "; extent " + std::to_string((long long)mins[0]) + " " + std::to_string((long long)mins[1]) + " " + std::to_string((long long)mins[2]) + " to " + std::to_string((long long)maxs[0]) + " " + std::to_string((long long)maxs[1]) + " " + std::to_string((long long)maxs[2]);
					}
					else
					{
						line += has ? "" : "NONE";
					}

					say(line);
				}
			}
		}


		step = 12;
		return;
	}

	if (step >= 12 && step <= 15)
	{
		// does the pose MOVE while the sequence plays, with C off (steps 12-13) and on (14-15)? The first sections' places and the camera, 25 frames apart
		static std::string before;
		std::vector<HierarchyViewSection> sections;
		std::string now;

		HierarchyView_Sections(&sections);

		for (size_t i = 0; i < sections.size() && i < 14; i++)
		{
			now += (i ? " " : "") + sections[i].name + "@" + std::to_string((long long)sections[i].world[0]) + "," + std::to_string((long long)sections[i].world[1]) + "," + std::to_string((long long)sections[i].world[2]);
		}

		now += " camera " + std::to_string((long long)theCamera->x) + "," + std::to_string((long long)theCamera->y) + "," + std::to_string((long long)theCamera->z) + " position " + std::to_string((long long)HierarchyView_Position()) + (HierarchyView_IsPlaying() ? " playing" : " paused");

		if (step == 12 || step == 14)
		{
			if (step == 14)
			{
				SendMessage(editorWindow, WM_KEYDOWN, 'C', 0);
			}

			HierarchyView_Play(true, true);
			before = now;
			playedFrom = frame;
			step++;
			return;
		}

		if (frame < playedFrom + 25)
		{
			last = frame - 3;
			return;
		}

		say(std::string("does it move, C ") + (step == 13 ? "off" : "on") + ": BEFORE " + before);
		say(std::string("does it move, C ") + (step == 13 ? "off" : "on") + ": AFTER  " + now);
		step = step == 13 ? 14 : 16;
		return;
	}

	if (step >= 16 && step <= 20)
	{
		// the Shotgun's Standard_Reload held at four places of its time, seen from the Camera Root (pictures 8 to 11)
		if (step == 16)
		{
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			std::vector<host::SequenceInfo> sequences;
			size_t found = sequences.size();

			host::HierarchyEdit_Sequences(open, "Shotgun", &sequences);

			for (size_t i = 0; i < sequences.size(); i++)
			{
				if (sequences[i].name.find("Reload") != std::string::npos && found == 0)
				{
					found = i + 1;
				}
			}

			int32_t place[3];
			std::string why;

			if (found == 0 || !ShowTheModelInFront(hierarchyMode.modelName.c_str(), "Shotgun", sequences[found - 1].number, sequences[found - 1].subNumber, place, &why))
			{
				say("Shotgun reload could not be shown: " + why);
				step = 99;
				return;
			}

			say("Shotgun " + sequences[found - 1].name + ": " + std::to_string((long long)sequences[found - 1].frames) + " frames, shown from the Camera Root");
			HierarchyView_Play(false, false);
			theCamera->x = 0;
			theCamera->y = 0;
			theCamera->z = 0;
			theCamera->turn = 0;
			theCamera->look = 0;
			FlyCamera_Hold(theCamera);
		}

		HierarchyView_SetPosition((step - 16) * 16000);
		say("Shotgun reload held at " + std::to_string((long long)(step - 16) * 16000));
		harnessPictureAsked = 8 + (step - 16);
		step = step == 19 ? 99 : step + 1;
		return;
	}

}

void RunTheSClickProof(long frame)
{
	static int step = 0;
	static long last = 0;
	static int tries = 0;
	static size_t before = 0;

	if (frame < 5 || frame < last + 3 || step == 99)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: sclick proof: %s\n", words.c_str());
		fflush(stdout);
	};

	auto const fail = [&](std::string const &words)
	{
		say("S + click: DOES NOT WORK: " + words);
		step = 99;
	};

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level() || proofViewWidth < 64)
	{
		return;
	}

	auto const onStrategy = [&]() { return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0; };

	if (step == 0)
	{
		if (!PressTheBarItem("Strategy Data: Normal") || !onStrategy())
		{
			fail("Strategy Data could not be entered");
			return;
		}

		keyLayout = KeyLayout::Classic;
		host::Selection_Clear();
		say("entered Strategy Data; the layout is Classic; the selection is empty");
		step = 1;
		return;
	}

	if (step == 1)
	{
		// S held: a press at one of the places of the view (the middle first)
		// a grid of 7 x 5 places over the view, and after each grid the camera turned a quarter round (four ways in all)
		if (tries >= 140)
		{
			fail("140 places of the view (four ways round) found no textured polygon whose texture most of a module uses");
			return;
		}

		if (tries > 0 && tries % 35 == 0 && theCamera)
		{
			theCamera->turn += 1024;
			FlyCamera_Hold(theCamera);
		}

		int const spotX[] = { 8, 22, 36, 50, 64, 78, 92 };
		int const spotY[] = { 15, 33, 50, 67, 85 };

		keyDown['S'] = true;
		pickX = proofViewWidth * spotX[tries % 7] / 100;
		pickY = proofViewHeight * spotY[(tries / 7) % 5] / 100;
		pickInBox = false;
		pickShift = false;
		pickWithS = keyLayout == KeyLayout::Classic && keyDown['S'] && onStrategy() && strategyFlagPage;   // the flag page's only (S-58)
		strategyTextureCount = 0;
		statusWords = nullptr;
		pickAsked = true;
		tries++;
		step = 2;
		return;
	}

	if (step == 2)
	{
		keyDown['S'] = false;

		std::string const words = statusWords ? statusWords : "";

		if (words.find("S + click: texture ") != 0)
		{
			step = 1;   // that place found no polygon, or a polygon with no texture: the next place
			return;
		}

		size_t const count = host::Selection_SetCount();

		say("press " + std::to_string((long long)tries) + " (S held): \"" + words + "\"; the selection holds " + std::to_string((unsigned long long)count));

		if (count == 0)
		{
			step = 1;   // the texture is not most of any module's polygons: the next place
			return;
		}

		// the picture the first selected module uses most, by the picture NUMBER UvEdit_ReadPolygon gives (a second way of reading the polygons); it must be the number the status line said
		std::string name;
		std::vector<host::LevelObject> rows;

		LevelObjects_Read(&rows);

		auto const countNamed = [&](uint8_t const *id, std::string const &pictureName, size_t *polygons)
		{
			size_t used = 0;
			size_t all = 0;
			model::ShapeModel shape;
			std::string why;

			if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
			{
				return (size_t)0;
			}

			all = shape.polygons.size();

			for (size_t i = 0; i < all; i++)
			{
				model::PolygonUv polygon;

				if (host::UvEdit_ReadPolygon(open, open->Level(), id, (int32_t)i, &polygon, &why) == host::UvEditResult::Done && polygon.textured && std::to_string((long long)polygon.pictureNumber) == pictureName)
				{
					used++;
				}
			}

			*polygons = all;
			return used;
		};

		// the number: the picture that most of the first selected module's polygons use
		uint8_t first[8];

		host::Selection_SetMember(0, first);

		{
			std::map<std::string, size_t> uses;
			model::ShapeModel shape;
			std::string why;

			if (host::ShapeEdit_Read(open->Level(), open, first, &shape, nullptr, &why) != host::ShapeEditResult::Done)
			{
				fail("the first selected module's shape could not be read");
				return;
			}

			for (size_t i = 0; i < shape.polygons.size(); i++)
			{
				model::PolygonUv polygon;

				if (host::UvEdit_ReadPolygon(open, open->Level(), first, (int32_t)i, &polygon, &why) == host::UvEditResult::Done && polygon.textured)
				{
					uses[std::to_string((long long)polygon.pictureNumber)]++;
				}
			}

			size_t best = 0;

			for (auto const &use : uses)
			{
				if (use.second > best)
				{
					best = use.second;
					name = use.first;
				}
			}
		}

		size_t bad = 0;

		for (size_t i = 0; i < count && i < 40; i++)
		{
			uint8_t id[8];
			size_t polygons = 0;

			host::Selection_SetMember(i, id);

			if (countNamed(id, name, &polygons) <= polygons / 2)
			{
				bad++;
			}
		}

		// a sample of the modules NOT selected: none of them may use that picture for more than half
		size_t wrongLeft = 0, sampled = 0;

		for (host::LevelObject const &row : rows)
		{
			int32_t index = 0;
			bool selected = false;

			if (!host::ModuleIndexOfId(LevelObjects_Level(), row.id, &index) || host::HiddenObjects_Is(row.id) || sampled >= 40)
			{
				continue;
			}

			for (size_t i = 0; i < count && !selected; i++)
			{
				uint8_t member[8];

				selected = host::Selection_SetMember(i, member) && memcmp(member, row.id, 8) == 0;
			}

			if (selected)
			{
				continue;
			}

			size_t polygons = 0;

			sampled++;

			if (countNamed(row.id, name, &polygons) > polygons / 2)
			{
				wrongLeft++;
			}
		}

		say("the picture is " + name + "; of the first " + std::to_string((unsigned long long)(count < 40 ? count : 40)) + " selected modules " + std::to_string((unsigned long long)bad) + " do not use it for more than half their polygons (must be 0); of " +
			std::to_string((unsigned long long)sampled) + " modules left out " + std::to_string((unsigned long long)wrongLeft) + " do (must be 0 unless the selection was full)");

		int said = -1;
		sscanf_s(words.c_str(), "S + click: texture %d", &said);
		say("the status line said texture " + std::to_string((long long)said) + "; the first module uses picture " + name + " most");

		if (name.empty() || std::to_string((long long)said) != name || bad != 0 || (wrongLeft != 0 && count < 256))
		{
			fail("the modules chosen are not the ones that are mostly that picture");
			return;
		}

		harnessPictureAsked = 1;
		before = count;
		step = 3;
		return;
	}

	if (step == 3)
	{
		// a press WITHOUT S picks the one object (the selection is then that one); no S + click was done
		keyDown['S'] = false;
		pickX = proofViewWidth / 2;
		pickY = proofViewHeight / 2;
		pickInBox = false;
		pickShift = false;
		pickWithS = false;
		pickAsked = true;
		step = 4;
		return;
	}

	if (step == 4)
	{
		size_t const now = host::Selection_SetCount();

		say("a click without S: the selection went from " + std::to_string((unsigned long long)before) + " to " + std::to_string((unsigned long long)now) + " (one pick replaces it: 1 or 0 when the place has none)");
		say(now <= 1 && before >= 1 ? "S + click: WORKS in the running editor (the modules that are mostly the clicked texture join the selection; a plain click still picks one)" : "S + click: DOES NOT WORK in the running editor");
		step = 99;
	}
}


void RunTheExitProof(long frame)
{
	static int step = 0;
	static long last = 0;
	static uint8_t id[8];
	static std::string copyPath;
	static std::vector<std::vector<uint8_t>> candidates;
	static size_t cand = 0;
	static int32_t kindMade = 0;
	static int viewPick = 0;

	if (frame < 5 || frame < last + 2 || step == 99)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: exit proof: %s\n", words.c_str());
		fflush(stdout);
	};

	auto const fail = [&](std::string const &words)
	{
		say("Exit sweep: DOES NOT WORK: " + words);
		step = 99;
	};

	auto const words = [&]() { return std::string(statusWords ? statusWords : ""); };
	auto const onScreen = [&](char const *name) { return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, name) == 0; };

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		fail("no level is open");
		return;
	}

	if (step == 0)
	{
		if (!PressTheBarItem("Strategy Data: Normal") || !onScreen("Strategy Data"))
		{
			fail("Strategy Data could not be entered");
			return;
		}

		bool const noChangeYet = !LevelChangedSinceSave();

		say(std::string("entered Strategy Data from Construction; the level is ") + (noChangeYet ? "unchanged since it was opened" : "CHANGED already"));
		step = 1;
		return;
	}

	if (step == 1)
	{
		// the keys of the Movement help, through the window as a person's: M selects all modules (Strategy Data), F3 restores the camera position that was saved in F3
		SendMessage(editorWindow, WM_KEYDOWN, 'M', 0);

		bool const selected = host::Selection_SetCount() > 0;
		FlyCamera const before = theCamera ? *theCamera : FlyCamera();

		CameraKey(3, true);

		if (theCamera)
		{
			theCamera->x += 5000;
			theCamera->turn += 100;
			FlyCamera_Hold(theCamera);
		}

		SendMessage(editorWindow, WM_KEYDOWN, VK_F3, 0);

		bool const restored = theCamera && theCamera->x == before.x && theCamera->y == before.y && theCamera->z == before.z && theCamera->turn == before.turn;

		say(std::string("M on Strategy Data selects ") + std::to_string((unsigned long long)host::Selection_SetCount()) + " modules (" + (selected ? "yes" : "NO") + "); F3 restores the camera position saved in F3 (" + (restored ? "yes" : "NO") + ")");
		host::Selection_Clear();

		if (!selected || !restored)
		{
			fail("the M or F3 key did not work");
			return;
		}

		step = 17;
		return;
	}

	if (step == 17)
	{
		// View Flag: the flag some module of the level holds (Airduct, Stair, Sky Visible, Fogged: the first with modules), the modules that hold it are coloured (the count is the modules' own), Stop Flag View clears them
		static char const *const names[4] = { "View Flag: Airduct", "View Flag: Stair", "View Flag: Sky Visible", "View Flag: Fogged" };
		static uint32_t const bits[4] = { 0x80000000u, 0x40000000u, 0x20000000u, 0x10000000u };
		size_t counts[4] = { 0, 0, 0, 0 };
		std::vector<host::LevelObject> rows;
		host::LevelStore store(open);

		LevelObjects_Read(&rows);

		for (host::LevelObject const &row : rows)
		{
			Chunk *flags = nullptr;
			std::vector<unsigned char> bytes;

			if (host::FindModuleFlags(LevelObjects_Level(), row.id, &flags) == host::LevelChunkFound::Found && flags && store.PayloadOf(flags, &bytes) && bytes.size() >= 4)
			{
				for (int k = 0; k < 4; k++)
				{
					counts[k] += (*(uint32_t const *)bytes.data() & bits[k]) != 0 ? 1 : 0;
				}
			}
		}

		int pick = 0;

		for (int k = 3; k >= 0; k--)
		{
			pick = counts[k] > 0 ? k : pick;
		}

		viewPick = pick;
		say(std::string("modules that hold: Airduct ") + std::to_string((unsigned long long)counts[0]) + ", Stair " + std::to_string((unsigned long long)counts[1]) + ", Sky Visible " + std::to_string((unsigned long long)counts[2]) + ", Fogged " + std::to_string((unsigned long long)counts[3]));

		if (!PressTheBarItem(names[pick]) || strategyViewBit != bits[pick])
		{
			fail(std::string(names[pick]) + " did not set the flag in view: " + words());
			return;
		}

		step = 2;
		return;
	}

	if (step == 2)
	{
		size_t expected = 0;
		std::vector<host::LevelObject> rows;
		host::LevelStore store(open);
		std::map<int32_t, int32_t> numbers;
		int32_t firstModule = -1;

		host::GameModuleNumbers(LevelObjects_Level(), &numbers);
		LevelObjects_Read(&rows);

		for (size_t index = 0; index < rows.size(); index++)
		{
			host::LevelObject const &row = rows[index];
			Chunk *flags = nullptr;
			std::vector<unsigned char> bytes;

			if (host::FindModuleFlags(LevelObjects_Level(), row.id, &flags) == host::LevelChunkFound::Found && flags && store.PayloadOf(flags, &bytes) && bytes.size() >= 4 && (*(uint32_t const *)bytes.data() & strategyViewBit) != 0)
			{
				expected++;

				if (firstModule < 0 && numbers.find((int32_t)index) != numbers.end())
				{
					firstModule = numbers[(int32_t)index];
				}
			}
		}

		bool const drawnRight = flagViewTintCount == (expected < kLevelViewMostSelected ? expected : kLevelViewMostSelected);

		say("View Flag (the one chosen): " + std::to_string((unsigned long long)expected) + " module(s) of the level hold the flag; " + std::to_string((unsigned long long)flagViewTintCount) + " are drawn orange " + (drawnRight ? "= yes" : "= NO") + " (picture-1.png)");

		if (!drawnRight)
		{
			fail("the modules that hold the flag are not the ones drawn");
			return;
		}

		// the camera goes into the first module that holds the flag, so the picture shows it drawn orange
		int32_t world[3];

		if (firstModule >= 0 && theCamera && LevelView_ModuleWorld(firstModule, world))
		{
			theCamera->x = world[0];
			theCamera->y = world[1];
			theCamera->z = world[2];
			FlyCamera_Hold(theCamera);
		}

		harnessPictureAsked = 1;
		step = 3;
		return;
	}

	if (step == 3)
	{
		bool const stopped = PressTheBarItem("Stop Flag View") && strategyViewBit == 0;

		textPanelText = nullptr;

		bool const help = PressTheBarItem("Help") && textPanelText == kIntengStrategyHelpText && strlen(kIntengStrategyHelpText) == 829;

		textPanelText = nullptr;
		say(std::string("Stop Flag View: ") + (stopped ? "the flag is out of view" : "NOT stopped") + "; Help: " + (help ? "inteng's 829 letters are shown" : "NOT shown"));

		if (!stopped || !help)
		{
			fail("Stop Flag View or Help did not work");
			return;
		}

		// the candidates for the Assign that makes the level change
		for (host::LevelObject const &one : std::vector<host::LevelObject>([&]() { std::vector<host::LevelObject> found; LevelObjects_Read(&found); return found; }()))
		{
			host::ObjectHeader header;

			if (LevelObjects_FindHeader(one.id, &header) && (header.flags & 0x400) != 0 && (header.flags & 0x200) == 0)
			{
				candidates.push_back(std::vector<uint8_t>(one.id, one.id + 8));
			}
		}

		step = 4;
		return;
	}

	if (step == 4)
	{
		if (cand >= candidates.size() || cand > 60)
		{
			fail("no placed object took a strategy (Assign)");
			return;
		}

		memcpy(id, candidates[cand].data(), 8);

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, id, 8);
		host::Selection_Set(picked);
		ShowTheSelected(host::Selection_Get());

		int32_t canTake[16];
		size_t const canCount = KindsTheObjectCanTake(id, canTake);

		kindMade = canCount ? canTake[0] : 0;

		if (kindMade == 0 || !PressTheBarItem("Assign Strategy"))
		{
			cand++;
			return;
		}

		promptText = std::to_string((long long)kindMade);
		RunThePrompt();

		if (promptFor == kPromptAssignReplace)
		{
			promptText = "yes";
			RunThePrompt();
		}

		if (!(statusWords && strstr(statusWords, "has a new strategy")))
		{
			cand++;
			return;
		}

		say("Assign Strategy: an object took strategy kind " + std::to_string((long long)kindMade) + "; the level is " + (LevelChangedSinceSave() ? "changed since the last save" : "NOT changed (wrong)"));
		step = 5;
		return;
	}

	if (step == 5)
	{
		// Exit: the question first
		if (!PressTheBarItem("Exit") || promptFor != kPromptExitSave || !onScreen("Strategy Data"))
		{
			fail("Exit did not ask \"Would you like to save first?\": " + words());
			return;
		}

		say("Exit on Strategy Data asks: Would you like to save first?");
		harnessPictureAsked = 2;
		step = 6;
		return;
	}

	if (step == 6)
	{
		promptText = "yes";
		RunThePrompt();

		bool const hub = onScreen("Construction");
		bool const saved = exitWords.find("Saved a copy: ") == 0 && exitWords.find("Camera position saved") != std::string::npos;

		if (saved)
		{
			size_t const at = exitWords.find("; Camera");

			copyPath = exitWords.substr(14, at == std::string::npos ? std::string::npos : at - 14);
		}

		bool good = false;
		OpenLevelFile again;

		if (saved && again.Open(copyPath.c_str()) == LevelOpenResult::Opened)
		{
			host::ObjectStrategy s;

			good = host::FindObjectStrategy(again.Level(), id, &s) == host::StrategyFound::Found && s.payload.size() >= 12 && *(int32_t const *)(s.payload.data() + 8) == kindMade;
		}

		say(std::string("yes: ") + (saved ? "\"" + exitWords + "\"" : "NOT saved: " + exitWords) + "; the hub is " + (hub ? "shown" : "NOT shown") + "; the game's loader on the copy " + (good ? "takes it and the object keeps the strategy" : "REFUSED it or the strategy is missing"));

		if (!saved || !hub || !good || LevelChangedSinceSave())
		{
			fail("Strategy Data's Exit with yes did not save and leave");
			return;
		}

		step = 7;
		return;
	}

	if (step == 7)
	{
		// Strategy Data again, nothing changed: Exit asks nothing, saves nothing, says the camera position was saved
		bool ok = PressTheBarItem("Strategy Data: Normal") && onScreen("Strategy Data") && PressTheBarItem("Exit") && promptFor == kPromptNone && onScreen("Construction") && exitWords == "Camera position saved";

		say(std::string("Strategy Data again with nothing changed: Exit ") + (ok ? "asks nothing, saves nothing, shows the hub (\"Camera position saved\")" : "did NOT behave (" + exitWords + ")"));

		if (!ok)
		{
			fail("Exit with nothing changed asked or saved");
			return;
		}

		step = 16;
		return;
	}

	if (step == 16)
	{
		// Tab on Object Placement enters Strategy mode (inteng's help), and Tab there goes back to Object Placement (not the hub), asking and saving nothing for an unchanged level; the key goes through the
		// window as a person's key does
		bool ok = PressTheBarItem("Object Placement") && onScreen("Object Placement");

		SendMessage(editorWindow, WM_KEYDOWN, VK_TAB, 0);
		ok = ok && onScreen("Strategy Data") && strategyFromPlacement;

		bool const entered = ok;

		SendMessage(editorWindow, WM_KEYDOWN, VK_TAB, 0);
		ok = ok && onScreen("Object Placement") && !strategyFromPlacement && promptFor == kPromptNone;

		say(std::string("Tab on Object Placement enters Strategy Data (") + (entered ? "yes" : "NO") + "); Tab there goes back to Object Placement, not the hub (" + (ok ? "yes" : "NO") + ")");

		if (!ok)
		{
			fail("Tab did not take Object Placement to Strategy mode and back");
			return;
		}

		// back to the hub for the next part
		ok = PressTheBarItem("Exit") && onScreen("Construction");

		if (!ok)
		{
			fail("Object Placement's Exit did not show the hub: " + words());
			return;
		}

		step = 8;
		return;
	}

	if (step == 8)
	{
		// Object Placement: a change (Copy Track, Paste Track Absolute, Stop Track Edit), then Exit saves a copy without asking
		std::vector<host::LevelObject> found;

		LevelObjects_Read(&found);

		bool haveTrack = false;

		for (host::LevelObject const &one : found)
		{
			host::TrackLoaded loaded;

			if (!haveTrack && host::TrackLoad(open, one.id, &loaded) == host::TrackEditResult::Done && loaded.had)
			{
				memcpy(id, one.id, 8);
				haveTrack = true;
			}
		}

		if (!haveTrack)
		{
			say("this level has no object with a track: Object Placement's Exit is not tried here");
			say("Exit sweep: WORKS (Strategy Data part; Object Placement part skipped: no track to change)");
			step = 99;
			return;
		}

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, id, 8);
		host::Selection_Set(picked);
		ShowTheSelected(host::Selection_Get());

		bool ok = PressTheBarItem("Object Placement") && onScreen("Object Placement") && PressTheBarItem("Copy Track") && PressTheBarItem("Paste Track Absolute") && onScreen("Track Editing") &&
			PressTheBarItem("Stop Track Edit") && onScreen("Object Placement") && LevelChangedSinceSave();

		if (!ok)
		{
			fail("the change on Object Placement did not happen: " + words());
			return;
		}

		ok = PressTheBarItem("Exit") && promptFor == kPromptNone && onScreen("Construction") && exitWords.find("Saved a copy: ") == 0 && !LevelChangedSinceSave();

		say(std::string("Object Placement's Exit after a change: ") + (ok ? "saved a copy without asking and showed the hub (\"" + exitWords + "\")" : "did NOT save and leave (" + exitWords + ")"));
		say(ok ? "Exit sweep: WORKS in the running editor (Strategy Data asks, the others save)" : "Exit sweep: DOES NOT WORK in the running editor");
		harnessPictureAsked = 3;
		step = 99;
	}
}

// -movementproof (a hidden -picture run): the Movement help's keys that the editor now has, through the window as a person's keys: keypad 9 and 7 roll the camera (a picture of the tilted view), Insert starts the zoom
// ("Zoom... Choose centre"), a press chooses the centre, the pointer moves the rectangle (checked against inteng's rule), a left press zooms in (the scale times width / (2 x half), and the camera turned toward the
// centre: its facing is checked against the direction through the pixel), a right press zooms out, Home puts the default zoom back, Esc cancels. (Added 2026-11-15 by Melanikus (Frost).)
void RunTheMovementProof(long frame)
{
	static int step = 0;
	static long last = 0;
	static int held = 0;
	static FlyCamera start = {};
	static int forwardBefore[3], rightBefore[3];
	static int pressAtX = 0, pressAtY = 0;

	if (frame < 5 || frame < last + 2 || step == 99)
	{
		return;
	}

	last = frame;

	auto const say = [](std::string const &words)
	{
		printf("AVP_OpenInteng: movement proof: %s\n", words.c_str());
		fflush(stdout);
	};

	auto const fail = [&](std::string const &words)
	{
		say("Movement block: DOES NOT WORK: " + words);
		step = 99;
	};

	if (!theCamera || zoomViewBox.width < 64)
	{
		return;   // the view is known after the first frames
	}

	if (step == 0)
	{
		start = *theCamera;
		keyDown[VK_NUMPAD9] = true;
		held = 0;
		step = 1;
		return;
	}

	if (step == 1)
	{
		// keypad 9 held for 8 frames: the camera rolls (the key is let go at the end of the 8)
		if (++held < 8)
		{
			return;
		}

		keyDown[VK_NUMPAD9] = false;

		int const rolled = theCamera->roll;

		say("keypad 9 held 8 frames: the camera's roll is " + std::to_string((long long)rolled) + " of 4096 (" + std::to_string((long long)(rolled * 360 / 4096)) + " degrees)" + (rolled != 0 ? "" : " = NOT rolled"));

		if (rolled == 0)
		{
			fail("keypad 9 did not roll the camera");
			return;
		}

		harnessPictureAsked = 1;
		keyDown[VK_NUMPAD7] = true;
		held = 0;
		step = 2;
		return;
	}

	if (step == 2)
	{
		// keypad 7 the same time rolls it back
		if (++held < 8)
		{
			return;
		}

		keyDown[VK_NUMPAD7] = false;

		bool const back = theCamera->roll == start.roll;

		say(std::string("keypad 7 held the same time: the roll is ") + std::to_string((long long)theCamera->roll) + (back ? " = level again" : " = NOT level again"));

		if (!back)
		{
			fail("keypad 7 did not roll it back");
			return;
		}

		step = 3;
		return;
	}

	if (step == 3)
	{
		// Insert: the zoom waits for its centre
		SendMessage(editorWindow, WM_KEYDOWN, VK_INSERT, 0);

		if (zoomStage != 1 || !statusWords || strcmp(statusWords, "Zoom... Choose centre") != 0)
		{
			fail("Insert did not ask for the centre of a zoom: " + std::string(statusWords ? statusWords : ""));
			return;
		}

		say("Insert: the status line says \"Zoom... Choose centre\"");
		start = *theCamera;
		step = 4;
		return;
	}

	if (step == 4)
	{
		// the centre: a quarter of the width to the right of the middle, on the middle line; the facing then, to compare the turn with
		if (!LevelView_CameraAxes(forwardBefore, rightBefore))
		{
			return;   // the frame that tells the axes is not there yet
		}

		pressAtX = zoomViewBox.x + zoomViewBox.width / 2 + zoomViewBox.width / 4;
		pressAtY = zoomViewBox.y + zoomViewBox.height / 2;

		if (!ZoomPress(pressAtX, pressAtY, false) || zoomStage != 2)
		{
			fail("a press did not choose the centre of the zoom");
			return;
		}

		// the rectangle for a pointer an eighth of the width further right: inteng's rule worked out here again
		int const px = pressAtX + zoomViewBox.width / 8;
		int const py = pressAtY;

		ZoomMove(px, py);

		int const width = zoomViewBox.width, height = zoomViewBox.height;
		int const cx = width / 2 + zoomCentre[0], cy = height / 2 + zoomCentre[1];
		int dx = abs((px - zoomViewBox.x) - cx), dy = abs((py - zoomViewBox.y) - cy);

		dx = dx < (dy * width) / height ? (dy * width) / height : dx;
		dx = dx > cx ? cx : dx;
		dx = dx > width - cx - 1 ? width - cx - 1 : dx;

		int half = (height * dx) / width;

		half = half > cy ? cy : half;
		half = half > height - cy - 1 ? height - cy - 1 : half;

		int const want = (half * width) / height;

		say("the centre is chosen; the pointer an eighth of the width further right makes a rectangle of half-width " + std::to_string((long long)zoomHalf) + " pixels (inteng's rule: " + std::to_string((long long)want) + ")");

		if (zoomHalf != want || zoomHalf <= 0)
		{
			fail("the rectangle is not the one inteng's rule gives");
			return;
		}

		harnessPictureAsked = 2;
		step = 5;
		return;
	}

	if (step == 5)
	{
		// a left press: the zoom in. scale = width / (2 x half); the camera turns toward the centre.
		double const factor = (double)zoomViewBox.width / (2.0 * (double)zoomHalf);
		float const expected = (float)factor > 10.0f ? 10.0f : (float)factor;
		double const focal = LevelView_FocalLength(zoomViewBox.width);
		double const turnWant = atan((double)zoomCentre[0] / focal) * 4096.0 / 6.28318530717959;
		int const turnBefore = theCamera->turn;

		ZoomPress(zoomViewBox.x + zoomViewBox.width / 2 + zoomViewBox.width / 4 + zoomViewBox.width / 8, pressAtY, false);

		int const turned = ((turnBefore - theCamera->turn) % 4096 + 4096 + 2048) % 4096 - 2048;   // how far it turned to the right

		say("a left press zooms in: the scale is " + std::to_string((long long)(zoomScale * 1000.0f + 0.5f)) + "/1000 (width / (2 x half) = " + std::to_string((long long)(expected * 1000.0f + 0.5f)) + "/1000); the camera turned " + std::to_string((long long)turned) +
			" of 4096 toward the centre (the angle whose tangent is the centre's distance over the focal length: " + std::to_string((long long)(turnWant + 0.5)) + ")");

		if (fabs((double)zoomScale - (double)expected) > 0.02 * (double)expected || abs(turned - (int)floor(turnWant + 0.5)) > 1 || zoomStage != 0)
		{
			fail("the zoom in did not scale and turn as inteng's code does");
			return;
		}

		harnessPictureAsked = 3;
		step = 6;
		return;
	}

	if (step == 6)
	{
		// the facing now goes through the pixel that was chosen: the direction forward x focal + right x distance, in the facing before
		int forwardNow[3], rightNow[3];

		if (!LevelView_CameraAxes(forwardNow, rightNow))
		{
			return;
		}

		double const focalThen = (zoomViewBox.width / 2.0) / tan(0.5 * 3.14159265358979 * (double)d3d.fieldOfView / 180.0);
		double through[3], len = 0;

		for (int k = 0; k < 3; k++)
		{
			through[k] = (double)forwardBefore[k] / 65536.0 * focalThen + (double)rightBefore[k] / 65536.0 * (double)(zoomViewBox.width / 4);
			len += through[k] * through[k];
		}

		len = sqrt(len);

		double dot = 0, now = 0;

		for (int k = 0; k < 3; k++)
		{
			dot += through[k] / len * ((double)forwardNow[k] / 65536.0);
			now += ((double)forwardNow[k] / 65536.0) * ((double)forwardNow[k] / 65536.0);
		}

		double const angle = acos(dot / sqrt(now)) * 180.0 / 3.14159265358979;

		say("after the zoom the camera faces the chosen pixel to within " + std::to_string((long long)(angle * 10.0 + 0.5)) + "/10 degrees");

		if (angle > 1.5)
		{
			fail("the camera does not face the pixel that was chosen");
			return;
		}

		// a right press: the zoom out; the centre on the middle, a rectangle an eighth of the width wide
		SendMessage(editorWindow, WM_KEYDOWN, VK_INSERT, 0);
		ZoomPress(zoomViewBox.x + zoomViewBox.width / 2, zoomViewBox.y + zoomViewBox.height / 2, true);
		ZoomMove(zoomViewBox.x + zoomViewBox.width / 2 + zoomViewBox.width / 8, zoomViewBox.y + zoomViewBox.height / 2);

		float const before = zoomScale;
		int const half = zoomHalf;
		float expected = (float)((double)before * (2.0 * half) / (double)zoomViewBox.width);

		expected = expected < 0.1f ? 0.1f : expected;
		ZoomPress(zoomViewBox.x + zoomViewBox.width / 2 + zoomViewBox.width / 8, zoomViewBox.y + zoomViewBox.height / 2, true);

		say("a right press zooms out: " + std::to_string((long long)(before * 1000.0f + 0.5f)) + "/1000 -> " + std::to_string((long long)(zoomScale * 1000.0f + 0.5f)) + "/1000 (the scale times 2 x half / width = " + std::to_string((long long)(expected * 1000.0f + 0.5f)) + "/1000)");

		if (fabs((double)zoomScale - (double)expected) > 0.02 * (double)expected + 0.001)
		{
			fail("the zoom out did not scale as inteng's code does");
			return;
		}

		harnessPictureAsked = 4;
		step = 7;
		return;
	}

	if (step == 7)
	{
		// Home: the default zoom level; Insert then Esc: nothing changes
		SendMessage(editorWindow, WM_KEYDOWN, VK_HOME, 0);

		bool const home = zoomScale == 1.0f;
		FlyCamera const before = *theCamera;

		SendMessage(editorWindow, WM_KEYDOWN, VK_INSERT, 0);
		ZoomPress(zoomViewBox.x + zoomViewBox.width / 3, zoomViewBox.y + zoomViewBox.height / 3, false);
		SendMessage(editorWindow, WM_KEYDOWN, VK_ESCAPE, 0);

		bool const cancelled = zoomStage == 0 && zoomScale == 1.0f && theCamera->turn == before.turn && theCamera->look == before.look;

		say(std::string("Home: the zoom is back to the default (") + (home ? "yes" : "NO") + "); Insert, a centre, Esc: nothing changed (" + (cancelled ? "yes" : "NO") + ")");
		say(home && cancelled ? "Movement block: WORKS in the running editor (roll, zoom in and out, Home, Esc)" : "Movement block: DOES NOT WORK in the running editor");
		step = 99;
	}
}
