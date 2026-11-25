// texture_submodes.cpp: the Texture Map's SUB-MODE frames' own command switches (inteng's FUN_00551f80 the texture window, FUN_005511f0 Merge, FUN_00552bb0 multiple nodes:
// each reads the menu object's command itself, DAT_00914d28 = vtable +4). TextureMapCommand hands a sub-mode's commands here first (textureSubMode, D1 of the TM-A / TM-B split, Bishop's
// ruling 11-17); what is not taken here goes on to TextureMapCommand's own switch. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
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
#include "multinode.h"        // TM-B sub-mode 3
#include "texture_window.h"   // the window's layout: where a node is on the screen (TM-B T)
#include "texture_quad.h"
#include "texture_shared.h"
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

#include "mishchnk.hpp"        // File_Chunk::list_objects
#include "chunk_object.hpp"
#include "shape_model.h"

#include "editor_shared.h"

// ---- MERGE, sub-mode 2 (TM-B 2a / 2b; the facts in AvP-docs/engine/inteng/reassess/tm-b-merge-map.md) ----
// Mode > Merge (0x4, FUN_0054caf0(2) at 0x54d1fb) starts it on bar page 2. A left click picks a shape (FUN_00559690), drawn RED; M views its merged polygons (FUN_00559d20: every other shape
// hidden, each polygon in a colour no neighbour has, a pair in one colour); a click picks a triangle (and with it its partner), a second click a second triangle that FUN_0055a360 lets pair with
// the first (ours: PolyEdit_Joinable, its own test); B pairs the two picks or, a pair picked, parts it; X stops viewing (FUN_0055a070). The pairing is the working copy of the shape's SHPMRGDT
// numbers (DAT_00914d3c), written only by the save question (2c). Each polygon picked shows "%d\tverts: %d,%d,%d" at (0x10, 0x20) / (0x10, 0x30), the shape's name at (0x10, 0x40).
// NAMED: inteng recolours the shape's own polygons (+0xc, flat kind 2); ours draws each polygon again as world triangles 2 mm out along its normal, in its colour, over the shape (the shape stays
// drawn, so a click still meets it). Ours works on the true-colour table (DAT_00d876d8 set: the editor always draws in 32 bits).
namespace
{
	bool mergeShapeChosen = false;           // DAT_00914c00 != -1
	uint8_t mergeId[8] = {};                 // the shape's object (a module's own object for a module)
	bool mergeIsModule = false;
	int32_t mergeModuleIndex = -1;           // the game's module number, for a module
	bool mergeViewing = false;               // DAT_005fd688
	int32_t mergeFirst = -1;                 // DAT_005fd680
	int32_t mergeSecond = -1;                // DAT_005fd684
	std::vector<int32_t> mergeWorking;       // DAT_00914d3c[shape] while viewing: per polygon its partner, or -1
	std::vector<int32_t> mergeOld;           // DAT_00914d40: the numbers when M was pressed
	bool mergeHadChunk = false;              // the shape has its SHPMRGDT
	bool mergeChanged = false;               // DAT_00914d18
	std::vector<uint32_t> mergeColours;      // each polygon's colour while viewing
	model::ShapeModel mergeModel;            // the shape as the level holds it
	std::vector<int32_t> mergeWorld;         // its points where the game puts them (FilePick_World: inteng's FUN_0055a900)
	std::vector<int32_t> mergeNormals;       // its polygons' normals, 65536ths, turned the same way
	bool mergeHiding = false;                // the other shapes are hidden now
	bool mergeTinted = false;                // the picked shape is drawn red now

	uint32_t const kMergeTable[6] = { 0x0000ffu, 0x00ff00u, 0xffff00u, 0xff0000u, 0xff7f00u, 0x7f007fu };   // DAT_00914bc8, true colour (FUN_00544510 0x544593-0x544601)
	uint32_t const kMergePicked = 0x7f007fu;     // a picked polygon: (-(DAT_00d876d8 != 0) & 0x7eff80) + 0xff
	uint32_t const kMergeShapeRed = 0xff0000u;   // the picked shape: FUN_0046da70 with (-(DAT_00d876d8 != 0) & 0xfeff06) + 0xfa
	uint32_t const kMergeWhite = 0xffffffu;      // FUN_0046da70 with (-(DAT_00d876d8 != 0) & 0xffff07) + 0xf8

