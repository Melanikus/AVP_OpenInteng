// track_edit_screen.cpp: TRACK EDITING: a track's copy and paste, its play in the level, and Track Editing's edits, its lines in the world and its frame. Moved here
// whole from editor_main.cpp on 2026-11-18 by Melanikus (Frost), so that each file stays within what Snyk Code reads (editor_shared.h says why); nothing in it
// changed. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

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

// Copy Track and Paste Track Absolute (Object Placement; track_edit.h, inteng's FUN_004f2ff0 and FUN_004f3330). The clipboard is the editor's, kept
// until the next copy, as inteng keeps its own.
static host::TrackClip trackClipboard;

void CopyTheTrack()
{
	host::Selected const selected = host::Selection_Get();

	if (selected.kind != host::SelectedKind::Object)
	{
		statusWords = "Copy Track: select an object first";
		return;
	}

	host::TrackEditResult const result = host::TrackCopy(LevelObjects_OpenForEdit(), selected.objectId, &trackClipboard);

	statusWords = result == host::TrackEditResult::Done ? "Track copied" : host::TrackEditWords(result);

	if (result == host::TrackEditResult::NoTrack)
	{
		DialogMessage(host::TrackEditWords(result));   // inteng: FUN_00474030 "Object doesn't have a track" shown by FUN_0057c800 (FUN_004f2ff0 0x4f303d)
	}
}

// inteng's Paste Track (FUN_004f3330 0x4f368f-0x4f36af) loads the pasted track into the track editor and shows page 2, state 2: the track is being edited
static void TrackEditAfterPaste(char const *what, bool followTheObject = true);

void PasteTheTrackAbsolute()
{
	host::Selected const selected = host::Selection_Get();

	if (selected.kind != host::SelectedKind::Object)
	{
		statusWords = "Paste Track: select an object first";
		return;
	}

	host::TrackEditResult const result = host::TrackPasteAbsolute(LevelObjects_OpenForEdit(), selected.objectId, trackClipboard);

	if (result == host::TrackEditResult::ClipboardEmpty)
	{
		DialogMessage(host::TrackEditWords(result));   // inteng: FUN_00474030 + FUN_0057c800 (FUN_004f3330 0x4f3373 / 0x4f33cc)
	}

	if (result == host::TrackEditResult::Done)
	{
		trackDirty = true;
		LoadTheObjectListAgain();
		ShowTheSelected(host::Selection_Get());
		statusWords = "Track pasted (absolute)";
		TrackEditAfterPaste("Track pasted (absolute)", false);
	}
	else
	{
		statusWords = host::TrackEditWords(result);
	}
}

void PasteTheTrackRelative()
{
	host::Selected const selected = host::Selection_Get();

	if (selected.kind != host::SelectedKind::Object)
	{
		statusWords = "Paste Track: select an object first";
		return;
	}

	host::TrackEditResult const result = host::TrackPasteRelative(LevelObjects_OpenForEdit(), selected.objectId, trackClipboard);

	if (result == host::TrackEditResult::ClipboardEmpty)
	{
		DialogMessage(host::TrackEditWords(result));   // inteng: FUN_00474030 + FUN_0057c800 (FUN_004f3330 0x4f3373 / 0x4f33cc)
	}

	if (result == host::TrackEditResult::Done)
	{
		trackDirty = true;
		LoadTheObjectListAgain();
		ShowTheSelected(host::Selection_Get());
		statusWords = "Track pasted (relative)";
		TrackEditAfterPaste("Track pasted (relative: turned and moved onto the object)");
	}
	else
	{
		statusWords = host::TrackEditWords(result);
	}
}

// ---- TRACK EDITING: inteng's Edit Track (0x36) and the pages 2 to 5 of its placement menu (track_editor.h has the state machine and the commands; track-menu-pairs.md the menus).
// This is the screen's part: the 3D cursor, the prompts, the screen change, and PLAYING with the game's own track code (track_play_game.h), shown as a cross in the three views.
bool trackShowStart = true;        // inteng's O: "Toggle display of section start position" (FUN_004f0c70 hides the start marker; 0x4dc8d7)
int trackFlyBase[3] = { 0, 0, 0 };   // the camera's turn, look and roll when the marker was last taken: how far it has turned since is how far the marker turned
int32_t trackFlyOffset[3] = { 0, 0, 0 };
bool trackTurnAboutPivot = false;   // inteng's P: "Switch between rotation about pivot and about centre": the Turn End's rotation
host::TrackEditor trackEd;

