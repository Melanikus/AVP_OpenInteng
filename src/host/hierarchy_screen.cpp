// hierarchy_screen.cpp: THE HIERARCHY SCREEN: its text lines and keys, entering it, its clicks, the model it shows and that model's sequences, and its forms. Moved
// here whole from editor_main.cpp on 2026-11-18 by Melanikus (Frost), so that each file stays within what Snyk Code reads (editor_shared.h says why); nothing in it
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

static std::string HierarchyFlagName(int bit)
{
	static char const *const names[] = { "0 'Blocking'", "1 'Cause Damage'", "2 'Can be Blocked'", "3 'Invincible'", "4 'Left Step'", "5 'Right Step'", "6 'Hit Floor Thud'", "7 'Hit Floor Sound'", "8 'Metal Chink'",
		"9 'Jump Now'", "'Combat Roll Left'", "'Combat Roll Right'", "'Swipe Sound'", "'Start Pushing'", "'Stop Pushing'", "'Start Moving'", "'Stop Moving'" };

	return bit >= 0 && bit < 17 ? names[bit] : std::to_string(bit);   // FUN_00478f00; the others are the number
}

static std::string HierarchyFloat(char const *format, double a, double b = 0.0)
{
	char text[96];

	_snprintf_s(text, sizeof(text), _TRUNCATE, format, a, b);
	return text;
}

void HierarchyTextLines(std::vector<HierarchyTextLine> *out)
{
	out->clear();

	if (!onHierarchy || !HierarchyView_IsShown())
	{
		return;
	}

	int32_t row = 3;

	auto put = [&](int32_t x, int32_t at, std::string const &text) { out->push_back({ x, at, text }); };

	if (!hierarchyText)
	{
		put(0, row, "(Text off)");
		return;
	}

	if (hierarchyPlayAll)
	{
		put(200, row, "Play commands affect all sequences");
	}

	if (hierarchyMode.vertexMode)
	{
		put(0, row++, "Vertex Selection Mode");
	}

	// the selected section (view +0x48): its name (FUN_0047fdd0), then FUN_0047fcb0's offset and quaternion. They are the section's own offset (+0x84..0x8c, set to the placement origin DAT_00637068/6c/70 = 0 each frame by
	// FUN_0047ef70 0x47efc7) and its matrix (+0x90, set to the identity DAT_005d45d0 at 0x47efd3) turned into a quaternion (FUN_004de070: the identity gives 0, 0, 0, 1)
	if (!hierarchyMode.section.empty())
	{
		put(0, row++, hierarchyMode.section);
		put(0, row++, "Offset : X:0  Y:0  Z:0");
		put(0, row++, "Rel Quat : X:0.0000  Y:0.0000  Z:0.0000  W:1.0000");
	}

	if (hierarchyMode.platformOn)
	{
		if (hierarchyMode.platform.lastAngle < 0)
		{
			put(0, row++, HierarchyFloat("Platform speed %.3f m/s", (double)hierarchyMode.platform.lastSpeed * 0.001));
		}
		else
		{
			put(0, row++, HierarchyFloat("Platform speed %.3f m/s (%.3f)", (double)hierarchyMode.platform.lastSpeed * 0.001, (double)hierarchyMode.platform.lastAngle * 0.001));
		}
	}

	put(0, row++, "Poly Count 0");

	if (hierarchyCameraRoot)
	{
		put(0, row++, "Use Camera Root");
	}

	put(0, row++, "Detail level " + std::to_string(hierarchyDetail));

	int distance = 0;

	if (HierarchyView_DetailDistance(hierarchyDetail, &distance))
	{
		put(0, row++, HierarchyFloat("Detail distance %.3fm", (double)distance * 0.001));
	}

	put(0, row++, HierarchyFloat("Current distance %.3fm", (double)HierarchyView_DistanceFromModel() * 0.001));

	OpenLevelFile *const file = HierarchyFile();

	// "Shape Set Collection: %s" (the selected collection, view +0x38) and a "Shape Set: %s" for each set applied (view +0x28)
	if (file && hierarchyMode.collectionSelected)
	{
		std::vector<model::ShapeSetCollection> collections;

		if (host::HierarchyEdit_Collections(file, &collections) == host::HierarchyEditResult::Done)
		{
			for (model::ShapeSetCollection const &one : collections)
			{
				if (one.number == hierarchyMode.collection)
				{
					put(0, row++, "Shape Set Collection: " + one.name);
					break;
				}
			}
		}
	}

	if (file && !hierarchyMode.applied.empty())
	{
		std::vector<model::AlternateSetInfo> alternates;

		if (host::HierarchyEdit_AlternateSets(file, &alternates) == host::HierarchyEditResult::Done)
		{
			for (int32_t const member : hierarchyMode.applied)
			{
				for (model::AlternateSetInfo const &one : alternates)
				{
					if (one.number == member)
					{
						put(0, row++, "Shape Set: " + one.name);
						break;
					}
				}
			}
		}
	}

	// what follows the main player, whether there is one or not (0x48a76e on): a blank row; the turret player's block (name, Timer, Seq. Time, Turret Angle, a blank row); each delta player's Timer and Seq. Time and a blank row
	std::vector<host::SequenceInfo> sequences;

	auto tail = [&]()
	{
		row++;

		int turretTimer = 0, turretMs = 0;

		if (HierarchyView_TurretFacts(&turretTimer, &turretMs))
		{
			int turretNumber = 0, turretSub = 0;
			std::string turretName;

			if (HierarchyView_TurretSequence(&turretNumber, &turretSub))
			{
				for (host::SequenceInfo const &one : sequences)
				{
					if (one.number == turretNumber && one.subNumber == turretSub)
					{
						turretName = one.name;
						break;
					}
				}
			}

			put(0, row++, turretName);
			put(0, row++, "Timer :" + std::to_string(turretTimer));
			put(0, row++, HierarchyFloat("Seq. Time :%.1f sec", (double)turretMs * 0.001));
			put(0, row++, "Turret Angle : " + std::to_string(HierarchyView_Elevation()));
			row++;
		}

		int deltaTimers[16], deltaMs[16];
		int const deltas = HierarchyView_DeltaFacts(deltaTimers, deltaMs, 16);

		for (int i = 0; i < deltas; i++)
		{
			put(0, row++, "Timer :" + std::to_string(deltaTimers[i]));
			put(0, row++, HierarchyFloat("Seq. Time :%.1f sec", (double)deltaMs[i] * 0.001));
			row++;
		}
	};

	// the main player: when a sequence has been chosen
	if (!file || !hierarchyMode.sequenceSelected || host::HierarchyEdit_Sequences(file, hierarchyMode.hierarchy, &sequences) != host::HierarchyEditResult::Done || hierarchyMode.sequence >= sequences.size())
	{
		tail();
		return;
	}

	host::SequenceInfo const &chosen = sequences[hierarchyMode.sequence];

	put(0, row++, chosen.name);

	uint32_t sequenceFlags = 0;
	bool flagsPresent = false;

	host::HierarchyEdit_ReadSequenceFlags(file, hierarchyMode.hierarchy, hierarchyMode.sequence, &sequenceFlags, &flagsPresent);

	if (sequenceFlags & 1)
	{
		put(0, row++, "Upper Sequence");
	}
	else if (sequenceFlags & 2)
	{
		put(0, row++, "Lower Sequence");
	}

	if (sequenceFlags & 0x10)
	{
		put(0, row++, "No Interpolation");
	}

	if (sequenceFlags & 0x20)
	{
		put(0, row++, "1/2 Frame Rate");
	}

	// FUN_00478bb0: for each of the 23 flag bits the frames that have it, then each frame's sound
	std::vector<host::FrameInfo> frames;
	int keyFrame = 0;

	if (host::HierarchyEdit_RootFrames(file, hierarchyMode.hierarchy, chosen.number, chosen.subNumber, &frames) == host::HierarchyEditResult::Done && !frames.empty())
	{
		for (int bit = 0; bit < 23; bit++)
		{
			std::string list;

			for (size_t i = 0; i < frames.size(); i++)
			{
				if (frames[i].flags & (1u << bit))
				{
					list += (list.empty() ? "" : ",") + std::to_string(i);
				}
			}

			if (!list.empty())
			{
				put(0, row++, "Flag " + HierarchyFlagName(bit) + " at " + list);
			}
		}

		for (size_t i = 0; i < frames.size(); i++)
		{
			uint32_t const sound = (frames[i].flags >> 24) & 0x7f;

			if (sound != 0)
			{
				put(0, row++, "Sound " + std::to_string(sound) + " at " + std::to_string(i));
			}
		}

		// FUN_004790a0: the frame whose time range holds the player's time (the last one when none does)
		int const timer = HierarchyView_Position();

		keyFrame = (int)frames.size() - 1;

		for (size_t i = 0; i < frames.size(); i++)
		{
			int const to = i + 1 < frames.size() ? frames[i + 1].at : 65536;

			if (frames[i].at <= timer && timer < to)
			{
				keyFrame = (int)i;
				break;
			}
		}
	}

	put(0, row++, "Key frame :" + std::to_string(keyFrame));
	put(0, row++, "Timer :" + std::to_string(HierarchyView_Position()));

	int32_t stored = 0;
	bool storedPresent = false;

	host::HierarchyEdit_ReadSequenceTime(file, hierarchyMode.hierarchy, hierarchyMode.sequence, &stored, &storedPresent);

	if (storedPresent && stored != 0)
	{
		put(0, row++, HierarchyFloat("Seq. Time :%.1f sec  (%.1f)", (double)HierarchyView_SequenceMilliseconds() * 0.001, (double)stored * 0.001));
	}
	else
	{
		put(0, row++, HierarchyFloat("Seq. Time :%.1f sec", (double)HierarchyView_SequenceMilliseconds() * 0.001));
	}

	if (hierarchyMode.saveTimesOn)
	{
		put(0, row++, "(Saving Seq. Times)");
	}

	tail();
}

