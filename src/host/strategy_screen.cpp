// strategy_screen.cpp: STRATEGY: the edit views of each kind of strategy (Edit Strategy 0x7), their windows and targets, and the Strategy screen's commands. Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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
#include "fmv_files.h"
#include "fmv_preview.h"
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

// THE EDIT VIEWS (Edit Strategy 0x7 -> FUN_005286b0 -> the strategy's own method +0x14, one per kind: strategy_views.h). A view is NOT a dialog: it DRAWS lines of text (the first "%s : <Kind>" at (200, 0)) and waits; a left click on a line flips a
// bit or asks for a number in a text dialog (FUN_00474340 reads a whole number; an answer outside what the view takes leaves the number as it was), the Escape key (slot 0xd87a45) or the right button ends it. A new strategy opens its view at once
// (FUN_00528830's last call).
// The level's sounds and particle generators (special_objects_edit.h), read again when the level changes: objects a strategy names (a switch's target, a light's buzz) that have no
// object chunk; inteng makes each a wrapper of its own (FUN_00521cb0: SOUNDOB2 -> FUN_0053ac30, type 5; PARGENER -> FUN_0053afa0, type 8). (Added 2026-11-17 by Frost, S-KC-12.)
static std::vector<host::SpecialObject> const &StrategySpecialObjects()
{
	static std::vector<host::SpecialObject> held;
	static OpenLevelFile *heldOpen = nullptr;
	static unsigned heldFor = ~0u;
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		held.clear();
		heldOpen = nullptr;
		return held;
	}

	if (heldOpen != open || heldFor != open->Generation())
	{
		held.clear();
		host::SpecialObjects_Read(open, &held);
		heldOpen = open;
		heldFor = open->Generation();
	}

	return held;
}

// The sound or particle generator whose ID that is, or null
static host::SpecialObject const *StrategySpecialObjectOf(uint8_t const *id)
{
	for (host::SpecialObject const &one : StrategySpecialObjects())
	{
		if ((one.kind == host::SpecialKind::Sound || one.kind == host::SpecialKind::ParticleGenerator) && memcmp(one.id, id, 8) == 0)
		{
			return &one;
		}
	}

	return nullptr;
}

// the name of the object whose ID a strategy holds ("Associated door :" and the like show it)
bool StrategyViewNameOf(uint8_t const *id, std::string *name)
{
	host::FoundObject found;

	if (!LevelObjects_Level())
	{
		return false;
	}

	if (host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found))
	{
		*name = found.name;
		return true;
	}

	// a virtual object the level brought has no object chunk, only its VIOBJECT: its VOBJPROP's name (inteng's record for it has a wrapper, so FUN_00529eb0 names it: S-KC-12).
	// Found on Genshd1: the Binary Switches' targets tolink01..04 are virtual objects, and the view said "(null)"
	std::vector<host::VirtualObjectRef> virtuals;

	host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

	for (host::VirtualObjectRef const &one : virtuals)
	{
		if (memcmp(one.id, id, 8) == 0)
		{
			*name = one.name;
			return true;
		}
	}

	// an enemy or a generator: its AVPGENER's name
	host::GeneratorRef generator;

	if (host::FindGenerator(LevelObjects_Level(), id, &generator))
	{
		*name = generator.name;
		return true;
	}

	// a placed hierarchy: its PLACHIDT's name (the wrapper's +4, FUN_0053b6b0)
	host::PlacedHierarchyRef placedHierarchy;

	if (host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy))
	{
		*name = placedHierarchy.name;
		return true;
	}

	// a sound (its SOUNDOB2's name, the wrapper's +4) or a particle generator (its PARGENDA's name) (S-KC-12)
	if (host::SpecialObject const *const special = StrategySpecialObjectOf(id))
	{
		*name = special->name;
		return true;
	}

	return false;
}

bool StrategyViewSubject(std::vector<uint8_t> *payload, std::string *name, int32_t *kind, uint8_t *id)
{
	host::ObjectStrategy now;

	// the bytes the editor holds NOW (an edit is in them; the game's class the lookup copies from still has what the file held). While a view is up its object is the one it was opened on (a pick selects another)
	if (strategyEditFan)
	{
		memcpy(id, strategyEditId, 8);
	}
	else if (!SelectedObjectId(id))
	{
		return false;
	}

	if (!LevelObjects_OpenForEdit())
	{
		return false;
	}

	host::LevelStore store(LevelObjects_OpenForEdit());

	if (host::LookUpStrategyBytes(LevelObjects_Level(), store, id, &now) != host::StrategyFound::Found || now.payload.size() < 12)
	{
		return false;
	}

	memcpy(kind, now.payload.data() + 8, sizeof(int32_t));
	*payload = now.payload;

	host::FoundObject found;

	*name = host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found) ? found.name : std::string();

	// a virtual object is not in the object chunks: its name is its VIOBJECT's
	if (name->empty() && LevelObjects_OpenForEdit())
	{
		std::vector<host::VirtualObjectInfo> made;

		if (host::VirtualObject_List(LevelObjects_OpenForEdit(), &made))
		{
			for (host::VirtualObjectInfo const &one : made)
			{
				if (memcmp(one.id, id, 8) == 0)
				{
					*name = one.name;
				}
			}
		}
	}

	// an enemy or a generator: its AVPGENER's name
	host::GeneratorRef generator;

	if (name->empty() && host::FindGenerator(LevelObjects_Level(), id, &generator))
	{
		*name = generator.name;
	}

	// a placed hierarchy: its PLACHIDT's name (the wrapper's +4, FUN_0053b6b0)
	host::PlacedHierarchyRef placedHierarchy;

	if (name->empty() && host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy))
	{
		*name = placedHierarchy.name;
	}

	return true;
}

static void StrategyViewApply(uint8_t const *id, int32_t kind, char const *field, double number);

// The view ends (Escape or the right button, S-KC-3): the screen is drawn again, the object stays the current one
static void EnemyPathFixAtEnd(uint8_t const *id, int32_t kind);   // below, with the Enemy / Generator windows

void StrategyViewEnd()
{
	// an Enemy's or a Generator's view fixes its path data as it ends (S-K69-end, S-K79-end)
	if (strategyEditFan && (strategyEditKind == 69 || strategyEditKind == 79))
	{
		EnemyPathFixAtEnd(strategyEditId, strategyEditKind);
	}

	if (strategyEditFan)
	{
		host::Selected current = {};

		current.kind = host::SelectedKind::Object;
		memcpy(current.objectId, strategyEditId, 8);
		host::Selection_Set(current);   // the object is the current one again (FUN_00529470 after the method)
	}

	strategyPickHave = false;
	trackPage = -1;
	requestPageOn = false;

	if (copyPurpose != 0)
	{
		copyCollecting = false;
		copyPurpose = 0;
		copyTargets.clear();
	}

	strategyEditFan = false;
	strategyEditKind = 0;
	strategyPickField.clear();
	strategyViewPickWords.clear();
	statusWords = "Strategy Data";
}

bool StrategyOfObject(uint8_t const *id, int32_t *kind, std::vector<uint8_t> *payload);
host::ObjectEditResult WriteStrategyBytes(OpenLevelFile *open, uint8_t const *id, std::vector<uint8_t> const &bytes);

// Space on a Multiple Target Switch's view: the next page of 15 targets when there is one, else the first (FUN_0052f700: local_6c + 1, then 0 at the next pass when it is past the last)
void MultiNextPage()
{
	int32_t kind = 0;
	std::vector<uint8_t> bytes;
	std::vector<host::MultiTarget> targets;

	multiPage = StrategyOfObject(strategyEditId, &kind, &bytes) && host::StrategyView_MultiTargets(bytes, &targets) && (int64_t)(multiPage + 1) * 15 < (int64_t)targets.size() ? multiPage + 1 : 0;
}

// a Multiple Target Switch's targets written back (StrategyView_MultiWith: the count and the list, the links / pad / flags after them kept); the view draws them again
static bool MultiWrite(uint8_t const *subject, std::vector<uint8_t> const &payload, std::vector<host::MultiTarget> const &targets)
{
	std::vector<uint8_t> bytes;

	if (!host::StrategyView_MultiWith(payload, targets, &bytes) || !LevelObjects_OpenForEdit())
	{
		return false;
	}

	// Drake's WriteStrategyBytes: a placed object or module through SetStrategyBytes, a VIRTUAL object (most Multi Switches: Temple's exitmultiswitch, Stat101's trigtospidgen) through its VIOBJECT
	return WriteStrategyBytes(LevelObjects_OpenForEdit(), subject, bytes) == host::ObjectEditResult::Done;
}

// S-K61-3: FUN_005282d0 0x528306 -> FUN_0052e000 (dump-full-3, read 11-17): when the level saved is the one open (DAT_0060801c), every record's strategy (DAT_00914954[i] +0x14, its +0x30) goes
// to FUN_0052dc90, which makes the Links of a Linked (0x3d), Multi (0x42) or Area (0x44) switch again (StrategyView_SwitchLinks) and writes them into it; any other kind returns at once (0x52dcc7).
// So a switch whose target was changed or taken out since the level was read is in the saved file's Links of the switch it now names, and no longer in the one it named. Every switch is
// made again from the records as they are BEFORE any of them is written (the Links are not what decides whether a strategy targets another). Gives back how many switches were written.
static int StrategyLinksMakeAgain()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		return 0;
	}

	std::vector<host::StrategyRecordRef> records;

	host::ListStrategyRecords(LevelObjects_Level(), &records);

	// each record's strategy as the open file holds it now (the edits' store, as every view reads it), so a write here never puts back older bytes
	for (host::StrategyRecordRef &one : records)
	{
		int32_t kind = 0;
		std::vector<uint8_t> payload;

		if (StrategyOfObject(one.id.data(), &kind, &payload))
		{
			one.kind = kind;
			one.payload = payload;
		}
	}

	int written = 0;

	for (host::StrategyRecordRef const &one : records)
	{
		std::vector<std::array<uint8_t, 8>> stored;

		if ((one.kind != 61 && one.kind != 66 && one.kind != 68) || !host::StrategyView_StoredLinks(one.kind, one.payload, &stored))
		{
			continue;
		}

		std::vector<std::array<uint8_t, 8>> const links = host::StrategyView_SwitchLinks(one.kind, one.id.data(), records);
		std::vector<uint8_t> bytes;

		if (links != stored && host::StrategyView_WithLinks(one.kind, one.payload, links, &bytes))
		{
			if (WriteStrategyBytes(open, one.id.data(), bytes) == host::ObjectEditResult::Done)
			{
				written++;
			}
			else
			{
				LogString("strategy save: a switch's Links could not be written (its record was not found to write)");
			}
		}
	}

	if (written > 0)
	{
		LoadTheObjectListAgain();
		LogString("strategy save: the Links of " + std::to_string(written) + " switch(es) made again (FUN_0052e000)");
	}

	return written;
}

// Strategy Data's Save (FUN_005282d0, F-3: the bar's Save, Mode > Exit's "Would you like to save first?" Yes, the Stability flags' Yes, the flag pages' Save), in inteng's order (dump-full-3, read
// 11-17): FUN_0053bde0 -> FUN_0053bda0 runs the REPAIR PASS FUN_00536ad0 first (as Find Errors does), then asks each record for problems; its "Warning : %d strategies have errors..." (0x5f0410)
// needs a count Find Errors' same walk leaves at 0, so it is not drawn (Bishop 11-17: a row, no code). Then FUN_0052e000, the Links; then the level written (FUN_004719e0; ours a copy in a hidden
// run, else over the file with its .bak: SaveTheCopy)
void StrategySave()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open && host::StrategyRepair_Run(open) > 0)
	{
		LoadTheObjectListAgain();
	}

	StrategyLinksMakeAgain();
	SaveTheCopy();
}

// The y of the row under the pointer in a kind's Edit view, from the view's top: -1 (no row) when the pointer is outside the view's box (the owner's box, 11-17: the keys that act on
// the row under the pointer read no row outside it, as a left click outside does nothing; inteng's page was the whole screen, so it had no outside). The pick shows no box: the whole window.
static int32_t PointerRowInBox()
{
	bool const outside = strategyViewBox[2] > 0 && (barPointerX < strategyViewBox[0] || barPointerX >= strategyViewBox[0] + strategyViewBox[2] || barPointerY < strategyViewBox[1] ||
		barPointerY >= strategyViewBox[1] + strategyViewBox[3]);

	return outside ? -1 : barPointerY - strategyViewTop;
}


static void MultiTargetsStart(uint8_t const *subject, std::vector<uint8_t> const &payload, int32_t index);   // below, with the collector

// S / D / Shift + D / T on a 66 view (S-K66-3 / -4 / -5), the target row under the pointer: S flips its request's bit 0 (0x52fe29-0x52fe3f); D puts a copy after it (FUN_00530430), Shift + D
// takes it out (FUN_005304a0); T flies to it (FUN_005294f0 + FUN_00529280, 0x52fe83) when the pointer is on one, then opens the targets' collector (FUN_0052ff10) whether it is or not
void MultiKeyUnderPointer(int key, bool shift)
{
	int32_t kind = 0;
	std::vector<uint8_t> payload;
	std::vector<host::MultiTarget> targets;

	if (!StrategyOfObject(strategyEditId, &kind, &payload) || (kind != 66 && kind != 68) || !host::StrategyView_MultiTargets(payload, &targets))
	{
		return;
	}

	size_t index = 0;
	bool const onRow = host::StrategyView_MultiTargetAt(payload, multiPage, PointerRowInBox(), &index);

	if (key == 'T')
	{
		MultiTargetsStart(strategyEditId, payload, onRow ? (int32_t)index : -1);
		return;
	}

	if (!onRow)
	{
		return;
	}

	if (key == 'S')
	{
		targets[index].request ^= 1u;
	}
	else if (shift)
	{
		targets.erase(targets.begin() + (std::ptrdiff_t)index);
	}
	else
	{
		host::MultiTarget const copy = targets[index];

		targets.insert(targets.begin() + (std::ptrdiff_t)index + 1, copy);
	}

	MultiWrite(strategyEditId, payload, targets);
}

// S-KC-12 (FUN_00529eb0 / FUN_00529b60): the words a view shows for an object it names (the null ID is the caller's "None"): an ID no object has prints the null name, "(null)"; else the name and, with a strategy,
// its kind's word (" - type '%d'" for a kind with none: 52 Binary Switch, 53, 63 Console, 67 Teleport, 79 Generator ...); with no strategy, by the wrapper's +8 (0x529d90): a module, placed object (0) or virtual
// object (1) the name alone, an enemy (2) " - Enemy", a generator (3) " - Generator", a sound (5) " - Sound", a placed hierarchy (7) " - Hierarchy", a particle generator (8) " - Particle Generator" (S-KC-12, 11-17).
// Never reached here: 4 " - Light" (FUN_00521cb0 makes no wrapper of type 4), 6 (MUMSOUND) and 10 (MUMMTRAP), the other games' (FUN_0057aa00(4)). NAMED: a cut-scene head (CUTSHEAD, 9 " - Cutscene") is not
// read by ours, and shows "(null)".
std::string StrategyTargetWords(uint8_t const *id)
{
	std::string name;

	if (!StrategyViewNameOf(id, &name))
	{
		return "(null)";
	}

	int32_t kind = 0;
	std::vector<uint8_t> bytes;

	if (!StrategyOfObject(id, &kind, &bytes))
	{
		host::GeneratorRef generator;

		// an AVPGENER is an enemy (2) when its +0x38 is not 0, else a generator (3) (FUN_00521cb0 0x521f01: FUN_00539ff0 / FUN_0053a7a0)
		if (LevelObjects_Level() && host::FindGenerator(LevelObjects_Level(), id, &generator))
		{
			return name + (generator.type != 0 ? " - Enemy" : " - Generator");   // 0x5ee318 / 0x5ee34c
		}

		host::PlacedHierarchyRef placedHierarchy;

		if (LevelObjects_Level() && host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy))
		{
			return name + " - Hierarchy";   // 7: 0x5ee324
		}

		if (host::SpecialObject const *const special = StrategySpecialObjectOf(id))
		{
			return name + (special->kind == host::SpecialKind::Sound ? " - Sound" : " - Particle Generator");   // 5: 0x5ee334, 8: 0x5ee300
		}

		return name;
	}

	int32_t type = 0;
	char const *word = nullptr;

	switch (kind)
	{
		case 50: word = " - Door"; break;                          // 0x5ee590
		case 51: word = " - Lift"; break;                          // 0x5ee598
		case 54: word = " - Platform lift"; break;                 // 0x5ee57c
		case 59: word = " - Switch door"; break;                   // 0x5ee56c
		case 61: word = " - Link switch"; break;                   // 0x5ee55c
		case 62: word = host::StrategyView_FieldNumber(62, bytes, "Type", &type) && type == 16 ? " - Light object" : " - Placed object"; break;   // 0x5ee4a8, 0x5ee4b8
		case 64: word = " - Lighting"; break;                      // 0x5ee530
		case 66: word = " - Multi switch"; break;                  // 0x5ee54c
		case 68: word = " - Area switch"; break;                   // 0x5ee53c
		case 69: word = " - Enemy"; break;                         // 0x5ee318
		case 70: word = " - Mission objective"; break;             // 0x5ee518
		case 72: word = " - Track object"; break;                  // 0x5ee508
		case 73: word = " - Message"; break;                       // 0x5ee4fc
		case 75: word = " - Fan"; break;                           // 0x5ee4f4
		case 76: word = " - Hierarchy"; break;                     // 0x5ee324
		case 77: word = " - Volume of Death"; break;               // 0x5ee4e0
		case 78: word = " - Self Destruct"; break;                 // 0x5ee4cc
		case 80: word = " - Destructable track object"; break;     // 0x5ee488
		case 1000: word = " - Swinging door"; break;               // the other games' kinds, as FUN_00529b60 names them
		case 1001: word = " - Bomb"; break;
		case 1002: word = " - Switch"; break;
		case 2000: word = " - Inanimate Object"; break;
		case 2001: word = " - Pickup"; break;
		case 2002: word = " - Simple Object"; break;
		case 2005: word = " - Chest"; break;
		case 2006: word = " - Camera Adjust Region"; break;
		case 2007: word = " - Pivot Object"; break;
		case 2009: word = " - Crumbling Floor"; break;
		case 2010: word = " - Trigger Volume"; break;
		case 2011: word = " - Track/Morphing"; break;
		case 2012: word = " - Liquid Death Zone"; break;
		case 2013: word = " - Starkey Fragment"; break;
		case 2014: word = " - Spike Trap"; break;
		case 2015: word = " - Training Text"; break;
		default: break;
	}

	if (word)
	{
		return name + word;
	}

	char other[32];

	snprintf(other, sizeof(other), " - type '%d'", kind);   // 0x5ee35c
	return name + other;
}

// MFC's float exchange of edobtype.dll's dialogs (DDX_Text float: leading and trailing spaces, then strtod; anything else left over is no number)
static bool LightFxNumber(std::string const &text, float *value)
{
	char const *at = text.c_str();

	while (*at == ' ' || *at == '\t')
	{
		at++;
	}

	char *end = nullptr;
	double const number = strtod(at, &end);

	if (end == at)
	{
		return false;
	}

	while (*end == ' ' || *end == '\t')
	{
		end++;
	}

	if (*end != '\0')
	{
		return false;
	}

	*value = (float)number;
	return true;
}

