// special_windows.cpp: THE SPECIAL OBJECTS' WINDOWS: Object Placement's windows for a special object (Generator settings, the particle generator, the bad guy's
// "Edit object properties", the simple edit) and the placed sound's window. Moved here whole from editor_main.cpp on 2026-11-18 by Melanikus (Frost), so that each
// file stays within what Snyk Code reads (editor_shared.h says why); nothing in it changed. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

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

// Object Placement's special objects (special_mode.h; the dialogs are edobtype.dll's, read in AvP-docs/engine/inteng/edobtype-dialogs.md): Create AVP generator / Bad Guy / Player starting position /
// Sound / Particle Generator at the view (the view's place and turn, as inteng's FUN_004d9af0 cases take them), Edit, Delete, and Find on the sounds screen. (Added 2026-11-15 by Melanikus (Vasquez).)
host::SpecialMode specialMode;
std::string specialTitle;
bool specialPromptFresh = false;

static void SpecialFormOpen();
static void GeneratorSettingsWindowShow(host::GeneratorSettingsTyped const &typed);   // a generator's Edit: edobtype's "Generator Settings" (below)
static void ParticleWindowOpen();   // a particle generator's Edit: inteng's own "Particle generator properties" (below)
static void BadGuyWindowShow();   // a bad guy's Edit: edobtype's "Edit object properties" in the AvP mode (below)

static void SpecialShow(host::SpecialStep const &step)
{
	promptList = specialMode.list;

	// Find (FUN_004d9af0 0x4da337): the list dialog "Select Object" (FUN_00475bf0); the pick is the object's place in the list as it was given
	if (step.prompt && specialMode.ask == host::SpecialAsk::PickObject && !specialMode.list.empty())
	{
		std::vector<std::string> names;

		for (std::string const &line : specialMode.list)
		{
			size_t digits = 0;

			while (digits < line.size() && line[digits] >= '0' && line[digits] <= '9')
			{
				digits++;
			}

			names.push_back(digits > 0 && line.compare(digits, 2, "  ") == 0 ? line.substr(digits + 2) : line);
		}

		DialogList("Select Object", names, [](int32_t place)
		{
			if (place >= 0)
			{
				SpecialTypedAnswer(std::to_string(place + 1));
			}
			else
			{
				host::SpecialMode_Cancel(&specialMode);
			}
		});
		return;
	}

	// Edit: the object's own dialog, ONE window with all its fields and OK / Cancel (edobtype.dll: "Generator Settings", "Edit object properties", "Sound Edit - %s")
	if (step.form)
	{
		// a generator's: edobtype's "Generator Settings" as its resource 133 lays it out (A-8, OP-131); the other kinds: the form of their fields
		std::function<void()> const openTheForm = specialMode.draft.kind == host::SpecialKind::Generator ? std::function<void()>([]() { GeneratorSettingsWindowShow(host::GeneratorSettings_Shown(specialMode.draft)); })
			: specialMode.draft.kind == host::SpecialKind::BadGuy ? std::function<void()>([]() { BadGuyWindowShow(); })
			: specialMode.draft.kind == host::SpecialKind::ParticleGenerator ? std::function<void()>([]() { ParticleWindowOpen(); }) : std::function<void()>([]() { SpecialFormOpen(); });

		if (!step.answer.empty())
		{
			DialogMessage(step.answer, openTheForm);
		}
		else
		{
			openTheForm();
		}

		return;
	}

	if (step.prompt)
	{
		specialTitle = step.title;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptSpecial;
		promptText = step.defaultText;
		specialPromptFresh = !step.defaultText.empty();
		return;
	}

	promptList.clear();
	specialPromptFresh = false;

	if (!step.answer.empty())
	{
		Answer(step.answer);
	}
}

static void SpecialFormOpen()
{
	std::string title;
	std::vector<host::SpecialFormItem> items;

	if (!host::SpecialMode_FormItems(specialMode, &title, &items))
	{
		return;
	}

	// the window: two columns of controls (a label and its edit, or a check box), 20 pixels a row, OK and Cancel under them
	host::DialogFormSpec spec;
	size_t const perColumn = (items.size() + 1) / 2;
	int32_t const rows = (int32_t)(items.size() > 12 ? perColumn : items.size());
	int32_t const columns = items.size() > 12 ? 2 : 1;

	spec.title = title;
	spec.width = columns == 2 ? 760 : 420;
	spec.height = rows * 22 + 70;

	for (size_t i = 0; i < items.size(); i++)
	{
		int32_t const column = columns == 2 && i >= perColumn ? 1 : 0;
		int32_t const row = (int32_t)(columns == 2 ? (i >= perColumn ? i - perColumn : i) : i);
		int32_t const x = 12 + column * 370;
		int32_t const y = 12 + row * 22;
		host::DialogWidget control;

		if (items[i].kind == 1)
		{
			control.type = host::DialogWidgetType::Check;
			control.text = items[i].label;
			control.checked = items[i].value == "y";
			control.box = { x, y, 340, 18 };
			control.group = -1;
		}
		else
		{
			host::DialogWidget label;

			label.type = host::DialogWidgetType::Label;
			label.text = items[i].label;
			label.box = { x, y, 220, 18 };
			spec.widgets.push_back(label);
			control.type = host::DialogWidgetType::Edit;
			control.text = items[i].value;
			control.most = 63;
			control.box = { x + 224, y, 120, 18 };
		}

		spec.widgets.push_back(control);
	}

	host::DialogWidget ok, cancel;

	ok.type = host::DialogWidgetType::Button;
	ok.text = "OK";
	ok.answer = host::kDialogOk;
	ok.box = { spec.width / 2 - 110, rows * 22 + 30, 100, 22 };
	cancel.type = host::DialogWidgetType::Button;
	cancel.text = "Cancel";
	cancel.answer = host::kDialogEscape;
	cancel.box = { spec.width / 2 + 10, rows * 22 + 30, 100, 22 };
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			host::SpecialMode_Cancel(&specialMode);
			return;
		}

		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return;
		}

		// the controls' answers in the order of the items: an edit's text, a check box as y / n (the labels and the buttons are not answers)
		std::vector<std::string> values;

		for (host::DialogWidget const &one : widgets)
		{
			if (one.type == host::DialogWidgetType::Edit)
			{
				values.push_back(one.text);
			}
			else if (one.type == host::DialogWidgetType::Check)
			{
				values.push_back(one.checked ? "y" : "n");
			}
		}

		unsigned const generation = open->Generation();

		SpecialShow(host::SpecialMode_FormAnswer(open, &specialMode, values));

		if (open->Generation() != generation)
		{
			LoadTheObjectListAgain();
		}
	});
}

// THE "Generator Settings" WINDOW, 1:1 (A-8, reassess/object-placement.md OP-131; generator_settings.h has the numbers): edobtype's resource 133 laid out as the DLL lays it (342 x 273 dialog units, 1.5 x
// 1.625 pixels each, the controls in the resource's order): the weights in three group boxes, the two rates, Max NPCs, the boxes, OK and Cancel. OK reads the controls in the DLL's DoDataExchange order and
// the first refused says MFC's words (the window then shows again as typed); else the generator is written as the old form wrote it (SpecialMode_FormAnswer). NAMED: MFC puts the first focus on OK (the
// resource's first control), the toolkit on the first edit. Added 2026-11-16 by Melanikus (Vasquez).
static std::vector<int32_t> generatorWindowIds;   // the control number of each widget of the window shown (0 for a label or a group box)

