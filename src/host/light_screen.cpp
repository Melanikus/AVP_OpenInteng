// light_screen.cpp: LIGHT SOURCING: the light picked by a click, its fields, its keys (attributes, copy / paste / drop / remove, the ambience, the write-back), Enter and Leave. Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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

// ---- Light Sourcing (M-3): a click on the view picks a light, and its numbers are typed in the field panel ----
// (Added 2026-11-15 by Melanikus (Frost).) On the two Light Sourcing screens a click on the view picks the light nearest to the line it
// makes (light_pick.h), and the selected light's numbers show in the field panel and are typed there (the same editable record as any chunk).
// inteng's own light keys (D drop, V paste, C copy, R remove, L fly, S on, X off, O overwrite) and the bar items that go with them (Toggle
// on/off, Switch all on) are NOT here: their handlers (FUN_004ab860, FUN_004ac080, FUN_004ac240 and the others) are not read yet
// (AvP-docs/engine/inteng/inteng-menus.md, 6.7), and what they do to the level must be copied exactly, not guessed. The functions that do
// those edits on the level are in light_edit.h, tested, and wait for the reads.
bool clickPicksLights = true;   // Light Sourcing's Select: Light (the state the screen starts in) or Object
std::string lightWords;   // the status line's words for a light key, kept here so statusWords can point at them

bool IsALightScreen(size_t screen)
{
	if (screen >= host::kIntengScreenCount)
	{
		return false;
	}

	char const *const table = host::kIntengScreens[screen].table;

	return strcmp(table, "0x5e1360") == 0 || strcmp(table, "0x5e1750") == 0;   // the two tables of Light Sourcing (inteng-menus.md section 6)
}

bool OnALightScreen()
{
	return IsALightScreen(barScreen);
}

// ENTERING LIGHT SOURCING LIGHTS THE VIEW (FUN_004aa440 calls FUN_0046e700(0x8253) at 0x4aa5ea): that call builds the screen's shape records afresh (FUN_0042bdb0 for every module, the 0x1 and 0x10 flags light each shape with FUN_004ff010, 0x40 prepares it with FUN_0046de60, 0x200
// takes the morph sub-shapes), and a freshly built polygon is a LIT item (type 6 / 0x18); only Misc > Lighting's FUN_004d67f0(0) turns them to the plain types 5 / 7, and no Light Sourcing code calls it. So the view is lit from the first frame, whatever the Lighting item of another mode said, and the
// mode that is entered next builds its records again from its own flag (FUN_004d68e0 0x4d84d9). The editor's Gouraud switch is that item: on at entry, put back as it was at the exit.
static bool lightEntryTurnedGouraudOn = false;

void RelightTheView();
void LightStateOfThisLevel();
static void ForgetPlacedColours();

void EnterLightSourcing()
{
	// the screen's start makes every light's record afresh (FUN_004ae5d0): each placed light's [5] is 0 again, its colour keys on its UP colour (TB-L5)
	ForgetPlacedColours();

	// and every light's changed flag is taken out, a placed light's NoSpecular moved into its engine flags (light_keys.h LightsAtStart, TB-L7 / TB-L8)
	size_t started = 0;
	host::LightEditResult const atStart = host::LightsAtStart(LevelObjects_OpenForEdit(), &started);

	if (atStart != host::LightEditResult::Done && atStart != host::LightEditResult::NothingOpen)
	{
		LogString(std::string("Light Sourcing's start: the lights' flags were not made as inteng's start makes them: ") + host::LightEditWords(atStart));
	}

	if (ViewToggles_Now().gouraud == host::Gouraud::Off)
	{
		ViewToggles_UseNow(host::ViewToggle::GouraudSwitch);
		lightEntryTurnedGouraudOn = true;
	}

	// the same call lights every shape with the level's lights (FUN_004ff010 for each, flags 0x1 and 0x10): the lights are worked out for the view at entry, as every later light change does
	LightStateOfThisLevel();
	RelightTheView();
}

void LeaveLightSourcing()
{
	if (lightEntryTurnedGouraudOn && ViewToggles_Now().gouraud == host::Gouraud::On)
	{
		ViewToggles_UseNow(host::ViewToggle::GouraudSwitch);
	}

	lightEntryTurnedGouraudOn = false;
}

// Light Sourcing's own state (light_state.h): which lights are on, whether the doors are closed. It is the editor's memory and writes nothing to a level; it starts afresh (every light on, doors open) when another
// level is the one the editor holds.
host::LightState lightState;
static std::string lightStateOfLevel;

// The SELECTED light's own part of the lit view is taken away while it is selected: FUN_004aab60 0x4aaeb0 sets the selection and calls FUN_004fe9d0 (takes the light's contribution off every vertex sum, sets the light's engine flag 0x20),
// FUN_004af660 (let go of, or another picked) calls FUN_004fe9d0 again and then FUN_004feb00 (puts it back) when the light's own on flag is set. S and X while it is selected are FUN_004ac240: on = FUN_004feb00 (back), off = FUN_004fe9d0.
// lightContributionOff is the light (index) whose part is taken off by that, -1 none; lightSelectionSeen the light the last frame had selected.
int32_t lightContributionOff = -1;
static int32_t lightSelectionSeen = -1;

void LightStateOfThisLevel()
{
	char path[MAX_PATH] = "";

	LevelView_FilePath(path, sizeof(path));

	if (lightStateOfLevel != path)
	{
		lightState.Reset();
		lightStateOfLevel = path;
		lightContributionOff = -1;
		lightSelectionSeen = -1;
	}
}

void HierarchyShow(host::HierarchyStep const &step);
OpenLevelFile *HierarchyFile();

// Gives the view's shapes the colours the lighting calculation made (light_scene.h), so the next frame draws them
static size_t GiveTheViewTheseColours(std::vector<host::LitObject> const &lit)
{
	std::vector<LevelViewBaked> baked;

	for (host::LitObject const &one : lit)
	{
		if (!one.reached)
		{
			continue;
		}

		LevelViewBaked item;

		item.name = one.name;
		memcpy(item.location, one.location, sizeof(item.location));
		item.colours = one.computed;
		baked.push_back(item);
	}

	return LevelView_SetBakedColours(baked, local_scale > 0.0 ? local_scale : 1.0);
}

// inteng's view is lit live by the lights that are on (FUN_004ac240 adds or takes away one light's part of every colour), so after a light is switched the view is lit again the
// same way, from the lights as the state holds them. Nothing is written to the level (Save Intensities does that).
// Its words go to relitWords, NEVER to lightWords: the status line may point at lightWords, and giving lightWords new words frees what it pointed at. OUR BUG, FIXED 2026-11-18 (Dietrich, the
// owner-path walk on his Genshd1): a pick that changed the selected light re-lit the view in the next frame (LightKeysEveryFrame), so the status line read freed memory ("Atten: y", "?????").
// Only the key F (lighting again) shows them.
static std::string relitWords;