bool TrackPivotEditing()
{
	return trackEd.state == host::TrackState::PivotEdit;
}
host::TrackPose trackPose = {};
bool trackPlaying = false;
static ULONGLONG trackPlayTick = 0;
std::vector<host::TrackLine> trackPlayBase;   // the lines of the track as they were when the play began
bool trackPlayBaseWanted = false;
std::string trackEditWords;
int trackPromptSound = 0;
static int32_t const kTrackPrompt[] = { 0, kPromptTrackTime, kPromptTrackFlags, kPromptTrackSound, kPromptTrackDelete };   // host::TrackPrompt -> the prompt numbers of editor_actions.h (by name: cut 9 renumbered Turn End to 46, and the bare 41 here stayed behind)

size_t ScreenOfName(char const *name)
{
	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		if (strcmp(host::kIntengScreens[s].name, name) == 0)
		{
			return s;
		}
	}

	return host::kIntengScreenCount;
}

size_t ScreenOfTable(char const *table)
{
	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		if (strcmp(host::kIntengScreens[s].table, table) == 0)
		{
			return s;
		}
	}

	return host::kIntengScreenCount;
}

// Object Placement's Acoustic Mode (its menu page 7 in inteng): the screen "Acoustic" (reassess/object-placement.md AC-1 .. AC-9)
bool IsTheAcousticScreen(size_t screen)
{
	return screen < host::kIntengScreenCount && strcmp(host::kIntengScreens[screen].name, "Acoustic") == 0;
}

bool OnAcousticScreen()
{
	return IsTheAcousticScreen(barScreen);
}

bool OnTheTrackScreen()
{
	return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Track Editing") == 0;
}

// TE-4 (FUN_004f09e0): while a track plays, inteng moves THE OBJECT along it every frame, so it is seen moving in the view, and leaves it where the play stopped (the track's end) until Stop
// track play (0x515) calls FUN_004f0da0, which puts it back at the current section's end. Ours: the play holds the object's live copy (level_view.h's shared "fly the real object"; in Track
// Editing it is already flying at the section's end, T-3c, and the play takes it over) at the game's track pose, the object's own turn kept unless the track turns it; at the track's end it
// stays there; Stop track play or leaving lets it go and TrackWorldLines puts it at the section's end again. Nothing is written to the level. The state line's trackplayed field says what the
// play did: "moves <n> from <x y z> to <x y z>" (the file's units: the first and the last place it was put at).
static bool trackPlayFlying = false;
static int32_t trackPlayOwnMatrix[9];
static int32_t trackPlayMoves = 0;
static int32_t trackPlayFirst[3] = { 0, 0, 0 }, trackPlayLast[3] = { 0, 0, 0 };

static void TrackPlayPutCopy()
{
	if (trackPlayFlying)
	{
		LevelView_FlyPutMatrix(trackPose.world, trackPose.turned ? trackPose.matrix : trackPlayOwnMatrix);

		if (trackPlayMoves == 0)
		{
			memcpy(trackPlayFirst, trackPose.place, sizeof(trackPlayFirst));
		}

		memcpy(trackPlayLast, trackPose.place, sizeof(trackPlayLast));
		trackPlayMoves += trackPlayMoves < 0x7fffffff ? 1 : 0;
	}
}

static void TrackStopPlaying()
{
	if (trackPlaying)
	{
		host::TrackPlay_Stop();
		trackPlaying = false;
	}

	if (trackPlayFlying)
	{
		LevelView_FlyEnd();
		trackPlayFlying = false;
	}
}

void ShowTheScreen(size_t screen)
{
	if (screen < host::kIntengScreenCount && screen != barScreen)
	{
		size_t const was = barScreen;

		barScreen = screen;
		OnScreenChanged(was, screen);

		if (strcmp(host::kIntengScreens[screen].name, "Strategy Data") == 0)
		{
			StrategyCleanNow();   // FUN_00522ab0 0x522ae2: DAT_00914950 = 0 on entry (Object Placement's Tab: FUN_00524ce0) (W-3)
		}

		if (!LoadTheMenus())
		{
			LogString("menus: the bar of screen " + std::to_string(barScreen) + " couldn't be made, so the editor closed");
			running = false;
			bRunning = false;
		}
	}
}