// LIGHTING EFFECTS (64; reassess/strategy-data-kinds.md S-K64-1..3): the view is edobtype.dll's "Light FX" dialog (resource 131, 248 x 146 dialog units: 1.5 x 1.625 pixels each). The display hand-over
// (FlipToGDISurface, the palette) is the full-screen display's: ours is a window (NAMED). The group box "Light Types" is a label here (the form has no frame part: NAMED).
static void LightFxOpen(uint8_t const *subject, std::vector<uint8_t> const &payload)
{
	auto const dx = [](int32_t u) { return (int32_t)(u * 3 / 2); };
	auto const dy = [](int32_t u) { return (int32_t)(u * 13 / 8); };
	int32_t type = 0, state = 0;

	host::StrategyView_FieldNumber(64, payload, "type", &type);
	host::StrategyView_FieldNumber(64, payload, "init_state", &state);

	// the model gives a Fixed16Seconds field in whole seconds; the dialog shows word / 65536 as a float (%.6g, MFC's FLT_DIG): read the words themselves
	auto const word = [&payload](size_t at) -> int32_t { int32_t v = 0; if (payload.size() >= at + 4) { memcpy(&v, payload.data() + at, 4); } return v; };
	auto const shown = [](int32_t w) { char text[64]; snprintf(text, sizeof(text), "%.6g", (double)((float)w / 65536.0f)); return std::string(text); };

	host::DialogFormSpec spec;

	spec.title = "Light FX";
	spec.width = dx(248) + 10;
	spec.height = dy(146) + 10;

	auto const add = [&spec](host::DialogWidgetType kind, char const *words, int32_t x, int32_t y, int32_t w, int32_t h, int32_t group, bool checked, int32_t answer)
	{
		host::DialogWidget one;

		one.type = kind;
		one.text = words;
		one.box = { x, y, w, h };
		one.group = group;
		one.checked = checked;
		one.answer = answer;
		spec.widgets.push_back(one);
	};

	int32_t const shownType = type >= 0 && type <= 3 ? type : 0;   // a type above 3 shows Random Flicker

	// 0 label; 1..4 the type radios (1008..1011)
	add(host::DialogWidgetType::Label, "Light Types", dx(17), dy(7), dx(89), 16, -1, false, 0);
	// each radio is its round button (the toolkit's 16 x 16 box) with its words beside it, as the resource's BS_AUTORADIOBUTTON draws them (the toolkit draws no words on a box)
	add(host::DialogWidgetType::Check, "", dx(21), dy(18), 16, 16, 0, shownType == 0, 0);
	add(host::DialogWidgetType::Check, "", dx(21), dy(29), 16, 16, 0, shownType == 1, 0);
	add(host::DialogWidgetType::Check, "", dx(21), dy(40), 16, 16, 0, shownType == 2, 0);
	add(host::DialogWidgetType::Check, "", dx(21), dy(51), 16, 16, 0, shownType == 3, 0);
	// 5..7 the column labels; 8 "Up"; 9 edit 1003; 10 edit 1004; 11 radio 1017; 12 "Down"; 13 edit 1005; 14 edit 1006; 15 radio 1018
	add(host::DialogWidgetType::Label, "Fading (sec)", dx(63), dy(76), dx(60), 16, -1, false, 0);
	add(host::DialogWidgetType::Label, "After Fading (sec)", dx(133), dy(76), dx(64), 16, -1, false, 0);
	add(host::DialogWidgetType::Label, "Default", dx(197), dy(76), dx(40), 16, -1, false, 0);
	add(host::DialogWidgetType::Label, "Up", dx(35), dy(92), dx(20), 16, -1, false, 0);
	add(host::DialogWidgetType::Edit, shown(word(28)).c_str(), dx(63), dy(91), dx(40), dy(14), -1, false, 0);
	add(host::DialogWidgetType::Edit, shown(word(36)).c_str(), dx(139), dy(91), dx(40), dy(14), -1, false, 0);
	add(host::DialogWidgetType::Check, "", dx(203), dy(93), 16, 16, 1, type != 0 && state == 0, 0);
	add(host::DialogWidgetType::Label, "Down", dx(35), dy(119), dx(24), 16, -1, false, 0);
	add(host::DialogWidgetType::Edit, shown(word(32)).c_str(), dx(63), dy(116), dx(40), dy(14), -1, false, 0);
	add(host::DialogWidgetType::Edit, shown(word(40)).c_str(), dx(139), dy(116), dx(40), dy(14), -1, false, 0);
	add(host::DialogWidgetType::Check, "", dx(203), dy(119), 16, 16, 1, type != 0 && state == 1, 0);
	// 16 OK, 17 Cancel
	add(host::DialogWidgetType::Button, "OK", dx(191), dy(7), dx(50), dy(14), -1, false, host::kDialogOk);
	add(host::DialogWidgetType::Button, "Cancel", dx(191), dy(24), dx(50), dy(14), -1, false, host::kDialogEscape);
	// the radios' words, after every widget the form numbers (form-field / the edits' order is unchanged)
	add(host::DialogWidgetType::Label, "Random Flicker", dx(21) + 20, dy(18), dx(80) - 20, 16, -1, false, 0);
	add(host::DialogWidgetType::Label, "Strobe", dx(21) + 20, dy(29), dx(80) - 20, 16, -1, false, 0);
	add(host::DialogWidgetType::Label, "Switch", dx(21) + 20, dy(40), dx(80) - 20, 16, -1, false, 0);
	add(host::DialogWidgetType::Label, "Flicky Switch", dx(21) + 20, dy(51), dx(80) - 20, 16, -1, false, 0);

	for (host::DialogWidget &one : spec.widgets)
	{
		one.most = one.type == host::DialogWidgetType::Edit ? 32 : one.most;
	}

	std::array<uint8_t, 8> lift;

	memcpy(lift.data(), subject, 8);
	host::Dialogs_Form(spec, [lift, type, state, shownType](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk || widgets.size() < 18)
		{
			return;   // Cancel / Escape: nothing changes
		}

		// UpdateData(TRUE) (DoDataExchange 0x10002700): 1006, 1004, 1005 (each read, then checked 0 to 100), the Up / Down radio, 1003, then the type radio. A box that is no number stops the exchange before it is
		// stored ("Please enter a number."); one out of range is stored and THEN stops it ("Please enter a number between 0 and 100."); what comes after a stop keeps the dialog's first values. OnOK does not test
		// the result.
		struct Box { size_t widget; size_t at; };
		Box const order[4] = { { 14, 40 }, { 10, 36 }, { 13, 32 }, { 9, 28 } };
		std::map<size_t, int32_t> words;
		std::string warning;
		int32_t radioState = state == 0 || state == 1 ? state : -1;
		int32_t radioType = shownType;
		bool stopped = false;

		for (size_t step = 0; step < 4 && !stopped; step++)
		{
			if (step == 3)
			{
				// the Up / Down radio is exchanged before 1003
				radioState = widgets[11].checked ? 0 : (widgets[15].checked ? 1 : -1);
			}

			float value = 0.0f;

			if (!LightFxNumber(widgets[order[step].widget].text, &value))
			{
				warning = "Please enter a number.";
				stopped = true;
				break;
			}

			words[order[step].at] = (int32_t)(value * 65536.0f);   // 0x1001a660: stored as (int)(value x 65536)

			if (!(value >= 0.0f && value <= 100.0f))
			{
				warning = "Please enter a number between 0 and 100.";
				stopped = true;
			}
		}

		if (!stopped)
		{
			radioType = widgets[1].checked ? 0 : (widgets[2].checked ? 1 : (widgets[3].checked ? 2 : (widgets[4].checked ? 3 : -1)));
		}

		auto const store = [lift, words, radioType, radioState, type]()
		{
			std::vector<uint8_t> now;
			int32_t kindNow = 0;

			if (!StrategyOfObject(lift.data(), &kindNow, &now) || kindNow != 64 || now.size() < 44)
			{
				return;
			}

			for (auto const &one : words)
			{
				memcpy(now.data() + one.first, &one.second, 4);
			}

			// the type = the radio (0..3: none checked keeps it); with 1, 2 or 3 the initial state = the Up / Down radio (left when neither is set); with 0 it is left as it was
			int32_t const newType = radioType >= 0 ? radioType : type;

			memcpy(now.data() + 20, &newType, 4);

			if (newType >= 1 && newType <= 3 && radioState >= 0)
			{
				memcpy(now.data() + 24, &radioState, 4);
			}

			if (LevelObjects_OpenForEdit())
			{
				WriteStrategyBytes(LevelObjects_OpenForEdit(), lift.data(), now);
			}
		};

		// OnOK (0x10002900): after the exchange, the debug message box "Radio num = %d" (0x1001f0b8) with the type radio's number, every time; COPIED as inteng shows it (Bishop's order 10b), NAMED so Melanikus
		// can veto it. Its caption (the DLL's application name) is not drawn by ours.
		char radio[48];

		snprintf(radio, sizeof(radio), "Radio num = %d", radioType);

		std::string const radioWords = radio;

		if (!warning.empty())
		{
			DialogMessage(warning, [radioWords, store]() { DialogMessage(radioWords, store); });
		}
		else
		{
			DialogMessage(radioWords, store);
		}
	});
}

// CONSOLE (63; S-K63-1): a window, not a page of lines (FUN_0052f260: FUN_0057d170(400, 200, 4), centred): "Console Number" (0x5eeedc), a number box at (0x82, 100), 0x96 x 0x12, holding ConsoleNum, "Ok" (100, 0x96)
// answering 2 and "Cancel" (200, 0x96). Ok (Return too): the number typed is stored when no other Console of the level has it (FUN_0052f1d0), else the message "This number is used by another console" (0x5ef054)
// and nothing changes
static void ConsoleOpen(uint8_t const *subject, std::vector<uint8_t> const &payload)
{
	int32_t number = 0;

	host::StrategyView_FieldNumber(63, payload, "ConsoleNum", &number);

	host::DialogFormSpec spec;
	host::DialogWidget box, ok, cancel;

	spec.title = "Console Number";
	spec.width = 400;
	spec.height = 200;
	box.type = host::DialogWidgetType::Edit;
	box.text = std::to_string((long long)number);
	box.box = { 0x82, 100, 0x96, 0x12 };
	box.most = 10;   // FUN_0057de30's number box
	ok.type = host::DialogWidgetType::Button;
	ok.text = "Ok";
	ok.box = { 100, 0x96, 0x32, 0x10 };
	ok.answer = 2;
	cancel = ok;
	cancel.text = "Cancel";
	cancel.box = { 200, 0x96, 0x32, 0x10 };
	cancel.answer = host::kDialogEscape;
	spec.widgets = { box, ok, cancel };

	std::array<uint8_t, 8> console;

	memcpy(console.data(), subject, 8);
	host::Dialogs_Form(spec, [console](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if ((answer != 2 && answer != host::kDialogOk) || widgets.empty())
		{
			return;
		}

		int32_t const typed = (int32_t)strtol(widgets[0].text.c_str(), nullptr, 10);
		std::vector<std::array<uint8_t, 8>> ids;

		host::ObjectEdit_Ids(LevelObjects_Level(), &ids);

		for (std::array<uint8_t, 8> const &one : ids)
		{
			int32_t kind = 0, other = 0;
			std::vector<uint8_t> bytes;

			if (memcmp(one.data(), console.data(), 8) != 0 && StrategyOfObject(one.data(), &kind, &bytes) && kind == 63 && host::StrategyView_FieldNumber(63, bytes, "ConsoleNum", &other) && other == typed)
			{
				DialogMessage("This number is used by another console");   // 0x5ef054, FUN_00474030
				return;
			}
		}

		StrategyViewApply(console.data(), 63, "ConsoleNum", (double)typed);
	});
}

// The kinds whose method +0x14 shows no page of lines: 64 (the Light FX dialog), 63 (the Console Number window), and the kinds with no class of their own (S-KC-11: 55, 56, 57 or 60 written literally, 58, 65, 71, 74:
// "Unknown strategy type" (0x5ee814) for 700 ms). True when the kind is one of them (and what it shows is up)
static bool StrategyViewIsADialog(int32_t kind, uint8_t const *id, std::vector<uint8_t> const &payload)
{
	if (kind == 64)
	{
		LightFxOpen(id, payload);
		return true;
	}

	if (kind == 63)
	{
		ConsoleOpen(id, payload);
		return true;
	}

	if (kind == 55 || kind == 56 || kind == 57 || kind == 58 || kind == 60 || kind == 65 || kind == 71 || kind == 74)
	{
		StrategyMessage("Unknown strategy type");
		statusWords = "Unknown strategy type";
		return true;
	}

	return false;
}

// THE OWNER'S ADDITION (11-18, through Bishop: the editor reads the user's FMVs): what the game makes of a Message strategy's number as an FMV, on the status line. Strategy n plays
// FMVs\message<n>.smk from the data folder (FMV-T-3, FMV-T-5 of reassess/fmv-trace-dietrich.md); fmv_files.h reads the file's header as hostile input and says what it is
static std::string messageFmvWords;   // the last words SayMessageFmv said (the Edit Strategy press shows them in place of its own for a Message's view)

static void SayMessageFmv(int32_t number)
{
	messageFmvWords = host::FmvFiles_MessageWords(host::FmvFiles_Folder(theGameFolder), number);
	statusWords.Say(messageFmvWords.c_str(), StatusSayer::StrategyView);   // the view draws it (the owner's call 11-19, status_words.h)
}

bool StrategyViewOpen()
{
	std::vector<uint8_t> payload;
	std::string name;
	int32_t kind = 0;
	uint8_t id[8];

	if (StrategyViewSubject(&payload, &name, &kind, id) && StrategyViewIsADialog(kind, id, payload))
	{
		strategyEditFan = false;
		strategyEditKind = 0;
		return true;   // no lines: a dialog (64, 63) or a message (S-KC-11) is what the kind's method shows
	}

	if (!StrategyViewSubject(&payload, &name, &kind, id) || !host::StrategyView_Has(kind))
	{
		strategyEditFan = false;
		strategyEditKind = 0;
		return false;
	}

	memcpy(strategyEditId, id, 8);
	strategyEditKind = kind;
	strategyEditFan = true;
	trackPage = -1;
	multiPage = 0;   // FUN_0052f700 0x52f73f: the first page each time the view opens
	strategyPickHave = false;

	// a Linked / Multi / Area switch: its Links made again, once, as its view opens (61: code 0x52dfd0, 66 / 68: FUN_0052f700 0x52f725 -> FUN_0052dc90; S-K61-1, S-K66-2), from every record's
	// strategy as it is now. inteng also writes them into the switch's record; the level's own bytes get them at Save (S-K61-3)
	strategyViewLinks.clear();

	if ((kind == 61 || kind == 66 || kind == 68) && LevelObjects_Level())
	{
		std::vector<host::StrategyRecordRef> records;

		host::ListStrategyRecords(LevelObjects_Level(), &records);
		strategyViewLinks = host::StrategyView_SwitchLinks(kind, id, records);
	}

	// what the kind's method writes before it draws (a door's times of 0, a door that is neither kind)
	std::vector<host::StrategyViewFix> fixes;

	host::StrategyView_OnOpen(kind, payload, &fixes);

	for (host::StrategyViewFix const &fix : fixes)
	{
		StrategyViewApply(id, kind, fix.field.c_str(), fix.value);
	}

	// a Message's view: its FMV said (ours, SayMessageFmv)
	int32_t messageNumber = 0;

	messageFmvWords.clear();

	if (kind == 73 && host::StrategyView_FieldNumber(73, payload, "message_string", &messageNumber))
	{
		SayMessageFmv(messageNumber);
	}

	return true;
}

static void StrategyViewApplyBody(uint8_t const *id, int32_t kind, char const *field, double number)
{
	if (!LevelObjects_OpenForEdit())
	{
		return;
	}

	host::LevelStore store(LevelObjects_OpenForEdit());
	host::FieldEdit edit;
	char const *why = nullptr;
	host::StrategyFound lookup;

	edit.field = field;
	edit.what = host::EditWhat::Number;
	edit.number = number;
	host::EditAnswer const answer = host::ApplyStrategyEdit(LevelObjects_Level(), id, kind, edit, &store, &why, &lookup);

	if (answer != host::EditAnswer::Applied)
	{
		static std::string refused;

		refused = std::string("Edit Strategy: the strategy would not take it") + (why ? std::string(" (") + why + ")" : std::string());   // (ours: inteng says nothing)
		statusWords.Say(refused.c_str(), StatusSayer::StrategyView);   // the view draws it (the owner's call 11-19)
	}
}

static void StrategyViewApply(uint8_t const *id, int32_t kind, char const *field, double number)
{
	StrategyViewApplyBody(id, kind, field, number);
}

// THE OBJECT PICK of a view line (FUN_00527540(start, 1); reassess/strategy-data-kinds.md S-KC-5): the screen is drawn as usual (FUN_00529370) with "Press return when finished" and the picked object's
// name; the picked object is RED; a left click takes the object under the pointer (any object the screen has a wrapper for: a module, a placed object, a virtual object's V ...), a click on nothing lets the picked one go;
// Return or the RIGHT button ends it (Escape does not: its slot stays set and the view ends once the pick does). Then the row's own end (StrategyPickEnd).
static bool StrategyObjectExists(uint8_t const *id)
{
	host::FoundObject found;
	host::GeneratorRef generator;

	if (LevelObjects_Level() && host::FindGenerator(LevelObjects_Level(), id, &generator))
	{
		return true;
	}

	if (host::PlacedHierarchyRef placedHierarchy; LevelObjects_Level() && host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy))
	{
		return true;
	}

	if (LevelObjects_Level() && host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found))
	{
		return true;
	}

	std::vector<host::VirtualObjectRef> virtuals;

	host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

	for (host::VirtualObjectRef const &one : virtuals)
	{
		if (memcmp(one.id, id, 8) == 0)
		{
			return true;
		}
	}

	return false;
}

// A virtual object (no shape: the rows that need the picked object's shape record, +0x154, refuse it)
static bool StrategyObjectIsVirtual(uint8_t const *id)
{
	std::vector<host::VirtualObjectRef> virtuals;

	host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

	for (host::VirtualObjectRef const &one : virtuals)
	{
		if (memcmp(one.id, id, 8) == 0)
		{
			return true;
		}
	}

	return false;
}

// FUN_00529280(block, 0): the camera goes to the object's place (its motion numbers are 0 at rest, so "less ten times them" is nothing), the turn kept
void StrategyFlyTo(uint8_t const *id)
{
	if (!theCamera || !LevelObjects_Level())
	{
		return;
	}

	// FUN_00529280(record, 0) (dump-full-3:61782; S-11, S-K52-7): the camera's place = the record's LESS 10 x its own forward axis (+0x50..+0x58, 16.16: 655360 inteng units, forward / 10 in
	// the game's), its turn and look kept. The record's place: an object chunk's (as edited), else a virtual object the editor made (its object chunk), else a level's virtual object (its VOBJPROP)
	int64_t place3[3], least[3], most[3];
	host::FoundObject found;
	double at[3] = { 0, 0, 0 };
	bool have = false;

	if (host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found))
	{
		at[0] = found.x; at[1] = found.y; at[2] = found.z;
		have = true;
	}
	else if (host::VirtualObject_Geometry(LevelObjects_Level(), id, place3, least, most))
	{
		at[0] = (double)place3[0]; at[1] = (double)place3[1]; at[2] = (double)place3[2];
		have = true;
	}
	else
	{
		std::vector<host::VirtualObjectRef> virtuals;

		host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

		for (host::VirtualObjectRef const &one : virtuals)
		{
			if (!have && memcmp(one.id, id, 8) == 0)
			{
				at[0] = one.place[0]; at[1] = one.place[1]; at[2] = one.place[2];
				have = true;
			}
		}
	}

	// an enemy or a generator (bishop-int's Find rows, Drake): its AVPGENER's place
	host::GeneratorRef generator;

	if (!have && host::FindGenerator(LevelObjects_Level(), id, &generator))
	{
		at[0] = generator.place[0]; at[1] = generator.place[1]; at[2] = generator.place[2];
		have = true;
	}

	// a placed hierarchy: its PLACHIDT's place (its wrapper's record +0x10..)
	host::PlacedHierarchyRef placedHierarchy;

	if (!have && host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy))
	{
		at[0] = placedHierarchy.place[0]; at[1] = placedHierarchy.place[1]; at[2] = placedHierarchy.place[2];
		have = true;
	}

	if (!have)
	{
		return;
	}

	int forward[3] = { 0, 0, 0 }, right[3] = { 0, 0, 0 };

	LevelView_CameraAxes(forward, right, nullptr);
	theCamera->x = (int)(at[0] * local_scale) - forward[0] / 10;
	theCamera->y = (int)(at[1] * local_scale) - forward[1] / 10;
	theCamera->z = (int)(at[2] * local_scale) - forward[2] / 10;
	FlyCamera_Hold(theCamera);
}

