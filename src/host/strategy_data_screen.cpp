// strategy_data_screen.cpp: STRATEGY DATA'S SELECTION and the screens' picks and tints: the icon and list clicks, select by texture, the hidden modules, the placement pick, poly counts, hide objects, the module links' edits, the overlaps, the tints, Write Asc File. Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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

// ---- STRATEGY DATA'S SELECTION (the screen's own Help text, FUN_00522950, 829 letters at 0x5ed824: "Left button to select objects", "Shift + left button to deselect objects", "L Deselects all objects"; the multi-pick collector FUN_00526bf0 0x526d30-0x526ef8: a click
// with Shift (DAT_00d87a5e) NOT down adds the object under the pointer to the list when it is not in it, with Shift down takes it out). The click picks the module or placed object under the pointer; the selection is the set of objects.
bool OnStrategyScreen()
{
	return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0;
}

// The icon (a generator, bad guy, sound, particle generator or virtual object: inteng's own display blocks) the line of the click meets first, nearer than what the level's pick met (distance < 0: the level's pick met nothing)
bool StrategyIconUnderClick(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight, int32_t levelDistance, uint8_t id[8])
{
	host::PickRay ray;

	if (!PickGame_RayAtClick(x, y, viewWidth, viewHeight, &ray))
	{
		return false;
	}

	std::vector<IconForPick> icons;
	std::vector<LevelViewTriangle> none;

	SpecialIconTriangles(&none, 0, true, &icons);

	double const length = sqrt((double)ray.direction[0] * ray.direction[0] + (double)ray.direction[1] * ray.direction[1] + (double)ray.direction[2] * ray.direction[2]);

	if (length <= 0.0)
	{
		return false;
	}

	double const d[3] = { ray.direction[0] / length, ray.direction[1] / length, ray.direction[2] / length };
	double best = levelDistance >= 0 ? (double)levelDistance : 1e18;
	bool found = false;

	for (IconForPick const &icon : icons)
	{
		for (host::IconWorldTriangle const &t : icon.triangles)
		{
			// Moller-Trumbore, both faces
			double const e1[3] = { (double)t.b[0] - t.a[0], (double)t.b[1] - t.a[1], (double)t.b[2] - t.a[2] };
			double const e2[3] = { (double)t.c[0] - t.a[0], (double)t.c[1] - t.a[1], (double)t.c[2] - t.a[2] };
			double const h[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
			double const det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];

			if (det > -1e-9 && det < 1e-9)
			{
				continue;
			}

			double const s[3] = { ray.from[0] - (double)t.a[0], ray.from[1] - (double)t.a[1], ray.from[2] - (double)t.a[2] };
			double const u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det;

			if (u < 0.0 || u > 1.0)
			{
				continue;
			}

			double const q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
			double const v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;

			if (v < 0.0 || u + v > 1.0)
			{
				continue;
			}

			double const along = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;

			if (along > 0.0 && along < best)
			{
				best = along;
				memcpy(id, icon.id, 8);
				found = true;
			}
		}
	}

	return found;
}

void StrategyListClick(host::PickReport const &report, bool shift, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8] = { 0 };
	bool have = false;

	if (open && open->Level())
	{
		if (report.found == host::PickFound::Module)
		{
			have = ModuleObject(open, report.moduleIndex, id);
		}
		else if (report.found == host::PickFound::Object)
		{
			memcpy(id, report.objectId, sizeof(id));
			have = true;
		}
	}

	// an icon nearer than what the level's pick met is what the click hit (the virtual object's V, a generator's G ...)
	bool iconHit = false;

	if (!strategyFlagPage && open && open->Level())
	{
		bool const levelHit = report.found == host::PickFound::Module || report.found == host::PickFound::Object;

		iconHit = StrategyIconUnderClick(x, y, viewWidth, viewHeight, levelHit ? report.distance : -1, id);
		have = have || iconHit;
	}

	// THE STRATEGY DATA SCREEN ITSELF (not its Module flags page): FUN_00524e90's last step (0x525242: the left button went down, no menu command) is FUN_00528250: ONE current object. The pick is FUN_00559720; a click on
	// the object that is current changes nothing; a click on an object that has no editor wrapper (DAT_00914954 holds none) changes nothing; a click on empty space (pick 0) lets the current object go; any other click takes the old
	// current object's red off and makes the new one current. The click never looks at Shift (the dispatcher does not read it): Shift + click is the same as a plain click. Only the collector (Module flags, Copy Strategy) adds and removes.
	if (!strategyFlagPage)
	{
		if (!iconHit && (report.found == host::PickFound::NoLine || report.found == host::PickFound::Nothing))
		{
			strategyPanelOn = false;
			host::Selection_Clear();
			LetGoOfTheFieldPanel();
			statusWords = "No object is current";
			return;
		}

		if (!have)
		{
			return;   // the game made it itself: no wrapper, no change (FUN_00528250 0x528273)
		}

		host::Selected current = {};

		current.kind = host::SelectedKind::Object;
		memcpy(current.objectId, id, sizeof(id));
		strategyPanelOn = false;
		host::Selection_Set(current);
		LetGoOfTheFieldPanel();
		statusWords = "1 object selected";
		return;
	}

	if (!have)
	{
		statusWords = "Nothing there to select";
		return;
	}

	host::Selected one = {};
	bool in = false;

	one.kind = host::SelectedKind::Object;
	memcpy(one.objectId, id, sizeof(id));

	for (size_t i = 0; i < host::Selection_SetCount(); i++)
	{
		uint8_t member[host::kSelectionIdBytes];

		in = in || (host::Selection_SetMember(i, member) && memcmp(member, id, sizeof(id)) == 0);
	}

	if (shift == in)
	{
		host::Selection_Toggle(one);
	}

	ShowTheSelected(host::Selection_Get());
	static std::string selectedWords;

	selectedWords = std::to_string((unsigned long long)host::Selection_SetCount()) + " object" + (host::Selection_SetCount() == 1 ? "" : "s") + " selected";
	statusWords = selectedWords.c_str();
}

