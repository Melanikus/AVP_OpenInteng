// construction_modes_screen.cpp: CONSTRUCTION'S MODES: Delete Module's Data and Merge Objects, the Shape Chooser, Adjacent Modules (Link, Base, Find Not Done, Recalculate Entry Points), the Entry Point Markers and their Fly. Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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

// ---- CONSTRUCTION, OBJECT: Delete Module's Data and Merge Objects (P11 hosts by Vasquez, wired by Melanikus (Drake) 2026-11-15) ----
// Both work on inteng's list of selected objects (DAT_00631230; here the selection set, in pick order).
//
// Delete Module's Data (FUN_004452b0 case 0x50, menu-reads-1-full.txt:6125-6140): when something is selected, every VMDARRAY inside the MODULEDT of
// the selected object is destroyed and the level is marked changed. inteng asks nothing and says nothing; the words here are ours. NAMED: the
// decompile does not show which object it takes when several are selected (the register argument of FUN_004bd890 is dropped), so with more than one
// selected it is refused (drake-deviations.md).
void DeleteTheModuleData()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[host::kSelectionIdBytes];
	size_t const count = host::Selection_SetCount();

	if (!open)
	{
		Answer("No level is held to work on");
		return;
	}

	if (count == 0 || !host::Selection_SetMember(0, id))
	{
		Answer("Delete Module's Data: select an object first");
		return;
	}

	if (count > 1)
	{
		Answer("Delete Module's Data: select just one object (with several selected, which one inteng takes is not shown by its code)");
		return;
	}

	size_t removed = 0;
	host::ModuleDataResult const result = host::ModuleData_DeleteLinks(open, id, &removed);

	if (result != host::ModuleDataResult::Done)
	{
		Answer(std::string("Delete Module's Data: ") + host::ModuleDataWords(result));
		return;
	}

	LoadTheObjectListAgain();
	Answer("Module's data deleted: " + std::to_string(removed) + " adjacency array" + (removed == 1 ? "" : "s") + " (kept in the editor's bytes until Save a copy)");
}

// Merge Objects (FUN_0044a950, menu-reads-5.txt, called from FUN_004452b0 0x445d03): the FIRST selected object's shape takes in each other selected
// object's shape in turn (FUN_00449cf0), and each of those is removed (FUN_0044d160); the selection is empty afterwards. inteng refuses first,
// with its message "Cannot merge objects with morphing ...", when any selected object has a SHPMORPH (the host refuses those too). The host is Vasquez's
// exact SUBSET (shape_merge_edit.h); Melanikus's ruling: it REFUSES when any other object's chunk names the second object (a named deviation,
// carded). With nothing selected inteng's code asserts; with one selected it merges nothing: both are said in words here.
// THE QUESTION FIRST (TB-C1, construction-closeout-frost.md): with more than one object selected inteng asks "Do you want to merge the selected objects?" (FUN_00474a40 at 0x445ce9,
// the words at 0x5cf964, shown by FUN_0057c800(200, 100)); only Yes (3) merges (0x445cfe -> FUN_0044a950), No does nothing.
static void MergeTheSelectedObjectsNow();

void MergeTheSelectedObjects()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t const count = host::Selection_SetCount();

	if (!open)
	{
		Answer("No level is held to work on");
		return;
	}

	if (count < 2)
	{
		Answer("Merge Objects: select the objects to merge (Shift+click for more): the first one picked keeps the merged shape");
		return;
	}

	DialogConfirm("Do you want to merge the selected objects?", [](bool yes)
	{
		if (yes)
		{
			MergeTheSelectedObjectsNow();
		}
		else
		{
			Answer("Merge Objects: nothing merged");
		}
	});
}

static void MergeTheSelectedObjectsNow()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t const count = host::Selection_SetCount();

	if (!open || count < 2)
	{
		Answer("Merge Objects: select the objects to merge (Shift+click for more): the first one picked keeps the merged shape");
		return;
	}

	std::vector<std::vector<uint8_t>> ids;

	for (size_t i = 0; i < count; i++)
	{
		std::vector<uint8_t> id(host::kSelectionIdBytes);

		if (!host::Selection_SetMember(i, id.data()))
		{
			Answer("Merge Objects: the selection changed, try again");
			return;
		}

		ids.push_back(id);
	}

	// TB-C8i: every selected object's shape is looked at for morphing data BEFORE anything is merged (FUN_0044a950 0x44a978-0x44aa5c); with any, inteng says
	// "Cannot merge objects with morphing data" (FUN_00474030, the words at 0x5cfbf0, shown by FUN_0057c800(100, 100)) and merges nothing
	for (std::vector<uint8_t> const &id : ids)
	{
		if (host::ShapeEdit_HasMorph(open->Level(), id.data()))
		{
			DialogMessage("Cannot merge objects with morphing data");
			return;
		}
	}

	size_t merged = 0;

	for (size_t i = 1; i < ids.size(); i++)
	{
		std::string why;
		host::ShapeMergeEditResult const result = host::ShapeMergeEdit_Objects(open, ids[0].data(), ids[i].data(), &why);

		if (result != host::ShapeMergeEditResult::Done)
		{
			std::string words = std::string("Merge Objects: ") + host::ShapeMergeEditWords(result);

			if (!why.empty())
			{
				words += " (" + why + ")";
			}

			if (merged > 0)
			{
				words += "; " + std::to_string(merged) + " merged before it";
				host::Selection_Clear();   // the merged ones are gone from the level
				LoadTheObjectListAgain();
				words += "; " + SaveAtIntengsPoint();   // TB-C8j: what was merged is written, as inteng's merge always ends (0x44aba2; inteng refuses no pair)
			}

			Answer(words);
			return;
		}

		merged++;
	}

	host::Selection_Clear();
	LoadTheObjectListAgain();

	// TB-C8j: inteng's merge ends by marking the level changed (DAT_00631264 = 1, 0x44ab98) and leaving the hub, which WRITES THE LEVEL FILE (FUN_004476d0 at 0x44aba2 ->
	// FUN_004719e0), then entering it again (FUN_00447550, 0x44aba7). Before, ours kept the merge in the editor's bytes until a save.
	std::string const saved = SaveAtIntengsPoint();

	Answer("Merged " + std::to_string(merged) + " object" + (merged == 1 ? "" : "s") + " into the first; " + saved);
	CameraPositionSaved(10);   // the merge ends by leaving the hub and entering it again: FUN_0044a950 0x44aba2 -> FUN_004476d0 (camera-saved-vasquez.md CS-2)
	RotationCentreDefault();   // and its set-up FUN_00447550 (0x44aba7) puts the rotation centre back on the objects' average (0x447652, CAM-7)
}

// ---- THE SHAPE CHOOSER (inteng's screen state 5, FUN_004dd910 enter / FUN_004ddb60 per frame; shape_chooser.h) ----
// Replace shape (0x1d) opens it for the selected placed object: one shape at a time ("Shape '%s'", "Polys %d, Verts %d"), Previous / Next through the list, S for a list pick,
// Return (Exit with shape: the object is given the shape, FUN_004e2e10) and Escape (Exit without shape). The screen's menu (table 0x5e78c8) has the same two exits. OUR LOOK:
// text lines over the level, no 3D preview of the shape (inteng's FUN_0053c4f0 draws it turning), no Show items (All Shapes / Sprites / Dynamic Objects: not read). (Added
// 2026-11-15 by Melanikus (Drake).)
std::vector<host::ShapeChoice> chooserList;
size_t chooserAt = 0;
static uint8_t chooserObject[host::kSelectionIdBytes];
static std::string chooserWords;   // the status line's words (statusWords points at it)
static size_t chooserBack = 0;   // the screen it was opened from: it goes back there (inteng: FUN_004d68e0, the placement screen)
static bool chooserForCreate = false;   // opened by Create > With shape > Choose (a new object) and not by Replace shape (the selected object)
static std::string createShapeName;     // the shape Create > Choose took, while the name is being asked

static size_t ScreenOfTable(const char *table, size_t otherwise)
{
	for (size_t i = 0; i < host::kIntengScreenCount; i++)
	{
		if (strcmp(host::kIntengScreens[i].table, table) == 0)
		{
			return i;
		}
	}

	return otherwise;
}

// THE SCREEN (FUN_004ddb60 every frame): the shape alone in the view (FUN_004dd870 / FUN_004dd6f0 / FUN_0053c4f0: the game's renderer, the identity turn, the camera 2 x the shape's maxx behind it), then FUN_0057b1b0
// writes "S : Select shape from list" at (200, height - 0x3c), "Return : Exit with shape" at (200, height - 0x28), "Escape : Exit without shape" at (200, height - 0x14), "Polys %d, Verts %d" at (10, 0x28) and,
// when the shape names an outside file, "Shape '%s'" at (10, 0x38). Nothing else is on the screen (no status line). Keys: , previous and . next (they wrap), S the list dialog "Select shape" (FUN_004d49e0:
// FUN_00475bf0 then FUN_0057c800), Return exit with the shape, Escape exit without.
static FlyCamera chooserSavedCamera;
static int32_t chooserLastNumber = -1;
int32_t chooserCameraZ = 0;

