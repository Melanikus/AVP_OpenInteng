// path_screen.cpp: PATH MODE: the working path, its tints, its properties form, its commands and prompts (Construction > Mode > Path, path_mode.h). Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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

// ---- PATH MODE (Construction > Mode > Path, 0xce; FUN_004f9780 / FUN_004f9d10, path-mode-facts.md, path_mode.h) ----
// The working path is the editor's own: number (-1 = none), name, flags, spare and the nodes. inteng writes it into the level at Exit, Create Path and Select Path; here it is written
// right after every change (Add Module, Delete Module, Properties, Delete Path), as the same level bytes (read by the game's loader first; kept until Save a copy), so no change can be lost
// by leaving the screen any way. Named deviation. A path with no node has no chunk (the write destroys it).
// The picked module is the first of the selection set's modules (inteng: the module a click sets, DAT_008f57c4).
int32_t pathNumber = -1;
std::string pathName;
static int32_t pathFlags = 0;
static int32_t pathSpare = 0;
std::vector<host::PathNode> pathNodes;
static std::vector<host::PathInfo> pathChoices;
static std::string pathWords;

// Writes the working path into the level; false (with the words) when that was refused
static bool WriteTheWorkingPath()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (pathNumber < 0)
	{
		return true;   // inteng does nothing without a number
	}

	if (!open)
	{
		pathWords = "Path: no level is held to work on";
		statusWords = pathWords.c_str();
		return false;
	}

	host::PathModeResult const result = host::PathMode_Write(open, pathNumber, pathName.c_str(), pathFlags, pathSpare, pathNodes.data(), pathNodes.size());

	if (result != host::PathModeResult::Done && result != host::PathModeResult::NothingToDo)
	{
		pathWords = std::string("Path: ") + host::PathModeWords(result);
		statusWords = pathWords.c_str();
		return false;
	}

	LoadTheObjectListAgain();
	return true;
}

bool OnThePathScreen()
{
	return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Path") == 0;
}

// PATH MODE'S COLOURS (FUN_004fa1c0, every pick and change; RGB values, the palette values of the 256-colour game are 0xfe, 0xfd, 0xfc, 0xff): the modules the person put in (flags bit 1 clear) blue (0x0000ff), the modules the route put in (flags bit 1 set) green (0x00ff00), the modules the route
// WOULD put in between the last node and the picked module (FUN_004f8c10) yellow (0xffff00), the picked module purple (0x7f007f). NAMED: FUN_004fa5a0's cycling colour over the nodes every frame (the node it marks and its colour are not read) is not drawn.
// a module's number in the level (the chunk's index, what a path node and ModuleIndexOfId name) -> the game's module number (what the tints name), found through the object's ID; made again when the level's bytes change.
// EVERY module of the game's array goes in the map, in play or not (Vasquez 11-20, Drake's catch: 13 selected in Waypoint, 5 painted red). The map is first made on the editor's first frame,
// before the view's first draw gives every module a display block. ModuleObject asks for one (LevelView_ModuleName), so the modules not seen from where the level starts (the U's 8)
// were left out, and they stayed out until the level changed. Here a module is found by its own name and place, as ModuleObject finds it, with no display block asked for
static bool ModuleObjectInPlayOrNot(OpenLevelFile *open, int32_t moduleIndex, uint8_t id[8])
{
	std::string name;
	int32_t world[3];

	return open && open->Level() && LevelView_ModuleOwnName(moduleIndex, &name) && LevelView_ModuleWorld(moduleIndex, world) &&
		host::PolygonItems_ModuleObject(open->Level(), name.c_str(), world, local_scale > 0.0 ? local_scale : 1.0, id);
}

int32_t GameModuleOfNumber(int32_t number)
{
	static std::map<int32_t, int32_t> known;
	static unsigned seen = 0xFFFFFFFFu;
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		return -1;
	}

	if (seen != open->Generation())
	{
		seen = open->Generation();
		known.clear();

		for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
		{
			uint8_t id[8];
			int32_t index = 0;

			if (ModuleObjectInPlayOrNot(open, m, id) && host::ModuleIndexOfId(open->Level(), id, &index))
			{
				known[index] = m;
			}
		}
	}

	auto const found = known.find(number);

	return found == known.end() ? -1 : found->second;
}