	Object_Chunk *MergeObjectWithId(File_Chunk *level, uint8_t const *id)
	{
		List<Object_Chunk *> all;

		level->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				return each();
			}
		}

		return nullptr;
	}

	int MergeSharedCorners(model::ShapePolygon const &a, model::ShapePolygon const &b)
	{
		int shared = 0;

		for (int i = 0; i < a.cornerCount && i < 5; i++)
		{
			for (int j = 0; j < b.cornerCount && j < 5; j++)
			{
				shared += a.corners[i] >= 0 && a.corners[i] == b.corners[j] ? 1 : 0;
			}
		}

		return shared;
	}

	int32_t MergePartner(int32_t polygon)
	{
		return polygon >= 0 && (size_t)polygon < mergeWorking.size() ? mergeWorking[(size_t)polygon] : -1;
	}

	// FUN_00559dd0(shape, p): the first colour of the table no NEIGHBOUR of p or of its partner has (a polygon sharing at least two corners with either, 0x559e33-0x559f07): blue, else the first free
	// of 1..4, else 5 (inteng's search reads a sixth flag past its five, the return address, which is never 0: 0x559fdb-0x559feb, so it ends at 5; the white stored at 0x55a009 is overwritten at once
	// by table[5], 0x55a00c); p and its partner get it
	void MergeColourOne(int32_t p)
	{
		size_t const count = mergeModel.polygons.size();

		if (p < 0 || (size_t)p >= count || mergeColours.size() != count)
		{
			return;
		}

		int32_t const q = MergePartner(p);
		bool blueTaken = false;
		bool taken[4] = { false, false, false, false };

		for (size_t j = 0; j < count; j++)
		{
			if ((int32_t)j == p || (int32_t)j == q)
			{
				continue;
			}

			bool const neighbour = MergeSharedCorners(mergeModel.polygons[(size_t)p], mergeModel.polygons[j]) >= 2 ||
				(q >= 0 && (size_t)q < count && MergeSharedCorners(mergeModel.polygons[(size_t)q], mergeModel.polygons[j]) >= 2);

			if (!neighbour)
			{
				continue;
			}

			for (int k = 0; k < 5; k++)
			{
				if (mergeColours[j] == kMergeTable[k])
				{
					if (k == 0)
					{
						blueTaken = true;
					}
					else
					{
						taken[k - 1] = true;
					}
				}
			}
		}

		int index = 0;

		if (blueTaken)
		{
			index = 1;

			while (index <= 4 && taken[index - 1])
			{
				index++;
			}
		}

		mergeColours[(size_t)p] = kMergeTable[index];

		if (q >= 0 && (size_t)q < count)
		{
			mergeColours[(size_t)q] = mergeColours[(size_t)p];
		}
	}

	// the shape read again from the level (its polygons, its points where the game puts them, its SHPMRGDT numbers into *numbers)
	bool MergeReadShape(std::vector<int32_t> *numbers)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return false;
		}

		mergeModel = model::ShapeModel();
		mergeWorld.clear();
		mergeNormals.clear();

		if (host::FilePick_World(open, open->Level(), mergeId, &mergeModel, &mergeWorld, &mergeNormals) != host::FilePickResult::Done)
		{
			return false;
		}

		if (numbers)
		{
			bool hasChunk = false;

			numbers->clear();

			if (!host::PolygonItems_ReadMergeData(open, open->Level(), mergeId, numbers, &hasChunk))
			{
				return false;
			}

			mergeHadChunk = hasChunk;

			if (!hasChunk || numbers->size() != mergeModel.polygons.size())
			{
				// a shape with no SHPMRGDT: inteng's start gave every level shape one of -1s (FUN_00544510 0x54489b)
				numbers->assign(mergeModel.polygons.size(), -1);
			}
		}

		return true;
	}

	void MergeLetGoOfTheView()
	{
		if (mergeHiding)
		{
			LevelView_HideModules(nullptr, 0, false);
			LevelView_HideObjects(nullptr, 0, false);
			mergeHiding = false;
		}

		if (mergeTinted)
		{
			LevelView_SetObjectTints(nullptr, nullptr, 0);
			LevelView_SetModuleTints(nullptr, nullptr, 0);
			mergeTinted = false;
		}
	}

	// FUN_0055a040: the view let go of (FUN_0055a070: every shape shown again, white), nothing viewed, no shape, no picks
	void MergeClear()
	{
		MergeLetGoOfTheView();
		mergeShapeChosen = false;
		mergeViewing = false;
		mergeFirst = mergeSecond = -1;
		mergeWorking.clear();
		mergeOld.clear();
		mergeColours.clear();
		mergeChanged = false;
	}

	// FUN_00559d20: the other shapes hidden, the shape's polygons coloured one by one in their order (each starts white)
	void MergeStartViewing()
	{
		mergeColours.assign(mergeModel.polygons.size(), kMergeWhite);

		for (size_t p = 0; p < mergeModel.polygons.size(); p++)
		{
			MergeColourOne((int32_t)p);
		}
	}

	std::string MergeVertsLine(int32_t polygon)
	{
		if (polygon < 0 || (size_t)polygon >= mergeModel.polygons.size())
		{
			return std::string();
		}

		model::ShapePolygon const &one = mergeModel.polygons[(size_t)polygon];
		char line[96];

		// "%d\tverts: %d,%d,%d" (0x5fec58): inteng's corner words are point numbers x 3 (an offset into the point list), divided by 3 at 0x551845: the point numbers
		_snprintf_s(line, sizeof(line), _TRUNCATE, "%d\tverts: %d,%d,%d", (int)polygon, (int)one.corners[0], (int)one.corners[1], (int)one.corners[2]);
		return line;
	}

	std::string MergeShapeName()
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		Object_Chunk *const object = open && open->Level() ? MergeObjectWithId(open->Level(), mergeId) : nullptr;

		// strncpy(buf, name, 0x1e) (0x55192b): at most 30 letters
		return object ? std::string(object->object_data.o_name, strnlen(object->object_data.o_name, 30 < sizeof(object->object_data.o_name) ? 30 : sizeof(object->object_data.o_name))) : std::string();
	}

	void MergeSay();

	// FUN_0055b900(shape): THE SAVE QUESTION, at X, at Polygon (2) and at Exit (5) while viewing, only when B changed something (DAT_00914d18, cleared before it asks: 0x55b94c). Fake Merge on
	// (DAT_00914d4c): the box is made and never shown, as a No. Yes (3): the pairing written into the shape's SHPMRGDT and its SHPMORPH / ASALTTEX sub-shapes (PolyEdit_SetMergeData), then the
	// level file (FUN_004719e0 at 0x55bf68: SaveAtIntengsPoint). No: the numbers as they were (0x55c016). then() runs after the answer. A shape other objects use too asks first which of them take
	// the change (FUN_0055aa50, TM-B 2d: MergeAskTheUsers).
	std::string mergeSaveWords;

	// The pairing written: in place (every user of the shape takes it) or, with movers, on a copy only they use (FUN_0055aa50's copy path); then the level file (0x55bf68)
	void MergeWrite(std::vector<std::string> const *movers)
	{
		std::string why;
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		host::PolyMergeResult result = host::PolyMergeResult::BadCall;
		size_t parted = 0;   // FUN_0055d500's checks (TM-B 2e): pairs whose polygons don't match are written parted (M reads them from the level again)

		if (open)
		{
			result = movers ? host::PolyEdit_SetMergeDataOnCopy(open, mergeId, *movers, mergeWorking, &why, &parted) : host::PolyEdit_SetMergeData(open, mergeId, mergeWorking, &why, &parted);
		}

		if (result == host::PolyMergeResult::Done)
		{
			LoadTheObjectListAgain();
			mergeOld = mergeWorking;

			std::string const saved = SaveAtIntengsPoint();

			mergeSaveWords = std::string("Texture Map: Merge: the shape's merged polygons were written") + (movers ? " on a copy of the shape for " + std::to_string(movers->size()) + " objects" : "") +
				(parted ? " (" + std::to_string(parted) + " pairs parted: their polygons did not match)" : "") + "; " + saved;
		}
		else
		{
			mergeWorking = mergeOld;
			mergeSaveWords = std::string("Texture Map: Merge: not written: ") + host::PolyMergeWords(result) + (why.empty() ? std::string() : " (" + why + ")");
		}
	}

	// FUN_0055aa50 / FUN_0055b4c0: the shape is used by other objects too: "The shape associated with %s\nis used by some other objects.\nWhich of the other objects would you like the\nchanges applied
	// to?" (0x600af0, the current object's name), 600 x 400, Ok (100, 370) and All (200, 370) (View Obj / Skip Obj / Skip All are only in sub-mode 0), a check box per other user in the list's order,
	// unticked, 10 a column, columns 200 apart. All, or Ok with every box ticked: the change for every user (in place). Ok with any unticked, or Escape (every box unticked): the current object and
	// the ticked ones get a copy of the shape (FUN_004d1090 / FUN_0050e880 / FUN_004d0ff0); the others keep it as it was.
	void MergeAskTheUsers(std::vector<std::string> const &others, std::string const &current, std::function<void()> const &then)
	{
		host::DialogFormSpec spec;
		host::DialogWidget text, ok, all;

		spec.width = 600;
		spec.height = 400;
		text.type = host::DialogWidgetType::Label;
		text.box = { 18, 18, 370, 100 };
		text.text = "The shape associated with " + current + "\nis used by some other objects.\nWhich of the other objects would you like the\nchanges applied to?";
		ok.type = host::DialogWidgetType::Button;
		ok.box = { 100, 370, 50, 16 };
		ok.text = "Ok";
		ok.answer = 3;
		all.type = host::DialogWidgetType::Button;
		all.box = { 200, 370, 50, 16 };
		all.text = "All";
		all.answer = 4;
		spec.widgets.push_back(text);
		spec.widgets.push_back(ok);
		spec.widgets.push_back(all);

		for (size_t i = 0; i < others.size(); i++)
		{
			host::DialogWidget check, name;
			int32_t const x = (int32_t)(i / 10) * 200, y = (int32_t)(i % 10 + 6) * 20;

			check.type = host::DialogWidgetType::Check;
			check.box = { x + 18, y, 16, 16 };
			name.type = host::DialogWidgetType::Label;
			name.box = { x + 40, y, 100, 18 };
			name.text = others[i].substr(0, 30);   // strncpy(buf, name, 30)
			spec.widgets.push_back(check);
			spec.widgets.push_back(name);
		}

		host::Dialogs_Form(spec, [others, current, then](int32_t answer, std::vector<host::DialogWidget> const &widgets)
		{
			std::vector<std::string> ticked;

			for (size_t i = 0, row = 0; i < widgets.size(); i++)
			{
				if (widgets[i].type == host::DialogWidgetType::Check)
				{
					if (answer == 3 && widgets[i].checked && row < others.size())
					{
						ticked.push_back(others[row]);   // Escape (1) and the others: every box unticked
					}

					row++;
				}
			}

			if (answer == 4 || (answer == 3 && ticked.size() == others.size()))
			{
				MergeWrite(nullptr);
			}
			else
			{
				std::vector<std::string> movers(1, current);

				movers.insert(movers.end(), ticked.begin(), ticked.end());
				MergeWrite(&movers);
			}

			then();
		});
	}

	void MergeSaveQuestion(std::function<void()> const &then)
	{
		mergeSaveWords.clear();   // only this question's words are said after it

		if (statusWords.From(mergeSaveWords.c_str()))
		{
			statusWords = mergeSaveWords.c_str();   // the status line showed them: it is blank now, as when it pointed at them (status_words.h)
		}

		if (!mergeChanged)
		{
			then();
			return;
		}

		mergeChanged = false;

		if (TextureFakeMergeOn())
		{
			mergeWorking = mergeOld;
			then();
			return;
		}

		DialogConfirm("Would you like to save the changes\nto this shape?.\nIf you don't the changes will be lost.", [then](bool yes)   // 0x600b98, inteng's own "?."
		{
			if (!yes)
			{
				mergeWorking = mergeOld;
				mergeSaveWords = "Texture Map: Merge: the changes were not saved";
				then();
				return;
			}

			// FUN_0055aa50: a shape with fewer than two users is written at once; else its other users are asked about first
			std::vector<std::string> users;
			OpenLevelFile *const open = LevelObjects_OpenForEdit();
			const std::string current = MergeShapeName();   // const first: Snyk Code's parser (a lambda given to a call)

			if (!open || !host::PolyEdit_ShapeUserNames(open, mergeId, &users) || users.size() < 2)
			{
				MergeWrite(nullptr);
				then();
				return;
			}

			std::vector<std::string> others;
			bool skipped = false;

			for (std::string const &one : users)
			{
				if (!skipped && one == current)
				{
					skipped = true;   // the current object is not a row (0x55ab5a-0x55abac)
					continue;
				}

				others.push_back(one);
			}

			MergeAskTheUsers(others, current, then);
		});
	}

	void MergeSay()
	{
		static std::string words;

		if (!mergeShapeChosen)
		{
			words = "Texture Map: Merge: click a shape, then M views its merged polygons";
		}
		else if (!mergeViewing)
		{
			words = "Texture Map: Merge: " + MergeShapeName() + " (M views its merged polygons)";
		}
		else
		{
			size_t pairs = 0;

			for (size_t p = 0; p < mergeWorking.size(); p++)
			{
				pairs += mergeWorking[p] > (int32_t)p ? 1 : 0;
			}

			words = "Texture Map: Merge: viewing " + MergeShapeName() + ", " + std::to_string(pairs) + " merged pairs; picked " + std::to_string(mergeFirst) + " " + std::to_string(mergeSecond) +
				(mergeChanged ? " (changed)" : "") + " (B merges or unmerges, X stops)";
		}

		statusWords = words.c_str();
	}
}