void RelightTheView()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::LightSceneLights lights;
	std::string why;

	if (!open || !host::LightScene_ReadLights(open, &lights, &why))
	{
		relitWords = "view not re-lit: " + (open ? why : std::string("no level is open"));
		return;
	}

	for (size_t i = 0; i < lights.lights.size(); i++)
	{
		lights.lights[i].on = lightState.IsOn(i) && (int32_t)i != lightContributionOff;
	}

	std::vector<host::LitObject> lit;

	host::LightScene_LightModules(open, lights, false, lightState.DoorsClosed(), &lit, lightState.FlatNormals());   // NAMED: the view shows what the game's save would bake (No PreLight lights off), so it starts as the level's own colours and only the light switched changes it; inteng's live view has those lights on
	size_t const given = GiveTheViewTheseColours(lit);
	relitWords = "view re-lit: " + std::to_string(given) + " modules";
}

static void SaveTheIntensities(bool toolsSave)
{
	host::SaveIntensitiesCounts counts;
	std::vector<host::LitObject> written;
	host::SaveIntensitiesResult const result = host::SaveIntensities(LevelObjects_OpenForEdit(), lightState, toolsSave, &counts, &written);

	if (result == host::SaveIntensitiesResult::Done)
	{
		// inteng's view is lit by the lights it has just summed; ours draws the baked colours the game made from the level's SHPVTINT, so the new ones are given to the shapes it draws
		GiveTheViewTheseColours(written);
		LoadTheObjectListAgain();
		trackDirty = true;
		lightWords = std::string("Intensities saved for the ") + (toolsSave ? "tools" : "game") + " (doors " + (lightState.DoorsClosed() ? "closed" : "open") + "): " + std::to_string(counts.written) + " modules, " +
			std::to_string(counts.vertices) + " vertices, " + std::to_string(counts.lightsOff) + " lights off; Save a copy writes it to a file";
	}
	else
	{
		lightWords = std::string("Save Intensities: ") + host::SaveIntensitiesWords(result);
	}

	statusWords = lightWords.c_str();
}

// The scenario harness's pick-module (kHarnessPickModule): the module with that object number becomes the picked one, as a click on it would make it
void HarnessPickModule(int32_t index)
{
	uint8_t id[host::kSelectionIdBytes];
	host::Selected picked = {};

	bool known = LevelObjects_Level() && host::ModuleIdOfIndex(LevelObjects_Level(), index, id);

	// not a module: any object of the level by its number (a placed light's object)
	if (!known)
	{
		known = LevelObjects_Level() && host::ObjectIdOfIndex(LevelObjects_Level(), index, id);
	}

	if (!known)
	{
		printf("AVP_OpenInteng: pick-module: no module has the number %d\n", (int)index);
		fflush(stdout);
		return;
	}

	// On the Path screen a click picks a module by the GAME's module number (the selection's kind Module, which PickedModuleIdBytes turns into its object): the same pick is made here, so
	// the screen draws it as after a click (purple, FUN_004fa1c0) and not as a selected object (found by acceptance-path: picked as an object, the module also took the selection's look)
	if (OnThePathScreen())
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		int32_t const count = LevelView_ModuleCount();

		for (int32_t m = 0; open && m < count; m++)
		{
			uint8_t game[host::kSelectionIdBytes];

			if (ModuleObject(open, m, game) && memcmp(game, id, host::kSelectionIdBytes) == 0)
			{
				picked.kind = host::SelectedKind::Module;
				picked.moduleIndex = m;
				host::Selection_Set(picked);
				printf("AVP_OpenInteng: pick-module: module %d picked (%s), the game's module %d, as a click on the Path screen picks it\n", (int)index, ModuleNameOfIndex(index).c_str(), (int)m);
				fflush(stdout);
				return;
			}
		}
	}

	// On Module Linking the picks are the screen's own list (linkPicks, FUN_004c0cb0), which the selection alone never fills: the pick is made by the screen's plain click with the module's
	// object, as LinkClick takes a door's (found by module-linking-keys: since e6ef0b4 every S / N / B after a pick-module said "pick a module first")
	if (OnModuleLinkingScreen())
	{
		host::PickReport used = {};

		used.found = host::PickFound::Object;
		memcpy(used.objectId, id, host::kSelectionIdBytes);
		LinkClick(used, false, false);
		printf("AVP_OpenInteng: pick-module: module %d picked (%s), as a click on Module Linking picks it\n", (int)index, ModuleNameOfIndex(index).c_str());
		fflush(stdout);
		return;
	}

	picked.kind = host::SelectedKind::Object;
	memcpy(picked.objectId, id, host::kSelectionIdBytes);
	host::Selection_Set(picked);
	printf("AVP_OpenInteng: pick-module: module %d picked (%s)\n", (int)index, ModuleNameOfIndex(index).c_str());
	fflush(stdout);
}

// The scenario harness's face-light (kHarnessFaceLight)
void HarnessFaceLight(size_t light)
{
	host::LightSceneLights lights;
	std::string why;

	if (!theCamera || !LevelObjects_OpenForEdit() || !host::LightScene_ReadLights(LevelObjects_OpenForEdit(), &lights, &why) || light >= lights.lights.size())
	{
		printf("AVP_OpenInteng: face-light: no such light\n");
		fflush(stdout);
		return;
	}

	// the light's place in the game's world units (millimetres): the memory units are those x 100 (AutoLinkPlace)
	double const dx = lights.lights[light].place[0] / 100.0 - theCamera->x;
	double const dz = lights.lights[light].place[2] / 100.0 - theCamera->z;
	int const turn = (int)floor(atan2(dx, dz) / (2.0 * 3.14159265358979323846) * kFlyCameraWholeTurn + 0.5);

	theCamera->turn = turn;
	FlyCamera_Hold(theCamera);

	host::Selected picked = {};

	picked.kind = host::SelectedKind::Light;
	picked.lightIndex = (int32_t)light;
	host::Selection_Set(picked);
	printf("AVP_OpenInteng: face-light: light %zu selected, the camera turned to %d (the light is %.0f mm away)\n", light + 1, theCamera->turn, sqrt(dx * dx + dz * dz));
	fflush(stdout);
}

// The bar's Light Sourcing items that the loop does (editor_actions.h, kLight...)
// ---- LIGHT SOURCING's KEYS (light_keys.h, screen_keys.h): the selected light's attributes, copy / paste / drop / remove, invisible, the ambience, the write-back ----
// C: the light copied (DAT_00703e50). inteng keeps a POINTER to the light's record, so V and O take its numbers as they are when THEY run (FUN_004ac080, 0x4ad364), and R lets it go when it removes
// that light (0x4ac301). Here: a light of the set by its place in it (a remove before it moves it down one), or a placed light by its object's ID (V makes a new object of it: light_paste.h).
// (Changed 2026-11-17 by Frost: it held the light's bytes as they were at C, and a placed light couldn't be copied.)
struct LightClipboard
{
	bool have = false;
	bool placed = false;
	size_t light = 0;
	uint8_t objectId[8] = {};
};
static LightClipboard lightClipboard;
int lightAttribute = 0;           // DAT_00703e58: the attribute < and > chose, 0 to 12
static int lightHeldFrames = 0;          // DAT_00703e5c: the frames + or - has been held
static bool lightNeedsRelight = false;
bool lightFlying = false;         // L: the selected light follows the camera (DAT_00703e60)
// T PER PLACED LIGHT (light-closeout-frost.md TB-L5): a placed light's record has its own [5] (FUN_004acf30 0x4ad4bf flips the SELECTED light's), 0 (the up colour) when the screen's start makes the records
// (FUN_004ae5d0 0x4ae9ff); the colour keys, the light's words and the panel show the colour it chose, O gives the target the copied light's (0x4ad3e5). Here: the objects whose light chose its DOWN colour,
// emptied at each start of the screen (EnterLightSourcing). (Before 11-17 one switch for every placed light, kept across the screen's starts.)
static std::set<std::string> placedColourDownOf;