void StrategySelectByTexture(host::PickReport const &report)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera || !report.hasHit)
	{
		statusWords = "S + click: the click found no polygon";
		return;
	}

	uint8_t target[8];

	if (report.found == host::PickFound::Object)
	{
		memcpy(target, report.objectId, 8);
	}
	else if (!ModuleObject(open, report.moduleIndex, target))
	{
		statusWords = "S + click: the level file has no single object for that module";
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
	model::ShapeModel shape;
	std::string why;

	if (host::FilePick_Object(open, open->Level(), target, ray, &hit) != host::FilePickResult::Done || !hit.found ||
		host::ShapeEdit_Read(open->Level(), open, target, &shape, nullptr, &why) != host::ShapeEditResult::Done || hit.polygon < 0 || (size_t)hit.polygon >= shape.polygons.size())
	{
		statusWords = "S + click: no polygon of that object under the click";
		return;
	}

	// GUARD (Bishop 11-15): a shape with its OWN list of pictures (an imported one, 16 of Invasion's 381) numbers its pictures by that list, not the level's: its texture number is not comparable here
	if (host::PolygonItems_ShapeHasOwnPictureList(open->Level(), target))
	{
		statusWords = "S + click: that object is an imported shape (its pictures are numbered by its own list)";
		return;
	}

	model::ShapePolygon const &clicked = shape.polygons[(size_t)hit.polygon];

	if (!model::EngineTypeIsTextured(clicked.engineType))
	{
		statusWords = "S + click: that polygon has no texture";
		return;
	}

	int32_t const texture = clicked.colour & 0x7fff;
	std::vector<host::LevelObject> rows;
	size_t added = 0;
	size_t using_ = 0;
	size_t bestCount = 0, bestOf = 0;   // the module that uses the texture most: how many of how many polygons (told when no module reaches half)
	bool full = false;

	LevelObjects_Read(&rows);

	for (host::LevelObject const &row : rows)
	{
		int32_t index = 0;
		model::ShapeModel other;
		std::string otherWhy;

		if (!host::ModuleIndexOfId(LevelObjects_Level(), row.id, &index) || host::HiddenObjects_Is(row.id) || host::PolygonItems_ShapeHasOwnPictureList(open->Level(), row.id) ||
			host::ShapeEdit_Read(open->Level(), open, row.id, &other, nullptr, &otherWhy) != host::ShapeEditResult::Done)
		{
			continue;
		}

		size_t count = 0;

		for (model::ShapePolygon const &polygon : other.polygons)
		{
			count += model::EngineTypeIsTextured(polygon.engineType) && (polygon.colour & 0x7fff) == texture ? 1 : 0;
		}

		if (count * bestOf > bestCount * other.polygons.size() || bestOf == 0)
		{
			bestCount = count;
			bestOf = other.polygons.size();
		}

		if (count <= other.polygons.size() / 2)
		{
			continue;
		}

		using_++;

		bool already = false;

		for (size_t i = 0; i < host::Selection_SetCount() && !already; i++)
		{
			uint8_t member[host::kSelectionIdBytes];

			already = host::Selection_SetMember(i, member) && memcmp(member, row.id, host::kSelectionIdBytes) == 0;
		}

		if (already)
		{
			continue;
		}

		host::Selected one = {};

		one.kind = host::SelectedKind::Object;
		memcpy(one.objectId, row.id, host::kSelectionIdBytes);

		if (host::Selection_Toggle(one))
		{
			added++;
		}
		else
		{
			full = true;
		}
	}

	strategyTextureCount = added;
	cameraWords = "S + click: texture " + std::to_string((long long)texture) + " is most of " + std::to_string((unsigned long long)using_) + " module" + (using_ == 1 ? "" : "s") + "; " + std::to_string((unsigned long long)added) + " added to the selection" + (using_ == 0 ? " (the module that uses it most has " + std::to_string((unsigned long long)bestCount) + " of its " + std::to_string((unsigned long long)bestOf) + " polygons)" : "") + (full ? " (the selection holds at most 256)" : "");
	statusWords = cameraWords.c_str();
}

// The hidden modules (hidden_objects.h) handed to the view (LevelView_HideModules, which takes the GAME's module numbers): worked out again only when the set or the level
// changes. Called each frame. The set is kept by 8-byte ID; an ID that is no module any more is left out.
bool hiddenModulesStale = false;   // set when a pass that hid every module ends (the hierarchy player's): the set is handed to the view again

void UpdateHiddenModules()
{
	static unsigned seenVersion = ~0u;
	static unsigned seenGeneration = ~0u;
	static std::map<int32_t, int32_t> numbers;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	unsigned const generation = open ? open->Generation() : 0;

	if (!hiddenModulesStale && seenVersion == host::HiddenObjects_Version() && seenGeneration == generation)
	{
		return;
	}

	hiddenModulesStale = false;

	seenVersion = host::HiddenObjects_Version();
	seenGeneration = generation;

	std::vector<int32_t> modules;

	if (open && host::HiddenObjects_Count() > 0)
	{
		host::GameModuleNumbers(LevelObjects_Level(), &numbers);

		for (size_t i = 0; i < host::HiddenObjects_Count() && modules.size() < kLevelViewMostHidden; i++)
		{
			uint8_t id[host::kHiddenIdBytes];
			int32_t index = 0;

			if (host::HiddenObjects_At(i, id) && host::ModuleIndexOfId(LevelObjects_Level(), id, &index))
			{
				auto const found = numbers.find(index);

				if (found != numbers.end())
				{
					modules.push_back(found->second);
				}
			}
		}
	}

	LevelView_HideModules(modules.data(), modules.size(), false);
}

// Module Linking's Object > Hide (0x96), Un-hide last (0x9a) and Un-hide all (0x98), FUN_004c1a90 0x4c2159-0x4c227f (menu-reads-1-full.txt:8362-8396), as
// hidden_objects.h says: Hide marks every picked module (the screen's picked list, here the selection set's modules, in pick order) and remembers the last of them;
// Un-hide last shows that last one again; Un-hide all shows every object. A view flag: the level is not changed, and the picks are kept. No current module is needed.
// Is the screen in use Module Linking? (the flag of Select With Centres is read only by its click, FUN_004c3760)
bool OnAdjacentModulesScreen()
{
	return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5c3508") == 0;
}

// #78: the context of the frame just saved (shot_context.h), gathered right after it was drawn. Every text goes through the log's own way of leaving the player out (log_files.h) here, and
// through Shot_ScrubPaths in Shot_Json.
extern std::string logFilename;   // logString.cpp: avp_log.txt, in the save folder

static std::string ShotFileSha256(std::string const &path, uint64_t mostBytes, uint64_t *bytes)
{
	*bytes = 0;

	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	LARGE_INTEGER size = {};

	if (file == INVALID_HANDLE_VALUE)
	{
		return std::string();
	}

	std::vector<uint8_t> data;
	bool read = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && (uint64_t)size.QuadPart <= mostBytes;

	if (read)
	{
		*bytes = (uint64_t)size.QuadPart;

		try
		{
			data.resize((size_t)size.QuadPart);
		}
		catch (std::exception const &)
		{
			read = false;
		}

		for (size_t at = 0; read && at < data.size();)
		{
			DWORD got = 0;
			DWORD const ask = (DWORD)((data.size() - at) < (1u << 24) ? (data.size() - at) : (1u << 24));

			read = ReadFile(file, data.data() + at, ask, &got, NULL) && got > 0;
			at += got;
		}
	}
	else if (size.QuadPart > 0)
	{
		*bytes = (uint64_t)size.QuadPart;   // too big to hash here: its size only
	}

	CloseHandle(file);
	return read ? host::IntengExe_Sha256(data.data(), data.size()) : std::string();
}

// the last lines of the editor's own log (the player already left out of them by the log's writer)
static std::vector<std::string> ShotLogLines()
{
	std::string const path = std::string(GetSaveFolderPath()) + logFilename;
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	LARGE_INTEGER size = {};
	std::string tail;
	bool cut = false;

	if (file == INVALID_HANDLE_VALUE)
	{
		return std::vector<std::string>();
	}

	if (GetFileSizeEx(file, &size) && size.QuadPart > 0)
	{
		LONGLONG const most = 32 * 1024;
		LARGE_INTEGER from = {};

		from.QuadPart = size.QuadPart > most ? size.QuadPart - most : 0;
		cut = from.QuadPart > 0;
		tail.resize((size_t)(size.QuadPart - from.QuadPart));

		DWORD got = 0;

		if (!SetFilePointerEx(file, from, NULL, FILE_BEGIN) || !ReadFile(file, &tail[0], (DWORD)tail.size(), &got, NULL))
		{
			got = 0;
		}

		tail.resize(got);
	}

	CloseHandle(file);
	return host::Shot_LastLines(tail, cut, host::kShotMostLogLines);
}

static std::string ShotObjectName(uint8_t const *id)
{
	host::FoundObject found;

	return host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found) ? found.name : "object " + HexOf(id);
}

static std::string ShotModuleName(int32_t moduleIndex)
{
	std::string name;

	return LevelView_ModuleName(moduleIndex, &name) ? name : ModuleNameOfIndex(moduleIndex);
}