static void ShapeChooserShow()
{
	chooserText.clear();

	if (!chooserActive || chooserList.empty())
	{
		return;
	}

	host::ShapeChoice const &now = chooserList[chooserAt];

	// The shape the text names, by its place in the game's list (found by the name the game loaded it by). now.number is the FILE's id for it, another numbering, and drew another
	// shape (Drake 11-17: "Shape 'Glass01'" over a room piece). A shape the game's list does not hold draws nothing.
	if (!ShapePreview_Show(ShapePreview_PlaceOf(now.gameName.c_str()), &chooserCameraZ))
	{
		chooserCameraZ = -4000;
	}

	LevelView_HideModules(nullptr, 0, true);   // the shape is alone: no module, no placed object
	LevelView_HideObjects(nullptr, 0, true);
}

// Leaves the chooser. With the shape: the selected object is given the chosen shape (host::ShapeReplace_ByNumber, FUN_004e2e10). screen: where the new screen number goes (when
// a menu pick asked), null for a key (the screen is changed here).
static void ShapeChooserLeave(bool withShape, size_t *screen)
{
	std::string words = "Shape Chooser: left without a shape";
	bool askName = false;

	if (withShape && chooserActive && !chooserList.empty() && chooserForCreate)
	{
		// Create > With shape > Choose (FUN_004d9af0 0x4d9450-0x4d97be): the shape is chosen, then the NAME is asked
		createShapeName = chooserList[chooserAt].name;
		askName = true;
		words = "Create: type a name for the new '" + createShapeName + "'";
	}
	else if (withShape && chooserActive && !chooserList.empty())
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		host::ShapeChoice const chosen = chooserList[chooserAt];
		host::ShapeReplaceResult const result = open ? host::ShapeReplace_ByNumber(open, chooserObject, chosen.number) : host::ShapeReplaceResult::BadCall;

		if (result == host::ShapeReplaceResult::Done)
		{
			LoadTheObjectListAgain();
			words = "Shape replaced by '" + chosen.name + "' (kept in the editor's bytes until Save a copy)";
		}
		else
		{
			words = std::string("Replace shape: ") + host::ShapeReplaceWords(result);
		}
	}

	if (chooserActive && !chooserList.empty() && chooserAt < chooserList.size())
	{
		chooserLastNumber = chooserList[chooserAt].number;
	}

	ShapePreview_Close();
	LevelView_HideModules(nullptr, 0, false);
	LevelView_HideObjects(nullptr, 0, false);

	if (theCamera && chooserActive)
	{
		*theCamera = chooserSavedCamera;   // the new object is made at the VIEW'S place, the one it had before the screen
		FlyCamera_Hold(theCamera);
	}

	chooserActive = false;
	chooserText.clear();
	chooserList.clear();

	size_t const back = chooserBack < host::kIntengScreenCount ? chooserBack : host::kIntengHub;

	if (screen)
	{
		*screen = back;
	}
	else if (back != barScreen)
	{
		barScreen = back;

		if (!LoadTheMenus())
		{
			LogString("menus: the bar of screen " + std::to_string(barScreen) + " couldn't be made (editor_bar.cpp), so the editor closed");
			running = false;
			bRunning = false;
		}
	}

	chooserWords = words;
	statusWords = chooserWords.c_str();
	chooserForCreate = false;

	if (askName)
	{
		OpenAPrompt(kPromptCreateName);
	}
}

// what: kChooserStart, kChooserStartCreate, kChooserWith or kChooserWithout. screen: the frame's screen number, changed here when the screen changes.
void ShapeChooserCommand(int32_t what, size_t *screen)
{
	if (what == kChooserStart || what == kChooserStartCreate)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		uint8_t id[host::kSelectionIdBytes] = {};
		bool const forCreate = what == kChooserStartCreate;
		char const *const who = forCreate ? "Create" : "Replace shape";

		if (!open || !LevelObjects_Level() || (!forCreate && !SelectedObjectId(id)))
		{
			Answer(forCreate ? "Create: no level is held to work on" : "Replace shape: select a placed object first");
			return;
		}

		if (!forCreate && !host::ShapeChooser_ObjectIsPlaced(LevelObjects_Level(), open, id))
		{
			Answer("Replace shape: only a placed object can be given another shape");
			return;
		}

		chooserForCreate = forCreate;

		if (host::ShapeChooser_List(LevelObjects_Level(), open, &chooserList) == 0)
		{
			chooserForCreate = false;
			Answer(std::string(who) + ": the level holds no outside shapes to choose from");
			return;
		}

		// FUN_004dd910 0x4dda55: the current shape is the list iterator DAT_008f4e90, which is KEPT between uses (it is moved to the list's first only when it stands on the list's end): the screen opens on the shape last
		// looked at, the list's first the first time
		chooserAt = 0;

		for (size_t i = 0; i < chooserList.size(); i++)
		{
			if (chooserList[i].number == chooserLastNumber)
			{
				chooserAt = i;
				break;
			}
		}

		memcpy(chooserObject, id, sizeof(chooserObject));
		chooserBack = *screen;

		if (theCamera)
		{
			chooserSavedCamera = *theCamera;
		}

		CameraPositionSaved(10);   // FUN_004dd910 0x4dd913: FUN_004704d0(10) first (CS-4)

		// FUN_004dd910 0x4dd9b5: no current object (DAT_008f4f3c = 0). INTENG BUG, FIXED (OP-5a, MAPPING-ORDERS section 15): inteng sets it to 0 without the unselect (FUN_004d64a0 / the object's +0x2c),
		// so the old object stays drawn red as selected with nothing current; ours lets it go as Backspace does (OP-82). A flight stops fully as the screen is left (PlacementFlyStop: inteng's half stop at
		// 0x4dd98d-0x4dd9ad, which leaves the arrows moving the hidden object, is the other bug fixed), and the split is taken down (inteng: DAT_008f4f60 = 0 without FUN_004d5d80).
		host::Selection_Clear();
		specialMode.haveCurrent = false;
		placedHierarchyCurrent = -1;
		chooserActive = true;
		*screen = ScreenOfTable("0x5e78c8", *screen);
		memset(keyDown, 0, sizeof(keyDown));
		ShapeChooserShow();
		return;
	}

	if (!chooserActive)
	{
		Answer("Shape Chooser: nothing is being chosen (Replace shape and Create > With shape > Choose in Object Placement open it)");
		return;
	}

	ShapeChooserLeave(what == kChooserWith, screen);
}

bool ShapeChooserKey(WPARAM key)
{
	if (key == VK_RETURN)
	{
		ShapeChooserLeave(true, nullptr);
	}
	else if (key == VK_ESCAPE)
	{
		ShapeChooserLeave(false, nullptr);
	}
	else if ((key == VK_OEM_COMMA || key == VK_OEM_PERIOD) && !chooserList.empty())
	{
		// the key flags DAT_00d87afc / DAT_00d87afe are the virtual keys 0xbc "," and 0xbe "." (the key-down handler FUN_0058c010 stores a key as DAT_00d87a40 + its code): previous and next, wrapping past the list's ends
		chooserAt = key == VK_OEM_PERIOD ? (chooserAt + 1) % chooserList.size() : (chooserAt + chooserList.size() - 1) % chooserList.size();
		ShapeChooserShow();
	}
	else if (key == 'S' && !chooserList.empty())
	{
		// DAT_00d87a93 is 'S' (0x53): FUN_004d49e0, the list dialog "Select shape" over the names; a pick makes that shape the current one
		std::vector<std::string> names;

		for (host::ShapeChoice const &one : chooserList)
		{
			names.push_back(one.name);
		}

		DialogList("Select shape", names, [](int32_t place)
		{
			if (place >= 0 && (size_t)place < chooserList.size())
			{
				chooserAt = (size_t)place;
				ShapeChooserShow();
			}
		});
	}

	return true;   // every key belongs to the chooser while it is open
}

// Create > With shape > Choose, after the shape: the typed name is asked for ("Name Object"), a name already used by any object is refused with "The name of the object has already been
// used." and asked again (FUN_004d9af0 0x4d9450-0x4d97be, p11-shape-handlers.md section 25); a new placed object of that shape is then made at the VIEW's place with the view's turn
// (host::CreateObjectFromShape, Frost's maker, the one Quick-create uses) and the selection is let go of. NAMED: the maker names the object itself and it is then renamed to the typed name
// (ObjectEdit_Rename, whose rules are 1 to 63 plain letters); the object number is the maker's (greatest + 1), not inteng's counter DAT_00636f4c.
bool ObjectNameInUse(std::string const &name)
{
	return host::ObjectNameInUse(LevelObjects_Level(), name.c_str());
}