bool PlacedColourDown(uint8_t const *id)
{
	return id && placedColourDownOf.count(std::string((char const *)id, 8)) != 0;
}

static void SetPlacedColourDown(uint8_t const *id, bool down)
{
	if (!id)
	{
		return;
	}

	if (down)
	{
		placedColourDownOf.insert(std::string((char const *)id, 8));
	}
	else
	{
		placedColourDownOf.erase(std::string((char const *)id, 8));
	}
}

static void ForgetPlacedColours()
{
	placedColourDownOf.clear();
}

// The selected PLACED light: the selected object, when it has a PLOBJLIT. In Select: Object (0x48) there is no current light (inteng's 0x48 clears DAT_00703d24, and FUN_004ab200 sets only the current
// object, DAT_00703e28), so an object picked there is no light the keys work on (light-closeout-frost.md TB-L4)
bool SelectedPlacedLight(uint8_t id[8])
{
	host::Selected const picked = host::Selection_Get();

	if (OnALightScreen() && !clickPicksLights)
	{
		return false;
	}

	if (picked.kind != host::SelectedKind::Object || !host::PlacedLight_Exists(LevelObjects_OpenForEdit(), picked.objectId))
	{
		return false;
	}

	memcpy(id, picked.objectId, host::kSelectionIdBytes);
	return true;
}

// "LIGHT PROPERTIES" (a placed-object light, key P / Select Object + P: FUN_004acf30 calls FUN_004b0470, which builds the dialog window, and FUN_004b0c00, which fills it from the light's record and reads it back): inteng's FORM, as FUN_004b0470 makes it:
// a window 600 x 420 (FUN_0057d170(600, 0x1a4, 0x20)) titled "Light properties" (0x5e2488); the heading "Times in milliseconds :" at (0x76, 0x14); five rows, the label at x 0x12 and an edit box at x 0x76 (100 x 0x12) at y 0x28 "Fade up time", 0x3c "Time up", 0x50 "Fade down time", 0x64 "Time down", 0x78 "Timer start";
// "On/off type :" at (0x12, 0xc8) over three radio choices at y 0xdc "Standard", 0xf0 "Fade", 0x104 "Flicker" (the radio at x 0x76); "Light type :" at (0x13e, 0xc8) over "Standard", "Strobe", "Flicker" (the radio at x 0x1a2);
// the check boxes "Start on" (y 300, the box at x 0x76) and "Use down colour for off" (y 0x154, the box at x 0xda); the buttons Ok at (200, 0x172) and Cancel at (0x15e, 0x172), 0x50 wide. The numbers are the PLOBJLIT's: fade up 28, fade down 32, time up 36, time down 40, timer start 44, light type 48, on/off type 52, flags 56 (bit 1 Start on, bit 2 Use down colour for off).
// Ok writes them into the chunk (a time below 0 is made 0, above 32767 refused as the loader refuses it: named); Cancel leaves the light as it was.
static void ShowThePlacedLightProperties(uint8_t const *id)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	Chunk *const chunk = (open && open->Level()) ? host::FindPlacedLightChunk(open->Level(), id) : nullptr;
	std::vector<unsigned char> bytes;

	if (!chunk || !open->PayloadOf(chunk, &bytes) || bytes.size() < 60)
	{
		statusWords = "Light properties: pick a placed light (Select, Light, then click it)";
		return;
	}

	auto const at = [&](size_t offset) { int32_t value = 0; memcpy(&value, bytes.data() + offset, 4); return value; };

	host::DialogFormSpec spec;

	spec.title = "Light properties";
	spec.width = 600;
	spec.height = 0x1a4;

	auto const label = [&](int32_t x, int32_t y, int32_t width, char const *words)
	{
		host::DialogWidget one;

		one.type = host::DialogWidgetType::Label;
		one.box = host::DialogRect{ x, y, width, 0x12 };
		one.text = words;
		spec.widgets.push_back(one);
	};

	label(0x76, 0x14, 200, "Times in milliseconds :");

	// the five edit boxes: widget order 1..5 after the labels are pushed alternately (label, edit)
	struct Row { int32_t y; char const *words; size_t offset; };
	Row const rows[5] = { { 0x28, "Fade up time", 28 }, { 0x3c, "Time up", 36 }, { 0x50, "Fade down time", 32 }, { 100, "Time down", 40 }, { 0x78, "Timer start", 44 } };

	for (Row const &row : rows)
	{
		host::DialogWidget edit;

		label(0x12, row.y, 100, row.words);
		edit.type = host::DialogWidgetType::Edit;
		edit.box = host::DialogRect{ 0x76, row.y, 100, 0x12 };
		edit.text = std::to_string((long long)at(row.offset));
		edit.most = 10;
		spec.widgets.push_back(edit);
	}

	auto const check = [&](int32_t x, int32_t y, bool on, int32_t group)
	{
		host::DialogWidget one;

		one.type = host::DialogWidgetType::Check;
		one.box = host::DialogRect{ x, y, 12, 12 };
		one.checked = on;
		one.group = group;
		spec.widgets.push_back(one);
	};

	label(0x12, 200, 0x78, "On/off type :");
	label(0x13e, 200, 0x78, "Light type :");

	char const *const onOff[3] = { "Standard", "Fade", "Flicker" };
	char const *const types[3] = { "Standard", "Strobe", "Flicker" };

	for (int i = 0; i < 3; i++)
	{
		label(0x12, 0xdc + 0x14 * i, 100, onOff[i]);
		check(0x76, 0xdc + 0x14 * i, at(52) == i, 0);
	}

	for (int i = 0; i < 3; i++)
	{
		label(0x13e, 0xdc + 0x14 * i, 100, types[i]);
		check(0x1a2, 0xdc + 0x14 * i, at(48) == i, 1);
	}

	label(0x12, 300, 100, "Start on");
	check(0x76, 300, (at(56) & 1) != 0, -1);
	label(0x12, 0x154, 200, "Use down colour for off");
	check(0xda, 0x154, (at(56) & 2) != 0, -1);

	host::DialogWidget ok, cancel;

	ok.type = host::DialogWidgetType::Button;
	ok.box = host::DialogRect{ 200, 0x172, 0x50, 0x12 };
	ok.text = "Ok";
	ok.answer = host::kDialogOk;
	cancel.type = host::DialogWidgetType::Button;
	cancel.box = host::DialogRect{ 0x15e, 0x172, 0x50, 0x12 };
	cancel.text = "Cancel";
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);

	static uint8_t lightId[8];

	memcpy(lightId, id, sizeof(lightId));

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			return;
		}

		OpenLevelFile *const file = LevelObjects_OpenForEdit();
		Chunk *const placed = (file && file->Level()) ? host::FindPlacedLightChunk(file->Level(), lightId) : nullptr;
		std::vector<unsigned char> now;

		if (!placed || !file->PayloadOf(placed, &now) || now.size() < 60)
		{
			return;
		}

		auto const put = [&](size_t offset, int32_t value) { memcpy(now.data() + offset, &value, 4); };
		int32_t const offsets[5] = { 28, 36, 32, 40, 44 };
		int edits = 0, onOffChosen = 0, typeChosen = 0, radios0 = 0, radios1 = 0;
		bool startOn = false, downOff = false;
		int checks = 0;

		for (host::DialogWidget const &w : widgets)
		{
			if (w.type == host::DialogWidgetType::Edit && edits < 5)
			{
				char *end = nullptr;
				long long const number = _strtoi64(w.text.c_str(), &end, 10);

				// an empty or unreadable box leaves the time as it was; below 0 is 0; above the loader's 32767 refuses the whole dialog (named)
				if (!w.text.empty() && end != w.text.c_str() && *end == '\0')
				{
					if (number > 32767)
					{
						statusWords = "Light properties: a time is at most 32767 milliseconds (the game's loader takes no more)";
						return;
					}

					put((size_t)offsets[edits], (int32_t)(number < 0 ? 0 : number));
				}

				edits++;
			}
			else if (w.type == host::DialogWidgetType::Check)
			{
				if (w.group == 0)
				{
					onOffChosen = w.checked ? radios0 : onOffChosen;
					radios0++;
				}
				else if (w.group == 1)
				{
					typeChosen = w.checked ? radios1 : typeChosen;
					radios1++;
				}
				else
				{
					(checks == 0 ? startOn : downOff) = w.checked;
					checks++;
				}
			}
		}

		put(52, onOffChosen);
		put(48, typeChosen);

		int32_t flags = 0;

		memcpy(&flags, now.data() + 56, 4);
		flags = (flags & ~3) | (startOn ? 1 : 0) | (downOff ? 2 : 0);
		put(56, flags);

		if (!file->SetPayload(placed, now))
		{
			statusWords = "Light properties: the level could not take them";
			return;
		}

		statusWords = "Light properties: written into the placed light's record (kept in the editor's bytes until Save a copy)";
	});
}