// the picked object is the red one (the screen's current object, which the view's own object is not while the pick runs: FUN_00529480 before the method)
static void StrategyPickShow()
{
	if (strategyPickHave)
	{
		host::Selected one = {};

		one.kind = host::SelectedKind::Object;
		memcpy(one.objectId, strategyPickId, 8);
		host::Selection_Set(one);
	}
	else
	{
		host::Selection_Clear();
	}
}

// a left click while the pick runs (FUN_00527540 0x5275db-0x527633): the object under the pointer, else none
void StrategyViewPickClicked(host::PickReport const &report, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t target[8] = { 0 };
	bool have = false;

	if (open && open->Level())
	{
		if (report.found == host::PickFound::Module)
		{
			have = ModuleObject(open, report.moduleIndex, target);
		}
		else if (report.found == host::PickFound::Object)
		{
			memcpy(target, report.objectId, sizeof(target));
			have = true;
		}

		bool const levelHit = report.found == host::PickFound::Module || report.found == host::PickFound::Object;

		have = StrategyIconUnderClick(x, y, viewWidth, viewHeight, levelHit ? report.distance : -1, target) || have;
	}

	if (!have)
	{
		strategyPickHave = false;   // a click on nothing lets the picked one go
	}
	else if (!strategyPickHave || memcmp(target, strategyPickId, 8) != 0)
	{
		memcpy(strategyPickId, target, 8);
		strategyPickHave = true;
	}

	StrategyPickShow();
}

// An object field of a strategy (by its object's ID and kind) set to an object, or to the null ID (0 0) when target is null
static bool StrategyFieldSetObject(uint8_t const *id, int32_t kind, char const *field, uint8_t const *target)
{
	if (!LevelObjects_OpenForEdit())
	{
		return false;
	}

	host::LevelStore store(LevelObjects_OpenForEdit());
	host::FieldEdit edit;
	char const *why = nullptr;
	host::StrategyFound lookup;
	uint8_t none[8] = { 0 };
	uint8_t const *const put = target ? target : none;

	edit.field = field;
	edit.what = host::EditWhat::Object;
	memcpy(&edit.first, put, 4);
	memcpy(&edit.second, put + 4, 4);
	return host::ApplyStrategyEdit(LevelObjects_Level(), id, kind, edit, &store, &why, &lookup) == host::EditAnswer::Applied;
}

// A strategy's whole bytes written. SetStrategyBytes reaches placed objects and modules; an ID none of them has is a virtual object's (every stock Mission Objective, S-K70-7 / -8, and the
// switches a virtual object holds), written through VirtualObject_SetStrategyBytes (until 2026-11-16 those writes were refused, unseen: P / V and the alterations never changed a stock level).
host::ObjectEditResult WriteStrategyBytes(OpenLevelFile *open, uint8_t const *id, std::vector<uint8_t> const &bytes)
{
	host::ObjectEditResult const placed = host::SetStrategyBytes(open, id, bytes.data(), bytes.size());

	if (placed != host::ObjectEditResult::NoSuchObject)
	{
		return placed;
	}

	return host::VirtualObject_SetStrategyBytes(open, id, bytes.data(), bytes.size()) == host::VirtualObjectResult::Done ? host::ObjectEditResult::Done : placed;
}

// The kind of an object's strategy, and its bytes; false when it has none
bool StrategyOfObject(uint8_t const *id, int32_t *kind, std::vector<uint8_t> *payload)
{
	if (!LevelObjects_OpenForEdit())
	{
		return false;
	}

	host::LevelStore store(LevelObjects_OpenForEdit());
	host::ObjectStrategy now;

	if (host::LookUpStrategyBytes(LevelObjects_Level(), store, id, &now) != host::StrategyFound::Found || now.payload.size() < 12)
	{
		return false;
	}

	memcpy(kind, now.payload.data() + 8, sizeof(int32_t));

	if (payload)
	{
		*payload = now.payload;
	}

	return true;
}

// THE LIFT'S EDITS (51; reassess/strategy-data-kinds.md S-K51-6..12). The payload: 20 bytes, the positions' count (+20) and their IDs (8 each), then the door, the call switch and the floor switch (8 each), the flags,
// the Floor, the external lifts' count and their 28-byte records, and the Facing (the last 4 bytes). An edit that changes the list's size writes the whole payload (object_edit.h SetStrategyBytes).
static int32_t StrategyGet32(std::vector<uint8_t> const &bytes, size_t at)
{
	int32_t value = 0;

	memcpy(&value, bytes.data() + at, sizeof(value));
	return value;
}

static void StrategyPut32(std::vector<uint8_t> *bytes, size_t at, int32_t value)
{
	memcpy(bytes->data() + at, &value, sizeof(value));
}

struct LiftParts
{
	std::vector<std::array<uint8_t, 8>> positions;
	size_t tail = 0;          // where the door starts (24 + 8 x the count)
	size_t externalEnd = 0;   // where the external lifts' records end
};

// The parts of a Lift's payload; false when the counts are negative or the bytes do not hold what they say (a hostile file): such a lift is not changed by these edits (inteng would read past its lists;
// memory safety, named in S-K51-7 and S-K51-12)
static bool LiftPartsOf(std::vector<uint8_t> const &payload, LiftParts *parts)
{
	if (payload.size() < 24)
	{
		return false;
	}

	int32_t const count = StrategyGet32(payload, 20);

	if (count < 0 || (size_t)count > (payload.size() - 24) / 8)
	{
		return false;
	}

	parts->tail = 24 + 8 * (size_t)count;

	if (payload.size() - parts->tail < 40)
	{
		return false;
	}

	int32_t const external = StrategyGet32(payload, parts->tail + 32);

	if (external < 0 || (size_t)external > (payload.size() - parts->tail - 40) / 28)
	{
		return false;
	}

	parts->externalEnd = parts->tail + 36 + 28 * (size_t)external;
	parts->positions.clear();

	for (int32_t i = 0; i < count; i++)
	{
		std::array<uint8_t, 8> one;

		memcpy(one.data(), payload.data() + 24 + 8 * (size_t)i, 8);
		parts->positions.push_back(one);
	}

	return true;
}

// The payload with these positions in place of its own, everything else as it was
static bool LiftWithPositions(std::vector<uint8_t> const &payload, std::vector<std::array<uint8_t, 8>> const &positions, std::vector<uint8_t> *out)
{
	LiftParts parts;

	if (!LiftPartsOf(payload, &parts) || positions.size() > 4096)
	{
		return false;
	}

	out->assign(payload.begin(), payload.begin() + 24);
	StrategyPut32(out, 20, (int32_t)positions.size());

	for (std::array<uint8_t, 8> const &one : positions)
	{
		out->insert(out->end(), one.begin(), one.end());
	}

	out->insert(out->end(), payload.begin() + (std::ptrdiff_t)parts.tail, payload.end());
	return true;
}

// FUN_0052bcf0: a lift's Floor = how many of these positions' objects lie before the lift's own object in the order y, then z, then x (RBOBJECT +0x48, +0x4c, +0x44: the place, with inteng's 50-byte
// names); a position no object has is not counted, and with no level it is 0
static int32_t LiftFloorOf(uint8_t const *own, std::vector<std::array<uint8_t, 8>> const &positions)
{
	host::FoundObject me;

	// inteng reads through a null object when the lift's own is not in the level; it always is here (a module's view): 0 then
	if (!LevelObjects_Level() || !host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(own).c_str(), &me))
	{
		return 0;
	}

	int32_t floor = 0;

	for (std::array<uint8_t, 8> const &one : positions)
	{
		host::FoundObject at;

		if (host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(one.data()).c_str(), &at) &&
		    (at.y < me.y || (at.y == me.y && (at.z < me.z || (at.z == me.z && at.x < me.x)))))
		{
			floor++;
		}
	}

	return floor;
}

static bool StrategyObjectInLevel(uint8_t const *id)
{
	host::FoundObject found;

	return LevelObjects_Level() && host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found);
}

// FUN_0052b770 ("make the other positions agree", S-K51-12): this lift's Floor first; then each position whose object is in the level: no strategy -> a new Lift; a Lift -> its positions = this lift's with
// its own place taken by this lift's object, its external lifts = this lift's, its Floor worked out the same way, its flags 2, 4 and 8 = this lift's (its flag 1 and the rest kept). Other strategies: left alone.
static void LiftMakeAgree(uint8_t const *subject)
{
	int32_t kind = 0;
	std::vector<uint8_t> mine;
	LiftParts me;
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level() || !StrategyOfObject(subject, &kind, &mine) || kind != 51 || !LiftPartsOf(mine, &me))
	{
		return;
	}

	StrategyViewApply(subject, 51, "Floor", (double)LiftFloorOf(subject, me.positions));   // 0x52ba07

	uint32_t const myFlags = (uint32_t)StrategyGet32(mine, me.tail + 24);

	for (size_t i = 0; i < me.positions.size(); i++)
	{
		uint8_t const *const position = me.positions[i].data();

		if (!StrategyObjectInLevel(position))
		{
			continue;   // FUN_005295e0 with the level's list: not there, left alone
		}

		int32_t theirKind = 0;
		std::vector<uint8_t> theirs;

		if (!StrategyOfObject(position, &theirKind, &theirs))
		{
			// 0x52ba62-0x52bab4: no strategy, a new Lift (FUN_0052b2f0)
			if (host::AssignStrategy(open, position, 51) != host::ObjectEditResult::Done)
			{
				continue;
			}

			LoadTheObjectListAgain();

			if (!StrategyOfObject(position, &theirKind, &theirs))
			{
				continue;
			}
		}

		LiftParts them;

		if (theirKind != 51 || !LiftPartsOf(theirs, &them))
		{
			continue;
		}

		std::vector<std::array<uint8_t, 8>> list = me.positions;

		memcpy(list[i].data(), subject, 8);   // 0x52bb2c-0x52bb5f

		uint32_t const flags = ((uint32_t)StrategyGet32(theirs, them.tail + 24) & ~0xEu) | (myFlags & 0xEu);   // 0x52bbe9-0x52bc1e
		std::vector<uint8_t> bytes(theirs.begin(), theirs.begin() + 24);

		StrategyPut32(&bytes, 20, (int32_t)list.size());

		for (std::array<uint8_t, 8> const &one : list)
		{
			bytes.insert(bytes.end(), one.begin(), one.end());
		}

		bytes.insert(bytes.end(), theirs.begin() + (std::ptrdiff_t)them.tail, theirs.begin() + (std::ptrdiff_t)(them.tail + 24));   // door, call switch, floor switch
		bytes.resize(bytes.size() + 8);
		StrategyPut32(&bytes, bytes.size() - 8, (int32_t)flags);
		StrategyPut32(&bytes, bytes.size() - 4, LiftFloorOf(position, list));   // 0x52bbd4: its Floor from its new list
		bytes.insert(bytes.end(), mine.begin() + (std::ptrdiff_t)(me.tail + 32), mine.begin() + (std::ptrdiff_t)me.externalEnd);   // 0x52bb6e-0x52bbbc: the external lifts
		bytes.insert(bytes.end(), theirs.begin() + (std::ptrdiff_t)them.externalEnd, theirs.end());                              // the Facing (and anything after the records) kept
		WriteStrategyBytes(open, position, bytes);
	}
}

// The positions block's click (0x52c765-0x52c7ab, FUN_0052b360 with edit; S-K51-6, -7): the camera first goes to the position clicked, then the collector runs with the positions in its list; its end is
// LiftPositionsEnd. The list given to the collector holds the positions the level has: an ID no object has would be a null block that inteng reads through (memory safety: left out, named in S-K51-7)
static void LiftPositionsStart(uint8_t const *subject, std::vector<uint8_t> const &payload, int32_t index)
{
	std::vector<std::array<uint8_t, 8>> positions;

	host::StrategyView_LiftPositions(payload, &positions);

	if (index >= 0 && (size_t)index < positions.size())
	{
		StrategyFlyTo(positions[(size_t)index].data());   // FUN_005294f0 + FUN_00529280
	}

	copyTargets.clear();

	for (std::array<uint8_t, 8> const &one : positions)
	{
		if (StrategyObjectExists(one.data()))
		{
			CopyTarget target = {};

			memcpy(target.id, one.data(), 8);
			copyTargets.push_back(target);
		}
	}

	memcpy(copyFrom, subject, 8);
	copyPurpose = 1;
	copyCollecting = true;
}

// The collector's end (0x52b443-0x52b6fd): the lift's own object is taken out, then every object that cannot take a Lift (its class's +8(0x33) is 0: anything but a module); what is left are the positions, in the
// list's order. None: no positions, nothing asked. Else "Would you like the other positions altered\nto agree with this one?" (FUN_00474a40, 0x5ee83c): Yes makes them agree (FUN_0052b770)
void LiftPositionsEnd()
{
	uint8_t subject[8];
	std::vector<std::array<uint8_t, 8>> kept;

	memcpy(subject, strategyEditId, 8);
	copyPurpose = 0;

	for (CopyTarget const &one : copyTargets)
	{
		int32_t kinds[16];
		size_t const count = memcmp(one.id, subject, 8) == 0 ? 0 : KindsTheObjectCanTake(one.id, kinds);
		bool can = false;

		for (size_t k = 0; k < count; k++)
		{
			can = can || kinds[k] == 51;
		}

		if (can)
		{
			std::array<uint8_t, 8> id;

			memcpy(id.data(), one.id, 8);
			kept.push_back(id);
		}
	}

	copyTargets.clear();

	int32_t kind = 0;
	std::vector<uint8_t> payload, bytes;
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !StrategyOfObject(subject, &kind, &payload) || kind != 51 || !LiftWithPositions(payload, kept, &bytes) ||
	    WriteStrategyBytes(open, subject, bytes) != host::ObjectEditResult::Done)
	{
		statusWords.Say("Edit Strategy: the lift's positions could not be written", StatusSayer::StrategyView);   // (ours: a lift whose bytes do not hold their lists, or a store that refused)
		return;
	}

	if (kept.empty())
	{
		if (keyDown[VK_ESCAPE])
		{
			StrategyViewEnd();   // Escape still held as the collector ends: the view's own check sees it
		}

		return;
	}

	uint8_t const *const keep = strategyEditId;
	std::array<uint8_t, 8> lift;

	memcpy(lift.data(), keep, 8);
	DialogConfirm("Would you like the other positions altered\nto agree with this one?", [lift](bool yes)   // 0x5ee83c
	{
		if (yes)
		{
			LiftMakeAgree(lift.data());
		}
	});
}

// A Lift's door (when it names one that is a Door in the level): bit 4 (open) = `open`
static void LiftDoorOpen(std::vector<uint8_t> const &liftPayload, bool open)
{
	uint8_t door[8];
	int32_t doorKind = 0, flags = 0;
	std::vector<uint8_t> doorBytes;

	if (!host::StrategyView_FieldObject(51, liftPayload, "AssocDoor", door) || !StrategyOfObject(door, &doorKind, &doorBytes) || doorKind != 50 || !StrategyObjectInLevel(door) ||
	    !host::StrategyView_FieldNumber(50, doorBytes, "DoorFlags", &flags))
	{
		return;
	}

	StrategyViewApply(door, 50, "DoorFlags", (double)(open ? ((uint32_t)flags | 4u) : ((uint32_t)flags & ~4u)));
}

// "Lift starts at" (S-K51-11): the window "Select Lift Start Position" (FUN_0052af00: 600 x 400, centred by FUN_0057c800) with "Here" (0x5edcb8) at (0x12, 0x14), 100 x 16, answering 2, and one button per
// position at (0x12, 40 + 20 i) with its name, answering 3 + i. Escape (1): nothing. Return answers 3, as every dialog's does: the first position.
static void LiftStartWindow(uint8_t const *subject, std::vector<uint8_t> const &payload)
{
	std::vector<std::array<uint8_t, 8>> positions;
	host::DialogFormSpec spec;
	host::DialogWidget here;

	host::StrategyView_LiftPositions(payload, &positions);
	spec.title = "Select Lift Start Position";   // 0x5ee78c
	spec.width = 600;
	spec.height = 400;
	here.type = host::DialogWidgetType::Button;
	here.text = "Here";
	here.box = { 0x12, 0x14, 100, 0x10 };
	here.answer = 2;
	spec.widgets.push_back(here);

	for (size_t i = 0; i < positions.size(); i++)
	{
		host::DialogWidget button;
		std::string name;

		button.type = host::DialogWidgetType::Button;
		button.text = StrategyViewNameOf(positions[i].data(), &name) ? name : std::string();   // FUN_005297f0: an ID no object has gives a null name (ours: no words)
		button.box = { 0x12, 40 + 20 * (int32_t)i, 100, 0x10 };
		button.answer = 3 + (int32_t)i;
		spec.widgets.push_back(button);
	}

	std::array<uint8_t, 8> lift;

	memcpy(lift.data(), subject, 8);
	host::Dialogs_Form(spec, [lift](int32_t answer, std::vector<host::DialogWidget> const &)
	{
		int32_t kind = 0, flags = 0;
		std::vector<uint8_t> mine;

		// 0x52c8bb: an answer of 2 or more; Escape (1) changes nothing
		if (answer < 2 || !StrategyOfObject(lift.data(), &kind, &mine) || kind != 51 || !host::StrategyView_FieldNumber(51, mine, "LiftFlags", &flags))
		{
			return;
		}

		bool const here = answer == 2;
		int32_t const chosen = answer - 3;   // -1 for "Here": no position is the start

		StrategyViewApply(lift.data(), 51, "LiftFlags", (double)(here ? ((uint32_t)flags | 1u) : ((uint32_t)flags & ~1u)));
		LiftDoorOpen(mine, here);   // 0x52c8db-0x52c949

		std::vector<std::array<uint8_t, 8>> list;

		host::StrategyView_LiftPositions(mine, &list);

		// 0x52c94f-0x52ca6d: each position in the level whose strategy is a Lift: flag 1 = it is the one chosen, and its door opens or closes the same way
		for (size_t i = 0; i < list.size(); i++)
		{
			int32_t theirKind = 0, theirFlags = 0;
			std::vector<uint8_t> theirs;

			if (!StrategyObjectInLevel(list[i].data()) || !StrategyOfObject(list[i].data(), &theirKind, &theirs) || theirKind != 51 ||
			    !host::StrategyView_FieldNumber(51, theirs, "LiftFlags", &theirFlags))
			{
				continue;
			}

			bool const start = (int32_t)i == chosen;

			StrategyViewApply(list[i].data(), 51, "LiftFlags", (double)(start ? ((uint32_t)theirFlags | 1u) : ((uint32_t)theirFlags & ~1u)));
			LiftDoorOpen(theirs, start);
		}
	});
}

