// texture_map_screen.cpp: TEXTURE MAPPING: the screen's selection, flags, picture windows, animated and multiple textures, polygon edits, the chooser, and its -texmapproof runs. Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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

// The live preview of a texture coordinate write: the game's items of those file polygons take the new corners, so the view shows them at once. Only a polygon that is ONE item of the same corner count is shown
// (a pair of triangles the loader joined into a quad, and an animated texture, keep the old look until the level is opened again: named).
static void ShowTheNewUv(OpenLevelFile *open, TexturePolygon const &lead, std::vector<model::PolygonUv> const &polygons)
{
	LevelViewShapeOf shape = {};
	model::ShapeModel model;
	std::string why;
	std::vector<uint8_t> corners;
	std::vector<int32_t> numbers, toItem, firstFile;
	bool hasMerge = false;

	shape.isModule = lead.isModule;
	shape.moduleIndex = lead.moduleIndex;
	memcpy(shape.objectId, lead.id, 8);

	if (host::ShapeEdit_Read(open->Level(), open, lead.id, &model, nullptr, &why) != host::ShapeEditResult::Done || !LevelView_ItemCorners(shape, &corners) ||
		!host::PolygonItems_ReadMergeData(open, open->Level(), lead.id, &numbers, &hasMerge) ||
		host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), model.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) != host::PolygonItemResult::Ok)
	{
		return;
	}

	for (model::PolygonUv const &polygon : polygons)
	{
		if (polygon.polygonNumber < 0 || (size_t)polygon.polygonNumber >= toItem.size() || polygon.uv.cornerCount < 3 || polygon.uv.cornerCount > 4)
		{
			continue;
		}

		int32_t uv[8];

		for (int c = 0; c < polygon.uv.cornerCount; c++)
		{
			uv[c * 2] = (int32_t)polygon.uv.corners[c].u;
			uv[c * 2 + 1] = (int32_t)polygon.uv.corners[c].v;
		}

		LevelView_SetItemUv(shape, toItem[(size_t)polygon.polygonNumber], uv, polygon.uv.cornerCount);
	}
}

// An imported shape's changes are never saved (FUN_00572440 rebuilds only shapes with no SHPEXTFL, Vasquez p11 section 68), so a write command leaves the selected polygons of imported shapes as they were: they are taken out of the selection first. Returns how many.
static size_t TextureDropImported()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t dropped = 0;

	if (!open || !open->Level())
	{
		return 0;
	}

	for (size_t i = texturePolygons.size(); i > 0; i--)
	{
		if (host::PolygonItems_ShapeHasOwnPictureList(open->Level(), texturePolygons[i - 1].id))
		{
			texturePolygons.erase(texturePolygons.begin() + (std::ptrdiff_t)(i - 1));
			dropped++;
		}
	}

	if (dropped)
	{
		ShowTheTexturePolygons();
	}

	return dropped;
}

void TextureMapDeselectAll()
{
	texturePolygons.clear();
	ShowTheTexturePolygons();
	statusWords = "Texture Map: all polygons deselected";
}

// TEXTURE MAP: THE SELECTION COMMANDS (menu Options > Select As and the Selection menu; inteng's FUN_0054d260 cases 0xf9-0x101, FUN_005590c0 the click, dump-all-4.txt:5326-5440 and 12100-12300).
// The selection mode DAT_00914d9c: 0 by Polygon (0xf9), 1 by Object (0xfa), 2 Rect (In or On) (0xfb), 3 Rect (In) (0xfc), 4 Vertex (0xfd), 5 by Element (0xfe). A left click in mode 0 or 5 with neither Ctrl nor Alt selects the
// polygon (FUN_0056db80) or its ELEMENT (FUN_0056de60: the ring FUN_005751d0 works out) and, with Shift, deselects it (FUN_0056dea0 / FUN_0056e060); in mode 1, or with Ctrl or Alt, it is the whole object
// (FUN_0056dd60 / FUN_0056dfa0). An element is the polygons joined through shared corners: FUN_005751d0 treats two points of the shape as one when each coordinate differs by under 11 of inteng's unit (1e-5 m), then every polygon that shares a point
// with another is in its element; NAMED: file points are whole numbers, 100 x scale of inteng's unit apart at least, so "the same point" here is the same number. 0xff Angle (FUN_0056e240) selects, in every shape, the polygons whose normal is
// within an angle of straight up: normal . (0, -65536, 0) >= ftol(cos(angle / 180 x pi) x 65536). 0x101 Invert (FUN_0056e130 / FUN_0056e1f0): the next click names an object whose every polygon is toggled, or A for every shape.
// 0x100 Arrow Selection Step Size: two numbers (vertical, horizontal), at least 1 each (DAT_005fd68c / DAT_005fd690, 1 and 1 at the start); what the arrow keys do with them is not read, so they are only kept (NAMED).
int textureSelectMode = 0;
// VERTEX MODE (Select 0xfd, mode 4; FUN_00565310): the corners held for Create poly (0x10e). Any change of selection mode clears them (FUN_0054d260 at 0x54eedc). The WORLD places of the shape's points are kept so a
// moving camera can draw the held corners each frame (polyShapeWorld, three per point, the game's millimetres).
host::PolyVertexList polyVertices;
std::vector<int32_t> polyShapeWorld;
int texturePickViewW = 0, texturePickViewH = 0;   // the size of the view the last Texture Map click was made in
static void TexturePolyCreate();
static void TexturePolyDeleteNow();
static void TexturePolyRestore();
static void TexturePolyZeroNormalsNow();
static void TexturePolyShowHide(bool hide);
void TexturePolyApplyRemoved();
struct SplitWaiting
{
	uint8_t id[8];
	bool asPlaced;   // As Placed Obj (0xc8) rather than As Module (0xc7)
	std::vector<int32_t> polygons;
	std::string name;   // the new object's name, "" for the default (Split<n>)
};
static std::vector<SplitWaiting> polySplit;   // Split Object's waiting list (see TexturePolySplitNow)
static void TexturePolySplitNow(bool asPlaced);
static void TexturePolyUnmerge();
static void TexturePolyMerge();
bool TexturePolyMarkedForSplit(uint8_t const *id, int32_t polygon);
int textureArrowVertical = 1, textureArrowHorizontal = 1;
// AUTOSAVE (Options > AutoSave 0x13: FUN_0055f470, dialog FUN_0055f500: "Frequency of autosave in minutes:" and "Prompt before save", "Currently saving every %d minutes"; the timer FUN_0055f3a0: when the minutes have passed since the last save and something changed, it saves - after
// "AutoSave... Would you like to save the changes you have made?" when the prompt box is on). NAMED: inteng's save is the Texture Map's own copying of its edits into the level and the file; here the save is Save a copy (a file of the editor's own folder, never the opened one); the numbers are
// kept for the session only (inteng keeps them in its settings file); nothing tells this editor whether the level changed since the last save, so it saves every time the minutes pass; and the first answer 0 turns it off (inteng's own start value is not read).
int textureAutoMinutes = 0;
bool textureAutoPrompt = false;
DWORD textureAutoLast = 0;
bool textureSHeld = false;   // S is down in Texture Map (the mode takes the key: texture_keys.h)
// POLYGON FLAGS AND HIDING (the Texture Map's Flags menu and the Hide items; FUN_0054d260 cases 0x42-0x46, 0x68-0x7d, 0x99-0x9b; FUN_0056fb70, FUN_005428c0, FUN_0056fd40, FUN_00560930). Add (FUN_0056fb70(mask, 1)) and Remove (mask, 0) put the bit into, or take it out of, the FLAG WORD of every selected polygon (SHPPOLYS +8:
// 0x2 No Light, 0x1 Not Visible, 0x20 No BFC, 0x10 Translucency, 0x20000000 Light Corona, 0x1000 Mirror) and the game's item of the polygon follows at once. View (FUN_005428c0(mask)) draws the polygons that have the flag in the highlight colour until None (0x4c) or another View; Animation (0x7d) views the polygons that have an
// animation. Hide Selection (0x42, FUN_0056fd40(0x40000000, 1) and the selection cleared), Hide Unselected (0x43) and Show All (0x45, and 0x46 which selects the polygons it shows, FUN_00560930) hide polygons in the view only (inteng's temporary bit 0x40000000: never saved). NAMED: Non-Planar Quads (0x11c) views a
// flag inteng sets itself and is not here; the highlight colour of View is the selection's yellow (inteng recolours the flagged polygons in its own colours); a polygon hidden here comes back when its level is replaced (an Anim command, a merge).
static std::vector<TexturePolygon> textureHidden;

struct TextureFlagRow
{
	int32_t add, remove, view;
	uint32_t mask;
	char const *name;
};

static TextureFlagRow const kTextureFlagRows[] =
{
	{ 0x6e, 0x6f, 0x70, 0x2, "No Light" }, { 0x71, 0x72, 0x73, 0x1, "Not Visible" }, { 0x74, 0x75, 0x76, 0x20, "No BFC" }, { 0x77, 0x78, 0x79, 0x10, "Translucency" }, { 0x68, 0x69, 0x6a, 0x20000000, "Light Corona" }, { 0x99, 0x9a, 0x9b, 0x1000, "Mirror" },
};

// The game's items of the polygons of one shape (polygon_items.h)
struct TextureItems
{
	bool ok;
	LevelViewShapeOf shape;
	model::ShapeModel model;
	std::vector<int32_t> toItem;
};

static TextureItems TextureItemsOf(OpenLevelFile *open, uint8_t const *id, bool isModule, int32_t moduleIndex)
{
	TextureItems made = {};
	std::string why;
	std::vector<uint8_t> corners;
	std::vector<int32_t> numbers, firstFile;
	bool hasMerge = false;

	made.shape.isModule = isModule;
	made.shape.moduleIndex = moduleIndex;
	memcpy(made.shape.objectId, id, 8);
	made.ok = host::ShapeEdit_Read(open->Level(), open, id, &made.model, nullptr, &why) == host::ShapeEditResult::Done && LevelView_ItemCorners(made.shape, &corners) &&
		host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge) &&
		host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), made.model.polygons.size(), corners.data(), corners.size(), &made.toItem, &firstFile) == host::PolygonItemResult::Ok;
	return made;
}

static void TextureFlagsSet(uint32_t mask, bool on, char const *name)
{
	TextureDropImported();

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		statusWords = "Texture Map: select the polygons first";
		return;
	}

	size_t changed = 0;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		TextureItems items = TextureItemsOf(open, lead.id, lead.isModule, lead.moduleIndex);
		std::string why;

		if (!items.ok)
		{
			continue;
		}

		std::vector<int32_t> touched;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (done[i] || memcmp(one.id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			if (one.polygon >= 0 && (size_t)one.polygon < items.model.polygons.size())
			{
				int32_t &flags = items.model.polygons[(size_t)one.polygon].flags;

				flags = on ? (int32_t)((uint32_t)flags | mask) : (int32_t)((uint32_t)flags & ~mask);
				touched.push_back(one.polygon);
			}
		}

		if (!touched.empty() && host::ShapeEdit_WriteSameCounts(open, open->Level(), lead.id, items.model, &why) == host::ShapeEditResult::Done)
		{
			changed += touched.size();

			for (int32_t const polygon : touched)
			{
				if ((size_t)polygon < items.toItem.size())
				{
					LevelView_SetItemFlagBits(items.shape, items.toItem[(size_t)polygon], (int32_t)mask, on);
				}
			}
		}
	}

	textureWords = std::string("Texture Map: ") + name + (on ? " added to " : " taken from ") + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s");
	statusWords = textureWords.c_str();
}

static void TextureFlagView(uint32_t mask, bool animated, char const *name)
{
	textureFlagViewMask = mask;
	textureViewAnimated = animated;
	ShowTheTexturePolygons();
	textureWords = mask || animated ? std::string("Texture Map: the polygons with the flag ") + name + " are shown: " + std::to_string(textureFlagViewCount) + " found (None puts it back)" : "Texture Map: no flag is viewed";
	statusWords = textureWords.c_str();
}

static void TextureHide(bool unselected)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		return;
	}

	size_t hidden = 0;

	auto const hideOne = [&](TextureItems &items, TexturePolygon const &one)
	{
		if (one.polygon >= 0 && (size_t)one.polygon < items.toItem.size() && LevelView_SetItemFlagBits(items.shape, items.toItem[(size_t)one.polygon], 1, true))
		{
			bool known = false;

			for (TexturePolygon const &have : textureHidden)
			{
				known = known || (have.polygon == one.polygon && memcmp(have.id, one.id, 8) == 0);
			}

			if (!known)
			{
				textureHidden.push_back(one);
			}

			hidden++;
		}
	};

	if (!unselected)
	{
		std::vector<TexturePolygon> const now = texturePolygons;

		for (TexturePolygon const &one : now)
		{
			TextureItems items = TextureItemsOf(open, one.id, one.isModule, one.moduleIndex);

			if (items.ok)
			{
				hideOne(items, one);
			}
		}

		TextureMapDeselectAll();   // FUN_0056e0a0: "Clearing list"
	}
	else
	{
		TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
		{
			TextureItems items = TextureItemsOf(open, id, isModule, moduleIndex);

			for (size_t p = 0; items.ok && p < items.model.polygons.size(); p++)
			{
				if (TextureSelectedIndex(id, (int32_t)p) < texturePolygons.size())
				{
					continue;
				}

				TexturePolygon one = {};

				memcpy(one.id, id, 8);
				one.polygon = (int32_t)p;
				one.isModule = isModule;
				one.moduleIndex = isModule ? moduleIndex : -1;
				hideOne(items, one);
			}
		});
	}

	textureWords = "Texture Map: " + std::to_string(hidden) + " polygon" + (hidden == 1 ? "" : "s") + " hidden (Show All brings them back)";
	statusWords = textureWords.c_str();
}

static void TextureShowAll(bool select)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t shown = 0;

	if (!open || !open->Level())
	{
		return;
	}

	std::vector<TexturePolygon> const hiddenNow = textureHidden;

	textureHidden.clear();

	for (TexturePolygon const &one : hiddenNow)
	{
		TextureItems items = TextureItemsOf(open, one.id, one.isModule, one.moduleIndex);

		if (!items.ok || one.polygon < 0 || (size_t)one.polygon >= items.toItem.size())
		{
			continue;
		}

		// the item shows again unless the polygon's own flag says Not Visible
		LevelView_SetItemFlagBits(items.shape, items.toItem[(size_t)one.polygon], 1, (items.model.polygons[(size_t)one.polygon].flags & 1) != 0);
		shown++;

		if (select)
		{
			TextureSelectPolygon(one.id, one.isModule, one.moduleIndex, one.polygon, true);
		}
	}

	ShowTheTexturePolygons();
	textureWords = "Texture Map: " + std::to_string(shown) + " polygon" + (shown == 1 ? "" : "s") + " shown again" + (select ? " and selected" : "");
	statusWords = textureWords.c_str();
}

// Shift UV Coords (0x133, FUN_005720e0): every selected textured polygon's coordinates are moved by (u, v) whole texels; Scale UV Coords (0x134, FUN_00572200): they are multiplied by the factor, each result cut towards zero (the x87 __ftol), nothing done for 1.0
static void TextureShiftOrScale(bool scale, double a, double b)
{
	TextureDropImported();

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		statusWords = "Texture Map: select the polygons first";
		return;
	}

	size_t changed = 0;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		std::vector<model::PolygonUv> forShape;
		std::string why;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			if (done[i] || memcmp(texturePolygons[i].id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			model::PolygonUv uv;

			if (host::UvEdit_ReadPolygon(open, open->Level(), lead.id, texturePolygons[i].polygon, &uv, &why) != host::UvEditResult::Done || !uv.textured)
			{
				continue;
			}

			for (int c = 0; c < uv.uv.cornerCount; c++)
			{
				if (scale)
				{
					uv.uv.corners[c].u = (float)(int)(uv.uv.corners[c].u * a);
					uv.uv.corners[c].v = (float)(int)(uv.uv.corners[c].v * a);
				}
				else
				{
					uv.uv.corners[c].u += (float)(int)a;
					uv.uv.corners[c].v += (float)(int)b;
				}
			}

			forShape.push_back(uv);
		}

		if (!forShape.empty() && host::UvEdit_WritePolygons(open, open->Level(), lead.id, forShape, &why) == host::UvEditResult::Done)
		{
			changed += forShape.size();
			ShowTheNewUv(open, lead, forShape);
		}
	}

	textureWords = std::string("Texture Map: the coordinates of ") + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s") + (scale ? " were scaled" : " were shifted");
	statusWords = textureWords.c_str();
}

// TOGGLE FLAG LIST (Misc > Toggle Flag List 0xb4: DAT_00914d78 is switched; FUN_00560ab0 is called every frame while it is on): the polygon under the pointer is described in lines of text at the top of the view - the object's name, "%d %d" (the polygon and the one it was merged with, -1 for none), its type
// (Polygon / 2dTextured / 3dTextured, else "type : %d"), "Texture : %d" for a textured one, "vert %d : %d,%d,%d" for each of its points (a merged pair: the points of both, then "norm1_" and "norm2_"; else "norm_"), then the names of its flags: for the AvP game type notvis (1), nolight (2),
// transparent (0x10), no_bfc (0x20), tx2dor3d (0x400000), linear_s (0x800000), sortfarz (0x4000000), avp_corona_flag (0x20000000) and temp_not_visible (0x40000000); the PSX and Gunlok lists are for the other game types (FUN_0057aa20, DAT_00915104; AvP is 0). NAMED: the points are shown in inteng's unit
// (the file's number x 100 x the level's scale) as the file holds them placed, where inteng shows its own transformed ones; the lines are drawn at the top left of the view below the bars, not at x 200.
bool textureFlagList = false;
std::vector<std::string> textureFlagLines;
int textureHoverX = -1, textureHoverY = -1;
bool textureHoverSet = false;
int textureArrowAsked = 0;   // an arrow key with S down was pressed (1 up, 2 left, 3 right, 4 down): the frame loop does the selection with that frame's camera
// 0x10f Select Number of Repeats (FUN_00557c00): the texture window is the picture repeated 1 to 10 times each way, or without end (infXinf): the window's right is width x repeats + DAT_00914d30 (Add 1), its bottom height x repeats + DAT_00914d30, left and top
// 0 - for "inf" the left (top) is -100,000,000 and the right (bottom) 100,000,000 - and its size is right - left. 11 stands for inf. 0x110 / 0x111 / 0x112 set DAT_00914db8 (0 Default to 1X1, 1 Maintain Used Area, 2 Maintain Num Repeats): what the texture screen does with the
// window when the current texture changes. NAMED: this editor's window is the lowest picture's whatever the current texture is, so that switch is kept and changes nothing; the used area (the four corners wrap works on) stays the picture's own size.
int textureRepeatX = 1, textureRepeatY = 1;
static int textureRepeatMode = 0;

static model::TextureWindow TextureWindowOf(int32_t width, int32_t height, int addOne)
{
	int32_t const infinite = 100000000;
	int32_t const left = textureRepeatX == 11 ? -infinite : 0, top = textureRepeatY == 11 ? -infinite : 0;
	int32_t const right = textureRepeatX == 11 ? infinite : (int32_t)(((int64_t)width * textureRepeatX + addOne) > 1000000000 ? 1000000000 : (int64_t)width * textureRepeatX + addOne);
	int32_t const bottom = textureRepeatY == 11 ? infinite : (int32_t)(((int64_t)height * textureRepeatY + addOne) > 1000000000 ? 1000000000 : (int64_t)height * textureRepeatY + addOne);

	return { left, top, right, bottom, right - left, bottom - top };
}

// THE TEXTURE WINDOW'S CORNER NODES (inteng's list of four (u, v) points at the texture screen's state +0xa4 / +0x52): the picture's corners at the start - (0,0) (W,0) (W,H) (0,H) of the window's picture - which every X-Flip / Y-Flip / turn moves as it moves the selected
// polygons ("THEN, whatever the command, the same flip / rotate is applied to the FOUR window corner points", FUN_00570f40, p11 section 51), and which Wrap and Tile use as the window's corners (FUN_00570590). The texture screen edits them.
static model::PolygonUv textureNodes;
static bool textureNodesMade = false;
static int32_t textureNodesWidth = 0, textureNodesHeight = 0;

static void TextureNodesFor(int32_t width, int32_t height)
{
	if (textureNodesMade && textureNodesWidth == width && textureNodesHeight == height)
	{
		return;
	}

	textureNodes = model::PolygonUv();
	textureNodes.textured = true;
	textureNodes.engineType = 7;
	textureNodes.cornerCount = 4;
	textureNodes.uv.cornerCount = 4;
	textureNodes.uv.corners[0] = { 0.0f, 0.0f };
	textureNodes.uv.corners[1] = { (float)width, 0.0f };
	textureNodes.uv.corners[2] = { (float)width, (float)height };
	textureNodes.uv.corners[3] = { 0.0f, (float)height };
	textureNodesMade = true;
	textureNodesWidth = width;
	textureNodesHeight = height;
}

static int textureAddOne = 0;   // 0x14 Add 1 to texture area (DAT_00914d30: 0 at the start): while on, the texture window is one larger each way - the right and bottom (and its size) are the picture's width and height PLUS 1 (0x54f0ce-0x54f15d, FUN_00555cd0)
static bool textureShowImported = false;   // 0x145 Show Imported Shapes (DAT_00914d50): off at the start; FUN_0054bdb0 sets object flag 0x800000 on every SHPEXTFL shape while it is off (they cannot be selected or inverted)
bool textureRectActive = false, textureRectAsked = false, textureRectShift = false;   // Rect (In or On) / Rect (In): the left button is down and dragging / the drag is let go and waits for the frame
int textureRectX0 = 0, textureRectY0 = 0, textureRectX1 = 0, textureRectY1 = 0, textureRectViewW = 0, textureRectViewH = 0;
static void TextureFind(int32_t command);
static void TextureAnimCommand(int32_t command);
static void TextureAnimSelectClicked(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon);
static bool TextureAnimTyped(int ask, std::string const &typed);
std::string TextureAnimTitle(int ask);
bool textureAnimPick = false;
static void TexturePolyVertexClicked(OpenLevelFile *open, uint8_t const *target, int32_t polygon, bool shift);
int const kTopologyEdgesMost = 4;
int textureMultiMode = 0;   // Copy Texturing > From Multiple / To Multiple: 1 the first reference polygon of the polygons to copy from, 2 the second, 3 and 4 the same for the polygons to copy to, 5 waiting for T (topology) or P (relative position)
static void TextureMultiClicked(uint8_t const *id, int32_t polygon);
static void TextureMultiCommand(int32_t command);
void TextureMultiDo(bool relative);
double textureGotoDistance = 2500.0;
void TextureMapEdit(int32_t command);
void TextureMapStepPicture(int step);
static void TextureWindowRotateNodes(int quarters);
bool textureWindowOn = false;
int textureWinScale = 0;   // 0: fit
bool TextureWindowLoad();
void TextureWindowApply();
bool textureWinAllNodes = false;   // Toggle Node Selection: the arrow keys move every node
bool LoadTheMenus();
static void TextureWindowBars(bool on);
static void TextureChooserOpen();
bool textureAnimCopyPick = false;
int textureCopyObject = 0;   // Copy Texturing > Object: 1 waiting for the object to copy from, 2 for an object to copy to
static uint8_t textureCopyObjectFrom[8];
static void TextureCopyObjectClicked(uint8_t const *target);
static void TextureAnimCopyClicked(uint8_t const *id, int32_t polygon);
bool AnimPlayStop();
void AnimPlayTick();
static bool textureSelectOnGoto = false;   // 0xd4 Select on GotoPoly (DAT_00914d98): going to a polygon (G, the Find items) also selects it (FUN_00563100 0x5633c2)
void TextureGoto(int direction);
bool textureCopyPending = false;   // 0x31 Copy Texturing > Polygon (key U): the next click names the polygon whose coordinates and picture the selected polygons take ("Get UV coords from which polygon")
bool textureInvertPending = false;   // 0x101 was chosen: the next click names the object to invert (A: all shapes, right click or Esc: cancel)

// TEXTURE MAP's held states, cleared on entry and when it is left (reassess/mode-entry-exit-frost.md): FUN_00544510 0x544551 sets the selection mode DAT_00914d9c = 0 on every entry
// (TM-E1); FUN_0054bfd0 frees everything of the mode on its Exit (TM-X2): no corners held, no plane or rectangle or invert waiting, no multiple copy half done
void TextureModeStateReset()
{
	textureSelectMode = 0;
	host::PolyEdit_VertexClear(&polyVertices);
	texturePlaneMode = 0;
	textureRectActive = false;
	textureRectAsked = false;
	textureRectShift = false;
	textureMultiMode = 0;
	textureInvertPending = false;
}
int textureAsk = 0;                  // the question kPromptTextureMap holds: 1 = the angle, 2 = the arrow step sizes

static const char *TextureModeWords(int mode)
{
	return mode == 0 ? "by Polygon" : mode == 1 ? "by Object" : mode == 2 ? "Rect (In or On)" : mode == 3 ? "Rect (In)" : mode == 4 ? "Vertex" : "by Element";
}

// Whether polygon number `polygon` of the object `id` is selected now
size_t TextureSelectedIndex(uint8_t const *id, int32_t polygon)
{
	for (size_t i = 0; i < texturePolygons.size(); i++)
	{
		if (texturePolygons[i].polygon == polygon && memcmp(texturePolygons[i].id, id, 8) == 0)
		{
			return i;
		}
	}

	return texturePolygons.size();
}

// Selects (or deselects) one polygon; false when the selection is full
// Select As (Options > Select As > Triangle 0xe / Quad 0xf; FUN_0054d260 sets the state's +0x9c to 3 or 4): with Quad, a polygon that was merged with another (the file's merge data SHPMRGDT: a quad the game made of two triangles) is selected - and deselected - together with its partner
// (FUN_0056db80 selects the partner, FUN_005734a0 does it for the selection already made when Quad is chosen). NAMED: inteng's own default of the state is not read; this editor starts with Triangle.
static int textureSelectAs = 3;

// FAKE MERGE DATA (Options > Fake Merge Data 0x143; FUN_005677d0 on, FUN_00567950 off; the message "Fake merge data turned on" / "turned off"): the pairing of triangles into quads (the merge data SHPMRGDT: DAT_00914d3c) is swapped for an in-memory COPY (DAT_00914d44) that Merge and Unmerge change
// without the file's chunk being written (FUN_00566d60 writes the pairing into SHPMRGDT only when it is off); turning it off puts the real pairing back, and a pair the copy has parted is parted in the real one too. HERE the pairing everything asks for is TexturePartnerOf: the file's while the
// switch is off, the copy's (made from the file's on first use of a shape) while it is on; TextureFakePairSet is for Merge / Unmerge (the poly edit group) to change the copy. NAMED: until those two exist nothing changes the copy, so the switch changes nothing yet that can be seen.
static bool textureFakeMerge = false;
static std::map<std::string, std::vector<int32_t>> textureFakePairs;

static int32_t TexturePartnerOf(OpenLevelFile *open, uint8_t const *id, int32_t polygon)
{
	std::string const key((char const *)id, 8);
	auto found = textureFakeMerge ? textureFakePairs.find(key) : textureFakePairs.end();

	if (textureFakeMerge && found == textureFakePairs.end())
	{
		std::vector<int32_t> numbers;
		bool hasMerge = false;

		if (open && open->Level() && host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge) && hasMerge)
		{
			found = textureFakePairs.emplace(key, numbers).first;
		}
		else
		{
			found = textureFakePairs.emplace(key, std::vector<int32_t>()).first;
		}
	}

	if (found != textureFakePairs.end())
	{
		return polygon >= 0 && (size_t)polygon < found->second.size() && found->second[(size_t)polygon] >= 0 && (size_t)found->second[(size_t)polygon] < found->second.size() ? found->second[(size_t)polygon] : -1;
	}

	std::vector<int32_t> numbers;
	bool hasMerge = false;

	if (open && open->Level() && host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge) && hasMerge && polygon >= 0 && (size_t)polygon < numbers.size() && numbers[(size_t)polygon] >= 0 && (size_t)numbers[(size_t)polygon] < numbers.size())
	{
		return numbers[(size_t)polygon];
	}

	return -1;
}

[[maybe_unused]] static void TextureFakePairSet(OpenLevelFile *open, uint8_t const *id, int32_t polygon, int32_t partner)
{
	TexturePartnerOf(open, id, polygon);   // makes the copy of that shape

	auto found = textureFakePairs.find(std::string((char const *)id, 8));

	if (textureFakeMerge && found != textureFakePairs.end() && polygon >= 0 && (size_t)polygon < found->second.size())
	{
		found->second[(size_t)polygon] = partner;
	}
}

static bool TextureSelectOne(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon, bool select);

bool TextureSelectPolygon(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon, bool select)
{
	if (!TextureSelectOne(id, isModule, moduleIndex, polygon, select))
	{
		return false;
	}

	if (textureSelectAs == 4)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		int32_t const partner = TexturePartnerOf(open, id, polygon);

		if (partner >= 0)
		{
			return TextureSelectOne(id, isModule, moduleIndex, partner, select);
		}
	}

	return true;
}

static bool TextureSelectOne(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon, bool select)
{
	size_t const at = TextureSelectedIndex(id, polygon);

	if (!select)
	{
		if (at < texturePolygons.size())
		{
			texturePolygons.erase(texturePolygons.begin() + (std::ptrdiff_t)at);
		}

		return true;
	}

	if (at < texturePolygons.size())
	{
		return true;
	}

	if (texturePolygons.size() >= kTexturePolygonsMost)
	{
		return false;
	}

	TexturePolygon one;

	memcpy(one.id, id, 8);
	one.polygon = polygon;
	one.isModule = isModule;
	one.moduleIndex = isModule ? moduleIndex : -1;
	texturePolygons.push_back(one);
	return true;
}

// The polygons of the ELEMENT the polygon `first` is in (see above): the ones reachable through shared points
static std::vector<int32_t> TextureElementOf(model::ShapeModel const &shape, int32_t first)
{
	std::vector<int32_t> element;

	if (first < 0 || (size_t)first >= shape.polygons.size())
	{
		return element;
	}

	std::vector<std::vector<int32_t>> byPoint(shape.points.size());
	std::map<std::array<int32_t, 3>, int32_t> sameAs;   // the first point number that has these coordinates
	std::vector<int32_t> pointClass(shape.points.size());

	for (size_t p = 0; p < shape.points.size(); p++)
	{
		std::array<int32_t, 3> const key = { shape.points[p].x, shape.points[p].y, shape.points[p].z };
		auto const found = sameAs.find(key);

		pointClass[p] = found == sameAs.end() ? (sameAs[key] = (int32_t)p) : found->second;
	}

	for (size_t i = 0; i < shape.polygons.size(); i++)
	{
		for (int c = 0; c < shape.polygons[i].cornerCount; c++)
		{
			int32_t const point = shape.polygons[i].corners[c];

			if (point >= 0 && (size_t)point < pointClass.size())
			{
				byPoint[(size_t)pointClass[(size_t)point]].push_back((int32_t)i);
			}
		}
	}

	std::vector<bool> seen(shape.polygons.size(), false);
	std::vector<int32_t> queue = { first };

	seen[(size_t)first] = true;

	for (size_t head = 0; head < queue.size(); head++)
	{
		int32_t const here = queue[head];

		element.push_back(here);

		for (int c = 0; c < shape.polygons[(size_t)here].cornerCount; c++)
		{
			int32_t const point = shape.polygons[(size_t)here].corners[c];

			if (point < 0 || (size_t)point >= pointClass.size())
			{
				continue;
			}

			for (int32_t const other : byPoint[(size_t)pointClass[(size_t)point]])
			{
				if (!seen[(size_t)other])
				{
					seen[(size_t)other] = true;
					queue.push_back(other);
				}
			}
		}
	}

	return element;
}

void TextureForEveryShape(OpenLevelFile *open, std::function<void(uint8_t const *id, bool isModule, int32_t moduleIndex)> const &each);

// 0xfb / 0xfc: the polygons with a corner (In or On) or every corner (In) inside the rectangle dragged on the view (FUN_0056e360, called from FUN_005590c0 when the left button is let go). The corners are put on the view the way the game draws
// them (pick_ray.h, PickRay_Project) and the rectangle is in the view's pixels, the corners' places ahead of the camera only; NAMED: inteng projects with its own view (its near plane and scale are not read), this uses the drawn view.
void TextureSelectRect(int x0, int y0, int x1, int y1, int viewW, int viewH, bool insideOnly, bool deselect)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::PickCamera camera;

	if (!open || !open->Level() || !PickGame_Camera(viewW, viewH, &camera))
	{
		statusWords = "Texture Map: there is no frame to put the rectangle on";
		return;
	}

	if (x1 < x0)
	{
		std::swap(x0, x1);
	}

	if (y1 < y0)
	{
		std::swap(y0, y1);
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	size_t chosen = 0;
	bool full = false;

	TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
	{
		model::ShapeModel shape;
		std::vector<int32_t> points, normals;

		if (full || host::FilePick_World(open, open->Level(), id, &shape, &points, &normals) != host::FilePickResult::Done)
		{
			return;
		}

		std::vector<char> inside(shape.points.size(), 0);

		for (size_t p = 0; p < shape.points.size() && p * 3 + 2 < points.size(); p++)
		{
			int32_t const world[3] = { (int32_t)(points[p * 3] * scale), (int32_t)(points[p * 3 + 1] * scale), (int32_t)(points[p * 3 + 2] * scale) };
			double px = 0, py = 0, ahead = 0;

			inside[p] = host::PickRay_Project(camera, world, &px, &py, &ahead) && px >= x0 && px <= x1 && py >= y0 && py <= y1;
		}

		for (size_t i = 0; i < shape.polygons.size() && !full; i++)
		{
			int in = 0;

			for (int c = 0; c < shape.polygons[i].cornerCount; c++)
			{
				int32_t const point = shape.polygons[i].corners[c];

				in += point >= 0 && (size_t)point < inside.size() && inside[(size_t)point] ? 1 : 0;
			}

			bool const take = insideOnly ? in == shape.polygons[i].cornerCount : in > 0;

			if (take)
			{
				if (!TextureSelectPolygon(id, isModule, moduleIndex, (int32_t)i, !deselect))
				{
					full = true;
				}
				else
				{
					chosen++;
				}
			}
		}
	});

	ShowTheTexturePolygons();
	textureWords = std::string("Texture Map: the rectangle ") + (deselect ? "deselected " : "selected ") + std::to_string(chosen) + " polygons (" + (insideOnly ? "Rect (In)" : "Rect (In or On)") + "), " + std::to_string(texturePolygons.size()) + " selected" + (full ? " (the selection is full)" : "");
	statusWords = textureWords.c_str();
}

