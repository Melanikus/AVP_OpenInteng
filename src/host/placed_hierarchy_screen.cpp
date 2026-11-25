// placed_hierarchy_screen.cpp: A PLACED HIERARCHY in Object Placement: the placed sounds, a placed hierarchy's selection, its frame, its edit, timing and sound
// windows, and its commands. Moved here whole from editor_main.cpp on 2026-11-18 by Melanikus (Frost), so that each file stays within what Snyk Code reads
// (editor_shared.h says why); nothing in it changed. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

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

OpenLevelFile *placementRowsOpen = nullptr;
unsigned placementRowsGeneration = 0;   // and which open of it (Generation(): a level opened in the same sitting may get the same address and Changes() count)
unsigned placementRowsFor = 0;
OpenLevelFile *placedSoundsOpen = nullptr;   // the level and generation the playing sounds (placed_sounds_game.h) were made from; none while the screen is not shown
unsigned placedSoundsFor = 0;
unsigned placedSoundsGeneration = 0;

// The level's placed sounds, in their order among the sounds, as the game's sound code takes them: the place and the ranges in the game's units (the file's x the level's scale, as the
// game's loader makes a track's sound, object_setup.cpp:178-183), the volume and the pitch as the file holds them (the game's own units)
std::vector<host::PlacedSoundIn> PlacedSoundsOfLevel(std::vector<host::SpecialObject> const &specials)
{
	extern double local_scale;
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<host::SpecialObject const *> sounds;

	for (host::SpecialObject const &one : specials)
	{
		if (one.kind == host::SpecialKind::Sound)
		{
			sounds.push_back(&one);
		}
	}

	std::sort(sounds.begin(), sounds.end(), [](host::SpecialObject const *a, host::SpecialObject const *b) { return a->number < b->number; });

	std::vector<host::PlacedSoundIn> list;

	for (host::SpecialObject const *one : sounds)
	{
		host::PlacedSoundIn s;

		s.name = one->name;
		s.wav = one->wav;

		for (int axis = 0; axis < 3; axis++)
		{
			s.place[axis] = (int32_t)(one->place[axis] * scale);
		}

		s.inner = (int32_t)(unsigned)((uint32_t)one->inner * scale);
		s.outer = (int32_t)(unsigned)((uint32_t)one->outer * scale);
		s.volume = one->volume;
		s.pitch = one->pitch;
		s.flags = (uint32_t)one->soundFlags;
		list.push_back(s);
	}

	return list;
}

// The view distance of Object Placement, in the game's millimetres (inteng's DAT_005fd678 is in its own units, 100 to the millimetre: 1,000,000 = 10 m)
int32_t PlacementHideDistance()
{
	return viewDistance / 100;
}

bool placementHideOn = false;   // Object Placement's view distance rule is applied (its own screen shown)

// THE CURRENT PLACED HIERARCHY'S PLAYER (reassess/object-placement.md PH-1, PH-2, PH-3, PH-14, PH-15). Added 2026-11-16 by Melanikus (Vasquez).
// inteng's select (FUN_004e9520, called by the class's select FUN_004e9b20) makes a Hierarchy player for the object when it has none. The player is at the object's place with its turn, and it shows
// the hierarchy's model while hiding the object's own icon. Then the FIRST sequence record becomes current and the player is set to its sequence, not running and looped (FUN_00478630(.., 1)).
// The pop-up goes to page 6. Unselect (FUN_004e94a0) stops it, destroys the player, shows the icon again and puts the pop-up back on page 0.
// Ours: the hierarchy view shows the model the game's way (HierarchyView_ShowPlaced). inteng's three placed hierarchies are FUN_004f4db0's table at 0x5e7b20, the same three the game knows
// (avpx projload.cpp PlacedHierarchyArray), read from avp_huds by the game's recipe; the hierarchy is found by the game's own names.
// NOT YET (later steps): a placed hierarchy with no sequence record gets one made for its first sequence (FUN_004ea290; here the first sequence is shown and no record is made); its sounds (PH-2 / PH-14 timings).
int placedHierarchyShown = -1;       // the placed hierarchy whose model the view shows (its place in placementHierarchies); -1: none
size_t placedHierarchyRecord = 0;    // its current sequence record (inteng's +0x64): a place in its sequences
static bool placedCommaWas = false;         // ',' / '.' were down the frame before (inteng's key slots 0xd87afc / 0xd87afe: set on the key press, cleared by the one who uses them)
static bool placedPeriodWas = false;

static const char *const kPlacedHierarchyModels[3][2] =
{
	{ "Dropship", "dropship" },
	{ "pred ship fury", "pred ship fury" },
	{ "pred ship ob", "pred ship ob" },
};

// The placed hierarchy's own model file (avp_huds\<file>.rif), read-only, for the names of its hierarchy's sequences (FUN_0047b450 lists them; FUN_0047b730 gives one)
static OpenLevelFile placedModelFile;
static std::string placedModelHierarchy;   // the hierarchy's name in that file

static bool PlacedHierarchyWriteRecords(std::vector<host::PlacedHierarchySequence> const &records);

static int32_t placedTimerBefore = 0;          // PH-14: the main player's timer the frame before (FUN_004e9cb0 tests each timing against it and the new one)
static bool placedSoundsWasPlaying = false;     // PH-3 / PH-14: the player played the frame before (its stopping, by Stop or a sequence's end, stops every sound)