// The door's pick ends (FUN_0052bd70, S-K51-8): the door is the null ID first; a picked object (not the lift's own, which `have` already leaves out) with a shape (not a virtual object) becomes the door, and
// the Facing comes from where it lies: d = its place less the lift's; |d.x| > |d.z| gives 1 when d.x > 0, else 3; otherwise 0 when d.z > 0, else 2. With no strategy it gets a new Door; a Door then gets
// flags 8 (switch activated, closed), plus 4 (open) when this lift starts here. Any other strategy on it is left alone.
static void LiftDoorPicked(uint8_t const *subject, int32_t kind, std::vector<uint8_t> const &payload, uint8_t const *picked)
{
	StrategyFieldSetObject(subject, kind, "AssocDoor", nullptr);

	host::FoundObject door, lift;

	if (!picked || StrategyObjectIsVirtual(picked) || !LevelObjects_Level() || !host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(picked).c_str(), &door) ||
	    !host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(subject).c_str(), &lift))
	{
		return;
	}

	StrategyFieldSetObject(subject, kind, "AssocDoor", picked);

	int64_t const dx = (int64_t)door.x - lift.x;
	int64_t const dz = (int64_t)door.z - lift.z;
	int32_t const facing = (dx < 0 ? -dx : dx) > (dz < 0 ? -dz : dz) ? (dx > 0 ? 1 : 3) : (dz > 0 ? 0 : 2);   // 0x52be92-0x52bec5

	StrategyViewApply(subject, kind, "Facing", (double)facing);

	int32_t doorKind = 0;

	if (!StrategyOfObject(picked, &doorKind, nullptr))
	{
		if (!LevelObjects_OpenForEdit() || host::AssignStrategy(LevelObjects_OpenForEdit(), picked, 50) != host::ObjectEditResult::Done)   // FUN_0052cb80
		{
			return;
		}

		LoadTheObjectListAgain();
		doorKind = 50;
	}

	if (doorKind == 50)
	{
		int32_t flags = 0;

		host::StrategyView_FieldNumber(kind, payload, "LiftFlags", &flags);
		StrategyViewApply(picked, 50, "DoorFlags", (double)(8u | ((flags & 1) != 0 ? 4u : 0u)));   // 0x52bf25-0x52bf5c
	}
}

// The call or floor switch's pick ends (FUN_0052bf80 / FUN_0052c170, S-K51-9, -10): the switch is the null ID first; a picked object with a shape becomes the switch. With no strategy, or one FUN_00521860
// takes for none (kind 53, 2002, or a placed object (62) of type 0 with extra data 0), it gets a new Binary Switch (FUN_0052d5e0). Then a Binary or Linked Switch (52 / 61): flags 0, mode 1, its target = this
// lift, request 0; a Multi or Area Switch (66 / 68): rest state 0, mode 1, and this lift is added to its targets with request 1 unless it is one already (FUN_00530650). Any other kind is left alone.
static void LiftSwitchPicked(uint8_t const *subject, int32_t kind, char const *field, uint8_t const *picked)
{
	StrategyFieldSetObject(subject, kind, field, nullptr);

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !picked || StrategyObjectIsVirtual(picked) || !StrategyObjectInLevel(picked))
	{
		return;
	}

	StrategyFieldSetObject(subject, kind, field, picked);

	int32_t switchKind = 0;
	std::vector<uint8_t> bytes;
	bool const has = StrategyOfObject(picked, &switchKind, &bytes);
	int32_t type = -1, extra = -1;

	if (has && switchKind == 62)
	{
		host::StrategyView_FieldNumber(62, bytes, "Type", &type);
		host::StrategyView_FieldNumber(62, bytes, "ExtraData", &extra);
	}

	if (!has || switchKind == 53 || switchKind == 2002 || (switchKind == 62 && type == 0 && extra == 0))
	{
		if (host::AssignStrategy(open, picked, 52) != host::ObjectEditResult::Done)
		{
			return;
		}

		LoadTheObjectListAgain();

		if (!StrategyOfObject(picked, &switchKind, &bytes))
		{
			return;
		}
	}

	if ((switchKind == 52 || switchKind == 61) && bytes.size() >= 48)
	{
		// 0x52c138-0x52c14d: flags (+0x14) 0, mode (+0x18) 1, target (+0x24) this lift, request (+0x2c) 0
		StrategyPut32(&bytes, 20, 0);
		StrategyPut32(&bytes, 24, 1);
		memcpy(bytes.data() + 36, subject, 8);
		StrategyPut32(&bytes, 44, 0);
		WriteStrategyBytes(open, picked, bytes);
	}
	else if ((switchKind == 66 || switchKind == 68) && bytes.size() >= 40)
	{
		int32_t const targets = StrategyGet32(bytes, 36);

		// a list the bytes do not hold: nothing is changed (inteng would read past it; memory safety, named in S-K51-9)
		if (targets < 0 || (size_t)targets > (bytes.size() - 40) / 12)
		{
			return;
		}

		StrategyPut32(&bytes, 20, 0);   // rest state
		StrategyPut32(&bytes, 24, 1);   // mode

		bool there = false;

		for (int32_t i = 0; i < targets && !there; i++)
		{
			there = memcmp(bytes.data() + 40 + 12 * (size_t)i, subject, 8) == 0;
		}

		if (!there)
		{
			uint8_t entry[12];
			int32_t const request = 1;

			memcpy(entry, subject, 8);
			memcpy(entry + 8, &request, 4);
			bytes.insert(bytes.begin() + (std::ptrdiff_t)(40 + 12 * (size_t)targets), entry, entry + 12);
			StrategyPut32(&bytes, 36, targets + 1);
		}

		WriteStrategyBytes(open, picked, bytes);
	}
}

// THE MISSION OBJECTIVE'S EDITS (70; reassess/strategy-data-kinds.md S-K70-2, -3, -7, -8). The alterations list: its count at +40, then 12 bytes each (the target's ID, its word), then the request targets.
static bool MissionWithTargets(std::vector<uint8_t> const &payload, std::vector<host::MissionTarget> const &targets, std::vector<uint8_t> *out)
{
	std::vector<host::MissionTarget> old;

	if (!host::StrategyView_MissionTargets(payload, &old) || targets.size() > 4096)
	{
		return false;
	}

	size_t const tail = 44 + 12 * old.size();

	out->assign(payload.begin(), payload.begin() + 44);
	StrategyPut32(out, 40, (int32_t)targets.size());

	for (host::MissionTarget const &one : targets)
	{
		uint8_t entry[12];

		memcpy(entry, one.id.data(), 8);
		memcpy(entry + 8, &one.word, 4);
		out->insert(out->end(), entry, entry + 12);
	}

	out->insert(out->end(), payload.begin() + (std::ptrdiff_t)tail, payload.end());
	return true;
}

// FUN_00532550 (S-K70-2, -3): a window 500 x 400, centred: "Mission Description" (0x5ef6a8) or "Completed Message" (0x5ef694) at (0x12, 0x12), a number box at (0x1e, 0x24), 300 x 0x12, holding the number, "Ok" (200, 0x15e)
// and "Cancel" (300, 0x15e), and a text area at (0x12, 0x50), 400 x 0xfa, with the message's text: always empty here (the table is never filled off Rebellion's network; ours draws an empty label there).
// Ok: the number is stored with no range check (FUN_00474340)
// THE OWNER'S ADDITION for a Message (11-18, the video picker): Play shows the number's FMV in the window's text area (fmv_preview.h) and the window comes back holding what was typed
// (`typed`); Ok, Cancel and Escape stop it
static int32_t const kMessageFmvPlay = 0x7311;   // ours: Play's answer (no inteng command)

static void MissionTextWindow(uint8_t const *subject, int32_t which, int32_t wanted = 70, std::string const *typed = nullptr)
{
	int32_t kind = 0, number = 0;
	std::vector<uint8_t> payload;
	char const *const field = wanted == 73 ? "message_string" : (which == 0 ? "mission_description_string" : "mission_complete_string");

	if (!StrategyOfObject(subject, &kind, &payload) || kind != wanted || !host::StrategyView_FieldNumber(wanted, payload, field, &number))
	{
		return;
	}

	host::DialogFormSpec spec;
	host::DialogWidget words, box, ok, cancel, text;

	spec.width = 500;
	spec.height = 400;
	words.type = host::DialogWidgetType::Label;
	words.text = wanted == 73 ? "Message" : (which == 0 ? "Mission Description" : "Completed Message");   // 0x5edb98, 0x5ef6a8, 0x5ef694
	words.box = { 0x12, 0x12, 300, 16 };
	box.type = host::DialogWidgetType::Edit;
	box.text = typed ? *typed : std::to_string((long long)number);
	box.box = { 0x1e, 0x24, 300, 0x12 };
	box.most = 10;

	// THE OWNER'S ADDITION for a Message (11-18): the number box lists the message numbers the data folder's FMVs\ holds an FMV for (fmv_files.h), lowest first; it still takes a typed
	// number, as inteng's box does. A pick says that FMV (SayMessageFmv)
	if (wanted == 73)
	{
		box.type = host::DialogWidgetType::Combo;
		box.editable = true;

		// its open list shows 13 rows and then scrolls (the owner 11-21: "We can't scroll down to see message 1001..."; dialog_box.h). Inteng's box took a typed number only, so
		// the list has no resource of its own: NAMED, it shows the rows of the DLL's own list of files, Sound Edit's wav names (121 dialog units high)
		box.listRows = host::DialogBox_ComboListRows(121);

		for (int32_t const one : host::FmvFiles_MessageNumbers(host::FmvFiles_Folder(theGameFolder)))
		{
			box.items.push_back(std::to_string((long long)one));
			box.selected = box.text == box.items.back() ? (int32_t)box.items.size() - 1 : box.selected;
		}

		spec.onChange = [](std::vector<host::DialogWidget> &widgets, size_t changed)
		{
			if (changed == 1 && changed < widgets.size())
			{
				SayMessageFmv((int32_t)strtol(widgets[changed].text.c_str(), nullptr, 10));
			}
		};
	}

	ok.type = host::DialogWidgetType::Button;
	ok.text = "Ok";
	ok.box = { 200, 0x15e, 0x32, 0x10 };
	ok.answer = host::kDialogOk;
	cancel = ok;
	cancel.text = "Cancel";
	cancel.box = { 300, 0x15e, 0x32, 0x10 };
	cancel.answer = host::kDialogEscape;
	text.type = host::DialogWidgetType::Label;
	text.box = { 0x12, 0x50, 400, 0xfa };
	spec.widgets = { words, box, ok, cancel, text };

	if (wanted == 73)
	{
		host::DialogWidget play = ok;

		play.text = "Play";
		play.box = { 0x1e + 300 + 10, 0x24, 0x32, 0x12 };
		play.answer = kMessageFmvPlay;
		spec.widgets.push_back(play);   // after the text area, which stays part 4: the preview plays in it
	}

	std::array<uint8_t, 8> objective;
	std::string const fieldName = field;

	memcpy(objective.data(), subject, 8);
	host::Dialogs_Form(spec, [objective, fieldName, wanted, which](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (wanted == 73 && answer == kMessageFmvPlay && widgets.size() >= 2)
		{
			// Play: the typed number's FMV in the text area of the window that comes back (part 4); it stops with that window
			std::string const typedText = widgets[1].text;
			int32_t const playNumber = (int32_t)strtol(typedText.c_str(), nullptr, 10);
			std::string const folder = host::FmvFiles_Folder(theGameFolder);
			std::string playWords;

			if (playNumber < 0 || playNumber > model::kMostMessageNumber)
			{
				playWords = host::FmvFiles_MessageWords(folder, playNumber);
			}
			else
			{
				host::FmvPreview_Start(folder, model::MessageFmvName(playNumber), layoutSettingsFile, [](int32_t *x, int32_t *y, int32_t *w, int32_t *h)
				{
					// this window only: its text area (part 4) and its Play (part 5)
					host::DialogRect area, play;

					if (!host::Dialogs_FormWidgetBox(4, &area) || !host::Dialogs_FormWidgetBox(5, &play))
					{
						return false;
					}

					*x = area.x;
					*y = area.y;
					*w = area.width;
					*h = area.height;
					return true;
				}, &playWords);
			}

			messageFmvWords = playWords;
			statusWords.Say(messageFmvWords.c_str(), StatusSayer::StrategyView);   // the view draws it (the owner's call 11-19, status_words.h)
			MissionTextWindow(objective.data(), which, 73, &typedText);
			return;
		}

		if (wanted == 73)
		{
			host::FmvPreview_Stop();   // the window is closing
		}

		if (answer != host::kDialogOk || widgets.size() < 2)
		{
			return;
		}

		int32_t const value = (int32_t)strtol(widgets[1].text.c_str(), nullptr, 10);

		StrategyViewApply(objective.data(), wanted, fieldName.c_str(), (double)value);

		// ours: the new number's FMV, once the strategy holds it (a number it refused keeps the refusal's words)
		int32_t storedKind = 0, stored = 0;
		std::vector<uint8_t> storedPayload;

		if (wanted == 73 && StrategyOfObject(objective.data(), &storedKind, &storedPayload) && storedKind == 73 && host::StrategyView_FieldNumber(73, storedPayload, "message_string", &stored) && stored == value)
		{
			SayMessageFmv(value);
		}
	});
}

// FUN_00532ea0 (S-K70-7): the camera first goes to the target row clicked; the collector starts with the targets in its list (an ID no object has is left out: memory safety, named in S-K70-7)
// FUN_0052ff10 (S-K66-5): the camera first goes to the target row (when T or the press was on one); the collector starts with the targets' objects in it, each once (an ID no object has is left out:
// memory safety, as S-K70-7), its words "Select targets for switch" (0x5ef178) at (200, h - 0x14)
static void MultiTargetsStart(uint8_t const *subject, std::vector<uint8_t> const &payload, int32_t index)
{
	std::vector<host::MultiTarget> targets;

	host::StrategyView_MultiTargets(payload, &targets);

	if (index >= 0 && (size_t)index < targets.size())
	{
		StrategyFlyTo(targets[(size_t)index].id.data());   // FUN_005294f0 + FUN_00529280 (0x52fe83-0x52fe9d)
	}

	copyTargets.clear();

	for (host::MultiTarget const &one : targets)
	{
		bool listed = false;

		for (CopyTarget const &have : copyTargets)
		{
			listed = listed || memcmp(have.id, one.id.data(), 8) == 0;   // 0x52ff90-0x52ffc4: each object once
		}

		if (!listed && StrategyObjectExists(one.id.data()))
		{
			CopyTarget target = {};

			memcpy(target.id, one.id.data(), 8);
			copyTargets.push_back(target);
		}
	}

	memcpy(copyFrom, subject, 8);
	copyPurpose = 4;
	copyCollecting = true;
	statusWords = "Select targets for switch";   // the collector's words along the foot (0x5ef178), on the status line for the harness as Copy Strategy's
}

// The collector's end (FUN_0052ff10 0x530029-0x5303c5): the switch's own object out; each old target in its order: when its object is still listed it is kept with its request, and so is every next
// entry with the same ID (a D copy), and the object comes off the list; when not, it goes; then each object left in the list is added with request 1 (0x53035f)
void MultiTargetsEnd()
{
	uint8_t subject[8];
	std::vector<std::array<uint8_t, 8>> list;

	memcpy(subject, strategyEditId, 8);
	copyPurpose = 0;

	for (CopyTarget const &one : copyTargets)
	{
		if (memcmp(one.id, subject, 8) != 0)
		{
			std::array<uint8_t, 8> id;

			memcpy(id.data(), one.id, 8);
			list.push_back(id);
		}
	}

	copyTargets.clear();

	int32_t kind = 0;
	std::vector<uint8_t> payload;
	std::vector<host::MultiTarget> old, now;

	if (!StrategyOfObject(subject, &kind, &payload) || (kind != 66 && kind != 68) || !host::StrategyView_MultiTargets(payload, &old))
	{
		return;
	}

	for (size_t i = 0; i < old.size(); )
	{
		bool kept = false;

		for (size_t at = 0; at < list.size(); at++)
		{
			if (list[at] == old[i].id)
			{
				list.erase(list.begin() + (std::ptrdiff_t)at);
				kept = true;
				break;
			}
		}

		if (!kept)
		{
			i++;
			continue;
		}

		// kept, with every next entry of the same ID (FUN_0042f5a0, 0x530227-0x53028b)
		std::array<uint8_t, 8> const id = old[i].id;

		while (i < old.size() && old[i].id == id)
		{
			now.push_back(old[i]);
			i++;
		}
	}

	for (std::array<uint8_t, 8> const &id : list)
	{
		host::MultiTarget added;

		added.id = id;
		added.request = 1u;   // 0x53035f
		now.push_back(added);
	}

	MultiWrite(subject, payload, now);
}

static void MissionTargetsStart(uint8_t const *subject, std::vector<uint8_t> const &payload, int32_t index)
{
	std::vector<host::MissionTarget> targets;

	host::StrategyView_MissionTargets(payload, &targets);

	if (index >= 0 && (size_t)index < targets.size())
	{
		StrategyFlyTo(targets[(size_t)index].id.data());
	}

	copyTargets.clear();

	for (host::MissionTarget const &one : targets)
	{
		if (StrategyObjectExists(one.id.data()))
		{
			CopyTarget target = {};

			memcpy(target.id, one.id.data(), 8);
			copyTargets.push_back(target);
		}
	}

	memcpy(copyFrom, subject, 8);
	copyPurpose = 2;
	copyCollecting = true;
}

// The collector's end: this objective's own object out; old entries still in the list kept in order with their words (and taken out of it), old ones not in it dropped; each object left must have a Mission Objective
// strategy (0x46 or 0x47), else it is taken out with "Targets must have mission objective strategies" (0x5ef85c, one box each); the rest are added with the word 3
void MissionTargetsEnd()
{
	uint8_t subject[8];
	std::vector<std::array<uint8_t, 8>> list;

	memcpy(subject, strategyEditId, 8);
	copyPurpose = 0;

	for (CopyTarget const &one : copyTargets)
	{
		if (memcmp(one.id, subject, 8) != 0)
		{
			std::array<uint8_t, 8> id;

			memcpy(id.data(), one.id, 8);
			list.push_back(id);
		}
	}

	copyTargets.clear();

	int32_t kind = 0;
	std::vector<uint8_t> payload, bytes;
	std::vector<host::MissionTarget> old, now;

	if (!StrategyOfObject(subject, &kind, &payload) || kind != 70 || !host::StrategyView_MissionTargets(payload, &old))
	{
		return;
	}

	for (host::MissionTarget const &entry : old)
	{
		for (size_t i = 0; i < list.size(); i++)
		{
			if (list[i] == entry.id)
			{
				now.push_back(entry);
				list.erase(list.begin() + (std::ptrdiff_t)i);
				break;
			}
		}
	}

	size_t refused = 0;

	for (std::array<uint8_t, 8> const &id : list)
	{
		int32_t theirKind = 0;

		if (StrategyOfObject(id.data(), &theirKind, nullptr) && (theirKind == 70 || theirKind == 71))
		{
			host::MissionTarget added;

			added.id = id;
			added.word = 3;
			now.push_back(added);
		}
		else
		{
			refused++;
		}
	}

	if (MissionWithTargets(payload, now, &bytes) && LevelObjects_OpenForEdit())
	{
		WriteStrategyBytes(LevelObjects_OpenForEdit(), subject, bytes);
	}

	// one message box for each object taken out (FUN_00474030, Ok)
	std::shared_ptr<std::function<void(size_t)>> boxes = std::make_shared<std::function<void(size_t)>>();

	*boxes = [boxes](size_t left)
	{
		if (left > 0)
		{
			DialogMessage("Targets must have mission objective strategies", [boxes, left]() { (*boxes)(left - 1); });
		}
	};
	(*boxes)(refused);
}

// P / V (S-K70-8): the alteration row under the pointer's y (from the window's top) has bit 2 (P, make possible) or bit 1 (V, make visible) of its word flipped
void MissionToggleUnderPointer(int32_t bit)
{
	int32_t kind = 0;
	std::vector<uint8_t> payload, bytes;
	std::vector<host::MissionTarget> targets;
	size_t index = 0;

	if (!StrategyOfObject(strategyEditId, &kind, &payload) || kind != 70 || !host::StrategyView_MissionTargets(payload, &targets) ||
	    !host::StrategyView_MissionTargetAt(payload, PointerRowInBox(), &index))
	{
		return;
	}

	targets[index].word ^= bit;

	if (MissionWithTargets(payload, targets, &bytes) && LevelObjects_OpenForEdit())
	{
		WriteStrategyBytes(LevelObjects_OpenForEdit(), strategyEditId, bytes);
	}
}