static void GeneratorSettingsWindowShow(host::GeneratorSettingsTyped const &typed)
{
	auto const dx = [](int32_t u) { return (int32_t)(u * 3 / 2); };
	auto const dy = [](int32_t u) { return (int32_t)(u * 13 / 8); };
	host::DialogFormSpec spec;

	// the dialog units are the client area's, which starts under the window's caption: the toolkit's title band (the font's height) and a pixel or two are kept above the first control
	int32_t const clientTop = 18;

	spec.title = "Generator Settings";   // resource 133's caption
	spec.width = dx(342) + 10;
	spec.height = dy(273) + 10 + clientTop;
	generatorWindowIds.clear();

	auto const add = [&spec, clientTop](host::DialogWidgetType kind, int32_t id, std::string const &words, int32_t x, int32_t y, int32_t w, int32_t h, int32_t answer)
	{
		host::DialogWidget one;

		one.type = kind;
		one.text = words;
		one.box = { x, y + clientTop, w, h };
		one.answer = answer;
		one.most = 255;
		spec.widgets.push_back(one);
		generatorWindowIds.push_back(id);
	};

	// the edits by control: 1027 .. 1036, 1038 .. 1041, 1049 hold the weights (generator_settings.h's order), 1042 / 1043 the rates, 1051 Max NPCs
	auto const edit = [&](int32_t id, int32_t x, int32_t y)
	{
		static int32_t const weightIds[15] = { 1027, 1028, 1029, 1030, 1031, 1032, 1033, 1034, 1035, 1036, 1038, 1039, 1040, 1041, 1049 };
		std::string words;

		for (int i = 0; i < 15; i++)
		{
			words = weightIds[i] == id ? typed.weights[i] : words;
		}

		words = id == 1042 ? typed.rate : id == 1043 ? typed.increase : id == 1051 ? typed.maxNpcs : words;
		add(host::DialogWidgetType::Edit, id, words, dx(x), dy(y), dx(40), dy(14), 0);
	};

	// a check box with its words (BS_AUTOCHECKBOX): the box and the words to its right
	auto const box = [&](int32_t id, std::string const &words, bool checked, int32_t x, int32_t y)
	{
		add(host::DialogWidgetType::Check, id, std::string(), dx(x), dy(y), 12, 12, 0);
		spec.widgets.back().checked = checked;
		add(host::DialogWidgetType::Label, 0, words, dx(x) + 16, dy(y) - 2, dx(100), 16, 0);
	};

	auto const label = [&](std::string const &words, int32_t x, int32_t y)
	{
		add(host::DialogWidgetType::Label, 0, words, dx(x), dy(y), dx(140), 16, 0);
	};

	auto const group = [&](std::string const &words, int32_t x, int32_t y, int32_t w, int32_t h)
	{
		add(host::DialogWidgetType::Group, 0, words, dx(x), dy(y), dx(w), dy(h), 0);
	};

	add(host::DialogWidgetType::Button, 1, "OK", dx(115), dy(255), dx(50), dy(14), host::kDialogOk);
	add(host::DialogWidgetType::Button, 2, "Cancel", dx(195), dy(255), dx(50), dy(14), host::kDialogEscape);
	edit(1027, 20, 15);
	edit(1028, 20, 35);
	edit(1029, 20, 55);
	edit(1030, 20, 75);
	edit(1031, 20, 95);
	edit(1032, 20, 115);
	group("Marine weightings", 10, 5, 108, 150);
	label("Pulse Rifle", 70, 20);
	label("Flamer", 70, 40);
	label("Smartgun", 70, 60);
	label("Sadar", 70, 80);
	label("Grenade", 70, 100);
	label("Minigun", 70, 120);
	edit(1033, 140, 15);
	edit(1034, 140, 35);
	edit(1035, 140, 55);
	edit(1036, 140, 75);
	edit(1038, 140, 95);
	label("Shotgun", 190, 20);
	label("Pistol", 190, 40);
	label("Flamer", 190, 60);
	label("Unarmed", 190, 80);
	label("Molotov", 190, 100);
	group("Civilian weightings", 130, 5, 95, 130);
	edit(1039, 245, 15);
	edit(1040, 245, 35);
	edit(1041, 245, 55);
	label("Alien", 300, 20);
	label("PredAlien", 300, 40);
	label("Praetorian", 300, 60);
	group("Alien weightings", 235, 5, 100, 130);
	edit(1042, 15, 180);
	edit(1043, 15, 200);
	box(1047, "Use global generator times", typed.useGlobalTimes, 15, 165);
	label("Initial generation rate per minute", 75, 185);
	label("Increase in rate per minute", 75, 205);
	box(1044, "Marine", typed.marine, 250, 165);
	box(1045, "Alien", typed.alien, 250, 175);
	box(1046, "Predator", typed.predator, 250, 185);
	group("Multiplayer settings", 240, 155, 70, 45);
	box(1048, "Starts inactive", typed.startsInactive, 130, 150);
	edit(1049, 20, 135);
	label("Pistol", 70, 140);
	edit(1051, 15, 235);
	box(1052, "Use own max npc setting", typed.useOwnMax, 15, 220);
	label("Max NPCs", 65, 240);
	box(1059, "Multiplayer start", typed.multiplayerStart, 240, 140);
	box(1009, "Easy", typed.easy, 245, 215);
	box(1010, "Medium", typed.medium, 245, 225);
	box(1011, "Hard", typed.hard, 245, 235);
	group("Difficulty", 240, 205, 70, 45);

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			host::SpecialMode_Cancel(&specialMode);   // Cancel or Escape: nothing kept
			return;
		}

		// what the window holds, by control
		host::GeneratorSettingsTyped now;
		static int32_t const weightIds[15] = { 1027, 1028, 1029, 1030, 1031, 1032, 1033, 1034, 1035, 1036, 1038, 1039, 1040, 1041, 1049 };

		for (size_t i = 0; i < widgets.size() && i < generatorWindowIds.size(); i++)
		{
			int32_t const id = generatorWindowIds[i];
			host::DialogWidget const &w = widgets[i];

			for (int k = 0; k < 15; k++)
			{
				if (weightIds[k] == id)
				{
					now.weights[k] = w.text;
				}
			}

			now.rate = id == 1042 ? w.text : now.rate;
			now.increase = id == 1043 ? w.text : now.increase;
			now.maxNpcs = id == 1051 ? w.text : now.maxNpcs;
			now.useGlobalTimes = id == 1047 ? w.checked : now.useGlobalTimes;
			now.startsInactive = id == 1048 ? w.checked : now.startsInactive;
			now.marine = id == 1044 ? w.checked : now.marine;
			now.alien = id == 1045 ? w.checked : now.alien;
			now.predator = id == 1046 ? w.checked : now.predator;
			now.useOwnMax = id == 1052 ? w.checked : now.useOwnMax;
			now.multiplayerStart = id == 1059 ? w.checked : now.multiplayerStart;
			now.easy = id == 1009 ? w.checked : now.easy;
			now.medium = id == 1010 ? w.checked : now.medium;
			now.hard = id == 1011 ? w.checked : now.hard;
		}

		host::SpecialObject made = specialMode.draft;
		std::string message;
		int32_t control = 0;

		if (!host::GeneratorSettings_Ok(now, &made, &message, &control))
		{
			DialogMessage(message, [now]() { GeneratorSettingsWindowShow(now); });   // MFC's words; the window stays as typed
			return;
		}

		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return;
		}

		// the generator written as the old form wrote it: its controls' answers in SpecialMode_FormItems' order (Easy, Medium, Hard, the 14 weights, the two rates, Marine, Alien, Predator, Use global
		// generator times, Starts inactive, Marine Pistol, Max NPCs, Use own max npc setting, Multiplayer start), from the values OK made
		auto const yes = [](bool b) { return std::string(b ? "y" : "n"); };
		auto const hundredths = [](int32_t v) { char text[32]; _snprintf_s(text, sizeof(text), _TRUNCATE, "%.2f", v * 0.01); return std::string(text); };
		std::vector<std::string> values = { yes(now.easy), yes(now.medium), yes(now.hard) };

		for (int k = 0; k < 14; k++)
		{
			values.push_back(std::to_string((long long)made.weights[k]));
		}

		values.push_back(hundredths(made.genRate));
		values.push_back(hundredths(made.genIncrease));
		values.push_back(yes(now.marine));
		values.push_back(yes(now.alien));
		values.push_back(yes(now.predator));
		values.push_back(yes(now.useGlobalTimes));
		values.push_back(yes(now.startsInactive));
		values.push_back(std::to_string((long long)made.weights[14]));
		values.push_back(std::to_string((unsigned)made.genLimit));
		values.push_back(yes(now.useOwnMax));
		values.push_back(yes(now.multiplayerStart));

		unsigned const generation = open->Generation();

		SpecialShow(host::SpecialMode_FormAnswer(open, &specialMode, values));

		if (open->Generation() != generation)
		{
			LoadTheObjectListAgain();
		}
	});
}

// THE PARTICLE GENERATOR'S WINDOW, 1:1 (A-8, reassess/object-placement.md OP-134; particle_edit.h has the numbers): inteng's own "Particle generator properties" (FUN_004e7310, 400 x 250, its parts at their
// pixels), Type opens "Select Particle Generator Type" and the window stays; every way out keeps what it holds (FUN_004e7750). Added 2026-11-16 by Melanikus (Vasquez).
namespace
{
	int32_t const kParticleType = 102, kParticleSound = 104, kParticleRemoveSound = 106, kParticleOk = 2;   // the buttons' answers: 100 + their control number; Ok answers 2 (FUN_004e79b0)

	host::ParticleTyped particleNow;   // what the window holds (kept while the Type list is open)
	std::string particleSoundName;
}