// PH-2 / PH-14: the shown placed hierarchy's sounds handed to the player (placed_sounds_game.h), their ranges and the hierarchy's place in the game's units (the file's x the level's scale)
static void PlacedHierarchySoundsSync(host::PlacedHierarchyInfo const &one, int32_t place[3])
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<host::HierarchySoundIn> in;

	for (host::PlacedHierarchySound const &sound : one.sounds)
	{
		if (!sound.read)
		{
			continue;
		}

		host::HierarchySoundIn made;

		made.index = sound.index;
		made.wav = sound.wav;
		made.inner = (int32_t)((double)sound.inner * scale);
		made.outer = (int32_t)((double)sound.outer * scale);
		made.volume = sound.volume;
		made.pitch = sound.pitch;
		made.flags = (uint32_t)sound.flags;
		in.push_back(made);
	}

	host::HierarchySounds_Sync(in);

	for (int k = 0; k < 3; k++)
	{
		place[k] = (int32_t)((double)one.place[k] * scale);
	}
}

static void PlacedHierarchyUnselect()
{
	if (placedHierarchyShown < 0)
	{
		return;
	}

	host::HierarchySounds_LetGo();   // its sounds go with it
	placedSoundsWasPlaying = false;

	// FUN_004e94a0: Stop (FUN_004ea210), the player destroyed (FUN_00477870), the icon shown again; FUN_004e9b40 then puts the pop-up back on page 0
	HierarchyView_Close();
	placedHierarchyShown = -1;
	placedHierarchyRecord = 0;
	EditorBar_SetPopupPage(0);
}

// THE LEVEL GOES (CloseTheLevel, before the game's close; Bishop's LEAD A, 2026-11-24). The placed hierarchy's model is the game's level's memory, so it goes first,
// as an unselect lets it go. If its object is still the current one, the next frame shows it again (PlacedHierarchyFrame).
void PlacedHierarchy_LevelGoes()
{
	PlacedHierarchyUnselect();
}