// inteng's Hierarchy keys that run the mode's commands (its help text, read from the exe; hierarchy-data-model.md): H select hierarchy, S select sequence, + / - sequence time, 0-9 add keyframe flag
// (Shift: remove), Q apply frame sound (Shift: remove), D / F previous / next sequence, P / O start / stop, N / M previous / next key frame. Shift on S, + , -, P, O applies to the delta sequence, which is not built.
// The current sequence's number and sub-number, from the file's list (hierarchyMode.sequence is its place)
static bool HierarchyCurrentSequence(host::SequenceInfo *out, std::vector<host::SequenceInfo> *all)
{
	if (!onHierarchy || !hierarchyModelOpen || hierarchyMode.hierarchy.empty() || !hierarchyMode.sequenceSelected ||
		host::HierarchyEdit_Sequences(&hierarchyModelFile, hierarchyMode.hierarchy, all) != host::HierarchyEditResult::Done || hierarchyMode.sequence >= all->size())
	{
		return false;
	}

	*out = (*all)[hierarchyMode.sequence];
	return true;
}

// Ctrl+R (key word 'R' | Ctrl -> 0x39, FUN_0048cde0 0x48ce02; rows K-1, C-39): with a sequence open, FUN_00479590 -> FUN_0047f720 -> FUN_00480eb0 turns every frame of the ROOT section's current
// sequence half round the vertical (the frames' chunks, so the file, and the player, so the view), then marks the file changed (DAT_00637134). No words on the screen.
static void HierarchyTurnTheSequence()
{
	host::SequenceInfo current = { 0, 0, 0, false };
	std::vector<host::SequenceInfo> all;

	if (!HierarchyCurrentSequence(&current, &all))
	{
		return;
	}

	if (host::HierarchyEdit_TurnRootFrames(&hierarchyModelFile, hierarchyMode.hierarchy, current.number, current.subNumber) != host::HierarchyEditResult::Done)
	{
		Answer("Rotate sequence by 180 degrees: the sequence's frames could not be changed");
		return;
	}

	HierarchyView_TurnRootSequence(current.number, current.subNumber);
	Answer("Sequence rotated by 180 degrees");
	hierarchyAnswerUntil = GetTickCount() + 5000;
}

// Ctrl+F (key word 'F' | Ctrl -> 0x3e, FUN_0048cde0 0x48cdfd; rows K-2, C-3e; not in Help): with a sequence open, FUN_004795d0: for EVERY sequence of the hierarchy, in the list's order, the player
// takes it (FUN_004784e0) and poses its start (FUN_00478920), the LOWEST point of every section's posed shape is found (FUN_0047f900: the largest y, y pointing down, from -10000000), and every frame
// of the ROOT section's sequence has it taken off its y (0x479670-0x4796a5): each sequence stands on the floor at its start. The player is left on the LAST sequence of the list. The file is
// marked changed. Ours poses with the game's own pose (HierarchyPlayer, as Calculate Bounding Box does) in the game's unit, the file's for a model. NAMED: a sequence whose sections have no point
// is left as it is (inteng would take -10000000 / its scale off).
static void HierarchyPutSequencesOnTheFloor()
{
	host::SequenceInfo current = { 0, 0, 0, false };
	std::vector<host::SequenceInfo> all;

	if (!HierarchyCurrentSequence(&current, &all) || all.empty())
	{
		return;
	}

	host::HierarchyPlayer player;
	std::string why;

	if (!player.Load(hierarchyMode.modelName, hierarchyMode.hierarchy, nullptr, &why))
	{
		Answer("Put sequences on the floor: " + why);
		return;
	}

	int done = 0;

	for (host::SequenceInfo const &each : all)
	{
		int32_t low[3] = { 0, 0, 0 }, high[3] = { 0, 0, 0 };

		if (!player.Start(each.number, each.subNumber, &why) || !player.Box(std::vector<int>(1, 0), low, high))
		{
			continue;
		}

		if (host::HierarchyEdit_LowerRootFrames(&hierarchyModelFile, hierarchyMode.hierarchy, each.number, each.subNumber, high[1]) == host::HierarchyEditResult::Done)
		{
			HierarchyView_LowerRootSequence(each.number, each.subNumber, high[1]);
			done++;
		}
	}

	// the player is left on the last sequence of the list (FUN_004784e0 for each, in turn): at timer 0, not running, its deltas let go
	hierarchyMode.sequence = 0;
	HierarchyStepSequence(-1);
	Answer("Put " + std::to_string(done) + " sequence(s) on the floor");
	hierarchyAnswerUntil = GetTickCount() + 5000;
}

// Ctrl+R and Ctrl+F (the key translator's Ctrl words, FUN_0048cde0 0x48cdfd-0x48ce18). They are read WITH Ctrl held, so the window's key step calls this one where Ctrl is held. They
// were in HierarchyModeKey, which is reached only through the keys that want NO Ctrl (the window's "!ctrlHeld" step): there Ctrl+R never arrived from the real keyboard, and a scenario's
// posted Ctrl got through only while ctrlHeld read the real keyboard and this read the key table (until 2f4e9ff made both read the table). True when the key was one of them.
bool HierarchyCtrlKey(WPARAM key)
{
	if (!onHierarchy || ShiftDown() || !KeyHeldNow(VK_CONTROL) || (key != 'R' && key != 'F'))
	{
		return false;
	}

	if (key == 'R')
	{
		HierarchyTurnTheSequence();
	}
	else
	{
		HierarchyPutSequencesOnTheFloor();
	}

	return true;
}

