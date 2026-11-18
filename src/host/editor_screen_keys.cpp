// editor_screen_keys.cpp: THE SCREEN KEYS: the keys inteng's screens read themselves (screen_keys.h). Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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

// ---- SCREEN KEYS (screen_keys.h): the keys inteng's screens read themselves ----
// Module Linking's Shift+S (DAT_00704384, FUN_004c1a90 0x4c227f-0x4c229a): the base follows the module the camera is in. While it is on inteng shows "Setting base to current module" (0x5e3ac4) and calls FUN_004c96f0 every
// frame; ours makes the module under the camera the current one when it is another than the current one.
bool linkFollowCamera = false;

void LinkFollowOff()
{
	linkFollowCamera = false;
}

void LinkFollowPass(bool onModuleLinking)
{
	if (!linkFollowCamera)
	{
		return;
	}

	if (!onModuleLinking || !theCamera)
	{
		linkFollowCamera = false;   // the screen is left (FUN_004c3a00 clears it at the start)
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t const here[3] = { (int32_t)theCamera->x, (int32_t)theCamera->y, (int32_t)theCamera->z };
	int32_t const room = LevelView_ModuleAt(here);
	uint8_t id[8];
	int32_t index = -1;

	// inteng's words for the follow are its text row at (10, 90) (the panels' text, row ML-M-7), not a status line

	if (!open || !open->Level() || room < 0 || !ModuleObject(open, room, id) || !host::ModuleIndexOfId(open->Level(), id, &index))
	{
		return;
	}

	if (linkTree && linkTree->RootModule() == index)
	{
		return;
	}

	// FUN_004c96f0 0x4c975c: the module made current directly (FUN_004c10c0), the picks untouched and the follow left on (until 2026-11-16 this went through the picks and Set Current,
	// which stopped working when Module Linking got its own picks, e6ef0b4). NAMED: inteng takes the FIRST module in its order whose box holds the camera (FUN_004a6f30); here the view's
	// own LevelView_ModuleAt (row ML-M-66)
	LinkMakeCurrent(index);

	// FUN_004c10c0 with the follow (param_3) and a write pending (DAT_0070438c, 0x4c16a5-0x4c16c2): the level FILE is written
	if (LevelChangedSinceSave())
	{
		SaveAtIntengsPoint();
	}
}

// Light Sourcing's S and X (FUN_004aab60 0x4aad37 / 0x4aad57: FUN_004ac240(the selected light, 1 / 0)): the selected light on or off, the editor's state only
static void LightSwitchSelected(bool on)
{
	LightStateOfThisLevel();

	host::Selected const picked = host::Selection_Get();

	if (picked.kind != host::SelectedKind::Light || picked.lightIndex < 0)
	{
		statusWords = on ? "Switch on: pick a light first (Select, Light, then click one)" : "Switch off: pick a light first (Select, Light, then click one)";
		return;
	}

	lightState.SetOn((size_t)picked.lightIndex, on);
	lightContributionOff = on ? -1 : picked.lightIndex;   // FUN_004ac240: on puts the part back (FUN_004feb00), off takes it off (FUN_004fe9d0)
	RelightTheView();
	lightWords = "Light " + std::to_string(picked.lightIndex + 1) + (on ? " is on" : " is off") + " (the editor's state: the light's chunk is not changed)";
	statusWords = lightWords.c_str();
}

// A key of the screen shown (screen_keys.h): true when the key does a command of this screen. A command that has an item on the bars now is done as a click on the item is; the rest is done by the editor.
bool RunScreenKey(uint8_t key, bool shift)
{
	if (barScreen >= host::kIntengScreenCount)
	{
		return false;
	}

	// Texture Map's Merge (sub-mode 2): M, X and B are FUN_005511f0's own (the key slots), so they win over the editor's keys (B is the editor's Gouraud otherwise): texture_submodes.cpp
	if (onTextureMap && TextureMergeOn() && TextureMergeKey(key))
	{
		return true;
	}

	char const *const table = host::kIntengScreens[barScreen].table;
	char const *const name = host::kIntengScreens[barScreen].name;
	bool const onWaypoint = strcmp(name, "Waypoint") == 0;
	int32_t const command = host::ScreenKeys_Command(table, name, onWaypoint ? waypointMode.Mode() : -1, key, shift);

	if (command == 0)
	{
		return false;
	}

	if (command == host::kScreenKeyLinkFollow)
	{
		linkFollowCamera = !linkFollowCamera;
		return true;
	}

	if (command == host::kScreenKeyLightHeadlight)
	{
		RunAction(kActionViewHeadlight);
		return true;
	}

	if (command >= host::kScreenKeyLightCopy && command <= host::kScreenKeyLightPlacedProperties && RunLightKey(command))
	{
		return true;
	}

	if (command == host::kScreenKeyLightOn || command == host::kScreenKeyLightOff)
	{
		LightSwitchSelected(command == host::kScreenKeyLightOn);
		return true;
	}

	if (onWaypoint)
	{
		// the Waypoint screen's keys are commands of its three modes, whatever page of the menu is shown
		EditorHostState host = {};

		host.running = running;
		host.layout = keyLayout;
		host.screen = barScreen;
		host.prompt = kPromptNone;
		DoWaypointCommand(command, host);
		return true;
	}

	int32_t const action = BarActionOfCommand(command);

	if (!action)
	{
		return false;
	}

	RunAction(action);
	return true;
}

// Exit + New Adjacencies (0x66) and Exit + All Adjacencies (0x65): FUN_004c5a30(1) and (0). The rules and what is named are in adjacency_generate.h; the level's links have been
// written by LinkExit already. FUN_004c5a30 ends with FUN_00403d50 (0x4c603a), which asks "Warning adjacent modules don't touch ..." for each pair that doesn't touch, a Yes taking
// that entry out (StartEntryRecalc, as Adjacent's Recalculate Entry Points), and then writes the level file (FUN_004719e0 at 0x4c6047): ours saves there (SaveAtIntengsPoint) once
// the last question is answered. (The questions and the save added 2026-11-17 by Melanikus (Dietrich), AD-6 of reassess/adjacent-closeout-dietrich.md; until then the entry points
// were worked out with no question and the data was kept in the editor's bytes until Save a copy.)
void GenerateAdjacency(bool newOnly)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		linkMessage.clear();
		Answer("No level is held to work on");
		return;
	}

	host::AdjacencyCounts counts;
	std::string why;
	host::AdjacencyResult const result = host::Adjacency_Generate(open, newOnly, local_scale > 0.0 ? local_scale : 1.0, &counts, &why, false);

	LoadTheObjectListAgain();

	if (result != host::AdjacencyResult::Done)
	{
		linkMessage.clear();
		Answer(std::string("Adjacent module data: ") + host::AdjacencyWords(result) + (why.empty() ? std::string() : " (" + why + ")"));
		return;
	}

	std::string const made = std::string(newOnly ? "Adjacent module data for the new modules: " : "Adjacent module data for every module: ") + std::to_string(counts.made) + " modules made (" +
		std::to_string(counts.entries) + " entries)" + (counts.noModuleData > 0 ? ", " + std::to_string(counts.noModuleData) + " had no module data" : std::string()) +
		(counts.noneFound > 0 ? ", " + std::to_string(counts.noneFound) + " found no adjacent module" : std::string());

	// NAMED: inteng asks these questions and saves while still on Module Linking (FUN_004c3f40 / FUN_00447550 come after FUN_004c5a30); ours has left for the hub at the click, as it does for
	// the "Calculating ..." words and Exit + All's own question (adjacency_generate.h, named 5), so the boxes come over the hub
	bool const started = StartEntryRecalc(newOnly ? "Exit + New Adjacencies" : "Exit + All Adjacencies", [made](std::string const &words)
	{
		const std::string saved = SaveAtIntengsPoint();   // const first: Snyk Code's parser skips the whole file on "std::string const" in a lambda given to a call

		linkMessage.clear();
		Answer(made + "; " + words + "; " + saved);
	});

	if (!started)
	{
		linkMessage.clear();
	}
}

