// editor_prompts.cpp: THE PROMPTS AND BOXES: Delete Multiple, Assign and Destroy, Strategy's pick, stability, message and fans, the name boxes, the one-line
// dialog, Open As, the outside shapes' update and Rescale Shape. Moved here whole from editor_main.cpp on 2026-11-18 by Melanikus (Frost), so that each file
// stays within what Snyk Code reads (editor_shared.h says why); nothing in it changed. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

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
#include "write_c_file.h"
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
#include "shot_context.h"       // #78: what a screenshot says about itself
#include "png_meta.h"           // #78: ... inside its PNG (Dietrich)
#include "log_files.h"          // #78: the log's own way of leaving the player out
#include "place_at_view.h"
#include "tool_buttons.h"
#include "file_pick.h"
#include "move_rel_poly.h"
#include "move_rel_poly_edit.h"
#include "shape_edit.h"
#include "uv_edit.h"
#include "anim_edit.h"
#include "texture_keys.h"
#include "texture_shared.h"
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
#include "shape_import.h"
#include "shape_search.h"     // the refresh's order: ShapeFolder= and the game's Tools\shape_rifs, never the game's shape_rifs
#include "shape_update.h"
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
#include "open_at.h"   // -at / -shot: the editor opened at a place (Drake 11-18)
#include "orbit_command.h"   // the harness's camera-orbit / camera-corner (Drake 11-18)
#include "editor_shared.h"   // what this file shares with the files moved out of it (editor_shared.h names them)
#include "placement_lighting.h"   // Object Placement's lighting (OP-3, OP-44)
#include "state_line.h"   // the state line's fields added by their owners (StateLine_Add)
#include "placement_special_alt.h"   // Alt Locations on the current special object (OP-34..38, OP-91; V-28)
#include "placement_hierarchy_fly.h"   // F on a placed hierarchy (V-6)


// DELETE MULTIPLE STRATEGIES' WINDOW (FUN_005376a0: FUN_0057d170(600, 400, 0x2b), the title "Select Strategies To Delete" 0x5f027c; Ok (0x96, 0x172) and Cancel (0x15e, 0x172), each 0x46 x 0x10; then twenty check boxes at x 0x10 (left column) or 0x13c (right column), their words 0x10 to the right (200 x 0x12), rows y 0x28 + 0x14 x row:
// left Switches, Obsolete Switches, Doors (Non-Lift), Lighting effects, Mission Objective, Messages, Volume of Death, Self Destruct, Marine Pickups, Predator Pickups; right Generators, Aliens, Marines, Predators, Alien Queens, Xenoborgs, Predator-Aliens, Praetorians, Sentry Guns, Facehuggers).
// Ok (FUN_005380d0, the window's answer 2) deletes for every ticked box; the boxes the data side has (Switches, Lighting effects, Mission Objective, Messages, Volume of Death, Self Destruct) go through StrategyMode_DeleteKinds, and a ticked box that is not built yet (Obsolete Switches FUN_00537030, Doors (Non-Lift) FUN_00536de0, the pickups FUN_005373e0, the species FUN_005375d0: they work on the game's live objects) is NAMED on the status line, never silently skipped.
void DeleteMultipleForm()
{
	static char const *const words[20] = { "Switches", "Obsolete Switches", "Doors (Non-Lift)", "Lighting effects", "Mission Objective", "Messages", "Volume of Death", "Self Destruct", "Marine Pickups", "Predator Pickups",
		"Generators", "Aliens", "Marines", "Predators", "Alien Queens", "Xenoborgs", "Predator-Aliens", "Praetorians", "Sentry Guns", "Facehuggers" };
	host::DialogFormSpec spec;

	spec.title = "Select Strategies To Delete";
	spec.width = 600;
	spec.height = 400;

	host::DialogWidget ok, cancel;

	ok.type = host::DialogWidgetType::Button;
	ok.box = host::DialogRect{ 0x96, 0x172, 0x46, 0x10 };
	ok.text = "Ok";
	ok.answer = host::kDialogOk;
	cancel.type = host::DialogWidgetType::Button;
	cancel.box = host::DialogRect{ 0x15e, 0x172, 0x46, 0x10 };
	cancel.text = "Cancel";
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);

	for (int i = 0; i < 20; i++)
	{
		int32_t const x = i < 10 ? 0x10 : 0x13c, y = 0x28 + 0x14 * (i % 10);
		host::DialogWidget box, label;

		box.type = host::DialogWidgetType::Check;
		box.box = host::DialogRect{ x, y, 12, 12 };
		box.group = -1;
		label.type = host::DialogWidgetType::Label;
		label.box = host::DialogRect{ x + 0x10, y, 200, 0x12 };
		label.text = words[i];
		spec.widgets.push_back(box);
		spec.widgets.push_back(label);
	}

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			return;
		}

		host::StrategyGroup const *groups = nullptr;
		size_t const groupCount = host::StrategyMode_Groups(&groups);
		std::string typed, notBuilt, liveWords;
		int box = 0;

		for (host::DialogWidget const &w : widgets)
		{
			if (w.type != host::DialogWidgetType::Check)
			{
				continue;
			}

			if (w.checked)
			{
				bool built = false;

				for (size_t g = 0; g < groupCount && !built; g++)
				{
					if (strcmp(groups[g].name, words[box]) == 0)
					{
						typed += std::to_string((unsigned long long)(g + 1)) + " ";
						built = true;
					}
				}

				bool const live = box == 1 || box == 2 || box == 8 || box == 9 || box >= 10;

				if (!built && !live)
				{
					notBuilt += (notBuilt.empty() ? "" : ", ") + std::string(words[box]);
				}
			}

			box++;
		}

		// S-87 (strategy-data.md, Vasquez 11-17): FUN_005380d0's order is the typed kinds first (0x53812e-0x5381da: Switches 52 / 61 / 66 / 68, Doors FUN_00536de0, Lighting effects, Mission
		// Objective, Messages, Volume of Death, Self Destruct), then the pickups (FUN_005373e0, 0x5381fc), the species (FUN_005375d0, 0x538699), OBSOLETE SWITCHES LAST (FUN_00537030, 0x53874f),
		// then always the repair pass (FUN_00536ad0, 0x538764) and the recolouring (FUN_00522650, 0x538769). (Ours ran Obsolete Switches first and no repair after: a virtual switch whose
		// only target went in the same Ok survived.)
		if (!typed.empty())
		{
			RunDeleteMulti(typed);
		}

		// the groups that work on the live objects (FUN_00536de0 doors, FUN_005373e0 pickups, FUN_005375d0 the species), in the order FUN_005380d0 runs them; boxes are numbered as the form's list
		std::string live;
		{
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			std::vector<bool> on;

			for (host::DialogWidget const &w : widgets)
			{
				if (w.type == host::DialogWidgetType::Check)
				{
					on.push_back(w.checked);
				}
			}

			size_t done = 0, total = 0;
			auto const count = [&](host::StrategyModeResult result, char const *what)
			{
				live += (live.empty() ? "" : ", ") + std::string(what) + (result == host::StrategyModeResult::Done ? "" : std::string(" (") + host::StrategyModeWords(result) + ")");
			};

			if (open && on.size() == 20)
			{
				if (on[2])
				{
					done = 0;
					count(host::StrategyMode_DeleteDoorsNonLift(open, &done), "Doors (Non-Lift)");
					total += done;
				}

				if (on[8] || on[9])
				{
					done = 0;
					count(host::StrategyMode_DeletePickups(open, on[8], on[9], &done), on[8] && on[9] ? "Pickups" : (on[8] ? "Marine Pickups" : "Predator Pickups"));
					total += done;
				}

				// Generators 0, Aliens 1, Marines 7, Predators 2, Alien Queens 6, Xenoborgs 4, Predator-Aliens 5, Praetorians 8, Sentry Guns 9, Facehuggers 3 (the form's right column, FUN_005380d0's list)
				static int const species[10] = { 0, 1, 7, 2, 6, 4, 5, 8, 9, 3 };
				uint32_t mask = 0;

				for (int i = 0; i < 10; i++)
				{
					mask |= on[(size_t)(10 + i)] ? (1u << species[i]) : 0u;
				}

				if (mask != 0)
				{
					done = 0;
					count(host::StrategyMode_DeleteSpecies(open, mask, &done), "species groups");
					total += done;
				}

				// Obsolete Switches (FUN_00537030, the second box, LAST, 0x53874f): the repair pass, then every virtual switch whose network of targets is all virtual switches
				if (on[1])
				{
					bool refused = false;

					done = host::StrategyRepair_DeleteObsoleteSwitches(open, &refused);
					live += (live.empty() ? "" : ", ") + std::string("Obsolete Switches") + (refused ? " (a destroy was refused)" : "");
					total += done;
				}

				// ... and always the repair pass (FUN_00536ad0, 0x538764): the targets the deletes left dead are reset
				if (host::StrategyRepair_Run(open) > 0 && live.empty())
				{
					live = "the repair pass";
				}

				if (!live.empty())
				{
					LoadTheObjectListAgain();
					liveWords = "Delete Multiple Strategies: " + std::to_string((unsigned long long)total) + " more deleted (" + live + ")";
				}
			}
		}

		if (!liveWords.empty())
		{
			strategyWords = typed.empty() ? liveWords : strategyWords + " | " + liveWords;
			statusWords = strategyWords.c_str();
		}

		if (!notBuilt.empty())
		{
			strategyWords = std::string((typed.empty() && liveWords.empty()) ? "" : strategyWords + " | ") + "Delete Multiple Strategies: not built yet: " + notBuilt + " (FUN_00537030 is read but its rule is not settled)";
			statusWords = strategyWords.c_str();
		}
		else if (typed.empty() && liveWords.empty())
		{
			statusWords = "Delete Multiple Strategies: nothing chosen";
		}
	});
}