bool SelectedLight(size_t *index)
{
	host::Selected const picked = host::Selection_Get();

	if (picked.kind != host::SelectedKind::Light || picked.lightIndex < 0)
	{
		return false;
	}

	*index = (size_t)picked.lightIndex;
	return true;
}

// V with a PLACED light copied (FUN_004ac080 -> FUN_004abb00, light_paste.h): the Name Object box with Light<n> as its text (FUN_004ab980: the first number no object's name has, capitals not
// mattering); Cancel makes nothing; a name an object has gives "The name of the object has already been used." (Re-enter / Cancel, FUN_004d4e00); then the new placed object at the camera's
// place (ViewFilePlace: the camera divided by the level's scale, cut toward 0) turned as the camera is, and it is the one selected. (Added 2026-11-17 by Frost.)
void NameInUseBox(std::string const &typed, std::function<void(std::string const &)> const &named);

static void PasteNamedPlacedLight(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t matrix[9];

	if (typed.empty())
	{
		lightWords = "Paste: no object was made (no name typed)";
		statusWords = lightWords.c_str();
		return;
	}

	if (!open || !theCamera || !LevelView_CameraMatrix(matrix) || !lightClipboard.have || !lightClipboard.placed)
	{
		lightWords = "Paste: no level is open, no frame has been drawn yet, or no placed light is copied";
		statusWords = lightWords.c_str();
		return;
	}

	if (host::ObjectNameInUse(LevelObjects_Level(), typed.c_str()))
	{
		NameInUseBox(typed, PasteNamedPlacedLight);
		return;
	}

	int32_t place[3];
	uint8_t newId[host::kSelectionIdBytes] = {};
	std::string why;

	ViewFilePlace(place);

	host::LightPasteResult const result = host::PastePlacedLight(open, lightClipboard.objectId, typed, place, matrix, newId, &why);

	if (result == host::LightPasteResult::Done)
	{
		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, newId, sizeof(picked.objectId));
		LoadTheObjectListAgain();
		host::Selection_Set(picked);
		lightNeedsRelight = true;
		lightWords = "Placed light pasted at the camera: " + typed;
	}
	else
	{
		lightWords = std::string("Paste: ") + host::LightPasteWords(result) + (why.empty() ? std::string() : ": " + why);
	}

	statusWords = lightWords.c_str();
}

static void PasteThePlacedLight()
{
	NameObjectBox(host::LightPasteFirstName(LevelObjects_OpenForEdit()), PasteNamedPlacedLight);
}