static host::ShotContext ShotGather(FlyCamera const &camera, bool levelDrawn, int windowWidth, int windowHeight)
{
	static std::string build;
	static LogFiles_Private privateThings;
	static bool privateGathered = false;
	host::ShotContext c;

	if (!privateGathered)
	{
		LogFiles_PrivateGather(privateThings, GetSaveFolderPath());
		privateGathered = true;
	}

	if (build.empty())
	{
		char self[MAX_PATH] = "";
		uint64_t selfBytes = 0;
		DWORD const length = GetModuleFileNameA(NULL, self, sizeof(self));

		build = length > 0 && length < sizeof(self) ? ShotFileSha256(self, 256u << 20, &selfBytes).substr(0, 16) : std::string();
	}

	c.build = build;

	char opened[MAX_PATH] = "";
	std::string const levelPath = LevelView_FilePath(opened, sizeof(opened)) ? std::string(opened) : std::string();

	c.map = host::Shot_NameOnly(levelPath.empty() ? openLevelName : levelPath);

	// a file opened from elsewhere is worked on as its copy in avp_rifs\OpenedByEditor (open_with.h, the name lower-cased there): the map is the file the user opened, by its own name
	if (!levelPath.empty() && levelPath.find("OpenedByEditor") != std::string::npos && !levelSourcePath.empty())
	{
		char narrow[MAX_PATH * 2] = "";

		if (WideCharToMultiByte(CP_ACP, 0, levelSourcePath.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, nullptr) > 0)
		{
			std::string const source = host::Shot_NameOnly(narrow);
			std::string const stemSource = source.substr(0, source.find_last_of('.')), stemOpened = c.map.substr(0, c.map.find_last_of('.'));

			c.map = _stricmp(stemOpened.c_str(), stemSource.c_str()) == 0 ? source : c.map;
		}
	}

	c.levelSha256 = levelPath.empty() ? std::string() : ShotFileSha256(levelPath, 512u << 20, &c.levelBytes);
	c.screen = barScreen < host::kIntengScreenCount ? host::kIntengScreens[barScreen].name : "";
	c.pos[0] = camera.x;
	c.pos[1] = camera.y;
	c.pos[2] = camera.z;
	c.turn = camera.turn;
	c.look = camera.look;
	c.viewDistance = OnModuleLinkingScreen() ? (int32_t)(viewDistance / 500000) : -1;   // Module Linking's "View Distance :%d"

	int32_t const place[3] = { camera.x, camera.y, camera.z };
	int32_t const here = levelDrawn ? LevelView_ModuleAt(place) : -1;

	c.current = here >= 0 ? ShotModuleName(here) : std::string();

	// what is selected: the current selection, then the set's other objects
	host::Selected const selected = host::Selection_Get();

	if (selected.kind == host::SelectedKind::Module)
	{
		c.selected.push_back(ShotModuleName(selected.moduleIndex));
	}
	else if (selected.kind == host::SelectedKind::Object)
	{
		c.selected.push_back(ShotObjectName(selected.objectId));
	}
	else if (selected.kind == host::SelectedKind::Light)
	{
		c.light = "light " + std::to_string((long long)selected.lightIndex);
	}

	for (size_t i = 0; i < host::Selection_SetCount() && c.selected.size() < host::kShotMostNames; i++)
	{
		uint8_t id[host::kSelectionIdBytes];

		if (host::Selection_SetMember(i, id) && !(selected.kind == host::SelectedKind::Object && memcmp(id, selected.objectId, sizeof(id)) == 0))
		{
			c.selected.push_back(ShotObjectName(id));
		}
	}

	// the modules this frame drew (none when the frame was a strategy view's black page)
	if (levelDrawn)
	{
		std::vector<int32_t> drawnModules;

		LevelView_DrawnModules(&drawnModules);

		for (int32_t const m : drawnModules)
		{
			c.visible.push_back(ShotModuleName(m));
		}
	}

	if (OnModuleLinkingScreen() && linkTree)
	{
		c.base = ModuleNameOfIndex(linkTree->RootModule());

		for (int32_t const m : linkPicks)
		{
			c.picks.push_back(ModuleNameOfIndex(m));
		}

		for (int32_t const m : linkTree->OpenBranches())
		{
			c.branches.push_back(ModuleNameOfIndex(m));
		}
	}

	if (strategyEditFan)
	{
		c.strategy = ShotObjectName(strategyEditId);
		c.strategyKind = strategyEditKind;
	}
	else if (OnStrategyScreen() && selected.kind == host::SelectedKind::Object)
	{
		host::ObjectStrategy found;

		c.strategy = ShotObjectName(selected.objectId);

		if (host::FindObjectStrategy(LevelObjects_Level(), selected.objectId, &found) == host::StrategyFound::Found && found.payload.size() >= 12)
		{
			memcpy(&c.strategyKind, found.payload.data() + 8, 4);
		}
	}

	if (onHierarchy)
	{
		c.hierarchy = hierarchyMode.modelName.empty() ? hierarchyMode.hierarchy : hierarchyMode.modelName + ": " + hierarchyMode.hierarchy;
		c.sequence = hierarchyMode.sequenceSelected ? std::to_string((unsigned long long)hierarchyMode.sequence) : std::string();
		c.keyFrame = HierarchyView_IsShown() ? HierarchyView_KeyFrame() : -1;
	}

	c.status = statusWords ? statusWords : "";
	c.log = ShotLogLines();
	c.windowWidth = windowWidth;
	c.windowHeight = windowHeight;
	c.display = "windowed; " + R_GetVideoModeDescription();
	c.gpu = R_GetDeviceName() ? R_GetDeviceName() : "";

	// the player left out of every text, the way the log does it
	auto leaveOut = [&](std::string *text) { *text = LogFiles_LeaveOutThePlayer(*text, privateThings); };

	for (std::string *text : { &c.map, &c.screen, &c.current, &c.base, &c.light, &c.strategy, &c.hierarchy, &c.sequence, &c.status, &c.display, &c.gpu })
	{
		leaveOut(text);
	}

	for (std::vector<std::string> *list : { &c.selected, &c.visible, &c.picks, &c.branches, &c.log })
	{
		for (std::string &text : *list)
		{
			leaveOut(&text);
		}
	}

	return c;
}

// After the frame was saved at shotPending: the context put into it, and the status line says where it went
void ShotFinish(bool drawn, FlyCamera const &camera, bool levelDrawn, int windowWidth, int windowHeight)
{
	std::string why;
	bool contextIn = false;

	if (drawn)
	{
		try
		{
			contextIn = host::PngMeta_AddToFile(shotPending.c_str(), host::Shot_Json(ShotGather(camera, levelDrawn, windowWidth, windowHeight)), &why) == host::PngMetaResult::Done;
		}
		catch (std::exception const &)
		{
			why = "out of memory";
		}
	}

	std::string const shown = ShotShownPlace(host::Shot_NameOnly(shotPending));

	shotWords = !drawn ? "Screenshot NOT saved: " + shown : contextIn ? "Screenshot saved: " + shown : "Screenshot saved WITHOUT its context (" + why + "): " + shown;

	if (drawn && shotFromShiftF12 && !shotPrintScreenSeen && !shotHintGiven)
	{
		shotWords += " (Print Screen did not reach the editor: Windows may keep it for its own screen capture, so Shift+F12 is the key here)";
		shotHintGiven = true;
	}

	statusWords = shotWords.c_str();
	shotPending.clear();
}

bool OnModuleLinkingScreen()
{
	if (barScreen >= host::kIntengScreenCount)
	{
		return false;
	}

	char const *const table = host::kIntengScreens[barScreen].table;

	return strcmp(table, "0x5e2e38") == 0 || strcmp(table, "0x5e2c88") == 0;   // the two tables of Module Linking (inteng-menus.md section 7)
}

// The click with Select With Centres on (FUN_004c3760 0x4c379c-0x4c37eb, pick_centre.h): among the modules not hidden, the one whose centre is nearest the pointer on the screen;
// the report is that module as an Object pick (the same report the normal click makes for an object), or Nothing when none passes
// placedOnly: Object Placement's click (FUN_004d6520: the candidates are the placement list's objects, the PLACED ones; the one whose projected centre is nearest the pointer, FUN_0046ddb0, wherever it is)
host::PickReport PickTheNearestCentre(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight, bool placedOnly)
{
	host::PickReport report;

	memset(&report, 0, sizeof(report));
	report.found = host::PickFound::Nothing;
	report.polygonNumber = -1;

	host::PickCamera camera;
	std::vector<host::PolyCountRow> rows;

	if (!PickGame_Camera(viewWidth, viewHeight, &camera) || !LevelObjects_Level() || host::PolyCounts_Read(LevelObjects_Level(), &rows) == 0)
	{
		report.found = host::PickFound::NoLine;
		return report;
	}

	std::vector<host::PickCentre> centres;
	std::vector<size_t> from;

	for (size_t i = 0; i < rows.size(); i++)
	{
		if ((placedOnly ? !rows[i].placed : rows[i].placed) || host::HiddenObjects_Is(rows[i].id))
		{
			continue;
		}

		host::PickCentre centre;

		for (int axis = 0; axis < 3; axis++)
		{
			double const world = rows[i].place[axis] * (local_scale > 0.0 ? local_scale : 1.0);

			centre.place[axis] = world > 2147483000.0 ? 2147483000 : (world < -2147483000.0 ? -2147483000 : (int32_t)world);
		}

		centres.push_back(centre);
		from.push_back(i);
	}

	int const taken = host::PickCentre_Nearest(camera, clickX, clickY, centres.data(), centres.size());

	if (taken >= 0)
	{
		report.found = host::PickFound::Object;
		memcpy(report.objectId, rows[from[(size_t)taken]].id, host::kSelectionIdBytes);
	}

	return report;
}