void PathTintsEveryFrame()
{
	static bool shown = false;
	static std::vector<int32_t> mods;
	static std::vector<uint32_t> cols;
	static std::vector<host::PathNode> seenNodes;
	static int32_t seenPicked = -2;

	if (!OnThePathScreen())
	{
		if (shown)
		{
			LevelView_SetModuleTints(nullptr, nullptr, 0);
			shown = false;
			seenPicked = -2;
		}

		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		return;
	}

	std::vector<uint8_t> const ids = PickedModuleIdBytes();
	int32_t picked = -1;

	if (!ids.empty() && !host::ModuleIndexOfId(LevelObjects_Level(), ids.data(), &picked))
	{
		picked = -1;
	}

	bool same = picked == seenPicked && seenNodes.size() == pathNodes.size();

	for (size_t i = 0; same && i < pathNodes.size(); i++)
	{
		same = pathNodes[i].module == seenNodes[i].module && pathNodes[i].flags == seenNodes[i].flags;
	}

	if (same && shown)
	{
		return;
	}

	seenPicked = picked;
	seenNodes = pathNodes;
	mods.clear();
	cols.clear();

	for (host::PathNode const &node : pathNodes)
	{
		int32_t const game = GameModuleOfNumber(node.module);

		if (game >= 0)
		{
			mods.push_back(game);
			cols.push_back((node.flags & 1) ? 0xFF00FF00u : 0xFF0000FFu);
		}
	}

	if (picked >= 0)
	{
		if (!pathNodes.empty())
		{
			host::PathGraph graph;
			std::vector<int32_t> numbers;
			std::vector<std::string> names;

			if (host::PathMode_BuildGraph(open, &graph, &numbers, &names))
			{
				int pickedAt = -1, lastAt = -1;

				for (size_t i = 0; i < numbers.size(); i++)
				{
					pickedAt = (pickedAt < 0 && numbers[i] == picked) ? (int)i : pickedAt;
					lastAt = (lastAt < 0 && numbers[i] == pathNodes.back().module) ? (int)i : lastAt;
				}

				std::vector<host::PathNode> trial = pathNodes;

				if (pickedAt >= 0 && lastAt >= 0 && host::PathMode_AddModule(&trial, graph, numbers, lastAt, pickedAt))
				{
					// the modules PathMode_AddModule put between the path's last node and the picked one: the nodes after pathNodes' own, all but
					// trial's last (read in order, without an index into trial)
					size_t at = 0;

					for (host::PathNode const &node : trial)
					{
						int32_t const game = (at >= pathNodes.size() && at + 1 < trial.size()) ? GameModuleOfNumber(node.module) : -1;

						if (game >= 0)
						{
							mods.push_back(game);
							cols.push_back(0xFFFFFF00u);
						}

						at++;
					}
				}
			}
		}

		int32_t const game = GameModuleOfNumber(picked);

		if (game >= 0)
		{
			mods.push_back(game);
			cols.push_back(0xFF7F007Fu);
		}
	}

	size_t const count = mods.size() < kLevelViewMostSelected ? mods.size() : kLevelViewMostSelected;

	LevelView_SetModuleTints(mods.data(), cols.data(), count);
	shown = true;
}

static void SayThePath()
{
	pathWords = "Path :" + std::to_string(pathNumber) + "  " + pathName + " (" + std::to_string((unsigned long long)pathNodes.size()) + " module(s))";
	statusWords = pathWords.c_str();
}