// What the command did, shown and done: the status line, the level's list, the 3D cursor, a prompt, the play, the way back to Object Placement
static void TrackEditApply(host::TrackAction const &a)
{
	trackEditWords = a.words;
	statusWords = trackEditWords.c_str();

	if (a.stopPlay)
	{
		TrackStopPlaying();
	}

	if (a.markerTurnTaken && theCamera)
	{
		trackFlyBase[0] = theCamera->turn;   // the marker's turn is the section's now: counted again from here
		trackFlyBase[1] = theCamera->look;
		trackFlyBase[2] = theCamera->roll;

		// ... and its place: the carry from where the marker is now (CAM-4: a swing about the pivot is in the place, not in the camera's offset)
		double const scale = local_scale > 0.0 ? local_scale : 1.0;
		int32_t const *const at = a.cursorSet ? a.cursor : threeCursor;

		trackFlyOffset[0] = (int32_t)floor((double)at[0] * scale + 0.5) - theCamera->x;
		trackFlyOffset[1] = (int32_t)floor((double)at[1] * scale + 0.5) - theCamera->y;
		trackFlyOffset[2] = (int32_t)floor((double)at[2] * scale + 0.5) - theCamera->z;
	}

	if (a.changed)
	{
		trackDirty = true;
		LoadTheObjectListAgain();
		ShowTheSelected(host::Selection_Get());
	}

	if (a.cursorSet)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			threeCursor[axis] = a.cursor[axis];
		}
	}

	if (trackEd.state != host::TrackState::Off)
	{
		trackSection = (int32_t)trackEd.current;
		trackDirty = true;
	}

	// Edit Pivot Point: FUN_004f3770 turns the split screen off first (0x4f37aa), then makes the three views
	if (trackEd.state == host::TrackState::PivotEdit && placementSplit)
	{
		placementSplit = false;
		splitCentreOn = false;
	}

	if (a.startPlay)
	{
		extern double local_scale;

		host::TrackPlay_UseLevelScale(local_scale > 0.0 ? local_scale : 1.0);
		host::TrackPlay_UseSoundDirectory(host::TrackLevelSoundDirectory(LevelObjects_OpenForEdit()));
		host::TrackPlay_UseSilence(editorHiddenRun);
		trackPlaying = host::TrackPlay_Start(a.playData, local_scale > 0.0 ? local_scale : 1.0, &trackPose, &a.playSounds);
		trackPlayTick = GetTickCount64();
		trackPlayBaseWanted = true;
		trackDirty = true;

		// TE-4: the object itself is seen moving: the play holds its live copy (Track Editing's own one at the section's end, taken over, or a new one), put at the pose from the start
		trackPlayMoves = 0;

		if (trackPlaying && !trackPlayFlying && !PlacementFlyHoldsTheCopy())
		{
			bool const begun = !LevelView_FlyActive() && LevelView_FlyBegin(trackEd.id);

			trackPlayFlying = LevelView_FlyActive() && LevelView_FlyMatrix(trackPlayOwnMatrix);

			if (trackPlayFlying)
			{
				TrackPlayPutCopy();
			}
			else if (begun)
			{
				LevelView_FlyEnd();
			}
		}

		if (!trackPlaying)
		{
			trackEditWords = "Play Track: the game's track code could not start this track";
			statusWords = trackEditWords.c_str();
			host::TrackEdit_PlayEnded(&trackEd);
		}
	}

	if (a.prompt == host::TrackPrompt::TrackFlags)
	{
		// Track Properties (FUN_004f4090): a 400 x 200 form, four check boxes at (0x10, 0x28 + 0x14 x n) with their words at (0x20, ...), Ok at (100, 0xa0) and Cancel at (200, 0xa0), ticked as the track's flags are
		static char const *const labels[4] = { "Loop Track", "Loop back and forth", "Playing at start", "Use Track Smoothing" };
		static char const letters[4] = { 'L', 'B', 'P', 'S' };
		host::DialogFormSpec spec;

		spec.title = "Track Properties";
		spec.width = 400;
		spec.height = 200;

		for (int i = 0; i < 4; i++)
		{
			host::DialogWidget check, words;

			check.type = host::DialogWidgetType::Check;
			check.box = host::DialogRect{ 0x10, 0x28 + 0x14 * i - 0x0, 12, 12 };
			check.checked = a.promptDefault.find(letters[i]) != std::string::npos;
			check.text = labels[i];
			words.type = host::DialogWidgetType::Label;
			words.box = host::DialogRect{ 0x20, 0x28 + 0x14 * i, 200, 0x12 };
			words.text = labels[i];
			spec.widgets.push_back(check);
			spec.widgets.push_back(words);
		}

		host::DialogWidget ok, cancel;

		ok.type = host::DialogWidgetType::Button;
		ok.box = host::DialogRect{ 100, 0xa0, 0x32, 0x10 };
		ok.text = "Ok";
		ok.answer = host::kDialogOk;
		cancel.type = host::DialogWidgetType::Button;
		cancel.box = host::DialogRect{ 200, 0xa0, 0x32, 0x10 };
		cancel.text = "Cancel";
		cancel.answer = host::kDialogEscape;
		spec.widgets.push_back(ok);
		spec.widgets.push_back(cancel);

		trackPromptSound = a.soundIndex;
		host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
		{
			if (answer != host::kDialogOk)
			{
				Answer("Cancelled");
				return;
			}

			std::string typed;
			int box = 0;

			for (host::DialogWidget const &w : widgets)
			{
				if (w.type == host::DialogWidgetType::Check)
				{
					typed += w.checked ? std::string(1, "LBPS"[box]) : std::string();
					box++;
				}
			}

			promptFor = kPromptTrackFlags;
			promptText = typed.empty() ? std::string("0") : typed;
			RunThePrompt();
		});
	}
	else if (a.prompt == host::TrackPrompt::Sound)
	{
		// Sound / Start Sound / End Sound > Edit: inteng's FUN_004f2bf0, edobtype's Sound Edit window, the one the placed sounds and the particle generators open. In: the name; the two ranges in
		// inteng's units (the file's x 100 x the scale: FUN_004f2af0 0x4f2b41; a new sound's 500000 / 1000000 as FUN_004f2a60 made them) x 1e-5 m; pitch x 0.0078125; volume x 100 x 0.007874016 %;
		// Sound Loops the only live box (mask 2), ticked when the sound's bit 0 is set. Back after OK OR Cancel alike (not Cancel on a new sound: TB-T1 below): the four numbers through __ftol, bit 0 = Sound Loops, and the ranges into the
		// file's units as the save makes them (FUN_004f1300 0x4f17bd: ftol(mem / unit)). (Until 2026-11-17 a gray form of our own, track-edit-facts.md named deviation 3; T-1 of
		// reassess/track-editing-closeout-vasquez.md.)
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		double const unit = 100.0 * (local_scale > 0.0 ? local_scale : 1.0);
		auto const ftol = [](double v) { return !(v == v) ? 0 : (v >= 2147483647.0 ? 2147483647 : (v <= -2147483648.0 ? (int32_t)-2147483647 - 1 : (int32_t)v)); };
		host::TrackSound const sound = a.sound;
		uint32_t const memInner = a.soundNew ? 500000u : (uint32_t)ftol((double)sound.innerRange * unit);
		uint32_t const memOuter = a.soundNew ? 1000000u : (uint32_t)ftol((double)sound.outerRange * unit);
		host::SoundEditRecord const record = host::SoundEdit_IndexedRecord(sound.wav, memInner, memOuter, sound.pitch, sound.maxVolume, sound.flags, true);
		std::string const soundDirectory = host::TrackLevelSoundDirectory(open);
		std::string const folder = std::string(theGameFolder) + "\\sound\\" + soundDirectory + "\\";
		int const index = a.soundIndex;

		trackPromptSound = index;
		SoundEditWindowOpen(record, folder, soundDirectory, [sound, index, unit, ftol, isNew = a.soundNew](host::SoundEditRecord const &back, bool ok)
		{
			// INTENG BUG TB-T1, FIXED (Bishop's ruling 11-17, track-editing-closeout-vasquez.md): inteng makes the new sound (FUN_004f2a60) before the window and keeps it on Cancel too, so
			// Edit + Cancel on a missing sound left a "Not Set" sound in the level (the game finds no wav of that name and plays nothing). Ours: Cancel on a sound this Edit made adds
			// nothing; OK on it, and OK or Cancel on a sound the track had, are inteng's
			if (isNew && !ok)
			{
				return;
			}

			host::TrackSound now = sound;
			int32_t inner = 0, outer = 0, pitch = 0, volume = 0, flags = 0;

			host::SoundEdit_IndexedBack(back, sound.flags, &inner, &outer, &pitch, &volume, &flags);
			now.wav = back.name;
			now.innerRange = (uint32_t)ftol((double)(uint32_t)inner / unit);
			now.outerRange = (uint32_t)ftol((double)(uint32_t)outer / unit);
			now.pitch = pitch;
			now.maxVolume = volume;
			now.flags = flags;

			host::TrackAction action;

			host::TrackEdit_SetSound(LevelObjects_OpenForEdit(), &trackEd, index, now, &action);
			TrackEditApply(action);
		});
	}
	else if (a.prompt == host::TrackPrompt::SectionTime)
	{
		// inteng asks it with its text dialog (FUN_00474bc0, "Time for section" 0x5e9340): the seconds, Ok / Cancel
		trackPromptSound = a.soundIndex;
		TextAs(kPromptTrackTime, "Time for section :", a.promptDefault);
	}
	else if (a.prompt == host::TrackPrompt::DeleteTrack)
	{
		// inteng asks Yes / No (FUN_00474a40)
		DialogConfirm("Delete this track?", [](bool yes)
		{
			if (yes)
			{
				promptFor = kPromptTrackDelete;
				promptText = "yes";
				RunThePrompt();
			}
		});
	}
	else if (a.prompt != host::TrackPrompt::None)
	{
		trackPromptSound = a.soundIndex;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kTrackPrompt[(int)a.prompt];
		promptText = a.promptDefault;
	}

	if (a.leave)
	{
		TrackStopPlaying();
		ShowTheScreen(ScreenOfTable("0x5e51b8"));
	}
}

