// editor_panels.cpp: THE PANELS OF THE FRAME: PlanThePanels, what each screen draws over the view (its text lines, boxes and titles). Moved here whole from editor_main.cpp on 2026-11-17 by Melanikus (Dietrich), so that each file stays
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
#include "module_link_avp.h"
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
#include "hub_text.h"
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
#include "autoplot_command.h"
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
#include "state_line.h"
#include "text_scale.h"   // the Options text sizes' ticks (the owner's addition)

// A Strategy Edit view's box in the window (the owner's addition, 11-17): half the window each way, never under inteng's 640 x 480 page (the size every view was laid out for), never over the
// window, centred in it
static ui::Box StrategyViewBoxIn(ui::Box const &window)
{
	int32_t const width = window.width / 2 > 640 ? window.width / 2 : (window.width > 640 ? 640 : window.width);
	int32_t const height = window.height / 2 > 480 ? window.height / 2 : (window.height > 480 ? 480 : window.height);

	return ui::Box{ window.x + (window.width - width) / 2, window.y + (window.height - height) / 2, width, height };
}

PanelExtras PlanThePanels(const ui::PanelLayout &layout, host::PaintText *lines, host::PaintBox *boxes, ui::Box *titleBoxes)
{
	size_t lineCount = 0;
	size_t boxCount = 0;
	int32_t titleWidths[ui::kMenuMostMenus];
	int32_t dropWidths[ui::kMenuMostMenus];
	static int32_t cascadeWidths[ui::kMenuMostMenus * ui::kMenuMostItems];   // each item's cascade, as wide as its widest row (BAR-11)

	// the state line's screentext is made again each frame: the screens' top-left text below adds to it (Strategy Data's current-object lines, the hub's, Module Linking's)
	screenTextNow.clear();

	// The menu bar: gray behind its titles only, as far as they go and a little past, and its drop-downs open over the level anywhere in the
	// window below the bar
	// (A drop-down starts at the top of its room, MenuBar::DropBox, so the room starts BELOW the bar: from the bar's foot to the window's
	// bottom. With the whole window as the room, a drop-down covered its own title and the button's release picked its first item at once.)
	// The toolbar is one bar for each physical row of the titles (they wrap to the window's width), the first on top
	EditorBarPlaceRows const place = EditorBar_PlaceRows(layout, barRowCount);
	int32_t titlesEnd[kEditorBarMostRows];

	for (size_t row = 0; row < barRowCount; row++)
	{
		host::MenuPaint_Measure(barRows.menus[row], barRows.menuCount[row], PanelPass_MenuWidth, titleWidths, dropWidths, cascadeWidths);
		BarRow(row).LayOut(place.bar[row], place.dropRoom[row], titleWidths, dropWidths, host::MenuPaint_ItemHeight(kPanelPassLineHeight));
		BarRow(row).SetCascadeWidths(cascadeWidths);
		titlesEnd[row] = layout.toolbar.x;

		for (size_t m = 0; m < barRows.menuCount[row]; m++)
		{
			ui::Box const title = BarRow(row).TitleBox(m);

			if (row == 0)
			{
				titleBoxes[m] = title;
			}

			titlesEnd[row] = title.x + title.width > titlesEnd[row] ? title.x + title.width : titlesEnd[row];
		}
	}

	// A title past the toolbar's edge (a window narrower than the least the bars need) is named on the status line, for as long as it is
	EditorBar_CutOff(menuBar, barRows.menus[0], barRows.menuCount[0], cutOffWords, sizeof(cutOffWords));

	for (size_t row = 1; row < barRowCount && !cutOffWords[0]; row++)
	{
		EditorBar_CutOff(lowerBar[row - 1], barRows.menus[row], barRows.menuCount[row], cutOffWords, sizeof(cutOffWords));
	}

	if (cutOffWords[0])
	{
		statusWords = cutOffWords;
	}
	else if (statusWords.From(cutOffWords))
	{
		statusWords = nullptr;
	}

	// FUN_004b7c50: the bar is shown while the pointer is at the top edge (y < 3) or Control is held, and stays while the pointer is inside the bar's height, or a menu is open
	{
		bool const open = menuBar.IsOpen() || ALowerMenuIsOpen();
		bool const control = KeyHeldNow(VK_CONTROL);
		bool const over = barPointerY >= 0 && (barPointerY < 3 || (barVisible && barPointerY < layout.toolbar.y + layout.toolbar.height));

		// a strategy kind's Edit view runs its own loop (S-KC-1: FUN_0052cbb0 and the others clear the window and draw their lines, then FUN_0057bd90): FUN_004b7c50 is not called, so no bar shows over it
		barVisible = !strategyEditFan && (open || control || over || TextureBarsAlwaysShown());   // Texture Map: FUN_0057b6f0 every frame (P, texture_map_screen.cpp)
	}

	titlesEnd[0] += 12;
	hitBar.x = layout.toolbar.x;
	hitBar.y = layout.toolbar.y;
	hitBar.width = titlesEnd[0] - layout.toolbar.x > 0 ? titlesEnd[0] - layout.toolbar.x : 0;
	hitBar.height = place.bar[0].height;

	if (!barVisible)
	{
		hitBar = ui::Box{ 0, 0, 0, 0 };   // hidden: nothing is drawn and nothing is hit
	}
	else
	{
		// inteng's bar is a BLUE strip the window's whole width (view-state-table.md H-5, H-5a: style +0x30 (0, 0, 0xff)); a press anywhere on it is the bar's
		hitBar.x = 0;
		hitBar.width = layout.view.x + layout.view.width;
		boxes[boxCount++] = host::PaintBox{ hitBar.x, hitBar.y, hitBar.width, hitBar.height, host::kMenuBlueColour, true };   // over the screen (BAR-6)
	}

	for (size_t row = 1; row < kEditorBarMostRows; row++)
	{
		if (row < barRowCount && barVisible)
		{
			titlesEnd[row] += 12;
			hitLower[row - 1].x = place.bar[row].x;
			hitLower[row - 1].y = place.bar[row].y;
			hitLower[row - 1].width = titlesEnd[row] - place.bar[row].x > 0 ? titlesEnd[row] - place.bar[row].x : 0;
			hitLower[row - 1].height = place.bar[row].height;
			hitLower[row - 1].x = 0;
			hitLower[row - 1].width = layout.view.x + layout.view.width;
			boxes[boxCount++] = host::PaintBox{ hitLower[row - 1].x, hitLower[row - 1].y, hitLower[row - 1].width, hitLower[row - 1].height, host::kMenuBlueColour, true };
		}
		else
		{
			hitLower[row - 1] = { 0, 0, 0, 0 };
		}
	}

	size_t menuLines = 0;
	size_t menuBoxes = 0;

	// The menu items of the state that is on are highlighted: the draw mode in use, and Gouraud, Textures, the headlight and back faces while on. They follow
	// every change, whatever made it (a key, a menu item, a button, a screen change, the start's value).
	{
		host::ViewToggles const now = ViewToggles_Now();

		// every row is told: an item sits in whichever row the titles wrapped it into
		for (size_t row = 0; row < barRowCount; row++)
		{
			ui::MenuBar &bar = BarRow(row);

			bar.SetChecked(kActionViewPolygon, now.drawMode == host::DrawMode::Polygon);
			bar.SetChecked(kActionViewPolyline, now.drawMode == host::DrawMode::Polyline);
			bar.SetChecked(kActionViewFilledPolyline, now.drawMode == host::DrawMode::FilledPolyline);
			bar.SetChecked(kActionViewWireTextured, now.drawMode == host::DrawMode::WireTextured);
			bar.SetChecked(kActionViewGouraud, now.gouraud == host::Gouraud::On);
			bar.SetChecked(kActionViewTextures, now.textured);
			bar.SetChecked(kActionViewHeadlight, now.headlight);
			bar.SetChecked(kActionViewBackFaces, now.backFaces);

			// The Options switches: ticked while on (Construction's Options and Adjacent Modules' Select by polygons)
			bar.SetChecked(ActionOfCommand("0x5ced30", 0x1f4), editorOptions.selectByPolygons);
			bar.SetChecked(ActionOfCommand("0x5c3508", 0xca), editorOptions.selectByPolygons);
			bar.SetChecked(ActionOfCommand("0x5ced30", 0x1f5), editorOptions.preferBmp);
			bar.SetChecked(ActionOfCommand("0x5ced30", 0x1f6), editorOptions.polygonViewDistance);
			for (int32_t step = 0; step < 4; step++) { bar.SetChecked(0x1ff02 + step, host::TextScale_Get() == host::kTextScaleSteps[step]); }   // the text sizes (inteng_menus.cpp additions)

			// Module Linking's two switches: ticked while on
			bar.SetChecked(ActionOfCommand("0x5e2e38", 0x49), viewDistanceOn);
			bar.SetChecked(ActionOfCommand("0x5e2e38", 0x4b), onlyDrawLinked);
			bar.SetChecked(ActionOfCommand("0x5e2e38", 0x190), host::ModuleLinkAvp_On());   // Link with AVP's tick (0x4c36da)
			bar.SetChecked(ActionOfCommand("0x5c3508", 0x46), adjacentViewDistanceOn);

			// Hierarchy's three switches with a tick (row M-TICK): each flip calls the menu object's method +0x2c(command, the new value) (0x4b90b0 -> FUN_004b7500), which SETS the item's "on" state to it (Adaptive Degradation 0x4896ef,
			// Dummy Objects 0x489714, Save changes to sequence times 0x488aa7); all three start off, so the tick is the switch. Toggle on/off (0x4c, 0x4897f3) flips DAT_00637118 with no tick.
			bar.SetChecked(HierarchyBarAction(0x45), hierarchyMode.adaptiveDegradation);
			bar.SetChecked(HierarchyBarAction(0x5c), hierarchyMode.dummyObjects);
			bar.SetChecked(HierarchyBarAction(0x48), hierarchyMode.saveTimesOn);

			// Object Placement's Misc > Toggle Gravity (0x58): ticked by DAT_008f4f50, at the mode's start (0x4d8422-0x4d842c) and at each flip (0x4dc67c-0x4dc6e0); Track Editing's page has it too (V-24)
			bar.SetChecked(ActionOfCommand("0x5e51b8", 0x58), PlacementGravityOn());
			bar.SetChecked(ActionOfCommand("0x5e6e98", 0x58), PlacementGravityOn());
			bar.SetChecked(ActionOfCommand("0x5e51b8", 0x54), PlacementLightingOn());   // Misc > Lighting by DAT_008f4f0c (0x4d840f-0x4d8419 at the start, 0x4d9f58 at each flip; OP-44)
			bar.SetChecked(ActionOfCommand("0x5e6e98", 0x54), PlacementLightingOn());
		}

		// The textures setting is kept in the editor's own settings file when it changes
		if (now.textured != texturesSaved && layoutSettingsFile[0])
		{
			WritePrivateProfileStringA("Editor", "Textured", now.textured ? "1" : "0", layoutSettingsFile);
			texturesSaved = now.textured;
		}
	}

	size_t const menuLinesFrom = lineCount;
	size_t const menuBoxesFrom = boxCount;

	if (barVisible)
	{
		host::MenuPaint_Plan(menuBar, barRows.menus[0], PanelPass_MenuWidth, kPanelPassLineHeight, lines + lineCount, kPanelPassMostExtras - lineCount, &menuLines, boxes + boxCount,
			kPanelPassMostExtras - boxCount, &menuBoxes);
		lineCount += menuLines;
		boxCount += menuBoxes;
	}

	for (size_t row = 1; row < barRowCount && barVisible; row++)
	{
		size_t menuLinesRow = 0;
		size_t menuBoxesRow = 0;

		host::MenuPaint_Plan(lowerBar[row - 1], barRows.menus[row], PanelPass_MenuWidth, kPanelPassLineHeight, lines + lineCount, kPanelPassMostExtras - lineCount, &menuLinesRow, boxes + boxCount,
			kPanelPassMostExtras - boxCount, &menuBoxesRow);
		lineCount += menuLinesRow;
		boxCount += menuBoxesRow;
	}

	// The pop-up, when open: at the pointer, kept inside the window, drawn over everything (its titles have no room, so only its drop-down shows)
	if (popupBar.IsOpen() && popupRoom.menuCount == 1)
	{
		int32_t popTitle[1];
		int32_t popDrop[1];
		int32_t const rowH = host::MenuPaint_ItemHeight(kPanelPassLineHeight);
		int32_t const windowRight = layout.view.x + layout.view.width;
		int32_t const windowBottom = layout.view.y + layout.view.height;
		int32_t const needed = (int32_t)popupRoom.menus[0].count * rowH;
		int32_t const top = popupAt.y + needed > windowBottom ? (windowBottom - needed > 0 ? windowBottom - needed : 0) : popupAt.y;
		ui::Box const popBar = { popupAt.x, top, windowRight - popupAt.x > 0 ? windowRight - popupAt.x : 0, 0 };
		ui::Box const popRoom = { 0, top, windowRight, windowBottom - top > 0 ? windowBottom - top : 0 };

		static int32_t popCascades[ui::kMenuMostMenus * ui::kMenuMostItems];

		host::MenuPaint_Measure(popupRoom.menus, 1, PanelPass_MenuWidth, popTitle, popDrop, popCascades);
		popupBar.LayOut(popBar, popRoom, popTitle, popDrop, rowH);
		popupBar.SetCascadeWidths(popCascades);

		size_t popLines = 0;
		size_t popBoxes = 0;

		host::MenuPaint_Plan(popupBar, popupRoom.menus, PanelPass_MenuWidth, kPanelPassLineHeight, lines + lineCount, kPanelPassMostExtras - lineCount, &popLines,
			boxes + boxCount, kPanelPassMostExtras - boxCount, &popBoxes);
		lineCount += popLines;
		boxCount += popBoxes;
	}

	// the bar's rows, their drop-downs and the pop-up are inteng's opaque blits over the frame (reassess/dialog-look-drake.md BAR-6): drawn over every word and box of the screen
	for (size_t i = menuLinesFrom; i < lineCount; i++)
	{
		lines[i].overlay = true;
	}

	for (size_t i = menuBoxesFrom; i < boxCount; i++)
	{
		boxes[i].overlay = true;
	}

	// The field panel follows the selection first: when it changes, the record for what's selected is shown, and the panel opens over the level
	// (or closes, with nothing to show)
	if (host::Selection_Changes() != fieldsSawChanges)
	{
		fieldsSawChanges = host::Selection_Changes();
		fieldsHaveKeys = false;

		if (OnStrategyScreen() && !strategyFlagPage && !strategyPanelOn)
		{
			LetGoOfTheFieldPanel();   // inteng's Strategy Data draws no panel for the current object
		}
		else
		{
			ShowTheSelected(host::Selection_Get());
		}

		fieldsOpenedAt = fieldHost.Showing() ? ++openClock : 0;
	}

	if (!fieldHost.Showing())
	{
		fieldsOpenedAt = 0;
		fieldsHaveKeys = false;
	}

	// The keyboard is in the field panel while it says so, and the focused field is marked (S3d)
	fieldHost.SetTyping(fieldsHaveKeys && !AMenuIsOpen());

	// The left column below the bar, shared by whichever of the two are open: the field panel at the foot of the list's half when both are, the
	// whole column to the list when only it is, the top of the column to the field panel when only it is. Nothing is drawn, nor can be hit,
	// for a panel that is closed.
	int32_t const rowHeight = host::MenuPaint_ItemHeight(kPanelPassLineHeight);

	// NO LIST AND NO FIELD PANEL ON THE HUB (ruling 10b, Bishop 2026-11-16): inteng's hub has neither (an object is picked by a click, FUN_004452b0 draws only the view, the bar and its dialogs); both stay on the screens
	// whose own inteng UI does not yet give every use of them (the report to Bishop lists those)
	if (barScreen == host::kIntengHub)
	{
		listOpenedAt = 0;
		fieldsOpenedAt = 0;
	}

	bool const listShown = listOpenedAt != 0;
	bool const fieldsShown = fieldsOpenedAt != 0 && !strategyEditFan && !(OnTheTrackScreen() && trackEd.state != host::TrackState::Off);   // inteng's Track Editing draws only its own text lines (TrackTextLines), no field panel;   // the Fan's edit view (FUN_005356b0) draws its own four lines where the field panel would be: inteng has no field panel on this screen
	int32_t const wanted = fieldsShown ? fieldHost.HeightFor(kPanelPassLineHeight) : 0;
	ui::Box listBox = { 0, 0, 0, 0 };
	ui::Box fieldsBox = { 0, 0, 0, 0 };

	// THE FIELD PANEL AT THE TOP RIGHT (Bishop 2026-11-16, Vasquez's find): inteng prints its own lines at the top LEFT of the view on most screens (Object Placement FUN_004d9af0
	// 0x4dc74e on, from x 16, y 32: View Distance, Num Objects, Difficulty, the object's name, In Object Control, Visible polygons; Path, Waypoint, Hierarchy, Light Sourcing
	// and Strategy Data in the same corner), and the panel covered them. It now stands at the top right of the view, as wide as the left column, where no screen prints; the
	// object list keeps the left column.
	if (listShown)
	{
		listBox = layout.list;
	}

	if (fieldsShown)
	{
		fieldsBox = layout.list;
		fieldsBox.x = layout.view.x + layout.view.width - layout.list.width;
		fieldsBox.height = wanted > 0 && wanted < layout.list.height ? wanted : (wanted > 0 ? layout.list.height : 0);
	}

	if (listBox.height > kListMostRows * rowHeight)
	{
		listBox.height = kListMostRows * rowHeight;
	}

	objectList.LayOut(listBox, rowHeight);
	fieldHost.LayOut(fieldsBox, kPanelPassLineHeight);

	host::Selected const selected = host::Selection_Get();

	if (host::Selection_Changes() != listSawChanges)
	{
		listSawChanges = host::Selection_Changes();

		if (selected.kind == host::SelectedKind::Object)
		{
			objectList.Choose(objectList.FindById(selected.objectId));
		}
	}

	hitList = ui::Box{ 0, 0, 0, 0 };
	hitFields = ui::Box{ 0, 0, 0, 0 };

	// The buttons (A8): the tool row at the top of the view, the action buttons above the status line. Not while a menu or a prompt has the panels
	toolPlaced = 0;
	panelPlaced = 0;

	// NO BUTTONS OF OURS (ruling 10b, Bishop 2026-11-16: inteng has no draw-mode row and no Open / Save copy / Add / Delete buttons: "remove everything our window shows that inteng never had"). The code stays, switched
	// off here, until the tool row's tests are retired.
	bool const kOurButtonsShown = false;

	if (kOurButtonsShown && !AMenuIsOpen() && promptFor == kPromptNone && !textPanelText && boxCount + 2 * host::kToolButtonMost < kPanelPassMostExtras
		&& lineCount + 2 * host::kToolButtonMost < kPanelPassMostExtras)
	{
		int32_t const buttonHeight = host::MenuPaint_ItemHeight(kPanelPassLineHeight);
		size_t const toolCount = host::ToolButtons_Tools(ViewToggles_Now(), ViewToggles_OutlinesCanBeShown(), toolButtons, host::kToolButtonMost);
		size_t const panelCount = host::ToolButtons_Panel(barScreen, host::Selection_Get().kind == host::SelectedKind::Object, panelButtons,
			host::kToolButtonMost);
		ui::Box const toolRoom = { layout.view.x + 4, layout.toolbar.y + layout.toolbar.height + 2, layout.view.width - 8, buttonHeight };
		ui::Box const panelRoom = { 4, layout.status.y - buttonHeight - 2, layout.toolbar.width - 8, buttonHeight };

		toolPlaced = host::ToolButtons_LayOut(toolButtons, toolCount, toolRoom, 4, buttonHeight, PanelPass_MenuWidth, toolBoxes);

		// The row is at the RIGHT end of the view's top, clear of the list and field panels, which open at the left (Apone's run 8: the first buttons covered the
		// field panel's title)
		if (toolPlaced > 0)
		{
			int32_t const shift = (toolRoom.x + toolRoom.width) - (toolBoxes[toolPlaced - 1].x + toolBoxes[toolPlaced - 1].width);

			for (size_t b = 0; b < toolPlaced && shift > 0; b++)
			{
				toolBoxes[b].x += shift;
			}
		}
		panelPlaced = host::ToolButtons_LayOut(panelButtons, panelCount, panelRoom, 4, buttonHeight, PanelPass_MenuWidth, panelBoxes);

		for (int row = 0; row < 2; row++)
		{
			host::ToolButton const *const shown = row == 0 ? toolButtons : panelButtons;
			ui::Box const *const where = row == 0 ? toolBoxes : panelBoxes;
			size_t const placed = row == 0 ? toolPlaced : panelPlaced;

			for (size_t b = 0; b < placed; b++)
			{
				host::PaintBox back = { where[b].x, where[b].y, where[b].width, where[b].height, shown[b].lit ? host::kMenuLitColour : host::kMenuDropColour };
				host::PaintText label;

				if (shown[b].off)
				{
					back.colour = kPanelPassBackground;
				}

				boxes[boxCount++] = back;
				label.text = shown[b].label;
				label.x = where[b].x + host::kPaintMarginAcross;
				label.y = where[b].y + (buttonHeight - kPanelPassLineHeight) / 2;
				lines[lineCount++] = label;
			}
		}
	}

	// While a menu is open it has every box and line to itself: every line is drawn after every box (panel_pass.h), so the list's names would
	// show through its drop-down
	if (!AMenuIsOpen())
	{
		if (listShown && listBox.height > 0 && listBox.width > 0)
		{
			size_t const rowsShown = objectList.Count() < (size_t)(listBox.height / rowHeight) ? objectList.Count() : (size_t)(listBox.height / rowHeight);
			int32_t const backHeight = rowsShown > 0 ? (int32_t)rowsShown * rowHeight : rowHeight;

			hitList = ui::Box{ listBox.x, listBox.y, listBox.width, backHeight < listBox.height ? backHeight : listBox.height };
			boxes[boxCount++] = GrayBehind(hitList.x, hitList.y, hitList.width, hitList.height);
		}

		if (fieldsShown && fieldsBox.height > 0 && fieldsBox.width > 0)
		{
			hitFields = fieldsBox;
			boxes[boxCount++] = GrayBehind(fieldsBox.x, fieldsBox.y, fieldsBox.width, fieldsBox.height);
		}

		if (listShown)
		{
			int32_t const chosen = objectList.Chosen();
			const uint8_t *const chosenId = chosen >= 0 ? objectList.Id((size_t)chosen) : nullptr;
			bool const bandShown = selected.kind == host::SelectedKind::Object && chosenId &&
				memcmp(chosenId, selected.objectId, host::kSelectionIdBytes) == 0;
			size_t listLines = 0;
			size_t listBoxes = 0;

			objectList.Plan(PanelPass_MenuWidth, kPanelPassLineHeight, host::kMenuLitColour, lines + lineCount, kPanelPassMostExtras - lineCount,
				&listLines, boxes + boxCount, bandShown ? kPanelPassMostExtras - boxCount : 0, &listBoxes);
			lineCount += listLines;
			boxCount += listBoxes;
		}

		if (fieldsShown)
		{
			size_t fieldLines = 0;

			fieldHost.Plan(PanelPass_MenuWidth, lines + lineCount, kPanelPassMostExtras - lineCount, &fieldLines);
			lineCount += fieldLines;

			// The harness asked what the panel shows after its select (kHarnessSelectRow): these lines, as drawn, through the safe text
			if (harnessPanelAsked)
			{
				for (std::string const &line : host::HarnessSelect_PanelLines(lines + lineCount - fieldLines, fieldLines))
				{
					printf("%s\n", line.c_str());
				}

				fflush(stdout);
				harnessPanelAsked = false;
			}
		}
	}

	// The status line, on gray as wide as its words
	host::PanelTexts texts;
	host::PaintText statusLines[4];

	// inteng has no status line: the Hierarchy screen draws only its own text (HierarchyTextLines) and the dialogs; the words are still kept (the state line's status) for the harness
	// The hub draws no status words either (FUN_004452b0 draws only the view, the bar and its dialogs: its strings are all prompts): the words are still kept for the harness' state line
	// Neither does MODULE LINKING (module-linking-closeout-vasquez.md ML-T2; Vasquez 11-17): FUN_004c1a90 draws only its own rows (the legend, the picks, the counts, the view distance, the follow) and
	// its link commands say nothing (ML-M-27); the words are kept for the state line
	// THE OWNER'S CALL (11-19, through Bishop; a named difference): inteng draws none on the Waypoint screen or in Strategy's edit view (FUN_005356b0's own four lines); ours draws
	// it there WHILE the words are that screen's mouse tool's (status_words.h StatusSayer), and hides it again at the next words of anything else
	bool const onWaypointScreen = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Waypoint") == 0;
	bool const inStrategyEditView = strategyEditFan && OnStrategyScreen();
	bool const mouseToolSays = StatusWords_ShownWhereIntengHidesIt(statusWords.Sayer(), onWaypointScreen, inStrategyEditView);
	texts.status = (onHierarchy || (OnTheTrackScreen() && trackEd.state != host::TrackState::Off) || (onWaypointScreen && !mouseToolSays) || barScreen == host::kIntengHub || OnModuleLinkingScreen() || textPanelText || polyScreenOpen || chooserActive || (inStrategyEditView && !mouseToolSays)) ? "" : (statusWords ? statusWords : "");
	StateLine_Add("statusdrawn", texts.status);   // the harness's: the words the status line draws this frame ("" none); "status" keeps every word, drawn or not
	hitStatus = ui::Box{ 0, 0, 0, 0 };

	size_t const statusCount = host::PanelPaint_Plan(layout, texts, PanelPass_MenuWidth, kPanelPassLineHeight, statusLines, 4);

	for (size_t i = 0; i < statusCount && lineCount < kPanelPassMostExtras && boxCount < kPanelPassMostExtras; i++)
	{
		int32_t width = 2 * host::kPaintMarginAcross;

		for (size_t c = 0; c < statusLines[i].text.size(); c++)
		{
			width += PanelPass_MenuWidth((unsigned char)statusLines[i].text[c]);
		}

		hitStatus = ui::Box{ statusLines[i].x - host::kPaintMarginAcross, layout.status.y, width, layout.status.height };
		boxes[boxCount++] = GrayBehind(hitStatus.x, hitStatus.y, hitStatus.width, hitStatus.height);
		lines[lineCount++] = statusLines[i];
	}

	// While the harness asked what the panel shows but the panel was closed or hidden, it says so, so a run never waits for lines
	if (harnessPanelAsked && !fieldsShown)
	{
		printf("AVP_OpenInteng: panel lines 0\n");
		fflush(stdout);
		harnessPanelAsked = false;
	}

	bool helpScreenUp = false;   // an inteng Help screen this frame: its lines and box are the last kept (H-1j)
	size_t helpLines = 0;
	size_t helpBoxes = 0;

	// A help text of inteng's: the screen cleared and the text laid out as inteng's text routine lays it out (H-1f, below). The gray box (the lines as they are, a tab to the next
	// multiple of 8 columns) is kept for any other text panel.
	const char *const panelText = textPanelText ? textPanelText : nullptr;

	if (panelText && boxCount < kPanelPassMostExtras)
	{
		std::vector<std::string> shown;
		std::string current;

		for (const char *c = panelText; *c; c++)
		{
			if (*c == '\n')
			{
				shown.push_back(current);
				current.clear();
			}
			else if (*c == '\t')
			{
				current.append(8 - current.size() % 8, ' ');
			}
			else
			{
				current += *c;
			}
		}

		if (!current.empty())
		{
			shown.push_back(current);
		}

		int32_t width = 0;

		for (std::string const &line : shown)
		{
			int32_t w = 2 * host::kPaintMarginAcross;

			for (size_t c = 0; c < line.size(); c++)
			{
				w += PanelPass_MenuWidth((unsigned char)line[c]);
			}

			width = w > width ? w : width;
		}

		// FUN_004724f0 (every Help text of inteng's: the keys of Construction / Module Linking / Strategy / Light / Hierarchy / Texture Map ...): the SCREEN IS CLEARED (FUN_00418b80), the text is written from (0, 0) over the whole
		// screen (FUN_0057be40), "Click to continue." (005d3ed8) is written at x 200, height - 0x14, and it waits for a mouse button. The chooser's lines (Shape Chooser) are not that screen and keep the gray box.
		int32_t const lineHeight = kPanelPassLineHeight + 2;
		int32_t const rows = (int32_t)(shown.size() < kPanelPassMostExtras - lineCount - 1 ? shown.size() : kPanelPassMostExtras - lineCount - 1);

		if (textPanelText)
		{
			int32_t const screenW = layout.toolbar.width > 0 ? layout.toolbar.width : 1008;
			int32_t const screenH = layout.status.y + layout.status.height > 0 ? layout.status.y + layout.status.height : 729;

			hitPrompt = ui::Box{ 0, 0, screenW, screenH };
			lineCount = 0;   // the screen is cleared: nothing else of ours is on it
			boxCount = 0;
			helpScreenUp = true;   // and nothing planned after it is drawn either (H-1j, at the end)
			boxes[boxCount++] = host::PaintBox{ 0, 0, screenW, screenH, 0xFF000000u };

			// laid out as FUN_0057be40 lays it out from (0, 0) (H-1f): a new line the letters' height down, a tab to the next multiple of 4 x the height, wrapped at the screen's edge, a line
			// below its bottom left out; one piece a run of letters between two jumps, all but the last place kept for "Click to continue."
			int32_t const letterHeight = PanelPass_MenuFontHeight() > 0 ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
			std::vector<host::IntengTextPiece> const pieces = host::IntengText_Layout(textPanelText, 0, 0, screenW, screenH, PanelPass_MenuWidth, letterHeight, kPanelPassMostExtras - 1);

			for (host::IntengTextPiece const &piece : pieces)
			{
				host::PaintText line;

				line.text = piece.text;
				line.x = piece.x;
				line.y = piece.y;
				lines[lineCount++] = line;
			}

			host::PaintText click;

			click.text = "Click to continue.";
			click.x = 200;
			click.y = screenH - 0x14;
			lines[lineCount++] = click;
			helpLines = lineCount;
			helpBoxes = boxCount;
		}
		else
		{
			int32_t const left = layout.view.width > width ? (layout.view.width - width) / 2 : 0;
			int32_t const top = layout.toolbar.y + layout.toolbar.height + 24;

			hitPrompt = ui::Box{ left, top, width, rows * lineHeight + 6 };
			boxes[boxCount++] = GrayBehind(hitPrompt.x, hitPrompt.y, hitPrompt.width, hitPrompt.height);

			for (int32_t i = 0; i < rows; i++)
			{
				host::PaintText line;

				line.text = shown[(size_t)i];
				line.x = left + host::kPaintMarginAcross + (left == 0 ? 1 : 0);
				line.y = top + 3 + i * lineHeight;
				lines[lineCount++] = line;
			}
		}
	}

	// Hierarchy's Return (C-47): "Saving" at (200, 100) (FUN_0057bd40(200, 100, 0, "Saving") 0x488a3f-0x488a4d) on the frame drawn before the save; the save itself runs at the start of the next
	// frame (HierarchySaveIfAsked), as inteng's save follows the drawn words
	if (onHierarchy && hierarchySaveAsked == 2 && lineCount < kPanelPassMostExtras)
	{
		host::PaintText saving;

		saving.text = "Saving";
		saving.x = 200;
		saving.y = layout.view.y + 100;
		lines[lineCount++] = saving;
		hierarchySaveAsked = 1;
	}

	// COPY STRATEGY'S COLLECTOR and the Strategy messages (above)
	if (OnStrategyScreen())
	{
		int32_t base = layout.toolbar.y + layout.toolbar.height;

		for (ui::Box const &lower : hitLower)
		{
			base = lower.height > 0 && lower.y + lower.height > base ? lower.y + lower.height : base;
		}

		textAreaTop = base;

		if (!strategyMessage.empty() && GetTickCount64() - strategyMessageAt > 700)
		{
			strategyMessage.clear();
		}

		if (!strategyMessage.empty() && lineCount < kPanelPassMostExtras)
		{
			// the words, a line under a line at 0x14 (FUN_0057bd40(200, 100) then (200, 0x74): "Saving" and the file's name)
			size_t at = 0;
			int32_t y = layout.view.y + 100;   // FUN_0057bd40(200, 100): from the window's top

			while (at <= strategyMessage.size() && lineCount < kPanelPassMostExtras)
			{
				size_t const newline = strategyMessage.find('\n', at);
				host::PaintText message;

				message.text = strategyMessage.substr(at, newline == std::string::npos ? std::string::npos : newline - at);
				message.x = 200;
				message.y = y;
				lines[lineCount++] = message;
				y += 0x14;

				if (newline == std::string::npos)
				{
					break;
				}

				at = newline + 1;
			}
		}

		if (volume.on && lineCount + 6 <= kPanelPassMostExtras)
		{
			char const *const rows[6] = { "S : Set volume to module extents", "X : Expand volume to include module", "Shift+Left Click : Select face of cube", "+/- : Move selected face", "Hold shift to move face faster", "Press return when finished" };   // 0x5ee12c, 0x5ee108, 0x5ee0e0, 0x5ee0c4, 0x5ee0a4, 0x5edfe4

			for (int row = 0; row < 6; row++)
			{
				host::PaintText line;

				line.text = rows[row];
				line.x = 16;
				line.y = layout.view.y + 16 * (3 + row);   // rows 3-8 (y 48 ..) from the window's top (strategy-data.md S-VOL-3)
				lines[lineCount++] = line;
			}

			// the caller's words along the foot (200, height - 0x14) on the second text list DAT_00914964: Area Switch FUN_00530900 0x53090d "Select trigger volume" (0x5e41b4), Volume of
			// Death FUN_00535970 0x535b39 "Edit Volume of Death" (0x5efe18)
			if (lineCount < kPanelPassMostExtras && (volume.kind == 68 || volume.kind == 77))
			{
				host::PaintText foot;

				foot.text = volume.kind == 68 ? "Select trigger volume" : "Edit Volume of Death";
				foot.x = 200;
				foot.y = layout.view.y + layout.view.height - 20;
				lines[lineCount++] = foot;
			}
		}

		if (stabilityActive && lineCount + 4 <= kPanelPassMostExtras)
		{
			char const *const rows[4] = { "Red : stable", "Green : unstable", "T : toggle polyline on/off", "Press return when finished" };   // 0x5ee054, 0x5ee040, 0x5ee024, 0x5edfe4

			for (int row = 0; row < 4; row++)
			{
				host::PaintText line;

				line.text = rows[row];
				line.x = 16;
				line.y = layout.view.y + 16 * (3 + row);   // rows 3-6 (y 48 ..) from the window's top
				lines[lineCount++] = line;
			}
		}

		// "View Distance :%d" (0x5c39b0, the distance / 500000) at (16, 0x20): FUN_00522980 0x5229e0 and FUN_00529370 0x52944b draw it every frame, on page 0 and in every loop of the screen (the flag page, Stability,
		// the volume editor, the collectors, the object pick); a kind's Edit view does not (strategy-data.md S-65). The volume editor and a collector run from an Edit view, with strategyEditFan still
		// set, and their loops (FUN_005277c0, FUN_00526bf0) draw the row through FUN_00529370 (S-VOL-3, Drake 11-17; a view's collector, S-57, Bishop 11-18: it was skipped there). A view's object pick
		// (FUN_00527540) draws it among the pick's own lines below (the view's font, as its other lines), so not here too. The state line's screentext has it
		bool const viewPickDrawsIt = strategyEditFan && !volume.on && !copyCollecting && !strategyPickField.empty();

		if ((!strategyEditFan || StrategyInASubLoop()) && !viewPickDrawsIt && lineCount < kPanelPassMostExtras)
		{
			static char distanceWords[48];
			host::PaintText distance;

			_snprintf_s(distanceWords, sizeof(distanceWords), _TRUNCATE, "View Distance :%d", (int)(viewDistance / 500000));
			distance.text = distanceWords;
			distance.x = 16;
			distance.y = layout.view.y + 0x20;
			lines[lineCount++] = distance;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + std::string(distanceWords);
		}

		// THE FLAG PAGE'S TEXT (FUN_00525700 0x526245-0x5266c5, strategy-data.md S-73), rows of 16 from row 3 (y 48 from the window's top): no flag viewed: "Flag View - None" (0x5ede30); a flag viewed:
		// "Flag View - Airduct" (0x5ede80) / "Flag View - Stair" (0x5ede6c) / "Flag View - Sky Visible" (0x5ededc) / "Flag View - Fogged" (0x5ede94), then "F : Add flag" (0x5ede5c) and "Shift + F : Remove flag" (0x5ede44).
		// With modules in the page's list, the one added LAST: one row left empty, "Flags for %s :" (0x5ede20, its name), then a row for each flag it has: "Airduct" (0x5edc38), "Stair" (0x5edc30),
		// "Sky Visible" (0x5edc24), "Fogged" (0x5edc1c) (the bits 0x80000000, 0x40000000, 0x20000000, 0x10000000 of its MODFLAGS word, in that order)
		if (strategyFlagPage && !strategyEditFan && lineCount + 10 <= kPanelPassMostExtras)
		{
			int32_t row = 3;
			auto put = [&](std::string const &words)
			{
				host::PaintText one;

				one.text = words;
				one.x = 16;
				one.y = layout.view.y + 16 * row;
				lines[lineCount++] = one;
				row++;
			};

			char const *viewed = nullptr;

			switch (strategyViewBit)
			{
			case 0x80000000u: viewed = "Flag View - Airduct"; break;
			case 0x40000000u: viewed = "Flag View - Stair"; break;
			case 0x20000000u: viewed = "Flag View - Sky Visible"; break;
			case 0x10000000u: viewed = "Flag View - Fogged"; break;
			default: break;
			}

			if (!viewed)
			{
				put("Flag View - None");
			}
			else
			{
				put(viewed);
				put("F : Add flag");
				put("Shift + F : Remove flag");
			}

			size_t const listed = host::Selection_SetCount();
			uint8_t lastId[8];
			OpenLevelFile *const flagOpen = LevelObjects_OpenForEdit();

			if (listed > 0 && flagOpen && host::Selection_SetMember(listed - 1, lastId))
			{
				host::FoundObject lastFound;
				std::string const lastName = host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(lastId).c_str(), &lastFound) ? lastFound.name : std::string();
				uint32_t flags = 0;
				Chunk *flagChunk = nullptr;
				std::vector<unsigned char> flagBytes;
				host::LevelStore flagStore(flagOpen);

				if (host::FindModuleFlags(LevelObjects_Level(), lastId, &flagChunk) == host::LevelChunkFound::Found && flagChunk && flagStore.PayloadOf(flagChunk, &flagBytes) && flagBytes.size() >= 4)
				{
					memcpy(&flags, flagBytes.data(), 4);
				}

				row++;   // 0x526539: the row counter goes up by two, the words on the second
				put("Flags for " + lastName + " :");

				if (flags & 0x80000000u) put("Airduct");
				if (flags & 0x40000000u) put("Stair");
				if (flags & 0x20000000u) put("Sky Visible");
				if (flags & 0x10000000u) put("Fogged");
			}
		}

		// THE CURRENT OBJECT'S TWO LINES (FUN_00524e90 0x52525a-0x5252bf, drawn every frame the screen's own loop runs while an object is current): its wrapper's name (method +4) and "ID - 0x%x %x" (005edddc, the two words
		// of its id) on the rows 3 and 4 (FUN_00522980 sets the row counter to 3 at 0x5229d4 before them): y 48 and 64 from the window's top (strategy-data.md S-66)
		// BAR-8: not while inteng's menu loop is up: FUN_00524e90 calls its command (0x524eab) before these lines; "View Distance" above is drawn before it (FUN_00522980 0x5229e0) and stays
		if (!strategyFlagPage && !copyCollecting && !stabilityActive && !volume.on && !strategyEditFan && !fieldHost.Showing() && !MenuLoopUp() && lineCount + 2 <= kPanelPassMostExtras)
		{
			uint8_t currentId[8];
			host::FoundObject currentFound;

			// the wrapper's name (+4), as the views name an object (StrategyViewNameOf: the object list, a virtual object, a generator or enemy, a placed hierarchy, a sound or a
			// particle generator). Until 2026-11-17 this looked in the object list and the virtual objects only, so a current generator had no lines.
			bool const haveCurrent = host::Selection_Get().kind == host::SelectedKind::Object && SelectedObjectId(currentId) && StrategyViewNameOf(currentId, &currentFound.name);

			if (haveCurrent)
			{
				static char idWords[64];
				uint32_t lowWord, highWord;

				memcpy(&lowWord, currentId, 4);
				memcpy(&highWord, currentId + 4, 4);
				_snprintf_s(idWords, sizeof(idWords), _TRUNCATE, "ID - 0x%x %x", (unsigned)lowWord, (unsigned)highWord);

				host::PaintText nameLine, idLine;

				nameLine.text = currentFound.name;
				nameLine.x = 16;
				nameLine.y = layout.view.y + 3 * 16;
				lines[lineCount++] = nameLine;
				idLine.text = idWords;
				idLine.x = 16;
				idLine.y = layout.view.y + 4 * 16;
				lines[lineCount++] = idLine;
				screenTextNow += (screenTextNow.empty() ? "" : "|") + nameLine.text + "|" + idLine.text;   // the state line's screentext: a test reads which object is current
			}
		}

		if (copyCollecting && lineCount + 2 <= kPanelPassMostExtras)
		{
			host::PaintText head, foot;

			head.text = "Press return when finished";   // 0x5edfe4
			head.x = 16;
			head.y = layout.view.y + 16 * 3;   // row 3 (y 48) from the window's top (strategy-data.md S-79)
			lines[lineCount++] = head;
			foot.text = copyPurpose == 1 ? "Select other positions for this lift" : (copyPurpose == 2 ? "Select mission objectives" : (copyPurpose == 3 ? "Select targets" : (copyPurpose == 4 ? "Select targets for switch" : "Select objects to copy to")));   // 0x5ee880, 0x5ef88c, 0x5efb5c, 0x5ee1ec
			foot.x = 200;
			foot.y = layout.view.y + layout.view.height - 20;
			lines[lineCount++] = foot;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + head.text;   // the state line's screentext: a test reads the list as the screen shows it

			for (size_t i = 0; i < copyTargets.size() && lineCount < kPanelPassMostExtras; i++)
			{
				host::PaintText name;

				// each member by its wrapper's name (FUN_00526bf0 0x526cc1), as the views name it; "?" only for an ID nothing names. Until 2026-11-17 the object list only: a generator or a
				// sound was "?"
				if (!StrategyViewNameOf(copyTargets[i].id, &name.text))
				{
					name.text = "?";
				}

				name.x = 16;
				name.y = layout.view.y + 16 * (5 + (int32_t)i);   // from row 5 (y 80)
				lines[lineCount++] = name;
				screenTextNow += "|" + name.text;
			}
		}
	}

	// THE EDIT VIEW'S DRAWN LINES (strategy_views.h): in the text area, each at inteng's x and y
	if (!strategyEditFan || copyCollecting)
	{
		strategyViewWord.clear();
	}

	// [S-KC-1] the view's lines (FUN_0057bd90(x, y, 1, ...): x, y from the window's top left)
	strategyViewBox[0] = strategyViewBox[1] = strategyViewBox[2] = strategyViewBox[3] = 0;
	strategyViewWord.clear();   // this frame's view lines only: no view drawn (the volume editor, a collector, the page), no lines on the state line (Bishop 11-18: it kept the last view's)

	if (strategyEditFan && OnStrategyScreen() && !volume.on && !copyCollecting)
	{
		std::vector<uint8_t> payload;
		std::string name;
		int32_t kind = 0;
		uint8_t id[8];

		if (StrategyViewSubject(&payload, &name, &kind, id) && kind == strategyEditKind)
		{
			std::vector<host::StrategyViewLine> shown;

			strategyViewWord.clear();

			// while an object pick runs the screen's own frame is drawn (FUN_00529370): "View Distance :%d" (16, 32), "Press return when finished" (16, 48, 0x5edfe4), the picked object's name (16, 80),
			// the row's words along the foot (200, height - 20); not the view's lines
			bool const picking = !strategyPickField.empty();

			// THE VIEW'S BOX (an OWNER'S ADDITION, 11-17, NAMED: inteng's views fill the whole screen black, FUN_00418b80 on every pass, S-KC-1): a view, its pages and E's pages are drawn in a box
			// centred in the window, half its width and half its height but never under inteng's own 640 x 480 (so every line sits where inteng laid it out), with a white frame and no title band
			// (inteng's views have none: the title is the (200, 0) line), and the level drawn round it. Its lines, its foot and its clicks are measured from the box's top left. The object pick
			// stays the whole screen (its clicks are on the level, S-KC-5): its lines from the window's top left, as before. (Before this, the view's lines were from the window's top: the bar,
			// when shown, covered 0..32 over them as in inteng.)
			ui::Box const box = picking ? layout.view : StrategyViewBoxIn(layout.view);

			strategyViewLeft = box.x;
			strategyViewTop = box.y;

			if (!picking)
			{
				strategyViewBox[0] = box.x;
				strategyViewBox[1] = box.y;
				strategyViewBox[2] = box.width;
				strategyViewBox[3] = box.height;
			}
			// the Lift's lines read the level: an object's radius (FUN_00529850) and whether a position's Lift starts there (FUN_0052cb10)
			host::StrategyViewWorld world;

			world.radius = [](uint8_t const *one, double *radius) { return host::ObjectEdit_ShapeRadius(LevelObjects_Level(), one, radius); };
			world.liftStartsHere = [](uint8_t const *one)
			{
				int32_t oneKind = 0, flags = 0;
				std::vector<uint8_t> bytes;

				return StrategyOfObject(one, &oneKind, &bytes) && oneKind == 51 && host::StrategyView_FieldNumber(51, bytes, "LiftFlags", &flags) && (flags & 1) != 0;
			};

			world.targetWords = StrategyTargetWords;
			world.trackSections = TrackSectionsOf;
			world.species = EnemySpeciesOf;
			world.path = LevelPathNamed;
			world.page = strategyEditKind == 66 || strategyEditKind == 68 ? multiPage : trackPage;
			world.links = &strategyViewLinks;
			world.strategyKind = [](uint8_t const *one)
			{
				int32_t oneKind = 0;
				std::vector<uint8_t> bytes;

				return StrategyOfObject(one, &oneKind, &bytes) ? oneKind : 0;
			};

			bool haveLines = false;

			if (requestPageOn && !picking)
			{
				host::StrategyView_RequestLines(&requestPage, box.height, LevelPathNamed, &shown);   // E's page: its own black screen (S-KC-8), in the box
				haveLines = true;
			}
			else
			{
				haveLines = picking || host::StrategyView_Lines(kind, payload, name, box.height, StrategyViewNameOf, id, &shown, &world);
			}

			if (picking)
			{
				host::StrategyViewLine one;

				one.x = 16;
				one.y = 32;
				one.text = "View Distance :" + std::to_string((long long)(viewDistance / 500000));   // 0x5c39b0
				shown.push_back(one);
				one.y = 48;
				one.text = "Press return when finished";   // 0x5edfe4
				shown.push_back(one);

				std::string pickedName;

				if (strategyPickHave && StrategyViewNameOf(strategyPickId, &pickedName))
				{
					one.y = 80;
					one.text = pickedName;
					shown.push_back(one);
				}

				one.x = 200;
				one.y = layout.view.height - 20;   // the pick is the whole screen's: its foot is the window's
				one.text = strategyViewPickWords;
				shown.push_back(one);
			}

			// the box: a white frame one pixel wide, black inside (as the dialogs' windows, dialogs.cpp window(), without their red title band)
			if (!picking && boxCount + 2 <= kPanelPassMostExtras)
			{
				boxes[boxCount++] = host::PaintBox{ box.x, box.y, box.width, box.height, 0xFFFFFFFFu };
				boxes[boxCount++] = host::PaintBox{ box.x + 1, box.y + 1, box.width - 2, box.height - 2, 0xFF000000u };
			}

			if (haveLines)
			{

				for (host::StrategyViewLine const &one : shown)
				{
					std::string safe;

					// the state line's words: a font code below 32 (a flag's tick 1 / X 3, the chosen line's triangle 2) as "{n}"
					for (char const letter : one.text)
					{
						if ((unsigned char)letter < 32 && letter != '\n')
						{
							safe += "{" + std::to_string((int)(unsigned char)letter) + "}";
						}
						else
						{
							safe += (letter == '"' || letter == '|' || letter == '\n') ? '?' : letter;
						}
					}

					strategyViewWord += (strategyViewWord.empty() ? "" : "|") + std::to_string((long long)one.x) + "@" + std::to_string((long long)one.y) + ":" + safe;
				}

				bool const intengFont = PanelPass_HasMenuFont();

				for (host::StrategyViewLine const &one : shown)
				{
					if (lineCount >= kPanelPassMostExtras)
					{
						break;
					}

					host::PaintText line;

					// inteng draws a view's words in its text font, Tools\font16.fnt (FUN_0057be40; W-2), the flags as its glyphs 1 (a tick) and 3 (an X) and the chosen line's mark as glyph 2 (a
					// triangle): so here, in that font when it was read. NAMED: without it, the game's font and letters for the three marks ("v", ">", "x")
					line.menuFont = true;
					line.text = one.text;

					if (!intengFont)
					{
						for (char &letter : line.text)
						{
							letter = letter == '\x01' ? 'v' : letter == '\x02' ? '>' : letter == '\x03' ? 'x' : letter;
						}
					}

					line.x = box.x + one.x;
					line.y = box.y + one.y;

					// a line whose letters would pass the box's bottom is cut, as inteng's 640 x 480 screen cut it and as a line with a tab stops there (IntengText_Layout); NAMED: a line past the
					// box's right edge is not cut, as no letter is (a long name)
					if (!picking && one.y + PanelPass_MenuFontHeight() > box.height)
					{
						continue;
					}

					if (intengFont && line.text.find('\t') != std::string::npos)
					{
						// a tab in a view's line ("D : Duplicate target\t Shift D : Remove target", 0x5ef100): FUN_0057be40 puts what follows at the next multiple of 4 x 16 from the screen's left (H-1f);
						// laid out in the box's own pixels, so the stops are from the box's left as inteng's are from its screen's, and the line is wrapped at the box's edge
						std::vector<host::IntengTextPiece> const pieces = host::IntengText_Layout(line.text, one.x, one.y, box.width, box.height, PanelPass_MenuWidth, PanelPass_MenuFontHeight(), kPanelPassMostExtras - lineCount);

						for (host::IntengTextPiece const &piece : pieces)
						{
							host::PaintText part = line;

							part.text = piece.text;
							part.x = box.x + piece.x;
							part.y = box.y + piece.y;
							lines[lineCount++] = part;
						}

						continue;
					}
					lines[lineCount++] = line;
				}
			}
		}
	}

	// the view's box on the state line (the owner's box): x y width height in the window's client pixels, empty when none is up or the pick runs; a scenario's click-box clicks from it
	StateLine_Add("strategybox", strategyViewBox[2] > 0 ? std::to_string(strategyViewBox[0]) + " " + std::to_string(strategyViewBox[1]) + " " + std::to_string(strategyViewBox[2]) + " " +
		std::to_string(strategyViewBox[3]) : std::string());

	// THE SELECTED LIGHT'S TEXT BLOCK (FUN_004ac430, called by FUN_004acf30 every frame; layout of the 16-pixel text lines, DAT_00915210 == 0x10; strings 0x5e2348..0x5e2204, read from inteng.exe's bytes): two columns at x 10 and 200, rows from 0x42 down in steps of 16
	// Brightness / Spread, Range / Shadows, Atten / Spread Atten, Omni / Runtime, Red / Green, Blue / No PreLight, Specular, then "Light saved - no. %d" (or Up / Down colour for a placed-object light), then "Light Turned Off". The attribute < and > chose (DAT_00703e58) shows its VALUE ONLY on every third frame (DAT_00703e44 % 3 == 0): it flashes.
	// The other layout (DAT_00915210 != 0x10: 8-pixel lines, rows from 0x32 with "Bright:" and the columns at x 0xa0) is the one for a small text area (inteng's 320-pixel modes); the editor's text area is never that small, so only this one is made.
	if (OnALightScreen() && !textPanelText && !MenuLoopUp())   // Light Sourcing's command step (0x4aab7f) comes before its text block (FUN_004acf30 0x4aafe3): none while a menu is up (BAR-8)
	{
		size_t light = 0;
		host::LightData data;
		uint8_t placedId[8];
		std::vector<unsigned char> placedBytes;
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		LightStateOfThisLevel();

		bool const ordinary = SelectedLight(&light) && host::CopyLight(open, light, &data) == host::LightEditResult::Done && data.size() >= 76;
		Chunk *const placedChunk = (!ordinary && open && open->Level() && SelectedPlacedLight(placedId)) ? host::FindPlacedLightChunk(open->Level(), placedId) : nullptr;
		bool const placed = placedChunk && open->PayloadOf(placedChunk, &placedBytes) && placedBytes.size() >= 28;

		if (ordinary || placed)
		{
			static uint32_t frames = 0;

			frames++;

			// the light's numbers in the file's own places: an ordinary light's STDLIGHT, or a placed-object light's PLOBJLIT (brightness 0, spread 4, range 8, up colour 12, down colour 16, engine flags 20, local flags 24)
			auto const at = [&](size_t offset) { int32_t value = 0; memcpy(&value, (ordinary ? data.data() : placedBytes.data()) + offset, 4); return value; };
			int32_t const bright = ordinary ? at(52) : at(0), spread = ordinary ? at(56) : at(4), range = ordinary ? at(60) : at(8);
			int32_t const colour = ordinary ? at(64) : at(PlacedColourDown(placedId) ? 16 : 12);
			uint32_t const engine = (uint32_t)(ordinary ? at(68) : at(20)), local = (uint32_t)(ordinary ? at(72) : at(24));
			bool const flash = frames % 3 == 0;
			int32_t const base = layout.toolbar.y + layout.toolbar.height;
			char const *const yes = "y", *const no = "n";

			auto const put = [&](int attribute, int32_t x, int32_t y, char const *full, char const *only, double number, char const *word)
			{
				char one[96];
				char const *const format = (lightAttribute == attribute && flash) ? only : full;

				if (word)
				{
					_snprintf_s(one, sizeof(one), _TRUNCATE, format, word);
				}
				else if (strstr(format, "%d"))
				{
					_snprintf_s(one, sizeof(one), _TRUNCATE, format, (int)number);
				}
				else
				{
					_snprintf_s(one, sizeof(one), _TRUNCATE, format, number);
				}

				std::string expanded;

				for (char const *c = one; *c; c++)
				{
					if (*c == '\t')
					{
						expanded.append(8 - expanded.size() % 8, ' ');
					}
					else if (*c != '\n')
					{
						expanded += *c;
					}
				}

				if (lineCount < kPanelPassMostExtras)
				{
					host::PaintText line;

					line.text = expanded;
					line.x = x;
					line.y = base + y;
					lines[lineCount++] = line;
				}
			};

			// the numbers: brightness x 2^-16 x 100 per cent, spread x 0.087890625 degrees, range x 1e-5 m (0x5ac120, 0x5ac0f8, 0x5adb68, 0x5adb64); the engine range is the file's x 100 x the level's scale (FUN_004ae5d0, DAT_00607bd8)
			put(0, 10, 0x42, "Brightness:\t%-6.2f%%", "\t\t%-6.2f%%", (double)bright * (1.0 / 65536.0) * 100.0, nullptr);
			put(1, 200, 0x42, "Spread:\t\t%-6.2f deg\n", "\t\t%-6.2f deg\n", (double)spread * 0.087890625, nullptr);
			put(2, 10, 0x52, "Range:\t\t%-6.2fm", "\t\t%-6.2fm", (double)((float)((double)range * 100.0 * (local_scale > 0.0 ? local_scale : 1.0)) * 1.0e-5f), nullptr);
			put(3, 200, 0x52, "Shadows:\t%s\n", "\t\t%s\n", 0, (local & 2) ? yes : no);
			put(4, 10, 0x62, "Atten:\t\t%s ", "\t\t%s ", 0, (engine & 1) ? yes : no);
			put(5, 200, 0x62, "Spread Atten:\t%s\n", "\t\t%s\n", 0, (engine & 2) ? yes : no);
			put(6, 10, 0x72, "Omni:\t\t%s\n", "\t\t%s\n", 0, (engine & 4) ? yes : no);
			put(7, 200, 0x72, "Runtime:\t%s\n", "\t\t%s\n", 0, (local & 0x10) ? yes : no);
			put(8, 10, 0x82, "Red:\t\t%d\n", "\t\t%d\n", (double)(colour >> 16), nullptr);
			put(9, 200, 0x82, "Green:\t\t%d\n", "\t\t%d\n", (double)((colour >> 8) & 0xff), nullptr);
			// inteng prints the whole colour word when Blue is the flashing attribute (the AND 0xff is only in the plain line, 0x4ac87a-0x4ac883)
			put(10, 10, 0x92, "Blue:\t\t%d\n", "\t\t%d\n", (double)((lightAttribute == 10 && flash) ? colour : (colour & 0xff)), nullptr);
			put(11, 200, 0x92, "No PreLight:\t%s\n", "\t\t\t%s\n", 0, (local & 0x40) ? yes : no);
			// Specular: the answer is the other way round (the bit CLEAR says y, 0x4ac92c-0x4ac95f)
			put(12, 10, 0xa2, "Specular:\t%s\n", "\t\t%s\n", 0, (engine & 0x800) ? no : yes);

			if (ordinary)
			{
				put(-1, 10, 0xb2, "Light saved - no. %d\n", "Light saved - no. %d\n", (double)at(0), nullptr);
			}
			else
			{
				// a placed-object light says which colour the colour keys work on (0x4ac9a7-0x4ac9dc: "Up colour" 0x5e2218 / "Down colour" 0x5e2224)
				put(-1, 10, 0xb2, PlacedColourDown(placedId) ? "Down colour" : "Up colour", PlacedColourDown(placedId) ? "Down colour" : "Up colour", 0, nullptr);
			}

			if (ordinary && !lightState.IsOn(light) && lineCount < kPanelPassMostExtras)
			{
				host::PaintText off;

				off.text = "Light Turned Off";
				off.x = 10;
				off.y = base + 0xc2;
				lines[lineCount++] = off;
			}
		}
	}

	// TEXTURE MAP'S MERGE (sub-mode 2, FUN_005511f0 0x551826-0x55194e, FUN_0057b1b0 at x 0x10): the picked polygons' "%d	verts: %d,%d,%d" at 0x20 / 0x30 and the shape's name at 0x40
	// (texture_submodes.cpp TextureMergeLines); the state line's screentext has them
	if (onTextureMap && textureSubMode == 2 && !textPanelText)
	{
		std::vector<std::pair<int32_t, std::string>> mergeLines;
		int32_t const base = layout.toolbar.y + layout.toolbar.height;

		TextureMergeLines(&mergeLines);

		for (std::pair<int32_t, std::string> const &one : mergeLines)
		{
			if (lineCount < kPanelPassMostExtras)
			{
				host::PaintText line;

				line.text = one.second;
				line.x = 0x10;
				line.y = base + one.first;
				lines[lineCount++] = line;
				screenTextNow += (screenTextNow.empty() ? "" : "|") + one.second;
			}
		}
	}

	// PATH MODE'S DRAWN TEXT (FUN_004f9d10 0x4f9f70 on, FUN_0057b1b0 at x 0, rows of 16 from row 2): "View Distance :%d" (the distance in 500000s), "Path :%d  %s" (the working path) when there is one, a line "%s" with the module's name for each node the person put in (not the ones the route added), and the picked module's name
	pathLinesNow.clear();

	// BAR-8: not while inteng's menu loop is up: FUN_004f9d10 calls the command (0x4f9d38) before this text (0x4f9f80-0x4fa0e1)
	if (OnThePathScreen() && !textPanelText && !MenuLoopUp() && lineCount + 4 <= kPanelPassMostExtras)
	{
		int32_t const base = layout.toolbar.y + layout.toolbar.height;

		int32_t row = 2;
		auto const say = [&](std::string const &words)
		{
			if (lineCount < kPanelPassMostExtras)
			{
				host::PaintText line;

				line.text = words;
				line.x = 0;   // inteng's x 0 (FUN_0057b1b0(font, 0, row * 16, ...)): the window's left edge
				line.y = base + 16 * row;
				lines[lineCount++] = line;
				pathLinesNow += (pathLinesNow.empty() ? "" : "|") + words;
			}

			row++;
		};

		say("View Distance :" + std::to_string(viewDistance / 500000));

		if (pathNumber >= 0)
		{
			say("Path :" + std::to_string(pathNumber) + "  " + pathName);
		}

		for (host::PathNode const &node : pathNodes)
		{
			if ((node.flags & 1) == 0)
			{
				say(ModuleNameOfIndex(node.module));
			}
		}

		std::vector<uint8_t> const pickedIds = PickedModuleIdBytes();
		int32_t pickedNumber = 0;

		if (!pickedIds.empty() && host::ModuleIndexOfId(LevelObjects_Level(), pickedIds.data(), &pickedNumber))
		{
			say(ModuleNameOfIndex(pickedNumber));
		}
	}

	// LIGHT SOURCING'S TWO DRAWN LINES (FUN_004aab60 0x4aaff8 and 0x4ab0f0, FUN_0057b1b0 into the screen's text area): "View Distance: %d" (the distance in 500000s) at (10, 0x22) and "Ambience: %d%%" on the line under it
	if (OnALightScreen() && !textPanelText && lineCount + 2 <= kPanelPassMostExtras)
	{
		int ambience = 0;
		int32_t const lineHeight = kPanelPassLineHeight + 2;
		int32_t const top = layout.toolbar.y + layout.toolbar.height + 0x22;
		host::PaintText distance, shown;

		distance.text = "View Distance: " + std::to_string(viewDistance / 500000);
		distance.x = 10;
		distance.y = top;
		lines[lineCount++] = distance;

		if (host::Ambience_Percent(LevelObjects_OpenForEdit(), &ambience) == host::LightEditResult::Done)
		{
			shown.text = "Ambience: " + std::to_string(ambience) + "%";
			shown.x = 10;
			shown.y = top + lineHeight;
			lines[lineCount++] = shown;
		}
	}

	// EVERY SCREEN'S TOP-LEFT TEXT (view-state-table.md H-1: the main loop 0x58cfe3 -> FUN_00471b60 0x471b8f / 0x471bcb, FUN_0057b1b0 at inteng's x 10, white): "Frame Rate = %d" at
	// y 0 and "Speed   :%d" (the camera speed / 3, DAT_005f071c / 3) at y 0x10. NAMED: the frame rate is this program's frames in the last second (inteng's DAT_00d88070 is its own
	// count). NOT YET: "(On Grid)" (DAT_00914b14), the cencon WARNING (DAT_00636f48) and the Recording line (DAT_00636f78): ours has none of those states.
	// THE HUB'S TEXT (H-2, FUN_004452b0 0x447104-0x447138, no object selected): "View Distance :%d" (DAT_005fd678 / 500000) at (10, 0x20). NOT YET: "Polygon count : %d / %d" at
	// (10, 0x32) (FUN_00472720: the polygons of each shape the level places, then those times its uses) and the selected objects' lines (name, radius, flags, Location).
	// FOR NOW ON THE HUB ONLY: inteng's rows 0 and 1 lie in the bar's band, where our text comes out as magenta boxes (fixed with the bar row H-5); drawn under the band they would sit
	// on the other screens' own rows 2 and 3 (Object Placement, Path, Waypoint, Hierarchy: FUN_004d9af0 0x4dc74e "View Distance" at y 0x20), so until then only the hub, which
	// has nothing on those rows, shows them. (screentext was cleared at the top of this function.)

	// While a menu is up, neither: FUN_00471b60 runs after the mode's frame (0x58cfe3) and the hub draws its text after its command step (0x4452c5 before 0x447104), so no picture shown
	// from inside the menu loop has them (BAR-6, BAR-8)
	if (LevelObjects_Level() && barScreen == host::kIntengHub && !textPanelText && !MenuLoopUp() && lineCount + 3 <= kPanelPassMostExtras)
	{
		static DWORD rateFrom = GetTickCount();
		static int rateFrames = 0;
		static int rateShown = 0;
		DWORD const now = GetTickCount();
		// inteng's y 0 is the top of its screen. NAMED, to fix with the bar row (H-5): ours starts under the bar's band, as the other screens' text does; text drawn in that band
		// comes out as solid magenta boxes today (the band is the bar's own area in the panels)
		int32_t const base = layout.toolbar.y + layout.toolbar.height;

		rateFrames++;

		if (now - rateFrom >= 1000)
		{
			rateShown = rateFrames;
			rateFrames = 0;
			rateFrom = now;
		}

		auto const sayAt = [&](int32_t y, std::string const &words)
		{
			host::PaintText line;

			line.text = words;
			line.x = 10;
			line.y = base + y;
			lines[lineCount++] = line;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + words;
		};

		sayAt(0, "Frame Rate = " + std::to_string(rateShown));
		sayAt(0x10, "Speed   :" + std::to_string(speedUnits / 3));

		if (barScreen == host::kIntengHub)
		{
			sayAt(0x20, "View Distance :" + std::to_string(viewDistance / 500000));

			// THE HUB'S OWN LINES (TB-C7, hub_text.h): "Polygon count : %d / %d" at 0x32 and each selected object's name / radius line and Location line. The counts are FUN_00472720's, made
			// at the hub's start (0x4475ed): here whenever the level's bytes are made again (NAMED: inteng keeps the start's numbers until the hub starts again; every hub command
			// that changes a shape's polygons, Merge Objects, leaves and starts the hub, so the two agree)
			static bool hubCountsMade = false;
			static unsigned hubCountsGeneration = 0;
			static File_Chunk *hubCountsLevel = nullptr;
			static int64_t hubShapes = 0;
			static int64_t hubPlaced = 0;
			OpenLevelFile *const hubOpen = LevelObjects_OpenForEdit();

			if (hubOpen && (!hubCountsMade || hubOpen->Generation() != hubCountsGeneration || LevelObjects_Level() != hubCountsLevel))
			{
				host::PolyCounts_Level(LevelObjects_Level(), &hubShapes, &hubPlaced);
				hubCountsGeneration = hubOpen->Generation();
				hubCountsLevel = LevelObjects_Level();
				hubCountsMade = true;
			}

			std::vector<std::array<uint8_t, 8>> hubSelected;

			for (size_t i = 0; i < host::Selection_SetCount(); i++)
			{
				std::array<uint8_t, 8> id;

				if (host::Selection_SetMember(i, id.data()))
				{
					hubSelected.push_back(id);
				}
			}

			std::vector<host::HubTextLine> hubLines;

			host::HubText_Lines(LevelObjects_Level(), hubOpen, hubShapes, hubPlaced, hubSelected, local_scale, 16, &hubLines);

			for (host::HubTextLine const &hubLine : hubLines)
			{
				if (lineCount >= kPanelPassMostExtras)
				{
					break;
				}

				sayAt(hubLine.y, hubLine.words);
			}
		}
	}

	// MODULE LINKING'S SHOW ALL LEGEND (FUN_004c1a90 0x4c1b1c-0x4c1b5a, FUN_0057b1b0 at x 10, row ML-M-3): while Show All holds the screen these four rows are its only text. Leaving the screen lets it go.
	if (showAllHeld && !OnModuleLinkingScreen())
	{
		ShowAllRelease();
	}

	if (showAllHeld && LevelObjects_Level() && lineCount + 4 <= kPanelPassMostExtras)
	{
		struct Legend { int32_t y; char const *words; };
		Legend const legend[] = { { 0x1e, "2000+(Quads) RED" }, { 0x32, "1500-1999 ORANGE" }, { 0x46, "1200-1499 BLUE" }, { 0x5a, "0-1199 GREEN" } };
		int32_t const base = layout.toolbar.y + layout.toolbar.height;   // inteng's y 0 is under our bar band, as the hub's rows above

		for (Legend const &one : legend)
		{
			host::PaintText line;

			line.text = one.words;
			line.x = 10;
			line.y = base + one.y;
			lines[lineCount++] = line;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + std::string(one.words);
		}
	}

	// MODULE LINKING'S TEXT (FUN_004c1a90 every frame, FUN_0057b1b0 at x 10, white; rows ML-M-4, ML-M-6, ML-M-7 of module-linking-map-dietrich.md; inteng's y 0 under our bar band, as above):
	// the picks' names in pick order at (10, 110 + 16 i) ("%s" 0x5c39c4 of the pick's file object +0x154, +100: its name), after the 21st a row "........" and no more (0x4c1b8d-0x4c1d33);
	// at (10, 70) "Draw Linked Modules Only" while that switch is on, else "View Distance :%d" (the distance / 500000) while Toggle View Distance is on (0x4c1d8d-0x4c1dc4); at (10, 90)
	// "Setting base to current module" while Shift+S's follow is on (0x4c1de7-0x4c1dff). Not while Show All holds the screen (its legend alone). NOT YET: "Num quads %d (%d tri)" and
	// "Num Modules %d" at (10, 30) / (10, 50) with a current module (ML-M-5: the totals FUN_004c0720 adds up, FUN_004725b0 to read).
	if (!showAllHeld && OnModuleLinkingScreen() && LevelObjects_Level())
	{
		int32_t const base = layout.toolbar.y + layout.toolbar.height;

		auto const sayAt = [&](int32_t y, std::string const &words)
		{
			if (lineCount >= kPanelPassMostExtras)
			{
				return;
			}

			host::PaintText line;

			line.text = words;
			line.x = 10;
			line.y = base + y;
			lines[lineCount++] = line;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + words;
		};

		int32_t row = 0;

		for (int32_t const pick : linkPicks)
		{
			sayAt(0x6e + row * 0x10, ModuleNameOfIndex(pick));
			row++;

			if (row > 0x14)
			{
				sayAt(0x6e + row * 0x10, "........");
				break;
			}
		}

		if (onlyDrawLinked)
		{
			sayAt(0x46, "Draw Linked Modules Only");
		}
		else if (viewDistanceOn)
		{
			sayAt(0x46, "View Distance :" + std::to_string(viewDistance / 500000));
		}

		if (linkFollowCamera)
		{
			sayAt(0x5a, "Setting base to current module");
		}
	}

	// ADJACENT MODULES' TEXT (FUN_00404a20 every frame, FUN_0057b1b0 at x 10, white; AD-3 of reassess/adjacent-closeout-dietrich.md; inteng's y 0 under our bar band, as Module
	// Linking's above): the picked module's name at (10, 0x32) ("%s" 0x5c39c4 of its record +0x154, +100) while a module is picked, and "View Distance :%d" (0x5c39b0, the
	// distance / 500000) at (10, 0x22) while Toggle View Distance is on (0x4055f0-0x405638). Not while Show All's paint is held: inteng's hold returns before the text is drawn
	// (DAT_00606784, the top of FUN_00404a20). (Added 2026-11-17 by Melanikus (Dietrich).)
	// BAR-8: not while inteng's menu loop is up either: FUN_00404a20 calls the command (0x404ae2) before its text
	if (!adjacentShowAllPaint && OnAdjacentModulesScreen() && LevelObjects_Level() && !MenuLoopUp())
	{
		int32_t const base = layout.toolbar.y + layout.toolbar.height;

		auto const sayAt = [&](int32_t y, std::string const &words)
		{
			if (lineCount >= kPanelPassMostExtras)
			{
				return;
			}

			host::PaintText line;

			line.text = words;
			line.x = 10;
			line.y = base + y;
			lines[lineCount++] = line;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + words;
		};

		std::vector<int32_t> const picked = PickedModules();

		if (!picked.empty())
		{
			sayAt(0x32, ModuleNameOfIndex(picked[0]));
		}

		if (adjacentViewDistanceOn)
		{
			sayAt(0x22, "View Distance :" + std::to_string(viewDistance / 500000));
		}
	}

	// Light Sourcing's on-screen message at (200, 100) of the text area (FUN_0057bd40(200, 100, 0, words))
	if (!lightMessage.empty() && lineCount < kPanelPassMostExtras)
	{
		host::PaintText message;

		message.text = lightMessage;
		message.x = 200;
		message.y = layout.toolbar.y + layout.toolbar.height + 100;
		lines[lineCount++] = message;
	}

	// Module Linking's adjacency message (ML-T1, AdjacencyWithMessage): FUN_0057bd40(200, 100) while the data is worked out
	if (!linkMessage.empty() && lineCount < kPanelPassMostExtras)
	{
		host::PaintText message;

		message.text = linkMessage;
		message.x = layout.view.x + 200;
		message.y = layout.view.y + 100;
		lines[lineCount++] = message;
		linkMessageShown = linkMessage;
	}

	// "Camera position saved" (FUN_004704d0 -> FUN_0057bd40(200, 100): onto the screen at once, from the window's top, gone with the next frame): this frame only
	if (cameraSavedDraw && lineCount < kPanelPassMostExtras)
	{
		host::PaintText message;

		message.text = "Camera position saved";   // 0x5d3d54
		message.x = layout.view.x + 200;
		message.y = layout.view.y + 100;
		lines[lineCount++] = message;
		cameraSavedDraw = false;
		cameraSavedDrawn++;
	}

	// inteng's dialogs (dialogs.h) are centred in the view's box and painted over everything else
	host::Dialogs_SetScreen(layout.view.x, layout.view.y, layout.view.width, layout.view.height);

	// Adjacent Modules, Link > Delete: inteng draws the text "BOO!" at (10, 0x50) of the screen (FUN_0057b1b0 at 0x404cbc, the string at 0x5c3a28); it stays until the screen is left
	if (!adjacentBoo.empty() && !MenuLoopUp() && lineCount < kPanelPassMostExtras)   // BAR-8: drawn after the command step (0x404ae2 before 0x404cbc)
	{
		host::PaintText boo;

		boo.text = adjacentBoo;
		boo.x = layout.view.x + 10;
		boo.y = layout.view.y + 0x50;
		lines[lineCount++] = boo;
	}

	// Show Poly Counts (FUN_004c0950): the screen is cleared and only its list is written
	if (polyScreenOpen)
	{
		int32_t const screenW = layout.toolbar.width > 0 ? layout.toolbar.width : 1008;
		int32_t const screenH = layout.status.y + layout.status.height > 0 ? layout.status.y + layout.status.height : 729;

		lineCount = 0;
		boxCount = 0;
		boxes[boxCount++] = host::PaintBox{ 0, 0, screenW, screenH, 0xFF000000u };

		for (size_t i = 0; i < polyRowsPerPage && polyPage * polyRowsPerPage + i < polyRows.size() && lineCount < kPanelPassMostExtras - 6; i++)
		{
			host::PaintText entry;
			std::string words = host::PolyCountText(polyRows[polyPage * polyRowsPerPage + i]);

			for (size_t tab; (tab = words.find('\t')) != std::string::npos;)
			{
				words.replace(tab, 1, "    ");
			}

			entry.text = words;
			entry.x = (int32_t)(i / 25) * 320 + 16;
			entry.y = (int32_t)(i % 25) * 16;
			lines[lineCount++] = entry;
		}

		static char const *const foot[4] = { "Left Button : Goto object", "Right Button : Exit", "Page Down : Show next 50 modules", "Page Up : Show Previous 50 modules" };

		for (int32_t f = 0; f < 4; f++)
		{
			host::PaintText line;

			line.text = foot[f];
			line.x = 1;
			line.y = screenH - 0x44 + f * 0x10;
			lines[lineCount++] = line;
		}
	}

	// The Shape Chooser's text (FUN_004ddb60 0x4ddedd-0x4ddf55)
	if (chooserActive && !chooserList.empty() && !MenuLoopUp() && lineCount + 6 <= kPanelPassMostExtras)   // BAR-8: its command (0x4ddb69) before its text
	{
		int32_t const screenH = layout.status.y + layout.status.height > 0 ? layout.status.y + layout.status.height : 729;
		host::ShapeChoice const &now = chooserList[chooserAt];
		struct Said { int32_t x, y; std::string text; };
		Said const said[] = {
			{ 200, screenH - 0x3c, "S : Select shape from list" },
			{ 200, screenH - 0x28, "Return : Exit with shape" },
			{ 200, screenH - 0x14, "Escape : Exit without shape" },
			{ 10, 0x28, "Polys " + std::to_string(now.polygons) + ", Verts " + std::to_string(now.points) },
			{ 10, 0x38, "Shape '" + now.name + "'" } };

		for (Said const &one : said)
		{
			host::PaintText line;

			line.text = one.text;
			line.x = one.x;
			line.y = one.y;
			lines[lineCount++] = line;
			screenTextNow += (screenTextNow.empty() ? "" : "|") + one.text;   // the state line's screentext: a test reads the chooser's lines as the screen shows them
		}
	}

	// The Hierarchy view's text: inteng draws it every frame (HierarchyTextLines), at x, 16 * row of the screen
	bool textAfterTheCommand = false;   // the mode draws this text after its command step (BAR-8)

	if (OnTheTrackScreen())
	{
		TrackTextLines(&hierarchyTextNow);
		textAfterTheCommand = trackEd.state != host::TrackState::PivotEdit;   // FUN_004d9af0 (Track Editing's frame too): states 2-4 reach its command step 0x4d9d1b, their text after it (0x4dc742); Pivot Edit returns at 0x4d9b51 before it
	}
	else if (OnAcousticScreen())
	{
		AcousticTextLines(&hierarchyTextNow);
		textAfterTheCommand = true;   // FUN_004f57b0: its commands first (0x4f57ba), its text from 0x4f5b82
	}
	else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Waypoint") == 0)
	{
		WaypointTextLines(&hierarchyTextNow);
		AutoPlotCommand_Frame();   // a running auto-plot's progress, its networks written when done (autoplot_command.h)
		textAfterTheCommand = true;   // FUN_00586450: each sub-state (FUN_00583d00 / 584ee0 / 585750 / 584d80) calls the command at its top, the list is drawn after them (0x58649f)
	}
	else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Object Placement") == 0)
	{
		PlacementTextLines(&hierarchyTextNow);
		textAfterTheCommand = true;   // FUN_004d9af0: its command step 0x4d9d1b, its text from 0x4dc73a
	}
	else
	{
		HierarchyTextLines(&hierarchyTextNow);
		textAfterTheCommand = true;   // FUN_004889d0: its command step 0x4889fb, its text from 0x48a07a
	}

	// While the bar or the pop-up is up, inteng's frame is stopped inside its menu loop (FUN_004b7c50 -> FUN_004b7dd0, modal) at the mode's command step: a mode that draws its text after that
	// step has none of it on the screen then (reassess/dialog-look-drake.md BAR-6, BAR-8, BAR-9: as render state 0 shows it). Track Editing is Object Placement's frame too
	// (FUN_004d9af0): its states 2-4 go through the command step before their text; Pivot Edit (state 5) returns before the step (0x4d9b3d-0x4d9b51), and FUN_004f3e30 draws no text
	// Insert's zoom waiting for its centre: FUN_00471cb0 0x471ccb writes "Zoom... Choose centre" at (200, 100) (FUN_0057bd40); the status line keeps saying it too. It shows on every screen, the volume
	// editor's too, where the status line is blank (a review of S-VOL-11, 11-17: nothing showed there)
	if (zoomStage == 1)
	{
		HierarchyTextLine line = { 200, 100 / 16, "Zoom... Choose centre" };

		line.pixelY = 100;
		hierarchyTextNow.push_back(line);
	}

	if (textAfterTheCommand && MenuLoopUp())
	{
		hierarchyTextNow.clear();
	}

	// Move rel. to poly waiting for its click: "Move relative to which poly" at (200, 100) (FUN_00564b60 0x564ba2-0x564bb0, every screen that has the item). After the menu loop's rule
	// above, as CAM-8's lines below: the command writes it once and then waits in its own loop (FUN_0058b700 until a press), never as the mode's text inside its menu loop, so the bar
	// being up (Texture Map draws it always) takes nothing away from it (acceptance-texture-map-move-rel-poly; Frost's find, 11-18)
	if (moveRelWaiting)
	{
		HierarchyTextLine line = { 200, 100 / 16, "Move relative to which poly" };

		line.pixelY = 100;
		hierarchyTextNow.push_back(line);
	}

	// Scroll Lock's pick of the rotation centre waiting (CAM-8): FUN_004720c0 0x4720fa / 0x47210c writes "Choose object for centre of rotation" at (200, 100) and "RMB or escape to cancel" at (200, 0x74).
	// After the menu loop's rule above: these are the wait's own lines, drawn by its own loop from WinMain (0x58d009), never the mode's text inside its menu loop, so the bar being up (Texture Map
	// draws it always) takes nothing away from them (acceptance-camera-scroll-lock-texture-map)
	if (rotationPickWaiting)
	{
		HierarchyTextLine first = { 200, 100 / 16, "Choose object for centre of rotation" };
		HierarchyTextLine second = { 200, 0x74 / 16, "RMB or escape to cancel" };

		first.pixelY = 100;
		second.pixelY = 0x74;
		hierarchyTextNow.push_back(first);
		hierarchyTextNow.push_back(second);
	}

	for (HierarchyTextLine const &one : hierarchyTextNow)
	{
		if (lineCount >= kPanelPassMostExtras)
		{
			break;
		}

		host::PaintText text;

		text.text = one.text;
		text.x = layout.view.x + one.x;   // inteng's x, a line at 0 on the window's left edge exactly (panel_pass.cpp AddLines)
		text.y = one.pixelY >= 0 ? one.pixelY : one.row * 16;

		if (one.grayBehind && boxCount < kPanelPassMostExtras)
		{
			int32_t width = 2 * host::kPaintMarginAcross;

			for (size_t c = 0; c < text.text.size(); c++)
			{
				width += PanelPass_MenuWidth((unsigned char)text.text[c]);
			}

			boxes[boxCount++] = GrayBehind(text.x - host::kPaintMarginAcross, text.y, width, kPanelPassLineHeight);
		}

		lines[lineCount++] = text;
	}

	if (host::Dialogs_IsOpen())
	{
		std::vector<host::PaintBox> dialogBoxes;
		std::vector<host::PaintText> dialogLines;

		host::Dialogs_Draw(&dialogBoxes, &dialogLines, kPanelPassBackground, host::kMenuLitColour);

		// a dialog is over everything (DL-2), the bar too. NEVER SKIPPED WHOLE (Bishop's ruling 2026-11-18): it was, when it had more boxes or lines than the frame's room
		// (kPanelPassMostExtras), and the owner would see no window while it took his keys. What fits is drawn, its boxes in their order (the window first, then its parts), and
		// the state line's dialogdrawn says how much ("<boxes drawn>/<boxes>"); the rest is logged once.
		size_t drawnBoxes = 0;
		size_t drawnLines = 0;

		for (host::PaintBox const &b : dialogBoxes)
		{
			if ((size_t)boxCount >= kPanelPassMostExtras)
			{
				break;
			}

			boxes[boxCount] = b;
			boxes[boxCount++].overlay = true;
			drawnBoxes++;
		}

		for (host::PaintText const &t : dialogLines)
		{
			if ((size_t)lineCount >= kPanelPassMostExtras)
			{
				break;
			}

			lines[lineCount] = t;
			lines[lineCount++].overlay = true;
			drawnLines++;
		}

		StateLine_Add("dialogdrawn", std::to_string((unsigned long long)drawnBoxes) + "/" + std::to_string((unsigned long long)dialogBoxes.size()));

		if (drawnBoxes < dialogBoxes.size() || drawnLines < dialogLines.size())
		{
			static bool told = false;

			if (!told)
			{
				told = true;
				LogString("dialogs: the open window has " + std::to_string(dialogBoxes.size()) + " boxes and " + std::to_string(dialogLines.size()) + " lines, more than the frame has room for (panel_pass.h): " +
					std::to_string(drawnBoxes) + " boxes and " + std::to_string(drawnLines) + " lines drawn");
			}
		}
	}

	hitPrompt = ui::Box{ 0, 0, 0, 0 };

	if (promptFor != kPromptNone && lineCount + 3 <= kPanelPassMostExtras && boxCount < kPanelPassMostExtras)
	{
		std::string title;
		std::string hint;

		PromptWords(&title, &hint);

		std::vector<std::string> words;

		if (!formRows.empty())
		{
			FormLines(title, &words);
		}
		else
		{
			words.push_back(title);
			words.push_back(promptText + "_");
			words.push_back(hint);
			words.insert(words.end(), promptList.begin(), promptList.end());
		}
		int32_t width = 0;

		for (std::string const &line : words)
		{
			int32_t w = 2 * host::kPaintMarginAcross;

			for (size_t c = 0; c < line.size(); c++)
			{
				w += PanelPass_MenuWidth((unsigned char)line[c]);
			}

			width = w > width ? w : width;
		}

		int32_t const lineHeight = kPanelPassLineHeight + 2;
		int32_t const left = layout.view.width > width ? (layout.view.width - width) / 2 : 0;
		int32_t const top = layout.toolbar.y + layout.toolbar.height + 24;

		int const rowCount = (int)words.size() < (int)(kPanelPassMostExtras - lineCount) ? (int)words.size() : (int)(kPanelPassMostExtras - lineCount);

		hitPrompt = ui::Box{ left, top, width, rowCount * lineHeight + 6 };
		boxes[boxCount] = GrayBehind(hitPrompt.x, hitPrompt.y, hitPrompt.width, hitPrompt.height);
		boxes[boxCount++].overlay = true;   // a prompt asks as a dialog does: over everything

		for (int i = 0; i < rowCount; i++)
		{
			host::PaintText line;

			line.text = words[(size_t)i];
			line.x = left + host::kPaintMarginAcross + (left == 0 ? 1 : 0);
			line.y = top + 3 + i * lineHeight;
			line.overlay = true;
			lines[lineCount++] = line;
		}
	}

	// the special objects of Object Placement, over the view, in what room is left
	AddSpecialMarkers(layout, lines, &lineCount, boxes, &boxCount);

	// Vertex mode's held corners (inteng lights them in the view: FUN_0046dc20): a small box with its number at each, where the drawn view puts it now
	if (onTextureMap && textureSelectMode == 4 && polyVertices.count > 0 && !polyShapeWorld.empty() && layout.view.width > 0 && layout.view.height > 0)
	{
		host::PickCamera camera;

		if (PickGame_Camera(layout.view.width, layout.view.height, &camera))
		{
			for (size_t i = 0; i < polyVertices.count && boxCount < kPanelPassMostExtras && lineCount < kPanelPassMostExtras; i++)
			{
				size_t const point = (size_t)polyVertices.point[i];

				if (point * 3 + 2 >= polyShapeWorld.size())
				{
					continue;
				}

				int32_t const world[3] = { polyShapeWorld[point * 3], polyShapeWorld[point * 3 + 1], polyShapeWorld[point * 3 + 2] };
				double px = 0, py = 0, ahead = 0;

				if (host::PickRay_Project(camera, world, &px, &py, &ahead))
				{
					host::PaintBox mark = { (int32_t)px - 4, (int32_t)py - 4, 9, 9, host::kMenuLitColour };
					host::PaintText number;

					number.text = std::to_string(i + 1);
					number.x = (int32_t)px + 6;
					number.y = (int32_t)py - 6;
					boxes[boxCount++] = mark;
					lines[lineCount++] = number;
				}
			}
		}
	}

	// The zoom's rectangle (FUN_00472200 draws its four sides): four thin boxes about the centre chosen
	if (zoomStage == 2 && zoomHalf > 0 && zoomViewBox.width >= 8 && zoomViewBox.height >= 8 && boxCount + 4 <= kPanelPassMostExtras)
	{
		int32_t const cx = zoomViewBox.x + zoomViewBox.width / 2 + zoomCentre[0];
		int32_t const cy = zoomViewBox.y + zoomViewBox.height / 2 + zoomCentre[1];
		int32_t const hw = zoomHalf;
		int32_t const hh = (zoomHalf * zoomViewBox.height) / zoomViewBox.width;

		boxes[boxCount++] = GrayBehind(cx - hw, cy - hh, 2 * hw + 1, 2);
		boxes[boxCount++] = GrayBehind(cx - hw, cy + hh, 2 * hw + 1, 2);
		boxes[boxCount++] = GrayBehind(cx - hw, cy - hh, 2, 2 * hh + 1);
		boxes[boxCount++] = GrayBehind(cx + hw, cy - hh, 2, 2 * hh + 1);
	}

	// H-1j: inteng's Help (FUN_004724f0) runs its OWN loop until a mouse button (0x472575 / 0x472584): no mode's frame, no top-left text, nothing of the screen is drawn over the cleared
	// screen and its words. So on every screen's Help, whatever the rest of this frame planned after the Help screen (a mode's rows, "View Distance", messages, the zoom box) is dropped.
	if (helpScreenUp)
	{
		lineCount = helpLines;
		boxCount = helpBoxes;
	}

	PanelExtras const extras = { boxes, boxCount, lines, lineCount };
	return extras;
}