// AutoLink > Load from file (0xa3, FUN_004c8be0, dump-full-2 64700, read 22:40): the level's .mlf next to it (autolink_mlf.h: the file is HOSTILE INPUT, parsed whole before anything is
// changed). Each record: the module of its name (the first in record order decides; one that moved since the file was made is skipped, "%s has moved since module file created"),
// made current with its links kept, ONE picture from the record's view (the place x 100, the matrix transposed: MlfView), with the doors' second pictures and the cut, on the worker thread
// (autolink_runner.h StartFromFile) and Frost's display ("Right click to cancel" at (200, 200), 0x4c8d9c); a right press read at the top of each record ends it (0x4c8cd5). At the end
// the last module done stays current (no FUN_004c10c0(kept)). NAMED: inteng moves the editor's camera to each record's view (0x4c8dc3-0x4c8e02); ours leaves the view where it is.
// INTENG BUG, FIXED (MAPPING-ORDERS section 15): inteng's counter prints a number set to 0 once (0x4c8cc2) and never changed, so it reads "000" for every record; ours shows the record's
// number, as AutoLink's and Delete Links' counters do. Until 2026-11-16 this was the old synchronous pass (AutoLink_FromMlf: no door steps, Normal's walk, no display, no cancel).
void AutoLinkLoadFileCommand()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	char opened[MAX_PATH] = "";

	if (!open || !LevelObjects_Level() || !LevelView_FilePath(opened, sizeof(opened)))
	{
		Answer("No level is held to work on");
		return;
	}

	if (autoLinkRun.Running() || DeleteLinksRunning())
	{
		Answer("AutoLink is running: right click to cancel");
		return;
	}

	// inteng reads the module file beside the level it opened (the level's name with its ending replaced, 0x4c8bf6-0x4c8c50). Ours opens a COPY in avp_rifs\OpenedByEditor (open_with.h),
	// so while that copy is the level named on the command line, the file is looked for beside the level where it really is; else beside the level's file in the game's folder.
	// (Until 2026-11-16 only the copy's folder was looked in, where no module file ever is.)
	std::string source;

	if (!levelSourcePath.empty())
	{
		char narrow[MAX_PATH * 2] = "";

		if (WideCharToMultiByte(CP_ACP, 0, levelSourcePath.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, nullptr) > 0)
		{
			std::string const openedName = opened, sourceName = narrow;
			size_t const a = openedName.find_last_of("/\\"), b = sourceName.find_last_of("/\\");
			std::string stemOpened = a == std::string::npos ? openedName : openedName.substr(a + 1), stemSource = b == std::string::npos ? sourceName : sourceName.substr(b + 1);

			stemOpened = stemOpened.substr(0, stemOpened.find_last_of('.'));
			stemSource = stemSource.substr(0, stemSource.find_last_of('.'));

			if (openedName.find("OpenedByEditor") != std::string::npos && _stricmp(stemOpened.c_str(), stemSource.c_str()) == 0)
			{
				source = sourceName;
			}
		}
	}

	std::string const path = host::MlfPathFor(source.empty() ? opened : source.c_str());
	std::vector<uint8_t> bytes;
	std::vector<host::MlfRecord> records;
	std::string why;

	if (!host::MlfReadFile(path.c_str(), &bytes, &why) || !host::MlfParse(bytes.data(), bytes.size(), &records, &why))
	{
		Answer("Load from file: nothing changed: " + why);
		return;
	}

	host::AutoLinkScene scene;

	host::AutoLinkScene_Build(open, local_scale > 0.0 ? local_scale : 1.0, &scene);

	std::vector<host::AutoLinkRunner::FileView> views;
	size_t unknown = 0;
	std::string moved;

	for (host::MlfRecord const &record : records)
	{
		host::AutoLinkRunner::FileView one;
		bool hasMoved = false;

		if (!host::MlfFindModule(scene, record, &one.object, &hasMoved) || !host::MlfView(record, &one.view))
		{
			unknown++;
			continue;
		}

		if (hasMoved || one.object < 0)
		{
			moved += (moved.empty() ? "" : ", ") + record.name;   // 0x4c907d: "%s has moved since module file created"
			continue;
		}

		views.push_back(one);
	}

	if (!WriteTheLinks())
	{
		return;
	}

	if (!autoLinkRun.StartFromFile(open, views, local_scale > 0.0 ? local_scale : 1.0, visibleDistance, &why))
	{
		Answer("Load from file: nothing changed: " + why + (moved.empty() ? std::string() : "; moved since the module file was made: " + moved));
		return;
	}

	autoLinkFromFile = true;
	autoLinkKept = -1;
	autoLinkFileWords = " of " + std::to_string(records.size()) + " records" + (unknown > 0 ? ", " + std::to_string(unknown) + " named no module" : std::string()) +
		(moved.empty() ? std::string() : ", moved since the module file was made: " + moved);
	linkTree.reset();
	AutoLinkShow_Begin(host::kAutoLinkWordsYFromFile);   // "Right click to cancel" at (200, 200), 0x4c8d9c
	Answer("Load from file: running (right click to cancel)");
}