// The numbers typed in Delete Multiple Strategies: the groups they name are deleted in one change
void RunDeleteMulti(std::string const &typed)
{
	host::StrategyGroup const *groups = nullptr;
	size_t const groupCount = host::StrategyMode_Groups(&groups);
	std::vector<int32_t> kinds;
	std::string named;
	size_t at = 0;

	while (at < typed.size())
	{
		if (typed[at] == ' ' || typed[at] == ',')
		{
			at++;
			continue;
		}

		char *end = nullptr;
		long const number = strtol(typed.c_str() + at, &end, 10);

		if (end == typed.c_str() + at || number < 1 || (size_t)number > groupCount)
		{
			statusWords = "Delete Multiple Strategies: type group numbers from the list, like 1 3";
			return;
		}

		host::StrategyGroup const &group = groups[(size_t)number - 1];

		for (size_t i = 0; i < group.count; i++)
		{
			kinds.push_back(group.kinds[i]);
		}

		named += (named.empty() ? "" : ", ") + std::string(group.name);
		at = (size_t)(end - typed.c_str());
	}

	if (kinds.empty())
	{
		statusWords = "Delete Multiple Strategies: nothing chosen";
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t deleted = 0;
	host::StrategyModeResult const result = open ? host::StrategyMode_DeleteKinds(open, kinds.data(), kinds.size(), &deleted) : host::StrategyModeResult::BadCall;

	if (result != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Delete Multiple Strategies: ") + host::StrategyModeWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	LoadTheObjectListAgain();
	strategyWords = "Delete Multiple Strategies (" + named + "): " + std::to_string((unsigned long long)deleted) + " strategy(ies) deleted (kept in the editor's bytes until Save a copy)";
	statusWords = strategyWords.c_str();
}

// WHICH STRATEGIES AN OBJECT CAN TAKE (Assign Strategy's chooser FUN_0052a160, in its order, each row shown only when the object's class says it can take the kind: method +8,
// vtable 0x5af9a0 at 0x538a80, read from the exe's bytes; this exe's game number makes the placed-object branches the ones below):
//   a MODULE (the object's header has no placed bit): Lift 51, Door 50, Airlock 57, Switch Door 59, Lighting Effects 64;
//   a PLACED object: Platform Lift 54, Multi Switch 66, Linked Switch 61, Track Object 72, Track Object (Destructable) 80, Fan 75, Placed Object 62.
// (Binary Switch 52, Simple Object 53 and the Console 63 are accepted by the class but have no row in the chooser; the virtual object's class takes 52, 61, 66, 68, 70, 73, 77, 78.)
// Gives how many kinds, into kinds (most 16), for the object with that ID; 0 when the object's header can't be read.
size_t KindsTheObjectCanTake(uint8_t const *id, int32_t *kinds)
{
	// an ENEMY (AVPGENER with a species, type != 0: wrapper vtable 0x5afa00, +8 at 0x53a2b0) takes Enemy 69 only; a GENERATOR (type 0: 0x5afa20, +8 at 0x53aa70) Generator 79 only (S-KC-6)
	{
		host::GeneratorRef generator;

		if (host::FindGenerator(LevelObjects_Level(), id, &generator))
		{
			kinds[0] = generator.type != 0 ? 69 : 79;
			return 1;
		}
	}

	// a PLACED HIERARCHY (PLACHIER: wrapper vtable 0x5afae0, +8 at 0x53b780) takes Hierarchy Object 76 only (S-KC-6, S-K76-W)
	{
		host::PlacedHierarchyRef placedHierarchy;

		if (host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy))
		{
			kinds[0] = 76;
			return 1;
		}
	}

	// a VIRTUAL object (VIOBJECT): class vtable 0x5af9e0, +8 at 0x539cf0 takes Binary 52, Linked 61, Multi 66, Area 68, Mission Objective 70, Message 73, Volume of Death 77 and Self Destruct 78;
	// the chooser has rows for these (no Binary Switch row), in its order
	{
		std::vector<host::VirtualObjectRef> virtuals;

		host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

		for (host::VirtualObjectRef const &one : virtuals)
		{
			if (memcmp(one.id, id, host::kSelectionIdBytes) == 0)
			{
				static int32_t const virtualKinds[] = { 66, 61, 68, 70, 73, 77, 78 };

				for (size_t i = 0; i < sizeof(virtualKinds) / sizeof(virtualKinds[0]); i++)
				{
					kinds[i] = virtualKinds[i];
				}

				return sizeof(virtualKinds) / sizeof(virtualKinds[0]);
			}
		}
	}

	host::ObjectHeader header;

	if (!LevelObjects_FindHeader(id, &header))
	{
		return 0;
	}

	static int32_t const moduleKinds[] = { 51, 50, 57, 59, 64 };
	static int32_t const placedKinds[] = { 54, 66, 61, 72, 80, 75, 62 };
	bool const placed = (header.flags & 0x400) != 0;
	int32_t const *const list = placed ? placedKinds : moduleKinds;
	size_t const count = placed ? sizeof(placedKinds) / sizeof(placedKinds[0]) : sizeof(moduleKinds) / sizeof(moduleKinds[0]);

	for (size_t i = 0; i < count; i++)
	{
		kinds[i] = list[i];
	}

	return count;
}

// The two questions of Assign Strategy: the kind (a number of the list), then, when the object has a strategy, yes or no to replacing it
void RunAssignPrompt(int32_t kind, std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		statusWords = "Assign Strategy: no level is held to work on";
		return;
	}

	if (kind == kPromptAssignKind)
	{
		char *end = nullptr;

		errno = 0;

		long long const number = _strtoi64(typed.c_str(), &end, 10);
		model::StrategyKind const *const found = errno == 0 && end != typed.c_str() && *end == '\0' && number > 0 && number < 100000 ? model::FindStrategyKind((int32_t)number) : nullptr;

		int32_t canTake[16];
		size_t const canCount = KindsTheObjectCanTake(assignId, canTake);
		bool listed = false;

		for (size_t k = 0; k < canCount; k++)
		{
			listed = listed || (found && canTake[k] == found->number);
		}

		if (!found || !found->offered || !listed)
		{
			StrategyMessage("Object cannot use that strategy");   // inteng's words (0x5ee294), drawn at (200, 100) for 700 ms (FUN_0057bd40, 0x528920)
			statusWords = "Object cannot use that strategy";
			return;
		}

		assignKind = found->number;   // the replace question was asked before the window (command 0x8 above)
	}
	else
	{
		std::string lower;

		for (char const letter : typed)
		{
			lower += (char)tolower((unsigned char)letter);
		}

		if (lower != "y" && lower != "yes")
		{
			statusWords = "Assign Strategy: nothing changed";
			return;
		}
	}

	bool isVirtual = false;
	{
		std::vector<host::VirtualObjectRef> virtuals;

		host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

		for (host::VirtualObjectRef const &one : virtuals)
		{
			isVirtual = isVirtual || memcmp(one.id, assignId, host::kSelectionIdBytes) == 0;
		}
	}

	host::ObjectEditResult result = host::ObjectEditResult::Done;

	host::GeneratorRef generator;

	if (isVirtual)
	{
		host::VirtualObjectResult const made = host::VirtualObject_AssignStrategy(open, assignId, assignKind);

		result = made == host::VirtualObjectResult::Done ? host::ObjectEditResult::Done : host::ObjectEditResult::BadStrategy;
	}
	else if (host::FindGenerator(LevelObjects_Level(), assignId, &generator))
	{
		// the enemy wrapper's +0xc (FUN_0053a2c0) / the generator's (FUN_0053aa80): the old strategy destroyed, or a holder made, then the new one (S-K69-0, S-K79-0)
		host::GeneratorStrategyResult const made = host::GeneratorStrategy_Assign(open, assignId, assignKind);

		result = made == host::GeneratorStrategyResult::Done ? host::ObjectEditResult::Done : made == host::GeneratorStrategyResult::LoaderRefused ? host::ObjectEditResult::LoaderRefused : host::ObjectEditResult::BadStrategy;
	}
	else if (host::PlacedHierarchyRef placedHierarchy; host::FindPlacedHierarchy(LevelObjects_Level(), assignId, &placedHierarchy))
	{
		// the placed hierarchy's wrapper +0xc (FUN_0053b790): the old strategy destroyed, or a holder made (FUN_0051e130 on the PLACHIER), then the new one (S-K76-0)
		host::PlacedHierarchyStrategyResult const made = host::PlacedHierarchyStrategy_Assign(open, assignId, assignKind);

		result = made == host::PlacedHierarchyStrategyResult::Done ? host::ObjectEditResult::Done :
		         made == host::PlacedHierarchyStrategyResult::LoaderRefused ? host::ObjectEditResult::LoaderRefused : host::ObjectEditResult::BadStrategy;
	}
	else
	{
		result = host::AssignStrategy(open, assignId, assignKind);
	}

	if (result != host::ObjectEditResult::Done)
	{
		StrategyMessage("Object cannot use that strategy");   // inteng's words (0x5ee294), on screen for 700 ms
		strategyWords = std::string("Object cannot use that strategy: ") + host::ObjectEditWords(result);   // then why, on the status line the harness reads
		statusWords = strategyWords.c_str();
		return;
	}

	LoadTheObjectListAgain();
	ShowTheSelected(host::Selection_Get());

	// FUN_00528830's last call: the new strategy's view at once (method +0x14(1), 0x529141), the current object's red off while it runs; a kind whose view is not built yet: the field panel (NAMED stand-in)
	bool const viewOpen = StrategyViewOpen();

	strategyWords = viewOpen ? "The object has a new strategy: its Edit view (Escape or the right button ends it)" : "The object has a new strategy (kept in the editor's bytes until Save a copy)";   // ours: inteng shows the view and says nothing
	statusWords = strategyWords.c_str();
}