bool HierarchyModeKey(WPARAM key)
{
	bool const shift = ShiftDown();

	// Dietrich's HierarchyKey (below) has the keys of the model's time, sequences, key frames, sounds, the turret and the delta variants; this one takes only the keys it lacks: T, W, C and Return
	// U / I (Alter detail level) and Shift+D (Set distance for current detail level, FUN_00479a80) are this one's too
	bool const detailKey = !shift && (key == 'U' || key == 'I');
	bool const distanceKey = shift && key == 'D';
	bool const vertexKey = key == 'V';   // V: Toggle vertex selection (0x63); Shift + V: Add selected vertices to selected bone (0x61)
	bool const deltaKey = shift && (key == 'S' || key == 'P' || key == 'O');   // the keys marked * apply to the delta sequence (inteng's help): commands 0x9 (select it), 0xb / 0xc (play / stop it)

	if (!detailKey && !distanceKey && !deltaKey && !vertexKey && (shift || (key != 'T' && key != 'W' && key != 'C' && key != 'B' && key != VK_RETURN)))
	{
		return false;
	}

	if (vertexKey)
	{
		if (!onHierarchy || !hierarchyModelOpen)
		{
			Answer("Vertex selection: open a model with Select Hierarchy first");
			return true;
		}

		HierarchyRunCommand(shift ? 0x61 : 0x63);
		return true;
	}

	if (!shift && key == 'B')
	{
		hierarchyBones = !hierarchyBones;
		Answer(hierarchyBones ? "Bones: on (the box round the sections' places is shown)" : "Bones: off");
		hierarchyAnswerUntil = GetTickCount() + 5000;
		return true;
	}

	if (deltaKey)
	{
		if (key == 'S')
		{
			HierarchyRunCommand(0x9);       // Add delta: which delta sequence plays over the main one
		}
		else
		{
			// Shift+P / Shift+O: cases 0xb / 0xc (FUN_0048cde0 0x48ce40 / 0x48ce48): the CURRENT delta (the one last added, the player's +8) gets +0x28 = 1 / 0: it plays / stops
			// (0x488b07-0x488b41; reassess/hierarchy-deltas-vasquez.md HD-19). No delta: nothing happens.
			if (HierarchyView_SetDeltaPlaying(key == 'P'))
			{
				Answer(key == 'P' ? "Delta sequence playing" : "Delta sequence stopped");
			}
		}

		hierarchyAnswerUntil = GetTickCount() + 5000;
		return true;
	}

	if (detailKey)
	{
		if (!HierarchyView_IsShown())
		{
			Answer("Detail level: open a model with Select Hierarchy first");
			return true;
		}

		int const levels = HierarchyView_DetailLevels();
		int const wanted = hierarchyDetail + (key == 'U' ? 1 : -1);

		// FUN_00477fb0 (0x477fe5-0x477ff2): the level is held to 0..9, whatever the model has; Adaptive Degradation is NOT touched (while it is on, the next frame's pick, FUN_00479b80, sets
		// the level by the distance again: the view shows that pick, editor_main.cpp's LevelView_SetAdaptiveDegradation)
		hierarchyDetail = wanted < 0 ? 0 : (wanted > 9 ? 9 : wanted);
		// NAMED: which shape a level beyond the model's own levels shows is FUN_0047e9f0's pick among the "L<k>#<name>" objects (0x47eb48-0x47ebe9), not read through yet
		hierarchyWords = "Detail level " + std::to_string(hierarchyDetail) + " (the model has " + std::to_string(levels) + ")";
		Answer(hierarchyWords);
		hierarchyAnswerUntil = GetTickCount() + 5000;
		return true;
	}

	if (distanceKey)
	{
		// Shift+D (FUN_00479a80): only with a hierarchy and a detail level above 0 (0x479a99: at level 0 nothing at all); the level's distance is the camera's distance from the WORLD'S
		// ORIGIN (FUN_004fd830 of the camera block's place +0x10: the length of the vector) / 100, inteng's unit to mm (the hierarchy stands at the origin); the levels below it can start no
		// farther, the ones above no nearer; the file is marked changed (DAT_00637134)
		int const levels = HierarchyView_DetailLevels();
		int32_t distances[10];
		bool present = false;

		if (!HierarchyView_IsShown() || !hierarchyModelOpen || hierarchyDetail <= 0 || !theCamera)
		{
			return true;
		}

		double const length = sqrt((double)theCamera->x * theCamera->x + (double)theCamera->y * theCamera->y + (double)theCamera->z * theCamera->z);
		int const distance = length < 2147483647.0 ? (int)length : 2147483647;

		if (host::HierarchyEdit_ReadDegradation(&hierarchyModelFile, distances, &present) != host::HierarchyEditResult::Done)
		{
			Answer("Set distance for current detail level: the model's distances could not be read");
			return true;
		}

		distances[hierarchyDetail] = distance;

		for (int i = 0; i < hierarchyDetail; i++)
		{
			distances[i] = distances[i] > distance ? distance : distances[i];
		}

		for (int i = hierarchyDetail + 1; i < 10; i++)
		{
			distances[i] = distances[i] < distance ? distance : distances[i];
		}

		host::HierarchyEditResult const wrote = host::HierarchyEdit_WriteDegradation(&hierarchyModelFile, distances);

		if (wrote != host::HierarchyEditResult::Done)
		{
			Answer(std::string("Set distance for current detail level: ") + host::HierarchyEditWords(wrote));
			return true;
		}

		for (int i = 0; i < levels && i < 10; i++)
		{
			HierarchyView_SetDetailDistance(i, distances[i]);
		}

		Answer("Detail level " + std::to_string(hierarchyDetail) + " now starts at " + std::to_string(distance) + " mm (kept in the model's HIDEGDIS; File > Save a copy writes it)");
		hierarchyAnswerUntil = GetTickCount() + 5000;
		return true;
	}

	if (KeyHeldNow(VK_CONTROL))
	{
		return false;
	}

	bool const plus = key == VK_OEM_PLUS || key == VK_ADD;
	bool const minus = key == VK_OEM_MINUS || key == VK_SUBTRACT;

	if (key == 'H')
	{
		HierarchyRunCommand(0x03);
	}
	else if (shift && (key == 'S' || plus || minus || key == 'P' || key == 'O'))
	{
		Answer("Shift + this key applies to the delta sequence: the delta sequence keys are not wired yet");
		hierarchyAnswerUntil = GetTickCount() + 5000;
	}
	else if (key == 'S')
	{
		HierarchyRunCommand(0x04);
	}
	else if (plus || minus)
	{
		HierarchyRunCommand(plus ? 0xd : 0xe);
		HierarchySyncView(true);
	}
	else if (key >= '0' && key <= '9')
	{
		HierarchyRunCommand((shift ? 0x26 : 0x15) + (int32_t)(key - '0'));
	}
	else if (key == 'Q')
	{
		HierarchyRunCommand(shift ? 0x51 : 0x50);
	}
	else if (key == 'D' || key == 'F')
	{
		HierarchyStepSequence(key == 'F' ? 1 : -1);
	}
	else if (HierarchyView_IsShown() && (key == 'P' || key == 'O'))
	{
		// P (case 5, FUN_0047b8b0) / O (case 6, FUN_0047b940): the main sequence; the turret and the deltas too only with Y's "Play commands affect all sequences" on (0x47b8c3, HP-4 / HP-5)
		HierarchyView_Play(key == 'P', true, hierarchyPlayAll);
		HierarchyStatus();
	}
	else if (HierarchyView_IsShown() && (key == 'N' || key == 'M'))
	{
		HierarchyView_StepFrame(key == 'M' ? 1 : -1, hierarchyPlayAll);
		HierarchyStatus();
	}
	else if (!shift && key == 'T')
	{
		// T: Toggle Text (inteng's help): the status words of the Hierarchy view
		hierarchyText = !hierarchyText;
		Answer(hierarchyText ? "Text: on" : "Text: off (the view's status words are hidden)");
		hierarchyAnswerUntil = GetTickCount() + 5000;
	}
	else if (!shift && key == 'W')
	{
		// W: Toggle wireframe: the model as lines or as polygons (the editor's draw modes Polyline and Polygon)
		hierarchyWire = !hierarchyWire;
		statusWords = ViewToggles_UseNow(hierarchyWire ? host::ViewToggle::Polyline : host::ViewToggle::Polygon);
		hierarchyAnswerUntil = GetTickCount() + 5000;
	}
	else if (!shift && key == 'R' && HierarchyView_IsShown())
	{
		// R: Restart all sequences (set timer to 0)
		HierarchyView_SetPosition(0);
		HierarchyStatus();
	}
	else if (!shift && key == 'C')
	{
		// C: Toggle use of camera root (for hud weapons), command 0x37 (FUN_004889d0 0x488da5): flips DAT_006370f0 (see HierarchyCameraRootFrame)
		hierarchyCameraRoot = !hierarchyCameraRoot;
		Answer(hierarchyCameraRoot ? "Use Camera Root: on (the camera sits at the hierarchy's Camera Root section and looks level: the HUD camera's view of the gun)" : "Use Camera Root: off");
		hierarchyAnswerUntil = GetTickCount() + 5000;
	}
	else if (!shift && key == 'Y')
	{
		// Y: the play commands affect the turret and delta sequences as well as the main one; press again to turn off
		hierarchyPlayAll = !hierarchyPlayAll;
		Answer(hierarchyPlayAll ? "Play commands affect the turret and delta sequences as well as the main sequence" : "Play commands affect the main sequence only");
		hierarchyAnswerUntil = GetTickCount() + 5000;
	}
	else if (!shift && key == VK_RETURN)
	{
		// Return (key code 6 -> 0x47, FUN_004889d0 0x488a37-0x488a80; row C-47): only when the hierarchy code marked the file changed (DAT_00637134; ours: the model differs from its last
		// save), the words "Saving" (0x5d5730) at (200, 100) (FUN_0057bd40(200, 100, 0, words)), then the save (FUN_00481b60 the sounds, FUN_004719e0 the file) and the collections' text
		// (FUN_0048cac0). Ours saves a COPY, never his file (SaveTheCopy: the model and its ".txt", as Exit does, HP-1 / HP-29). Not changed: nothing at all.
		if (hierarchyModelOpen && hierarchyModelFile.Generation() != hierarchyModelSavedGeneration && hierarchySaveAsked == 0)
		{
			hierarchySaveAsked = 2;
		}
	}
	else
	{
		return false;
	}

	return true;
}