// OBJECT PLACEMENT'S CLICK (reassess/object-placement.md OP-120; FUN_004d9af0 0x4da83b-0x4da957 calls FUN_004d6520 on a left press in state 0 when not snapping): every object of the PLACEMENT LIST that has a
// block not hidden is a candidate: the placed shapes AND the special objects (generators, bad guys, sounds, player starts, particle generators); FUN_0046ddb0 gives each the pixel distance from the pointer
// to its CENTRE projected on the screen (-1.0 behind the camera); the smallest with 0.0 <= d wins wherever it is (the best starts at 100000000.0, no nearest-enough limit); FUN_004d64a0 makes it THE current
// object (one object: no Shift, no set). No candidate in front of the view: FUN_004d64a0(0), none current. The "select by polygons" option of the Construction screen is not read here.

PlacementPick PlacementClickPick(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight)
{
	PlacementPick out;

	memset(&out.report, 0, sizeof(out.report));
	out.report.found = host::PickFound::Nothing;
	out.report.polygonNumber = -1;

	host::PickCamera camera;

	if (!PickGame_Camera(viewWidth, viewHeight, &camera) || !LevelObjects_Level())
	{
		return out;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	auto const worldOf = [scale](int32_t file) { double const world = file * scale; return world > 2147483000.0 ? 2147483000 : (world < -2147483000.0 ? -2147483000 : (int32_t)world); };
	std::vector<host::PickCentre> centres;
	std::vector<host::PolyCountRow> rows;
	std::vector<host::SpecialObject> specials;

	host::PolyCounts_Read(LevelObjects_Level(), &rows);

	for (host::PolyCountRow const &row : rows)
	{
		if (row.placed && !host::HiddenObjects_Is(row.id))
		{
			host::PickCentre centre;

			for (int axis = 0; axis < 3; axis++)
			{
				centre.place[axis] = worldOf(row.place[axis]);
			}

			centres.push_back(centre);
		}
	}

	size_t const placedCount = centres.size();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open)
	{
		host::SpecialObjects_Read(open, &specials);
	}

	for (host::SpecialObject const &one : specials)
	{
		host::PickCentre centre;

		for (int axis = 0; axis < 3; axis++)
		{
			centre.place[axis] = worldOf(one.place[axis]);
		}

		centres.push_back(centre);
	}

	// the placed hierarchies come last in the list here (NAMED: inteng has them among the SPECLOBJ children in the file's order; it matters only when two centres are on the same pixel)
	std::vector<host::PlacedHierarchyInfo> hierarchies;
	size_t const beforeHierarchies = centres.size();

	if (open)
	{
		host::PlacedHierarchies_Read(open, &hierarchies);
	}

	for (host::PlacedHierarchyInfo const &one : hierarchies)
	{
		host::PickCentre centre;

		for (int axis = 0; axis < 3; axis++)
		{
			centre.place[axis] = worldOf(one.place[axis]);
		}

		centres.push_back(centre);
	}

	int const taken = host::PickCentre_Nearest(camera, clickX, clickY, centres.data(), centres.size(), true);

	if (taken < 0)
	{
		return out;
	}

	if ((size_t)taken >= beforeHierarchies)
	{
		host::PlacedHierarchyInfo const &hierarchy = hierarchies[(size_t)taken - beforeHierarchies];

		out.what = 3;
		out.number = hierarchy.number;
		out.name = hierarchy.name;
		return out;
	}

	if ((size_t)taken < placedCount)
	{
		size_t placedAt = 0;

		for (host::PolyCountRow const &row : rows)
		{
			if (row.placed && !host::HiddenObjects_Is(row.id) && placedAt++ == (size_t)taken)
			{
				out.what = 1;
				out.report.found = host::PickFound::Object;
				memcpy(out.report.objectId, row.id, host::kSelectionIdBytes);
				break;
			}
		}

		return out;
	}

	host::SpecialObject const &special = specials[(size_t)taken - placedCount];

	out.what = 2;
	out.kind = special.kind;
	out.number = special.number;
	out.name = special.name;
	return out;
}

static std::string placementClickWords;

// The click's result, as FUN_004d64a0 does it: the old current object let go of, the new one made current (red), none when nothing was taken
void PlacementClickUse(PlacementPick const &pick)
{
	placedHierarchyCurrent = -1;

	if (pick.what == 3)
	{
		host::Selection_Clear();
		specialMode.haveCurrent = false;
		placedHierarchyCurrent = (int)pick.number;
		placementClickWords = std::string("Selected: ") + (pick.name.empty() ? std::string("(no name)") : pick.name);
		statusWords = placementClickWords.c_str();
		return;
	}

	if (pick.what == 2)
	{
		host::Selection_Clear();
		specialMode.haveCurrent = true;
		specialMode.kind = pick.kind;
		specialMode.number = pick.number;
		placementClickWords = std::string("Selected: ") + (pick.name.empty() ? std::string("(no name)") : pick.name);
		statusWords = placementClickWords.c_str();
		return;
	}

	specialMode.haveCurrent = false;

	if (pick.what == 1)
	{
		statusWords = host::Selection_UsePick(pick.report);
		return;
	}

	host::Selection_Clear();
	statusWords = "Nothing selected";
}

// Base > Show Poly Counts (0xcc, FUN_004c0950, poly_counts.h): inteng's FULL-SCREEN list. The screen is cleared (FUN_00418b80), then up to 50 modules a page are written in two columns of 25 rows: the entry i of the page
// at x = (i / 25) * 320 + 16, y = (i % 25) * 16 as "%d\t%d m\t%s" (polygons, modules seen, name: FUN_004d5e60), the screen's foot says "Left Button : Goto object", "Right Button : Exit", "Page Down : Show next 50 modules",
// "Page Up : Show Previous 50 modules" at height - 0x44, - 0x34, - 0x24, - 0x14. It then waits (FUN_0058b700) for a mouse button or Page Down / Page Up (wrapping): a LEFT press above y = 400 on an entry that exists goes to that
// module (the camera is put at its place, FUN_004c0c96 then FUN_004c0cb0) and the screen is left; the RIGHT button leaves it. NAMED: the width of the tab (4 spaces here), the marker (char 2) before the current module's entry.
std::vector<host::PolyCountRow> polyRows;
size_t polyPage = 0;
static std::string polyWords;

void PolyCountsCommand()
{
	if (!LevelObjects_Level() || host::PolyCounts_Read(LevelObjects_Level(), &polyRows) == 0)
	{
		Answer("Show Poly Counts: no level with shapes is held to count");
		return;
	}

	polyPage = 0;
	polyScreenOpen = true;
	memset(keyDown, 0, sizeof(keyDown));
}

void PolyScreenPage(int32_t direction)
{
	size_t const pages = (polyRows.size() + polyRowsPerPage - 1) / polyRowsPerPage;

	polyPage = direction > 0 ? (polyPage + 1) % (pages ? pages : 1) : (polyPage + (pages ? pages : 1) - 1) % (pages ? pages : 1);
}