bool TextureMergeOn()
{
	return textureSubMode == 2;
}

// Mode > Merge (0x4): FUN_0054caf0(2) at 0x54d1fb: bar page 2 with Merge ticked, the polygons' own save question FUN_0055c070(0) (Frost's, TO BUILD: until it is in, nothing is asked here),
// the polygon selection cleared (FUN_0056e0a0), sub-mode 2, FUN_0055a040
void TextureMergeEnter()
{
	if (textureWindowOn)
	{
		TextureWindowToggle();   // the texture window's own leaving first (its bars, its drag)
	}

	TextureMapDeselectAll();
	textureSubMode = 2;
	MergeClear();
	EditorBar_SetPage(2);

	if (!LoadTheMenus())
	{
		LogString("menus: the bars of Texture Map's Merge couldn't be made");
	}

	MergeSay();
}

// Exit (5) while viewing (FUN_005511f0 0x551269-0x551279): the save question before the mode is left; true while it asks (the Exit runs again after the answer, with nothing changed)
bool TextureMergeHoldsTheExit()
{
	if (!TextureMergeOn() || !mergeViewing || !mergeChanged)
	{
		return false;
	}

	MergeSaveQuestion([]()
	{
		ExitTheMode();
	});

	return true;
}

// The screen left, or sub-mode 0 chosen: everything Merge holds let go (the save question comes first where inteng asks it: 2c)
void TextureMergeLeave()
{
	MergeClear();

	if (textureSubMode == 2)
	{
		textureSubMode = 0;
	}
}

// The keys FUN_005511f0 reads straight from the key slots (not the key table): M 0xd87a8d, X 0xd87a98, B 0xd87a82, each with no other condition than the state's
bool TextureMergeKey(uint8_t key)
{
	if (!TextureMergeOn())
	{
		return false;
	}

	if (key == 'M')
	{
		// 0x551368: only when not viewing and a shape is picked: the working numbers a copy, the old kept, the view started (FUN_00559d20), changed 0, viewing, picks -1
		if (!mergeViewing && mergeShapeChosen)
		{
			std::vector<int32_t> numbers;

			if (!MergeReadShape(&numbers))
			{
				statusWords = "Texture Map: Merge: the shape could not be read";
				return true;
			}

			mergeOld = numbers;
			mergeWorking = numbers;
			mergeChanged = false;
			mergeViewing = true;
			mergeFirst = mergeSecond = -1;
			MergeStartViewing();
			MergeSay();
		}

		return true;
	}

	if (key == 'X')
	{
		// 0x551451: only while viewing: the save question (FUN_0055b900: 2c), viewing 0, FUN_0055a070, the shape red again, picks -1
		if (mergeViewing)
		{
			MergeSaveQuestion([]()
			{
				mergeViewing = false;
				mergeFirst = mergeSecond = -1;
				mergeColours.clear();
				MergeLetGoOfTheView();
				MergeSay();

				if (!mergeSaveWords.empty())
				{
					statusWords = mergeSaveWords.c_str();   // the save's words win over the plain ones
				}
			});
		}

		return true;
	}

	if (key == 'B')
	{
		// 0x5514c2: viewing with both picks: the first unpaired: the two paired; else both parted; both recoloured, picks -1, changed
		if (mergeViewing && mergeFirst >= 0 && mergeSecond >= 0 && (size_t)mergeFirst < mergeWorking.size() && (size_t)mergeSecond < mergeWorking.size())
		{
			if (mergeWorking[(size_t)mergeFirst] == -1)
			{
				mergeWorking[(size_t)mergeFirst] = mergeSecond;
				mergeWorking[(size_t)mergeSecond] = mergeFirst;
			}
			else
			{
				mergeWorking[(size_t)mergeFirst] = -1;
				mergeWorking[(size_t)mergeSecond] = -1;
			}

			MergeColourOne(mergeFirst);
			MergeColourOne(mergeSecond);
			mergeFirst = mergeSecond = -1;
			mergeChanged = true;
			MergeSay();
		}

		return true;
	}

	return false;
}

// A left click in sub-mode 2 (FUN_005511f0 0x5512cd-0x551307 not viewing, 0x551571-0x551817 viewing). polygon: the polygon of `id`'s shape under the click (FilePick_Object), -1 for none
void TextureMergeClicked(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon)
{
	if (!mergeViewing)
	{
		// FUN_00559690: the shape under the pointer; a new one (not the one picked) is picked: the old one white again, the new one red
		if (mergeShapeChosen && memcmp(id, mergeId, 8) == 0)
		{
			return;
		}

		MergeLetGoOfTheView();
		memcpy(mergeId, id, 8);
		mergeIsModule = isModule;
		mergeModuleIndex = moduleIndex;
		mergeShapeChosen = true;
		MergeSay();
		return;
	}

	// viewing: only the shape is drawn (the others hidden), so a click elsewhere meets nothing (FUN_005596b0 gives -1)
	if (memcmp(id, mergeId, 8) != 0 || polygon < 0 || (size_t)polygon >= mergeWorking.size())
	{
		if (mergeFirst == -1)
		{
			MergeSay();   // 0x5515ac: no polygon, nothing more this frame
		}

		return;
	}

	if (mergeFirst == -1)
	{
		// 0x55157e: the first pick, and its partner with it
		mergeFirst = polygon;
		mergeColours[(size_t)mergeFirst] = kMergePicked;
		mergeSecond = MergePartner(mergeFirst);

		if (mergeSecond != -1)
		{
			mergeColours[(size_t)mergeSecond] = kMergePicked;
		}
	}
	else if (mergeSecond == -1)
	{
		// 0x55160a: the second pick (not the first itself)
		if (polygon != mergeFirst)
		{
			mergeSecond = polygon;
			mergeColours[(size_t)mergeSecond] = kMergePicked;

			if (MergePartner(mergeSecond) == -1)
			{
				// 0x5516fd: FUN_0055a360(shape, first, second, 0) must let the two pair; else the first is recoloured and the clicked one is the first
				if (!host::PolyEdit_Joinable(mergeModel, mergeFirst, mergeSecond, false, nullptr))
				{
					MergeColourOne(mergeFirst);
					mergeFirst = mergeSecond;
					mergeSecond = -1;
				}
			}
			else
			{
				// 0x5516a2: the clicked one is paired: the first recoloured, the clicked one's partner is the first (so the pair is picked)
				MergeColourOne(mergeFirst);
				mergeFirst = MergePartner(mergeSecond);
				mergeColours[(size_t)mergeFirst] = kMergePicked;
			}
		}
	}
	else if (polygon != mergeFirst && polygon != mergeSecond)
	{
		// 0x551739: both picked and a click on another: both recoloured, and the clicked one (with its partner) the new pick
		MergeColourOne(mergeFirst);
		MergeColourOne(mergeSecond);
		mergeFirst = polygon;
		mergeSecond = -1;
		mergeColours[(size_t)mergeFirst] = kMergePicked;

		if (MergePartner(mergeFirst) != -1)
		{
			mergeSecond = MergePartner(mergeFirst);
			mergeColours[(size_t)mergeSecond] = kMergePicked;
		}
	}

	MergeSay();
}