// THE TRACK OBJECT'S EDITS (72 / 80; reassess/strategy-data-kinds.md S-K72-2..7). trackPage is the page shown: -1 the main page, else the point whose page is open.
static bool TrackWrite(uint8_t const *subject, std::vector<uint8_t> const &payload, std::vector<host::TrackEffect> const &effects)
{
	std::vector<uint8_t> bytes;

	if (!host::StrategyView_TrackWith(payload, effects, &bytes) || !LevelObjects_OpenForEdit())
	{
		return false;
	}

	// a Hierarchy Object (76) lives in its placed hierarchy's PLACHIER (placed_hierarchy_strategy.h); a Track Object (72 / 80) in its placed object's OBJPRJDT
	if (host::PlacedHierarchyRef placedHierarchy; host::FindPlacedHierarchy(LevelObjects_Level(), subject, &placedHierarchy))
	{
		return host::PlacedHierarchyStrategy_Put(LevelObjects_OpenForEdit(), subject, bytes.data(), bytes.size()) == host::PlacedHierarchyStrategyResult::Done;
	}

	return WriteStrategyBytes(LevelObjects_OpenForEdit(), subject, bytes) == host::ObjectEditResult::Done;   // a placed object, or a virtual object (S-K70-7)
}

// a Track Object's (72 / 80) or a Hierarchy Object's (76) effects: the same list in the file (point effects; 76's "point" is the flag number, strategy_fields.cpp TrackFields)
static bool TrackNow(uint8_t const *subject, int32_t *kind, std::vector<uint8_t> *payload, std::vector<host::TrackEffect> *effects)
{
	return StrategyOfObject(subject, kind, payload) && (*kind == 72 || *kind == 80 || *kind == 76) && host::StrategyView_TrackEffects(*payload, effects);
}

// the object's track's sections (OBJTRAK2 +0x28), 0 when it has none
// An enemy's species (its AVPGENER's type: inteng's record +0x38, 7 marine, 2 predator ...), -1 when the ID is no enemy's or generator's
int32_t EnemySpeciesOf(uint8_t const *id)
{
	host::GeneratorRef generator;

	return LevelObjects_Level() && host::FindGenerator(LevelObjects_Level(), id, &generator) ? generator.type : -1;
}

// The level's paths (SPECLOBJ > AVPPATH2), read again when the level changes: FUN_004fbaf0 finds the one whose number (+0x34) is asked
static std::vector<host::PathInfo> levelPaths;
static unsigned levelPathsFor = ~0u;
static OpenLevelFile *levelPathsOpen = nullptr;

static std::vector<host::PathInfo> const &LevelPaths()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open && (levelPathsOpen != open || levelPathsFor != open->Generation()))
	{
		levelPaths.clear();

		if (!host::PathMode_ReadPaths(open, &levelPaths))
		{
			levelPaths.clear();
		}

		levelPathsOpen = open;
		levelPathsFor = open->Generation();
	}

	return levelPaths;
}

bool LevelPathNamed(int32_t number, std::string *name)
{
	for (host::PathInfo const &one : LevelPaths())
	{
		if (one.number == number)
		{
			*name = one.name;
			return true;
		}
	}

	return false;
}

// "Select Mission Type" (an enemy's: FUN_00531fb0 a marine's six, FUN_00532230 a predator's two; a generator's: FUN_005323c0): 400 x 280, centred; buttons 200 x 16 at x 20, y 20, 40 ...; "Cancel" at
// (200, 250), 50 x 16. A button sets the Mission (the button methods 0x5321f0 / 0x532380 / 0x532510), and when that changes it, ExtraMissionData = 0; Cancel or Escape: nothing
// THE SWITCH'S "Switch Properties" FORM (52 / 61: P, or a press on y 0x20..0x4f; strategy-data.md S-K52-3 / S-K52-6; re-read 11-16 on dump-full-3:65201 FUN_0052e1a0, 65498 FUN_0052e7f0
// and the form's own method 0x52ece0 in listing-all-3). FUN_0057d170(400, 0x15e) titled "Switch Properties" (0x5eef94); the parts in inteng's order (their id is their place: the Timer box 1,
// Wait 2, Toggle 3, Rest 4, Ok 14, Cancel 15, Time Delay 0x11, Time Delay (Autoexec) 0x13, Reversed 0x15, Same 0x17, None 0x19). The five mode boxes are one group and the three switch-off boxes
// another (0x52ed23-0x52ee2b: the pressed one set, the others cleared). Filled from the switch (FUN_0052e7f0 0x52e7fa-0x52e8ed): its mode's box ticked (3: none), Security, Rest = +0x14 & 1, the
// seconds "%.3f" of (float)+0x1c / 65536 with the trailing zeros and the point cut, & 2 Same / else & 4 None / else Reversed. Ok (14, 0x52ed8a): the number box's value (FUN_0057d620) below 0 ->
// "Security must be at least 0" (0x5eefb0) and the form stays; else (0x52e90a-0x52ea4a) the mode from the first ticked of Timer 0, Wait 1, Toggle 2, Time Delay 4, else 5 (a Linked Switch's 3 or 4
// is 0); Security; Rest bit 1; bits 2 / 4 from Same / None; the seconds read by sscanf "%f" (0.0 when nothing is read), kept when they differ from now and lie in 0..30000, as ftol(s x 65536).
// The Ok button's method answers 2 (0x52ece0) and the form is kept only on 2 (0x52e901): Return (the toolkit's 3), Cancel (1) and Escape keep nothing.
static int32_t const kSwitchFormOk = 2;

static void SwitchPropertiesWindow(uint8_t const *subject, int32_t kind, std::vector<host::DialogWidget> const *again)
{
	std::vector<uint8_t> payload;
	int32_t had = 0;

	if (!StrategyOfObject(subject, &had, &payload) || had != kind)
	{
		return;
	}

	int32_t mode = 0, flags = 0, time = 0, security = 0;

	host::StrategyView_FieldNumber(kind, payload, "Mode", &mode);
	host::StrategyView_FieldNumber(kind, payload, "flags", &flags);
	host::StrategyView_FieldNumber(kind, payload, "Time", &time);
	host::StrategyView_FieldNumber(kind, payload, "Security", &security);

	// 0x52e85f-0x52e8b3: "%.3f" (0x5eefa8), then the zeros at the end and a point left last are cut
	char seconds[64];

	snprintf(seconds, sizeof(seconds), "%.3f", (double)((float)time * 1.5258789e-05f));

	if (strchr(seconds, '.'))
	{
		size_t end = strlen(seconds);

		while (end > 0 && seconds[end - 1] == '0')
		{
			seconds[--end] = '\0';
		}

		if (end > 0 && seconds[end - 1] == '.')
		{
			seconds[--end] = '\0';
		}
	}

	host::DialogFormSpec spec;

	spec.title = "Switch Properties";   // 0x5eef94
	spec.width = 400;
	spec.height = 0x15e;

	auto const add = [&spec](host::DialogWidgetType type, std::string const &words, int32_t x, int32_t y, int32_t w, int32_t h, int32_t group, bool on, int32_t answer, size_t most)
	{
		host::DialogWidget one;

		one.type = type;
		one.text = words;
		one.box = { x, y, w, h };
		one.group = group;
		one.checked = on;
		one.answer = answer;
		one.most = most;
		spec.widgets.push_back(one);
	};

	uint32_t const bits = (uint32_t)flags;

	add(host::DialogWidgetType::Check, "", 0x10, 0x28, 16, 16, 0, mode == 0, 0, 0);                                    // 1 Timer
	add(host::DialogWidgetType::Check, "", 0x10, 100, 16, 16, 0, mode == 1, 0, 0);                                     // 2 Wait
	add(host::DialogWidgetType::Check, "", 0x10, 0x78, 16, 16, 0, mode == 2, 0, 0);                                    // 3 Toggle
	int32_t restWord = 0;

	host::StrategyView_FieldNumber(kind, payload, "RestState", &restWord);

	// 4 Rest state on: 52 / 61 bit 0 of +0x14 (FUN_0052e7f0); a Multiple Target Switch's whole +0x14 word (FUN_0052ea70 0x52eac7)
	add(host::DialogWidgetType::Check, "", 0xf0, 0xb4, 16, 16, -1, kind == 66 || kind == 68 ? restWord != 0 : (bits & 1u) != 0, 0, 0);
	add(host::DialogWidgetType::Edit, seconds, 0x84, 0x28, 100, 0x12, -1, false, 0, 0x32);                              // 5 the seconds (FUN_0057d8e0, 50 letters)
	add(host::DialogWidgetType::Label, "Timer", 0x20, 0x28, 100, 0x12, -1, false, 0, 0);                               // 0x5eef8c
	add(host::DialogWidgetType::Label, "Wait", 0x20, 100, 100, 0x12, -1, false, 0, 0);                                 // 0x5eef84
	add(host::DialogWidgetType::Label, "Toggle", 0x20, 0x78, 100, 0x12, -1, false, 0, 0);                              // 0x5eef7c
	add(host::DialogWidgetType::Label, "Mode :", 0x10, 0x14, 100, 0x12, -1, false, 0, 0);                              // 0x5eef74
	add(host::DialogWidgetType::Label, "seconds", 0xf0, 0x28, 100, 0x12, -1, false, 0, 0);                             // 0x5eef6c
	add(host::DialogWidgetType::Edit, std::to_string((long long)security), 0x8c, 0x8c, 0x3c, 0x12, -1, false, 0, 10);   // 11 Security (FUN_0057de30)
	add(host::DialogWidgetType::Label, "Security :", 0x10, 0x8c, 0x78, 0x12, -1, false, 0, 0);                         // 0x5eef60
	add(host::DialogWidgetType::Label, "Rest state on", 0x8c, 0xb4, 100, 0x12, -1, false, 0, 0);                       // 0x5eef50
	add(host::DialogWidgetType::Button, "Ok", 100, 300, 0x32, 0x10, -1, false, kSwitchFormOk, 0);                       // 14
	add(host::DialogWidgetType::Button, "Cancel", 200, 300, 0x32, 0x10, -1, false, host::kDialogEscape, 0);             // 15
	add(host::DialogWidgetType::Label, "Time Delay", 0x20, 0x3c, 100, 0x12, -1, false, 0, 0);                          // 0x5eef44
	add(host::DialogWidgetType::Check, "", 0x10, 0x3c, 16, 16, 0, mode == 4, 0, 0);                                    // 0x11 Time Delay
	add(host::DialogWidgetType::Label, "Time Delay (Autoexec)", 0x20, 0x50, 200, 0x12, -1, false, 0, 0);               // 0x5eef2c
	add(host::DialogWidgetType::Check, "", 0x10, 0x50, 16, 16, 0, mode == 5, 0, 0);                                    // 0x13 Time Delay (Autoexec)
	add(host::DialogWidgetType::Label, "Switch off message :", 0x10, 200, 100, 0x12, -1, false, 0, 0);                 // 0x5eef14
	add(host::DialogWidgetType::Check, "", 0x10, 0xdc, 16, 16, 1, (bits & 6u) == 0, 0, 0);                             // 0x15 Reversed
	add(host::DialogWidgetType::Label, "Reversed (default)", 0x20, 0xdc, 200, 0x12, -1, false, 0, 0);                  // 0x5eef00
	add(host::DialogWidgetType::Check, "", 0x10, 0xf0, 16, 16, 1, (bits & 2u) != 0, 0, 0);                             // 0x17 Same
	add(host::DialogWidgetType::Label, "Same as switch on", 0x20, 0xf0, 200, 0x12, -1, false, 0, 0);                   // 0x5eeeec
	add(host::DialogWidgetType::Check, "", 0x10, 0x104, 16, 16, 1, (bits & 2u) == 0 && (bits & 4u) != 0, 0, 0);        // 0x19 None
	add(host::DialogWidgetType::Label, "None", 0x20, 0x104, 200, 0x12, -1, false, 0, 0);                               // 0x5d126c

	if (again && again->size() == spec.widgets.size())
	{
		spec.widgets = *again;   // the form shown again after the Security message, as the person left it
	}

	std::array<uint8_t, 8> who;

	memcpy(who.data(), subject, 8);
	host::Dialogs_Form(spec, [who, kind](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != kSwitchFormOk || widgets.size() < 26)
		{
			return;   // Return, Cancel or Escape: nothing kept
		}

		int32_t const security = host::HierarchyMode_EditNumber(widgets[10].text);   // FUN_0057d620

		if (security < 0)
		{
			std::vector<host::DialogWidget> const kept = widgets;

			DialogMessage("Security must be at least 0", [who, kind, kept]() { SwitchPropertiesWindow(who.data(), kind, &kept); });   // 0x5eefb0
			return;
		}

		std::vector<uint8_t> payload;
		int32_t had = 0, flags = 0, time = 0;

		if (!StrategyOfObject(who.data(), &had, &payload) || had != kind)
		{
			return;
		}

		host::StrategyView_FieldNumber(kind, payload, "flags", &flags);
		host::StrategyView_FieldNumber(kind, payload, "Time", &time);

		int32_t mode = widgets[0].checked ? 0 : widgets[1].checked ? 1 : widgets[2].checked ? 2 : widgets[16].checked ? 4 : 5;   // 0x52e90a-0x52e95d

		if (kind == 61 && (mode == 3 || mode == 4))
		{
			mode = 0;   // 0x52e967-0x52e97a
		}

		// 52 / 61: Rest is bit 0 of +0x14 (0x52e98f-0x52e9a9); 66: +0x14 is the box's value, a whole word, and the flags (+0x38) keep their bit 0 (FUN_0052ea70 0x52ebf2-0x52ebfd)
		uint32_t bits = kind == 66 || kind == 68 ? (uint32_t)flags : (widgets[3].checked ? ((uint32_t)flags | 1u) : ((uint32_t)flags & ~1u));

		if (kind == 66 || kind == 68)
		{
			StrategyViewApply(who.data(), kind, "RestState", widgets[3].checked ? 1.0 : 0.0);
		}

		bits &= ~6u;
		bits |= widgets[22].checked ? 2u : (widgets[24].checked ? 4u : 0u);   // 0x52e9ac-0x52e9dc

		float typed = 0.0f;

		sscanf_s(widgets[4].text.c_str(), "%f", &typed);   // 0x52e9df-0x52e9ea: "%f" (0x5d3f10)

		StrategyViewApply(who.data(), kind, "Mode", (double)mode);
		StrategyViewApply(who.data(), kind, "Security", (double)security);
		StrategyViewApply(who.data(), kind, "flags", (double)bits);

		if ((float)time * 1.5258789e-05f != typed && typed >= 0.0f && typed <= 30000.0f)   // 0x52ea02-0x52ea36
		{
			StrategyViewApply(who.data(), kind, "Time", (double)(int32_t)((double)typed * 65536.0));   // ftol(s x 65536.0)
		}
	});
}

static void MissionTypeWindow(uint8_t const *subject, int32_t kind, std::vector<std::pair<std::string, int32_t>> const &buttons)
{
	host::DialogFormSpec spec;

	spec.title = "Select Mission Type";   // 0x5ef660
	spec.width = 400;
	spec.height = 280;

	for (size_t i = 0; i < buttons.size(); i++)
	{
		host::DialogWidget button;

		button.type = host::DialogWidgetType::Button;
		button.text = buttons[i].first;
		button.box = { 20, 20 + 20 * (int32_t)i, 200, 16 };
		button.answer = 10 + (int32_t)i;
		spec.widgets.push_back(button);
	}

	host::DialogWidget cancel;

	cancel.type = host::DialogWidgetType::Button;
	cancel.text = "Cancel";
	cancel.box = { 200, 250, 50, 16 };
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(cancel);

	std::array<uint8_t, 8> who;

	memcpy(who.data(), subject, 8);
	host::Dialogs_Form(spec, [who, kind, buttons](int32_t answer, std::vector<host::DialogWidget> const &)
	{
		int32_t had = 0, now = 0;
		std::vector<uint8_t> payload;

		if (answer < 10 || (size_t)(answer - 10) >= buttons.size() || !StrategyOfObject(who.data(), &had, &payload) || had != kind || !host::StrategyView_FieldNumber(kind, payload, "MissionType", &now))
		{
			return;
		}

		int32_t const chosen = buttons[(size_t)(answer - 10)].second;

		if (chosen != now)
		{
			StrategyViewApply(who.data(), kind, "MissionType", (double)chosen);
			StrategyViewApply(who.data(), kind, "ExtraMissionData", 0.0);
		}
	});
}

// "Select Path" (FUN_004fb6a0 -> FUN_0045dd10: the list window with a row per path of the level, its number, two spaces, its name); a path chosen: ExtraMissionData = (old & 0x0ffff000) | its number
// (0x531a63 / 0x531ef2); Escape: nothing
static void SelectPathWindow(uint8_t const *subject, int32_t kind)
{
	std::vector<host::PathInfo> const paths = LevelPaths();
	std::vector<std::string> rows;

	for (host::PathInfo const &one : paths)
	{
		rows.push_back(std::to_string((long long)one.number) + "  " + one.name);
	}

	std::array<uint8_t, 8> who;

	memcpy(who.data(), subject, 8);
	DialogList("Select Path", rows, [who, kind, paths](int32_t choice)   // 0x5e97e8
	{
		int32_t had = 0, extra = 0;
		std::vector<uint8_t> payload;

		if (choice < 0 || (size_t)choice >= paths.size() || !StrategyOfObject(who.data(), &had, &payload) || had != kind || !host::StrategyView_FieldNumber(kind, payload, "ExtraMissionData", &extra))
		{
			return;
		}

		StrategyViewApply(who.data(), kind, "ExtraMissionData", (double)(int32_t)(((uint32_t)extra & 0x0ffff000u) | (uint32_t)paths[(size_t)choice].number));
	});
}

// The view's end for an Enemy (69) or a Generator (79): a path follower (a marine's Mission 5, a predator's 1, a generator's 1) keeps the path's number (ExtraMissionData & 0xffff); when the level has that
// path, the module holding its place (FUN_00474de0: the first module, in the level's order, whose box holds the place, its edges included) is the start, and the path's module nearest to it by the
// breadth-first distance over the modules' adjacency (FUN_004f89f0 / FUN_004f8240: PathMode_BuildGraph; ties: the first module in the graph's order) gives its object number to the high 16 bits
// (FUN_004f9650: its record +0x98), which the game turns into the step to start at (ConvertObjectIndexToPathIndex). Any other Mission or species: ExtraMissionData = 0. NAMED: a place no module holds
// is an assert in inteng (PathMode.cpp line 0x1ac); here the high bits stay 0.
static void EnemyPathFixAtEnd(uint8_t const *id, int32_t kind)
{
	int32_t had = 0, mission = 0, extra = 0;
	std::vector<uint8_t> payload;
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !StrategyOfObject(id, &had, &payload) || had != kind || !host::StrategyView_FieldNumber(kind, payload, "MissionType", &mission) ||
	    !host::StrategyView_FieldNumber(kind, payload, "ExtraMissionData", &extra))
	{
		return;
	}

	int32_t const species = kind == 69 ? EnemySpeciesOf(id) : -1;
	bool const follower = kind == 69 ? ((species == 7 && mission == 5) || (species == 2 && mission == 1)) : mission == 1;

	if (!follower)
	{
		if (extra != 0)
		{
			StrategyViewApply(id, kind, "ExtraMissionData", 0.0);
		}

		return;
	}

	uint32_t value = (uint32_t)extra & 0xffffu;
	host::PathInfo const *path = nullptr;

	for (host::PathInfo const &one : LevelPaths())
	{
		if (one.number == (int32_t)value && !path)
		{
			path = &one;
		}
	}

	host::GeneratorRef generator;
	std::vector<host::ModuleBox> boxes;
	host::PathGraph graph;
	std::vector<int32_t> numbers;
	std::vector<std::string> names;

	if (path && !path->nodes.empty() && host::FindGenerator(open->Level(), id, &generator) && host::ModuleOverlaps_ReadBoxes(open, &boxes) &&
	    host::PathMode_BuildGraph(open, &graph, &numbers, &names) && graph.neighbours.size() == numbers.size())
	{
		// the module holding the place: extents are max x, min x, max y, min y, max z, min z about the module's place
		int32_t startNumber = -1;

		for (host::ModuleBox const &one : boxes)
		{
			bool inside = true;

			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const least = (int64_t)one.place[axis] + one.extents[2 * axis + 1];
				int64_t const most = (int64_t)one.place[axis] + one.extents[2 * axis];

				inside = inside && generator.place[axis] >= least && generator.place[axis] <= most;
			}

			if (inside)
			{
				startNumber = one.objectIndex;
				break;
			}
		}

		int start = -1;

		for (size_t i = 0; i < numbers.size() && start < 0; i++)
		{
			start = numbers[i] == startNumber ? (int)i : start;
		}

		if (start >= 0)
		{
			int const chosen = host::PathMode_NearestStep(graph, numbers, start, path->nodes);   // FUN_004f9650

			if (chosen >= 0)
			{
				value |= (uint32_t)numbers[(size_t)chosen] << 16;
			}
		}
	}

	if ((int32_t)value != extra)
	{
		StrategyViewApply(id, kind, "ExtraMissionData", (double)(int32_t)value);
	}
}