// A press on the screen: the right button leaves it, the left one goes to the entry under it (y above 400)
void PolyScreenPress(int32_t x, int32_t y, bool right)
{
	if (right)
	{
		polyScreenOpen = false;
		return;
	}

	size_t const at = polyPage * polyRowsPerPage + (size_t)(y / 16) + (size_t)(x / 320) * 25;

	if (y < 0 || x < 0 || y >= 400 || at >= polyRows.size())
	{
		return;
	}

	host::PolyCountRow const &chosen = polyRows[at];
	host::Selected picked = {};

	picked.kind = host::SelectedKind::Object;
	memcpy(picked.objectId, chosen.id, host::kSelectionIdBytes);
	host::Selection_Set(picked);

	if (theCamera)
	{
		theCamera->x = (int)(chosen.place[0] * local_scale);
		theCamera->y = (int)(chosen.place[1] * local_scale);
		theCamera->z = (int)(chosen.place[2] * local_scale);
		FlyCamera_Hold(theCamera);
	}

	polyScreenOpen = false;
	polyWords = "Poly count of " + chosen.name + ": " + host::PolyCountText(chosen) + " (polygons, modules seen); the view is at its place";
	Answer(polyWords);
}

void HideObjectsCommand(int32_t what)
{
	if (what == kLinkUnhideAll)
	{
		size_t const was = host::HiddenObjects_Count();

		host::HiddenObjects_ShowAll(true);
		Answer(was ? "Un-hide all: " + std::to_string(was) + " hidden object(s) shown again" : "Un-hide all: nothing was hidden");
		return;
	}

	if (what == kLinkUnhideLast)
	{
		uint8_t id[host::kHiddenIdBytes];

		if (!host::HiddenObjects_Last(id))
		{
			Answer("Un-hide last: nothing has been hidden yet");
			return;
		}

		bool const was = host::HiddenObjects_Is(id);

		host::HiddenObjects_UnhideLast();
		Answer(was ? "Un-hide last: the last hidden object is shown again" : "Un-hide last: the last hidden object was already shown");
		return;
	}

	// Hide: the picked modules' IDs, in pick order
	std::vector<uint8_t> ids;
	size_t count = 0;

	for (size_t i = 0; i < host::Selection_SetCount(); i++)
	{
		uint8_t id[host::kSelectionIdBytes];
		int32_t index = 0;

		if (host::Selection_SetMember(i, id) && host::ModuleIndexOfId(LevelObjects_Level(), id, &index))
		{
			ids.insert(ids.end(), id, id + host::kSelectionIdBytes);
			count++;
		}
	}

	if (count == 0)
	{
		Answer("Hide: pick modules first (click, Shift+click for more)");
		return;
	}

	size_t const added = host::HiddenObjects_Hide(ids.data(), count);

	Answer("Hide: " + std::to_string(count) + " picked module(s), " + std::to_string(added) + " newly hidden");
}

void LinkEdit(int32_t what)
{
	std::vector<int32_t> const picked = PickedModules();

	if (!linkTree)
	{
		Answer("Links: make a module the current one first (Set Current)");
		return;
	}

	if (picked.empty())
	{
		Answer("Links: pick modules first (click, Shift+click for more)");
		return;
	}

	int32_t const root = linkTree->RootModule();
	size_t done = 0;

	switch (what)
	{
		case kLinkNormal:
			for (int32_t const module : picked)
			{
				done += linkTree->Normal(module) ? 1 : 0;
			}

			LinkPicksLetGo();
			Answer("Normal Link: " + std::to_string(done) + " of " + std::to_string(picked.size()) + " linked (a module already in the tree, or the current one, is not)");
			break;
		case kLinkBranch:
			// Only the first picked, and not the current module; then every pick is let go of
			if (picked[0] != root)
			{
				done = linkTree->Branch(picked[0]) ? 1 : 0;
				LinkPicksLetGo();
				Answer(done ? "Branch Link made" : "Branch Link: not made (the last link's branch is closed)");
			}

			break;
		case kLinkDelete:
			for (int32_t const module : picked)
			{
				if (module != root)
				{
					linkTree->Delete(module);
				}
			}

			LinkPicksLetGo();
			Answer("Delete Link done");
			break;
		case kLinkClose:
			for (int32_t const module : picked)
			{
				if (module != root)
				{
					linkTree->Close(module);
				}
			}

			LinkPicksLetGo();
			Answer("Close Branch done");
			break;
		case kLinkOpen:
			// FUN_004c1a90 0x4c2eca-0x4c2fbe (menu-reads-1-full.txt:8815-8850): each picked module is taken off the picks in turn and, when it is not the current
			// module, its branch is opened (FUN_004c43c0 -> FUN_004c43d0: module_links.h Open); the picks are all let go of
			for (int32_t const module : picked)
			{
				if (module != root)
				{
					linkTree->Open(module);
				}
			}

			LinkPicksLetGo();
			Answer("Open Branch done");
			break;
		case kLinkView:
		case kLinkHide:
			// FUN_004c1a90 0x4c2fcc / 0x4c3023 (menu-reads-1-full.txt:8851-8880): each picked module in turn, the picks kept (not let go of); the tree
			// is walked from the root and the module's branch shown or hidden (module_links.h)
			for (int32_t const module : picked)
			{
				if (what == kLinkView)
				{
					linkTree->View(module);
				}
				else
				{
					linkTree->Hide(module);
				}
			}

			Answer(what == kLinkView ? "View Branch done" : "Hide Branch done");
			break;
		default:
			break;
	}
}


// Module Linking's colours, inteng's (P5): FUN_004c17a0 (menu-reads-2.txt:5779-5891) draws every module white and then the tree by FUN_004c0720
// (menu-reads-4.txt:1891-1970; link_tree Tints): the current module blue, a link yellow, a link with an open branch green and with a closed branch red.
// The picked modules are drawn 0x7F007F there (the selection set's own colour is the view's). The view takes the game's module numbers, worked out
// from the level only when it changes. Called each frame. (Added 2026-11-15 by Melanikus (Drake).)
// MODULE OVERLAPS (Construction, Miscellaneous 0x56; module_overlaps.h): a switch. On, the boxes of the level's modules are compared (inteng: FUN_00444c80) and the overlapping modules are tinted red (a big overlap), purple,
// orange or yellow (they only touch); off, the normal view is back. The tints are worked out when it is switched on and when the level changes (a later edit of a shape or place moves a box). (Added 2026-11-15 by Melanikus (Hicks).)
bool moduleOverlapsOn = false;
static std::vector<int32_t> overlapObjects;     // the object index of each overlapping module
static std::vector<uint32_t> overlapColours;    // its colour, ARGB
static unsigned overlapsFor = ~0u;              // the level generation they were worked out for

static void WorkOutModuleOverlaps(OpenLevelFile *open)
{
	std::vector<host::ModuleBox> boxes;

	overlapObjects.clear();
	overlapColours.clear();
	overlapsFor = open->Generation();
	host::ModuleOverlaps_ReadBoxes(open, &boxes);

	std::vector<host::OverlapSeverity> const worst = host::ModuleOverlaps_Worst(boxes);
	size_t counts[5] = { 0, 0, 0, 0, 0 };

	for (size_t i = 0; i < boxes.size(); i++)
	{
		counts[(size_t)worst[i]]++;

		if (worst[i] != host::OverlapSeverity::None)
		{
			overlapObjects.push_back(boxes[i].objectIndex);
			overlapColours.push_back(0xFF000000u | host::ModuleOverlaps_Rgb(worst[i]));
		}
	}

	statusWords = "";
	textureWords = "Module Overlaps on: " + std::to_string(overlapObjects.size()) + " of " + std::to_string(boxes.size()) + " modules overlap another: " + std::to_string(counts[4]) + " red (over 1000), " + std::to_string(counts[3]) +
		" purple (101 to 1000), " + std::to_string(counts[2]) + " orange (3 to 100), " + std::to_string(counts[1]) + " yellow (touching); Module Overlaps again turns it off";
	statusWords = textureWords.c_str();
}

