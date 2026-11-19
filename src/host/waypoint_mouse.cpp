// See waypoint_mouse.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The includes are construction_modes_screen.cpp's (editor_shared.h needs them).

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

#include <algorithm>

#include "waypoint_mouse.h"
#include "waypoint_pick.h"

namespace
{
	std::string mouseWords;   // the status line's words (statusWords copies them)

	// ITEM 3, THE DRAG: a press that took a box remembers it and its keys; a release far enough from it asks the next frame's pick there (linkEndAsked), which ends the drag
	int32_t pressBox = -1;
	int32_t pressX = 0, pressY = 0;
	bool pressShift = false, pressCtrl = false;
	bool linkEndAsked = false;
	int32_t const kDragLeast = 6;   // pixels the pointer must move between the press and the release for a drag (less is a click)

	void Say(std::string const &words)
	{
		mouseWords = words;
		statusWords = mouseWords.c_str();
	}
}

bool WaypointMouse_Release(int32_t x, int32_t y)
{
	int32_t const from = pressBox;

	pressBox = -1;

	if (from < 0 || !OnWaypointScreen() || waypointMode.Mode() != 1 || !waypointMode.AlienList() || abs(x - pressX) + abs(y - pressY) < kDragLeast)
	{
		return false;
	}

	// the next frame's pick, where the button was let go (mode 1 has no three views: the whole view's pixels)
	pressBox = from;
	linkEndAsked = true;
	pickX = x;
	pickY = y;
	pickInBox = false;
	pickAsked = true;
	return true;
}