// Module Linking, Mode > Exit (command 0x64; FUN_004c1a90 0x4c1e1f-0x4c1e3e, menu-reads-1-full.txt:8238-8245): FUN_004c3b90 writes the current module's links
// when the tree was changed and has a link (menu-reads-2.txt: DAT_00704374[6] and [1], then the VMDARRAY made again: the same as WriteTheLinks), FUN_004c3f40
// lets go of the screen's state (the tree, the picks), then the construction screen starts (the screen change is the item's: editor_bar.cpp). If the links
// cannot be written (the loader refuses: inteng has no such case) the tree stays, so a later Set Current or Exit tries again, and the words say so.
void LinkExit()
{
	if (!WriteTheLinks())
	{
		return;
	}

	// FUN_004c3b90 0x4c3ea3 (the tree written) and FUN_004c3f40 0x4c3f5e (a write pending, DAT_0070438c): the level FILE is written when anything changed since it last was
	std::string const saved = LevelChangedSinceSave() ? SaveAtIntengsPoint() : std::string();

	// FUN_004c3f40: the current module 0 and EVERY PICK let go (0x4c3f71-0x4c3fff, row ML-E-4): back in the screen nothing is picked
	linkTree.reset();
	LinkPicksLetGo();
	Answer(saved.empty() ? std::string("Module Linking left") : "Module Linking left: " + saved);
}