int32_t TrackSectionsOf(uint8_t const *id)
{
	host::TrackLoaded loaded;

	if (!LevelObjects_OpenForEdit() || host::TrackLoad(LevelObjects_OpenForEdit(), id, &loaded) != host::TrackEditResult::Done || !loaded.had)
	{
		return 0;
	}

	return (int32_t)loaded.track.sections.size();
}

// "Which track point?" (S-K72-2; the whole-number dialog holding 0): Ok and 0 <= n < sections + 1 -> the effect for point n (a new one with no targets, put in its place in the list kept sorted by point) and its page
void TrackAskPoint(uint8_t const *subject)
{
	host::DialogField field;
	std::array<uint8_t, 8> track;
	int32_t asked = 0;

	field.text = "0";
	memcpy(track.data(), subject, 8);

	// a Hierarchy Object (76) asks "Which flag?" (0x5efae8): Ok and 0 <= n < 32 (FUN_00533a40 0x533bf4-0x533c0e; S-K76-2)
	bool const flags = StrategyOfObject(subject, &asked, nullptr) && asked == 76;

	DialogText(flags ? "Which flag?" : "Which track point?", std::vector<host::DialogField>(1, field), [track, flags](int32_t answer, std::vector<std::string> const &texts)   // 0x5efb98
	{
		int32_t kind = 0;
		std::vector<uint8_t> payload;
		std::vector<host::TrackEffect> effects;

		if (answer != host::kDialogOk || texts.empty() || !TrackNow(track.data(), &kind, &payload, &effects))
		{
			return;
		}

		int32_t const n = (int32_t)strtol(texts[0].c_str(), nullptr, 10);

		if (n < 0 || n >= (flags ? 32 : TrackSectionsOf(track.data()) + 1))
		{
			return;
		}

		size_t at = 0;

		while (at < effects.size() && effects[at].point < n)
		{
			at++;
		}

		if (at == effects.size() || effects[at].point != n)
		{
			host::TrackEffect added;

			added.point = n;
			added.pad = 0;
			effects.insert(effects.begin() + (std::ptrdiff_t)at, added);

			if (!TrackWrite(track.data(), payload, effects))
			{
				return;
			}
		}

		trackPage = n;
	});
}

// a target row's request (x < 50: 1 when it was 0, else 0) or direction (50 <= x < 190: Forward only 1 -> Backward only 2 -> Opposite back 7 -> Both directions 3 -> Forward only, 0x534c8a-0x534cbc)
static void TrackChangeTarget(uint8_t const *subject, int32_t row, bool direction)
{
	int32_t kind = 0;
	std::vector<uint8_t> payload;
	std::vector<host::TrackEffect> effects;

	if (!TrackNow(subject, &kind, &payload, &effects))
	{
		return;
	}

	for (host::TrackEffect &effect : effects)
	{
		if (effect.point == trackPage && row >= 0 && (size_t)row < effect.targets.size())
		{
			host::TrackTarget &t = effect.targets[(size_t)row];
			uint32_t const d = (uint32_t)t.direction;

			if (!direction)
			{
				t.request = t.request == 0 ? 1 : 0;   // any extra data is lost
			}
			else if ((d & 1u) != 0 && (d & 2u) != 0)
			{
				t.direction = (int32_t)((((uint32_t)(int32_t)(int8_t)(d & 0xffu) & 4u) | 2u) >> 1);   // 3 -> 1, 7 -> 3
			}
			else
			{
				t.direction = (d & 1u) != 0 ? 2 : 7;   // 1 -> 2, no bit 1 -> 7
			}

			TrackWrite(subject, payload, effects);
			return;
		}
	}
}

// The targets editor of a point page (FUN_00534df0): the camera first goes to the target row clicked; the collector starts with the page's targets (an ID no object has is left out: memory safety)
static void TrackTargetsStart(uint8_t const *subject, int32_t row)
{
	int32_t kind = 0;
	std::vector<uint8_t> payload;
	std::vector<host::TrackEffect> effects;

	if (!TrackNow(subject, &kind, &payload, &effects))
	{
		return;
	}

	copyTargets.clear();

	for (host::TrackEffect const &effect : effects)
	{
		if (effect.point != trackPage)
		{
			continue;
		}

		if (row >= 0 && (size_t)row < effect.targets.size())
		{
			StrategyFlyTo(effect.targets[(size_t)row].id.data());   // FUN_005294f0 + FUN_00529280 (0x534cd1)
		}

		for (host::TrackTarget const &t : effect.targets)
		{
			if (StrategyObjectExists(t.id.data()))
			{
				CopyTarget target = {};

				memcpy(target.id, t.id.data(), 8);
				copyTargets.push_back(target);
			}
		}
	}

	memcpy(copyFrom, subject, 8);
	copyPurpose = 3;
	copyCollecting = true;
}

// The collector's end: the track's own object out; old targets still listed kept in order with their words; the others go; new ones added with request 1 (on) and direction 3 (both)
void TrackTargetsEnd()
{
	uint8_t subject[8];
	std::vector<std::array<uint8_t, 8>> list;

	memcpy(subject, strategyEditId, 8);
	copyPurpose = 0;

	for (CopyTarget const &one : copyTargets)
	{
		if (memcmp(one.id, subject, 8) != 0)
		{
			std::array<uint8_t, 8> id;

			memcpy(id.data(), one.id, 8);
			list.push_back(id);
		}
	}

	copyTargets.clear();

	int32_t kind = 0;
	std::vector<uint8_t> payload;
	std::vector<host::TrackEffect> effects;

	if (!TrackNow(subject, &kind, &payload, &effects))
	{
		return;
	}

	for (host::TrackEffect &effect : effects)
	{
		if (effect.point != trackPage)
		{
			continue;
		}

		std::vector<host::TrackTarget> now;

		for (host::TrackTarget const &t : effect.targets)
		{
			for (size_t i = 0; i < list.size(); i++)
			{
				if (list[i] == t.id)
				{
					now.push_back(t);
					list.erase(list.begin() + (std::ptrdiff_t)i);
					break;
				}
			}
		}

		for (std::array<uint8_t, 8> const &id : list)
		{
			host::TrackTarget added;

			added.id = id;
			added.request = 1;
			added.direction = 3;
			now.push_back(added);
		}

		effect.targets = now;
		TrackWrite(subject, payload, effects);
		return;
	}
}

// The right button on a point page (S-K72-7): back to the main page; an effect left with no targets is taken out of the list. NAMED (memory safety): inteng shifts the list from the index = the effect's POINT
// NUMBER (0x534d4b), not its place: with the effect not last that keeps the freed effect and drops the next one; ours takes out the effect itself
void TrackPageBack()
{
	int32_t kind = 0;
	std::vector<uint8_t> payload;
	std::vector<host::TrackEffect> effects;
	int32_t const point = trackPage;

	trackPage = -1;

	if (!TrackNow(strategyEditId, &kind, &payload, &effects))
	{
		return;
	}

	for (size_t i = 0; i < effects.size(); i++)
	{
		if (effects[i].point == point && effects[i].targets.empty())
		{
			effects.erase(effects.begin() + (std::ptrdiff_t)i);
			TrackWrite(strategyEditId, payload, effects);
			return;
		}
	}
}

// The end of an object pick (Return or the right button): the row's field takes the picked object, then what the row does more (reassess/strategy-data-kinds.md S-KC-5 and the rows named)
void StrategyPickEnd()
{
	std::string const field = strategyPickField;
	int32_t const row = strategyPickRow;
	std::string const request = strategyPickRequest;
	// the Escape slot (0xd87a45) is set while the key is HELD (FUN_0058c010 sets it, FUN_0058c5e0 clears it when the key comes up): the pick does not read it, and the view ends after the pick only when it is still down
	bool const escaped = keyDown[VK_ESCAPE];
	uint8_t subject[8];
	uint8_t picked[8];

	memcpy(subject, strategyEditId, 8);
	memcpy(picked, strategyPickId, 8);

	// the picked object is never the view's own object (FUN_00527540's callers compare with the wrapper's block)
	bool const have = strategyPickHave && memcmp(picked, subject, 8) != 0;

	strategyPickField.clear();
	strategyViewPickWords.clear();
	strategyPickHave = false;
	strategyPickRow = 0;
	strategyPickRequest.clear();

	int32_t kind = 0;
	std::vector<uint8_t> payload;

	if (!StrategyOfObject(subject, &kind, &payload))
	{
		StrategyViewEnd();
		return;
	}

	uint8_t before[8] = { 0 };
	bool const hadOne = host::StrategyView_FieldObject(kind, payload, field.c_str(), before);

	if (row == host::kPickTarget)
	{
		// FUN_00531160 / FUN_00531b70 / FUN_005353c0: the target = the picked object's ID or the null ID; changed -> the request = 1 (on, no extra data)
		StrategyFieldSetObject(subject, kind, field.c_str(), have ? picked : nullptr);

		uint8_t none[8] = { 0 };
		bool const changed = memcmp(hadOne ? before : none, have ? picked : none, 8) != 0;

		if (changed && !request.empty())
		{
			StrategyViewApply(subject, kind, request.c_str(), 1.0);
		}
	}
	else if (row == host::kPickSwitchTarget)
	{
		// FUN_0052db10 0x52dba7-0x52dc07 (S-K52-4): the target = the null ID, then the picked object's when one was picked (never the switch's own: `have`); a changed target -> +0x2c = 0
		StrategyFieldSetObject(subject, kind, field.c_str(), have ? picked : nullptr);

		uint8_t none[8] = { 0 };
		bool const changed = memcmp(hadOne ? before : none, have ? picked : none, 8) != 0;

		if (changed && !request.empty())
		{
			StrategyViewApply(subject, kind, request.c_str(), 0.0);
		}
	}
	else if (row == host::kPickSwitchDoor || row == host::kPickTeleport)
	{
		// FUN_0052d490 / FUN_00530ce0: the field = the picked object when it has a shape (a module or placed object, not a virtual one), else the null ID. That object with no strategy gets a new one of the same kind
		// (its wrapper's +0xc, FUN_0052d020 / FUN_00530b10) naming this one; a Switch Door already there is made to name this one; any other strategy on it is left alone
		bool const shaped = have && !StrategyObjectIsVirtual(picked);
		int32_t const pairKind = row == host::kPickSwitchDoor ? 59 : 67;
		char const *const pairField = row == host::kPickSwitchDoor ? "AssocDoor" : "TeleportTo";

		StrategyFieldSetObject(subject, kind, field.c_str(), shaped ? picked : nullptr);

		if (shaped)
		{
			int32_t otherKind = 0;
			bool const otherHas = StrategyOfObject(picked, &otherKind, nullptr);

			if (!otherHas && LevelObjects_OpenForEdit() && host::AssignStrategy(LevelObjects_OpenForEdit(), picked, pairKind) == host::ObjectEditResult::Done)
			{
				LoadTheObjectListAgain();
				StrategyFieldSetObject(picked, pairKind, pairField, subject);

				if (row == host::kPickTeleport)
				{
					int32_t type = 0;

					if (host::StrategyView_FieldNumber(kind, payload, "Type", &type))
					{
						StrategyViewApply(picked, pairKind, "Type", (double)type);   // FUN_00530ce0 0x530df6: the new one's Type is this one's
					}
				}
			}
			else if (otherHas && otherKind == 59 && row == host::kPickSwitchDoor)
			{
				StrategyFieldSetObject(picked, 59, "AssocDoor", subject);   // 0x52d5ad: the pair name each other
			}
		}
	}

	else if (row == host::kPickLiftDoor)
	{
		LiftDoorPicked(subject, kind, payload, have ? picked : nullptr);
	}
	else if (row == host::kPickLiftCall || row == host::kPickLiftFloor)
	{
		LiftSwitchPicked(subject, kind, field.c_str(), have ? picked : nullptr);
	}

	// the view's object is the red one again, and the view goes on (or ends now when Escape is held as the pick ends)
	host::Selected current = {};

	current.kind = host::SelectedKind::Object;
	memcpy(current.objectId, subject, 8);
	host::Selection_Set(current);

	if (escaped)
	{
		StrategyViewEnd();
	}
}

// E: THE TARGET'S EXTRA REQUEST DATA (reassess/strategy-data-kinds.md S-KC-8, S-K62-5, S-K69-6, S-K72-6; strategy-data.md S-82..S-85). The view's E slot (DAT_00d87a85) calls FUN_00529b30(target ID,
// &request word): FUN_00529570 finds the target's wrapper (none for an ID no object has, or for the view's own object: nothing happens), whose +0x10 runs its page (strategy_views.h RequestPage).
// The page edits the VIEW's request word for that target: it is put back when the page ends.
void StrategyRequestE()
{
	int32_t kind = 0;
	std::vector<uint8_t> payload;

	if (!StrategyOfObject(strategyEditId, &kind, &payload) || kind != strategyEditKind)
	{
		return;
	}

	uint8_t target[8] = { 0 };
	int32_t word = 0;
	std::string field;
	int32_t row = -1;

	if (kind == 62 || (kind == 80 && trackPage < 0))
	{
		// 62's E (0x53110e-0x53112c: the target +0x2c, the word +0x28); a Track (Destructable)'s main page (0x534a82-0x534aa6: its destruct target, the same places). A Track (72)'s main page
		// calls it with the null ID its class starts with: NAMED, ours finds no object of the null ID, as no level object has one
		if (!host::StrategyView_FieldObject(kind, payload, "targetID", target))
		{
			return;
		}

		field = "target_request";
		host::StrategyView_FieldNumber(kind, payload, "target_request", &word);
	}
	else if (kind == 69)
	{
		// 0x531a70-0x531a87: the death target and its word
		if (!host::StrategyView_FieldObject(kind, payload, "DeathTarget", target))
		{
			return;
		}

		field = "request";
		host::StrategyView_FieldNumber(kind, payload, "request", &word);
	}
	else if (kind == 52 || kind == 61)
	{
		// a Binary or Linked Switch's E (slot 0xd87a85, FUN_0052d700 0x52da16-0x52da29: FUN_00529b30(the target +0x24 / +0x28, &the request +0x2c); S-K52-5), editable or not
		if (!host::StrategyView_FieldObject(kind, payload, "Target", target))
		{
			return;
		}

		field = "request";
		host::StrategyView_FieldNumber(kind, payload, "request", &word);
	}
	else if ((kind == 72 || kind == 80 || kind == 76) && trackPage >= 0)
	{
		// S-K72-6, 0x534c21-0x534d1d: the target row under the pointer (y >= 48: (y - 48) / 16, else none), when the page has that row. A Hierarchy Object's flag page is one row higher
		// (S-K76-5, 0x533e5d-0x533e6c: y > 31: (y - 32) / 16)
		std::vector<host::TrackEffect> effects;
		int32_t const y = PointerRowInBox();
		int32_t const top = kind == 76 ? 32 : 48;

		if (y < top || !host::StrategyView_TrackEffects(payload, &effects))
		{
			return;
		}

		row = (y - top) / 16;

		bool have = false;

		for (host::TrackEffect const &effect : effects)
		{
			if (effect.point == trackPage && (size_t)row < effect.targets.size())
			{
				memcpy(target, effect.targets[(size_t)row].id.data(), 8);
				word = effect.targets[(size_t)row].request;
				have = true;
				break;
			}
		}

		if (!have || (target[0] | target[1] | target[2] | target[3] | target[4] | target[5] | target[6] | target[7]) == 0)
		{
			return;
		}
	}
	else if (kind == 66 || kind == 68)
	{
		// 66's E (slot 0xd87a85, 0x52fc24-0x52fc72): the target row under the POINTER, FUN_00529b30(its ID, &its request) (S-K66-6)
		std::vector<host::MultiTarget> targets;
		size_t index = 0;

		if (!host::StrategyView_MultiTargets(payload, &targets) || !host::StrategyView_MultiTargetAt(payload, multiPage, PointerRowInBox(), &index))
		{
			return;
		}

		memcpy(target, targets[index].id.data(), 8);
		word = (int32_t)targets[index].request;
		row = (int32_t)index;
	}
	else
	{
		return;   // a view with no E
	}

	// FUN_00529570: the view's own object is no target (0x52957b); else the object of that ID: an enemy or a generator (their wrappers), a module, placed or virtual object
	bool const own = memcmp(target, strategyEditId, 8) == 0;
	host::GeneratorRef generator;
	bool const isGenerator = !own && LevelObjects_Level() && host::FindGenerator(LevelObjects_Level(), target, &generator);
	bool const found = !own && (isGenerator || StrategyObjectExists(target));
	int32_t targetKind = 0;
	std::vector<uint8_t> targetPayload;
	bool const hasStrategy = found && StrategyOfObject(target, &targetKind, &targetPayload);
	host::PlacedHierarchyRef targetHierarchy;
	bool const isPlacedHierarchy = found && host::FindPlacedHierarchy(LevelObjects_Level(), target, &targetHierarchy);   // its own page (FUN_0053b830, S-K76-E)
	host::RequestPage const page = host::StrategyView_RequestPageOf(found, isGenerator && generator.type != 0, isGenerator ? generator.type : -1, hasStrategy, targetKind, isPlacedHierarchy);

	if (page == host::RequestPage::Nothing)
	{
		return;
	}

	if (page == host::RequestPage::NotUsed)
	{
		// FUN_0052b220: the word cut to bit 1, then the message (Ok)
		if (!field.empty())
		{
			StrategyViewApply(strategyEditId, kind, field.c_str(), (double)(word & 1));
		}
		else if (kind == 66 || kind == 68)
		{
			std::vector<host::MultiTarget> targets;

			if (host::StrategyView_MultiTargets(payload, &targets) && row >= 0 && (size_t)row < targets.size())
			{
				targets[(size_t)row].request &= 1u;
				MultiWrite(strategyEditId, payload, targets);
			}
		}
		else
		{
			std::vector<host::TrackEffect> effects;

			if (TrackNow(strategyEditId, &kind, &payload, &effects))
			{
				for (host::TrackEffect &effect : effects)
				{
					if (effect.point == trackPage && row >= 0 && (size_t)row < effect.targets.size())
					{
						effect.targets[(size_t)row].request &= 1;
						TrackWrite(strategyEditId, payload, effects);
						break;
					}
				}
			}
		}

		DialogMessage("The target strategy doesn't use extended request data");   // 0x5ee7c4
		return;
	}

	if (page == host::RequestPage::NotMarine)
	{
		DialogMessage("Only available for marines/civilians");   // 0x5f02d4: the word kept
		return;
	}

	std::string name;
	int32_t targetType = 0, integrity = 0;

	StrategyViewNameOf(target, &name);

	if (page == host::RequestPage::PlacedObject)
	{
		host::StrategyView_FieldNumber(62, targetPayload, "Type", &targetType);          // the target's +0x14
		host::StrategyView_FieldNumber(62, targetPayload, "integrity", &integrity);      // its +0x20
	}

	if (!host::StrategyView_RequestOpen(page, (uint32_t)word, name, targetType, integrity, &requestPage))
	{
		return;
	}

	requestPageField = field;
	requestPageRow = row;
	memcpy(requestPageTarget, target, 8);
	requestPageOn = true;
}