void NameInUseBox(std::string const &typed, std::function<void(std::string const &)> const &named);
void NameObjectBox(std::string const &start, std::function<void(std::string const &)> const &named);

void CreateNamedAtTheView(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t matrix[9];

	if (typed.empty())
	{
		Answer("Create: no object was made (no name typed)");
		return;
	}

	if (!open || !theCamera || !LevelView_CameraMatrix(matrix))
	{
		Answer("Create: no level is open, or no frame has been drawn yet");
		return;
	}

	if (ObjectNameInUse(typed))
	{
		NameInUseBox(typed, CreateNamedAtTheView);   // FUN_004d4e00: the box "The name of the object has already been used." with Re-enter and Cancel
		return;
	}

	int32_t place[3];
	uint8_t newId[host::kSelectionIdBytes] = {};
	std::string autoName;

	ViewFilePlace(place);

	host::ObjectMakeResult const result = host::CreateObjectFromShape(open, createShapeName.c_str(), place, matrix, newId, &autoName);

	if (result != host::ObjectMakeResult::Done)
	{
		quickCreateWords = createShapeName + ": " + host::ObjectMakeWords(result);
		statusWords = quickCreateWords.c_str();
		return;
	}

	LoadTheObjectListAgain();

	std::string why;
	uint8_t renamedId[host::kSelectionIdBytes] = {};
	host::ObjectEditAnswer const renamed = host::ObjectEdit_Rename(open, LevelObjects_Level(), newId, typed.c_str(), renamedId, &why);

	LoadTheObjectListAgain();
	MakePlacedObjectCurrent(renamed == host::ObjectEditAnswer::Done ? renamedId : newId);   // FUN_004d64a0 (0x4d97aa): the new object is THE current one (OP-5)

	if (renamed == host::ObjectEditAnswer::Done)
	{
		quickCreateWords = "Object " + typed + " made at the view";
	}
	else
	{
		quickCreateWords = "Object " + autoName + " made at the view; the name \"" + typed + "\" was refused: " + why;
	}

	statusWords = quickCreateWords.c_str();
}

// "Select shape from list" (FUN_004d49e0): by position (1 to the count) or by part of a name
void ShapeChooserPick(std::string const &typed)
{
	char *end = nullptr;
	errno = 0;

	long long const number = _strtoi64(typed.c_str(), &end, 10);

	if (!typed.empty() && end != typed.c_str() && *end == '\0' && errno == 0)
	{
		if (number >= 1 && (unsigned long long)number <= chooserList.size())
		{
			chooserAt = (size_t)number - 1;
			ShapeChooserShow();
		}
		else
		{
			Answer("Select shape: a number from 1 to " + std::to_string(chooserList.size()));
		}

		return;
	}

	std::string wanted = typed;

	for (char &c : wanted)
	{
		c = (char)tolower((unsigned char)c);
	}

	for (size_t i = 0; i < chooserList.size(); i++)
	{
		std::string name = chooserList[i].name;

		for (char &c : name)
		{
			c = (char)tolower((unsigned char)c);
		}

		if (!wanted.empty() && name.find(wanted) != std::string::npos)
		{
			chooserAt = i;
			ShapeChooserShow();
			return;
		}
	}

	Answer("Select shape: no shape's name has \"" + typed + "\" in it");
}

// ---- ADJACENT MODULES: Link > Add, Link > Delete, Base > Set Current, Mode > Exit, Base > Show All, Find Not Done (inteng's FUN_00404a20 0x32 / 0x34 / 100 / 10 / 0x66 / 0x68, FUN_004025d0) ----
// The rules are host::AdjacentList's (adjacent_list.h); the write-back (FUN_00404040) is adjacent_write.h's. The module picked is the first of the picked modules (the selection set, as Module Linking's
// are). The list is put into the current module's ADJMDLEP when another module is made current, at Find Not Done and at Exit (the three places inteng calls FUN_00404040), only when it was changed;
// then its entries are worked out with the "modules don't touch" questions and the level file is written (0x404511; AD-6a, WriteTheAdjacentListThen). (Added 2026-11-15 by Melanikus (Drake).)
std::unique_ptr<host::AdjacentList> adjacentList;
bool adjacentShowAllPaint = false;   // showAllModules / showAllColours hold the Adjacent Modules screen's Show All (blue: has an ADJMDLEP, white: none)
static std::string adjacentWriteWords;      // what the last write-back did, for the answers
bool adjacentExitHeld = false;              // Exit's write-back is asking its questions: RunAction keeps the screen until the last answer (AD-6a)
static std::string adjacentExitWords;       // what that write-back did, for the words of the Exit run again after the answers

// FUN_00404040: the list of the current module goes into the level when it was changed, then (0x404361-0x4044a6) each of its entries is worked out again, asking "Warning adjacent
// modules don't touch" for a pair that doesn't touch (StartEntryRecalc kept to the module; a Yes takes that entry out), then the level file is written (FUN_004719e0 at 0x404511, on the
// empty list's way too). then(true) when there is nothing more to do: at once when nothing was to be written or nothing asked, after the last answer otherwise; then(false) when the list
// could not be written (the answer says why and the list stays, so a later Set Current / Exit tries again). INTENG BUG, FIXED (NAMED, adjacent_write.h): only the entry answered Yes
// goes, not every later one. (The questions and the save added 2026-11-17 by Melanikus (Dietrich), AD-6a of reassess/adjacent-closeout-dietrich.md; until then nothing was asked and
// the data was kept in the editor's bytes until Save a copy.)
void WriteTheAdjacentListThen(std::function<void(bool)> const &then, bool keepChanged)
{
	adjacentWriteWords.clear();

	if (!adjacentList || adjacentList->Current() < 0 || !adjacentList->Changed())
	{
		then(true);
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		Answer("No level is held to work on");
		then(false);
		return;
	}

	int32_t const current = adjacentList->Current();
	host::AdjacentWriteCounts counts;
	host::AdjacentWriteResult const result = host::AdjacentWriteBack(open, current, adjacentList->Members(), &counts, false);

	if (result != host::AdjacentWriteResult::Done)
	{
		Answer(std::string("Adjacent modules of ") + ModuleNameOfIndex(current) + " not written: " + host::AdjacentWriteWords(result));
		then(false);
		return;
	}

	// FUN_00404040 never clears DAT_0060677c (only Set Current 0x4026ab / 0x4026cc and the Find Not Done row 0x4052e4 do): after Find Not Done's write the list stays changed
	// (keepChanged), so a later Exit / Set Current / Find Not Done writes it again and asks again (AD-6a-2)
	if (!keepChanged)
	{
		adjacentList->MarkWritten();
	}

	if (counts.removed)
	{
		// the empty list (0x4040b8 -> 0x4044ed): the chunk taken out, no entry to work out, then the same save (0x404511)
		std::string const saved = SaveAtIntengsPoint();

		adjacentWriteWords = "the module's adjacent-module chunk was taken out (the list is empty); " + saved;
		then(true);
		return;
	}

	std::string const made = std::to_string(counts.entries) + " adjacent modules written (" + std::to_string(counts.kept) + " kept as they were, " + std::to_string(counts.made) + " new)";
	bool const started = StartEntryRecalc("Adjacent modules of " + ModuleNameOfIndex(current), [made, then](std::string const &words)
	{
		const std::string saved = SaveAtIntengsPoint();   // const first: Snyk Code's parser skips the whole file on "std::string const" in a lambda given to a call

		adjacentWriteWords = made + "; " + words + "; " + saved;
		then(true);
	}, current);

	if (!started)
	{
		adjacentWriteWords = made + "; the entry points were not worked out; " + SaveAtIntengsPoint();
		then(true);
	}
}