// The Light key a screen key is (screen_keys.h); true when it was one
bool RunLightKey(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t light = 0;
	bool const haveLight = SelectedLight(&light);

	switch (command)
	{
		case host::kScreenKeyLightCopy:
		{
			uint8_t placedId[8];

			lightClipboard = LightClipboard();

			if (haveLight)
			{
				lightClipboard.have = true;
				lightClipboard.light = light;
				lightWords = "Light " + std::to_string(light + 1) + " copied (V pastes it at the camera, O puts its numbers over the selected light)";
			}
			else if (SelectedPlacedLight(placedId))
			{
				// a placed object's light (FUN_004acf30 works on whichever light is selected): V makes a new placed object of it (FUN_004abb00)
				lightClipboard.have = true;
				lightClipboard.placed = true;
				memcpy(lightClipboard.objectId, placedId, sizeof(lightClipboard.objectId));
				lightWords = "The placed light copied (V makes a new object with it at the camera)";
			}
			else
			{
				lightWords = "Copy: pick a light first (Select, Light, then click one)";
			}

			statusWords = lightWords.c_str();
			return true;
		}
		case host::kScreenKeyLightDrop:
		case host::kScreenKeyLightPaste:
		{
			if (command == host::kScreenKeyLightPaste && !lightClipboard.have)
			{
				statusWords = "Paste: copy a light first (C)";
				return true;
			}

			if (!theCamera)
			{
				return true;
			}

			// a placed light copied: a new placed object (FUN_004ac080 -> FUN_004abb00), named first
			if (command == host::kScreenKeyLightPaste && lightClipboard.placed)
			{
				PasteThePlacedLight();
				return true;
			}

			// the copied light's numbers as they are NOW (inteng's clipboard is the light itself)
			host::LightData copied;

			if (command == host::kScreenKeyLightPaste && host::CopyLight(open, lightClipboard.light, &copied) != host::LightEditResult::Done)
			{
				lightClipboard = LightClipboard();
				statusWords = "Paste: the copied light is no longer in the level";
				return true;
			}

			int32_t const place[3] = { (int32_t)theCamera->x, (int32_t)theCamera->y, (int32_t)theCamera->z };
			int32_t matrix[9];
			bool const haveMatrix = LevelView_CameraMatrix(matrix);   // the camera's right, down and forward: the light's turn (FUN_004ab860 copies DAT_008f4f70)
			host::LightData made84 = command == host::kScreenKeyLightPaste ? copied : host::LightDefaultDrop(haveMatrix ? matrix : nullptr);
			size_t made = 0;

			if (command == host::kScreenKeyLightPaste && haveMatrix && made84.size() == host::kLightBytes)
			{
				memcpy(made84.data() + 16, matrix, 36);
			}

			host::LightEditResult const result = host::DropLight(open, place, &made84, &made);

			if (result == host::LightEditResult::Done)
			{
				host::Selected picked = {};

				picked.kind = host::SelectedKind::Light;
				picked.lightIndex = (int32_t)made;
				host::Selection_Set(picked);
				LoadTheObjectListAgain();
				lightNeedsRelight = true;
				lightWords = std::string(command == host::kScreenKeyLightPaste ? "Light pasted" : "Light dropped") + " at the camera: light " + std::to_string(made + 1);
			}
			else
			{
				lightWords = std::string("Light not made: ") + host::LightEditWords(result);
			}

			statusWords = lightWords.c_str();
			return true;
		}
		case host::kScreenKeyLightRemove:
			if (!haveLight)
			{
				statusWords = "Remove: pick a light first";
				return true;
			}

			if (host::RemoveLight(open, light) == host::LightEditResult::Done)
			{
				host::Selection_Clear();
				LoadTheObjectListAgain();
				lightNeedsRelight = true;
				lightWords = "Light " + std::to_string(light + 1) + " removed";

				// the clipboard: the light removed is let go of (0x4ac301); one after it is one place down now
				if (lightClipboard.have && !lightClipboard.placed)
				{
					if (lightClipboard.light == light)
					{
						lightClipboard = LightClipboard();
					}
					else if (lightClipboard.light > light)
					{
						lightClipboard.light--;
					}
				}
			}
			else
			{
				lightWords = "Light not removed";
			}

			statusWords = lightWords.c_str();
			return true;
		case host::kScreenKeyLightOverwrite:
		{
			// O works on the SELECTED light, a light of the set or a placed object's, with the copied light of either kind (FUN_004acf30 0x4ad34e-0x4ad3f5, TB-L1: light_keys.h LightCopyOver),
			// with the copied light's numbers as they are NOW (0x4ad364 reads the record)
			uint8_t placedId[8];
			bool const placedTarget = !haveLight && SelectedPlacedLight(placedId);

			if ((!haveLight && !placedTarget) || !lightClipboard.have)
			{
				statusWords = "Copy over: pick a light, and copy one first (C)";
				return true;
			}

			host::LightKeyRef target, source;

			target.placed = placedTarget;
			target.light = light;
			source.placed = lightClipboard.placed;
			source.light = lightClipboard.light;

			if (placedTarget)
			{
				memcpy(target.objectId, placedId, sizeof(target.objectId));
			}

			if (lightClipboard.placed)
			{
				memcpy(source.objectId, lightClipboard.objectId, sizeof(source.objectId));
			}

			host::LightEditResult const over = host::LightCopyOver(open, target, source, source.placed && PlacedColourDown(source.objectId));

			// the target takes the copied light's [5] (0x4ad3e5; a light of the set's is 0, the up colour)
			if (over == host::LightEditResult::Done && placedTarget)
			{
				SetPlacedColourDown(placedId, source.placed && PlacedColourDown(source.objectId));
			}

			lightWords = over == host::LightEditResult::Done ? std::string("The copied light's numbers are over ") + (placedTarget ? "the selected placed light" : "light " + std::to_string(light + 1)) :
				std::string("Copy over: not done: ") + host::LightEditWords(over);
			lightNeedsRelight = true;
			statusWords = lightWords.c_str();
			return true;
		}
		case host::kScreenKeyLightInvisible:
		{
			bool invisible = false;

			if (!haveLight || host::LightInvisible_Toggle(open, light, &invisible) != host::LightEditResult::Done)
			{
				statusWords = "Visibility: pick a light first";
				return true;
			}

			lightWords = "Light " + std::to_string(light + 1) + (invisible ? " is invisible" : " is visible");
			statusWords = lightWords.c_str();
			return true;
		}
		case host::kScreenKeyLightUpdate:
			lightWords = haveLight && host::LightChanged_Clear(open, light) == host::LightEditResult::Done ? "Light " + std::to_string(light + 1) + " updated (its changed flag is out)" : std::string("Update: pick a light first");
			statusWords = lightWords.c_str();
			return true;
		case host::kScreenKeyLightAttrPrev:
		case host::kScreenKeyLightAttrNext:
			lightAttribute = (lightAttribute + (command == host::kScreenKeyLightAttrNext ? 1 : host::kLightAttributeCount - 1)) % host::kLightAttributeCount;
			uint8_t placedId[8];

			lightWords = haveLight ? host::LightAttribute_Words(open, light, lightAttribute) : (SelectedPlacedLight(placedId) ? host::PlacedLightAttribute_Words(open, placedId, lightAttribute, PlacedColourDown(placedId)) : std::string("Attribute: ") + host::LightAttribute_Name(lightAttribute) + " (pick a light)");
			lightWords += " (+ and - change it)";
			statusWords = lightWords.c_str();
			return true;
		case host::kScreenKeyLightRelight:
			RelightTheView();
			lightWords = relitWords;
			statusWords = lightWords.c_str();
			return true;
		case host::kScreenKeyLightReturn:
		{
			size_t changed = 0;
			host::LightEditResult const result = host::LightsWriteBack(open, false, &changed);

			lightWords = result == host::LightEditResult::Done ? "Lights written back: " + std::to_string(changed) + " lights had engine flag bits 0x300 taken out" : std::string("Write-back refused: ") + host::LightEditWords(result);

			if (open && open->HasChanges())
			{
				SaveTheCopy();
				lightWords += " | " + std::string(statusWords ? statusWords : "");
			}

			statusWords = lightWords.c_str();
			return true;
		}
		case host::kScreenKeyLightFly:
			// FUN_004acf30 0x4ad40d: the light is marked changed and made visible, and follows the camera until L again (FUN_004aaae0 puts it where it is)
			if (lightFlying)
			{
				lightFlying = false;
				lightNeedsRelight = true;
				lightWords = haveLight ? "Fly light off: light " + std::to_string(light + 1) + " stays where the camera put it" : std::string("Fly light off");
			}
			else if (haveLight && theCamera)
			{
				lightFlying = true;
				lightWords = "Fly light on: light " + std::to_string(light + 1) + " follows the camera (L again puts it down)";
			}
			else
			{
				lightWords = "Fly light: pick a light first (Select, Light, then click one)";
			}

			statusWords = lightWords.c_str();
			return true;
		case host::kScreenKeyLightPlacedColour:
		{
			uint8_t placedId[8];

			if (!SelectedPlacedLight(placedId))
			{
				statusWords = "T: only for a placed light (Select, Light, then click it)";
				return true;
			}

			SetPlacedColourDown(placedId, !PlacedColourDown(placedId));   // the SELECTED light's own [5] (0x4ad54c)
			lightWords = PlacedColourDown(placedId) ? "The colour keys now change the placed light's DOWN colour" : "The colour keys now change the placed light's UP colour";
			statusWords = lightWords.c_str();
			return true;
		}
		case host::kScreenKeyLightPlacedProperties:
		{
			uint8_t placedId[8];

			if (!SelectedPlacedLight(placedId))
			{
				statusWords = "P: only for a placed light (Select, Light, then click it)";
				return true;
			}

			ShowThePlacedLightProperties(placedId);
			return true;
		}
		default:
			return false;
	}
}