// Each frame (editor_main): the view as the sub-mode wants it: not viewing, the picked shape red; viewing, every other shape hidden. *triangles gets the shape's polygons in their colours
// (true while viewing)
bool TextureMergeFrame(std::vector<LevelViewTriangle> *triangles)
{
	if (!TextureMergeOn() || !onTextureMap)
	{
		if (mergeShapeChosen || mergeHiding || mergeTinted)
		{
			MergeLetGoOfTheView();
		}

		return false;
	}

	if (mergeShapeChosen && !mergeViewing && !mergeTinted)
	{
		uint32_t const red = 0xFF000000u | kMergeShapeRed;

		if (mergeIsModule)
		{
			LevelView_SetModuleTints(&mergeModuleIndex, &red, 1);
		}
		else
		{
			LevelView_SetObjectTints(mergeId, &red, 1);
		}

		mergeTinted = true;
	}

	if (!mergeViewing)
	{
		if (mergeHiding)
		{
			LevelView_HideModules(nullptr, 0, false);
			LevelView_HideObjects(nullptr, 0, false);
			mergeHiding = false;
		}

		return false;
	}

	if (mergeTinted)
	{
		LevelView_SetObjectTints(nullptr, nullptr, 0);
		LevelView_SetModuleTints(nullptr, nullptr, 0);
		mergeTinted = false;
	}

	if (!mergeHiding)
	{
		// FUN_00559d20: FUN_0046dd20(obj, 0) on every other slot: every module but the shape's, every placed object but the shape's
		std::vector<int32_t> modules;
		std::vector<std::vector<uint8_t>> objects;
		std::map<int32_t, int32_t> numbers;
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (open && open->Level())
		{
			host::GameModuleNumbers(open->Level(), &numbers);
		}

		for (auto const &one : numbers)
		{
			if (!(mergeIsModule && one.second == mergeModuleIndex))
			{
				modules.push_back(one.second);
			}
		}

		LevelView_ActiveObjectIds(&objects);

		std::vector<uint8_t> ids;

		for (std::vector<uint8_t> const &one : objects)
		{
			if (one.size() == 8 && (mergeIsModule || memcmp(one.data(), mergeId, 8) != 0))
			{
				ids.insert(ids.end(), one.begin(), one.end());
			}
		}

		LevelView_HideModules(modules.data(), modules.size(), false);
		LevelView_HideObjects((const uint8_t (*)[8])ids.data(), ids.size() / 8, false);
		mergeHiding = true;
	}

	if (triangles)
	{
		triangles->clear();

		for (size_t p = 0; p < mergeModel.polygons.size() && p < mergeColours.size(); p++)
		{
			model::ShapePolygon const &one = mergeModel.polygons[p];
			int32_t const *normal = p * 3 + 2 < mergeNormals.size() ? &mergeNormals[p * 3] : nullptr;
			int32_t out[3] = { 0, 0, 0 };

			for (int axis = 0; axis < 3 && normal; axis++)
			{
				out[axis] = (int32_t)(((int64_t)normal[axis] * 2) / 65536);   // 2 mm out along the polygon's normal (65536ths)
			}

			for (int c = 1; c + 1 < one.cornerCount && c + 1 < 5; c++)
			{
				int32_t const corner[3] = { one.corners[0], one.corners[c], one.corners[c + 1] };
				LevelViewTriangle triangle;
				bool good = true;

				for (int k = 0; k < 3; k++)
				{
					good = good && corner[k] >= 0 && (size_t)corner[k] * 3 + 2 < mergeWorld.size();
				}

				if (!good)
				{
					continue;
				}

				for (int axis = 0; axis < 3; axis++)
				{
					triangle.a[axis] = mergeWorld[(size_t)corner[0] * 3 + axis] + out[axis];
					triangle.b[axis] = mergeWorld[(size_t)corner[1] * 3 + axis] + out[axis];
					triangle.c[axis] = mergeWorld[(size_t)corner[2] * 3 + axis] + out[axis];
				}

				triangle.colour = 0xFF000000u | mergeColours[p];
				triangles->push_back(triangle);
			}
		}
	}

	return true;
}

// The text FUN_005511f0 draws (0x551826-0x55194e, FUN_0057b1b0 at x 0x10): the first pick at 0x20, the second at 0x30, the shape's name at 0x40; (y, words) pairs
void TextureMergeLines(std::vector<std::pair<int32_t, std::string>> *lines)
{
	lines->clear();

	if (!TextureMergeOn())
	{
		return;
	}

	if (mergeFirst != -1 && mergeShapeChosen)
	{
		lines->push_back(std::make_pair(0x20, MergeVertsLine(mergeFirst)));
	}

	if (mergeSecond != -1 && mergeShapeChosen)
	{
		lines->push_back(std::make_pair(0x30, MergeVertsLine(mergeSecond)));
	}

	if (mergeShapeChosen)
	{
		lines->push_back(std::make_pair(0x40, MergeShapeName()));
	}
}

// The help texts of sub-mode 2: 0x27 FUN_00557390 (0x60029c), shown by FUN_004724f0 as the others are
char const *const kIntengMergeHelpText = "\t\t\tMouse Controls\n\nLeft click\t\tSelect polygon or shape.\n\n\t\t\tKeyboard Controls\n\nM\t\tView merged polygons for selected shape.\nX\t\tStop viewing merged polygons.\nB\t\tMerge two triangles or unmerge a quad.\nP\t\tShow\\Hide text\nCtrl+X\t\tExit texture mapper.";

// THE SUB-MODES' OWN COMMAND SWITCHES (D1): Merge's is FUN_005511f0's (0x55121c): 2 Polygon (and Tab) back to the polygons (FUN_0054caf0(0); while viewing, the save question first: 2c),
// 0x27 / 0x28 the help texts, 0x32 (P) the text shown or hidden; every other command does nothing in it (Merge 0x4 again included: its switch has no case for it). Exit (5) leaves through the
// editor's Exit (LeaveTextureMap lets Merge go). The texture window's commands (sub-mode 1) are still TextureMapCommand's own.
bool TextureMultiCommand(int32_t command);

bool TextureWindowCommand(int32_t command)
{
	if (textureSubMode == 3)
	{
		return TextureMultiCommand(command);
	}

	if (textureSubMode != 2)
	{
		return false;
	}

	switch (command)
	{
		case 0x2:
		{
			// FUN_0054caf0(0) first, then, while viewing, the save question (0x551225-0x551252)
			bool const ask = mergeViewing && mergeShapeChosen;

			textureSubMode = 0;
			EditorBar_SetPage(0);

			if (!LoadTheMenus())
			{
				LogString("menus: the Texture Map bars couldn't be made");
			}

			statusWords = "Texture Map: the polygons";

			if (ask)
			{
				MergeSaveQuestion([]()
				{
					MergeClear();

					if (!mergeSaveWords.empty())
					{
						statusWords = mergeSaveWords.c_str();
					}
				});
			}
			else
			{
				MergeClear();
			}

			return true;
		}
		case 0x27:
			textPanelText = kIntengMergeHelpText;
			textPanelNext = nullptr;
			return true;
		case 0x28:
			textPanelText = kIntengTextureMapMovementText;
			textPanelNext = nullptr;
			return true;
		case 0x32:
			textureTextShown = !textureTextShown;   // DAT_005fd69c, the polygon screen's P's own flag (texture_map_screen.cpp): FUN_00543fa0 draws the menus while it is set (FUN_0057b6f0 at 0x544048)
			statusWords = textureTextShown ? "Texture Map: Merge: the menus are shown" : "Texture Map: Merge: the menus are hidden (P shows them)";
			return true;
		default:
			return true;
	}
}

// ================================================================= SUB-MODE 3: THE MULTIPLE NODE MODE (TM-B; reassess/tm-b-multinode-map.md)
// M (0x3a, FUN_0054d260 0x54d5b4) turns All Nodes Shown (DAT_00914d74) on and off; Tab (0x3) with polygons selected then opens the multiple node mode in place of the texture window
// (0x54d2cf). Its entry (FUN_0054caf0 case 3, FUN_00573f20) joins the selected polygons' corners into nodes - a corner within 400 of a node on each axis is one of its members -
// links them along the polygons' edges and lines them up on the current picture (model/multinode.h); with that it shows the picture, the nodes and their links, bar page 3. A line-up
// that fails gives "Cannot use multiple node mode with currently selected polygons" (0x601b44) and the polygon screen. Polygon (2), Tab and Exit leave it (FUN_005604f0): every
// selected polygon's coordinates are brought onto the picture (FUN_005702e0 -> FUN_00566b90, TextureWrapPolygon).
// THE NODES MOVED (TM-B 3b; FUN_00553be0 0x55411e, FUN_005546b0, FUN_00554ca0, FUN_00555070): a press within 4 pixels of a node (both ways) makes it the current node and takes it; else
// a press within 5 pixels of a link takes both its nodes; else a press on the picture takes EVERY node; the drag moves the taken nodes with the mouse (one node: to the mouse), held
// in the texture area. With Selectable Nodes on (S, 0x102) the taken nodes stay taken after the release and a press with Shift draws a box that takes the nodes in it (one under 2
// pixels: the nearest node in or out); without it they are let go. Shift+press (Selectable Nodes off) puts the current node at the mouse. The arrow keys move the current node 1
// texel (Shift 10); with Ctrl every node, with Selectable Nodes the taken ones; held in the texture area. After each move every member corner takes its node's place
// (FUN_00560450, model::MultiNodeWriteBack) and the changed polygons are written.
// NAMED (3a/3b): the mode's own commands (flips and turns of the nodes, S / L / T, 0x105, Reset, the scales: 3c) do nothing yet. INTENG BUG, FIXED: the current node is 0 at
// every entry (inteng leaves +0x88 as it was, and an arrow key then writes past the nodes when there are fewer: 0x5551a5); a drag moves only the taken nodes (inteng makes every
// node's place again from its screen place, 0x554c26, so untouched nodes snap to the screen's steps at a scale that is not x1).
// DECIDED: the corners are measured where the game puts them (FilePick_World, as Merge's FUN_0055a900; inteng's DAT_00914c48 is not read); the shapes are taken in the order their
// first polygon was selected (inteng walks its shape slots in order); a selected polygon with no texture is first given the current texture and the window's
// coordinates (FUN_00570040, TextureMultiTextureTheUntextured); the "do not fit in the used area" box after the entry (FUN_0055cfa0) is not built.
bool textureAllNodesShown = false;   // DAT_00914d74