// Show All (0x66, FUN_00404a20 0x4c: every module with a record is painted, blue when its MODULEDT holds an ADJMDLEP and white when it does not; read as the chunk's presence). It stays until the next
// Add / Delete / Set Current, whose repaint (FUN_004046b0) paints everything white and then the current module and its list again.
static void AdjacentShowAll()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		Answer("Show All: no level is open");
		return;
	}

	std::vector<host::ModuleSnapshot> snapshots;
	std::map<int32_t, int32_t> numbers;

	if (host::ReadModuleSnapshots(open, &snapshots) != host::ObjectEditResult::Done)
	{
		Answer("Show All: the modules could not be read");
		return;
	}

	host::GameModuleNumbers(open->Level(), &numbers);
	showAllModules.clear();
	showAllColours.clear();

	size_t with = 0;
	size_t without = 0;

	for (host::ModuleSnapshot const &one : snapshots)
	{
		auto const found = numbers.find(one.index);

		if (!one.module || found == numbers.end() || showAllModules.size() >= kLevelViewMostSelected)
		{
			continue;
		}

		showAllModules.push_back(found->second);
		showAllColours.push_back(one.hasAdjacency ? 0xFF0000FFu : 0xFFFFFFFFu);
		with += one.hasAdjacency ? 1 : 0;
		without += one.hasAdjacency ? 0 : 1;
	}

	adjacentShowAllPaint = true;

	static std::string words;

	words = "Show All: " + std::to_string(with) + " modules have adjacent modules (blue), " + std::to_string(without) + " have none (white)";
	statusWords = words.c_str();
}

static void AdjacentLetGoOfShowAll()
{
	if (adjacentShowAllPaint)
	{
		showAllModules.clear();
		showAllColours.clear();
		adjacentShowAllPaint = false;
	}
}

// FUN_004045b0 + the current module's set-up (FUN_004025d0's second half; the Find Not Done row's 0x40522b-0x4052e4): the module's own list is loaded from the level (a module with no
// adjacency chunk has no list, and is still made the current one), the pick cleared, the list unchanged
static void MakeTheModuleCurrent(int32_t pick, std::string const &written)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		Answer("No level is held to work on");
		return;
	}

	host::LevelStore const store(open);   // made now: a write-back may have made a new image
	std::vector<int32_t> entries;

	host::AdjacentModulesOf(LevelObjects_Level(), &store, pick, &entries);
	adjacentList->SetCurrent(pick, entries);
	host::Selection_Clear();
	Answer("Current module: " + ModuleNameOfIndex(pick) + " (" + std::to_string(adjacentList->Members().size()) + " adjacent modules)" + (written.empty() ? std::string() : "; the one before: " + written));
}

// Find Not Done's row (FUN_00404a20 0x40522b-0x4052e4): the module picked in the list is made current WITHOUT writing the list of the one before (inteng's row does not go through
// FUN_004025d0: Find Not Done wrote it already, and the row clears the changed mark, 0x4052e4) (AD-6a-2)
void AdjacentMakeCurrentFromList()
{
	if (!adjacentList)
	{
		adjacentList.reset(new host::AdjacentList());
	}

	AdjacentLetGoOfShowAll();   // the row repaints (FUN_004046b0)

	std::vector<int32_t> const picked = PickedModules();

	if (!picked.empty())
	{
		MakeTheModuleCurrent(picked[0], std::string());
	}
}

void AdjacentEdit(int32_t what)
{
	if (!adjacentList)
	{
		adjacentList.reset(new host::AdjacentList());
	}

	if (what == kAdjShowAll)
	{
		AdjacentShowAll();
		return;
	}

	if (what == kAdjViewDistance)
	{
		adjacentViewDistanceOn = !adjacentViewDistanceOn;
		viewWords = adjacentViewDistanceOn ? "View Distance :" + std::to_string(viewDistance / 500000) + " (J and M change it)" : std::string("View distance: off");
		statusWords = viewWords.c_str();
		return;
	}

	if (what == kAdjExit)
	{
		// Exit (command 10): FUN_00404040, then FUN_004049d0 lets go of the screen's state, then the hub (FUN_00447550). The item has already gone to the hub. When the write-back asks its
		// questions (AD-6a), inteng is still on Adjacent Modules: adjacentExitHeld makes RunAction keep the screen, and after the last answer the Exit item is run again, now with nothing
		// left to write, so it leaves as an Exit with nothing to write does (its words kept in adjacentExitWords).
		AdjacentLetGoOfShowAll();

		std::shared_ptr<bool> const answered = std::make_shared<bool>(false), waiting = std::make_shared<bool>(false);

		adjacentExitHeld = false;
		WriteTheAdjacentListThen([answered, waiting](bool ok)
		{
			*answered = true;

			if (!ok)
			{
				return;
			}

			if (*waiting)
			{
				const int32_t exitItem = BarActionOfCommand(0xa);   // const first: Snyk Code's parser (a lambda given to a call)

				adjacentExitWords = adjacentWriteWords;
				adjacentExitHeld = false;

				if (exitItem)
				{
					RunAction(exitItem);
				}

				return;
			}

			const std::string done = adjacentWriteWords.empty() ? adjacentExitWords : adjacentWriteWords;

			adjacentExitWords.clear();
			adjacentList->Clear();
			adjacentBoo.clear();
			host::Selection_Clear();
			Answer(done.empty() ? std::string("Adjacent Modules left") : "Adjacent Modules left: " + done);
		});

		if (!*answered)
		{
			*waiting = true;
			adjacentExitHeld = true;
		}

		return;
	}

	AdjacentLetGoOfShowAll();   // Add, Delete and Set Current all repaint (FUN_004046b0)

	std::vector<int32_t> const picked = PickedModules();
	int32_t const pick = picked.empty() ? -1 : picked[0];

	if (what == kAdjSetCurrent)
	{
		if (pick < 0)
		{
			Answer("Set Current: pick a module first (click it)");
			return;
		}

		if (!LevelObjects_OpenForEdit())
		{
			Answer("No level is held to work on");
			return;
		}

		// FUN_004025d0 writes the list of the module that WAS current first (with its questions, AD-6a); the pick is made current once that is done
		WriteTheAdjacentListThen([pick](bool ok)
		{
			if (ok)
			{
				MakeTheModuleCurrent(pick, adjacentWriteWords);
			}
		});
		return;
	}

	if (what == kAdjAdd)
	{
		switch (adjacentList->Add(pick))
		{
			case host::AdjacentAdd::Added:
				host::Selection_Clear();
				Answer("Add: " + ModuleNameOfIndex(pick) + " is adjacent to " + ModuleNameOfIndex(adjacentList->Current()) + " now (written into the level at Set Current, Find Not Done or Exit)");
				break;
			case host::AdjacentAdd::NoCurrent:
				Answer("Add: make a module the current one first (Set Current)");
				break;
			case host::AdjacentAdd::NoPick:
				Answer("Add: pick a module first (click it)");
				break;
			case host::AdjacentAdd::IsCurrent:
				Answer("Add: that is the current module");
				break;
			case host::AdjacentAdd::AlreadyThere:
				Answer("Add: that module is on the list already");
				break;
		}

		return;
	}

	if (what == kAdjDelete)
	{
		if (adjacentList->Delete(pick) == host::AdjacentDelete::Deleted)
		{
			host::Selection_Clear();
			adjacentBoo = "BOO!";   // FUN_0057b1b0(DAT_0060674c, 10, 0x50, "BOO!") at 0x404cbc
			Answer("Delete: " + ModuleNameOfIndex(pick) + " is not adjacent any more (written into the level at Set Current, Find Not Done or Exit)");
		}
		else
		{
			Answer("Delete: pick a module that is on the list");
		}
	}
}

// A module made the current one (Set Current: FUN_004c1a90 0x4c2377-0x4c2959; the follow: FUN_004c96f0 0x4c975c-0x4c9766, FUN_004c10c0(i, 0, 1) then +0x18 = 0): the links of the one
// that was current are written first when it was changed, the module's are loaded from its VMDARRAY, and its tree ends with changed = 0 (a module only looked at is not written when it
// is left). The picks are not touched here. False when the old links could not be written (nothing then changes).
bool LinkMakeCurrent(int32_t module)
{
	if (!WriteTheLinks())
	{
		return false;
	}

	std::set<int32_t> modules;
	std::vector<host::LinkItem> items;

	host::ModuleIndexes(LevelObjects_Level(), &modules);
	linkTree.reset(new host::LinkTree(module));

	if (host::ReadModuleLinks(LevelObjects_Level(), module, &items))
	{
		linkTree->Load(items.data(), (int)items.size(), [&modules](int32_t index) { return modules.count(index) != 0; });
	}

	// changed = 0 (0x4c2959 / 0x4c9766, row ML-M-19). (Until 2026-11-16 this marked it changed, citing FUN_004c10c0 0x4c14df, which Set Current does not call.)
	linkTree->ClearChanged();
	return true;
}