static void PlacedHierarchySelect(int which)
{
	PlacedHierarchyUnselect();

	if (which < 0 || (size_t)which >= placementHierarchies.size())
	{
		return;
	}

	host::PlacedHierarchyInfo const &one = placementHierarchies[(size_t)which];

	// FUN_004e9b20 puts the pop-up on page 6 whether or not a player could be made (FUN_004f4db0 gives none outside 0..2)
	EditorBar_SetPopupPage(6);
	placedHierarchyShown = which;
	placedHierarchyRecord = 0;

	if (one.hierarchyIndex < 0 || one.hierarchyIndex > 2)
	{
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	int32_t const place[3] = { (int32_t)(one.place[0] * scale), (int32_t)(one.place[1] * scale), (int32_t)(one.place[2] * scale) };
	int sequence = -1;
	int sub = 0;

	if (!one.sequences.empty())
	{
		sequence = one.sequences[0].sequence;
		sub = one.sequences[0].sub;
	}

	std::string why;
	char const *const model = kPlacedHierarchyModels[one.hierarchyIndex][0];
	char const *const hierarchy = kPlacedHierarchyModels[one.hierarchyIndex][1];

	// No record: the hierarchy's first sequence (FUN_004784e0(0))
	bool const madeRecord = sequence < 0;

	if (sequence < 0 && !HierarchyView_FirstSequence(model, hierarchy, &sequence, &sub, &why))
	{
		Answer("The placed hierarchy's model can't be shown: " + why);
		return;
	}

	if (!HierarchyView_ShowPlaced(model, hierarchy, sequence, sub, -1, place, one.turn, &why))
	{
		Answer("The placed hierarchy's model can't be shown: " + why);
		return;
	}

	HierarchyView_Play(false, true, false);
	HierarchyView_SetPosition(0);

	// its model file, read-only, for its sequences' names (PH-4 / PH-6); the hierarchy is the one with the game's name, else the file's first
	std::vector<std::string> names;

	placedModelHierarchy.clear();

	if (placedModelFile.Open((std::string("avp_huds\\") + model + ".rif").c_str()) == LevelOpenResult::Opened && host::HierarchyEdit_Names(&placedModelFile, &names) == host::HierarchyEditResult::Done && !names.empty())
	{
		placedModelHierarchy = names[0];

		for (std::string const &each : names)
		{
			if (_stricmp(each.c_str(), hierarchy) == 0)
			{
				placedModelHierarchy = each;
			}
		}
	}

	// FUN_004e9520 0x4e9649-0x4e96a1: with no record, one is made for that first sequence (FUN_004ea290: index 0, 1000 ms, Loop on) and it is current; the save writes it (PH-1)
	if (madeRecord)
	{
		host::PlacedHierarchySequence first;

		first.index = 0;
		first.sequence = sequence;
		first.sub = sub;
		first.milliseconds = 1000;
		first.flags = 1;
		PlacedHierarchyWriteRecords({ first });
	}
}

// The frame of Object Placement for its placed hierarchy: select / unselect follow the current object, and while one is current and STOPPED, one ',' / '.' press is one key frame back / forward
// (FUN_004e9cb0 0x4e9f29-0x4e9f4f: FUN_00479290 / FUN_00479110, each key slot cleared once it is used). The player's own step is the hierarchy view's frame (FUN_00478920).
void PlacedHierarchyFrame(bool here)
{
	int const wanted = here ? placedHierarchyCurrent : -1;

	if (wanted != placedHierarchyShown)
	{
		if (wanted < 0)
		{
			PlacedHierarchyUnselect();
		}
		else
		{
			PlacedHierarchySelect(wanted);
		}
	}

	bool const comma = keyDown[VK_OEM_COMMA];
	bool const period = keyDown[VK_OEM_PERIOD];

	// PH-14: ',' -> FUN_00479290, '.' -> FUN_00479110 on the placed hierarchy's OWN player (+0x5c, made by FUN_00477450 at 0x4e95df). Their "all sequences" test reads that player's +0x88
	// (FUN_00479110 0x47914a), which FUN_00477450 sets to 0 (0x477603) and nothing of the placed hierarchy writes (Y is Hierarchy mode's, on its own player): all = false, the main only.
	if (placedHierarchyShown >= 0 && HierarchyView_IsShown() && !HierarchyView_IsPlaying() && !AMenuIsOpen() && !ADialogIsOpen() && promptFor == kPromptNone)
	{
		if (comma && !placedCommaWas)
		{
			HierarchyView_StepFrame(-1, false);
		}

		if (period && !placedPeriodWas)
		{
			HierarchyView_StepFrame(1, false);
		}
	}

	placedCommaWas = comma;
	placedPeriodWas = period;

	// PH-14 (FUN_004e9cb0): while the main player plays, each timing of the current record against the timer the frame before and now. Inside its window (start <= timer < end) its sound
	// is started when the old timer was outside (FUN_004ebfe0), else moved to the place (FUN_004ec120); outside, it is stopped (FUN_004ec190), in the list's order. When the player stops
	// (Stop, any command's Stop, or a sequence with no loop ending: FUN_0047dbc0), every sound stops (FUN_004ea210 / 0x4e9d0c).
	bool const playing = placedHierarchyShown >= 0 && (size_t)placedHierarchyShown < placementHierarchies.size() && HierarchyView_IsShown() && HierarchyView_IsPlaying();

	if (playing)
	{
		host::PlacedHierarchyInfo const &one = placementHierarchies[(size_t)placedHierarchyShown];
		int32_t const timer = HierarchyView_Position();
		int32_t place[3];

		PlacedHierarchySoundsSync(one, place);

		if (placedHierarchyRecord < one.sequences.size())
		{
			for (host::PlacedHierarchyTimedSound const &timing : one.sequences[placedHierarchyRecord].sounds)
			{
				bool const inside = timing.start <= timer && timer < timing.end;
				bool const was = timing.start <= placedTimerBefore && placedTimerBefore < timing.end;

				if (inside && !was)
				{
					host::HierarchySounds_Start(timing.soundIndex, place);
				}
				else if (inside)
				{
					host::HierarchySounds_Move(timing.soundIndex, place);
				}
				else
				{
					host::HierarchySounds_Stop(timing.soundIndex);
				}
			}
		}

		placedTimerBefore = timer;
	}
	else if (placedSoundsWasPlaying)
	{
		host::HierarchySounds_StopAll();
	}

	placedSoundsWasPlaying = playing;
}

// THE SEQUENCE RECORDS (PH-4..PH-7, NA-6 step 2). inteng keeps them in the object (+0x70) and its save writes them as PLHISEQU (FUN_004e96d0); ours writes them into the level's bytes at once
// (special_objects_edit.cpp PlacedHierarchy_WriteSequences), and the copy saved holds them. Added 2026-11-16 by Melanikus (Vasquez).
static bool PlacedHierarchyWriteRecords(std::vector<host::PlacedHierarchySequence> const &records)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::SpecialResult const done = open ? host::PlacedHierarchy_WriteSequences(open, (size_t)placedHierarchyShown, records) : host::SpecialResult::BadCall;

	if (done != host::SpecialResult::Done)
	{
		Answer(std::string("The placed hierarchy's sequences were not changed: ") + host::SpecialWords(done));
		return false;
	}

	// the list is read again from the bytes when the level's generation changes (PlacementFrame); until then it holds what was written
	placementHierarchies[(size_t)placedHierarchyShown].sequences = records;
	return true;
}

static bool PlacedModelSequences(std::vector<host::SequenceInfo> *sequences)
{
	sequences->clear();
	return !placedModelHierarchy.empty() && host::HierarchyEdit_Sequences(&placedModelFile, placedModelHierarchy, sequences) == host::HierarchyEditResult::Done;
}

static std::string PlacedSequenceName(int32_t number, int32_t sub)
{
	std::vector<host::SequenceInfo> sequences;

	if (PlacedModelSequences(&sequences))
	{
		for (host::SequenceInfo const &one : sequences)
		{
			if (one.number == number && one.subNumber == sub)
			{
				return one.name;
			}
		}
	}

	return std::to_string(number) + "." + std::to_string(sub);
}