void ToggleModuleOverlaps()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	// both ways the camera is saved first (0x446a3a: DAT_0063126c 0 -> FUN_00445110, its first call FUN_004704d0(10) 0x445115; else FUN_00445270 -> the hub's leave FUN_004476d0 0x447804; CS-2, CS-3)
	CameraPositionSaved(10);
	moduleOverlapsOn = !moduleOverlapsOn;

	if (!moduleOverlapsOn)
	{
		overlapObjects.clear();
		overlapColours.clear();
		overlapsFor = ~0u;
		statusWords = "Module Overlaps off";
		return;
	}

	if (!open || !open->Level())
	{
		moduleOverlapsOn = false;
		statusWords = "Module Overlaps: no level is open";
		return;
	}

	WorkOutModuleOverlaps(open);
}

// Hierarchy > Dummy Objects (0x5c) and Dummy Object Size (0x5d): inteng makes a "dummy object" for EVERY section, at the section's place (FUN_00481d70 / FUN_004822c0, placed by FUN_0047ef70): a cube of side N, its eight corners at +/- N/2, and three
// axes of length N from the origin (the points (N,0,0), (0,N,0), (0,0,N)); N is the size in inteng's unit (1e-5 m: 40000 to start with, 400 mm), hidden by bit 4 of its flags when the switch is off. Here the cube and
// the axes are drawn as lines in the world (R_SetEditorWorldLines) while the switch is on. NAMED: inteng's polygons of the shape are not read, only its points; the lines are the cube's twelve edges. The model
// is drawn heading 0, so the cube is aligned with the world's axes.
void UpdateDummyObject()
{
	static bool drawn = false;
	bool const want = onHierarchy && (hierarchyMode.dummyObjects || hierarchyBones || (hierarchyMode.vertexMode && !hierarchySkin.empty())) && HierarchyView_IsShown();

	if (!want)
	{
		if (drawn)
		{
			LevelView_WorldLines(nullptr, 0);
			drawn = false;
		}

		return;
	}

	int32_t const side = hierarchyMode.dummyObjectSize / 100;   // mm
	int32_t const half = side / 2;
	int32_t const centre[3] = { hierarchyShownPlace[0], hierarchyShownPlace[1], hierarchyShownPlace[2] };
	std::vector<host::TrackLine> lines(4096);
	size_t count = 0;
	uint32_t const cubeColour = 0xFFFFFFFFu;
	uint32_t const axisColours[3] = { 0xFFFF4040u, 0xFF40FF40u, 0xFF4040FFu };

	// ONE DUMMY OBJECT PER SECTION (row C-5c): each section's update (FUN_0047ef70 0x47f2df-0x47f338) puts its own dummy block (+8) at the section's place (+0x54..0x5c) with the matrix DAT_005d4e28
	// (the identity, read from the exe) and the size DAT_005d4e24, shown while Dummy Objects is on (its ObFlags +0x5c bit 4 clear): a cube and three axes stand at EVERY section's place
	std::vector<HierarchyViewSection> sections;

	if (hierarchyMode.dummyObjects)
	{
		HierarchyView_Sections(&sections);
	}

	for (HierarchyViewSection const &section : sections)
	{
		int32_t const *const at = section.world;

		if (count + 15 > lines.size())
		{
			break;
		}

		for (int c = 0; c < 8; c++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if ((c & (1 << axis)) != 0)
				{
					continue;
				}

				int32_t from[3], to[3];

				for (int k = 0; k < 3; k++)
				{
					from[k] = at[k] + ((c & (1 << k)) ? half : -half);
					to[k] = from[k];
				}

				to[axis] = at[axis] + half;

				for (int k = 0; k < 3; k++)
				{
					lines[count].from[k] = from[k];
					lines[count].to[k] = to[k];
				}

				lines[count].colour = cubeColour;
				count++;
			}
		}

		for (int axis = 0; axis < 3; axis++)
		{
			for (int k = 0; k < 3; k++)
			{
				lines[count].from[k] = at[k];
				lines[count].to[k] = at[k] + (k == axis ? side : 0);
			}

			lines[count].colour = axisColours[axis];
			count++;
		}
	}

	// B: the box round the sections' places, 3 x (their extent from the root with +/- 10 mm kept in it), FUN_0047f4a0
	int32_t mins[3], maxs[3];

	if (hierarchyBones && HierarchyView_Extent(mins, maxs))
	{
		int32_t low[3], high[3];

		for (int k = 0; k < 3; k++)
		{
			int32_t const a = mins[k] - centre[k] < -10 ? mins[k] - centre[k] : -10;
			int32_t const b = maxs[k] - centre[k] > 10 ? maxs[k] - centre[k] : 10;

			low[k] = centre[k] + a * 3;
			high[k] = centre[k] + b * 3;
		}

		for (int c = 0; c < 8; c++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if ((c & (1 << axis)) != 0)
				{
					continue;
				}

				for (int k = 0; k < 3; k++)
				{
					lines[count].from[k] = (c & (1 << k)) ? high[k] : low[k];
					lines[count].to[k] = lines[count].from[k];
				}

				lines[count].to[axis] = high[axis];
				lines[count].colour = 0xFFFFFF40u;
				count++;
			}
		}
	}

	// V: every point of the attached shape as a small cross (up to what the buffer holds), the selected ones larger and yellow
	if (hierarchyMode.vertexMode)
	{
		size_t const points = hierarchySkin.size() / 3;

		for (size_t pass = 0; pass < 2; pass++)
		{
			for (size_t i = 0; i < points && count + 3 <= 4096; i++)
			{
				bool selected = false;

				for (int32_t const v : hierarchyMode.vertices)
				{
					selected = selected || (size_t)v == i;
				}

				if ((pass == 1) != selected)
				{
					continue;
				}

				int32_t const arm = selected ? 80 : 25;

				for (int axis = 0; axis < 3; axis++)
				{
					for (int k = 0; k < 3; k++)
					{
						int32_t const at = hierarchySkin[i * 3 + (size_t)k] + hierarchyShownPlace[k];

						lines[count].from[k] = at - (k == axis ? arm : 0);
						lines[count].to[k] = at + (k == axis ? arm : 0);
					}

					lines[count].colour = selected ? 0xFFFFFF00u : 0xFF40C0FFu;
					count++;
				}
			}
		}
	}

	LevelView_WorldLines(lines.data(), count);
	drawn = true;
}

// Strategy mode's View Flag (0x10 Airduct, 0x13 Stair, 0x16 Sky Visible, 0x19 Fogged: FUN_00525700 0x5257e7..): the modules that hold the flag are coloured and the others are drawn as they are
// (FUN_00522680 sets a draw bit, 0x4000000 at +0x64 of the module's object, for a module whose MODFLAGS word has the bit; the colour that bit means is not read: ours, orange). Stop Flag View (0x35)
// clears it. The module numbers are the game's (GameModuleNumbers), worked out again only when the level or the flag changes.
size_t flagViewTintCount = 0;   // how many modules the View Flag colours now (the hidden run reads it)

int32_t GameModuleOfNumber(int32_t number);