// Every frame on a Light screen: + and - change the selected light's attribute (FUN_004acf30: a step of 1, 10 after 4 frames, 100 after 31); with Shift they change the ambience by a hundredth (FUN_004aab60 0x4ab020).
static bool lightViewApplied = false;

// Every frame: a light just picked has its part taken off the lit view (FUN_004aab60 0x4aaeb0 -> FUN_004fe9d0); the one let go of (or another picked) has it put back when it is on (FUN_004af660). Leaving the Light screens lets go of it.
static void LightSelectionContributionEveryFrame()
{
	int32_t now = -1;

	if (OnALightScreen())
	{
		host::Selected const picked = host::Selection_Get();

		if (picked.kind == host::SelectedKind::Light && picked.lightIndex >= 0)
		{
			now = picked.lightIndex;
		}
	}

	if (now == lightSelectionSeen)
	{
		return;
	}

	lightSelectionSeen = now;
	LightStateOfThisLevel();
	lightContributionOff = now;

	if (LevelObjects_OpenForEdit())
	{
		RelightTheView();
	}
}

void LightKeysEveryFrame()
{
	LightSelectionContributionEveryFrame();

	if (lightSavePending > 0 && --lightSavePending == 0)
	{
		SaveTheIntensities(lightSaveTools);
		lightMessage.clear();
	}

	if (!OnALightScreen())
	{
		lightHeldFrames = 0;
		lightFlying = false;

		if (lightViewApplied)
		{
			lightViewApplied = false;
			LevelView_HideBeyond(false, 0);
		}

		return;
	}

	// The view distance (FUN_004aab60 0x4aac17: H adds 500000 and N takes 500000 away, never below 0; FUN_004ab120 every frame: a module whose distance from the camera, less its radius, is not under the distance is not drawn;
	// the shared distance starts at 1,000,000). NAMED: the modules near the SELECTED light stay drawn in inteng (its second test), here only the camera's distance counts.
	if (keyDown['H'] || keyDown['N'])
	{
		int64_t const next = (int64_t)viewDistance + (keyDown['H'] ? 500000 : 0) - (keyDown['N'] ? 500000 : 0);

		viewDistance = ViewDistanceHeld(next);
		lightWords = "View Distance: " + std::to_string(viewDistance / 500000);
		statusWords = lightWords.c_str();
	}

	LevelView_HideBeyond(true, viewDistance / 100);   // inteng's DAT_005fd678 is 100 to the millimetre (H-6); the hiding is in millimetres
	lightViewApplied = true;

	// Fly light: the selected light is where the camera is, every frame
	if (lightFlying && theCamera)
	{
		size_t flying = 0;

		if (SelectedLight(&flying))
		{
			int32_t const place[3] = { (int32_t)theCamera->x, (int32_t)theCamera->y, (int32_t)theCamera->z };

			host::LightPlace_Set(LevelObjects_OpenForEdit(), flying, place);
		}
		else
		{
			lightFlying = false;
		}
	}

	// the keys held: the window's own notes of the key messages (a scenario sends messages, not real key states) or the keyboard's state
	bool const plus = KeyHeldNow(VK_OEM_PLUS) || KeyHeldNow(VK_ADD);
	bool const minus = KeyHeldNow(VK_OEM_MINUS) || KeyHeldNow(VK_SUBTRACT);
	bool const shift = ShiftDown();
	int direction = plus && !minus ? 1 : (minus && !plus ? -1 : 0);

	if (direction == 0)
	{
		lightHeldFrames = 0;

		if (lightNeedsRelight)
		{
			std::string const keep = statusWords ? statusWords : "";

			lightNeedsRelight = false;
			RelightTheView();
			lightWords = keep;   // the key's own words stay on the status line
			statusWords = lightWords.c_str();
		}

		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (shift)
	{
		int percent = 0;

		if (host::Ambience_Change(open, direction, &percent) == host::LightEditResult::Done)
		{
			lightWords = "Ambience: " + std::to_string(percent) + "%";
			statusWords = lightWords.c_str();
			lightNeedsRelight = true;
		}

		return;
	}

	size_t light = 0;
	uint8_t placedId[8];
	bool const placed = !SelectedLight(&light) && SelectedPlacedLight(placedId);

	if (!placed && !SelectedLight(&light))
	{
		statusWords = "Attribute: pick a light first (Select, Light, then click one; for a placed light Select, Object)";
		return;
	}

	lightHeldFrames++;

	int steps = direction;

	if (lightHeldFrames > 3)
	{
		steps *= 10;
	}

	if (lightHeldFrames > 30)
	{
		steps *= 10;
	}

	if (placed)
	{
		if (host::PlacedLightAttribute_Change(open, placedId, lightAttribute, steps, PlacedColourDown(placedId)) == host::LightEditResult::Done)
		{
			lightWords = host::PlacedLightAttribute_Words(open, placedId, lightAttribute, PlacedColourDown(placedId));
			statusWords = lightWords.c_str();
			lightNeedsRelight = true;
		}

		return;
	}

	if (host::LightAttribute_Change(open, light, lightAttribute, steps) == host::LightEditResult::Done)
	{
		lightWords = host::LightAttribute_Words(open, light, lightAttribute);
		statusWords = lightWords.c_str();
		lightNeedsRelight = true;
	}
}

void RunLightWork(int32_t work)
{
	LightStateOfThisLevel();

	switch (work)
	{
		case kLightExit:
		{
			// FUN_004aa910: Unhide all lights (FUN_004ab4d0), the write-back (FUN_004ad630), the level written: here the copy, when the level changed (as every mode's Exit does)
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			size_t changed = 0;
			host::LightEditResult const result = host::LightsWriteBack(open, true, &changed);

			lightWords = result == host::LightEditResult::Done ? "Light Sourcing left: every light un-hidden, " + std::to_string(changed) + " lights had engine flag bits 0x300 taken out" : std::string("Light Sourcing left: write-back refused: ") + host::LightEditWords(result);

			if (open && open->HasChanges())
			{
				SaveTheCopy();
				lightWords += " | " + std::string(statusWords ? statusWords : "");
			}

			statusWords = lightWords.c_str();
			return;
		}
		case kLightHierarchy:
		case kLightSequence:
		{
			// FUN_0048b8a0 / FUN_0048bc10 (the same Select Hierarchy / Select Sequence as the Hierarchy mode's, 0x3 / 0x4), then FUN_004b1720: every shape's vertex normals are made again from its polygons (FUN_004b15e0), which the
			// lighting uses from then on, and the lights are worked out again
			HierarchyShow(host::HierarchyMode_Command(HierarchyFile(), &hierarchyMode, work == kLightHierarchy ? 0x03 : 0x04));
			lightState.SetFlatNormals(true);
			RelightTheView();
			lightWords = std::string(statusWords ? statusWords : "") + (statusWords && statusWords[0] ? " | " : "") + "vertex normals made again from the polygons (inteng does this after Hierarchy and Sequence)";
			statusWords = lightWords.c_str();
			return;
		}
		case kLightSaveGame:
		case kLightSaveTools:
			// FUN_004aab60 at 0x4aabb7 / 0x4aabd9: the message "Saving intensities for game" (0x5e2188) / "... for tools" (0x5e2168) is put on the screen (FUN_0057bd40(200, 100)) BEFORE FUN_004aef90 does the work, so it is seen while it runs: the work waits two frames
			lightMessage = work == kLightSaveTools ? "Saving intensities for tools" : "Saving intensities for game";
			lightSavePending = 2;
			lightSaveTools = work == kLightSaveTools;
			return;
		case kLightDoorsClose:
		case kLightDoorsOpen:
			// FUN_004a9f40 closes only when they are open and FUN_004aa1b0 opens only when they are closed (DAT_00703e54): the state is what a save lights
			lightState.SetDoorsClosed(work == kLightDoorsClose);
			LevelView_SetDoorsClosed(work == kLightDoorsClose);
			RelightTheView();
			lightWords = work == kLightDoorsClose ? "Doors closed: the doors are drawn shut and Save Intensities lights them so" : "Doors open: the doors are drawn open and Save Intensities lights them so";
			statusWords = lightWords.c_str();
			return;
		case kLightSwitchAllOn:
			lightState.SwitchAllOn();
			RelightTheView();
			lightWords = "Every light is on";
			statusWords = lightWords.c_str();
			return;
		case kLightToggle:
		{
			// FUN_004ac240(selected light, light is off): the editor's state only, nothing is written to the light's chunk
			host::Selected const picked = host::Selection_Get();

			if (picked.kind != host::SelectedKind::Light || picked.lightIndex < 0)
			{
				statusWords = "Toggle on/off: pick a light first (Select, Light, then click one)";
				return;
			}

			bool const now = lightState.Toggle((size_t)picked.lightIndex);
			lightContributionOff = now ? -1 : picked.lightIndex;   // FUN_004ac240: on puts the part back (FUN_004feb00), off takes it off (FUN_004fe9d0)
			RelightTheView();
			lightWords = "Light " + std::to_string(picked.lightIndex + 1) + (now ? " is on" : " is off") + " (the editor's state: the light's chunk is not changed)";
			statusWords = lightWords.c_str();
			return;
		}
		case kLightSelectLights:
			// inteng's case 0x46 (menu-reads-1-full.txt:7861-7870): a click picks lights, and the selection is let go of (both of inteng's
			// selections, the light's and the object's, are set to none)
			clickPicksLights = true;
			host::Selection_Clear();
			ShowTheSelected(host::Selection_Get());
			statusWords = "A click on the view picks a light";
			return;
		case kLightSelectObjects:
			// inteng's case 0x48 (menu-reads-1-full.txt:7871-7881): the same the other way, a click picks objects
			clickPicksLights = false;
			host::Selection_Clear();
			ShowTheSelected(host::Selection_Get());
			statusWords = "A click on the view picks an object";
			return;
		case kLightHideAll:
		case kLightUnhideAll:
		{
			// matches FUN_004ab5a0 (Hide All, 0x32) and FUN_004ab4d0 (Unhide All, 0x34), out/menu-reads-2.txt:3692-3725 and 3655-3690, as light_edit.cpp says
			bool const hide = work == kLightHideAll;
			size_t changed = 0;
			host::LightEditResult const result = host::HideAllLights(LevelObjects_OpenForEdit(), hide, &changed);

			if (result == host::LightEditResult::Done)
			{
				ShowTheSelected(host::Selection_Get());
				lightWords = std::to_string(changed) + (hide ? " lights hidden" : " lights shown");
				statusWords = lightWords.c_str();
			}
			else
			{
				statusWords = host::LightEditWords(result);
			}

			return;
		}
		case kLightHelp:
			// inteng's case 100 (FUN_004aab60 at 0x4aabab) calls FUN_004aa420, which shows TWO help texts one after the other (FUN_004724f0 twice: the general keys at 0x5e1ea8, then the light states at 0x5e1c68); each stays until a key
			textPanelText = kIntengLightHelpGeneralText;
			textPanelNext = kIntengLightHelpStatesText;
			return;
		default:
			return;
	}
}

// LIGHT SOURCING'S CLICKS (light-closeout-frost.md TB-L3 / TB-L4). A click's centre, as the screen's picks measure it: a place in file units times the level's scale (the camera's world)
static host::PickCentre LightPickCentre(int32_t const *filePlace)
{
	host::PickCentre centre;

	for (int axis = 0; axis < 3; axis++)
	{
		double const world = filePlace[axis] * (local_scale > 0.0 ? local_scale : 1.0);

		centre.place[axis] = world > 2147483000.0 ? 2147483000 : (world < -2147483000.0 ? -2147483000 : (int32_t)world);
	}

	return centre;
}

// SELECT: LIGHT's click (0x46; FUN_004ab2c0, called by the frame FUN_004aab60 0x4aae87 on a press): 1. the record whose BLOCK is under the pointer (FUN_00559720: an ordinary light's marker, or a placed
// light's OBJECT: the record's block is the object's, FUN_004ae5d0 0x4aea21), else 2. the light, ordinary OR PLACED, whose block's centre is nearest the pointer ON THE SCREEN (FUN_0046ddb0, pixels; a centre
// not ahead of the camera is never taken; 0.0 <= d, the best from 1,000,000.0, so the nearest is taken however far; the first on a tie, in the records' order: the placed lights first, FUN_004ae5d0
// makes them before the set's). Every light is a candidate, hidden or not (no test). A placed light picked is its object selected (as the light keys take it). An ordinary light's marker is met by
// its polygons as drawn (LightMarkerUnderPointer), the nearer of it and what the game's line meets is the block under the pointer: OUR BUG, FIXED 2026-11-17 (TB-L3 first took a placed light's
// object under the pointer even with a marker in front of it, so a click on a marker before a lamp picked the lamp; acceptance-light-sourcing L3 went red at 822ab201). NAMED: inteng picks on a
// press of any button (_DAT_00d8806a != 0), here the left click. (Before 11-17 ours had a rule of its own: ordinary lights only, nearest the click's line within 300 mm + 2.5%, in file units.)
size_t LightMarkerUnderPointer(host::PickCamera const &camera, int32_t x, int32_t y, double *distance);

void PickTheLight(host::PickReport const &under, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	// 1. the block under the pointer: a light's marker, unless what the game's line meets is nearer; else a placed light's object
	host::PickCamera markerCamera;
	double markerDistance = 0.0;
	size_t const marker = PickGame_Camera(viewWidth, viewHeight, &markerCamera) && LevelObjects_Level() ? LightMarkerUnderPointer(markerCamera, x, y, &markerDistance) : SIZE_MAX;
	bool const lineMetNearer = (under.found == host::PickFound::Module || under.found == host::PickFound::Object) && (double)under.distance < markerDistance;   // distance: Module and Object only

	if (marker != SIZE_MAX && !lineMetNearer)
	{
		host::Selected picked = {};
		picked.kind = host::SelectedKind::Light;
		picked.lightIndex = (int32_t)marker;
		host::Selection_Set(picked);
		lightWords = "Picked: light " + std::to_string(marker + 1) + ", its marker under the pointer";
		statusWords = lightWords.c_str();
		return;
	}

	if (under.found == host::PickFound::Object && host::PlacedLight_Exists(open, under.objectId))
	{
		host::Selected picked = {};
		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, under.objectId, host::kSelectionIdBytes);
		host::Selection_Set(picked);
		lightWords = "Picked: the placed light under the pointer";
		statusWords = lightWords.c_str();
		return;
	}

	// 2. the nearest centre on the screen: the placed lights (their objects' places), then the set's lights
	host::PickCamera camera;
	std::vector<int32_t> places;
	std::vector<host::PolyCountRow> rows;

	if (!PickGame_Camera(viewWidth, viewHeight, &camera) || !LevelObjects_Level())
	{
		statusWords = "No light picked: no view to pick in";
		return;
	}

	host::LightEditResult const found = host::LightPlaces(open, &places);

	if (found != host::LightEditResult::Done && found != host::LightEditResult::NoLightSet)
	{
		statusWords = host::LightEditWords(found);
		return;
	}

	host::PolyCounts_Read(LevelObjects_Level(), &rows);

	std::vector<host::PickCentre> centres;
	std::vector<size_t> placedRow;   // for each candidate: its row in rows (a placed light), or SIZE_MAX (a light of the set)

	for (size_t i = 0; i < rows.size(); i++)
	{
		if (rows[i].placed && host::PlacedLight_Exists(open, rows[i].id))
		{
			centres.push_back(LightPickCentre(rows[i].place));
			placedRow.push_back(i);
		}
	}

	size_t const placedCount = centres.size();

	for (size_t i = 0; i + 2 < places.size(); i += 3)
	{
		centres.push_back(LightPickCentre(places.data() + i));
		placedRow.push_back(SIZE_MAX);
	}

	int const taken = host::PickCentre_Nearest(camera, x, y, centres.data(), centres.size(), true, host::kPickCentreLightFarthest);

	if (taken < 0)
	{
		statusWords = "No light picked: none is ahead of the camera";
		return;
	}

	host::Selected picked = {};

	if (placedRow[(size_t)taken] != SIZE_MAX)
	{
		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, rows[placedRow[(size_t)taken]].id, host::kSelectionIdBytes);
		host::Selection_Set(picked);
		lightWords = "Picked: the placed light nearest the pointer";
	}
	else
	{
		size_t const light = (size_t)taken - placedCount;

		picked.kind = host::SelectedKind::Light;
		picked.lightIndex = (int32_t)light;
		host::Selection_Set(picked);
		lightWords = "Picked: light " + std::to_string(light + 1) + " of " + std::to_string(places.size() / 3);
	}

	statusWords = lightWords.c_str();
}