// AutoLink > Delete Links: Module (0xa7, FUN_004c8af0 with the picked modules; menu-reads-1-full.txt:9028-9090 and menu-reads-2.txt) and Environment (0xa8,
// FUN_004c8af0(0): every module, after "Delete all module linking for th..." and a yes; kPromptDeleteAllLinks here). For each module that has a record,
// inteng calls FUN_004c10c0(module, 1, 0) (menu-reads-4.txt): the OLD current module's links are written first (WriteTheLinks), then the module becomes the
// current one with a NEW, EMPTY tree marked changed, and its VMDARRAY is destroyed there and then (0x4c162b: the loaded chunk's destructor). The empty
// changed tree takes the module flag out when it is next written (a later module here, Exit, or Set Current). NAMED: inteng destroys only the FIRST VMDARRAY
// of the module (the host destroys every one: a module has one).
// THE LOOP (FUN_004c8af0, dump-full-2 64648, read 22:25): "Right click to cancel" at (200, 0xdc) (0x4c8b05); the RECORDS in number order (the modules in the file's order), each that has a
// file object and, with a list, is in it (0x4c8b37-0x4c8b6f); a right press read at the TOP of each (0x4c8b2a) ends the loop; the counter "%03d" of the record number once FUN_004c10c0 said
// yes (0x4c8b9e); at the end the object list is drawn again (0x4c8bbf). It does NOT make the module that was current current again: the last one done stays current. Ours takes one module a
// frame (DeleteLinksFrame) on AutoLink's display (Frost's autolink_show_draw.h), so the counter shows and the right press is read; until 2026-11-16 the loop ran in one go, in object-number
// (or pick) order, with no words and no cancel. The records are AutoLinkScene's list (the modules with a shape, in the file's order: Vasquez's AM-24), modules with no MODULEDT included.
struct DeleteLinksRun
{
	bool on = false;
	std::vector<int32_t> modules;   // object numbers, in record order
	std::vector<int32_t> records;   // each one's record number (the counter's number)
	size_t at = 0;
	size_t done = 0;
	size_t withLinks = 0;
};