void EnterHierarchy()
{
	if (onHierarchy)
	{
		return;
	}

	onHierarchy = true;
	host::HierarchyMode_Reset(&hierarchyMode);
	hierarchyModelOpen = false;

	// the model files the game has (avp_huds\*.rif): what Select Hierarchy offers when the open file holds no hierarchy
	{
		WIN32_FIND_DATAA found;
		HANDLE const search = FindFirstFileA("avp_huds\\*.rif", &found);

		if (search != INVALID_HANDLE_VALUE)
		{
			do
			{
				std::string name = found.cFileName;

				if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name.size() > 4 && hierarchyMode.models.size() < 200)
				{
					hierarchyMode.models.push_back(name.substr(0, name.size() - 4));
				}
			} while (FindNextFileA(search, &found));

			FindClose(search);
		}
	}
	promptList.clear();
	statusWords = "Hierarchy: Select Hierarchy first (it asks for a model of avp_huds, then a hierarchy and a sequence); the model is drawn in front of the camera and played: P / O play and stop, N / M key frames, - / + the sequence time";
	HierarchyOpenOnSelectedObject();   // a file of objects with no modules and an object picked: opens on its hierarchy (and says what it did)
}

void LeaveHierarchy()
{
	if (!onHierarchy)
	{
		return;
	}

	onHierarchy = false;
	hierarchyModelOpen = false;
	host::TrackPlay_ListenStop();   // the listen's sound goes with the screen (the reload audit's HELD 1: the game frees it with the level)
	HierarchyView_Close();
	hierarchyShownModel.clear();
	hierarchyShownName.clear();
	host::HierarchyMode_Reset(&hierarchyMode);
	HierarchyView_Close();

	if (promptFor == kPromptHierarchy)
	{
		promptFor = kPromptNone;
		promptText.clear();
	}

	promptList.clear();
}

// THE LEVEL GOES while this screen is up (CloseTheLevel, before the game's close; Bishop's LEAD A, 2026-11-24). The model shown is the game's level's memory, so it
// goes first, with everything the screen knew of it, as LeaveHierarchy lets it go. The screen itself stays, and the next Select Hierarchy starts afresh. The model
// files of avp_huds stay offered: EnterHierarchy listed them from the game's folder, not from the level.
void HierarchyScreen_LevelGoes()
{
	if (!onHierarchy)
	{
		return;
	}

	hierarchyModelOpen = false;
	HierarchyView_Close();
	hierarchyShownModel.clear();
	hierarchyShownName.clear();
	std::vector<std::string> models;
	models.swap(hierarchyMode.models);
	host::HierarchyMode_Reset(&hierarchyMode);
	hierarchyMode.models.swap(models);

	if (promptFor == kPromptHierarchy)
	{
		promptFor = kPromptNone;
		promptText.clear();
	}

	promptList.clear();
}

// A click in the Hierarchy view selects the section whose polygons are under it (inteng's FUN_00559720 + FUN_00479da0), drawn again in yellow; the section's name goes on the status line (ours: inteng
// says nothing). A click on no section lets the selected one go.
void HierarchyClicked(host::PickReport const &report)
{
	// Vertex selection (V), inteng's way (hierarchy-closeout-frost.md TB-2; FUN_004889d0 0x489dd4-0x489efa, FUN_00485290; vertex_pick.h; Vasquez 11-17): the click must be on the skin object's
	// polygons, else nothing; then the point nearest on the screen and every point within 1 of it on each axis (its duplicates) are ADDED to the selection, or with Shift TAKEN OFF it. (Until
	// 11-17 ours toggled the one nearest point wherever the click was.) NAMED: the skin is the attached object in the reference pose where the view draws its points, not moved with the
	// playing frame.
	if (hierarchyMode.vertexMode && !hierarchySkin.empty())
	{
		host::PickCamera camera;
		host::PickRay ray;

		if (!PickGame_Camera(hierarchyPickViewW, hierarchyPickViewH, &camera) || host::PickRay_FromClick(camera, pickX, pickY, &ray) != host::PickRayMade::Made)
		{
			Answer("Vertex selection: there is no frame to put the points on");
			return;
		}

		std::vector<int32_t> world(hierarchySkin.size());

		for (size_t i = 0; i < hierarchySkin.size(); i++)
		{
			world[i] = hierarchySkin[i] + hierarchyShownPlace[i % 3];
		}

		// 0x48530f-0x48531b: FUN_00559720 must give the vertex object
		if (!host::VertexPick_RayHitsFaces(ray, world, hierarchySkinFaces))
		{
			Answer("Vertex selection: the click is not on " + hierarchyMode.attachObject);
			hierarchyAnswerUntil = GetTickCount() + 5000;
			return;
		}

		// 0x4853c4: a point not beyond the view block's +0x3c is skipped: 0x140 of inteng's units for the marine's lens, 0xa0 for the alien's (100 to the millimetre)
		double const nearAhead = hierarchyMode.viewKind == 1 ? 1.6 : 3.2;
		std::vector<int32_t> const picked = host::VertexPick_Points(camera, pickX, pickY, world, nearAhead);
		size_t changed = 0;

		for (int32_t const point : picked)
		{
			const std::vector<int32_t>::iterator at = std::find(hierarchyMode.vertices.begin(), hierarchyMode.vertices.end(), point);

			if (pickShift && at != hierarchyMode.vertices.end())
			{
				hierarchyMode.vertices.erase(at);   // FUN_0046dce0: the mark cleared
				changed++;
			}
			else if (!pickShift && at == hierarchyMode.vertices.end())
			{
				hierarchyMode.vertices.push_back(point);   // FUN_0046dc90: the mark set
				changed++;
			}
		}

		Answer(std::string("Vertex ") + (picked.empty() ? std::string("none") : std::to_string(picked[0])) + ": " + std::to_string(picked.size()) + " points " + (pickShift ? "taken off" : "added")
			+ " (" + std::to_string(changed) + " changed, " + std::to_string(hierarchyMode.vertices.size()) + " selected)");
		hierarchyAnswerUntil = GetTickCount() + 5000;
		return;
	}

	// inteng (0x489f5d-0x489f74, HP-16; hierarchy-closeout-frost.md TB-1): FUN_00559720(x, y), the object whose POLYGONS are under the pointer, nearest along the ray, then FUN_00479da0: the
	// section before is let go (+0x48 = 0), and the section that object is (FUN_0047f880) becomes the selected one; an object that is no section, or nothing at all, leaves NONE selected. Ours: the
	// game's line of sight over what was drawn met that section's posed polygons (PickReport::hierarchySection). (Until 2026-11-16 ours took the section whose bounding sphere the ray crossed,
	// and a click on nothing kept the selection.)
	int index = -1;
	std::vector<HierarchyViewSection> sections;

	if (!HierarchyView_SectionOfHit(report.hierarchySection, &index) || !HierarchyView_Sections(&sections) || index < 0 || (size_t)index >= sections.size())
	{
		HierarchyView_SelectSection(-1);
		hierarchyMode.section.clear();
		Answer("Hierarchy: no section under the click");
		return;
	}

	HierarchyView_SelectSection(index);
	hierarchyMode.section = sections[(size_t)index].name;   // Change object for section and the vertex selection work on the selected section (inteng's view +0x48)
	Answer("Hierarchy: section " + sections[(size_t)index].name + " selected");
}