// The particle window has ended (any way out): what it held is kept (FUN_004e7750 whatever FUN_0057c800 answered), and the generator's sound with it (OP-135)
static void ParticleWindowKeep()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		return;
	}

	unsigned const generation = open->Generation();

	SpecialShow(host::SpecialMode_FormWrite(open, &specialMode, host::Particle_Ok(specialMode.draft, particleNow)));

	if (open->Generation() != generation)
	{
		LoadTheObjectListAgain();
	}
}

static void ParticleWindowShow()
{
	host::DialogFormSpec spec;

	spec.title = "Particle generator properties";   // 0x5e8f60
	spec.width = 400;
	spec.height = 0xfa;

	auto const add = [&spec](host::DialogWidgetType kind, std::string const &words, int32_t x, int32_t y, int32_t w, int32_t h, int32_t answer, size_t most)
	{
		host::DialogWidget one;

		one.type = kind;
		one.text = words;
		one.box = { x, y, w, h };
		one.answer = answer;
		one.most = most;
		spec.widgets.push_back(one);
	};

	add(host::DialogWidgetType::Button, "Ok", 0xaf, 0xe1, 0x50, 0x12, kParticleOk, 0);                                      // 1
	add(host::DialogWidgetType::Button, "Type", 0x12, 0x28, 0x46, 0x12, kParticleType, 0);                                  // 2
	add(host::DialogWidgetType::Label, host::Particle_TypeName(particleNow.type), 100, 0x28, 0x118, 0x12, 0, 0);            // 3
	add(host::DialogWidgetType::Button, "Sound", 0x12, 0x3c, 0x46, 0x12, kParticleSound, 0);                                // 4
	add(host::DialogWidgetType::Label, particleSoundName, 100, 0x3c, 0x118, 0x12, 0, 0);                                    // 5
	add(host::DialogWidgetType::Button, "Remove Sound", 0x12, 0x50, 0x78, 0x12, kParticleRemoveSound, 0);                   // 6
	add(host::DialogWidgetType::Label, "Active", 0x28, 100, 100, 0x12, 0, 0);                                                 // 7
	add(host::DialogWidgetType::Check, std::string(), 0x12, 100, 16, 16, 0, 0);                                               // 8
	spec.widgets.back().checked = particleNow.active;
	add(host::DialogWidgetType::Edit, particleNow.frequency, 0x12, 0x78, 100, 0x12, 0, 10);                                 // 9 (FUN_0057d8e0, 10 letters)
	add(host::DialogWidgetType::Label, "Frequency (seconds) (0-6000)", 0x76, 0x78, 0x118, 0x12, 0, 0);                      // 10
	add(host::DialogWidgetType::Edit, std::to_string((long long)particleNow.probability), 0x12, 0x8c, 100, 0x12, 0, 10);   // 11 (FUN_0057de30, a number box)
	add(host::DialogWidgetType::Label, "Probability(0-100)", 0x76, 0x8c, 200, 0x12, 0, 0);                                  // 12
	add(host::DialogWidgetType::Edit, std::to_string((long long)particleNow.quantity), 0x12, 0xa0, 100, 0x12, 0, 10);      // 13
	add(host::DialogWidgetType::Label, "Quantity(1-1000)", 0x76, 0xa0, 200, 0x12, 0, 0);                                    // 14
	add(host::DialogWidgetType::Edit, particleNow.speed, 0x12, 0xb4, 100, 0x12, 0, 10);                                     // 15
	add(host::DialogWidgetType::Label, "Speed (m/s) (0-600)", 0x76, 0xb4, 200, 0x12, 0, 0);                                 // 16

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &w)
	{
		if (w.size() < 16)
		{
			return;
		}

		particleNow.active = w[7].checked;
		particleNow.frequency = w[8].text;
		particleNow.probability = host::HierarchyMode_EditNumber(w[10].text);   // FUN_0057d620
		particleNow.quantity = host::HierarchyMode_EditNumber(w[12].text);
		particleNow.speed = w[14].text;

		if (answer == kParticleType)
		{
			// FUN_004e6950: "Select Particle Generator Type"; the window stays, showing the pick's name
			DialogList("Select Particle Generator Type", { host::Particle_TypeName(0), host::Particle_TypeName(1), host::Particle_TypeName(2), host::Particle_TypeName(3) }, [](int32_t place)
			{
				if (place >= 0 && place < 4)
				{
					particleNow.type = place;
				}

				ParticleWindowShow();
			});
			return;
		}

		if (answer == kParticleRemoveSound)
		{
			// REMOVE SOUND (FUN_004e79b0 case 6, FUN_004ebd20): the record destroyed, the label "None" (0x5d126c); the window stays, redrawn. Its INDSOUND goes when the window ends.
			specialMode.draft.hasSound = false;
			specialMode.draft.particleSoundRead = false;
			specialMode.draft.particleSoundWav.clear();
			specialMode.draft.particleSoundExtra.clear();
			particleSoundName = host::Particle_Shown(specialMode.draft).soundName;
			ParticleWindowShow();
			return;
		}

		if (answer == kParticleSound)
		{
			// SOUND (FUN_004e79b0 case 4, 0x4e7a0d-0x4e7a6f; reassess/object-placement.md OP-135): with no sound a record is made (FUN_004ebc80(0, 1): "Not Set" (0x5e3f5c), inner 500000 and
			// outer 4000000 in inteng's memory (x 1e-5 m), volume 127, pitch 0, flags 1, its loops box live); a sound read from the file is the record FUN_004ebb80 makes of it (the
			// ranges x DAT_00607bd8 = 100 x the level's scale, its loops box live too: 0x4e6849). Either way its Sound Edit (FUN_004ebd60) runs, and then the particle window ENDS and
			// keeps what it holds. Cancel in Sound Edit keeps the record as it was. INTENG BUG, FIXED (Bishop's ruling Q1 11-17, as PH-8's and TB-T1): inteng keeps a record it made for this
			// window on Cancel too, leaving a "Not Set" sound on the generator; ours adds nothing on Cancel. The INDSOUND is made from the record (FUN_004ec1b0: the ranges / DAT_00607bd8).
			host::SpecialObject &draft = specialMode.draft;

			if (draft.hasSound && !draft.particleSoundRead)
			{
				// HOSTILE INPUT: an INDSOUND this editor could not read whole is never remade from guesses; it stays as the file holds it
				Answer("Sound: this generator's sound could not be read whole, so it is kept as the file holds it");
				ParticleWindowShow();
				return;
			}

			double const unit = 100.0 * (local_scale > 0.0 ? local_scale : 1.0);
			auto const ftol = [](double v) { return !(v == v) ? 0 : (v >= 2147483647.0 ? 2147483647 : (v <= -2147483648.0 ? (int32_t)-2147483647 - 1 : (int32_t)v)); };
			bool const made = !draft.hasSound;
			uint32_t const memInner = made ? 500000u : (uint32_t)ftol((double)draft.particleSoundInner * unit);
			uint32_t const memOuter = made ? 4000000u : (uint32_t)ftol((double)draft.particleSoundOuter * unit);

			if (made)
			{
				draft.hasSound = true;
				draft.particleSoundRead = true;
				draft.particleSoundIndex = 0;
				draft.particleSoundWav = "Not Set";
				draft.particleSoundVolume = 127;
				draft.particleSoundPitch = 0;
				draft.particleSoundFlags = 1;
				draft.particleSoundExtra.clear();
			}

			host::SoundEditRecord const record = host::SoundEdit_IndexedRecord(draft.particleSoundWav, memInner, memOuter, draft.particleSoundPitch, draft.particleSoundVolume, draft.particleSoundFlags, true);
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			std::string const soundDirectory = host::TrackLevelSoundDirectory(open);
			std::string const folder = std::string(theGameFolder) + "\\Sound\\" + soundDirectory + "\\";

			SoundEditWindowOpen(record, folder, soundDirectory, [unit, ftol, made](host::SoundEditRecord const &back, bool ok)
			{
				host::SpecialObject &now = specialMode.draft;

				if (made && !ok)
				{
					// the named fix above: the record made for this window goes, and the generator keeps no sound
					now.hasSound = false;
					now.particleSoundRead = false;
					now.particleSoundWav.clear();
					ParticleWindowKeep();
					return;
				}
				int32_t inner = 0, outer = 0, pitch = 0, volume = 0, flags = 0;

				host::SoundEdit_IndexedBack(back, now.particleSoundFlags, &inner, &outer, &pitch, &volume, &flags);
				now.particleSoundWav = back.name;
				now.particleSoundInner = (uint32_t)ftol((double)(uint32_t)inner / unit);
				now.particleSoundOuter = (uint32_t)ftol((double)(uint32_t)outer / unit);
				now.particleSoundPitch = pitch;
				now.particleSoundVolume = volume;
				now.particleSoundFlags = flags;
				ParticleWindowKeep();
			});
			return;
		}

		// every other way out keeps what the window holds (FUN_004e7750 whatever FUN_0057c800 answered)
		ParticleWindowKeep();
	});
}