// The two answers of Destroy Virtual Object: which one (a number of the list), then yes or no to "Delete this object?" (only yes cuts it)
void RunDestroyPrompt(int32_t kind, std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		statusWords = "Destroy Virtual Object: no level is held to work on";
		return;
	}

	if (kind == kPromptDestroyPick)
	{
		char *end = nullptr;

		errno = 0;

		long long const row = _strtoi64(typed.c_str(), &end, 10);

		if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0 || row < 1 || (unsigned long long)row > destroyChoices.size())
		{
			statusWords = "Destroy Virtual Object: type the number of a virtual object in the list";
			return;
		}

		memcpy(destroyId, destroyChoices[(size_t)row - 1].id, sizeof(destroyId));
		ConfirmAs(kPromptDestroyAsk, "Delete this object?");   // 0x5eddec, FUN_00474a40
		return;
	}

	std::string lower;

	for (char const letter : typed)
	{
		lower += (char)tolower((unsigned char)letter);
	}

	if (lower != "y" && lower != "yes")
	{
		statusWords = "Destroy Virtual Object: nothing changed";
		return;
	}

	host::VirtualObjectResult const result = host::VirtualObject_Destroy(open, destroyId);

	if (result != host::VirtualObjectResult::Done)
	{
		strategyWords = std::string("Destroy Virtual Object: ") + host::VirtualObjectWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	host::Selection_Clear();   // the selection is cleared (FUN_00539e00)
	LoadTheObjectListAgain();
	strategyWords = "The virtual object is destroyed (kept in the editor's bytes until Save a copy)";
	statusWords = strategyWords.c_str();
}

// The click that T (change target) waits for (see StrategyModeCommand 0x100)
static std::string strategyPickWords;

void StrategyPickClicked(host::PickReport const &report)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t selected[8];
	host::ObjectStrategy now;

	if (!open || !SelectedObjectId(selected) || host::FindObjectStrategy(LevelObjects_Level(), selected, &now) != host::StrategyFound::Found || now.payload.size() < 12)
	{
		strategyPick = 0;
		statusWords = "The pick is let go of: the selected object has no strategy any more";
		return;
	}

	int32_t const kind = *(int32_t const *)(now.payload.data() + 8);
	model::StrategyKind const *const model = model::FindStrategyKind(kind);
	host::LevelStore store(open);
	char const *why = nullptr;

	if (strategyPick == 1)
	{
		// the object clicked: an Object by its ID, a module by its name (the object that has that name)
		uint8_t target[8];
		bool have = false;

		if (report.found == host::PickFound::Object)
		{
			memcpy(target, report.objectId, 8);
			have = true;
		}
		else if (report.found == host::PickFound::Module)
		{
			std::vector<host::LevelObject> rows;

			LevelObjects_Read(&rows);

			// the module's object: the one whose number the game gave this module (the same map the module tints use)
			std::map<int32_t, int32_t> numbers;

			host::GameModuleNumbers(LevelObjects_Level(), &numbers);

			for (host::LevelObject const &row : rows)
			{
				int32_t index = 0;

				if (!host::ModuleIndexOfId(LevelObjects_Level(), row.id, &index))
				{
					continue;
				}

				auto const found = numbers.find(index);

				if (found != numbers.end() && found->second == report.moduleIndex)
				{
					memcpy(target, row.id, 8);
					have = true;
					break;
				}
			}
		}

		if (!have)
		{
			strategyPickWords = "Change Target: that click found no object (click an object or a module; the game found kind " + std::to_string((int)report.found) + ")";
			statusWords = strategyPickWords.c_str();
			return;
		}

		// the first object field of the kind that sits at its start (a list's targets are not this)
		model::Field const *field = nullptr;

		for (size_t i = 0; model && i < model->fieldCount && !field; i++)
		{
			if (model->fields[i].storage == model::Storage::ObjectId && model->fields[i].anchor == model::Anchor::Start)
			{
				field = &model->fields[i];
			}
		}

		if (!field)
		{
			strategyPick = 0;
			statusWords = "This kind of strategy has no target to change";
			return;
		}

		host::FieldEdit e;

		e.field = field->name;
		e.what = host::EditWhat::Object;
		memcpy(&e.first, target, 4);
		memcpy(&e.second, target + 4, 4);

		host::StrategyFound lookup;

		if (host::ApplyStrategyEdit(LevelObjects_Level(), selected, kind, e, &store, &why, &lookup) != host::EditAnswer::Applied)
		{
			strategyPickWords = std::string("Change Target: ") + (why ? why : "the strategy would not take it");
			statusWords = strategyPickWords.c_str();
			strategyPick = 0;
			return;
		}

		strategyPick = 0;
		ShowTheFieldsAgain();
		strategyPickWords = std::string("The target of ") + field->name + " is the object you clicked (kept in the editor's bytes until Save a copy)";
		statusWords = strategyPickWords.c_str();
		return;
	}
}