// Every shape of the level the mode works on, once each: the modules' and the placed objects' (imported shapes, which inteng hides here, are left out)
void TextureForEveryShape(OpenLevelFile *open, std::function<void(uint8_t const *id, bool isModule, int32_t moduleIndex)> const &each)
{
	for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
	{
		uint8_t id[8];

		if (ModuleObject(open, m, id) && (textureShowImported || !host::PolygonItems_ShapeHasOwnPictureList(open->Level(), id)))
		{
			each(id, true, m);
		}
	}

	std::vector<std::vector<uint8_t>> active;

	LevelView_ActiveObjectIds(&active);

	for (std::vector<uint8_t> const &id : active)
	{
		if (id.size() >= 8 && (textureShowImported || !host::PolygonItems_ShapeHasOwnPictureList(open->Level(), id.data())))
		{
			each(id.data(), false, -1);
		}
	}
}

// Selects (or deselects) every polygon of one object
static size_t TextureSelectObject(OpenLevelFile *open, uint8_t const *id, bool isModule, int32_t moduleIndex, bool select, bool *full)
{
	model::ShapeModel shape;
	std::string why;
	size_t done = 0;

	if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
	{
		return 0;
	}

	for (size_t p = 0; p < shape.polygons.size(); p++)
	{
		if (!TextureSelectPolygon(id, isModule, moduleIndex, (int32_t)p, select))
		{
			*full = true;
			break;
		}

		done++;
	}

	return done;
}

// 0xff: the polygons within `degrees` of straight up, in every shape
static void TextureSelectByAngle(int degrees)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Texture Map: no level is open";
		return;
	}

	uint64_t const piBits = 0x400921FB54524550ull, perDegreeBits = 0x3F76C16C16C16C17ull;   // the doubles at 0x5afb48 and 0x5afb50: pi and 1/180
	double pi, perDegree;

	memcpy(&pi, &piBits, sizeof(pi));
	memcpy(&perDegree, &perDegreeBits, sizeof(perDegree));

	double const scaled = cos((double)degrees * perDegree * pi) * 65536.0;
	int32_t const threshold = (int32_t)scaled;   // __ftol: cut toward zero
	size_t chosen = 0;
	bool full = false;

	TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
	{
		model::ShapeModel shape;
		std::vector<int32_t> points, normals;

		if (full || host::FilePick_World(open, open->Level(), id, &shape, &points, &normals) != host::FilePickResult::Done)
		{
			return;
		}

		for (size_t p = 0; p < shape.polygons.size() && p * 3 + 2 < normals.size(); p++)
		{
			// normal . (0, -65536, 0) >> 16, term by term as FUN_004b3170 does: the y term only
			int32_t const dot = (int32_t)(((int64_t)normals[p * 3 + 1] * -65536) >> 16);

			if (dot >= threshold)
			{
				if (!TextureSelectPolygon(id, isModule, moduleIndex, (int32_t)p, true))
				{
					full = true;
					return;
				}

				chosen++;
			}
		}
	});

	ShowTheTexturePolygons();
	textureWords = "Texture Map: selected the polygons within " + std::to_string(degrees) + " degrees of vertical: " + std::to_string(chosen) + " polygons, " + std::to_string(texturePolygons.size()) + " selected" + (full ? " (the selection is full)" : "");
	statusWords = textureWords.c_str();
}

// 0x101: every polygon of the object toggled (selected ones deselected, the others selected); allShapes: of every shape
void TextureInvert(OpenLevelFile *open, uint8_t const *id, bool isModule, int32_t moduleIndex, bool allShapes)
{
	size_t toggled = 0;
	bool full = false;

	auto const one = [&](uint8_t const *objectId, bool module, int32_t index)
	{
		model::ShapeModel shape;
		std::string why;

		if (host::ShapeEdit_Read(open->Level(), open, objectId, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			return;
		}

		for (size_t p = 0; p < shape.polygons.size() && !full; p++)
		{
			bool const was = TextureSelectedIndex(objectId, (int32_t)p) < texturePolygons.size();

			if (!TextureSelectPolygon(objectId, module, index, (int32_t)p, !was))
			{
				full = true;
			}

			toggled++;
		}
	};

	if (allShapes)
	{
		TextureForEveryShape(open, one);
	}
	else
	{
		one(id, isModule, moduleIndex);
	}

	ShowTheTexturePolygons();
	textureWords = std::string("Texture Map: selection inverted over ") + (allShapes ? "every shape" : "the object") + ": " + std::to_string(toggled) + " polygons toggled, " + std::to_string(texturePolygons.size()) + " selected" + (full ? " (the selection is full)" : "");
	statusWords = textureWords.c_str();
}

void TextureWindowToggle();
static void TextureWindowReset();

void TextureMapCommand(int32_t command)
{
	switch (command)
	{
		case 0x9:
			TextureWindowReset();   // the texture screen's Reset Source Rect
			break;
		case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe:
		{
			textureSelectMode = command == 0xf9 ? 0 : command == 0xfa ? 1 : command == 0xfb ? 2 : command == 0xfc ? 3 : command == 0xfd ? 4 : 5;
			host::PolyEdit_VertexClear(&polyVertices);   // inteng takes the held corners back whenever a selection mode is chosen
			textureWords = std::string("Texture Map: selecting ") + TextureModeWords(textureSelectMode) +
				(textureSelectMode == 4 ? " (a left click takes the corner nearest the click of the polygon under it, up to 4 of one shape; Shift takes one out; then Poly edit > Create poly)" :
				" (a left click selects, Shift deselects; Ctrl takes the whole object)");
			statusWords = textureWords.c_str();
			break;
		}
		case 0x10e:
			TexturePolyCreate();
			break;
		case 0xc3:
			// FUN_0054d260 case 0xc3: a YES / NO question (FUN_00474a40, shown by FUN_0057c800) with these words; Yes deletes (FUN_0056fd40, FUN_0056e0a0)
			DialogConfirm("Are you sure that you want to delete these polygons?", [](bool yes)
			{
				if (yes)
				{
					TexturePolyDeleteNow();
				}
				else
				{
					statusWords = "Texture Map: Delete polys: nothing deleted";
				}
			});
			break;
		case 0xc9:
			// case 0xc9: a YES / NO question, then FUN_005639d0 flags them
			DialogConfirm("Would you like polygons with 0 normals deleted?", [](bool yes)
			{
				if (yes)
				{
					TexturePolyZeroNormalsNow();
				}
				else
				{
					statusWords = "Texture Map: Delete 0-normal polys: nothing flagged";
				}
			});
			break;
		case 0x119:
			TexturePolyMerge();
			break;
		case 0x11a:
			TexturePolyUnmerge();
			break;
		case 0xc7: case 0xc8:
			textureAsk = command == 0xc7 ? 22 : 23;
			memset(keyDown, 0, sizeof(keyDown));
			promptFor = kPromptTextureMap;
			promptText.clear();
			break;
		case 0xc4:
			TexturePolyRestore();
			break;
		case 0xc5:
			TexturePolyShowHide(false);
			break;
		case 0xc6:
			TexturePolyShowHide(true);
			break;
		case 0xff:
		{
			// case 0xff: a TEXT dialog with one field (FUN_00474170, FUN_00474340 reads it); out of range is a MESSAGE "Angle needs to be from 0 to 180"; then TextureSelectByAngle (FUN_0056e240) runs
			host::DialogField angle;

			angle.text = "";
			DialogText("Select polygons based on angle away from vertical.\nPlease enter an angle from 0 to 180", { angle }, [](int32_t answer, std::vector<std::string> const &texts)
			{
				if (answer != host::kDialogOk || texts.empty())
				{
					return;
				}

				char *end = nullptr;
				long const degrees = strtol(texts[0].c_str(), &end, 10);

				if (texts[0].empty() || !end || *end != '\0' || degrees < 0 || degrees > 180)
				{
					statusWords = "Angle needs to be from 0 to 180";
					DialogMessage("Angle needs to be from 0 to 180");
					return;
				}

				TextureSelectByAngle((int)degrees);
			});
			break;
		}
		case 0x100:
		{
			// case 0x100: a TEXT dialog titled "Arrow Selection Step Size" with the labelled fields "Vertical" and "Horizontal" (FUN_00474350; FUN_004747d0 sets them up), holding the current steps
			host::DialogField vertical, horizontal;

			vertical.label = "Vertical";
			vertical.text = std::to_string(textureArrowVertical);
			horizontal.label = "Horizontal";
			horizontal.text = std::to_string(textureArrowHorizontal);
			DialogText("Arrow Selection Step Size", { vertical, horizontal }, [](int32_t answer, std::vector<std::string> const &texts)
			{
				if (answer != host::kDialogOk || texts.size() < 2)
				{
					return;
				}

				long const v = strtol(texts[0].c_str(), nullptr, 10);
				long const h = strtol(texts[1].c_str(), nullptr, 10);

				textureArrowVertical = v < 1 ? 1 : (v > 1000000 ? 1000000 : (int)v);
				textureArrowHorizontal = h < 1 ? 1 : (h > 1000000 ? 1000000 : (int)h);
				textureWords = "Texture Map: arrow selection step size: vertical " + std::to_string(textureArrowVertical) + ", horizontal " + std::to_string(textureArrowHorizontal) + " (S and an arrow key select that many polygons further)";
				statusWords = textureWords.c_str();
			});
			break;
		}
		case 0x11: case 0x12:
		{
			// Polys Shown: Untextured (0x11, FUN_0055c1d0: DAT_005fd6b4 = 0) / Textured (0x12: DAT_005fd6b4 = 1), each then FUN_0055e240 redraws; the editor's own switch for that is View > Textured (view_toggles.h, the same variable)
			bool const want = command == 0x12;

			if (ViewToggles_Now().textured != want)
			{
				ViewToggles_UseNow(host::ViewToggle::Textures);
			}

			statusWords = want ? "Texture Map: Polys Shown: Textured" : "Texture Map: Polys Shown: Untextured";
			break;
		}
		case 0x27:
			textPanelText = kIntengTextureMapHelpText;   // Help > General (FUN_00557320)
			break;
		case 0x28:
			textPanelText = kIntengTextureMapMovementText;   // Help > Movement (FUN_005573c0)
			break;
		case 0xcd:
			// Show Gouraud (FUN_0054d260 0xcd: DAT_00914d6c, then FUN_00542a40 redraws): the editor's own Gouraud switch
			ViewToggles_UseNow(host::ViewToggle::GouraudSwitch);
			statusWords = "Texture Map: Show Gouraud toggled";
			break;
		case 0xd4:
			textureSelectOnGoto = !textureSelectOnGoto;
			statusWords = textureSelectOnGoto ? "Texture Map: Select on GotoPoly on" : "Texture Map: Select on GotoPoly off";
			break;
		case 0x10f:
			textureAsk = 3;
			memset(keyDown, 0, sizeof(keyDown));
			promptFor = kPromptTextureMap;
			promptText.clear();
			break;
		case 0x110: case 0x111: case 0x112:
			textureRepeatMode = command - 0x110;
			textureWords = std::string("Texture Map: ") + (textureRepeatMode == 0 ? "Default to 1X1" : textureRepeatMode == 1 ? "Maintain Used Area" : "Maintain Num Repeats") + " (kept; this editor's window does not follow the current texture)";
			statusWords = textureWords.c_str();
			break;
		case 0x14:
			textureAddOne = textureAddOne ? 0 : 1;
			textureWords = std::string("Texture Map: Add 1 to texture area ") + (textureAddOne ? "on: the texture window is the picture's size plus 1 (the flips, turns, wrap and tile use it)" : "off: the texture window is the picture's own size");
			statusWords = textureWords.c_str();
			break;
		case 0x145:
			textureShowImported = !textureShowImported;
			textureWords = textureShowImported ? "Texture Map: Show Imported Shapes on (they can be selected, but what is done to them is never saved: inteng rebuilds only shapes with no SHPEXTFL)" : "Texture Map: Show Imported Shapes off (imported shapes are left out)";
			statusWords = textureWords.c_str();
			break;
		case 0xe: case 0xf:
		{
			textureSelectAs = command == 0xe ? 3 : 4;

			if (textureSelectAs == 4)
			{
				std::vector<TexturePolygon> const now = texturePolygons;

				for (TexturePolygon const &one : now)
				{
					TextureSelectPolygon(one.id, one.isModule, one.moduleIndex, one.polygon, true);   // each selected polygon takes its partner too
				}

				ShowTheTexturePolygons();
			}

			textureWords = textureSelectAs == 3 ? "Texture Map: selecting as triangles (one polygon of the file at a time)" : "Texture Map: selecting as quads (a polygon is selected with the one it was merged with)";
			statusWords = textureWords.c_str();
			break;
		}
		case 0x13:
			textureAsk = 13;
			memset(keyDown, 0, sizeof(keyDown));
			promptFor = kPromptTextureMap;
			promptText.clear();
			break;
		case 0x2a:
			TextureChooserOpen();
			break;
		// THE TEXTURE WINDOW'S OWN MENUS (bar page 3 while Tab has the window open): Mode > Polygon 0x2 leaves it (FUN_00551f80 case 2), Texture 0x3 is the mode it is in; X-Flip 0x6 / Y-Flip 0x7 (FUN_00558880 / FUN_00558900), Next / Prev Texture 0xa / 0xb (FUN_00555dd0 +1 / -1),
		// Rotate Nodes: 90 / 180 / 270 Texture Centre 0x19 / 0x1a / 0x1b and 90 / 180 / 270 Node Centre 0x16 / 0x17 / 0x18 (FUN_00556ce0(angle, flag)), Toggle Texture Resize 0x103 (state +0x48), Set Default Size x1/4 .. x3 0xec-0xf0 (FUN_0055d350), Toggle Node Selection 0x102.
		// NAMED: the scale of the picture is a whole number here, so x1/4, x1/2 and x1 all show it at 1; Toggle Texture Resize only says what it is (this window has no resizing of the used part of the texture yet).
		case 0x2:
			if (textureWindowOn)
			{
				TextureWindowToggle();
			}
			else
			{
				statusWords = "Texture Map: Mode > Polygon: the polygon screen is the one shown";
			}

			break;
		case 0x3:
			statusWords = "Texture Map: Mode > Texture: the texture window edits the polygon's nodes (Tab opens it)";
			break;
		case 0x6: case 0x7: case 0x19: case 0x1b:
			TextureMapEdit(command);

			if (textureWindowOn)
			{
				TextureWindowLoad();
			}

			break;
		case 0x1a:
			TextureMapEdit(0x19);
			TextureMapEdit(0x19);

			if (textureWindowOn)
			{
				TextureWindowLoad();
			}

			break;
		case 0xa: case 0xb:
			TextureMapStepPicture(command == 0xa ? 1 : -1);
			break;
		case 0x16: case 0x17: case 0x18:
			TextureWindowRotateNodes(command - 0x15);
			break;
		case 0xec: case 0xed: case 0xee: case 0xef: case 0xf0:
			textureWinScale = command == 0xef ? 2 : (command == 0xf0 ? 3 : 1);
			statusWords = "Texture Map: the texture window's size is set";
			break;
		case 0x102:
			textureWinAllNodes = !textureWinAllNodes;
			statusWords = textureWinAllNodes ? "Texture Map: the arrow keys move every node" : "Texture Map: the arrow keys move the selected node";
			break;
		case 0x103:
			statusWords = "Texture Map: Toggle Texture Resize: the window does not resize the used part of the texture yet";
			break;
		case 0x2b:
			TextureMapDeselectAll();
			statusWords = "Texture Map: all polygons deselected";
			break;
		case 0x2e: case 0x2f:
			texturePlaneMode = texturePlaneMode == (command == 0x2f ? 2 : 1) ? 0 : (command == 0x2f ? 2 : 1);
			statusWords = texturePlaneMode ? (texturePlaneMode == 2 ? "Tiling texture: Choose Polygon for plane" : "Wrap: Choose Polygon for plane") : "Texture Map: plane pick cancelled";
			break;
		case 0x42:
			TextureHide(false);
			break;
		case 0x43:
			TextureHide(true);
			break;
		case 0x45: case 0x46:
			TextureShowAll(command == 0x46);
			break;
		case 0x4c:
			TextureFlagView(0, false, "");
			break;
		case 0x11c:   // View: Non-Planar Quads, FUN_005428c0(0x80000000): the polygons Split Object marked (the only place inteng sets that flag)
			TextureFlagView(0x80000000u, false, "Non-Planar Quads (marked by Split Object)");
			break;
		case 0x7d:
			TextureFlagView(0, true, "Animation");
			break;
		case 0x133:
		{
			// case 0x133: a TEXT dialog "Shift uv coordinates by how much?" with the fields "u shift" and "v shift" (FUN_005720e0 applies them)
			host::DialogField u, v;

			u.label = "u shift";
			u.text = "";
			v.label = "v shift";
			v.text = "";
			DialogText("Shift uv coordinates by how much?", { u, v }, [](int32_t answer, std::vector<std::string> const &texts)
			{
				if (answer != host::kDialogOk || texts.size() < 2)
				{
					return;
				}

				char *endU = nullptr, *endV = nullptr;
				double const first = strtod(texts[0].c_str(), &endU);
				double const second = strtod(texts[1].c_str(), &endV);

				if (texts[0].empty() || endU == texts[0].c_str() || texts[1].empty() || endV == texts[1].c_str())
				{
					statusWords = "Texture Map: Shift UV Coords: type the u shift and the v shift";
					return;
				}

				TextureShiftOrScale(false, first, second);
			});
			break;
		}
		case 0x134:
		{
			// case 0x134: a TEXT dialog "How much would you like to scale the uv coordinates by?" (FUN_004747f0 and FUN_00474a10 set its field, FUN_00572200 applies it)
			host::DialogField factor;

			factor.text = "";
			DialogText("How much would you like to scale the uv coordinates by?", { factor }, [](int32_t answer, std::vector<std::string> const &texts)
			{
				if (answer != host::kDialogOk || texts.empty())
				{
					return;
				}

				char *end = nullptr;
				double const scale = strtod(texts[0].c_str(), &end);

				if (texts[0].empty() || end == texts[0].c_str() || scale == 1.0 || scale <= -100000.0 || scale >= 100000.0)
				{
					statusWords = "Texture Map: the coordinates stay as they are";
					return;
				}

				TextureShiftOrScale(true, scale, 0);
			});
			break;
		}
		case 0x6e: case 0x6f: case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: case 0x78: case 0x79: case 0x68: case 0x69: case 0x6a: case 0x99: case 0x9a: case 0x9b:
			for (TextureFlagRow const &row : kTextureFlagRows)
			{
				if (command == row.add || command == row.remove)
				{
					TextureFlagsSet(row.mask, command == row.add, row.name);
				}
				else if (command == row.view)
				{
					TextureFlagView(row.mask, false, row.name);
				}
			}
			break;
		case 0x143:
			textureFakeMerge = !textureFakeMerge;
			textureFakePairs.clear();
			textureWords = textureFakeMerge ? "Texture Map: Fake merge data turned on (the pairing of triangles is a copy that Merge and Unmerge change without writing the file's merge data)" : "Texture Map: Fake merge data turned off (the file's pairing is used again)";
			statusWords = textureWords.c_str();
			break;
		case 0xb4:
			textureFlagList = !textureFlagList;
			textureFlagLines.clear();
			textureWords = textureFlagList ? "Texture Map: the flag list is on (the polygon under the pointer is described at the top of the view)" : "Texture Map: the flag list is off";
			statusWords = textureWords.c_str();
			break;
		case 0x13f: case 0x140:
			TextureMultiCommand(command);
			break;
		case 0xb5:
			textureCopyObject = 1;
			textureCopyPending = false;
			statusWords = "Select object to copy from (a click on one of its polygons; Esc cancels)";
			break;
		case 0x9d: case 0x9e: case 0x9f: case 0xa4: case 0xa6: case 0xa7: case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xad: case 0xae: case 0xaf: case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xa2: case 0xa3:
			TextureAnimCommand(command);
			break;
		case 0xb7: case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc:
			TextureFind(command);
			break;
		case 0x31:
			// FUN_0054d51f: only when something is selected; FUN_0055c210(0) asks "Get UV coords from which polygon" and waits for a click (right click cancels)
			if (texturePolygons.empty())
			{
				statusWords = "Texture Map: nothing is selected to copy a texturing onto";
			}
			else
			{
				textureCopyPending = true;
				statusWords = "Get UV coords from which polygon: click one (Esc: cancel)";
			}

			break;
		case 0x101:
			textureInvertPending = true;
			statusWords = "Invert selection for which object? Click one. A : All Shapes, Esc : Cancel";
			break;
		default:
			statusWords = "Texture Map: that item is not built yet";
			break;
	}
}

// The typed answer of kPromptTextureMap
void TextureMapTypedAnswer(std::string const &typed)
{
	int const ask = textureAsk;

	textureAsk = 0;

	if (TextureAnimTyped(ask, typed))
	{
		return;
	}

	if (ask == 24)
	{
		// the new object's name (the last split waiting); empty keeps the default
		if (!polySplit.empty())
		{
			polySplit.back().name = typed;
			textureWords = "Texture Map: Split Object: the new object will be called " + (typed.empty() ? std::string("Split<n> (the default)") : typed);
			statusWords = textureWords.c_str();
		}

		return;
	}

	if (ask == 22 || ask == 23)
	{
		if (typed == "y" || typed == "Y" || typed == "yes" || typed == "Yes")
		{
			TexturePolySplitNow(ask == 23);
		}
		else
		{
			statusWords = "Texture Map: Split Object: nothing marked";
		}

		return;
	}

	if (ask == 20 || ask == 21)
	{
		if (typed == "y" || typed == "Y" || typed == "yes" || typed == "Yes")
		{
			if (ask == 20)
			{
				TexturePolyDeleteNow();
			}
			else
			{
				TexturePolyZeroNormalsNow();
			}
		}
		else
		{
			statusWords = ask == 20 ? "Texture Map: Delete polys: nothing deleted" : "Texture Map: Delete 0-normal polys: nothing flagged";
		}

		return;
	}

	if (ask == 13)
	{
		long minutes = -1, prompt = 0;

		if (sscanf_s(typed.c_str(), "%ld %ld", &minutes, &prompt) < 1 || minutes < 0 || minutes > 100000)
		{
			statusWords = "Texture Map: AutoSave: the frequency is a whole number of minutes, 0 for off";
			return;
		}

		textureAutoMinutes = (int)minutes;
		textureAutoPrompt = prompt != 0;
		textureAutoLast = GetTickCount();
		textureWords = minutes ? "Texture Map: Currently saving every " + std::to_string(minutes) + " minutes" + (textureAutoPrompt ? " (asking first)" : "") : "Texture Map: AutoSave is off";
		statusWords = textureWords.c_str();
		return;
	}

	if (ask == 15 || ask == 16)
	{
		double first = 0, second = 0;
		char *end = nullptr;

		first = strtod(typed.c_str(), &end);

		if (typed.empty() || end == typed.c_str())
		{
			statusWords = ask == 15 ? "Texture Map: Shift UV Coords: type the u shift and the v shift" : "Texture Map: Scale UV Coords: type the factor";
			return;
		}

		if (ask == 15)
		{
			char *end2 = nullptr;

			second = strtod(end, &end2);

			if (end2 == end)
			{
				statusWords = "Texture Map: Shift UV Coords: type the u shift and the v shift";
				return;
			}

			TextureShiftOrScale(false, first, second);
		}
		else if (first != 1.0 && first > -100000.0 && first < 100000.0)
		{
			TextureShiftOrScale(true, first, 0);
		}
		else
		{
			statusWords = "Texture Map: the coordinates stay as they are";
		}

		return;
	}

	if (ask == 14)
	{
		if (typed == "y" || typed == "Y" || typed == "yes" || typed == "Yes")
		{
			SaveTheCopy();
		}
		else
		{
			statusWords = "Texture Map: AutoSave: not saved this time";
		}

		return;
	}

	if (ask == 12)
	{
		bool const yes = typed == "y" || typed == "Y" || typed == "yes" || typed == "Yes";

		textureCopyObject = yes ? 2 : 0;
		statusWords = yes ? "Select object to copy to (Esc cancels)" : "Texture Map: copying texturing between objects is done";
		return;
	}

	if (ask == 1)
	{
		char *end = nullptr;
		long const degrees = strtol(typed.c_str(), &end, 10);

		if (typed.empty() || !end || *end != '\0' || degrees < 0 || degrees > 180)
		{
			statusWords = "Angle needs to be from 0 to 180";
			return;
		}

		TextureSelectByAngle((int)degrees);
	}
	else if (ask == 3)
	{
		// two words: 1 to 10 or inf (inteng's grid: infXinf, infX%d, %dXinf, %dX%d)
		int across = 0, down = 0;
		char first[16] = "", second[16] = "";

		if (sscanf_s(typed.c_str(), "%15s %15s", first, (unsigned)sizeof(first), second, (unsigned)sizeof(second)) == 2)
		{
			auto const one = [](char const *word) { return _stricmp(word, "inf") == 0 ? 11 : (atoi(word) >= 1 && atoi(word) <= 10 ? atoi(word) : 0); };

			across = one(first);
			down = one(second);
		}

		if (!across || !down)
		{
			statusWords = "Texture Map: the repeats are two words, each 1 to 10 or inf";
			return;
		}

		textureRepeatX = across;
		textureRepeatY = down;
		textureWords = std::string("Texture Map: the texture window is ") + (across == 11 ? "infinite" : std::to_string(across)) + "X" + (down == 11 ? "infinite" : std::to_string(down)) + " pictures (the flips, turns, wrap and tile use it)";
		statusWords = textureWords.c_str();
	}
	else if (ask == 2)
	{
		long vertical = 0, horizontal = 0;
		char *end = nullptr;

		vertical = strtol(typed.c_str(), &end, 10);

		if (end && end != typed.c_str())
		{
			horizontal = strtol(end, nullptr, 10);
		}

		textureArrowVertical = vertical < 1 ? 1 : (vertical > 1000000 ? 1000000 : (int)vertical);
		textureArrowHorizontal = horizontal < 1 ? 1 : (horizontal > 1000000 ? 1000000 : (int)horizontal);
		textureWords = "Texture Map: arrow selection step size: vertical " + std::to_string(textureArrowVertical) + ", horizontal " + std::to_string(textureArrowHorizontal) + " (S and an arrow key select that many polygons further)";
		statusWords = textureWords.c_str();
	}
}


// The Texture Map commands that act on the whole selection at once: X-Flip (inteng command 6), Y-Flip (7) and the two turns (0x19, 0x1b). inteng (FUN_0054d260, menu-reads-1-full.txt:9685-9770)
// does nothing at all when nothing is selected, otherwise marks the level changed and runs FUN_00570f40 over every selected polygon with the texture window's numbers; the maths is
// model::FlipU / FlipV / TurnA / TurnB (Vasquez, 70d4da7), put back through UvEdit_WritePolygons, one atomic write for each shape. THE WINDOW is inteng's own: the texture screen's state keeps a window
// that FUN_00555cd0 + FUN_00555dd0 set at the start to the WHOLE of its current texture, the smallest texture number the level loaded (listing-all-4.txt:20948-21165: +8, +0xc = 0, +0x10 = width + DAT_00914d30,
// +0x14 = height + DAT_00914d30, +0x48 and +0x4c their differences), and every selected polygon is moved over that one window whatever picture it uses. The editor has no texture screen yet, so
// the window stays what it is at the start: the whole of the lowest-numbered picture the level's shapes use (named approximation of "the smallest loaded texture number"), DAT_00914d30 (the "Add 1 to
// texture area" switch, command 0x14) taken as 0, its start value being unread. A polygon with no texture, a texture list shared with a polygon outside the selection, or a result past the texel limit is left as it was and counted. The keys are the words of
// inteng's own help (X, Y flip; E, R turn), the menu rows wait for the Texture Map menus to be put in. The view still shows the level as it was opened (the change is in the editor's bytes).
void TextureMapEdit(int32_t command)
{
	TextureDropImported();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Texture Map: no level is open";
		return;
	}

	if (texturePolygons.empty())
	{
		statusWords = "Texture Map: nothing is selected";
		return;
	}

	int32_t windowWidth = 0, windowHeight = 0;

	if (!LevelView_LowestPictureSize(&windowWidth, &windowHeight))
	{
		statusWords = "Texture Map: the game has no picture loaded to make the window of";
		return;
	}

	size_t changed = 0, notTextured = 0, left = 0, shapesRefused = 0;
	std::string firstWhy;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		LevelViewShapeOf shape = {};

		shape.isModule = lead.isModule;
		shape.moduleIndex = lead.moduleIndex;
		memcpy(shape.objectId, lead.id, 8);

		model::ShapeModel model;
		std::string why;
		std::vector<uint8_t> corners;
		std::vector<int32_t> numbers, toItem, firstFile;
		bool hasMerge = false;

		bool const mapped = host::ShapeEdit_Read(open->Level(), open, lead.id, &model, nullptr, &why) == host::ShapeEditResult::Done && LevelView_ItemCorners(shape, &corners) &&
			host::PolygonItems_ReadMergeData(open, open->Level(), lead.id, &numbers, &hasMerge) &&
			host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), model.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) == host::PolygonItemResult::Ok;

		std::vector<model::PolygonUv> polygons;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (done[i] || one.isModule != lead.isModule || one.moduleIndex != lead.moduleIndex || memcmp(one.id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			model::PolygonUv uv;
			std::string readWhy;
			if (!mapped || one.polygon < 0 || (size_t)one.polygon >= toItem.size() || host::UvEdit_ReadPolygon(open, open->Level(), lead.id, one.polygon, &uv, &readWhy) != host::UvEditResult::Done)
			{
				left++;
				continue;
			}

			if (!uv.textured)
			{
				notTextured++;
				continue;
			}

			model::TextureWindow window = TextureWindowOf(windowWidth, windowHeight, textureAddOne);
			model::UvModelResult const result = command == 6 ? model::FlipU(&uv, window) : command == 7 ? model::FlipV(&uv, window) : command == 0x19 ? model::TurnA(&uv, window) : model::TurnB(&uv, window);

			if (result != model::UvModelResult::Ok)
			{
				left++;

				if (firstWhy.empty())
				{
					firstWhy = model::UvModelWords(result);
				}

				continue;
			}

			polygons.push_back(uv);
		}

		if (polygons.empty())
		{
			continue;
		}

		std::string writeWhy;
		host::UvEditResult const written = host::UvEdit_WritePolygons(open, open->Level(), lead.id, polygons, &writeWhy);

		if (written == host::UvEditResult::Done)
		{
			changed += polygons.size();
			ShowTheNewUv(open, lead, polygons);
		}
		else
		{
			shapesRefused++;
			left += polygons.size();

			if (firstWhy.empty())
			{
				firstWhy = writeWhy.empty() ? host::UvEditWords(written) : writeWhy;
			}
		}
	}

	{
		// the corner nodes move the same way, over the same window
		TextureNodesFor(windowWidth, windowHeight);

		model::TextureWindow const window = TextureWindowOf(windowWidth, windowHeight, textureAddOne);

		if (command == 6) model::FlipU(&textureNodes, window);
		else if (command == 7) model::FlipV(&textureNodes, window);
		else if (command == 0x19) model::TurnA(&textureNodes, window);
		else model::TurnB(&textureNodes, window);
	}

	char const *const name = command == 6 ? "X-Flip" : command == 7 ? "Y-Flip" : command == 0x19 ? "Turn A" : "Turn B";

	textureWords = std::string("Texture Map: ") + name + " done on " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s");

	if (notTextured)
	{
		textureWords += ", " + std::to_string(notTextured) + " not textured";
	}

	if (left)
	{
		textureWords += ", " + std::to_string(left) + " left as they were" + (firstWhy.empty() ? "" : " (" + firstWhy + ")");
	}

	statusWords = textureWords.c_str();
}

// The Texture Map FLAG commands (FUN_0054d260, menu-reads-1-full.txt:9860-10175): each is up to three calls of FUN_0056fb70(mask, set) on every selected polygon, after FUN_00573570 (the undo
// record). FUN_0056fb70 sets (set 1) or clears (set 0) the mask's bits in the polygon's flag word (the file's SHPPOLYS +8, the game's iflag_* bits: Vasquez's model::SetPolygonFlags, which refuses
// setting 0x40000000, the one bit the game's loader refuses). The commands that only SHOW polygons with a flag (0x4f, 0x52, 0x55 ... the FUN_005428c0 ones) are not here: they colour the view. Commands
// 0x7a (FUN_0056fd40), 0x90 (FUN_005676e0) and the undo record itself are not read and not here. NAMED: a polygon with no texture cannot be written by UvEdit_WritePolygons (its deviation 3), so a flag
// command leaves it as it was and counts it; the undo record is not kept.
struct TextureFlagStep
{
	int32_t mask;
	bool set;
};

struct TextureFlagCommand
{
	int32_t command;
	TextureFlagStep steps[3];
	int stepCount;
};