static OpenLevelFile *TrackLevel()
{
	extern double local_scale;

	host::TrackPlay_UseLevelScale(local_scale > 0.0 ? local_scale : 1.0);
	return LevelObjects_OpenForEdit();
}

// A Track Editing item (inteng's command number): what it does is track_editor.cpp's switch of FUN_004d9af0
void TrackEditCommand(int32_t command)
{
	OpenLevelFile *const open = TrackLevel();

	if (!open)
	{
		statusWords = "No level is held to work on";
		return;
	}

	host::TrackAction action;
	bool const pivotBefore = trackEd.state == host::TrackState::PivotEdit;

	host::TrackEdit_Command(open, &trackEd, command, threeCursor, &action);

	// Edit Pivot Point's start (FUN_004f3770: flies the object when it is not flying, then FUN_004704d0(10) at 0x4f3792; camera-saved-vasquez.md CS-18)
	if (!pivotBefore && trackEd.state == host::TrackState::PivotEdit)
	{
		CameraPositionSaved(10);
	}

	TrackEditApply(action);
}

// SHIFT+T, "Updating track positions" (inteng's FUN_004efd50, object-placement.md OP-83, T-2 of reassess/track-editing-closeout-vasquez.md; Vasquez 11-17): on Object Placement, sound placement and
// Track Editing alike (the key test 0x4dd0cd-0x4dd0e1 comes after FUN_004d9af0's state switch, so every state takes it). inteng's message box first (FUN_00474030 "Updating track positions"
// 0x5e9324 shown by FUN_0057c800(100, 100): Ok), then every object holding a track has it re-based onto where the object is now and written, in the list's order (track_editor.h
// TrackEdit_UpdatePositions); a track being edited is written first and Track Editing ends (INTENG BUG TB-T2, FIXED: inteng stays on Track Editing with no track)
void UpdateTrackPositions()
{
	DialogMessage("Updating track positions", []()
	{
		OpenLevelFile *const open = TrackLevel();

		if (!open)
		{
			statusWords = "No level is held to work on";
			return;
		}

		host::TrackAction action;

		host::TrackEdit_UpdatePositions(open, &trackEd, &action);
		TrackEditApply(action);
	});
}