static void FlagViewTints(OpenLevelFile *open)
{
	static std::map<int32_t, int32_t> numbers;
	static unsigned numbersFor = ~0u;
	static unsigned tintsFor = ~0u;
	static uint32_t tintsBit = 0;
	static std::vector<int32_t> modules;
	static std::vector<uint32_t> colours;

	if (numbersFor != open->Generation())
	{
		host::GameModuleNumbers(LevelObjects_Level(), &numbers);
		numbersFor = open->Generation();
	}

	static std::vector<uint8_t> objectIds;
	static std::vector<uint32_t> objectColours;

	if (tintsFor != open->Generation() || tintsBit != strategyViewBit)
	{
		std::vector<host::LevelObject> rows;
		host::LevelStore store(open);

		modules.clear();
		colours.clear();
		objectIds.clear();
		objectColours.clear();
		LevelObjects_Read(&rows);

		for (size_t i = 0; i < rows.size() && modules.size() < kLevelViewMostSelected; i++)
		{
			int32_t number = 0;
			int32_t const game = host::ModuleIndexOfId(LevelObjects_Level(), rows[i].id, &number) ? GameModuleOfNumber(number) : -1;

			// every block that has a strategy is GREEN (FUN_00522680 0x522935: the block's flag 0x2000000; FUN_004a7eca makes it item flag 0x100, FUN_004980f0 the colour 0x00ff00), and green wins over the
			// flag view's orange
			if (host::StrategyMode_Has(open, rows[i].id) == host::StrategyModeResult::Done)
			{
				if (game >= 0)
				{
					modules.push_back(game);
					colours.push_back(0xFF00FF00u);
				}
				else if (objectColours.size() < kLevelViewMostSelected)
				{
					objectIds.insert(objectIds.end(), rows[i].id, rows[i].id + 8);
					objectColours.push_back(0xFF00FF00u);
				}

				continue;
			}
   // through the object's ID: a row's place in the list is not the module's number (the list holds the placed objects too)
			Chunk *flags = nullptr;
			std::vector<unsigned char> bytes;

			if (game >= 0 && host::FindModuleFlags(LevelObjects_Level(), rows[i].id, &flags) == host::LevelChunkFound::Found && flags && store.PayloadOf(flags, &bytes) && bytes.size() >= 4 &&
				(*(uint32_t const *)bytes.data() & strategyViewBit) != 0)
			{
				modules.push_back(game);
				colours.push_back(0xFFFF7F00u);   // the flag view's colour: item flag 0x200, FUN_004980f0: 0xff7f00
			}
		}

		tintsFor = open->Generation();
		tintsBit = strategyViewBit;
	}

	// RED modules (FUN_00529470: block flag 0x40000000 -> FUN_004a7cd0 0x4a7eca), FIRST in the list so they win over a green or orange (strategy-data.md S-62): the selection's red reaches only placed objects
	// (blocks with a strategy block), so a module's red is given here: the volume editor's picked module; on page 0 the current module; on the flag page every module in its list. Not while a kind's
	// Edit view runs (0x5286d7) nor on the Stability page (0x527379: the current's red is cleared first)
	std::vector<int32_t> redModules;
	// THE COLLECTOR'S MEMBERS (FUN_00526bf0: every object added is FUN_00529470'd, RED; at its end each is FUN_00529480'd): Copy's targets, a Lift's positions, a switch's, a track
	// point's or a mission's targets. Copy takes the source's own red off before it (0x5283a5) and gives it back after (0x5285bd): the selection is not red while it runs.
	std::vector<uint8_t> redObjects;

	if (OnStrategyScreen() && copyCollecting)
	{
		for (CopyTarget const &target : copyTargets)
		{
			int32_t number = 0;

			if (host::ModuleIndexOfId(LevelObjects_Level(), target.id, &number))
			{
				int32_t const game = GameModuleOfNumber(number);

				if (game >= 0 && redModules.size() < 64)
				{
					redModules.push_back(game);
				}
			}
			else if (redObjects.size() < 64 * 8)
			{
				redObjects.insert(redObjects.end(), target.id, target.id + 8);
			}
		}
	}

	if (volume.on && volume.module >= 0)
	{
		redModules.push_back(volume.module);
	}
	else if (OnStrategyScreen() && !strategyEditFan && !stabilityActive && !copyCollecting && host::Selection_SetCount() > 0)
	{
		for (size_t s = 0; s < host::Selection_SetCount() && redModules.size() < 64; s++)
		{
			uint8_t memberId[8];
			int32_t number = 0;

			if (host::Selection_SetMember(s, memberId) && host::ModuleIndexOfId(LevelObjects_Level(), memberId, &number))
			{
				int32_t const game = GameModuleOfNumber(number);

				if (game >= 0)
				{
					redModules.push_back(game);
				}
			}
		}
	}

	if (!redModules.empty())
	{
		std::vector<int32_t> withModule = redModules;
		std::vector<uint32_t> withColour(redModules.size(), 0xFFFF0000u);

		withModule.insert(withModule.end(), modules.begin(), modules.end());
		withColour.insert(withColour.end(), colours.begin(), colours.end());
		LevelView_SetModuleTints(withModule.data(), withColour.data(), withModule.size());
	}
	else
	{
		LevelView_SetModuleTints(modules.data(), colours.data(), modules.size());
	}

	if (volume.on && volume.objectPicked)
	{
		// the object the volume editor picked is RED (FUN_00529470: block flag 0x40000000), over its green
		std::vector<uint8_t> withObject(volume.object, volume.object + 8);
		std::vector<uint32_t> withColour(1, 0xFFFF0000u);

		withObject.insert(withObject.end(), objectIds.begin(), objectIds.end());
		withColour.insert(withColour.end(), objectColours.begin(), objectColours.end());
		LevelView_SetObjectTints(withObject.data(), withColour.data(), withColour.size());
	}
	else if (!redObjects.empty())
	{
		// the collector's placed members RED, first so they win over their green
		std::vector<uint8_t> withObject = redObjects;
		std::vector<uint32_t> withColour(redObjects.size() / 8, 0xFFFF0000u);

		withObject.insert(withObject.end(), objectIds.begin(), objectIds.end());
		withColour.insert(withColour.end(), objectColours.begin(), objectColours.end());
		LevelView_SetObjectTints(withObject.data(), withColour.data(), withColour.size());
	}
	else
	{
		LevelView_SetObjectTints(objectIds.data(), objectColours.data(), objectColours.size());
	}

	flagViewTintCount = modules.size();
}

// Waypoint's modes 1 to 3: only the AI module's modules are drawn (FUN_00583b40 0x583b69-0x583b8d sets +0x5c bit 4 on every module, then FUN_005890c0 clears it on the master and its slaves;
// mode 1's Exit shows them all again, FUN_00583c10 -> FUN_004d41f0). The view rule's own hidden set carries it (Module Linking and Adjacent Modules set that set only on their screens).
void WaypointHidePass()
{
	static bool hiding = false;
	static std::vector<int32_t> hidden;
	static std::vector<int32_t> hiddenFor;
	static unsigned hiddenGeneration = 0xFFFFFFFFu;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!WaypointWorldOn() || !open || !open->Level())
	{
		if (hiding)
		{
			LevelView_HideByRule(nullptr, 0, false);
			hiding = false;
			hiddenFor.clear();
		}

		return;
	}

	std::vector<int32_t> const group = waypointMode.CurrentGroup();

	if (!hiding || group != hiddenFor || hiddenGeneration != open->Generation())
	{
		std::vector<int32_t> keep;

		for (int32_t const objectIndex : group)
		{
			int32_t const game = GameModuleOfNumber(objectIndex);

			if (game >= 0)
			{
				keep.push_back(game);
			}
		}

		hidden.clear();

		for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
		{
			if (std::find(keep.begin(), keep.end(), m) == keep.end())
			{
				hidden.push_back(m);
			}
		}

		hiddenFor = group;
		hiddenGeneration = open->Generation();
	}

	LevelView_HideByRule(hidden.data(), hidden.size(), false);
	hiding = true;
}