static void ParticleWindowOpen()
{
	host::ParticleShown const shown = host::Particle_Shown(specialMode.draft);

	particleNow = host::ParticleTyped();
	particleNow.type = specialMode.draft.particleType;
	particleNow.active = shown.active;
	particleNow.frequency = shown.frequency;
	particleNow.probability = shown.probability;
	particleNow.quantity = shown.quantity;
	particleNow.speed = shown.speed;
	particleSoundName = shown.soundName;
	ParticleWindowShow();
}

// THE BAD GUY'S "Edit object properties" WINDOW, 1:1 (A-8, reassess/object-placement.md OP-132; bad_guy_edit.h has the numbers): edobtype's resource 129 in the AvP mode (302 x 231 dialog units under the
// caption), the parts the bad-guy class shows: "Type of Object" and its combo, the type's own controls on the left (1001 combo / 1003 edit under 1004's words) and on the right (1018 combo / 1020 edit under
// 1017's), "Texturing ID" and its combo (live for a Marine), "Starts Inactive", OK, Cancel. A pick in the type combo shows that type's controls at once (FUN_10005400). Game mode and Difficulty are hidden as
// inteng hides them (Bishop's ruling 11-16) and their flags come back unchanged. Added 2026-11-16 by Melanikus (Vasquez).
namespace
{
	// the window's parts by place (BadGuyWindowShow adds them in this order)
	enum BadGuyPart : size_t
	{
		kBgOk, kBgCancel, kBgTypeLabel, kBgType, kBgLeftLabel, kBgLeftCombo, kBgLeftEdit, kBgRightLabel, kBgRightCombo, kBgRightEdit, kBgTextureLabel, kBgTexture, kBgInactive, kBgInactiveLabel, kBgParts
	};

	host::BadGuyControls badGuyShown;   // what the window shows now (its combos' numbers)

	// THE NEW-BAD-GUY DEFAULTS (OP-132): after every Edit of a bad guy, OK or Cancel alike, its type, subtype, texture and extras become the next new one's (0x4e5e96-0x4e5eb5: DAT_005e7a48 the type,
	// DAT_008f4f94 / 95 / 96 / 97 the subtype, extra 1, extra 2, texture), which Create > AVP Bad Guy takes (FUN_004dd600 0x4dd65e-0x4dd692; the flags are 0x3801 always). Before any Edit: type 1, the rest 0.
	struct BadGuyDefaults
	{
		bool set = false;
		int32_t type = 1;
		uint8_t subType = 0, extra1 = 0, extra2 = 0, texture = 0;
	};

	BadGuyDefaults badGuyDefaults;

	void BadGuyDefaultsFrom(host::SpecialObject const &one)
	{
		badGuyDefaults.set = true;
		badGuyDefaults.type = one.type;
		badGuyDefaults.subType = one.subType;
		badGuyDefaults.extra1 = one.extra1;
		badGuyDefaults.extra2 = one.extra2;
		badGuyDefaults.texture = one.texture;
	}
}

// The type's controls into the window's parts (at the opening, and after a pick in the type combo)
static void BadGuyPartsFor(std::vector<host::DialogWidget> &w, host::BadGuyControls const &c)
{
	auto const names = [](host::BadGuyChoices const &choices)
	{
		std::vector<std::string> out;

		for (auto const &one : choices)
		{
			out.push_back(one.first);
		}

		return out;
	};

	auto const combo = [&](size_t part, host::BadGuyChoices const &choices, int32_t pick, bool shown, bool live)
	{
		w[part].items = names(choices);
		w[part].selected = pick;
		w[part].text = pick >= 0 && (size_t)pick < choices.size() ? choices[(size_t)pick].first : std::string();
		w[part].visible = shown;
		w[part].enabled = live;
		w[part].listOpen = false;
	};

	w[kBgLeftLabel].text = c.leftLabel;
	w[kBgLeftLabel].visible = !c.leftLabel.empty();
	w[kBgRightLabel].text = c.rightLabel;
	w[kBgRightLabel].visible = !c.rightLabel.empty();
	combo(kBgLeftCombo, c.leftChoices, c.leftPick, c.leftCombo, true);
	combo(kBgRightCombo, c.rightChoices, c.rightPick, c.rightCombo, true);
	combo(kBgTexture, c.textures, c.texturePick, true, c.textureLive);
	w[kBgLeftEdit].text = c.leftText;
	w[kBgLeftEdit].visible = c.leftEdit;
	w[kBgRightEdit].text = c.rightText;
	w[kBgRightEdit].visible = c.rightEdit;
}

static void BadGuyWindowShow()
{
	auto const dx = [](int32_t u) { return (int32_t)(u * 3 / 2); };
	auto const dy = [](int32_t u) { return (int32_t)(u * 13 / 8); };
	int32_t const clientTop = 18;   // the dialog units start under the caption (the toolkit's title band)
	host::DialogFormSpec spec;

	badGuyShown = host::BadGuy_Controls(specialMode.draft, specialMode.draft.type);
	spec.title = "Edit object properties";   // resource 129's caption
	spec.width = dx(302) + 10;
	spec.height = dy(231) + 10 + clientTop;
	spec.widgets.resize(kBgParts);

	auto const put = [&](size_t part, host::DialogWidgetType kind, std::string const &words, int32_t x, int32_t y, int32_t width, int32_t height, int32_t answer)
	{
		host::DialogWidget &one = spec.widgets[part];

		one.type = kind;
		one.text = words;
		one.box = { dx(x), dy(y) + clientTop, width, height };
		one.answer = answer;
		one.most = 63;
	};

	put(kBgOk, host::DialogWidgetType::Button, "OK", 55, 190, dx(50), dy(14), host::kDialogOk);
	put(kBgCancel, host::DialogWidgetType::Button, "Cancel", 170, 190, dx(50), dy(14), host::kDialogEscape);
	put(kBgTypeLabel, host::DialogWidgetType::Label, "Type of Object", 10, 10, dx(100), 16, 0);      // 1016
	put(kBgType, host::DialogWidgetType::Combo, std::string(), 10, 24, dx(115), 18, 0);              // 1002
	put(kBgLeftLabel, host::DialogWidgetType::Label, std::string(), 10, 45, dx(114), 16, 0);         // 1004
	put(kBgLeftCombo, host::DialogWidgetType::Combo, std::string(), 10, 60, dx(115), 18, 0);         // 1001
	put(kBgLeftEdit, host::DialogWidgetType::Edit, std::string(), 10, 60, dx(105), dy(12) + 2, 0);   // 1003
	put(kBgRightLabel, host::DialogWidgetType::Label, std::string(), 165, 45, dx(110), 16, 0);       // 1017
	put(kBgRightCombo, host::DialogWidgetType::Combo, std::string(), 165, 60, dx(115), 18, 0);       // 1018
	put(kBgRightEdit, host::DialogWidgetType::Edit, std::string(), 165, 60, dx(105), dy(12) + 2, 0); // 1020
	put(kBgTextureLabel, host::DialogWidgetType::Label, "Texturing ID", 10, 85, dx(70), 16, 0);      // 1019
	put(kBgTexture, host::DialogWidgetType::Combo, std::string(), 10, 95, dx(115), 18, 0);           // 1022
	// each list shows the rows its dialog resource drops it to, then scrolls (dialog_box.h DialogBox_ComboListRows): 1002 is 74 dialog units high, 1001, 1018 and 1022 81
	spec.widgets[kBgType].listRows = host::DialogBox_ComboListRows(74);
	spec.widgets[kBgLeftCombo].listRows = host::DialogBox_ComboListRows(81);
	spec.widgets[kBgRightCombo].listRows = host::DialogBox_ComboListRows(81);
	spec.widgets[kBgTexture].listRows = host::DialogBox_ComboListRows(81);
	put(kBgInactive, host::DialogWidgetType::Check, std::string(), 10, 130, 12, 12, 0);              // 1008 "Starts Inactive"
	spec.widgets[kBgInactive].checked = badGuyShown.startsInactive;
	put(kBgInactiveLabel, host::DialogWidgetType::Label, "Starts Inactive", 10, 130, dx(104), 16, 0);
	spec.widgets[kBgInactiveLabel].box.x += 16;
	spec.widgets[kBgInactiveLabel].box.y -= 2;

	{
		std::vector<std::string> types;

		for (auto const &one : badGuyShown.types)
		{
			types.push_back(one.first);
		}

		spec.widgets[kBgType].items = types;
		spec.widgets[kBgType].selected = badGuyShown.typePick;
		spec.widgets[kBgType].text = badGuyShown.typePick >= 0 ? badGuyShown.types[(size_t)badGuyShown.typePick].first : std::string();
	}

	BadGuyPartsFor(spec.widgets, badGuyShown);

	// a pick in the type combo: that type's controls at once (FUN_10005400 with the new type; the subtype and the numbers are the bad guy's)
	spec.onChange = [](std::vector<host::DialogWidget> &w, size_t part)
	{
		if (part != kBgType || w[kBgType].selected < 0 || (size_t)w[kBgType].selected >= badGuyShown.types.size())
		{
			return;
		}

		host::BadGuyChoices const types = badGuyShown.types;
		int32_t const typePick = w[kBgType].selected;

		badGuyShown = host::BadGuy_Controls(specialMode.draft, types[(size_t)typePick].second);
		BadGuyPartsFor(w, badGuyShown);
	};

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &w)
	{
		if (answer != host::kDialogOk || w.size() < kBgParts)
		{
			BadGuyDefaultsFrom(specialMode.draft);     // 0x4e5e96: the next new bad guy's, after Cancel too
			host::SpecialMode_Cancel(&specialMode);   // Cancel or Escape: nothing kept
			return;
		}

		auto const valueOf = [](host::BadGuyChoices const &choices, host::DialogWidget const &part) -> int32_t
		{
			return part.visible && part.selected >= 0 && (size_t)part.selected < choices.size() ? choices[(size_t)part.selected].second : -1;
		};

		host::BadGuyTyped typed;

		typed.type = w[kBgType].selected >= 0 && (size_t)w[kBgType].selected < badGuyShown.types.size() ? badGuyShown.types[(size_t)w[kBgType].selected].second : specialMode.draft.type;
		typed.leftPick = valueOf(badGuyShown.leftChoices, w[kBgLeftCombo]);
		typed.rightPick = valueOf(badGuyShown.rightChoices, w[kBgRightCombo]);
		typed.texturePick = valueOf(badGuyShown.textures, w[kBgTexture]);
		typed.leftText = w[kBgLeftEdit].text;
		typed.rightText = w[kBgRightEdit].text;
		typed.startsInactive = w[kBgInactive].checked;

		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return;
		}

		unsigned const generation = open->Generation();
		host::SpecialObject const kept = host::BadGuy_Ok(specialMode.draft, typed);

		BadGuyDefaultsFrom(kept);   // 0x4e5e96-0x4e5eb5: the next new bad guy's
		SpecialShow(host::SpecialMode_FormWrite(open, &specialMode, kept));

		if (open->Generation() != generation)
		{
			LoadTheObjectListAgain();
		}
	});
}