// The screen "Track Editing" was chosen (Object Placement's Edit Track, 0x36): the selected object's track is loaded. Gives back false (the screen stays where it was) when it cannot.
bool TrackEditEnter(bool followTheObject)
{
	OpenLevelFile *const open = TrackLevel();
	host::Selected const selected = host::Selection_Get();

	if (!open || selected.kind != host::SelectedKind::Object)
	{
		statusWords = "Edit Track: select an object first (click it, or choose it in the object list)";
		return false;
	}

	host::TrackAction action;

	if (!host::TrackEdit_Enter(open, selected.objectId, &trackEd, &action, followTheObject))
	{
		trackEditWords = action.words;
		statusWords = trackEditWords.c_str();
		return false;
	}

	trackSection = 0;

	TrackEditApply(action);
	return true;
}

// Paste Track: the pasted track is the one being edited (the Track Edit screen, state 2), as inteng shows it after FUN_004f3330
static void TrackEditAfterPaste(char const *what, bool followTheObject)
{
	if (TrackEditEnter(followTheObject))
	{
		ShowTheScreen(ScreenOfName("Track Editing"));
		trackEditWords = std::string(what) + ": " + trackEditWords;
		statusWords = trackEditWords.c_str();
	}
}

// The Track Edit screen is left some other way than Stop Track Edit (Back to Construction, the Screens menu): the editing stops as Stop Track Edit does
void TrackEditLeftByOtherWay()
{
	if (trackEd.state == host::TrackState::Off)
	{
		return;
	}

	OpenLevelFile *const open = TrackLevel();
	host::TrackAction action;

	if (open)
	{
		host::TrackEdit_Command(open, &trackEd, 0x51c, threeCursor, &action);
	}

	TrackStopPlaying();
	trackEd.state = host::TrackState::Off;
	trackEd.working.reset();   // (a track the loader refused is let go too: the level keeps the one it had)

	if (action.changed)
	{
		trackDirty = true;
		LoadTheObjectListAgain();
		ShowTheSelected(host::Selection_Get());
	}
}