static const TextureFlagCommand kTextureFlagCommands[] =
{
	{ 0x4d, { { 0x40000, false }, { 0x400, false }, { 0x4000000, true } }, 3 },
	{ 0x4e, { { 0x4000000, false } }, 1 },
	{ 0x50, { { 0x4000000, false }, { 0x40000, true }, { 0x400, false } }, 3 },
	{ 0x51, { { 0x40000, false } }, 1 },
	{ 0x94, { { 0x40000, false } }, 1 },
	{ 0x53, { { 0x40000, false }, { 0x4000000, false }, { 0x400, true } }, 3 },
	{ 0x84, { { 0x400, true } }, 1 },
	{ 0x54, { { 0x400, false } }, 1 },
	{ 0x85, { { 0x400, false } }, 1 },
	{ 0x56, { { 4, true } }, 1 },
	{ 0x57, { { 4, false } }, 1 },
	{ 0x59, { { 8, true } }, 1 },
	{ 0x5a, { { 8, false } }, 1 },
	{ 0x5c, { { 0x80, true } }, 1 },
	{ 0x8d, { { 0x80, true } }, 1 },
	{ 0x5d, { { 0x80, false } }, 1 },
	{ 0x8e, { { 0x80, false } }, 1 },
	{ 0x5f, { { 0x40, true } }, 1 },
	{ 0x60, { { 0x40, false } }, 1 },
	{ 0x62, { { 0x100, true } }, 1 },
	{ 0x7e, { { 0x100, true } }, 1 },
	{ 0x63, { { 0x100, false } }, 1 },
	{ 0x7f, { { 0x100, false } }, 1 },
	{ 0x65, { { 0x200, true } }, 1 },
	{ 0x81, { { 0x200, true } }, 1 },
	{ 0x66, { { 0x200, false } }, 1 },
	{ 0x82, { { 0x200, false } }, 1 },
	{ 0x68, { { 0x20000000, true } }, 1 },
	{ 0x69, { { 0x20000000, false } }, 1 },
	{ 0x7b, { { 0x20000000, false } }, 1 },
	{ 0x6b, { { 0x400000, true } }, 1 },
	{ 0x6c, { { 0x400000, false } }, 1 },
	{ 0x6e, { { 2, true } }, 1 },
	{ 0x6f, { { 2, false } }, 1 },
	{ 0x71, { { 1, true } }, 1 },
	{ 0x72, { { 1, false } }, 1 },
	{ 0x74, { { 0x20, true } }, 1 },
	{ 0x75, { { 0x20, false } }, 1 },
	{ 0x77, { { 0x10, true } }, 1 },
	{ 0x78, { { 0x10, false } }, 1 },
	{ 0x87, { { 0x800, true } }, 1 },
	{ 0x88, { { 0x800, false } }, 1 },
	{ 0x8a, { { 0x1000, true } }, 1 },
	{ 0x99, { { 0x1000, true } }, 1 },
	{ 0x8b, { { 0x1000, false } }, 1 },
	{ 0x9a, { { 0x1000, false } }, 1 },
	{ 0x91, { { 0x3e000, false } }, 1 },
	{ 0x93, { { 0x40000, true } }, 1 },
};

// FUN_0056fb70 on one polygon: the mask's bits set or cleared in its flag word. Setting 0x40000000 is refused (the game's loader refuses a level with it, chunk_shape.cpp:1051). (model::SetPolygonFlags is
// declared in uv_model.h but defined nowhere yet, so the rule is here.)
static bool ApplyFlagStep(model::PolygonUv *uv, int32_t mask, bool set)
{
	if (set && (mask & 0x40000000))
	{
		return false;
	}

	uv->flags = set ? (uv->flags | mask) : (uv->flags & ~mask);
	return true;
}

static void TextureMapFlags(int32_t command)
{
	TextureDropImported();
	TextureFlagCommand const *which = nullptr;

	for (TextureFlagCommand const &known : kTextureFlagCommands)
	{
		if (known.command == command)
		{
			which = &known;
			break;
		}
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!which || !open || !open->Level())
	{
		statusWords = "Texture Map: no such flag command, or no level is open";
		return;
	}

	if (texturePolygons.empty())
	{
		statusWords = "Texture Map: nothing is selected";
		return;
	}

	size_t changed = 0, notTextured = 0, left = 0;
	std::string firstWhy;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		std::vector<model::PolygonUv> polygons;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (done[i] || memcmp(one.id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			model::PolygonUv uv;
			std::string readWhy;

			if (host::UvEdit_ReadPolygon(open, open->Level(), lead.id, one.polygon, &uv, &readWhy) != host::UvEditResult::Done)
			{
				left++;
				continue;
			}

			if (!uv.textured)
			{
				notTextured++;
				continue;
			}

			bool ok = true;

			for (int k = 0; ok && k < which->stepCount; k++)
			{
				ok = ApplyFlagStep(&uv, which->steps[k].mask, which->steps[k].set);
			}

			if (!ok)
			{
				left++;
				continue;
			}

			polygons.push_back(uv);
		}

		if (polygons.empty())
		{
			continue;
		}

		std::string writeWhy;
		host::UvEditResult const written = host::UvEdit_WritePolygons(open, open->Level(), lead.id, polygons, &writeWhy);

		if (written == host::UvEditResult::Done)
		{
			changed += polygons.size();
		}
		else
		{
			left += polygons.size();
			firstWhy = firstWhy.empty() ? (writeWhy.empty() ? host::UvEditWords(written) : writeWhy) : firstWhy;
		}
	}

	static char hex[32];

	_snprintf_s(hex, sizeof(hex), _TRUNCATE, "0x%x", (unsigned)command);
	textureWords = std::string("Texture Map: flag command ") + hex + " done on " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s");

	if (notTextured)
	{
		textureWords += ", " + std::to_string(notTextured) + " not textured (left as they were)";
	}

	if (left)
	{
		textureWords += ", " + std::to_string(left) + " refused" + (firstWhy.empty() ? "" : " (" + firstWhy + ")");
	}

	statusWords = textureWords.c_str();
}

// TEXTURE MAP: SET TEXTURE (FUN_00555dd0 sets the window's current texture and, at its end, FUN_00570360(texture) puts it on every selected polygon; Next Texture 0xa and Prev Texture 0xb step the
// current texture by one and wrap between the smallest and the largest; the colour word of the written polygon, FUN_00572440: bits 0-11 = the picture's index (DAT_00914d34[the engine's texture number]), the
// UV list number's top four bits in bits 12-15 and its low 16 bits above, nothing else; p11-shape-handlers.md sections 54 and 68). The stock levels' shapes name NO pictures of their own (SHPTEXFN is empty
// in all of them: texnames_check.cpp): a polygon's picture number indexes the LEVEL's list of pictures, so setting a picture is writing that index, no name is added and no chunk changes size. The index of a
// picture is found from the polygons themselves: the game's texture number of a polygon's item against the file's colour word. NAMED: the pictures offered are the ones some textured polygon of the level
// uses (inteng's chooser shows every loaded texture), and they are stepped in the order of their numbers; a shape other objects share is the same bytes for all of them (inteng asks which to change,
// FUN_0055aa50); the keys Page Down and Page Up are ours (inteng's items are Next Texture 0xa and Prev Texture 0xb).
static int32_t textureCurrent = -1;               // the game's texture number of the current picture; -1 until the mode picks the lowest
static std::map<int32_t, int32_t> pictureIndexOf; // the game's texture number -> the level's picture index, from the polygons
static unsigned pictureIndexFor = ~0u;
static size_t pictureIndexConflicts = 0;
static size_t pictureIndexAmbiguous = 0;            // polygons whose picture index was seen with another game texture number: should be 0
static std::map<int32_t, int32_t> pictureTextureOf; // the level's picture index -> the game's texture number
// Shapes with their OWN picture list (SHPEXTFL with a BMPLSTST: 16 of Invasion's 381; the game numbers their pictures by that list): per shape (its 8-byte object ID), the game's texture number -> the shape's own index, from its polygons
static std::map<std::string, std::map<int32_t, int32_t>> ownPictureIndexOf;

static void BuildThePictureIndex(OpenLevelFile *open)
{
	if (pictureIndexFor == open->Generation() && !pictureIndexOf.empty())
	{
		return;
	}

	pictureIndexOf.clear();
	pictureIndexConflicts = 0;
	pictureIndexAmbiguous = 0;
	pictureTextureOf.clear();
	ownPictureIndexOf.clear();
	pictureIndexFor = open->Generation();

	auto const scan = [&](LevelViewShapeOf const &shape, uint8_t const *id)
	{
		model::ShapeModel shapeModel;
		std::string why;
		std::vector<uint8_t> corners;
		std::vector<int32_t> numbers, toItem, firstFile;
		bool hasMerge = false;

		if (host::ShapeEdit_Read(open->Level(), open, id, &shapeModel, nullptr, &why) != host::ShapeEditResult::Done || !LevelView_ItemCorners(shape, &corners) ||
			!host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge) ||
			host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), shapeModel.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) != host::PolygonItemResult::Ok)
		{
			return;
		}

		bool const ownList = host::PolygonItems_ShapeHasOwnPictureList(open->Level(), id);

		for (size_t p = 0; p < shapeModel.polygons.size(); p++)
		{
			int32_t texture = 0;

			if (!model::EngineTypeIsTextured(shapeModel.polygons[p].engineType) || !LevelView_ItemTexture(shape, toItem[p], &texture))
			{
				continue;
			}

			int32_t const index = shapeModel.polygons[p].colour & 0xfff;

			if (ownList)
			{
				ownPictureIndexOf[std::string((char const *)id, 8)].emplace(texture, index);   // a shape with its own list: its numbers mean only that shape's list
				continue;
			}

			auto const seenIndex = pictureTextureOf.find(index);

			if (seenIndex == pictureTextureOf.end())
			{
				pictureTextureOf[index] = texture;
			}
			else if (seenIndex->second != texture)
			{
				pictureIndexAmbiguous++;
			}

			auto const found = pictureIndexOf.find(texture);

			if (found == pictureIndexOf.end())
			{
				pictureIndexOf[texture] = index;
			}
			else if (found->second != index)
			{
				pictureIndexConflicts++;
			}
		}
	};

	for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
	{
		uint8_t id[8];
		LevelViewShapeOf shape = {};

		shape.isModule = true;
		shape.moduleIndex = m;

		if (ModuleObject(open, m, id))
		{
			memcpy(shape.objectId, id, 8);
			scan(shape, id);
		}
	}

	std::vector<std::vector<uint8_t>> active;

	LevelView_ActiveObjectIds(&active);

	for (std::vector<uint8_t> const &id : active)
	{
		LevelViewShapeOf shape = {};

		memcpy(shape.objectId, id.data(), 8);
		scan(shape, id.data());
	}
}

// The number the shape of object `id` gives the game's texture `texture` in the list it is numbered by (its own list when it has one, else the level's); -1 when that list is not known to hold it
static int32_t PictureIndexForShape(OpenLevelFile *open, uint8_t const *id, int32_t texture)
{
	if (host::PolygonItems_ShapeHasOwnPictureList(open->Level(), id))
	{
		auto const own = ownPictureIndexOf.find(std::string((char const *)id, 8));

		if (own == ownPictureIndexOf.end())
		{
			return -1;
		}

		auto const mine = own->second.find(texture);

		return mine != own->second.end() && mine->second >= 0 && mine->second <= 0xfff ? mine->second : -1;
	}

	auto const index = pictureIndexOf.find(texture);

	return index != pictureIndexOf.end() && index->second >= 0 && index->second <= 0xfff ? index->second : -1;
}

// Puts the game's texture number `texture` on every selected textured polygon: each shape's polygon colour words rewritten (bits 0-11 only) through ShapeEdit_WriteSameCounts
static void TextureMapSetPicture(int32_t texture)
{
	TextureDropImported();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Texture Map: no level is open";
		return;
	}

	if (texturePolygons.empty())
	{
		statusWords = "Texture Map: nothing is selected";
		return;
	}

	BuildThePictureIndex(open);

	size_t changed = 0, notTextured = 0, left = 0, notInList = 0;
	std::string firstWhy;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		model::ShapeModel shapeModel;
		std::string why;

		if (host::ShapeEdit_Read(open->Level(), open, lead.id, &shapeModel, nullptr, &why) != host::ShapeEditResult::Done)
		{
			for (size_t i = first; i < texturePolygons.size(); i++)
			{
				if (!done[i] && memcmp(texturePolygons[i].id, lead.id, 8) == 0)
				{
					done[i] = true;
					left++;
				}
			}

			continue;
		}

		size_t here = 0;
		std::vector<int32_t> changedPolygons;

		// the picture's number in the list THIS shape is numbered by: its own list when it has one (known only from its own polygons), else the level's
		int32_t const shapeIndex = PictureIndexForShape(open, lead.id, texture);

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (done[i] || memcmp(one.id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			if (shapeIndex < 0)
			{
				notInList++;
			}
			else if (one.polygon < 0 || (size_t)one.polygon >= shapeModel.polygons.size())
			{
				left++;
			}
			else if (!model::EngineTypeIsTextured(shapeModel.polygons[(size_t)one.polygon].engineType))
			{
				notTextured++;
			}
			else
			{
				int32_t &colour = shapeModel.polygons[(size_t)one.polygon].colour;

				colour = (int32_t)(((uint32_t)colour & ~0xfffu) | (uint32_t)shapeIndex);
				changedPolygons.push_back(one.polygon);
				here++;
			}
		}

		if (here == 0)
		{
			continue;
		}

		std::string writeWhy;
		host::ShapeEditResult const written = host::ShapeEdit_WriteSameCounts(open, open->Level(), lead.id, shapeModel, &writeWhy);

		if (written == host::ShapeEditResult::Done)
		{
			changed += here;

			// the live preview: the game's items of those polygons take the picture too, so the view shows it now (the file's polygons are what is saved; the game's items are what is drawn)
			LevelViewShapeOf shape = {};
			std::vector<uint8_t> corners;
			std::vector<int32_t> numbers, toItem, firstFile;
			bool hasMerge = false;

			shape.isModule = lead.isModule;
			shape.moduleIndex = lead.moduleIndex;
			memcpy(shape.objectId, lead.id, 8);

			if (LevelView_ItemCorners(shape, &corners) && host::PolygonItems_ReadMergeData(open, open->Level(), lead.id, &numbers, &hasMerge) &&
				host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), shapeModel.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) == host::PolygonItemResult::Ok)
			{
				for (int32_t const polygon : changedPolygons)
				{
					LevelView_SetItemTexture(shape, toItem[(size_t)polygon], texture);
				}
			}
		}
		else
		{
			left += here;
			firstWhy = firstWhy.empty() ? (writeWhy.empty() ? host::ShapeEditWords(written) : writeWhy) : firstWhy;
		}
	}

	std::vector<LevelViewPicture> pictures;

	LevelView_Pictures(&pictures, 4096);

	std::string name = "?";

	for (LevelViewPicture const &picture : pictures)
	{
		if (picture.texture == texture)
		{
			name = picture.name;
		}
	}

	textureWords = "Texture Map: picture " + name + " put on " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s");

	if (notInList)
	{
		textureWords += ", " + std::to_string(notInList) + " left as they were (the picture is not in the list their shape is numbered by)";
	}

	if (notTextured)
	{
		textureWords += ", " + std::to_string(notTextured) + " not textured (left as they were)";
	}

	if (left)
	{
		textureWords += ", " + std::to_string(left) + " refused" + (firstWhy.empty() ? "" : " (" + firstWhy + ")");
	}

	statusWords = textureWords.c_str();
}

// Next Texture (+1) and Prev Texture (-1): the current picture steps to the next one in the level's numbering, wrapping, and is put on the selection at once (FUN_00555dd0)
void TextureMapStepPicture(int step)
{
	std::vector<LevelViewPicture> pictures;

	if (!LevelView_Pictures(&pictures, 4096))
	{
		statusWords = "Texture Map: the game has no picture loaded";
		return;
	}

	// only pictures the level's own list holds are offered (a picture only some shape's private list holds has no number the other shapes could take)
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open && open->Level())
	{
		BuildThePictureIndex(open);
		pictures.erase(std::remove_if(pictures.begin(), pictures.end(), [](LevelViewPicture const &picture) { return pictureIndexOf.find(picture.texture) == pictureIndexOf.end(); }), pictures.end());
	}

	if (pictures.empty())
	{
		statusWords = "Texture Map: no picture of the level's list is in use";
		return;
	}

	size_t at = 0;

	for (size_t i = 0; i < pictures.size(); i++)
	{
		if (pictures[i].texture == textureCurrent)
		{
			at = i;
		}
	}

	if (textureCurrent < 0)
	{
		at = step > 0 ? pictures.size() - 1 : 0;   // the mode starts on the lowest: the first Next lands on the one after it
	}

	at = (size_t)(((int64_t)at + step + (int64_t)pictures.size()) % (int64_t)pictures.size());
	textureCurrent = pictures[at].texture;
	TextureMapSetPicture(textureCurrent);
}

// TEXTURE MAP: WRAP (0x2e, FUN_0055e400) and TILE (0x2f, FUN_0055e5d0), both ending in FUN_00570590(module, polygon, mode): the command asks "Choose Polygon for plane", the next click names a polygon, and the
// texture coordinates of every selected polygon's corners are made by projecting the corners' WORLD points onto that polygon's plane (model/uv_project.h: Vasquez's read of FUN_00570590, fixed-point maths
// copied). Wrap fits the selection's spread onto the texture window, tile divides by the scales; then the current texture goes on the selection (inteng's FUN_00555dd0 end). The world points are the game's own (the
// object's turn and place: FilePick_World). NAMED: (1) the window corners are the whole of the lowest-numbered picture, (0,0) (W,0) (W,H) (0,H), as the flips' window is (the state's +0xa4 corners at start are not read);
// (2) the tile scales are 1000 and 1000 with offsets 0 (DAT_005fd694 / 98 at start not read: the arrow keys that change them are not built); (3) the chosen polygon's whole-number normal is the file's float normal
// times 65536 cut toward 0 (inteng's own conversion not read: Vasquez's deviation 2); (4) the view is not redrawn with the new coordinates, only the picture is (the game's loaded lists are separate from the file's).
// inteng holds a shape's points and a place in ONE unit, 1e-5 m: the file's number times 100 times the level's scale (move_rel_poly_edit.h; p11 section 32), rounded to the nearest whole number (ours, named)
static int32_t InIntengUnits(int32_t fileNumber)
{
	double const unit = 100.0 * (local_scale > 0.0 ? local_scale : 1.0);
	double const value = (double)fileNumber * unit;

	return value >= 2147483647.0 ? INT32_MAX : (value <= -2147483648.0 ? INT32_MIN : (int32_t)(value < 0 ? value - 0.5 : value + 0.5));
}


// COPY TEXTURING > POLYGON (0x31, key U; FUN_0055c210(0), FUN_00558b90, FUN_005589e0, FUN_0055c660): the clicked polygon's texture coordinates and picture are put on the selected polygons. A triangle source is copied corner for corner onto
// the selected triangles; a QUAD source onto the selected quads turned so that the destination's edge most nearly in the direction of one of the source's edges lines up with it (each polygon's four edges as unit vectors; the pair of edges with
// the greatest dot product, the first found on a tie, starting above -10: FUN_0055c660 0x55c8b3-0x55c905; then destination corner k takes the source's corner (k - j + 4 + i) mod 4, i = the source's edge, j = the destination's), and the
// first three corners of a quad source onto selected triangles; the picture is the source's. NAMED: a source or destination that is not textured (inteng gives it type 7) and a plain-colour source (FUN_00570490) are left out, since the texture store refuses
// them; the animation form (Shift+U) is not built; the polygons' corner order is the file's.
// The core of Copy Texturing and of the texture window: `uv` (count corners, 3 or 4) belongs to the geometry of polygon `refPolygon` of object `refId`; every selected polygon takes it (see above): a triangle its first three corners, a quad turned to the edge that
// lines up when the reference is a quad too. Returns how many polygons changed; *left how many were left as they were.
static size_t TextureApplyUv(OpenLevelFile *open, uint8_t const refId[8], int32_t refPolygon, model::UvList const &uv, int count, size_t *left)
{
	model::ShapeModel refShape;
	std::string why;

	*left = 0;

	if (host::ShapeEdit_Read(open->Level(), open, refId, &refShape, nullptr, &why) != host::ShapeEditResult::Done || refPolygon < 0 || (size_t)refPolygon >= refShape.polygons.size() || count < 3)
	{
		return 0;
	}

	model::ShapePolygon const &from = refShape.polygons[(size_t)refPolygon];
	bool const sourceIsQuad = count == 4 && from.cornerCount == 4;
	model::Fix3 sourceEdges[4];

	for (int i = 0; sourceIsQuad && i < 4; i++)
	{
		model::ShapePoint const a = refShape.points[(size_t)from.corners[i]], b = refShape.points[(size_t)from.corners[(i + 1) & 3]];

		sourceEdges[i] = { b.x - a.x, b.y - a.y, b.z - a.z };
		model::FixedNormalise(&sourceEdges[i]);
	}

	size_t changed = 0;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		model::ShapeModel shape;
		std::vector<model::PolygonUv> forShape;
		bool const read = host::ShapeEdit_Read(open->Level(), open, lead.id, &shape, nullptr, &why) == host::ShapeEditResult::Done;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			if (done[i] || memcmp(texturePolygons[i].id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			model::PolygonUv dest;
			int32_t const number = texturePolygons[i].polygon;

			if (!read || number < 0 || (size_t)number >= shape.polygons.size() || host::UvEdit_ReadPolygon(open, open->Level(), lead.id, number, &dest, &why) != host::UvEditResult::Done || !dest.textured)
			{
				(*left)++;
				continue;
			}

			model::ShapePolygon const &polygon = shape.polygons[(size_t)number];

			if (dest.uv.cornerCount == 3)
			{
				for (int c = 0; c < 3; c++)
				{
					dest.uv.corners[c] = uv.corners[c];
				}
			}
			else if (dest.uv.cornerCount == 4 && polygon.cornerCount == 4 && sourceIsQuad)
			{
				model::Fix3 edges[4];

				for (int j = 0; j < 4; j++)
				{
					model::ShapePoint const a = shape.points[(size_t)polygon.corners[j]], b = shape.points[(size_t)polygon.corners[(j + 1) & 3]];

					edges[j] = { b.x - a.x, b.y - a.y, b.z - a.z };
					model::FixedNormalise(&edges[j]);
				}

				int32_t best = -10;
				int bestSource = 0, bestDest = 0;

				for (int a = 0; a < 4; a++)
				{
					for (int b = 0; b < 4; b++)
					{
						int32_t const dot = model::FixedDot(sourceEdges[a], edges[b]);

						if (best < dot)
						{
							best = dot;
							bestSource = a;
							bestDest = b;
						}
					}
				}

				for (int k = 0; k < 4; k++)
				{
					dest.uv.corners[k] = uv.corners[(k - bestDest + 4 + bestSource) & 3];
				}
			}
			else
			{
				(*left)++;   // a quad destination of a triangle source, or the other way round: untouched, as inteng's loops leave it
				continue;
			}

			forShape.push_back(dest);
		}

		if (forShape.empty())
		{
			continue;
		}

		std::string writeWhy;

		if (host::UvEdit_WritePolygons(open, open->Level(), lead.id, forShape, &writeWhy) == host::UvEditResult::Done)
		{
			changed += forShape.size();
			ShowTheNewUv(open, lead, forShape);
		}
		else
		{
			*left += forShape.size();
		}
	}

	return changed;
}

static void TextureCopyFrom(uint8_t const sourceId[8], int32_t sourcePolygon)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	TextureDropImported();

	model::PolygonUv source;
	std::string why;

	if (!open || !open->Level() || host::UvEdit_ReadPolygon(open, open->Level(), sourceId, sourcePolygon, &source, &why) != host::UvEditResult::Done || !source.textured || source.uv.cornerCount < 3)
	{
		statusWords = "Texture Map: that polygon has no texture coordinates to copy (a plain-colour polygon's colour is not copied)";
		return;
	}

	size_t left = 0;
	size_t const changed = TextureApplyUv(open, sourceId, sourcePolygon, source.uv, source.uv.cornerCount, &left);

	// the picture: the source's index in the list of its shape, as the game's texture number
	BuildThePictureIndex(open);

	auto const texture = pictureTextureOf.find(source.colour & 0xfff);

	if (texture != pictureTextureOf.end())
	{
		TextureMapSetPicture(texture->second);
	}

	textureWords = "Texture Map: copied the coordinates of polygon " + std::to_string(sourcePolygon) + " onto " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s") + (left ? ", " + std::to_string(left) + " left as they were" : "");
	statusWords = textureWords.c_str();
}

// THE TEXTURE WINDOW SCREEN (Tab; inteng's texture screen, FUN_00551f80 sub-mode 1, whose own menu sets 2 and 3 are Mode / Polygon / Texture / Select / Reset Source Rect (0x9) / Help): the current texture's picture is shown over the level with the first
// selected polygon's texture coordinates on it as nodes joined by lines, and the nodes are edited there - 1 to 4 pick a node, the arrows move it (Shift 10 at a time, Ctrl all the nodes), a press near a node takes it and the mouse moves it - and every edit is
// given to all the selected polygons through TextureApplyUv (as Copy Texturing gives them one polygon's coordinates). The current texture is the first selected polygon's picture. X / Y / E / R work on the polygons as in the main view and the nodes are read again.
// NAMED: the picture is drawn at a whole-number scale ("Texture Scale x%d", = and - change it) that fits the window; there is no scrolling of the picture and no second window of repeats; "Reset Source Rect" puts the nodes back on the polygon's own coordinates.
static uint8_t textureWinId[8];
static int32_t textureWinPolygon = -1;
static model::UvList textureWinUv;
int textureWinCount = 0;
int textureWinNode = 0;
bool textureWinDrag = false;
int32_t textureWinFrameW = 0, textureWinFrameH = 0;   // the size of the frame the window was last drawn on (the panels' pixels, which the mouse is in)

bool TextureWindowLoad()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		return false;
	}

	model::PolygonUv uv;
	std::string why;
	TexturePolygon const first = texturePolygons[0];

	if (host::UvEdit_ReadPolygon(open, open->Level(), first.id, first.polygon, &uv, &why) != host::UvEditResult::Done || !uv.textured || uv.uv.cornerCount < 3)
	{
		return false;
	}

	memcpy(textureWinId, first.id, 8);
	pictureIndexOf.clear();   // made again from the items as they are now (a saved copy or an edit may have changed the game's picture numbers)
	textureWinPolygon = first.polygon;
	textureWinUv = uv.uv;
	textureWinCount = uv.uv.cornerCount;

	if (textureWinNode >= textureWinCount)
	{
		textureWinNode = 0;
	}

	BuildThePictureIndex(open);

	auto const texture = pictureTextureOf.find(uv.colour & 0xfff);

	if (texture != pictureTextureOf.end())
	{
		textureCurrent = texture->second;
	}

	return true;
}

void TextureWindowToggle()
{
	if (textureWindowOn)
	{
		textureWindowOn = false;
		textureWinDrag = false;
		TextureWindowBars(false);
		statusWords = "Texture Map: back to the level";
		return;
	}

	if (!TextureWindowLoad())
	{
		statusWords = "Texture Map: select a textured polygon first (the texture window shows its coordinates)";
		return;
	}

	textureWindowOn = true;
	TextureWindowBars(true);
	textureWords = "Texture Map: the texture window (1-4 pick a node, arrows move it, Shift 10, Ctrl all; = - scale; Tab leaves)";
	statusWords = textureWords.c_str();
}

static void TextureWindowReset()
{
	if (textureWindowOn && TextureWindowLoad())
	{
		statusWords = "Texture Map: the nodes are back on the polygon's own coordinates";
	}
}

// The bars of the texture window (page 3 of the Texture Map screen: Mode, Texture, Help; Rotate Nodes, X-Flip, Y-Flip, Next / Prev Texture, Toggle Texture Resize, Set Default Size, Toggle Node Selection) and back (page 0)
static void TextureWindowBars(bool on)
{
	EditorBar_SetPage(on ? 3 : 0);

	if (!LoadTheMenus())
	{
		LogString("menus: the bars of the texture window couldn't be made");
		statusWords = "Texture Map: the texture window's bars couldn't be made";
	}
}

// Rotate Nodes > ... Node Centre: the nodes turned about their own middle by quarter turns, anticlockwise on the screen, then given to the polygons
static void TextureWindowRotateNodes(int quarters)
{
	if (!textureWindowOn || textureWinCount < 3)
	{
		statusWords = "Texture Map: the nodes are turned in the texture window (Tab opens it)";
		return;
	}

	double centreU = 0, centreV = 0;

	for (int k = 0; k < textureWinCount; k++)
	{
		centreU += textureWinUv.corners[k].u / textureWinCount;
		centreV += textureWinUv.corners[k].v / textureWinCount;
	}

	for (int k = 0; k < textureWinCount; k++)
	{
		double du = textureWinUv.corners[k].u - centreU, dv = textureWinUv.corners[k].v - centreV;

		for (int q = 0; q < quarters; q++)
		{
			double const turned = du;

			du = dv;
			dv = -turned;
		}

		textureWinUv.corners[k].u = (float)floor(centreU + du + 0.5);
		textureWinUv.corners[k].v = (float)floor(centreV + dv + 0.5);
	}

	TextureWindowApply();
}

void TextureWindowApply()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || textureWinPolygon < 0)
	{
		return;
	}

	size_t left = 0;
	size_t const changed = TextureApplyUv(open, textureWinId, textureWinPolygon, textureWinUv, textureWinCount, &left);

	textureWords = "Texture Map: the nodes were given to " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s") + (left ? ", " + std::to_string(left) + " left as they were" : "");
	statusWords = textureWords.c_str();
}

void TextureWindowMoveNode(int node, float du, float dv, bool all)
{
	for (int k = 0; k < textureWinCount; k++)
	{
		if (all || k == node)
		{
			textureWinUv.corners[k].u += du;
			textureWinUv.corners[k].v += dv;
		}
	}
}

// Where the picture and the nodes are on the window (the window's pixels): used by the drawing and by the mouse
struct TextureWinPlace
{
	bool ok;
	int32_t texture, width, height, x, y, scale, areaTop, areaHeight, tilesX, tilesY;
};

static TextureWinPlace TextureWindowPlace(int32_t windowWidth, int32_t windowHeight)
{
	TextureWinPlace place = {};
	std::vector<LevelViewPicture> pictures;

	if (!textureWindowOn || textureCurrent < 0 || !LevelView_Pictures(&pictures, 4096))
	{
		return place;
	}

	for (LevelViewPicture const &picture : pictures)
	{
		if (picture.texture == textureCurrent)
		{
			place.texture = picture.texture;
			place.width = picture.width;
			place.height = picture.height;
		}
	}

	if ((place.width < 1 || place.height < 1) && !pictures.empty())
	{
		// the current texture is not among the game's pictures (it was renumbered): the first picture stands in and becomes the current one
		textureCurrent = pictures[0].texture;
		place.texture = pictures[0].texture;
		place.width = pictures[0].width;
		place.height = pictures[0].height;
	}

	if (place.width < 1 || place.height < 1)
	{
		return place;
	}

	place.areaTop = 56;   // below the two menu bars
	place.areaHeight = windowHeight - 56 - 36;

	// inteng's window is the picture repeated 1 to 10 times each way (0x10f Select Number of Repeats; inf is shown as 3)
	place.tilesX = textureRepeatX == 11 ? 3 : textureRepeatX;
	place.tilesY = textureRepeatY == 11 ? 3 : textureRepeatY;

	int32_t fit = (windowWidth - 40) / (place.width * place.tilesX);
	int32_t const fitHeight = (place.areaHeight - 72) / (place.height * place.tilesY);

	if (fitHeight < fit)
	{
		fit = fitHeight;
	}

	place.scale = textureWinScale > 0 ? textureWinScale : (fit < 1 ? 1 : fit);
	place.x = (windowWidth - place.width * place.scale * place.tilesX) / 2;
	place.y = place.areaTop + 68;
	place.ok = place.areaHeight > 100;
	return place;
}

static void TextureWindowNodeAt(TextureWinPlace const &place, int node, int32_t *x, int32_t *y)
{
	*x = place.x + (int32_t)floor(textureWinUv.corners[node].u * place.scale + 0.5f);
	*y = place.y + (int32_t)floor(textureWinUv.corners[node].v * place.scale + 0.5f);
}

// A press at (x, y) of the window takes the nearest node within 16 pixels; true if one was taken
bool TextureWindowPress(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight)
{
	TextureWinPlace const place = TextureWindowPlace(windowWidth, windowHeight);
	int best = -1;
	int64_t bestDistance = 16 * 16 + 1;

	for (int k = 0; place.ok && k < textureWinCount; k++)
	{
		int32_t nx, ny;

		TextureWindowNodeAt(place, k, &nx, &ny);

		int64_t const distance = (int64_t)(nx - x) * (nx - x) + (int64_t)(ny - y) * (ny - y);

		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = k;
		}
	}

	if (best < 0)
	{
		return false;
	}

	textureWinNode = best;
	textureWinDrag = true;
	return true;
}

void TextureWindowDragTo(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight)
{
	TextureWinPlace const place = TextureWindowPlace(windowWidth, windowHeight);

	if (place.ok && textureWinDrag && textureWinNode < textureWinCount)
	{
		textureWinUv.corners[textureWinNode].u = (float)floor((double)(x - place.x) / place.scale + 0.5);
		textureWinUv.corners[textureWinNode].v = (float)floor((double)(y - place.y) / place.scale + 0.5);
	}
}