static DeleteLinksRun deleteLinksRun;

bool DeleteLinksRunning()
{
	return deleteLinksRun.on;
}

// THE VIEW GAP (Bishop 2026-11-16 for Melanikus: "on every screen entry the game view must load OUR working bytes, never the opened file"). inteng's entries LOAD THE LEVEL AGAIN from the RIF it
// holds in memory: its loader FUN_0046e700 (each object's MAPBLOCK read, autolink-meet-vasquez.md AM-23) at the hub's entry FUN_00447550 (flags 0x82, mode-entry-exit-frost.md HUB-E1 / TM-H1)
// and at the modes' (Strategy Data's 0x93), so what a mode changed is in the next screen's picture. NAMED: ours loads only when the bytes changed since the view's last load (the same picture,
// without the wait). Ours draws through the game's own renderer, which was loaded from the opened file once (LevelView_Open): an edit changes the editor's bytes, and
// only some edits the game's live blocks too (a flown object is a copy, let go of when its mode is left, and the object is then drawn where it was opened). So at a screen change, when the level
// changed since the view was loaded, the working bytes are written to the editor's own copy <data folder>\avp_rifs\OpenedByEditor\<name>.rif (open_with.h: only the editor puts files there;
// LevelSave_ViewCopy: the bytes the next save would write, a temporary file, read back, then put in place, no .bak; NOT a save, so the open level keeps its changes and stays paired, where a
// SaveAs took the copy for the level's file and lost every edit after it) and the view loads that copy (LevelView_ShowFile). LevelView_FilePath still names the opened file, so Save's target and
// its .bak are the same. Not while AutoLink's worker or Delete Links runs (they read the game's blocks), nor with Hierarchy's model or the Shape Chooser's preview shown (blocks of their own in the
// game's memory), nor for a model file (no module: Hierarchy's own view): the next screen change tries again. Doors Light Sourcing closed stay closed; the toggles are given to the game again.
unsigned viewChanges = ~0u;      // the level's Changes() the game's view was loaded at: the opening's, then each ShowTheWorkingBytes' (Generation() missed a same-size edit: Drake, 11-17)