// The prompts of the track items: Enter
void RunTrackPrompt(int32_t kind, std::string const &typed)
{
	OpenLevelFile *const open = TrackLevel();
	host::TrackAction action;

	if (!open)
	{
		statusWords = "No level is held to work on";
		return;
	}

	if (kind == kTrackPrompt[(int)host::TrackPrompt::SectionTime])
	{
		host::TrackEdit_AnswerTime(open, &trackEd, typed, &action);
	}
	else if (kind == kTrackPrompt[(int)host::TrackPrompt::TrackFlags])
	{
		host::TrackEdit_AnswerFlags(open, &trackEd, typed, &action);
	}
	else if (kind == kTrackPrompt[(int)host::TrackPrompt::Sound])
	{
		host::TrackEdit_AnswerSound(open, &trackEd, trackPromptSound, typed, &action);
	}
	else
	{
		host::TrackEdit_AnswerDelete(open, &trackEd, typed, &action);
	}

	TrackEditApply(action);
}

// Each frame: a playing track is moved by the game's Update_Track_Position (frame time in 16.16 seconds, at most a quarter of a second so a slow frame does not skip a section)
// The track in the 3D view (inteng shows it in states 2 to 4 as the two marker objects and the track's other markers in the world): every section as dim lines, the current section's start and end as crosses with the line
// between (TrackView_Section, the same lines as the three views), the end marker (the 3D cursor) as a cross while the end is edited, and the object's place as a cross while the track plays. The track's numbers are the
// file's units, the world's are those times local_scale. Made again only when something it shows changed. (Added 2026-11-15 by Melanikus (Frost).)
// The track the screen draws: the one in memory while it is edited (the level's bytes keep the old one until Stop Track Edit), else the level's
host::TrackEditResult TrackClipNow(OpenLevelFile *open, uint8_t const *id, host::TrackClip *clip)
{
	if (host::TrackEdit_WorkingClip(trackEd, id, clip))
	{
		return host::TrackEditResult::Done;
	}

	return host::TrackCopy(open, id, clip);
}


// The object itself is shown at the current section's end (FUN_004f0da0): in the editing's states but the end edit (where it is the flying marker) and the play (T-3c)
static bool TrackObjectAtEndWanted()
{
	return OnTheTrackScreen() && trackEd.working && trackEd.current < trackEd.working->track.sections.size() &&
		(trackEd.state == host::TrackState::Loaded || trackEd.state == host::TrackState::PivotEdit) && !PlacementFlyHoldsTheCopy() && !trackPlayFlying;   // (the play holds it: TE-4)
}