void TextureWindowDraw(int32_t windowWidth, int32_t windowHeight)
{
	textureWinFrameW = windowWidth;
	textureWinFrameH = windowHeight;

	TextureWinPlace const place = TextureWindowPlace(windowWidth, windowHeight);

	if (!place.ok)
	{
		return;
	}

	TextureWindowPicture picture = {};
	static char words[3][200];
	char const *lines[3] = { words[0], words[1], words[2] };

	picture.texture = place.texture;
	picture.x = place.x;
	picture.y = place.y;
	picture.width = place.width * place.scale;
	picture.height = place.height * place.scale;
	picture.tilesX = place.tilesX;
	picture.tilesY = place.tilesY;
	picture.nodeCount = textureWinCount;
	picture.activeNode = textureWinNode;
	picture.areaTop = place.areaTop;
	picture.areaHeight = place.areaHeight;

	for (int k = 0; k < textureWinCount && k < 4; k++)
	{
		TextureWindowNodeAt(place, k, &picture.nodeX[k], &picture.nodeY[k]);
	}

	snprintf(words[0], sizeof(words[0]), "Texture window: Texture Scale x%d (%d by %d)   node %d of %d", place.scale, place.width, place.height, textureWinNode + 1, textureWinCount);
	snprintf(words[1], sizeof(words[1]), "%d %d  %d %d  %d %d  %d %d", (int)textureWinUv.corners[0].u, (int)textureWinUv.corners[0].v, (int)textureWinUv.corners[1].u, (int)textureWinUv.corners[1].v,
		(int)textureWinUv.corners[2].u, (int)textureWinUv.corners[2].v, textureWinCount > 3 ? (int)textureWinUv.corners[3].u : 0, textureWinCount > 3 ? (int)textureWinUv.corners[3].v : 0);
	snprintf(words[2], sizeof(words[2]), "1-4 node, arrows move (Shift 10, Ctrl all), drag a node, X Y flip, E R turn, = - scale, PgUp PgDn picture, Tab leaves");
	TextureWindowPaint_Draw(windowWidth, windowHeight, picture, lines, 3, 16, place.areaTop + 4);
}

// THE ANIM MENU of the Texture Map mode (inteng's 0x9d Select, 0x9e Create, 0x9f Destroy, 0xa4 Set Anim Number, Sequence 0xa6 Next / 0xa7 Previous / 0xa8 Add / 0xa9 Remove / 0xaa Copy / 0xab Change Properties, Frame 0xad Next / 0xae Previous / 0xaf Add / 0xb0 Remove,
// 0xb1 Add Polys, 0xb2 Remove Polys; FUN_0054d260 cases 0x9d-0xb2, handlers FUN_005406a0 .. FUN_00541720, read 2026-11-15): the textures that change. A shape's TEXTANIM chunk (model/texture_anim.h) holds, for each animated polygon, its GROUP (the ID), its sequences and their
// frames (a picture number and the three (u, v) of the polygon's corners); inteng's "current animation" is one group, DAT_005fd3b0, and every command works on all the polygons of the group at once. The polygon in the level shows the CURRENT frame of the CURRENT
// sequence (FUN_0053fa00) and takes into that frame, before another frame or sequence is gone to, whatever the polygon shows then (FUN_0053fa70): so the Texture Map's own edits (flips, wrap, Set Texture, the texture window) made while an animation is current go
// into its frame. THIS EDITOR: the chunk is made again through OpenLevelFile::ReplaceLevel after each command (anim_edit.h: its size changes), so the file is always right; the frame is taken from the polygon before each command and when a copy is saved.
// NAMED DEVIATIONS: Change Properties is one typed line where inteng has a dialog (the same fields, read from FUN_005411d0 / FUN_00540d10 and copied exactly); Add Polys and Create make an untextured triangle a textured one first, as inteng does (Hicks's PolyEdit_MakeTextured); a quad of the file (the game's loader animates only triangles; how inteng's edit records, which are triangles, turn a file quad into two is not read yet); the current frame of each
// sequence is this session's only.
static int32_t animGroup = -1;
static int animSeq = 0;
static std::vector<int> animFrameOf;
static int32_t animNumber = 0;
static int animConfirmKind = 0;   // textureAsk of a question waiting for a y: 7 destroy the animation, 8 destroy the sequence, 9 remove the frame, 10 add the polygons, 11 remove the polygons

struct AnimShapeState
{
	uint8_t id[8];
	bool isModule;
	int32_t moduleIndex;
	int32_t number;
	model::TextureAnim anim;
	bool changed;
};

static void AnimReadAll(OpenLevelFile *open, std::vector<AnimShapeState> *shapes)
{
	shapes->clear();
	TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
	{
		AnimShapeState state;

		memcpy(state.id, id, 8);
		state.isModule = isModule;
		state.moduleIndex = moduleIndex;
		state.number = 0;
		state.changed = false;

		if (host::AnimEdit_Read(open, id, &state.number, &state.anim) != host::AnimEditResult::Done)
		{
			return;
		}

		for (AnimShapeState const &have : *shapes)
		{
			if (have.number == state.number)
			{
				return;
			}
		}

		shapes->push_back(state);
	});
}

static AnimShapeState *AnimShapeOfId(std::vector<AnimShapeState> *shapes, uint8_t const *id, OpenLevelFile *open)
{
	int32_t number = 0;
	model::TextureAnim ignored;

	if (host::AnimEdit_Read(open, id, &number, &ignored) != host::AnimEditResult::Done)
	{
		return nullptr;
	}

	for (AnimShapeState &state : *shapes)
	{
		if (state.number == number)
		{
			return &state;
		}
	}

	return nullptr;
}

// The first polygon of the current group (the reference for the counts)
static model::AnimPolygon *AnimFirstOfGroup(std::vector<AnimShapeState> *shapes)
{
	for (AnimShapeState &shape : *shapes)
	{
		for (model::AnimPolygon &polygon : shape.anim.polygons)
		{
			if (polygon.group == animGroup)
			{
				return &polygon;
			}
		}
	}

	return nullptr;
}

// The sequence and frame numbers made right for the group as it is now; the group is gone (-1) when it has no polygon
static void AnimClamp(std::vector<AnimShapeState> *shapes)
{
	model::AnimPolygon *const reference = animGroup < 0 ? nullptr : AnimFirstOfGroup(shapes);

	if (!reference)
	{
		animGroup = -1;
		animSeq = 0;
		animFrameOf.clear();
		return;
	}

	int const count = (int)reference->sequences.size();

	if (animSeq >= count)
	{
		animSeq = count - 1;
	}

	if (animSeq < 0)
	{
		animSeq = 0;
	}

	animFrameOf.resize((size_t)count, 0);

	for (int s = 0; s < count; s++)
	{
		int const frames = (int)reference->sequences[(size_t)s].Frames();

		if (animFrameOf[(size_t)s] >= frames)
		{
			animFrameOf[(size_t)s] = frames - 1;
		}

		if (animFrameOf[(size_t)s] < 0)
		{
			animFrameOf[(size_t)s] = 0;
		}
	}

	animNumber = reference->identifier;
}

// What polygon `polygon` of the shape shows now: its picture number and the (u, v) of its first three corners
static bool AnimPolygonNow(OpenLevelFile *open, uint8_t const *id, int32_t polygon, int32_t *texture, int32_t uv[6])
{
	model::PolygonUv now;
	std::string why;

	if (host::UvEdit_ReadPolygon(open, open->Level(), id, polygon, &now, &why) != host::UvEditResult::Done || !now.textured || now.uv.cornerCount < 3)
	{
		return false;
	}

	*texture = now.colour & 0xfff;

	for (int c = 0; c < 3; c++)
	{
		uv[c * 2] = (int32_t)floor(now.uv.corners[c].u + 0.5f);
		uv[c * 2 + 1] = (int32_t)floor(now.uv.corners[c].v + 0.5f);
	}

	return true;
}

// FUN_0053fa70 for every polygon of the group: the frame now shown takes what the polygon shows
static void AnimCapture(OpenLevelFile *open, std::vector<AnimShapeState> *shapes)
{
	for (AnimShapeState &shape : *shapes)
	{
		for (model::AnimPolygon &polygon : shape.anim.polygons)
		{
			if (polygon.group != animGroup || animSeq < 0 || (size_t)animSeq >= polygon.sequences.size())
			{
				continue;
			}

			model::AnimSequence &sequence = polygon.sequences[(size_t)animSeq];
			size_t const frame = (size_t)animFrameOf[(size_t)animSeq];
			int32_t texture, uv[6];

			if (frame < sequence.Frames() && AnimPolygonNow(open, shape.id, polygon.polygon, &texture, uv))
			{
				if (sequence.textures[frame] != texture || memcmp(&sequence.uv[frame * 6], uv, sizeof(uv)) != 0)
				{
					sequence.textures[frame] = texture;
					memcpy(&sequence.uv[frame * 6], uv, sizeof(uv));
					shape.changed = true;
				}
			}
		}
	}
}

// FUN_0053fa00 for every polygon of the group: the polygons show the current frame of the current sequence
static void AnimShow(OpenLevelFile *open, std::vector<AnimShapeState> *shapes)
{
	for (AnimShapeState &shape : *shapes)
	{
		std::vector<model::PolygonUv> coordinates;
		std::vector<std::pair<int32_t, int32_t>> pictures;   // polygon, picture number

		for (model::AnimPolygon const &polygon : shape.anim.polygons)
		{
			if (polygon.group != animGroup || animSeq < 0 || (size_t)animSeq >= polygon.sequences.size())
			{
				continue;
			}

			model::AnimSequence const &sequence = polygon.sequences[(size_t)animSeq];
			size_t const frame = (size_t)animFrameOf[(size_t)animSeq];
			model::PolygonUv now;
			std::string why;

			if (frame >= sequence.Frames() || host::UvEdit_ReadPolygon(open, open->Level(), shape.id, polygon.polygon, &now, &why) != host::UvEditResult::Done || !now.textured || now.uv.cornerCount < 3)
			{
				continue;
			}

			for (int c = 0; c < 3; c++)
			{
				now.uv.corners[c].u = (float)sequence.uv[frame * 6 + (size_t)c * 2];
				now.uv.corners[c].v = (float)sequence.uv[frame * 6 + (size_t)c * 2 + 1];
			}

			coordinates.push_back(now);
			pictures.push_back(std::make_pair(polygon.polygon, sequence.textures[frame]));
		}

		std::string why;

		if (!coordinates.empty())
		{
			host::UvEdit_WritePolygons(open, open->Level(), shape.id, coordinates, &why);
		}

		model::ShapeModel model;

		if (!pictures.empty() && host::ShapeEdit_Read(open->Level(), open, shape.id, &model, nullptr, &why) == host::ShapeEditResult::Done)
		{
			bool any = false;

			for (auto const &picture : pictures)
			{
				if (picture.first >= 0 && (size_t)picture.first < model.polygons.size())
				{
					int32_t &colour = model.polygons[(size_t)picture.first].colour;

					colour = (int32_t)(((uint32_t)colour & ~0xfffu) | ((uint32_t)picture.second & 0xfffu));
					any = true;
				}
			}

			if (any)
			{
				host::ShapeEdit_WriteSameCounts(open, open->Level(), shape.id, model, &why);
			}
		}
	}
}

// The chunks of every shape that changed are made again, in one new image of the level
static bool AnimCommit(OpenLevelFile *open, std::vector<AnimShapeState> *shapes)
{
	std::vector<host::AnimWrite> writes;

	for (AnimShapeState const &shape : *shapes)
	{
		if (shape.changed)
		{
			host::AnimWrite one;

			one.shapeNumber = shape.number;
			one.anim = shape.anim;
			writes.push_back(one);
		}
	}

	if (writes.empty())
	{
		return true;
	}

	std::string why;
	host::AnimEditResult const result = host::AnimEdit_Write(open, writes, &why);

	if (result != host::AnimEditResult::Done)
	{
		textureWords = std::string("Anim: the animation was not changed (") + (result == host::AnimEditResult::NotFit ? why : result == host::AnimEditResult::LoaderRefused ? "the game's loader would not take it" : "the level could not be put together") + ")";
		statusWords = textureWords.c_str();
		return false;
	}

	LoadTheObjectListAgain();
	return true;
}

static std::string AnimWords(std::vector<AnimShapeState> *shapes)
{
	model::AnimPolygon *const reference = animGroup < 0 ? nullptr : AnimFirstOfGroup(shapes);

	if (!reference)
	{
		return "Anim: no animation is current (Create makes one of the selected polygons, Select picks one)";
	}

	size_t members = 0;

	for (AnimShapeState const &shape : *shapes)
	{
		for (model::AnimPolygon const &polygon : shape.anim.polygons)
		{
			members += polygon.group == animGroup ? 1 : 0;
		}
	}

	return "Anim: animation " + std::to_string(animGroup) + " (" + std::to_string(members) + " polygon" + (members == 1 ? "" : "s") + "), sequence " + std::to_string(animSeq + 1) + " of " + std::to_string(reference->sequences.size()) + ", frame " +
		std::to_string(animFrameOf[(size_t)animSeq] + 1) + " of " + std::to_string(reference->sequences[(size_t)animSeq].Frames()) + ", anim number " + std::to_string(reference->identifier);
}

static void AnimSay(std::vector<AnimShapeState> *shapes, std::string const &first)
{
	textureWords = first + (first.empty() ? "" : ": ") + AnimWords(shapes);
	statusWords = textureWords.c_str();
}

static void AnimPlayStart(OpenLevelFile *open, std::vector<AnimShapeState> *shapes, bool all);

// FUN_00540880 and FUN_0053ff60 make an UNTEXTURED selected polygon a textured one first (type 7, zero coordinates; FUN_0053fdd0 then takes it into the animation): here the selected untextured TRIANGLES of the file are made textured by Hicks's PolyEdit_MakeTextured (poly_edit.h: a new zero list, the
// current picture's number or 0), through the level rebuild, before the animation's own step. Returns how many; a quad or a polygon of five corners is left as it is (the loader animates triangles only).
static size_t AnimMakeSelectedTextured(OpenLevelFile *open)
{
	size_t made = 0;
	std::vector<bool> done(texturePolygons.size(), false);

	BuildThePictureIndex(open);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		model::ShapeModel model;
		std::string why;
		std::vector<int32_t> wanted;

		if (host::ShapeEdit_Read(open->Level(), open, lead.id, &model, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			if (done[i] || memcmp(texturePolygons[i].id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			int32_t const p = texturePolygons[i].polygon;

			if (p >= 0 && (size_t)p < model.polygons.size() && !model::EngineTypeIsTextured(model.polygons[(size_t)p].engineType) && model.polygons[(size_t)p].cornerCount == 3)
			{
				wanted.push_back(p);
			}
		}

		if (wanted.empty())
		{
			continue;
		}

		int32_t picture = -1;
		auto const index = pictureIndexOf.find(textureCurrent);

		if (index != pictureIndexOf.end())
		{
			picture = index->second;
		}

		size_t changed = 0;

		if (host::PolyEdit_MakeTextured(open, lead.id, wanted.data(), wanted.size(), picture, &changed, &why) == host::PolyTypeResult::Done)
		{
			made += changed;
			LoadTheObjectListAgain();
		}
	}

	return made;
}

// Every command, after the frame the polygons show has been taken into the current frame
static void TextureAnimCommand(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Anim: no level is open";
		return;
	}

	TextureDropImported();

	if (command == 0x9d)
	{
		textureAnimPick = true;
		statusWords = "Click on a polygon in animation (Esc cancels)";
		return;
	}

	if (command == 0xb3)
	{
		textureAnimCopyPick = true;
		statusWords = "Get Anim from which polygon (click one in an animation; Esc cancels)";
		return;
	}

	if (command == 0x9e && !texturePolygons.empty())
	{
		AnimMakeSelectedTextured(open);
	}

	std::vector<AnimShapeState> shapes;

	AnimReadAll(open, &shapes);

	if (command == 0xa2 || command == 0xa3)
	{
		// Play (FUN_00541720 with the current animation) and Play All (DAT_005fd3b0 = -1: every animation): what the polygons show is put into the current frame first, then the animation runs on the polygons until a key or a click
		if (command == 0xa2 && (animGroup < 0 || !AnimFirstOfGroup(&shapes)))
		{
			statusWords = "Anim: no animation is current (Select picks one by a click, Create makes one of the selected polygons)";
			return;
		}

		if (animGroup >= 0 && AnimFirstOfGroup(&shapes))
		{
			AnimClamp(&shapes);
			AnimCapture(open, &shapes);

			if (!AnimCommit(open, &shapes))
			{
				return;
			}

			AnimReadAll(open, &shapes);
		}

		AnimPlayStart(open, &shapes, command == 0xa3);
		return;
	}

	if (command == 0x9e)
	{
		// Create: every selected textured triangle with no animation gets one of one sequence of one frame (FUN_0053ff60: a new group number, FUN_0053fdd0 and FUN_0053fb90 for each polygon)
		if (texturePolygons.empty())
		{
			statusWords = "Anim: select the polygons to make an animation of first";
			return;
		}

		if (animGroup >= 0)
		{
			AnimCapture(open, &shapes);
		}

		int32_t group = 0;

		for (AnimShapeState const &shape : shapes)
		{
			group = model::NextAnimGroup(shape.anim, group);
		}

		size_t created = 0, notTriangle = 0;

		for (TexturePolygon const &one : texturePolygons)
		{
			AnimShapeState *const shape = AnimShapeOfId(&shapes, one.id, open);
			model::ShapeModel model;
			std::string why;
			int32_t texture, uv[6];

			if (!shape || host::ShapeEdit_Read(open->Level(), open, one.id, &model, nullptr, &why) != host::ShapeEditResult::Done || one.polygon < 0 || (size_t)one.polygon >= model.polygons.size())
			{
				continue;
			}

			bool animated = false;

			for (model::AnimPolygon const &have : shape->anim.polygons)
			{
				animated = animated || have.polygon == one.polygon;
			}

			if (animated)
			{
				continue;
			}

			if (model.polygons[(size_t)one.polygon].cornerCount != 3)
			{
				notTriangle++;
				continue;
			}

			if (!AnimPolygonNow(open, one.id, one.polygon, &texture, uv))
			{
				continue;
			}

			model::AnimPolygon record;

			record.polygon = one.polygon;
			record.group = group;
			record.sequences.push_back(model::NewAnimSequence(texture, uv));
			shape->anim.polygons.push_back(record);
			shape->changed = true;
			created++;
		}

		if (!created)
		{
			statusWords = notTriangle ? "Anim: only triangles of the file can be animated (the game's loader takes no other); nothing was made" : "Anim: nothing to make: the selected polygons are animated already or have no texture";
			return;
		}

		animGroup = group;
		animSeq = 0;
		animFrameOf.assign(1, 0);

		if (AnimCommit(open, &shapes))
		{
			AnimSay(&shapes, "Created " + std::to_string(created) + " polygon" + (created == 1 ? "" : "s") + (notTriangle ? " (" + std::to_string(notTriangle) + " quads left out)" : ""));
		}

		return;
	}

	if (animGroup < 0 || !AnimFirstOfGroup(&shapes))
	{
		animGroup = -1;
		statusWords = "Anim: no animation is current (Select picks one by a click, Create makes one of the selected polygons)";
		return;
	}

	AnimClamp(&shapes);

	// the polygons' own state goes into the current frame first (FUN_0053f960, FUN_0053fb90 and the others do it)
	AnimCapture(open, &shapes);

	model::AnimPolygon *const reference = AnimFirstOfGroup(&shapes);
	int const sequenceCount = (int)reference->sequences.size();
	int const frameCount = (int)reference->sequences[(size_t)animSeq].Frames();
	int &frameNow = animFrameOf[(size_t)animSeq];

	auto const everyMember = [&](std::function<void(AnimShapeState &, model::AnimPolygon &)> const &act)
	{
		for (AnimShapeState &shape : shapes)
		{
			for (model::AnimPolygon &polygon : shape.anim.polygons)
			{
				if (polygon.group == animGroup)
				{
					act(shape, polygon);
					shape.changed = true;
				}
			}
		}
	};

	switch (command)
	{
	case 0xa6: case 0xa7:   // Sequence Next / Previous (FUN_0053f800 / FUN_0053f850)
		animSeq = (animSeq + (command == 0xa6 ? 1 : sequenceCount - 1)) % sequenceCount;
		break;
	case 0xad: case 0xae:   // Frame Next / Previous (FUN_0053f740 / FUN_0053f7a0)
		frameNow = (frameNow + (command == 0xad ? 1 : frameCount - 1)) % frameCount;
		break;
	case 0xa8:   // Sequence Add (FUN_0053fdd0): a new sequence after the current one, of one frame holding what the polygon shows
		everyMember([&](AnimShapeState &shape, model::AnimPolygon &polygon)
		{
			int32_t texture, uv[6];

			if (AnimPolygonNow(open, shape.id, polygon.polygon, &texture, uv) && (size_t)animSeq < polygon.sequences.size())
			{
				polygon.sequences.insert(polygon.sequences.begin() + animSeq + 1, model::NewAnimSequence(texture, uv));
			}
		});
		animFrameOf.insert(animFrameOf.begin() + animSeq + 1, 0);
		animSeq++;
		break;
	case 0xaf:   // Frame Add (FUN_0053fb90): a frame after the current one, holding what the polygon shows
		everyMember([&](AnimShapeState &shape, model::AnimPolygon &polygon)
		{
			int32_t texture, uv[6];

			if (AnimPolygonNow(open, shape.id, polygon.polygon, &texture, uv) && (size_t)animSeq < polygon.sequences.size())
			{
				model::AnimAddFrame(&polygon.sequences[(size_t)animSeq], (size_t)frameNow, texture, uv);
			}
		});
		frameNow++;
		break;
	case 0xaa:   // Sequence Copy: asks which sequence (FUN_0053fcd0), then the current one becomes a copy of it
		textureAsk = 5;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptTextureMap;
		promptText.clear();
		return;
	case 0xa4:   // Set Anim Number
		textureAsk = 4;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptTextureMap;
		promptText.clear();
		return;
	case 0xab:   // Change Properties: one typed line
		textureAsk = 6;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptTextureMap;
		promptText.clear();
		return;
	case 0x9f: case 0xa9: case 0xb0: case 0xb1: case 0xb2:
	{
		// the questions: inteng's own words, answered by y (FUN_00474a40 and FUN_0057c800: the dialog's OK is 3)
		int kind = command == 0x9f ? 7 : command == 0xa9 ? 8 : command == 0xb0 ? 9 : command == 0xb1 ? 10 : 11;

		if (command == 0xa9 && sequenceCount == 1)
		{
			kind = 7;   // FUN_005402c0: the only sequence is the animation
		}
		else if (command == 0xb0 && frameCount == 1)
		{
			kind = sequenceCount == 1 ? 7 : 8;   // FUN_00540490: the only frame is the sequence
		}

		if ((kind == 10 || kind == 11) && texturePolygons.empty())
		{
			statusWords = "Anim: select the polygons first";
			return;
		}

		animConfirmKind = kind;
		textureAsk = kind;
		memset(keyDown, 0, sizeof(keyDown));
		promptFor = kPromptTextureMap;
		promptText.clear();
		return;
	}
	default:
		return;
	}

	AnimClamp(&shapes);
	AnimShow(open, &shapes);

	if (AnimCommit(open, &shapes))
	{
		AnimSay(&shapes, "");
	}
}

// The click of Select (FUN_005406a0): the group of the polygon under the click becomes the current one and its polygons are the selection
static void TextureAnimSelectClicked(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<AnimShapeState> shapes;

	if (!open || !open->Level())
	{
		return;
	}

	AnimReadAll(open, &shapes);

	AnimShapeState *const shape = AnimShapeOfId(&shapes, id, open);
	int32_t found = -1;
	model::AnimPolygon clicked;

	for (size_t i = 0; shape && i < shape->anim.polygons.size(); i++)
	{
		if (shape->anim.polygons[i].polygon == polygon)
		{
			found = shape->anim.polygons[i].group;
			clicked = shape->anim.polygons[i];
		}
	}

	if (found < 0)
	{
		animGroup = -1;
		statusWords = "Anim: that polygon is not in an animation";
		return;
	}

	// FUN_005406a0 (0x540764-0x54087c): each polygon of the group whose count of sequences, and count of frames in every sequence, are the clicked one's is selected; one that differs is given a NEW group number (one for all of them: the
	// counter is raised once after the walk), so the group the click chose holds only polygons that animate alike
	int32_t newGroup = 0;
	size_t split = 0;

	for (AnimShapeState const &each : shapes)
	{
		newGroup = model::NextAnimGroup(each.anim, newGroup);
	}

	for (AnimShapeState &each : shapes)
	{
		for (model::AnimPolygon &record : each.anim.polygons)
		{
			if (record.group != found)
			{
				continue;
			}

			bool same = record.sequences.size() == clicked.sequences.size();

			for (size_t s = 0; same && s < record.sequences.size(); s++)
			{
				same = record.sequences[s].Frames() == clicked.sequences[s].Frames();
			}

			if (!same)
			{
				record.group = newGroup;
				each.changed = true;
				split++;
			}
		}
	}

	if (split && !AnimCommit(open, &shapes))
	{
		return;
	}

	animGroup = found;
	animSeq = 0;
	animFrameOf.clear();
	AnimClamp(&shapes);
	TextureMapDeselectAll();

	for (AnimShapeState const &each : shapes)
	{
		// the polygons of the group, by the object this shape was reached through
		for (model::AnimPolygon const &record : each.anim.polygons)
		{
			if (record.group == animGroup)
			{
				TextureSelectPolygon(each.id, each.isModule, each.moduleIndex, record.polygon, true);
			}
		}
	}

	ShowTheTexturePolygons();
	AnimSay(&shapes, split ? "Selected (" + std::to_string(split) + " polygon" + (split == 1 ? "" : "s") + " that animate differently moved to a new animation)" : "Selected");
	(void)isModule;
	(void)moduleIndex;
}

// The lines of the flag list for the polygon the pointer is over (see textureFlagList); empty when it is over none
void TextureFlagListMake(host::PickReport const &report)
{
	textureFlagLines.clear();

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera || !report.hasHit)
	{
		return;
	}

	uint8_t target[8];

	if (report.found == host::PickFound::Object)
	{
		memcpy(target, report.objectId, 8);
	}
	else if (!ModuleObject(open, report.moduleIndex, target))
	{
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	host::FilePickRay ray;
	double const camera[3] = { (double)theCamera->x, (double)theCamera->y, (double)theCamera->z };

	for (int axis = 0; axis < 3; axis++)
	{
		ray.from[axis] = camera[axis] / scale;
		ray.direction[axis] = ((double)report.hitPoint[axis] - camera[axis]) / scale;
	}

	host::FilePickHit hit;

	if (host::FilePick_Object(open, open->Level(), target, ray, &hit) != host::FilePickResult::Done || !hit.found)
	{
		return;
	}

	model::ShapeModel shape;
	std::vector<int32_t> points, normals;

	if (host::FilePick_World(open, open->Level(), target, &shape, &points, &normals) != host::FilePickResult::Done || hit.polygon < 0 || (size_t)hit.polygon >= shape.polygons.size())
	{
		return;
	}

	model::ShapePolygon const &polygon = shape.polygons[(size_t)hit.polygon];
	int32_t partner = TexturePartnerOf(open, target, hit.polygon);

	if (partner >= (int32_t)shape.polygons.size())
	{
		partner = -1;
	}

	char line[160];
	std::vector<host::LevelObject> everyObject;

	LevelObjects_Read(&everyObject);

	for (host::LevelObject const &one : everyObject)
	{
		if (one.name && memcmp(one.id, target, 8) == 0)
		{
			textureFlagLines.push_back(one.name);
			break;
		}
	}

	snprintf(line, sizeof(line), "%d %d", hit.polygon, partner);
	textureFlagLines.push_back(line);

	if (polygon.engineType == 2)
	{
		textureFlagLines.push_back("Polygon");
	}
	else if (polygon.engineType == 5)
	{
		textureFlagLines.push_back("2dTextured");
	}
	else if (polygon.engineType == 7)
	{
		textureFlagLines.push_back("3dTextured");
	}
	else
	{
		snprintf(line, sizeof(line), "type : %d", polygon.engineType);
		textureFlagLines.push_back(line);
	}

	if (model::EngineTypeIsTextured(polygon.engineType))
	{
		snprintf(line, sizeof(line), "Texture : %d", polygon.colour & 0xfff);
		textureFlagLines.push_back(line);
	}

	std::vector<int32_t> shown;

	for (int pass = 0; pass < 2; pass++)
	{
		model::ShapePolygon const &one = pass == 0 ? polygon : shape.polygons[(size_t)(partner >= 0 ? partner : hit.polygon)];

		if (pass == 1 && partner < 0)
		{
			break;
		}

		for (int c = 0; c < one.cornerCount; c++)
		{
			int32_t const point = one.corners[c];

			if (point < 0 || (size_t)point * 3 + 2 >= points.size() || std::find(shown.begin(), shown.end(), point) != shown.end())
			{
				continue;
			}

			shown.push_back(point);
			snprintf(line, sizeof(line), "vert %d : %d,%d,%d", point, (int)(points[(size_t)point * 3] * scale * 100.0), (int)(points[(size_t)point * 3 + 1] * scale * 100.0), (int)(points[(size_t)point * 3 + 2] * scale * 100.0));
			textureFlagLines.push_back(line);
		}
	}

	for (int pass = 0; pass < (partner >= 0 ? 2 : 1); pass++)
	{
		size_t const which = (size_t)(pass == 0 ? hit.polygon : partner);

		if (which * 3 + 2 < normals.size())
		{
			snprintf(line, sizeof(line), partner >= 0 ? "norm%d_%d,%d,%d" : "norm_%d,%d,%d", pass + 1, normals[which * 3], normals[which * 3 + 1], normals[which * 3 + 2]);

			if (partner < 0)
			{
				snprintf(line, sizeof(line), "norm_%d,%d,%d", normals[which * 3], normals[which * 3 + 1], normals[which * 3 + 2]);
			}

			textureFlagLines.push_back(line);
		}
	}

	struct FlagName
	{
		uint32_t bit;
		char const *name;
	};

	static FlagName const names[] = { { 0x1, "notvis" }, { 0x2, "nolight" }, { 0x10, "transparent" }, { 0x20, "no_bfc" }, { 0x400000, "tx2dor3d" }, { 0x800000, "linear_s" }, { 0x4000000, "sortfarz" }, { 0x20000000, "avp_corona_flag" }, { 0x40000000, "temp_not_visible" } };

	for (FlagName const &one : names)
	{
		if ((uint32_t)polygon.flags & one.bit)
		{
			textureFlagLines.push_back(one.name);
		}
	}
}

// ARROW SELECTION (FUN_0054d260 0x54ff31-0x55000a: with the key in slot 0xd87a93 - S - down, an arrow key calls FUN_0056e620(direction, step size); FUN_0056e620 and FUN_0056e990): the polygons next to the selected ones, in the direction of the arrow ON THE SCREEN, are selected,
// as many times over as the step size says (1 to 100; the vertical one for up and down, the horizontal one for left and right: Set 'Arrow' Repeat Count, 0x100): each time the polygons just added are the ones that are stepped from. A polygon's step is across its edge that lies furthest that way
// in the camera's own space (inteng's corner furthest that way and the edge of it that is more across); the polygon on the other side of the edge is the one that shares both its points and is not selected yet. The other shapes are looked through too, as inteng does (points within 5000 of its unit of the edge's points, two corners in them). The edge is inteng's: the corner furthest that way in the camera's space and the more across of its two edges.
void TextureArrowSelect(int direction, int steps, int viewW, int viewH)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::PickCamera camera;

	if (!open || !open->Level() || !PickGame_Camera(viewW, viewH, &camera))
	{
		statusWords = "Texture Map: there is no frame to take the arrow's direction from";
		return;
	}

	if (texturePolygons.empty())
	{
		statusWords = "Texture Map: select a polygon first (S and an arrow key select the polygons next to the selected ones)";
		return;
	}

	struct Cache
	{
		bool ok;
		model::ShapeModel shape;
		std::vector<int32_t> points, normals;
	};

	struct OtherShape
	{
		uint8_t id[8];
		bool isModule;
		int32_t moduleIndex;
	};

	std::vector<OtherShape> others;

	TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
	{
		OtherShape one = {};

		memcpy(one.id, id, 8);
		one.isModule = isModule;
		one.moduleIndex = moduleIndex;
		others.push_back(one);
	});

	std::map<std::string, Cache> cache;
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<TexturePolygon> frontier = texturePolygons;
	size_t added = 0;

	steps = steps < 1 ? 1 : (steps > 100 ? 100 : steps);

	for (int step = 0; step < steps && !frontier.empty(); step++)
	{
		std::vector<TexturePolygon> next;

		for (TexturePolygon const &from : frontier)
		{
			std::string const key((char const *)from.id, 8);
			auto found = cache.find(key);

			if (found == cache.end())
			{
				Cache made;

				made.ok = host::FilePick_World(open, open->Level(), from.id, &made.shape, &made.points, &made.normals) == host::FilePickResult::Done;
				found = cache.emplace(key, made).first;
			}

			Cache const &shape = found->second;

			if (!shape.ok || from.polygon < 0 || (size_t)from.polygon >= shape.shape.polygons.size())
			{
				continue;
			}

			model::ShapePolygon const &polygon = shape.shape.polygons[(size_t)from.polygon];
			int const n = polygon.cornerCount;
			double sx[5] = { 0, 0, 0, 0, 0 }, sy[5] = { 0, 0, 0, 0, 0 }, cx[5] = { 0, 0, 0, 0, 0 }, cy[5] = { 0, 0, 0, 0, 0 };   // the screen places, and the camera's own x (across) and y (down) of each corner
			bool seen = n >= 3 && n <= 5;

			for (int c = 0; seen && c < n; c++)
			{
				size_t const point = (size_t)polygon.corners[c];

				if (polygon.corners[c] < 0 || point * 3 + 2 >= shape.points.size())
				{
					seen = false;
					break;
				}

				int32_t const world[3] = { (int32_t)(shape.points[point * 3] * scale), (int32_t)(shape.points[point * 3 + 1] * scale), (int32_t)(shape.points[point * 3 + 2] * scale) };
				double ahead = 0;

				seen = host::PickRay_Project(camera, world, &sx[c], &sy[c], &ahead);

				// back from the screen to the camera's space: x = (across - half the width) / half the width x ahead / xScale, y the same down (inteng works on the corners in the camera's space, FUN_004b4ba0)
				cx[c] = (sx[c] - camera.viewWidth / 2.0) / (camera.viewWidth / 2.0) * ahead / camera.xScale;
				cy[c] = (sy[c] - camera.viewHeight / 2.0) / (camera.viewHeight / 2.0) * ahead / camera.yScale;
			}

			if (!seen)
			{
				continue;
			}

			// FUN_0056e990 0x56ea6f-0x56ebdd: the corner furthest that way in the camera's space (0 up: -y, 1 left: -x, 2 right: x, 3 down: y; the first of equals), and of its two edges the one that lies more across the direction - for up and down the one with the greater x change, for left and right
			// the greater y change (the incoming edge, from the corner before, when its change is not smaller than the outgoing one's)
			int extreme = 0;
			double most = -1e300;

			for (int c = 0; c < n; c++)
			{
				double const value = direction == 0 ? -cy[c] : direction == 1 ? -cx[c] : direction == 2 ? cx[c] : cy[c];

				if (most < value)
				{
					most = value;
					extreme = c;
				}
			}

			int const before = (extreme + n - 1) % n, after = (extreme + 1) % n;
			double const incoming = fabs(direction == 0 || direction == 3 ? cx[extreme] - cx[before] : cy[extreme] - cy[before]);
			double const outgoing = fabs(direction == 0 || direction == 3 ? cx[after] - cx[extreme] : cy[after] - cy[extreme]);
			int const bestEdge = incoming < outgoing ? extreme : before;   // the edge's first corner; its second is the next one

			int32_t const pa = polygon.corners[bestEdge], pb = polygon.corners[(bestEdge + 1) % n];

			bool foundHere = false;

			for (size_t q = 0; q < shape.shape.polygons.size(); q++)
			{
				if ((int32_t)q == from.polygon)
				{
					continue;
				}

				model::ShapePolygon const &other = shape.shape.polygons[q];
				bool hasA = false, hasB = false;

				for (int c = 0; c < other.cornerCount; c++)
				{
					hasA = hasA || other.corners[c] == pa;
					hasB = hasB || other.corners[c] == pb;
				}

				if (hasA && hasB && TextureSelectedIndex(from.id, (int32_t)q) >= texturePolygons.size())
				{
					if (!TextureSelectPolygon(from.id, from.isModule, from.moduleIndex, (int32_t)q, true))
					{
						steps = 0;   // the selection is full
						break;
					}

					TexturePolygon one = from;

					one.polygon = (int32_t)q;
					next.push_back(one);
					added++;
					foundHere = true;
					break;
				}
			}

			if (foundHere || steps == 0)
			{
				continue;
			}

			// FUN_0056e990 0x56f0ef onward: no neighbour in this shape, so the OTHER shapes are looked through: the points of each that lie within 5000 of inteng's unit (every axis) of the edge's first point, or of its second, are listed, and a polygon with two corners in the lists is the
			// neighbour. The edge's two points are put in the world; the others' points are the file's, placed (FilePick_World), in the same unit (the file's number x 100 x the level's scale).
			{
				double const tolerance = 5000.0 / (100.0 * scale);
				double edgeA[3], edgeB[3];
				bool edge = true;

				for (int axis = 0; axis < 3; axis++)
				{
					size_t const ia = (size_t)pa * 3 + (size_t)axis, ib = (size_t)pb * 3 + (size_t)axis;

					edge = edge && ia < shape.points.size() && ib < shape.points.size();

					if (edge)
					{
						edgeA[axis] = shape.points[ia];
						edgeB[axis] = shape.points[ib];
					}
				}

				bool done = false;

				for (OtherShape &other : others)
				{
					if (!edge || done || memcmp(other.id, from.id, 8) == 0)
					{
						continue;
					}

					auto cached = cache.find(std::string((char const *)other.id, 8));

					if (cached == cache.end())
					{
						Cache made;

						made.ok = host::FilePick_World(open, open->Level(), other.id, &made.shape, &made.points, &made.normals) == host::FilePickResult::Done;
						cached = cache.emplace(std::string((char const *)other.id, 8), made).first;
					}

					Cache const &there = cached->second;

					if (!there.ok)
					{
						continue;
					}

					std::vector<char> closeTo(there.shape.points.size(), 0);

					for (size_t p = 0; p < there.shape.points.size() && p * 3 + 2 < there.points.size(); p++)
					{
						bool nearA = true, nearB = true;

						for (int axis = 0; axis < 3; axis++)
						{
							nearA = nearA && fabs(there.points[p * 3 + (size_t)axis] - edgeA[axis]) < tolerance;
							nearB = nearB && fabs(there.points[p * 3 + (size_t)axis] - edgeB[axis]) < tolerance;
						}

						closeTo[p] = (nearA || nearB) ? 1 : 0;
					}

					for (size_t q = 0; q < there.shape.polygons.size() && !done; q++)
					{
						int corners = 0;

						for (int c = 0; c < there.shape.polygons[q].cornerCount; c++)
						{
							int32_t const point = there.shape.polygons[q].corners[c];

							corners += (point >= 0 && (size_t)point < closeTo.size() && closeTo[(size_t)point]) ? 1 : 0;
						}

						if (corners >= 2 && TextureSelectedIndex(other.id, (int32_t)q) >= texturePolygons.size())
						{
							if (!TextureSelectPolygon(other.id, other.isModule, other.moduleIndex, (int32_t)q, true))
							{
								steps = 0;
								break;
							}

							TexturePolygon one = {};

							memcpy(one.id, other.id, 8);
							one.polygon = (int32_t)q;
							one.isModule = other.isModule;
							one.moduleIndex = other.moduleIndex;
							next.push_back(one);
							added++;
							done = true;
						}
					}
				}
			}
		}

		frontier = next;
	}

	ShowTheTexturePolygons();
	textureWords = "Texture Map: arrow selection added " + std::to_string(added) + " polygon" + (added == 1 ? "" : "s") + " (" + (direction == 0 ? "up" : direction == 1 ? "left" : direction == 2 ? "right" : "down") + ", " + std::to_string(steps) + " step" + (steps == 1 ? "" : "s") + ")";
	statusWords = textureWords.c_str();
}