// SELECT: OBJECT's click (0x48; FUN_004ab200, the frame 0x4aaeee on a press): the object (any record with an RBOBJECT, +0x154: a module or a placed object) whose centre is nearest the pointer on the
// screen (FUN_0046ddb0, the same rule as the light's: 0.0 <= d, the best from 1,000,000.0), hidden or not; it becomes the current object (its item drawn tinted, flag 0x80: FUN_0046db60, the last one's
// taken off, FUN_0046db90; here the selection's own look). None ahead of the camera: none current. In this mode there is no current light (0x48 clears DAT_00703d24), so the light keys find none.
void PickTheLightScreenObject(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	host::PickCamera camera;
	std::vector<host::PolyCountRow> rows;

	host::Selection_Clear();

	if (!PickGame_Camera(viewWidth, viewHeight, &camera) || !LevelObjects_Level() || host::PolyCounts_Read(LevelObjects_Level(), &rows) == 0)
	{
		statusWords = "No object picked: no view to pick in";
		return;
	}

	std::vector<host::PickCentre> centres;

	for (host::PolyCountRow const &row : rows)
	{
		centres.push_back(LightPickCentre(row.place));
	}

	int const taken = host::PickCentre_Nearest(camera, x, y, centres.data(), centres.size(), true, host::kPickCentreLightFarthest);

	if (taken < 0)
	{
		statusWords = "No object picked: none is ahead of the camera";
		return;
	}

	host::Selected picked = {};
	picked.kind = host::SelectedKind::Object;
	memcpy(picked.objectId, rows[(size_t)taken].id, host::kSelectionIdBytes);
	host::Selection_Set(picked);
	lightWords = "Picked: " + rows[(size_t)taken].name;
	statusWords = lightWords.c_str();
}