// FUN_004ea3f0: Stop; the record becomes current (+0x64); FUN_00478630(its number, sub, 1): that sequence from 0, stopped, looped (a sequence the hierarchy has not: the player keeps its own)
static void PlacedHierarchyMakeCurrent(size_t place)
{
	host::PlacedHierarchyInfo const &one = placementHierarchies[(size_t)placedHierarchyShown];

	if (place >= one.sequences.size())
	{
		return;
	}

	HierarchyView_Play(false, true, false);
	placedHierarchyRecord = place;
	HierarchyView_SetSequence(one.sequences[place].sequence, one.sequences[place].sub);
	HierarchyView_Play(false, true, false);
	HierarchyView_SetPosition(0);
}

// The lowest index no record has (FUN_004ea290 0x4ea2b0-0x4ea2dd)
static int32_t PlacedHierarchyFreeIndex(std::vector<host::PlacedHierarchySequence> const &records)
{
	for (int32_t index = 0;; index++)
	{
		bool used = false;

		for (host::PlacedHierarchySequence const &one : records)
		{
			used = used || one.index == index;
		}

		if (!used)
		{
			return index;
		}
	}
}

// PH-4, Sequence > Edit sequence (0x7d2, FUN_004ea9a0): inteng's window FUN_004ec2b0 (600 x 400, style 0xb), run by FUN_004ec650. `draft` is the record as the window shows it: the name button
// (control 1, FUN_004ec6e0) picks another of the hierarchy's sequences (FUN_0047b450) and the window comes back with it; ours closes the window for that list and opens it again (NAMED: inteng
// shows the list over the open window).
static void PlacedHierarchyEditWindow(host::PlacedHierarchySequence draft)
{
	host::DialogFormSpec spec;
	host::DialogWidget name, timeLabel, timeEdit, loopLabel, loopTick, initialLabel, initialTick, playingLabel, playingTick, ok;

	spec.title = "Sequence " + std::to_string(draft.index);
	spec.width = 600;
	spec.height = 400;
	name.type = host::DialogWidgetType::Button;
	name.text = PlacedSequenceName(draft.sequence, draft.sub);
	name.box = { 0x12, 0x14, 0x172, 0x12 };
	name.answer = 10;
	timeLabel.type = host::DialogWidgetType::Label;
	timeLabel.text = "Time in ms";
	timeLabel.box = { 0x12, 0x28, 100, 0x12 };
	timeEdit.type = host::DialogWidgetType::Edit;
	timeEdit.text = std::to_string(draft.milliseconds);
	timeEdit.box = { 0x76, 0x28, 100, 0x12 };
	loopLabel.type = host::DialogWidgetType::Label;
	loopLabel.text = "Loop";
	loopLabel.box = { 0x12, 0x3c, 200, 0x11 };
	loopTick.type = host::DialogWidgetType::Check;
	loopTick.checked = (draft.flags & 1) != 0;
	loopTick.box = { 0xda, 0x3c, 0x11, 0x11 };
	initialLabel.type = host::DialogWidgetType::Label;
	initialLabel.text = "Initial sequence";
	initialLabel.box = { 0x12, 0x50, 200, 0x11 };
	initialTick.type = host::DialogWidgetType::Check;
	initialTick.checked = (draft.flags & 2) != 0;
	initialTick.box = { 0xda, 0x50, 0x11, 0x11 };
	playingLabel.type = host::DialogWidgetType::Label;
	playingLabel.text = "Playing at start";
	playingLabel.box = { 0x12, 100, 200, 0x11 };
	playingTick.type = host::DialogWidgetType::Check;
	playingTick.checked = (draft.flags & 4) != 0;
	playingTick.box = { 0xda, 100, 0x11, 0x11 };
	ok.type = host::DialogWidgetType::Button;
	ok.text = "OK";
	ok.box = { 200, 0x15e, 100, 0x12 };
	ok.answer = host::kDialogOk;

	for (host::DialogWidget const &part : { name, timeLabel, timeEdit, loopLabel, loopTick, initialLabel, initialTick, playingLabel, playingTick, ok })
	{
		spec.widgets.push_back(part);
	}

	// FUN_004ec6e0, control 9: ticking Playing at start ticks Initial sequence too
	spec.onCheck = [](std::vector<host::DialogWidget> &widgets, size_t changed)
	{
		if (changed == 8 && widgets[8].checked)
		{
			widgets[6].checked = true;
		}
	};

	host::Dialogs_Form(spec, [draft](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		host::PlacedHierarchySequence edited = draft;
		std::string const typed = widgets[2].text;
		bool digits = !typed.empty() && typed.size() <= 9;

		for (char const c : typed)
		{
			digits = digits && c >= '0' && c <= '9';
		}

		// FUN_004ec650, whatever the window answered: a time over 10 ms is taken (and its 16.16 length with it), the three ticks always
		if (digits && atoi(typed.c_str()) > 10)
		{
			edited.milliseconds = atoi(typed.c_str());
		}

		edited.flags = (widgets[4].checked ? 1 : 0) | (widgets[6].checked ? 2 : 0) | (widgets[8].checked ? 4 : 0);

		if (answer == 10)
		{
			// the name button: the hierarchy's own list (FUN_0047b450, "Select Sequence"); a pick sets the record's sequence, Cancel keeps it; then the window again
			std::vector<host::SequenceInfo> sequences;
			std::vector<std::string> names;

			PlacedModelSequences(&sequences);

			for (host::SequenceInfo const &one : sequences)
			{
				names.push_back(one.name);
			}

			DialogList("Select Sequence", names, [edited, sequences](int32_t picked)
			{
				host::PlacedHierarchySequence again = edited;

				if (picked >= 0 && (size_t)picked < sequences.size())
				{
					again.sequence = sequences[(size_t)picked].number;
					again.sub = sequences[(size_t)picked].subNumber;
				}

				PlacedHierarchyEditWindow(again);
			});
			return;
		}

		if (placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size())
		{
			return;
		}

		std::vector<host::PlacedHierarchySequence> records = placementHierarchies[(size_t)placedHierarchyShown].sequences;
		size_t place = records.size();

		for (size_t i = 0; i < records.size(); i++)
		{
			if (records[i].index == edited.index)
			{
				place = i;
			}
		}

		if (place == records.size())
		{
			return;
		}

		records[place] = edited;

		// FUN_004ea9a0 0x4ea9f4-0x4eaaac: an Initial sequence is the only one: every other record loses Initial sequence and Playing at start
		if (edited.flags & 2)
		{
			for (size_t i = 0; i < records.size(); i++)
			{
				if (i != place)
				{
					records[i].flags &= ~6;
				}
			}
		}

		if (PlacedHierarchyWriteRecords(records))
		{
			PlacedHierarchyMakeCurrent(place);
		}
	});
}