// Enter on Stability flags' six letters (FUN_00527210: the faces are written into the shape header's flag word when the answer to "Would you like to keep the changes?" is yes;
// typing the six letters is the answer here): r = red = stable (the bit is set), g = green = unstable (cleared)
void RunStrategyStability(std::string const &typed)
{
	std::string letters;

	for (char const letter : typed)
	{
		if (letter != ' ')
		{
			letters += (char)tolower((unsigned char)letter);
		}
	}

	if (letters.size() != 6 || letters.find_first_not_of("rg") != std::string::npos)
	{
		statusWords = "Stability flags: type six letters, r (red: stable) or g (green: unstable), one for each face";
		return;
	}

	uint32_t faces = 0;

	for (int face = 0; face < 6; face++)
	{
		faces |= letters[(size_t)face] == 'r' ? (1u << face) : 0u;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::StrategyModeResult const result = open ? host::StrategyMode_SetStability(open, stabilityId, faces) : host::StrategyModeResult::BadCall;

	if (result != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Stability flags: ") + host::StrategyModeWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	LoadTheObjectListAgain();
	strategyWords = "Stability flags: " + letters + " written into the module's shape (kept in the editor's bytes until Save a copy)";
	statusWords = strategyWords.c_str();
}

// Enter on Message's number (FUN_00525700 0x525b74-0x525c10): a whole number 0 to 127 clears the message bits (0xfe00000) of every picked module and sets number << 21;
// a number outside 0..127 gives inteng's "Number not between 0 and 127" and changes nothing
void RunStrategyMessage(std::string const &typed)
{
	char *end = nullptr;

	errno = 0;

	long long const number = _strtoi64(typed.c_str(), &end, 10);

	if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0 || number < 0 || number > host::kStrategyMostMessage)
	{
		statusWords = "Number not between 0 and 127";   // inteng's own words (0x5edf74)
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<uint8_t> const ids = PickedModuleIdBytes();

	if (!open || ids.empty())
	{
		statusWords = "Message: the picked modules are gone";
		return;
	}

	size_t const count = ids.size() / host::kSelectionIdBytes;
	size_t cleared = 0;
	size_t set = 0;
	host::StrategyModeResult result = host::StrategyMode_SetModuleFlag(open, ids.data(), count, host::kStrategyMessageBits, false, &cleared);

	if (result == host::StrategyModeResult::Done)
	{
		result = host::StrategyMode_SetModuleFlag(open, ids.data(), count, (uint32_t)number << 21, true, &set);
	}

	if (result != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Message: ") + host::StrategyModeWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	LoadTheObjectListAgain();
	strategyWords = "Message " + std::to_string(number) + ": " + std::to_string((unsigned long long)count) + " picked module(s) (kept in the editor's bytes until Save a copy)";
	statusWords = strategyWords.c_str();
}

// Enter on the question of Reset fan wind speed: only "y" or "yes" resets (FUN_005354d0: the answer 3)
void RunStrategyFans(std::string const &typed)
{
	std::string lower;

	for (char const letter : typed)
	{
		lower += (char)tolower((unsigned char)letter);
	}

	if (lower != "y" && lower != "yes")
	{
		statusWords = "Reset fan wind speed: nothing changed";
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t fans = 0;
	// S-13: FUN_005354d0 writes no DAT_00914950, so a reset alone does not make Exit ask: when nothing of Strategy's had changed, the clean mark moves with it (W-3)
	bool const wasClean = open && !StrategyChanged();
	host::StrategyModeResult const result = open ? host::StrategyMode_ResetFans(open, &fans) : host::StrategyModeResult::BadCall;

	if (result == host::StrategyModeResult::Done && wasClean)
	{
		strategyCleanGeneration = open->Generation();
	}

	if (result == host::StrategyModeResult::NoStrategy)
	{
		statusWords = "Reset fan wind speed: the level has no fan";
		return;
	}

	if (result != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Reset fan wind speed: ") + host::StrategyModeWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	strategyWords = "The wind strength of " + std::to_string((unsigned long long)fans) + " fan(s) is reset to 100 (kept in the editor's bytes until Save a copy)";
	statusWords = strategyWords.c_str();
}

// The questions inteng asks in a WINDOW (FUN_0057c800) are shown as that window (dialog_box.h) and the answer goes the way the typed one did:
// - Exit + All Adjacencies (0x4c1eb7): the confirm box "Do you want to regenerate adjacent module data?\nThis will mess up any one-way adjacencies that have been set up." (0x5e3a28); only Yes (3) goes on
// - Set Visibility (0xa4, FUN_004c3620): the text box "Visible distance in metres :" with the distance now (DAT_00704378 / 100000) in its field, Ok or Cancel (FUN_00474340, an input box of the FUN_00474350 family)
// - Edge > Soft (FUN_004b0270): the text box "Type in the maximum angle for soft edges (flat is 0)." with Ok and Cancel (the field's first contents are not known: empty)
// THE NAME OBJECT DIALOG (reassess/object-placement.md OP-24; Bishop checked the row 11-16): FUN_004d53d0 (0x4d53d0-0x4d560f) runs it for Create > AVP generator / Bad Guy / Sound / Particle Generator, and
// FUN_004d68e0 0x4d9460-0x4d97be / FUN_004d9af0 0x4db0e1-0x4dbe0c run the same loop for Create > With shape (Choose, and P on a placed shape). The window is FUN_004d4c70: 400 x 200, titled "Name Object" (0x5e7e68),
// the words "Type in the name of the object." (0x5e7e74) at (18, 18, 370 x 40), an edit at (30, 100, 300 x 35) of up to 200 letters holding the name passed in, "OK" (0x5cfabc) at (100, 150, 50 x 16) and "Cancel" (0x5cfab4)
// at (200, 150, 50 x 16); Return is OK (the window's default 2); OK answers 2, Cancel and Escape 1 (method 0x447b90). Not OK: no object. OK: the typed name is compared, byte for byte (FUN_0053ec30; the RBOBJECT names
// with a byte loop at 0x4d54d8-0x4d5501), with the name of every object of the placement list and of every RBOBJECT (here: every object chunk of the level and every special object); used -> the window
// FUN_004d4e00: 400 x 150, titled "Name Object", "The name of the object has already been used." (0x5e7ea0) at (18, 70, 370 x 40), "Re-enter" (0x5e7e94) at (75, 120, 75 x 16) and "Cancel" at (200, 120, 75 x 16);
// Return is Re-enter; Re-enter (2) shows the name window again holding the typed name, Cancel (1) makes no object. A free name goes on to the create. (The earlier box here had no OK button: a misread.)
static bool NameUsedByAnObject(std::string const &name)
{
	if (host::ObjectNameInUse(LevelObjects_Level(), name.c_str()))
	{
		return true;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<host::SpecialObject> specials;

	if (open && host::SpecialObjects_Read(open, &specials) == host::SpecialResult::Done)
	{
		for (host::SpecialObject const &one : specials)
		{
			if (one.name == name)
			{
				return true;
			}
		}
	}

	return false;
}

void NameObjectBox(std::string const &start, std::function<void(std::string const &)> const &named)
{
	host::DialogFormSpec spec;
	host::DialogWidget words, edit, ok, cancel;

	spec.title = "Name Object";
	spec.width = 400;
	spec.height = 200;
	words.type = host::DialogWidgetType::Label;
	words.text = "Type in the name of the object.";
	words.box = { 0x12, 0x12, 0x172, 0x28 };
	edit.type = host::DialogWidgetType::Edit;
	edit.box = { 0x1e, 100, 300, 0x23 };
	edit.most = 200;
	edit.text = start.size() > edit.most ? start.substr(0, edit.most) : start;
	ok.type = host::DialogWidgetType::Button;
	ok.text = "OK";
	ok.box = { 100, 0x96, 0x32, 0x10 };
	ok.answer = host::kDialogOk;
	cancel.type = host::DialogWidgetType::Button;
	cancel.text = "Cancel";
	cancel.box = { 200, 0x96, 0x32, 0x10 };
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(words);
	spec.widgets.push_back(edit);
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);

	host::Dialogs_Form(spec, [named](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			return;   // Cancel or Escape: no object
		}

		for (host::DialogWidget const &one : widgets)
		{
			if (one.type == host::DialogWidgetType::Edit)
			{
				if (NameUsedByAnObject(one.text))
				{
					NameInUseBox(one.text, named);
				}
				else if (named)
				{
					named(one.text);
				}

				return;
			}
		}
	});
}

void NameInUseBox(std::string const &typed, std::function<void(std::string const &)> const &named)
{
	host::DialogFormSpec spec;
	host::DialogWidget words, again, cancel;

	spec.title = "Name Object";
	spec.width = 400;
	spec.height = 0x96;
	words.type = host::DialogWidgetType::Label;
	words.text = "The name of the object has already been used.";
	words.box = { 0x12, 0x46, 0x172, 0x28 };
	again.type = host::DialogWidgetType::Button;
	again.text = "Re-enter";
	again.box = { 0x4b, 0x78, 0x4b, 0x10 };
	again.answer = host::kDialogOk;
	cancel.type = host::DialogWidgetType::Button;
	cancel.text = "Cancel";
	cancel.box = { 200, 0x78, 0x4b, 0x10 };
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(words);
	spec.widgets.push_back(again);
	spec.widgets.push_back(cancel);

	host::Dialogs_Form(spec, [typed, named](int32_t answer, std::vector<host::DialogWidget> const &)
	{
		if (answer == host::kDialogOk)
		{
			NameObjectBox(typed, named);
		}
	});
}

// inteng's one-line text window (FUN_00447860 and FUN_00474170 / FUN_004747f0, the same layout: 400 x 200, a label at (0x12, 0x12, 0x172 x 0x28), the edit at (0x1e, 100, 300 x editHeight) of up to `most` letters, Ok at (100, 0x96) and
// Cancel at (200, 0x96), 0x32 x 0x10 each; Return and Ok answer 3, Escape and Cancel 1). `ok` gets the text when the answer is Ok.
void DialogLine(std::string const &title, std::string const &label, std::string const &initial, size_t most, int32_t editHeight, std::function<void(std::string const &)> const &ok)
{
	host::DialogFormSpec spec;
	host::DialogWidget one;

	spec.title = title;
	spec.width = 400;
	spec.height = 200;

	one.type = host::DialogWidgetType::Label;
	one.box = { 0x12, 0x12, 0x172, 0x28 };
	one.text = label;
	spec.widgets.push_back(one);

	host::DialogWidget edit;

	edit.type = host::DialogWidgetType::Edit;
	edit.box = { 0x1e, 100, 300, editHeight };
	edit.text = initial;
	edit.most = most;
	spec.widgets.push_back(edit);

	host::DialogWidget okButton;

	okButton.type = host::DialogWidgetType::Button;
	okButton.box = { 100, 0x96, 0x32, 0x10 };
	okButton.text = "Ok";
	okButton.answer = host::kDialogOk;
	spec.widgets.push_back(okButton);

	host::DialogWidget cancelButton;

	cancelButton.type = host::DialogWidgetType::Button;
	cancelButton.box = { 200, 0x96, 0x32, 0x10 };
	cancelButton.text = "Cancel";
	cancelButton.answer = host::kDialogEscape;
	spec.widgets.push_back(cancelButton);

	host::Dialogs_Form(spec, [ok](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			return;
		}

		for (host::DialogWidget const &w : widgets)
		{
			if (w.type == host::DialogWidgetType::Edit)
			{
				ok(w.text);
				return;
			}
		}
	});
}

std::string HexOf(const uint8_t *id);

static bool OpenAsDialog(int32_t kind)
{
	if (kind == kPromptRegenAdjacency)
	{
		DialogConfirm("Do you want to regenerate adjacent module data?\nThis will mess up any one-way adjacencies that have been set up.", [](bool yes)
		{
			if (yes)
			{
				promptFor = kPromptRegenAdjacency;
				promptText = "yes";
				RunThePrompt();
			}
		});
		return true;
	}

	// AutoLink > Delete Links > Environment (0xa8, FUN_004c1a90 0x4c35bf): the confirm box "Delete all module linking for this level?" (FUN_00474a40, FUN_0057c800); only Yes (3) calls FUN_004c8af0(0)
	// Create > With shape > Choose, the name (FUN_004d4c70, 0x4d9450): see NameObjectBox
	if (kind == kPromptCreateName)
	{
		NameObjectBox("", CreateNamedAtTheView);
		return true;
	}

	if (kind == kPromptDeleteAllLinks)
	{
		DialogConfirm("Delete all module linking for this level?", [](bool yes)
		{
			if (yes)
			{
				promptFor = kPromptDeleteAllLinks;
				promptText = "yes";
				RunThePrompt();
			}
		});
		return true;
	}

	if (kind == kPromptVisibility || kind == kPromptSoftEdge)
	{
		host::DialogField field;

		field.text = kind == kPromptVisibility ? std::to_string(visibleDistance / 100000) : std::string();

		DialogText(kind == kPromptVisibility ? "Visible distance in metres :" : "Type in the maximum angle for soft edges (flat is 0).", std::vector<host::DialogField>{ field }, [kind](int32_t answer, std::vector<std::string> const &texts)
		{
			if (answer == host::kDialogOk && !texts.empty())
			{
				promptFor = kind;
				promptText = texts[0];
				RunThePrompt();
			}
		});
		return true;
	}

	// Write Asc File x2 (FUN_00447860: title "Write asc file" / "Write asc file (doors closed)", label "Type filename (without .asc extension)", an edit of 200 letters; Ok writes it, nothing typed writes nothing, 0x445476-0x4454a2)
	// THE START TEXT (TB-C3, construction-closeout-frost.md): the edit starts with the level's RIFFNAME name (0x44545f: REBENVDT > RIFFNAME +0x28 into the edit's +0x24), and for Doors closed
	// with "%sb" of it (0x4457c2, the format at 0x5cf99c): the name as it is, kept to the edit's 200 letters
	if (kind == kPromptAsc || kind == kPromptAscDoors)
	{
		std::string start;

		if (host::LevelRiffName(LevelObjects_Level(), 200, &start) && kind == kPromptAscDoors)
		{
			start = (start + "b").substr(0, 200);
		}

		DialogLine(kind == kPromptAsc ? "Write asc file" : "Write asc file (doors closed)", "Type filename (without .asc extension)", start, 200, 0x23, [kind](std::string const &typed)
		{
			if (!typed.empty())
			{
				promptFor = kind;
				promptText = typed;
				RunThePrompt();
			}
		});
		return true;
	}

	// Write C File (0x445867-0x4458eb, write_c_file.h): only with exactly one object selected, else nothing happens; FUN_00447bc0's "Write .c file", "Type filename for shape
	// (without .c extension)", an edit of 200 letters (0x447c61: 300 x 40), nothing in it at the start; Ok with a name writes the file
	if (kind == kPromptWriteC)
	{
		uint8_t id[8];

		if (WriteC_TheObject(id))
		{
			DialogLine("Write .c file", "Type filename for shape (without .c extension)", std::string(), 200, 0x28, [](std::string const &typed)
			{
				if (!typed.empty())
				{
					promptFor = kPromptWriteC;
					promptText = typed;
					RunThePrompt();
				}
			});
		}

		return true;
	}

	// Fix tears (FUN_00474170 at 0x445d44 / 0x445dcf): the words with a line between, the last tolerance as the start text (DAT_005ced28, 5), an edit of 10 letters
	if (kind == kPromptTears)
	{
		DialogLine(std::string(), tearAll ? "Fixing tears for ALL objects\nType in tolerance level for tear fixing (in mm)" : "Fixing tears for selected objects\nType in tolerance level for tear fixing (in mm)", std::to_string((int)tearTolerance), 10, 0x12, [](std::string const &typed)
		{
			promptFor = kPromptTears;
			promptText = typed;
			RunThePrompt();
		});
		return true;
	}

	// Find Small Modules (FUN_004747f0 at 0x445ef6): "Search for modules with x or z extents smaller than what?(in m)", the limit in metres as the start text (printed with a fraction, the zeros at its end taken off: 0x4749e5), an edit of 200 letters
	if (kind == kPromptSmallLimit)
	{
		char shown[48];

		_snprintf_s(shown, sizeof(shown), _TRUNCATE, "%f", host::SmallModules_MetresOfLimit(smallLimit));

		if (strchr(shown, '.'))
		{
			size_t length = strlen(shown);

			while (length > 0 && shown[length - 1] == '0')
			{
				shown[--length] = 0;
			}

			if (length > 0 && shown[length - 1] == '.')
			{
				shown[--length] = 0;
			}
		}

		DialogLine(std::string(), "Search for modules with x or z extents smaller than what?(in m)", shown, 200, 0x12, [](std::string const &typed)
		{
			promptFor = kPromptSmallLimit;
			promptText = typed;
			RunThePrompt();
		});
		return true;
	}

	// Change Object Type: the question about an object that has module linking data (FUN_00448530: the confirm box (FUN_00474a40) "%s has module linking data. Do you want to delete it?"; Yes deletes it)
	if (kind == kPromptTypeLinks)
	{
		host::FoundObject asked;
		bool const found = typeAsking < typeCalls.size() && host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(typeCalls[typeAsking].id).c_str(), &asked);

		DialogConfirm((found ? asked.name : std::string("The object")) + " has module linking data. Do you want to delete it?", [](bool yes)
		{
			RunTypeAnswer(yes ? std::string("yes") : std::string());
		});
		return true;
	}

	// Find Object (0x52, 0x446636-0x44696e): the list dialog "Select Object" of every object of the level by name (FUN_00475bf0, rows sorted by the dialog); the one chosen is selected and the view goes to its place
	// STRATEGY DATA's Find Object (strategy-data.md S-11; reassess/strategy-data-verify-frost.md S-11): FUN_00524e90 case 0x41 -> FUN_005253c0(1) (dump-full-3:58960) makes the list AGAIN each
	// time (DAT_0091495c is only ever set to 0) of every record whose wrapper is a virtual object (+8 != 0, 0x525496) or holds a strategy (+0x14 +0x30, 0x52549b), EACH RECORD ONCE, by its
	// name (wrapper +4(-1)); FUN_00475bf0 "Select Object" sorts the rows. Our object list holds the virtual objects too, so they are taken from it once, as virtual objects. A choice:
	// FUN_00529280(record, 1) (dump-full-3:61782) lets the old current go, puts the camera at the record's place LESS 10 x the camera's FORWARD axis (+0x50..+0x58, a 16.16 unit vector:
	// camera-mover-trace.md step 8; 655360 inteng units = 6553.6 of the game's), the turn and look as they were, and makes the record current and red. No words.
	if (kind == kPromptFindSelect && OnStrategyScreen())
	{
		struct One
		{
			std::string name;
			std::array<uint8_t, 8> id;
			bool virt;
			bool generator = false;
			int32_t place[3] = { 0, 0, 0 };
		};

		std::vector<One> found;
		std::vector<std::string> rows;
		std::vector<host::VirtualObjectRef> virtuals;
		std::set<std::array<uint8_t, 8>> virtualIds;

		host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

		for (host::VirtualObjectRef const &one : virtuals)
		{
			std::array<uint8_t, 8> key;

			memcpy(key.data(), one.id, 8);
			virtualIds.insert(key);
		}

		for (size_t i = 0; i < objectList.Count(); i++)
		{
			const uint8_t *const id = objectList.Id(i);
			host::ObjectStrategy now;
			std::array<uint8_t, 8> key = {};

			if (id)
			{
				memcpy(key.data(), id, 8);
			}

			// S-11: a virtual object is its own row below (0x525496), never a second one here
			if (id && !virtualIds.count(key) && host::FindObjectStrategy(LevelObjects_Level(), id, &now) == host::StrategyFound::Found)
			{
				found.push_back(One{ objectList.Name(i), key, false });
			}
		}

		for (host::VirtualObjectRef const &one : virtuals)
		{
			std::array<uint8_t, 8> key;

			memcpy(key.data(), one.id, 8);
			found.push_back(One{ one.name, key, true });
		}

		// the enemies and generators (their wrappers are not level blocks: +8 != 0, so inteng lists them whether or not they have a strategy), and the placed hierarchies below. NOT YET: the sounds and
		// particle generators inteng lists the same way
		std::vector<host::GeneratorRef> generators;

		host::ListGenerators(LevelObjects_Level(), &generators);

		for (host::GeneratorRef const &one : generators)
		{
			std::array<uint8_t, 8> key;

			memcpy(key.data(), one.id, 8);

			One row{ one.name, key, true };

			row.generator = true;
			memcpy(row.place, one.place, sizeof(row.place));
			found.push_back(row);
		}

		// the placed hierarchies (wrapper class 0x5afae0, no level block: listed as the generators are, S-K76-W), by their PLACHIDT names
		std::vector<host::PlacedHierarchyRef> placedHierarchies;

		host::ListPlacedHierarchies(LevelObjects_Level(), &placedHierarchies);

		for (host::PlacedHierarchyRef const &one : placedHierarchies)
		{
			std::array<uint8_t, 8> key;

			memcpy(key.data(), one.id, 8);

			One row{ one.name, key, true };

			memcpy(row.place, one.place, sizeof(row.place));
			found.push_back(row);
		}

		for (One const &one : found)
		{
			rows.push_back(one.name);
		}

		DialogList("Select Object", rows, [found](int32_t place)
		{
			if (place < 0 || (size_t)place >= found.size())
			{
				return;
			}

			One const &chosen = found[(size_t)place];

			// FUN_00529280(record, 1): 0x52928e-0x5292a5 / 0x529342-0x529357 the old current let go, the record current and red; 0x5292c1-0x529321 the camera behind it (StrategyFlyTo)
			host::Selected picked = {};

			picked.kind = host::SelectedKind::Object;
			memcpy(picked.objectId, chosen.id.data(), host::kSelectionIdBytes);
			host::Selection_Set(picked);
			StrategyFlyTo(chosen.id.data());
		});
		return true;
	}

	if (kind == kPromptFindSelect)
	{
		std::vector<std::string> rows;
		std::vector<std::string> ids;

		for (size_t i = 0; i < objectList.Count(); i++)
		{
			if (const uint8_t *const id = objectList.Id(i))
			{
				rows.push_back(objectList.Name(i));
				ids.push_back(HexOf(id));
			}
		}

		DialogList("Select Object", rows, [ids](int32_t place)
		{
			if (place >= 0 && (size_t)place < ids.size())
			{
				promptFor = kPromptFindSelect;
				promptText = ids[(size_t)place];
				RunThePrompt();
			}
		});
		return true;
	}

	return false;
}

// Object > Rescale (0x1a, OP-13): FUN_004d9af0 0x4dc28f-0x4dc2a6 calls the CURRENT object's +0x18 when Gravity is off; only the placed shape has one (0x4e1a80, the others RET 4: nothing happens).
// FUN_004e1a80 opens inteng's "Rescale Shape" window FUN_004d4f80: 400 x 240, the words "Type in the new scale of the shape." at 18,18 (370 x 40), an EMPTY edit at 30,100 (300 x 35, up to 200 letters),
// OK at 100,150 and Cancel at 200,150 (50 x 16), "Take scale from shape rif" at 75,200 (200 x 16); Return is OK. The answers (0x4d5150): OK 2, Cancel 1, Take scale 3. OK: the text is read by
// sscanf "%lf" (0x5e8af8: a number at its start; nothing read, nothing done) and taken when it is 0 to 1,000,000 and the shape has an outside file: the shape made that size (shape_rescale_edit.h, with
// its named deviations). Take scale (FUN_004e1970) brings the shape up to date from its own .rif, forced (OutsideShapesUpdate, OP-137..OP-141), then rescales it by new / old (FUN_004e1340).
static std::string rescaleWords;

// THE OUTSIDE SHAPES' UPDATE (inteng FUN_00512fc0; reassess/object-placement.md section 7, OP-137..OP-141; shape_update.h). Added 2026-11-16 by Melanikus (Vasquez).
// Every outside shape of the held level (forced false: inteng does it at every load, silently), or the one the named object uses (forced true: Take scale), is brought up to date from its own
// file, found in THE REFRESH'S ORDER (shape_search.h ShapeSearch_RefreshOrder, Frost's ruling 2026-11-23: ShapeFolder=, then the game's Tools\shape_rifs; NEVER the game's shape_rifs, where
// Option C's files would swap a level's own copy for another level's variant; before: ShapeFolder= or shape_rifs beside the program). A file in no folder changes nothing, as in inteng. A
// folder of the order that is refused, or a file there that is refused, keeps the level's own copy: it is never taken from a folder past it (fail closed). Each refusal goes to the log with
// the file's name and why (OP-141 (g)). All the changes go to the game's own loader at once (OpenLevelFile::ReplaceLevel), which refuses the lot or takes it. The file is read on one handle,
// a plain file with one name, not a folder, a link or a device (ShapeImport_Read; OP-141 (b)); the name is held to plain letters and the size to 4 MB.
// Gives how many shapes changed; *before / *after get the last changed shape's rescale.
static size_t const kOutsideMostLevelBytes = 32u << 20;   // what the game's loader takes, unpacked (OpenLevelFile's MostUnpackedBytes, level_file.cpp)

static int OutsideShapesUpdateUnguarded(OpenLevelFile *open, bool forced, std::string const &onlyObject, double *before, double *after, std::vector<std::string> *names)
{
	using namespace host::shape_chunks;

	std::vector<char> image;
	std::vector<Span> top;
	size_t total = 0;
	std::vector<host::ShapeSearchFolder> const order = host::ShapeSearch_RefreshOrder(shapeFolderSetting, theGameFolder);

	if (!open || order.empty() || !open->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2") || !ChunkSizeAt(image, 0, image.size(), &total) ||
		!Children(image, kHeadingBytes, total, &top))
	{
		return 0;
	}

	std::set<std::string> usedFolders;   // the labels of the folders the changed shapes came from, for the log's last line

	std::vector<std::vector<char>> made(top.size());
	int changed = 0;
	std::vector<int32_t> users;   // how many of the level's objects each shape has (FUN_004bf1d0's tie), for the scale's "no object uses it" test

	host::ShapeUpdate_LevelUsers(image, &users);
	size_t wouldBe = image.size();   // the level as it would be put together with the shapes made so far

	for (size_t i = 0; i < top.size(); i++)
	{
		if (!NameIs(image, top[i].at, "REBSHAPE"))
		{
			continue;
		}

		std::vector<char> const chunk(image.begin() + (std::ptrdiff_t)top[i].at, image.begin() + (std::ptrdiff_t)(top[i].at + top[i].size));
		std::string name;

		if ((!onlyObject.empty() && !host::ShapeUpdate_UsedBy(chunk, onlyObject)) || !host::ShapeUpdate_FileName(chunk, &name))
		{
			continue;
		}

		// the first folder of the refresh's order that holds <name>.rif, each folder judged as it is looked in (shape_search.h)
		host::ShapeSearchFolder found;
		std::string searchWhy;
		host::ShapeSearchResult const searched = host::ShapeSearch_Find(order, name, &found, &searchWhy);

		if (searched == host::ShapeSearchResult::NotFound || searched == host::ShapeSearchResult::NothingToLook)
		{
			continue;   // in no folder: nothing, as inteng (0x5130ad)
		}

		if (searched != host::ShapeSearchResult::Found)
		{
			LogString("outside shape " + name + ": refused, " + searchWhy + "; the level's own copy is kept");
			continue;
		}

		std::vector<char> bytes;
		host::ShapeImportResult const read = host::ShapeImport_Read(found.path.c_str(), name, &bytes, nullptr);

		if (read == host::ShapeImportResult::NotFound)
		{
			continue;
		}

		if (read != host::ShapeImportResult::Done)
		{
			LogString("outside shape " + name + ": refused, " + host::ShapeImportWords(read) + "; the level's own copy is kept");
			continue;
		}

		std::string why;
		host::ShapeUpdateResult const result = host::ShapeUpdate_Build(chunk, bytes, forced, i < users.size() ? users[i] : 0, &made[i], &why, before, after);

		if (result == host::ShapeUpdateResult::Done || result == host::ShapeUpdateResult::NameOnly)
		{
			// HOSTILE INPUT: many shapes can name one big file, and every made shape is kept until the level is put together, so the level as it would be is measured as each one
			// is made, and the lot is refused as soon as it passes what the game's loader takes, before the next one is made (hostile-input-1 of the 11-17 review: it ran the 32-bit
			// editor out of memory while a level opened)
			wouldBe = wouldBe - top[i].size + made[i].size();

			if (wouldBe > kOutsideMostLevelBytes)
			{
				LogString("outside shapes: the level would be bigger than the game's loader takes (" + std::to_string(kOutsideMostLevelBytes >> 20) + " MB) with them; its own shapes are kept");

				if (names)
				{
					names->clear();
				}

				return 0;
			}

			changed++;
			usedFolders.insert(found.label);

			if (names)
			{
				names->push_back(name);
			}
		}
		else if (result != host::ShapeUpdateResult::NotOutside)
		{
			made[i].clear();
			LogString("outside shape " + name + ": refused, " + host::ShapeUpdateWords(result) + (why.empty() ? std::string() : " (" + why + ")") + "; the level's own copy is kept");
		}
	}

	if (changed == 0)
	{
		return 0;
	}

	std::vector<char> level(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);

	for (size_t i = 0; i < top.size(); i++)
	{
		if (!made[i].empty())
		{
			level.insert(level.end(), made[i].begin(), made[i].end());
		}
		else
		{
			AddCopy(&level, image, top[i]);
		}
	}

	if (level.size() > 0x7fffffff)
	{
		LogString("outside shapes: the level would be too big; its own shapes are kept");
		return 0;
	}

	PutIntAt(&level, 8, (int32_t)level.size());

	if (open->ReplaceLevel(level) != LevelOpenResult::Opened)
	{
		LogString("outside shapes: the game's own loader refused the level with them; its own shapes are kept");
		return 0;
	}

	std::string from;

	for (std::string const &label : usedFolders)
	{
		from += (from.empty() ? "" : " and ") + label;
	}

	LogString("outside shapes: " + std::to_string(changed) + " brought up to date from " + from);
	return changed;
}

// The update, with running out of memory on a hostile level refused like any other refusal: the level is as it was (hostile-input-1)
int OutsideShapesUpdate(OpenLevelFile *open, bool forced, std::string const &onlyObject, double *before, double *after, std::vector<std::string> *names)
{
	try
	{
		return OutsideShapesUpdateUnguarded(open, forced, onlyObject, before, after, names);
	}
	catch (std::bad_alloc const &)
	{
		LogString("outside shapes: not enough memory to bring them up to date; the level's own shapes are kept");

		if (names)
		{
			names->clear();
		}

		return 0;
	}
}

// OP-137a (inteng: the SHPEXTFL's post_input_processing FUN_005149e0 -> FUN_00512fc0(0) runs WHILE THE RIF IS READ, vtable 0x5aeb9c +0x24, so nothing is ever drawn with the file's old
// outside shapes): the refresh of a whole level FILE, for the open path to call on the editor's OWN copy (avp_rifs\OpenedByEditor\...) before the game loads it for the view (agreed with
// Frost 11-16: the open path is his). The file is opened by the game's hardened loader (OpenLevelFile::Open: hostile input), every outside shape is brought up to date (OutsideShapesUpdate,
// not forced), and only when one changed is the file written again, over THAT path (SaveAs: the game's loader checks the new bytes first). Never called with his own file. Silent as in
// inteng (OP-139): each refusal goes to the log only. *why (when given) says what happened. Added 2026-11-16 by Melanikus (Vasquez).
// (its answer, OutsideRefresh, is in editor_shared.h: editor_main.cpp reads it too)

std::string outsideShapesWords;   // OP-140: the open's refresh, for the status line (empty: nothing changed)

// OP-140's words for the shapes an update brought up to date (the file names OutsideShapesUpdate gives): each file once, in the level's order, with how many of its shapes when more than one;
// at most 8, so the line stays readable
std::string OutsideShapesWordsOf(std::vector<std::string> const &refreshed)
{
	std::string words;

	std::vector<std::pair<std::string, int>> named;

	for (std::string const &one : refreshed)
	{
		auto const same = std::find_if(named.begin(), named.end(), [&one](std::pair<std::string, int> const &n) { return n.first == one; });

		if (same == named.end())
		{
			named.push_back(std::make_pair(one, 1));
		}
		else
		{
			same->second++;
		}
	}

	words = "Outside shapes brought up to date from their own files:";

	for (size_t i = 0; i < named.size() && i < 8; i++)
	{
		words += (i ? ", " : " ") + named[i].first + (named[i].second > 1 ? " (" + std::to_string(named[i].second) + " shapes)" : std::string());
	}

	if (named.size() > 8)
	{
		words += " and " + std::to_string(named.size() - 8) + " more files";
	}

	return words;
}


OutsideRefresh OutsideShapes_RefreshFile(const char *rifPath, std::string *why)
{
	std::string words;
	OutsideRefresh result = OutsideRefresh::Unchanged;
	OpenLevelFile file;
	std::vector<std::string> refreshed;
	LevelOpenResult const opened = rifPath && rifPath[0] ? file.Open(rifPath) : LevelOpenResult::NoPath;

	if (opened != LevelOpenResult::Opened)
	{
		result = OutsideRefresh::Refused;
		words = std::string("outside shapes: the level file wasn't refreshed, ") + NotHeldWords(opened);
	}
	else if (OutsideShapesUpdate(&file, false, std::string(), nullptr, nullptr, &refreshed) == 0)
	{
		words = "outside shapes: nothing to bring up to date";
	}
	else if (file.SaveAs(rifPath) != LevelSaveResult::Saved)
	{
		result = OutsideRefresh::Refused;
		words = "outside shapes: the refreshed level couldn't be written; the file is as it was";
	}
	else
	{
		result = OutsideRefresh::Changed;
		words = OutsideShapesWordsOf(refreshed);   // OP-140 (Bishop's ruling 11-17): inteng's refresh is silent; ours says which shapes it brought up to date
	}

	if (result != OutsideRefresh::Unchanged)
	{
		LogString(words);
	}

	if (why)
	{
		*why = words;
	}

	return result;
}

static void RescaleShapeBox()
{
	uint8_t id[8];

	if (placementGravity || specialMode.haveCurrent || placedHierarchyCurrent >= 0 || host::Selection_Get().kind != host::SelectedKind::Object || !SelectedObjectId(id))
	{
		return;   // no current placed shape (or Gravity on): inteng does nothing
	}

	host::DialogFormSpec spec;
	host::DialogWidget words, edit, ok, cancel, fromRif;

	spec.title = "Rescale Shape";
	spec.width = 400;
	spec.height = 0xf0;
	words.type = host::DialogWidgetType::Label;
	words.text = "Type in the new scale of the shape.";
	words.box = { 0x12, 0x12, 0x172, 0x28 };
	edit.type = host::DialogWidgetType::Edit;
	edit.box = { 0x1e, 100, 300, 0x23 };
	edit.most = 200;
	ok.type = host::DialogWidgetType::Button;
	ok.text = "OK";
	ok.box = { 100, 0x96, 0x32, 0x10 };
	ok.answer = host::kDialogOk;
	cancel.type = host::DialogWidgetType::Button;
	cancel.text = "Cancel";
	cancel.box = { 200, 0x96, 0x32, 0x10 };
	cancel.answer = host::kDialogEscape;
	fromRif.type = host::DialogWidgetType::Button;
	fromRif.text = "Take scale from shape rif";
	fromRif.box = { 0x4b, 200, 200, 0x10 };
	fromRif.answer = 4;   // its own answer (inteng's 3; ours numbers OK as 3)
	spec.widgets.push_back(words);
	spec.widgets.push_back(edit);
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);
	spec.widgets.push_back(fromRif);

	std::vector<uint8_t> const object(id, id + 8);

	host::Dialogs_Form(spec, [object](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer == 4)
		{
			// Take scale from shape rif (FUN_004e1970): the shape brought up to date from its own file, FORCED (FUN_00512fc0(1): rescale 1.0, then the file's ENVSDSCL when it has one), then every
			// placed object of the shape rescaled by new / old (its +0x14 = FUN_004e1340, here Drake's ShapeRescale_ByValue with the value that gives that factor; NAMED there: a shape other
			// objects use too is refused, where inteng scales it once per object)
			std::vector<host::LevelObject> objects;
			std::string name;

			if (LevelObjects_Read(&objects))
			{
				for (host::LevelObject const &one : objects)
				{
					if (memcmp(one.id, object.data(), 8) == 0 && one.name)
					{
						name = one.name;
					}
				}
			}

			double before = 0.0, after = 0.0;

			if (name.empty() || OutsideShapesUpdate(LevelObjects_OpenForEdit(), true, name, &before, &after) == 0)
			{
				rescaleWords = "Take scale from shape rif: the shape's own file is not in the shape folder, or was refused (the log says why); nothing changed";
				Answer(rescaleWords);
				return;
			}

			// the update put the level together again (ReplaceLevel): every Chunk * of the old one is gone, so the list and the field panel let go of them (open-path-1 of the 11-17 review)
			LoadTheObjectListAgain();

			// FUN_004e1970: +0x14 (FUN_004e1340) with new / old, the rescale read before and after FUN_00512fc0(1): the FACTOR is checked and used (ShapeRescale_ByFactor), not a typed size made from
			// it, whose own 1,000,000 bound refused factors inteng takes (open-path-4). The RIFFNAME-only update gives the same rescale twice: a factor of 1.0, as inteng.
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			double const factor = before > 0.0 ? after / before : 0.0;
			std::string reason;
			unsigned const generation = open ? open->Generation() : 0;
			host::ShapeRescaleResult const result = before > 0.0 && open ? host::ShapeRescale_ByFactor(open, LevelObjects_Level(), object.data(), factor, &reason) : host::ShapeRescaleResult::BadShape;

			if (open && open->Generation() != generation)
			{
				LoadTheObjectListAgain();   // the rescale made a SHPCENTR by putting the level together again (V-30a)
			}

			rescaleWords = "Take scale from shape rif: the shape is its file's again at scale " + std::to_string(after) + (result == host::ShapeRescaleResult::Done ?
				", then " + std::to_string(factor) + " times that (new / old)" : std::string(", but the rescale by new / old was not done: ") + host::ShapeRescaleWords(result));
			Answer(rescaleWords);
			return;
		}

		if (answer != host::kDialogOk)
		{
			return;   // Cancel or Escape
		}

		std::string typed;

		for (host::DialogWidget const &one : widgets)
		{
			if (one.type == host::DialogWidgetType::Edit)
			{
				typed = one.text;
			}
		}

		double value = 0.0;

		if (sscanf_s(typed.c_str(), "%lf", &value) != 1 || !(value >= 0.0 && value <= host::kRescaleMost))
		{
			return;   // nothing read, or outside 0 to 1,000,000: inteng does nothing
		}

		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		double factor = 0.0;
		std::string reason;

		if (!open)
		{
			return;
		}

		unsigned const generation = open->Generation();
		host::ShapeRescaleResult const result = host::ShapeRescale_ByValue(open, LevelObjects_Level(), object.data(), value, local_scale > 0.0 ? local_scale : 1.0, &factor, &reason);

		// V-30b: a shape with no SHPCENTR gets one by the level being put together again (V-30a's ReplaceLevel): every Chunk * of the old level is gone, so the list and the field panel
		// let go of them, as every other edit that swaps the level does
		if (open->Generation() != generation)
		{
			LoadTheObjectListAgain();
		}

		if (result != host::ShapeRescaleResult::Done)
		{
			rescaleWords = std::string("Rescale: ") + host::ShapeRescaleWords(result) + (reason.empty() ? std::string() : " (" + reason + ")");
			Answer(rescaleWords);
			return;
		}

		rescaleWords = "Rescaled: every point of the shape is " + std::to_string(factor) + " times what it was (kept in the editor's bytes until Save a copy)";
		Answer(rescaleWords);
	});
}

void OpenAPrompt(int32_t kind)
{
	// Object > Rescale: inteng's window, not a typed prompt
	if (kind == kPromptRescale)
	{
		RescaleShapeBox();
		return;
	}

	uint8_t id[8];

	if (!LevelObjects_Level() || !editedLevel)
	{
		Answer("No level is held to work on");
		return;
	}

	if (kind == kPromptSoftEdge && !SelectedObjectOrModuleId(id))
	{
		Answer("Soft: select an object first (Select > Object, then click it)");
		return;
	}

	// Edge > Soft (FUN_004b0270): the text dialog "Type in the maximum angle for soft edges (flat is 0)." with Ok / Cancel (sscanf %d), not a typed prompt
	if (kind == kPromptSoftEdge)
	{
		memset(keyDown, 0, sizeof(keyDown));
		TextAs(kPromptSoftEdge, "Type in the maximum angle for soft edges (flat is 0).", "");
		return;
	}

	if ((kind == kPromptMove || kind == kPromptName) && !SelectedObjectId(id))
	{
		Answer("Select an object first: click it in the level, or choose it in the object list");
		return;
	}

	if (OpenAsDialog(kind))
	{
		return;
	}

	memset(keyDown, 0, sizeof(keyDown));
	promptFor = kind;
	promptText.clear();

	if (kind == kPromptTears)
	{
		promptText = std::to_string((int)tearTolerance);   // the last number is the default (DAT_005ced28)
	}
}