void UpdateModuleTints(bool onAdjacent)
{
	static std::map<int32_t, int32_t> numbers;
	static unsigned numbersFor = ~0u;
	static bool drawn = false;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (moduleOverlapsOn && open && overlapsFor != open->Generation())
	{
		WorkOutModuleOverlaps(open);
	}

	// Object Placement's Show Polygon Counts: the paint alone, no link tree
	if (placementPolyCounts && (barScreen >= host::kIntengScreenCount || (strcmp(host::kIntengScreens[barScreen].name, "Object Placement") != 0 && strcmp(host::kIntengScreens[barScreen].name, "Track Editing") != 0)))
	{
		placementPolyCounts = false;
		showAllModules.clear();
		showAllColours.clear();
	}

	// Module Linking's Show All while it holds the screen: its paint ALONE, with or without a current module (FUN_004c17a0 is not called until Escape or a button lets it go, row ML-M-3)
	if (open && !showAllModules.empty() && ((!linkTree && placementPolyCounts) || (showAllHeld && OnModuleLinkingScreen())))
	{
		size_t const count = showAllModules.size() < kLevelViewMostSelected ? showAllModules.size() : kLevelViewMostSelected;

		LevelView_SetModuleTints(showAllModules.data(), showAllColours.data(), count);
		drawn = true;
		return;
	}

	// The Adjacent Modules screen's own paint (FUN_004046b0): every module white, the current module blue and the modules of its list yellow (palette 0xff / 0xfc, the same blue and yellow as Module Linking's);
	// Show All's blue and white replace it until the next repaint.
	if (!onAdjacent && adjacentShowAllPaint)
	{
		showAllModules.clear();
		showAllColours.clear();
		adjacentShowAllPaint = false;
	}

	bool const adjacentPaint = onAdjacent && open && (adjacentShowAllPaint || (adjacentList && adjacentList->Current() >= 0));

	bool const linkPicksShown = !linkPicks.empty() && OnModuleLinkingScreen();

	// Waypoint: inteng's paint of the modules (green: an AI module with a WAYPOINT, white: none, red: the pick, 0xffffffff: the current AI module; in modes 1 to 3 the AI module white; waypoint_mode.h)
	if (open && open->Level() && OnWaypointScreen())
	{
		std::map<int32_t, uint32_t> const &paint = waypointMode.ModulePaint(open);
		std::vector<int32_t> games;
		std::vector<uint32_t> colours;

		for (std::map<int32_t, uint32_t>::const_iterator it = paint.begin(); it != paint.end(); ++it)
		{
			int32_t const game = GameModuleOfNumber(it->first);

			if (game >= 0)
			{
				games.push_back(game);
				colours.push_back(it->second);
			}
		}

		LevelView_SetObjectTints(nullptr, nullptr, 0);
		LevelView_SetModuleTints(games.data(), colours.data(), games.size());
		drawn = true;
		return;
	}

	if ((!linkTree && !adjacentPaint && !moduleOverlapsOn && !linkPicksShown) || !open)
	{
		bool const onStrategy = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0;

		if (open && onStrategy)
		{
			FlagViewTints(open);
			drawn = true;
			return;
		}

		// Acoustic Mode's picks are RED (FUN_004f57b0 0x4f5a6f: the block's +0x64 bit 0x40000000, drawn 0xff0000 by FUN_004a7cd0 0x4a7eca)
		if (open && OnAcousticScreen())
		{
			std::vector<int32_t> red;

			for (std::array<uint8_t, 8> const &picked : acousticPicks)
			{
				int32_t number = 0;
				int32_t const game = host::ModuleIndexOfId(open->Level(), picked.data(), &number) ? GameModuleOfNumber(number) : -1;

				if (game >= 0)
				{
					red.push_back(game);
				}
			}

			std::vector<uint32_t> const colours(red.size(), 0xFFFF0000u);

			LevelView_SetObjectTints(nullptr, nullptr, 0);
			LevelView_SetModuleTints(red.data(), colours.data(), red.size());
			drawn = true;
			return;
		}

		if (drawn)
		{
			LevelView_SetObjectTints(nullptr, nullptr, 0);
			LevelView_SetModuleTints(nullptr, nullptr, 0);
			drawn = false;
			flagViewTintCount = 0;
		}

		return;
	}

	if (numbersFor != open->Generation())
	{
		host::GameModuleNumbers(LevelObjects_Level(), &numbers);
		numbersFor = open->Generation();
	}

	std::vector<int32_t> modules;
	std::vector<uint32_t> colours;
	auto const add = [&](int32_t objectIndex, uint32_t colour)
	{
		auto const found = numbers.find(objectIndex);

		if (found != numbers.end() && modules.size() < 256)
		{
			modules.push_back(found->second);
			colours.push_back(colour);
		}
	};

	// the picks first (the first colour given for a module wins in the view): 0x7f007f, as FUN_004c17a0 paints them after the tree (0x4c18b2, row ML-M-61)
	if (linkPicksShown)
	{
		for (int32_t const pick : linkPicks)
		{
			add(pick, 0xFF7F007Fu);
		}
	}

	if (linkTree && !onAdjacent)
	{
		for (host::LinkTree::Tint const &tint : linkTree->Tints())
		{
			add(tint.module, 0xFF000000u | tint.rgb);
		}
	}

	if (adjacentPaint && !adjacentShowAllPaint)
	{
		for (int32_t const member : adjacentList->Members())
		{
			add(member, 0xFFFFFF00u);
		}

		add(adjacentList->Current(), 0xFF0000FFu);
	}

	linkedGameModules = modules;

	if (moduleOverlapsOn)
	{
		for (size_t i = 0; i < overlapObjects.size() && i < overlapColours.size(); i++)
		{
			add(overlapObjects[i], overlapColours[i]);
		}
	}

	for (size_t i = 0; i < showAllModules.size() && modules.size() < kLevelViewMostSelected; i++)
	{
		modules.push_back(showAllModules[i]);
		colours.push_back(showAllColours[i]);
	}

	LevelView_SetModuleTints(modules.data(), colours.data(), modules.size());
	drawn = true;
}


// Write Asc File (P7, FUN_00506f90): the level's shapes as a .asc. inteng asks "Type filename (without .asc extension)" and writes <name>.asc beside
// itself; here the file goes to <Documents>\AVP_OpenInteng\edited\<name>.asc and NEVER inside the game's folder (save_copy.h's rules). The numbers are
// asc_level.h's, the text asc_text.h's. (Added 2026-11-15 by Melanikus (Drake), P7.)
void WriteTheAsc(std::string const &typed, bool doorsClosed)
{
	char opened[MAX_PATH] = "";

	if (!LevelObjects_Level() || !LevelView_FilePath(opened, sizeof(opened)))
	{
		Answer("Write Asc File: no level is open");
		return;
	}

	std::string path;
	std::string folder;
	char const *why = nullptr;

	if (!host::SaveExportPlace(opened, typed.c_str(), ".asc", theCopiesFolder, theGameFolder, &path, &folder, &why))
	{
		Answer(std::string("Write Asc File: not written: ") + (why ? why : "no place for the file"));
		return;
	}

	char checked[MAX_PATH];

	std::string const baseNotUsable = MakeTheCopiesFolder();

	if (!baseNotUsable.empty())
	{
		Answer("Write Asc File: the folder for copies can't be used: " + baseNotUsable);
		return;
	}

	if (!CreateDirectoryA(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		Answer("Write Asc File: the editor couldn't make its folder for exports");
		return;
	}

	if (char const *const notUsable = SaveFolder_WhyNotUsable(folder.c_str(), checked, sizeof(checked)))
	{
		Answer(std::string("Write Asc File: the folder for exports can't be used: ") + notUsable);
		return;
	}

	// A file already there is the editor's own earlier export, replaced; a link or a folder is never written through
	DWORD const there = GetFileAttributesA(path.c_str());

	if (there != INVALID_FILE_ATTRIBUTES && (there & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)))
	{
		Answer("Write Asc File: not written: the name is a folder or a link");
		return;
	}

	std::vector<host::AscLevelObject> objects;
	host::AscOffset offset;

	host::AscWalkLevel(LevelObjects_Level(), doorsClosed, host::AscScale(), &objects, &offset);

	std::string const text = host::AscFileText(objects, offset, host::AscScale());

	// inteng's CreateFileA(name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, ...), then WriteFile for each line
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		Answer("Write Asc File: the file couldn't be made");
		return;
	}

	DWORD written = 0;
	bool const whole = WriteFile(file, text.data(), (DWORD)text.size(), &written, NULL) && written == (DWORD)text.size();

	CloseHandle(file);

	if (!whole)
	{
		Answer("Write Asc File: the file couldn't be written whole: " + path);
		return;
	}

	savedWords = "Written " + path + " (" + std::to_string(objects.size()) + " objects)";
	statusWords = savedWords.c_str();
	LogString("write asc: " + path);
}