// PH-11 / PH-12 (reassess/ph8-13-map-vasquez.json): the current record's timing number `timing` written back as `edited`, the record kept current
static void PlacedHierarchyWriteTiming(size_t timing, host::PlacedHierarchyTimedSound const &edited)
{
	if (placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size())
	{
		return;
	}

	std::vector<host::PlacedHierarchySequence> records = placementHierarchies[(size_t)placedHierarchyShown].sequences;
	size_t const record = placedHierarchyRecord;

	if (record >= records.size() || timing >= records[record].sounds.size())
	{
		return;
	}

	records[record].sounds[timing] = edited;

	if (PlacedHierarchyWriteRecords(records))
	{
		PlacedHierarchyMakeCurrent(record);
	}
}

// PH-11 / PH-12, inteng's "Edit sound times" window (FUN_004ec790, vtable 0x5ae738, run by FUN_004eca00): 600 x 400; "Sound index %d" a BUTTON at 18,20 (370 x 18) that opens the sound
// chooser "Use which sound?" over it (FUN_004eca40 control 1 -> FUN_004eac40; with no sound inteng's message box "Please add a sound first"); "Start time" and "End time " with a number
// field each (18,40 / 18,60, 100 x 18); OK at 200,350. WHATEVER ends it (OK, Return, Escape: there is no Cancel), the fields are read back, each taken when 0 or more
// (PlacedHierarchy_TimingFromFields). Ours closes the window for the chooser and opens it again (NAMED, as PH-4's sequence button: our forms end on any button). `timing` is the
// timing's place in the current record's list.
static void PlacedHierarchyTimingWindow(size_t timing, host::PlacedHierarchyTimedSound draft)
{
	host::DialogFormSpec spec;
	host::DialogWidget soundButton, startLabel, startEdit, endLabel, endEdit, ok;

	spec.title = "Edit sound times";
	spec.width = 600;
	spec.height = 400;
	soundButton.type = host::DialogWidgetType::Button;
	soundButton.text = "Sound index " + std::to_string(draft.soundIndex);
	soundButton.box = { 18, 20, 370, 18 };
	soundButton.answer = 10;
	startLabel.type = host::DialogWidgetType::Label;
	startLabel.text = "Start time";
	startLabel.box = { 118, 40, 100, 18 };
	startEdit.type = host::DialogWidgetType::Edit;
	startEdit.text = std::to_string(draft.start);
	startEdit.box = { 18, 40, 100, 18 };
	endLabel.type = host::DialogWidgetType::Label;
	endLabel.text = "End time ";
	endLabel.box = { 118, 60, 200, 17 };
	endEdit.type = host::DialogWidgetType::Edit;
	endEdit.text = std::to_string(draft.end);
	endEdit.box = { 18, 60, 100, 18 };
	ok.type = host::DialogWidgetType::Button;
	ok.text = "OK";
	ok.box = { 200, 350, 100, 18 };
	ok.answer = host::kDialogOk;

	for (host::DialogWidget const &part : { soundButton, startLabel, startEdit, endLabel, endEdit, ok })
	{
		spec.widgets.push_back(part);
	}

	host::Dialogs_Form(spec, [timing, draft](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		host::PlacedHierarchyTimedSound edited = draft;

		host::PlacedHierarchy_TimingFromFields(widgets[2].text, widgets[4].text, &edited);

		if (answer == 10)
		{
			std::vector<host::PlacedHierarchySound> sounds;

			if (placedHierarchyShown >= 0 && (size_t)placedHierarchyShown < placementHierarchies.size())
			{
				sounds = placementHierarchies[(size_t)placedHierarchyShown].sounds;
			}

			if (sounds.empty())
			{
				DialogMessage("Please add a sound first", [timing, edited]() { PlacedHierarchyTimingWindow(timing, edited); });
				return;
			}

			std::vector<std::string> rows;

			for (host::PlacedHierarchySound const &sound : sounds)
			{
				rows.push_back(host::PlacedHierarchy_SoundRow(sound));
			}

			DialogList("Use which sound?", rows, [timing, edited, sounds](int32_t picked)
			{
				host::PlacedHierarchyTimedSound again = edited;

				if (picked >= 0 && (size_t)picked < sounds.size())
				{
					again.soundIndex = sounds[(size_t)picked].index;
				}

				PlacedHierarchyTimingWindow(timing, again);
			});
			return;
		}

		PlacedHierarchyWriteTiming(timing, edited);
	});
}

