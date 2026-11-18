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
	bool mergeTextShown = false;             // DAT_005fd69c, flipped by P (0x32)
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
bool TextureWindowCommand(int32_t command)
{
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
			mergeTextShown = !mergeTextShown;   // DAT_005fd69c; the text FUN_00543fa0 draws while it is set (FUN_0057b6f0 at 0x544048) is NOT READ yet (tm-b-merge-map.md)
			statusWords = mergeTextShown ? "Texture Map: Merge: text shown" : "Texture Map: Merge: text hidden";
			return true;
		default:
			return true;
	}
}