// Object Placement's Edit of an ORDINARY object (simple_object_edit.h): edobtype.dll's "Edit object properties" window
static host::SimpleObjectEdit simpleEdit;

static void SimpleEditShow()
{
	// edobtype.dll's dialog 129 as its resource lays it out (rects in dialog units, 1.5 px by 1.625 px): the type combo box, the list headed "Enter type", Mass and Integrity, Game mode, Difficulty, the Explosion
	// radio buttons, OK and Cancel. The parts keep their places; what a type shows is made visible (FUN_10004520), at once when the combo box's pick changes (CBN_SELCHANGE, FUN_100060e0).
	std::string title;
	std::vector<host::SimpleFormItem> items;

	host::SimpleEdit_Items(simpleEdit, &title, &items);

	auto const dx = [](int32_t u) { return (int32_t)(u * 3 / 2); };
	auto const dy = [](int32_t u) { return (int32_t)(u * 13 / 8); };

	host::DialogFormSpec spec;

	spec.title = title;
	spec.width = dx(302) + 10;
	spec.height = dy(231) + 10;

	auto const label = [&](char const *words, int32_t x, int32_t y)
	{
		host::DialogWidget one;

		one.type = host::DialogWidgetType::Label;
		one.text = words;
		one.box = { dx(x), dy(y), 200, 16 };
		spec.widgets.push_back(one);
	};

	// 0 label, 1 Type combo
	label("Type of Object", 10, 10);

	host::DialogWidget typeBox;

	typeBox.type = host::DialogWidgetType::Combo;
	typeBox.box = { dx(10), dy(24) - 4, dx(115), 18 };
	typeBox.listRows = host::DialogBox_ComboListRows(74);   // 1002: 74 dialog units high with its list (dialog_box.h)
	typeBox.items = items[0].choices;
	typeBox.text = items[0].value;

	for (size_t i = 0; i < typeBox.items.size(); i++)
	{
		if (typeBox.items[i] == typeBox.text)
		{
			typeBox.selected = (int32_t)i;
		}
	}

	spec.widgets.push_back(typeBox);

	// 2 label, 3 the list headed "Enter type"
	label("Enter type", 10, 45);

	host::DialogWidget extraBox;

	extraBox.type = host::DialogWidgetType::Combo;
	extraBox.box = { dx(10), dy(60) - 4, dx(115), 18 };
	extraBox.listRows = host::DialogBox_ComboListRows(81);   // 1001: 81 dialog units high with its list (dialog_box.h)
	spec.widgets.push_back(extraBox);

	// 4 label, 5 Mass, 6 label, 7 Integrity
	label("Mass (x100g)", 10, 85);

	host::DialogWidget massBox;

	massBox.type = host::DialogWidgetType::Edit;
	massBox.box = { dx(10), dy(95), dx(48), 18 };
	massBox.most = 10;
	massBox.text = simpleEdit.massText;
	spec.widgets.push_back(massBox);
	label("Integrity", 65, 85);

	host::DialogWidget integrityBox = massBox;

	integrityBox.box = { dx(65), dy(95), dx(50), 18 };
	integrityBox.text = simpleEdit.integrityText;
	spec.widgets.push_back(integrityBox);

	// 8 label "Game mode", 9-11 Marine Alien Predator, 12 label "Difficulty", 13-15 Easy Medium Hard: ALL HIDDEN, as inteng hides them (resource 129's style 0x48010003 / 0x48000007 has no
	// WS_VISIBLE, and the simple object's init FUN_10003de0 never shows them; Bishop's ruling for the same boxes of the bad guy, 11-16). Their ticks stay in the widgets, so OK gives the
	// flags back as they were. (Fixed 11-17 by Drake: they were shown and could be changed.)
	label("Game mode", 165, 95);
	spec.widgets.back().visible = false;
	label("Difficulty", 230, 95);
	spec.widgets.back().visible = false;

	char const *const modeWords[3] = { "Marine", "Alien", "Predator" };
	char const *const difficultyWords[3] = { "Easy", "Medium", "Hard" };

	for (int i = 0; i < 6; i++)
	{
		host::DialogWidget check;

		check.type = host::DialogWidgetType::Check;
		check.text = i < 3 ? modeWords[i] : difficultyWords[i - 3];
		check.box = { dx(i < 3 ? 171 : 235), dy(105 + (i % 3) * 10) + 8, 14, 14 };
		check.group = -1;
		check.visible = false;   // hidden as inteng (above)

		for (host::SimpleFormItem const &one : items)
		{
			if (one.kind == 1 && one.label == check.text)
			{
				check.checked = one.value == "y";
			}
		}

		spec.widgets.push_back(check);

		host::DialogWidget words;

		words.type = host::DialogWidgetType::Label;
		words.text = check.text;
		words.box = { check.box.x + 18, check.box.y, 90, 16 };
		words.visible = false;
		spec.widgets.push_back(words);
	}

	// then the Explosion group: a label, four radio buttons (one group) each with its words
	label("Explosion", 200, 10);

	size_t const firstRadio = spec.widgets.size();

	for (int i = 0; i < 4; i++)
	{
		host::DialogWidget radio;
		static char const *const explosionWords[4] = { "None", "Small", "Big", "Molotov" };

		radio.type = host::DialogWidgetType::Check;
		radio.text = explosionWords[i];
		radio.group = 0;
		radio.box = { dx(205), dy(20 + i * 10) + 8, 14, 14 };
		radio.checked = items.back().value == explosionWords[i];
		spec.widgets.push_back(radio);

		host::DialogWidget words;

		words.type = host::DialogWidgetType::Label;
		words.text = explosionWords[i];
		words.box = { radio.box.x + 18, radio.box.y, 90, 16 };
		spec.widgets.push_back(words);
	}

	host::DialogWidget ok, cancel;

	ok.type = host::DialogWidgetType::Button;
	ok.text = "OK";
	ok.answer = host::kDialogOk;
	ok.box = { dx(55), dy(190), dx(50), 22 };
	cancel.type = host::DialogWidgetType::Button;
	cancel.text = "Cancel";
	cancel.answer = host::kDialogEscape;
	cancel.box = { dx(170), dy(190), dx(50), 22 };
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);

	// the parts a type shows: set now and again whenever the type's pick changes
	static size_t const kTypeCombo = 1, kExtraLabel = 2, kExtraCombo = 3, kMassLabel = 4, kMassEdit = 5, kIntegrityLabel = 6, kIntegrityEdit = 7;

	auto const layoutFor = [](std::vector<host::DialogWidget> &widgets, int32_t type, int32_t extra)
	{
		host::SimpleLayout const layout = host::SimpleEdit_LayoutFor(type);

		widgets[kExtraLabel].visible = layout.extra;
		widgets[kExtraCombo].visible = layout.extra;
		widgets[kExtraCombo].items = layout.extraChoices;
		widgets[kExtraCombo].listOpen = false;
		widgets[kExtraCombo].selected = layout.extra ? host::SimpleEdit_ExtraRow(type, extra) : -1;
		widgets[kExtraCombo].text = widgets[kExtraCombo].selected >= 0 ? layout.extraChoices[(size_t)widgets[kExtraCombo].selected] : std::string();
		widgets[kMassLabel].visible = layout.mass;
		widgets[kMassEdit].visible = layout.mass;
		widgets[kIntegrityLabel].visible = layout.mass;
		widgets[kIntegrityEdit].visible = layout.mass;
	};

	layoutFor(spec.widgets, simpleEdit.draft.type, simpleEdit.draft.extra);

	spec.onChange = [layoutFor](std::vector<host::DialogWidget> &widgets, size_t changed)
	{
		int32_t type = 0;

		if (changed == kTypeCombo && host::SimpleEdit_TypeOfWord(widgets[kTypeCombo].text, &type))
		{
			simpleEdit.draft.type = type;
			layoutFor(widgets, type, simpleEdit.draft.extra);
		}
	};

	(void)firstRadio;

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		if (answer != host::kDialogOk)
		{
			simpleEdit.open = false;
			return;
		}

		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return;
		}

		// the controls' words in the order of SimpleEdit_Items for the picked type: the type, the list if shown, the number boxes if shown, the six boxes, the radio button that is on
		std::vector<std::string> values;
		bool const extraShown = widgets[3].visible, massShown = widgets[5].visible;

		values.push_back(widgets[1].text);

		if (extraShown)
		{
			values.push_back(widgets[3].text);
		}

		if (massShown)
		{
			values.push_back(widgets[5].text);
			values.push_back(widgets[7].text);
		}

		for (size_t i = 8; i < widgets.size(); i++)
		{
			host::DialogWidget const &one = widgets[i];

			if (one.type == host::DialogWidgetType::Check && one.group < 0)
			{
				values.push_back(one.checked ? "y" : "n");
			}
		}

		std::string explosion;

		for (size_t i = 8; i < widgets.size(); i++)
		{
			if (widgets[i].type == host::DialogWidgetType::Check && widgets[i].group == 0 && widgets[i].checked)
			{
				explosion = widgets[i].text;
			}
		}

		values.push_back(explosion);

		unsigned const generation = open->Generation();
		std::string words;
		host::SimpleAnswer const result = host::SimpleEdit_Answer(open, open->Level(), &simpleEdit, values, &words);

		if (open->Generation() != generation)
		{
			LoadTheObjectListAgain();
		}

		if (result == host::SimpleAnswer::Message)
		{
			while (!words.empty() && words.back() == '\n')
			{
				words.pop_back();
			}

			DialogMessage(words, []() { SimpleEditShow(); });   // inteng: the box "Object Edit", then the window is still open
		}
		else if (result == host::SimpleAnswer::Done)
		{
			Answer("Object properties changed");
		}
		else
		{
			Answer("Edit: " + words);
		}
	});
}