// COPY TEXTURING > FROM MULTIPLE (0x13f) and TO MULTIPLE (0x140), then T or P (P: see below) (FUN_0054d260 -> FUN_00576360 / FUN_005765e0, the clicks FUN_00578ad0, the copy FUN_00576930 with FUN_00576cb0 (T) and FUN_00577fe0 (P)): the selected polygons (at least 2) are kept as the polygons to copy from and "Select reference poly 1" and "2" are clicked on two of
// them; then the same for the polygons to copy to (at least 2); then "T : Copy polygons based on topology / P : Copy polygons based on relative position / Escape : Cancel". T matches the two patches by how their polygons are joined (model/topology_match.h) and gives each polygon of the second the texturing (picture and coordinates, rotated by the match) of the
// one it was matched with (FUN_00575da0). NAMED: P (relative position, FUN_00577fe0) is not read yet and says so; a polygon that is a pair of triangles merged into a quad of the file's merge data is a quad here (its four coordinates are copied, which is what FUN_00575da0 does for two quads, but a quad matched with a pair of triangles is not); the 150 ms and the dialog texts of
// the original are not copied.
static std::vector<TexturePolygon> textureMultiFrom, textureMultiTo;
static int textureMultiRef[4] = { -1, -1, -1, -1 };   // positions in the two lists: from 1, from 2, to 1, to 2

static void TextureMultiCommand(int32_t command)
{
	if (command == 0x13f)
	{
		if (texturePolygons.size() < 2)
		{
			statusWords = "You must select at least 2 polygons to copy from";
			return;
		}

		textureMultiFrom = texturePolygons;
		textureMultiTo.clear();

		for (int &r : textureMultiRef)
		{
			r = -1;
		}

		textureMultiMode = 1;
		statusWords = "Copy from multiple polygons: Select reference poly 1 (a click on one of the selected polygons; Escape to cancel)";
		return;
	}

	if (textureMultiFrom.size() < 2)
	{
		statusWords = "You must select polygons to copy from first";
		return;
	}

	if (texturePolygons.size() < 2)
	{
		statusWords = "You must select at least 2 polygons to copy to";
		return;
	}

	textureMultiTo = texturePolygons;
	textureMultiRef[2] = textureMultiRef[3] = -1;
	textureMultiMode = 3;
	statusWords = "Copy to multiple polygons: Select reference poly 1 (a click on one of the selected polygons; Escape to cancel)";
}

static void TextureMultiClicked(uint8_t const *id, int32_t polygon)
{
	bool const to = textureMultiMode >= 3;
	std::vector<TexturePolygon> const &set = to ? textureMultiTo : textureMultiFrom;
	int at = -1;

	for (size_t i = 0; i < set.size(); i++)
	{
		if (set[i].polygon == polygon && memcmp(set[i].id, id, 8) == 0)
		{
			at = (int)i;
		}
	}

	if (at < 0)
	{
		statusWords = "Texture Map: that polygon is not one of the selected ones (click one of them)";
		return;
	}

	int &slot = textureMultiRef[textureMultiMode - 1];

	if ((textureMultiMode == 2 || textureMultiMode == 4) && at == textureMultiRef[textureMultiMode - 2])
	{
		statusWords = "Texture Map: the second reference polygon is another one than the first";
		return;
	}

	slot = at;

	if (textureMultiMode == 1)
	{
		textureMultiMode = 2;
		statusWords = "Copy from multiple polygons: Select reference poly 2";
	}
	else if (textureMultiMode == 2)
	{
		textureMultiMode = 0;
		statusWords = "Texture Map: the polygons to copy from are kept; select the polygons to copy to and use To Multiple";
	}
	else if (textureMultiMode == 3)
	{
		textureMultiMode = 4;
		statusWords = "Copy to multiple polygons: Select reference poly 2";
	}
	else
	{
		textureMultiMode = 5;
		statusWords = "Copying multiple polygons: T : Copy polygons based on topology, P : Copy polygons based on relative position, Escape : Cancel";
	}
}

// The nodes of one patch (model/topology_match.h): the polygons' corners in the world in inteng's unit, the edges shared with another of the patch
static bool TextureMultiNodes(OpenLevelFile *open, std::vector<TexturePolygon> const &set, bool reversed, std::vector<model::TopologyNode> *nodes, std::vector<int> *cornerCounts)
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<std::vector<int64_t>> positions(set.size());   // 3 numbers for each corner, in the order the node takes them
	std::map<std::string, std::pair<model::ShapeModel, std::vector<int32_t>>> cache;

	nodes->assign(set.size(), model::TopologyNode());
	cornerCounts->assign(set.size(), 0);

	for (size_t i = 0; i < set.size(); i++)
	{
		std::string const key((char const *)set[i].id, 8);
		auto found = cache.find(key);

		if (found == cache.end())
		{
			model::ShapeModel shape;
			std::vector<int32_t> points, normals;

			if (host::FilePick_World(open, open->Level(), set[i].id, &shape, &points, &normals) != host::FilePickResult::Done)
			{
				return false;
			}

			found = cache.emplace(key, std::make_pair(shape, points)).first;
		}

		model::ShapeModel const &shape = found->second.first;
		std::vector<int32_t> const &points = found->second.second;

		if (set[i].polygon < 0 || (size_t)set[i].polygon >= shape.polygons.size())
		{
			return false;
		}

		model::ShapePolygon const &polygon = shape.polygons[(size_t)set[i].polygon];

		if (polygon.cornerCount < 3 || polygon.cornerCount > 4)
		{
			return false;
		}

		(*nodes)[i].corners = polygon.cornerCount;
		(*cornerCounts)[i] = polygon.cornerCount;

		for (int k = 0; k < kTopologyEdgesMost; k++)
		{
			(*nodes)[i].neighbour[k] = -1;
		}

		for (int c = 0; c < polygon.cornerCount; c++)
		{
			int const source = reversed ? polygon.cornerCount - 1 - c : c;
			size_t const point = (size_t)polygon.corners[source];

			if (polygon.corners[source] < 0 || point * 3 + 2 >= points.size())
			{
				return false;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				positions[i].push_back((int64_t)floor(points[point * 3 + (size_t)axis] * scale * 100.0 + 0.5));
			}
		}
	}

	auto const same = [&](size_t a, int ca, size_t b, int cb)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			int64_t const d = positions[a][(size_t)ca * 3 + (size_t)axis] - positions[b][(size_t)cb * 3 + (size_t)axis];

			if (d <= -1000 || d > 1000)
			{
				return false;
			}
		}

		return true;
	};

	// FUN_00577930: edge k of a polygon (corner k to corner k+1) is shared with edge (l - 1) of another (corner l-1 to corner l) when the corners coincide, the two edges running opposite ways
	for (size_t a = 0; a < set.size(); a++)
	{
		for (size_t b = a + 1; b < set.size(); b++)
		{
			int const na = (*nodes)[a].corners, nb = (*nodes)[b].corners;

			for (int k = 0; k < na; k++)
			{
				for (int l = 0; l < nb; l++)
				{
					if (same(a, k, b, l) && same(a, (k + 1) % na, b, (l + nb - 1) % nb))
					{
						(*nodes)[a].neighbour[k] = (int)b;
						(*nodes)[b].neighbour[(l + nb - 1) % nb] = (int)a;
					}
				}
			}
		}
	}

	return true;
}

// THE P COPY (Copy Texturing > multiple, "P : Copy polygons based on relative position": FUN_00576930 -> FUN_00577fe0, with FUN_005788c0, FUN_005789c0, FUN_00576180, FUN_00576200): the second patch is laid over the first by the two reference polygons of each: a frame is made at each patch (the normal of reference 1, the direction from reference
// 1's middle to reference 2's, and their cross products), and the first patch's corners are carried into the second's space - minus its reference 1's middle, turned from the first frame to the second, scaled by the ratio of the two reference distances, plus the second's reference 1's middle, every number cut towards zero. A polygon of the first
// whose corners then lie on those of a polygon of the second (within 1000 of inteng's unit, the same count of corners, one corner of the second on its first and the rest in order) gives it its texturing; the rotation found is the corner it starts at. Done twice, the second time with the FIRST patch mirrored (corners the other way round, places and normals
// negated); the one that found more polygons is used (the first when equal). The copy per pair is FUN_00575da0's: triangles use the rotation as found, quads 4 minus it when not mirrored.
struct TextureMultiGeometry
{
	std::vector<std::vector<model::Fix3>> corners;   // each polygon's corners in inteng's unit, in the order of the pass
	std::vector<model::Fix3> normals;
};

static bool TextureMultiPositions(OpenLevelFile *open, std::vector<TexturePolygon> const &set, bool mirrored, TextureMultiGeometry *out)
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::map<std::string, std::tuple<model::ShapeModel, std::vector<int32_t>, std::vector<int32_t>>> cache;

	out->corners.assign(set.size(), std::vector<model::Fix3>());
	out->normals.assign(set.size(), model::Fix3{ 0, 0, 0 });

	for (size_t i = 0; i < set.size(); i++)
	{
		std::string const key((char const *)set[i].id, 8);
		auto found = cache.find(key);

		if (found == cache.end())
		{
			model::ShapeModel shape;
			std::vector<int32_t> points, normals;

			if (host::FilePick_World(open, open->Level(), set[i].id, &shape, &points, &normals) != host::FilePickResult::Done)
			{
				return false;
			}

			found = cache.emplace(key, std::make_tuple(shape, points, normals)).first;
		}

		model::ShapeModel const &shape = std::get<0>(found->second);
		std::vector<int32_t> const &points = std::get<1>(found->second);
		std::vector<int32_t> const &normals = std::get<2>(found->second);

		if (set[i].polygon < 0 || (size_t)set[i].polygon >= shape.polygons.size() || (size_t)set[i].polygon * 3 + 2 >= normals.size())
		{
			return false;
		}

		model::ShapePolygon const &polygon = shape.polygons[(size_t)set[i].polygon];

		if (polygon.cornerCount < 3 || polygon.cornerCount > 4)
		{
			return false;
		}

		for (int c = 0; c < polygon.cornerCount; c++)
		{
			int const source = mirrored ? polygon.cornerCount - 1 - c : c;
			size_t const point = (size_t)polygon.corners[source];

			if (polygon.corners[source] < 0 || point * 3 + 2 >= points.size())
			{
				return false;
			}

			model::Fix3 p;

			p.x = (int32_t)floor(points[point * 3] * scale * 100.0 + 0.5);
			p.y = (int32_t)floor(points[point * 3 + 1] * scale * 100.0 + 0.5);
			p.z = (int32_t)floor(points[point * 3 + 2] * scale * 100.0 + 0.5);

			if (mirrored)
			{
				p.x = -p.x;
				p.y = -p.y;
				p.z = -p.z;
			}

			out->corners[i].push_back(p);
		}

		model::Fix3 normal = { normals[(size_t)set[i].polygon * 3], normals[(size_t)set[i].polygon * 3 + 1], normals[(size_t)set[i].polygon * 3 + 2] };

		if (mirrored)
		{
			normal = { -normal.x, -normal.y, -normal.z };
		}

		out->normals[i] = normal;
	}

	return true;
}

static model::Fix3 TextureMultiCentre(std::vector<model::Fix3> const &corners)
{
	int64_t x = 0, y = 0, z = 0;

	for (model::Fix3 const &c : corners)
	{
		x += c.x;
		y += c.y;
		z += c.z;
	}

	int64_t const n = (int64_t)corners.size();

	return model::Fix3{ (int32_t)(x / n), (int32_t)(y / n), (int32_t)(z / n) };
}

// FUN_005789c0: the frame at a reference pair: the normal of the first, the direction between the two middles (cross products), and the distance between the middles
struct TextureMultiFrame
{
	double axis[3][3];   // normal, across, along (unit vectors)
	double length;
	model::Fix3 centre;
};

static bool TextureMultiFrameOf(TextureMultiGeometry const &g, int first, int second, TextureMultiFrame *frame)
{
	model::Fix3 const c1 = TextureMultiCentre(g.corners[(size_t)first]), c2 = TextureMultiCentre(g.corners[(size_t)second]);
	model::Fix3 direction = { c2.x - c1.x, c2.y - c1.y, c2.z - c1.z };
	double const length = sqrt((double)direction.x * direction.x + (double)direction.y * direction.y + (double)direction.z * direction.z);
	model::Fix3 normal = g.normals[(size_t)first];

	frame->centre = c1;
	frame->length = length;

	if (!(length > 0.0) || !model::FixedNormalise(&direction) || !model::FixedNormalise(&normal))
	{
		return false;
	}

	model::Fix3 across = model::FixedCross(direction, normal);

	if (!model::FixedNormalise(&across))
	{
		return false;
	}

	model::Fix3 along = model::FixedCross(normal, across);

	if (!model::FixedNormalise(&along))
	{
		return false;
	}

	model::Fix3 const rows[3] = { normal, across, along };

	for (int r = 0; r < 3; r++)
	{
		frame->axis[r][0] = rows[r].x / 65536.0;
		frame->axis[r][1] = rows[r].y / 65536.0;
		frame->axis[r][2] = rows[r].z / 65536.0;
	}

	return true;
}

// the first patch's corners carried into the second's space (FUN_00576180), then which polygons of the second they lie on (FUN_00576200): for each polygon of the first, the polygon of the second and the corner it starts at, or -1
static bool TextureMultiLay(TextureMultiGeometry const &from, int from1, int from2, TextureMultiGeometry const &to, int to1, int to2, std::vector<std::pair<int, int>> *found)
{
	TextureMultiFrame a, b;

	if (!TextureMultiFrameOf(from, from1, from2, &a) || !TextureMultiFrameOf(to, to1, to2, &b))
	{
		return false;
	}

	double const scale = b.length / a.length;

	found->assign(from.corners.size(), std::make_pair(-1, 0));

	for (size_t i = 0; i < from.corners.size(); i++)
	{
		std::vector<model::Fix3> moved;

		for (model::Fix3 const &p : from.corners[i])
		{
			double const d[3] = { (double)p.x - a.centre.x, (double)p.y - a.centre.y, (double)p.z - a.centre.z };
			double const coordinate[3] = { a.axis[0][0] * d[0] + a.axis[0][1] * d[1] + a.axis[0][2] * d[2], a.axis[1][0] * d[0] + a.axis[1][1] * d[1] + a.axis[1][2] * d[2], a.axis[2][0] * d[0] + a.axis[2][1] * d[1] + a.axis[2][2] * d[2] };
			double turned[3] = { 0, 0, 0 };

			for (int r = 0; r < 3; r++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					turned[axis] += coordinate[r] * b.axis[r][axis];
				}
			}

			moved.push_back(model::Fix3{ (int32_t)(turned[0] * scale) + b.centre.x, (int32_t)(turned[1] * scale) + b.centre.y, (int32_t)(turned[2] * scale) + b.centre.z });
		}

		for (size_t j = 0; j < to.corners.size(); j++)
		{
			size_t const n = moved.size();

			if (to.corners[j].size() != n)
			{
				continue;
			}

			bool hit = false;

			for (size_t r = 0; r < n && !hit; r++)
			{
				bool all = true;

				for (size_t k = 0; k < n && all; k++)
				{
					model::Fix3 const &t = to.corners[j][(r + k) % n];

					all = moved[k].x - t.x > -1000 && moved[k].x - t.x <= 1000 && moved[k].y - t.y > -1000 && moved[k].y - t.y <= 1000 && moved[k].z - t.z > -1000 && moved[k].z - t.z <= 1000;
				}

				if (all)
				{
					(*found)[i] = std::make_pair((int)j, (int)r);
					hit = true;
				}
			}

			if (hit)
			{
				break;
			}
		}
	}

	return true;
}

static void TextureMultiByPosition(OpenLevelFile *open)
{
	TextureMultiGeometry fromPlain, fromMirrored, to;

	if (!TextureMultiPositions(open, textureMultiFrom, false, &fromPlain) || !TextureMultiPositions(open, textureMultiFrom, true, &fromMirrored) || !TextureMultiPositions(open, textureMultiTo, false, &to))
	{
		statusWords = "Texture Map: the polygons could not be put in the world (only triangles and quads of readable shapes are matched)";
		return;
	}

	std::vector<std::pair<int, int>> plain, mirrored;
	bool const okPlain = TextureMultiLay(fromPlain, textureMultiRef[0], textureMultiRef[1], to, textureMultiRef[2], textureMultiRef[3], &plain);
	bool const okMirrored = TextureMultiLay(fromMirrored, textureMultiRef[0], textureMultiRef[1], to, textureMultiRef[2], textureMultiRef[3], &mirrored);

	if (!okPlain && !okMirrored)
	{
		statusWords = "Texture Map: the reference polygons give no frame (they must not sit at one place)";
		return;
	}

	auto const count = [](std::vector<std::pair<int, int>> const &list) { size_t n = 0; for (auto const &one : list) n += one.first >= 0 ? 1 : 0; return n; };
	size_t const countPlain = okPlain ? count(plain) : 0, countMirrored = okMirrored ? count(mirrored) : 0;
	bool const useMirrored = okMirrored && countPlain < countMirrored;
	std::vector<std::pair<int, int>> const &chosen = useMirrored ? mirrored : plain;

	if (count(chosen) == 0)
	{
		statusWords = "Texture Map: no polygon of the first patch lies on one of the second when laid over it by those reference polygons";
		return;
	}

	size_t copied = 0, left = 0;
	std::map<std::string, std::vector<model::PolygonUv>> uvForShape;
	std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> pictureForShape;

	for (size_t i = 0; i < chosen.size(); i++)
	{
		if (chosen[i].first < 0)
		{
			continue;
		}

		TexturePolygon const &fromPolygon = textureMultiFrom[i];
		TexturePolygon const &toPolygon = textureMultiTo[(size_t)chosen[i].first];
		model::PolygonUv source, destination;
		std::string why;

		if (host::UvEdit_ReadPolygon(open, open->Level(), fromPolygon.id, fromPolygon.polygon, &source, &why) != host::UvEditResult::Done || host::UvEdit_ReadPolygon(open, open->Level(), toPolygon.id, toPolygon.polygon, &destination, &why) != host::UvEditResult::Done ||
			!source.textured || !destination.textured)
		{
			left++;
			continue;
		}

		int const n = (int)to.corners[(size_t)chosen[i].first].size();
		int rotation = chosen[i].second;

		if (!useMirrored && n == 4)
		{
			rotation = 4 - rotation;
		}

		if (source.uv.cornerCount < n || destination.uv.cornerCount < n)
		{
			left++;
			continue;
		}

		model::UvList made = destination.uv;

		for (int k = 0; k < n; k++)
		{
			made.corners[k] = source.uv.corners[model::TopologySourceCorner(n, rotation, useMirrored, k)];
		}

		destination.uv = made;
		uvForShape[std::string((char const *)toPolygon.id, 8)].push_back(destination);
		pictureForShape[std::string((char const *)toPolygon.id, 8)].push_back(std::make_pair(toPolygon.polygon, source.colour & 0xfff));
		copied++;
	}

	for (auto &entry : uvForShape)
	{
		uint8_t id[8];
		std::string why;

		memcpy(id, entry.first.data(), 8);

		if (host::UvEdit_WritePolygons(open, open->Level(), id, entry.second, &why) != host::UvEditResult::Done)
		{
			left += entry.second.size();
			copied -= entry.second.size();
			continue;
		}

		model::ShapeModel shape;

		if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) == host::ShapeEditResult::Done)
		{
			for (auto const &picture : pictureForShape[entry.first])
			{
				if (picture.first >= 0 && (size_t)picture.first < shape.polygons.size())
				{
					int32_t &colour = shape.polygons[(size_t)picture.first].colour;

					colour = (int32_t)(((uint32_t)colour & ~0xfffu) | ((uint32_t)picture.second & 0xfffu));
				}
			}

			host::ShapeEdit_WriteSameCounts(open, open->Level(), id, shape, &why);
		}

		for (TexturePolygon const &one : textureMultiTo)
		{
			if (memcmp(one.id, id, 8) == 0)
			{
				ShowTheNewUv(open, one, entry.second);
				break;
			}
		}
	}

	textureWords = "Texture Map: the texturing of " + std::to_string(copied) + " polygon" + (copied == 1 ? "" : "s") + " was copied by relative position" + (useMirrored ? " (the first patch mirrored)" : "") + (left ? ", " + std::to_string(left) + " left as they were" : "") + " (found " + std::to_string(countPlain) + " plain, " + std::to_string(countMirrored) + " mirrored)";
	statusWords = textureWords.c_str();
}

void TextureMultiDo(bool relative)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	textureMultiMode = 0;

	if (!open || !open->Level())
	{
		return;
	}

	if (relative)
	{
		TextureMultiByPosition(open);
		return;
	}

	std::vector<model::TopologyNode> fromNodes, toNodes, toMirrored;
	std::vector<int> counts, countsMirrored, countsFrom;

	if (!TextureMultiNodes(open, textureMultiFrom, false, &fromNodes, &countsFrom) || !TextureMultiNodes(open, textureMultiTo, false, &toNodes, &counts) || !TextureMultiNodes(open, textureMultiTo, true, &toMirrored, &countsMirrored))
	{
		statusWords = "Texture Map: the polygons could not be put in the world (only triangles and quads of readable shapes are matched)";
		return;
	}

	if (fromNodes[(size_t)textureMultiRef[0]].corners != toNodes[(size_t)textureMultiRef[2]].corners)
	{
		statusWords = "The reference polygons have different number of sides";
		return;
	}

	model::TopologyMatch const match = model::MatchByTopology(fromNodes, textureMultiRef[0], textureMultiRef[1], toNodes, toMirrored, textureMultiRef[2], textureMultiRef[3]);

	if (!match.found)
	{
		statusWords = "Texture Map: the two patches do not match from those reference polygons (their second references are not matched together)";
		return;
	}

	size_t copied = 0, left = 0;
	std::map<std::string, std::vector<model::PolygonUv>> uvForShape;
	std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> pictureForShape;   // the shape, then (polygon, picture number)

	for (model::TopologyPair const &pair : match.pairs)
	{
		TexturePolygon const &from = textureMultiFrom[(size_t)pair.from];
		TexturePolygon const &to = textureMultiTo[(size_t)pair.to];
		model::PolygonUv source, destination;
		std::string why;

		if (host::UvEdit_ReadPolygon(open, open->Level(), from.id, from.polygon, &source, &why) != host::UvEditResult::Done || host::UvEdit_ReadPolygon(open, open->Level(), to.id, to.polygon, &destination, &why) != host::UvEditResult::Done ||
			!source.textured || !destination.textured)
		{
			left++;
			continue;
		}

		int const n = toNodes[(size_t)pair.to].corners;

		if (source.uv.cornerCount < n || destination.uv.cornerCount < n)
		{
			left++;
			continue;
		}

		model::UvList made = destination.uv;

		for (int k = 0; k < n; k++)
		{
			made.corners[k] = source.uv.corners[model::TopologySourceCorner(n, pair.rotation, match.mirrored, k)];
		}

		destination.uv = made;
		uvForShape[std::string((char const *)to.id, 8)].push_back(destination);
		pictureForShape[std::string((char const *)to.id, 8)].push_back(std::make_pair(to.polygon, source.colour & 0xfff));
		copied++;
	}

	for (auto &entry : uvForShape)
	{
		uint8_t id[8];
		std::string why;

		memcpy(id, entry.first.data(), 8);

		if (host::UvEdit_WritePolygons(open, open->Level(), id, entry.second, &why) != host::UvEditResult::Done)
		{
			left += entry.second.size();
			copied -= entry.second.size();
			continue;
		}

		// the picture numbers, on the shape as it is then
		model::ShapeModel shape;

		if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) == host::ShapeEditResult::Done)
		{
			for (auto const &picture : pictureForShape[entry.first])
			{
				if (picture.first >= 0 && (size_t)picture.first < shape.polygons.size())
				{
					int32_t &colour = shape.polygons[(size_t)picture.first].colour;

					colour = (int32_t)(((uint32_t)colour & ~0xfffu) | ((uint32_t)picture.second & 0xfffu));
				}
			}

			host::ShapeEdit_WriteSameCounts(open, open->Level(), id, shape, &why);
		}

		for (TexturePolygon const &one : textureMultiTo)
		{
			if (memcmp(one.id, id, 8) == 0)
			{
				ShowTheNewUv(open, one, entry.second);
				break;
			}
		}
	}

	textureWords = "Texture Map: the texturing of " + std::to_string(copied) + " polygon" + (copied == 1 ? "" : "s") + " was copied by topology" + (match.mirrored ? " (the second patch mirrored)" : "") + ", rotation " + std::to_string(match.rotation) + (left ? ", " + std::to_string(left) + " left as they were" : "") +
		(match.pairs.size() < textureMultiTo.size() ? "; " + std::to_string(textureMultiTo.size() - match.pairs.size()) + " polygons of the second patch were not matched" : "");
	statusWords = textureWords.c_str();
}