// What the mode says next. HIERARCHYSHOW IS THE ONE PLACE a Hierarchy step becomes what inteng shows (Bishop 2026-11-16: each owner traces the dialog kind of his asks and returns it):
//   step.messageBox            inteng's MESSAGE BOX (FUN_00474030 + FUN_0057c800(100, 100)): the words and Ok; with soundForm the Sound Edit window opens after the Ok
//   step.soundForm             the Sound Edit window of hierarchyMode.soundBeingEdited (FUN_004ebd60 -> edobtype's edit_sound): HierarchySoundFormOpen, answered by HierarchyMode_SoundForm
//   step.prompt + textDialog   inteng's TEXT DIALOG (FUN_00474170: the words, one edit holding defaultText, Ok / Cancel): Ok gives the edit's words to HierarchyMode_Typed (a number: read it with
//                              HierarchyMode_EditNumber = FUN_0057d620, as FUN_00474340 does), Cancel calls HierarchyMode_Cancel
//   step.prompt + the Help ask the two Help pages as inteng's full text screens (FUN_004724f0, "Click to continue.")
//   step.prompt + a list       inteng's LIST DIALOG (FUN_00475bf0, the rows sorted): the pick is answered as its place from 1
//   step.prompt, nothing else  OUR TYPED LINE: a substitute, still to be traced to one of the kinds above
//   anything else              the answer is kept for the state line only (the Hierarchy screen draws no status line)
// (The mode's helpers in hierarchy_mode.cpp: AskInDialog, Box, Line + Ask.)
static void HierarchySoundFormOpen();

// The sound chooser's right click (0x481720: FUN_00501080, then FUN_004ec0a0(the sound, the camera block's place +0x10)): the sound with that index plays once at the camera's place, with its
// ranges (the file's x the level's scale, as the game takes a track's), volume and pitch; the sound folder is the level's (SOUNDDIR, as Track Editing's sounds). Its words are the state line's.
static void HierarchyListen(int32_t index)
{
	std::vector<host::SoundInfo> sounds;

	if (!hierarchyModelOpen || !theCamera || host::HierarchyEdit_Sounds(&hierarchyModelFile, &sounds) != host::HierarchyEditResult::Done)
	{
		return;
	}

	for (host::SoundInfo const &each : sounds)
	{
		if (each.index != index)
		{
			continue;
		}

		double const scale = hierarchyMode.levelScale > 0.0 && hierarchyMode.levelScale <= 1000000.0 ? hierarchyMode.levelScale : 1.0;
		int32_t const place[3] = { theCamera->x, theCamera->y, theCamera->z };

		host::TrackPlay_UseSoundDirectory(host::TrackLevelSoundDirectory(LevelObjects_OpenForEdit()));
		bool const playing = host::TrackPlay_Listen(each.wav, (int32_t)(each.inner * scale), (int32_t)(each.outer * scale), each.volume, each.pitch, place);

		Answer(playing ? "Listening to " + each.wav : "Listen: " + each.wav + " could not be played (no sound device, or no such wav in the sound folder)");
		hierarchyAnswerUntil = GetTickCount() + 5000;
		return;
	}
}
static void HierarchyFemaleFormOpen(bool checked);

void HierarchyShow(host::HierarchyStep const &step)
{
	promptList = hierarchyMode.list;

	// inteng's message box (FUN_00474030 + FUN_0057c800(100, 100)): the refusals of Select Sequence and Add delta (reassess/hierarchy-deltas-vasquez.md HD-10, HD-16), the sound items' (C-SND,
	// C-4d); a Sound Edit window that is shown again after it (a refused answer) opens when Ok closes the box
	if (step.messageBox)
	{
		promptList.clear();
		Answer(step.answer);
		hierarchyAnswerUntil = GetTickCount() + 5000;

		// a box and then a question ("That name is already used", Ok, the same text dialog again: FUN_0048af00 / FUN_0047a880's loops)
		if (step.prompt)
		{
			host::HierarchyStep question = step;

			question.messageBox = false;
			question.answer.clear();
			memset(keyDown, 0, sizeof(keyDown));
			DialogMessage(step.answer, [question]() { HierarchyShow(question); });
			return;
		}

		if (step.soundForm)
		{
			DialogMessage(step.answer, []() { HierarchySoundFormOpen(); });
		}
		else
		{
			DialogMessage(step.answer);
		}

		return;
	}

	// Set Properties' window (FUN_0048d6a0 / FUN_0048d820)
	if (step.prompt && step.femaleForm)
	{
		promptList.clear();
		memset(keyDown, 0, sizeof(keyDown));
		HierarchyFemaleFormOpen(step.femaleChecked);
		return;
	}

	// the sound's Sound Edit window (FUN_004ebd60: edobtype's edit_sound)
	if (step.soundForm)
	{
		promptList.clear();

		if (!step.answer.empty())
		{
			Answer(step.answer);
			hierarchyAnswerUntil = GetTickCount() + 5000;
		}

		HierarchySoundFormOpen();
		return;
	}

	// inteng's text dialog (FUN_00474170): the words, one edit holding the number offered, Ok / Cancel (FUN_0057c800(100, 100)); Ok gives the edit's words to the mode, Cancel closes the question
	if (step.prompt && step.textDialog)
	{
		host::DialogField field;

		promptList.clear();
		hierarchyTitle = step.title;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptNone;
		promptText.clear();
		field.text = step.defaultText;
		DialogText(step.title, { field }, [](int32_t answer, std::vector<std::string> const &texts)
		{
			if (answer == host::kDialogOk && !texts.empty())
			{
				HierarchyDialogAnswer(texts[0]);
			}
			else
			{
				HierarchyDialogCancel();
			}
		});
		return;
	}

	// Help (0x38, FUN_0048ab60): FUN_004724f0 twice, the model's keys and then the boned object and turret commands. FUN_004724f0 is NOT a toolkit message box (it never calls FUN_0057c800): it draws the text over the view
	// (FUN_0057be40) with "Click to continue." (005d3ed8) and runs its own message loop until a click: the text panel Module Linking's, Strategy's and Light's Help use (textPanelText / textPanelNext)
	if (step.prompt && hierarchyMode.ask == host::HierarchyAsk::Help)
	{
		auto words = [](std::vector<std::string> const &lines)
		{
			std::string text;

			for (std::string const &line : lines)
			{
				text += (text.empty() ? "" : "\n") + line;
			}

			return text;
		};

		std::string const first = words(hierarchyMode.list);

		host::HierarchyMode_Typed(HierarchyFile(), &hierarchyMode, std::string());   // the next page

		std::string const second = hierarchyMode.ask == host::HierarchyAsk::Help ? words(hierarchyMode.list) : std::string();

		if (hierarchyMode.ask == host::HierarchyAsk::Help)
		{
			host::HierarchyMode_Typed(HierarchyFile(), &hierarchyMode, std::string());   // closes the question
		}

		promptList.clear();
		hierarchyMode.list.clear();

		static std::string helpPages[2];   // the panel keeps pointers to these until it is closed

		helpPages[0] = first;
		helpPages[1] = second;
		textPanelText = helpPages[0].c_str();
		textPanelNext = second.empty() ? nullptr : helpPages[1].c_str();
		return;
	}

	if (step.prompt && !hierarchyMode.list.empty())
	{
		// choices: inteng's list dialog (DialogList, FUN_00475bf0), no typed prompt; the rows come as "N  name" lines, the dialog shows the names, the answer is the row's place in the list from 1
		hierarchyTitle = step.title;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptNone;
		promptText.clear();

		std::vector<std::string> names;

		for (std::string const &line : promptList)
		{
			size_t digits = 0;

			while (digits < line.size() && line[digits] >= '0' && line[digits] <= '9')
			{
				digits++;
			}

			names.push_back(digits > 0 && line.compare(digits, 2, "  ") == 0 ? line.substr(digits + 2) : line);
		}

		// the sound chooser (FUN_00481740) gives its list a right-click handler (FUN_00476f70 with 0x481720): a right click on a row plays that row's sound, the list stays open
		std::function<void(int32_t)> listen;

		if (hierarchyMode.ask == host::HierarchyAsk::SoundEditPick || hierarchyMode.ask == host::HierarchyAsk::SoundDeletePick || hierarchyMode.ask == host::HierarchyAsk::SoundApplyPick)
		{
			std::vector<int32_t> const indices = hierarchyMode.listValues;   // each row's sound index

			listen = [indices](int32_t place)
			{
				if (place >= 0 && (size_t)place < indices.size())
				{
					HierarchyListen(indices[(size_t)place]);
				}
			};
		}

		DialogList(step.title, names, [](int32_t choice)
		{
			if (choice < 0)
			{
				HierarchyDialogCancel();
			}
			else
			{
				HierarchyDialogAnswer(std::to_string(choice + 1));
			}
		}, listen);
		return;
	}

	// inteng's Yes / No box (FUN_00474a40)
	if (step.prompt && step.confirm)
	{
		hierarchyTitle = step.title;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptNone;
		promptText.clear();
		DialogConfirm(step.title, [](bool yes) { HierarchyDialogAnswer(yes ? "yes" : "no"); });
		return;
	}

	if (step.prompt)
	{
		hierarchyTitle = step.title;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptHierarchy;
		promptText = step.defaultText;
		hierarchyPromptFresh = !step.defaultText.empty();
		return;
	}

	promptList.clear();
	hierarchyPromptFresh = false;

	if (!step.answer.empty())
	{
		Answer(step.answer);
		hierarchyAnswerUntil = GetTickCount() + 5000;
	}

	HierarchySyncView();
}