// Set Current (command 0xc8): the first picked module becomes the current one (LinkMakeCurrent), and the picks are let go of. (The picks are object indexes, the same after the level is swapped.)
void LinkSetCurrent()
{
	// S / Set Current turns Shift+S's follow OFF first (DAT_00704384 = 0, FUN_004c1a90 0x4c2314-0x4c2322), with picks or without (row ML-M-19, Drake's review R2)
	LinkFollowOff();

	std::vector<int32_t> const picked = PickedModules();

	if (picked.empty())
	{
		Answer("Set Current: pick a module first (click it, Shift+click for more)");
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	unsigned const before = open ? open->Generation() : 0;

	if (!LinkMakeCurrent(picked[0]))
	{
		return;
	}

	// 0x4c2604-0x4c2634: the old current tree, changed, was written into the level, and then the level FILE (FUN_004719e0)
	std::string const saved = open && open->Generation() != before ? SaveAtIntengsPoint() : std::string();

	LinkPicksLetGo();
	Answer("Current module: " + ModuleNameOfIndex(picked[0]) + " (" + std::to_string(linkTree->Count()) + " links)" + (saved.empty() ? std::string() : "; " + saved));
}

// AutoLink > Add Reverse Links: Module (0x12c, FUN_004c86b0 with the picked modules) and Environment (0x12d, FUN_004c86b0(0): every module). The rules and what
// is named are in reverse_links.h; here the current module's links are written first (the first make-current does that) and the current module is made
// current again at the end (0x4c88e9). Then the level FILE is written (FUN_004719e0 at 0x4c8903; SaveAtIntengsPoint).
void MakeTheCurrentModuleCurrentAgain(int32_t current)
{
	// the module that was current is made current again: its tree is loaded from what was written
	linkTree.reset();

	if (current >= 0)
	{
		std::set<int32_t> modules;
		std::vector<host::LinkItem> items;

		host::ModuleIndexes(LevelObjects_Level(), &modules);
		linkTree.reset(new host::LinkTree(current));

		if (host::ReadModuleLinks(LevelObjects_Level(), current, &items))
		{
			linkTree->Load(items.data(), (int)items.size(), [&modules](int32_t index) { return modules.count(index) != 0; });
		}

		linkTree->MarkChanged();
	}
}

void AddReverseLinksCommand(bool all)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		Answer("No level is held to work on");
		return;
	}

	std::vector<int32_t> picked;

	if (!all)
	{
		picked = PickedModules();

		if (picked.empty())
		{
			Answer("Add Reverse Links: pick modules first (click, Shift+click for more)");
			return;
		}
	}

	int32_t const current = linkTree ? linkTree->RootModule() : -1;

	if (!WriteTheLinks())
	{
		return;
	}

	host::ReverseLinksCounts counts;
	std::string why;
	host::ReverseLinksResult const result = host::AddReverseLinks(open, all ? nullptr : &picked, &counts, &why);

	if (result != host::ReverseLinksResult::Done)
	{
		Answer(std::string("Add Reverse Links: ") + host::ReverseLinksWords(result) + (why.empty() ? std::string() : " (" + why + ")"));
		LoadTheObjectListAgain();
		return;
	}

	MakeTheCurrentModuleCurrentAgain(current);
	LoadTheObjectListAgain();

	std::string const saved = SaveAtIntengsPoint();   // FUN_004c86b0's end, 0x4c8903: the level FILE written

	Answer("Add Reverse Links: " + std::to_string(counts.walked) + " modules walked, " + std::to_string(counts.linksAdded) + " links added in " + std::to_string(counts.changed) + " modules" +
		(counts.skipped > 0 ? ", " + std::to_string(counts.skipped) + " with no module data skipped (inteng makes it)" : std::string()) + "; " + saved);
}

// AutoLink > Module (0xa1), Environment (0xa2), Module (delete old) (0xa5), Environment (delete old) (0xa6): FUN_004c8930 (a copy of the picks, or 0 for every module; delete old 0 or 1),
// run by host::AutoLinkRunner (autolink_runner.h: the pictures on a worker thread, the trees written by the frame; rows ML-A-1..7). The current module's links are written first; the
// display (Frost's autolink_show_draw.h) shows "Right click to cancel", the counter and the pictures; AutoLinkFrame steps the run each frame; at its end every tree is written and the module
// that was current is made current again (FUN_004c8930 0x4c8aa5-0x4c8ad2). The visibility distance is the one Set Visibility holds. NAMED (the owner's ruling): the view goes on while it
// runs; the link commands are refused until it ends (inteng held the program).
// Module Linking's start takes out every link that names no module (FUN_004c90c0 at 0x4c3a15, module_links_clean.h, row ML-E-12): a link to a placed object or to an object the level no
// longer has, with the branch numbers renumbered; with anything changed the object list is drawn again (FUN_004719e0, 0x4c9338). inteng says nothing: nor does this.
void ModuleLinkingCleanTheLinks()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		return;
	}

	host::ModuleLinksCleaned done;
	std::string why;

	if (!host::ModuleLinks_CleanLevel(open, &done, &why))
	{
		printf("AVP_OpenInteng: Module Linking's start could not take out the links that name no module: %s\n", why.c_str());
		fflush(stdout);
	}

	if (done.modules > 0)
	{
		LoadTheObjectListAgain();
		SaveAtIntengsPoint();   // FUN_004719e0 at 0x4c9338: anything dropped, the level FILE written
	}
}

host::AutoLinkRunner autoLinkRun;
int32_t autoLinkKept = -1;   // the module current when the run began (FUN_004c8930's local: made current again at the end)
bool autoLinkFromFile = false;   // the run is Load from file's (FUN_004c8be0): the last module done stays current
static bool autoLinkDeleteOld = false;  // the run began with the modules' links thrown away (Module / Environment (delete old)), for the end's words
std::string autoLinkFileWords;   // what Load from file's start found in the module file (records that name no module, or one that moved), for the end's words

bool AutoLinkRunning()
{
	return autoLinkRun.Running();
}

void AutoLinkCommand(bool all, bool deleteOld)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		Answer("No level is held to work on");
		return;
	}

	if (autoLinkRun.Running() || DeleteLinksRunning())
	{
		Answer("AutoLink is running: right click to cancel");
		return;
	}

	std::vector<int32_t> picked;

	if (!all)
	{
		picked = PickedModules();

		if (picked.empty())
		{
			Answer("AutoLink: pick modules first (click, Shift+click for more)");
			return;
		}
	}

	autoLinkFromFile = false;

	int32_t const current = linkTree ? linkTree->RootModule() : -1;

	if (!WriteTheLinks())
	{
		return;
	}

	std::string why;

	if (!autoLinkRun.Start(open, all ? nullptr : &picked, deleteOld, local_scale > 0.0 ? local_scale : 1.0, visibleDistance, &why))
	{
		Answer("AutoLink: " + why);
		return;
	}

	autoLinkKept = current;
	autoLinkDeleteOld = deleteOld;
	linkTree.reset();   // inteng's current module is the one in hand while the run goes (DAT_00704374); ours are the run's own trees until the end
	CameraPositionSaved(10);   // FUN_004704d0(10) at 0x4c8961, once the setup said yes: the camera in slot 10 and "Camera position saved" (camera-saved-vasquez.md CS-13)
	AutoLinkShow_Begin(host::kAutoLinkWordsY);   // "Right click to cancel" at (200, 220), 0x4c8977
	Answer("AutoLink: running (right click to cancel)");
}

// The run's frame: a right press cancels it (read before each module, 0x4c899c), the posted modules and pictures are put into the trees and on the display, and at its end the trees are
// written and the kept module made current again
void AutoLinkFrame()
{
	if (!autoLinkRun.Running())
	{
		return;
	}

	if (AutoLinkShow_Cancelled())
	{
		autoLinkRun.Cancel();
	}

	host::AutoLinkRunner::Frame frame;

	autoLinkRun.Step(&frame);

	for (int32_t const record : frame.counters)
	{
		AutoLinkShow_Counter(record);   // "%03d" at (230, 180), 0x4c8a20
	}

	for (std::vector<uint32_t> const &picture : frame.pictures)
	{
		AutoLinkShow_Picture(picture);
	}

	if (!frame.finished)
	{
		return;
	}

	host::AutoLinkRunCounts counts;
	std::string why;
	host::AutoLinkResult const result = autoLinkRun.End(LevelObjects_OpenForEdit(), &counts, &why);

	AutoLinkShow_End();

	// AutoLink makes the module that was current current again (FUN_004c10c0(kept, 0, 0), 0x4c8abc); Load from file does not (FUN_004c8be0 0x4c908e-0x4c90af): its last module stays
	MakeTheCurrentModuleCurrentAgain(autoLinkFromFile ? counts.lastObject : autoLinkKept);
	LoadTheObjectListAgain();                          // FUN_004719e0, 0x4c8acd / 0x4c909d
	autoLinkKept = -1;

	char const *const name = autoLinkFromFile ? "Load from file: " : "AutoLink: ";

	if (result != host::AutoLinkResult::Done)
	{
		Answer(std::string(name) + host::AutoLinkWords(result) + (why.empty() ? std::string() : " (" + why + ")"));
		return;
	}

	// FUN_004719e0 at 0x4c8acd (AutoLink) / 0x4c909d (Load from file): the level FILE is written, as inteng's run always ends
	std::string const saved = SaveAtIntengsPoint();

	if (autoLinkFromFile)
	{
		Answer("Load from file: " + std::to_string(counts.modules) + " views drawn" + autoLinkFileWords + ", " + std::to_string(counts.pictures) + " pictures, " +
			std::to_string(counts.linksAdded) + " links added, " + std::to_string(counts.written) + " modules written" + (why.empty() ? std::string() : " (" + why + ")") + "; " + saved);
		return;
	}

	// "0 links added" is true when every module the pictures saw is in the module's links already: AutoLink (not "delete old") keeps the links a module has and adds only new ones (FUN_004c10c0(i, 0, 0)
	// reads them first). The owner read it as wrong (Genshd1 SHED06, 11-17: its 20 links kept, none new; Module (delete old) gives 16 added and 16 links), so the words say why
	bool const nothingNew = counts.linksAdded == 0 && !autoLinkDeleteOld && counts.pictures > 0;

	Answer("AutoLink: " + std::to_string(counts.modules) + " modules run, " + std::to_string(counts.pictures) + " pictures, " + std::to_string(counts.linksAdded) + " links added" +
		(nothingNew ? std::string(" (every module seen was linked already; the links there are kept)") : std::string()) + ", " + std::to_string(counts.written) + " modules written" + (counts.badPictures > 0 ? ", " + std::to_string(counts.badPictures) + " pictures with an unexpected colour" : std::string()) +
		(counts.skipped > 0 ? ", " + std::to_string(counts.skipped) + " with no module data skipped" : std::string()) + (why.empty() ? std::string() : " (" + why + ")") + "; " + saved);
}