void ShowTheWorkingBytes()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || viewChanges == ~0u || open->Changes() == viewChanges || autoLinkRun.Running() || DeleteLinksRunning() || chooserActive || HierarchyView_IsShown() ||
		!theGameFolder[0] || openLevelName.empty())
	{
		return;
	}

	bool anyModule = false;

	for (int32_t m = 0; m < LevelView_ModuleCount() && !anyModule; m++)
	{
		std::string moduleName;

		anyModule = LevelView_ModuleName(m, &moduleName);
	}

	if (!anyModule)
	{
		return;
	}

	size_t const slash = openLevelName.find_last_of("/\\");
	std::string const stem = slash == std::string::npos ? openLevelName : openLevelName.substr(slash + 1);
	std::string const folder = std::string(theGameFolder) + (theGameFolder[strlen(theGameFolder) - 1] == '\\' ? "" : "\\") + "avp_rifs\\OpenedByEditor";
	std::string const path = folder + "\\" + stem + ".rif";
	std::string why, words;
	char checked[MAX_PATH];

	CreateDirectoryA(folder.c_str(), NULL);

	char const *const notUsable = SaveFolder_WhyNotUsable(folder.c_str(), checked, sizeof(checked));

	if (notUsable)
	{
		words = std::string("view: the level as edited is NOT shown: the editor's own OpenedByEditor folder can't be used (") + notUsable + "): " + folder;
	}
	else if (host::LevelSave_ViewCopy(open, path.c_str(), &why) != host::LevelSaveOver::Saved)
	{
		words = "view: the level as edited is NOT shown: " + why;
	}
	else
	{
		bool const doorsClosed = LevelView_DoorsAreClosed();
		char said[400] = "";
		bool const shown = LevelView_ShowFile(("OpenedByEditor/" + stem).c_str(), said, sizeof(said));

		if (doorsClosed)
		{
			LevelView_SetDoorsClosed(true);
		}

		ViewToggles_LevelOpenedAgain();
		viewChanges = shown ? open->Changes() : viewChanges;
		words = shown ? "view: the level as edited is shown (" + path + ")" : "view: the level as edited is NOT shown, the opened file is: " + std::string(said);
	}

	LogString(words);

	if (NobodyWatching())
	{
		printf("AVP_OpenInteng: %s\n", words.c_str());
		fflush(stdout);
	}
}

void DeleteModuleLinks(bool all)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		Answer("No level is held to work on");
		return;
	}

	if (deleteLinksRun.on || AutoLinkRunning())
	{
		Answer("Delete Links: a run is on (right click to cancel)");
		return;
	}

	std::vector<int32_t> picked;

	if (!all)
	{
		picked = PickedModules();   // 0xa7 works on a COPY of the picked list (0x4c32d6-0x4c3364): the picks are kept

		if (picked.empty())
		{
			Answer("Delete Links: pick modules first (click, Shift+click for more)");
			return;
		}
	}

	// the records: the level's modules with a shape, in the file's order (AutoLinkScene's list, the order Vasquez's AutoLink_Records traced: AM-24)
	host::AutoLinkScene scene;

	host::AutoLinkScene_Build(open, local_scale > 0.0 ? local_scale : 1.0, &scene);

	DeleteLinksRun run;

	for (size_t record = 0; record < scene.list.size(); record++)
	{
		int32_t const object = scene.list[record].number;

		if (all || std::find(picked.begin(), picked.end(), object) != picked.end())
		{
			run.modules.push_back(object);
			run.records.push_back((int32_t)record);
		}
	}

	run.on = true;
	deleteLinksRun = run;
	AutoLinkShow_Begin(host::kAutoLinkWordsY);   // "Right click to cancel" at (200, 0xdc), 0x4c8b05
	Answer("Delete Links: running (right click to cancel)");
}