// What a mode lets go of when it is left (inteng's tear-down of the mode's state)
void ExitCleanup(size_t screen)
{
	if (screen >= host::kIntengScreenCount)
	{
		return;
	}

	char const *const name = host::kIntengScreens[screen].name;

	if (strcmp(name, "Strategy Data") == 0)
	{
		strategyFromPlacement = false;
		strategyFlagPage = false;
		strategyViewBit = 0;
		strategyMarkedChanged = false;   // FUN_00522ab0 0x522ae2: DAT_00914950 = 0 on the next entry
		host::Selection_Clear();   // FUN_00529480: the current object's selection mark is cleared
	}
	else if (strcmp(name, "Path") == 0)
	{
		pathNumber = -1;           // FUN_004f9b90 releases the working path (it was written by every change here, as FUN_004fab40 does on the way out)
		pathNodes.clear();
	}
	else if (IsALightScreen(screen))
	{
		// Light Sourcing's Exit comes here (LeaveTheMode), not through the bar's change of screen, so what leaving the screen undoes runs here too: the Gouraud switch its entry turned on
		// (EnterLightSourcing / LeaveLightSourcing), as when the bar leaves it. (Bishop's queue 11-16, after Frost found the same in Hierarchy.)
		LeaveLightSourcing();
	}
	else if (IsTheTextureMapScreen(screen))
	{
		// Texture Map's Exit (FUN_0054d260 case 5): FUN_0054bfd0 frees the mode, then the hub. Exit comes here (LeaveTheMode), not through the bar's change of screen, so the mode is left here too:
		// before, the hub kept onTextureMap and drew the picked polygons (Melanikus 11-16; reassess/mode-entry-exit-frost.md TM-X1)
		LeaveTextureMap();
	}
	else if (IsTheHierarchyScreen(screen))
	{
		// Hierarchy's Exit (case 0x2, FUN_004888d0, reassess/hierarchy-play-frost.md HP-1): the player is freed (FUN_00477870) and the hub shows the level again. Mode > Exit comes here (LeaveTheMode),
		// not through the bar's change of screen, so the mode is left here: the model, its text rows and the view switch's hiding go with it
		LeaveHierarchy();
	}
}

// "PATH PROPERTIES" (FUN_004fbdf0 / FUN_004fbee0: FUN_0057d170(400, 200, 7), the title 0x5e98c8; two check boxes at x 0x10, y 0x50 and 100 with the words "Loop path" (0x5e98bc) and "Go back and forth" (0x5e98a8) at x 0x20 (200 x 0x12), Ok at (100, 0x96) and Cancel at (200, 0x96), each 0x32 x 0x12);
// the first box is on when the path's flags have bit 2 clear (FUN_004fc170 sets bit 2 when "Loop path" is NOT picked); they are a pair (picking one lets the other go)
static void PathPropertiesForm()
{
	host::DialogFormSpec spec;

	spec.title = "Path Properties";
	spec.width = 400;
	spec.height = 200;

	bool const loop = (pathFlags & 2) == 0;
	int32_t const ys[2] = { 0x50, 100 };
	char const *const words[2] = { "Loop path", "Go back and forth" };

	for (int i = 0; i < 2; i++)
	{
		host::DialogWidget box, label;

		box.type = host::DialogWidgetType::Check;
		box.box = host::DialogRect{ 0x10, ys[i], 12, 12 };
		box.checked = i == 0 ? loop : !loop;
		box.group = 0;
		label.type = host::DialogWidgetType::Label;
		label.box = host::DialogRect{ 0x20, ys[i], 200, 0x12 };
		label.text = words[i];
		spec.widgets.push_back(box);
		spec.widgets.push_back(label);
	}

	host::DialogWidget ok, cancel;

	ok.type = host::DialogWidgetType::Button;
	ok.box = host::DialogRect{ 100, 0x96, 0x32, 0x12 };
	ok.text = "Ok";
	ok.answer = host::kDialogOk;
	cancel.type = host::DialogWidgetType::Button;
	cancel.box = host::DialogRect{ 200, 0x96, 0x32, 0x12 };
	cancel.text = "Cancel";
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			return;
		}

		int box = 0;
		bool loopPicked = true;

		for (host::DialogWidget const &w : widgets)
		{
			if (w.type == host::DialogWidgetType::Check)
			{
				if (box == 1 && w.checked)
				{
					loopPicked = false;
				}

				box++;
			}
		}

		RunPathPrompt(kPromptPathProps, loopPicked ? "loop" : "back");
	});
}