// The file Hierarchy mode works on: the opened model, or else the open level
OpenLevelFile *HierarchyFile()
{
	return hierarchyModelOpen ? &hierarchyModelFile : LevelObjects_OpenForEdit();
}

// The model the typed Open model answer named: opened from avp_huds beside the data folder, then Select Hierarchy runs again over it
static void HierarchyOpenRequestedModel()
{
	std::string const name = hierarchyMode.modelRequested;

	hierarchyMode.modelRequested.clear();

	if (name.empty())
	{
		return;
	}

	std::string const path = "avp_huds\\" + name + ".rif";
	LevelOpenResult const opened = hierarchyModelFile.Open(path.c_str());

	if (opened != LevelOpenResult::Opened)
	{
		Answer("Open model " + name + ": the game's own loader did not take the file");
		return;
	}

	hierarchyModelOpen = true;
	hierarchyModelSavedGeneration = hierarchyModelFile.Generation();
	hierarchyMode.modelName = name;
	hierarchyMode.hierarchy.clear();
	hierarchyMode.sequenceSelected = false;
	hierarchyMode.collectionSelected = false;
	hierarchyMode.applied.clear();
	HierarchyShow(host::HierarchyMode_Command(&hierarchyModelFile, &hierarchyMode, 0x03));
}