// One module of the run a frame: the right press first (0x4c8b2a), then FUN_004c10c0(i, 1, 0): the old current written, i's links destroyed, an empty tree marked changed, the counter
void DeleteLinksFrame()
{
	DeleteLinksRun &run = deleteLinksRun;

	if (!run.on)
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	bool const cancelled = AutoLinkShow_Cancelled();
	std::string stopped;

	if (!cancelled && run.at < run.modules.size() && open)
	{
		int32_t const module = run.modules[run.at];
		int32_t const record = run.records[run.at];
		uint8_t id[host::kSelectionIdBytes];

		run.at++;

		if (!WriteTheLinks())
		{
			stopped = "the current module's links could not be written";
		}
		else if (host::ModuleIdOfIndex(LevelObjects_Level(), module, id))
		{
			size_t removed = 0;
			host::ModuleDataResult const result = host::ModuleData_DeleteLinks(open, id, &removed);

			if (result != host::ModuleDataResult::Done && result != host::ModuleDataResult::NothingToDelete)
			{
				stopped = host::ModuleDataWords(result);
			}
			else
			{
				run.withLinks += removed > 0 ? 1 : 0;
				linkTree.reset(new host::LinkTree(module));
				linkTree->MarkChanged();
				run.done++;
				AutoLinkShow_Counter(record);   // "%03d" at (230, 180), 0x4c8b9e
			}
		}

		if (stopped.empty() && run.at < run.modules.size())
		{
			return;
		}
	}

	// the end (0x4c8bbf): the level FILE written (FUN_004719e0); the last module done stays current
	run.on = false;
	AutoLinkShow_End();
	LoadTheObjectListAgain();

	std::string const saved = SaveAtIntengsPoint();

	Answer("Delete Links: " + std::to_string(run.done) + " module" + (run.done == 1 ? "" : "s") + ", " + std::to_string(run.withLinks) + " had links" +
		(cancelled ? ", cancelled by a right press" : std::string()) + (stopped.empty() ? std::string() : " (stopped: " + stopped + ")") + "; " + saved);
}

// Normal Link (0x32), Branch Link (0x34), Delete Link (0x36) and Close Branch (0x40): each works on the picked modules, and all of them are
// let go of; nothing happens with no current module or no picks (inteng's `DAT_00704374 == 0 || DAT_00704324 == 0`).
// The IDs (8 bytes each, in pick order) of the picked modules: the selection set's members that are modules
std::vector<uint8_t> PickedModuleIdBytes()
{
	std::vector<uint8_t> ids;

	for (size_t i = 0; i < host::Selection_SetCount(); i++)
	{
		uint8_t id[host::kSelectionIdBytes];
		int32_t index = 0;

		if (host::Selection_SetMember(i, id) && host::ModuleIndexOfId(LevelObjects_Level(), id, &index))
		{
			ids.insert(ids.end(), id, id + host::kSelectionIdBytes);
		}
	}

	// A plain click on a module picks it by the GAME's module number (the selection's kind Module), which the set above never holds: its object's ID joins the list, as PickedModules does (found by the
	// Path scenario: Create Path, a click on a module, Add Module said "pick a module first" every time)
	host::Selected const clicked = host::Selection_Get();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (clicked.kind == host::SelectedKind::Module && open && open->Level())
	{
		uint8_t id[host::kSelectionIdBytes];
		int32_t index = -1;
		bool known = false;

		if (ModuleObject(open, clicked.moduleIndex, id) && host::ModuleIndexOfId(open->Level(), id, &index))
		{
			for (size_t at = 0; at + host::kSelectionIdBytes <= ids.size(); at += host::kSelectionIdBytes)
			{
				known = known || memcmp(ids.data() + at, id, host::kSelectionIdBytes) == 0;
			}

			if (!known)
			{
				ids.insert(ids.begin(), id, id + host::kSelectionIdBytes);
			}
		}
	}

	return ids;
}

// Strategy mode's S + click (FUN_00525700 0x525db2-0x525f78, the Movement help's "S: select by texture"): with S down a click takes the texture of the polygon under it (FUN_0042f620 of the polygon the pick
// found; a polygon with no texture, FUN_0042bce0, ends it) and then ADDS to the selection every module that is not hidden (the object flag 0x400 skips it) and whose shape uses that texture for MORE than half its
// polygons (the count is over the polygon list, the test is count > number / 2 in whole numbers); a module already picked stays (the list is searched first), and nothing is let go of. NAMED: a shape
// with its own picture list (an imported one, 16 of Invasion's 381) numbers its pictures its own way and is counted by the number in its file here (inteng counts the game's loaded number); the selection holds at most 256 (the editor's set).
size_t strategyTextureCount = 0;   // how many modules the last S + click added (the hidden run reads it)