// Waypoint mode's command (waypoint_mode.h): the module picked in the view is the one the mode works with; what it says goes on the status line; a menu page it asks for is shown on
// both bars; the screen is left when it says so; a level it swapped is made again for the object list.
// ---- ENTRY POINT MARKERS and Fly (entry_markers.h): the markers of the picked AI module are drawn as small orange cubes in the 3D view (FUN_00588260); a click on one picks it (it turns cyan); Fly (0x25 or F) lets the
// movement keys move it instead of the camera ("Moving Entry Point"); turned off, or when another is picked or the screen is left, its new place is written back (FUN_0058a6b0). NAMED: the click picks the marker nearest
// the line the click makes (light_pick.h's rule: inteng picks the marker's box with the game's object pick); the camera's turn and look are not given to the marker (it moves, it does not turn).
static int32_t entryMarkersOf = -2;          // the module (an object index) the markers are of, -2 for none made yet
static unsigned entryMarkersGeneration = ~0u;
static bool entryRecalcAsking = false;       // Recalculate all is asking its questions: FUN_00588770 let the markers go first, FUN_00588260 makes them again at the end
static std::string entryWords;

static int32_t EntryMarkerModule()
{
	host::Selected const picked = host::Selection_Get();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t index = -1;
	uint8_t id[8];

	if (picked.kind == host::SelectedKind::Module && open && open->Level() && ModuleObject(open, picked.moduleIndex, id) && host::ModuleIndexOfId(open->Level(), id, &index))
	{
		return index;
	}

	// a module picked by its object (the harness's pick-module, the object list)
	if (picked.kind == host::SelectedKind::Object && open && open->Level() && host::ModuleIndexOfId(open->Level(), picked.objectId, &index))
	{
		return index;
	}

	if (waypointMode.CurrentModule() >= 0)
	{
		return waypointMode.CurrentModule();
	}

	return -1;
}

// The markers made again when the module picked or the level changed (FUN_00588260 is called by the pick and by Recalculate all)
void RefreshTheEntryMarkers()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	bool const wanted = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Waypoint") == 0 && waypointMode.Mode() == 0 && open && open->Level();

	if (entryRecalcAsking)
	{
		entryMarkers.clear();
		entryMarkerPicked = -1;
		return;
	}

	if (!wanted)
	{
		if (entryFlying)
		{
			StopFlyingTheEntryPoint("the screen is left");
		}

		entryMarkers.clear();
		entryMarkersOf = -2;
		entryMarkerPicked = -1;
		return;
	}

	int32_t const module = EntryMarkerModule();

	if (module == entryMarkersOf && entryMarkersGeneration == open->Generation())
	{
		return;
	}

	if (entryFlying)
	{
		StopFlyingTheEntryPoint("another module was picked");
	}

	entryMarkersOf = module;
	entryMarkersGeneration = open->Generation();
	entryMarkerPicked = -1;
	entryMarkers.clear();

	if (module >= 0)
	{
		host::EntryMarkers_Of(open, module, &entryMarkers);
	}
}

// The marker's place in the game's world (millimetres) and the cube's twelve edges there
void EntryMarkerLines(std::vector<host::TrackLine> *lines)
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	for (size_t i = 0; i < entryMarkers.size(); i++)
	{
		int32_t const place[3] = { entryMarkers[i].point[0], entryMarkers[i].point[1], entryMarkers[i].point[2] };
		bool const flyingThis = entryFlying && (int)i == entryMarkerPicked;
		int32_t centre[3];

		for (int a = 0; a < 3; a++)
		{
			centre[a] = (int32_t)(((double)(flyingThis ? entryFlyPlace[a] : place[a])) * scale);
		}

		int32_t const half = (int32_t)(host::kEntryMarkerHalf * scale);
		uint32_t const colour = (int)i == entryMarkerPicked ? 0xFF40FFFFu : 0xFFFF7F00u;   // cyan when picked (inteng 0x7eff80 + 0xff), orange (0xff7e05 + 0xfb)

		for (int axis = 0; axis < 3; axis++)
		{
			int const u = (axis + 1) % 3, v = (axis + 2) % 3;

			for (int corner = 0; corner < 4; corner++)
			{
				host::TrackLine line;

				line.colour = colour;

				for (int a = 0; a < 3; a++)
				{
					line.from[a] = centre[a];
					line.to[a] = centre[a];
				}

				line.from[axis] = centre[axis] - half;
				line.to[axis] = centre[axis] + half;
				line.from[u] = line.to[u] = centre[u] + ((corner & 1) ? half : -half);
				line.from[v] = line.to[v] = centre[v] + ((corner & 2) ? half : -half);
				lines->push_back(line);
			}
		}
	}
}

// THE LIGHT MARKERS (FUN_004b2590, the descriptor at 0x5e1c28): every light has a marker object of the editor's own shape 2 (FUN_0042c700's slot 2, the SHAPEHEADER at 0x5e1138 through the
// pointer at 0x5cd960: five points, the apex at the light's place and a square base behind it along the light's direction; five flat polygons, the four sides and the base, items at 0x5e12a8).
// Hide All / Unhide All and the I key hide and show them (the invisible flag); the selected light's is flat red. The polygons are filled (the avpx hook R_SetEditorWorldTriangles).
// THE RULING (Melanikus via Bishop 11-16): inteng's binary data is read from his own inteng.exe when the editor runs, never copied into our source: the shape is IntengShapes slot 2 of the
// shared exe (inteng_exe.h, inteng_shapes.h; every polygon fanned from its first corner, as Vasquez's reader gives it). NAMED: without the exe (not found, or refused) the marker is OUR OWN
// plain shape (a grey pyramid of our own size) and the log and the status line say why. The memory units are 100 to a world unit, so the world numbers are the shape's divided by 100.
static std::vector<host::IntengShapeTriangle> const &LightMarkerShape()
{
	static bool tried = false;
	static std::vector<host::IntengShapeTriangle> shape;

	if (tried)
	{
		return shape;
	}

	tried = true;

	std::string why;
	host::IntengExeImage const *const image = host::IntengExe_Shared(&why);

	if (image)
	{
		int const wanted[] = { 2 };
		host::IntengShapes shapes;
		host::IntengShapesRefusal refused = host::IntengShapesRefusal::None;

		if (host::IntengShapes_Read(*image, wanted, 1, &shapes, &refused) && shapes.have[2] && !shapes.triangles[2].empty())
		{
			shape = shapes.triangles[2];
			LogString("light markers: inteng's shape 2, " + std::to_string((long long)shape.size()) + " triangles, from inteng.exe");
			return shape;
		}

		why = host::IntengShapes_Why(refused);
	}

	// our own plain marker: a square pyramid, the apex at the light, the base 1000 world units behind it and 200 across (x 100 in memory units), grey
	int32_t const tip[3] = { 0, 0, 0 };
	int32_t const base[4][3] = { { -20000, -20000, -100000 }, { -20000, 20000, -100000 }, { 20000, 20000, -100000 }, { 20000, -20000, -100000 } };
	auto add = [&](int32_t const *a, int32_t const *b, int32_t const *c)
	{
		host::IntengShapeTriangle t = {};

		for (int k = 0; k < 3; k++)
		{
			t.p[k] = a[k];
			t.p[3 + k] = b[k];
			t.p[6 + k] = c[k];
		}

		t.colour = 0x808080u;
		shape.push_back(t);
	};

	for (int side = 0; side < 4; side++)
	{
		add(tip, base[(side + 1) % 4], base[side]);
	}

	add(base[0], base[1], base[2]);
	add(base[0], base[2], base[3]);

	std::string const words = "light markers: inteng.exe not read (" + why + "): our own plain grey markers";

	LogString(words);
	static std::string held;
	held = words;
	statusWords = held.c_str();
	return shape;
}