// COPY TEXTURING > OBJECT (0xb5: FUN_0054d260 sets DAT_00914d80 = -1; the click, FUN_005590c0, takes the object; FUN_00561690 and FUN_0055df10 copy): "Select object to copy from", then "Select object to copy to"; the two shapes must have the same number of polygons ("Objects
// contain different number of polygons"), and polygon i of the first gives its texturing to polygon i of the second when the two have the same corner points (the same point numbers): the engine type, the flags and the picture, the coordinates by corner (the second's corner takes the
// coordinates of the first's corner with the same point number), and the animation (the second's own is taken away first; it joins the first's group, its frames' coordinates by corner too). Then "Would you like to copy to another object?" (y keeps the first object). NAMED: the question
// about merge data that differ ("The merge data for the first shape won't work on the second...") is not asked: only polygons that match are copied, as if it were answered yes; a polygon that is untextured in either shape is left as it is (inteng would change its type, which needs a new list of coordinates).
static void TextureCopyObjectClicked(uint8_t const *target)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		textureCopyObject = 0;
		return;
	}

	if (textureCopyObject == 1)
	{
		memcpy(textureCopyObjectFrom, target, 8);
		textureCopyObject = 2;
		statusWords = "Select object to copy to (Esc cancels)";
		return;
	}

	if (memcmp(textureCopyObjectFrom, target, 8) == 0)
	{
		statusWords = "Texture Map: that is the object copied from (select another one to copy to)";
		return;
	}

	model::ShapeModel from, to;
	std::string why;

	if (host::ShapeEdit_Read(open->Level(), open, textureCopyObjectFrom, &from, nullptr, &why) != host::ShapeEditResult::Done || host::ShapeEdit_Read(open->Level(), open, target, &to, nullptr, &why) != host::ShapeEditResult::Done)
	{
		statusWords = "Texture Map: one of the two objects has no shape the editor can read";
		return;
	}

	if (from.polygons.size() != to.polygons.size())
	{
		statusWords = "Objects contain different number of polygons";
		return;
	}

	struct One
	{
		int32_t polygon;
		int map[5];   // the first shape's corner that the second's corner k takes its coordinates from
	};

	std::vector<One> matches;
	std::vector<model::PolygonUv> coordinates;
	size_t untextured = 0;

	for (size_t i = 0; i < from.polygons.size(); i++)
	{
		model::ShapePolygon const &a = from.polygons[i], &b = to.polygons[i];
		One one = {};

		one.polygon = (int32_t)i;

		bool same = a.cornerCount == b.cornerCount;

		for (int k = 0; same && k < b.cornerCount; k++)
		{
			one.map[k] = -1;

			for (int j = 0; j < a.cornerCount; j++)
			{
				if (a.corners[j] == b.corners[k])
				{
					one.map[k] = j;
				}
			}

			same = one.map[k] >= 0;
		}

		if (!same)
		{
			continue;
		}

		if (!model::EngineTypeIsTextured(a.engineType) || !model::EngineTypeIsTextured(b.engineType))
		{
			untextured++;
			continue;
		}

		model::PolygonUv ua, ub;

		if (host::UvEdit_ReadPolygon(open, open->Level(), textureCopyObjectFrom, (int32_t)i, &ua, &why) != host::UvEditResult::Done || host::UvEdit_ReadPolygon(open, open->Level(), target, (int32_t)i, &ub, &why) != host::UvEditResult::Done ||
			!ua.textured || !ub.textured || ua.uv.cornerCount < b.cornerCount || ub.uv.cornerCount < b.cornerCount)
		{
			continue;
		}

		for (int k = 0; k < b.cornerCount; k++)
		{
			ub.uv.corners[k] = ua.uv.corners[one.map[k]];
		}

		matches.push_back(one);
		coordinates.push_back(ub);
	}

	if (matches.empty())
	{
		statusWords = "Texture Map: no polygon of the second object has the corner points of the same polygon of the first (nothing was copied)";
		return;
	}

	// the coordinates first (they may give a polygon a list of its own), then the type, flags and picture on the shape as it is then
	std::string writeWhy;

	if (host::UvEdit_WritePolygons(open, open->Level(), target, coordinates, &writeWhy) != host::UvEditResult::Done)
	{
		textureWords = "Texture Map: the coordinates could not be written (" + writeWhy + ")";
		statusWords = textureWords.c_str();
		return;
	}

	model::ShapeModel now;

	if (host::ShapeEdit_Read(open->Level(), open, target, &now, nullptr, &why) == host::ShapeEditResult::Done)
	{
		for (One const &one : matches)
		{
			model::ShapePolygon &b = now.polygons[(size_t)one.polygon];
			model::ShapePolygon const &a = from.polygons[(size_t)one.polygon];

			b.engineType = a.engineType;
			b.flags = a.flags;
			b.colour = (int32_t)(((uint32_t)b.colour & ~0xfffu) | ((uint32_t)a.colour & 0xfffu));
		}

		host::ShapeEdit_WriteSameCounts(open, open->Level(), target, now, &writeWhy);
	}

	// the animation
	size_t animated = 0;
	{
		int32_t fromNumber = 0, toNumber = 0;
		model::TextureAnim fromAnim, toAnim;

		if (host::AnimEdit_Read(open, textureCopyObjectFrom, &fromNumber, &fromAnim) == host::AnimEditResult::Done && host::AnimEdit_Read(open, target, &toNumber, &toAnim) == host::AnimEditResult::Done && fromNumber != toNumber)
		{
			bool changed = false;

			for (One const &one : matches)
			{
				model::AnimPolygon const *source = nullptr;

				for (model::AnimPolygon const &record : fromAnim.polygons)
				{
					source = record.polygon == one.polygon ? &record : source;
				}

				size_t const before = toAnim.polygons.size();

				toAnim.polygons.erase(std::remove_if(toAnim.polygons.begin(), toAnim.polygons.end(), [&](model::AnimPolygon const &record) { return record.polygon == one.polygon; }), toAnim.polygons.end());
				changed = changed || toAnim.polygons.size() != before;

				if (!source || to.polygons[(size_t)one.polygon].cornerCount != 3)
				{
					continue;
				}

				model::AnimPolygon record = *source;

				for (model::AnimSequence &sequence : record.sequences)
				{
					std::vector<int32_t> mapped = sequence.uv;

					for (size_t f = 0; f < sequence.Frames(); f++)
					{
						for (int k = 0; k < 3; k++)
						{
							mapped[f * 6 + (size_t)k * 2] = sequence.uv[f * 6 + (size_t)one.map[k] * 2];
							mapped[f * 6 + (size_t)k * 2 + 1] = sequence.uv[f * 6 + (size_t)one.map[k] * 2 + 1];
						}
					}

					sequence.uv = mapped;
				}

				toAnim.polygons.push_back(record);
				changed = true;
				animated++;
			}

			if (changed)
			{
				host::AnimWrite write;

				write.shapeNumber = toNumber;
				write.anim = toAnim;

				std::vector<host::AnimWrite> writes(1, write);

				if (host::AnimEdit_Write(open, writes, &writeWhy) == host::AnimEditResult::Done)
				{
					LoadTheObjectListAgain();
				}
			}
		}
	}

	textureWords = "Texture Map: the texturing of " + std::to_string(matches.size()) + " polygon" + (matches.size() == 1 ? "" : "s") + " was copied to the second object" + (animated ? " (" + std::to_string(animated) + " with their animation)" : "") +
		(untextured ? ", " + std::to_string(untextured) + " untextured left as they were" : "");
	statusWords = textureWords.c_str();

	// "Would you like to copy to another object?"
	textureAsk = 12;
	memset(keyDown, 0, sizeof(keyDown));
	promptFor = kPromptTextureMap;
	promptText.clear();
}

// THE TEXTURE CHOOSER (Select Texture 0x2a, also the texture window's Texture > Select; FUN_005575a0): a grid of every texture of the level over the view, each picture in its cell; the hovered one is framed and its size and name are told; a left click picks it (the same "set this texture" as Next and Prev:
// every selected polygon takes it and it becomes the current one); Esc cancels. NAMED: only the level's own pictures are offered (those a polygon's number can name, as Next Texture offers them); inteng's used-only switch, the sets and the keys (K: Keys, U, ...) are not built.
bool textureChooserOn = false;
static std::vector<LevelViewPicture> textureChooserList;
int textureChooserMouseX = -1, textureChooserMouseY = -1;
static int32_t textureChooserFrameW = 0, textureChooserFrameH = 0;

static void TextureChooserCells(int32_t windowWidth, int32_t windowHeight, std::vector<TextureChooserCell> *cells)
{
	cells->clear();

	int32_t const top = 66, bottom = windowHeight - 36 - 48;   // under the bars, over the status line and the words
	int32_t const width = windowWidth - 16, height = bottom - top - 4;
	size_t const count = textureChooserList.size();

	if (count == 0 || width < 64 || height < 64)
	{
		return;
	}

	int32_t columns = 1;

	while ((size_t)columns * (size_t)columns * (size_t)height < count * (size_t)width)
	{
		columns++;
	}

	int32_t const rows = (int32_t)((count + (size_t)columns - 1) / (size_t)columns);
	int32_t const cell = (width / columns < height / rows ? width / columns : height / rows);

	if (cell < 8)
	{
		return;
	}

	for (size_t i = 0; i < count; i++)
	{
		TextureChooserCell one;

		one.texture = textureChooserList[i].texture;
		one.x = 8 + (int32_t)(i % (size_t)columns) * cell + 2;
		one.y = top + 2 + (int32_t)(i / (size_t)columns) * cell + 2;
		one.width = cell - 4;
		one.height = cell - 4;
		cells->push_back(one);
	}
}

static int TextureChooserCellAt(std::vector<TextureChooserCell> const &cells, int x, int y)
{
	for (size_t i = 0; i < cells.size(); i++)
	{
		if (x >= cells[i].x && x < cells[i].x + cells[i].width && y >= cells[i].y && y < cells[i].y + cells[i].height)
		{
			return (int)i;
		}
	}

	return -1;
}

static void TextureChooserOpen()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	textureChooserList.clear();

	if (!open || !open->Level() || !LevelView_Pictures(&textureChooserList, 4096))
	{
		statusWords = "Texture Map: the game has no picture loaded";
		return;
	}

	BuildThePictureIndex(open);
	textureChooserList.erase(std::remove_if(textureChooserList.begin(), textureChooserList.end(), [](LevelViewPicture const &picture) { return pictureIndexOf.find(picture.texture) == pictureIndexOf.end(); }), textureChooserList.end());
	textureChooserOn = !textureChooserList.empty();
	statusWords = textureChooserOn ? "Texture Map: the texture chooser (a left click picks the texture, Esc cancels)" : "Texture Map: no picture of the level's list is in use";
}

void TextureChooserDraw(int32_t windowWidth, int32_t windowHeight)
{
	textureChooserFrameW = windowWidth;
	textureChooserFrameH = windowHeight;

	std::vector<TextureChooserCell> cells;

	TextureChooserCells(windowWidth, windowHeight, &cells);

	int const hovered = TextureChooserCellAt(cells, textureChooserMouseX, textureChooserMouseY);
	std::vector<std::string> lines;

	if (hovered >= 0)
	{
		LevelViewPicture const &picture = textureChooserList[(size_t)hovered];

		lines.push_back(std::to_string(picture.width) + " X " + std::to_string(picture.height));
		lines.push_back(picture.name);
	}
	else
	{
		lines.push_back("Select a texture: a left click picks it, Esc cancels");
	}

	TextureChooserPaint_Draw(windowWidth, windowHeight, 56, windowHeight - 56 - 36, cells, hovered, lines, 16, windowHeight - 36 - 40);
}

// A left click in the chooser: the cell under it is the texture
void TextureChooserClick(int x, int y)
{
	std::vector<TextureChooserCell> cells;

	TextureChooserCells(textureChooserFrameW, textureChooserFrameH, &cells);

	int const at = TextureChooserCellAt(cells, x, y);

	if (at >= 0)
	{
		textureChooserOn = false;
		TextureMapSetPicture(textureChooserList[(size_t)at].texture);
		textureCurrent = textureChooserList[(size_t)at].texture;
	}
}

// PLAY and PLAY ALL (FUN_00541720: the game's own texture animation is built from each polygon's current sequence - its speed in 16.16 frames a second, its flags, one more frame leading back to the first unless the flag 0x80000000 is set - and runs on the polygons
// until a mouse click, FUN_00541b70, puts them back). Here the items of the polygons take the frame of the moment through the level view (no file write), the frame found from the time; the Interpolate flag mixes the coordinates of two frames, the No Loop flag stops on the last.
struct AnimPlayItem
{
	LevelViewShapeOf shape;
	int32_t item;
	model::AnimSequence sequence;
	int32_t fileTexture;   // what the polygon shows in the file (the picture number), to put back
	int32_t fileUv[6];
};

static std::vector<AnimPlayItem> animPlay;
static DWORD animPlayStart = 0;

static void AnimPlayStart(OpenLevelFile *open, std::vector<AnimShapeState> *shapes, bool all)
{
	animPlay.clear();
	BuildThePictureIndex(open);

	for (AnimShapeState const &shape : *shapes)
	{
		LevelViewShapeOf place = {};
		model::ShapeModel model;
		std::string why;
		std::vector<uint8_t> corners;
		std::vector<int32_t> numbers, toItem, firstFile;
		bool hasMerge = false;

		place.isModule = shape.isModule;
		place.moduleIndex = shape.moduleIndex;
		memcpy(place.objectId, shape.id, 8);

		if (host::ShapeEdit_Read(open->Level(), open, shape.id, &model, nullptr, &why) != host::ShapeEditResult::Done || !LevelView_ItemCorners(place, &corners) || !host::PolygonItems_ReadMergeData(open, open->Level(), shape.id, &numbers, &hasMerge) ||
			host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), model.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) != host::PolygonItemResult::Ok)
		{
			continue;
		}

		for (model::AnimPolygon const &record : shape.anim.polygons)
		{
			int const at = all ? 0 : animSeq;

			if ((!all && record.group != animGroup) || (size_t)at >= record.sequences.size() || record.polygon < 0 || (size_t)record.polygon >= toItem.size())
			{
				continue;
			}

			AnimPlayItem one;

			one.shape = place;
			one.item = toItem[(size_t)record.polygon];
			one.sequence = record.sequences[(size_t)at];
			one.fileTexture = 0;
			memset(one.fileUv, 0, sizeof(one.fileUv));

			if (AnimPolygonNow(open, shape.id, record.polygon, &one.fileTexture, one.fileUv))
			{
				animPlay.push_back(one);
			}
		}
	}

	animPlayStart = GetTickCount();
	textureWords = animPlay.empty() ? "Anim: nothing to play" : "Anim: playing " + std::to_string(animPlay.size()) + " polygon" + (animPlay.size() == 1 ? "" : "s") + " (a key or a click stops it)";
	statusWords = textureWords.c_str();
}

static void AnimPlayShow(AnimPlayItem const &one, int32_t texture, int32_t const uv[6])
{
	auto const found = pictureTextureOf.find(texture);

	if (found != pictureTextureOf.end())
	{
		LevelView_SetItemTexture(one.shape, one.item, found->second);
	}

	LevelView_SetItemUv(one.shape, one.item, uv, 3);
}

void AnimPlayTick()
{
	if (animPlay.empty())
	{
		return;
	}

	double const seconds = (double)(GetTickCount() - animPlayStart) / 1000.0;

	for (AnimPlayItem const &one : animPlay)
	{
		model::AnimSequence const &sequence = one.sequence;
		int const frames = (int)sequence.Frames();
		int const count = frames + ((sequence.flags & model::kAnimNoInterpToFirst) ? 0 : 1);

		if (frames < 1 || count < 1)
		{
			continue;
		}

		double t = seconds * (double)sequence.speed / 65536.0;

		t = (sequence.flags & model::kAnimNoLoop) ? (t > count - 1 ? (double)(count - 1) : t) : fmod(t, (double)count);

		int const a = (int)floor(t);
		double const fraction = t - a;
		int const frameA = a % frames, frameB = (a + 1 >= count && !(sequence.flags & model::kAnimNoLoop) ? 0 : a + 1) % frames;
		int32_t uv[6];

		for (int k = 0; k < 6; k++)
		{
			double const from = sequence.uv[(size_t)frameA * 6 + (size_t)k], to = sequence.uv[(size_t)frameB * 6 + (size_t)k];

			uv[k] = (int32_t)floor(((sequence.flags & model::kAnimInterpolate) ? from + (to - from) * fraction : from) + 0.5);
		}

		AnimPlayShow(one, sequence.textures[(size_t)frameA], uv);
	}
}

// A key or a click ends the play and the polygons show what the file holds again (FUN_00541b70)
bool AnimPlayStop()
{
	if (animPlay.empty())
	{
		return false;
	}

	for (AnimPlayItem const &one : animPlay)
	{
		AnimPlayShow(one, one.fileTexture, one.fileUv);
	}

	animPlay.clear();
	statusWords = "Anim: play stopped";
	return true;
}

// COPY (0xb3, FUN_0055c210 with 1: "Get Anim from which polygon"): the clicked polygon must be in an animation; every selected polygon but that one that is a textured triangle first takes the clicked polygon's coordinates as Copy Texturing gives them, has its own
// animation taken away if it had one (FUN_005413d0), and gets a copy of the clicked polygon's record in a NEW group of its own (the largest group number plus one, for each polygon): every sequence and frame the same, the picture and the six coordinates of each frame copied as they are.
static void TextureAnimCopyClicked(uint8_t const *id, int32_t polygon)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<AnimShapeState> shapes;

	if (!open || !open->Level())
	{
		return;
	}

	AnimReadAll(open, &shapes);

	AnimShapeState *const sourceShape = AnimShapeOfId(&shapes, id, open);
	model::AnimPolygon source;
	bool have = false;

	for (size_t i = 0; sourceShape && i < sourceShape->anim.polygons.size(); i++)
	{
		if (sourceShape->anim.polygons[i].polygon == polygon)
		{
			source = sourceShape->anim.polygons[i];
			have = true;
		}
	}

	if (!have)
	{
		statusWords = "Anim: that polygon is not in an animation (nothing was copied)";
		return;
	}

	if (texturePolygons.empty())
	{
		statusWords = "Anim: select the polygons to give the animation to first";
		return;
	}

	// the coordinates, as Copy Texturing does
	model::PolygonUv sourceUv;
	std::string why;

	if (host::UvEdit_ReadPolygon(open, open->Level(), id, polygon, &sourceUv, &why) == host::UvEditResult::Done && sourceUv.textured && sourceUv.uv.cornerCount >= 3)
	{
		size_t left = 0;

		TextureApplyUv(open, id, polygon, sourceUv.uv, sourceUv.uv.cornerCount, &left);
	}

	size_t copied = 0;

	for (TexturePolygon const &one : texturePolygons)
	{
		AnimShapeState *const shape = AnimShapeOfId(&shapes, one.id, open);
		model::ShapeModel model;
		int32_t texture, uv[6];

		if (!shape || (memcmp(one.id, id, 8) == 0 && one.polygon == polygon) || host::ShapeEdit_Read(open->Level(), open, one.id, &model, nullptr, &why) != host::ShapeEditResult::Done || one.polygon < 0 || (size_t)one.polygon >= model.polygons.size() ||
			model.polygons[(size_t)one.polygon].cornerCount != 3 || !AnimPolygonNow(open, one.id, one.polygon, &texture, uv))
		{
			continue;
		}

		int32_t group = 0;

		for (AnimShapeState const &each : shapes)
		{
			group = model::NextAnimGroup(each.anim, group);
		}

		shape->anim.polygons.erase(std::remove_if(shape->anim.polygons.begin(), shape->anim.polygons.end(), [&](model::AnimPolygon const &have) { return have.polygon == one.polygon; }), shape->anim.polygons.end());

		model::AnimPolygon record = source;

		record.polygon = one.polygon;
		record.group = group;
		shape->anim.polygons.push_back(record);
		shape->changed = true;
		copied++;
	}

	if (!copied)
	{
		statusWords = "Anim: no selected polygon could take the animation (a textured triangle that is not the clicked one)";
		return;
	}

	if (AnimCommit(open, &shapes))
	{
		textureWords = "Anim: the animation was copied to " + std::to_string(copied) + " polygon" + (copied == 1 ? "" : "s") + ", each in an animation of its own";
		statusWords = textureWords.c_str();
	}
}

// The typed answers (kPromptTextureMap with textureAsk 4 to 11); false when the question is none of these
static bool TextureAnimTyped(int ask, std::string const &typed)
{
	if (ask < 4 || ask > 11)
	{
		return false;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<AnimShapeState> shapes;

	if (!open || !open->Level())
	{
		return true;
	}

	if (ask == 10 && (typed == "y" || typed == "Y" || typed == "yes" || typed == "Yes"))
	{
		AnimMakeSelectedTextured(open);   // an untextured polygon is made a textured one first, as inteng does
	}

	AnimReadAll(open, &shapes);
	AnimClamp(&shapes);

	model::AnimPolygon *const reference = animGroup < 0 ? nullptr : AnimFirstOfGroup(&shapes);

	if (!reference)
	{
		statusWords = "Anim: no animation is current";
		return true;
	}

	AnimCapture(open, &shapes);

	auto const everyMember = [&](std::function<void(AnimShapeState &, model::AnimPolygon &)> const &act)
	{
		for (AnimShapeState &shape : shapes)
		{
			for (model::AnimPolygon &polygon : shape.anim.polygons)
			{
				if (polygon.group == animGroup)
				{
					act(shape, polygon);
					shape.changed = true;
				}
			}
		}
	};

	bool const yes = typed == "y" || typed == "Y" || typed == "yes" || typed == "YES" || typed == "Yes";

	if (ask >= 7)
	{
		if (!yes)
		{
			statusWords = "Anim: nothing was changed";
			return true;
		}

		if (ask == 7)   // destroy the animation (FUN_00540100)
		{
			everyMember([&](AnimShapeState &, model::AnimPolygon &polygon) { polygon.group = -2; });

			for (AnimShapeState &shape : shapes)
			{
				shape.anim.polygons.erase(std::remove_if(shape.anim.polygons.begin(), shape.anim.polygons.end(), [](model::AnimPolygon const &polygon) { return polygon.group == -2; }), shape.anim.polygons.end());
			}

			animGroup = -1;
		}
		else if (ask == 8)   // destroy the current sequence (FUN_00540400)
		{
			int const at = animSeq;

			everyMember([&](AnimShapeState &, model::AnimPolygon &polygon)
			{
				if ((size_t)at < polygon.sequences.size())
				{
					polygon.sequences.erase(polygon.sequences.begin() + at);
				}
			});
			animFrameOf.erase(animFrameOf.begin() + at);
		}
		else if (ask == 9)   // remove the current frame (FUN_00540600)
		{
			int const at = animFrameOf[(size_t)animSeq];
			int const seq = animSeq;

			everyMember([&](AnimShapeState &, model::AnimPolygon &polygon) { model::AnimRemoveFrame(&polygon.sequences[(size_t)seq], (size_t)at); });
		}
		else if (ask == 10)   // make the selected polygons part of the animation (FUN_00540880): each gets the group's sequences and frames, every frame holding what the polygon shows
		{
			model::AnimPolygon const templateRecord = *reference;
			size_t added = 0;

			for (TexturePolygon const &one : texturePolygons)
			{
				AnimShapeState *const shape = AnimShapeOfId(&shapes, one.id, open);
				model::ShapeModel model;
				std::string why;
				int32_t texture, uv[6];

				if (!shape || host::ShapeEdit_Read(open->Level(), open, one.id, &model, nullptr, &why) != host::ShapeEditResult::Done || one.polygon < 0 || (size_t)one.polygon >= model.polygons.size() || model.polygons[(size_t)one.polygon].cornerCount != 3 || !AnimPolygonNow(open, one.id, one.polygon, &texture, uv))
				{
					continue;
				}

				bool animated = false;

				for (model::AnimPolygon const &have : shape->anim.polygons)
				{
					animated = animated || have.polygon == one.polygon;
				}

				if (animated)
				{
					continue;
				}

				model::AnimPolygon record = templateRecord;

				record.polygon = one.polygon;

				for (model::AnimSequence &sequence : record.sequences)
				{
					for (size_t f = 0; f < sequence.Frames(); f++)
					{
						sequence.textures[f] = texture;
						memcpy(&sequence.uv[f * 6], uv, sizeof(uv));
					}
				}

				shape->anim.polygons.push_back(record);
				shape->changed = true;
				added++;
			}

			textureWords = "Anim: " + std::to_string(added) + " polygon" + (added == 1 ? "" : "s") + " made part of the animation";
		}
		else   // 11: take the selected polygons out of the animation (FUN_00540ab0)
		{
			for (TexturePolygon const &one : texturePolygons)
			{
				AnimShapeState *const shape = AnimShapeOfId(&shapes, one.id, open);

				if (!shape)
				{
					continue;
				}

				size_t const before = shape->anim.polygons.size();

				shape->anim.polygons.erase(std::remove_if(shape->anim.polygons.begin(), shape->anim.polygons.end(), [&](model::AnimPolygon const &polygon) { return polygon.group == animGroup && polygon.polygon == one.polygon; }), shape->anim.polygons.end());
				shape->changed = shape->changed || shape->anim.polygons.size() != before;
			}
		}
	}
	else if (ask == 4)   // Set Anim Number
	{
		char *end = nullptr;
		long const number = strtol(typed.c_str(), &end, 10);

		if (typed.empty() || !end || *end != '\0')
		{
			statusWords = "Anim: the anim number is a whole number";
			return true;
		}

		everyMember([&](AnimShapeState &, model::AnimPolygon &polygon) { polygon.identifier = (int32_t)number; });
	}
	else if (ask == 5)   // Sequence Copy
	{
		char *end = nullptr;
		long const which = strtol(typed.c_str(), &end, 10);

		if (typed.empty() || !end || *end != '\0' || which < 0 || which >= (long)reference->sequences.size() || which == animSeq)
		{
			statusWords = "Anim: Copy which sequence: a number from 0 up to the last sequence, not the current one";
			return true;
		}

		int const into = animSeq;

		everyMember([&](AnimShapeState &, model::AnimPolygon &polygon) { polygon.sequences[(size_t)into] = polygon.sequences[(size_t)which]; });
		animFrameOf[(size_t)into] = 0;
	}
	else   // 6: Change Properties
	{
		// "frames seconds loop interpolate nointerp notplaying" (the dialog's fr/sec fields and its four check boxes)
		long values[6] = { 0, 0, 0, 0, 0, 0 };
		char const *at = typed.c_str();
		int got = 0;

		while (got < 6)
		{
			char *end = nullptr;
			long const value = strtol(at, &end, 10);

			if (end == at)
			{
				break;
			}

			values[got++] = value;
			at = end;
		}

		if (got != 6)
		{
			statusWords = "Anim: Change Properties: six whole numbers - frames and seconds (fr/sec; the speed changes only when both are above 0), then loop, interpolate, no interp from last to first, not playing at start (each 0 or 1)";
			return true;
		}

		// FUN_005411d0 (0x541263-0x5412b8): the speed is (double)frames / (double)seconds x 65536.0 truncated (the constant at 0x5ac7c8 is 65536.0), set only when both numbers are above 0; the sequence's flags are made again from the check boxes (0, then
		// 4 when Loop is NOT checked, 8 for Interpolate, 0x80000000 for No Interp from last to first), the polygon's flags too (0, then 1 for Not playing at start); the first polygon's values are then copied to every other polygon of the group
		int32_t speed = reference->sequences[(size_t)animSeq].speed;

		if (values[0] > 0 && values[1] > 0)
		{
			speed = (int32_t)((double)values[0] / (double)values[1] * 65536.0);
		}

		int32_t const flags = (values[2] ? 0 : model::kAnimNoLoop) | (values[3] ? model::kAnimInterpolate : 0) | (values[4] ? model::kAnimNoInterpToFirst : 0);
		int32_t const recordFlags = values[5] ? model::kAnimNotPlaying : 0;
		int const into = animSeq;

		everyMember([&](AnimShapeState &, model::AnimPolygon &polygon)
		{
			model::AnimSequence &sequence = polygon.sequences[(size_t)into];

			sequence.speed = speed;
			sequence.flags = flags;
			polygon.flags = recordFlags;
		});
	}

	AnimClamp(&shapes);

	if (animGroup >= 0)
	{
		AnimShow(open, &shapes);
	}

	if (AnimCommit(open, &shapes))
	{
		AnimSay(&shapes, ask == 10 ? textureWords : "");
	}

	return true;
}

std::string TextureAnimTitle(int ask)
{
	switch (ask)
	{
	case 4: return "Select anim number for texture animation (now " + std::to_string(animNumber) + ")";
	case 5: return "Copy which sequence? Type its number (0 is the first; not the current one, " + std::to_string(animSeq) + ")";
	case 6: return "Animation properties: type frames and seconds (fr/sec, 1 1 is one frame a second), then loop, interpolate, no interp from last to first, not playing at start, each 0 or 1 (for example 1 1 1 0 0 0)";
	case 7: return "Are you sure you want to destroy this animation? (type y for yes)";
	case 8: return "Are you sure you want to destroy this sequence? (type y for yes)";
	case 9: return "Are you sure you want to remove this frame? (type y for yes)";
	case 10: return "Are you sure you want to make the selected polygons part of the animation? (type y for yes)";
	case 11: return "Are you sure you want to remove the selected polygons from the animation? (type y for yes)";
	case 12: return "Would you like to copy to another object? (type y for yes)";
	case 13: return std::string("AutoSave: type the frequency in minutes (0 turns it off) and 1 to be asked before each save or 0 not to (now ") + (textureAutoMinutes ? "saving every " + std::to_string(textureAutoMinutes) + " minutes" : "off") + (textureAutoPrompt ? ", asking first" : "") + "; for example 10 0)";
	case 14: return "AutoSave... Would you like to save the changes you have made? (type y for yes)";
	case 15: return "Shift uv coordinates by how much? Type the u shift and the v shift in whole texels (for example 4 -2)";
	case 16: return "How much would you like to scale the uv coordinates by? Type the factor (1 changes nothing)";
	case 20: return "Are you sure that you want to delete the selected polygons? (type y for yes)";
	case 21: return "Would you like polygons with 0 normals to be flagged for deletion? (type y for yes)";
	case 22: case 23: return "Are you sure that you want to make these polys into a new object? (type y for yes)";
	case 24: return "Spliting Objects - Please give name for new object (Escape chooses default name)";
	}

	return "";
}

// Before a copy is saved: the frame the polygons show goes into the animation's chunk (inteng does it when another frame is gone to and at the save)
void TextureAnimFlush()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (animGroup < 0 || !open || !open->Level())
	{
		return;
	}

	std::vector<AnimShapeState> shapes;

	AnimReadAll(open, &shapes);
	AnimClamp(&shapes);

	if (animGroup < 0)
	{
		return;
	}

	AnimCapture(open, &shapes);
	AnimCommit(open, &shapes);
}

// THE FIND ITEMS of the Misc menu (0xb7 Untextured polys, 0xb8 textured poly, 0xb9 Invalid PSX UV coords, and 0xba / 0xbb / 0xbc the same with "Find and select"; FUN_0054d260 cases 0xb7-0xbc, FUN_00561970, FUN_00561e00, FUN_00562770): the polygons
// of every shape that is not imported are listed - the ones with no picture (0xb7), the ones using the current texture (0xb8), the ones with a texture coordinate outside the picture (0xb9: for the first three corners, a number below 0 or not below
// the picture's width: FUN_00562770, 0x562909 / 0x5629df) - one for each pair of triangles (the first of a quad's two); the selecting forms also select them (FUN_0056db80). G (0xbd) goes to the next polygon of the list, Shift+G to the previous: the camera is put
// in front of the polygon, 2.5 metres out along its normal, looking at it (FUN_00563100, dist 250000 of inteng's unit). NAMED: inteng asks "N polygons haven't been done in <object>. Would you like to go to this object?" and goes to the first
// object on a yes; here the list is kept, the first polygon is gone to at once, and "All polygons have been done." is said for an empty list; the animated-texture lists (the polygons whose texture has animation frames) are not walked; the camera's look up/down
// sign is the game's pan angle (positive looks up) as read from its Euler matrix, not tried against inteng.

std::vector<TextureFound> textureFound;
size_t textureFoundAt = 0;

void TextureGoto(int direction)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera || textureFound.empty())
	{
		statusWords = "Texture Map: there is no list of polygons to go through (use a Find item first)";
		return;
	}

	if (direction > 0)
	{
		textureFoundAt = (textureFoundAt + 1) % textureFound.size();
	}
	else
	{
		textureFoundAt = (textureFoundAt + textureFound.size() - 1) % textureFound.size();
	}

	TextureFound const &one = textureFound[textureFoundAt];
	model::ShapeModel shape;
	std::vector<int32_t> points, normals;

	if (host::FilePick_World(open, open->Level(), one.id, &shape, &points, &normals) != host::FilePickResult::Done || one.polygon < 0 || (size_t)one.polygon >= shape.polygons.size() || shape.polygons[(size_t)one.polygon].cornerCount < 3)
	{
		statusWords = "Texture Map: that polygon cannot be placed";
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	double centre[3] = { 0, 0, 0 };

	for (int c = 0; c < 3; c++)   // the average of the first three corners (FUN_00563100 0x563148-0x563224)
	{
		size_t const point = (size_t)shape.polygons[(size_t)one.polygon].corners[c];

		if (point * 3 + 2 >= points.size())
		{
			return;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			centre[axis] += (double)points[point * 3 + axis] * scale / 3.0;
		}
	}

	double n[3] = { (double)normals[(size_t)one.polygon * 3], (double)normals[(size_t)one.polygon * 3 + 1], (double)normals[(size_t)one.polygon * 3 + 2] };
	double const length = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

	if (!(length > 0.0))
	{
		statusWords = "Texture Map: that polygon has no normal to look along";
		return;
	}

	for (double &part : n)
	{
		part /= length;
	}

	double const distance = textureGotoDistance;   // 250000 of inteng's unit (2500 of the camera's), unless -gotodistance says another
	double const pi = 3.14159265358979323846;

	theCamera->x = (int)(centre[0] + n[0] * distance);
	theCamera->y = (int)(centre[1] + n[1] * distance);
	theCamera->z = (int)(centre[2] + n[2] * distance);

	// looking back along the normal: forward = (-sin turn cos look, -sin look, cos turn cos look) in 4096ths of a turn
	double const fx = -n[0], fy = -n[1], fz = -n[2];
	double const turn = atan2(-fx, fz) * 4096.0 / (2.0 * pi);
	double const look = asin(fy < -1.0 ? -1.0 : (fy > 1.0 ? 1.0 : -fy)) * 4096.0 / (2.0 * pi);

	theCamera->turn = (int)floor(turn + 0.5);
	theCamera->look = (int)floor(look + 0.5);
	FlyCamera_Hold(theCamera);

	if (textureSelectOnGoto && TextureSelectPolygon(one.id, one.isModule, one.moduleIndex, one.polygon, true))
	{
		ShowTheTexturePolygons();
	}

	textureWords = "Texture Map: polygon " + std::to_string(one.polygon) + " (" + std::to_string(textureFoundAt + 1) + " of " + std::to_string(textureFound.size()) + ")";
	statusWords = textureWords.c_str();
}

static void TextureFind(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Texture Map: no level is open";
		return;
	}

	bool const select = command >= 0xba;
	int const kind = (command - 0xb7) % 3;   // 0 untextured, 1 textured with the current texture, 2 invalid PSX UV
	std::vector<LevelViewPicture> pictures;

	LevelView_Pictures(&pictures, 4096);
	BuildThePictureIndex(open);
	textureFound.clear();
	textureFoundAt = 0;

	if (kind == 1 && textureCurrent < 0)
	{
		statusWords = "Texture Map: no current texture is chosen yet (Next Texture picks one)";
		return;
	}

	bool full = false;
	size_t chosen = 0;

	TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
	{
		model::ShapeModel shape;
		std::string why;

		if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			return;
		}

		for (size_t p = 0; p < shape.polygons.size(); p++)
		{
			bool take = false;

			if (kind == 0)
			{
				take = !model::EngineTypeIsTextured(shape.polygons[p].engineType);
			}
			else
			{
				model::PolygonUv uv;

				if (!model::EngineTypeIsTextured(shape.polygons[p].engineType) || host::UvEdit_ReadPolygon(open, open->Level(), id, (int32_t)p, &uv, &why) != host::UvEditResult::Done || !uv.textured)
				{
					continue;
				}

				auto const texture = pictureTextureOf.find(uv.colour & 0xfff);

				if (kind == 1)
				{
					take = texture != pictureTextureOf.end() && texture->second == textureCurrent;
				}
				else
				{
					int32_t width = 0;

					for (LevelViewPicture const &picture : pictures)
					{
						width = texture != pictureTextureOf.end() && picture.texture == texture->second ? picture.width : width;
					}

					for (int c = 0; width > 0 && c < 3 && c < uv.uv.cornerCount; c++)
					{
						take = take || uv.uv.corners[c].u < 0 || uv.uv.corners[c].u >= (float)width || uv.uv.corners[c].v < 0 || uv.uv.corners[c].v >= (float)width;
					}
				}
			}

			if (!take)
			{
				continue;
			}

			TextureFound one;

			memcpy(one.id, id, 8);
			one.isModule = isModule;
			one.moduleIndex = moduleIndex;
			one.polygon = (int32_t)p;
			textureFound.push_back(one);

			if (select && !TextureSelectPolygon(id, isModule, moduleIndex, (int32_t)p, true))
			{
				full = true;
			}
			else
			{
				chosen += select ? 1 : 0;
			}
		}
	});

	if (select)
	{
		ShowTheTexturePolygons();
	}

	if (textureFound.empty())
	{
		statusWords = kind == 0 ? "All polygons have been done." : (kind == 1 ? "Texture Map: no polygon uses the current texture" : "Texture Map: no polygon has a texture coordinate outside its picture");
		return;
	}

	textureFoundAt = textureFound.size() - 1;
	TextureGoto(1);
	textureWords += std::string(" - ") + std::to_string(textureFound.size()) + (kind == 0 ? " polygons haven't been done" : kind == 1 ? " polygons use the current texture" : " polygons have invalid PSX UV coordinates") + (select ? ", selected" : "") + (full ? " (the selection is full)" : "") + "; G goes to the next, Shift+G to the previous";
	statusWords = textureWords.c_str();
}