// The page ends (Escape, or the right button): the word goes back into the view's strategy (*word |= data << 1); the view under it stays (INTENG BUG, FIXED: StrategyView_RequestEscapeEndsView)
void RequestPageEnd(bool escaped)
{
	if (!requestPageOn)
	{
		return;
	}

	requestPageOn = false;

	int32_t const word = (int32_t)host::StrategyView_RequestWord(requestPage);
	int32_t kind = 0;
	std::vector<uint8_t> payload;

	if (!requestPageField.empty())
	{
		if (StrategyOfObject(strategyEditId, &kind, &payload) && kind == strategyEditKind)
		{
			StrategyViewApply(strategyEditId, kind, requestPageField.c_str(), (double)word);
		}
	}
	else if (strategyEditKind == 66 || strategyEditKind == 68)
	{
		std::vector<host::MultiTarget> targets;

		if (StrategyOfObject(strategyEditId, &kind, &payload) && (kind == 66 || kind == 68) && host::StrategyView_MultiTargets(payload, &targets) && requestPageRow >= 0 && (size_t)requestPageRow < targets.size())
		{
			targets[(size_t)requestPageRow].request = (uint32_t)word;
			MultiWrite(strategyEditId, payload, targets);
		}
	}
	else
	{
		std::vector<host::TrackEffect> effects;

		if (TrackNow(strategyEditId, &kind, &payload, &effects))
		{
			for (host::TrackEffect &effect : effects)
			{
				if (effect.point == trackPage && requestPageRow >= 0 && (size_t)requestPageRow < effect.targets.size())
				{
					effect.targets[(size_t)requestPageRow].request = word;
					TrackWrite(strategyEditId, payload, effects);
					break;
				}
			}
		}
	}

	if (escaped && host::StrategyView_RequestEscapeEndsView(requestPage.page))
	{
		StrategyViewEnd();
	}
}

// A left click on the page: its change at once, or its question (the integrity's whole-number dialog, the Enemy page's Select Path)
static void RequestPageClick(int32_t y)
{
	host::RequestAsk const ask = host::StrategyView_RequestClick(&requestPage, y);

	if (ask == host::RequestAsk::Integrity)
	{
		// FUN_00474170("New integrity value", the target's +0x20 & 0xff) + FUN_0057c800: Ok (3) takes the number, Cancel nothing
		host::DialogField number;

		number.text = std::to_string((long long)(requestPage.targetIntegrity & 0xff));
		DialogText("New integrity value", std::vector<host::DialogField>(1, number), [](int32_t answer, std::vector<std::string> const &texts)   // 0x5ef380
		{
			if (!requestPageOn || answer != host::kDialogOk || texts.empty())
			{
				return;
			}

			char *end = nullptr;
			long long const typed = _strtoi64(texts[0].c_str(), &end, 10);

			host::StrategyView_RequestIntegrity(&requestPage, end == texts[0].c_str() ? 0 : typed);
		});
		return;
	}

	if (ask == host::RequestAsk::SelectPath)
	{
		// FUN_004fb6a0: the list "Select Path" of the level's paths; a path chosen puts its number in the data
		std::vector<host::PathInfo> const paths = LevelPaths();
		std::vector<std::string> rows;

		for (host::PathInfo const &one : paths)
		{
			rows.push_back(std::to_string((long long)one.number) + "  " + one.name);
		}

		DialogList("Select Path", rows, [paths](int32_t choice)   // 0x5e97e8
		{
			if (requestPageOn && choice >= 0 && (size_t)choice < paths.size())
			{
				host::StrategyView_RequestPath(&requestPage, paths[(size_t)choice].number);
			}
		});
		return;
	}

	if (ask == host::RequestAsk::SelectSequence)
	{
		// FUN_0053b830 0x53b8f2-0x53bade: the placed hierarchy's PLHISEQU chunks (in the file's order); none: "No sequences!!!!!!" (Ok); else the list "Select Sequence Index", a row "%d" per
		// sequence's index; one chosen sets the word's bits 8-15 to its index. NAMED: a PLHISEQU the game's reader would refuse is not listed (special_objects_edit.h)
		std::vector<host::PlacedHierarchyInfo> placed;
		std::vector<int32_t> indexes;

		if (LevelObjects_OpenForEdit() && host::PlacedHierarchies_Read(LevelObjects_OpenForEdit(), &placed) == host::SpecialResult::Done)
		{
			for (host::PlacedHierarchyInfo const &one : placed)
			{
				if (memcmp(one.id, requestPageTarget, 8) == 0 && indexes.empty())
				{
					for (host::PlacedHierarchySequence const &s : one.sequences)
					{
						indexes.push_back(s.index);
					}

					break;
				}
			}
		}

		if (indexes.empty())
		{
			DialogMessage("No sequences!!!!!!");   // 0x5f0394
			return;
		}

		std::vector<std::string> rows;

		for (int32_t const index : indexes)
		{
			rows.push_back(std::to_string((long long)index));   // "%d" (0x5d1268)
		}

		DialogList("Select Sequence Index", rows, [indexes](int32_t choice)   // 0x5f03a8
		{
			if (requestPageOn && choice >= 0 && (size_t)choice < indexes.size())
			{
				host::StrategyView_RequestSequence(&requestPage, indexes[(size_t)choice]);
			}
		});
	}
}

// a left click at (x, y) of the text area (y from its top) in the view that is up
void StrategyViewClickAt(int32_t x, int32_t y)
{
	if (requestPageOn)
	{
		RequestPageClick(y);
		return;
	}

	std::vector<uint8_t> payload;
	std::string name;
	int32_t kind = 0;
	uint8_t id[8];
	host::StrategyViewClick click;

	if (!StrategyViewSubject(&payload, &name, &kind, id) || kind != strategyEditKind || !host::StrategyView_Click(kind, payload, x, y, &click, trackPage, kind == 69 ? EnemySpeciesOf(id) : -1))
	{
		return;
	}

	if (click.action == host::StrategyViewClick::Toggle)
	{
		int32_t now = 0;

		if (host::StrategyView_FieldNumber(kind, payload, click.field.c_str(), &now))
		{
			StrategyViewApply(id, kind, click.field.c_str(), (double)(uint32_t)((uint32_t)now ^ click.mask));
		}

		return;
	}

	if (click.action == host::StrategyViewClick::Pick)
	{
		// FUN_00527540(start, edit): the field's object, when the level has it, is drawn red and the camera goes to its place (FUN_00529280(block, 0)); none -> nothing is red
		strategyPickField = click.field;
		strategyViewPickWords = click.pickWords;
		strategyPickRow = click.pickRow;
		strategyPickRequest = click.requestField;
		strategyPickHave = host::StrategyView_FieldObject(kind, payload, click.field.c_str(), strategyPickId) && StrategyObjectExists(strategyPickId);

		if (strategyPickHave)
		{
			StrategyFlyTo(strategyPickId);
		}

		StrategyPickShow();
		return;
	}

	if (click.action == host::StrategyViewClick::Collect && click.pickRow == host::kCollectLiftPositions)
	{
		LiftPositionsStart(id, payload, click.index);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowTrackPoint)
	{
		TrackAskPoint(id);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kOpenTrackPage)
	{
		trackPage = click.index;
		return;
	}

	if (click.action == host::StrategyViewClick::Run && (click.pickRow == host::kRunTrackRequest || click.pickRow == host::kRunTrackDirection))
	{
		TrackChangeTarget(id, click.index, click.pickRow == host::kRunTrackDirection);
		return;
	}

	if (click.action == host::StrategyViewClick::Collect && click.pickRow == host::kCollectTrackTargets)
	{
		TrackTargetsStart(id, click.index);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowSwitchProperties)
	{
		SwitchPropertiesWindow(id, kind, nullptr);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowEnemyMission)
	{
		if (EnemySpeciesOf(id) == 7)
		{
			MissionTypeWindow(id, 69, { { "Wait then wander", 0 }, { "Wander", 1 }, { "Guard", 2 }, { "Local Guard", 4 }, { "Noncombatant", 3 }, { "Path follower", 5 } });   // FUN_00531fb0
		}
		else if (EnemySpeciesOf(id) == 2)
		{
			MissionTypeWindow(id, 69, { { "Wait then wander", 0 }, { "Path follower", 1 } });   // FUN_00532230
		}

		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowGeneratorMission)
	{
		MissionTypeWindow(id, 79, { { "Hunt", 0 }, { "Path follower", 1 } });   // FUN_005323c0 (0x5ef674)
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowSelectPath)
	{
		SelectPathWindow(id, kind);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowMessageText)
	{
		MissionTextWindow(id, 0, 73);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowMissionText)
	{
		MissionTextWindow(id, click.index);
		return;
	}

	if (click.action == host::StrategyViewClick::Run && click.pickRow == host::kRunMultiToggle)
	{
		std::vector<host::MultiTarget> targets;

		if (host::StrategyView_MultiTargets(payload, &targets) && click.index >= 0 && (size_t)click.index < targets.size())
		{
			targets[(size_t)click.index].request ^= 1u;   // 0x52fe29-0x52fe3f
			MultiWrite(id, payload, targets);
		}

		return;
	}

	if (click.action == host::StrategyViewClick::Collect && click.pickRow == host::kCollectMultiTargets)
	{
		MultiTargetsStart(id, payload, click.index);
		return;
	}

	if (click.action == host::StrategyViewClick::Collect && click.pickRow == host::kCollectMissionTargets)
	{
		MissionTargetsStart(id, payload, click.index);
		return;
	}

	if (click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowLiftStart)
	{
		LiftStartWindow(id, payload);
		return;
	}

	if (click.action == host::StrategyViewClick::Run && click.pickRow == host::kRunLiftExit)
	{
		// 0x52ca98-0x52cabf: flag 8 (Exit opposite side) flips, then every position is made to agree (FUN_0052b770)
		int32_t flags = 0;

		host::StrategyView_FieldNumber(kind, payload, "LiftFlags", &flags);
		StrategyViewApply(id, kind, "LiftFlags", (double)((uint32_t)flags ^ 8u));
		LiftMakeAgree(id);
		return;
	}

	if (click.action == host::StrategyViewClick::Set)
	{
		StrategyViewApply(id, kind, click.field.c_str(), click.value);
		return;
	}

	if (click.action != host::StrategyViewClick::Number)
	{
		return;
	}

	host::DialogField field;

	field.text = click.initialText;

	host::StrategyViewClick const asked = click;

	DialogText(click.title, std::vector<host::DialogField>(1, field), [asked, kind](int32_t answer, std::vector<std::string> const &texts)
	{
		if (answer != host::kDialogOk || texts.empty() || texts[0].empty())
		{
			return;
		}

		char *end = nullptr;
		double const typed = asked.isFloat ? strtod(texts[0].c_str(), &end) : (double)_strtoi64(texts[0].c_str(), &end, 10);

		// a number the view takes (the scaled value from lowest up to highest); anything else leaves the number as it was
		double scaled = typed * asked.factor;

		if (asked.clampBelow && scaled < asked.lowest)
		{
			scaled = asked.lowest;
		}

		if (end == texts[0].c_str() || !(scaled >= asked.lowest) || !(asked.highestIncluded ? scaled <= asked.highest : scaled < asked.highest))
		{
			return;
		}

		std::vector<uint8_t> now;
		std::string who;
		int32_t kindNow = 0;
		uint8_t id[8];

		if (StrategyViewSubject(&now, &who, &kindNow, id) && kindNow == kind)
		{
			StrategyViewApply(id, kind, asked.field.c_str(), (double)(int64_t)(scaled + asked.bias));
		}
	});
}

// FUN_00521860: a strategy Assign (and Copy) replaces without asking: none, kind 0x35 (53), kind 0x7d2 (2002), or a Placed Object (0x3e) whose +0x14 and +0x18 are 0 (the file's payload 20 and 24)
bool StrategyReplaceableWithoutAsking(std::vector<uint8_t> const &payload)
{
	if (payload.size() < 12)
	{
		return true;
	}

	int32_t kind = 0;

	memcpy(&kind, payload.data() + 8, 4);

	if (kind == 0x35 || kind == 0x7d2)
	{
		return true;
	}

	if (kind != 0x3e || payload.size() < 28)
	{
		return false;
	}

	int32_t type = 0, extra = 0;

	memcpy(&type, payload.data() + 20, 4);
	memcpy(&extra, payload.data() + 24, 4);
	return type == 0 && extra == 0;
}

// ASSIGN STRATEGY'S WINDOW (FUN_0052a160: FUN_0057d170(600, 400, 0x18) titled "Select Strategy" 0x5ee77c): one BUTTON per strategy kind the object's class accepts (its method +8(kind)), in the code's own order, each 200 x 16 at x 0x12 and y = row x 0x14 (the first row at y 0x14),
// and a Cancel button at (0xfa, 0x177) 100 wide; clicking a kind answers with it (FUN_0057c800 returns the kind), Cancel (or Escape) answers nothing. The labels are inteng's own strings (0x5ee75c ...).
static void AssignWindow(int32_t const *canTake, size_t canCount)
{
	struct Kind { int32_t kind; char const *label; };
	static Kind const order[] =
	{
		{ 51, "Lift" }, { 50, "Door" }, { 54, "Platform Lift" }, { 66, "Multi Switch" }, { 57, "Airlock" }, { 59, "Switch Door" }, { 61, "Linked Switch" }, { 64, "Lighting Effects" }, { 68, "Area Switch" }, { 69, "Enemy" },
		{ 70, "Mission Objective" }, { 72, "Track Object" }, { 80, "Track Object (Destructable)" }, { 76, "Hierarchy Object" }, { 73, "Message" }, { 75, "Fan" }, { 62, "Placed Object" }, { 77, "Volume of Death" }, { 78, "Self Destruct" },
		{ 79, "Generator" }, { 1000, "Swinging Door" }, { 1001, "Bomb" }, { 1002, "Switch" }, { 2010, "Trigger Volume" }, { 2015, "Training Text" }, { 2012, "Liquid Death Zone" }, { 2007, "Pivoting Object (Traps)" },
		{ 2005, "Chest" }, { 2006, "Camera Adjust Region" }, { 2009, "Crumbling Floor" }, { 2011, "Track/Morphing" }, { 2014, "Spike Trap" }, { 2013, "Starkey Fragment" },
	};

	host::DialogFormSpec spec;
	int32_t row = 1;

	spec.title = "Select Strategy";
	spec.width = 600;
	spec.height = 400;

	for (Kind const &one : order)
	{
		bool offered = false;

		for (size_t k = 0; k < canCount; k++)
		{
			offered = offered || canTake[k] == one.kind;
		}

		if (!offered)
		{
			continue;
		}

		host::DialogWidget button;

		button.type = host::DialogWidgetType::Button;
		button.box.x = 0x12;
		button.box.y = row * 0x14;
		button.box.width = 200;
		button.box.height = 0x10;
		button.text = one.label;
		button.answer = one.kind;
		spec.widgets.push_back(button);
		row++;
	}

	host::DialogWidget cancel;

	cancel.type = host::DialogWidgetType::Button;
	cancel.box.x = 0xfa;
	cancel.box.y = 0x177;
	cancel.box.width = 100;
	cancel.box.height = 0x10;
	cancel.text = "Cancel";
	cancel.answer = host::kDialogEscape;
	spec.widgets.push_back(cancel);

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &)
	{
		if (answer > host::kDialogOk)
		{
			promptFor = kPromptAssignKind;
			promptText = std::to_string((long long)answer);
			RunThePrompt();
		}
	});
}

// Whether a virtual object (a VIOBJECT of SPECLOBJ, strategy_lookup.h) has this ID: Delete Strategy's question and its answer take one by its own branch (FUN_00528790 on inteng's virtual record)
static bool IsVirtualObject(File_Chunk *level, uint8_t const *id)
{
	std::vector<host::VirtualObjectRef> virtuals;

	host::ListVirtualObjects(level, &virtuals);

	for (host::VirtualObjectRef const &one : virtuals)
	{
		if (memcmp(one.id, id, host::kSelectionIdBytes) == 0)
		{
			return true;
		}
	}

	return false;
}