void WaypointMouseClick(host::PickReport const &picked, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight, bool shift, bool ctrl)
{
	bool const linkEnd = linkEndAsked;
	int32_t const linkFrom = pressBox;

	linkEndAsked = false;
	pressBox = -1;

	// Edit Extents' press in the 3D quarter, and the ground list: inteng's own pick
	if (waypointMode.Mode() != 1 || !waypointMode.AlienList())
	{
		WaypointBoxClick(x, y, viewWidth, viewHeight);

		if (waypointMode.Mode() == 1)
		{
			Say("Waypoint by mouse works on the alien list only: the game never reads the ground list, so here a click picks as inteng's (the box nearest the pointer)");
		}

		return;
	}

	host::PickRay ray;

	if (!PickGame_RayAtClick(x, y, viewWidth, viewHeight, &ray))
	{
		Say("Waypoint by mouse: no line through that point of the view");
		return;
	}

	double const from[3] = { (double)ray.from[0], (double)ray.from[1], (double)ray.from[2] };
	double const direction[3] = { (double)ray.direction[0], (double)ray.direction[1], (double)ray.direction[2] };
	bool const metSomething = picked.found == host::PickFound::Module || picked.found == host::PickFound::Object;
	double const wallAt = metSomething ? (double)picked.distance : -1.0;

	// the box the line meets first, in front of the wall it met: the current one (any: the current and the base too)
	int32_t const box = host::WaypointPick_BoxOnRay(waypointMode.List(), local_scale, waypointMode.Minimised(), waypointMode.CurrentWaypoint(), from, direction, wallAt);

	// the drag's end: let go on another box, the two linked (plain both ways, Shift aliens only, Ctrl one way from the first; both: both), written at once
	if (linkEnd)
	{
		OpenLevelFile *const openForLink = LevelObjects_OpenForEdit();

		if (box < 0 || box == linkFrom)
		{
			Say("Link: let go on another waypoint's box to link it with waypoint " + std::to_string((long long)linkFrom));
			return;
		}

		int32_t const flags = (pressShift ? host::kLinkAlien : 0) | (pressCtrl ? host::kLinkOneWay : 0);
		host::WaypointOutcome const linked = waypointMode.MouseLink(openForLink, linkFrom, box, flags);

		if (linked.levelChanged)
		{
			LoadTheObjectListAgain();
		}

		Say(linked.say);
		return;
	}

	// ITEM 3b, A LINK: the line nearest the pointer of those it takes (waypoint_pick.h WaypointPick_NearLink: within 4 px, 12 px clear of both ends), each pair once as the view draws it;
	// asked before the boxes, as a link lies inside them. Clicked once it is SELECTED (its ends the base and the current one); clicked again, its flags go one on in the cycle
	host::PickCamera camera;
	std::vector<host::WaypointRec> const &list = waypointMode.List();
	int32_t linkA = -1, linkB = -1;
	double linkPixels = 1e300;

	if (PickGame_Camera(viewWidth, viewHeight, &camera))
	{
		for (size_t w = 0; w < list.size(); w++)
		{
			for (host::WaypointLinkRec const &link : list[w].links)
			{
				if (link.index < 0 || (size_t)link.index >= list.size() || (size_t)link.index == w)
				{
					continue;
				}

				// drawn once a pair: by the earlier waypoint when the later one links back (waypoint_view.cpp)
				host::WaypointRec const &other = list[(size_t)link.index];
				bool const back = std::any_of(other.links.begin(), other.links.end(), [w](host::WaypointLinkRec const &l) { return l.index == (int32_t)w; });

				if ((size_t)link.index < w && back)
				{
					continue;
				}

				int32_t a[3], b[3];
				double ax = 0, ay = 0, bx = 0, by = 0, aAhead = 0, bAhead = 0, pixels = 0;

				host::WaypointPick_AimWorld(list[w], local_scale, a);
				host::WaypointPick_AimWorld(other, local_scale, b);

				if (host::PickRay_Project(camera, a, &ax, &ay, &aAhead) && host::PickRay_Project(camera, b, &bx, &by, &bAhead) &&
					host::WaypointPick_NearLink(ax, ay, bx, by, (double)x, (double)y, &pixels) && pixels < linkPixels)
				{
					linkA = (int32_t)w;
					linkB = link.index;
					linkPixels = pixels;
				}
			}
		}
	}

	if (linkA >= 0)
	{
		bool const again = waypointMode.LinkSelected() && ((waypointMode.BaseWaypoint() == linkA && waypointMode.CurrentWaypoint() == linkB) ||
			(waypointMode.BaseWaypoint() == linkB && waypointMode.CurrentWaypoint() == linkA));

		if (!again)
		{
			Say(waypointMode.MouseSelectLink(linkA, linkB).say);
			return;
		}

		OpenLevelFile *const openForFlags = LevelObjects_OpenForEdit();
		int32_t const now = waypointMode.LinkFlagsOf(waypointMode.BaseWaypoint(), waypointMode.CurrentWaypoint());
		host::WaypointOutcome const changed = waypointMode.MouseSetLinkFlags(openForFlags, host::WaypointPick_NextLinkFlags(now));

		if (changed.levelChanged)
		{
			LoadTheObjectListAgain();
		}

		Say(changed.say);
		return;
	}

	if (box >= 0)
	{
		waypointMode.MousePick(box);
		Say("Waypoint " + std::to_string((long long)box) + " is the current one");

		// a drag may start here (WaypointMouse_Release)
		pressBox = box;
		pressX = x;
		pressY = y;
		pressShift = shift;
		pressCtrl = ctrl;
		return;
	}

	// ITEM 1, a FLOOR (its face within 45 degrees of up) of a module: in the AI module, a new 2 x 2 x 2 m box on it, the current one, written at once; another AI module's, refused
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (picked.found == host::PickFound::Module && picked.hasHit && host::WaypointPick_IsFloor(picked.hitNormal) && open && open->Level())
	{
		uint8_t id[8];
		int32_t objectIndex = -1;
		std::vector<int32_t> const group = waypointMode.CurrentGroup();

		if (!ModuleObject(open, picked.moduleIndex, id) || !host::ModuleIndexOfId(open->Level(), id, &objectIndex))
		{
			Say("That floor's module is not one of the level file's: no waypoint made");
			return;
		}

		if (std::find(group.begin(), group.end(), objectIndex) == group.end())
		{
			Say("That floor is not in AI module " + std::to_string((long long)waypointMode.CurrentModule()) + " (module " + std::to_string((long long)objectIndex) +
				" belongs to another): no waypoint made; Exit and Edit Module to work there");
			return;
		}

		host::WaypointOutcome const made = waypointMode.MouseCreate(open, host::WaypointPick_NewBox(picked.hitPoint, local_scale));

		if (made.levelChanged)
		{
			LoadTheObjectListAgain();
		}

		Say(made.say);
		return;
	}

	// nothing (a wall, a ceiling, nothing at all): the current one let go
	bool const had = waypointMode.CurrentWaypoint() >= 0;

	waypointMode.MousePick(-1);
	Say(had ? "No waypoint under the pointer: none is current now" : "No waypoint under the pointer");
}