static void TextureMapProject(uint8_t const planeId[8], int32_t planePolygon, bool tile)
{
	TextureDropImported();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t width = 0, height = 0;

	if (!open || !open->Level() || texturePolygons.empty() || !LevelView_LowestPictureSize(&width, &height))
	{
		statusWords = "Texture Map: nothing is selected";
		return;
	}

	// the chosen polygon's plane
	model::ShapeModel planeShape;
	std::vector<int32_t> planePoints, planeNormals;

	if (host::FilePick_World(open, open->Level(), planeId, &planeShape, &planePoints, &planeNormals) != host::FilePickResult::Done || planePolygon < 0 || (size_t)planePolygon >= planeShape.polygons.size() ||
		planeShape.polygons[(size_t)planePolygon].cornerCount < 3)
	{
		statusWords = "Texture Map: that polygon has no plane the editor can read";
		return;
	}

	model::ShapePolygon const &chosen = planeShape.polygons[(size_t)planePolygon];
	model::Fix3 corners[3];

	for (int c = 0; c < 3; c++)
	{
		size_t const at = (size_t)chosen.corners[c];

		if (chosen.corners[c] < 0 || at * 3 + 2 >= planePoints.size())
		{
			statusWords = "Texture Map: that polygon has no plane the editor can read";
			return;
		}

		corners[c] = { InIntengUnits(planePoints[at * 3]), InIntengUnits(planePoints[at * 3 + 1]), InIntengUnits(planePoints[at * 3 + 2]) };
	}

	model::Fix3 const normal = { planeNormals[(size_t)planePolygon * 3], planeNormals[(size_t)planePolygon * 3 + 1], planeNormals[(size_t)planePolygon * 3 + 2] };
	model::PlaneBasis plane;

	if (model::ProjectionBasis(corners, model::TriangleIsOriented(corners[0], corners[1], corners[2], normal), &plane) != model::UvModelResult::Ok)
	{
		statusWords = "Texture Map: that polygon has no usable plane (an edge of no length)";
		return;
	}

	// the selected polygons: their corners' world points, shape by shape, and their texture coordinates to be written
	struct Piece
	{
		uint8_t id[8];
		model::PolygonUv uv;
		size_t first;      // where this polygon's corners start in the world points
	};

	std::vector<Piece> pieces;
	std::vector<model::Fix3> world;
	size_t refused = 0;

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		TexturePolygon const &one = texturePolygons[first];
		model::ShapeModel shape;
		std::vector<int32_t> points, normals;
		model::PolygonUv uv;
		std::string why;

		if (host::FilePick_World(open, open->Level(), one.id, &shape, &points, &normals) != host::FilePickResult::Done || one.polygon < 0 || (size_t)one.polygon >= shape.polygons.size() ||
			host::UvEdit_ReadPolygon(open, open->Level(), one.id, one.polygon, &uv, &why) != host::UvEditResult::Done || !uv.textured || uv.uv.cornerCount != uv.cornerCount)
		{
			refused++;
			continue;
		}

		Piece piece;
		bool good = true;

		memcpy(piece.id, one.id, 8);
		piece.uv = uv;
		piece.first = world.size();

		for (int c = 0; c < uv.cornerCount && good; c++)
		{
			size_t const at = (size_t)shape.polygons[(size_t)one.polygon].corners[c];

			if (shape.polygons[(size_t)one.polygon].corners[c] < 0 || at * 3 + 2 >= points.size())
			{
				good = false;
				break;
			}

			world.push_back({ InIntengUnits(points[at * 3]), InIntengUnits(points[at * 3 + 1]), InIntengUnits(points[at * 3 + 2]) });
		}

		if (!good)
		{
			world.resize(piece.first);
			refused++;
			continue;
		}

		pieces.push_back(piece);
	}

	if (pieces.empty())
	{
		statusWords = "Texture Map: no selected polygon has texture coordinates to make";
		return;
	}

	TextureNodesFor(width, height);

	model::WindowCorners window = {};

	for (int k = 0; k < 4; k++)
	{
		window.u[k] = (int32_t)textureNodes.uv.corners[k].u;
		window.v[k] = (int32_t)textureNodes.uv.corners[k].v;
	}

	model::TextureWindow const rect = TextureWindowOf(width, height, textureAddOne);
	model::TileSettings const tiles = { 1000, 1000, 0, 0 };
	std::vector<model::UvCorner> made;

	if (model::ProjectCorners(tile, plane, window, rect, tiles, world, &made) != model::UvModelResult::Ok || made.size() != world.size())
	{
		statusWords = "Texture Map: the projection gave a texture coordinate out of range";
		return;
	}

	size_t changed = 0;
	std::string firstWhy;

	for (size_t start = 0; start < pieces.size(); start++)
	{
		if (pieces[start].uv.polygonNumber < 0)
		{
			continue;   // done with an earlier shape
		}

		std::vector<model::PolygonUv> forShape;
		uint8_t lead[8];

		memcpy(lead, pieces[start].id, 8);

		for (size_t i = start; i < pieces.size(); i++)
		{
			if (pieces[i].uv.polygonNumber < 0 || memcmp(pieces[i].id, lead, 8) != 0)
			{
				continue;
			}

			model::PolygonUv uv = pieces[i].uv;

			for (int c = 0; c < uv.cornerCount; c++)
			{
				uv.uv.corners[c] = made[pieces[i].first + (size_t)c];
			}

			forShape.push_back(uv);
			pieces[i].uv.polygonNumber = -1;
		}

		std::string why;
		host::UvEditResult const written = host::UvEdit_WritePolygons(open, open->Level(), lead, forShape, &why);

		if (written == host::UvEditResult::Done)
		{
			changed += forShape.size();

			for (TexturePolygon const &one : texturePolygons)
			{
				if (memcmp(one.id, lead, 8) == 0)
				{
					ShowTheNewUv(open, one, forShape);
					break;
				}
			}
		}
		else
		{
			refused += forShape.size();
			firstWhy = firstWhy.empty() ? (why.empty() ? host::UvEditWords(written) : why) : firstWhy;
		}
	}

	// then the current texture goes on the selection, as inteng's apply does (FUN_00555dd0's end)
	if (textureCurrent >= 0)
	{
		TextureMapSetPicture(textureCurrent);
	}

	textureWords = std::string("Texture Map: ") + (tile ? "tiled" : "wrapped") + " " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s") + " onto the chosen polygon's plane";

	if (refused)
	{
		textureWords += ", " + std::to_string(refused) + " left as they were" + (firstWhy.empty() ? "" : " (" + firstWhy + ")");
	}

	statusWords = textureWords.c_str();
}

// DELETE POLYS / RESTORE POLYS / DELETE 0-NORMAL POLYS / SHOW AND HIDE REMOVED (Poly edit menu 0xc3 0xc4 0xc9, Options 0xc5 0xc6; FUN_0056fd40, FUN_005639d0, FUN_00563d90 with FUN_00435080). Delete only FLAGS the
// selected polygons (inteng's flag 0x4000 in the Texture Map's own record: it is not in the file); Restore takes the flag back; Delete 0-normal flags every polygon of every shape whose normal is all 0 or whose first
// three corners are not three different places; Show / Hide Removed is the view switch (flagged polygons drawn or not). The polygons are TAKEN OUT of their shapes when the Texture Map mode is left
// (FUN_0054bfd0 -> FUN_00563d90), here also when a copy is saved. NAMED: inteng then asks "would you like to have the affected shapes recentred": this answers no (src/host/poly_edit.h); a shape that has
// every polygon flagged is left as it is (inteng deletes the whole object then: Object Placement's Delete does that here); a shape with a SHPMORPH or an ANIMSEQU cannot be flagged at all (inteng's own refusal).
// (Added 2026-11-15 by Melanikus (Hicks).)
struct RemovedPoly
{
	uint8_t id[8];
	int32_t polygon;
	bool isModule;
	int32_t moduleIndex;
};

static std::vector<RemovedPoly> polyRemoved;
static bool polyHideRemoved = false;   // DAT_005fd6d8: inteng starts with them shown

static size_t PolyRemovedAt(uint8_t const *id, int32_t polygon)
{
	for (size_t i = 0; i < polyRemoved.size(); i++)
	{
		if (polyRemoved[i].polygon == polygon && memcmp(polyRemoved[i].id, id, 8) == 0)
		{
			return i;
		}
	}

	return polyRemoved.size();
}

// Draws or hides these polygons in the view, shape by shape (the game's item flag bit 1, what Hide uses too)
static void PolyShowHideInView(OpenLevelFile *open, std::vector<RemovedPoly> const &polygons, bool hidden)
{
	std::vector<bool> done(polygons.size(), false);

	for (size_t first = 0; first < polygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TextureItems items = TextureItemsOf(open, polygons[first].id, polygons[first].isModule, polygons[first].moduleIndex);

		for (size_t i = first; i < polygons.size(); i++)
		{
			if (done[i] || memcmp(polygons[i].id, polygons[first].id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			if (items.ok && polygons[i].polygon >= 0 && (size_t)polygons[i].polygon < items.toItem.size())
			{
				LevelView_SetItemFlagBits(items.shape, items.toItem[(size_t)polygons[i].polygon], 1, hidden);
			}
		}
	}
}

// Delete polys, after "Are you sure that you want to delete the selected polygons?" (FUN_0056fd40(0x4000, set))
static void TexturePolyDeleteNow()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		statusWords = "Texture Map: Delete polys: select the polygons first";
		return;
	}

	std::vector<RemovedPoly> fresh;
	size_t refused = 0;

	for (TexturePolygon const &one : texturePolygons)
	{
		if (!host::PolyEdit_CanFlagForDeletion(open, one.id))
		{
			refused++;
		}
		else if (PolyRemovedAt(one.id, one.polygon) == polyRemoved.size())
		{
			RemovedPoly flagged;

			memcpy(flagged.id, one.id, 8);
			flagged.polygon = one.polygon;
			flagged.isModule = one.isModule;
			flagged.moduleIndex = one.moduleIndex;
			polyRemoved.push_back(flagged);
			fresh.push_back(flagged);
		}
	}

	if (polyHideRemoved)
	{
		PolyShowHideInView(open, fresh, true);
	}

	TextureMapDeselectAll();   // inteng clears the list after the flags are set (FUN_0056e0a0)
	textureWords = "Texture Map: Delete polys: " + std::to_string(fresh.size()) + " polygons flagged for deletion (" + std::to_string(polyRemoved.size()) + " flagged in all; they are taken out when this mode is left or a copy is saved; Restore polys takes the flag back)";

	if (refused)
	{
		textureWords += "; " + std::to_string(refused) + " were left: Can't alter shapes with morphing data (or an animation)";
	}

	statusWords = textureWords.c_str();
}

// Restore polys (FUN_0056fd40(0x4000, clear)): the selected polygons are not flagged any more
static void TexturePolyRestore()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<RemovedPoly> back;

	for (TexturePolygon const &one : texturePolygons)
	{
		size_t const at = PolyRemovedAt(one.id, one.polygon);

		if (at < polyRemoved.size())
		{
			back.push_back(polyRemoved[at]);
			polyRemoved.erase(polyRemoved.begin() + (std::ptrdiff_t)at);
		}
	}

	if (open && open->Level() && polyHideRemoved)
	{
		PolyShowHideInView(open, back, false);
	}

	textureWords = "Texture Map: Restore polys: " + std::to_string(back.size()) + " polygons are not flagged for deletion any more";
	statusWords = textureWords.c_str();
}

// Delete 0-normal polys, after "Would you like polygons with 0 normals to be flagged for deletion?" (FUN_005639d0): every polygon whose normal is all 0, or whose first three corners are not three
// different places, in every shape of the level; a shape with morphing data stops it.
static void TexturePolyZeroNormalsNow()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Texture Map: Delete 0-normal polys: no level is open";
		return;
	}

	size_t flagged = 0;
	bool morphing = false;
	std::vector<RemovedPoly> fresh;

	TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
	{
		model::ShapeModel shape;
		std::string why;

		if (morphing || host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			return;
		}

		for (size_t p = 0; p < shape.polygons.size(); p++)
		{
			model::ShapePolygon const &polygon = shape.polygons[p];
			bool bad = false;

			if (polygon.normalIndex >= 0 && (size_t)polygon.normalIndex < shape.polygonNormals.size())
			{
				model::ShapeNormal const &n = shape.polygonNormals[(size_t)polygon.normalIndex];

				bad = n.x == 0.0f && n.y == 0.0f && n.z == 0.0f;
			}

			if (!bad && polygon.cornerCount >= 3)
			{
				model::ShapePoint const *three[3] = { nullptr, nullptr, nullptr };

				for (int c = 0; c < 3; c++)
				{
					if (polygon.corners[c] >= 0 && (size_t)polygon.corners[c] < shape.points.size())
					{
						three[c] = &shape.points[(size_t)polygon.corners[c]];
					}
				}

				for (int c = 0; c < 3 && !bad; c++)
				{
					model::ShapePoint const *const a = three[c];
					model::ShapePoint const *const b = three[(c + 1) % 3];

					bad = a && b && a->x == b->x && a->y == b->y && a->z == b->z;
				}
			}

			if (!bad || PolyRemovedAt(id, (int32_t)p) < polyRemoved.size())
			{
				continue;
			}

			if (!host::PolyEdit_CanFlagForDeletion(open, id))
			{
				morphing = true;   // "0 Normal in shape with morphing data": inteng stops there
				return;
			}

			RemovedPoly one;

			memcpy(one.id, id, 8);
			one.polygon = (int32_t)p;
			one.isModule = isModule;
			one.moduleIndex = moduleIndex;
			polyRemoved.push_back(one);
			fresh.push_back(one);
			flagged++;
		}
	});

	if (polyHideRemoved)
	{
		PolyShowHideInView(open, fresh, true);
	}

	textureWords = morphing ? "Texture Map: 0 Normal in shape with morphing data (" + std::to_string(flagged) + " polys flagged before it)" : "Texture Map: " + std::to_string(flagged) + " polys flagged for deletion";
	statusWords = textureWords.c_str();
}

// Show Removed polys (0xc5) / Hide Removed polys (0xc6): DAT_005fd6d8 and a redraw
static void TexturePolyShowHide(bool hide)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	polyHideRemoved = hide;

	if (open && open->Level())
	{
		PolyShowHideInView(open, polyRemoved, hide);
	}

	textureWords = std::string("Texture Map: ") + (hide ? "Hide" : "Show") + " Removed polys: the " + std::to_string(polyRemoved.size()) + " flagged polygons are " + (hide ? "not drawn" : "drawn") + " (NAMED: only in the main view)";
	statusWords = textureWords.c_str();
}

// The polygons flagged are taken out of their shapes (FUN_00563d90): one shape at a time, the polygon numbers of that shape move, so its selection and hidden polygons are cleared
void TexturePolyApplyRemoved()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if ((polyRemoved.empty() && polySplit.empty()) || !open || !open->Level())
	{
		return;
	}

	std::vector<RemovedPoly> pending = polyRemoved;
	size_t taken = 0, kept = 0;
	std::string left;

	polyRemoved.clear();

	for (size_t first = 0; first < pending.size(); first++)
	{
		bool seen = false;

		for (size_t i = 0; i < first && !seen; i++)
		{
			seen = memcmp(pending[i].id, pending[first].id, 8) == 0;
		}

		if (seen)
		{
			continue;
		}

		std::vector<int32_t> numbers;

		for (RemovedPoly const &one : pending)
		{
			if (memcmp(one.id, pending[first].id, 8) == 0)
			{
				numbers.push_back(one.polygon);
			}
		}

		host::PolyRemoveMap map;
		std::string why;
		host::PolyRemoveResult const result = host::PolyEdit_Remove(open, pending[first].id, numbers.data(), numbers.size(), &map, &why);

		if (result == host::PolyRemoveResult::Done)
		{
			taken += numbers.size();

			// the polygons waiting to be split off from this shape have new numbers (a polygon is never both flagged for deletion and marked to split)
			for (SplitWaiting &waiting : polySplit)
			{
				if (memcmp(waiting.id, pending[first].id, 8) == 0)
				{
					for (int32_t &polygon : waiting.polygons)
					{
						polygon = polygon >= 0 && (size_t)polygon < map.polygon.size() ? map.polygon[(size_t)polygon] : -1;
					}
				}
			}

			for (size_t i = texturePolygons.size(); i > 0; i--)
			{
				if (memcmp(texturePolygons[i - 1].id, pending[first].id, 8) == 0)
				{
					texturePolygons.erase(texturePolygons.begin() + (std::ptrdiff_t)(i - 1));
				}
			}

			for (size_t i = textureHidden.size(); i > 0; i--)
			{
				if (memcmp(textureHidden[i - 1].id, pending[first].id, 8) == 0)
				{
					textureHidden.erase(textureHidden.begin() + (std::ptrdiff_t)(i - 1));
				}
			}
		}
		else
		{
			kept += numbers.size();
			left = std::string(host::PolyRemoveWords(result)) + (why.empty() ? "" : " (" + why + ")");

			for (RemovedPoly const &one : pending)
			{
				if (memcmp(one.id, pending[first].id, 8) == 0)
				{
					polyRemoved.push_back(one);
				}
			}
		}
	}

	textureWords = (taken || kept) ? "Texture Map: " + std::to_string(taken) + " flagged polygons taken out of their shapes (not recentred)" : std::string("Texture Map");

	if (kept)
	{
		textureWords += "; " + std::to_string(kept) + " left flagged: " + left;
	}

	// Split Object As Placed Obj (FUN_005746b0's marks, carried out when the mode is left): each waiting shape's polygons become a new shape with a new placed object. As Module is not built.
	if (!polySplit.empty())
	{
		std::vector<SplitWaiting> const waiting = polySplit;
		size_t made = 0, notMade = 0;
		std::string splitWhy;

		polySplit.clear();

		for (SplitWaiting const &one : waiting)
		{
			std::vector<int32_t> numbers;

			for (int32_t const polygon : one.polygons)
			{
				if (polygon >= 0)
				{
					numbers.push_back(polygon);
				}
			}

			if (numbers.empty())
			{
				notMade += one.polygons.size();
				splitWhy = "its polygons were taken out";
				continue;
			}

			uint8_t newId[8];
			std::string newName, why;
			host::PolySplitResult const result = host::PolyEdit_SplitAsObject(open, one.id, numbers.data(), numbers.size(), newId, &newName, &why, !one.asPlaced, one.name.c_str());

			if (result == host::PolySplitResult::Done || result == host::PolySplitResult::OldNotCut || result == host::PolySplitResult::NotModule)
			{
				made += numbers.size();
				splitWhy = result != host::PolySplitResult::Done ? std::string(host::PolySplitWords(result)) : newName;

				for (size_t i = texturePolygons.size(); i > 0; i--)
				{
					if (memcmp(texturePolygons[i - 1].id, one.id, 8) == 0)
					{
						texturePolygons.erase(texturePolygons.begin() + (std::ptrdiff_t)(i - 1));
					}
				}

				for (size_t i = textureHidden.size(); i > 0; i--)
				{
					if (memcmp(textureHidden[i - 1].id, one.id, 8) == 0)
					{
						textureHidden.erase(textureHidden.begin() + (std::ptrdiff_t)(i - 1));
					}
				}
			}
			else
			{
				notMade += numbers.size();
				splitWhy = std::string(host::PolySplitWords(result)) + (why.empty() ? "" : " (" + why + ")");
			}
		}

		if (made)
		{
			LoadTheObjectListAgain();   // there is a new object
		}

		textureWords += std::string(": Split Object: ") + std::to_string(made) + " polygons made into a new object (" + splitWhy + ")";

		if (notMade)
		{
			textureWords += "; " + std::to_string(notMade) + " NOT split: " + splitWhy;
		}
	}

	statusWords = textureWords.c_str();
}

// SPLIT OBJECT (Poly edit > Split Object > As Module 0xc7 / As Placed Obj 0xc8; FUN_005746b0(0 or 1)). inteng asks "Are you sure that you want to make these polys into a new object?", then, for the selection (which must
// be in ONE shape): refuses "More than one shape selected", "Can't alter shapes with morphing data" (a SHPMORPH or an ANIMSEQU), "A similar shape has been already been split" (a split is waiting for that shape),
// "Some of the polys are already marked for destruction/new object" (a polygon with the flag 0x4000 or 0x80000000) and "Cannot remove all polygons from shape"; then it puts the flag 0x80000000 on the polygons
// (the ONLY place inteng sets it: Non-Planar Quads, 0x11c, reads it) and keeps them in a waiting list; the new object is made when the Texture Map mode is left. THIS EDITOR: the marking, the refusals and the
// waiting list are built; MAKING the new object (a new shape of those polygons, a new module or placed object, the polygons out of the old shape) is NOT built yet: the marks are dropped when the mode is left or a
// copy is saved, and the status line says so (NAMED GAP, unfinished-in-inteng.md). TexturePolyMarkedForSplit is for Dietrich's Non-Planar Quads view. (Added 2026-11-15 by Melanikus (Hicks).)

// Whether polygon number `polygon` of the object `id` carries inteng's flag 0x80000000 (marked to become a new object)
bool TexturePolyMarkedForSplit(uint8_t const *id, int32_t polygon)
{
	for (SplitWaiting const &one : polySplit)
	{
		if (memcmp(one.id, id, 8) == 0)
		{
			for (int32_t const marked : one.polygons)
			{
				if (marked == polygon)
				{
					return true;
				}
			}
		}
	}

	return false;
}

static void TexturePolySplitNow(bool asPlaced)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		statusWords = "Texture Map: Split Object: select the polygons first";
		return;
	}

	for (TexturePolygon const &one : texturePolygons)
	{
		if (memcmp(one.id, texturePolygons[0].id, 8) != 0)
		{
			statusWords = "More than one shape selected";
			return;
		}
	}

	uint8_t const *const id = texturePolygons[0].id;

	if (!host::PolyEdit_CanFlagForDeletion(open, id))
	{
		statusWords = "Can't alter shapes with morphing data";
		return;
	}

	for (SplitWaiting const &waiting : polySplit)
	{
		if (memcmp(waiting.id, id, 8) == 0)
		{
			statusWords = "A similar shape has been already been split";
			return;
		}
	}

	size_t already = 0;

	for (TexturePolygon const &one : texturePolygons)
	{
		already += PolyRemovedAt(one.id, one.polygon) < polyRemoved.size() ? 1 : 0;
	}

	if (already)
	{
		statusWords = "Some of the polys are already marked for destruction/new object";
		return;
	}

	model::ShapeModel shape;
	std::string why;

	if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
	{
		statusWords = "Texture Map: Split Object: the shape could not be read";
		return;
	}

	size_t flaggedInShape = 0;

	for (RemovedPoly const &one : polyRemoved)
	{
		flaggedInShape += memcmp(one.id, id, 8) == 0 ? 1 : 0;
	}

	if (texturePolygons.size() + flaggedInShape >= shape.polygons.size())
	{
		statusWords = "Cannot remove all polygons from shape";
		return;
	}

	SplitWaiting waiting;

	memcpy(waiting.id, id, 8);
	waiting.asPlaced = asPlaced;

	for (TexturePolygon const &one : texturePolygons)
	{
		waiting.polygons.push_back(one.polygon);
	}

	polySplit.push_back(waiting);
	TextureMapDeselectAll();   // the list is cleared first so the words below stay on the status line
	textureWords = "Texture Map: Split Object: " + std::to_string(waiting.polygons.size()) + " polygons are marked to become a new " + (asPlaced ? "placed object (made when this mode is left or a copy is saved)" : "module (made when this mode is left or a copy is saved)") + " (the flag 0x80000000)";
	statusWords = textureWords.c_str();

	// inteng asks for the new object's name ("Spliting Objects - Please give name for new object (Escape chooses default name)", FUN_00564300 at the mode's exit); here it is asked right after the marking, the
	// default (the first Split<n> no object has) already typed, Escape or an empty answer keeping the default. NAMED: asked at the marking, not at the exit.
	std::vector<std::string> alreadyProposed;

	for (SplitWaiting const &other : polySplit)
	{
		alreadyProposed.push_back(other.name);
	}

	std::string const proposal = host::PolyEdit_NextFreeObjectName(open, "Split", alreadyProposed);

	textureAsk = 24;
	memset(keyDown, 0, sizeof(keyDown));
	promptFor = kPromptTextureMap;
	promptText = proposal;
}

// MERGE SELECTED POLYGONS (Poly edit 0x119, FUN_00573620): the selected triangles that are not paired are paired into quads by inteng's own joinability test (host::PolyEdit_Joinable, constants read from the exe), the best
// pairs first. With Fake Merge Data on, the copy of the pairing is changed (TexturePartnerOf / TextureFakePairSet) and the file's merge data is not written; otherwise the shape's SHPMRGDT is (made when the shape has none).
// (Added 2026-11-15 by Melanikus (Hicks).)
static void TexturePolyMerge()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		statusWords = "Texture Map: Merge selected polygons: select the polygons first";
		return;
	}

	size_t joined = 0;
	std::string refused;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		uint8_t id[8];
		std::vector<int32_t> numbers;

		memcpy(id, texturePolygons[first].id, 8);

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			if (!done[i] && memcmp(texturePolygons[i].id, id, 8) == 0)
			{
				done[i] = true;
				numbers.push_back(texturePolygons[i].polygon);
			}
		}

		if (textureFakeMerge)
		{
			model::ShapeModel shape;
			std::string why;

			if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
			{
				refused = "a shape could not be read";
				continue;
			}

			std::vector<int32_t> pairing(shape.polygons.size(), -1);

			for (size_t p = 0; p < pairing.size(); p++)
			{
				pairing[p] = TexturePartnerOf(open, id, (int32_t)p);
			}

			std::vector<int32_t> const before = pairing;

			joined += host::PolyEdit_MergeInModel(shape, &pairing, numbers.data(), numbers.size(), true);

			for (size_t p = 0; p < pairing.size(); p++)
			{
				if (pairing[p] != before[p])
				{
					TextureFakePairSet(open, id, (int32_t)p, pairing[p]);
				}
			}

			continue;
		}

		size_t here = 0;
		std::string why;
		host::PolyMergeResult const result = host::PolyEdit_Merge(open, id, numbers.data(), numbers.size(), &here, &why);

		if (result == host::PolyMergeResult::Done)
		{
			joined += here;
		}
		else
		{
			refused = host::PolyMergeWords(result);
		}
	}

	textureWords = "Texture Map: Merge selected polygons: " + std::to_string(joined) + " pairs made" + (textureFakeMerge ? " (in the fake merge data only)" : "") + (refused.empty() ? "" : "; " + refused);
	statusWords = textureWords.c_str();
}

// UNMERGE SELECTED POLYGONS (Poly edit 0x11a, FUN_00573e20): each selected polygon that is paired with another is parted from it, both made unpaired. With Fake Merge Data on, the copy of the pairing that Options >
// Fake Merge Data keeps is changed (TexturePartnerOf / TextureFakePairSet) and the file's merge data is not written; otherwise the shape's SHPMRGDT is. NAMED: Merge selected polygons (0x119) is not built (inteng's joinability
// test FUN_0055a360 uses constants not read). (Added 2026-11-15 by Melanikus (Hicks).)
static void TexturePolyUnmerge()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		statusWords = "Texture Map: Unmerge selected polygons: select the polygons first";
		return;
	}

	size_t parted = 0;
	std::string refused;
	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		uint8_t id[8];
		std::vector<int32_t> numbers;

		memcpy(id, texturePolygons[first].id, 8);

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			if (!done[i] && memcmp(texturePolygons[i].id, id, 8) == 0)
			{
				done[i] = true;
				numbers.push_back(texturePolygons[i].polygon);
			}
		}

		if (textureFakeMerge)
		{
			for (int32_t const polygon : numbers)
			{
				int32_t const partner = TexturePartnerOf(open, id, polygon);

				if (partner >= 0)
				{
					TextureFakePairSet(open, id, polygon, -1);
					TextureFakePairSet(open, id, partner, -1);
					parted++;
				}
			}

			continue;
		}

		size_t here = 0;
		std::string why;
		host::PolyUnmergeResult const result = host::PolyEdit_Unmerge(open, id, numbers.data(), numbers.size(), &here, &why);

		if (result == host::PolyUnmergeResult::Done)
		{
			parted += here;
		}
		else if (result != host::PolyUnmergeResult::NoMergeData)
		{
			refused = host::PolyUnmergeWords(result);
		}
	}

	textureWords = "Texture Map: Unmerge selected polygons: " + std::to_string(parted) + " pairs parted" + (textureFakeMerge ? " (in the fake merge data only)" : "") + (refused.empty() ? "" : "; " + refused);
	statusWords = textureWords.c_str();
}

// VERTEX MODE, one click (FUN_00565310 with FUN_00564dc0; AvP-docs\engine\inteng\vertex-mode-facts.md): the corner nearest the click on the screen among the corners of the polygon under the click and of its quad
// partner (the file's merge data names it); Shift takes a held corner out. NAMED: projected with the drawn view (pick_ray.h), as the Rect modes do. (Added 2026-11-15 by Melanikus (Hicks).)
static void TexturePolyVertexClicked(OpenLevelFile *open, uint8_t const *target, int32_t polygon, bool shift)
{
	model::ShapeModel shape;
	std::vector<int32_t> points, normals;
	host::PickCamera camera;

	if (host::FilePick_World(open, open->Level(), target, &shape, &points, &normals) != host::FilePickResult::Done || polygon < 0 || (size_t)polygon >= shape.polygons.size())
	{
		statusWords = "Texture Map: Vertex: the shape of that click could not be read";
		return;
	}

	if (!PickGame_Camera(texturePickViewW, texturePickViewH, &camera))
	{
		statusWords = "Texture Map: Vertex: there is no frame to put the corners on";
		return;
	}

	std::vector<int32_t> candidates;
	std::vector<int32_t> merge;
	bool hasMerge = false;

	candidates.push_back(polygon);

	if (host::PolygonItems_ReadMergeData(open, open->Level(), target, &merge, &hasMerge) && hasMerge && merge.size() == shape.polygons.size() && merge[(size_t)polygon] >= 0 && (size_t)merge[(size_t)polygon] < shape.polygons.size())
	{
		candidates.push_back(merge[(size_t)polygon]);
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	std::vector<host::PolyCornerSpot> spots;

	for (int32_t const number : candidates)
	{
		model::ShapePolygon const &one = shape.polygons[(size_t)number];

		for (int c = 0; c < one.cornerCount; c++)
		{
			int32_t const point = one.corners[c];
			bool seen = false;

			for (host::PolyCornerSpot const &spot : spots)
			{
				seen = seen || spot.point == point;
			}

			if (seen || point < 0 || (size_t)point >= shape.points.size() || (size_t)point * 3 + 2 >= points.size())
			{
				continue;
			}

			int32_t const world[3] = { (int32_t)(points[(size_t)point * 3] * scale), (int32_t)(points[(size_t)point * 3 + 1] * scale), (int32_t)(points[(size_t)point * 3 + 2] * scale) };
			double px = 0, py = 0, ahead = 0;
			host::PolyCornerSpot spot;

			spot.point = point;
			spot.onScreen = host::PickRay_Project(camera, world, &px, &py, &ahead);
			spot.x = px;
			spot.y = py;
			spots.push_back(spot);
		}
	}

	int32_t nearest = -1;

	// the corners the click chose from and where the view puts them (pixels of the view, and as a fraction of it for a scenario's click-view): so a proof can click exactly at a corner
	printf("AVP_OpenInteng: vertex pick: click %d,%d in a view of %dx%d, polygon %d:", pickX, pickY, texturePickViewW, texturePickViewH, polygon);

	for (host::PolyCornerSpot const &spot : spots)
	{
		if (spot.onScreen && texturePickViewW > 0 && texturePickViewH > 0)
		{
			printf(" [point %d at %.0f,%.0f = %.3f %.3f]", spot.point, spot.x, spot.y, spot.x / texturePickViewW, spot.y / texturePickViewH);
		}
		else
		{
			printf(" [point %d off the screen]", spot.point);
		}
	}

	printf("\n");
	fflush(stdout);

	if (!host::PolyEdit_NearestCorner(spots.data(), spots.size(), (double)pickX, (double)pickY, &nearest))
	{
		statusWords = "Texture Map: Vertex: no corner of that polygon is on the screen";
		return;
	}

	host::PolyVertexClick const answer = host::PolyEdit_VertexClick(&polyVertices, target, nearest, shift, shape.points);

	// the world places of this shape's points, kept for drawing the held corners
	polyShapeWorld.assign(shape.points.size() * 3, 0);

	for (size_t p = 0; p < shape.points.size() && p * 3 + 2 < points.size(); p++)
	{
		for (size_t axis = 0; axis < 3; axis++)
		{
			polyShapeWorld[p * 3 + axis] = (int32_t)(points[p * 3 + axis] * scale);
		}
	}

	textureWords = std::string("Texture Map: Vertex: ") + host::PolyVertexClickWords(answer) + " (" + std::to_string(polyVertices.count) + " of 4 held; Poly edit > Create poly needs 3 or 4)";
	statusWords = textureWords.c_str();
}

// Poly edit > Create poly (0x10e; case 0x10e of FUN_0054d260 with FUN_00565460): only in Vertex mode with 3 or more corners held. Three corners make a triangle, four two paired triangles. The
// camera's direction turns a polygon that faces away from it (inteng's own rule). NAMED: see poly_edit.h. (Added 2026-11-15 by Melanikus (Hicks).)
static void TexturePolyCreate()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (textureSelectMode != 4 || polyVertices.count < 3)
	{
		statusWords = "Texture Map: Create poly needs Select > Vertex with 3 or 4 corners held (inteng does nothing otherwise)";
		return;
	}

	if (!open || !open->Level())
	{
		statusWords = "Texture Map: Create poly: no level is open";
		return;
	}

	int forward[3] = { 0, 0, 0 };
	int right[3] = { 0, 0, 0 };
	double view[3] = { 0, 0, 0 };

	if (LevelView_CameraAxes(forward, right))
	{
		for (int axis = 0; axis < 3; axis++)
		{
			view[axis] = (double)forward[axis] / 65536.0;
		}
	}

	size_t first = 0, added = 0;
	std::string why;
	host::PolyCreateResult const result = host::PolyEdit_Create(open, polyVertices.id, polyVertices.point, polyVertices.count, view, &first, &added, &why);

	if (result != host::PolyCreateResult::Done)
	{
		textureWords = std::string("Texture Map: Create poly: ") + host::PolyCreateWords(result) + (why.empty() ? "" : " (" + why + ")");
		statusWords = textureWords.c_str();
		return;
	}

	host::PolyEdit_VertexClear(&polyVertices);
	polyShapeWorld.clear();
	textureWords = "Texture Map: Create poly: " + std::to_string(added) + " polygon(s) made, numbers " + std::to_string(first) + (added > 1 ? " and " + std::to_string(first + 1) : std::string()) + " (plain white, facing the camera; Texture them next)";
	statusWords = textureWords.c_str();
}