// PH-8..10: the shown placed hierarchy's sounds written (PlacedHierarchy_WriteSounds); the list holds them until the level is read again (PlacementFrame, on a new generation)
static bool PlacedHierarchyWriteSounds(std::vector<host::PlacedHierarchySound> const &sounds)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size())
	{
		return false;
	}

	host::SpecialResult const done = open ? host::PlacedHierarchy_WriteSounds(open, (size_t)placedHierarchyShown, sounds) : host::SpecialResult::BadCall;

	if (done != host::SpecialResult::Done)
	{
		Answer(std::string("The placed hierarchy's sounds were not changed: ") + host::SpecialWords(done));
		return false;
	}

	placementHierarchies[(size_t)placedHierarchyShown].sounds = sounds;
	return true;
}

// PH-8 / PH-9: a sound of the shown placed hierarchy in edobtype's Sound Edit window (FUN_004ebd60 -> edit_sound, the window the track sounds and the particle generators open). In: the name;
// the ranges in inteng's units (the file's x DAT_00607bd8, 100 x the level scale: FUN_004ebb80 0x4ebbc6) x 1e-5 m; pitch x 1/128; volume x 100 x 0.007874016 %; Sound Loops live (the
// record's +0x18: FUN_004ebc80(index, 1) and the load FUN_004ebb80(chunk, 1)). Back, OK or Cancel alike (FUN_004ebd60 writes back whatever edit_sound answers), in the file's units again
// (FUN_004ec1b0 0x4ec221: ftol(units / DAT_00607bd8)). INTENG BUG, FIXED (Bishop's ruling Q1 11-17, as TB-T1): Add makes the sound before the window and keeps it on Cancel, leaving a
// "Not Set" sound in the level; ours adds nothing on Cancel.
static void PlacedHierarchySoundWindow(host::PlacedHierarchySound sound, bool isNew)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	double const unit = 100.0 * (local_scale > 0.0 ? local_scale : 1.0);
	auto const ftol = [](double v) { return !(v == v) ? 0 : (v >= 2147483647.0 ? 2147483647 : (v <= -2147483648.0 ? (int32_t)-2147483647 - 1 : (int32_t)v)); };
	uint32_t const memInner = isNew ? 500000u : (uint32_t)ftol((double)sound.inner * unit);
	uint32_t const memOuter = isNew ? 4000000u : (uint32_t)ftol((double)sound.outer * unit);
	host::SoundEditRecord const record = host::SoundEdit_IndexedRecord(sound.wav, memInner, memOuter, sound.pitch, sound.volume, sound.flags, true);
	std::string const soundDirectory = host::TrackLevelSoundDirectory(open);
	std::string const folder = std::string(theGameFolder) + "\\Sound\\" + soundDirectory + "\\";

	SoundEditWindowOpen(record, folder, soundDirectory, [sound, isNew, unit, ftol](host::SoundEditRecord const &back, bool ok)
	{
		if ((isNew && !ok) || placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size())
		{
			return;
		}

		host::PlacedHierarchySound now = sound;
		int32_t inner = 0, outer = 0, pitch = 0, volume = 0, flags = 0;

		host::SoundEdit_IndexedBack(back, sound.flags, &inner, &outer, &pitch, &volume, &flags);
		now.wav = back.name;
		now.inner = (uint32_t)ftol((double)(uint32_t)inner / unit);
		now.outer = (uint32_t)ftol((double)(uint32_t)outer / unit);
		now.pitch = pitch;
		now.volume = volume;
		now.flags = flags;
		now.read = true;
		now.raw.clear();
		now.extra.clear();   // FUN_004ec1b0 makes the chunk again with no extra words

		std::vector<host::PlacedHierarchySound> sounds = placementHierarchies[(size_t)placedHierarchyShown].sounds;
		bool placed = false;

		for (size_t i = 0; i < sounds.size() && !isNew && !placed; i++)
		{
			if (sounds[i].index == sound.index)
			{
				sounds[i] = now;
				placed = true;
			}
		}

		if (!placed)
		{
			sounds.push_back(now);   // Add: at the END of the list (FUN_004eab10 0x4eabd6)
		}

		PlacedHierarchyWriteSounds(sounds);
	});
}