// The view follows the mode: once a model is open with a hierarchy and a sequence selected, the hierarchy is shown in front of the camera playing that sequence; a new sequence switches it;
// restart shows it again with the sequence's time in the mode (after the time keys), keeping the position and whether it plays.
void HierarchySyncView(bool restart)
{
	// Calculate Bounding Box: every sequence is played through its keyframes by the game's own pose (hierarchy_player.h), the box of the shapes' points is written into the sequence's HIERBBOX, and
	// the box over all sequences into the hierarchy's (inteng's FUN_004796f0; the poses are the keyframe starts: inteng adds one time step, the frame clock, to each)
	if (hierarchyMode.boundsRequested)
	{
		hierarchyMode.boundsRequested = false;

		host::HierarchyPlayer player;
		std::string why;
		std::vector<host::SequenceInfo> sequences;
		bool const loaded = hierarchyModelOpen && player.Load(hierarchyMode.modelName, hierarchyMode.hierarchy, nullptr, &why) &&
			host::HierarchyEdit_Sequences(&hierarchyModelFile, hierarchyMode.hierarchy, &sequences) == host::HierarchyEditResult::Done;
		int32_t overall[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
		bool anyAt = false;
		int done = 0;

		for (size_t i = 0; loaded && i < sequences.size(); i++)
		{
			std::vector<host::FrameInfo> frames;
			std::vector<int> times;
			int32_t low[3] = { 0, 0, 0 }, high[3] = { 0, 0, 0 };

			if (host::HierarchyEdit_RootFrames(&hierarchyModelFile, hierarchyMode.hierarchy, sequences[i].number, sequences[i].subNumber, &frames) != host::HierarchyEditResult::Done)
			{
				continue;
			}

			for (host::FrameInfo const &each : frames)
			{
				times.push_back(each.at);
			}

			if (!player.Start(sequences[i].number, sequences[i].subNumber, &why) || !player.Box(times, low, high) ||
				host::HierarchyEdit_WriteBoundingBox(&hierarchyModelFile, hierarchyMode.hierarchy, (int)i, low, high) != host::HierarchyEditResult::Done)
			{
				continue;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				overall[0][axis] = anyAt && overall[0][axis] < low[axis] ? overall[0][axis] : low[axis];
				overall[1][axis] = anyAt && overall[1][axis] > high[axis] ? overall[1][axis] : high[axis];
			}

			anyAt = true;
			done++;
		}

		if (anyAt && host::HierarchyEdit_WriteBoundingBox(&hierarchyModelFile, hierarchyMode.hierarchy, -1, overall[0], overall[1]) == host::HierarchyEditResult::Done)
		{
			Answer("Calculated bounding box (" + std::to_string(done) + " sequences; x " + std::to_string(overall[0][0]) + " to " + std::to_string(overall[1][0]) + ", y " + std::to_string(overall[0][1]) + " to " +
				std::to_string(overall[1][1]) + ", z " + std::to_string(overall[0][2]) + " to " + std::to_string(overall[1][2]) + ")");
		}
		else
		{
			Answer("Calculate Bounding Box: no box could be worked out" + (why.empty() ? std::string() : ": " + why));
		}

		hierarchyAnswerUntil = GetTickCount() + 6000;
	}

	// Add delta / Remove delta: the mode asks, the view does (the game's own delta controllers)
	if (hierarchyMode.removeDeltasRequested)
	{
		hierarchyMode.removeDeltasRequested = false;
		HierarchyView_RemoveDeltas();
	}

	if (hierarchyMode.addDeltaRequested)
	{
		hierarchyMode.addDeltaRequested = false;

		if (!HierarchyView_AddDelta(hierarchyMode.addDeltaNumber, hierarchyMode.addDeltaSub))
		{
			Answer("Add delta: the view shows no model, or the hierarchy has no such sequence");
		}
		else if (hierarchyMode.deltaSelected)
		{
			HierarchyView_SetDeltaMilliseconds(hierarchyMode.deltaMs);
		}
	}

	if (hierarchyMode.deltaTimeRequested)
	{
		hierarchyMode.deltaTimeRequested = false;
		HierarchyView_SetDeltaMilliseconds(hierarchyMode.deltaMs);
	}

	if (!onHierarchy || !hierarchyModelOpen || hierarchyMode.hierarchy.empty() || !hierarchyMode.sequenceSelected || hierarchyMode.modelName.empty())
	{
		return;
	}

	std::vector<host::SequenceInfo> sequences;

	if (host::HierarchyEdit_Sequences(&hierarchyModelFile, hierarchyMode.hierarchy, &sequences) != host::HierarchyEditResult::Done || hierarchyMode.sequence >= sequences.size())
	{
		return;
	}

	host::SequenceInfo const &wanted = sequences[hierarchyMode.sequence];
	bool const same = HierarchyView_IsShown() && hierarchyShownModel == hierarchyMode.modelName && hierarchyShownName == hierarchyMode.hierarchy;
	int sequence = 0, sub = 0;

	// A sequence player's time is ITS OWN: 1000 ms unless the sequence has an OBASEQTM above 0 (FUN_0047d6f0 0x47d80c +0x18 = 1000, +0x14 = 0x10000; 0x47d982-0x47d99c the chunk's time),
	// never the game's default length of the sequence (reassess/hierarchy-play-frost.md HP-32): hierarchyMode.sequenceMs holds it
	if (same && !restart)
	{
		if (!(HierarchyView_CurrentSequence(&sequence, &sub) && sequence == wanted.number && sub == wanted.subNumber))
		{
			HierarchyView_SetSequence(wanted.number, wanted.subNumber);
			HierarchyView_ChangeTime(hierarchyMode.sequenceMs - HierarchyView_SequenceMilliseconds());
		}

		return;
	}

	int32_t const position = same ? HierarchyView_Position() : 0;
	bool const playing = same && HierarchyView_IsPlaying();
	std::string why;
	int32_t place[3] = { 0, 0, 0 };

	if (!HierarchyView_LoadModel(hierarchyMode.modelName.c_str(), &why) ||
		!ShowTheModelInFront(hierarchyMode.modelName.c_str(), hierarchyMode.hierarchy.c_str(), wanted.number, wanted.subNumber, place, &why,
			(int32_t)(((int64_t)hierarchyMode.sequenceMs * 65536) / 1000)))
	{
		Answer("Hierarchy view: " + why);
		return;
	}

	hierarchyShownModel = hierarchyMode.modelName;
	hierarchyShownName = hierarchyMode.hierarchy;

	if (same)
	{
		HierarchyView_SetPosition(position);
		HierarchyView_Play(playing, true);
	}
	else
	{
		HierarchyView_Play(false, true);   // a chosen sequence stands at position 0 until P (FUN_0047d6f0 makes its player not running)
	}
}

// D / F: the previous / next sequence of the hierarchy becomes the current one (inteng's keys; the sequence's time is read as Select Sequence reads it)
// D / F (the key table FUN_0048cde0: D -> 0x40, F -> 0x3f; FUN_004889d0 cases 0x40 0x488e4d / 0x3f 0x488e26, reassess/hierarchy-play-frost.md HP-11 / HP-12 / HP-30): only with a player and a main
// sequence (else NOTHING, no message); FUN_004784e0(main index -/+ 1): the main player is replaced by a new one for that index, wrapped by the hierarchy's sequence count (FUN_0047d6f0 0x47d743-0x47d74c,
// the file's order), at timer 0 and NOT running (0x47d810 / 0x47d816), with its own time (OBASEQTM, else 1000 ms); the delta players are freed (0x478526-0x4785dc). Its name shows in the text rows.
void HierarchyStepSequence(int direction)
{
	if (hierarchyMode.hierarchy.empty() || !hierarchyModelOpen || !hierarchyMode.sequenceSelected)
	{
		return;
	}

	std::vector<host::SequenceInfo> sequences;

	if (host::HierarchyEdit_Sequences(&hierarchyModelFile, hierarchyMode.hierarchy, &sequences) != host::HierarchyEditResult::Done || sequences.empty())
	{
		return;
	}

	size_t const count = sequences.size();

	hierarchyMode.sequence = (hierarchyMode.sequence + (direction > 0 ? 1 : count - 1)) % count;

	int32_t milliseconds = 0;
	bool has = false;

	hierarchyMode.sequenceMs = 1000;

	if (host::HierarchyEdit_ReadSequenceTime(&hierarchyModelFile, hierarchyMode.hierarchy, hierarchyMode.sequence, &milliseconds, &has) == host::HierarchyEditResult::Done && has && milliseconds > 0)
	{
		hierarchyMode.sequenceMs = milliseconds;
	}

	HierarchyView_RemoveDeltas();
	hierarchyMode.deltaSelected = false;
	HierarchySyncView();   // the new sequence (the turret player stays: FUN_004784e0 keeps [1])
	HierarchyView_ChangeTime(hierarchyMode.sequenceMs - HierarchyView_SequenceMilliseconds());   // at its own time
	HierarchyView_SetPosition(0);   // timer 0, not running
}

// inteng's Hierarchy mode works on the OPENED FILE (FUN_0048b8a0 lists its OBJCHIER objects): when the file the editor opened holds hierarchies (a HUD or weapon file, a model file) it becomes the mode's file, drawn
// from avp_rifs\<level>.rif by the game's own loader; a level with none leaves the mode as it was, and Select Hierarchy then does nothing, as in inteng
static void HierarchyOpenTheLevelAsTheFile()
{
	if (hierarchyModelOpen || openLevelName.empty())
	{
		return;
	}

	std::string const path = "avp_rifs\\" + openLevelName + ".rif";
	std::vector<std::string> names;

	if (hierarchyModelFile.Open(path.c_str()) != LevelOpenResult::Opened || host::HierarchyEdit_Names(&hierarchyModelFile, &names) != host::HierarchyEditResult::Done || names.empty())
	{
		return;
	}

	hierarchyModelOpen = true;
	hierarchyModelSavedGeneration = hierarchyModelFile.Generation();
	hierarchyMode.modelName = "avp_rifs/" + openLevelName;
	hierarchyMode.hierarchy.clear();
	hierarchyMode.sequenceSelected = false;
}

void HierarchyRunCommand(int32_t command)
{
	HierarchyOpenTheLevelAsTheFile();

	OpenLevelFile *const open = HierarchyFile();

	if (!open || !open->Level())
	{
		Answer("No level is held to work on");
		return;
	}

	hierarchyMode.position = HierarchyView_IsShown() ? HierarchyView_Position() : 0;
	HierarchyShow(host::HierarchyMode_Command(open, &hierarchyMode, command));
	HierarchySyncView();

	if (hierarchyMode.vertexMode && hierarchySkin.empty())
	{
		if (host::HierarchyEdit_SkinPoints(open, hierarchyMode.attachObject, &hierarchySkin) != host::HierarchyEditResult::Done
			|| host::HierarchyEdit_SkinFaces(open, hierarchyMode.attachObject, &hierarchySkinFaces) != host::HierarchyEditResult::Done)
		{
			hierarchySkin.clear();
			hierarchySkinFaces.clear();
			hierarchyMode.vertexMode = false;
			Answer("Vertex selection: the attached object's points could not be read");
		}
	}
	else if (!hierarchyMode.vertexMode)
	{
		hierarchySkin.clear();
		hierarchySkinFaces.clear();
	}
	else if (command == 0x61)
	{
		hierarchySkin.clear();   // the points are read again next frame (the attachment changed, the shape did not: this is only to be safe)
		host::HierarchyEdit_SkinPoints(open, hierarchyMode.attachObject, &hierarchySkin);
		host::HierarchyEdit_SkinFaces(open, hierarchyMode.attachObject, &hierarchySkinFaces);
	}
}

// THE SOUND EDIT WINDOW of a hierarchy's sound (New sound and Edit sound: FUN_004ebd60 -> edobtype's edit_sound, resource 132 "Sound Edit"): the DLL's own window, the one the placed
// sounds, the tracks and the placed hierarchies open (special_windows.cpp SoundEditWindowOpen: the wav name an EDITABLE combo of the sound folder's wavs, the four numbers, the boxes,
// Play (1065), OK and Cancel, with the DLL's limits and words). In (0x4ebe12-0x4ebe91): the name, the ranges in inteng's unit (HierarchyMode_SoundRanges), pitch, volume, the flags word
// from bit 0, the MASK 0: both boxes off for a hierarchy's sound (FUN_004ebc80's third number is 0, 0x4ebe3b). The caption's folder is DAT_008f4dac, made by FUN_004f68b0: the base
// FUN_0057a850 makes from inteng's own path with "Sound\" after it (DAT_00915060, 0x57a909-0x57a930), then the level's SOUNDDIR and "\" (0x4f6913-0x4f69a8; DAT_00606824 before it
// is empty): ours, the game folder's "Sound\<SOUNDDIR>\". Back after OK (0x4ebf13-0x4ebf5d, SoundEdit_IndexedBack) into the sound (HierarchyMode_SoundWrite); Cancel writes nothing.
// (Row C-4e of hierarchy-view-sound-dietrich.md, Dietrich 2026-11-18: until then a form of our own with the name as a plain edit and no Play.)
static void HierarchySoundFormOpen()
{
	uint32_t inner = 0, outer = 0;

	host::HierarchyMode_SoundRanges(hierarchyMode, &inner, &outer);

	host::SoundEditRecord const record = host::SoundEdit_IndexedRecord(hierarchyMode.soundBeingEdited.wav, inner, outer, hierarchyMode.soundBeingEdited.pitch,
		hierarchyMode.soundBeingEdited.volume, (int32_t)hierarchyMode.soundBeingEdited.flags, false);
	std::string const soundDirectory = host::TrackLevelSoundDirectory(LevelObjects_OpenForEdit());
	std::string const folder = std::string(theGameFolder) + "\\Sound\\" + soundDirectory + "\\";

	SoundEditWindowOpen(record, folder, soundDirectory, [](host::SoundEditRecord const &back, bool ok)
	{
		OpenLevelFile *const open = HierarchyFile();

		if (!ok || !open || !open->Level())
		{
			return;   // Cancel: the sound keeps what it had (a new sound its start values); FUN_004816f0 still marks the file changed
		}

		int32_t backInner = 0, backOuter = 0, pitch = 0, volume = 0, flags = 0;

		host::SoundEdit_IndexedBack(back, (int32_t)hierarchyMode.soundBeingEdited.flags, &backInner, &backOuter, &pitch, &volume, &flags);
		HierarchyShow(host::HierarchyMode_SoundWrite(open, &hierarchyMode, back.name, backInner, backOuter, pitch, volume));
	});
}

// SET PROPERTIES' WINDOW (case 0x58, FUN_0048d6a0 / FUN_0048d820; reassess/hierarchy-edit-frost.md HF-0x58): FUN_0057d170(600, 0x1a4, 5) with its title "Shape set properties" (0x5d5d2c), "Ok"
// (0x5cf68c) at (200, 0x172) and "Cancel" (0x5cfab4) at (0x15e, 0x172), each 0x50 x 0x12, the label "Female" (0x5d5d24) at (0x12, 0x28) 100 x 0x12 (FUN_0057d400) and the check box FUN_0057d670 at
// (0x76, 0x28) holding the applied set's flags bit 0; Ok gives the box's state to the mode, Cancel nothing
static void HierarchyFemaleFormOpen(bool checked)
{
	host::DialogFormSpec spec;

	spec.title = "Shape set properties";
	spec.width = 600;
	spec.height = 0x1a4;

	auto const add = [&spec](host::DialogWidgetType kind, std::string const &words, int32_t x, int32_t y, int32_t w, int32_t h, int32_t answer)
	{
		host::DialogWidget one;

		one.type = kind;
		one.text = words;
		one.box = { x, y, w, h };
		one.answer = answer;
		one.most = 10;
		one.enabled = true;
		spec.widgets.push_back(one);
	};

	add(host::DialogWidgetType::Button, "Ok", 200, 0x172, 0x50, 0x12, host::kDialogOk);
	add(host::DialogWidgetType::Button, "Cancel", 0x15e, 0x172, 0x50, 0x12, host::kDialogEscape);
	add(host::DialogWidgetType::Label, "Female", 0x12, 0x28, 100, 0x12, 0);
	add(host::DialogWidgetType::Check, std::string(), 0x76, 0x28, 12, 12, 0);
	spec.widgets.back().checked = checked;

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			HierarchyDialogCancel();
			return;
		}

		bool on = false;

		for (host::DialogWidget const &one : widgets)
		{
			on = one.type == host::DialogWidgetType::Check ? one.checked : on;
		}

		HierarchyDialogAnswer(on ? "yes" : "no");
	});
}