void PathModeCommand(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		statusWords = "Path: no level is held to work on";
		return;
	}

	switch (command)
	{
		case 3:   // Add Module (key L): FUN_004fa6a0
		case 4:   // Delete Module (key D): FUN_004fa950
		{
			if (pathNumber < 0)
			{
				statusWords = "Path: Create Path or Select Path first";
				return;
			}

			if (command == 4)
			{
				host::PathMode_DeleteLast(&pathNodes);

				if (WriteTheWorkingPath())
				{
					SayThePath();
				}

				return;
			}

			std::vector<uint8_t> const ids = PickedModuleIdBytes();
			int32_t pickedNumber = 0;

			if (ids.empty() || !host::ModuleIndexOfId(LevelObjects_Level(), ids.data(), &pickedNumber))
			{
				statusWords = "Path: pick a module first (click on it)";
				return;
			}

			host::PathGraph graph;
			std::vector<int32_t> numbers;
			std::vector<std::string> names;

			if (!host::PathMode_BuildGraph(open, &graph, &numbers, &names))
			{
				statusWords = "Path: the level's modules could not be read";
				return;
			}

			int pickedAt = -1;
			int lastAt = -1;

			for (size_t i = 0; i < numbers.size(); i++)
			{
				if (pickedAt < 0 && numbers[i] == pickedNumber)
				{
					pickedAt = (int)i;
				}

				if (lastAt < 0 && !pathNodes.empty() && numbers[i] == pathNodes.back().module)
				{
					lastAt = (int)i;
				}
			}

			if (pickedAt < 0 || (!pathNodes.empty() && lastAt < 0))
			{
				statusWords = "Path: that module is not one the route can use";
				return;
			}

			std::vector<host::PathNode> const before = pathNodes;

			if (!host::PathMode_AddModule(&pathNodes, graph, numbers, lastAt, pickedAt))
			{
				pathNodes = before;
				statusWords = "Path: the module could not be added";
				return;
			}

			if (!WriteTheWorkingPath())
			{
				pathNodes = before;
				return;
			}

			host::Selection_Clear();   // the pick is cleared (FUN_004fa6a0)
			SayThePath();
			return;
		}
		case 5:   // Create Path: the name first (FUN_004fb300): the text dialog "Name of path (optional)" (0x5e982c, FUN_00474bc0 with Ok / Cancel; only the answer Ok keeps the text)
			TextAs(kPromptPathName, "Name of path (optional)", "");
			return;
		case 6:   // Select Path: the chooser of the level's paths (FUN_004fb6a0), the working path first written
		{
			if (!WriteTheWorkingPath())
			{
				return;
			}

			if (!host::PathMode_ReadPaths(open, &pathChoices) || pathChoices.empty())
			{
				statusWords = "Path: the level has no path to select";
				return;
			}

			// FUN_004fb6a0: the list dialog titled "Select Path" (0x5e97e8, FUN_0045dd10): a row is the path's name and the two spaces at 0x5e98a4; the dialog sorts its rows, the answer is the row's place in the list given (an answer below 2 picks none)
			{
				std::vector<std::string> rows;

				for (host::PathInfo const &one : pathChoices)
				{
					rows.push_back(one.name + "  ");
				}

				DialogList("Select Path", rows, [](int32_t choice)
				{
					if (choice >= 0)
					{
						RunPathPrompt(kPromptPathPick, std::to_string((long long)choice + 1));
					}
				});
			}

			return;
		}
		case 7:   // Delete Path: "Delete this path?"
			if (pathNumber < 0)
			{
				statusWords = "Path: there is no path to delete";
				return;
			}

			ConfirmAs(kPromptPathDelete, "Delete this path?");   // 0x5e9818, FUN_00474a40: Yes / No
			return;
		case 8:   // Keys: the help text (string 0x5e9844, FUN_004fa110)
			textPanelText = kIntengPathKeysText;   // FUN_004fa110 / FUN_004724f0: inteng's own text over the level, until a key or a press
			textPanelNext = nullptr;
			return;
		case 9:   // Properties: the "Path Properties" dialog (FUN_004fbee0)
			if (pathNumber < 0)
			{
				statusWords = "Path: Create Path or Select Path first";
				return;
			}

			PathPropertiesForm();
			return;
		default:
			statusWords = "That Path mode item isn't built yet";
			return;
	}
}