void SimpleEditOpen()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::Selected const selected = host::Selection_Get();

	if (!open || !open->Level() || selected.kind != host::SelectedKind::Object)
	{
		Answer("Edit: select an object first");
		return;
	}

	if (!host::SimpleEdit_Begin(open, open->Level(), selected.objectId, &simpleEdit))
	{
		Answer("Edit: the object could not be read");
		return;
	}

	SimpleEditShow();
}

void SpecialRunCommand(int32_t command, bool soundsScreen)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		Answer("No level is held to work on");
		return;
	}

	bool const create = command == 0x34 || command == 0x35 || command == 0x3a || command == 0x37 || command == 0x39;

	// OP-12 / OP-47 (V-8, V-21): Edit (0x18, key E) and Delete (0x16) need Gravity off (0x4dc1fe); Delete does nothing while an object flies (0x4dc343): nothing happens, as in inteng
	if ((command == 0x18 || command == 0x16) && (PlacementGravityOn() || (command == 0x16 && PlacementIsFlying())))
	{
		return;
	}

	// Create > AVP generator / Bad Guy / Sound / Particle Generator: inteng's "Name Object" dialog first (FUN_004d53d0 at 0x4daa1b, 0x4dab31, 0x4dad35, 0x4dac43; OP-24); Cancel makes nothing. The player
	// start takes no name (0x4db273).
	if (create && command != 0x3a && !specialNameGiven)
	{
		NameObjectBox("", [command, soundsScreen](std::string const &name)
		{
			specialMode.createName = name;
			specialNameGiven = true;
			SpecialRunCommand(command, soundsScreen);
			specialNameGiven = false;
		});
		return;
	}

	// Object > Edit / E on the current SOUND: the sound's +0x38 (0x4ed060), edobtype's Sound Edit window (OP-133a..c)
	if (command == 0x18 && specialMode.haveCurrent && specialMode.kind == host::SpecialKind::Sound && host::Selection_Get().kind != host::SelectedKind::Object)
	{
		PlacedSoundEdit();
		return;
	}

	// Object > Find on Object Placement: inteng's "Select Object" list of every object (OP-10); the sounds screen keeps its own Find (the sounds only)
	if (command == 0x19 && !soundsScreen)
	{
		PlacementFind();
		return;
	}

	// an ordinary object selected after the special one: that one is the object the commands work on now
	if (!create && host::Selection_Get().kind == host::SelectedKind::Object)
	{
		specialMode.haveCurrent = false;
	}

	specialMode.levelScale = local_scale > 0.0 ? local_scale : 1.0;
	specialMode.soundsOnly = soundsScreen;
	specialMode.useForm = true;

	if (create)
	{
		// the view's place and turn (FUN_004d9af0: DAT_00962308 + 0x10 and +0x38): the file's place is the game's divided by the level's scale (object_setup.cpp:1225)
		int32_t matrix[9];
		int32_t heading = 0;
		float quaternion[4] = { 0, 0, 0, 1 };

		if (!theCamera || !LevelView_CameraMatrix(matrix) || !LevelView_CameraHeading(&heading))
		{
			Answer("Create: no frame has been drawn yet, so there is no view to make it at");
			return;
		}

		LevelView_CameraQuaternion(quaternion);
		specialMode.where = host::SpecialPlace();
		specialMode.where.place[0] = (int32_t)(theCamera->x / specialMode.levelScale);
		specialMode.where.place[1] = (int32_t)(theCamera->y / specialMode.levelScale);
		specialMode.where.place[2] = (int32_t)(theCamera->z / specialMode.levelScale);
		specialMode.where.heading = heading;
		memcpy(specialMode.where.matrix, matrix, sizeof(matrix));
		memcpy(specialMode.where.turn, quaternion, sizeof(quaternion));
	}

	unsigned const generation = open->Generation();
	host::SpecialStep const step = host::SpecialMode_Command(open, &specialMode, command);

	if (step.passOn)
	{
		if (command == 0x18 && host::Selection_Get().kind == host::SelectedKind::Object)
		{
			SimpleEditOpen();
			return;
		}

		Answer(command == 0x16 ? "Delete: select an object first" : command == 0x19 ? "Find: nothing to find here" : "Edit: select an object first (Create makes a special one and selects it; Find on the sounds screen selects a sound)");
		return;
	}

	if (create)
	{
		host::Selection_Clear();
	}

	// a NEW bad guy takes the new-bad-guy defaults the last Edit of a bad guy made (OP-132, FUN_004dd600 0x4dd65e-0x4dd692)
	if (command == 0x35 && badGuyDefaults.set && open->Generation() != generation && specialMode.haveCurrent && specialMode.kind == host::SpecialKind::BadGuy)
	{
		std::vector<host::SpecialObject> all;

		if (host::SpecialObjects_Read(open, &all) == host::SpecialResult::Done)
		{
			for (host::SpecialObject &one : all)
			{
				if (one.kind == host::SpecialKind::BadGuy && one.number == specialMode.number)
				{
					one.type = badGuyDefaults.type;
					one.subType = badGuyDefaults.subType;
					one.extra1 = badGuyDefaults.extra1;
					one.extra2 = badGuyDefaults.extra2;
					one.texture = badGuyDefaults.texture;
					host::SpecialObject_Write(open, one);
					break;
				}
			}
		}
	}

	// a NEW sound takes the new-sound defaults when P has set them (OP-26)
	if (command == 0x37 && soundDefaults.set && open->Generation() != generation && specialMode.haveCurrent && specialMode.kind == host::SpecialKind::Sound)
	{
		std::vector<host::SpecialObject> all;

		if (host::SpecialObjects_Read(open, &all) == host::SpecialResult::Done)
		{
			for (host::SpecialObject &one : all)
			{
				if (one.kind == host::SpecialKind::Sound && one.number == specialMode.number)
				{
					auto const fileOf = [](double mm) { double const file = mm / specialMode.levelScale; return file > 2147483000.0 ? 2147483000 : (file < -2147483000.0 ? -2147483000 : (int32_t)file); };

					one.inner = fileOf(soundDefaults.innerMm);
					one.outer = fileOf(soundDefaults.outerMm);
					one.volume = soundDefaults.volume;
					one.pitch = soundDefaults.pitch;
					one.soundFlags = soundDefaults.flags;
					one.probability = soundDefaults.probability;
					host::SpecialObject_Write(open, one);
					break;
				}
			}
		}
	}

	SpecialShow(step);

	if (open->Generation() != generation)
	{
		LoadTheObjectListAgain();
	}
}