// The current placed hierarchy's pop-up items (page 6): Play (0x7d0, FUN_004e9f70), Stop (0x7d1, FUN_004ea210), the Sequence items (0x7d2 / 0x7db / 0x7d9 / 0x7da), the Sounds items
// (0x7d3 / 0x7d4 / 0x7d5, PH-8..10) and the Sound Timing items (0x7d6 / 0x7d7 / 0x7d8, PH-11..13).
void PlacedHierarchyCommand(int32_t command)
{
	// FUN_004d9af0 0x4db540-0x4db75c: each needs a current object of kind 5
	if (placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size() || placedHierarchyCurrent != placedHierarchyShown)
	{
		return;
	}

	host::PlacedHierarchyInfo const &one = placementHierarchies[(size_t)placedHierarchyShown];

	if (command == 0x7d0)
	{
		// Play: only a STOPPED main player; timer 0, the record's time (16.16 seconds, [4]) and loop ([5]); the record's sounds start with their timings (later steps)
		if (!HierarchyView_IsShown() || HierarchyView_IsPlaying())
		{
			return;
		}

		bool loop = true;

		if (placedHierarchyRecord < one.sequences.size())
		{
			host::PlacedHierarchySequence const &record = one.sequences[placedHierarchyRecord];

			loop = (record.flags & 1) != 0;
			HierarchyView_SetSequenceSeconds((int32_t)(((int64_t)record.milliseconds << 16) / 1000));
		}

		HierarchyView_SetPosition(0);
		HierarchyView_Play(true, loop, false);

		// PH-2: each timing is linked to the first sound of its index; a linked one that starts at 0 starts at once (FUN_004fc350 / FUN_004ebfe0 at the hierarchy's place)
		int32_t place[3];

		PlacedHierarchySoundsSync(one, place);
		placedTimerBefore = 0;

		if (placedHierarchyRecord < one.sequences.size())
		{
			for (host::PlacedHierarchyTimedSound const &timing : one.sequences[placedHierarchyRecord].sounds)
			{
				if (timing.start == 0)
				{
					host::HierarchySounds_Start(timing.soundIndex, place);
				}
			}
		}
	}
	else if (command == 0x7d1)
	{
		// Stop: only a running one; the pose stays where it is (and every sound of the object stops: later steps)
		if (HierarchyView_IsShown() && HierarchyView_IsPlaying())
		{
			HierarchyView_Play(false, true, false);
		}
	}
	else if (command == 0x7db)
	{
		// PH-5 Choose sequence (FUN_004ea4f0): the list "Select Sequence" of the records' indexes ("%d", in the list's order); Cancel: nothing; a pick: FUN_004ea3f0
		std::vector<std::string> rows;

		for (host::PlacedHierarchySequence const &record : one.sequences)
		{
			rows.push_back(std::to_string(record.index));
		}

		DialogList("Select Sequence", rows, [](int32_t picked)
		{
			if (picked >= 0 && placedHierarchyShown >= 0 && (size_t)placedHierarchyShown < placementHierarchies.size())
			{
				PlacedHierarchyMakeCurrent((size_t)picked);
			}
		});
	}
	else if (command == 0x7d9)
	{
		// PH-6 Add sequence (FUN_004ea490): the hierarchy's own "Select Sequence" list (FUN_0047b450: the names, 600 wide); Cancel: nothing; a pick: a new record at the END with the lowest unused
		// index, that sequence, 1000 ms, Loop on, Initial and Playing off, no sound timings (FUN_004ea290), then it is current (FUN_004ea3f0)
		std::vector<host::SequenceInfo> sequences;
		std::vector<std::string> names;

		if (!PlacedModelSequences(&sequences))
		{
			return;
		}

		for (host::SequenceInfo const &each : sequences)
		{
			names.push_back(each.name);
		}

		DialogList("Select Sequence", names, [sequences](int32_t picked)
		{
			if (picked < 0 || (size_t)picked >= sequences.size() || placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size())
			{
				return;
			}

			std::vector<host::PlacedHierarchySequence> records = placementHierarchies[(size_t)placedHierarchyShown].sequences;
			host::PlacedHierarchySequence made;

			made.index = PlacedHierarchyFreeIndex(records);
			made.sequence = sequences[(size_t)picked].number;
			made.sub = sequences[(size_t)picked].subNumber;
			made.milliseconds = 1000;
			made.flags = 1;
			records.push_back(made);

			if (PlacedHierarchyWriteRecords(records))
			{
				PlacedHierarchyMakeCurrent(records.size() - 1);
			}
		});
	}
	else if (command == 0x7da)
	{
		// PH-7 Delete sequence (FUN_004ea800): with ONE record inteng's message box "Must have at least one sequence"; otherwise Stop, the current record taken out, the FIRST one current
		if (placedHierarchyRecord >= one.sequences.size())
		{
			return;
		}

		if (one.sequences.size() == 1)
		{
			DialogMessage("Must have at least one sequence");
			return;
		}

		std::vector<host::PlacedHierarchySequence> records = one.sequences;

		HierarchyView_Play(false, true, false);
		records.erase(records.begin() + (std::ptrdiff_t)placedHierarchyRecord);

		if (PlacedHierarchyWriteRecords(records))
		{
			PlacedHierarchyMakeCurrent(0);
		}
	}
	else if (command == 0x7d2)
	{
		// PH-4 Edit sequence (FUN_004ea9a0): needs a current record; Stop, then the window
		if (placedHierarchyRecord >= one.sequences.size())
		{
			return;
		}

		HierarchyView_Play(false, true, false);
		PlacedHierarchyEditWindow(one.sequences[placedHierarchyRecord]);
	}
	else if (command == 0x7d6)
	{
		// PH-11 Sound Timing > Add (FUN_004eb0f0): Stop FIRST, then a current record is needed (0x4eb10b, 0x4eb110: with none nothing more, no message); a new timing (FUN_004ebb60) at
		// the END of its list, then the window on it. There is no Cancel: the timing stays however the window is closed.
		if (HierarchyView_IsShown() && HierarchyView_IsPlaying())
		{
			HierarchyView_Play(false, true, false);
		}

		if (placedHierarchyRecord >= one.sequences.size())
		{
			return;
		}

		std::vector<host::PlacedHierarchySequence> records = one.sequences;
		size_t const record = placedHierarchyRecord;
		host::PlacedHierarchyTimedSound const fresh = host::PlacedHierarchy_NewTiming();

		records[record].sounds.push_back(fresh);

		size_t const timing = records[record].sounds.size() - 1;

		if (PlacedHierarchyWriteRecords(records))
		{
			PlacedHierarchyMakeCurrent(record);
			PlacedHierarchyTimingWindow(timing, fresh);
		}
	}
	else if (command == 0x7d7 || command == 0x7d8)
	{
		// PH-12 Sound Timing > Edit (FUN_004eb1f0) / PH-13 > Remove (FUN_004eb270): a current record is needed BEFORE Stop (0x4eb20b / 0x4eb274: with none nothing at all happens); Stop;
		// the timing chooser (FUN_004eb340: "Select sound to edit" / "Remove which sound?", the rows "%d start %d , end %d"; no timing: nothing, no message). Edit: the window on the
		// pick, edited in place. Remove: the pick taken out, no question asked.
		if (placedHierarchyRecord >= one.sequences.size())
		{
			return;
		}

		if (HierarchyView_IsShown() && HierarchyView_IsPlaying())
		{
			HierarchyView_Play(false, true, false);
		}

		std::vector<host::PlacedHierarchyTimedSound> const timings = one.sequences[placedHierarchyRecord].sounds;

		if (timings.empty())
		{
			return;
		}

		std::vector<std::string> rows;

		for (host::PlacedHierarchyTimedSound const &timing : timings)
		{
			rows.push_back(host::PlacedHierarchy_TimingRow(timing));
		}

		bool const remove = command == 0x7d8;

		DialogList(remove ? "Remove which sound?" : "Select sound to edit", rows, [remove, timings](int32_t picked)
		{
			if (picked < 0 || (size_t)picked >= timings.size() || placedHierarchyShown < 0 || (size_t)placedHierarchyShown >= placementHierarchies.size())
			{
				return;
			}

			if (!remove)
			{
				PlacedHierarchyTimingWindow((size_t)picked, timings[(size_t)picked]);
				return;
			}

			std::vector<host::PlacedHierarchySequence> records = placementHierarchies[(size_t)placedHierarchyShown].sequences;
			size_t const record = placedHierarchyRecord;

			if (record < records.size() && (size_t)picked < records[record].sounds.size())
			{
				records[record].sounds.erase(records[record].sounds.begin() + picked);

				if (PlacedHierarchyWriteRecords(records))
				{
					PlacedHierarchyMakeCurrent(record);
				}
			}
		});
	}
	else if (command == 0x7d3)
	{
		// PH-8 Sounds > Add (FUN_004eab10): Stop; a new sound (FUN_004ebc80(index, 1): the lowest unused index, "Not Set", 5 m / 40 m (500000 / 4000000 inteng units), volume 127, pitch 0,
		// Sound Loops on and live) in the Sound Edit window first; at the end of the list on OK (Cancel adds nothing: the named fix above)
		if (HierarchyView_IsShown() && HierarchyView_IsPlaying())
		{
			HierarchyView_Play(false, true, false);
		}

		host::PlacedHierarchySound fresh;

		fresh.index = host::PlacedHierarchy_FreeSoundIndex(one.sounds);
		fresh.wav = "Not Set";
		fresh.volume = 127;
		fresh.pitch = 0;
		fresh.flags = 1;
		fresh.read = true;
		PlacedHierarchySoundWindow(fresh, true);
	}
	else if (command == 0x7d4 || command == 0x7d5)
	{
		// PH-9 Sounds > Edit (FUN_004eafe0) / PH-10 > Remove (FUN_004eb010): Stop; the sound chooser (FUN_004eac40: "Select sound to edit" / "Delete which sound?", "%d %s" rows; with no
		// sound inteng's message box "Please add a sound first"). Edit: the Sound Edit window on the pick. Remove: the pick taken out, no question asked; the timings that name it keep its
		// index (and play nothing until a sound takes the index again)
		if (HierarchyView_IsShown() && HierarchyView_IsPlaying())
		{
			HierarchyView_Play(false, true, false);
		}

		std::vector<host::PlacedHierarchySound> const sounds = one.sounds;

		if (sounds.empty())
		{
			DialogMessage("Please add a sound first");
			return;
		}

		std::vector<std::string> rows;

		for (host::PlacedHierarchySound const &sound : sounds)
		{
			rows.push_back(host::PlacedHierarchy_SoundRow(sound));
		}

		bool const remove = command == 0x7d5;

		DialogList(remove ? "Delete which sound?" : "Select sound to edit", rows, [remove, sounds](int32_t picked)
		{
			if (picked < 0 || (size_t)picked >= sounds.size())
			{
				return;
			}

			if (!remove)
			{
				PlacedHierarchySoundWindow(sounds[(size_t)picked], false);
				return;
			}

			std::vector<host::PlacedHierarchySound> left = sounds;

			left.erase(left.begin() + picked);
			PlacedHierarchyWriteSounds(left);
		});
	}
}