void HierarchyDialogAnswer(std::string const &typed)
{
	promptList.clear();
	HierarchyTypedAnswer(typed);
}

void HierarchyDialogCancel()
{
	promptList.clear();
	host::HierarchyMode_Cancel(&hierarchyMode);
}

void HierarchyTypedAnswer(std::string const &typed)
{
	OpenLevelFile *const open = HierarchyFile();

	if (!open || !open->Level())
	{
		Answer("No level is held to work on");
		return;
	}

	unsigned const generation = open->Generation();

	hierarchyMode.position = HierarchyView_IsShown() ? HierarchyView_Position() : 0;
	HierarchyShow(host::HierarchyMode_Typed(open, &hierarchyMode, typed));
	HierarchySyncView();

	if (hierarchyMode.turretChosen)
	{
		hierarchyMode.turretChosen = false;

		std::vector<host::SequenceInfo> sequences;

		if (host::HierarchyEdit_Sequences(&hierarchyModelFile, hierarchyMode.hierarchy, &sequences) == host::HierarchyEditResult::Done && hierarchyMode.turretSequence < sequences.size() &&
			HierarchyView_SetTurret(sequences[hierarchyMode.turretSequence].number, sequences[hierarchyMode.turretSequence].subNumber))
		{
			Answer("Hierarchy: turret sequence " + std::to_string(sequences[hierarchyMode.turretSequence].number) + "." + std::to_string(sequences[hierarchyMode.turretSequence].subNumber) + " plays on the sections under the turret section");
		}
	}

	if (!hierarchyMode.modelRequested.empty())
	{
		HierarchyOpenRequestedModel();
	}

	// the level was rebuilt: the object list and the picture are made again from it
	if (open->Generation() != generation && !hierarchyModelOpen)
	{
		LoadTheObjectListAgain();
	}
}

// A file of objects with no modules, an object picked, and the Hierarchy screen entered: the Hierarchy mode opens on THAT object's hierarchy, ready to play its sequences (Bishop's order for Melanikus's HUD and weapon files:
// "click an object, go to Hierarchy, and it opens on that object's hierarchy"). The hierarchy is the first of the file whose sections' objects include the picked one (HierarchyEdit_SectionObjects), the model is the file itself in
// avp_huds (the way the mode opens a model), and the first sequence is selected, so the view shows the hierarchy playing it. Nothing happens when no object is picked, the file has no model objects, or avp_huds has no copy of the file.
// The hierarchy of the open file that has the object with this 8-byte ID as one of its sections (HierarchyEdit_SectionObjects: the objects the sections' shapes are); empty when none does. *objectName gets the object's name.
std::string HierarchyOfObject(uint8_t const *id, std::string *objectName)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<host::LevelObject> rows;
	std::string name;

	LevelObjects_Read(&rows);

	for (host::LevelObject const &row : rows)
	{
		if (memcmp(row.id, id, host::kSelectionIdBytes) == 0 && row.name)
		{
			name = row.name;
			break;
		}
	}

	if (objectName)
	{
		*objectName = name;
	}

	std::vector<std::string> names;

	if (!open || !open->Level() || name.empty() || host::HierarchyEdit_Names(open, &names) != host::HierarchyEditResult::Done)
	{
		return std::string();
	}

	for (std::string const &hierarchy : names)
	{
		std::vector<std::string> objects;

		if (host::HierarchyEdit_SectionObjects(open, hierarchy, &objects) == host::HierarchyEditResult::Done)
		{
			for (std::string const &object : objects)
			{
				if (object == name)
				{
					return hierarchy;
				}
			}
		}
	}

	return std::string();
}

// The hand-off's words: on the status line and, for a hidden run, on the output (the screen change that follows replaces the status line)
static void HandOffSays(std::string const &words)
{
	Answer(words);
	printf("AVP_OpenInteng: hand-off: %s\n", words.c_str());
	fflush(stdout);
}

void HierarchyOpenOnSelectedObject()
{
	if (ModelObjects_Count() == 0)
	{
		return;
	}

	uint8_t id[host::kSelectionIdBytes];

	if (!SelectedObjectId(id))
	{
		return;
	}

	std::string objectName;
	std::string const chosen = HierarchyOfObject(id, &objectName);

	if (chosen.empty())
	{
		HandOffSays("Hierarchy: the picked object, " + objectName + ", is not a section of any hierarchy of this file");
		return;
	}

	// the mode works on the OPENED FILE (HierarchyOpenTheLevelAsTheFile, inteng's FUN_0048b8a0 lists the opened file's hierarchies): Select Hierarchy asks, and the answer is the chosen one's place in its list
	HierarchyRunCommand(0x03);
	promptFor = kPromptNone;
	promptText.clear();
	promptList.clear();   // the list dialog the command opened is answered below, not shown
	host::Dialogs_DismissList();

	if (!hierarchyModelOpen)
	{
		HandOffSays("Hierarchy: the opened file could not be taken as the hierarchy file");
		return;
	}

	std::vector<std::string> modelNames;

	if (host::HierarchyEdit_Names(&hierarchyModelFile, &modelNames) != host::HierarchyEditResult::Done)
	{
		return;
	}

	size_t place = 0;

	for (; place < modelNames.size() && modelNames[place] != chosen; place++)
	{
	}

	if (place >= modelNames.size())
	{
		HandOffSays("Hierarchy: the file does not list the hierarchy " + chosen);
		return;
	}

	HierarchyDialogAnswer(std::to_string((unsigned long long)(place + 1)));   // Select Hierarchy: its number
	promptFor = kPromptNone;
	promptText.clear();
	HierarchyRunCommand(0x04);                                               // Select Sequence ...
	promptFor = kPromptNone;
	promptText.clear();
	promptList.clear();   // the list dialog the command opened is answered below, not shown
	host::Dialogs_DismissList();
	HierarchyDialogAnswer("1");                                               // ... the first one: the view plays it
	promptFor = kPromptNone;
	promptText.clear();
}