std::string lightSpinWord;   // the state line's lightspin (LightMarkerTriangles makes it each frame)

// When each turning marker began to turn (LightMarkerTriangles keeps it every frame; the click's marker test reads it, so a marker is met where it is drawn)
static std::map<size_t, ULONGLONG> lightMarkerSpinFrom;
static double const kLightMarkerSpinPerMs = 300.0 / 4096.0 * 2.0 * 3.14159265358979323846 / 1000.0;

// A marker's three axes as drawn at `now`: the light's own rows, the two side ones turned about the third while it turns. True (and *angle, radians) while it turns.
static bool LightMarkerAxes(host::LightMarker const &m, size_t i, ULONGLONG now, double axes[9], double *angle)
{
	for (size_t k = 0; k < 9; k++)
	{
		axes[k] = (double)m.matrix[k] / 65536.0;
	}

	auto const from = lightMarkerSpinFrom.find(i);

	*angle = 0.0;

	if (from == lightMarkerSpinFrom.end())
	{
		return false;
	}

	*angle = (double)(now - from->second) * kLightMarkerSpinPerMs;

	double const c = cos(*angle), s = sin(*angle);

	for (int a = 0; a < 3; a++)
	{
		double const x = axes[a], y = axes[3 + a];

		axes[a] = x * c + y * s;
		axes[3 + a] = -x * s + y * c;
	}

	return true;
}

// One face of a marker in the world: the shape's x, y, z points go along the three rows (the shape's memory units are 100 to a world unit), from the light's place
static void LightMarkerFace(host::LightMarker const &m, double const axes[9], double scale, host::IntengShapeTriangle const &face, int32_t world[3][3])
{
	for (int corner = 0; corner < 3; corner++)
	{
		for (int a = 0; a < 3; a++)
		{
			double sum = 0.0;

			for (int k = 0; k < 3; k++)
			{
				sum += (double)face.p[corner * 3 + k] / 100.0 * axes[k * 3 + a];
			}

			world[corner][a] = (int32_t)(m.place[a] * scale + sum);
		}
	}
}

void LightMarkerTriangles(std::vector<LevelViewTriangle> *triangles, uint32_t frame)
{
	static std::vector<host::LightMarker> markers;
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		return;
	}

	host::LightScene_Markers(open, &markers);   // read every frame: Hide All and the I key change the flag with SetPayload, which does not move the file's generation

	size_t selected = (size_t)-1;
	bool const haveSelected = SelectedLight(&selected);
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<host::IntengShapeTriangle> const &shape = LightMarkerShape();

	// A CHANGED LIGHT'S MARKER TURNS (FUN_004ab670, every frame of the screen; light-closeout-frost.md TB-L6): each light of the set with the changed flag (4), but the one flying with L (DAT_00703e60 and the
	// selected), gets the turn rate 300 about its own axis (its block's +0x1b4, the third of the three Euler rates, 4096 to a turn: a turn in about 13.7 seconds) and is turned by the game's step FUN_0053c4f0
	// (the rates times the frame time, the block's quaternion times the step: about the block's own z, the light's direction, which so stays). A light that is not changed stands still (nothing else turns a
	// block). Here the marker's two side rows turn about its third from when it was first seen changed; the light's own turn in the file is NOT the turned one: INTENG BUG, FIXED (inteng's write-back takes a
	// light's turn from its block, 0x4ae1cc-0x4ae3ca, so a changed light was saved turned by however long the screen had been open). NAMED: the sense of the turn is not checked against a running inteng.
	lightSpinWord.clear();
	ULONGLONG const now = GetTickCount64();

	for (size_t i = 0; i < markers.size(); i++)
	{
		host::LightMarker const &m = markers[i];
		bool const spins = m.changed && !(lightFlying && haveSelected && selected == i);

		if (!spins)
		{
			lightMarkerSpinFrom.erase(i);
		}
		else if (lightMarkerSpinFrom.count(i) == 0)
		{
			lightMarkerSpinFrom[i] = now;
		}

		if (m.hidden)
		{
			continue;
		}

		bool const flashing = haveSelected && selected == i;

		// the light's axes, the two side ones turned about the third while it spins
		double axes[9];
		double angle = 0.0;

		if (LightMarkerAxes(m, i, now, axes, &angle))
		{
			// the state line's lightspin: "<light from 1>:<its turn, tenths of a degree>" for each turning marker
			lightSpinWord += (lightSpinWord.empty() ? "" : "|") + std::to_string(i + 1) + ":" + std::to_string((long long)(fmod(angle * 180.0 / 3.14159265358979323846, 360.0) * 10.0));
		}

		for (host::IntengShapeTriangle const &face : shape)
		{
			LevelViewTriangle triangle;
			int32_t world[3][3];

			LightMarkerFace(m, axes, scale, face, world);

			// the SELECTED light's block is the one with the flag 0x40000000 (FUN_004aab60 0x4aaeb0 sets it on the block just picked, FUN_004af660 clears it on the one let go of); a flagged block's polygons take the item flag 0x80 (FUN_004a7cd0 0x4a7eca), drawn flat red,
			// 0xff0000: it does NOT flash (nothing in the screen's frame changes the flag; the only blinking is the text of the attribute, 0x4ac87a)
			triangle.colour = flashing ? 0xFFFF0000u : (0xFF000000u | (face.colour & 0x00FFFFFFu));

			for (int a = 0; a < 3; a++)
			{
				triangle.a[a] = world[0][a];
				triangle.b[a] = world[1][a];
				triangle.c[a] = world[2][a];
			}

			triangles->push_back(triangle);
		}
	}
}

// SELECT: LIGHT's block test for the lights of the set (FUN_00559720 meets a light's MARKER, a block like any other, by its polygons: TB-L3, fixed 2026-11-17): the light whose marker, as it is drawn
// now (turned if it turns), holds the pointer on the screen and is nearest the camera, or SIZE_MAX. *distance: how far that face's middle is from the camera, in the world's millimetres.
// A hidden marker is not drawn and is not met.
size_t LightMarkerUnderPointer(host::PickCamera const &camera, int32_t x, int32_t y, double *distance)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<host::LightMarker> markers;

	*distance = 0.0;

	if (!open)
	{
		return SIZE_MAX;
	}

	host::LightScene_Markers(open, &markers);

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<host::IntengShapeTriangle> const &shape = LightMarkerShape();
	ULONGLONG const now = GetTickCount64();
	size_t best = SIZE_MAX;
	double bestDistance = 0.0;

	for (size_t i = 0; i < markers.size(); i++)
	{
		host::LightMarker const &m = markers[i];

		if (m.hidden)
		{
			continue;
		}

		double axes[9];
		double angle = 0.0;

		LightMarkerAxes(m, i, now, axes, &angle);

		for (host::IntengShapeTriangle const &face : shape)
		{
			int32_t world[3][3];
			double sx[3], sy[3];
			bool ahead = true;

			LightMarkerFace(m, axes, scale, face, world);

			for (int corner = 0; corner < 3 && ahead; corner++)
			{
				host::PickCentre point;

				memcpy(point.place, world[corner], sizeof(point.place));
				ahead = host::PickCentre_Project(camera, point, &sx[corner], &sy[corner]);
			}

			if (!ahead)
			{
				continue;
			}

			// the pointer inside the face on the screen, either way round
			double const px = (double)x, py = (double)y;
			double const d1 = (px - sx[1]) * (sy[0] - sy[1]) - (sx[0] - sx[1]) * (py - sy[1]);
			double const d2 = (px - sx[2]) * (sy[1] - sy[2]) - (sx[1] - sx[2]) * (py - sy[2]);
			double const d3 = (px - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (py - sy[0]);
			bool const someBelow = d1 < 0.0 || d2 < 0.0 || d3 < 0.0;
			bool const someAbove = d1 > 0.0 || d2 > 0.0 || d3 > 0.0;

			if (someBelow && someAbove)
			{
				continue;
			}

			double offset[3];

			for (int a = 0; a < 3; a++)
			{
				offset[a] = ((double)world[0][a] + (double)world[1][a] + (double)world[2][a]) / 3.0 - (double)camera.place[a];
			}

			double const away = sqrt(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);

			if (best == SIZE_MAX || away < bestDistance)
			{
				best = i;
				bestDistance = away;
			}
		}
	}

	*distance = bestDistance;
	return best;
}