void TrackWorldLines()
{
	// THE END MARKER IS THE REAL OBJECT (inteng's end edit works with the object itself, flown with F: FUN_004dd280, 0x4dc9a6): while the end is edited it is drawn at the marker's place and turn, live
	{
		bool const flyWanted = OnTheTrackScreen() && trackEd.state == host::TrackState::EndEdit && trackEd.working && trackEd.current < trackEd.working->track.sections.size();

		if (flyWanted && !LevelView_FlyActive())
		{
			LevelView_FlyBegin(trackEd.id);
		}

		if (flyWanted && LevelView_FlyActive() && !TrackMarkerSnapping())   // (while V snaps the marker, the snap puts the live copy)
		{
			double const flyScale = local_scale > 0.0 ? local_scale : 1.0;
			int32_t const world[3] = { (int32_t)floor((double)threeCursor[0] * flyScale + 0.5), (int32_t)floor((double)threeCursor[1] * flyScale + 0.5), (int32_t)floor((double)threeCursor[2] * flyScale + 0.5) };
			float turned[4], base[4];

			// turned from the marker's base: the section's end turn, or the start's after Jump To Start (T-3a)
			if (host::TrackEdit_MarkerBase(trackEd, base) && host::TrackPlay_TurnedBy(base, trackEd.markerTurn[0], trackEd.markerTurn[1], trackEd.markerTurn[2], turned))
			{
				LevelView_FlyPut(world, turned);
			}
		}
		else if (!flyWanted && LevelView_FlyActive() && !PlacementFlyHoldsTheCopy() && !TrackObjectAtEndWanted() && !trackPlayFlying)   // (Object Placement's F holds the copy itself; the play holds it: TE-4)
		{
			LevelView_FlyEnd();
		}
	}

	// INTENG'S TWO OBJECTS OF THE CURRENT SECTION (T-3c of reassess/track-editing-closeout-vasquez.md; Bishop's ruling (b): our lines and crosses below stay, a named addition): FUN_004f0da0 puts
	// the OBJECT itself at the current section's END (place +0x28..0x30, matrix +0x58) and the START MARKER, a copy of the object made when the track was loaded (FUN_004b2590 in FUN_004eff90), at
	// its START (+0x1c..0x24, +0x34), at every section change, in every state of the editing; O moves the start marker out of sight (0x4dc8d7). While the end is edited the object is the marker
	// above; while the track plays, and after its end until Stop track play, the play holds it at the game's track pose (TE-4, TrackPlayPutCopy; it was a cross only, fixed 11-18).
	{
		bool const editing = OnTheTrackScreen() && trackEd.state != host::TrackState::Off && trackEd.working && trackEd.current < trackEd.working->track.sections.size();
		double const scale = local_scale > 0.0 ? local_scale : 1.0;
		auto const world = [scale](int32_t const file[3], int32_t out[3])
		{
			for (int axis = 0; axis < 3; axis++)
			{
				out[axis] = (int32_t)floor((double)file[axis] * scale + 0.5);
			}
		};

		// the two poses are worked out again only when the track, the section or the state changed, or a copy was just made (each takes the game's track code twice)
		static std::vector<uint8_t> lastTrack;
		static size_t lastSection = ~(size_t)0;
		static int lastState = -1;
		std::vector<uint8_t> trackNow;

		if (editing)
		{
			host::TrackData_Bytes(trackEd.working->track, &trackNow);
		}

		bool const same = editing && trackNow == lastTrack && lastSection == trackEd.current && lastState == (int)trackEd.state;

		lastTrack = trackNow;
		lastSection = trackEd.current;
		lastState = editing ? (int)trackEd.state : -1;

		if (TrackObjectAtEndWanted())
		{
			bool const begun = !LevelView_FlyActive() && LevelView_FlyBegin(trackEd.id);

			if (!same || begun)
			{
				int32_t end[3], at[3];

				host::TrackData_MarkerAt(trackEd.working->track, trackEd.current, true, host::TrackPlay_ObjectPlace, end);
				world(end, at);
				LevelView_FlyPut(at, trackEd.working->track.sections[trackEd.current].quatEnd);
			}
		}

		if (editing && trackShowStart)
		{
			bool const begun = !LevelView_GhostActive() && LevelView_GhostBegin(trackEd.id);

			if (!same || begun)
			{
				int32_t start[3], at[3];

				host::TrackData_MarkerAt(trackEd.working->track, trackEd.current, false, host::TrackPlay_ObjectPlace, start);
				world(start, at);
				LevelView_GhostPut(at, trackEd.working->track.sections[trackEd.current].quatStart);
			}
		}
		else if (LevelView_GhostActive())
		{
			LevelView_GhostEnd();
		}
	}

	static bool shown = false;
	static uint64_t key = ~0ull;

	bool const want = OnTheTrackScreen() && trackEd.state != host::TrackState::Off;

	if (!want)
	{
		if (shown)
		{
			LevelView_WorldLines(nullptr, 0);
			shown = false;
			key = ~0ull;
		}

		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::Selected const picked = host::Selection_Get();
	uint64_t now = (uint64_t)host::Selection_Changes() * 1000003ull + (uint64_t)(trackSection + 1) * 7919ull + (trackShowStart ? 1ull : 0ull) + (uint64_t)trackEd.state * 31ull + (trackDirty ? 99991ull : 0ull);

	now = now * 1000003ull + (uint64_t)(uint32_t)threeCursor[0] * 31ull + (uint64_t)(uint32_t)threeCursor[1] * 17ull + (uint64_t)(uint32_t)threeCursor[2];
	now = now * 1000003ull + (uint64_t)(int64_t)(trackEd.markerTurn[0] * 100.0) * 7ull + (uint64_t)(int64_t)(trackEd.markerTurn[1] * 100.0) * 13ull + (uint64_t)(int64_t)(trackEd.markerTurn[2] * 100.0);

	if (trackPlaying)
	{
		now = ~0ull - 1;   // every frame while it plays: the object moves
	}

	if (shown && now == key)
	{
		return;
	}

	key = now;

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<host::TrackLine> file;
	host::TrackClip clip;

	if (open && picked.kind == host::SelectedKind::Object && TrackClipNow(open, picked.objectId, &clip) == host::TrackEditResult::Done && clip.track.size() > 12)
	{
		uint8_t const *const payload = (uint8_t const *)clip.track.data() + 12;
		size_t const length = clip.track.size() - 12;
		int32_t maxAbs[3] = { 0, 0, 0 };
		size_t count = 0;

		if (host::TrackView_Section(payload, length, 0, -1, &file, maxAbs, &count) && count > 0)
		{
			for (host::TrackLine &line : file)
			{
				line.colour = 0xFF808080u;   // the whole track, grey
			}

			int32_t const section = (int32_t)(((int64_t)trackSection % (int64_t)count + (int64_t)count) % (int64_t)count);
			std::vector<host::TrackLine> current;

			if (host::TrackView_Section(payload, length, (int32_t)(400.0 / scale), section, &current, maxAbs, nullptr, !trackShowStart))
			{
				file.insert(file.end(), current.begin(), current.end());
			}
		}
	}

	// the end marker while the end is edited (a cross at the cursor), and the object while the track plays
	auto const cross = [&](int32_t const place[3], uint32_t colour)
	{
		int32_t const half = (int32_t)(500.0 / scale);

		for (int axis = 0; axis < 3; axis++)
		{
			host::TrackLine line = {};

			memcpy(line.from, place, sizeof(line.from));
			memcpy(line.to, place, sizeof(line.to));
			line.from[axis] -= half;
			line.to[axis] += half;
			line.colour = colour;
			file.push_back(line);
		}
	};

	if (trackEd.state == host::TrackState::EndEdit)
	{
		cross(threeCursor, host::kTrackPivotColour);

		// the marker's three axes, turned as the keys have turned it (red x, green y, blue z): the end turn Finish End Pos Edit takes
		if (trackEd.working && trackEd.current < trackEd.working->track.sections.size())
		{
			float turned[4], base[4];
			double axes[9];

			if (host::TrackEdit_MarkerBase(trackEd, base) && host::TrackPlay_TurnedBy(base, trackEd.markerTurn[0], trackEd.markerTurn[1], trackEd.markerTurn[2], turned) && host::TrackPlay_QuatAxes(turned, axes))
			{
				uint32_t const colours[3] = { 0xFFFF4040u, 0xFF40FF40u, 0xFF4040FFu };
				double const length = 900.0 / scale;

				for (int axis = 0; axis < 3; axis++)
				{
					host::TrackLine line = {};

					for (int k = 0; k < 3; k++)
					{
						line.from[k] = threeCursor[k];
						line.to[k] = threeCursor[k] + (int32_t)floor(axes[axis * 3 + k] * length + 0.5);
					}

					line.colour = colours[axis];
					file.push_back(line);
				}
			}
		}
	}

	if (trackPlaying)
	{
		cross(trackPose.place, host::kTrackOffsetColour);
	}

	std::vector<host::TrackLine> world = file;

	for (host::TrackLine &line : world)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			line.from[axis] = (int32_t)floor((double)line.from[axis] * scale + 0.5);
			line.to[axis] = (int32_t)floor((double)line.to[axis] * scale + 0.5);
		}
	}

	LevelView_WorldLines(world.data(), world.size());
	shown = true;

	if (!TrackPivotEditing())
	{
		trackDirty = false;   // the three views (Pivot Edit) keep it for their own lines
	}
}