// The answers of Path mode's four prompts
void RunPathPrompt(int32_t kind, std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		statusWords = "Path: no level is held to work on";
		return;
	}

	std::string lower;

	for (char const letter : typed)
	{
		lower += (char)tolower((unsigned char)letter);
	}

	if (kind == kPromptPathName)
	{
		// Create Path: the working path is written, then a new empty one with the smallest free number (FUN_004fb300)
		if (!WriteTheWorkingPath())
		{
			return;
		}

		int32_t number = 0;

		if (!host::PathMode_NextNumber(open, &number))
		{
			statusWords = "Path: the level's paths could not be read";
			return;
		}

		if (number < 0 || number > 255)
		{
			statusWords = "Path: the level has no free path number (0 to 255)";
			return;
		}

		pathNodes.clear();
		pathNumber = number;
		pathName = typed.substr(0, 60);
		pathFlags = 0;
		pathSpare = 0;
		pathWords = "Path :" + std::to_string(pathNumber) + "  " + pathName + ": a new path (it is in the level once a module is added)";
		statusWords = pathWords.c_str();
		return;
	}

	if (kind == kPromptPathPick)
	{
		char *end = nullptr;

		errno = 0;

		long long const row = _strtoi64(typed.c_str(), &end, 10);

		if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0 || row < 1 || (unsigned long long)row > pathChoices.size())
		{
			statusWords = "Path: type the number of a path in the list";
			return;
		}

		host::PathInfo const &chosen = pathChoices[(size_t)row - 1];
		host::PathGraph graph;
		std::vector<int32_t> numbers;
		std::vector<std::string> names;

		host::PathMode_BuildGraph(open, &graph, &numbers, &names);

		// FUN_004faf40: a point with no module in the level is dropped from the working path
		pathNodes.clear();

		for (host::PathNode const &node : chosen.nodes)
		{
			bool known = false;

			for (int32_t const number : numbers)
			{
				known = known || number == node.module;
			}

			if (known)
			{
				pathNodes.push_back(node);
			}
		}

		pathNumber = chosen.number;
		pathName = chosen.name;
		pathFlags = chosen.flags;
		pathSpare = chosen.spare;
		promptList.clear();
		SayThePath();
		return;
	}

	if (kind == kPromptPathDelete)
	{
		if (lower != "y" && lower != "yes")
		{
			statusWords = "Delete Path: nothing changed";
			return;
		}

		pathNodes.clear();   // FUN_004fa950 over and over until no node is left

		if (WriteTheWorkingPath())
		{
			pathWords = "Path :" + std::to_string(pathNumber) + " is deleted from the level (kept in the editor's bytes until Save a copy)";
			statusWords = pathWords.c_str();
		}

		return;
	}

	if (kind == kPromptPathProps)
	{
		// "Loop path" or "Go back and forth": the flags' bit 2 is set when "Loop path" is NOT the one picked
		bool const loop = lower == "loop" || lower == "l" || lower == "loop path";
		bool const back = lower == "back" || lower == "b" || lower == "go back and forth";

		if (!loop && !back)
		{
			statusWords = "Path Properties: type Loop path or Go back and forth";
			return;
		}

		pathFlags = host::PathMode_FlagsAfterProperties(pathFlags, loop);

		if (WriteTheWorkingPath())
		{
			SayThePath();
		}
	}
}