void SpecialTypedAnswer(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		Answer("No level is held to work on");
		return;
	}

	unsigned const generation = open->Generation();

	SpecialShow(host::SpecialMode_Typed(open, &specialMode, typed));

	if (open->Generation() != generation)
	{
		LoadTheObjectListAgain();
	}
}

// THE SOUND EDIT WINDOW, 1:1 (A-10; reassess/object-placement.md OP-133a..c; sound_edit.h has the numbers): edobtype's resource 132 as it lays it out (245 x 178 dialog units, 1.5 x 1.625
// pixels each), its wav name an EDITABLE combo (CBS_DROPDOWN) holding every wav of the sound folder and the folders below it (0x10006a68, edobtype's finder), the one whose name BEGINS with the
// sound's name picked (CB_SELECTSTRING, any capitals; an empty name picks the first); Inner Range (m), Outer Range (m), Pitch +/- semitones, Volume % as MFC's float fields; Playing at Start and
// Sound Loops (live under the mask); Play; OK; Cancel; the probability hidden (AvP). OK (and Play) read the fields in the DLL's order and the first that fails says MFC's words and the window
// stays. The window answers with the record (sound_edit.h: OK changes it, Cancel leaves it). Added 2026-11-16 by Melanikus (Vasquez).
namespace
{
	struct SoundEditTyped
	{
		std::string name;
		int32_t picked = -1;
		std::string fields[4];
		bool playingAtStart = false;
		bool loops = false;
	};

	struct SoundEditWindow
	{
		host::SoundEditRecord record;
		std::string folder;
		std::vector<std::string> wavs;
		std::function<void(host::SoundEditRecord const &, bool)> done;
	};

	SoundEditWindow soundEditWindow;
}

static int32_t const kSoundEditPlay = 1065;   // the Play button's control (the DLL's message map 0x1001ab78)

static void SoundEditWindowShow(SoundEditTyped const &typed);

// The fields, the boxes and the name as the window holds them now (OK and Play read them; a refusal shows the window again with them)
static SoundEditTyped SoundEditTypedOf(std::vector<host::DialogWidget> const &widgets)
{
	SoundEditTyped t;
	size_t edit = 0, check = 0;

	for (host::DialogWidget const &w : widgets)
	{
		if (w.type == host::DialogWidgetType::Combo)
		{
			t.name = w.text;
			t.picked = w.selected;
		}
		else if (w.type == host::DialogWidgetType::Edit && edit < 4)
		{
			t.fields[edit++] = w.text;
		}
		else if (w.type == host::DialogWidgetType::Check)
		{
			(check++ == 0 ? t.playingAtStart : t.loops) = w.checked;
		}
	}

	return t;
}

// UpdateData(TRUE) in the DLL's DoDataExchange order (0x10006830): the name, then inner, outer, pitch, volume; the first that fails shows MFC's words and gives false
static bool SoundEditRead(SoundEditTyped const &typed, float values[4])
{
	for (int i = 0; i < 4; i++)
	{
		std::string message;

		if (!host::SoundEdit_ReadField(i, typed.fields[i], &values[i], &message))
		{
			DialogMessage(message, [typed]() { SoundEditWindowShow(typed); });
			return false;
		}
	}

	return true;
}

static void SoundEditWindowShow(SoundEditTyped const &typed)
{
	auto const dx = [](int32_t u) { return (int32_t)(u * 3 / 2); };
	auto const dy = [](int32_t u) { return (int32_t)(u * 13 / 8); };
	host::SoundEditShown const shown = host::SoundEdit_Shown(soundEditWindow.record);
	host::DialogFormSpec spec;

	spec.title = "Sound Edit - " + soundEditWindow.folder;   // 0x10006b5a: "Sound Edit - %s" with the folder
	spec.width = dx(245) + 10;
	spec.height = dy(178) + 10;

	auto const add = [&spec](host::DialogWidgetType kind, std::string const &words, int32_t x, int32_t y, int32_t w, int32_t h, int32_t answer, size_t most, bool enabled)
	{
		host::DialogWidget one;

		one.type = kind;
		one.text = words;
		one.box = { x, y, w, h };
		one.answer = answer;
		one.most = most;
		one.enabled = enabled;
		spec.widgets.push_back(one);
	};

	add(host::DialogWidgetType::Label, "Wav file name", dx(13), dy(9), dx(60), 16, 0, 10, true);
	add(host::DialogWidgetType::Combo, typed.name, dx(13), dy(25), dx(216), 18, 0, 255, true);
	spec.widgets.back().items = soundEditWindow.wavs;
	spec.widgets.back().selected = typed.picked;
	spec.widgets.back().editable = true;
	spec.widgets.back().listRows = host::DialogBox_ComboListRows(121);   // 1019: 121 dialog units high with its list (dialog_box.h)
	add(host::DialogWidgetType::Label, "Inner Range (m)", dx(13), dy(44), dx(60), 16, 0, 10, true);
	add(host::DialogWidgetType::Edit, typed.fields[0], dx(15), dy(55), dx(63), 18, 0, 20, true);
	add(host::DialogWidgetType::Label, "Outer Range (m)", dx(13), dy(76), dx(60), 16, 0, 10, true);
	add(host::DialogWidgetType::Edit, typed.fields[1], dx(15), dy(90), dx(63), 18, 0, 20, true);
	add(host::DialogWidgetType::Label, "Pitch +/- semitones", dx(91), dy(44), dx(70), 16, 0, 10, true);
	add(host::DialogWidgetType::Edit, typed.fields[2], dx(91), dy(56), dx(63), 18, 0, 20, true);
	add(host::DialogWidgetType::Label, "Volume %", dx(91), dy(76), dx(62), 16, 0, 10, true);
	add(host::DialogWidgetType::Edit, typed.fields[3], dx(91), dy(88), dx(63), 18, 0, 20, true);
	add(host::DialogWidgetType::Check, std::string(), dx(90), dy(115), 12, 12, 0, 10, shown.playingAtStartLive);
	spec.widgets.back().checked = typed.playingAtStart;
	add(host::DialogWidgetType::Label, "Playing at Start", dx(90) + 16, dy(115) - 2, dx(60), 16, 0, 10, true);
	add(host::DialogWidgetType::Check, std::string(), dx(90), dy(129), 12, 12, 0, 10, shown.loopsLive);
	spec.widgets.back().checked = typed.loops;
	add(host::DialogWidgetType::Label, "Sound Loops", dx(90) + 16, dy(129) - 2, dx(60), 16, 0, 10, true);
	add(host::DialogWidgetType::Button, "Play", dx(175), dy(85), dx(50), 20, kSoundEditPlay, 10, true);
	add(host::DialogWidgetType::Button, "OK", dx(35), dy(155), dx(50), 20, host::kDialogOk, 10, true);
	add(host::DialogWidgetType::Button, "Cancel", dx(170), dy(155), dx(50), 20, host::kDialogEscape, 10, true);

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		SoundEditTyped const now = SoundEditTypedOf(widgets);

		if (answer == kSoundEditPlay)
		{
			// Play (0x10007420 -> 0x10006db0 -> 0x10007060): the fields read first; the file is the PICKED row's (none picked: nothing); what the last Play started stops; it plays in 2D at the
			// pitch, looping when Sound Loops is ticked; then the window again. NAMED (placed_sounds_game.h): through the game's loader and sound code, the volume as the game's 0..127
			float values[4];

			if (SoundEditRead(now, values))
			{
				if (now.picked >= 0 && (size_t)now.picked < soundEditWindow.wavs.size())
				{
					std::string const &wav = soundEditWindow.wavs[(size_t)now.picked];
					bool const playing = host::SoundGame_PreviewPlay(wav, (int32_t)(values[3] * 0.01f * 127.0f), (int32_t)(values[2] * 128.0f), now.loops);

					Answer("Sound Edit: Play " + wav + (playing ? ": playing" : (host::SoundGame_DeviceOn() ? ": the game did not load it" : ": no sound device")));
				}
				else
				{
					Answer("Sound Edit: Play: no wav is picked in the list, nothing plays");
				}

				SoundEditWindowShow(now);
			}

			return;
		}

		if (answer != host::kDialogOk)
		{
			// Cancel (or Escape): the record as it was; the window closing stops what Play started (0x10007010)
			host::SoundGame_PreviewStop();

			std::function<void(host::SoundEditRecord const &, bool)> const done = soundEditWindow.done;

			if (done)
			{
				done(soundEditWindow.record, false);
			}

			return;
		}

		float values[4];

		if (!SoundEditRead(now, values))
		{
			return;
		}

		host::SoundEdit_Ok(&soundEditWindow.record, now.name, values, now.playingAtStart, now.loops);
		host::SoundGame_PreviewStop();   // the window closes (0x10007010)

		std::function<void(host::SoundEditRecord const &, bool)> const done = soundEditWindow.done;

		if (done)
		{
			done(soundEditWindow.record, true);
		}
	});
}