// A click in the Texture Map mode: the object under it (a module click is mapped to the object of that name), then which of that object's FILE polygons the click's line meets
// What a click on polygon `polygon` of the object `target` does in the mode now (a plane for wrap or tile, the animation to select or copy, Copy Texturing, the selection): the click's own work after the polygon is known - and what the harness's pick-poly does, by number
void TexturePolygonClicked(OpenLevelFile *open, uint8_t const *target, bool isModule, int32_t moduleIndex, int32_t polygon, bool deselect, bool copyNow)
{
	if (textureCopyObject)
	{
		TextureCopyObjectClicked(target);   // Copy Texturing > Object takes the object of the polygon (the click path does this before it finds the polygon; pick-poly comes here)
		return;
	}

	if (textureMultiMode >= 1 && textureMultiMode <= 4)
	{
		TextureMultiClicked(target, polygon);
		return;
	}

	if (texturePlaneMode)
	{
		int const mode = texturePlaneMode;

		texturePlaneMode = 0;
		TextureMapProject(target, polygon, mode == 2);
		return;
	}

	if (textureAnimCopyPick)
	{
		textureAnimCopyPick = false;
		TextureAnimCopyClicked(target, polygon);
		return;
	}

	if (textureAnimPick)
	{
		textureAnimPick = false;
		TextureAnimSelectClicked(target, isModule, moduleIndex, polygon);
		return;
	}

	if (copyNow)
	{
		TextureCopyFrom(target, polygon);
		return;
	}

	// Vertex mode (Select 0xfd): the click takes a corner, not a polygon (Hicks)
	if (textureSelectMode == 4)
	{
		TexturePolyVertexClicked(open, target, polygon, deselect);
		return;
	}

	// inteng's selection mode (see TextureMapCommand): by Object, or with Ctrl or Alt, takes the whole object; by Element takes the polygons joined to the one clicked
	{
		bool const whole = textureSelectMode == 1 || KeyHeldNow(VK_CONTROL) || KeyHeldNow(VK_MENU);

		if (whole || textureSelectMode == 5)
		{
			bool full = false;
			size_t done = 0;

			if (whole)
			{
				done = TextureSelectObject(open, target, isModule, isModule ? moduleIndex : -1, !deselect, &full);
			}
			else
			{
				model::ShapeModel shape;
				std::string why;

				if (host::ShapeEdit_Read(open->Level(), open, target, &shape, nullptr, &why) == host::ShapeEditResult::Done)
				{
					for (int32_t const polygon : TextureElementOf(shape, polygon))
					{
						if (!TextureSelectPolygon(target, isModule, isModule ? moduleIndex : -1, polygon, !deselect))
						{
							full = true;
							break;
						}

						done++;
					}
				}
			}

			ShowTheTexturePolygons();
			textureWords = std::string("Texture Map: ") + (whole ? "the object" : "the element") + " " + (deselect ? "deselected" : "selected") + ": " + std::to_string(done) + " polygons, " + std::to_string(texturePolygons.size()) + " selected" + (full ? " (the selection is full)" : "");
			statusWords = textureWords.c_str();
			return;
		}
	}

	size_t at = texturePolygons.size();

	for (size_t i = 0; i < texturePolygons.size(); i++)
	{
		if (texturePolygons[i].polygon == polygon && memcmp(texturePolygons[i].id, target, 8) == 0)
		{
			at = i;
			break;
		}
	}

	bool const was = at < texturePolygons.size();

	if (deselect)
	{
		if (was)
		{
			texturePolygons.erase(texturePolygons.begin() + (std::ptrdiff_t)at);
		}
	}
	else if (!was)
	{
		if (texturePolygons.size() >= kTexturePolygonsMost)
		{
			statusWords = "Texture Map: the selection is full";
			return;
		}

		TexturePolygon added;

		memcpy(added.id, target, 8);
		added.polygon = polygon;
		added.isModule = isModule;
		added.moduleIndex = isModule ? moduleIndex : -1;
		texturePolygons.push_back(added);
	}

	ShowTheTexturePolygons();

	// What the polygon is, from the texture model: its picture and corners, for the status line
	model::PolygonUv uv;
	std::string why;
	host::UvEditResult const read = host::UvEdit_ReadPolygon(open, open->Level(), target, polygon, &uv, &why);

	textureWords = "Texture Map: polygon " + std::to_string(polygon) + (deselect ? (was ? " deselected" : " was not selected") : (was ? " was already selected" : " selected"));

	if (read == host::UvEditResult::Done)
	{
		textureWords += uv.textured ? " (picture " + (uv.pictureName.empty() ? std::string("number ") + std::to_string(uv.pictureNumber) : "\"" + uv.pictureName + "\"") + ", " +
			std::to_string(uv.cornerCount) + " corners)" : std::string(" (not textured)");
	}
	else
	{
		textureWords += std::string(" (no texture data: ") + (why.empty() ? host::UvEditWords(read) : why) + ")";
	}

	textureWords += "; " + std::to_string(texturePolygons.size()) + " selected";

	if (textureShapesNotDrawn)
	{
		textureWords += " (" + std::to_string(textureShapesNotDrawn) + " shape" + (textureShapesNotDrawn == 1 ? "" : "s") + " not drawn: the file's merge data and the game's shape do not agree)";
	}

	statusWords = textureWords.c_str();
}

void TextureMapClicked(host::PickReport const &report, bool deselect)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera || !report.hasHit)
	{
		statusWords = "Texture Map: the click found no polygon";
		return;
	}

	uint8_t target[8];
	if (report.found == host::PickFound::Object)
	{
		memcpy(target, report.objectId, 8);
	}
	else if (!ModuleObject(open, report.moduleIndex, target))
	{
		statusWords = "Texture Map: the level file has no single object for that module";
		return;
	}

	if (textureCopyObject)
	{
		TextureCopyObjectClicked(target);
		return;
	}

	bool const copyNow = textureCopyPending;

	textureCopyPending = false;

	if (textureInvertPending)
	{
		textureInvertPending = false;
		TextureInvert(open, target, report.found == host::PickFound::Module, report.found == host::PickFound::Module ? report.moduleIndex : -1, false);
		return;
	}

	// An IMPORTED shape (a SHPEXTFL: 16 of Invasion's 381) is hidden in inteng's Texture Map mode (FUN_0054bdb0 sets object flag 0x800000 on it unless the "Show Imported Shapes" switch is on) and a change to it
	// is never saved (FUN_00572440 rebuilds only shapes with no SHPEXTFL), so a click on one selects nothing (Vasquez, p11-shape-handlers.md section 68). NAMED: the switch itself is not built.
	if (!textureShowImported && host::PolygonItems_ShapeHasOwnPictureList(open->Level(), target))
	{
		statusWords = "Texture Map: that shape is an imported one, which this mode hides and never saves (inteng's Show Imported Shapes switch, Options)";
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	host::FilePickRay ray;
	double const camera[3] = { (double)theCamera->x, (double)theCamera->y, (double)theCamera->z };

	for (int axis = 0; axis < 3; axis++)
	{
		ray.from[axis] = camera[axis] / scale;
		ray.direction[axis] = ((double)report.hitPoint[axis] - camera[axis]) / scale;
	}

	host::FilePickHit hit;

	if (host::FilePick_Object(open, open->Level(), target, ray, &hit) != host::FilePickResult::Done || !hit.found)
	{
		statusWords = "Texture Map: no polygon of that object under the click";
		return;
	}

	TexturePolygonClicked(open, target, report.found == host::PickFound::Module, report.found == host::PickFound::Module ? report.moduleIndex : -1, hit.polygon, deselect, copyNow);
}

// THE POLYGON MAP CHECK (-texmapproof, a proof run only): every shape the game has in play, a module or an object the level file names, has its file polygons mapped to the game's items
// (polygon_items.h) and each item's corner numbers laid against the file's. A polygon alone: the item has the polygon's own corners in the same order. A pair the loader joined: the item's four
// corners are the two triangles' corners together. Every polygon is looked at both ways: from its file number to its item, and from each item back to the file polygons it came from.
void RunThePolygonMapCheck()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		printf("AVP_OpenInteng: polygon map check: no level is held\n");
		return;
	}

	size_t shapes = 0, refused = 0, filePolygons = 0, items = 0, single = 0, joined = 0, apart = 0, cornersWrong = 0, backWrong = 0, withMerge = 0, notInTheFile = 0, noObject = 0;
	std::string firstRefusal;

	auto checkOne = [&](LevelViewShapeOf const &shape, uint8_t const *id, std::string const &label)
	{
		model::ShapeModel model;
		std::string why;
		std::vector<uint8_t> corners;
		std::vector<int32_t> points, numbers, toItem, firstFile;
		bool hasMerge = false;

		if (host::ShapeEdit_Read(open->Level(), open, id, &model, nullptr, &why) != host::ShapeEditResult::Done)
		{
			notInTheFile++;
			return;
		}

		shapes++;

		if (!LevelView_ItemCorners(shape, &corners, &points) || !host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge))
		{
			refused++;
			firstRefusal = firstRefusal.empty() ? label + ": the game has no shape in play for it" : firstRefusal;
			return;
		}

		host::PolygonItemResult const result = host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), model.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile);

		if (result != host::PolygonItemResult::Ok)
		{
			size_t minusOnes = 0;

			for (int32_t const number : numbers)
			{
				minusOnes += number == -1 ? 1 : 0;
			}

			refused++;
			firstRefusal = firstRefusal.empty() ? label + ": " + host::PolygonItemWords(result) : firstRefusal;
			printf("AVP_OpenInteng: polygon map check: refused %s: %s (file polygons %zu, merge data %s with %zu numbers of which %zu are -1, the game's items %zu)\n", label.c_str(),
				host::PolygonItemWords(result), model.polygons.size(), hasMerge ? "yes" : "no", numbers.size(), minusOnes, corners.size());
			return;
		}

		withMerge += hasMerge ? 1 : 0;
		filePolygons += model.polygons.size();
		items += corners.size();

		// item -> the file polygons that are in it
		std::vector<std::vector<size_t>> members(corners.size());

		for (size_t p = 0; p < toItem.size(); p++)
		{
			members[(size_t)toItem[p]].push_back(p);
		}

		for (size_t j = 0; j < corners.size(); j++)
		{
			if (members[j].empty() || members[j].front() != (size_t)firstFile[j])
			{
				backWrong++;
				continue;
			}

			int32_t const *const itemPoints = &points[j * 5];

			if (members[j].size() == 1)
			{
				single++;
				model::ShapePolygon const &polygon = model.polygons[members[j][0]];
				bool same = (size_t)polygon.cornerCount == corners[j];

				for (int k = 0; same && k < 5; k++)
				{
					same = itemPoints[k] == polygon.corners[k];
				}

				if (!same)
				{
					cornersWrong++;
				}
			}
			else if (members[j].size() == 2)
			{
				joined++;

				std::vector<int32_t> together;

				for (size_t p : members[j])
				{
					for (int k = 0; k < model.polygons[p].cornerCount; k++)
					{
						if (std::find(together.begin(), together.end(), model.polygons[p].corners[k]) == together.end())
						{
							together.push_back(model.polygons[p].corners[k]);
						}
					}
				}

				std::vector<int32_t> mine;

				for (int k = 0; k < 5; k++)
				{
					if (itemPoints[k] != -1)
					{
						mine.push_back(itemPoints[k]);
					}
				}

				std::sort(together.begin(), together.end());
				std::sort(mine.begin(), mine.end());

				if (together != mine || corners[j] != 4)
				{
					cornersWrong++;
				}
			}
			else
			{
				backWrong++;
			}
		}

		// a pair that stayed apart is two items for two file polygons that name each other
		for (size_t p = 0; hasMerge && p < numbers.size(); p++)
		{
			if (numbers[p] > (int32_t)p && toItem[p] != toItem[(size_t)numbers[p]])
			{
				apart++;
			}
		}
	};

	// the modules, found in the file by their names
	for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
	{
		std::string name;
		uint8_t id[8];

		if (!LevelView_ModuleName(m, &name))
		{
			continue;
		}

		if (!ModuleObject(open, m, id))
		{
			printf("AVP_OpenInteng: polygon map check: module %s has no single object in the file\n", name.c_str());
			noObject++;
			continue;
		}

		LevelViewShapeOf shape = {};

		shape.isModule = true;
		shape.moduleIndex = m;
		memcpy(shape.objectId, id, 8);
		checkOne(shape, id, "module " + name);
	}

	// the objects in play
	std::vector<std::vector<uint8_t>> active;

	LevelView_ActiveObjectIds(&active);

	for (std::vector<uint8_t> const &id : active)
	{
		LevelViewShapeOf shape = {};

		shape.isModule = false;
		memcpy(shape.objectId, id.data(), 8);
		checkOne(shape, id.data(), "an object");
	}

	printf("AVP_OpenInteng: polygon map check: %zu shapes (%zu with merge data), %zu refused, %zu in the game but not in the file; %zu file polygons -> %zu items: %zu alone, %zu joined pairs, "
		"%zu pairs left apart; item corners that do not match the file's %zu, items that do not map back to their file polygons %zu; modules with no single object in the file %zu%s%s\n", shapes, withMerge, refused, notInTheFile, filePolygons, items,
		single, joined, apart, cornersWrong, backWrong, noObject, firstRefusal.empty() ? "" : "; first refusal: ", firstRefusal.c_str());
}

// For a -texmapproof run: X-Flip and Y-Flip on the selection, read back. After X-Flip every corner's u is (width - 1) - u of before (the window = the whole of the lowest-numbered picture, whole numbers),
// v is unchanged; after Y-Flip v is (height - 1) - v; and a second flip of the same kind brings every corner back to its whole-number start.
void RunTheFlipProof()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t w = 0, h = 0;

	if (!open || !open->Level() || texturePolygons.empty() || !LevelView_LowestPictureSize(&w, &h))
	{
		printf("AVP_OpenInteng: flip proof: nothing to flip\n");
		return;
	}

	auto read = [&](std::vector<model::PolygonUv> *out)
	{
		out->clear();

		for (TexturePolygon const &one : texturePolygons)
		{
			model::PolygonUv uv;
			std::string why;

			if (host::UvEdit_ReadPolygon(open, open->Level(), one.id, one.polygon, &uv, &why) == host::UvEditResult::Done && uv.textured)
			{
				out->push_back(uv);
			}
		}
	};

	std::vector<model::PolygonUv> start, afterX, afterXX, afterY, afterYY;

	read(&start);
	TextureMapEdit(6);
	std::string const saidX = statusWords ? statusWords : "";
	read(&afterX);
	TextureMapEdit(6);
	read(&afterXX);
	TextureMapEdit(7);
	std::string const saidY = statusWords ? statusWords : "";
	read(&afterY);
	TextureMapEdit(7);
	read(&afterYY);

	size_t corners = 0, xRight = 0, vKept = 0, xxBack = 0, yRight = 0, yyBack = 0;

	if (start.size() == afterX.size() && start.size() == afterXX.size() && start.size() == afterY.size() && start.size() == afterYY.size())
	{
		for (size_t p = 0; p < start.size(); p++)
		{
			for (int c = 0; c < start[p].uv.cornerCount; c++)
			{
				float const u0 = (float)(int32_t)start[p].uv.corners[c].u, v0 = (float)(int32_t)start[p].uv.corners[c].v;

				corners++;
				xRight += afterX[p].uv.corners[c].u == (float)(w - 1) - u0 ? 1 : 0;
				vKept += afterX[p].uv.corners[c].v == v0 ? 1 : 0;
				xxBack += afterXX[p].uv.corners[c].u == u0 && afterXX[p].uv.corners[c].v == v0 ? 1 : 0;
				yRight += afterY[p].uv.corners[c].v == (float)(h - 1) - afterXX[p].uv.corners[c].v && afterY[p].uv.corners[c].u == afterXX[p].uv.corners[c].u ? 1 : 0;
				yyBack += afterYY[p].uv.corners[c].u == u0 && afterYY[p].uv.corners[c].v == v0 ? 1 : 0;
			}
		}
	}

	// the FLAG commands: set bit 4 on the selection (0x56), clear it (0x57), then the exclusive trio (0x4d: clears 0x40000 and 0x400, sets 0x4000000), each read back; every other bit must stay
	{
		std::vector<model::PolygonUv> before, withFour, withoutFour, trio;

		read(&before);
		TextureMapFlags(0x56);
		read(&withFour);
		TextureMapFlags(0x57);
		read(&withoutFour);
		TextureMapFlags(0x4d);
		read(&trio);

		size_t same = before.size() == withFour.size() && before.size() == withoutFour.size() && before.size() == trio.size() ? before.size() : 0;
		size_t setRight = 0, clearRight = 0, trioRight = 0, othersKept = 0;

		for (size_t p = 0; p < same; p++)
		{
			setRight += (withFour[p].flags & 4) == 4 ? 1 : 0;
			clearRight += (withoutFour[p].flags & 4) == 0 ? 1 : 0;
			trioRight += (trio[p].flags & 0x40000) == 0 && (trio[p].flags & 0x400) == 0 && (trio[p].flags & 0x4000000) == 0x4000000 ? 1 : 0;
			othersKept += (trio[p].flags & ~(4 | 0x40000 | 0x400 | 0x4000000)) == (before[p].flags & ~(4 | 0x40000 | 0x400 | 0x4000000)) ? 1 : 0;
		}

		// then through the loader: the level as the editor holds it is built and read again, and the flags must still be there
		std::vector<char> image;
		bool const built = open->BuildLevel(image);
		bool const taken = built && open->ReplaceLevel(image) == LevelOpenResult::Opened;
		std::vector<model::PolygonUv> reloaded;

		read(&reloaded);

		size_t kept = 0;

		for (size_t p = 0; p < reloaded.size() && p < trio.size(); p++)
		{
			kept += reloaded[p].flags == trio[p].flags ? 1 : 0;
		}

		printf("AVP_OpenInteng: flag proof: %zu textured polygons: set 4 right in %zu, cleared right in %zu, the trio right in %zu, every other bit kept in %zu; the loader takes the level %s, and %zu of %zu polygons hold the same flags after it\n",
			same, setRight, clearRight, trioRight, othersKept, taken ? "yes" : "NO", kept, trio.size());
	}

	// the texture chooser's data: every picture the level's shapes use
	{
		std::vector<LevelViewPicture> pictures;

		LevelView_Pictures(&pictures, 4096);

		printf("AVP_OpenInteng: pictures proof: %zu pictures in use, lowest %d (%s, %d x %d)\n", pictures.size(), pictures.empty() ? -1 : pictures[0].texture, pictures.empty() ? "" : pictures[0].name.c_str(),
			pictures.empty() ? 0 : pictures[0].width, pictures.empty() ? 0 : pictures[0].height);
	}

	// SET TEXTURE: the selection gets another picture (Next Texture from the lowest), and only the polygons' colour words change: the level is built before and after, the two images must be the same size and
	// differ in nothing but those words, the picture index must be right on every textured polygon, the loader must take it, and the index must survive the reload
	{
		BuildThePictureIndex(open);

		std::vector<model::PolygonUv> beforePictures, afterPictures;
		std::vector<char> imageBefore, imageAfter;

		read(&beforePictures);
		bool const builtBefore = open->BuildLevel(imageBefore);

		textureCurrent = -1;
		TextureMapStepPicture(1);

		int32_t const chosen = textureCurrent;
		bool const builtAfter = open->BuildLevel(imageAfter);

		read(&afterPictures);

		// the textured polygons of the selection in the order read() gives them, with the index each must have: the number of the chosen picture in the list its shape is numbered by (own list or the level's);
		// a shape whose list is not known to hold the picture must be left as it was
		std::vector<int32_t> expected;

		for (TexturePolygon const &one : texturePolygons)
		{
			model::PolygonUv uv;
			std::string why;

			if (host::UvEdit_ReadPolygon(open, open->Level(), one.id, one.polygon, &uv, &why) == host::UvEditResult::Done && uv.textured)
			{
				expected.push_back(PictureIndexForShape(open, one.id, chosen));
			}
		}

		size_t right = 0, others = 0, unchanged = 0, ownListPolygons = 0;

		for (size_t p = 0; p < afterPictures.size() && p < beforePictures.size() && p < expected.size(); p++)
		{
			bool const known = expected[p] >= 0;

			right += (known ? (afterPictures[p].colour & 0xfff) == expected[p] : afterPictures[p].colour == beforePictures[p].colour) ? 1 : 0;
			others += (afterPictures[p].colour & ~0xfff) == (beforePictures[p].colour & ~0xfff) ? 1 : 0;
			unchanged += known ? 0 : 1;
		}

		for (TexturePolygon const &one : texturePolygons)
		{
			ownListPolygons += host::PolygonItems_ShapeHasOwnPictureList(open->Level(), one.id) ? 1 : 0;
		}

		size_t differing = 0;

		if (imageBefore.size() == imageAfter.size())
		{
			for (size_t b = 0; b < imageBefore.size(); b++)
			{
				differing += imageBefore[b] != imageAfter[b] ? 1 : 0;
			}
		}

		bool const taken = builtAfter && open->ReplaceLevel(imageAfter) == LevelOpenResult::Opened;
		std::vector<model::PolygonUv> reloadedPictures;

		read(&reloadedPictures);

		size_t kept = 0;

		for (size_t p = 0; p < reloadedPictures.size() && p < afterPictures.size(); p++)
		{
			kept += reloadedPictures[p].colour == afterPictures[p].colour ? 1 : 0;
		}

		printf("AVP_OpenInteng: set-texture proof: picture number %d; the level's list: %zu polygons seen with another index for the same picture, %zu with an index that meets another picture; %zu selected polygons sit in shapes with their own list, %zu left as they were because their list has no such picture; %zu textured polygons: index right in %zu, every other bit of the colour word kept in %zu; the level's two images %s, %zu bytes differ (at most %zu colour words of 4 bytes would); the loader takes it %s; %zu of %zu hold it after the reload\n",
			chosen, pictureIndexConflicts, pictureIndexAmbiguous, ownListPolygons, unchanged, afterPictures.size(), right, others, builtBefore && builtAfter && imageBefore.size() == imageAfter.size() ? "are the same size" : "DIFFER IN SIZE",
			differing, afterPictures.size() * 4, taken ? "yes" : "NO", kept, afterPictures.size());
	}

	// WRAP then TILE onto the plane of the first selected polygon: every wrapped corner lies inside the window (0..W-1, 0..H-1), the coordinates changed, the loader takes the level, they survive the reload; the
	// tile gives other coordinates than the wrap. (The chosen polygon's own corners are in the selection: wrapped onto its own plane they keep their place relative to each other.)
	{
		std::vector<model::PolygonUv> beforeWrap, afterWrap, afterTile, reloaded;
		std::vector<char> image;
		TexturePolygon const plane = texturePolygons[0];

		// how many texture units a unit of the world gives along the first edge of each polygon, median over the selection (units check against the stock level's own coordinates)
		auto const medianScale = [&]()
		{
			std::vector<double> ratios;

			for (TexturePolygon const &one : texturePolygons)
			{
				model::ShapeModel shape;
				std::vector<int32_t> points, normals;
				model::PolygonUv uv;
				std::string why;

				if (ratios.size() >= 400 || host::FilePick_World(open, open->Level(), one.id, &shape, &points, &normals) != host::FilePickResult::Done || one.polygon < 0 || (size_t)one.polygon >= shape.polygons.size() ||
					host::UvEdit_ReadPolygon(open, open->Level(), one.id, one.polygon, &uv, &why) != host::UvEditResult::Done || !uv.textured || uv.uv.cornerCount < 2)
				{
					continue;
				}

				model::ShapePolygon const &polygon = shape.polygons[(size_t)one.polygon];
				size_t const a = (size_t)polygon.corners[0], b = (size_t)polygon.corners[1];

				if (polygon.corners[0] < 0 || polygon.corners[1] < 0 || a * 3 + 2 >= points.size() || b * 3 + 2 >= points.size())
				{
					continue;
				}

				double const dx = (double)points[b * 3] - points[a * 3], dy = (double)points[b * 3 + 1] - points[a * 3 + 1], dz = (double)points[b * 3 + 2] - points[a * 3 + 2];
				double const world = sqrt(dx * dx + dy * dy + dz * dz);
				double const du = (double)uv.uv.corners[1].u - uv.uv.corners[0].u, dv = (double)uv.uv.corners[1].v - uv.uv.corners[0].v;

				if (world > 0.0)
				{
					ratios.push_back(sqrt(du * du + dv * dv) / world);
				}
			}

			std::sort(ratios.begin(), ratios.end());
			return ratios.empty() ? 0.0 : ratios[ratios.size() / 2];
		};

		double const scaleBefore = medianScale();

		read(&beforeWrap);
		TextureMapProject(plane.id, plane.polygon, false);
		std::string const saidWrap = statusWords ? statusWords : "";
		read(&afterWrap);

		double const scaleAfterWrap = medianScale();

		auto const sameCorners = [](model::PolygonUv const &a, model::PolygonUv const &b)
		{
			if (a.uv.cornerCount != b.uv.cornerCount)
			{
				return false;
			}

			for (int c = 0; c < a.uv.cornerCount; c++)
			{
				if (a.uv.corners[c].u != b.uv.corners[c].u || a.uv.corners[c].v != b.uv.corners[c].v)
				{
					return false;
				}
			}

			return true;
		};

		size_t inWindow = 0, changedByWrap = 0;

		for (size_t p = 0; p < afterWrap.size() && p < beforeWrap.size(); p++)
		{
			bool inside = true;

			for (int c = 0; c < afterWrap[p].uv.cornerCount; c++)
			{
				inside = inside && afterWrap[p].uv.corners[c].u >= 0 && afterWrap[p].uv.corners[c].u <= (float)(w - 1) && afterWrap[p].uv.corners[c].v >= 0 && afterWrap[p].uv.corners[c].v <= (float)(h - 1);
			}

			inWindow += inside ? 1 : 0;
			changedByWrap += sameCorners(afterWrap[p], beforeWrap[p]) ? 0 : 1;
		}

		bool const built = open->BuildLevel(image);
		bool const taken = built && open->ReplaceLevel(image) == LevelOpenResult::Opened;

		read(&reloaded);

		size_t kept = 0;

		for (size_t p = 0; p < reloaded.size() && p < afterWrap.size(); p++)
		{
			kept += sameCorners(reloaded[p], afterWrap[p]) ? 1 : 0;
		}

		TextureMapProject(plane.id, plane.polygon, true);
		std::string const saidTile = statusWords ? statusWords : "";
		read(&afterTile);

		size_t tileDiffers = 0;

		for (size_t p = 0; p < afterTile.size() && p < afterWrap.size(); p++)
		{
			tileDiffers += sameCorners(afterTile[p], afterWrap[p]) ? 0 : 1;
		}

		printf("AVP_OpenInteng: wrap proof: texture units per world unit along each polygon's first edge (median of the selection): flipped stock %.5f, after the wrap %.5f, after the tile %.5f (the level's scale, game units per file unit: %.6f)\n", scaleBefore, scaleAfterWrap, medianScale(), local_scale);
		printf("AVP_OpenInteng: wrap proof: %zu textured polygons; \"%s\"; inside the window in %zu, coordinates changed in %zu; the loader takes it %s; %zu hold after the reload; tile: \"%s\", %zu differ from the wrap\n",
			afterWrap.size(), saidWrap.c_str(), inWindow, changedByWrap, taken ? "yes" : "NO", kept, saidTile.c_str(), tileDiffers);
	}

	// and Save a copy, the editor's own way (the game's loader checks the file before it is put in place)
	SaveTheCopy();
	printf("AVP_OpenInteng: save proof: %s\n", statusWords ? statusWords : "");

	printf("AVP_OpenInteng: flip proof: window %d x %d; %zu textured polygons, %zu corners: X-Flip u'=%d-1-u right in %zu, v kept in %zu; X twice back in %zu; Y-Flip right in %zu; Y twice back in %zu. said: \"%s\" / \"%s\"\n",
		w, h, start.size(), corners, w, xRight, vKept, xxBack, yRight, yyBack, saidX.c_str(), saidY.c_str());
}

// For the picture of a -texmapproof run: every second file polygon of the room the camera is in (a module) is selected, so the yellow highlight shows
void SelectSomePolygonsForTheProof()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera)
	{
		return;
	}

	int32_t const here[3] = { (int32_t)theCamera->x, (int32_t)theCamera->y, (int32_t)theCamera->z };
	int32_t const room = LevelView_ModuleAt(here);
	std::string name;
	uint8_t id[8];

	if (room < 0 || !LevelView_ModuleName(room, &name) || !ModuleObject(open, room, id))
	{
		printf("AVP_OpenInteng: texmap proof: no room under the camera (at %d %d %d: room %d)\n", here[0], here[1], here[2], room);
		return;
	}

	model::ShapeModel model;
	std::string why;

	if (host::ShapeEdit_Read(open->Level(), open, id, &model, nullptr, &why) != host::ShapeEditResult::Done)
	{
		return;
	}

	texturePolygons.clear();

	for (int32_t p = 0; p < 3000 && (size_t)p < model.polygons.size(); p += 2)
	{
		TexturePolygon polygon;

		memcpy(polygon.id, id, 8);
		polygon.polygon = p;
		polygon.isModule = true;
		polygon.moduleIndex = room;
		texturePolygons.push_back(polygon);
	}

	ShowTheTexturePolygons();
	printf("AVP_OpenInteng: texmap proof: room %d (%s), %zu polygons selected, %zu shapes not drawn\n", room, name.c_str(), texturePolygons.size(), textureShapesNotDrawn);
}

// Construction > Shape > Move centre to top / bottom (0x14a / 0x14b, inteng FUN_004452b0 + FUN_00473c20): for EVERY selected object that has a shape,
// the shape's origin moves to the middle of its extents on x and z and its top or bottom on y, and the place of every object that uses the shape moves
// the opposite way, so the world stays where it was (ShapeEdit_Recentre, shape_edit.h). A shape with a SHPMORPH is refused as inteng refuses it; the
// objects after a refused one are still done, as inteng's loop goes on. NAMED DEVIATIONS: inteng first asks to confirm (FUN_00474a40, its words not
// read) and says something when nothing is selected (FUN_00474030, words not read): ours does it at once and says so in the status line; the change is in
// the editor's bytes only, saved by Save a copy.
void MoveTheCentre(bool top)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t const count = host::Selection_SetCount();

	if (!open || !open->Level())
	{
		statusWords = "Move centre: no level is open";
		return;
	}

	if (count == 0)
	{
		statusWords = "Move centre: select an object first";
		return;
	}

	size_t done = 0;
	size_t usersMoved = 0;
	size_t refused = 0;
	std::string firstWhy;

	for (size_t i = 0; i < count; i++)
	{
		uint8_t id[8];

		if (!host::Selection_SetMember(i, id))
		{
			continue;
		}

		size_t users = 0;
		std::string why;
		host::ShapeEditResult const result = host::ShapeEdit_Recentre(open, open->Level(), id, top, &users, &why);

		if (result == host::ShapeEditResult::Done)
		{
			done++;
			usersMoved += users;
		}
		else
		{
			refused++;

			if (firstWhy.empty())
			{
				firstWhy = why.empty() ? host::ShapeEditWords(result) : why;
			}
		}
	}

	static std::string centreWords;   // kept here so statusWords can point at it

	centreWords = std::string("Move centre to ") + (top ? "top" : "bottom") + ": " + std::to_string(done) + " shape" + (done == 1 ? "" : "s") +
		" moved, " + std::to_string(usersMoved) + " object place" + (usersMoved == 1 ? "" : "s") + " kept in the world";

	if (refused != 0)
	{
		centreWords += "; " + std::to_string(refused) + " refused (" + firstWhy + ")";
	}

	statusWords = centreWords.c_str();

	if (done != 0)
	{
		LoadTheObjectListAgain();
	}
}

// Construction > Miscellaneous > Recalculate Normals and Extents (0x25b, inteng FUN_0044baa0): every shape of the level, through the edit store
void RecalculateTheNormals()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Recalculate Normals and Extents: no level is open";
		return;
	}

	host::ShapeRecalcReport report;

	if (!host::ShapeRecalc_All(open, open->Level(), &report))
	{
		statusWords = "Recalculate Normals and Extents: not done";
		return;
	}

	static std::string recalcWords;   // kept here so statusWords can point at it

	recalcWords = host::ShapeRecalc_Words(report);
	statusWords = recalcWords.c_str();
}