void TrackPlayFrame()
{
	StateLine_Add("trackplayed", trackPlayMoves > 0 ? "moves " + std::to_string(trackPlayMoves) + " from " + std::to_string(trackPlayFirst[0]) + " " + std::to_string(trackPlayFirst[1]) + " " +
		std::to_string(trackPlayFirst[2]) + " to " + std::to_string(trackPlayLast[0]) + " " + std::to_string(trackPlayLast[1]) + " " + std::to_string(trackPlayLast[2]) : std::string());   // TE-4

	if (!trackPlaying)
	{
		return;
	}

	ULONGLONG const now = GetTickCount64();
	ULONGLONG elapsed = now - trackPlayTick;

	trackPlayTick = now;

	if (elapsed > 250)
	{
		elapsed = 250;
	}

	bool const goesOn = host::TrackPlay_Step((int32_t)(elapsed * 65536 / 1000), &trackPose);

	TrackPlayPutCopy();

	if (!goesOn)
	{
		// the game's controller is let go; the object STAYS where the track ended (FUN_004f09e0 leaves it at the controller's place) until Stop track play or leaving puts it back
		// (TrackStopPlaying ends the copy)
		host::TrackPlay_Stop();
		trackPlaying = false;
		// the editing stays in state 4 (page 4, Stop track play), as inteng's does: nothing in FUN_004f09e0 changes the state at the end. It was set back to 2 here while page 4 stayed, so
		// Stop track play was then refused ("not on this page ... (state 2)") and nothing could put the object back but leaving (fixed 11-18, TE-4)
		trackEditWords = "The track has reached its end (Stop track play goes back to editing it)";
		statusWords = trackEditWords.c_str();
	}
}