namespace
{
	struct MultiShape
	{
		uint8_t id[8];
		bool isModule;
		int32_t moduleIndex;
		model::ShapeModel model;
		std::vector<int32_t> world;   // its points where the game puts them, 3 per point
		std::vector<int32_t> merge;   // its merge data, one per polygon (-1s for a shape with none)
	};

	struct MultiRef
	{
		size_t shape;          // in multiShapes
		int32_t polygon;       // the polygon's number in the file's SHPPOLYS
		model::PolygonUv uv;   // as read at the entry
	};

	std::vector<MultiShape> multiShapes;
	std::vector<MultiRef> multiRefs;                    // one per multiPolygons entry
	std::vector<model::MultiNodePolygon> multiPolygons;
	std::vector<model::MultiNode> multiNodes;
	int32_t multiCurrent = 0;                           // state +0x88
	std::vector<int32_t> multiTaken;                    // +0x78, the taken nodes
	int multiDrag = 0;                                  // +0x74: 1 the taken nodes move, 0x10 a box
	int32_t multiPressX = 0, multiPressY = 0, multiLastX = 0, multiLastY = 0;
	bool multiSelectable = false;                       // +0x8c, Selectable Nodes (+0x90, Resizing Texture, is textureWin.resizing: one for both windows)

	// The polygons' coordinates as multiPolygons holds them now, written for each shape whose coordinates differ from the entry's
	bool MultiWrite(std::string *why)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return false;
		}

		for (size_t s = 0; s < multiShapes.size(); s++)
		{
			std::vector<model::PolygonUv> changed;

			for (size_t p = 0; p < multiPolygons.size(); p++)
			{
				if (multiRefs[p].shape != s)
				{
					continue;
				}

				model::PolygonUv uv = multiRefs[p].uv;
				bool differs = false;

				for (int c = 0; c < multiPolygons[p].corners && c < uv.uv.cornerCount; c++)
				{
					differs = differs || (int32_t)uv.uv.corners[c].u != multiPolygons[p].u[c] || (int32_t)uv.uv.corners[c].v != multiPolygons[p].v[c];
					uv.uv.corners[c].u = (float)multiPolygons[p].u[c];
					uv.uv.corners[c].v = (float)multiPolygons[p].v[c];
				}

				if (differs)
				{
					changed.push_back(uv);
				}
			}

			if (!changed.empty() && host::UvEdit_WritePolygons(open, open->Level(), multiShapes[s].id, changed, why) != host::UvEditResult::Done)
			{
				return false;
			}

			for (model::PolygonUv const &one : changed)
			{
				for (size_t p = 0; p < multiRefs.size(); p++)
				{
					if (multiRefs[p].shape == s && multiRefs[p].polygon == one.polygonNumber)
					{
						multiRefs[p].uv = one;
					}
				}
			}
		}

		return true;
	}

	// The place of polygon `polygon` of shape `shape` in the lists, read and added when it is not there yet (selected: one of the selection's; else a partner); -1 when it can't be read
	int32_t MultiPolygonOf(OpenLevelFile *open, size_t shape, int32_t polygon, bool selected)
	{
		for (size_t p = 0; p < multiRefs.size(); p++)
		{
			if (multiRefs[p].shape == shape && multiRefs[p].polygon == polygon)
			{
				multiPolygons[p].selected = multiPolygons[p].selected || selected;
				return (int32_t)p;
			}
		}

		MultiShape const &owner = multiShapes[shape];
		model::PolygonUv uv;
		std::string why;

		if (polygon < 0 || (size_t)polygon >= owner.model.polygons.size() ||
			host::UvEdit_ReadPolygon(open, open->Level(), owner.id, polygon, &uv, &why) != host::UvEditResult::Done || !uv.textured || uv.uv.cornerCount < 3)
		{
			return -1;
		}

		model::ShapePolygon const &item = owner.model.polygons[(size_t)polygon];
		model::MultiNodePolygon one = {};
		int corners = 0;

		while (corners < 5 && item.corners[corners] >= 0)
		{
			corners++;
		}

		if (corners < 3 || corners > 4 || uv.uv.cornerCount < corners)
		{
			return -1;
		}

		one.selected = selected;
		one.partner = -1;
		one.corners = corners;

		for (int c = 0; c < corners; c++)
		{
			size_t const at = (size_t)item.corners[c] * 3;

			if (at + 2 >= owner.world.size())
			{
				return -1;
			}

			one.x[c] = owner.world[at];
			one.y[c] = owner.world[at + 1];
			one.z[c] = owner.world[at + 2];
			one.u[c] = (int32_t)uv.uv.corners[c].u;   // whole texels, as inteng holds them
			one.v[c] = (int32_t)uv.uv.corners[c].v;
		}

		multiPolygons.push_back(one);
		multiRefs.push_back({ shape, polygon, uv });
		return (int32_t)multiPolygons.size() - 1;
	}

	void MultiClear()
	{
		multiShapes.clear();
		multiRefs.clear();
		multiPolygons.clear();
		multiNodes.clear();
		multiCurrent = 0;
		multiTaken.clear();
		multiDrag = 0;
	}

	void MultiTake(int32_t node)
	{
		if (node >= 0 && (size_t)node < multiNodes.size() && std::find(multiTaken.begin(), multiTaken.end(), node) == multiTaken.end())
		{
			multiTaken.push_back(node);
		}
	}
}

bool TextureCurrentPictureSize(int32_t *width, int32_t *height);   // texture_map_screen.cpp: the current texture's picture (state +0x58)

int32_t TextureCurrentPictureFor(OpenLevelFile *open, uint8_t const *id);   // texture_map_screen.cpp
void TextureCurrentEnsure(OpenLevelFile *open);
void TextureWindowCornerNodes(model::UvCorner corners[4], int *count);

// FUN_00570040 (the entry's first step, 0x573f44): every selected UNTEXTURED polygon is given the current texture (type 7) with the window's corner nodes as its coordinates
// (FUN_005589e0): a polygon with no merge partner, or one whose partner is not selected or is textured, takes nodes 1-3; a merged pair both selected and untextured takes the four
// nodes as one quad (FUN_0055cdf0) when the window has four (Select As Quad), else both take the texture with every coordinate 0 (0x570139-0x5701ef). The polygons are written, so
// the join that follows finds them textured. Gives how many were textured. NAMED: a quad of the file takes the four nodes; a shape whose own picture list has not the current
// texture is left as it is (inteng's copy holds the engine's texture number and its write-back is not read); inteng walks every shape slot from 0 (bug candidate G-11): ours the
// selection's shapes.
static size_t textureMultiUntexturedLeft = 0;   // untextured selected polygons the entry could not texture (their shape's list has not the current texture, or the write was refused)