// Screenshot (Light sourcing, command 0x66; inteng's FUN_00472930, see screenshot_name.h for what is copied and what differs): the first free
// "<level>P<nn>.bmp" in the editor's own "pictures" folder is found now, and the next frame drawn is saved there.
std::string screenshotPending;   // the path the next frame is saved to, or empty
std::string screenshotWords;     // the status line's words about it

bool ScreenshotNameTaken(std::string const &name, void *folder)
{
	return GetFileAttributesA((*(std::string const *)folder + name).c_str()) != INVALID_FILE_ATTRIBUTES;
}

void TakeAScreenshot()
{
	char opened[MAX_PATH] = "";

	if (!LevelView_FilePath(opened, sizeof(opened)))
	{
		statusWords = "Screenshot: no level is open";
		return;
	}

	// The place is worked out with the same rules as a saved copy's (save_copy.h): never in the game's folder, never beside the level
	std::string copyPath;
	std::string copyFolder;
	char const *why = nullptr;

	if (!host::SaveCopyPlace(opened, theCopiesFolder, theGameFolder, &copyPath, &copyFolder, &why))
	{
		screenshotWords = std::string("Screenshot not saved: ") + (why ? why : "no place for the picture");
		statusWords = screenshotWords.c_str();
		return;
	}

	std::string const folder = host::ScreenshotFolderFromCopyFolder(copyFolder);
	char checked[MAX_PATH];

	std::string const baseNotUsable = MakeTheCopiesFolder();

	if (!baseNotUsable.empty())
	{
		screenshotWords = "Screenshot not saved: the folder for copies can't be used: " + baseNotUsable;
		statusWords = screenshotWords.c_str();
		return;
	}

	if (folder.empty() || (!CreateDirectoryA(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS))
	{
		statusWords = "Screenshot not saved: the editor couldn't make its folder for pictures";
		return;
	}

	if (char const *const notUsable = SaveFolder_WhyNotUsable(folder.c_str(), checked, sizeof(checked)))
	{
		screenshotWords = std::string("Screenshot not saved: the folder for pictures can't be used: ") + notUsable;
		statusWords = screenshotWords.c_str();
		return;
	}

	int const number = host::ScreenshotFirstFree(opened, ScreenshotNameTaken, (void *)&folder);

	if (number < 0)
	{
		// inteng saves nothing and says nothing here; the editor says so
		screenshotWords = "Screenshot not saved: all 100 numbers (P00 to P99) are used in " + folder;
		statusWords = screenshotWords.c_str();
		return;
	}

	screenshotPending = folder + host::ScreenshotName(opened, number);
	screenshotWords = "Saving " + screenshotPending;
	statusWords = screenshotWords.c_str();
}