// Waypoint mode's waypoints (mode 1): a click in the 3D view (FUN_00584ee0 0x58542d-0x58555d): FUN_0046ddb0 measures, for each box of the shown list, the pixels from the pointer to its block's centre
// on the screen (-1 behind the view); the least above 0.0 (from 100,000,000.0) is the one; it becomes the current waypoint unless it is the current one or the base (pick_centre.h does the sums)
void WaypointBoxClick(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	host::PickCamera camera;
	std::vector<host::WaypointRec> const &list = waypointMode.List();

	if (list.empty() || !PickGame_Camera(viewWidth, viewHeight, &camera))
	{
		return;
	}

	std::vector<host::PickCentre> centres(list.size());

	for (size_t i = 0; i < list.size(); i++)
	{
		host::WaypointView_BoxCentre(waypointMode, i, local_scale, centres[i].place);
	}

	int const taken = host::PickCentre_Nearest(camera, x, y, centres.data(), centres.size());

	if (taken >= 0)
	{
		waypointMode.PickBox(taken);
	}
}

// A click in the 3D view in the AI modules mode: the marker nearest the line it makes is picked (true), or nothing is (false: the click is a module pick)
bool PickAnEntryMarker(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	if (entryMarkers.empty() || barScreen >= host::kIntengScreenCount || strcmp(host::kIntengScreens[barScreen].name, "Waypoint") != 0 || waypointMode.Mode() != 0)
	{
		return false;
	}

	host::PickRay ray;
	std::vector<int32_t> places;
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	if (!PickGame_RayAtClick(x, y, viewWidth, viewHeight, &ray))
	{
		return false;
	}

	for (host::EntryMarker const &marker : entryMarkers)
	{
		for (int a = 0; a < 3; a++)
		{
			places.push_back((int32_t)((double)marker.point[a] * scale));
		}
	}

	size_t found = 0;

	if (!host::LightPick_Nearest(ray, places.data(), entryMarkers.size(), &found))
	{
		return false;
	}

	if (entryFlying)
	{
		StopFlyingTheEntryPoint("another entry point was picked");
	}

	entryMarkerPicked = (int)found;
	entryWords = "Entry point " + std::to_string(found + 1) + " of " + std::to_string(entryMarkers.size()) + ": " + ModuleNameOfIndex(entryMarkers[found].main) + " to " + ModuleNameOfIndex(entryMarkers[found].adjacent) + ". Fly (F) moves it with the movement keys";
	statusWords = entryWords.c_str();
	return true;
}

// FUN_0058b550: the movement keys move the marker picked, or move the camera again (and the marker is written)
void FlyTheEntryPoint()
{
	if (entryMarkerPicked < 0 || (size_t)entryMarkerPicked >= entryMarkers.size())
	{
		Answer("Fly: pick an entry point first (click one of the orange cubes)");
		return;
	}

	if (entryFlying)
	{
		StopFlyingTheEntryPoint("Fly turned off");
		return;
	}

	entryFlying = true;

	for (int a = 0; a < 3; a++)
	{
		entryFlyPlace[a] = entryMarkers[(size_t)entryMarkerPicked].point[a];
	}

	Answer("Moving Entry Point: the movement keys move it, Fly (F) again writes it");
}

// FUN_0058b5e0 and the second half of FUN_0058b550: the moving stops and the marker's place is written back
void StopFlyingTheEntryPoint(char const *why)
{
	if (!entryFlying)
	{
		return;
	}

	entryFlying = false;

	if (entryMarkerPicked < 0 || (size_t)entryMarkerPicked >= entryMarkers.size())
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::EntryMarker const marker = entryMarkers[(size_t)entryMarkerPicked];
	size_t written = 0;
	host::EntryMoveResult const result = host::EntryMarkers_Move(open, marker, entryFlyPlace, &written);

	entryMarkersGeneration = ~0u;   // the markers are made again from what the level holds now

	switch (result)
	{
		case host::EntryMoveResult::Done:
			entryWords = std::string("Entry point written (") + why + "): " + std::to_string(written) + " entries of " + ModuleNameOfIndex(marker.main) + " and its AI module got the new point (kept in the editor's bytes until Save a copy)";
			break;
		case host::EntryMoveResult::NothingMoved:
			entryWords = std::string("Entry point not moved (") + why + ")";
			break;
		default:
			entryWords = std::string("Entry point not written (") + why + "): the game's loader would not take the new entries";
			break;
	}

	statusWords = entryWords.c_str();
}

// The module the editor has picked, as Waypoint mode counts it (its object's place in the level file), or -1.
// A click picks a module by the GAME's module number; Waypoint mode counts objects in the level file, so the pick is turned into its object's place in the file (found by the module's name and
// world place, as the other commands do: ModuleObject). Found by the scenario harness: the number was used as it was, and no click ever picked a module for Edit Module.
int32_t WaypointPickedObject(OpenLevelFile *open)
{
	host::Selected const picked = host::Selection_Get();

	if (picked.kind == host::SelectedKind::Module && open && open->Level())
	{
		uint8_t id[8];
		int32_t objectIndex = -1;

		if (ModuleObject(open, picked.moduleIndex, id) && host::ModuleIndexOfId(open->Level(), id, &objectIndex))
		{
			return objectIndex;
		}
	}

	return -1;
}

void DoWaypointCommand(int32_t command, EditorHostState &host)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::WaypointContext context;

	context.open = open;
	context.pickedModule = WaypointPickedObject(open);
	context.pickedModules = PickedModules();

	// Help (0x15): a help text screen of the mode (FUN_00584e70 / FUN_00584ea0 / FUN_00585720 through FUN_004724f0); the linking page has none
	if (command == 0x15 && waypointMode.Mode() <= 2)
	{
		textPanelText = waypointMode.Mode() == 0 ? kIntengWaypointModulesHelpText : (waypointMode.Mode() == 1 ? kIntengWaypointWaypointsHelpText : kIntengWaypointExtentsHelpText);
		textPanelNext = nullptr;
		return;
	}

	// Entry points > Fly (0x25): the marker picked is moved by the movement keys instead of the camera; turned off, it is written back (entry_markers.h)
	if (command == 0x25 && waypointMode.Mode() == 0)
	{
		FlyTheEntryPoint();
		return;
	}

	// Entry points > Recalculate all (0x26, FUN_00583d00 0x583fe9-0x583ff8): FUN_00588770 first (a marker being moved is written back, FUN_0058b5e0 at 0x588779, the hover is let go and the
	// markers are gone while it asks), FUN_00403d50 with its "modules don't touch" questions (StartEntryRecalc, as Adjacent's Recalculate Entry Points: a Yes takes that entry out), then the
	// picked AI module's markers made again (FUN_00588260); no box and no save. The written-back marker swaps the level, so it comes before the run is worked out. The mode's own 0x26
	// (waypoint_mode.cpp, the same work with no question) is what its unit test drives. (Added 2026-11-17 by Melanikus (Dietrich), AD-6 of reassess/adjacent-closeout-dietrich.md.)
	if (command == 0x26 && waypointMode.Mode() == 0)
	{
		StopFlyingTheEntryPoint("Recalculate all");
		entryRecalcAsking = true;
		entryMarkers.clear();
		entryMarkerPicked = -1;

		bool const started = StartEntryRecalc("Recalculate all", [](std::string const &words)
		{
			LoadTheObjectListAgain();
			entryRecalcAsking = false;
			entryMarkersGeneration = ~0u;   // the markers are made again from what the level holds now (FUN_00588260)
			Answer("Recalculate all: " + words);
		});

		if (!started)
		{
			entryRecalcAsking = false;
			entryMarkersGeneration = ~0u;
		}

		return;
	}

	host::WaypointOutcome const outcome = waypointMode.Command(command, context);

	if (outcome.levelChanged)
	{
		LoadTheObjectListAgain();
	}

	Answer(outcome.say);

	if (outcome.askExtents)
	{
		OpenAPrompt(kPromptWaypointExtents);
	}

	if (outcome.page >= 0)
	{
		EditorBar_SetPage(outcome.page);

		if (!LoadTheMenus())
		{
			LogString("menus: the bar of the Waypoint screen's page couldn't be made, so the editor closed");
			host.running = false;
		}
	}

	if (outcome.leaveScreen)
	{
		waypointMode.Enter();
		EditorBar_SetPage(0);
		host.screen = host::kIntengHub;
	}
}