void StrategyModeCommand(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];

	// Mode > Object selection (0x4): FUN_00524e90's switch sends command 4 to its common tail (0x525242) and nothing else: it does NOTHING. The page that holds the item (set 6, page 2 of bar A) is never shown either: the only calls that switch a page
	// (+0x18) are at 0x524bc3 (start, page 0), 0x525725 (page 1) and 0x5267e3 (page 0). So the item exists on a page no key or click reaches, and does nothing; there is no mode to build (strategy-menu-structure.md).
	if (command == 0x4)
	{
		return;
	}

	if (command == 0x43)
	{
		// FUN_005354d0: the question first, for every fan of the level
		if (!open)
		{
			statusWords = "Reset fan wind speed: no level is held to work on";
			return;
		}

		ConfirmAs(kPromptStrategyFans, host::StrategyMode_ResetFansQuestion());   // FUN_005354d0: the question 0x5efd24 in a Yes / No box
		return;
	}

	// Create Virtual Object (0x39 Here, 0x3a-0x3f Above to Behind: FUN_00524e90 0x524fc6, FUN_005396d0, virtual_object.h): the place first, then the name is asked. Everything here is in inteng's
	// memory units, the file's x 100 x the level's scale (virtual_object.h "INTENG'S UNITS": its stacks step 1100 file units, its size is 1000), and RunVirtualName divides by 100 x the scale.
	if (command >= host::kVirtualHere && command <= host::kVirtualBehind)
	{
		double const scale = local_scale > 0.0 ? local_scale : 1.0;
		double const intengUnits = (double)host::kIntengUnitsPerFileUnit * scale;   // one file unit, in inteng's memory units

		if (!open || !LevelObjects_Level())
		{
			statusWords = "Create Virtual Object: no level is held to work on";
			return;
		}

		if (command == host::kVirtualHere)
		{
			if (!theCamera)
			{
				statusWords = "Create Virtual Object: there is no view to take the place from";
				return;
			}

			// the view's place is the game's (the file's x the scale): x 100 for inteng's units
			virtualPlace[0] = (int64_t)theCamera->x * host::kIntengUnitsPerFileUnit;
			virtualPlace[1] = (int64_t)theCamera->y * host::kIntengUnitsPerFileUnit;
			virtualPlace[2] = (int64_t)theCamera->z * host::kIntengUnitsPerFileUnit;
		}
		else
		{
			uint8_t selected[8];
			int64_t place[3], low[3], high[3], from[3], least[3], most[3];
			bool found = SelectedObjectId(selected);

			// a placed object or a module: its place and its shape's own min and max (the file's units), into inteng's units
			if (found && host::VirtualObject_Geometry(LevelObjects_Level(), selected, place, low, high))
			{
				for (int axis = 0; axis < 3; axis++)
				{
					from[axis] = (int64_t)((double)place[axis] * intengUnits);
					least[axis] = (int64_t)((double)low[axis] * intengUnits);
					most[axis] = (int64_t)((double)high[axis] * intengUnits);
				}
			}
			// a virtual object of the level (the owner's "stack them, spawn them side by side or up"): its VOBJPROP place, and the extents of the shape its record shows, shape 13, which are in
			// inteng's units already (FUN_00539ee0(0xd, ...); editor_icons.h EditorIcon_Extents: the shape the editor draws for it)
			else if (found)
			{
				std::vector<host::VirtualObjectRef> virtuals;
				int32_t iconLeast[3], iconMost[3];

				found = false;
				host::ListVirtualObjects(LevelObjects_Level(), &virtuals);

				for (host::VirtualObjectRef const &one : virtuals)
				{
					if (!found && memcmp(one.id, selected, 8) == 0 && host::EditorIcon_Extents(host::kVirtualObjectShape, iconLeast, iconMost))
					{
						for (int axis = 0; axis < 3; axis++)
						{
							from[axis] = (int64_t)((double)one.place[axis] * intengUnits);
							least[axis] = iconLeast[axis];
							most[axis] = iconMost[axis];
						}

						found = true;
					}
				}
			}

			if (!found)
			{
				statusWords = "Create Virtual Object: select an object that has a shape, or a virtual object, first";
				return;
			}

			host::VirtualObject_Moved(command, from, least, most, virtualPlace);
		}

		TextAs(kPromptVirtualName, "Type name for new virtual object", "");   // 0x5f02b0, FUN_00474bc0: a text dialog (Ok / Cancel)
		return;
	}

	// Delete Multiple Strategies (0x42, FUN_005376a0 builds the window, FUN_005380d0 does the work, strategy_mode.h): the form of twenty check boxes with Ok and Cancel (DeleteMultipleForm); Ok deletes at once (inteng asks nothing more).
	if (command == 0x42)
	{
		host::StrategyGroup const *groups = nullptr;
		size_t const groupCount = host::StrategyMode_Groups(&groups);

		if (!open)
		{
			statusWords = "Delete Multiple Strategies: no level is held to work on";
			return;
		}

		(void)groupCount;
		(void)groups;
		DeleteMultipleForm();
		return;
	}

	// Copy Strategy (0x6, FUN_00528340): "Select object to copy from first" (0x5ee230), "Selected object has no strategy" (0x5ee174) are on-screen messages; then the collector (above) takes the objects to copy to
	if (command == 0x6)
	{
		uint8_t from[8];
		host::ObjectStrategy now;

		if (!open || !SelectedObjectId(from))
		{
			StrategyMessage("Select object to copy from first");
			return;
		}

		if (host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), from, &now) != host::StrategyFound::Found || now.payload.size() < 12)
		{
			StrategyMessage("Selected object has no strategy");
			return;
		}

		// the kind's +0x10 (0x52838c; reassess/strategy-data-kinds.md S-KC-7): only 50, 52, 61, 62, 64, 66 and 68 return 1 (0x4316d0); every other kind, the Lift (51, its Airlock and LiftNoTel too) and
		// the kinds with no class of their own included, returns 0 (0x404550): "Copying not supported for this strategy" (0x5ee208) for 700 ms and no collector
		int32_t const sourceKind = *(int32_t const *)(now.payload.data() + 8);

		if (sourceKind != 50 && sourceKind != 52 && sourceKind != 61 && sourceKind != 62 && sourceKind != 64 && sourceKind != 66 && sourceKind != 68)
		{
			StrategyMessage("Copying not supported for this strategy");
			statusWords = "Copying not supported for this strategy";
			return;
		}

		memcpy(copyFrom, from, sizeof(copyFrom));
		copyKind = sourceKind;
		copyTargets.clear();
		copyCollecting = true;
		copyPurpose = 0;
		strategyEditFan = false;
		StrategyMessage("Select objects to copy to");   // 0x5ee1ec
		statusWords = "Select objects to copy to";
		return;
	}

	// Assign Strategy (0x8, FUN_00528830): inteng lists the strategies the object can use, asks "Object already has a strategy. Would you like to replace it?" when it has one, and gives it a new
	// strategy of the kind picked, every field at its creation value (object_edit.h AssignStrategy, model::MakeNewStrategy). Here the offered kinds are listed under the prompt and the number typed.
	if (command == 0x8)
	{
		if (!open || !SelectedObjectId(assignId))
		{
			return;   // FUN_00528830 0x528850: no current object: nothing (strategy-data.md S-38)
		}

		promptList.clear();

		int32_t canTake[16];
		size_t const canCount = KindsTheObjectCanTake(assignId, canTake);

		if (canCount == 0)
		{
			statusWords = "Assign Strategy: that object's header can't be read";
			return;
		}

		for (size_t k = 0; k < canCount; k++)
		{
			model::StrategyKind const *const known = model::FindStrategyKind(canTake[k]);

			promptList.push_back(std::to_string((long long)canTake[k]) + "  " + (known ? (known->label ? known->label : known->name) : "?"));
		}

		// FUN_00528830 0x528859-0x5288cb: the question comes FIRST, only when the strategy the object has is not one FUN_00521860 replaces without asking (none, kind 0x35, kind 0x7d2, or a Placed Object
		// (0x3e) whose +0x14 and +0x18 are 0: our payload 20 "Type" and 24 "ExtraData", by its writer FUN_0051ed20); the bytes as edited so far (the store). Not Yes: nothing. Yes, or no question: the window
		// (FUN_0052a160 at 0x5288db)
		host::ObjectStrategy had;

		if (host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), assignId, &had) == host::StrategyFound::Found && !StrategyReplaceableWithoutAsking(had.payload))
		{
			std::vector<int32_t> const kinds(canTake, canTake + canCount);

			DialogConfirm("Object already has a strategy.\nWould you like to replace it?", [kinds](bool yes)   // 0x5ee2b4, FUN_00474a40 (400 x 200, Yes / No)
			{
				if (yes)
				{
					AssignWindow(kinds.data(), kinds.size());
				}
			});
			return;
		}

		AssignWindow(canTake, canCount);
		return;
	}

	// Destroy Virtual Object (0x40, FUN_00524e90 0x525199, FUN_00539e00): works on the CURRENT object only, and only when it is a virtual object (the wrapper's +8 == 1); with no current object, or a current object that
	// is not a virtual one, it does NOTHING (no message: the case just ends). Then "Delete this object?" (0x5eddec) Yes / No; Yes cuts the VIOBJECT with its VOBJPROP and AVPSTRAT and lets the current object go.
	// A virtual object made by Create is NOT current (FUN_005396d0 never sets DAT_0091496c): a click on it makes it current.
	if (command == 0x40)
	{
		uint8_t current[8];

		if (!open || !SelectedObjectId(current) || !host::VirtualObject_List(open, &destroyChoices))
		{
			return;
		}

		for (host::VirtualObjectInfo const &one : destroyChoices)
		{
			if (memcmp(one.id, current, sizeof(current)) == 0)
			{
				memcpy(destroyId, one.id, sizeof(destroyId));
				ConfirmAs(kPromptDestroyAsk, "Delete this object?");   // 0x5eddec, FUN_00474a40
				return;
			}
		}

		return;
	}

	// Find Errors (0x44, FUN_0053bf00; strategy-virtual-object-facts.md): inteng first runs each strategy's repair (+0x1c: a dangling target is reset, target lists compacted), then asks every editor
	// object for its problems; the only class that adds one (FUN_0053a700, "NPC has no trigger volume") is gated by a game number this exe only ever sets to 0, so inteng says "No Errors Found"
	// (0x5f04a0). NAMED: the repair pass is not done here (it changes the strategies' targets; its rules are read class by class, not yet coded).
	if (command == 0x44)
	{
		if (open)
		{
			// FUN_0053bf00 first runs FUN_00536ad0: every strategy's repair method (+0x1c), the dead targets reset (strategy_repair.h says which classes are done)
			size_t const repaired = host::StrategyRepair_Run(open);

			if (repaired > 0)
			{
				LoadTheObjectListAgain();
			}

			DialogMessage("No Errors Found");   // 0x5f04a0, FUN_00473f90: a message box with Ok
		}
		else
		{
			statusWords = "Find Errors: no level is held to work on";
		}
		return;
	}

	// V (0x101, our number): the volume editor FUN_005277c0 (VolumeBegin). inteng enters it from two Edit views only: a Volume of Death's FUN_00535970 (slot 0xd87a96, "V : Edit Volume", 0x535bd7) and an
	// Area Switch's FUN_0052f700 -> FUN_00530900 ("V : Change Trigger Volume", 0x5309b2). Their V reaches it here (no view key takes V before it), and VolumeBegin starts nothing outside those two views
	// (strategy-data.md S-59)
	if (command == 0x101)
	{
		VolumeBegin();
		return;
	}

	// Edit Strategy (0x7, FUN_005286b0; strategy-data.md S-37): NO current object (DAT_00914970 == 0, 0x5286b5): NOTHING, no message. A current object without a strategy: "Selected object has no strategy"
	// (0x5ee174, FUN_0057bd40 at (200, 100), held 700 ms). Else the level counts as changed (0x5286cc, even when nothing is edited), the current object's red is off while the kind's view runs (0x5286d7) and
	// back after it (0x5286f9). Kinds whose view is not built yet: the field panel (NAMED stand-in).
	if (command == 0x7)
	{
		uint8_t selected[8];

		if (!open || !SelectedObjectId(selected))
		{
			return;
		}

		// any object with a strategy: a level object, a virtual object, an enemy or a generator (their strategies are found by the same lookup the views read)
		host::LevelStore heldNow(open);
		host::ObjectStrategy hasNow;

		if (host::LookUpStrategyBytes(LevelObjects_Level(), heldNow, selected, &hasNow) != host::StrategyFound::Found)
		{
			StrategyMessage("Selected object has no strategy");   // inteng's own words (0x5ee174), drawn at (200, 100) for 700 ms (FUN_0057bd40)
			statusWords = "Selected object has no strategy";
			return;
		}

		strategyMarkedChanged = true;   // 0x5286cc: DAT_00914950 = 1

		if (StrategyViewOpen())
		{
			if (strategyEditFan)
			{
				// a Message's view says its FMV instead (SayMessageFmv, in StrategyViewOpen)
				if (strategyEditKind == 73 && !messageFmvWords.empty())
				{
					statusWords.Say(messageFmvWords.c_str(), StatusSayer::StrategyView);   // drawn in the view (the owner's call 11-19)
				}
				else
				{
					statusWords = "Edit Strategy: click a line to change it; Escape ends";   // the hint: kept for the harness, not drawn (inteng's view draws none)
				}
			}

			return;
		}

		strategyPanelOn = true;
		ShowTheSelected(host::Selection_Get());
		fieldsOpenedAt = fieldHost.Showing() ? ++openClock : 0;
		statusWords = "Edit Strategy: the strategy's fields are in the field panel: edit them there";
		return;
	}

	// Stability flags (0x37): see StabilityBegin
	if (command == 0x37)
	{
		StabilityBegin();
		return;
	}

	// Module flags (0x2, FUN_00525700): the flag pages
	if (command == 0x2)
	{
		// the flag loop starts with a list of its own (FUN_00525700 0x525746: an empty list), apart from the current object: our selection stands in for that list, so it starts empty
		host::Selection_Clear();
		LetGoOfTheFieldPanel();
		strategyFlagPage = true;
		EditorBar_SetPage(1);

		if (!LoadTheMenus())
		{
			LogString("menus: the Strategy Data flag bars couldn't be made, so the editor closed");
			running = false;
			bRunning = false;
		}

		statusWords = "Module flags: pick modules (click, Shift+click takes one out), then Add Flag / Remove Flag / View Flag; Exit goes back";
		return;
	}

	// Exit (0x5): the mode's Exit (the save question, the save, the hub)
	if (command == 0x5)
	{
		ExitTheMode();
		return;
	}

	// Help (0x36, FUN_00522950): inteng's text of the camera and selection keys
	if (command == 0x36)
	{
		textPanelText = kIntengStrategyHelpText;
		return;
	}

	// Stop Flag View (0x35: DAT_00914944 := 0 and the two commands cleared) and View Flag (0x10 0x13 0x16 0x19 ..: the bit, the commands of the flag's Add and Remove for the F key)
	if (command == 0x35)
	{
		strategyViewBit = 0;
		statusWords = "Stop Flag View: the modules are drawn as they are";
		return;
	}

	if (uint32_t const viewBit = host::StrategyMode_ViewFlagBit(command))
	{
		strategyViewBit = viewBit;
		strategyViewAdd = command - 2;
		statusWords = "View Flag: the modules that hold the flag are drawn orange (F adds the flag to the picked modules, Shift+F removes it)";
		return;
	}

	// Module flags, Add and Remove (FUN_00525700's switch, FUN_0052f360 on the picked modules: strategy_mode.h)
	host::StrategyFlagStep steps[2];
	size_t const stepCount = host::StrategyMode_FlagSteps(command, steps);

	if (command == host::kStrategyMessageCommand)
	{
		if (!open || PickedModuleIdBytes().empty())
		{
			statusWords = "Message: pick modules first (click, Shift+click for more)";
			return;
		}

		OpenAPrompt(kPromptStrategyMessage);
		return;
	}

	if (stepCount > 0)
	{
		std::vector<uint8_t> const ids = PickedModuleIdBytes();

		// S-19: FUN_0052f360 0x52f660 sets DAT_00914950 = 1 after its loop whatever the list held (an empty one, a flag a module had already): Exit then asks
		strategyMarkedChanged = true;

		if (!open || ids.empty())
		{
			return;   // nothing picked: the loop does nothing and says nothing
		}

		size_t changed = 0;
		host::StrategyModeResult result = host::StrategyModeResult::Done;

		for (size_t i = 0; i < stepCount && result == host::StrategyModeResult::Done; i++)
		{
			size_t now = 0;

			result = host::StrategyMode_SetModuleFlag(open, ids.data(), ids.size() / host::kSelectionIdBytes, steps[i].bit, steps[i].add, &now);
			changed = now > changed ? now : changed;
		}

		if (result != host::StrategyModeResult::Done)
		{
			strategyWords = std::string("Module flags: ") + host::StrategyModeWords(result);
			statusWords = strategyWords.c_str();
			return;
		}

		LoadTheObjectListAgain();
		strategyWords = "Module flags: " + std::to_string((unsigned long long)changed) + " of " + std::to_string((unsigned long long)(ids.size() / host::kSelectionIdBytes)) + " picked module(s) changed (kept in the editor's bytes until Save a copy)";
		statusWords = strategyWords.c_str();
		return;
	}

	if (command != 0xc)
	{
		statusWords = "That Strategy mode item isn't built yet";
		return;
	}

	// FUN_00528790 does nothing without a current object that has a strategy chunk
	if (!open || !SelectedObjectId(id))
	{
		statusWords = "Delete Strategy: select an object first";
		return;
	}

	// an enemy or a generator: its strategy is in its AVPGENEX; with none, nothing (FUN_00528790: no holder)
	host::GeneratorRef generator;

	if (host::FindGenerator(LevelObjects_Level(), id, &generator))
	{
		host::ObjectStrategy had;

		if (host::FindObjectStrategy(LevelObjects_Level(), id, &had) != host::StrategyFound::Found)
		{
			return;
		}

		memcpy(strategyDeleteId, id, sizeof(strategyDeleteId));
		ConfirmAs(kPromptStrategyDelete, host::StrategyMode_DeleteQuestion());   // FUN_00528790: 0x5ee254 in a Yes / No box
		return;
	}

	// a placed hierarchy or a virtual object (inteng's records of class 0x5afae0, FUN_0053b5a0, and 0x5af9e0, FUN_005393c0): FUN_00528790's holder (+0x14) is the FIRST AVPSTRAT child of its
	// PLACHIER / VIOBJECT; with none, nothing (no question, no message). Until 11-17 both went to StrategyMode_Has, which knows the level's objects only: "no object has that name", no question
	// (strategy-regen-suite-design.md F2, INTENG DIFFERENCE, FIXED)
	host::PlacedHierarchyRef placedHierarchy;

	if (host::FindPlacedHierarchy(LevelObjects_Level(), id, &placedHierarchy) || IsVirtualObject(LevelObjects_Level(), id))
	{
		host::ObjectStrategy had;

		if (host::FindObjectStrategy(LevelObjects_Level(), id, &had) != host::StrategyFound::Found)
		{
			return;
		}

		memcpy(strategyDeleteId, id, sizeof(strategyDeleteId));
		ConfirmAs(kPromptStrategyDelete, host::StrategyMode_DeleteQuestion());   // FUN_00528790: 0x5ee254 in a Yes / No box
		return;
	}

	host::StrategyModeResult const has = host::StrategyMode_Has(open, id);

	if (has != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Delete Strategy: ") + host::StrategyModeWords(has);
		statusWords = strategyWords.c_str();
		return;
	}

	memcpy(strategyDeleteId, id, sizeof(strategyDeleteId));
	ConfirmAs(kPromptStrategyDelete, host::StrategyMode_DeleteQuestion());   // FUN_00528790: 0x5ee254 in a Yes / No box
}

// Enter on the question: "y" or "yes" destroys the strategy chunk; anything else changes nothing (inteng: only the answer 3 does)
void RunStrategyDelete(std::string const &typed)
{
	std::string lower;

	for (char const letter : typed)
	{
		lower += (char)tolower((unsigned char)letter);
	}

	if (lower != "y" && lower != "yes")
	{
		statusWords = "Delete Strategy: nothing changed";
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::GeneratorRef generator;

	if (open && host::FindGenerator(open->Level(), strategyDeleteId, &generator))
	{
		host::GeneratorStrategyResult const taken = host::GeneratorStrategy_Delete(open, strategyDeleteId);

		if (taken != host::GeneratorStrategyResult::Done)
		{
			strategyWords = std::string("Delete Strategy: ") + host::GeneratorStrategyWords(taken);
			statusWords = strategyWords.c_str();
			return;
		}

		LoadTheObjectListAgain();
		statusWords = "The object's strategy is deleted (kept in the editor's bytes until Save a copy)";
		return;
	}

	// a placed hierarchy: its PLACHIER's AVPSTRAT taken out (S-K76)
	if (host::PlacedHierarchyRef placedHierarchy; open && host::FindPlacedHierarchy(open->Level(), strategyDeleteId, &placedHierarchy))
	{
		host::PlacedHierarchyStrategyResult const taken = host::PlacedHierarchyStrategy_Delete(open, strategyDeleteId);

		if (taken != host::PlacedHierarchyStrategyResult::Done)
		{
			strategyWords = std::string("Delete Strategy: ") + host::PlacedHierarchyStrategyWords(taken);
			statusWords = strategyWords.c_str();
			return;
		}

		LoadTheObjectListAgain();
		statusWords = "The object's strategy is deleted (kept in the editor's bytes until Save a copy)";
		return;
	}

	// a virtual object: its VIOBJECT's first AVPSTRAT taken out, the VIOBJECT and its VOBJPROP kept (FUN_00528790 destroys the holder only; virtual_object.h VirtualObject_DeleteStrategy)
	if (open && IsVirtualObject(open->Level(), strategyDeleteId))
	{
		host::VirtualObjectResult const taken = host::VirtualObject_DeleteStrategy(open, strategyDeleteId);

		if (taken != host::VirtualObjectResult::Done)
		{
			strategyWords = std::string("Delete Strategy: ") + host::VirtualObjectWords(taken);
			statusWords = strategyWords.c_str();
			return;
		}

		LoadTheObjectListAgain();
		statusWords = "The object's strategy is deleted (kept in the editor's bytes until Save a copy)";
		return;
	}

	host::StrategyModeResult const result = open ? host::StrategyMode_Delete(open, strategyDeleteId) : host::StrategyModeResult::BadCall;

	if (result != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Delete Strategy: ") + host::StrategyModeWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	LoadTheObjectListAgain();
	statusWords = "The object's strategy is deleted (kept in the editor's bytes until Save a copy)";
}