// Opens the window for the record; `done` gets the record after OK (changed) or Cancel (as it was), and whether it was OK (only Track Editing's new sound asks, TB-T1). The list (Bishop's ruling 11-16; the sound import's, 11-27: both, sound_edit.h SoundEdit_MergeNames): the raw folder's wavs when the folder is there
// (edobtype's finder, 1:1), else the wavs the game's loader holds in its fastfiles under "sound/<the level's sound directory>/" in the finder's order (NAMED, sound_edit.h)
void SoundEditWindowOpen(host::SoundEditRecord const &record, std::string const &folder, std::string const &soundDirectory, std::function<void(host::SoundEditRecord const &, bool)> const &done)
{
	DWORD const folderIs = GetFileAttributesA(folder.c_str());
	bool const rawFolder = folderIs != INVALID_FILE_ATTRIBUTES && (folderIs & FILE_ATTRIBUTE_DIRECTORY) != 0;
	std::string const prefix = "sound/" + soundDirectory + "/";

	soundEditWindow.record = record;
	soundEditWindow.folder = folder;
	soundEditWindow.wavs = host::SoundEdit_MergeNames(rawFolder ? host::SoundEdit_WavNames(folder) : std::vector<std::string>(),
		host::SoundEdit_WavNamesFromFiles(prefix, host::SoundGame_FastFileNames(prefix)));
	soundEditWindow.done = done;
	host::SoundGame_UseSilence(editorHiddenRun);
	host::SoundGame_UseSoundDirectory(soundDirectory);

	host::SoundEditShown const shown = host::SoundEdit_Shown(record);
	SoundEditTyped typed;

	typed.name = record.name;

	// CB_SELECTSTRING(-1, name) (0x10006ae8): the first row that BEGINS with the name, any capitals, is picked and shown
	std::string lowerName;

	for (char ch : record.name)
	{
		lowerName += (char)tolower((unsigned char)ch);
	}

	for (size_t row = 0; row < soundEditWindow.wavs.size(); row++)
	{
		std::string lowerRow;

		for (char ch : soundEditWindow.wavs[row])
		{
			lowerRow += (char)tolower((unsigned char)ch);
		}

		if (lowerRow.compare(0, lowerName.size(), lowerName) == 0)
		{
			typed.picked = (int32_t)row;
			typed.name = soundEditWindow.wavs[row];
			break;
		}
	}

	for (int i = 0; i < 4; i++)
	{
		typed.fields[i] = shown.fields[i];
	}

	typed.playingAtStart = shown.playingAtStart;
	typed.loops = shown.loops;
	SoundEditWindowShow(typed);
}

// Object > Edit / E on a placed SOUND (0x4ed060): the record from the sound (inteng's units: the file's x 100 x the scale, FUN_004ecd40 0x4ecddf), the window, and after OK OR Cancel alike the
// numbers back through the floats (0x4ed20c-0x4ed290) and into the file's units (the save 0x4ed4f8: ftol(mem / unit)); they also become the next new sound's (DAT_005e7a60 .., OP-30).
void PlacedSoundEdit()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<host::SpecialObject> all;

	if (!open || !open->Level() || host::SpecialObjects_Read(open, &all) != host::SpecialResult::Done)
	{
		Answer("Sound Edit: no level is held to work on");
		return;
	}

	host::SpecialObject const *sound = nullptr;

	for (host::SpecialObject const &one : all)
	{
		sound = one.kind == host::SpecialKind::Sound && one.number == specialMode.number ? &one : sound;
	}

	if (!sound)
	{
		Answer("Sound Edit: the current sound is not in the level any more");
		return;
	}

	double const unit = 100.0 * (local_scale > 0.0 ? local_scale : 1.0);
	auto const ftol = [](double v) { return !(v == v) ? 0 : (v >= 2147483647.0 ? 2147483647 : (v <= -2147483648.0 ? (int32_t)-2147483647 - 1 : (int32_t)v)); };
	uint32_t const inner = (uint32_t)ftol((double)(uint32_t)sound->inner * unit);
	uint32_t const outer = (uint32_t)ftol((double)(uint32_t)sound->outer * unit);
	host::SoundEditRecord const record = host::SoundEdit_Record(sound->wav, inner, outer, sound->pitch, sound->volume, (uint32_t)sound->soundFlags, sound->probability);
	std::string const soundDirectory = host::TrackLevelSoundDirectory(open);
	std::string const folder = std::string(theGameFolder) + "\\Sound\\" + soundDirectory + "\\";
	size_t const number = sound->number;

	// 0x4ed13e-0x4ed1ac: the sound stops and its wav is let go before the window (placed_sounds_game.h)
	host::PlacedSounds_EditStart(number);

	SoundEditWindowOpen(record, folder, soundDirectory, [number, unit, ftol](host::SoundEditRecord const &back, bool)
	{
		OpenLevelFile *const file = LevelObjects_OpenForEdit();
		std::vector<host::SpecialObject> again;

		if (!file || !file->Level() || host::SpecialObjects_Read(file, &again) != host::SpecialResult::Done)
		{
			return;
		}

		int32_t memInner = 0, memOuter = 0, pitch = 0, volume = 0, probability = 0;
		uint32_t flags = 0;

		host::SoundEdit_Back(back, &memInner, &memOuter, &pitch, &volume, &flags, &probability);

		for (host::SpecialObject &one : again)
		{
			if (one.kind != host::SpecialKind::Sound || one.number != number)
			{
				continue;
			}

			unsigned const generation = file->Generation();

			one.wav = back.name;
			one.inner = ftol((double)(uint32_t)memInner / unit);
			one.outer = ftol((double)(uint32_t)memOuter / unit);
			one.pitch = pitch;
			one.volume = volume;
			one.soundFlags = (int32_t)flags;
			one.probability = probability;

			host::SpecialResult const written = host::SpecialObject_Write(file, one);

			// 0x4ed295-0x4ed32b: its wav loaded by the (new) name and, when bit 0 is clear, played as when made (placed_sounds_game.h)
			std::vector<host::PlacedSoundIn> const playedNow = PlacedSoundsOfLevel(std::vector<host::SpecialObject>(1, one));

			if (!playedNow.empty())
			{
				host::PlacedSounds_EditEnd(number, playedNow[0]);
			}

			// 0x4ed212-0x4ed290: every number becomes the next new sound's
			soundDefaults.set = true;
			soundDefaults.innerMm = (double)(uint32_t)memInner / 100.0;
			soundDefaults.outerMm = (double)(uint32_t)memOuter / 100.0;
			soundDefaults.volume = volume;
			soundDefaults.pitch = pitch;
			soundDefaults.flags = (int32_t)flags;
			soundDefaults.probability = probability;

			Answer(written == host::SpecialResult::Done ? "Sound Edit: " + back.name + " written" : std::string("Sound Edit: not written: ") + host::SpecialWords(written));

			if (file->Generation() != generation)
			{
				LoadTheObjectListAgain();
			}

			return;
		}
	});
}