static size_t TextureMultiTextureTheUntextured(OpenLevelFile *open)
{
	textureMultiUntexturedLeft = 0;
	model::UvCorner nodes[4];
	int count = 4;
	size_t textured = 0;
	std::vector<std::string> shapesDone;

	TextureWindowCornerNodes(nodes, &count);

	for (TexturePolygon const &lead : texturePolygons)
	{
		std::string const key((char const *)lead.id, 8);

		if (std::find(shapesDone.begin(), shapesDone.end(), key) != shapesDone.end())
		{
			continue;
		}

		shapesDone.push_back(key);

		model::ShapeModel shape;
		std::vector<int32_t> merge;
		bool hasChunk = false;
		std::string why;

		if (host::ShapeEdit_Read(open->Level(), open, lead.id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		if (!host::PolygonItems_ReadMergeData(open, open->Level(), lead.id, &merge, &hasChunk) || merge.size() != shape.polygons.size())
		{
			merge.assign(shape.polygons.size(), -1);
		}

		auto const untextured = [&shape](int32_t p) { return p >= 0 && (size_t)p < shape.polygons.size() && !model::EngineTypeIsTextured(shape.polygons[(size_t)p].engineType); };
		auto const selected = [&lead](int32_t p) { return TextureSelectedIndex(lead.id, p) < texturePolygons.size(); };

		// which polygons of this shape take the texture, and what each corner takes (-1: 0, 0)
		std::vector<int32_t> make;
		std::vector<std::pair<int32_t, std::vector<int>>> corners;   // polygon, the node each corner takes

		for (TexturePolygon const &one : texturePolygons)
		{
			if (memcmp(one.id, lead.id, 8) != 0 || !untextured(one.polygon) || std::find(make.begin(), make.end(), one.polygon) != make.end())
			{
				continue;
			}

			int32_t const p = one.polygon, partner = merge[(size_t)p];
			int own = 0;

			while (own < 5 && shape.polygons[(size_t)p].corners[own] >= 0)
			{
				own++;
			}

			if (partner < 0 || !selected(partner) || !untextured(partner))
			{
				std::vector<int> take;

				for (int c = 0; c < own && c < 4; c++)
				{
					take.push_back(c);   // nodes 1-3 (a quad of the file: 1-4)
				}

				make.push_back(p);
				corners.push_back(std::make_pair(p, take));
				continue;
			}

			model::TextureQuad quad;
			int32_t const lower = p < partner ? p : partner, higher = p < partner ? partner : p;

			make.push_back(lower);
			make.push_back(higher);

			if (count != 4 || model::TextureQuadOf(shape, lower, higher, &quad) != model::TextureQuadResult::Ok)
			{
				corners.push_back(std::make_pair(lower, std::vector<int>(3, -1)));   // 0x570162-0x5701ef: the texture, every coordinate 0
				corners.push_back(std::make_pair(higher, std::vector<int>(3, -1)));
				continue;
			}

			std::vector<int> lowerTakes(3, -1), higherTakes(3, -1);

			for (int k = 0; k < 4; k++)
			{
				if (quad.ownCorners[k] >= 0)
				{
					lowerTakes[(size_t)quad.ownCorners[k]] = k;
				}

				if (quad.partnerCorners[k] >= 0)
				{
					higherTakes[(size_t)quad.partnerCorners[k]] = k;
				}
			}

			corners.push_back(std::make_pair(lower, lowerTakes));
			corners.push_back(std::make_pair(higher, higherTakes));
		}

		int32_t const picture = make.empty() ? -1 : TextureCurrentPictureFor(open, lead.id);
		size_t made = 0;

		if (make.empty())
		{
			continue;
		}

		if (picture < 0 || host::PolyEdit_MakeTextured(open, lead.id, make.data(), make.size(), picture, &made, &why) != host::PolyTypeResult::Done)
		{
			textureMultiUntexturedLeft += make.size();
			continue;
		}

		// the coordinates: the nodes each corner takes (MakeTextured left every one at 0, 0)
		std::vector<model::PolygonUv> written;

		for (auto const &one : corners)
		{
			model::PolygonUv uv;

			if (host::UvEdit_ReadPolygon(open, open->Level(), lead.id, one.first, &uv, &why) != host::UvEditResult::Done || !uv.textured)
			{
				continue;
			}

			bool any = false;

			for (size_t c = 0; c < one.second.size() && (int)c < uv.uv.cornerCount; c++)
			{
				if (one.second[c] >= 0)
				{
					uv.uv.corners[c] = nodes[one.second[c]];
					any = true;
				}
			}

			if (any)
			{
				written.push_back(uv);
			}
		}

		if (!written.empty())
		{
			host::UvEdit_WritePolygons(open, open->Level(), lead.id, written, &why);
		}

		textured += made;
	}

	return textured;
}

// FUN_0054caf0 case 3 and FUN_00573f20: the entry (from TextureTab, with polygons selected and All Nodes Shown on)
void TextureMultiEnter()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t width = 0, height = 0;

	if (!open || !open->Level() || texturePolygons.empty() || !TextureCurrentPictureSize(&width, &height))
	{
		return;   // 0x54cb1e (no texture) / 0x54cbd7 (nothing selected)
	}

	MultiClear();
	TextureCurrentEnsure(open);   // the window this mode shows is the current texture's (state +0x58)

	size_t const madeTextured = TextureMultiTextureTheUntextured(open);   // FUN_00570040, first (0x573f44)

	// the selected polygons, each shape read once (its world points and its merge data)
	for (TexturePolygon const &one : texturePolygons)
	{
		size_t shape = multiShapes.size();

		for (size_t s = 0; s < multiShapes.size(); s++)
		{
			if (memcmp(multiShapes[s].id, one.id, 8) == 0)
			{
				shape = s;
			}
		}

		if (shape == multiShapes.size())
		{
			MultiShape made;
			bool hasChunk = false;

			memcpy(made.id, one.id, 8);
			made.isModule = one.isModule;
			made.moduleIndex = one.moduleIndex;

			std::vector<int32_t> normals;

			if (host::FilePick_World(open, open->Level(), one.id, &made.model, &made.world, &normals) != host::FilePickResult::Done)
			{
				continue;
			}

			if (!host::PolygonItems_ReadMergeData(open, open->Level(), one.id, &made.merge, &hasChunk) || !hasChunk || made.merge.size() != made.model.polygons.size())
			{
				made.merge.assign(made.model.polygons.size(), -1);
			}

			multiShapes.push_back(made);
		}

		MultiPolygonOf(open, shape, one.polygon, true);
	}

	// each selected triangle's merge partner (added unselected when it is not selected) and the pair as one quad (FUN_0055cdf0)
	size_t const selectedCount = multiPolygons.size();

	for (size_t p = 0; p < selectedCount; p++)
	{
		MultiShape const &owner = multiShapes[multiRefs[p].shape];
		int32_t const number = multiRefs[p].polygon;
		int32_t const partnerNumber = (size_t)number < owner.merge.size() ? owner.merge[(size_t)number] : -1;

		if (partnerNumber < 0 || multiPolygons[p].corners != 3)
		{
			continue;
		}

		int32_t const partner = MultiPolygonOf(open, multiRefs[p].shape, partnerNumber, false);
		model::TextureQuad quad;

		if (partner >= 0 && model::TextureQuadOf(owner.model, number, partnerNumber, &quad) == model::TextureQuadResult::Ok)
		{
			multiPolygons[p].partner = partner;
			multiPolygons[p].quadKnown = true;
			multiPolygons[p].quad = quad;
		}
	}

	multiNodes = model::MultiNodeJoin(multiPolygons);
	model::MultiNodeLink(&multiNodes, multiPolygons);

	std::string why;

	if (multiNodes.empty() || !model::MultiNodeLineUp(&multiNodes, &multiPolygons, width, height) || !MultiWrite(&why))
	{
		MultiClear();
		DialogMessage("Cannot use multiple node mode with currently selected polygons");   // 0x5742ad: the box, then the polygon screen

		static std::string failed;

		failed = "Texture Map: the multiple node mode could not start: " + std::string(multiNodes.empty() ? "no selected polygon gave a node" : "the nodes could not be lined up on the picture") +
			(textureMultiUntexturedLeft ? " (" + std::to_string(textureMultiUntexturedLeft) + " untextured polygons could not take the current texture)" : "") + (why.empty() ? "" : " - " + why);
		statusWords = failed.c_str();
		return;
	}

	textureSubMode = 3;
	textureWindowOn = true;   // the window's frame draws it (TextureWindowDraw's sub-mode 3 branch)
	multiSelectable = false;
	EditorBar_SetPage(3);   // 0x54cbee (+0x90 Resizing Texture is not reset: tm-b-multinode-map.md B, E)

	if (!LoadTheMenus())
	{
		LogString("menus: the multiple node mode's bars couldn't be made");
	}

	static std::string words;

	words = "Texture Map: multiple node mode: " + std::to_string(multiNodes.size()) + " nodes from " + std::to_string(selectedCount) + " polygons" +
		(madeTextured ? " (" + std::to_string(madeTextured) + " untextured ones given the current texture first)" : "");
	statusWords = words.c_str();
}

// FUN_005604f0: the mode left (Polygon 2, Tab, Exit 5, and Texture Map left): every selected polygon brought onto its picture (FUN_005702e0), the polygon screen
void TextureMultiLeave()
{
	if (textureSubMode != 3)
	{
		return;
	}

	for (size_t p = 0; p < multiPolygons.size(); p++)
	{
		if (multiPolygons[p].selected)
		{
			std::string why;
			MultiShape const &owner = multiShapes[multiRefs[p].shape];

			TextureWrapPolygon(owner.id, owner.isModule, owner.moduleIndex, multiRefs[p].polygon, &why);
		}
	}

	MultiClear();
	textureSubMode = 0;
	textureWindowOn = false;
	TextureOutlineAllShown();   // FUN_0056f9d0 (0x552cd7): every selected polygon's outline shown again
	EditorBar_SetPage(0);

	if (!LoadTheMenus())
	{
		LogString("menus: the Texture Map bars couldn't be made");
	}

	statusWords = "Texture Map: the polygons";
}

// What the window draws in sub-mode 3: each node's (u, v), the links (pairs of node numbers), which are lit (the current node), and the text (FUN_00552bb0 0x5530e5-0x55333d:
// "Texture Scale x%d" comes from the window; the node rows "%03d" u and v for the first 20, "..." when there are 20 or more, then "All Nodes Shown", "Selectable Nodes",
// "Resizing Texture")
void TextureMultiForDraw(std::vector<int32_t> *u, std::vector<int32_t> *v, std::vector<std::pair<int32_t, int32_t>> *links, std::vector<bool> *lit, std::vector<std::string> *lines)
{
	u->clear();
	v->clear();
	links->clear();
	lit->clear();
	lines->clear();

	for (size_t n = 0; n < multiNodes.size(); n++)
	{
		u->push_back(multiNodes[n].u);
		v->push_back(multiNodes[n].v);
		lit->push_back((int32_t)n == multiCurrent);

		for (int32_t const to : multiNodes[n].links)
		{
			links->push_back(std::make_pair((int32_t)n, to));
		}

		if (n < 20)
		{
			char row[40];

			snprintf(row, sizeof(row), "%s%03d %03d", (int32_t)n == multiCurrent ? "> " : "  ", multiNodes[n].u, multiNodes[n].v);
			lines->push_back(row);
		}
	}

	if (multiNodes.size() >= 20)
	{
		lines->push_back("...");   // 0x5532a2: also with exactly 20 (a bug candidate, kept with the rows: tm-b-multinode-map.md G-8)
	}

	if (textureAllNodesShown)
	{
		lines->push_back("All Nodes Shown");
	}

	if (multiSelectable)
	{
		lines->push_back("Selectable Nodes");
	}

	if (textureWin.resizing)
	{
		lines->push_back("Resizing Texture");
	}
}

bool TextureCurrentArea(int32_t *left, int32_t *top, int32_t *right, int32_t *bottom);   // texture_map_screen.cpp: the texture area (state +8 .. +0x14)

namespace
{
	// Every node in `which` moved by (du, dv) texels, held so that each stays in the texture area (FUN_00555070 / FUN_005546b0 clamp the step for the whole group)
	void MultiMove(std::vector<int32_t> const &which, int32_t du, int32_t dv)
	{
		int32_t left = 0, top = 0, right = 0, bottom = 0;

		if (!TextureCurrentArea(&left, &top, &right, &bottom))
		{
			return;
		}

		for (int32_t const n : which)
		{
			model::MultiNode const &node = multiNodes[(size_t)n];

			du = node.u + du < left ? left - node.u : node.u + du > right - 1 ? right - 1 - node.u : du;
			dv = node.v + dv < top ? top - node.v : node.v + dv > bottom - 1 ? bottom - 1 - node.v : dv;
		}

		for (int32_t const n : which)
		{
			multiNodes[(size_t)n].u += du;
			multiNodes[(size_t)n].v += dv;
		}
	}

	// FUN_00560450 for every node, then the changed polygons written
	void MultiWriteBack()
	{
		std::string why;
		static std::string words;

		model::MultiNodeWriteBack(multiNodes, &multiPolygons);

		if (!MultiWrite(&why))
		{
			words = "Texture Map: the nodes could not be written: " + why;
			statusWords = words.c_str();
			return;
		}

		if ((size_t)multiCurrent < multiNodes.size())
		{
			words = "Texture Map: node " + std::to_string(multiCurrent + 1) + " at " + std::to_string(multiNodes[(size_t)multiCurrent].u) + " " + std::to_string(multiNodes[(size_t)multiCurrent].v) +
				" (" + std::to_string(multiTaken.size()) + " taken)";
			statusWords = words.c_str();
		}
	}

	// A node's place on the screen (FUN_005567b0) and the texel under a screen place (FUN_00556710)
	int32_t MultiScreenX(model::TextureWindowState const &win, int32_t n) { return model::WindowScreenX(win, multiNodes[(size_t)n].u); }
	int32_t MultiScreenY(model::TextureWindowState const &win, int32_t n) { return model::WindowScreenY(win, multiNodes[(size_t)n].v); }
}

// A press on the window (win: its layout, model/texture_window.h); true: the drag follows
bool TextureMultiPress(int32_t x, int32_t y, model::TextureWindowState const &win, bool shift)
{
	if (textureSubMode != 3 || multiNodes.empty())
	{
		return false;
	}

	multiPressX = multiLastX = x;
	multiPressY = multiLastY = y;
	multiDrag = 0;
	bool const onPicture = x > win.winL + 3 && y > win.winT + 3 && x < win.winR && y < win.winB;   // 0x5545b0: inside the view

	if (multiSelectable)
	{
		multiDrag = shift ? 0x10 : 1;   // the taken nodes move, or a box (0x55411e)
		return true;
	}

	if (shift && onPicture && (size_t)multiCurrent < multiNodes.size())
	{
		// Shift: the current node to the mouse (0x55414e-0x554256)
		multiTaken.assign(1, multiCurrent);
		MultiMove(multiTaken, model::WindowTexelX(win, x) - multiNodes[(size_t)multiCurrent].u, model::WindowTexelY(win, y) - multiNodes[(size_t)multiCurrent].v);
		MultiWriteBack();
		multiTaken.clear();
		return false;
	}

	for (size_t n = 0; n < multiNodes.size(); n++)
	{
		if (abs(MultiScreenX(win,(int32_t)n) - x) <= 4 && abs(MultiScreenY(win,(int32_t)n) - y) <= 4)   // 0x55428b, the first node within 4
		{
			multiCurrent = (int32_t)n;
			MultiTake((int32_t)n);
			multiDrag = 1;
			return true;
		}
	}

	for (size_t n = 0; n < multiNodes.size(); n++)
	{
		for (int32_t const to : multiNodes[n].links)
		{
			// a link within 5 pixels, between its ends (0x554405-0x55447e)
			double const ax = MultiScreenX(win,(int32_t)n), ay = MultiScreenY(win,(int32_t)n), bx = MultiScreenX(win,to), by = MultiScreenY(win,to);
			double const dx = bx - ax, dy = by - ay, length = sqrt(dx * dx + dy * dy);

			if (length > 0 && (x - ax) * (x - bx) + (y - ay) * (y - by) < 0 && fabs((x - ax) * dy - (y - ay) * dx) / length < 5.0)
			{
				MultiTake((int32_t)n);
				MultiTake(to);
				multiDrag = 1;
				return true;
			}
		}
	}

	if (onPicture)
	{
		for (size_t n = 0; n < multiNodes.size(); n++)
		{
			MultiTake((int32_t)n);   // 0x5545b0-0x554633: every node
		}

		multiDrag = 1;
		return true;
	}

	return false;
}

// The drag (FUN_005546b0): the taken nodes move with the mouse (one node: to the mouse)
void TextureMultiDragTo(int32_t x, int32_t y, model::TextureWindowState const &win)
{
	if (textureSubMode != 3 || multiDrag != 1 || multiTaken.empty())
	{
		multiLastX = x;
		multiLastY = y;
		return;
	}

	if (multiTaken.size() == 1)
	{
		model::MultiNode const &node = multiNodes[(size_t)multiTaken[0]];

		MultiMove(multiTaken, model::WindowTexelX(win, x) - node.u, model::WindowTexelY(win, y) - node.v);
	}
	else
	{
		double const f = win.scaleF;
		int32_t const du = model::WindowFtol((x - multiPressX) / f) - model::WindowFtol((multiLastX - multiPressX) / f);
		int32_t const dv = model::WindowFtol((y - multiPressY) / f) - model::WindowFtol((multiLastY - multiPressY) / f);

		MultiMove(multiTaken, du, dv);
	}

	multiLastX = x;
	multiLastY = y;
}

// The release (FUN_00554ca0): a box takes the nodes in it (one under 2 pixels: the nearest node in or out); the moved nodes are written back; without Selectable Nodes nothing stays taken
void TextureMultiRelease(int32_t x, int32_t y, model::TextureWindowState const &win)
{
	if (textureSubMode != 3)
	{
		return;
	}

	if (multiDrag == 0x10)
	{
		int32_t const x0 = multiPressX < x ? multiPressX : x, x1 = multiPressX < x ? x : multiPressX, y0 = multiPressY < y ? multiPressY : y, y1 = multiPressY < y ? y : multiPressY;

		if (x1 - x0 < 2 && y1 - y0 < 2)
		{
			int32_t nearest = -1;
			int64_t best = 10000000;   // 0x989680

			for (size_t n = 0; n < multiNodes.size(); n++)
			{
				int64_t const dx = MultiScreenX(win,(int32_t)n) - x, dy = MultiScreenY(win,(int32_t)n) - y;

				if (dx * dx + dy * dy < best)
				{
					best = dx * dx + dy * dy;
					nearest = (int32_t)n;
				}
			}

			auto const at = std::find(multiTaken.begin(), multiTaken.end(), nearest);

			if (at != multiTaken.end())
			{
				multiTaken.erase(at);
			}
			else
			{
				MultiTake(nearest);
			}
		}
		else
		{
			for (size_t n = 0; n < multiNodes.size(); n++)
			{
				int32_t const nx = MultiScreenX(win,(int32_t)n), ny = MultiScreenY(win,(int32_t)n);

				if (nx >= x0 && nx <= x1 && ny >= y0 && ny <= y1)
				{
					MultiTake((int32_t)n);
				}
			}
		}
	}
	else if (multiDrag == 1)
	{
		MultiWriteBack();
	}

	multiDrag = 0;

	if (!multiSelectable)
	{
		multiTaken.clear();   // 0x554fe0
	}
}

// The arrow keys (FUN_00555070): du / dv texels (1, or 10 with Shift); with Ctrl every node, with Selectable Nodes the taken ones, else the current node
void TextureMultiArrow(int32_t du, int32_t dv, bool ctrl)
{
	if (textureSubMode != 3 || multiNodes.empty())
	{
		return;
	}

	std::vector<int32_t> which;

	if (ctrl)
	{
		for (size_t n = 0; n < multiNodes.size(); n++)
		{
			which.push_back((int32_t)n);
		}
	}
	else if (multiSelectable)
	{
		which = multiTaken;
	}
	else if ((size_t)multiCurrent < multiNodes.size())
	{
		which.push_back(multiCurrent);
	}

	MultiMove(which, du, dv);
	MultiWriteBack();
}

// Sub-mode 3's own commands (FUN_00552bb0's switches 0x552cc5 / 0x552e5a; TM-B 3c). True: done here; false: the Texture Map screen's own switch does it as in the window (Reset Source Rect
// 0x9, Next / Prev Texture 0xa / 0xb, Select Texture 0x2a, the scales 0xec-0xf4: the same calls in both).
//  - 2 Polygon (and Tab) leaves; 5 Exit is the editor's (its LeaveTextureMap leaves this mode first).
//  - 6 / 7: every node flipped over the texture area (FUN_00558880 / FUN_00558900: u = left + right - 1 - u, v = top + bottom - 1 - v).
//  - 0x16 / 0x17 / 0x18: every node turned 90 / 180 / 270 degrees about the nodes' middle; 0x19 / 0x1a / 0x1b: about the texture area's middle (FUN_00556ce0(angle, 0 / 1)).
//  - 0x102 (S) Selectable Nodes on / off, the taken nodes let go (0x552f0d); 0x104 (L) the taken nodes let go (0x552fa1); 0x103 (T) Resizing Texture on / off (0x55301d).
//  - 0x10f Select Number of Repeats as in the window; 0x28 the Movement help.
// Each move is written back (FUN_00560450). NAMED: the turns are made on texels, rounded to the nearest, the same way round as the window's (inteng turns the nodes' screen places in
// floats, FUN_00556a90 / FUN_00556710: its rounding and which way is +90 are not read). T (Texture Resize), I (0x105, the selected polygons' outline) and the scales are the single
// window's own (TextureMapCommand); undo / redo (0x36 / 0x37, the window's ring) do nothing yet.
void TextureRepeatsChooserOpen();

// The mode's own help (0x27, FUN_005572d0): two text screens, the mouse's (0x5ff788) then the keys' (0x5ff434), inteng's words
char const *const kIntengMultiNodeMouseHelpText = "\t\t\tMouse Controls\n\nLeft click and drag on node or line to move that node or line.\nLeft click and drag in polygon to move it.\nLeft click and drag near edges of texture to resize texture.\nShift+Left click to move current node to cursor.\nRight click and drag on texture to rotate nodes.\nControl+Right click and drag on texture to resize polygon.\nRight click outside of texture to get pop-up menu.\n\nIn node selection mode (S) :\nLeft click and drag to move all selected nodes\nShift+Left click near node to toggle selection\n\nWhile in texture resizing mode (T to toggle on and off)\nLeft click and drag in texture to scroll currently viewed area\nLeft click and drag near edges of texture to resize texture.\n";
char const *const kIntengMultiNodeKeysHelpText = "\t\t\t Keyboard Controls\n\nArrow keys\tMove the selected node.\nShift+Arrow keys\tMove the selected node in steps of 10.\nCtrl+Arrow keys\tMove all nodes.\nCtrl+Shift+Arrow \tMove all nodes in steps of 10.\nIn texture resizing mode(T) , arrow keys scroll texture\n\nP\t\tSelect previous texture.\nN\t\tSelect next texture.\nC\t\tSelect texture.\nX\t\tX-Flip nodes.\nY\t\tY-Flip nodes.\nE\t\tRotate nodes 90 degrees anti-clockwise.\nR\t\tRotate nodes 90 degrees clockwise.\n6\t\tReduce texture scale in powers of 2\n7\t\tReduce texture scale by 1\n8\t\tSet texture scale to x1\n9\t\tIncrease texture scale by 1\n0\t\tIncrease texture scale in powers of 2\nT\t\tToggle texture resizing\nShift+N\t\tSelect number of repeats of texture to use\nL\t\tDeselect all nodes\nI\t\tToggle polyline on selected polygons\nSpace\t\tForce screen redraw.\nTab\t\tChange to polygon mode.\n<\t\tUndo.\n>\t\tRedo.\nCtrl+X\t\tExit texture mapper.";

namespace
{
	// Every node turned quarters x 90 degrees about (cu, cv) in texels, rounded to the nearest, held in the texture area
	void MultiTurn(int quarters, double cu, double cv)
	{
		int32_t left = 0, top = 0, right = 0, bottom = 0;

		if (!TextureCurrentArea(&left, &top, &right, &bottom))
		{
			return;
		}

		for (model::MultiNode &node : multiNodes)
		{
			double du = node.u - cu, dv = node.v - cv;

			for (int q = 0; q < quarters; q++)
			{
				double const turned = du;

				du = dv;
				dv = -turned;
			}

			int32_t const u = (int32_t)floor(cu + du + 0.5), v = (int32_t)floor(cv + dv + 0.5);

			node.u = u < left ? left : u > right - 1 ? right - 1 : u;
			node.v = v < top ? top : v > bottom - 1 ? bottom - 1 : v;
		}
	}
}

bool TextureMultiCommand(int32_t command)
{
	int32_t left = 0, top = 0, right = 0, bottom = 0;
	static std::string words;

	switch (command)
	{
		case 0x6: case 0x7:
			if (TextureCurrentArea(&left, &top, &right, &bottom))
			{
				for (model::MultiNode &node : multiNodes)
				{
					if (command == 0x6)
					{
						node.u = left + right - 1 - node.u;
					}
					else
					{
						node.v = top + bottom - 1 - node.v;
					}
				}

				MultiWriteBack();
			}

			return true;
		case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
			if (!multiNodes.empty() && TextureCurrentArea(&left, &top, &right, &bottom))
			{
				double cu = 0, cv = 0;

				if (command >= 0x19)
				{
					cu = (left + right) * 0.5;   // the texture area's middle (0x556d94)
					cv = (top + bottom) * 0.5;
				}
				else
				{
					for (model::MultiNode const &node : multiNodes)
					{
						cu += (double)node.u / multiNodes.size();   // the nodes' middle (0x556d68)
						cv += (double)node.v / multiNodes.size();
					}
				}

				MultiTurn(command >= 0x19 ? command - 0x18 : command - 0x15, cu, cv);
				MultiWriteBack();
			}

			return true;
		case 0x27:
			textPanelText = kIntengMultiNodeMouseHelpText;   // FUN_005572d0: the mouse's page, then the keys' (FUN_004724f0 twice)
			textPanelNext = kIntengMultiNodeKeysHelpText;
			return true;
		case 0x102:
			multiSelectable = !multiSelectable;
			multiTaken.clear();
			statusWords = multiSelectable ? "Texture Map: Selectable Nodes on (presses take nodes; Shift draws a box)" : "Texture Map: Selectable Nodes off";
			return true;
		case 0x104:
			multiTaken.clear();
			statusWords = "Texture Map: no node taken";
			return true;
		case 0x9: case 0xa: case 0xb: case 0x2a: case 0xec: case 0xed: case 0xee: case 0xef: case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0x103: case 0x105:
			return false;   // (0x103 Toggle Texture Resize: the window's one flag, +0x90; 0x105 I: the same FUN_0056fa60 as the window's; both in TextureMapCommand)
		case 0x2:
			TextureMultiLeave();
			return true;
		case 0x5:
			return false;   // Exit: the editor's Exit, whose LeaveTextureMap leaves this mode first (0x552cf0)
		case 0x28:
			textPanelText = kIntengTextureMapMovementText;
			textPanelNext = nullptr;
			return true;
		case 0x10f:
			TextureRepeatsChooserOpen();
			return true;
		default:
			return true;
	}
}

// The multiple node mode's node places (texels), for the window's right drag (texture_map_screen.cpp); setting them holds each in the texture area, and write puts them in the polygons
void TextureMultiPlaces(std::vector<int32_t> *u, std::vector<int32_t> *v)
{
	u->clear();
	v->clear();

	for (model::MultiNode const &node : multiNodes)
	{
		u->push_back(node.u);
		v->push_back(node.v);
	}
}

void TextureMultiSetPlaces(std::vector<int32_t> const &u, std::vector<int32_t> const &v, bool write)
{
	int32_t left = 0, top = 0, right = 0, bottom = 0;

	if (textureSubMode != 3 || u.size() != multiNodes.size() || v.size() != multiNodes.size() || !TextureCurrentArea(&left, &top, &right, &bottom))
	{
		return;
	}

	for (size_t n = 0; n < multiNodes.size(); n++)
	{
		multiNodes[n].u = u[n] < left ? left : u[n] > right - 1 ? right - 1 : u[n];
		multiNodes[n].v = v[n] < top ? top : v[n] > bottom - 1 ? bottom - 1 : v[n];
	}

	if (write)
	{
		MultiWriteBack();
	}
}
