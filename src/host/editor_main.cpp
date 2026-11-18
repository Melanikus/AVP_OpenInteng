// editor_main.cpp: the editor's start (the phase 2 plan, F3 and F4). Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It reads its settings, opens a window that never takes the mouse, makes the game's own Direct3D device in the level view's child
// window, opens one level and draws it from a fly camera, with the editor's panels around it: the menu bar along the toolbar and the
// status line at the foot, until the window is closed. (Modified 2026-11-14 by Melanikus (Dietrich), F3: the view's child window, the
// panels and the menu bar. Modified 2026-11-14 by Melanikus (Frost): a click on the level's view picks what it shows there.)
//
// SETTINGS: AVP_OpenInteng.ini, beside the program:
//     [Editor]
//     DataFolder=<the editor's own copy of the game's data: a folder that holds avp_rifs>
//     Level=<a level's file name, without .rif: the level opened when none is given on the command line>
//     KeyLayout=<Classic or ModernWASD: the key layout; Classic inteng's when the line is missing or says anything else (key_layout.h)>
//     StartWidth=, StartHeight=, Windowed=<the start size and a framed window (editor_options.h; the release zip's own file says 1280, 720, true)>
// The game's files are opened from that folder, as the game opens them from its own. The only thing ever written there is a level
// file given on the command line, copied into its avp_rifs (open_with.h). In AVP_OpenInteng.ini the editor writes only the KeyLayout=
// line, when the mapper picks a key layout in the View menu.
//
// ON THE COMMAND LINE:
// - A level file, as Windows' "Open with" gives it: AVP_OpenInteng.exe <file.rif>. It's copied into the data folder's avp_rifs under
//   its own name, if it isn't there already, and opened instead of Level=. open_with.h has the rules it's held to. The command line is
//   read as the wide text Windows keeps for it (CommandLineToArgvW), never as main's narrow argv: open_with.h says why.
// Whatever stops the editor (a refused file, a command line or settings it can't use) is said on the error output, and with someone at
// the screen in a message box too, as "Open with" and a double-click give no console. A command line with -picture on it has nobody at
// the screen, so it never shows one.
// - -picture <frames>. After that many frames, the last one is saved as first-picture.png beside the program, and the editor closes.
//   That's how a run proves it drew the level, with nobody at the screen. The window then opens without taking the keyboard from
//   whoever is using the machine.
// - -viewmode <normal|polygon|polyline|filledpolyline|wire-textured|unlit|headlight|polyline+unlit|wire-textured+unlit|
//   normal+headlight>: the View menu's items, used before the first frame with no click, so a -picture run proves that mode draws. Only
//   those words. (Added 2026-11-14 by Melanikus (Frost).)
// - -screen <name>: the editor starts on that screen's bar (Track Editing, Light Sourcing (with Doors), ...), so a proof run can show
//   any screen. The name has to be EXACTLY one of the screens' (inteng_menus.h, IntengMenus_ScreenFromCommandWord); anything else is
//   refused, naming the allowed ones, and nothing opens. (Added 2026-11-15 by Melanikus (Apone).)
//
// NO CONSOLE: the editor is a Windows program, not a console one (the project's SubSystem), so "Open with" and a double-click open its
// window and nothing else: no black console window flashing up and closing with the reason in it. What it says on its error and normal
// output still reaches a program that starts it with those caught, as a -picture run does.
//
// ONE EDITOR AT A TIME (one_editor.h): a second editor says another is running, and stops before it reads or writes anything.
//
// KEYS: two layouts (key_layout.h), picked in the View menu and kept in AVP_OpenInteng.ini; Classic inteng's to start with. In Classic,
// as inteng had them (with NumLock on): A and Z move forward and back, the numeric keypad's 1 and 3 sideways, its minus and plus up and
// down, its 4 and 6 turn, and its 2 and 8 look up and down. In Modern WASD: W and S move forward and back, A and D sideways, R and F up
// and down, and the arrow keys turn and look. In both, moving the mouse with its right button held down turns and looks too. The mouse
// is only read: never held, moved, hidden or kept inside the window. Help, Keys says the keys in use.
// (F10 and an Alt tap no longer open the menu bar: they are not inteng's; F10 is camera slot 9.) Once the bar is open, the arrows, Enter and Esc work it; a click on a menu opens it too. A key
// the table of actions gives an action in the layout in use (editor_actions.cpp), such as Ctrl+O or the main row's 8 for Polyline, does
// it. Those keys are read as the keys themselves, so the keypad's 8 and the main row's 8 are different keys. (Modified 2026-11-14 by
// Melanikus (Dietrich), F3: the two key layouts.)
// A click with the left button on the level's view, while no menu is open, picks the thing the game shows at that pixel (pick_game.h),
// and the status line says what it was.
// The object list, in the list's column, has a row for each of the level's objects. A click on a row selects that object; a pick in the
// view of an object the list has chooses its row. Only clicks work it for now.
// The field panel, at the foot of the list's column, shows what's selected: for now only a title, a stand-in (field_adapters.h says why).

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

extern HWND hWndMain;                   // win_proj.cpp:78: the window the game's renderer draws into: here, the view's child window
extern bool bRunning;                   // win_proj.cpp:82: the game's own "keep going", which its waits look at

HWND editorWindow = NULL;        // the editor's own window, which holds the panels and the view's child window

// The menu bar (menu_bar.h), made once from the editor's table of actions (editor_actions.h), and what the status line says
ui::MenuBar menuBar;       // the first PHYSICAL row of the bars
// The bars' titles WRAP onto more rows when they are wider than the window (editor_bar.h, EditorBar_MakeRows): the rows under the first are these, loaded only while there are that many rows
ui::MenuBar lowerBar[kEditorBarMostRows - 1];
size_t barRowCount = 1;    // how many physical rows the bars have now, 1 to kEditorBarMostRows; the toolbar is that many bars tall
static int32_t barWrapWidth = 0;  // the window width the rows were wrapped for; the bars are made again when the window's width is another
// The bars of the old editor's screen in use (editor_bar.h): inteng's menus with their tags, then the editor's own, as logical rows (barRows.logical) cut into physical rows (barRows.menus) to fit the window. They're
// made again when a pick goes to another screen, or the key layout changes, or the window's width does. (Modified 2026-11-15 by Melanikus (Dietrich), A6 (iii-b): it was the editor's own four menus.)
EditorBarRows barRows;
size_t barScreen = host::kIntengHub;
static int32_t barMinClientWidth = 0;     // the least the client area is wide, so no title of any screen's bar is past its edge
char cutOffWords[160] = "";        // which titles are past the toolbar's edge, for the status line, while any are
static AltTap altTap = { false, false };
const char *statusWords = 0;

// The key layout in use (key_layout.h), Classic inteng's until the settings say otherwise, and the settings file that keeps it
// (Added 2026-11-14 by Melanikus (Dietrich), F3.)
KeyLayout keyLayout = KeyLayout::Classic;
char layoutSettingsFile[MAX_PATH] = "";
// The Options switches kept in the settings file (editor_options.h): Select by polygons (a click takes the object under the pointer, else the object whose centre is nearest it), Prefer .BMP over .RIM, Use Polygon View Distance
host::EditorOptions editorOptions = host::EditorOptions_Defaults();
static const char kLayoutNotSavedWords[] =
	"The keys have changed, but AVP_OpenInteng.ini couldn't be written, so the editor starts with the keys it had before next time";

// The view's child window goes where the panels' layout puts the view in the editor's window (panel_layout.h). It's asked every frame,
// and moves the window only when it isn't there: the layout changes whenever the editor's window is resized, and the game's own
// InitialiseDirect3D moves hWndMain to the top left at its picture's size (ChangeWindowsSize, d3_func.cpp:2509). Nothing else moves it:
// the device's reset (view_surface.cpp) doesn't call ChangeWindowsSize. A view the layout gives no room is kept 1 by 1, as
// view_surface.h draws it. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
// The panels' sizes are host_frame.h's, so the level's view is the main part of the window at every size. (Modified 2026-11-14 by
// Melanikus (Dietrich), F3: it was a narrow strip between two empty grey columns.)

bool LoadTheMenus();

static ui::PanelLayout PlaceTheView(int windowWidth, int windowHeight)
{
	// the bars' titles wrap to the window's width: when the width is another, the rows are made again (so the toolbar's height below is right for this width)
	if (windowWidth > 0 && windowWidth != barWrapWidth)
	{
		barWrapWidth = windowWidth;
		LoadTheMenus();
	}

	// a screen with more than one row of bars has a toolbar that many bars tall (the first on top), so the panels below it and the tool row move down
	ui::PanelLayout layout = ui::LayOutPanels(windowWidth, windowHeight, EditorBar_SizesRows(HostFrame_PanelSizes(windowWidth), barRowCount));

	// L-1: the level's view covers the whole client area, and the panels are drawn over it (see PlanThePanels). The layout's other boxes are
	// where the panels go, not where the view is. (Modified 2026-11-15 by Melanikus (Dietrich), L-1: the view was the room between them.)
	layout.view.x = 0;
	layout.view.y = 0;
	layout.view.width = windowWidth > 0 ? windowWidth : 0;
	layout.view.height = windowHeight > 0 ? windowHeight : 0;

	int const width = layout.view.width > 0 ? layout.view.width : 1;
	int const height = layout.view.height > 0 ? layout.view.height : 1;
	RECT now = { 0, 0, 0, 0 };

	GetWindowRect(hWndMain, &now);
	MapWindowPoints(HWND_DESKTOP, editorWindow, (POINT *)&now, 2);

	if (now.left != layout.view.x || now.top != layout.view.y || now.right - now.left != width || now.bottom - now.top != height)
	{
		SetWindowPos(hWndMain, NULL, layout.view.x, layout.view.y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	return layout;
}

bool keyDown[256];

// Shift is down: the real key, or the harness's (a hidden run posts messages, which never move GetKeyState: the scenario's `chord Shift D` posts a Shift key-down the table above records, and a posted click carries MK_SHIFT)
// A key held now (key_reader.h): the editor's own table, and the real keyboard only when no scenario drives the editor (a -picture run: then the person at the computer
// cannot change what the scenario's keys do, Bishop 2026-11-16). Every key the editor reads itself goes through here.
bool KeyHeldNow(int vk)
{
	return host::KeyReader_Held(vk, keyDown, NobodyWatching());
}

bool ShiftDown()
{
	return KeyHeldNow(VK_SHIFT);
}
static int screenKeySwallowChar = 0;   // a screen key (screen_keys.h) was done on its key message: the character message that follows it (L types "l", which would also deselect all) is let go of
static int dialogKeySwallowChar = 0;   // a key opened one of inteng's windows (dialog_box.h): the letter its character message brings is not typed into it, as inteng's modal window never gets it (Vasquez 11-16)

// THE CAMERA'S MOVER, inteng's LIVE one (Bishop's trace 2026-11-16, AvP-docs\engine\inteng\reassess\camera-mover-trace.md; the code is FUN_0053db40, an orphan in dump-orphans-3.txt line
// 12615: the camera block's strategy slot 2, table 0x5f0754, run each frame by FUN_0053c350). The mover this editor was first built from, FUN_0053d7f0, never runs (DAT_0093110c, read once
// at 0x53dbed, is never written). Each frame, in inteng's order (MoveTheCamera):
//  - ] / [ change the speed by 3, at least 3 (0x53db60-0x53dbc2; ChangeTheSpeed);
//  - C held: nothing moves this frame, the glide neither (0x53dc0e JNZ 0x53e58b, past the glide at 0x53e582);
//  - Shift NOT held: A forward, Z back, Left / Right (and numpad 1 / 3) along the camera's right axis, Up / Down (and numpad - / +) along its up axis, each by the step below, and EVERY
//    one of them sets the glide to 0 (0x53e216-0x53e362);
//  - Shift HELD: no step at all; Shift+A adds 100000 to the glide each frame, Shift+Z takes 100000 off (0x53e088-0x53e0f6); Shift with an arrow does nothing;
//  - Space held: the glide is 0 (0x53e54d);
//  - the glide (block +0x168, inteng's units a second, 100 to the millimetre) moves the camera along the way it faces by glide x the frame's time, NOT held to 0.1 s (FUN_0053c7d0,
//    0x53e582): nothing slows it, letting go of the keys keeps it going. Melanikus remembers exactly that: "A makes you move slowly, holding shift speeds you up, letting go of both
//    makes you keep moving, pressing space stops you".
// NOT HERE: numpad * levelling the view (0x53e41b-0x53e51a), the joystick's glide (DAT_00d881a0), and Object Placement's world-axis movement (DAT_00914af8, toggled by its key at
// 0x4dc5bf: the same keys along the level's axes; its Down / numpad + add 0x3e87360 to the y sum where the others add 0x10000, 0x53e1ca: inteng's own oddity, named for Vasquez).
static int const kGlidePerFrame = 100000;   // Shift+A / Shift+Z: the glide's change each frame (0x53e0b9 ADD +0x168, 0x186a0; 0x53e0f6 ADD +0x168, 0xfffe7960: listing read)
static int32_t cameraGlide = 0;              // the camera block's +0x168: inteng's units a second along the way it faces

// THE CAMERA'S STEP (FUN_0053db40 0x53e37f-0x53e3ee): each move key held without Shift moves the camera by FUN_0053d7d0(speed x the axis' component, t) >> 2 along that axis, where speed is DAT_005f071c (6 at the start:
// .data; the text shows it divided by 3, FUN_00471bcb), the component is +-65536 (a unit vector in 16.16) and t is the FRAME TIME DAT_00d876dc in 16.16 seconds, held to 0x199a (0.1 s). So the distance is speed x 16384
// engine units for each second held; the engine's units are 100 to the game's millimetre, so at the start the camera moves 6 x 163.84 = 983 mm a second, whatever the frame rate. ] and [ are the speed keys (one
// takes 3 off and holds it at 3 at the least, the other adds 3, no limit). Shift and Space: THE CAMERA'S MOVER above (the glide). (Frost 2026-11-16: the step is time-based; Dietrich 2026-11-16: the live mover.)
static int const kSpeedStart = 6;
static int const kSpeedLeast = 3;
static int const kSpeedStepBy = 3;
int speedUnits = kSpeedStart;       // inteng's DAT_005f071c
static double stepMillimetres = 0.0;       // this frame's distance in whole millimetres, from the frame's time
static double stepCarry = 0.0;             // what is left of a fraction of a millimetre, carried to the next frame
static LARGE_INTEGER stepClock = {};
static double frameSeconds = 0.0;          // this frame's time, NOT held to 0.1 s: the glide's (FUN_0053c7d0 reads DAT_00d876dc as it is)
static char speedWords[80];

// The speed a saved CameraSpeed= gives (the settings file is the editor's own, but anyone can write it): kSpeedLeast at the least, as ] and [
// hold it, and 3000 at the most
static int CameraSpeedHeld(UINT saved)
{
	return saved < (UINT)kSpeedLeast ? kSpeedLeast : (saved > 3000 ? 3000 : (int)saved);
}

// Called once at the top of each frame, before the camera moves: works the frame's step out from the time since the last (at most 0.1 s, as inteng)
static void BeginTheFrameStep()
{
	LARGE_INTEGER now, frequency;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);

	double seconds = stepClock.QuadPart != 0 && frequency.QuadPart > 0 ? (double)(now.QuadPart - stepClock.QuadPart) / (double)frequency.QuadPart : 0.0;

	stepClock = now;
	frameSeconds = seconds < 0.0 ? 0.0 : (seconds > 10.0 ? 10.0 : seconds);   // the glide's time is not held to 0.1 s; held to 10 s only so a stalled frame can't throw the camera far
	seconds = seconds < 0.0 ? 0.0 : (seconds > 0.1 ? 0.1 : seconds);

	// speed x 16384 of inteng's engine units a second; its engine unit is a hundredth of the game's millimetre (DAT_00607bd8 = 100 x the file's scale, and the game's world is the file's
	// units x that same scale), so the step is speed x 163.84 game millimetres a second on every level, with NO scale factor (Vasquez's read through Bishop, 2026-11-16: the step was
	// multiplied by local_scale, too fast or too slow on any level whose scale isn't 1)
	stepCarry += (double)speedUnits * 16384.0 / 100.0 * seconds;
	stepMillimetres = floor(stepCarry);
	stepCarry -= stepMillimetres;
}

static int StepAtTheSpeed()
{
	return (int)stepMillimetres;
}

static void SaySpeed()
{
	extern double local_scale;

	_snprintf_s(speedWords, sizeof(speedWords), _TRUNCATE, "Camera speed: %d (%d mm a second)", speedUnits / 3, (int)((double)speedUnits * 16384.0 / 100.0 * (local_scale > 0.0 ? local_scale : 1.0)));
	statusWords = speedWords;
}

// A change of speed, by 3 a press as inteng, 3 at the least; says the speed
static void ChangeTheSpeed(int by)
{
	int const wanted = speedUnits + by * kSpeedStepBy;

	speedUnits = wanted < kSpeedLeast ? kSpeedLeast : wanted;

	// kept for the next start (inteng writes it when it ends); the settings file is the editor's own
	if (layoutSettingsFile[0])
	{
		char number[16];

		_snprintf_s(number, sizeof(number), _TRUNCATE, "%d", speedUnits);
		WritePrivateProfileStringA("Editor", "CameraSpeed", number, layoutSettingsFile);
	}

	SaySpeed();
}
bool running = true;
static bool rightButtonDown = false;

// THE RIGHT-CLICK POP-UP (editor_bar.h): a screen's second menu, opened by a click of the RIGHT button that didn't drag (a drag turns and looks). It
// opens at the pointer, closes on a pick, a click elsewhere or Esc, and its items do what they do on a bar. (Added 2026-11-15 by Melanikus (Dietrich).)
ui::MenuBar popupBar;
EditorBarRoom popupRoom;
POINT popupAt = { 0, 0 };        // where it opens, in the editor window
static POINT rightPressedAt = { 0, 0 }; // where the right button went down
static bool rightDragged = false;       // the pointer moved while the right button was down
static int rightRawX = 0;               // where the right button went down, in the window that got the message
static int rightRawY = 0;

// THE BUTTONS (A8, tool_buttons.h): the tool row at the top of the view and the action buttons above the status line. Planned each frame, with the
// boxes kept for the presses.
host::ToolButton toolButtons[host::kToolButtonMost];
host::ToolButton panelButtons[host::kToolButtonMost];
ui::Box toolBoxes[host::kToolButtonMost];
ui::Box panelBoxes[host::kToolButtonMost];
size_t toolPlaced = 0;
size_t panelPlaced = 0;

// THE THREE VIEWS (three_view.h): on inteng's Track Editing and Point Editing screens. Their numbers are fitted to the level (ours until read); the shared
// 3D cursor starts at 0, 0, 0 as inteng's does.
static host::ThreeViews threeViews;
static bool threeViewsFitted = false;
int32_t threeCursor[3] = { 0, 0, 0 };
static ui::Box threeBox = { 0, 0, 0, 0 };
int32_t trackSection = 0;          // the CURRENT section of the track, whose start and end markers the views show (FUN_004f0da0); stepped by , and . (ours)
static size_t trackSectionCount = 0;
static char trackWords[96];
bool trackDirty = false;           // the selected object's track changed (a paste, a field kept): its lines are made again
static uint32_t trackSeenChanges = 0xFFFFFFFFu;
static char threeWords[64];
static ui::Box threeMainBox = { 0, 0, 0, 0 };   // the bottom right quarter of the window's middle, where the 3D view is while the three views are shown

host::WaypointMode waypointMode;   // Waypoint mode's state (waypoint_mode.h)

bool OnWaypointScreen()
{
	return barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Waypoint") == 0;
}

// Waypoint mode's Edit Extents (mode 2) shows the waypoints' boxes and links in the three views (waypoint_view.h): inteng makes them in FUN_005846b0 only (W-5); modes 1 and 3 are the 3D view
static bool WaypointViewsOn()
{
	return OnWaypointScreen() && waypointMode.Mode() == 2;
}

// Waypoint mode's AI modules mode (mode 0): its click and its module colours (waypoint_mode.h, MODE 0's PICK AND CURRENT)
static bool OnWaypointModules()
{
	return OnWaypointScreen() && waypointMode.Mode() == 0;
}

// Waypoint mode's modes 1 to 3: the 3D view draws the boxes and the links in inteng's colours, and only the AI module's modules (waypoint_view.h, W-2 / W-3)
bool WaypointWorldOn()
{
	return OnWaypointScreen() && waypointMode.Mode() >= 1;
}


// inteng's three orthographic views on the track screens are PIVOT EDIT's alone (state 5: FUN_004f3770 turns the split screen off and makes them, 0x4f37aa-0x4f3838; its page has only Move Pivot To Centre and Stop Pivot Edit).
// The other states (2 a track loaded, 3 the end edit, 4 play) use the normal 3D view, where the track is drawn as lines in the world (TrackWorldLines, the avpx hook R_SetEditorWorldLines).
bool ThreeViewsOn()
{
	if (barScreen < host::kIntengScreenCount && (strcmp(host::kIntengScreens[barScreen].table, "0x5e6e98") == 0 || strcmp(host::kIntengScreens[barScreen].name, "Track Editing") == 0))
	{
		return TrackPivotEditing();
	}

	return (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5e7618") == 0) || WaypointViewsOn();
}

bool texturesSaved = true;   // what the settings file holds for Textured: the start is textured

// A physical row of the bars: 0 is the first, the others are under it
ui::MenuBar &BarRow(size_t row)
{
	return row == 0 ? menuBar : lowerBar[row - 1];
}

bool ALowerMenuIsOpen()
{
	for (size_t row = 1; row < barRowCount; row++)
	{
		if (lowerBar[row - 1].IsOpen())
		{
			return true;
		}
	}

	return false;
}

bool AMenuIsOpen()
{
	return menuBar.IsOpen() || ALowerMenuIsOpen() || popupBar.IsOpen();
}

static int lastMouseX = 0;
static int lastMouseY = 0;
static int mouseTurn = 0;               // how far the mouse has asked to turn and look since the last frame, in the game's angle units
static int mouseLook = 0;

// THE HARNESS'S QUESTIONS. Hicks's simulated-input harness (AvP-docs\review\sim-input-harness-hicks.md) drives a -picture run with posted
// keys and clicks, and asks three things with three messages of the editor's own (WM_APP is where a program's own messages start):
// - kHarnessPicture, with wParam n from 1 to 999: on the next frame drawn, picture-<n>.png (the level's back buffer, as
//   first-picture.png) and screen-<n>.png (the window as Windows shows it, as first-screen.png, with its self-check) beside the program,
//   then one line saying so;
// - kHarnessState: one line of the editor's state after the next frame: the open menu and its lit item, the camera, the View menu's
//   modes, and the view's, the window's and the panels' sizes.
// - kHarnessSelectRow, with wParam a row of the object list from 0: that row's object is selected and the row chosen, as a click on it
//   does (harness_select.h), and on the next frame drawn, one line for each line the field panel draws, then their count; a row the list
//   hasn't got (or an object the selection refuses) is refused, saying so. (Added 2026-11-14 by Melanikus (Dietrich), checkpoint 1.)
// Any program can post a message to a window, so they're input from outside: they're answered only in a -picture run, where nobody
// is at the screen (nobody_watching.h), and otherwise ignored, with nothing saved or printed. An n outside 1 to 999 is refused, saying
// so. Nothing else in the editor changes for them. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
static UINT const kHarnessPicture = WM_APP + 1;
static UINT const kHarnessState = WM_APP + 2;
static UINT const kHarnessSelectRow = WM_APP + 3;
// THE SCENARIO HARNESS'S THREE MORE QUESTIONS (tests/drive_scenario; added 2026-11-15 by Melanikus (Apone), Bishop's order: "a mode's MVP claim needs its scenario passing"). Like the three above they are
// answered only in a -picture run (nobody_watching.h) and ignored otherwise:
// - kHarnessItem, wParam an inteng command number: the item of the bars on the screen (the first bar, then the second) whose command is that one is done as a click on it is (RunAction), and a line says
//   which item it was or that no item has that command;
// - kHarnessSave: Save a copy, the same SaveTheCopy File > Save a copy does (its status words say where the copy went);
// - kHarnessScenarioState: one line, on the next frame drawn: the screen, the titles of the bars, the open prompt (its kPrompt number, 0 for none) and the status words. Every item and every save asks for it too.
static UINT const kHarnessItem = WM_APP + 4;
static UINT const kHarnessSave = WM_APP + 5;
static UINT const kHarnessScenarioState = WM_APP + 6;
// - kHarnessLocate, wParam a waypoint's number in the active list (Waypoint mode, mode 1), or 0xffff for the current one: one line with where the middle of that waypoint's box is in each of the three
//   views, in the window's own client pixels (a click posted there is a click on the waypoint; a view the waypoint is not in says -1 -1), so a scenario can click on what it made without knowing the fit.
static UINT const kHarnessLocate = WM_APP + 7;
// - kHarnessPage (WM_APP + 9: 8 is Drake's face-light), wParam a page number of the code-built bars: the bars show that page now (a hidden run takes a picture of each page of Track Editing, which only a
//   command of the track's handlers reaches in inteng and the editor has not all of them); the state line follows
static UINT const kHarnessPage = WM_APP + 9;
static void HarnessShowPage(int page);
// - kHarnessFaceLight, wParam a light's number from 1 (Light Sourcing): that light is selected and the camera turns on the spot to face it (the turn only: the camera's place and look stay), so a scenario can switch a light
//   that is in the picture on and off and measure what the picture does. A line "AVP_OpenInteng: face-light: ..." says what was done. Only in a -picture run.
static UINT const kHarnessFaceLight = WM_APP + 8;
void HarnessFaceLight(size_t light);
// - kHarnessCamera, wParam 0 to 3 and lParam a whole number: sets the camera's x, y, z (millimetres) or turn (0 to 4095: 0 looks along z, 1024 along x) as the scenario says. Only in a -picture run. A line "AVP_OpenInteng: camera: x y z turn" says where it is.
static UINT const kHarnessCamera = WM_APP + 10;
// - kHarnessPickPoly (WM_APP + 13: 10 is Drake's camera), wParam a row of the object list from 0, lParam a polygon number of that object's shape (bit 0x40000000: deselect): what a click on that polygon does in Texture Map, by number, so a scenario need not find the polygon on the screen
static UINT const kHarnessPickPoly = WM_APP + 13;
void TexturePolygonClicked(OpenLevelFile *open, uint8_t const *target, bool isModule, int32_t moduleIndex, int32_t polygon, bool deselect, bool copyNow);
static void HarnessLocate(int32_t index);
// - kHarnessPickModule, wParam a module's object number (the number the level file gives its object, as the Adjacent Modules screen counts them): that module is picked, as a click on it would (on the Path screen by
//   the game's module number, as that screen's click does; on Module Linking into the screen's own picks, as its plain click, LinkClick). A line "AVP_OpenInteng: pick-module: ..." says what was done. Only in a -picture run.
static UINT const kHarnessPickModule = WM_APP + 11;
// - kHarnessPickMarker, wParam an entry point marker's number from 0 (Waypoint, AI modules): that marker is picked, as a click on its cube would (the click's own rule is light_pick.h's, tested there). A line
//   "AVP_OpenInteng: pick-marker: ..." says what was done. Only in a -picture run.
static UINT const kHarnessPickMarker = WM_APP + 12;
void HarnessPickModule(int32_t index);
void LinkClick(host::PickReport const &used, bool shift, bool held);
static bool harnessScenarioAsked = false;
static void HarnessRunItem(int32_t command);
int harnessPictureAsked = 0;
static bool harnessStateAsked = false;
bool harnessPanelAsked = false;

// The menu bar, made from the table of actions for the key layout in use, with the items that can't be used now shown switched off:
// Back faces and Filled polyline until the game can show them (view_toggles.h), and the key layout already in use. Gives back false if
// the bar refuses the menus, which would be a mistake in the editor's own table. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
bool LoadTheMenus()
{
	// the bars' rows, wrapped to the window's width (1008 before the first frame has said): every title is in the window, whatever the width
	if (!EditorBar_MakeRows(barScreen, keyLayout, barWrapWidth > 0 ? barWrapWidth : 1008, PanelPass_MenuWidth, &barRows) || (barRows.menuCount[0] > 0 ? !menuBar.Load(barRows.menus[0], barRows.menuCount[0]) : (menuBar.Load(nullptr, 0), false)))
	{
		return false;
	}

	// the rows under the first; a row the bar refuses is left off, never a reason to close the editor
	barRowCount = 1;

	for (size_t row = 1; row < kEditorBarMostRows; row++)
	{
		if (row < barRows.rowCount && lowerBar[row - 1].Load(barRows.menus[row], barRows.menuCount[row]))
		{
			barRowCount = row + 1;
		}
		else
		{
			lowerBar[row - 1].Load(nullptr, 0);
		}
	}

	// the editor's own items are in whichever row they fell in: every row is told (an item a row does not hold is not found, and nothing happens)
	for (size_t row = 0; row < barRowCount; row++)
	{
		ui::MenuBar &bar = BarRow(row);

		bar.SetEnabled(kActionViewBackFaces, ViewToggles_BackFacesCanBeShown());
		bar.SetEnabled(kActionViewPolyline, ViewToggles_OutlinesCanBeShown());
		bar.SetEnabled(kActionViewFilledPolyline, ViewToggles_OutlinesCanBeShown());
		bar.SetEnabled(kActionViewGouraudLevelOnly, false);   // inteng's Gouraud state 2: not built yet
		bar.SetEnabled(kActionViewKeysClassic, keyLayout != KeyLayout::Classic);
		bar.SetEnabled(kActionViewKeysModern, keyLayout != KeyLayout::ModernWasd);
	}

	return true;
}
// A click on the level's view waits here until the next frame has been drawn, because the game's camera and its list of what's on screen
// are only that frame's right after it's drawn (pick_game.h). The place is in the view's own pixels. (Added 2026-11-14 by Melanikus
// (Frost).)
bool pickAsked = false;
bool pickInBox = false;   // the pick is a click in the 3D view's quarter of the window (the three views): its place is from the quarter's corner
bool pickWithS = false;   // S was held at the press on the Strategy Data screen (inteng's S + click: select by texture)
static bool pickDelete = false;   // the Delete key was held at the press (Module Linking's un-pick, row ML-M-61)
bool pickShift = false;   // Shift was held at the press: the click adds to the selection or takes out (inteng's Shift+LMB)
static bool pickCtrl = false;    // Ctrl was held at the press: Strategy Data's collector adds the clicked object's whole shape group (FUN_00526bf0 0x526e3b, S-79)
int pickX = 0;
int pickY = 0;

// The object list, in the list's column (object_list.h): the open level's objects, with the one selection a pick in the view uses too
// (selection.h). listSawChanges is the selection's count of changes when the list last followed it. At most kListMostRows rows show
// at once, so the rows, the menu bar's titles and the chosen row's band always fit in what one frame of the panels draws
// (kPanelPassMostExtras, panel_pass.h). (Added 2026-11-14 by Melanikus (Dietrich), F3.)
host::ObjectList objectList;
uint32_t listSawChanges = 0;
static const char kListCutWords[] = "This level has more objects than the object list holds, so the list shows only the first of them";
static_assert(host::kObjectIdBytes == host::kSelectionIdBytes, "the list and the selection know an object by the same 8 bytes");

// The level as the editor holds it, for everything it looks up in the level: the object list, the field panel's headers and strategies,
// and later the edits and the save. level_objects.h says why it isn't the game's own copy: the game lets go of that as soon as the level
// is loaded. openLevel, in main, is that hold. When the hold fails the level still shows, but there's no object list and the field panel
// has nothing to read: the status line says why, and so does a -picture run's output. Gives back null when the level is held. These
// are fixed texts of the editor's own, never the level's words. (Added 2026-11-15 by Melanikus (Dietrich).)
const char *NotHeldWords(LevelOpenResult result)
{
	switch (result)
	{
	case LevelOpenResult::Opened:
		return nullptr;
	case LevelOpenResult::NoPath:
		return "No object list: the editor has no usable path to the level's file";
	case LevelOpenResult::CantRead:
		return "No object list: the level's file couldn't be read whole";
	case LevelOpenResult::LoaderRefused:
		return "No object list: the game's loader refuses the level's file";
	case LevelOpenResult::ChangedWhileOpening:
		return "No object list: the level's file changed while it was being read";
	case LevelOpenResult::DoesntPair:
		return "No object list: the level's chunks don't match its file's, one for one";
	case LevelOpenResult::TooManyChunks:
		return "No object list: the level has more chunks than the editor takes";
	}

	return "No object list: the editor couldn't hold the level's file, for a reason it doesn't know";
}

// The object list's chosen row becomes the editor's one selection. The selection refuses an object whose ID is all 0, the game's "no
// name", and then stays as it was.
static void SelectTheChosenObject()
{
	int32_t const row = objectList.Chosen();
	const uint8_t *const id = row >= 0 ? objectList.Id((size_t)row) : nullptr;

	if (id)
	{
		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, id, host::kSelectionIdBytes);
		host::Selection_Set(picked);
	}
}

// The field panel host (field_panel_host.h), at the foot of the list's column (HostFrame_ShareTheColumn). It follows the selection as the
// list does: when the selection changes, the record the field adapters build for it (field_adapters.h) is shown. The host copies a record's
// descriptors but not the words they point to, so shownRecord keeps the record alive while it's shown, and lets go of it only after the
// host has. fieldsSawChanges is the selection's count of changes when the panel last followed it. (Added 2026-11-14 by Melanikus
// (Dietrich), F3.)
host::FieldPanelHost fieldHost;
static host::FieldRecord shownRecord;
static unsigned fieldsGeneration = 0;   // V-30b: the level generation (OpenLevelFile::Generation) the field panel's record, and the chunks it holds, are of
uint32_t fieldsSawChanges = 0;

// What the shown record writes to when a field is changed (S3d): the chunk and its kind, empty when the record has nothing to edit. And whether
// the keyboard is in the field panel: a click on one of its fields puts it there, and Esc or a click elsewhere takes it out. While it is, keys
// go to the panel and not to the camera and the shortcuts. (Added 2026-11-15 by Melanikus (Drake), S3d.)
static host::ShownEdit shownEdit;
bool fieldsHaveKeys = false;
static bool showingLevelSettings = false;   // the panel shows the level's own settings (L), not what's selected

// The folder for copies, <Documents>\AVP_OpenInteng\ (save_copy.h, SaveCopyBase), and the game's folder (set in main): a copy, an export or a screenshot is written in the first,
// never in the second. Empty when Windows gave no usable Documents folder: then nothing is saved, and the status line says why.
char theCopiesFolder[MAX_PATH] = "";
char theGameFolder[MAX_PATH] = "";

// The folder for copies is made when it isn't there yet (the user's Documents\AVP_OpenInteng), and it has to be a plain folder, not a link: the same check as the folder made inside it.
// Gives back nullptr when it can be used, else why not.
// Empty when the folder is there and usable; else why not, naming the folder (Bishop 11-16: a refusal says which path)
std::string MakeTheCopiesFolder()
{
	char checked[MAX_PATH];

	if (!theCopiesFolder[0])
	{
		return "the folder for copies (Documents\\AVP_OpenInteng) isn't known: Windows gave no usable Documents folder";
	}

	if (!CreateDirectoryA(theCopiesFolder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		return std::string("the editor couldn't make ") + theCopiesFolder + " (Windows error " + std::to_string((unsigned long long)GetLastError()) + ")";
	}

	if (char const *const why = SaveFolder_WhyNotUsable(theCopiesFolder, checked, sizeof(checked)))
	{
		return std::string(theCopiesFolder) + ": " + why;
	}

	return std::string();
}

// Shows what's selected in the field panel, or nothing when there's nothing to show. A record the panel's own check refuses is a mistake
// in an adapter: nothing is shown, and the log says which field and why (with the title read through the safe text, as it may hold a
// level's name).
extern double local_scale;   // the level's scale, as the game's loader left it (chunk_load.cpp:63)

bool OnALightScreen();

// Strategy Data draws no field panel for what is current (FUN_00524e90 draws two text lines, nothing else): the panel is let go of, and Edit Strategy opens it on purpose (our stand-in for the strategy's own view)
void LetGoOfTheFieldPanel()
{
	fieldHost.Clear();
	shownRecord = host::FieldRecord();
	shownEdit = host::ShownEdit();
	showingLevelSettings = false;
}

void ShowTheSelected(const host::Selected &selected)
{
	host::FieldRecord next;

	host::SetDisplayLevelScale(local_scale);   // a light's range is shown in metres as inteng shows it, which takes the level's scale

	// The host lets go of the record it shows first, so it never points into one that's gone
	fieldHost.Clear();
	shownRecord = host::FieldRecord();
	shownEdit = host::ShownEdit();

	// The record is built from the bytes the open level holds now, with the fields the model can set open for typing (S3d)
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::LevelStore store(open);

	showingLevelSettings = false;

	// a light selected on a Light Sourcing screen has no panel of fields: inteng DRAWS its numbers as a text block (FUN_004ac430) and changes them with the keys (+ - < >), see LightTextBlock
	if (selected.kind == host::SelectedKind::Light && OnALightScreen())
	{
		return;
	}

	if (!(open ? host::FieldAdapters_Build(selected, &next, &store, &shownEdit) : host::FieldAdapters_Build(selected, &next)))
	{
		return;
	}

	shownRecord = std::move(next);
	fieldsGeneration = LevelObjects_OpenForEdit() ? LevelObjects_OpenForEdit()->Generation() : 0;   // V-30b

	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	if (!fieldHost.Show(shownRecord.title.c_str(), shownRecord.list.data(), shownRecord.list.size(), shownRecord.values.data(), &problem,
		!shownEdit.parts.empty()))
	{
		shownEdit = host::ShownEdit();
		LogString("field panel: the record \"" + host::PanelPaint_SafeText(shownRecord.title) + "\" was refused: field "
			+ std::to_string(problem.field) + ", reason " + std::to_string((int)problem.why));
		shownRecord = host::FieldRecord();
	}
}

// Does one action of the table (editor_actions.h): it changes the editor only through what it's handed back. Quitting ends the game's
// own waits too, as closing the window does. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
// The panels over the level (L-1): which are open, and where each is this frame. PlanThePanels, below, works them out.
uint32_t openClock = 0;
uint32_t listOpenedAt = 0;     // 0 while the list is closed; otherwise when it opened (openClock), so the later one is the top one
uint32_t fieldsOpenedAt = 0;   // the same, for the field panel
ui::Box hitLower[kEditorBarMostRows - 1] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };   // the gray of each row under the first this frame, for clicks; empty for a row there is not
ui::Box hitBar = { 0, 0, 0, 0 };      // where each panel's gray is this frame, for clicks; empty while it isn't shown
ui::Box hitList = { 0, 0, 0, 0 };
ui::Box hitFields = { 0, 0, 0, 0 };
ui::Box hitStatus = { 0, 0, 0, 0 };

// The prompt (M-1): a line the user types for Find, Move relative or Name object, drawn over the level, which takes every key and every press
// until Enter does it or Esc cancels it. promptFor is one of the kPrompt numbers (editor_actions.h).
int32_t promptFor = kPromptNone;
std::string promptText;
ui::Box hitPrompt = { 0, 0, 0, 0 };
// THE BAR IS HIDDEN until asked for (FUN_004b7c50 0x4b7d12: the menu's per-frame input shows the bar, method +8 FUN_004b7dd0, only while the pointer is at the very top of the screen (y < 3) or the Control key
// (VK 0x11, DAT_00d87a5f) is held; method +8 then keeps it up while the pointer stays inside the bar's height or Control stays down, and a left press in it picks; FUN_004b6a00 draws the bar and is called nowhere else, so nothing
// draws it at any other time). barPointerY is the pointer's y in the panels' pixels from the last mouse message; barVisible is decided each frame (BarDecide).
int32_t barPointerY = -1;
int32_t barPointerX = -1;   // and its x (the Strategy view box's pointer keys, strategy_screen.cpp PointerRowInBox)
bool barVisible = false;
// inteng's menu loop is up: FUN_004b7c50 runs the bar (FUN_004b7dd0) while the bar shows, and the pop-up (+0xc) while it is open, modally, from the mode's command step; what the mode
// draws after that step is not on the screen meanwhile (reassess/dialog-look-drake.md BAR-6, BAR-8)
bool MenuLoopUp()
{
	return barVisible || AMenuIsOpen();
}
// The Hierarchy mode's questions that list their choices are inteng's list dialog (DialogList below, FUN_00475bf0): a click or Return answers with the row's place in the list, Esc cancels

// ---- inteng's DIALOGS (dialogs.h: message, confirm, text, list and the form toolkit; one open at a time, modal). These are thin names for the editor's own code.
std::string adjacentBoo;   // Adjacent Modules: the text inteng draws after a Delete ("BOO!", FUN_0057b1b0 0x404cbc), drawn at (10, 0x50) of the view until the screen is left

bool ADialogIsOpen()
{
	return host::Dialogs_IsOpen();
}

void DialogMessage(std::string const &text, std::function<void()> const &then)
{
	host::Dialogs_Message(text, then);
}

void DialogConfirm(std::string const &text, std::function<void(bool)> const &then)
{
	host::Dialogs_Confirm(text, then);
}

void DialogText(std::string const &text, std::vector<host::DialogField> const &fields, std::function<void(int32_t, std::vector<std::string> const &)> const &then)
{
	host::Dialogs_Text(text, fields, then);
}

void DialogList(std::string const &title, std::vector<std::string> const &rows, std::function<void(int32_t)> const &then, std::function<void(int32_t)> const &onRight)
{
	host::Dialogs_List(title, rows, then, onRight);
}

static void DialogPress(int32_t x, int32_t y)
{
	host::Dialogs_Press(x, y);
}

static void DialogKey(UINT message, WPARAM key)
{
	host::Dialogs_Key(message == WM_CHAR, (uint32_t)key);
}

bool editorHiddenRun = false;           // a -picture run: nobody is at the screen, so nothing is played aloud
FlyCamera *theCamera = nullptr;          // the editor's camera: Find moves it (S8, M-1) and the edits that put something at the view's place use it (S5). Set in main once the level is open, and null again before it goes.
OpenLevelFile *editedLevel = nullptr;    // the level as the editor holds it, which Move and Name change
const char *textPanelText = nullptr;      // a help text of inteng's, shown over the level until a key or a press (Keys - Mode)
const char *textPanelNext = nullptr;      // the help text that follows it (Light Sourcing's Help shows two texts in a row), or null
static char answerWords[240] = "";              // the status line's words for what the last prompt did

void OpenAPrompt(int32_t kind);
bool onHierarchy = false;                // the Hierarchy screen is the one shown (hierarchy_mode.h)
OpenLevelFile hierarchyModelFile;         // the model file (avp_huds\<name>.rif) Hierarchy mode works on once one is opened: hierarchies live in models, not levels
bool hierarchyModelOpen = false;
unsigned hierarchyModelSavedGeneration = ~0u;   // the model's generation when it was opened or last saved as a copy: inteng's "changed" DAT_00637134 is the model differing from it
int hierarchySaveAsked = 0;   // Return (C-47): 2 = "Saving" is drawn this frame, 1 = the frame with it was drawn, so the save runs now (HierarchySaveIfAsked); 0 = none
std::string openLevelName;   // the level the editor opened (its file is avp_rifs\<name>.rif in the data folder): inteng's Hierarchy mode works on the OPENED FILE
// The model and hierarchy the view shows now (so a change of either shows it again, and a change of sequence only switches it)
DWORD hierarchyAnswerUntil = 0;           // a command's own words stay on the status line until this tick; the playing view's per-frame words come back after it
bool hierarchyPromptFresh = false;        // the open prompt holds the question's default text: the first typed letter replaces it (as a selected field in inteng's dialogs does)
std::string hierarchyShownModel;
std::string hierarchyShownName;
static int promptSwallow = 0;   // a key that opened a prompt also sends its letter as a character a moment later: that one letter is not typed into the prompt
host::HierarchyMode hierarchyMode;      // its state: the current hierarchy, sequence and collection, the platform numbers
std::string hierarchyTitle;             // the words of the question the mode asked
bool IsTheHierarchyScreen(size_t screen);
static bool IsTheTrackScreen(size_t screen);
static void ShowTrackPageAfter(int32_t command);
void LeaveHierarchy();
void TextureMapCommand(int32_t command);
// OP-47 / section 8a (V-6, V-7, V-8, V-11, V-21; FUN_004d9af0 0x4dc12c-0x4dc204, 0x4dc343, 0x4dc66e): Gravity on (DAT_008f4f50) skips the current object's commands and keys; a flying object
// (DAT_008f4f44) can't be deleted and Gravity can't be switched while it flies
bool PlacementGravityOn();
void NameObjectBox(std::string const &start, std::function<void(std::string const &)> const &named);   // inteng's "Name Object" dialog (FUN_004d4c70 / FUN_004d53d0)
// Every editor object's icon as it is drawn, with the object's id, for Strategy Data's click (an icon is a thing the click can land on, as inteng's display blocks are)
static bool OnAPlacementScreen();
std::vector<std::string> promptList;   // extra lines under an open prompt: the chooser's rows (Find Small Modules)
static void FindNotDoneAdjacent();
void AdjacentEdit(int32_t what);
bool chooserActive = false;              // the Shape Chooser is open (shape_chooser.h)
std::string chooserText;                  // its lines, shown over the level
void ShapeChooserCommand(int32_t what, size_t *screen);
void CameraPositionSaved(int slot);   // FUN_004704d0 (below, with the camera slots)
bool ShapeChooserKey(WPARAM key);
void ShapeChooserPick(std::string const &typed);
void LinkExit();
void DeleteModuleLinks(bool all);
void AddReverseLinksCommand(bool all);
void AutoLinkCommand(bool all, bool deleteOld);
bool AutoLinkRunning();
void AutoLinkFrame();
void ModuleLinkingCleanTheLinks();
std::wstring levelSourcePath;   // the level file named on the command line, where it is (the editor opens its copy in avp_rifs\OpenedByEditor, open_with.h)
void DeleteLinksFrame();
bool DeleteLinksRunning();
void ShowTheWorkingBytes();   // the view gap: the game's view loads the editor's working bytes at a screen change (defined after Delete Links' run)
void AutoLinkLoadFileCommand();
void GenerateAdjacency(bool newOnly);

// MODULE LINKING'S ADJACENCY MESSAGE (module-linking-closeout-vasquez.md ML-T1; Vasquez 11-17): Exit + New Adjacencies puts "Calculating new module adjacency" (0x5e3a9c) and Exit + All
// Adjacencies, after its Yes, "Calculating adjacency data" (0x5e3a0c) on the screen at (200, 100) (FUN_0057bd40, 0x4c1e56 / 0x4c1eb1) BEFORE FUN_004c5a30 works the data out, so they stay
// while it works. Ours: the words are drawn, and the work is done two frames later (as Light Sourcing's save, lightSavePending); linkMessageShown is the last one drawn (the state line's
// linkmessage, for the harness: the words are gone when the work is done). The work ends with FUN_00403d50's questions (AD-6): the words stay under the boxes until the last answer, so
// GenerateAdjacency lets them go on each of its ways out (editor_screen_keys.cpp).
std::string linkMessage;
std::string linkMessageShown;
static int linkAdjacencyPending = 0;
static bool linkAdjacencyNewOnly = false;

static void AdjacencyWithMessage(bool newOnly)
{
	linkMessage = newOnly ? "Calculating new module adjacency" : "Calculating adjacency data";
	linkAdjacencyNewOnly = newOnly;
	linkAdjacencyPending = 2;
}

static void AdjacencyEveryFrame()
{
	if (linkAdjacencyPending > 0 && --linkAdjacencyPending == 0)
	{
		GenerateAdjacency(linkAdjacencyNewOnly);
	}
}

void DoWaypointCommand(int32_t command, EditorHostState &host);
static int waypointDragMask = 0;   // the three-view quarter (its axes) a face of a waypoint's box was taken in; the drag only follows the mouse in that quarter
static int64_t waypointViewCentre[3] = { 0, 0, 0 };   // Edit Extents' views' centre in the world (DAT_0091bee8: the master's place, moved by Shift + right drag)
static unsigned waypointViewsSeen = ~0u;             // WaypointMode::ViewsMade when the centre was last taken
static bool waypointPanning = false;                 // Shift + right drag in a pane of Edit Extents (DAT_0091bf08)
static int waypointPanPane = 0;                      // its pane (DAT_0091bf18: 0 top left, 1 top right, 2 bottom left)
static int32_t waypointPanAt[2] = { 0, 0 };          // where the mouse was, in the 640 x 480 picture (DAT_0091bf10 / bf14)
void MakeTheCurrentModuleCurrentAgain(int32_t current);
int32_t visibleDistance = 0;   // AutoLink > Set Visibility (DAT_00704378): metres x 100000; 0 keeps every module (inteng's start value is not on disk: 0 is assumed)
extern double local_scale;
void DeleteTheModuleData();
void MergeTheSelectedObjects();
static void RecalculateEntryPoints();
void LinkSetCurrent();
void LinkFollowOff();
bool RunScreenKey(uint8_t key, bool shift);
bool RunLightKey(int32_t command);
void LightKeysEveryFrame();
std::string lightMessage;      // an on-screen message of Light Sourcing (FUN_0057bd40(200, 100, 0, words)), empty for none
int lightSavePending = 0;      // frames before Save Intensities does its work (the message is seen first)
bool lightSaveTools = false;
void SaveTheCopy();
void StrategySave();   // Strategy Data's Save (FUN_005282d0): every switch's Links made again, then SaveTheCopy (below, with the switches)
void LoadTheObjectListAgain();
bool OnALightScreen();
bool PickAnEntryMarker(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
void StopFlyingTheEntryPoint(char const *why);
void FlyTheEntryPoint();
void RefreshTheEntryMarkers();
void EntryMarkerLines(std::vector<host::TrackLine> *lines);
bool entryFlying = false;
std::vector<host::EntryMarker> entryMarkers;   // the entry point markers of the AI module picked (Waypoint, AI modules)
int entryMarkerPicked = -1;
int32_t entryFlyPlace[3];
void LinkFollowPass(bool onModuleLinking);
void LinkEdit(int32_t what);
void HideObjectsCommand(int32_t what);
std::vector<uint8_t> PickedModuleIdBytes();
bool OnModuleLinkingScreen();
bool OnAdjacentModulesScreen();
void Answer(const std::string &words);
// Select With Centres (0xcd, FUN_004c1a90: flips DAT_0070437c; the screen's start sets it to 0): the click reads it (the nearest module centre, FUN_004c3760)
static bool selectWithCentres = false;
void PolyCountsCommand();
bool polyScreenOpen = false;
void PolyScreenPage(int32_t direction);
void PolyScreenPress(int32_t x, int32_t y, bool right);
bool WriteTheLinks();
// MODULE LINKING'S VIEW RULES (module-linking-reads.md section 2, FUN_004c1a90 / FUN_004c36f0 / FUN_004c17a0). inteng marks records NotVis (ObFlags bit 4, not drawn): Toggle View Distance (0x49,
// DAT_005e2c80, STARTS ON, the exe's data holds 1) makes every frame hide each record whose distance from the camera less its radius is at least the view distance (DAT_005fd678, starts at
// 1,000,000, inteng's units: 100 to the millimetre, so 10 m (H-6); the status line "View Distance :%d" shows it divided by 500,000); the keys J and M add and take away 500,000 for each frame they are held, never below 0, only while the toggle is on. Only
// Draw Linked Modules (0x4b, DAT_00704388, off on entering the mode) hides EVERY record at once and then shows the selected module and the modules linked from it; while it is on the distance rule is
// skipped and the status line reads "Draw Linked Modules Only". Turning the toggle off shows everything again. Here the hiding is LevelView_HideBeyond / HideModules / HideObjects (avpx
// R_SetEditorHiddenBlocks). NAMED: the key repeat is per frame of OUR window (60 a second, so a held J is fast; held to kViewDistanceMost); leaving the screen shows everything (other modes' use of the
// same rule, FUN_00405620, is not wired); the modules kept are those the link tree colours (the current one and the linked ones).
std::vector<int32_t> showAllModules;      // Show All's paint: the game's module numbers and their colours
std::vector<uint32_t> showAllColours;
// Module Linking's Show All HOLDS the screen (DAT_0070439c, set by 0xca at 0x4c214a; FUN_004c1a90 0x4c1aee-0x4c1b74, row ML-M-3 of module-linking-map-dietrich.md): while it is on, the frame draws
// the four legend rows and ENDS before the command is read, so no menu item or key does anything; the Escape key (slot 0x45) or a mouse button lets it go, and the modules are drawn as before
// it (the tree's colours, FUN_004c17a0). The button that lets it go is still read as a click that frame (0x4c36a8).
bool showAllHeld = false;

void ShowAllRelease()
{
	showAllHeld = false;
	showAllModules.clear();
	showAllColours.clear();
}

bool viewDistanceOn = true;
bool adjacentViewDistanceOn = false;   // the Adjacent Modules screen's own switch (DAT_00606780: off; the distance itself, DAT_005fd678, is the one Module Linking uses)
int32_t viewDistance = 1000000;
// THE DISTANCE'S UPPER BOUND (Bishop's ruling 2026-11-16, row ML-E-8 of module-linking-map-dietrich.md): 2,147,000,000, the largest multiple of 500,000 below 2^31, for every key
// that changes it, the ini and the command line. A FAULT GUARD, the only difference from inteng: inteng's keys have no bound, so a held key wraps its signed int past 2^31 and
// every record is hidden (signed overflow is undefined behaviour in C++).
static int32_t const kViewDistanceMost = 2147000000;

int32_t ViewDistanceHeld(int64_t next)
{
	return next < 0 ? 0 : (next > kViewDistanceMost ? kViewDistanceMost : (int32_t)next);
}
static bool viewDistanceFromCommandLine = false;   // -viewdistance gave it (a proof run): not read from the ini, not kept (H-7)
bool onlyDrawLinked = false;
std::vector<int32_t> linkedGameModules;   // the game's numbers of the modules the link tree colours, from UpdateModuleTints
std::string viewWords;                     // kept here so statusWords can point at it

static void SayTheViewDistance()
{
	viewWords = onlyDrawLinked ? std::string("Draw Linked Modules Only") : (viewDistanceOn ? "View Distance :" + std::to_string(viewDistance / 500000) : std::string("View distance: off"));
	statusWords = viewWords.c_str();
}

// USE POLYGON VIEW DISTANCE (0x1f6): inteng's FUN_00474ee0 sets, every frame, the haze bit (0x100) and two haze numbers (+0x154, +0x158 of the block its view object points to) of its own view block, or clears the
// bit. This program's VIEWDESCRIPTORBLOCK is not that block (its haze numbers are at +0x104 and +0x108, and the D3D renderer reads none of them), so nothing is set: the switch is the setting, kept and ticked.

// THE SCREENS' ENTRY LOOKS (view-state-table.md section 0, R-1 to R-17; Bishop verified R-1 in the listing). Each inteng screen's start reloads the level with its own load flags and
// sets what every polygon is: the CONSTRUCTION hub (FUN_00447550 0x4475e0-0x447626: flags 0x82, no textures, every level shape painted white, every polygon 0xe | 2), MODULE LINKING
// (FUN_004c3a00, 0x481), ADJACENT MODULES (FUN_00404780, 0x481), PATH (FUN_004f9780, 0x483) and WAYPOINT (FUN_00581650, 0x83) all start as a WHITE WIREFRAME, unlit and see-through;
// Object Placement (0x6017), Texture Map (0x22007), Light Sourcing (0x8253), Hierarchy (0x3) and Strategy Data (0x13) start TEXTURED. Every mode's Exit goes through the hub's start
// again, so leaving a screen gives the hub's wire back. A screen's own View Mode items then change it (Module Linking 0x46 / 0x47 / 0x48, Adjacent Modules 0xc8 / 0xc9): Polyline is
// the white wire again, the others leave it for the draw mode they pick. The wire itself is the Module Linking look (view_toggles_game.h, ViewToggles_SetModuleWire).
// Module Linking's pop-up ("Module Linking: links", a screen of its own here) is the same screen as Module Linking: going to it is not an entry.
// Waypoint's green modules (R-11) are the AI modules mode's paint (waypoint_mode.h, UpdateModuleTints). NOT YET (rows of the table): Module Linking's hidden see-through polygons (flag 0x10 -> 1, R-3),
// Light Sourcing's and Hierarchy's unlit shapes (R-14, R-15).
static bool screenWire = true;   // the first frame is the hub's
static std::string screenWireFor = "Construction";

static std::string ScreenLookGroup(size_t screen)
{
	if (screen >= host::kIntengScreenCount)
	{
		return std::string();
	}

	char const *const name = host::kIntengScreens[screen].name;

	return strncmp(name, "Module Linking", 14) == 0 ? std::string("Module Linking") : std::string(name);
}

static bool ScreenStartsAsWire(std::string const &group)
{
	return group == "Construction" || group == "Module Linking" || group == "Adjacent Modules" || group == "Path" || group == "Waypoint";
}

// Once a frame, before the level is drawn: entering a screen gives it its start's look. On a wire screen a change of the draw mode (its View Mode items, the 8 / 9 keys that
// Module Linking, Adjacent Modules and Path read in slots DAT_00d87a78 / DAT_00d87a79: R-5, R-6, R-8, R-10) picks the look: Polyline is the white wire (FUN_0046daa0), Filled polyline
// the white hidden-line look (FUN_0046dae0), Polygon white and lit (FUN_0046db20); these screens have no textures loaded (ViewToggles_SetScreenNoTextures).
static void ScreenLookPass()
{
	static host::DrawMode drawModeSeen = host::DrawMode::Polygon;
	std::string const group = ScreenLookGroup(barScreen);
	bool const wireScreen = ScreenStartsAsWire(group);
	host::DrawMode const drawModeNow = ViewToggles_Now().drawMode;

	if (group != screenWireFor)
	{
		screenWireFor = group;
		screenWire = wireScreen;
	}
	else if (wireScreen && drawModeNow != drawModeSeen)
	{
		screenWire = drawModeNow == host::DrawMode::Polyline;
	}

	drawModeSeen = drawModeNow;
	ViewToggles_SetModuleWire(screenWire);
	ViewToggles_SetScreenNoTextures(wireScreen);
}

static void ModuleLinkingViewPass(bool onModuleLinking, bool onAdjacent)
{
	static bool applied = false;
	static bool wasOnModuleLinking = false;

	// THE SCREEN'S START (FUN_004c3a00 0x4c3a03-0x4c3b0e, row ML-E-2 of module-linking-map-dietrich.md): Mixed, Shift+S's follow and Only Draw Linked Modules off, Select With
	// Centres off, "the last hidden" forgotten (the hidden ones stay hidden). Toggle View Distance keeps its value (inteng does not reset DAT_005e2c80), and so does the distance.
	if (onModuleLinking && !wasOnModuleLinking)
	{
		ModuleLinkingCleanTheLinks();   // FUN_004c90c0, called first (0x4c3a15): the links that name no module taken out (row ML-E-12)
		ViewToggles_SetMixed(false);
		LinkFollowOff();
		onlyDrawLinked = false;
		selectWithCentres = false;
		host::HiddenObjects_ForgetLast();
	}

	wasOnModuleLinking = onModuleLinking;

	if (onAdjacent)
	{
		// Adjacent Modules' View Distance (FUN_00404a20 0x40555f-0x4055e7): when on, J adds and M takes away 500,000 each frame, never below 0, and every frame the modules beyond the distance are hidden (FUN_00405620)
		applied = true;

		if (adjacentViewDistanceOn && (keyDown['J'] || keyDown['M']))
		{
			int64_t const next = (int64_t)viewDistance + (keyDown['J'] ? 500000 : 0) - (keyDown['M'] ? 500000 : 0);

			viewDistance = ViewDistanceHeld(next);
			viewWords = "View Distance :" + std::to_string(viewDistance / 500000);
			statusWords = viewWords.c_str();
		}

		LevelView_HideByRule(nullptr, 0, false);
		LevelView_HideBeyond(adjacentViewDistanceOn, viewDistance / 100);   // inteng's DAT_005fd678 is 100 to the millimetre (H-6); the hiding is in millimetres
		return;
	}

	if (!onModuleLinking)
	{
		if (applied)
		{
			ViewToggles_SetMixed(false);
			showAllModules.clear();
			showAllColours.clear();
			LevelView_HideByRule(nullptr, 0, false);
			LevelView_HideBeyond(false, 0);
			applied = false;
		}

		return;
	}

	applied = true;

	if (viewDistanceOn && !onlyDrawLinked && (keyDown['J'] || keyDown['M']))
	{
		int64_t const next = (int64_t)viewDistance + (keyDown['J'] ? 500000 : 0) - (keyDown['M'] ? 500000 : 0);

		viewDistance = ViewDistanceHeld(next);
		SayTheViewDistance();
	}

	if (onlyDrawLinked)
	{
		std::vector<int32_t> others;

		for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
		{
			if (std::find(linkedGameModules.begin(), linkedGameModules.end(), m) == linkedGameModules.end())
			{
				others.push_back(m);
			}
		}

		LevelView_HideByRule(others.data(), others.size(), true);
		LevelView_HideBeyond(false, 0);
	}
	else
	{
		LevelView_HideByRule(nullptr, 0, false);
		LevelView_HideBeyond(viewDistanceOn, viewDistance / 100);   // inteng's DAT_005fd678 is 100 to the millimetre (H-6); the hiding is in millimetres
	}
}

// The action number of the item of a table that has this command, for the ticks (0 when there is none)
int32_t ActionOfCommand(char const *table, int32_t command)
{
	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		if (strcmp(host::kIntengScreens[s].table, table) != 0)
		{
			continue;
		}

		for (size_t m = 0; m < host::kIntengScreens[s].count; m++)
		{
			for (size_t i = 0; i < host::kIntengScreens[s].menus[m].count; i++)
			{
				if (host::kIntengScreens[s].menus[m].items[i].command == command)
				{
					return host::kIntengScreens[s].menus[m].items[i].action;
				}
			}
		}
	}

	return 0;
}

// The same for Hierarchy's code-built bar (its first row, page 0: AvP's set 1), found by the screen's name (a code-built screen has no table address)
int32_t HierarchyBarAction(int32_t command)
{
	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		if (strcmp(host::kIntengScreens[s].name, "Hierarchy") != 0)
		{
			continue;
		}

		for (size_t m = 0; m < host::kIntengScreens[s].count; m++)
		{
			for (size_t i = 0; i < host::kIntengScreens[s].menus[m].count; i++)
			{
				if (host::kIntengScreens[s].menus[m].items[i].command == command)
				{
					return host::kIntengScreens[s].menus[m].items[i].action;
				}
			}
		}
	}

	return 0;
}

void UpdateModuleTints(bool onAdjacent);
std::string ModuleNameOfIndex(int32_t index);
void ToggleModuleOverlaps();
static bool SmallModuleStep(bool back);
static void SmallModulesForget();
static void TurnTheSelectedQuarter();
void UpdateHiddenModules();
static void SetTheBaseModule();
static void ChangeTheObjectType(int32_t which);
void StrategyModeCommand(int32_t command);
void RunStrategyDelete(std::string const &typed);
int strategyPick = 0;   // a click in the view is waited for: 1 a target (T, Change Target)
static void RunVirtualName(std::string const &typed);
size_t KindsTheObjectCanTake(uint8_t const *id, int32_t *kinds);
std::vector<host::VirtualObjectInfo> destroyChoices;
uint8_t destroyId[8];
uint8_t assignId[8];
int32_t assignKind = 0;
int64_t virtualPlace[3];
uint8_t stabilityId[8];
void PathModeCommand(int32_t command);
bool OnThePathScreen();
void RunPathPrompt(int32_t kind, std::string const &typed);
static void StopTheTypeQuestions();
static void AltLocationCommand(int32_t command);

// OP-29: a player start's name as inteng shows it. FUN_004216d0 names a new one "Default name" (0x5cd0cc), but FUN_00422110 then names EVERY player start "Player Start %d" (0x5cd1e0),
// 1, 2, ... in the list's order, at the screen's start (0x4d90fb) and after each Create (0x4db30f): the order of the level's AVPSTART chunks, a new one last. `number` from 0.
static std::string PlayerStartName(size_t number)
{
	return "Player Start " + std::to_string((unsigned long long)number + 1);
}
static void AltAssignGroup(int64_t typed);
static void AltChangeShown(int64_t typed);
static void PromptKey(UINT message, WPARAM key);

static void ToggleTheObjectList();   // below, with the panels over the level (L-1)
static void CloseTheTopPanel();
static bool OnAPanel(int32_t x, int32_t y);


// The level's own settings in the field panel (L), editable, in place of what's selected: the sky colour and kit, the enemy generated, the scale
// and the sound folder, as the open level holds them (field_adapters.h). Shown until the selection changes. (Added 2026-11-15 by Melanikus
// (Drake), L.)
static void ShowTheLevelSettings()
{
	host::FieldRecord next;

	fieldHost.Clear();
	shownRecord = host::FieldRecord();
	shownEdit = host::ShownEdit();
	showingLevelSettings = true;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::LevelStore store(open);

	if (!open || !host::FieldAdapters_BuildLevelSettings(&store, &next, &shownEdit))
	{
		showingLevelSettings = false;
		statusWords = "This level has no settings the editor can show";
		return;
	}

	shownRecord = std::move(next);
	fieldsGeneration = LevelObjects_OpenForEdit() ? LevelObjects_OpenForEdit()->Generation() : 0;   // V-30b

	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	if (!fieldHost.Show(shownRecord.title.c_str(), shownRecord.list.data(), shownRecord.list.size(), shownRecord.values.data(), &problem, true))
	{
		showingLevelSettings = false;
		shownEdit = host::ShownEdit();
		LogString("field panel: the level settings were refused: field " + std::to_string(problem.field) + ", reason " + std::to_string((int)problem.why));
		shownRecord = host::FieldRecord();
		return;
	}

	fieldsOpenedAt = ++openClock;
}

// What the panel shows, made again from the bytes the level holds: the settings if it shows them, else what's selected
void ShowTheFieldsAgain()
{
	if (showingLevelSettings)
	{
		ShowTheLevelSettings();
	}
	else
	{
		ShowTheSelected(host::Selection_Get());
	}
}

// The object list made again from the level the editor holds now, and what the field panel showed of the old one let go of. For after an
// edit that swaps the level (object_edit.h), which leaves no Chunk * of the old level alive. (Added 2026-11-15 by Melanikus (Drake), S5.)
// V-30b, for the scenario line (fieldsfresh): the field panel holds no record, or one of the level held now. A swap (ReplaceLevel) leaves the old level's chunks gone, so a record
// of an older generation points at nothing: the edit that swaps must let go of it (LoadTheObjectListAgain).
static bool FieldsFresh()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	return (shownRecord.title.empty() && shownRecord.list.empty()) || !open || fieldsGeneration == open->Generation();
}

void LoadTheObjectListAgain()
{
	std::vector<host::LevelObject> found;

	LevelObjects_Read(&found);
	objectList.Load(found.data(), found.size());
	showingLevelSettings = false;
	fieldHost.Clear();
	shownRecord = host::FieldRecord();
}


bool specialNameGiven = false;   // SpecialRunCommand is running again from the Name Object dialog's OK

// THE PLACED HIERARCHIES of Object Placement (OP-2, OP-105, NA-6): inteng makes an object of class 0x5ae6c4 (kind 5) for every PLACHIER of SPECLOBJ on entry (FUN_004e8cc0 <- 0x4d8e96), drawn as the
// editor's icon shape 4 at its place with its turn (FUN_004e4ae0(5, ..., shape 4) at 0x4e8ce3-0x4e8d4a); it is in the placement list, so the click, Find and Num Objects take it, and it can be THE current
// object (its select FUN_004e9b20 also puts the right-click pop-up on page 6: NOT BUILT here yet). The list is read again whenever the level changes (PlacementFrame).
std::vector<host::PlacedHierarchyInfo> placementHierarchies;
int placedHierarchyCurrent = -1;   // its place in placementHierarchies; -1: none is current

SoundDefaults soundDefaults;

// Object > Edit / E on a SOUND (OP-133a..c): inteng's Sound Edit window 1:1 (below, with the hierarchy's sound window)

// Object > Find (0x19) on Object Placement (OP-10; FUN_004d9af0 0x4da22b-0x4da528): inteng's list dialog FUN_00475bf0 titled "Select Object" (0x5cf744) holds the name of EVERY object of the placement list, in the
// list's order (OP-2: the placed objects in the level's order, then the SPECLOBJ children in the file's order); Escape: nothing. A pick: the VIEW jumps to the object's place (the camera block's place, its turn
// kept) and the object is made current (FUN_004d64a0, OP-40). The placed hierarchies are listed too (since 35cc2522; inteng's list holds them, FUN_004d68e0 0x4d8e96 / 0x4d8eec); NAMED: ours lists them after
// every special object, inteng's between the sounds and the particle generators (the dialog sorts its rows, so only two of one name could show it). (Note made current 11-17 by Drake, OP-10.)
static std::string placementFindWords;

void PlacementFind()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<host::PolyCountRow> rows;
	std::vector<host::SpecialObject> specials;

	if (LevelObjects_Level())
	{
		host::PolyCounts_Read(LevelObjects_Level(), &rows);
	}

	if (open)
	{
		host::SpecialObjects_Read(open, &specials);
	}

	std::vector<host::PolyCountRow> placed;
	std::vector<std::string> names;

	for (host::PolyCountRow const &row : rows)
	{
		if (row.placed)
		{
			placed.push_back(row);
			names.push_back(row.name);
		}
	}

	for (host::SpecialObject const &one : specials)
	{
		names.push_back(one.kind == host::SpecialKind::PlayerStart ? PlayerStartName(one.number) : one.name);   // OP-29: FUN_00422110 names every player start "Player Start %d" in order
	}

	std::vector<host::PlacedHierarchyInfo> hierarchies;

	if (open)
	{
		host::PlacedHierarchies_Read(open, &hierarchies);
	}

	for (host::PlacedHierarchyInfo const &one : hierarchies)
	{
		names.push_back(one.name);   // NAMED: inteng lists them among the SPECLOBJ children in the file's order; here after the other special objects (the dialog sorts the names)
	}

	DialogList("Select Object", names, [placed, specials, hierarchies](int32_t at)
	{
		if (at < 0 || (size_t)at >= placed.size() + specials.size() + hierarchies.size())
		{
			return;
		}

		placedHierarchyCurrent = -1;

		double const scale = local_scale > 0.0 ? local_scale : 1.0;
		int32_t const *place = nullptr;
		std::string name;

		if ((size_t)at < placed.size())
		{
			host::Selected picked = {};

			picked.kind = host::SelectedKind::Object;
			memcpy(picked.objectId, placed[(size_t)at].id, host::kSelectionIdBytes);
			specialMode.haveCurrent = false;
			host::Selection_Set(picked);
			place = placed[(size_t)at].place;
			name = placed[(size_t)at].name;
		}
		else if ((size_t)at >= placed.size() + specials.size())
		{
			host::PlacedHierarchyInfo const &one = hierarchies[(size_t)at - placed.size() - specials.size()];

			host::Selection_Clear();
			specialMode.haveCurrent = false;
			placedHierarchyCurrent = (int)one.number;
			place = one.place;
			name = one.name;
		}
		else
		{
			host::SpecialObject const &one = specials[(size_t)at - placed.size()];

			host::Selection_Clear();
			specialMode.haveCurrent = true;
			specialMode.kind = one.kind;
			specialMode.number = one.number;
			place = one.place;
			name = one.name;
		}

		if (theCamera)
		{
			// a hostile place times the scale is held inside what an int holds before the camera's own limits (FlyCamera_Hold)
			auto const held = [scale](int32_t file) { double const world = file * scale; return world > 2147483000.0 ? 2147483000 : (world < -2147483000.0 ? -2147483000 : (int)world); };

			theCamera->x = held(place[0]);
			theCamera->y = held(place[1]);
			theCamera->z = held(place[2]);
			FlyCamera_Hold(theCamera);
		}

		placementFindWords = "Selected: " + (name.empty() ? std::string("(no name)") : name);
		Answer(placementFindWords);
	});
}


// Construction > Miscellaneous > Fix tears between Objects (0x5a Selected, 0x5b All; FUN_0044ad20 / FUN_0044b1f0, p11-shape-handlers.md sections 4, 8, 47, 48): the tolerance in millimetres is typed first
// (inteng: "Fixing tears for selected objects" / "... for ALL objects", "Type in tolerance level for tear fixing (in mm)", the number kept in DAT_005ced28, 5 to start with), then the weld. Selected
// wants exactly two objects ("You need to select two objects for tear fixing") and neither placed ("Tear fixing doesn't work on placed objects"); the FIRST selected is the one whose points move.
bool ModuleObject(OpenLevelFile *open, int32_t moduleIndex, uint8_t id[8]);
int32_t tearTolerance = 5;
bool tearAll = false;

static void FixTearsStart(bool all)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		Answer("Fix tears: no level is open");
		return;
	}

	if (!all && host::Selection_SetCount() != 2)
	{
		DialogMessage("You need to select two objects for tear fixing");   // the message box (FUN_00474030, 0x445d8a)
		return;
	}

	tearAll = all;
	OpenAPrompt(kPromptTears);
}

static void FixTearsDo(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	char *end = nullptr;
	long const value = strtol(typed.c_str(), &end, 10);

	if (typed.empty() || !end || *end != '\0' || value < 0 || value > 1000000)
	{
		Answer("Fix tears: the tolerance is a whole number of millimetres, 0 to 1000000");
		return;
	}

	if (!open || !open->Level())
	{
		Answer("Fix tears: no level is open");
		return;
	}

	tearTolerance = (int32_t)value;

	static std::string words;

	if (tearAll)
	{
		host::FixTearsAllCounts counts;

		host::FixTearsAll(open, open->Level(), tearTolerance, &counts);
		words = "Fixing tears for all objects: " + std::to_string((unsigned long long)counts.pairs) + " pairs of " + std::to_string((unsigned long long)counts.objects) + " objects welded (" +
			std::to_string((unsigned long long)counts.welded) + " points together)";

		if (counts.refused != 0)
		{
			words += "; " + std::to_string((unsigned long long)counts.refused) + " refused (" + counts.firstWhy + ")";
		}
	}
	else
	{
		uint8_t moved[8], reference[8];

		if (host::Selection_SetCount() != 2 || !host::Selection_SetMember(0, moved) || !host::Selection_SetMember(1, reference))
		{
			Answer("You need to select two objects for tear fixing");
			return;
		}

		if (host::ShapeChooser_ObjectIsPlaced(open->Level(), open, moved) || host::ShapeChooser_ObjectIsPlaced(open->Level(), open, reference))
		{
			Answer("Tear fixing doesn't work on placed objects");
			return;
		}

		size_t welded = 0;
		std::string why;
		host::ShapeEditResult const result = host::ShapeEdit_FixTears(open, open->Level(), moved, reference, tearTolerance, &welded, &why);

		if (result != host::ShapeEditResult::Done)
		{
			Answer("Fix tears: " + (why.empty() ? std::string(host::ShapeEditWords(result)) : why));
			return;
		}

		words = "Fixing tears for the selected objects: " + std::to_string((unsigned long long)welded) + " points of the first welded onto the second within " + std::to_string((int)tearTolerance) + " mm";
	}

	Answer(words);
	LoadTheObjectListAgain();
	CameraPositionSaved(10);   // then the hub is left and entered again: FUN_004476d0 (FUN_0044ad20 0x44b189, FUN_0044b1f0 0x44ba69; camera-saved-vasquez.md CS-2)
	RotationCentreDefault();   // and its set-up FUN_00447550 (0x44b18e, 0x44ba6e) puts the rotation centre back on the objects' average (0x447652, CAM-7)
}

// Texture Map > Recentre Shape (0xd6, FUN_00565bd0): "Recentre object: Select polygon in object to recentre that object to the centre of the polygon". The click names the object and the file polygon; the
// shape is moved so that the polygon's centre is the origin and every user's place is moved the other way (shape_edit.h). Right button cancels (the click mode's).
static const int32_t kClickRecentrePoly = 0xd6;

static void RecentrePolygonStart()
{
	bool const waiting = host::ClickMode_Start(kClickRecentrePoly, host::ClickWants::Polygon, "Recentre object");

	Answer(waiting ? "Recentre object: select a polygon in the object to recentre that object to the centre of the polygon (right button: cancel)" : "That command can't wait for a click");
}

static void RecentrePolygonClicked(host::PickReport const &report)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera || !report.hasHit)
	{
		Answer("Recentre object: the click found no polygon");
		return;
	}

	uint8_t target[8];

	if (report.found == host::PickFound::Object)
	{
		memcpy(target, report.objectId, 8);
	}
	else if (!ModuleObject(open, report.moduleIndex, target))
	{
		Answer("Recentre object: the level file has no single object for that module");
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
		Answer("Recentre object: no polygon of that object under the click");
		return;
	}

	size_t users = 0;
	std::string why;
	host::ShapeEditResult const result = host::ShapeEdit_RecentreOnPolygon(open, open->Level(), target, hit.polygon, &users, &why);

	if (result != host::ShapeEditResult::Done)
	{
		Answer("Recentre object: " + (why.empty() ? std::string(host::ShapeEditWords(result)) : why));
		return;
	}

	Answer("Recentred: polygon " + std::to_string(hit.polygon) + " is now the centre of the shape, " + std::to_string((unsigned long long)users) + " object place(s) kept in the world");
	LoadTheObjectListAgain();
	TextureRecentredBox(target);   // inteng's "Recentred %s" box (texture_map_screen.cpp)
}

// Object Placement > Misc > Toggle Split Screen (0x52, FUN_004d9af0 0x4da0c0-0x4da17e; FUN_004d5980 enters, FUN_004d5d80 leaves, FUN_004d58e0 places the cameras): DAT_008f4f60 flips. On, the window is
// cut in four: the main view goes to the bottom right quarter and three more perspective renders of the level fill the others, each 500000 (5 m) back from the current object (or from the view when there is
// none) along z, x and y, looking along +z, +x and +y (the matrices 0x10000 / 0xffff0000 of FUN_004d5980). READ: which quarter takes which is not (the track screens' layout is used: z top left, x top
// right, y bottom left). The picture is drawn by LevelView_SetExtraViews (level_view.h). The markers are not drawn while it is on.
bool placementSplit = false;
bool splitCentreOn = false;      // the split screen's three views are put round this place (OP-45 / OP-46 / OP-40); off: round the camera
static int32_t splitCentre[3] = { 0, 0, 0 };   // game units (mm)
static bool placementText = true;       // T: inteng's text switch DAT_005fd69c (1 at start; T flips it, 0x4dd0cd-0x4dd0ef; the frame's text is drawn only when it is on, 0x4dd126): all of Object Placement's text lines, and the names beside the markers (the owner's addition) with them
static bool LinkedObjectWorld(int32_t out[3], int32_t *radius = nullptr, bool *special = nullptr, uint8_t *idOut = nullptr);

// OP-45 / OP-46 / OP-40 (section 8a V-3, 8b): the place FUN_004d5980 and FUN_004d58e0 put the three views round, ONCE: the current object's block place (DAT_008f4f3c[1] +0x10.., where it flies too:
// LinkedObjectWorld), else the view's (DAT_00962308 +0x10.., 0x4da105 / 0x4da161). The views then stay there: only the split's start, D and another object made current (FUN_004d64a0 -> FUN_004d58e0)
// move them; the camera and Backspace (FUN_004d64a0(0)) don't. Corrected 2026-11-16 by Melanikus (Vasquez): the views followed the camera every frame.
static void SplitCentreOnCurrent()
{
	int32_t world[3];

	if (LinkedObjectWorld(world))
	{
		memcpy(splitCentre, world, sizeof(splitCentre));
	}
	else if (theCamera)
	{
		splitCentre[0] = theCamera->x;
		splitCentre[1] = theCamera->y;
		splitCentre[2] = theCamera->z;
	}

	splitCentreOn = true;
}

static void SplitScreenCommand()
{
	placementSplit = !placementSplit;

	// FUN_004d5980 (on) places the views; FUN_004d5d80 (off) frees them
	if (placementSplit)
	{
		SplitCentreOnCurrent();
	}
	else
	{
		splitCentreOn = false;
	}

	Answer(placementSplit ? "Toggle Split Screen: on (three more views of the level 5 m from the current object, back along z, x and y; the main view is the bottom right)" : "Toggle Split Screen: off");
}

// Object Placement > Object > Move relative (0x1c; FUN_004d9af0 calls the current object's slot +0x1c, which is FUN_004e1d70 for a placed SHAPE object only, a no-op in the special objects' classes): a dialog
// (FUN_0057c800(100, 100)) with three fields shows (module place - object place) x 1e-5 per axis as "%-3.5f" metres, the module being the one the object is in (the +0x110 inteng's per-frame line "In Object %s -
// radius %d" keeps); on OK the object's place becomes the module's minus what was typed. The module is the one the game's view puts the object's place in (LevelView_ModuleAt) and its world place is
// MODULE.m_world (NAMED: assumed to be what inteng's +0x10..+0x18 of the module's record holds).
// THE WINDOW 1:1 (Vasquez 11-17, listing 0x4d5190-0x4d5358 and 0x4e1d70-0x4e1f87): "RelMove Shape", 400 x 200; "Type in the new coordinates." at (18, 18, 370 x 40); three fields at (30, 100 / 124 / 148),
// 300 x 20, up to 200 letters, each the axis's offset in metres ("%-3.5f" of (module - object) x 1e-5, inteng's unit); Ok at (100, 180), Cancel at (200, 180), 50 x 16; Return is Ok. Ok (answer 2): each
// field read by sscanf "%lf" (0x5e8af8), the object's place in inteng's unit = module's - ftol(value x 100000.0) (0x5ae598), then saved to the file cut toward 0 (FUN_004dfc70's __ftol).
// INTENG BUG, FIXED (MAPPING-ORDERS section 15): a field that is no number left sscanf's double unset (stack bytes), so that axis went to a garbage place; here that axis stays where it was, as does
// one whose number would not fit inteng's whole numbers.
bool SelectedObjectId(uint8_t *id);
static int32_t moveRelModule = -1;
static uint8_t moveRelModuleMover[8];

// FUN_004e1d70's Ok on one axis: false (the axis kept) for a text that is no number or a value that does not fit
static bool MoveRelativeAxis(std::string const &typed, int32_t moduleWorld, double scale, int32_t *file)
{
	double value = 0.0;

	if (sscanf_s(typed.c_str(), "%lf", &value) != 1 || !(fabs(value * 100000.0) < 2147483647.0))
	{
		return false;
	}

	int64_t const offset = (int64_t)(value * 100000.0);                    // __ftol: toward 0
	double const placeInteng = (double)moduleWorld * 100.0 - (double)offset;   // inteng's unit, 1e-5 m: the game's millimetres x 100
	double const placeFile = placeInteng / (100.0 * scale);                 // FUN_004dfc70: back to the file's unit, cut toward 0

	if (!(fabs(placeFile) < 2147483647.0))
	{
		return false;
	}

	*file = (int32_t)placeFile;
	return true;
}

// FUN_004d5190's window, its fields filled by FUN_004e1d70 (0x4e1dab-0x4e1e81)
static void MoveRelativeWindow(int32_t const place[3], int32_t const moduleWorld[3])
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	host::DialogFormSpec spec;
	host::DialogWidget label;

	spec.title = "RelMove Shape";
	spec.width = 400;
	spec.height = 200;

	label.type = host::DialogWidgetType::Label;
	label.box = { 0x12, 0x12, 0x172, 0x28 };
	label.text = "Type in the new coordinates.";
	spec.widgets.push_back(label);

	for (int axis = 0; axis < 3; axis++)
	{
		host::DialogWidget field;
		char shown[64];

		_snprintf_s(shown, sizeof(shown), _TRUNCATE, "%-3.5f", ((double)moduleWorld[axis] - (double)place[axis] * scale) / 1000.0);   // (module - object) in 1e-5 m, x 1e-5
		field.type = host::DialogWidgetType::Edit;
		field.box = { 0x1e, 0x64 + 0x18 * axis, 0x12c, 0x14 };
		field.text = shown;
		field.most = 0xc8;
		spec.widgets.push_back(field);
	}

	host::DialogWidget okButton;

	okButton.type = host::DialogWidgetType::Button;
	okButton.box = { 0x64, 0xb4, 0x32, 0x10 };
	okButton.text = "Ok";
	okButton.answer = host::kDialogOk;
	spec.widgets.push_back(okButton);

	host::DialogWidget cancelButton;

	cancelButton.type = host::DialogWidgetType::Button;
	cancelButton.box = { 0xc8, 0xb4, 0x32, 0x10 };
	cancelButton.text = "Cancel";
	cancelButton.answer = host::kDialogEscape;
	spec.widgets.push_back(cancelButton);

	host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		int32_t moduleNow[3], placeNow[3];
		double const scaleNow = local_scale > 0.0 ? local_scale : 1.0;

		if (answer != host::kDialogOk || !open || !open->Level() || moveRelModule < 0 || !LevelView_ModuleWorld(moveRelModule, moduleNow) ||
			host::ObjectPlace_Get(open, open->Level(), moveRelModuleMover, placeNow) != host::MoveRelPolyEditResult::Done)
		{
			return;
		}

		int axis = 0;

		for (host::DialogWidget const &w : widgets)
		{
			if (w.type == host::DialogWidgetType::Edit && axis < 3)
			{
				MoveRelativeAxis(w.text, moduleNow[axis], scaleNow, &placeNow[axis]);
				axis++;
			}
		}

		host::MoveRelPolyEditResult const result = host::ObjectPlace_Set(open, open->Level(), moveRelModuleMover, placeNow);

		if (result != host::MoveRelPolyEditResult::Done)
		{
			Answer(std::string("Not moved: ") + host::MoveRelPolyEdit_Words(result));
			return;
		}

		Answer("Moved: the object's place is the module's place less what was typed");
		LoadTheObjectListAgain();
	});
}

static void MoveRelativeModuleStart()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];

	if (!open || !open->Level() || host::Selection_Get().kind != host::SelectedKind::Object || !SelectedObjectId(id))
	{
		Answer("Move relative: select a placed object first");
		return;
	}

	int32_t place[3];

	if (host::ObjectPlace_Get(open, open->Level(), id, place) != host::MoveRelPolyEditResult::Done)
	{
		Answer("Move relative: that object has no place the editor can read");
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	int32_t const world[3] = { (int32_t)(place[0] * scale), (int32_t)(place[1] * scale), (int32_t)(place[2] * scale) };
	int32_t const module = LevelView_ModuleAt(world);
	int32_t moduleWorld[3];

	if (module < 0 || !LevelView_ModuleWorld(module, moduleWorld))
	{
		Answer("Move relative: that object is not inside a module");
		return;
	}

	memcpy(moveRelModuleMover, id, 8);
	moveRelModule = module;
	MoveRelativeWindow(place, moduleWorld);
}

// Object Placement > Misc > Toggle Gravity (0x58, FUN_004d9af0 0x4dc67c-0x4dc6e0): DAT_008f4f50 flips (only when DAT_008f4f44 == 0), the menu tick follows, and every object of inteng's list gets its vtable
// slot +0x50 called with the new value. That slot is a no-op in 19 of the 20 placed-object classes (the five special ones among them); in class 0x5ae538 (a placed SHAPE object) FUN_004e3e30 does this for
// the kinds 1, 2, 3, 4, 5 and 15 only: on, the object's game block gets dynamics type 7, loses bit 0x2000 of its +0x5c and gains bit 0x10000000 of its +0x60 (it falls while inteng's view runs), and +0x1c4
// = 9; off, +0x1c4 = 0: inteng's ENGINE then moves those blocks (they fall and land), and the level file changes only when the objects are saved after (Mode > Save, Exit, Tab: each object's save
// writes its block's place). Ours (Drake 11-17, Bishop's ruling: the game's own code): LevelView_SetGravity gives the game's inanimate objects of those types to avpx's ObjectDynamics, which runs
// each frame of the screen while Gravity is on (LevelView_GravityStep); PlacementGravityFlush writes the places the engine moved them to into the level before a save.
bool placementGravity = false;
static void PlacementGravityFlush();

bool PlacementGravityOn()
{
	return placementGravity;
}

// Object Placement's LIGHTING (OP-3, OP-44): placement_lighting.h

static void GravityToggleCommand()
{
	// OP-47 / OP-119 (V-21): only when no object flies (0x4dc66e: DAT_008f4f44 == 0); otherwise nothing
	if (PlacementIsFlying())
	{
		return;
	}

	// off: the places the engine moved the objects to go into the level now (the blocks stay where they landed, and a later save writes them, as inteng's object saves do)
	if (placementGravity)
	{
		PlacementGravityFlush();
	}

	placementGravity = !placementGravity;

	bool const running = LevelView_SetGravity(placementGravity);

	Answer(placementGravity ? (running ? "Toggle Gravity: on" : "Toggle Gravity: on (the game's dynamics can't run on this level: it has no player body)") : "Toggle Gravity: off");
}

// The places Gravity's engine moved objects to, written into the level (each object's OBJHEAD1 place and turn, as Move relative writes them), before a save: inteng's object save writes its
// block's place (Mode > Save, Exit, Tab). The level's scale turns the game's units into the file's.
static void PlacementGravityFlush()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<LevelViewMovedObject> moved;

	if (!placementGravity || !open)
	{
		return;
	}

	LevelView_GravityMoved(&moved);

	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	for (LevelViewMovedObject const &one : moved)
	{
		model::AltLocation where;

		for (int k = 0; k < 3; k++)
		{
			where.place[k] = (int32_t)((double)one.world[k] / scale);
		}

		host::ObjectMake_TurnFromMatrix(one.matrix, where.turn);
		host::AlternateLocations_PutOnObject(open, one.id, where);
	}
}

// Leaving the screen (Exit, Tab, another mode): the moved places written, the blocks given back (inteng's DAT_008f4f50 is 0 again at the next entry, 0x4d8409)
static void PlacementGravityLeave()
{
	if (placementGravity)
	{
		PlacementGravityFlush();
		LevelView_SetGravity(false);
		placementGravity = false;
	}
}

// Object Placement > Misc > Snap To Vertex (0x4e2; FUN_004dd280 starts it with the current object, FUN_004f7000 does it under the mouse): the current object (the special object just made or found, else the
// selected one) goes onto the vertex of the clicked object's shape that is nearest the click (its own vertex nearest that one is the one that lands on it; a special object is a point, so its place does).
// A PLACED object snaps live, as inteng's (SnapLiveStart, after PlacementFlyStop). NAMED: a special object (a sound, a generator: a point here) keeps this click, which snaps it once.
bool SelectedObjectId(uint8_t *id);
bool ModuleObject(OpenLevelFile *open, int32_t moduleIndex, uint8_t id[8]);
static const int32_t kClickSnapVertex = 0x4e2;
static bool snapWaiting = false;

static bool SnapLiveStart();

static void SnapToVertexStart()
{
	// a placed object snaps LIVE, as inteng's (SnapLiveStart); a special object (a point) keeps the click's snap below (NAMED)
	if (SnapLiveStart())
	{
		return;
	}

	bool const haveSpecial = specialMode.haveCurrent && host::Selection_Get().kind != host::SelectedKind::Object;
	bool const haveObject = host::Selection_Get().kind == host::SelectedKind::Object;

	if (!haveSpecial && !haveObject)
	{
		Answer("Snap To Vertex: select the object that is to move first");
		return;
	}

	snapWaiting = host::ClickMode_Start(kClickSnapVertex, host::ClickWants::Polygon, "Snap To Vertex");
	Answer(snapWaiting ? "Snap To Vertex: click the object whose vertex it is to land on (right button: cancel)" : "That command can't wait for a click");
}

static void SnapToVertexClicked(host::PickReport const &report)
{
	snapWaiting = false;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !report.hasHit)
	{
		Answer("Snap To Vertex: nothing under the click");
		return;
	}

	bool const special = specialMode.haveCurrent && host::Selection_Get().kind != host::SelectedKind::Object;
	uint8_t moving[8] = {};

	if (!special && !SelectedObjectId(moving))
	{
		Answer("Snap To Vertex: no object is selected any more");
		return;
	}

	uint8_t target[8];

	if (report.found == host::PickFound::Object)
	{
		memcpy(target, report.objectId, 8);
	}
	else if (!ModuleObject(open, report.moduleIndex, target))
	{
		Answer("Snap To Vertex: the level file has no single object for that module");
		return;
	}

	if (!special && memcmp(target, moving, 8) == 0)
	{
		Answer("Snap To Vertex: not the object's own vertex - click another object");
		snapWaiting = host::ClickMode_Start(kClickSnapVertex, host::ClickWants::Polygon, "Snap To Vertex");
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	double const point[3] = { (double)report.hitPoint[0] / scale, (double)report.hitPoint[1] / scale, (double)report.hitPoint[2] / scale };
	int32_t vertex[3];
	host::MoveRelPolyEditResult result = host::SnapToVertex_Target(open, open->Level(), target, point, vertex);

	if (result != host::MoveRelPolyEditResult::Done)
	{
		Answer(std::string("Not moved: ") + host::MoveRelPolyEdit_Words(result));
		return;
	}

	if (special)
	{
		Answer(host::SpecialMode_MoveCurrent(open, &specialMode, vertex).answer);
	}
	else
	{
		result = host::SnapToVertex_MoveObject(open, open->Level(), moving, vertex);

		if (result != host::MoveRelPolyEditResult::Done)
		{
			Answer(std::string("Not moved: ") + host::MoveRelPolyEdit_Words(result));
			return;
		}

		Answer("Moved: one of its vertices is on the vertex of the other object nearest the click");
	}

	LoadTheObjectListAgain();
}

// Object Placement's Object, Delete (inteng's &Delete, D3): the selected object is taken out of the level the editor holds. A refusal says
// why in the status line, in object_edit.h's own words. (Added 2026-11-15 by Melanikus (Drake), S5.)
static void DeleteTheSelectedObject()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::Selected const selected = host::Selection_Get();

	// OP-47 / OP-119 (V-21): Delete (0x16) needs Gravity off (0x4dc1fe) and does nothing while an object flies (0x4dc343): nothing happens, as in inteng
	if (PlacementGravityOn() || PlacementIsFlying())
	{
		return;
	}

	// a special object that was just made or found is the one Delete takes (the selection is cleared when it is made)
	if (specialMode.haveCurrent && selected.kind != host::SelectedKind::Object)
	{
		SpecialRunCommand(0x16, false);
		return;
	}

	if (!open || selected.kind != host::SelectedKind::Object)
	{
		statusWords = "Delete: select an object first";
		return;
	}

	host::ObjectEditResult const result = host::DeleteObject(open, selected.objectId);

	if (result != host::ObjectEditResult::Done)
	{
		statusWords = host::ObjectEditWords(result);
		return;
	}

	host::Selection_Clear();
	LoadTheObjectListAgain();
	statusWords = "Object deleted";
}

// Object Placement's Quick-create items (inteng's FUN_004d9af0 cases calling FUN_004de260, P3): the shape the item names is made into a placed object AT
// the view's place with the view's turn, selected, as object_make.h says (what is copied, and what is a named gap: a shape the level doesn't hold is
// "shape not in this level"). (Added 2026-11-15 by Melanikus (Frost).)
std::string quickCreateWords;
// Where a shape the level doesn't hold is looked for ([Editor] ShapeFolder= in AVP_OpenInteng.ini; empty: nowhere): shape_import.h. Held to the data
// folder's rules (SaveFolder_WhyNotUsable) when the program starts; an unusable one is left empty and the log says why.
std::string shapeFolder;

// The view's place in the FILE's units (inteng: the view block's place / DAT_00607bd8, which is 100 x the level's scale, by ftol: the game's millimetres divided by the scale, cut toward 0). The object makers
// (object_make.h, object_edit.h) write the place they are given into the object's header as it is.
void ViewFilePlace(int32_t out[3])
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	out[0] = theCamera ? (int32_t)(theCamera->x / scale) : 0;
	out[1] = theCamera ? (int32_t)(theCamera->y / scale) : 0;
	out[2] = theCamera ? (int32_t)(theCamera->z / scale) : 0;
}

// FUN_004d64a0 for a placed object just made: THE current object (the special one let go of)
void MakePlacedObjectCurrent(uint8_t const *id)
{
	host::Selected picked = {};

	picked.kind = host::SelectedKind::Object;
	memcpy(picked.objectId, id, host::kSelectionIdBytes);
	specialMode.haveCurrent = false;
	host::Selection_Set(picked);
}

// OP-50 (FUN_004de260): a Quick-create item's shape, looked up in the level first. NOT IN IT: inteng's confirm "%s is not in this file.\nWould you like to have it imported?" (0x5e89a8,
// FUN_00474a40 Yes / No); No: nothing; Yes: the shape's file is looked for and imported (ours: the [Editor] ShapeFolder; inteng: the ini's RifSearchPath), and a failure is inteng's message box:
// "Failed to find the file containing that shape" (0x5e8960) when there is no file, "Failed to load file" (0x5e894c) when the file can't be used. Found (or imported): the object is made at the
// view with the first free "<shape><n>" name (FUN_004d5620), no dialog, current.
static void QuickCreateAtTheView(int32_t command, bool importAsked = false);

static void QuickCreateAtTheView(int32_t command, bool importAsked)
{
	char const *const shape = host::QuickCreate_ShapeName(command);
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!shape)
	{
		statusWords = "That Quick-create item has no shape";
		return;
	}

	int32_t matrix[9];

	if (!open || !theCamera || !LevelView_CameraMatrix(matrix))
	{
		statusWords = "Create: no level is open, or no frame has been drawn yet";
		return;
	}

	int32_t place[3];
	uint8_t newId[host::kSelectionIdBytes] = {};
	std::string name;
	std::string importWhy;

	ViewFilePlace(place);
	host::ObjectMakeResult const result = host::CreateObjectFromShape(open, shape, place, matrix, newId, &name, importAsked ? shapeFolder.c_str() : nullptr, &importWhy);

	if (!importAsked && result == host::ObjectMakeResult::NoSuchShape)
	{
		host::Dialogs_Confirm(std::string(shape) + " is not in this file.\nWould you like to have it imported?", [command](bool yes)
		{
			if (yes)
			{
				QuickCreateAtTheView(command, true);
			}
		});
		return;
	}

	if (importAsked && (result == host::ObjectMakeResult::NoSuchShape || result == host::ObjectMakeResult::ShapeFileRefused))
	{
		host::Dialogs_Message(result == host::ObjectMakeResult::NoSuchShape ? "Failed to find the file containing that shape" : "Failed to load file");
	}

	if (result != host::ObjectMakeResult::Done)
	{
		quickCreateWords = std::string(shape) + ": " + host::ObjectMakeWords(result);

		if (!importWhy.empty())
		{
			quickCreateWords += " (" + importWhy + ")";
		}

		if (result == host::ObjectMakeResult::NoSuchShape)
		{
			quickCreateWords += shapeFolder.empty() ? " (the shape folder in AVP_OpenInteng.ini, [Editor] ShapeFolder=, can't be used)"
			                                         : " (put the shape .rif files in " + shapeFolder + ")";
		}

		statusWords = quickCreateWords.c_str();
		return;
	}

	LoadTheObjectListAgain();
	PutTheNewObjectInPlay();   // OP-26c: in the game's view at once
	MakePlacedObjectCurrent(newId);
	quickCreateWords = "Object " + name + " made at the view";
	statusWords = quickCreateWords.c_str();
}

// Create > With shape > Selected (0x33) and key P (OP-26 / OP-88, FUN_004d9af0 0x4db0b2-0x4dc098): needs a current object. A PLACED SHAPE: the Name Object dialog (as OP-24); OK with a free name makes a NEW placed
// object of the SAME shape at the VIEW's place with the view's turn (FUN_004e0ed0), its simple-object properties (+0x44..+0x58: type, extra, flags, mass, integrity, explosion) the current one's; it is made
// current. A SOUND: the Name Object dialog; OK: the current sound's ranges, volume, pitch, flags and probability become the new-sound defaults and a new sound is made at the view's place with them, current.
// Any other kind (generator, bad guy, particle generator, player start): nothing. Here the new placed object is made by name from the shape's outside file (CreateObjectFromShape, as the chooser's create)
// and renamed to the typed name; NAMED: a shape with no outside file name is copied with the whole object instead (CreateObjectLike: its strategy, notes, track and alternate locations come too) and turned
// to the view's turn.
static uint8_t duplicateFrom[host::kSelectionIdBytes];
static std::string duplicateWords;

// OP-26c (found 11-18 on the kens-co-op bunker switch): a NEW object is in play at once, as inteng's FUN_004e0ed0 makes it in its own scene. Ours made it in the working bytes only, and the
// game's view loads them at a screen change (ShowTheWorkingBytes, the view gap), so a P copy was not drawn ("selection: in play no") until the next screen. Now the view loads the working bytes
// as soon as the object is made (the camera stays the editor's own). Not while a live copy flies or a ghost shows (their blocks belong to the level loaded): the next screen change shows it.
void PutTheNewObjectInPlay()
{
	if (!LevelView_FlyActive() && !LevelView_GhostActive())
	{
		ShowTheWorkingBytes();
	}
}

static void PlacementDuplicateNamed(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t matrix[9];

	if (typed.empty())
	{
		Answer("Create Duplicate: no object was made (no name typed)");
		return;
	}

	if (!open || !open->Level() || !theCamera || !LevelView_CameraMatrix(matrix))
	{
		Answer("Create Duplicate: no level is open, or no frame has been drawn yet");
		return;
	}

	int32_t place[3];
	int32_t shapeNumber = -1;
	std::vector<host::ShapeChoice> shapes;
	std::string shapeName;
	uint8_t madeId[host::kSelectionIdBytes] = {};

	ViewFilePlace(place);

	if (host::ShapeChooser_ShapeOfObject(open->Level(), open, duplicateFrom, &shapeNumber) && host::ShapeChooser_List(open->Level(), open, &shapes) > 0)
	{
		for (host::ShapeChoice const &one : shapes)
		{
			if (one.number == shapeNumber)
			{
				shapeName = one.name;
				break;
			}
		}
	}

	host::SimpleObjectDraft properties;
	bool const haveProperties = host::SimpleObject_Read(open, open->Level(), duplicateFrom, &properties) == host::SimpleObjectResult::Done;

	if (!shapeName.empty())
	{
		uint8_t newId[host::kSelectionIdBytes] = {};
		std::string autoName;
		host::ObjectMakeResult const made = host::CreateObjectFromShape(open, shapeName.c_str(), place, matrix, newId, &autoName, shapeFolder.c_str());

		if (made != host::ObjectMakeResult::Done)
		{
			duplicateWords = "Create Duplicate: " + shapeName + ": " + host::ObjectMakeWords(made);
			Answer(duplicateWords);
			return;
		}

		LoadTheObjectListAgain();

		std::string why;
		host::ObjectEditAnswer const renamed = host::ObjectEdit_Rename(open, LevelObjects_Level(), newId, typed.c_str(), madeId, &why);

		if (renamed != host::ObjectEditAnswer::Done)
		{
			memcpy(madeId, newId, sizeof(madeId));
		}

		LoadTheObjectListAgain();

		if (haveProperties)
		{
			host::SimpleObject_Write(open, LevelObjects_Level(), madeId, properties);
			LoadTheObjectListAgain();
		}

		duplicateWords = renamed == host::ObjectEditAnswer::Done ? "Object " + typed + " made at the view" : "Object " + autoName + " made at the view; the name \"" + typed + "\" was refused: " + why;
	}
	else
	{
		host::ObjectEditResult const copied = host::CreateObjectLike(open, duplicateFrom, typed.c_str(), place, madeId);

		if (copied != host::ObjectEditResult::Done)
		{
			duplicateWords = std::string("Create Duplicate: ") + host::ObjectEditWords(copied);
			Answer(duplicateWords);
			return;
		}

		float turn[4];

		host::ObjectMake_TurnFromMatrix(matrix, turn);
		host::TrackSetObjectPose(open, madeId, place, turn);
		LoadTheObjectListAgain();
		duplicateWords = "Object " + typed + " made at the view (a copy of the whole object: its shape has no outside file name)";
	}

	PutTheNewObjectInPlay();
	MakePlacedObjectCurrent(madeId);
	Answer(duplicateWords);
}

static void PlacementDuplicate()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::Selected const selected = host::Selection_Get();

	if (!open || !open->Level())
	{
		Answer("No level is held to work on");
		return;
	}

	// the current object as the commands take it: a selected placed object first, else the current special object
	if (selected.kind == host::SelectedKind::Object)
	{
		memcpy(duplicateFrom, selected.objectId, sizeof(duplicateFrom));
		NameObjectBox("", PlacementDuplicateNamed);
		return;
	}

	if (!specialMode.haveCurrent)
	{
		Answer("Create Duplicate: select an object first");
		return;
	}

	if (specialMode.kind != host::SpecialKind::Sound)
	{
		return;   // a generator, bad guy, particle generator or player start: nothing (0x4dc07e)
	}

	NameObjectBox("", [](std::string const &name)
	{
		OpenLevelFile *const file = LevelObjects_OpenForEdit();
		std::vector<host::SpecialObject> all;

		if (!file || host::SpecialObjects_Read(file, &all) != host::SpecialResult::Done)
		{
			Answer("Create Duplicate: the level's special objects could not be read");
			return;
		}

		double const scale = local_scale > 0.0 ? local_scale : 1.0;

		for (host::SpecialObject const &one : all)
		{
			if (one.kind == host::SpecialKind::Sound && specialMode.haveCurrent && specialMode.kind == host::SpecialKind::Sound && one.number == specialMode.number)
			{
				soundDefaults.set = true;
				soundDefaults.innerMm = one.inner * scale;
				soundDefaults.outerMm = one.outer * scale;
				soundDefaults.volume = one.volume;
				soundDefaults.pitch = one.pitch;
				soundDefaults.flags = one.soundFlags;
				soundDefaults.probability = one.probability;
				break;
			}
		}

		specialMode.createName = name;
		specialNameGiven = true;
		SpecialRunCommand(0x37, false);
		specialNameGiven = false;
	});
}

// The field panel kept a value in a field (S3d): it is made an edit of the bytes the level holds, checked by the field model's own refusals
// (record_edit.h, ApplyKeptStrategyField), and the panel is shown again from those bytes, so what it shows is what the level holds: a value the
// model refused is shown as it was, with the model's reason on the status line. The field with focus keeps it. Nothing is written to a file:
// that's Save a copy. (Added 2026-11-15 by Melanikus (Drake), S3d.)
// A target picked by name (record_edit.h): the first object of the level (virtual objects included) with that name, the letters of either case alike
static bool ObjectByName(char const *name, size_t length, uint32_t *first, uint32_t *second)
{
	std::vector<host::LevelObject> rows;

	if (!LevelObjects_Read(&rows))
	{
		return false;
	}

	for (host::LevelObject const &row : rows)
	{
		if (row.name && strlen(row.name) == length && _strnicmp(row.name, name, length) == 0)
		{
			memcpy(first, row.id, 4);
			memcpy(second, row.id + 4, 4);
			return true;
		}
	}

	return false;
}

bool SelectedObjectId(uint8_t *id);
static void KeepTheField(size_t field)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::EditPart const *const part = shownEdit.PartOf(field);

	if (!open || !part || !part->chunk)
	{
		statusWords = "There is nothing here the editor can change";
		return;
	}

	int32_t const focus = fieldHost.FocusedField();
	host::LevelStore store(open);
	host::EditAnswer editAnswer = host::EditAnswer::Applied;
	char const *why = nullptr;
	host::KeptAnswer answer = host::KeptAnswer::BadCall;

	// A MODULE'S FLAGS typed in the panel go through the Strategy code, as the bar's Add / Remove Flag items do (strategy_mode.h StrategyMode_FlagWordChanged, FUN_0052f360's rules)
	bool flagsDone = false;

	if (part->chunkKind && !part->strategyKind && strcmp(part->chunkKind->id, "MODFLAGS") == 0 && field < shownRecord.sources.size() && shownRecord.sources[field].field)
	{
		uint8_t selected[8];
		std::vector<unsigned char> bytes;
		double number = 0;
		model::Field const &flagField = *shownRecord.sources[field].field;

		if (SelectedObjectId(selected) && store.PayloadOf(part->chunk, &bytes) && model::ReadChunkField(*part->chunkKind, flagField, bytes.data(), bytes.size(), &number, 0))
		{
			uint32_t const wordNow = bytes.size() >= 4 ? *(uint32_t const *)bytes.data() : 0;
			host::FieldEdit made;
			char const *refused = nullptr;
			uint32_t wordNew = wordNow;

			// the edit says the new value of the field as it stores it (after its mask); the word it makes is worked out here from the bytes the store holds now
			if (host::EditFromPanel(shownRecord, fieldHost.Panel(), field, wordNow, &made, &refused) == host::PanelEditAnswer::Made && made.what == host::EditWhat::Number && made.number >= 0 && made.number <= 4294967295.0)
			{
				uint32_t const value = (uint32_t)made.number;

				if (flagField.mask == 0)
				{
					wordNew = value;
				}
				else
				{
					uint32_t shift = 0;

					while (shift < 31 && ((flagField.mask >> shift) & 1u) == 0)
					{
						shift++;
					}

					wordNew = (wordNow & ~flagField.mask) | ((value << shift) & flagField.mask);
				}

				size_t changedModules = 0;
				host::StrategyModeResult const flagResult = host::StrategyMode_FlagWordChanged(open, selected, wordNow, wordNew, &changedModules);

				answer = flagResult == host::StrategyModeResult::Done ? host::KeptAnswer::Applied : host::KeptAnswer::EditRefused;
				flagsDone = true;

				if (flagResult != host::StrategyModeResult::Done)
				{
					why = host::StrategyModeWords(flagResult);
				}
				else
				{
					LoadTheObjectListAgain();
				}
			}
			else if (refused)
			{
				answer = host::KeptAnswer::ValueRefused;
				why = refused;
				flagsDone = true;
			}
		}
	}

	// A strategy's field, or a chunk's the model has a table for (the module's flags, the level's settings)
	if (flagsDone)
	{
	}
	else if (part->strategyKind)
	{
		answer = host::ApplyKeptStrategyField(&store, part->chunk, *part->strategyKind, shownRecord, fieldHost.Panel(), field, &editAnswer, &why);
	}
	else if (part->chunkKind)
	{
		answer = host::ApplyKeptChunkField(&store, part->chunk, *part->chunkKind, shownRecord, fieldHost.Panel(), field, &editAnswer, &why);

		// inteng's coupling: No PreLight on also leaves the Omni bit on in what it saves (F-LIGHT-COUPLING)
		if (answer == host::KeptAnswer::Applied)
		{
			host::ApplyLightCoupling(&store, part->chunk, *part->chunkKind, shownRecord, field);
		}
	}

	switch (answer)
	{
		case host::KeptAnswer::Applied:
			statusWords = "Changed in the open level; Save a copy (Ctrl+E) writes it to a file";
			trackDirty = true;
			break;
		case host::KeptAnswer::ValueRefused:
		case host::KeptAnswer::EditRefused:
			statusWords = why ? why : "The level can't take that value";
			break;
		case host::KeptAnswer::NotEditable:
			statusWords = "That field can't be changed";
			break;
		default:
			statusWords = "That change couldn't be made";
			break;
	}

	// What the panel shows is made again from the bytes the level holds, and focus goes back where it was
	ShowTheFieldsAgain();

	if (fieldHost.Showing() && focus >= 0)
	{
		fieldHost.ClickField((size_t)focus);
	}
}

// What a key or a click did in the field panel: when the field it left, or the one it's in, now holds a value the level doesn't (the panel
// keeps a value when Enter is pressed, or focus moves off a field being edited), that value is applied. previous is the field that had focus
// before the event, or -1.
static void ThePanelAnswered(ui::PanelAnswer answer, int32_t previous)
{
	if (answer == ui::PanelAnswer::Refused)
	{
		// The panel says why on its own status line
		if (fieldHost.Showing() && fieldHost.Panel().StatusWords())
		{
			statusWords = fieldHost.Panel().StatusWords();
		}

		return;
	}

	if ((answer == ui::PanelAnswer::Kept || answer == ui::PanelAnswer::FocusMoved) && previous >= 0 && fieldHost.Showing() &&
		fieldHost.Panel().Changed((size_t)previous))
	{
		KeepTheField((size_t)previous);
	}
	else if (fieldHost.Showing() && fieldHost.Panel().StatusWords())
	{
		statusWords = fieldHost.Panel().StatusWords();
	}
}

// Save a copy (File, Ctrl+E): the open level is written to <Documents>\AVP_OpenInteng\edited\<name>-edited.rif, and the status line says where.
// NEVER over the file that was opened or anything in the game's folder (owner-intent 7; save_copy.h works the place out and refuses those).
// The game's own loader checks the new file before it is put in place (level_file.h). (Added 2026-11-15 by Melanikus (Drake), S3d.)
std::string savedWords;   // the status line's words after a save: kept here so statusWords can point at them
int32_t strategyViewAdd = 0;   // the Add Flag command of the flag in view (the Remove is the next one): the F key does it, Shift+F the Remove (FUN_00525700 0x52578c: DAT_0091494c / DAT_00914948)
// THE CAMERA POSITIONS (every 3D screen: "F-Keys: Restore camera position", "Shift+F-Keys: Save camera position"): FUN_004705a0 looks at inteng's key codes 8 to 0x13 (F1 to F12): without the Shift-like slot the
// position of slot code - 8 is restored (0..11), with it (codes 8 to 0x11, F1 to F10) saved (FUN_004704d0 says "Camera position saved"). Slot 10 is the one every mode's Exit saves (FUN_004704d0(10)) and
// every mode's start restores. F10 is slot 9 like the others (Windows sends it as a system key; the editor's window handles it as the F key it is, Bishop 2026-11-16).
static FlyCamera cameraSlots[12];

// The camera file's slots and the editor's camera. form: slot.matrix is CreateEulerMatrix's own (the game's, VDB_Mat for no look); camera_file.cpp turns the file's inteng form over.
// form: MatrixToEuler takes CreateEulerMatrix's TURNED OVER (as LevelView_CameraHeading), so the slot's is turned over first.
static FlyCamera CameraOfFileSlot(host::CameraFileSlot const &slot)
{
	MATRIXCH matrix;
	EULER euler;

	matrix.mat11 = slot.matrix[0]; matrix.mat21 = slot.matrix[1]; matrix.mat31 = slot.matrix[2];
	matrix.mat12 = slot.matrix[3]; matrix.mat22 = slot.matrix[4]; matrix.mat32 = slot.matrix[5];
	matrix.mat13 = slot.matrix[6]; matrix.mat23 = slot.matrix[7]; matrix.mat33 = slot.matrix[8];
	MatrixToEuler(&matrix, &euler);

	FlyCamera camera;

	camera.x = slot.place[0];
	camera.y = slot.place[1];
	camera.z = slot.place[2];
	camera.look = euler.EulerX & 4095;
	camera.turn = euler.EulerY & 4095;
	camera.roll = euler.EulerZ & 4095;

	if (camera.look > 2048)
	{
		camera.look -= 4096;
	}

	FlyCamera_Hold(&camera);
	return camera;
}

static host::CameraFileSlot FileSlotOfCamera(FlyCamera const &camera)
{
	EULER euler = { camera.look & 4095, camera.turn & 4095, camera.roll & 4095 };
	MATRIXCH matrix;
	host::CameraFileSlot slot;

	CreateEulerMatrix(&euler, &matrix);
	slot.place[0] = camera.x;
	slot.place[1] = camera.y;
	slot.place[2] = camera.z;
	slot.matrix[0] = matrix.mat11; slot.matrix[1] = matrix.mat12; slot.matrix[2] = matrix.mat13;
	slot.matrix[3] = matrix.mat21; slot.matrix[4] = matrix.mat22; slot.matrix[5] = matrix.mat23;
	slot.matrix[6] = matrix.mat31; slot.matrix[7] = matrix.mat32; slot.matrix[8] = matrix.mat33;
	return slot;
}
static bool cameraSlotSet[12];
std::string cameraWords;

// THE CAMERA FILE (FUN_00470610): the open level's <RIFFNAME>.cam in the editor's own camera folder (camera_file.h), its name and CAMORIGN offset set when the level opens (no name: none is written).
// inteng writes it at EVERY save of a slot (FUN_004704d0 0x47053c: every Shift+F and every mode's Exit) and when the program ends (FUN_0058c9e0 0x58d06e), so a position saved is on disk at once:
// a crash or a hard kill keeps it (Vasquez 11-18; until then ours wrote it only on its way out).
static std::string cameraFileName;
static std::string cameraFolder;
static int32_t cameraFileOffset[3];

static bool WriteTheCameraFile()
{
	if (cameraFileName.empty())
	{
		return false;
	}

	host::CameraFileSlot toWrite[host::kCameraFileSlots];

	for (size_t s = 0; s < host::kCameraFileSlots; s++)
	{
		toWrite[s] = FileSlotOfCamera(cameraSlots[s]);
	}

	return host::CameraFile_Write(cameraFolder, cameraFileName, cameraFileOffset, toWrite);
}

static void SaveCameraSlot(int slot)
{
	if (theCamera && slot >= 0 && slot < 12)
	{
		cameraSlots[slot] = *theCamera;
		cameraSlotSet[slot] = true;
	}
}

// FUN_004704d0(slot) as inteng runs it (reassess/camera-saved-vasquez.md CS-1..CS-18): the camera saved in the slot AND "Camera position saved" (0x5d3d54) written at once at (200, 100) of the window
// (FUN_0057bd40: straight onto the screen, so it is gone with the next frame drawn). Ours draws it in the next frame only; cameraSavedCount is the state line's camerasaved (how many times it ran).
bool cameraSavedDraw = false;
static unsigned cameraSavedCount = 0;
unsigned cameraSavedDrawn = 0;   // the frames that drew the words (the state line's camerasaveddrawn: the proof they were on the screen, as a picture can't catch one frame)

void CameraPositionSaved(int slot)
{
	SaveCameraSlot(slot);
	WriteTheCameraFile();   // 0x47053c: FUN_00470610, the file written at once
	cameraSavedDraw = true;
	cameraSavedCount++;
}

void CameraKey(int n, bool save)
{
	int const slot = n - 1;   // F1 is slot 0

	if (!theCamera || slot < 0 || slot > 11)
	{
		return;
	}

	if (save && slot > 9)
	{
		// with Shift held, FUN_004705a0 saves only codes 8 to 0x11 (F1 to F10; 0x4705c8): Shift + F11 and Shift + F12 do nothing in inteng. OUR BUG, FIXED (Drake 2026-11-17): they
		// saved slots 10 and 11 here, and slot 10 is the one every mode's Exit saves and every mode's start restores, so a Shift + F11 moved the camera at the next mode change
		return;
	}

	if (save)
	{
		CameraPositionSaved(slot);   // FUN_004705a0 0x4705ca: Shift + F1 .. F10
		cameraWords = "Camera position saved (F" + std::to_string((long long)n + 0) + ")";
	}
	else if (cameraSlotSet[slot])
	{
		*theCamera = cameraSlots[slot];
		FlyCamera_Hold(theCamera);
		cameraWords = "Camera position restored (F" + std::to_string((long long)n) + ")";
	}
	else
	{
		// Shift saves F1 to F10 only; slot 10 (F11) is saved by every mode's Exit, slot 11 (F12) by nothing
		cameraWords = "No camera position is saved in F" + std::to_string((long long)n) + (n <= 10 ? " (Shift+F" + std::to_string((long long)n) + " saves one)" : n == 11 ? std::string(" (a mode's Exit saves it)") : std::string());
	}

	statusWords = cameraWords.c_str();
}

bool strategyFromPlacement = false;   // Strategy mode was entered from Object Placement (Tab, DAT_0091493c): its Exit and Tab go back there, not to the hub
bool strategyMarkedChanged = false;   // inteng's "changed" mark DAT_00914950 for what changes no bytes (an Edit view opened: FUN_005286b0 0x5286cc): Exit asks "Would you like to save first?"; Save (0x52831b) and leaving the screen clear it
uint32_t strategyViewBit = 0;   // Strategy mode's View Flag (FUN_0052f680 sets DAT_00914944 to the bit, FUN_00522650 colours the modules that hold it): 0 is none
static unsigned copySavedChanges = ~0u;   // defined with the mode's Exit below: the level's Changes() at the opening, then at each Save a copy (its Generation() until 2026-11-17: a same-size edit, SetPayload, moves only Changes(), so the Exit did not save it; the view gap, Dietrich)
static host::LevelSaveSitting levelSaveSitting;   // this run's saves: the first save of the level keeps it as .bak, later ones leave that .bak alone (level_save.h, Bishop's ruling B)

// STRATEGY DATA'S "CHANGED" (strategy-data.md S-10, S-5; reassess/strategy-data-verify-frost.md W-3): inteng's mark DAT_00914950 is cleared on EVERY entry of the mode (FUN_00522ab0 0x522ae2,
// Object Placement's Tab included) and by its Save (FUN_005282d0 0x52831b), and set only by the mode's own edits (its 30 writers, all in 0x5286b0-0x53b7a9). So a change made before the mode
// was entered (Object Placement's, carried in by Tab) does not make Exit ask. Ours: the level's generation at the entry or the last Save, and strategyMarkedChanged for the marks that change no bytes.
unsigned strategyCleanGeneration = ~0u;

void StrategyCleanNow()   // W-3: the entry (FUN_00522ab0 0x522ae2) and Save (0x52831b)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	strategyCleanGeneration = open ? open->Generation() : ~0u;
	strategyMarkedChanged = false;
}

bool StrategyChanged()   // W-3: DAT_00914950 as FUN_00524d00 0x524d1d reads it
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	return strategyMarkedChanged || (open && strategyCleanGeneration != ~0u && open->Generation() != strategyCleanGeneration);
}

void TextureAnimFlush();

void TexturePolyApplyRemoved();

// THE COLLECTIONS' TEXT FILE (FUN_0048cac0, run after inteng's save of the file by Hierarchy's Exit 0x488991 and Return 0x488a7b; reassess/hierarchy-play-frost.md HP-29): with no HSETCOLL in the file
// nothing; else a text file of one line per shape-set collection, "%d\t%s\n" (0x5d5d14: its number +0x2c, its name +0x28), in the file's order, written over any earlier one (CreateFile GENERIC_WRITE,
// CREATE_ALWAYS). Its name: DAT_0091506c (set to "." at the start, 0x57a969-0x57a973) + the RIFFNAME, cut at the FIRST '.' (_strchr 0x48cbed), + ".txt" (0x5d5d1c): the cut falls on the leading
// ".", so inteng's file is always named ".txt", in its working folder. INTENG BUG, FIXED (MAPPING-ORDERS section 15, HP-29): what it meant is the RIFFNAME without its extension, + ".txt"
// ("hnpcmarine.txt" for MelPyth's ShotgunBot); the name cleaned as the camera file's is (CameraFile_Name: the last part only, letters, digits and . - _), and a file with no RIFFNAME gets
// "collections.txt" (named). Ours writes it beside the saved copy, in the editor's own folder for copies (Melanikus's standing ruling: nothing is written in the game folder). Security:
// each line is bounded (inteng's sprintf into 1000 bytes had none); the names are the file's own, written as text.
static void WriteTheCollectionsText(OpenLevelFile *model, std::string const &folder)
{
	std::vector<model::ShapeSetCollection> collections;

	if (host::HierarchyEdit_Collections(model, &collections) != host::HierarchyEditResult::Done || collections.empty())
	{
		return;
	}

	std::string base;

	if (!model || !host::CameraFile_Name(model->Level(), &base) || base.size() <= 4)
	{
		base.clear();
	}
	else
	{
		base.resize(base.size() - 4);   // CameraFile_Name's ".cam"
	}

	if (base.find('.') != std::string::npos)
	{
		base.resize(base.find('.'));   // the RIFFNAME's own extension, as inteng meant the cut (0x48cbed)
	}

	std::string const path = folder + "\\" + (base.empty() ? std::string("collections") : base) + ".txt";
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		LogString("collections text: not written, " + path);
		return;
	}

	for (model::ShapeSetCollection const &each : collections)
	{
		char line[1000];
		int const length = _snprintf_s(line, sizeof(line), _TRUNCATE, "%d\t%s\n", (int)each.number, each.name.c_str());
		DWORD written = 0;

		if (length > 0)
		{
			WriteFile(file, line, (DWORD)length, &written, NULL);
		}
	}

	CloseHandle(file);
	LogString("collections text: " + path);
}

// The file a level was opened from, as a whole path, from the game's own path of the level it loaded (LevelView_FilePath: avp_rifs\<name>.rif, or its copy in avp_rifs\OpenedByEditor):
// the copy's is the file named on the command line (levelSourcePath) when the two names agree; any other is the game folder's own file. Empty when it can't be told (a model's copy).
static std::string OpenedFileOf(std::string const &opened, bool ofModel)
{
	std::string relative = opened;
	std::string openedFile;

	for (char &letter : relative)
	{
		letter = letter == '/' ? '\\' : letter;
	}

	bool const ofOpenedByEditor = relative.find("OpenedByEditor") != std::string::npos;

	if (ofOpenedByEditor && !ofModel && !levelSourcePath.empty())
	{
		char narrow[MAX_PATH * 2] = "";

		if (WideCharToMultiByte(CP_ACP, 0, levelSourcePath.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, nullptr) > 0)
		{
			std::string const source = narrow;
			size_t const a = relative.find_last_of('\\'), b = source.find_last_of("/\\");
			std::string stemOpened = a == std::string::npos ? relative : relative.substr(a + 1), stemSource = b == std::string::npos ? source : source.substr(b + 1);

			stemOpened = stemOpened.substr(0, stemOpened.find_last_of('.'));
			stemSource = stemSource.substr(0, stemSource.find_last_of('.'));
			openedFile = _stricmp(stemOpened.c_str(), stemSource.c_str()) == 0 ? source : std::string();
		}
	}
	else if (!ofOpenedByEditor && theGameFolder[0])
	{
		openedFile = std::string(theGameFolder) + (theGameFolder[strlen(theGameFolder) - 1] == '\\' ? "" : "\\") + relative;
	}

	return openedFile;
}

// THE HUB'S READ-ONLY BOX (TB-C10, construction-closeout-frost.md; FUN_00447550 0x4476bd -> FUN_00444be0): at every start of the hub, with a level open, the level's file is looked at
// (FUN_0046c5c0: GetFileAttributesA; 2 = a file that can be written, 1 = read-only, 0 = missing or a folder) and anything but 2 shows the message box "This file is read only\n(Press escape to
// continue)" (0x5cf518, FUN_00474030 + FUN_0057c800(100, 100)). Ours looks at the file the level was opened from (OpenedFileOf: the one Save writes over). NAMED: a file that can't be
// told (an empty path) shows nothing.
static void HubReadOnlyBox()
{
	char opened[MAX_PATH] = "";

	if (!LevelObjects_Level() || !LevelView_FilePath(opened, sizeof(opened)))
	{
		return;
	}

	std::string const file = OpenedFileOf(opened, false);

	if (file.empty())
	{
		return;
	}

	DWORD const attributes = GetFileAttributesA(file.c_str());
	bool const writable = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_READONLY);

	if (!writable)
	{
		LogString("hub: " + file + " is read only: the box says so (FUN_00444be0)");
		DialogMessage("This file is read only\n(Press escape to continue)");
	}
}

void SaveTheCopy()
{
	// whatever stops this save, its words are this save's: an Exit reads them to say whether the level was saved, so the last save's "Saved a copy" must never stand
	savedWords = "Not saved: the save did not finish";
	statusWords = savedWords.c_str();

	// the places Object Placement's Gravity moved objects to (OP-47: inteng's object save writes its block's place)
	PlacementGravityFlush();

	// the polygons flagged for deletion are taken out of their shapes first (inteng does it when the Texture Map mode is left)
	TexturePolyApplyRemoved();

	// the frame the polygons of the current animation show goes into its chunk (the Texture Map's Anim menu, TextureAnimFlush)
	TextureAnimFlush();

	// NAMED DEVIATION D-LINK-FLUSH: inteng writes the current module's links into the level when another module is made current or at its Exit
	// items; a copy saved now would lack them, so they are written first. What is written is the same; only the moment differs.
	if (!WriteTheLinks())
	{
		std::string const why(statusWords && statusWords != savedWords.c_str() ? statusWords : "");   // WriteTheLinks said why (Answer), when it did

		savedWords = std::string("Not saved: the current module's links could not be written first") + (why.empty() ? std::string() : " (" + why + ")");
		statusWords = savedWords.c_str();
		return;
	}

	// In Hierarchy mode with a model open, the copy is the MODEL (the file the hierarchy edits were made in), saved the same way
	bool const saveTheModel = onHierarchy && hierarchyModelOpen;
	OpenLevelFile *const open = saveTheModel ? &hierarchyModelFile : LevelObjects_OpenForEdit();
	char opened[MAX_PATH] = "";

	if (saveTheModel)
	{
		// a model of the opened level's own file is named "avp_rifs/<file>", any other is a file of avp_huds
		bool const ofTheLevel = hierarchyMode.modelName.compare(0, 9, "avp_rifs/") == 0;

		_snprintf_s(opened, sizeof(opened), _TRUNCATE, ofTheLevel ? "avp_rifs\\%s.rif" : "avp_huds\\%s.rif", ofTheLevel ? hierarchyMode.modelName.c_str() + 9 : hierarchyMode.modelName.c_str());
	}

	if (!open || (!saveTheModel && !LevelView_FilePath(opened, sizeof(opened))))
	{
		savedWords = "Not saved: no level is open";
		statusWords = savedWords.c_str();
		return;
	}

	// THE OWNER'S RULING (Melanikus 2026-11-16, through Bishop: "Overwrite, with backup"; it replaces owner-intent 7's save-a-copy): Save writes the level OVER the file it was opened from,
	// the one before kept beside it as "<file>.bak" (level_save.h: a temporary file, read back through the game's loader, then put in place; on any failure the file is untouched); ONE .bak A
	// SITTING (Bishop's ruling B): the run's first save keeps the level as it was opened, later saves leave that .bak alone (levelSaveSitting). The file
	// it was opened from: the one named on the command line when the game's level is its copy in avp_rifs\OpenedByEditor (open_with.h), else the game folder's own (avp_rifs\<name>.rif, or a
	// model's avp_huds\<name>.rif). A HIDDEN TEST RUN (-picture) never writes a file that was opened: its stand-in is the copy in its own folder for copies, as before (save_copy.h), first
	// filled from the opened file so its .bak is real, and its words are the old "Saved a copy: <path>" (the harness reads them).
	std::string copyPath;
	std::string copyFolder;
	char const *why = nullptr;
	bool const copyPlace = host::SaveCopyPlace(opened, theCopiesFolder, theGameFolder, &copyPath, &copyFolder, &why);

	// The folders for copies are the editor's own (Documents\AVP_OpenInteng, then its edited), plain folders (not links): the same check the editor's settings folder passes. Still made for
	// the exports a save writes beside it (the hierarchy's collections text), and for a hidden run's stand-in.
	bool copyFolderUsable = false;
	std::string copiesWhy;   // when not usable: why, naming the folder (Bishop 11-16: a refusal says which path; kept through the merge of 6b85e7f)

	if (copyPlace)
	{
		copiesWhy = MakeTheCopiesFolder();

		if (copiesWhy.empty())
		{
			char checked[MAX_PATH];

			if (!CreateDirectoryA(copyFolder.c_str(), NULL))
			{
				DWORD const error = GetLastError();

				if (error != ERROR_ALREADY_EXISTS)
				{
					copiesWhy = "the editor couldn't make " + copyFolder + " (Windows error " + std::to_string((unsigned long long)error) + ")";
				}
			}

			if (copiesWhy.empty())
			{
				if (char const *const notUsable = SaveFolder_WhyNotUsable(copyFolder.c_str(), checked, sizeof(checked)))
				{
					copiesWhy = copyFolder + ": " + notUsable;
				}
				else
				{
					copyFolderUsable = true;
				}
			}
		}
	}

	// the file the level was opened from, as a whole path
	std::string const openedFile = OpenedFileOf(opened, saveTheModel);

	std::string target;

	if (editorHiddenRun)
	{
		if (!copyFolderUsable)
		{
			savedWords = !copyPlace ? std::string("Not saved: ") + (why ? why : "there is no place for the copy") : "Not saved: the folder for copies can't be used: " + copiesWhy;
			statusWords = savedWords.c_str();
			return;
		}

		target = copyPath;

		// the stand-in starts the sitting as the opened file (one from an earlier run replaced), so the sitting's .bak is the level as it was opened
		if (!levelSaveSitting.SavedBefore(target.c_str()) && !openedFile.empty())
		{
			CopyFileA(openedFile.c_str(), target.c_str(), FALSE);
		}
	}
	else
	{
		if (openedFile.empty())
		{
			savedWords = std::string("Not saved: the file this level was opened from is not known (") + opened + ")";
			statusWords = savedWords.c_str();
			return;
		}

		target = openedFile;
	}

	std::string backup, overWhy;
	host::LevelSaveOver const result = levelSaveSitting.Save(open, target.c_str(), &backup, &overWhy);   // the sitting's first save keeps the file as .bak, later ones leave it (ruling B)

	if (result != host::LevelSaveOver::Saved)
	{
		savedWords = std::string("Not saved (") + host::LevelSaveOverWords(result) + "; the file is as it was): " + overWhy;
		statusWords = savedWords.c_str();
		LogString("save: refused, " + overWhy);
		return;
	}

	if (editorHiddenRun)
	{
		savedWords = "Saved a copy: " + target;
	}
	else
	{
		size_t const slash = backup.find_last_of('\\');

		savedWords = "Saved: " + target + (backup.empty() ? std::string(" | there was no file before it") : " | the level as you opened it is kept as " + backup.substr(slash == std::string::npos ? 0 : slash + 1));
	}

	statusWords = savedWords.c_str();
	copySavedChanges = open->Changes();

	if (saveTheModel)
	{
		hierarchyModelSavedGeneration = open->Generation();

		if (copyFolderUsable)
		{
			WriteTheCollectionsText(open, copyFolder);
		}
	}
	StrategyCleanNow();   // FUN_005282d0 0x52831b: DAT_00914950 = 0 (W-3)
	LogString("save: " + target + (backup.empty() ? std::string() : ", the level as it was opened kept as " + backup));
}

// INTENG'S SAVE AT ITS OWN POINTS (Bishop's ruling 2026-11-16 23:3x, rule 10d "like inteng did"): inteng writes the level file (FUN_004719e0) with no question at Module Linking's Exit
// when the tree changed (0x4c3ea3 / 0x4c3f5e), at Set Current (0x4c2634), at the follow's make-current with a write pending (0x4c16c2), at the ends of AutoLink (0x4c8acd), Delete Links
// (0x4c8bbf), Load from file (0x4c909d), Add Reverse Links (0x4c8903) and the link clean (0x4c9338), at Adjacent Modules' Recalculate Entry Points once its box is closed (0x4054a6,
// changed or not), at every mode's Exit (ExitTheMode) and when the hub is left with the hub changed
// (FUN_004476d0 0x4476e3; Quit goes through it); a window closed writes nothing. Ours saves at the same points with SaveTheCopy (over the opened file, the level as it was opened kept as .bak at
// the run's first save; a hidden run's stand-in). Gives the save's words, for the command's own words to carry.
std::string SaveAtIntengsPoint()
{
	SaveTheCopy();
	return savedWords;   // the save's own words, "Not saved: ..." with the path and the reason when it was not (b3d3e16)
}

// (LIGHT SOURCING: in light_screen.cpp)
// ---- THE SCREENSHOT KEYS (GitHub #78; the owner's addition, shot_context.h): Print Screen, and Shift+F12 where Windows keeps Print Screen for its own capture. The next frame drawn
// is saved as it is shown (the bar, an open menu, pop-up or dialog with it) as <Documents>\AVP_OpenInteng\shots\<map>-<YYYYMMDD>-<HHMMSS>.png (a hidden test run: the folder for copies
// it was given), and the editor's context goes into the PNG as JSON (png_meta.h). The keys are taken before anything else sees them, so nothing open is closed or changed, and the status
// line says where the picture went only after the frame was saved (the picture shows the screen as it was). inteng has neither key (FUN_004705a0: Shift saves F1 to F10 only).
std::string shotPending;            // the path the next frame drawn is saved to; empty for none
std::string shotWords;              // the status line's words about it
bool shotPrintScreenSeen = false;   // a Print Screen reached the window since the editor started
bool shotHintGiven = false;         // the words about Print Screen and Windows' own capture were said once
bool shotFromShiftF12 = false;
static bool shotShiftHeld = false;         // Shift went down and not up yet, as ShotKey saw it (before any menu or dialog takes the key)

// The place the status line names: the folder for copies' own part only, never the user's folder in front of it (the status line is in later pictures, which may be sent to strangers)
std::string ShotShownPlace(std::string const &name)
{
	return std::string(editorHiddenRun ? "" : "Documents\\") + "AVP_OpenInteng\\shots\\" + name;
}

static void ShotAsk(bool fromShiftF12)
{
	if (!shotPending.empty())
	{
		return;   // one waits for its frame already
	}

	if (!MakeTheCopiesFolder().empty())
	{
		shotWords = "Screenshot not saved: the folder Documents\\AVP_OpenInteng can't be used";
		statusWords = shotWords.c_str();
		return;
	}

	std::string folder = theCopiesFolder;

	if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
	{
		folder += '\\';
	}

	folder += "shots\\";

	char checked[MAX_PATH];

	if ((!CreateDirectoryA(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) || SaveFolder_WhyNotUsable(folder.c_str(), checked, sizeof(checked)) != nullptr)
	{
		shotWords = "Screenshot not saved: the folder " + ShotShownPlace("") + " can't be made or used";
		statusWords = shotWords.c_str();
		return;
	}

	char opened[MAX_PATH] = "";
	std::string const map = LevelView_FilePath(opened, sizeof(opened)) ? std::string(opened) : openLevelName;
	SYSTEMTIME clock = {};

	GetLocalTime(&clock);

	host::ShotTime when;

	when.year = clock.wYear, when.month = clock.wMonth, when.day = clock.wDay, when.hour = clock.wHour, when.minute = clock.wMinute, when.second = clock.wSecond;

	for (int number = 0; number < host::kShotNumbers; number++)
	{
		std::string const name = host::Shot_FileName(map, when, number);

		if (!name.empty() && !ScreenshotNameTaken(name, (void *)&folder))
		{
			shotPending = folder + name;
			shotFromShiftF12 = fromShiftF12;
			return;
		}
	}

	shotWords = "Screenshot not saved: 100 shots of this level in this second already in " + ShotShownPlace("");
	statusWords = shotWords.c_str();
}

// The screenshot keys, before anything else sees a key: Print Screen's letting go (Windows hands Print Screen to a window as a key-up only, WM_KEYUP, or WM_SYSKEYUP with Alt) and Shift+F12's
// press (its repeats ignored). Both are swallowed whole, press and letting go, so no menu, pop-up, dialog, prompt or view gets them. True when the message was one of them.
static bool ShotKey(UINT message, WPARAM wParam, LPARAM lParam)
{
	bool const press = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
	bool const letGo = message == WM_KEYUP || message == WM_SYSKEYUP;

	if (wParam == VK_SNAPSHOT && (press || letGo))
	{
		if (letGo)
		{
			shotPrintScreenSeen = true;
			ShotAsk(false);
		}

		return true;
	}

	// Shift is followed here too, and let through: with a drop-down or a dialog open, their code takes the key before the editor's table of keys held (keyDown) records it, so ShiftDown()
	// alone would miss a Shift+F12 pressed over an open menu. Losing the keyboard forgets it (its letting go may go to another window).
	if (message == WM_KILLFOCUS)
	{
		shotShiftHeld = false;
	}

	if ((wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) && (press || letGo))
	{
		shotShiftHeld = press;
		return false;
	}

	if (wParam == VK_F12 && (press || letGo) && (shotShiftHeld || ShiftDown()))
	{
		if (press && (lParam & (1L << 30)) == 0)
		{
			ShotAsk(true);
		}

		return true;
	}

	return false;
}


// ---- THE TRACK DIALOGS as inteng draws them (Track Properties: check boxes "Loop Track", "Loop back and forth", "Playing at start", "Use Track Smoothing" with OK and Cancel, FUN_004f4090; the sound dialog:
// a file name, the ranges, the volume and pitch, loop, with OK and Cancel, edit_sound): a form over the level, in the same gray overlay as a prompt. Up / Down / Tab move between the rows, Space ticks a
// box, letters go into the row that has a text, Enter is OK, Esc is Cancel; a click on a row selects it (and ticks a box), a click on OK or Cancel does that. The form answers the same functions as the
// prompts' typed answers do (the letters L B P S; "name inner outer volume pitch loop"), so the checks are the same. (Frost, 2026-11-15)

std::vector<FormRow> formRows;   // not empty: the open prompt is a form
static size_t formFocus = 0;

void RunThePrompt();
void DeleteMultipleForm();
static void CopyCollectEnd();
static void StabilityEnd();
static void StabilityToggleLines();
bool OnStrategyScreen();

// A question inteng asks with a Yes / No box (FUN_00474a40): shown as the dialog, and answered through the code the typed prompt ran (RunThePrompt reads promptFor and promptText)
void ConfirmAs(int32_t kind, std::string const &text)
{
	DialogConfirm(text, [kind](bool yes)
	{
		promptFor = kind;
		promptText = yes ? "yes" : "no";
		RunThePrompt();
	});
}

// A question inteng asks with a text dialog (FUN_00474bc0: the words, one edit, Ok / Cancel): shown as the dialog, and answered through the code the typed prompt ran (RunThePrompt reads promptFor and promptText)
void TextAs(int32_t kind, std::string const &text, std::string const &initial)
{
	host::DialogField field;

	field.text = initial;
	DialogText(text, std::vector<host::DialogField>(1, field), [kind](int32_t answer, std::vector<std::string> const &texts)
	{
		if (answer == host::kDialogOk && !texts.empty())
		{
			promptFor = kind;
			promptText = texts[0];
			RunThePrompt();
		}
	});
}

static void FormOpen(int32_t kind, std::vector<FormRow> const &rows)
{
	memset(keyDown, 0, sizeof(keyDown));
	promptFor = kind;
	promptText.clear();
	formRows = rows;
	formFocus = 0;
}

// What the form says, as the typed answer the prompt functions take
static std::string FormTyped()
{
	if (promptFor == 38)   // kPromptTrackFlags: the letters of the boxes that are ticked
	{
		static char const letters[4] = { 'L', 'B', 'P', 'S' };
		std::string typed;

		for (size_t i = 0; i < formRows.size() && i < 4; i++)
		{
			typed += formRows[i].checked ? std::string(1, letters[i]) : std::string();
		}

		return typed.empty() ? std::string("0") : typed;
	}

	// the sound: name, inner range, outer range, volume, pitch, loop
	std::string typed;

	for (size_t i = 0; i < formRows.size(); i++)
	{
		std::string const part = formRows[i].isCheck ? std::string(formRows[i].checked ? "1" : "0") : formRows[i].text;

		typed += (i ? " " : "") + (part.empty() ? std::string("!") : part);
	}

	return typed;
}

static void FormOk()
{
	promptText = FormTyped();
	RunThePrompt();
}

static void FormCancel()
{
	formRows.clear();
	promptFor = kPromptNone;
	promptText.clear();
	trackEditWords = "Cancelled";
	statusWords = trackEditWords.c_str();
}

void FormKey(UINT message, WPARAM key)
{
	if (formRows.empty())
	{
		return;
	}

	FormRow &row = formRows[formFocus];

	if (message == WM_CHAR)
	{
		if (key == ' ' && row.isCheck)
		{
			row.checked = !row.checked;
		}
		else if (!row.isCheck && key >= 32 && key <= 126 && row.text.size() < 63)
		{
			row.text += (char)key;
		}

		return;
	}

	if (message != WM_KEYDOWN)
	{
		return;
	}

	if (key == VK_ESCAPE)
	{
		FormCancel();
	}
	else if (key == VK_RETURN)
	{
		FormOk();
	}
	else if (key == VK_BACK && !row.isCheck && !row.text.empty())
	{
		row.text.pop_back();
	}
	else if (key == VK_DOWN || key == VK_TAB)
	{
		formFocus = (formFocus + 1) % formRows.size();
	}
	else if (key == VK_UP)
	{
		formFocus = (formFocus + formRows.size() - 1) % formRows.size();
	}
}

// A press at x, y in the window: rows are lines 1 to n of the overlay, the OK / Cancel line follows
static void FormClick(ui::Box const &box, int32_t x, int32_t y, int32_t lineHeight)
{
	if (formRows.empty() || !ui::BoxHolds(box, x, y))
	{
		return;
	}

	int32_t const line = (y - box.y - 3) / (lineHeight > 0 ? lineHeight : 1);

	if (line >= 1 && (size_t)line <= formRows.size())
	{
		formFocus = (size_t)line - 1;

		if (formRows[formFocus].isCheck)
		{
			formRows[formFocus].checked = !formRows[formFocus].checked;
		}
	}
	else if ((size_t)line == formRows.size() + 1)
	{
		if (x < box.x + box.width / 2)
		{
			FormOk();
		}
		else
		{
			FormCancel();
		}
	}
}

// The words of the form's lines: the title, a line a row, then "OK / Cancel" and the keys
void FormLines(std::string const &title, std::vector<std::string> *words)
{
	words->push_back(title);

	for (size_t i = 0; i < formRows.size(); i++)
	{
		FormRow const &row = formRows[i];
		std::string line = i == formFocus ? "> " : "  ";

		line += row.isCheck ? (row.checked ? "[x] " : "[ ] ") + row.label : row.label + ": " + row.text + (i == formFocus ? "_" : "");
		words->push_back(line);
	}

	words->push_back("      [ OK ]                [ Cancel ]");
	words->push_back("Up / Down: move, Space: tick, Enter: OK, Esc: Cancel");
}


// ---- THE MODE'S EXIT (the exit sweep). Every inteng mode's Exit does the same four things, read in each handler: (1) the mode's own write-back of what it holds (Path: the working path, FUN_004fab40; Hierarchy:
// FUN_00481b60 when it changed; Waypoint, Light Sourcing, Object Placement, the others: what they hold is in the level already here), (2) FUN_004719e0: the LEVEL FILE IS WRITTEN (silently, the old editor
// wrote over the opened file; here the level is saved as a COPY, never over the opened file: Save a copy's own place), (3) FUN_004704d0(10): "Camera position saved" (the one camera slot every mode shares, so the
// camera is where it was when the next mode starts: the editor has one camera, which already does that), (4) the hub (FUN_00447550). Strategy Data's Exit (0x5, FUN_00524e90 -> FUN_00524d80 -> FUN_00524d00)
// first asks "Would you like to save first?" when the mode changed something (its flag DAT_00914950, set by every edit), and saves (FUN_005282d0 "Saving") on yes; no leaves without saving. Ours: the level is
// "changed" when its Changes() is not the one the last Save a copy (or the opening) left. AvP-docs\engine\inteng\exit-and-strategy-leftovers.md has every address.
std::string exitWords;
void ExitCleanup(size_t screen);

bool LevelChangedSinceSave()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	return open && copySavedChanges != ~0u && open->Changes() != copySavedChanges;
}

// ---- THE ROTATION CENTRE (camera-closeout-drake.md CAM-3 / CAM-7 / CAM-8; inteng's DAT_00931110 / 14 / 18): the point '/' and numpad / swing the camera round (MoveTheCamera). Every start of the hub
// sets it to the average place of the level's objects (FUN_00470e70, called by the hub's set-up FUN_00447550 at 0x447652: the blocks of the level's own shapes, one for each of its objects, modules and
// placed objects alike; each place divided by their count before it is added, inteng's rounding; 0, 0, 0 for none). Scroll Lock (FUN_004720c0, WinMain 0x58d004: slot 0x22) puts it on the object or
// module the next left press hits: "Choose object for centre of rotation" at (200, 100) and "RMB or escape to cancel" at (200, 0x74) until then; a press that hits nothing waits on; the right button or
// Esc gives up. OURS, NAMED: the places are the game's (the file's times the level's scale, local_scale), so the rounding is in millimetres; inteng's wait is modal (the view stands still), ours draws on.
int32_t rotationCentre[3] = { 0, 0, 0 };
bool rotationPickWaiting = false;
static bool rotationPickButtonHeld = false;   // a left or middle press of the wait is still down: a miss picks again on each move, and Esc pressed then gives up (FUN_004720c0's loop goes round with the button bits kept)

void RotationCentreDefault()
{
	rotationCentre[0] = rotationCentre[1] = rotationCentre[2] = 0;

	std::vector<int32_t> places;

	if (!LevelObjects_Places(&places) || places.size() < 3)
	{
		return;
	}

	int const count = (int)(places.size() / 3);
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	for (size_t at = 0; at + 2 < places.size(); at += 3)
	{
		for (int i = 0; i < 3; i++)
		{
			int32_t const world = (int32_t)floor(places[at + (size_t)i] * scale + 0.5);

			rotationCentre[i] += world / count;   // FUN_00470e70 0x470ebf-0x470ee7: each divided first (IDIV, toward 0, as C++'s /)
		}
	}
}

// Scroll Lock's press answered (FUN_004720c0 0x472142-0x472180): the place of the module or object the pick hit (a module: MODULE.m_world; an object: its file place times the level's scale); false, and the
// wait goes on, for a press that hit neither
static bool RotationPickAnswer(host::PickReport const &picked)
{
	int32_t world[3] = { 0, 0, 0 };
	bool found = false;

	if (picked.found == host::PickFound::Module)
	{
		found = LevelView_ModuleWorld(picked.moduleIndex, world);
	}
	else if (picked.found == host::PickFound::Object)
	{
		int32_t place[3] = { 0, 0, 0 };
		double const scale = local_scale > 0.0 ? local_scale : 1.0;

		found = LevelObjects_PlaceOf(picked.objectId, place);

		for (int i = 0; i < 3 && found; i++)
		{
			world[i] = (int32_t)floor(place[i] * scale + 0.5);
		}
	}

	if (found)
	{
		memcpy(rotationCentre, world, sizeof(rotationCentre));
		rotationPickWaiting = false;
		rotationPickButtonHeld = false;
	}

	return found;
}

// After the mode's Exit has saved or not: the mode's own state is let go, the camera position is said to be saved, the hub is shown
static void LeaveTheMode(char const *saved, bool cameraSaved = true, size_t target = host::kIntengHub)
{
	size_t const was = barScreen;

	ExitCleanup(was);

	// a mode's Exit frees its own list (Object Placement FUN_004d9af0 0x4d9d58-0x4d9e99, Texture Map FUN_0054bfd0 ...) and the hub starts with nothing selected (FUN_00447550; its lists were freed
	// when it was left, FUN_004476d0): nothing picked in one mode is selected in the next (reassess/mode-entry-exit-frost.md SEL-1)
	host::Selection_Clear();

	if (cameraSaved)
	{
		CameraPositionSaved(10);   // FUN_004704d0(10): the slot the next mode starts from (the modes' exits, camera-saved-vasquez.md CS-5 .. CS-12)
	}

	exitWords = std::string(saved && *saved ? saved : "") + (saved && *saved && cameraSaved ? "; " : "") + (cameraSaved ? "Camera position saved" : "");
	exitWords = exitWords.empty() ? std::string(target == host::kIntengHub ? "Construction" : "Object Placement") : exitWords;
	ShowTheScreen(target);

	// the next screen's start loads the level from inteng's tree in memory (the hub's FUN_00447550, FUN_0046e700), so the view shows what the mode changed: ShowTheWorkingBytes, as the
	// screen change of a bar pick does (RunAction). ShowTheScreen sets barScreen itself, so a mode's Exit never reached that screen change and the hub kept the picture of the level as it
	// was opened. (Added 2026-11-17 by Melanikus (Dietrich), the view gap; found by a read-only trace of acceptance-view-gap-same-size.)
	ShowTheWorkingBytes();
	statusWords = exitWords.c_str();

	if (target == host::kIntengHub)
	{
		RotationCentreDefault();   // the hub's start (FUN_00447550 0x447652): a Scroll Lock pick lasts until then
	}
}

// Strategy Data's Module flags mode (command 0x2, FUN_00525700): bar page 1 of both sets is shown (+0x18(1) at 0x525725): bar A = Mode > Exit, Help; bar B = the flag menus (Add Flag, Remove Flag, View Flag, Save); its Exit (0x5) gives page 0 back (+0x18(0) at 0x5267e3) and does not leave the screen
bool strategyFlagPage = false;
bool strategyPanelOn = false;   // Edit Strategy opened the field panel (our stand-in for the strategy's own view): until the next click on the screen the panel may show; otherwise Strategy Data draws none
// THE VOLUME EDITOR (FUN_005277c0, "V : Edit Volume"; Frost 2026-11-16): inteng takes the screen over: the volume is drawn as a RED WIREFRAME CUBE (FUN_00583130 with 0xff0000, block flag 0x10000000); the text lines at (16, 0 ..):
// "S : Set volume to module extents" (0x5ee12c), "X : Expand volume to include module" (0x5ee108), "Shift+Left Click : Select face of cube" (0x5ee0e0), "+/- : Move selected face" (0x5ee0c4), "Hold shift to move face faster"
// (0x5ee0a4), "Press return when finished" (0x5edfe4). A left click picks a MODULE (it turns red: FUN_00529470); Shift + left click picks a FACE of the cube (FUN_00559a60: 0 min X, 1 max X, 2 min Y, 3 max Y, 4 min Z, 5 max Z; the
// face turns green, 0x00ff00); S sets the cube to the picked module's box, X grows it to include that box; + / - move the selected face out / in by 200000 units a second (2 metres, 20 with Shift: FUN_00527e00 is the 16.16 product of
// the speed and the frame time, at most 0x199a = 0.1 s), a face never crossing its opposite; Return or the right button ends (Escape does nothing). The numbers are in inteng's units (file x 100 x scale); here the cube is in millimetres.
VolumeEditor volume;
static void VolumeEnd();
void VolumeBegin();
static bool VolumeKey(WPARAM key);
static void VolumeClick(host::PickReport const &report, bool shift, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
int32_t strategyViewTop = 0;   // the y of the Edit view's box's top in the panels' pixels: the view's lines and clicks are measured from it (inteng's FUN_0057bd90 y 0 is its screen's top)
int32_t strategyViewLeft = 0;  // and its left (the owner's box, 11-17: the views are drawn in a centred box; the object pick stays the whole window, from 0)
int32_t strategyViewBox[4] = { 0, 0, 0, 0 };   // the Edit view's box as drawn (x, y, width, height), all 0 when none is up or the pick runs: the state line's strategybox
int32_t textAreaTop = 0;   // where inteng's text area starts under the menu bars (the lowest bottom of the bars), set each frame by the drawing code
std::string strategyViewWord;           // the lines of the Edit view of the frame, "x@y:text|..." (the state line's strategyview)
std::string strategyPickField;       // the object field a Pick line waits to fill (the next click on an object in the view fills it), empty for none
std::string strategyViewPickWords;   // the words drawn along the foot while it waits ("Select associated door")
int32_t strategyEditKind = 0;     // the strategy kind whose Edit view is up (strategy_views.h), 0 for none
std::vector<std::array<uint8_t, 8>> strategyViewLinks;   // a Linked / Multi / Area switch's Links, made again as its view opened (FUN_0052dc90; S-K61-1)
bool strategyEditFan = false;   // the Fan's Edit Strategy view is up (FUN_005356b0: drawn lines, a click on one asks its number, Escape ends)
void StrategyViewEnd();   // below, with the views
uint8_t strategyEditId[8];        // the object whose view is up: the current object when Edit / Assign called its method +0x14 (DAT_0091496c), kept while a pick makes another one red
bool strategyPickHave = false;    // the object pick holds an object (FUN_00527540's param_1, drawn red)
uint8_t strategyPickId[8];
int32_t strategyPickRow = 0;      // what the end of the pick does (host::StrategyPickRow)
std::string strategyPickRequest;  // kPickTarget: the request field set to 1 when the target changed
void StrategyPickEnd();
void LiftPositionsEnd();   // the Lift's positions collector ends (FUN_0052b360 after FUN_00526bf0)
void MissionTargetsEnd();  // the Mission Objective's targets collector ends (FUN_00532ea0)
void MissionToggleUnderPointer(int32_t bit);   // the Mission Objective's P / V (S-K70-8)
int32_t trackPage = -1;    // a Track Object's view: -1 its main page, else the point whose page is open (S-K72-4)
int32_t multiPage = 0;     // a Multiple Target Switch's view: its page of 15 targets, from 0 (FUN_0052f700 local_6c; Space the next, S-K66-2)
void MultiNextPage();      // Space on that view (below)
void MultiKeyUnderPointer(int key, bool shift);   // S / D / T on that view (below)
void MultiTargetsEnd();    // its targets' collector ends (FUN_0052ff10, below)
bool requestPageOn = false;          // E: a target's extra request data page is up over the view (S-KC-8: FUN_00529b30 -> the target's wrapper +0x10)
host::RequestPageState requestPage;  // what the page edits (strategy_views.h)
uint8_t requestPageTarget[8] = { 0 };   // the target the page is about (a placed hierarchy's page lists its sequences, S-K76-E)
std::string requestPageField;        // the view's request field the page puts its word back into (62 / 80 "target_request", 69 "request"); empty for a point page's target
int32_t requestPageRow = -1;         // the Track point page's target row whose request word the page edits (S-K72-6)
void RequestPageEnd(bool escaped);   // below, with the views
void StrategyRequestE();             // E on a view (below)
void TrackTargetsEnd();    // a point page's targets collector ends (FUN_00534df0)
void TrackPageBack();      // the right button on a point page (S-K72-7)
void TrackAskPoint(uint8_t const *subject);   // "Which track point?" (S-K72-2)
void StrategyViewClickAt(int32_t x, int32_t y);   // below, with the views

// COPY STRATEGY (FUN_00528340 with the collector FUN_00526bf0): the object to copy from is the current one; then the collector runs: a click adds the object under the pointer to the list (when it is not in it), Shift + click takes it out, L empties the list,
// Return or the right button ends; the screen draws "Press return when finished" at (16, 0) and the names from the third row on (rows of 16; DAT_00914928 is incremented twice before the first name), and "Select objects to copy to" along the foot (200, height - 20).
// Then each target in the list is dealt with in order: the source itself and a target whose class cannot take the kind are passed; one that has a strategy FUN_00521860 does not call replaceable gets the four-answer question (CopyQuestion, FUN_00529f30: Yes All / Yes / No / No All; Escape replaces); a copy that fails says "Unable to copy this strategy" and the rest is abandoned.
// Ctrl + click (S-79, 0x526e3b-0x526ecc) adds every object of the level that uses the clicked object's shape (CopyCollectShapeGroup).
bool copyCollecting = false;
int32_t copyPurpose = 0;   // what the collector's list is for: 0 Copy Strategy's targets, 1 a Lift's positions (FUN_0052b360, the Lift view's positions block)
uint8_t copyFrom[8];
int32_t copyKind = 0;
static size_t copyDone = 0;
std::vector<CopyTarget> copyTargets;
// STABILITY FLAGS (0x37, FUN_00527370 with FUN_00527020 and FUN_00527210): the picked module's box is drawn (FUN_00447f10: eight corners from the shape's extremes, six faces: x min, x max, y min, y max, z min, z max, in the order of the six bits 0x80000 .. 0x4000 of the
// shape's flag word), each face RED when its bit is set (stable) or GREEN when not; the screen says "Red : stable", "Green : unstable", "T : toggle polyline on/off", "Press return when finished" (rows of 16 from (16, 0)); a click on a face (the polygon under the pointer, FUN_00559750) turns it red <-> green;
// T switches the faces between filled and lines; Return or the right button ends and asks "Would you like to keep the changes?" (Yes writes the six bits into the shape's flag word).
// The faces are filled (the avpx hook R_SetEditorWorldTriangles); T switches them to their outlines (lines in the face's colour).
bool stabilityActive = false;
static uint8_t stabilityObject[8];
static uint32_t stabilityFaces = 0;     // bit i set: face i is red (stable)
static bool stabilityPolygons = false;   // T: the faces as polygons (the cross) or as lines only; they START as lines (FUN_00447f10 0x44816a: polygon type 0xe; T at 0x5274c9 switches 0xe <-> 2)
static int32_t stabilityMin[3], stabilityMax[3];

// Strategy Data runs one of the loops whose frame is FUN_00529370 (the flag page FUN_00525700, Stability FUN_00527370, the volume editor FUN_005277c0, the collector FUN_00526bf0, the pick loop
// FUN_00527540): only there Insert / Home zoom (0x5293a2 / 0x5293b0). The screen's own page 0 (FUN_00522980) and a kind's Edit view (its own loop) read neither (strategy-data.md S-57).
// The volume editor, the collector and the pick also run FROM a kind's Edit view (V; a Lift's positions FUN_0052b360 0x52b42c, a switch's, a mission's or a track's targets; every object line's pick
// FUN_00527540), so strategyEditFan is still set while they do: their loop is FUN_00529370's all the same (S-VOL-11, Drake 11-17; the collector and the pick, S-57, Bishop 11-18: before, the view's
// flag shut them out, so H / N, Insert / Home and the "View Distance" line did nothing there). Only the view's own loop, with none of them running, reads none of these.
bool StrategyInASubLoop()
{
	return OnStrategyScreen() && (volume.on || strategyFlagPage || stabilityActive || copyCollecting || strategyPick != 0 || !strategyPickField.empty());
}

std::string strategyMessage;          // an on-screen message of Strategy Data at (200, 100) (FUN_0057bd40(200, 100, 0, words)), gone after 700 ms (FUN_00529220 waits that long for a click or key)
ULONGLONG strategyMessageAt = 0;

void StrategyMessage(std::string const &words)
{
	strategyMessage = words;
	strategyMessageAt = GetTickCount64();
}

static bool OnAPlacementScreen();

// OBJECT PLACEMENT'S EXIT (0xa) AND SAVE (0xe) run every placed shape's save first (FUN_004d9af0 0x4d9d66 / 0x4d9eba: each object of the placement list, its +0xc = FUN_004e1f90; OP-130a), which also
// gives an object of Type "Placed light" its default light and takes a light off any other Type (light-closeout-frost.md TB-L2): simple_object_edit.h SimpleObject_SaveAll. A refusal is said and
// the Exit / Save goes on with the objects saved before it.
static std::string placedSaveWords;

static void SaveEveryPlacedShape()
{
	if (!OnAPlacementScreen())
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		return;
	}

	unsigned const generation = open->Generation();
	size_t saved = 0, changed = 0;
	host::SimpleObjectResult const result = host::SimpleObject_SaveAll(open, &saved, &changed);

	if (open->Generation() != generation)
	{
		LoadTheObjectListAgain();
	}

	if (result != host::SimpleObjectResult::Done)
	{
		placedSaveWords = std::string("The placed objects' save stopped after ") + std::to_string(saved) + ": " + host::SimpleObjectWords(result);
		LogString(placedSaveWords.c_str());
	}
}

void ExitTheMode()
{
	if (barScreen >= host::kIntengScreenCount || barScreen == host::kIntengHub)
	{
		return;
	}

	if (onTextureMap && TextureMergeHoldsTheExit())
	{
		return;   // Texture Map's Merge asks its save question first (texture_submodes.cpp)
	}

	// OP-6: Object Placement's Exit saves every object (its block's place): the places Gravity moved them go into the level first, then every placed shape is saved
	PlacementGravityFlush();
	SaveEveryPlacedShape();

	bool const strategy = strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0;
	// Hierarchy: inteng's Exit (FUN_004888d0 0x488967) saves when the hierarchy code marked the file changed (DAT_00637134): ours, the model differing from its last save
	bool const hierarchyChanged = IsTheHierarchyScreen(barScreen) && hierarchyModelOpen && hierarchyModelFile.Generation() != hierarchyModelSavedGeneration;
	// Strategy Data: ONLY its own mark (FUN_00524d00 0x524d1d: DAT_00914950, W-3): a change carried in from Object Placement is not asked about, and the mode's Exit writes nothing
	// silently (FUN_00524d80 calls no FUN_004719e0). The other modes: the level changed since the last save (their Exits write the level).
	bool const changed = strategy ? StrategyChanged() : (LevelChangedSinceSave() || hierarchyChanged);

	// THE FLAG PAGE'S EXIT (strategy-data.md S-14, FUN_00525700 0x5267cd-0x526845): the bar's page 0 (+0x18(0)), every listed module's red off and the list freed, the flag view STOPPED
	// (FUN_0052f680(0): DAT_00914944 = 0, nothing coloured for a flag) and F / Shift + F's commands DAT_00914948 / DAT_0091494c = 0; back on the screen, not out of Strategy Data
	if (strategy && strategyFlagPage)
	{
		strategyFlagPage = false;
		host::Selection_Clear();   // the loop's list goes with it
		strategyViewBit = 0;       // S-14: FUN_0052f680(0) 0x526831
		strategyViewAdd = 0;       // S-14: 0x52683f / 0x526845
		EditorBar_SetPage(0);

		if (!LoadTheMenus())
		{
			LogString("menus: the Strategy Data bars couldn't be made, so the editor closed");
			running = false;
			bRunning = false;
		}

		statusWords = "Strategy Data";
		return;
	}

	// Shape Morph's Exit (FUN_004ca190, command 10): the menu object and the mode's state are let go (FUN_004ca170) and the hub is shown: no save, no camera slot
	if (strcmp(host::kIntengScreens[barScreen].name, "Shape Morph") == 0)
	{
		LeaveTheMode("", false);
		return;
	}

	if (strategy && changed)
	{
		memset(keyDown, 0, sizeof(keyDown));
		ConfirmAs(kPromptExitSave, "Would you like to save first?");   // 0x5edd8c, FUN_00474a40: Yes / No
		return;
	}

	bool const back = strategy && strategyFromPlacement;

	if (changed && !strategy)
	{
		SaveTheCopy();
		LeaveTheMode(savedWords.c_str(), !back, back ? ScreenOfTable("0x5e51b8") : host::kIntengHub);
		return;
	}

	LeaveTheMode("", !back, back ? ScreenOfTable("0x5e51b8") : host::kIntengHub);
}

// "Would you like to save first?": yes saves a copy and leaves, no leaves, anything else stays
static void RunExitSavePrompt(std::string const &typed)
{
	std::string lower;

	for (char ch : typed)
	{
		lower += (char)tolower((unsigned char)ch);
	}

	bool const back = strategyFromPlacement;   // only Strategy Data asks
	size_t const target = back ? ScreenOfTable("0x5e51b8") : host::kIntengHub;

	if (lower == "yes" || lower == "y")
	{
		StrategySave();   // FUN_00524d00: Yes -> FUN_005282d0 (the Links made again, S-K61-3)
		LeaveTheMode(savedWords.c_str(), !back, target);
	}
	else if (lower == "no" || lower == "n")
	{
		LeaveTheMode("not saved", !back, target);
	}
	else
	{
		statusWords = "Strategy Data stays open: Exit asks \"Would you like to save first?\" and takes yes or no";
	}
}

// Move rel. to poly (0x51, inteng FUN_00564b60 + FUN_005633e0): the item waits for a click on a polygon of ANOTHER object (the right button gives it up), then asks the distance
// from the polygon in metres in inteng's number window, its start the last distance used, and puts the mover there (move_rel_poly_edit.h): the flying object, or the camera (StartMoveRelPoly).
extern double local_scale;   // the level's scale, as the game's loader left it (chunk_load.cpp:63)

// The object of the level file that a module of the game is (polygon_items.h PolygonItems_ModuleObject): by the name and the world place the game keeps for the module
bool ModuleObject(OpenLevelFile *open, int32_t moduleIndex, uint8_t id[8])
{
	std::string name;
	int32_t world[3];

	return open && open->Level() && LevelView_ModuleName(moduleIndex, &name) && LevelView_ModuleWorld(moduleIndex, world) &&
		host::PolygonItems_ModuleObject(open->Level(), name.c_str(), world, local_scale > 0.0 ? local_scale : 1.0, id);
}

static const int32_t kClickMoveRelPoly = 0x51;
bool moveRelWaiting = false;
static bool parentWaiting = false;   // Q's "Click on parent for generator" is waiting (OP-90): a right press gives it up with nothing changed
static uint8_t moveRelMover[8];
static uint8_t moveRelTarget[8];
static int32_t moveRelPolygon = -1;
static int32_t moveRelHit[3];
static int32_t moveRelLastDistance = 0;   // inteng keeps the last distance and offers it again (FUN_00564b60)

// WHAT MOVES (OP-42; listing-all-3 0x4da1d5-0x4da214, read by Vasquez 11-17): the current object when it is FLYING and the camera is not linked to it (DAT_008f4f44, DAT_005e78bc == 0: FUN_00564b60 of its
// block); otherwise the CAMERA's own block (DAT_00962304 +0x68): the VIEW goes to the polygon. No selection is needed. (Until 11-17 ours moved the selected object in the file, flying or not.)
static int moveRelMoverKind = 0;   // 1: the flying object (its live pose; the file gets it when the flight stops, as inteng's block); 2: the camera
static bool PlacementFlyIsTheMover(uint8_t id[8]);                            // defined with the fly's state
static void PlacementFlyTakePose(int32_t const world[3], float const turn[4]);   // defined with the fly's state
static std::string AcousticShown(float value);
static bool AcousticTyped(std::string const &typed, float *value);

static void StartMoveRelPoly()
{
	moveRelMoverKind = PlacementFlyIsTheMover(moveRelMover) ? 1 : 2;

	if (moveRelMoverKind == 2)
	{
		memset(moveRelMover, 0, sizeof(moveRelMover));
	}

	// FUN_00564b60 0x564ba2-0x564bb0: "Move relative to which poly" at (200, 100) while it waits (drawn with the frame's text: MoveRelPolyWaitingLine)
	moveRelWaiting = host::ClickMode_Start(kClickMoveRelPoly, host::ClickWants::Polygon, "Move relative to which poly");

	if (!moveRelWaiting)
	{
		statusWords = "That command can't wait for a click";
	}
}

// FUN_00564b60 0x564c5d-0x564ca0, the window's Ok: the typed metres read as FUN_00474a10 reads them (sscanf "%f"), times 100000.0 (the float at 0x5ac9a0) and cut toward 0 (__ftol): the distance,
// kept as the next one offered (DAT_00914d2c); then FUN_005633e0's place and matrix put on the mover. INTENG BUG, FIXED (as Acoustic Mode's reverb): a word that is no number left FUN_00474a10's
// float unset (the window's own address), so inteng moved to a garbage distance; here it does nothing.
static void MoveRelPolyAnswer(std::string const &typed)
{
	float metres = 0.0f;

	if (!AcousticTyped(typed, &metres) || !(fabs((double)metres * 100000.0) < 2147483647.0))
	{
		return;
	}

	int32_t const distance = (int32_t)((double)metres * 100000.0);   // a float times a float, held by the x87 in more bits: the double gives the same product
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	moveRelLastDistance = distance;

	if (!open || !open->Level())
	{
		return;
	}

	int32_t place[3], matrix[9];
	std::string why;
	host::MoveRelPolyEditResult const result = host::MoveRelPolyEdit_Pose(open, open->Level(), moveRelTarget, moveRelPolygon, distance, moveRelHit, local_scale > 0.0 ? local_scale : 1.0,
		place, matrix, &why);

	if (result != host::MoveRelPolyEditResult::Done)
	{
		Answer(std::string("Not moved: ") + host::MoveRelPolyEdit_Words(result));
		return;
	}

	int32_t const world[3] = { place[0] / 100, place[1] / 100, place[2] / 100 };   // inteng's unit (1e-5 m) to the game's millimetres, cut toward 0

	if (moveRelMoverKind == 1)
	{
		float turn[4];

		host::MoveRelPolyEdit_MatrixToTurn(matrix, turn);
		PlacementFlyTakePose(world, turn);
	}
	else if (theCamera)
	{
		host::CameraFileSlot slot;

		memcpy(slot.place, world, sizeof(slot.place));
		memcpy(slot.matrix, matrix, sizeof(slot.matrix));
		*theCamera = CameraOfFileSlot(slot);   // the camera's block +0x38 and +0x10, as a camera position is kept (camera_file.h)
	}
}

// The click that names the polygon: the object under it, then which of the object's FILE polygons the click's line meets (file_pick.h), then the prompt for the distance
static void MoveRelPolyClicked(host::PickReport const &report)
{
	moveRelWaiting = false;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !theCamera || !report.hasHit)
	{
		statusWords = "Move rel. to poly: nothing to measure from";
		return;
	}

	uint8_t target[8];
	if (report.found == host::PickFound::Object)
	{
		memcpy(target, report.objectId, 8);
	}
	else if (!ModuleObject(open, report.moduleIndex, target))
	{
		statusWords = "Move rel. to poly: the level file has no single object for that module";
		moveRelWaiting = host::ClickMode_Start(kClickMoveRelPoly, host::ClickWants::Polygon, "Move relative to which poly");
		return;
	}

	if (moveRelMoverKind == 1 && memcmp(target, moveRelMover, 8) == 0)
	{
		// the moving block's own polygon ENDS it, nothing moved (0x564c12-0x564c1c -> 0x564ce2)
		return;
	}

	// the click's line in FILE units: from the camera through the point the game's pick hit
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
		statusWords = "Move relative to which poly: no polygon of that object under the click - click again";
		moveRelWaiting = host::ClickMode_Start(kClickMoveRelPoly, host::ClickWants::Polygon, "Move relative to which poly");
		return;
	}

	memcpy(moveRelTarget, target, 8);
	moveRelPolygon = hit.polygon;
	memcpy(moveRelHit, hit.point, sizeof(moveRelHit));

	// 0x564c22-0x564c53: FUN_004747f0's window "distance to move from the poly in metres :" (0x6013e0), its start the last distance x 1e-5 as a float (0x5ac358); FUN_0057c800, answer 3 is Ok
	DialogLine(std::string(), "distance to move from the poly in metres :", AcousticShown((float)((double)moveRelLastDistance * 1e-05)), 200, 0x12, [](std::string const &typed)
	{
		MoveRelPolyAnswer(typed);
	});
}

// Module Linking > Show All (0xca, FUN_004c1a90 0x4c1eda-0x4c2158): every module that has an object is painted by its polygon figure (module_poly_count.h: its own polygons plus those of the modules its
// VMDARRAY lists): green 0x00ff00 below 1200, blue 0x0000ff 1200 to 1499, orange 0xff7f00 1500 to 1999, RED 0xff0000 2000 and up (0x4c207b: 0xfa + 0xfeff06; inteng's own legend says "2000+(Quads) RED"),
// white when it has no MODULEDT or no VMDARRAY (the colours are 0xRRGGBB as the tree's are: the base blue 0x0000ff, a link yellow 0xffff00, as the manual says). NAMED: inteng REPAINTS the shape's
// polygons in that colour, here the colour is the editor's half see-through tint over the level. On Module Linking the screen is then HELD until Escape or a mouse button (showAllHeld, row ML-M-3).
// everyFigure: Object Placement's Show Polygon Counts (OP-48): FUN_004d5e60 counts a module's own polygons and adds those of its VMDARRAY only when it has a MODULEDT, so a module with no link data
// is painted by its own figure there, never white (white for "no data" is Module Linking's Show All rule)
static void ShowAllTheModules(bool everyFigure = false)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		statusWords = "Show All: no level is open";
		return;
	}

	showAllModules.clear();
	showAllColours.clear();
	showAllHeld = OnModuleLinkingScreen();   // DAT_0070439c = 1 (0x4c214a); Object Placement's Show Polygon Counts uses the paint without the hold

	size_t byColour[5] = { 0, 0, 0, 0, 0 };

	for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
	{
		uint8_t id[8];
		int32_t count = 0;
		bool figure = false;

		if (!ModuleObject(open, m, id) || !host::ModulePolyCount_Of(open, open->Level(), id, &count, &figure))
		{
			continue;
		}

		showAllModules.push_back(m);
		figure = figure || everyFigure;
		showAllColours.push_back(host::ModulePolyCount_Colour(figure, count));
		byColour[!figure ? 4 : count >= 2000 ? 3 : count >= 1500 ? 2 : count >= 1200 ? 1 : 0]++;
	}

	static std::string words;

	words = std::string(everyFigure ? "Show Polygon Counts: on: " : "Show All: ") + std::to_string(showAllModules.size()) + " modules painted by polygon count: " + std::to_string(byColour[0]) + " green (under 1200), " + std::to_string(byColour[1]) +
		" blue (1200 up), " + std::to_string(byColour[2]) + " orange (1500 up), " + std::to_string(byColour[3]) + " red (2000 up), " + std::to_string(byColour[4]) + " white (no data)";
	statusWords = words.c_str();
}

// Object Placement > Misc > Show Polygon Counts (0x5a, FUN_004d9af0 0x4d9f96-0x4da05f): flips DAT_008f4f5c, then every loaded module that is a module (flag 0x200) is painted by its polygon count (FUN_004d5e60;
// under 1200, 1200 to 1499, 1500 to 1999, 2000 and up: the same figure and colours as Module Linking's Show All) or, flipped off, painted as it was (FUN_0046dbf0). NAMED: the editor's tint over the level
// as in Show All (inteng ORs the figure's bits into the module shape's items for the renderer: the same colours); a module with no link data is painted by its own figure (ShowAllTheModules(true)).
bool placementPolyCounts = false;

static void PlacementPolyCountsToggle()
{
	placementPolyCounts = !placementPolyCounts;

	if (placementPolyCounts)
	{
		ShowAllTheModules(true);   // its words: "Show Polygon Counts: on: N modules painted by polygon count: ..."
	}
	else
	{
		showAllModules.clear();
		showAllColours.clear();
		LevelView_SetModuleTints(nullptr, nullptr, 0);
		statusWords = "Show Polygon Counts: off";
	}
}

// Construction > Mode > Texture Map (0x46, inteng FUN_00544510 starts it, FUN_0054bfd0 leaves it; p11-shape-handlers.md sections 51-54). WHAT IS DONE so far: the mode is a
// screen of its own ("Texture Map", a stand-in menu until its code-built menus are paired); entering it draws the level as Polygon (inteng's start sets its draw mode 2) and
// leaving puts the draw mode back; a LEFT click selects a polygon and SHIFT+left click deselects it, L deselects all (inteng's own words for the mouse, section 52). The texture
// commands themselves (flips, wrap, tile, rotate, apply) are not wired yet.
// NAMED DEVIATIONS: (1) inteng's start first checks that the level's textures are loaded (FUN_00544110) and goes back to Construction when one failed; here the game's own loader has
// the textures and there is no such check. (2) inteng selects the game's split triangles of a quad (Q switches to whole quads); here a click selects the FILE polygon whole (file_pick.h,
// the polygon the level file names), which is what the texture model edits (uv_edit.h). (3) The selection is not drawn in the view yet (inteng recolours the polygon): the status line
// says what was selected; the highlight waits for an editor-only AvPx hook. (4) The click picks the nearest polygon of the object the game's pick found, not inteng's nearest over every
// object (FUN_00559750 mode B).
bool onTextureMap = false;
static host::DrawMode textureMapSavedMode = host::DrawMode::Polygon;


std::vector<TexturePolygon> texturePolygons;
std::string textureWords;   // kept here so statusWords can point at it

void ShowTheTexturePolygons();
void TextureForEveryShape(OpenLevelFile *open, std::function<void(uint8_t const *id, bool isModule, int32_t moduleIndex)> const &each);
int texturePlaneMode = 0;   // 0 = none; 1 = the next click names the plane for WRAP; 2 = for TILE
size_t TextureSelectedIndex(uint8_t const *id, int32_t polygon);
bool TextureSelectPolygon(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon, bool select);
uint32_t textureFlagViewMask = 0;
size_t textureFlagViewCount = 0;   // how many polygons the last Flags > View found
bool TexturePolyMarkedForSplit(uint8_t const *id, int32_t polygon);
bool textureViewAnimated = false;

bool IsTheTextureMapScreen(size_t screen)
{
	return screen < host::kIntengScreenCount && strcmp(host::kIntengScreens[screen].name, "Texture Map") == 0;
}

void TextureModeStateReset();   // the mode's held states (reassess/mode-entry-exit-frost.md TM-E1 / TM-X2), defined with them below
void TextureLeaveQuestion();   // FUN_0055c070(1): the question when the mode is left (texture_map_screen.cpp)

static void EnterTextureMap()
{
	if (onTextureMap)
	{
		return;
	}

	onTextureMap = true;
	TextureModeStateReset();   // FUN_00544510 0x544551: the selection mode DAT_00914d9c = 0 on every entry (TM-E1)
	if (ViewToggles_Now().headlight) ViewToggles_UseNow(host::ViewToggle::Headlight);   // 0x54bd9b: the headlight off at every entry (DAT_00962308 +0x84 = 0); Ctrl+B (0x13e) turns it on again
	textureMapSavedMode = ViewToggles_Now().drawMode;
	texturePolygons.clear();
	ShowTheTexturePolygons();

	if (textureMapSavedMode != host::DrawMode::Polygon)
	{
		ViewToggles_UseNow(host::ViewToggle::Polygon);
	}

	statusWords = "Texture Map: left click selects a polygon, Shift+left click deselects it, L deselects all";
}

void LeaveTextureMap()
{
	if (!onTextureMap)
	{
		return;
	}

	TextureMergeLeave();   // Merge (sub-mode 2) lets its view and picks go (texture_submodes.cpp)
	TextureMultiLeave();   // the multiple node mode (sub-mode 3) is left first (FUN_005604f0, texture_submodes.cpp)

	onTextureMap = false;
	TextureWrapPolygons(texturePolygons);   // FUN_0054bfd0 0x54c0af: FUN_0056e0a0 brings each selected polygon onto its picture as it clears the list (texture_shared.h)
	texturePolygons.clear();   // FUN_0054bfd0 "delete poly list": DAT_00914dcc freed (TM-X2)
	TextureLeaveQuestion();   // 0x54c0d6: FUN_0055c070(1) asks to save; Yes saves, then takes the flagged polygons out (FUN_00563d90); No drops them
	TextureModeStateReset();   // FUN_0054bfd0 frees the rest of the mode (TM-X2)
	ShowTheTexturePolygons();

	switch (textureMapSavedMode)
	{
		case host::DrawMode::Polyline:
			ViewToggles_UseNow(host::ViewToggle::Polyline);
			break;
		case host::DrawMode::FilledPolyline:
			ViewToggles_UseNow(host::ViewToggle::FilledPolyline);
			break;
		case host::DrawMode::WireTextured:
			ViewToggles_UseNow(host::ViewToggle::WireTextured);
			break;
		default:
			break;   // it was Polygon already
	}
}

// The selected polygons drawn in the view: each file polygon is turned into the game's item of the same shape (polygon_items.h: a pair the loader joined into a quad is one item for
// both of its file polygons), and the view draws those items again in yellow. A shape the map is refused for (hostile merge data, a shape the game has not loaded) is not drawn and
// is counted; the selection itself is kept.
size_t textureShapesNotDrawn = 0;

void ShowTheTexturePolygons()
{
	textureShapesNotDrawn = 0;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<LevelViewHighlight> shown;

	if (!open || !open->Level())
	{
		LevelView_SetHighlightedItems(nullptr, 0);
		return;
	}

	std::vector<bool> done(texturePolygons.size(), false);

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		// every selected polygon of this one's shape, worked out together
		TexturePolygon const &lead = texturePolygons[first];
		LevelViewShapeOf shape = {};

		shape.isModule = lead.isModule;
		shape.moduleIndex = lead.moduleIndex;
		memcpy(shape.objectId, lead.id, 8);

		model::ShapeModel model;
		std::string why;
		std::vector<uint8_t> corners;
		std::vector<int32_t> numbers, toItem, firstFile;
		bool hasMerge = false;

		bool const worked = host::ShapeEdit_Read(open->Level(), open, lead.id, &model, nullptr, &why) == host::ShapeEditResult::Done && LevelView_ItemCorners(shape, &corners) &&
			host::PolygonItems_ReadMergeData(open, open->Level(), lead.id, &numbers, &hasMerge) &&
			host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), model.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) == host::PolygonItemResult::Ok;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (done[i] || one.isModule != lead.isModule || one.moduleIndex != lead.moduleIndex || memcmp(one.id, lead.id, 8) != 0)
			{
				continue;
			}

			done[i] = true;

			if (worked && one.polygon >= 0 && (size_t)one.polygon < toItem.size() && shown.size() < kLevelViewMostHighlighted && !TexturePolygonOutlineOff(one))   // (I, 0x105: hidden)
			{
				LevelViewHighlight item;

				item.shape = shape;
				item.item = toItem[(size_t)one.polygon];
				shown.push_back(item);
			}
		}

		if (!worked)
		{
			textureShapesNotDrawn++;
		}
	}

	// Flags > View: the polygons that have the viewed flag (or an animation) are drawn in the highlight too
	textureFlagViewCount = 0;

	if (textureFlagViewMask != 0 || textureViewAnimated)
	{
		TextureForEveryShape(open, [&](uint8_t const *id, bool isModule, int32_t moduleIndex)
		{
			model::ShapeModel model;
			std::string why;
			std::vector<uint8_t> corners;
			std::vector<int32_t> numbers, toItem, firstFile;
			bool hasMerge = false;
			LevelViewShapeOf shape = {};

			shape.isModule = isModule;
			shape.moduleIndex = moduleIndex;
			memcpy(shape.objectId, id, 8);

			if (host::ShapeEdit_Read(open->Level(), open, id, &model, nullptr, &why) != host::ShapeEditResult::Done || !LevelView_ItemCorners(shape, &corners) || !host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge) ||
				host::PolygonItems_Map(hasMerge ? numbers.data() : nullptr, numbers.size(), model.polygons.size(), corners.data(), corners.size(), &toItem, &firstFile) != host::PolygonItemResult::Ok)
			{
				return;
			}

			std::set<int32_t> animated;

			if (textureViewAnimated)
			{
				int32_t number = 0;
				model::TextureAnim anim;

				if (host::AnimEdit_Read(open, id, &number, &anim) == host::AnimEditResult::Done)
				{
					for (model::AnimPolygon const &record : anim.polygons)
					{
						animated.insert(record.polygon);
					}
				}
			}

			for (size_t p = 0; p < model.polygons.size() && p < toItem.size() && shown.size() < kLevelViewMostHighlighted; p++)
			{
				bool const take = textureViewAnimated ? animated.count((int32_t)p) != 0 : (((uint32_t)model.polygons[p].flags & textureFlagViewMask) != 0) || (textureFlagViewMask == 0x80000000u && TexturePolyMarkedForSplit(id, (int32_t)p));   // 0x80000000 is Split Object's mark (kept in the waiting list, not in the file)

				if (take)
				{
					LevelViewHighlight item;

					textureFlagViewCount++;
					item.shape = shape;
					item.item = toItem[p];
					shown.push_back(item);
				}
			}
		});
	}

	LevelView_SetHighlightedItems(shown.data(), shown.size());
}

// HIERARCHY VIEW (hierarchy_view.h): a model shown in front of the camera, and the keys that run it. The game draws a free object only inside a room the camera sees, so the place is the lowest and farthest one
// in front of the camera that is inside a room; AVP_HV_DIST and AVP_HV_DROP (millimetres) choose another, for finding a good one in a proof run.
std::string hierarchyWords;   // kept here so statusWords can point at it

int32_t hierarchyShownPlace[3] = { 0, 0, 0 };   // where the model was last put in the world (mm): the dummy object is drawn there

// WHERE THE MODEL IS DRAWN (inteng, traced: AvP-docs\engine\inteng\reassess\hierarchy-view-trace.md): at the world ORIGIN of the opened file with the identity turn (FUN_0047ef70 takes its base place from DAT_00637068 /
// DAT_0063706c / DAT_00637070, which nothing ever writes, and the matrix from the identity block DAT_005d45d0), the root section at the frame's own offset from it (FUN_00480bf0); a sequence that travels travels.
// The camera is not touched: no camera write is in the open path (FUN_0048b550, FUN_0048bc10) or in the per-frame step (FUN_00478850); it is the level's own camera (File > Open ends with FUN_00470550(10): slot 10, the origin,
// identity, looking +z, unless the file gives one). The earlier "in front of the camera, framed, played in place" was ours and is gone (Melanikus: "behaves EXACTLY").
bool ShowTheModelInFront(char const *model, char const *hierarchy, int sequence, int sub, int32_t place[3], std::string *why, int32_t seconds)
{
	place[0] = 0;
	place[1] = 0;
	place[2] = 0;
	hierarchyShownPlace[0] = 0;
	hierarchyShownPlace[1] = 0;
	hierarchyShownPlace[2] = 0;
	HierarchyView_SetInPlace(false, place);

	return HierarchyView_Show(model, hierarchy, sequence, sub, seconds, place, 0, why);
}

// Hierarchy mode's T, W, C and Y switches (inteng's help keys)
bool hierarchyText = true;
bool hierarchyWire = false;
bool hierarchyCameraRoot = false;
bool hierarchyPlayAll = false;   // Y, "Play commands affect all sequences" (the player's +0x88); Texture Map's + / - read it too
// V / Shift+V (vertex selection): the points of the attached object's shape as the attachment sees them (hierarchy space, mm), drawn at the model's place; clicks take the nearest one on the screen
std::vector<int32_t> hierarchySkin;
std::vector<int32_t> hierarchySkinFaces;   // its polygons (HierarchyEdit_SkinFaces): the vertex click must be on one of them (FUN_00485290, TB-2)
int hierarchyPickViewW = 0, hierarchyPickViewH = 0;   // the size of the view the last Hierarchy click was made in
bool hierarchyBones = false;   // B (command 0x5f, DAT_00637128): the box round the hierarchy's sections is shown (FUN_0047f4a0 makes it, FUN_0047f6fc hides it when off)
int hierarchyDetail = 0;   // U / I: the detail level shown (inteng's view +0x3c, 0 to 9; 0 the full shape): FUN_00477fb0 via the commands 0x41 / 0x42

// The Hierarchy view's words are NOT on the status line: inteng draws them over the view every frame (HierarchyTextLines below, FUN_004889d0 0x48a07a-0x48a96b). The old status words are gone.
void HierarchyStatus()
{
}

// THE HIERARCHY VIEW'S TEXT, as inteng draws it (FUN_004889d0 0x48a07a to 0x48a96b, every frame, FUN_0057b1b0(font, x, y, format, ...)): lines at x 0, y = 16 * row, the row starting at 3 and going down by one per line
// (the first, at 200, doesn't count). The order and the words are the exe's: "(Text off)" when T has turned the text off, then, as each applies, "Play commands affect all sequences" (x 200), "Vertex Selection Mode", Offset / Rel Quat
// of the selected section, "Platform speed %.3f m/s (%.3f)", "Poly Count %d", "Use Camera Root", "Detail level %d", "Detail distance %.3fm", "Current distance %.3fm", the main sequence's name, its flags (FUN_0047dc80:
// Upper / Lower Sequence, No Interpolation, 1/2 Frame Rate), the frame flags and sounds (FUN_00478bb0: "Flag %s at 3,7", "Sound %d at %d"; the names FUN_00478f00), "Key frame :%d" (FUN_004790a0), "Timer :%d" (the
// player's position, 0 to 65535: FUN_0047dbc0 adds the frame time to it while it runs and wraps it), "Seq. Time :%.1f sec" or "... sec  (%.1f)" with its OBASEQTM time, "(Saving Seq. Times)".
// NOT A GAP (Vasquez 11-17, hierarchy-closeout-frost.md TB-5): "Variant %d of %s" (0x48a0e0-0x48a124) is drawn only when FUN_0057aa00(4) says the game kind DAT_00915104 is 4, and AvP's inteng
// only ever sets it to 0 (FUN_0057a850 0x57a918): never in AvP. NAMED GAPS (not built, the lines are missing when they would show): the delta-state lines of the other game's mode; Poly Count shows 0 (the exe's hierarchy code only ever sets it to 0). The selected section's
// Offset / Rel Quat are the constants the exe's own code leaves in them (0 and the identity: FUN_0047ef70 writes them each frame); the order of the delta players' blocks is the game's list order (newest first).

std::vector<HierarchyTextLine> hierarchyTextNow;   // the lines of the frame being drawn (the state line says them as hierarchylines)
std::string screenTextNow;   // every screen's top-left text of the frame being drawn (view-state-table.md H-1, H-2), lines joined by '|' (the state line says it as screentext)
std::string pathLinesNow;    // the Path screen's drawn rows of the frame being drawn (FUN_004f9d10 0x4f9f70), lines joined by '|' (the state line says them as pathlines)
static void PlacementFrame();
static void PlacementFlyStop();
static void AcousticEnter();
static void AcousticLeave();
static void AcousticCommand(int32_t command);
static void AcousticClick(host::PickReport const &used, bool shift);
static void AcousticPointerFrame(int32_t viewWidth, int32_t viewHeight);
void AcousticTextLines(std::vector<HierarchyTextLine> *out);
static void SnapLiveEnd(bool keep);
// Acoustic Mode's state (the functions are after PickedModules)
std::vector<std::array<uint8_t, 8>> acousticPicks;   // the picked modules (inteng's list DAT_008f4af0), in pick order, by their objects' IDs
static int acousticPointerX = -1, acousticPointerY = -1;       // the pointer on the view, kept from the last move (FUN_00559720 picks under it every frame)
static bool acousticPointerOn = false;                          // a module is under the pointer: acousticPointerId
static uint8_t acousticPointerId[8];
void PlacementTextLines(std::vector<HierarchyTextLine> *out);
static void CurrentObjectTextLines(std::function<void(std::string const &)> const &put, bool trackEditing);
static model::AltLocations const *PlacementAltShown(OpenLevelFile *open, uint8_t const *id);


// TRACK EDITING DRAWS THIS (FUN_004d9af0's frame, FUN_0057b1b0 at x 0x10, y 16 x row; the rows count from 2: 0x4dc742-0x4dc79f): "View Distance :%d" (DAT_005fd678 over 500,000), "Num Objects :%d", the current
// object's lines (CurrentObjectTextLines, 0x4dc7be: T-4, 11-17), then the state's
// header ("Track Edit" in state 2, "Track Edit : Edit Section End" in 3, "Playing" in 4), "Section %d/%d" (the current section's number from 0 and the count: DAT_008f4b90 / DAT_008f4b7c, 0x4dcf86-0x4dcfac), "Has Sound"
// or "Has Sound (looped)" when the track has a middle sound (0x4dcfb1-0x4dcffe) and "Using pivot point" when the turn is about the pivot (0x4dd001-0x4dd029). NAMED GAPS: "Visible polygons %d" (inteng draws it with the
// polygon counts switch on), and the text drawn while an object flies (0x4dc6f9-0x4dc73f). Our field panel and status line are not drawn on these screens.
void TrackTextLines(std::vector<HierarchyTextLine> *out)
{
	out->clear();

	// T-4c, FIXED 11-18: in state 5 (Edit Pivot Point) FUN_004d9af0 calls FUN_004f3e30 and returns at 0x4d9b51, before every line below; FUN_004f3e30 (menu-reads-2.txt:9158-9235) draws a
	// cross in each of the three views and no text. So nothing here in that state (ours drew View Distance, Num Objects, the Section and "Using pivot point")
	if (!OnTheTrackScreen() || trackEd.state == host::TrackState::Off || trackEd.state == host::TrackState::PivotEdit)
	{
		return;
	}

	int32_t row = 2;

	auto put = [&](std::string const &text) { out->push_back({ 16, row++, text }); };

	put("View Distance :" + std::to_string((long long)(viewDistance / 500000)));
	put("Num Objects :" + std::to_string((unsigned long long)PlacementObjectCount()));   // the same global as Object Placement's (T-4b)

	// the current object's lines (0x4dc7be), then the state's header: inteng's line counter runs on, so the header comes after them (until 11-17 it was held at row 4)
	CurrentObjectTextLines(put, true);

	if (trackEd.state == host::TrackState::Playing)
	{
		put("Playing");
	}
	else if (trackEd.state == host::TrackState::EndEdit)
	{
		put("Track Edit : Edit Section End");
	}
	else if (trackEd.state == host::TrackState::Loaded)
	{
		put("Track Edit");
	}

	size_t const count = trackEd.working ? trackEd.working->track.sections.size() : 0;

	put("Section " + std::to_string((unsigned long long)trackEd.current) + "/" + std::to_string((unsigned long long)count));

	if (trackEd.working)
	{
		for (host::TrackSound const &sound : trackEd.working->sounds)
		{
			if (sound.index == 0)
			{
				put((sound.flags & 1) ? "Has Sound (looped)" : "Has Sound");
			}
		}
	}

	if (trackTurnAboutPivot)
	{
		put("Using pivot point");
	}
}

// WAYPOINT MODE DRAWS THIS AND NOTHING ELSE (FUN_0057b1b0 at x 0, y = 16 x row, the row counter DAT_0091bed0 starting at 2: AvP-docs/engine/inteng/reassess/waypoint.md finding 1): mode 0 (FUN_00583d00 0x58420f / 0x584239)
// "View Distance :%d" and, while an entry point is flown, "Moving Entry Point"; mode 1 (FUN_00584ee0 0x585562-0x58566f) "Waypoint :%d" (with a current waypoint), "Can Crawl", "Alien Mode" or "Ground Mode",
// "Highlight Alien Link" / "Highlight Deviation Link"; mode 2 (FUN_00585750 0x5862e8) "Centre Edit". Mode 3 draws no text. No status line, no field panel.
void WaypointTextLines(std::vector<HierarchyTextLine> *out)
{
	out->clear();

	if (barScreen >= host::kIntengScreenCount || strcmp(host::kIntengScreens[barScreen].name, "Waypoint") != 0)
	{
		return;
	}

	int32_t row = 2;

	auto put = [&](std::string const &text) { out->push_back({ 0, row++, text }); };

	if (waypointMode.Mode() == 0)
	{
		put("View Distance :" + std::to_string((long long)(viewDistance / 500000)));

		if (entryFlying)
		{
			put("Moving Entry Point");
		}
	}
	else if (waypointMode.Mode() == 1)
	{
		std::vector<host::WaypointRec> const &list = waypointMode.List();
		int32_t const at = waypointMode.CurrentWaypoint();

		if (at >= 0 && (size_t)at < list.size())
		{
			put("Waypoint :" + std::to_string((long long)at));

			if (list[(size_t)at].flags & 1)
			{
				put("Can Crawl");
			}
		}

		put(waypointMode.AlienList() ? "Alien Mode" : "Ground Mode");

		if (waypointMode.HighlightFlag() == host::kLinkAlien)
		{
			put("Highlight Alien Link");
		}

		if (waypointMode.HighlightFlag() == host::kLinkDeviation)
		{
			put("Highlight Deviation Link");
		}
	}
	else if (waypointMode.Mode() == 2)
	{
		std::vector<host::WaypointRec> const &list = waypointMode.List();
		int32_t const at = waypointMode.CurrentWaypoint();

		if (waypointMode.CentreEdit())
		{
			put("Centre Edit");
		}

		// FUN_00585750 0x58631e-0x58637a (W-5): the current box's size on each axis, "%d" (0x5d1268) of (max - min) / 100 of inteng's units: millimetres
		if (at >= 0 && (size_t)at < list.size())
		{
			double const scale = local_scale > 0.0 ? local_scale : 1.0;
			host::WaypointRec const &wp = list[(size_t)at];

			for (int axis = 0; axis < 3; axis++)
			{
				put(std::to_string((long long)(((double)wp.max[axis] - (double)wp.min[axis]) * scale)));
			}
		}
	}
}


// Object Placement's keys (inteng's help, AvP-docs/engine/inteng/inteng-help-keys.md): Backspace deselects, T text on and off, S split screen, D centre the split screen on the selected object, E edit the
// selected object, P duplicate it, V snap it to a vertex, Q set the particle generator's parent (click an object). Not on the Modern WASD layout's movement keys (S, D, F, R there move the camera).
// F (fly), L (collisions), X and B (while flying), R / Shift+R and C (the camera linked to the flying object) and Tab (Strategy) are below too; their rows and what still differs from inteng:
// reassess/object-placement.md section 8a (V-1..V-22). O, < and > and P-about-the-pivot belong to Track Editing.
static const int32_t kClickParentGenerator = 0x4e7;

static bool IsAMoveKey(WPARAM key)
{
	for (size_t m = 0; m < (size_t)EditorMove::Count; m++)
	{
		if (EditorActions_MoveKey(keyLayout, (EditorMove)m) == (uint8_t)key)
		{
			return true;
		}
	}

	return false;
}

// R / Shift+R: "Toggle camera link to object" / "Camera link to object, with camera at centre of object", and C held: "Move camera relative to object (while camera linked)" (inteng's help). The camera
// keeps its place relative to the object (the selected one, else the current special object) as that moves; with Shift+R the camera is AT the object's centre; while C is held the camera moves freely and the
// place it holds relative to the object is taken again. NAMED: how inteng links (a chase of the object's block each frame) is not read; this keeps the offset.
bool PressTheBarItem(char const *words);
static bool Held(EditorMove move);
static FlyCamera cameraDrawn = { 0, 0, 0, 0, 0 };   // the camera as the last frame had it (the link takes its offset from it)
static bool cameraLinked = false;
static int32_t cameraLinkDistance = 0;     // the camera object's +0x7c: mm from the object's centre, back along the orbit's forward (inteng: 2 x the object's radius, or 0 with Shift+R: FUN_004d9af0 0x4dc3d8-0x4dc41e)
static int cameraOrbitPitch = 0;           // the camera object's +0x74 (hold C: numpad 8 / 2), 4096ths
static int cameraOrbitYaw = 0;             // its +0x78 (hold C: numpad 4 / 6)

// THE LINK LET GO (V-17): R again (0x4dc534-0x4dc593) and the fly's stop (FUN_004df090 0x4df11a-0x4df18b): the camera block takes the camera object's place and matrix, which ours already has (UpdateCameraLink
// puts the orbit's place and its angles in the camera each frame), and the orbit's angles are 0 again; R also makes the distance 0 (the stop leaves it: it is set again by the next R). NAMED: the camera keeps the
// orbit's view as its turn, look and roll in 4096ths (LevelView_AnglesOfMatrix), where inteng keeps the matrix itself.
static void PlacementCameraUnlink(bool byR)
{
	cameraLinked = false;
	cameraOrbitPitch = 0;
	cameraOrbitYaw = 0;

	if (byR)
	{
		cameraLinkDistance = 0;
	}

	LevelView_SetViewMatrix(nullptr);
}
// F: "Fly selected object" (FUN_004def60 = slot +0x48 of the current object, DAT_008f4f44): the object takes the camera's place as it moves, and the camera is let go of it when F is pressed again. The object
// keeps the offset from the camera it had when F was pressed; its place is written when F is pressed again (the editor draws no live copy of an ordinary object: a cube marks where it is meanwhile).
static bool flyOn = false;

bool PlacementIsFlying()
{
	return flyOn;
}

static bool flyIsSpecial = false;
static bool flyIsHierarchy = false;   // the flying object is a placed hierarchy (V-6: F flies its block +4; placement_hierarchy_fly.h)
static int flyHierarchy = -1;         // which (its place in placementHierarchies)
static bool linkedWasHierarchy = false;   // LinkedObjectWorldInFile's last answer was a placed hierarchy's
static uint8_t flyId[8];
static int32_t flyWorld[3] = { 0, 0, 0 };
// OP-110 / OP-111: the FLY SPEED. F puts it in the camera speed's place (DAT_005f071c = DAT_005e51b4, 2 at start, 0x4def60-0x4defc5), keeping the camera speed (DAT_008f4e88), so the speed keys change the fly
// speed while flying and the step of the movement keys is the engine's at that speed (OP-112: FUN_0053d7d0(speed x component, the frame time) >> 2, StepAtTheSpeed); the stop (FUN_004df090) remembers it for the
// next flight and gives the camera its speed back
static int flySpeedUnits = 2;
static int flyCameraSpeedKept = 6;
static host::SpecialKind flyKind = host::SpecialKind::Generator;   // the flying special object (the current one when F was pressed)
static size_t flyNumber = 0;
static int flyTurnAcc[3] = { 0, 0, 0 };      // how far the turn keys have turned the flying object since F: turn, look and roll, in 4096ths of a turn
// THE FLOWN OBJECT'S GLIDE (A-5, OP-112): inteng gives the flown object the camera's own mover (FUN_0053db40, strategy slot 2 at 0x4df0b7; the camera gets slot 0), so the object's block +0x168 glides it as
// the camera's glides the camera: Shift+A / Shift+Z add or take 100000 a frame, any plain move key or Space stops it, and it carries the object along its own forward axis by glide x the frame's time (MoveTheFlyingObject).
// Neither the start (FUN_004df090 0x4df0b1-0x4df0fb) nor the stop (0x4df104-0x4df1c7) touches +0x168, so an object let go while it glides glides on when it is flown again. NAMED: inteng keeps it in the object's block,
// which lasts until the level is loaded again (the hub's entry, FUN_0046e700); ours keeps it by the object until Object Placement (with its Track and Acoustic pages) is left, which in inteng goes through the hub.
static int32_t flyGlide = 0;                         // the flying object's
static std::map<std::string, int32_t> flyGlides;     // every object flown since Object Placement was entered, by FlyGlideKey: its glide when it was let go
static double flyGlideCarry[3] = { 0.0, 0.0, 0.0 };  // the part of a millimetre the glide has not moved it yet

// Which object flies, as flyGlides keeps it: a special object by its kind and number, a placed one by its ID
static std::string FlyGlideKey()
{
	char key[40];

	if (flyIsHierarchy)
	{
		snprintf(key, sizeof(key), "hierarchy %d", flyHierarchy);
	}
	else if (flyIsSpecial)
	{
		snprintf(key, sizeof(key), "special %d %zu", (int)flyKind, flyNumber);
	}
	else
	{
		snprintf(key, sizeof(key), "object %02x%02x%02x%02x%02x%02x%02x%02x", flyId[0], flyId[1], flyId[2], flyId[3], flyId[4], flyId[5], flyId[6], flyId[7]);
	}

	return key;
}
// The ordinary object flies AS ITSELF (level_view.h LevelView_FlyBegin: a live copy of its display block, shared with Track Editing's end marker): its turn is the turn it had when F was pressed, turned with the camera
static bool flyReal = false;                 // the live copy is there (false: a cube marks the object)
// SNAP TO VERTEX, LIVE (OP-43): on (DAT_008f4f10); the moving object; its pose saved at the start (DAT_008f4b60.., DAT_008f4bb8..) and the one shown now (file units and the file's turn); the pointer;
// the view's height of the last frame (the help lines are placed from it)
static bool snapLive = false;
static uint8_t snapId[8];
static int32_t snapSavedPlace[3];
static float snapSavedTurn[4];
static int32_t snapNowPlace[3];
static float snapNowTurn[4];
static int snapPointerX = -1, snapPointerY = -1;
static int32_t snapViewHeight = 0;
static bool snapForTrack = false;   // T-3b: the live snap is Track Editing's, on the flying end marker (the marker's pose, not the object's record, is what it keeps)

bool PlacementFlyHoldsTheCopy()
{
	return (flyOn && flyReal) || snapLive;   // (the live snap shows the object as the same live copy)
}

bool TrackMarkerSnapping()
{
	return snapLive && snapForTrack;
}
static float flyStartQuat[4] = { 0, 0, 0, 1 };
static float flyTurnNow[4] = { 0, 0, 0, 1 };
// A-7 (OP-113): the flown object's own matrix (its block's +0x38, the game's ObMat), which the turn keys turn about its own axes each frame as inteng's mover does (MoveTheFlyingObject), and the part of
// a 4096th of each axis' turn not turned yet (the block's +0x1b8 / +0x1bc / +0x1c0, FUN_0053c6c0). The file's turn (flyTurnNow) is taken from the matrix when the object is let go.
static int32_t flyMatrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
static int flyTurnCarry[3] = { 0, 0, 0 };

// Move rel. to poly's mover (OP-42): the current object flies and the camera is not linked to it; its ID (a special object's: all 0)
static bool PlacementFlyIsTheMover(uint8_t id[8])
{
	if (!flyOn || cameraLinked)
	{
		return false;
	}

	if (flyReal)
	{
		memcpy(id, flyId, 8);
	}
	else
	{
		memset(id, 0, 8);
	}

	return true;
}

// The flying object put at a place (the game's millimetres) and turn (the file's four floats): its block, as FUN_005633e0 moves it; the file gets it when the flight stops (PlacementFlyStop)
static void PlacementFlyTakePose(int32_t const world[3], float const turn[4])
{
	if (!flyOn)
	{
		return;
	}

	// the place, a number at a time (as the fly's other copies of it): numbers, never text (Snyk took a memcpy here for an unterminated text)
	flyWorld[0] = world[0];
	flyWorld[1] = world[1];
	flyWorld[2] = world[2];

	if (flyReal)
	{
		memcpy(flyStartQuat, turn, sizeof(flyStartQuat));
		memcpy(flyTurnNow, turn, sizeof(flyTurnNow));
		flyTurnAcc[0] = flyTurnAcc[1] = flyTurnAcc[2] = 0;

		// the flown object's own matrix is its turn's (A-7: the frame draws the copy from flyMatrix, and the stop takes the file's turn from it)
		LevelView_MatrixOfTurn(turn, flyMatrix);
	}
}
// X: "Toggle movement relative to axes" and B: "Toggle bounding box increase" (FUN_004d9af0 0x4dc59f-0x4dc5f1, only while an object flies, DAT_005e78c4): X flips DAT_00914af8 ("Move Rel to Axes" in the flying
// object's text, FUN_004e9b10), read by the engine's key movement of the flying object (0x53e123: its movement keys then go along the world's axes, not the object's own); B widens the flying object's bounds
// by 2000 each way (FUN_004df1e0, and back with FUN_004df2d0, DAT_008f4f08, "Inc Bounding Box"), for its collisions. OURS: the switches and their words only (here the object takes the camera's place and flies
// through the level, as with L: Toggle collisions).
bool flyRelAxes = false;
bool flyIncBox = false;
// F in Track Editing's end edit (inteng flies the END MARKER OBJECT there, the same F as Object Placement's: state 3's frame code 0x4dc850-0x4dc9a1 copies the marker's place into the track's end): the 3D cursor,
// which is the marker here, takes the camera's place plus the offset it had when F was pressed; F again lets it go there; Finish End Pos Edit takes it.
bool trackFly = false;
// (trackFlyOffset is declared with trackFlyBase)
// L: "Toggle collisions" (V-7, OP-86; FUN_004d9af0 0x4dc124-0x4dc1db): a current object (DAT_008f4f3c), Gravity off (0x4dc133), state 0 or 3 (0x4dc14f), L (slot 0x4c): the CURRENT object's block
// gets +0x20c = 7, +0x5c ^= 0x2000 (set: it does NOT collide) and +0x60 |= 0x10000000. A mark of THAT object: "Colliding" (FUN_004df3c0 0x4df4ea flying; FUN_004e9b60 0x4e9bfc, a placed hierarchy) is
// its own. Every start of the mode clears every mark (0x4d847d); each frame with nothing current FUN_004dd2f0 (0x4dd343-0x4dd37c) clears it on the level objects (a rif object +0x154, a level shape,
// not +0x64 bit 0x1000000) that are within the view distance (the camera's distance to the block less its radius +0x100, under DAT_005fd678). Was: one switch for every object (Drake 11-17).
// NAMED: Gravity on clears it on the shapes of kinds 1-5 and 15 (FUN_004e3e30), which needs the objects' kinds (with OP-47's physics); Track Editing's markers and FUN_004f49f0's hidden shapes set
// it on blocks that are not level objects (0x4f03c2, 0x4f4be5); the radius here is half the object's drawn box's diagonal (inteng's +0x100, the shape's radius)
static std::set<std::string> placementNoCollide;   // the objects marked "not colliding": PlacementCurrentKey's keys

// The current object of Object Placement as a key (inteng's DAT_008f4f3c: the special object just made or found, a placed hierarchy, else the selected placed object); empty for none
static std::string PlacementCurrentKey()
{
	if (specialMode.haveCurrent)
	{
		return "special " + std::to_string((int)specialMode.kind) + " " + std::to_string(specialMode.number);
	}

	if (placedHierarchyCurrent >= 0)
	{
		return "hierarchy " + std::to_string(placedHierarchyCurrent);
	}

	uint8_t id[8];

	return SelectedObjectId(id) ? "object " + std::string((char const *)id, 8) : std::string();
}

// "Colliding" for the current object: its block's +0x5c bit 0x2000 is clear
static bool PlacementCurrentCollides()
{
	std::string const key = PlacementCurrentKey();

	return !key.empty() && placementNoCollide.count(key) == 0;
}

// EVERY SCREEN CHANGE, whoever makes it: a bar pick (the frame's screen change, RunAction) or a mode's own Exit or Tab (LeaveTheMode -> ShowTheScreen, which sets barScreen itself and so
// never reached the frame's screen change: the view gap Dietrich found, the lighting look Drake found, 11-17). Called once per change, with barScreen already the new one. A mode's own
// start and leave hooks go here, one line each. (Added 2026-11-17 by Drake: Bishop's ruling, a shared API, editor_shared.h.)
void OnScreenChanged(size_t was, size_t now)
{
	if (was == now)
	{
	return;
	}

	// Object Placement left (its Track Editing and Acoustic pages are its own): every flown object's glide is let go of, as inteng's are when the hub loads the level again (THE FLOWN OBJECT'S GLIDE),
	// and Gravity's places are written and its blocks given back. The Shape Chooser is the mode's own too: inteng comes back from it through its entry with DAT_008f4f38 = 1, which keeps Gravity
	// (0x4d83b3 skips 0x4d83fe) and loads no level, so the blocks stay where they fell (V-24; the chooser's screen runs no Gravity step, PlacementFrame)
	auto const placementFamily = [](size_t screen) {
		return screen < host::kIntengScreenCount && (strcmp(host::kIntengScreens[screen].name, "Object Placement") == 0 || strcmp(host::kIntengScreens[screen].name, "Placement mode 1 (sounds)") == 0 ||
			strcmp(host::kIntengScreens[screen].name, "Track Editing") == 0 || strcmp(host::kIntengScreens[screen].name, "Shape Chooser") == 0);
	};

	if (placementFamily(was) && !placementFamily(now))
	{
		PlacementGravityLeave();
		flyGlides.clear();
	}

	// Object Placement STARTED (V-24, OP-1): inteng's entry FUN_004d68e0, called only by the hub (FUN_004452b0) and by the Shape Chooser's return, sets DAT_008f4f50 = 0 (0x4d83fe)
	// unless it comes back from the chooser (DAT_008f4f38 = 1: 0x4d83b3 skips it; our chooser's return does not come through here); the Misc item 0x58 is ticked by it (0x4d8422-0x4d842c,
	// the panels' loop: editor_panels.cpp). Its own pages (Track Editing, the sounds' page) are no new start.
	auto const placementAny = [](size_t screen) {
		if (screen >= host::kIntengScreenCount)
		{
			return false;
		}

		char const *const name = host::kIntengScreens[screen].name;

		return strncmp(name, "Object Placement", 16) == 0 || strncmp(name, "Placement mode", 14) == 0 || strcmp(name, "Track Editing") == 0 || strcmp(name, "Shape Chooser") == 0;
	};

	if (placementAny(now) && !placementAny(was))
	{
		placementGravity = false;
		placementNoCollide.clear();   // and every block collides again (0x4d847d: +0x5c bit 0x2000 cleared; V-7)
		PlacementLighting_Start();   // the modules unlit, the objects as Lighting says (OP-3)
	}
}


// OBJECT PLACEMENT'S TEXT (reassess/object-placement.md OP-100..OP-105, OP-80, OP-83). inteng draws, every frame, at x 16 (FUN_004d9af0 0x4dc74e-0x4dc832): "View Distance :%d" (0x5c39b0) at y 32,
// "Num Objects :%d" (0x5e8354, the placement list's count) at y 48, then from y 64 the CURRENT object's lines (its +0x4c), then "Visible polygons %d" (0x5e8340) when the camera's target is in a module.
// All of it only while T is on. The current object's lines (FUN_004df3c0 and the classes' own): generator and bad guy "Difficulty (" + E / M / H for each difficulty it appears at + ")"; a bad guy "%s (%s)"
// (type and subtype); a sound "%s i_r %f o_r %f" (name, ranges in metres); a particle generator "Particle Generator", "Type :%s", "Parent :%s" ("None" / the parent's name / "Unknown"), "Sound :%s";
// then the common lines: "Location %d/%d" and "Location group %d" (alternate locations: placed objects here; NAMED: a special object's ALTLOCAT is not read yet), the name (not for a bad guy), while
// flying "Flying object" / "Flying object, camlink", "Move Rel to Axes", "Inc Bounding Box", "Colliding"; "Has Track"; a special object's "In Object %s"; a placed shape's "In Object %s - radius %d"
// (the radius in inteng's units, the game's x 100). NAMED: a placed shape's "Object scale %-5.6f" line is not drawn yet (the outside file's scale is not read here).
static std::vector<host::PolyCountRow> placementRows;
static std::vector<host::SpecialObject> placementSpecials;

// "Num Objects :%d" (DAT_008f4ef4) is inteng's object-record list's size: +1 for each object its mode's start makes (0x4d8a80-0x4d9789), -1 for each one deleted (0x4e4e95), emptied when
// the list is freed (0x4d4488). FUN_004d9af0's frame prints it (0x4dc788) in Object Placement AND in Track Editing: one frame function, one global, and Track Editing builds no list. Ours: the
// placement list (placed objects, specials, placed hierarchies) in both. T-4b, FIXED 11-18: the track screen counted every object of the level, modules too (Invasion 599 for 301,
// kens-co-op 93 for 54).
size_t PlacementObjectCount()
{
	size_t placed = 0;

	for (host::PolyCountRow const &one : placementRows)
	{
		placed += one.placed ? 1 : 0;
	}

	return placed + placementSpecials.size() + placementHierarchies.size();
}

// THE '=' AND '-' KEYS (OP-14, FUN_004d9af0 0x4dc2c3-0x4dc308): with a current object, the keys live and Gravity off (0x4dc1ea), the main keyboard's '=' (slot 0xd87afb) calls the current
// object's +0x14 with 10/7 and '-' (0xd87afd) with 0.7, EVERY FRAME the key is held (the slots are set on key-down and cleared on key-up, never by this code; both held: '=' first). A placed shape
// whose shape has an outside file (SHPEXTFL) is scaled (FUN_004e1340): the file's bytes by ShapeRescale_ByFactor (the points, extents, radius and the SHPEXTFN's rescale), the drawn shape
// at once by LevelView_ScaleObjectShape. Every other kind's +0x14 does nothing, and so does a shape with no outside file. NAMED (shape_rescale_edit.h): a shape other objects use too is
// refused here. The status line says how much the shape has been scaled since the key went down; inteng says nothing. Added 2026-11-17 by Drake.
static double scaleKeysTotal = 1.0;   // the product of the factors since the keys were last both up
static bool scaleKeysDrawn = true;     // every step since then scaled the view's own shape too
static std::string scaleKeysWords;

static void ScaleKeysFrame(bool up, bool down)
{
	host::Selected const current = host::Selection_Get();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!up && !down)
	{
		scaleKeysTotal = 1.0;
		scaleKeysDrawn = true;
		return;
	}

	// a placed shape that is THE current object (a special object current has the +0x14 that does nothing)
	if (specialMode.haveCurrent || current.kind != host::SelectedKind::Object || !open || !LevelObjects_Level())
	{
		return;
	}

	double const factors[2] = { host::kScaleKeyUp, host::kScaleKeyDown };
	bool const held[2] = { up, down };

	for (int k = 0; k < 2; k++)
	{
		if (!held[k])
		{
			continue;
		}

		std::string why;
		unsigned const generation = open->Generation();
		host::ShapeRescaleResult const result = host::ShapeRescale_ByFactor(open, LevelObjects_Level(), current.objectId, factors[k], &why);

		if (open->Generation() != generation)
		{
			LoadTheObjectListAgain();   // V-30b: the SHPCENTR was made by putting the level together again (the first frame of a shape that had none)
		}

		if (result == host::ShapeRescaleResult::NoExternal)
		{
			return;   // inteng: nothing
		}

		if (result != host::ShapeRescaleResult::Done)
		{
			scaleKeysWords = std::string("Scale: ") + host::ShapeRescaleWords(result) + (why.empty() ? std::string() : " (" + why + ")");
			Answer(scaleKeysWords);
			return;
		}

		LevelViewShapeOf drawn = {};

		drawn.isModule = false;
		memcpy(drawn.objectId, current.objectId, 8);
		scaleKeysDrawn = LevelView_ScaleObjectShape(drawn, factors[k]) && scaleKeysDrawn;
		scaleKeysTotal *= factors[k];
	}

	char words[240];

	// the words say whether the view's own shape was scaled too (the game holds a block for the object), else the next screen change shows it
	_snprintf_s(words, sizeof(words), _TRUNCATE, "Scaled: the shape is %.4g times its size when the key went down, %s (kept in the editor's bytes until Save a copy)", scaleKeysTotal,
		scaleKeysDrawn ? "drawn so now" : "drawn so at the next screen change");
	scaleKeysWords = words;
	Answer(scaleKeysWords);
}

// OBJECT PLACEMENT'S FRAME, called every frame (OP-80, OP-4 (5)): on its screen, H / N held change the view distance (the shared DAT_005fd678, `viewDistance`: the key flags are set on key-down and cleared
// on key-up, FUN_0058c010 / 0x58c74b, and 0x4d9f64-0x4d9f81 does not clear them, so it changes every frame a key is held; never below 0); FUN_004dd2f0 then hides every block whose distance from the camera
// less its radius is not below the distance, except the current object's (LevelView_HideBeyond / HideBeyondKeep; the special objects' icons by the same rule in SpecialIconTriangles). Off its screen the
// rule is taken away. NAMED: inteng's Track Editing state 5 frame skips FUN_004dd2f0 (the blocks keep the hiding they had); here that screen shows everything. Acoustic Mode's frame
// (FUN_004f57b0 0x4f5b6a / 0x4f5b6f) runs FUN_004dd2f0 and FUN_004dd3c0 (the hiding, the placed sounds) but not the keys of FUN_004d9af0 (it returns at 0x4d9b69): H / N do nothing there.
static void PlacementFrame()
{
	bool const acoustic = OnAcousticScreen();
	bool const here = acoustic || (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Object Placement") == 0);

	// OP-47: while Gravity is on, the engine moves the chosen objects one frame (inteng's frame runs its engine on its blocks)
	if (here && placementGravity)
	{
		LevelView_GravityStep();
	}

	// OP-40 / OP-111: making ANOTHER object current stops the flight (FUN_004d64a0 calls FUN_004df090(0) for the old one), and so does leaving the screen (Acoustic Mode AC-1, the chooser, Exit). NAMED: Track
	// Editing (inteng's state 3 of this screen) keeps it here as inteng does; its pivot edit (TE-5) stops it there.
	if (flyOn)
	{
		host::Selected const flyingNow = host::Selection_Get();
		// the current object as F takes it (LinkedObjectWorldInFile: a selected placed object first, else the current special object)
		bool const sameObject = flyIsHierarchy ? placedHierarchyCurrent == flyHierarchy
			: flyIsSpecial ? flyingNow.kind != host::SelectedKind::Object && specialMode.haveCurrent && specialMode.kind == flyKind && specialMode.number == flyNumber
			: flyingNow.kind == host::SelectedKind::Object && memcmp(flyingNow.objectId, flyId, 8) == 0;
		bool const onTrack = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Track Editing") == 0;

		if (!sameObject || (!here && !onTrack))
		{
			PlacementFlyStop();
		}
	}

	// PH-1 / PH-14: the current placed hierarchy's player follows the current object
	PlacedHierarchyFrame(here);

	if (!here)
	{
		if (placementHideOn)
		{
			LevelView_HideBeyond(false, 0);
			LevelView_HideBeyondKeep(nullptr);
			placementHideOn = false;
		}

		// the screen left: a live Snap To Vertex ends with the object where it was (Track Editing's snap on its end marker is not this screen's: SnapLiveFrame ends it when the end edit is left)
		if (!snapForTrack)
		{
			SnapLiveEnd(false);
		}

		// the screen left: its sounds stop and are let go (placed_sounds_game.h)
		if (placedSoundsOpen)
		{
			host::PlacedSounds_LetGo();
			host::HierarchySounds_LetGo();
			placedSoundsOpen = nullptr;
		}

		return;
	}

	if (!acoustic && !AMenuIsOpen() && !host::Dialogs_IsOpen() && (keyDown['H'] || keyDown['N']))
	{
		int64_t const next = (int64_t)viewDistance + (keyDown['H'] ? 500000 : 0) - (keyDown['N'] ? 500000 : 0);

		viewDistance = ViewDistanceHeld(next);
	}

	// OP-14: '=' / '-' held scale the current placed shape every frame (Gravity off; Acoustic Mode's frame does not run these keys)
	if (!acoustic && !AMenuIsOpen() && !host::Dialogs_IsOpen() && !PlacementGravityOn())
	{
		ScaleKeysFrame(keyDown[VK_OEM_PLUS], keyDown[VK_OEM_MINUS]);
	}

	host::Selected const selectedNow = host::Selection_Get();

	LevelView_HideBeyondKeep(!specialMode.haveCurrent && selectedNow.kind == host::SelectedKind::Object ? selectedNow.objectId : nullptr);
	LevelView_HideBeyond(true, PlacementHideDistance());
	placementHideOn = true;

	// one current object (FUN_004d64a0): another made current lets go of the placed hierarchy; one past the list's end (the level changed) is none
	if (specialMode.haveCurrent || host::Selection_Get().kind == host::SelectedKind::Object || placedHierarchyCurrent >= (int)placementHierarchies.size())
	{
		placedHierarchyCurrent = -1;
	}

	// OP-40 (FUN_004d64a0 0x4d64f7-0x4d6507): ANOTHER object made current puts the split screen's views round its place (FUN_004d58e0, which does nothing while the split is off); none made
	// current (Backspace) leaves them
	{
		static std::string currentWas;
		std::string now;
		host::Selected const current = host::Selection_Get();

		if (current.kind == host::SelectedKind::Object)
		{
			now.assign((char const *)current.objectId, 8);
			now.insert(0, "o");
		}
		else if (specialMode.haveCurrent)
		{
			now = "s" + std::to_string((int)specialMode.kind) + ":" + std::to_string(specialMode.number);
		}
		else if (placedHierarchyCurrent >= 0)
		{
			now = "h" + std::to_string(placedHierarchyCurrent);
		}

		if (now != currentWas)
		{
			if (!now.empty() && placementSplit)
			{
				SplitCentreOnCurrent();
			}

			currentWas = now;
		}
	}

	// with no current object the level's objects within the view distance collide again (FUN_004dd2f0 0x4dd343-0x4dd37c clears block +0x5c bit 0x2000 on the blocks with a rif object and a level
	// shape: not the special objects' or the hierarchies' records); one beyond it keeps its mark
	if (PlacementCurrentKey().empty() && theCamera)
	{
		double const scale = local_scale > 0.0 ? local_scale : 1.0;

		for (auto mark = placementNoCollide.begin(); mark != placementNoCollide.end(); )
		{
			int32_t place[3] = { 0, 0, 0 }, mins[3] = { 0, 0, 0 }, maxs[3] = { 0, 0, 0 };
			bool const isObject = mark->size() == 15 && mark->compare(0, 7, "object ") == 0;
			uint8_t id[8];

			if (isObject)
			{
				memcpy(id, mark->data() + 7, 8);
			}

			if (!isObject || !LevelObjects_PlaceOf(id, place))
			{
				++mark;
				continue;
			}

			double const d[3] = { place[0] * scale - theCamera->x, place[1] * scale - theCamera->y, place[2] * scale - theCamera->z };
			double radius = 0.0;

			if (LevelView_ObjectBox(id, mins, maxs))
			{
				double const half[3] = { (maxs[0] - mins[0]) / 2.0, (maxs[1] - mins[1]) / 2.0, (maxs[2] - mins[2]) / 2.0 };

				radius = sqrt(half[0] * half[0] + half[1] * half[1] + half[2] * half[2]);
			}

			// FUN_004b31b0 the distance, less the block's radius, under DAT_005fd678 (100 to the millimetre)
			if (sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - radius < viewDistance / 100.0)
			{
				mark = placementNoCollide.erase(mark);
			}
			else
			{
				++mark;
			}
		}
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open && (placementRowsOpen != open || placementRowsFor != open->Generation()))
	{
		placementRows.clear();
		placementSpecials.clear();

		if (LevelObjects_Level())
		{
			host::PolyCounts_Read(LevelObjects_Level(), &placementRows);
		}

		host::SpecialObjects_Read(open, &placementSpecials);
		host::PlacedHierarchies_Read(open, &placementHierarchies);
		placementRowsOpen = open;
		placementRowsFor = open->Generation();
	}

	// THE PLACED SOUNDS (placed_sounds_game.h): made when the screen shows them (FUN_004d68e0 -> FUN_004ecd40) and when the level's objects changed, then FUN_004ed7e0 for each, every frame
	if (open && placementRowsOpen == open && (placedSoundsOpen != open || placedSoundsFor != placementRowsFor))
	{
		host::SoundGame_UseSilence(editorHiddenRun);
		host::SoundGame_UseSoundDirectory(host::TrackLevelSoundDirectory(open));
		host::PlacedSounds_Sync(PlacedSoundsOfLevel(placementSpecials));
		placedSoundsOpen = open;
		placedSoundsFor = placementRowsFor;
	}

	int32_t eye[3];

	if (LevelView_EyePlace(eye))
	{
		host::PlacedSounds_Frame(eye);
	}
}

// a bad guy's "%s (%s)" (0x4e5f30 0x4e6097-0x4e6264): the type's words (1 Alien, 2 Predator, 3 Facehugger, 4 Xenoborg, 5 PredAlien, 6 AlienQueen, 7 by the marine subtype, 8 Praetorian, 9 Sentry Gun, any other "Type %d")
// and the object's name
static std::string SpecialTypeLine(host::SpecialObject const &o)
{
	char words[64];

	switch (o.type)
	{
	case 1: snprintf(words, sizeof(words), "Alien"); break;
	case 2: snprintf(words, sizeof(words), "Predator"); break;
	case 3: snprintf(words, sizeof(words), "Facehugger"); break;
	case 4: snprintf(words, sizeof(words), "Xenoborg"); break;
	case 5: snprintf(words, sizeof(words), "PredAlien"); break;
	case 6: snprintf(words, sizeof(words), "AlienQueen"); break;
	case 8: snprintf(words, sizeof(words), "Praetorian"); break;
	case 9: snprintf(words, sizeof(words), "Sentry Gun"); break;
	case 7:
		switch (o.subType)
		{
		case 0: snprintf(words, sizeof(words), "Marine - Pulse Rifle"); break;
		case 5: snprintf(words, sizeof(words), "Marine - Pistol"); break;
		case 10: snprintf(words, sizeof(words), "Marine - Smartgun"); break;
		case 20: snprintf(words, sizeof(words), "Marine - Grenade Launcher"); break;
		case 50: snprintf(words, sizeof(words), "Marine - FlameThrower"); break;
		case 60: snprintf(words, sizeof(words), "Marine - Sadar"); break;
		case 70: snprintf(words, sizeof(words), "Marine - Minigun"); break;
		case 30: case 31: case 32: snprintf(words, sizeof(words), "Civ - Shotgun"); break;
		case 40: case 41: snprintf(words, sizeof(words), "Civ - Pistol"); break;
		case 100: snprintf(words, sizeof(words), "Civ - Flamer"); break;
		case 80: snprintf(words, sizeof(words), "Civ - Molotov"); break;
		case 110: snprintf(words, sizeof(words), "Civ - Unarmed"); break;
		case 90: case 91: snprintf(words, sizeof(words), "Civ - Scientist"); break;
		case 120: snprintf(words, sizeof(words), "Android"); break;
		default: snprintf(words, sizeof(words), "Marine"); break;
		}
		break;
	default: snprintf(words, sizeof(words), "Type %d", (int)o.type); break;
	}

	return std::string(words) + " (" + o.name + ")";
}

// a particle generator's "Parent :%s" (FUN_004e71f0): "None" for the no-parent ID, else the name of the placed object with that ID, else "Unknown"
static std::string SpecialParentName(host::SpecialObject const &o)
{
	static uint8_t const none[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	if (memcmp(o.parentId, none, 8) == 0)
	{
		return "None";
	}

	for (host::PolyCountRow const &one : placementRows)
	{
		if (one.placed && memcmp(one.id, o.parentId, 8) == 0)
		{
			return one.name;
		}
	}

	return "Unknown";
}

static std::string PlacementDifficulty(uint32_t notBits)
{
	std::string text = "Difficulty (";

	text += (notBits & 0x100000) ? "" : "E";
	text += (notBits & 0x200000) ? "" : "M";
	text += (notBits & 0x400000) ? "" : "H";
	return text + ")";
}

// THE CURRENT OBJECT'S LINES: its class's +0x4c, which FUN_004d9af0 calls at 0x4dc7be on DAT_008f4f3c whenever the frame draws text: in Object Placement AND in Track Editing's
// states 2, 3 and 4 (DAT_008f4f2c; the object whose track is edited stays current, and entering Track Editing never clears it). Traced twice, independently, 11-17 (T-4 of
// reassess/track-editing-closeout-vasquez.md: the close-out had Track Editing draw its own lines only, WRONG). For a placed shape (FUN_004e3600) only "In Object %s - radius %d"
// (and the module's own "Has Track", not drawn here: a NAMED gap of OP-102) wait for state 0. Moved here from PlacementTextLines unchanged but for that one test.
static void CurrentObjectTextLines(std::function<void(std::string const &)> const &put, bool trackEditing)
{
	char line[300];
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::Selected const selected = host::Selection_Get();
	auto const moduleNameAt = [scale](int32_t const file[3], std::string *name, int32_t *radius) -> bool
	{
		int32_t const world[3] = { (int32_t)(file[0] * scale), (int32_t)(file[1] * scale), (int32_t)(file[2] * scale) };
		int32_t const index = LevelView_ModuleAt(world);

		if (index < 0 || !LevelView_ModuleName(index, name))
		{
			return false;
		}

		if (radius && !LevelView_ModuleRadius(index, radius))
		{
			*radius = 0;
		}

		return true;
	};
	auto const flyLines = [&](bool flyingThis)
	{
		if (!flyingThis)
		{
			return;
		}

		put(cameraLinked ? "Flying object, camlink" : "Flying object");

		if (flyRelAxes)
		{
			put("Move Rel to Axes");
		}

		if (flyIncBox)
		{
			put("Inc Bounding Box");
		}

		if (PlacementCurrentCollides())
		{
			put("Colliding");
		}
	};

	if (placedHierarchyCurrent >= 0 && (size_t)placedHierarchyCurrent < placementHierarchies.size())
	{
		flyLines(flyOn && flyIsHierarchy && flyHierarchy == placedHierarchyCurrent);   // V-6

		// FUN_004e9b60 (the class 0x5ae6c4's +0x4c): while it flies the fly lines, "Colliding" when its block collides, then (PH-15) "Sequence %d" (the current
		// record's index, 0x4e9c24) when there is a current record, and "Timer %d" (the main player's +0x10) and "Frame %d" (FUN_004790a0) when the player has a main sequence: no name line and none
		// of FUN_004df3c0's common lines
		if (PlacementCurrentCollides())
		{
			put("Colliding");
		}

		host::PlacedHierarchyInfo const &current = placementHierarchies[(size_t)placedHierarchyCurrent];

		if (placedHierarchyShown == placedHierarchyCurrent && placedHierarchyRecord < current.sequences.size())
		{
			put("Sequence " + std::to_string(current.sequences[placedHierarchyRecord].index));
		}

		if (placedHierarchyShown == placedHierarchyCurrent && HierarchyView_IsShown())
		{
			put("Timer " + std::to_string(HierarchyView_Position()));
			put("Frame " + std::to_string(HierarchyView_KeyFrame()));
		}
	}
	else if (specialMode.haveCurrent)
	{
		for (host::SpecialObject const &o : placementSpecials)
		{
			if (o.kind != specialMode.kind || o.number != specialMode.number)
			{
				continue;
			}

			if (o.kind == host::SpecialKind::Generator)
			{
				put(PlacementDifficulty(o.flags));
			}
			else if (o.kind == host::SpecialKind::BadGuy)
			{
				put(PlacementDifficulty(o.flags));
				put(SpecialTypeLine(o));
			}
			else if (o.kind == host::SpecialKind::Sound)
			{
				snprintf(line, sizeof(line), "%s i_r %f o_r %f", o.name.c_str(), o.inner * scale * 0.001, o.outer * scale * 0.001);
				put(line);
			}
			else if (o.kind == host::SpecialKind::ParticleGenerator)
			{
				static char const *const kTypes[] = { "Intermittent Sparks", "Steam", "Black smoke", "Flame", "Unknown" };

				put("Particle Generator");
				put(std::string("Type :") + kTypes[o.particleType >= 0 && o.particleType <= 3 ? o.particleType : 4]);
				put(std::string("Parent :") + SpecialParentName(o));

				if (o.hasSound)
				{
					put("Sound :" + o.wav);
				}
			}

			for (std::string const &one : SpecialAlt_ShownLines(LevelObjects_OpenForEdit(), (int)o.kind, o.number))   // "Location %d/%d", "Location group %d" (FUN_004df3c0 0x4df3cb-0x4df40e)
			{
				put(one);
			}

			if (o.kind != host::SpecialKind::BadGuy)
			{
				put(o.kind == host::SpecialKind::PlayerStart ? PlayerStartName(o.number) : o.name);   // OP-29: "Player Start %d" (FUN_00422110), not FUN_004216d0's first "Default name"
			}

			flyLines(flyOn && flyIsSpecial);

			std::string module;

			if (moduleNameAt(o.place, &module, nullptr))
			{
				put("In Object " + module);
			}

			break;
		}
	}
	else if (selected.kind == host::SelectedKind::Object && open)
	{
		for (host::PolyCountRow const &one : placementRows)
		{
			if (!one.placed || memcmp(one.id, selected.objectId, host::kSelectionIdBytes) != 0)
			{
				continue;
			}

			host::SimpleObjectDraft draft;

			if (host::SimpleObject_Read(open, LevelObjects_Level(), one.id, &draft) == host::SimpleObjectResult::Done)
			{
				put(PlacementDifficulty(draft.flags));
			}

			// FUN_004df3c0 0x4df3cb-0x4df40e: with 2 or more locations, "Location %d/%d" (the shown one, from 0, and the count), and inside that, "Location group %d" when the group is not 0
			model::AltLocations const *const alternates = PlacementAltShown(open, one.id);

			if (alternates && alternates->list.size() > 1)
			{
				snprintf(line, sizeof(line), "Location %d/%d", (int)alternates->current, (int)alternates->list.size());
				put(line);

				if (alternates->group != 0)
				{
					snprintf(line, sizeof(line), "Location group %d", (int)alternates->group);
					put(line);
				}
			}

			put(one.name);
			flyLines(flyOn && !flyIsSpecial && memcmp(flyId, one.id, 8) == 0);

			if (host::TrackExists(open, one.id))
			{
				put("Has Track");
			}

			// OP-102 (FUN_004e3600 0x4e3783-0x4e39f6), after the common lines (FUN_004df3c0): when the object's OWN shape has an SHPEXTFL holding an SHPEXTFN (the first of each;
			// no level-wide test), "Object scale %-5.6f" (0x5e8bec) = DAT_00607bd8 x 0.01 (0x5ac438) x that SHPEXTFN's rescale, multiplied in that order (0x4e39d4-0x4e39f6).
			// DAT_00607bd8 is 100 x the first REBENVDT's first ENVSDSCL, or 100 with none (FUN_0046e700 / FUN_0046e140): 100 x local_scale here, as Rescale works its factor out.
			double rescale = 0.0;

			if (host::ShapeRescale_Current(open, LevelObjects_Level(), one.id, &rescale))
			{
				snprintf(line, sizeof(line), "Object scale %-5.6f", ((100.0 * (local_scale > 0.0 ? local_scale : 1.0)) * host::kRescaleHundredth) * rescale);
				put(line);
			}

			std::string module;
			int32_t radius = 0;

			// only when no track is edited (FUN_004e3600 0x4e3ba7: DAT_008f4f2c != 0 skips it; the module is still kept in +0x110)
			if (!trackEditing && moduleNameAt(one.place, &module, &radius))
			{
				snprintf(line, sizeof(line), "In Object %s - radius %d", module.c_str(), radius * 100);
				put(line);
			}

			break;
		}
	}
}

void PlacementTextLines(std::vector<HierarchyTextLine> *out)
{
	out->clear();

	// the live Snap To Vertex's words (0x5e8364, three lines) at x 20, y = the window's height - 60 (0x4dc6f9-0x4dc73f), a new line the letters' height (16) down
	if (snapLive && snapViewHeight > 60)
	{
		static char const *const kSnapWords[3] = { "Press LMB to snap to vertex", "Hold shift to align to polygon", "Press escape to cancel (and return to original position)" };

		for (int i = 0; i < 3; i++)
		{
			int32_t const y = snapViewHeight - 60 + 16 * i;
			HierarchyTextLine line = { 20, y / 16, kSnapWords[i] };

			line.pixelY = y;
			out->push_back(line);
		}
	}

	if (!placementText)
	{
		return;
	}

	int32_t row = 2;
	auto put = [&](std::string const &text) { out->push_back({ 16, row++, text }); };
	char line[300];
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	snprintf(line, sizeof(line), "View Distance :%d", viewDistance / 500000);   // 0x4dc746-0x4dc768: DAT_005fd678 / 500000 (the multiply by 0x431bde83, shift 49)
	put(line);
	snprintf(line, sizeof(line), "Num Objects :%d", (int)PlacementObjectCount());
	put(line);

	CurrentObjectTextLines(put, false);

	// the module the camera's target is in (FUN_004d63b0) and its count (FUN_004d5e60, Module Linking's figure)
	if (theCamera)
	{
		int32_t const target[3] = { theCamera->x, theCamera->y, theCamera->z };
		int32_t const index = LevelView_ModuleAt(target);
		std::string name;

		if (index >= 0 && LevelView_ModuleName(index, &name))
		{
			for (host::PolyCountRow const &one : placementRows)
			{
				if (!one.placed && one.name == name)
				{
					snprintf(line, sizeof(line), "Visible polygons %d", one.count);
					put(line);
					break;
				}
			}
		}
	}
}

// The linked object's place in the game's units (mm); false when no object is there to follow
static bool LinkedObjectWorldInFile(int32_t out[3], int32_t *radius, bool *special, uint8_t *idOut)
{
	if (radius)
	{
		*radius = 1000;
	}

	if (special)
	{
		*special = false;
	}

	linkedWasHierarchy = false;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	uint8_t id[8];
	int32_t place[3];

	if (!open || !open->Level())
	{
		return false;
	}

	if (host::Selection_Get().kind == host::SelectedKind::Object && SelectedObjectId(id) && host::ObjectPlace_Get(open, open->Level(), id, place) == host::MoveRelPolyEditResult::Done)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			out[axis] = (int32_t)(place[axis] * scale);
		}

		if (idOut)
		{
			memcpy(idOut, id, 8);
		}

		if (radius)
		{
			model::ShapeModel shape;
			std::string why;

			if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) == host::ShapeEditResult::Done)
			{
				*radius = (int32_t)(shape.radius * scale);
			}
		}

		return true;
	}

	if (specialMode.haveCurrent)
	{
		std::vector<host::SpecialObject> shown;

		if (host::SpecialObjects_Read(open, &shown) == host::SpecialResult::Done)
		{
			for (host::SpecialObject const &o : shown)
			{
				if (o.kind == specialMode.kind && o.number == specialMode.number)
				{
					if (special)
					{
						*special = true;
					}

					for (int axis = 0; axis < 3; axis++)
					{
						out[axis] = (int32_t)(o.place[axis] * scale);
					}

					return true;
				}
			}
		}
	}

	// a current placed hierarchy: its block +4's place (V-6: F flies it, 0x4def60; V-23: the split views go round it, FUN_004d5980 / FUN_004d58e0); R's radius stays the icon's 1000 (NAMED)
	if (placedHierarchyCurrent >= 0 && PlacedHierarchyFly_World(open, (size_t)placedHierarchyCurrent, scale, out))
	{
		linkedWasHierarchy = true;
		return true;
	}

	return false;
}

// While an object flies the camera link follows IT where the keys have taken it (the file still holds where it was)
static bool LinkedObjectWorld(int32_t out[3], int32_t *radius, bool *special, uint8_t *idOut)
{
	if (!LinkedObjectWorldInFile(out, radius, special, idOut))
	{
		return false;
	}

	if (flyOn)
	{
		memcpy(out, flyWorld, sizeof(flyWorld));
	}

	return true;
}

static void UpdateCameraLink(FlyCamera *camera)
{
	int32_t world[3];

	// F in the track's end edit: the marker (the 3D cursor) takes the camera's place plus its offset; it lets go when the end edit is left. While V snaps it (T-3b) it is not carried: inteng puts
	// the flying marker back to its saved pose every frame before the snap (0x4dc9a6-0x4dc9e4)
	if (trackFly && !snapLive)
	{
		if (!OnTheTrackScreen() || trackEd.state != host::TrackState::EndEdit)
		{
			trackFly = false;
			flyRelAxes = false;
			flyIncBox = false;
		}
		else
		{
			// the track's places are the file's units, the camera's the world's (file x scale)
			double const scale = local_scale > 0.0 ? local_scale : 1.0;

			threeCursor[0] = (int32_t)floor(((double)camera->x + (double)trackFlyOffset[0]) / scale + 0.5);
			threeCursor[1] = (int32_t)floor(((double)camera->y + (double)trackFlyOffset[1]) / scale + 0.5);
			threeCursor[2] = (int32_t)floor(((double)camera->z + (double)trackFlyOffset[2]) / scale + 0.5);

			// the marker is turned with the camera: the turn since it was last taken, in degrees (4096 to a turn)
			auto const since = [](int now, int base) { int d = (now - base) & 4095; return (double)(d > 2048 ? d - 4096 : d) * 360.0 / 4096.0; };

			trackEd.markerTurn[0] = since(camera->turn, trackFlyBase[0]);
			trackEd.markerTurn[1] = since(camera->look, trackFlyBase[1]);
			trackEd.markerTurn[2] = since(camera->roll, trackFlyBase[2]);

			// CAM-4 (camera-closeout-drake.md; Vasquez 11-17): with P on (inteng's DAT_005e5188, copied to DAT_00914afc while the marker flies, 0x4dc184 / 0x4dc8b3) the mover turns the marker about
			// its PIVOT, not its centre: the point place + M d stays still (0x53dedc-0x53df73: V = d turned by the old matrix is added to the place, the matrix turned, V turned by the new one
			// taken away), d being minus the section's object offset (FUN_004f0c70 0x4f0c97). Ours carries the marker with the camera (NAMED), so the place carried is swung by
			// (M_now - M_base) x offset: the pivot is carried, the object swings round it, and Finish End finds the pivot where it was
			if (trackTurnAboutPivot && trackEd.working && trackEd.current < trackEd.working->track.sections.size())
			{
				int32_t const *const offset = trackEd.working->track.sections[trackEd.current].offset;
				float base[4], turned[4];
				double before[9], after[9];

				if ((offset[0] || offset[1] || offset[2]) && host::TrackEdit_MarkerBase(trackEd, base) && host::TrackPlay_QuatAxes(base, before) &&
					host::TrackPlay_TurnedBy(base, trackEd.markerTurn[0], trackEd.markerTurn[1], trackEd.markerTurn[2], turned) && host::TrackPlay_QuatAxes(turned, after))
				{
					double const at[3] = { (double)camera->x, (double)camera->y, (double)camera->z };

					for (int axis = 0; axis < 3; axis++)
					{
						double swing = 0.0;

						for (int i = 0; i < 3; i++)
						{
							swing += (double)offset[i] * (after[i * 3 + axis] - before[i * 3 + axis]);   // the local offset in the world: its parts along the rows (the local axes)
						}

						threeCursor[axis] = (int32_t)floor((at[axis] + (double)trackFlyOffset[axis]) / scale + swing + 0.5);
					}
				}
			}
		}
	}

	// F: the flying object is drawn where the keys have taken it and turned as they have turned it (MoveTheFlyingObject: its own matrix, A-7): the camera stays where it is, unless R links it
	if (flyOn && flyReal)
	{
		LevelView_FlyPutMatrix(flyWorld, flyMatrix);
	}

	if (flyOn && flyIsHierarchy)
	{
		HierarchyView_MoveTo(flyWorld);   // the model follows its block (FUN_004e9cb0 -> FUN_0047ef70 lays it out from block +4 each frame)
	}

	if (!cameraLinked)
	{
		LevelView_SetViewMatrix(nullptr);
		return;
	}

	if (!LinkedObjectWorld(world))
	{
		PlacementCameraUnlink(true);
		return;
	}

	// THE ORBIT (V-17): hold C, while it flies and the camera is linked (FUN_004d9af0 0x4dc390-0x4dc49f, every frame C is held; C also stops the object's mover, MoveTheFlyingObject): numpad 8 / 2 turn the
	// orbit's pitch (+0x74) by +50 / -50, numpad 4 / 6 its yaw (+0x78) by -50 / +50, each kept in 0 .. 4095; A / Z bring the camera 50000 of inteng's units (500 mm) nearer / farther (+0x7c), never past the object
	if (keyDown['C'])
	{
		cameraOrbitPitch = (cameraOrbitPitch + (keyDown[VK_NUMPAD8] ? 50 : 0) - (keyDown[VK_NUMPAD2] ? 50 : 0)) & 4095;
		cameraOrbitYaw = (cameraOrbitYaw - (keyDown[VK_NUMPAD4] ? 50 : 0) + (keyDown[VK_NUMPAD6] ? 50 : 0)) & 4095;

		long long distance = (long long)cameraLinkDistance - (keyDown['A'] ? 500 : 0) + (keyDown['Z'] ? 500 : 0);

		cameraLinkDistance = (int32_t)(distance < 1 ? 0 : (distance > kFlyCameraMostPlace ? kFlyCameraMostPlace : distance));
	}

	// the camera object of kind 1 (FUN_00580a40): the object's matrix (the live copy's, as drawn; a special object's is the world's), turned by the yaw and the pitch, and the camera that far back along it
	int32_t blockMatrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
	int32_t place[3], matrix[9];

	if (flyReal)
	{
		LevelView_FlyMatrix(blockMatrix);
	}

	LevelView_OrbitOf(world, blockMatrix, cameraOrbitPitch, cameraOrbitYaw, cameraLinkDistance, place, matrix);
	camera->x = place[0];
	camera->y = place[1];
	camera->z = place[2];
	LevelView_AnglesOfMatrix(matrix, &camera->turn, &camera->look, &camera->roll);
	LevelView_SetViewMatrix(matrix);
}

// X and B while an object flies: the words are inteng's ("Move Rel to Axes", "Inc Bounding Box", FUN_004e9b10)
// OP-117 (FUN_004df1e0 / FUN_004df2d0): B grows the flying object's box (its block's extents, the box the engine collides it by) by 2000 each way, and puts it back; every stop of the flight puts it back.
// NAMED: inteng grows the SHAPE's extents too (+0x38..+0x4c), which only its visibility test reads; ours grows the block's only.
static void FlyBoxSet(bool on)
{
	if (on != flyIncBox && flyReal && !trackFly)
	{
		LevelView_FlyBoxGrow(flyId, on);
	}

	flyIncBox = on;
}

static void FlySwitchKey(bool x)
{
	if (x)
	{
		flyRelAxes = !flyRelAxes;
		Answer(flyRelAxes ? "Move Rel to Axes: on (the movement keys go along the world's axes, not the object's own)" : "Move Rel to Axes: off");
	}
	else
	{
		FlyBoxSet(!flyIncBox);
		Answer(flyIncBox ? "Inc Bounding Box: on (the flying object's bounds are 2000 wider each way, for its collisions)" : "Inc Bounding Box: off");
	}
}

// THE FLY STOP (FUN_004df090(0), OP-111): F again, and every other stop (another object made current, FUN_004d64a0; leaving the screen): the object stays where it flew (written into the level here), the
// camera is let go of it (the link off: the view stays where it is), the fly speed is remembered and the camera speed given back, "Move Rel to Axes" and the box increase off
static void PlacementFlyStop()
{
	if (!flyOn)
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	int32_t place[3] = { (int32_t)(flyWorld[0] / scale), (int32_t)(flyWorld[1] / scale), (int32_t)(flyWorld[2] / scale) };

	FlyBoxSet(false);
	flyOn = false;
	PlacementCameraUnlink(false);
	flyRelAxes = false;
	flySpeedUnits = speedUnits;
	speedUnits = flyCameraSpeedKept;
	flyGlides[FlyGlideKey()] = flyGlide;   // the stop leaves the block's +0x168 as it is (THE FLOWN OBJECT'S GLIDE)
	flyGlide = 0;

	if (!open || !open->Level())
	{
		Answer("Fly selected object: no level");
	}
	else if (flyIsHierarchy)
	{
		Answer(PlacedHierarchyFly_Stop(open, (size_t)flyHierarchy, place));   // its PLACHIDT's place (Vasquez's writer; the turn kept); the list is read again on the level's change
		flyIsHierarchy = false;
	}
	else if (flyIsSpecial)
	{
		// the object that flew, whichever is current now
		host::SpecialMode flying = specialMode;

		flying.haveCurrent = true;
		flying.kind = flyKind;
		flying.number = flyNumber;
		Answer(host::SpecialMode_MoveCurrent(open, &flying, place, true).answer);
	}
	else if (flyReal)
	{
		// the real object: its place AND turn are written (OBJHEAD1), the turn the file's of its own matrix (A-7), then the live copy is taken away and the real block shows where the level's bytes put it
		LevelView_TurnOfMatrix(flyMatrix, flyTurnNow);

		host::TrackEditResult const result = host::TrackSetObjectPose(open, flyId, place, flyTurnNow);

		flyReal = false;
		LevelView_FlyEnd();
		Answer(result == host::TrackEditResult::Done ? "Fly selected object: off, the object is where it flew to" : std::string("Fly selected object: not moved: ") + host::TrackEditWords(result));
		LoadTheObjectListAgain();
	}
	else
	{
		host::MoveRelPolyEditResult const result = host::ObjectPlace_Set(open, open->Level(), flyId, place);

		Answer(result == host::MoveRelPolyEditResult::Done ? "Fly selected object: off, the object is where it flew to" : std::string("Fly selected object: not moved: ") + host::MoveRelPolyEdit_Words(result));
		LoadTheObjectListAgain();
	}
}

// SNAP TO VERTEX, LIVE (reassess/object-placement.md OP-43 and V-10; read 11-17 on dump-full-3 / listing-all-3): FUN_004dd280 starts it with the current object, FUN_004d9af0 0x4da855-0x4da944 runs
// it every frame, FUN_004f7000 snaps under the pointer. The start stops a flight (FUN_004df090(0)) and saves the object's place and turn; every frame the object is put back to them, then
// snapped: with ANOTHER object under the pointer (the moving one left out of the pick), that object's vertex nearest the point under the pointer is T, and the moving object's own vertex
// nearest T lands on it, the object first turned to the hit polygon's normal and squared up when Shift is held (host::SnapToVertex_Pose). A LEFT press keeps what is shown (written into
// the level); Esc puts the saved pose back, nothing written. While it is on, a click picks nothing else (FUN_004d6520 is skipped). Here the object is shown as the fly's live copy.

// the pose drawn: the live copy at the place (file units times the level's scale, as the fly's) and turn
static void SnapLiveShow(int32_t const place[3], float const turn[4])
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	int32_t const world[3] = { (int32_t)floor((double)place[0] * scale + 0.5), (int32_t)floor((double)place[1] * scale + 0.5), (int32_t)floor((double)place[2] * scale + 0.5) };

	LevelView_FlyPut(world, turn);
}

// Esc (keep false: the saved pose stays, nothing written) or the left press (keep true: the pose shown is written, OBJHEAD1's place and turn)
static void TrackMarkerSnapPut(int32_t const place[3], float const turn[4], bool kept);

static void SnapLiveEnd(bool keep)
{
	if (!snapLive)
	{
		return;
	}

	snapLive = false;

	// Track Editing's (T-3b): the marker flies on (its live copy stays); a left press makes the pose shown the marker's: the 3D cursor its place, its turn the one the keys turn from, the
	// camera's carry counted from here (inteng: DAT_008f4f10 = 0 with the flying marker where the snap put it, 0x4dca24); Escape: the marker as it was (0x4dca33)
	if (snapForTrack)
	{
		snapForTrack = false;

		// kept: the pose shown; Escape: the pose saved at V (the camera may have flown meanwhile, so the carry is counted again from it)
		TrackMarkerSnapPut(keep ? snapNowPlace : snapSavedPlace, keep ? snapNowTurn : snapSavedTurn, keep);
		return;
	}

	LevelView_FlyEnd();

	if (!keep || (memcmp(snapNowPlace, snapSavedPlace, sizeof(snapNowPlace)) == 0 && memcmp(snapNowTurn, snapSavedTurn, sizeof(snapNowTurn)) == 0))
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::TrackEditResult const result = open ? host::TrackSetObjectPose(open, snapId, snapNowPlace, snapNowTurn) : host::TrackEditResult::NothingOpen;

	if (result != host::TrackEditResult::Done)
	{
		Answer(std::string("Snap To Vertex: not moved: ") + host::TrackEditWords(result));
		return;
	}

	LoadTheObjectListAgain();
}

// The start (FUN_004dd280) for a placed object: false when the current object is none of those (a special object keeps the click's snap)
static bool SnapLiveStart()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];

	if (!open || !open->Level() || host::Selection_Get().kind != host::SelectedKind::Object || !SelectedObjectId(id))
	{
		return false;
	}

	SnapLiveEnd(false);

	if (flyOn)
	{
		PlacementFlyStop();
	}

	if (host::ObjectPose_Get(open, open->Level(), id, snapSavedPlace, snapSavedTurn) != host::MoveRelPolyEditResult::Done || !LevelView_FlyBegin(id))
	{
		Answer("Snap To Vertex: the object's place and turn couldn't be read");
		return true;
	}

	memcpy(snapId, id, sizeof(snapId));
	memcpy(snapNowPlace, snapSavedPlace, sizeof(snapNowPlace));
	memcpy(snapNowTurn, snapSavedTurn, sizeof(snapNowTurn));
	snapLive = true;
	SnapLiveShow(snapNowPlace, snapNowTurn);
	return true;
}

// V IN TRACK EDITING'S END EDIT (T-3b of reassess/track-editing-closeout-vasquez.md; Vasquez 11-17): inteng's state 3 sends V (slot 0xd87a96, 0x4dc8f6-0x4dc902) to FUN_004dd280 as Object
// Placement does, on the FLYING object, which in the end edit is the end marker (F flies it): its place and matrix are saved (DAT_008f4b60.., DAT_008f4bb8..), and every frame it is put back to them
// and snapped under the pointer (0x4dc995-0x4dca28, FUN_004f7000); a left press keeps the snap, Escape (0x4dca33) puts the saved pose back. With the marker not flying, V does nothing.
// Ours: Object Placement's live snap (SnapLiveFrame, SnapToVertex_Pose) on the marker's live copy, from the marker's pose (the 3D cursor, its turn); what is kept is the marker's pose.
static bool TrackSnapLiveStart()
{
	if (!OnTheTrackScreen() || trackEd.state != host::TrackState::EndEdit || !trackFly || !LevelView_FlyActive() || !trackEd.working)
	{
		return false;
	}

	float base[4], turned[4];

	if (!host::TrackEdit_MarkerBase(trackEd, base) || !host::TrackPlay_TurnedBy(base, trackEd.markerTurn[0], trackEd.markerTurn[1], trackEd.markerTurn[2], turned))
	{
		return false;
	}

	SnapLiveEnd(false);
	memcpy(snapId, trackEd.id, sizeof(snapId));
	memcpy(snapSavedPlace, threeCursor, sizeof(snapSavedPlace));
	memcpy(snapSavedTurn, turned, sizeof(snapSavedTurn));
	memcpy(snapNowPlace, snapSavedPlace, sizeof(snapNowPlace));
	memcpy(snapNowTurn, snapSavedTurn, sizeof(snapNowTurn));
	// the pointer where it is now (V is a key: no mouse move need come before the snap's first frame)
	POINT pointer = { -1, -1 };

	RECT inside = { 0, 0, 0, 0 };

	if (GetCursorPos(&pointer) && ScreenToClient(hWndMain, &pointer) && GetClientRect(hWndMain, &inside) && PtInRect(&inside, pointer))   // (over the view only)
	{
		snapPointerX = pointer.x;
		snapPointerY = pointer.y;
	}

	snapLive = true;
	snapForTrack = true;
	SnapLiveShow(snapNowPlace, snapNowTurn);
	return true;
}

// The track's snap over: the marker at this pose (kept: where the snap put it; Escape: where it was at V), its turn the one the keys turn from, the camera's carry counted from here
static void TrackMarkerSnapPut(int32_t const place[3], float const turn[4], bool kept)
{
	double const scale = local_scale > 0.0 ? local_scale : 1.0;

	memcpy(threeCursor, place, sizeof(threeCursor));

	for (int i = 0; i < 4; i++)
	{
		trackEd.markerBase[i] = turn[i];
	}

	trackEd.markerBaseSet = true;
	trackEd.markerBaseSection = trackEd.current;
	trackEd.markerTurn[0] = trackEd.markerTurn[1] = trackEd.markerTurn[2] = 0.0;

	if (theCamera)
	{
		trackFlyBase[0] = theCamera->turn;
		trackFlyBase[1] = theCamera->look;
		trackFlyBase[2] = theCamera->roll;
		trackFlyOffset[0] = (int32_t)floor((double)threeCursor[0] * scale + 0.5) - theCamera->x;
		trackFlyOffset[1] = (int32_t)floor((double)threeCursor[1] * scale + 0.5) - theCamera->y;
		trackFlyOffset[2] = (int32_t)floor((double)threeCursor[2] * scale + 0.5) - theCamera->z;
	}

	trackDirty = true;
	statusWords = kept ? "Snap To Vertex: the end marker is on the vertex (Finish End Pos Edit takes it)" : "Snap To Vertex: cancelled, the end marker is where it was";
}

// Every frame while it is on, after the frame was drawn (its list of what is on screen is the pick's): the saved pose, then the snap under the pointer
static void SnapLiveFrame(int32_t viewWidth, int32_t viewHeight)
{
	if (!snapLive)
	{
		return;
	}

	// Track Editing's snap ends, the marker as it was, when the end edit or the marker's flight is over (Finish End, Stop, F, Shift+T, another screen)
	if (snapForTrack && (!OnTheTrackScreen() || trackEd.state != host::TrackState::EndEdit || !trackFly))
	{
		SnapLiveEnd(false);
		return;
	}

	snapViewHeight = viewHeight;
	memcpy(snapNowPlace, snapSavedPlace, sizeof(snapNowPlace));   // 0x4da865-0x4da89b: back to the saved place and matrix first
	memcpy(snapNowTurn, snapSavedTurn, sizeof(snapNowTurn));

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open && open->Level() && snapPointerX >= 0 && snapPointerY >= 0)
	{
		host::PickReport const pick = PickGame_AtClickLeavingOut(snapPointerX, snapPointerY, viewWidth, viewHeight, LevelView_FlyCopyBlock());
		uint8_t target[8];
		bool found = false;

		if (pick.hasHit && pick.found == host::PickFound::Object)
		{
			memcpy(target, pick.objectId, sizeof(target));
			found = true;
		}
		else if (pick.hasHit && pick.found == host::PickFound::Module)
		{
			found = ModuleObject(open, pick.moduleIndex, target);
		}

		// another object under the pointer, and the point on it (FUN_00473680); nothing hit: nothing moves (0x4f72be)
		if (found && memcmp(target, snapId, sizeof(target)) != 0)
		{
			double const scale = local_scale > 0.0 ? local_scale : 1.0;
			double const point[3] = { (double)pick.hitPoint[0] / scale, (double)pick.hitPoint[1] / scale, (double)pick.hitPoint[2] / scale };
			bool const align = keyDown[VK_SHIFT] || (GetKeyState(VK_SHIFT) & 0x8000) != 0;   // 0x4da8a4: DAT_00d87a5e, the Shift slot
			int32_t place[3];
			float turn[4];

			if (host::SnapToVertex_Pose(open, open->Level(), snapId, target, point, pick.polygonNumber, align, place, turn, snapForTrack ? snapSavedPlace : nullptr, snapForTrack ? snapSavedTurn : nullptr) == host::MoveRelPolyEditResult::Done)
			{
				memcpy(snapNowPlace, place, sizeof(snapNowPlace));
				memcpy(snapNowTurn, turn, sizeof(snapNowTurn));
			}
		}
	}

	SnapLiveShow(snapNowPlace, snapNowTurn);
}

static bool PlacementKey(WPARAM key, bool shift)
{
	if (barScreen >= host::kIntengScreenCount || KeyHeldNow(VK_CONTROL))
	{
		return false;
	}

	char const *const screen = host::kIntengScreens[barScreen].name;

	if (strcmp(screen, "Object Placement") != 0 && strcmp(screen, "Placement mode 1 (sounds)") != 0)
	{
		return false;
	}

	bool const sounds = strcmp(screen, "Object Placement") != 0;

	// OP-91: ',' and '.' (slots 0xd87afc / 0xd87afe) step the current object's alternate locations: Prev and Next (FUN_004df990 / FUN_004df8c0, OP-37), from the object's +0x34 (FUN_004e01e0),
	// which runs each frame in mode 0 with Gravity off (0x4dc144). Ours: for a placed object (our Alt Locations' own reach); no current object: nothing, the key goes on
	if ((key == VK_OEM_COMMA || key == VK_OEM_PERIOD) && !sounds && trackEd.state == host::TrackState::Off && !PlacementGravityOn())
	{
		uint8_t id[8];

		if (!SelectedObjectId(id) && !specialMode.haveCurrent)   // a current special object's too (OP-91: +0x34 of every class but the player start and the hierarchy)
		{
			return false;
		}

		AltLocationCommand(key == VK_OEM_PERIOD ? 0x67 : 0x68);
		return true;
	}

	if (key == VK_BACK)
	{
		host::Selection_Clear();
		specialMode.haveCurrent = false;
		placedHierarchyCurrent = -1;
		// OP-82 (V-3): FUN_004d64a0(0) leaves the split screen's views where they are
		Answer("Deselected");
		return true;
	}

	if (key == 'R' && !IsAMoveKey(key))
	{
		// OP-114 (V-16): R and Shift+R are fly keys: FUN_004d9af0 0x4dc37f-0x4dc385 skips them unless an object flies (DAT_008f4f44); not flying, nothing
		if (!flyOn)
		{
			return true;
		}

		if (cameraLinked)
		{
			PlacementCameraUnlink(true);
			Answer("Camera link to object: off");
		}
		else
		{
			int32_t world[3];
			int32_t radius = 1000;

			if (!LinkedObjectWorld(world, &radius))
			{
				Answer("Camera link to object: select an object first");
			}
			else
			{
				cameraLinked = true;
				cameraLinkDistance = shift ? 0 : radius * 2;
				Answer(shift ? "Camera link to object: on, the camera at the centre of the object" :
					"Camera link to object: on, 2 radii back (the camera turns with the object; hold C: numpad 8 / 2 and 4 / 6 swing it round, A / Z bring it nearer or farther)");
			}
		}

		return true;
	}

	if (key == 'F' && !shift && !IsAMoveKey(key))
	{
		// OP-85 (V-6): F needs Gravity off (0x4dc13a); with it on, nothing
		if (PlacementGravityOn())
		{
			return true;
		}

		if (flyOn)
		{
			PlacementFlyStop();
		}
		else
		{
			int32_t world[3];
			bool special = false;

			if (!LinkedObjectWorld(world, nullptr, &special, flyId))
			{
				Answer("Fly selected object: select an object first");
			}
			else
			{
				flyOn = true;
				flyIsSpecial = special;
				flyIsHierarchy = linkedWasHierarchy;   // a placed hierarchy (V-6): no live copy of a placed object (flyId is not its)
				flyHierarchy = placedHierarchyCurrent;
				flyKind = specialMode.kind;
				flyNumber = specialMode.number;
				flyCameraSpeedKept = speedUnits;   // OP-110: DAT_008f4e88 = DAT_005f071c; DAT_005f071c = DAT_005e51b4; the link off (DAT_005e78bc = 0)
				speedUnits = flySpeedUnits;
				cameraLinked = false;
				flyTurnAcc[0] = flyTurnAcc[1] = flyTurnAcc[2] = 0;
				flyRelAxes = false;
				flyIncBox = false;
				flyWorld[0] = world[0];
				flyWorld[1] = world[1];
				flyWorld[2] = world[2];

				// its glide is the one it had when it was last let go (the block's +0x168: the start leaves it as it is)
				auto const glided = flyGlides.find(FlyGlideKey());

				flyGlide = glided == flyGlides.end() ? 0 : glided->second;
				flyGlideCarry[0] = flyGlideCarry[1] = flyGlideCarry[2] = 0.0;
				flyReal = false;

				if (!special && !flyIsHierarchy)
				{
					OpenLevelFile *const flyOpen = LevelObjects_OpenForEdit();
					int32_t startPlace[3];

					if (flyOpen && host::TrackObjectPlace(flyOpen, flyId, startPlace, flyStartQuat) && LevelView_FlyBegin(flyId))
					{
						flyReal = true;
						memcpy(flyTurnNow, flyStartQuat, sizeof(flyTurnNow));

						// its own matrix, as the game holds it (the live copy's ObMat is the object's: LevelView_FlyBegin), and no part of a turn carried yet (A-7)
						LevelView_FlyMatrix(flyMatrix);
						flyTurnCarry[0] = flyTurnCarry[1] = flyTurnCarry[2] = 0;
					}
				}

				Answer("Fly selected object: on (the movement keys move the object, the camera stays; R links the camera to it; F again lets it go there)");
			}
		}

		return true;
	}

	if ((key == 'X' || key == 'B') && !shift && !IsAMoveKey(key) && flyOn)
	{
		FlySwitchKey(key == 'X');
		return true;
	}

	if (key == 'L' && !shift && !IsAMoveKey(key))
	{
		// OP-86 (V-7): L needs Gravity off (0x4dc13a), state 0 or 3 (0x4dc14f: no track loaded, or its end being edited) and a current object (0x4dc124); then THAT object's mark flips
		std::string const key = PlacementCurrentKey();

		if (PlacementGravityOn() || !(trackEd.state == host::TrackState::Off || trackEd.state == host::TrackState::EndEdit) || key.empty())
		{
			return true;
		}

		if (placementNoCollide.count(key) != 0)
		{
			placementNoCollide.erase(key);
		}
		else
		{
			placementNoCollide.insert(key);
		}

		Answer(PlacementCurrentCollides() ? "Collisions: on for the current object" : "Collisions: off for the current object");
		return true;
	}

	if (key == VK_TAB)
	{
		// Tab: Enter Strategy Mode (inteng's help): the Construction hub's Strategy Data item
		ShowTheScreen(host::kIntengHub);

		bool const entered = PressTheBarItem("Strategy Data: Normal");

		// entered from Object Placement (DAT_0091493c): Tab and Exit there go back to Object Placement, not to the hub (FUN_00524e90 0x524eca)
		strategyFromPlacement = entered;
		Answer(entered ? "Strategy mode" : "Strategy mode: the item is not on the bar");
		return true;
	}

	// Shift+T: "Updating track positions" (FUN_004efd50, OP-83): T without Shift turns the text off and on (below)
	if (shift && key == 'T')
	{
		UpdateTrackPositions();
		return true;
	}

	if (shift || IsAMoveKey(key))
	{
		return false;
	}

	switch (key)
	{
	case 'T':
		placementText = !placementText;
		Answer(placementText ? "Text: on" : "Text: off");
		return true;

	case 'S':
		SplitScreenCommand();
		return true;

	case 'D':
		// OP-46 (FUN_004d9af0 0x4da131-0x4da17e): FUN_004d58e0 with the current object's place, else the VIEW's; it does nothing while the split screen is off (DAT_008f4f60 == 0)
		if (!placementSplit)
		{
			Answer("Centre view on object: only in the split screen mode (S)");
		}
		else
		{
			SplitCentreOnCurrent();
			Answer("The split screen is centred on the current object (on the view when there is none)");
		}

		return true;

	case 'E':
		SpecialRunCommand(0x18, sounds);
		return true;

	case 'P':
		if (!sounds)
		{
			PlacementDuplicate();
			return true;
		}

		return false;

	case 'V':
		SnapToVertexStart();
		return true;

	case 'Q':
		// OP-90 (V-11): Q is read inside the current object's per-frame +0x30, which runs only with Gravity off (0x4dc13a); with it on, nothing
		if (PlacementGravityOn())
		{
			return true;
		}

		if (specialMode.haveCurrent && specialMode.kind == host::SpecialKind::ParticleGenerator)
		{
			// OP-90 (V-11): FUN_004e7100 waits for a press on ANY block (FUN_00559720), a module too; a press on nothing keeps it waiting, a right press ends it with nothing changed
			bool const waiting = host::ClickMode_Start(kClickParentGenerator, host::ClickWants::Polygon, "Click on parent for generator");

			parentWaiting = waiting;   // the right button ends it (WM_RBUTTONUP, FUN_004e7100 0x4e7184)
			Answer(waiting ? "Click on parent for generator (right button: cancel)" : "That command can't wait for a click");
		}
		else
		{
			Answer("Set particle generator parent: select a particle generator first");
		}

		return true;

	default:
		return false;
	}
}

static void ParentClicked(host::PickReport const &report)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t target[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // the null ID (DAT_00914930/34): "None"

	parentWaiting = false;

	if (!open || !open->Level())
	{
		Answer("Click on parent for generator: no level");
		return;
	}

	// OP-90 (V-11), FUN_004e7100 0x4e7184-0x4e71d7: a block whose rif object (+0x154) is a PLACED object (its definition's 0x400, FUN_004d0fc0; poly_counts' `placed`) becomes the parent
	// (+0x60/+0x64 = its ID +0x9c/+0xa0); the generator's own block, a module, or any other block makes the parent the null ID
	if (report.found == host::PickFound::Object)
	{
		for (host::PolyCountRow const &one : placementRows)
		{
			if (one.placed && memcmp(one.id, report.objectId, 8) == 0)
			{
				memcpy(target, report.objectId, 8);
				break;
			}
		}
	}

	Answer(host::SpecialMode_SetParent(open, &specialMode, target).answer);
}

// Space plays / stops, comma and period step one keyframe back / forward, Home and End take the previous / next sequence of the hierarchy (the keys are ours). True when the key was one of these and a model is shown.
static bool HierarchyKey(WPARAM key)
{
	if (onHierarchy && HierarchyModeKey(key))
	{
		return true;
	}

	if (!HierarchyView_IsShown())
	{
		return false;
	}

	bool const shift = ShiftDown();
	bool const inMode = onHierarchy && hierarchyModelOpen;
	DWORD const answerWas = hierarchyAnswerUntil;   // a key whose command gave an answer (the time keys, the sound keys) keeps those words; the position words are for the other keys

	// (Space was ours: inteng's keys are P to start and O to stop; Shift+P / Shift+O, the delta's, are HierarchyModeKey's)
	if (key == 'P' || key == 'O')
	{
		HierarchyView_Play(key == 'P', true, hierarchyPlayAll);   // the main only unless Y's switch is on (FUN_0047b8b0 0x47b8c3)
	}
	else if (key == 'N' || key == 'M')
	{
		// N (0x3b, FUN_00479290) / M (0x3a, FUN_00479110): the main to its previous / next key frame; with Y on the turret and the deltas by the same time (HP-78)
		HierarchyView_StepFrame(key == 'M' ? 1 : -1, hierarchyPlayAll);
	}
	else if ((key == VK_OEM_PLUS || key == VK_OEM_MINUS) && shift)
	{
		// Shift + and Shift -: the delta sequence's time (commands 0xf / 0x10), as + and - do the main one's
		if (!inMode)
		{
			Answer("Delta time: open a model with Select Hierarchy first");
			return true;
		}

		HierarchyRunCommand(key == VK_OEM_PLUS ? 0xf : 0x10);
	}
	else if (key == VK_OEM_PLUS || key == VK_OEM_MINUS)
	{
		if (inMode)
		{
			HierarchyRunCommand(key == VK_OEM_PLUS ? 0xd : 0xe);
			HierarchyView_ChangeTime(hierarchyMode.sequenceMs - HierarchyView_SequenceMilliseconds());
		}
		else
		{
			HierarchyView_ChangeTime(key == VK_OEM_PLUS ? 100 : -100);
		}
	}
	else if (key == VK_OEM_COMMA || key == VK_OEM_PERIOD)
	{
		// , and . are HELD keys in inteng (key slots 0xd87afc / 0xd87afe read each frame, FUN_004889d0 0x489f9d-0x48a030): the frame loop scrubs (HierarchyScrubEveryFrame); a press does nothing more
	}
	else if (key == 'K' || key == 'L')
	{
		// inteng's K / L: the key translator FUN_0048cde0 gives K 0x64 and L 0x65 (the exe's key table 0x48d16c -> 0x48d0e4, Drake); 0x64 is FUN_0047b800, the player's +0x80 += 10 kept to
		// 0..359, and 0x65 FUN_0047b820, += 350 (10 the other way). So K adds 10 and L takes 10 away (reassess/hierarchy-deltas-vasquez.md; it was the other way round here)
		HierarchyView_TurnTurret(key == 'K' ? 10 : -10);
		hierarchyWords = "Hierarchy: turret elevation " + std::to_string(HierarchyView_Elevation()) + " degrees" + (HierarchyView_HasTurret() ? "" : " (this hierarchy has no turret section: nothing turns)");
		statusWords = hierarchyWords.c_str();
		return true;
	}
	else if (key == 'R')
	{
		HierarchyView_RestartAll();   // inteng's R: every sequence's position back to 0
	}
	else if (key == 'Y')
	{
		// Y (FUN_0048cde0 -> 0x67, case 0x67 0x489d30): the player's +0x88 flips, "Play commands affect all sequences": P / O (FUN_0047b8b0 0x47b8c3) and , / . (FUN_0047b9c0) then act on the turret
		// and the deltas as well; the text row says so at x 200 (0x48a088), no other words (hierarchy-play-frost.md HP-4)
		hierarchyPlayAll = !hierarchyPlayAll;

		if (HierarchyView_ToggleTurretFollows() != hierarchyPlayAll)
		{
			HierarchyView_ToggleTurretFollows();
		}

		return true;
	}
	else if (key == 'T' && shift)
	{
		// Add 'turret delta' (0x7, Shift+T: the key translator FUN_0048cde0 0x48ce58; AvP's menus do not carry it): FUN_0048c070, in its order, with inteng's MESSAGE BOXES (FUN_00474030 +
		// FUN_0057c800(100, 100)): no hierarchy: "Please select hierarchy first"; no turret section (player +0x21, "dum turret"): "This hierarchy hasn't got a turret section"; then the main sequence
		// and the sequences (case 0x07 of hierarchy_mode); then "Select Turret Sequence" and the chosen sequence plays on the sections under the turret (reassess/hierarchy-deltas-vasquez.md HD-18)
		if (!inMode || hierarchyMode.hierarchy.empty())
		{
			DialogMessage("Please select hierarchy first");
			return true;
		}

		if (!HierarchyView_HasTurret())
		{
			DialogMessage("This hierarchy hasn't got a turret section");
			return true;
		}

		HierarchyRunCommand(0x07);
		promptSwallow = promptFor != kPromptNone ? 'g' : 0;   // the key's own letter (G, or T with Shift) arrives as a character next
		if (promptSwallow && key == 'T')
		{
			promptSwallow = 't';
		}

		return true;
	}
	else if (inMode && ((key >= '0' && key <= '9') || key == 'Q'))
	{
		hierarchyMode.position = HierarchyView_Position();
		HierarchyRunCommand(key == 'Q' ? (shift ? 0x51 : 0x50) : (shift ? 0x26 : 0x15) + (int32_t)(key - '0'));
	}
	else if (inMode && !shift && (key == 'D' || key == 'F'))
	{
		// D / F: the previous / next sequence (FUN_0048cde0: D -> 0x40, F -> 0x3f; reassess/hierarchy-play-frost.md HP-11 / HP-12), on the Hierarchy screen. DECIDED PH-14 (Drake 11-18): in Object
		// Placement a placed hierarchy's frame (FUN_004e9cb0) reads only ',' and '.', so there D re-centres the split screen (0x4da131) and F flies it (0x4dc14f, V-6): the keys go on to the mode
		HierarchyStepSequence(key == 'F' ? 1 : -1);
	}
	else if (key == VK_HOME || key == VK_END)
	{
		if (inMode)
		{
			std::vector<host::SequenceInfo> sequences;

			if (host::HierarchyEdit_Sequences(&hierarchyModelFile, hierarchyMode.hierarchy, &sequences) == host::HierarchyEditResult::Done && !sequences.empty())
			{
				size_t const now = hierarchyMode.sequenceSelected && hierarchyMode.sequence < sequences.size() ? hierarchyMode.sequence : 0;

				hierarchyMode.sequence = (now + (key == VK_END ? 1 : sequences.size() - 1)) % sequences.size();
				hierarchyMode.sequenceSelected = true;
			}
		}
		else
		{
			std::vector<HierarchyViewSequence> sequences;
			int sequence = 0, sub = 0;

			if (HierarchyView_Sequences(&sequences) && !sequences.empty() && HierarchyView_CurrentSequence(&sequence, &sub))
			{
				size_t at = 0;

				for (size_t i = 0; i < sequences.size(); i++)
				{
					at = sequences[i].sequence == sequence && sequences[i].sub == sub ? i : at;
				}

				at = (at + (key == VK_END ? 1 : sequences.size() - 1)) % sequences.size();
				HierarchyView_SetSequence(sequences[at].sequence, sequences[at].sub);
			}
		}
	}
	else
	{
		return false;
	}

	HierarchySyncView();

	if (hierarchyAnswerUntil == answerWas)
	{
		HierarchyStatus();
	}

	return true;
}
// (TEXTURE MAPPING: in texture_map_screen.cpp)

void RunAction(int32_t id)
{
	EditorHostState host = {};

	host.running = running;
	host.layout = keyLayout;
	host.screen = barScreen;
	host.prompt = kPromptNone;

	// The editor's own items and inteng's, through the bar's one door (editor_bar.h)
	if (!EditorBar_Run(id, &host))
	{
		return;
	}

	// An Options switch (editor_options.h): flipped, kept in the settings file, said on the status line; its tick is drawn from the state every frame
	if (host.optionToggle != 0)
	{
		bool *const slot = host.optionToggle == 1 ? &editorOptions.selectByPolygons : host.optionToggle == 2 ? &editorOptions.preferBmp : &editorOptions.polygonViewDistance;
		char const *const key = host.optionToggle == 1 ? "SelectByPolygons" : host.optionToggle == 2 ? "PreferBmp" : "PolygonViewDistance";
		static std::string optionWords;

		*slot = !*slot;
		optionWords = host.optionToggle == 1 ? (*slot ? "Select by polygons: on (a click takes the object under the pointer)" : "Select by polygons: off (a click takes the object whose centre is nearest the pointer)")
			: host.optionToggle == 2 ? (*slot ? "Prefer .BMP over .RIM: on (the game's data has no .BMP pictures, so no picture changes)" : "Prefer .BMP over .RIM: off")
			: (*slot ? "Use Polygon View Distance: on (kept as the setting: this renderer has no polygon view distance to apply)" : "Use Polygon View Distance: off");

		if (!layoutSettingsFile[0] || !host::EditorOptions_Save(layoutSettingsFile, key, *slot))
		{
			optionWords += " - the setting could not be written to the settings file";
		}

		statusWords = optionWords.c_str();
	}

	// An item of Track Editing's bars that DOES something (an item that says it is not done does nothing, and so shows no other page): the page its command shows, as inteng's handler does (track_pages.h)
	if (IsTheTrackScreen(barScreen) && host.screen == barScreen)
	{
		host::IntengItem const *const done = host::IntengMenus_Find(id);

		if (done && done->work != host::IntengWork::None)
		{
			ShowTrackPageAfter(done->command);
		}
	}

	if (host.toggleObjectList)
	{
		ToggleTheObjectList();
	}

	// Taking an object out of the level, or adding a copy of one (S5): the bar gave no status words for these, so each says its own
	if (host.deleteSelectedObject)
	{
		DeleteTheSelectedObject();
	}

	if (host.createLikeSelected)
	{
		PlacementDuplicate();
	}

	if (host.copyTrack)
	{
		CopyTheTrack();
	}

	if (host.pasteTrackAbsolute)
	{
		PasteTheTrackAbsolute();
		host.screen = barScreen;
	}

	if (host.pasteTrackRelative)
	{
		PasteTheTrackRelative();
		host.screen = barScreen;
	}

	if (host.setBaseModule)
	{
		SetTheBaseModule();
	}

	if (host.changeType != 0)
	{
		ChangeTheObjectType(host.changeType);
	}

	if (host.pathCommand != 0)
	{
		PathModeCommand(host.pathCommand);
	}

	if (host.strategyCommand != 0)
	{
		StrategyModeCommand(host.strategyCommand);
	}

	// WAYPOINT'S EXIT in a deeper mode goes back one mode (FUN_00583c10: the waypoints are written, mode 0 again), it is not the screen's Exit: the command is taken by the mode (DoWaypointCommand, below)
	if (host.exitMode && barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Waypoint") == 0 && waypointMode.Mode() != 0)
	{
		host.exitMode = false;
		host.waypointCommand = 2;
		host.waypointAsked = true;
	}

	// Light Sourcing's Exit: its write-back (every light un-hidden, bits 0x300 out of the engine's lights) comes BEFORE the mode's Exit saves the copy
	if (host.exitMode && host.lightWork == kLightExit)
	{
		RunLightWork(kLightExit);
		host.lightWork = kLightNone;
	}

	if (host.exitMode)
	{
		ExitTheMode();
		host.screen = barScreen;
	}

	if (host.trackCommand != 0)
	{
		TrackEditCommand(host.trackCommand);
		host.screen = barScreen;   // Stop Track Edit may have shown another screen already
	}

	// Acoustic Mode's items: Exit (0xa, FUN_004f56c0) goes back to Object Placement (the screen change below lets go of the picks: AcousticLeave); the others are AcousticCommand's
	if (host.acousticCommand == 0xa)
	{
		host.screen = ScreenOfName("Object Placement");
	}
	else if (host.acousticCommand != 0)
	{
		AcousticCommand(host.acousticCommand);
	}

	// OP-47 (V-21): the alternate-location commands 0x64, 0x65, 0x66, 0x67, 0x68 (0x4dc235-0x4dc28d), Move relative 0x1c and Replace 0x1d need Gravity off (FUN_004d9af0 0x4dc1fe); with it on, nothing
	if (host.altLocation != 0 && (host.altLocation == 0x69 || !PlacementGravityOn()))   // 0x69 is read before the Gravity block (0x4dc0a3); the others inside it
	{
		AltLocationCommand(host.altLocation);
	}

	if (host.quickCreate != 0)
	{
		QuickCreateAtTheView(host.quickCreate);
	}

	if (host.moveRelToPoly)
	{
		StartMoveRelPoly();
	}

	if (host.moveCentre != 0)
	{
		MoveTheCentre(host.moveCentre == 1);
	}

	if (host.recalculateNormals)
	{
		RecalculateTheNormals();
	}

	if (host.moduleOverlaps)
	{
		ToggleModuleOverlaps();
	}

	if (host.takeScreenshot)
	{
		TakeAScreenshot();
	}

	// Save a copy (S3d): says where it went itself. On Strategy Data (its bar's Save, the flag pages' too: FUN_00524e90 / FUN_00525700 -> FUN_005282d0) every switch's Links first (S-K61-3)
	if (host.saveCopy)
	{
		if (OnStrategyScreen())
		{
			StrategySave();
		}
		else
		{
			SaveEveryPlacedShape();   // Object Placement's Save (0xe): every placed shape's save first (OP-130a, TB-L2)
			SaveTheCopy();
		}
	}

	// The level's settings in the field panel (L)
	if (host.levelSettings)
	{
		ShowTheLevelSettings();
	}

	if (host.prompt != kPromptNone)
	{
		OpenAPrompt(host.prompt);
	}

	// Light Sourcing's bar items (M-3)
	if (host.lightWork != kLightNone)
	{
		RunLightWork(host.lightWork);
	}

	// Construction's Keys - Mode: inteng copies its text and shows it (FUN_00445280); here it is a text over the level, closed by any key or press
	if (host.showConstructionKeys)
	{
		textPanelText = kIntengConstructionKeysText;
		memset(keyDown, 0, sizeof(keyDown));
	}

	// Construction's Keys - Movement (0x2be, FUN_004452b0 0x445f40 -> FUN_005573c0): the camera keys' text, the one Texture Map's Help > Movement shows (the words at 0x600394), on the
	// shared text screen (FUN_004724f0), closed by any key or press (TB-C6)
	if (host.showMovementKeys)
	{
		textPanelText = kIntengTextureMapMovementText;
		memset(keyDown, 0, sizeof(keyDown));
	}

	// Object Placement's Keys (0x53): FUN_004724f0 over the key list of FUN_004d9af0
	if (host.showPlacementKeys)
	{
		textPanelText = kIntengPlacementKeysText;
		memset(keyDown, 0, sizeof(keyDown));
	}

	// Module Linking's Help (0xd2, FUN_004c0ed0): inteng copies its text and shows it (FUN_004724f0)
	if (host.showLinkHelp)
	{
		textPanelText = kIntengLinkHelpText;
		memset(keyDown, 0, sizeof(keyDown));
	}

	// Command, then click (P4): an item that waits for the next click in the view. The status line says what it waits for; Esc cancels.
	if (host.clickWants != 0)
	{
		if (host::ClickMode_Start(host.clickCommand, static_cast<host::ClickWants>(host.clickWants), host.clickWords))
		{
			statusWords = host::ClickMode_Words();
		}
		else
		{
			statusWords = "That command can't wait for a click";
		}
	}

	if (host.chooserOp != kChooserNone && !(host.chooserOp == kChooserStart && PlacementGravityOn()))
	{
		ShapeChooserCommand(host.chooserOp, &host.screen);
	}

	if (host.hierarchyCommand != 0)
	{
		HierarchyRunCommand(host.hierarchyCommand);
	}

	if (host.placedHierarchyCommand != 0)
	{
		PlacedHierarchyCommand(host.placedHierarchyCommand);
	}

	if (host.fixTears != 0)
	{
		FixTearsStart(host.fixTears == 2);
	}

	if (host.recentrePolygon)
	{
		RecentrePolygonStart();
	}

	if (host.moveRelativeModule && !PlacementGravityOn())
	{
		MoveRelativeModuleStart();
	}

	if (host.splitScreen)
	{
		SplitScreenCommand();
	}

	if (host.gravityToggle)
	{
		GravityToggleCommand();
	}

	if (host.snapToVertex)
	{
		SnapToVertexStart();
	}

	if (host.placementPolyCounts)
	{
		PlacementPolyCountsToggle();
	}

	if (host.specialCommand != 0)
	{
		SpecialRunCommand(host.specialCommand, host.screen < host::kIntengScreenCount && strcmp(host::kIntengScreens[host.screen].name, "Placement mode 1 (sounds)") == 0);
	}

	if (host.textureCommand != 0)
	{
		TextureMapCommand(host.textureCommand);
	}

	if (host.deleteModuleData)
	{
		DeleteTheModuleData();
	}

	if (host.mergeObjects)
	{
		MergeTheSelectedObjects();
	}

	if (host.findNotDone)
	{
		FindNotDoneAdjacent();
	}

	if (host.recalcEntryPoints)
	{
		RecalculateEntryPoints();
	}

	// A screen's own View Mode items (Module Linking 0x46 / 0x47 / 0x48, Adjacent Modules 0xc8 / 0xc9): the draw mode every shape is drawn in. inteng calls
	// FUN_0046daa0 / FUN_0046dae0 / FUN_0046db20 on each shape of its table, and (Module Linking) lets go of its Mixed colouring first; the editor's
	// draw modes are global and the status line and the buttons follow them (view_toggles.h).
	// Object Placement's Misc > Lighting (0x54, OP-44; only Object Placement's and Track Editing's tables hold it): DAT_008f4f0c flips (0x4d9f31-0x4d9f5f), the objects follow at the next frame
	// (LevelView_PlacementLighting) and the item is ticked by it (editor_panels.cpp). inteng says nothing; the status words are ours. Was: the whole level's Gouraud switch
	host.viewMode = host.viewMode == 4 && PlacementLighting_Flip(&statusWords) ? 0 : host.viewMode;

	if (host.viewMode != 0)
	{
		host::ViewToggle const which = host.viewMode == 1 ? host::ViewToggle::Polygon : host.viewMode == 2 ? host::ViewToggle::Polyline : host.viewMode == 4 ? host::ViewToggle::GouraudSwitch : host::ViewToggle::FilledPolyline;

		if (host.viewMode != 4)
		{
			ViewToggles_SetMixed(false);

			// on a screen that starts as the white wire (ScreenLookPass), Polyline is that wire (FUN_0046daa0) and the others leave it (FUN_0046db20 / FUN_0046dae0)
			if (ScreenStartsAsWire(ScreenLookGroup(barScreen)))
			{
				screenWire = host.viewMode == 2;
			}
		}

		statusWords = ViewToggles_UseNow(which);
	}

	// Adjacent Modules' Add, Delete and Set Current (the list only: the write-back is not read, see adjacent_list.h)
	if (host.adjacentOp != kAdjNone)
	{
		AdjacentEdit(host.adjacentOp);

		// Exit whose write-back is asking its questions (AD-6a): the screen stays on Adjacent Modules, and the status on the question's words, until the last answer runs the Exit again
		if (host.adjacentOp == kAdjExit && adjacentExitHeld)
		{
			host.screen = barScreen;
			host.statusWords = nullptr;
		}
	}

	// While Show All holds the screen, inteng's frame ends before it reads the command (0x4c1b74): the item chosen does nothing (row ML-M-3)
	if (showAllHeld && OnModuleLinkingScreen())
	{
		host.linkOp = kLinkNone;
	}

	// While AutoLink or Delete Links runs (inteng holds the program; ours keeps the view going, autolink_runner.h, DeleteLinksFrame): no link command, the trees are the run's
	if (host.linkOp != kLinkNone && (AutoLinkRunning() || DeleteLinksRunning()))
	{
		host.linkOp = kLinkNone;
		Answer("AutoLink is running: right click to cancel");
	}

	// A link made (any Module Linking action but the view ones) lets go of Show All's paint
	if (host.linkOp != kLinkNone && host.linkOp != kLinkShowAll && host.linkOp != kLinkViewDistance && host.linkOp != kLinkOnlyLinked && host.linkOp != kLinkWithAvp)
	{
		showAllModules.clear();
		showAllColours.clear();
	}

	// Module Linking (P5)
	if (host.linkOp == kLinkExit)
	{
		LinkExit();
	}
	else if (host.linkOp == kLinkDeleteLinks)
	{
		DeleteModuleLinks(false);
	}
	else if (host.linkOp == kLinkReverseModule || host.linkOp == kLinkReverseAll)
	{
		AddReverseLinksCommand(host.linkOp == kLinkReverseAll);
	}
	else if (host.linkOp == kLinkLoadFile)
	{
		AutoLinkLoadFileCommand();
	}
	else if (host.linkOp == kLinkExitNew)
	{
		// Exit + New Adjacencies (0x66, FUN_004c1a90 0x4c1e3f-0x4c1e76): the current module's links are written (FUN_004c3b90), the adjacency is worked out for the modules with none,
		// the screen's state is let go of. Ours: the exit first (LinkExit writes the links and lets go), then the work.
		LinkExit();
		AdjacencyWithMessage(true);   // "Calculating new module adjacency" (0x4c1e56), then FUN_004c5a30(1)
	}
	else if (host.linkOp == kLinkExitAll)
	{
		// Exit + All Adjacencies (0x65, 0x4c1e77-0x4c1ed9): asks "Do you want to regenerate adjacent module data?"; only an OK works it out. The exit happens either way.
		LinkExit();
		OpenAPrompt(kPromptRegenAdjacency);
	}
	else if (host.linkOp == kLinkAutoModule || host.linkOp == kLinkAutoEnvironment || host.linkOp == kLinkAutoModuleNew || host.linkOp == kLinkAutoEnvironmentNew)
	{
		AutoLinkCommand(host.linkOp == kLinkAutoEnvironment || host.linkOp == kLinkAutoEnvironmentNew, host.linkOp == kLinkAutoModuleNew || host.linkOp == kLinkAutoEnvironmentNew);
	}
	else if (host.linkOp == kLinkSetCurrent)
	{
		LinkSetCurrent();
	}
	else if (host.linkOp == kLinkMixed)
	{
		// inteng: Mixed colour set, every shape in Polyline lines, the tree's modules painted (FUN_004c17a0). Here: the Polyline draw mode, orange lines, the tinted modules solid.
		if (!ViewToggles_OutlinesCanBeShown())
		{
			statusWords = "Outlines can't be shown yet: the game's renderer can't draw them in this build";
		}
		else
		{
			ViewToggles_UseNow(host::ViewToggle::Polyline);
			ViewToggles_SetMixed(true);
			statusWords = "Mixed: every module as lines, the current and linked modules solid";
		}
	}
	else if (host.linkOp == kLinkShowAll)
	{
		ShowAllTheModules();
	}
	else if (host.linkOp == kLinkViewDistance)
	{
		viewDistanceOn = !viewDistanceOn;
		SayTheViewDistance();
	}
	else if (host.linkOp == kLinkOnlyLinked)
	{
		onlyDrawLinked = !onlyDrawLinked;
		SayTheViewDistance();
	}
	else if (host.linkOp == kLinkWithAvp)
	{
		host::ModuleLinkAvp_Toggle();
	}
	else if (host.linkOp == kLinkPolyCounts)
	{
		PolyCountsCommand();
	}
	else if (host.linkOp == kLinkCentres)
	{
		selectWithCentres = !selectWithCentres;
		Answer(selectWithCentres ? "Select With Centres: on (a click takes the module whose centre is nearest the pointer)" : "Select With Centres: off");
	}
	else if (host.linkOp == kLinkHideObjects || host.linkOp == kLinkUnhideLast || host.linkOp == kLinkUnhideAll)
	{
		HideObjectsCommand(host.linkOp);
	}
	else if (host.linkOp != kLinkNone)
	{
		LinkEdit(host.linkOp);
	}

	// WAYPOINT MODE (waypoint_mode.h): an item of its code-built menus was chosen. Exit is the menu's own "back to Construction" item, so in a deeper mode of the screen it is
	// taken here instead and goes back one mode.
	{
		bool const onWaypoint = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Waypoint") == 0;

		if (onWaypoint && !host.waypointAsked && host.screen != barScreen && waypointMode.Mode() != 0)
		{
			host.screen = barScreen;
			host.waypointCommand = 2;
			host.waypointAsked = true;
		}

		if (onWaypoint && host.waypointAsked)
		{
			DoWaypointCommand(host.waypointCommand, host);
		}
	}

	// Mode > Strategy Data: No Textures draws the level as a white wireframe while the Strategy Data screen is the one in use (a screen change away from it puts the look back: ours,
	// inteng reloads the level at each screen's start); Mode > Normal and the others leave it as it is
	{
		static bool paintWhite = false;
		bool const toStrategy = host.screen < host::kIntengScreenCount && strcmp(host::kIntengScreens[host.screen].name, "Strategy Data") == 0;

		if (host.paintWhite)
		{
			paintWhite = true;
		}
		else if (host.screen != barScreen && !toStrategy)
		{
			paintWhite = false;
		}

		ViewToggles_SetPaintWhite(paintWhite);
	}

	// LIGHT SOURCING'S BAR IS THE LEVEL'S CHOICE, NOT THE USER'S (FUN_004aa440 0x4aa4d7-0x4aa59b): the start looks for OBJCHIER chunks in the level (FUN_004316e0 with "OBJCHIER" 0x5ccbe0); with none it registers the table at 0x5e1360 (the Doors
	// menu), with any the table at 0x5e1750 (the Hierarchy menu). So the hub's Light Sourcing item shows the one the level calls for.
	if (host.screen != barScreen && host.screen == ScreenOfTable("0x5e1360") && host::LevelHasHierarchies(LevelObjects_Level()))
	{
		host.screen = ScreenOfTable("0x5e1750");
	}

	// A pick that went to another screen: that screen's bar is made and shown
	if (host.screen != barScreen)
	{
		bool const intoTextureMap = !IsTheTextureMapScreen(barScreen) && IsTheTextureMapScreen(host.screen), outOfTextureMap = IsTheTextureMapScreen(barScreen) && !IsTheTextureMapScreen(host.screen);
		bool const intoLight = !IsALightScreen(barScreen) && IsALightScreen(host.screen), outOfLight = IsALightScreen(barScreen) && !IsALightScreen(host.screen);
		bool const intoHierarchy = !IsTheHierarchyScreen(barScreen) && IsTheHierarchyScreen(host.screen), outOfHierarchy = IsTheHierarchyScreen(barScreen) && !IsTheHierarchyScreen(host.screen);

		// the mode left lets go of its own first (Texture Map's removed polygons go into the bytes), then the view loads the working bytes when they changed (the view gap), then the mode
		// entered starts on that view
		if (outOfTextureMap)
		{
			LeaveTextureMap();
		}

		if (outOfLight)
		{
			LeaveLightSourcing();
		}

		if (outOfHierarchy)
		{
			LeaveHierarchy();
		}

		ShowTheWorkingBytes();

		if (intoTextureMap)
		{
			EnterTextureMap();
		}

		size_t const wasScreen = barScreen;

		// Construction left for a mode: the hub's leave FUN_004476d0 runs FUN_004704d0(10) (0x447804) before the mode's start (camera-saved-vasquez.md CS-2)
		if (wasScreen == host::kIntengHub && host.screen != host::kIntengHub)
		{
			CameraPositionSaved(10);
			SmallModulesForget();   // and its first loop empties Find Small Modules' list (DAT_00631250, 0x4476f1; TB-C4)
		}
		else if (host.screen == host::kIntengHub && wasScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[wasScreen].name, "Shape Morph") != 0)
		{
			// a mode left for Construction by an item of its own that is not the exit sweep's (Object Placement's Acoustic page Exit: FUN_004d9820 0x4d9925): every mode's exit but Shape Morph's
			// runs FUN_004704d0(10) (CS-5 .. CS-16; Shape Morph FUN_004ca190 does not)
			CameraPositionSaved(10);
		}

		// every arrival at Construction is the hub's set-up FUN_00447550, which puts the rotation centre back on the objects' average (0x447652, CAM-7), Shape Morph's way back too
		if (host.screen == host::kIntengHub && wasScreen != host::kIntengHub)
		{
			RotationCentreDefault();
		}

		if (intoLight)
		{
			EnterLightSourcing();
		}

		if (intoHierarchy)
		{
			EnterHierarchy();
		}

		barScreen = host.screen;

		OnScreenChanged(wasScreen, barScreen);   // every mode's start and leave hooks (also run by ShowTheScreen, a mode's own Exit or Tab)

		// leaving the hub for a mode: the hub's selection lists are freed (FUN_004476d0) and the mode builds its own (SEL-2): nothing selected in the hub is current in the mode. First, with the
		// hub's changes not yet in the file (DAT_00631264), the level FILE is written (FUN_004719e0 at 0x4476e3)
		if (wasScreen == host::kIntengHub)
		{
			if (LevelChangedSinceSave())
			{
				SaveAtIntengsPoint();
			}

			host::Selection_Clear();
		}

		if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0)
		{
			StrategyCleanNow();   // FUN_00524cc0 -> FUN_00522ab0 0x522ae2: DAT_00914950 = 0 on entry from the hub (W-3)
		}

		// a screen starts on its first page (Track Editing on page 2: a track is loaded when it is entered, host::kTrackPageEntry); Waypoint mode starts in its first mode
		EditorBar_SetPage(IsTheTrackScreen(barScreen) ? host::kTrackPageEntry : 0);
		waypointMode.Enter();

		if (!LoadTheMenus())
		{
			LogString("menus: the bar of screen " + std::to_string(barScreen) + " couldn't be made (editor_bar.cpp), so the editor closed");
			host.running = false;
		}

		// Object Placement's Edit Track (0x36) goes to the Track Edit screen: the selected object's track is loaded there, or the screen does not change
		if (OnTheTrackScreen() && trackEd.state == host::TrackState::Off)
		{
			if (!TrackEditEnter())
			{
				barScreen = wasScreen;
				LoadTheMenus();
			}

			host.statusWords = nullptr;   // Edit Track's own words stay on the status line
		}
		else if (!OnTheTrackScreen() && trackEd.state != host::TrackState::Off)
		{
			TrackEditLeftByOtherWay();
		}

		// Acoustic Mode: entered (AC-1, FUN_004f5620), or left by its Exit or any other way (AC-8, FUN_004f56c0: the picks let go of)
		if (IsTheAcousticScreen(barScreen) && !IsTheAcousticScreen(wasScreen))
		{
			AcousticEnter();
		}
		else if (!IsTheAcousticScreen(barScreen) && IsTheAcousticScreen(wasScreen))
		{
			AcousticLeave();
		}
	}

	if (host.statusWords)
	{
		statusWords = host.statusWords;
	}

	// A key layout picked: kept in the settings for the next start, the menus made again with its keys, and every key counted as let go,
	// as a key held now may do something else in the new layout. Menus the bar refuses would be a mistake in the editor's own table
	// (the test makes them for both layouts), and the editor then closes, saying so in its log. (Added 2026-11-14 by Melanikus
	// (Dietrich), F3.)
	if (host.layout != keyLayout)
	{
		keyLayout = host.layout;
		memset(keyDown, 0, sizeof(keyDown));

		if (!KeyLayout_Save(layoutSettingsFile, keyLayout))
		{
			statusWords = kLayoutNotSavedWords;
			LogString(std::string("key layout: ") + kLayoutNotSavedWords);
		}

		if (!LoadTheMenus())
		{
			LogString("key layout: the editor's own table of actions doesn't make a menu bar for these keys (editor_actions.cpp), so it closed");
			host.running = false;
		}
	}

	if (!host.running)
	{
		// the hub's Quit (DAT_0091c01c, the main loop's end 0x58d07d): the hub is left (FUN_004476d0), so the level FILE is written when it changed. A window closed writes nothing (inteng:
		// WM_CLOSE goes to DefWindowProc, WM_DESTROY quits)
		if (barScreen == host::kIntengHub && LevelChangedSinceSave())
		{
			SaveAtIntengsPoint();
		}

		running = false;
		bRunning = false;
	}
}

// The action of the menu's item whose inteng command number is command, or 0: a cascade's rows are searched too (BAR-10: "Choose" under Object Placement's "Choose/Selected" is command
// 0x32), and an item that opens a cascade is passed over (it picks nothing, FUN_004b7dd0 0x4b821e-0x4b8248)
static int32_t MenuActionOfCommand(ui::Menu const &menu, int32_t command)
{
	for (size_t i = 0; i < menu.count; i++)
	{
		ui::MenuItem const &entry = menu.items[i];

		for (size_t k = 0; k < entry.subCount; k++)
		{
			host::IntengItem const *const row = host::IntengMenus_Find(entry.subItems[k].action);

			if (row && row->command == command)
			{
				return entry.subItems[k].action;
			}
		}

		host::IntengItem const *const item = entry.subCount == 0 ? host::IntengMenus_Find(entry.action) : nullptr;

		if (item && item->command == command)
		{
			return entry.action;
		}
	}

	return 0;
}

// The action of the item of the bars whose inteng command number is command (the first bar is searched, then the second), or 0
int32_t BarActionOfCommand(int32_t command)
{
	int32_t found = 0;

	for (size_t row = 0; row < barRowCount && !found; row++)
	{
		for (size_t m = 0; m < barRows.menuCount[row] && !found; m++)
		{
			found = MenuActionOfCommand(barRows.menus[row][m], command);
		}
	}

	// then the right-click pop-up's table (method +0x14, FUN_004b8350): its items are the same commands (FUN_004b7c50 returns the pick to the screen's command switch)
	for (size_t m = 0; m < barRows.logical[1].menuCount && !found; m++)
	{
		found = MenuActionOfCommand(barRows.logical[1].menus[m], command);
	}

	return found;
}

// The scenario harness's item (kHarnessItem): the item of the bars whose inteng command number is command, done as a click on it is. The first bar is searched, then the second.
static void HarnessRunItem(int32_t command)
{
	int32_t const found = BarActionOfCommand(command);   // a cascade's rows included (BAR-10)

	if (!found)
	{
		printf("AVP_OpenInteng: item 0x%x refused: no item of the bars has that command\n", (unsigned)command);
		fflush(stdout);
		harnessScenarioAsked = true;
		return;
	}

	host::IntengItem const *const item = host::IntengMenus_Find(found);

	printf("AVP_OpenInteng: item 0x%x done: \"%s\"\n", (unsigned)command, item ? item->name : "?");
	fflush(stdout);
	RunAction(found);
	harnessScenarioAsked = true;
}

// The scenario harness's locate (kHarnessLocate): the window pixel of the middle of waypoint index's box in each of the three views
static void HarnessLocate(int32_t index)
{
	std::vector<host::WaypointRec> const &list = waypointMode.List();
	int32_t const at = index < 0 ? waypointMode.CurrentWaypoint() : index;
	RECT inside = { 0, 0, 0, 0 };
	D3DPRESENT_PARAMETERS picture;
	IDirect3DSwapChain9 *const panelsChain = ViewSurface_PanelsChain();

	// no panels' swap chain to ask (a hidden run may have none): the window's pixels are then the panels' (InThePanels does the same), so the scale is 1
	bool const haveChain = panelsChain && SUCCEEDED(panelsChain->GetPresentParameters(&picture)) && picture.BackBufferWidth > 0 && picture.BackBufferHeight > 0;

	// modes 1 and 3 draw the boxes in the 3D view: "3d x y" is where the box's block centre is on the screen (what mode 1's click measures to: WaypointBoxClick), in the window's client pixels
	if (WaypointWorldOn() && !WaypointViewsOn())
	{
		RECT view = { 0, 0, 0, 0 };
		host::PickCamera camera;
		host::PickCentre centre;
		double vx = 0.0, vy = 0.0;

		if (at >= 0 && (size_t)at < list.size() && GetClientRect(hWndMain, &view) && PickGame_Camera(view.right - view.left, view.bottom - view.top, &camera))
		{
			host::WaypointView_BoxCentre(waypointMode, (size_t)at, local_scale, centre.place);

			if (host::PickCentre_Project(camera, centre, &vx, &vy))
			{
				POINT point = { (LONG)floor(vx + 0.5), (LONG)floor(vy + 0.5) };

				MapWindowPoints(hWndMain, editorWindow, &point, 1);
				printf("AVP_OpenInteng: locate: waypoint %d of %zu; 3d %ld %ld\n", (int)at, list.size(), (long)point.x, (long)point.y);
				fflush(stdout);
				return;
			}
		}

		printf("AVP_OpenInteng: locate: no (waypoint %d, %zu in the list, the 3D view: not in front of the camera)\n", (int)at, list.size());
		fflush(stdout);
		return;
	}

	if (!WaypointViewsOn() || at < 0 || (size_t)at >= list.size() || !threeViewsFitted || threeBox.width < 64 || !GetClientRect(editorWindow, &inside))
	{
		printf("AVP_OpenInteng: locate: no (waypoint %d, %zu in the list, views %s)\n", (int)at, list.size(), WaypointViewsOn() ? "on" : "off");
		fflush(stdout);
		return;
	}

	host::WaypointRec const &wp = list[(size_t)at];
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	int32_t world[3];

	for (int i = 0; i < 3; i++)
	{
		world[i] = (int32_t)(((double)wp.min[i] + (double)wp.max[i]) / 2.0 * scale);
	}

	// the window pixel of a world place in one of the views (0 top, 1 side, 2 front), or -1 -1 when it is outside that view's quarter
	auto project = [&](int32_t const place[3], int v, int32_t out[2])
	{
		int32_t lx = 0, ly = 0;

		out[0] = out[1] = -1;

		if (v == 0 ? host::ThreeView_CrosshairTop(threeViews, place, &lx, &ly) : v == 1 ? host::ThreeView_CrosshairSide(threeViews, place, &lx, &ly) : host::ThreeView_CrosshairFront(threeViews, place, &lx, &ly))
		{
			int64_t const panelX = threeBox.x + (int64_t)lx * threeBox.width / 640;
			int64_t const panelY = threeBox.y + (int64_t)ly * threeBox.height / 480;
			int64_t const cw = inside.right - inside.left, ch = inside.bottom - inside.top;
			int64_t const pw = haveChain ? (int64_t)picture.BackBufferWidth : cw, ph = haveChain ? (int64_t)picture.BackBufferHeight : ch;

			out[0] = pw > 0 ? (int32_t)(panelX * cw / pw) : -1;
			out[1] = ph > 0 ? (int32_t)(panelY * ch / ph) : -1;
		}
	};

	int32_t lo[3], hi[3];

	for (int i = 0; i < 3; i++)
	{
		lo[i] = (int32_t)((double)wp.min[i] * scale);
		hi[i] = (int32_t)((double)wp.max[i] * scale);
	}

	int32_t mid[3][2], low[3][2], high[3][2];

	for (int v = 0; v < 3; v++)
	{
		project(world, v, mid[v]);
		project(lo, v, low[v]);
		project(hi, v, high[v]);
	}

	// each view: the middle of the box, then its lower corner and its upper corner (the box's faces are the lines through them)
	printf("AVP_OpenInteng: locate: waypoint %d of %zu; top %d %d; side %d %d; front %d %d; toplow %d %d; tophigh %d %d; sidelow %d %d; sidehigh %d %d; frontlow %d %d; fronthigh %d %d\n", (int)at, list.size(),
		mid[0][0], mid[0][1], mid[1][0], mid[1][1], mid[2][0], mid[2][1], low[0][0], low[0][1], high[0][0], high[0][1], low[1][0], low[1][1], high[1][0], high[1][1], low[2][0], low[2][1], high[2][0], high[2][1]);
	fflush(stdout);
}

// What a menu answer means to the editor: a pick does its action, and a menu opening stops the camera, as its keys go to the menu now
static void AfterMenu(ui::MenuAnswer answer, ui::MenuBar &bar)
{
	// A press on an item that's off says why on the status line (MenuBar::Refused), and the menu stays open
	if (answer == ui::MenuAnswer::Refused)
	{
		switch (bar.Refused())
		{
			case kActionViewBackFaces:
				statusWords = "Back faces can't be shown yet: the game has no switch for them";
				break;
			case kActionViewPolyline:
			case kActionViewFilledPolyline:
				statusWords = "Outlines can't be shown yet: the game's renderer can't draw them in this build";
				break;
			case kActionViewGouraudLevelOnly:
				statusWords = "Gouraud, the level's own only (inteng's third state, command 0xcc): not built yet";
				break;
			case kActionViewKeysClassic:
				statusWords = "The Classic inteng keys are in use already";
				break;
			case kActionViewKeysModern:
				statusWords = "The Modern WASD keys are in use already";
				break;
			default:
				statusWords = "That item is off at the moment";
				break;
		}
	}

	if (answer == ui::MenuAnswer::Opened)
	{
		memset(keyDown, 0, sizeof(keyDown));

		// A BARE TITLE runs at once: inteng's bar (FUN_004b7dd0 0x4b7fe1-0x4b7ff8) returns a title's own command on the press when the title has no items (+4 == 0) and its +0xc is 0, and
		// opens no drop-down (Hierarchy's Help, command 0x38, row M-HELP). A code-built screen's bare title (a kind-0 record with a command and nothing under it) is made by
		// tools/make-inteng-menus.js as a menu of ONE item with the title's own words: that menu runs its item on the press that opens it. Only the code-built screens' tables are made
		// that way (a data table's one-item menu is a real drop-down).
		size_t row = 0;

		while (row < barRowCount && &BarRow(row) != &bar)
		{
			row++;
		}

		int const open = bar.OpenMenu();

		if (row < barRowCount && open >= 0 && (size_t)open < barRows.menuCount[row] && barScreen < host::kIntengScreenCount && host::kIntengScreens[barScreen].pages)
		{
			ui::Menu const &menu = barRows.menus[row][open];

			if (menu.count == 1 && menu.title && menu.items[0].name && strcmp(menu.title, menu.items[0].name) == 0)
			{
				int32_t const action = menu.items[0].action;

				bar.Handle(ui::KeyEvent(ui::Key::Escape));
				RunAction(action);
				return;
			}
		}
	}

	if (answer == ui::MenuAnswer::Picked)
	{
		RunAction(bar.Picked());
	}
}

// The pop-up's pick: an entry with items under it (a cascade entry) opens them as the next pop-up in the same place; anything else is the command (inteng's pop-up, FUN_004b8350, opens sub-levels to the side)
static void PopupPicked(int32_t action)
{
	if (action >= kEditorPopupGroupBase)
	{
		if (EditorBar_MakePopupLevel(barScreen, action - kEditorPopupGroupBase, &popupRoom) && popupBar.Load(popupRoom.menus, popupRoom.menuCount))
		{
			popupBar.Handle(ui::KeyEvent(ui::Key::Menu));
			memset(keyDown, 0, sizeof(keyDown));
		}

		return;
	}

	RunAction(action);
}

// One event from the keyboard (window_events.h), for the menu bar: while a menu is open it takes every key, and the menu key opens one.
// The layout's keys are read as keys, in the window's procedure. Nothing else in the editor takes typing yet. (Modified 2026-11-14 by
// Melanikus (Dietrich), F3: the shortcuts went to the layout's keys.)
static void HandleEvent(const ui::Event &event)
{
	// An open pop-up takes every key
	if (popupBar.IsOpen())
	{
		ui::MenuAnswer const answer = popupBar.Handle(event);

		if (answer == ui::MenuAnswer::Picked)
		{
			PopupPicked(popupBar.Picked());
		}

		return;
	}

	// an open row under the first takes every key too
	for (size_t row = 1; row < barRowCount; row++)
	{
		if (lowerBar[row - 1].IsOpen())
		{
			AfterMenu(lowerBar[row - 1].Handle(event), lowerBar[row - 1]);
			return;
		}
	}

	if (menuBar.IsOpen() || event.key == ui::Key::Menu)
	{
		AfterMenu(menuBar.Handle(event), menuBar);
		return;
	}

	// With the keyboard in the field panel (a click on one of its fields), every key goes to the field with focus: typing, Tab, Enter. A value
	// it keeps is made an edit (ThePanelAnswered). (Added 2026-11-15 by Melanikus (Drake), S3d.)
	if (fieldsHaveKeys && fieldHost.Showing())
	{
		int32_t const before = fieldHost.FocusedField();

		ThePanelAnswered(fieldHost.Handle(event), before);
	}
}

// Where a press or a release of the mouse's button is in the panels' own picture. The menu bar's places are the editor window's, so a
// place on the view's child window is moved into them first; and the panels are drawn into a picture of their own, which Windows
// stretches over the window if the two sizes differ, so the place is moved into that picture's pixels (host_frame.h). A press and its
// release are placed the same way, here. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
// ---- ZOOM (Insert) and RESET ZOOM (Home), the Movement help of every 3D screen: "Insert  Zoom : Left click to choose centre of zoom, then after choosing area, left click to zoom in or right click to zoom out. (escape cancels
// process)", "Home  Reset to default zoom level." FUN_00471cb0 (0x471cb0) is the zoom, FUN_004721a0 (0x4721a0) the reset, both called by the shared 3D frame FUN_00529370 when the key slots 0xd87a54 (Insert) / 0xd87a57 (Home) are
// down (0x5293a2-0x5293b4). The zoom shows "Zoom... Choose centre" (0x5d3e80), waits for a press (any button) = the CENTRE; then the mouse moves a RECTANGLE of the screen's own shape about the centre (half the
// width = the larger of the horizontal distance and the vertical distance times width / height, kept on the screen: 0x471ea6-0x471f3e; it is drawn with FUN_00472200); a press ends it, Esc (0xd87a45) cancels. The press does
// (0x471f8b-0x471fbe): the view's projection scale (+0x34 and +0x38 of the view block, then the larger in +0x3c) times width / (2 x half the rectangle's width) for the LEFT button, times (2 x half the width) / width for the
// RIGHT one, held to 10 .. 1,000,000 (whole numbers; the default is half the width, 0x4721a0 puts it back); and the camera is turned toward the centre first chosen: by the angles whose tangents are the centre's distance from the
// middle over the scale now (FUN_0053ca80 with 0x10000 and the scale, 0x471d15-0x471d38), as a rotation multiplied into the camera's matrix (FUN_004b34b0 at 0x47207d).
// OURS, named: the scale is the view's own projection scale (LevelView_SetProjectionScale: from 0.1 to 10 times the game's, where inteng's whole numbers reach 1000000 / (half the width)); the turn is added to the camera's turn and
// look (the same rotation when the camera is not rolled); a zoom is kept while the level is open.
int zoomStage = 0;               // 0 none, 1 waiting for the centre, 2 the rectangle moves

// inteng's modal waits that stand the screen's frame still: Scroll Lock's pick (FUN_004720c0) and Insert's zoom (FUN_00471cb0: its own loops 0x471cda-0x471ce6 and 0x471e80-0x471f71 read only the
// mouse word, the pointer and Escape). Meanwhile no key acts (a key down is only held), no screen frame reads a held key, the camera's mover does not run (a review of S-VOL-11, 11-17)
static bool ModalWaitOn()
{
	return rotationPickWaiting || zoomStage != 0;
}
float zoomScale = 1.0f;          // the projection scale the zoom gives (1 = the game's own, what Home puts back)
int zoomCentre[2] = { 0, 0 };    // the centre chosen, in the view's own pixels from its middle
int zoomHalf = 0;                // half the rectangle's width, in pixels
ui::Box zoomViewBox = { 0, 0, 0, 0 };   // the 3D view's box in the panels' coordinates, as the last frame drew it
static std::string zoomWords;

static void ZoomCancel()
{
	zoomStage = 0;
	zoomHalf = 0;
}

static void ZoomStart()
{
	zoomStage = 1;
	zoomWords = "Zoom... Choose centre";   // inteng's words (0x5d3e80)
	statusWords = zoomWords.c_str();
}

// The rectangle for the pointer at x, y (panels' coordinates): FUN_00471cb0 0x471ea6-0x471f3e. Gives the half-width in pixels (0 when it has none).
static int ZoomRectangle(int x, int y)
{
	int const width = zoomViewBox.width;
	int const height = zoomViewBox.height;

	if (width < 8 || height < 8)
	{
		return 0;
	}

	int const cx = width / 2 + zoomCentre[0];   // the centre in the view's own pixels from its top left
	int const cy = height / 2 + zoomCentre[1];
	int dx = abs((x - zoomViewBox.x) - cx);
	int dy = abs((y - zoomViewBox.y) - cy);
	int const scaled = (dy * width) / height;

	dx = dx < scaled ? scaled : dx;
	dx = dx > cx ? cx : dx;
	dx = dx > (width - cx - 1) ? (width - cx - 1) : dx;

	int half = (height * dx) / width;

	half = half > cy ? cy : half;
	half = half > (height - cy - 1) ? (height - cy - 1) : half;
	return (half * width) / height;
}

// A press at x, y (panels' coordinates) with the right button or the left: the centre, then the zoom. Gives back false when no zoom is waiting.
bool ZoomPress(int x, int y, bool right)
{
	if (zoomStage == 0 || !theCamera || zoomViewBox.width < 8 || zoomViewBox.height < 8)
	{
		return false;
	}

	if (zoomStage == 1)
	{
		zoomCentre[0] = (x - zoomViewBox.x) - zoomViewBox.width / 2;
		zoomCentre[1] = (y - zoomViewBox.y) - zoomViewBox.height / 2;
		zoomHalf = 0;
		zoomStage = 2;
		return true;
	}

	int const half = zoomHalf;

	if (half > 0)
	{
		double const wide = (double)zoomViewBox.width;
		double const factor = right ? (2.0 * (double)half) / wide : wide / (2.0 * (double)half);
		float scale = (float)((double)zoomScale * factor);

		scale = scale < 0.1f ? 0.1f : (scale > 10.0f ? 10.0f : scale);

		// the turn toward the centre first chosen, by the focal length as it was (FUN_00471cb0 0x471d15: the scale before the click)
		double const focal = LevelView_FocalLength(zoomViewBox.width);

		if (focal > 1.0)
		{
			double const turn = atan((double)zoomCentre[0] / focal) * (double)kFlyCameraWholeTurn / 6.28318530717959;
			double const look = atan((double)zoomCentre[1] / focal) * (double)kFlyCameraWholeTurn / 6.28318530717959;

			theCamera->turn -= (int)floor(turn + (turn < 0 ? -0.5 : 0.5));   // the game's angles turn the other way (MoveTheCamera): a centre to the right turns right
			theCamera->look -= (int)floor(look + (look < 0 ? -0.5 : 0.5));   // a centre above the middle looks up
			FlyCamera_Hold(theCamera);
		}

		zoomScale = scale;
		zoomWords = std::string("Zoom: ") + (right ? "out" : "in") + ", the view is " + std::to_string((long long)(zoomScale * 100.0f + 0.5f)) + "% of the game's own (Home puts it back)";
		statusWords = zoomWords.c_str();
	}

	zoomStage = 0;
	zoomHalf = 0;
	return true;
}

void ZoomMove(int x, int y)
{
	if (zoomStage == 2)
	{
		zoomHalf = ZoomRectangle(x, y);
	}
}

static void ZoomReset()
{
	zoomScale = 1.0f;   // FUN_004721a0
	zoomStage = 0;
}

static POINT InThePanels(HWND window, LPARAM lParam)
{
	POINT at = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	if (window != editorWindow)
	{
		MapWindowPoints(window, editorWindow, &at, 1);
	}

	RECT inside = { 0, 0, 0, 0 };
	D3DPRESENT_PARAMETERS picture;
	IDirect3DSwapChain9 *const panelsChain = ViewSurface_PanelsChain();

	if (panelsChain && GetClientRect(editorWindow, &inside) && SUCCEEDED(panelsChain->GetPresentParameters(&picture)))
	{
		int32_t panelsX = at.x;
		int32_t panelsY = at.y;

		HostFrame_WindowToPanels(at.x, at.y, inside.right - inside.left, inside.bottom - inside.top, (int32_t)picture.BackBufferWidth,
			(int32_t)picture.BackBufferHeight, &panelsX, &panelsY);
		at.x = panelsX;
		at.y = panelsY;
	}

	return at;
}

// The window's messages. Keys and the mouse are only noted here; the camera moves once a frame, in MoveTheCamera.
// (Modified 2026-11-14 by Melanikus (Dietrich), F3: keys go through window_events.h to the menu bar and the shortcuts as well, a press of
// the left button goes to the menu bar, and Windows' own key menu is swallowed for Alt alone and F10, as the editor's bar has those.)
// STARTED WITH NO FILE (inteng: FUN_00447ed0 = FUN_00474030 + FUN_0057c800 + FUN_0057d2b0, in the window WinMain made first): the window holds only inteng's message box. Written 2026-11-16 by Melanikus (Dietrich).
static bool noFileScreenOn = false;   // the windows' procedure is the message's alone
static bool noFileScreenOpen = true;

static LRESULT CALLBACK NoFileProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_LBUTTONDOWN:
			DialogPress((int32_t)(short)LOWORD(lParam), (int32_t)(short)HIWORD(lParam));
			return 0;
		case WM_KEYDOWN:
		case WM_CHAR:
			DialogKey(message, wParam);
			return 0;
		case WM_CLOSE:
			noFileScreenOpen = false;
			return 0;
	}

	return DefWindowProcA(window, message, wParam, lParam);
}

struct NoFileFrame
{
	int32_t width, height;
};

static void AddTheNoFilePanels(void *context)
{
	NoFileFrame const *const frame = (NoFileFrame const *)context;
	std::vector<host::PaintBox> boxes;
	std::vector<host::PaintText> lines;

	host::Dialogs_Draw(&boxes, &lines, kPanelPassBackground, host::kMenuLitColour);

	PanelExtras extras = { boxes.data(), boxes.size(), lines.data(), lines.size() };

	PanelPass_AddToFrame(frame->width, frame->height, extras);
}

// The message, drawn each frame until Ok (a click, or Return / Escape) is used. A hidden run saves the picture on its third frame, says what the box holds, and presses Ok itself on the fifth.
static int NoFileScreen(int width, int height, char const *picturePath)
{
	char const *const words = "Please load a rif file by double clicking on it";
	bool answered = false;
	NoFileFrame frame = { width, height };

	host::Dialogs_SetScreen(0, 0, width, height);
	DialogMessage(words, [&answered]() { answered = true; });

	for (long frames = 1; !answered && noFileScreenOpen; frames++)
	{
		MSG message;

		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}

		// the game's picture is made the size of the window (as every frame of the editor does it, view_surface.h)
		if (!ViewSurface_BeforeFrame(width, height, width, height))
		{
			if (ViewSurface_IsBroken())
			{
				break;
			}

			MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
			frames--;
			continue;
		}

		bool const saveNow = picturePath && frames == 3;

		LevelView_DrawNoLevel(saveNow ? picturePath : nullptr, AddTheNoFilePanels, &frame);

		if (picturePath && frames == 3)
		{
			host::DialogsState const now = host::Dialogs_State();

			printf("AVP_OpenInteng: message: %s (dialog kind %s, buttons %s)\n", now.title.c_str(), now.kind.c_str(), now.buttons.c_str());
			fflush(stdout);
		}

		if (picturePath && frames == 5)
		{
			host::DialogsState const now = host::Dialogs_State();
			int bx = 0, by = 0, bw = 0, bh = 0;
			char const *const at = strchr(now.buttons.c_str(), ' ');

			if (at && sscanf_s(at + 1, "%d %d %d %d", &bx, &by, &bw, &bh) == 4)
			{
				DialogPress(bx + bw / 2, by + bh / 2);
			}
		}

		Sleep(15);
	}

	if (picturePath)
	{
		printf("AVP_OpenInteng: message answered: %s\n", answered ? "Ok" : "no");
		fflush(stdout);
	}

	return 0;
}

// SCROLL LOCK'S PICK, a press or a held button's move while it waits (CAM-8; FUN_004720c0 0x472114-0x472166, modal: it takes every press, before any screen's own): the frame picks at the pointer
// (RotationPickAnswer). Only the 3D view counts: with the three views up, its quarter (inteng's FUN_00559750 tests the press against the outlines drawn where it is, so a 2D quarter's or the bars'
// press most likely hits nothing); elsewhere a press hits nothing and the wait goes on.
static void RotationPickPress(HWND window, LPARAM lParam)
{
	POINT const at = InThePanels(window, lParam);

	if (OnAPanel(at.x, at.y) || window != hWndMain || (threeMainBox.width > 0 && !ui::BoxHolds(threeMainBox, at.x, at.y)))
	{
		return;
	}

	pickAsked = true;
	pickShift = false;
	pickCtrl = false;
	pickDelete = false;
	pickWithS = false;
	pickX = GET_X_LPARAM(lParam);
	pickY = GET_Y_LPARAM(lParam);
	pickInBox = threeMainBox.width > 0;

	if (pickInBox)
	{
		pickX = at.x - threeMainBox.x;
		pickY = at.y - threeMainBox.y;
	}
}

// Scroll Lock's pick given up (the right button, or Esc held while a button is down): the centre as it was; a held button's move may have asked the frame for a pick, which is then no click either
static void RotationPickGiveUp()
{
	rotationPickWaiting = false;
	rotationPickButtonHeld = false;
	pickAsked = false;
}

// A button pressed while Scroll Lock's pick waits (FUN_004720c0 0x472130-0x472140): the right one gives up (bit 2), so does any with Esc held at that moment (the Esc slot is read only then: Esc on its
// own does nothing); the left and the middle pick (any bit but 2 reaches FUN_00559750). True: the press is the pick's.
static bool RotationPickButton(HWND window, LPARAM lParam, bool right)
{
	if (!rotationPickWaiting)
	{
		return false;
	}

	if (right || KeyHeldNow(VK_ESCAPE))
	{
		RotationPickGiveUp();
	}
	else
	{
		rotationPickButtonHeld = true;
		RotationPickPress(window, lParam);
	}

	return true;
}

static LRESULT CALLBACK EditorWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (noFileScreenOn)
	{
		return NoFileProc(window, message, wParam, lParam);
	}

	// the screenshot keys come before anything else (#78): no menu, pop-up, dialog or prompt sees them
	if (ShotKey(message, wParam, lParam))
	{
		return 0;
	}

	// F10 is an F key like the others (camera slot 9): Windows sends it as a system key, which the editor takes as the plain key it is
	if ((message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && wParam == VK_F10)
	{
		message = message == WM_SYSKEYDOWN ? WM_KEYDOWN : WM_KEYUP;
	}

	if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
	{
		barPointerY = InThePanels(window, lParam).y;
		barPointerX = InThePanels(window, lParam).x;
	}

	// an open menu's row under the pointer is the pointed one, orange (FUN_004b7dd0 -> FUN_004b7590, view-state-table.md H-5d)
	if (message == WM_MOUSEMOVE)
	{
		POINT const pointed = InThePanels(window, lParam);

		menuBar.PointAt(pointed.x, pointed.y);
		popupBar.PointAt(pointed.x, pointed.y);

		for (ui::MenuBar &lower : lowerBar)
		{
			lower.PointAt(pointed.x, pointed.y);
		}

		// BAR-5 across our rows (inteng's bar is one row; ours wraps its titles into rows, each its own MenuBar): with a menu open in one row, the pointer on a title of ANOTHER row opens
		// that title's menu and closes the other, with no press (FUN_004b7dd0 0x4b81de-0x4b82da), as MenuBar::PointAt does inside one row
		for (size_t row = 0; row < barRowCount; row++)
		{
			ui::MenuBar &here = BarRow(row);
			bool otherOpen = false;

			for (size_t other = 0; other < barRowCount; other++)
			{
				otherOpen = otherOpen || (other != row && BarRow(other).IsOpen());
			}

			if (!otherOpen || here.IsOpen())
			{
				continue;
			}

			for (size_t m = 0; m < barRows.menuCount[row]; m++)
			{
				ui::Box const title = here.TitleBox(m);

				if (pointed.x >= title.x && pointed.x < title.x + title.width && pointed.y >= title.y && pointed.y < title.y + title.height)
				{
					for (size_t other = 0; other < barRowCount; other++)
					{
						if (other != row && BarRow(other).IsOpen())
						{
							BarRow(other).Handle(ui::KeyEvent(ui::Key::Escape));
						}
					}

					here.Press(pointed.x, pointed.y);
					break;
				}
			}
		}
	}

	// No Alt tap and no F10 for the menu bar: inteng shows its bar with a press at the top of the window or with Control held (FUN_004b7c50, DAT_00d87a5f = Control, FUN_0058c010 case 0x11; Vasquez), and F10 is camera slot 9

	// Show Poly Counts' screen takes every press and the page keys (FUN_004c0950: it waits for a mouse button, Page Down or Page Up)
	if (polyScreenOpen)
	{
		if (message == WM_KEYDOWN && (wParam == VK_NEXT || wParam == VK_PRIOR))
		{
			PolyScreenPage(wParam == VK_NEXT ? 1 : -1);
			return 0;
		}

		if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN)
		{
			POINT const at = InThePanels(window, lParam);

			PolyScreenPress(at.x, at.y, message == WM_RBUTTONDOWN);
			return 0;
		}

		if (message == WM_KEYDOWN || message == WM_CHAR || message == WM_LBUTTONUP || message == WM_RBUTTONUP)
		{
			return 0;
		}
	}


	switch (message)
	{
		case WM_SYSCOMMAND:
			if (WindowEvents_SwallowSysCommand(wParam, lParam))
			{
				return 0;
			}
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_CHAR:
		{
			if (textureColourPicking) { return 0; }   // Select Colour's grid: FUN_00560620's loop reads no key, only a mouse press (0x56086a)

			// Scroll Lock's pick waiting (CAM-8): FUN_004720c0's loop reads no key, so no key acts; a key down is only HELD (inteng's key tables follow the keys during the loop, FUN_0058c010),
			// which is how Esc held at a press gives the pick up (0x472139) and how keys held through the wait act after it
			if (rotationPickWaiting)
			{
				if (message == WM_KEYDOWN)
				{
					keyDown[wParam & 0xFF] = true;

					// Esc pressed while a press of the wait is still down: the loop's next round sees the button and Esc together and gives up (0x472139)
					if (wParam == VK_ESCAPE && rotationPickButtonHeld)
					{
						RotationPickGiveUp();
					}
				}

				// Windows' own system keys go on to it (Alt+F4 closes the window, as it does in inteng's wait: the pump FUN_0058b700 dispatches them)
				if (message == WM_SYSKEYDOWN)
				{
					break;
				}

				return 0;
			}

			// Insert's zoom waiting (FUN_00471cb0): its loops read only the mouse word, the pointer and Escape (0x471ce6 / 0x471f69: Escape gives it up), so no other key acts meanwhile, Home
			// included; a key down is only held (a review of S-VOL-11, 11-17: Home, H / N, F keys and the volume editor's keys acted during the wait)
			if (zoomStage != 0)
			{
				if (message == WM_KEYDOWN)
				{
					keyDown[wParam & 0xFF] = true;

					if (wParam == VK_ESCAPE)
					{
						ZoomCancel();   // "(escape cancels process)"
						statusWords = "Zoom cancelled";
					}
				}

				if (message == WM_SYSKEYDOWN)
				{
					break;
				}

				return 0;
			}

			// the live Snap To Vertex: Escape (slot 0xd87a45) puts the object back where it was, nothing written (0x4da8ef-0x4da941)
			if (snapLive && message == WM_KEYDOWN && wParam == VK_ESCAPE)
			{
				SnapLiveEnd(false);
				return 0;
			}

			// Module Linking's Show All is let go by Escape (slot 0x45, FUN_004c1a90 0x4c1aff), which does nothing else (row ML-M-3)
			if (showAllHeld && OnModuleLinkingScreen() && message == WM_KEYDOWN && wParam == VK_ESCAPE)
			{
				ShowAllRelease();
				return 0;
			}

			// the volume editor (FUN_005277c0): S, X, Return and the keys of the screen it shadows
			if (volume.on && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && VolumeKey(wParam))
			{
				return 0;
			}

			if (volume.on && message == WM_CHAR)
			{
				return 0;
			}

			// Stability flags' keys (FUN_00527370): Return ends (slot 0xd87a46), T switches the polygons / lines (slot 0xd87a94)
			if (stabilityActive && OnStrategyScreen() && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && (wParam == VK_RETURN || wParam == 'T'))
			{
				if (wParam == VK_RETURN)
				{
					StabilityEnd();
				}
				else
				{
					StabilityToggleLines();
				}

				return 0;
			}

			// the collector's keys (FUN_00526bf0): Return ends (slot 0xd87a46), L empties the list (slot 0xd87a8c)
			if (copyCollecting && OnStrategyScreen() && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && (wParam == VK_RETURN || wParam == 'L'))
			{
				if (wParam == VK_RETURN)
				{
					CopyCollectEnd();
				}
				else
				{
					copyTargets.clear();
				}

				return 0;
			}

			// the object pick of a view (S-KC-5) and the Lift's positions collector (FUN_00526bf0): Return (slot 0xd87a46) ends the pick; neither reads Escape: its slot is only HELD (kept in keyDown), and the view
			// ends after them when it is still down
			if (strategyEditFan && (!strategyPickField.empty() || copyCollecting) && !ADialogIsOpen() && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && (wParam == VK_RETURN || wParam == VK_ESCAPE))
			{
				if (wParam == VK_ESCAPE)
				{
					keyDown[VK_ESCAPE] = true;
				}
				else if (!strategyPickField.empty())
				{
					StrategyPickEnd();
				}

				return 0;
			}

			// the pick (FUN_00527540) and the collector (FUN_00526bf0) are loops of their own: each reads the mouse and Return (the collector also its L, Ctrl and Shift) and runs FUN_00529370's frame
			// (the view distance, Insert / Home), never the mode's own key reader FUN_00524e90 (E = Edit Strategy 0x524ee7, Tab = back to Object Placement 0x524eca) nor a view's V. So E, V and Tab do
			// nothing while either waits. Before, they fell through to the mode: E opened the view again and left the pick's choice the selection, drawn red with nothing picked (S-62's review, 11-17)
			if (OnStrategyScreen() && (!strategyPickField.empty() || copyCollecting) && !ADialogIsOpen() && message == WM_KEYDOWN && (wParam == 'E' || wParam == 'V' || wParam == VK_TAB))
			{
				return 0;
			}

			// the Mission Objective's P / V (S-K70-8, 0x532dc3: slots 0xd87a90 / 0xd87a96): the alteration row under the POINTER's y has its word bit 2 / bit 1 flipped
			// a Track Object's main page: P (slot 0xd87a90) acts as a click at y 32 (S-K72-2)
			// (the "Which ...?" window P opens never gets P's own letter: dialogKeySwallowChar, as for every key that opens one of inteng's windows)
			if (strategyEditFan && !volume.on && (strategyEditKind == 72 || strategyEditKind == 80) && trackPage < 0 && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && wParam == 'P')
			{
				StrategyViewClickAt(0, 32);
				dialogKeySwallowChar = ADialogIsOpen() ? 'p' : 0;
				return 0;
			}

			// a Hierarchy Object's main page: P (slot 0xd87a90) acts as a click at y 16 (FUN_00533a40 0x533ba5; S-K76-2)
			if (strategyEditFan && !volume.on && strategyEditKind == 76 && trackPage < 0 && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && wParam == 'P')
			{
				StrategyViewClickAt(0, 16);
				dialogKeySwallowChar = ADialogIsOpen() ? 'p' : 0;
				return 0;
			}

			// a Binary or Linked Switch's T (slot 0xd87a94, FUN_0052d700 0x52d9ee): the target pick, as a press on its line (S-K52-4); P (slot 0xd87a90, 0x52da56): the Switch Properties form (S-K52-3)
			if (strategyEditFan && !volume.on && (strategyEditKind == 52 || strategyEditKind == 61) && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && (wParam == 'T' || wParam == 'P'))
			{
				StrategyViewClickAt(0, wParam == 'T' ? 0x50 : 0x20);
				dialogKeySwallowChar = wParam == 'P' && ADialogIsOpen() ? 'p' : 0;   // P's letter is not typed into the form's seconds field
				return 0;
			}

			// a Multiple Target Switch's keys (FUN_0052f700 0x52fbda-0x52feb0; S-K66-3..7), each on the target row under the POINTER (StrategyView_MultiTargetAt): S flips its request's bit 0 (slot 0xd87a93),
			// D duplicates it, Shift + D removes it (0xd87a84, FUN_00530430 / FUN_005304a0), T flies to it and opens the targets' collector (0xd87a94, FUN_0052ff10), P the form (0xd87a90); E is the common E
			if (strategyEditFan && !volume.on && (strategyEditKind == 66 || strategyEditKind == 68) && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && (wParam == 'S' || wParam == 'D' || wParam == 'T' || wParam == 'P'))
			{
				if (wParam == 'P')
				{
					StrategyViewClickAt(0, 0x20);
					dialogKeySwallowChar = ADialogIsOpen() ? 'p' : 0;   // P's letter is not typed into the form's seconds field
				}
				else
				{
					MultiKeyUnderPointer((int)wParam, ShiftDown());
				}

				return 0;
			}

			// a Multiple Target Switch's Space (slot 0xd87a5d, 0x52fdbc-0x52fdcf): the next page of 15 targets, back to the first past the last
			if (strategyEditFan && !volume.on && (strategyEditKind == 66 || strategyEditKind == 68) && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && wParam == VK_SPACE)
			{
				MultiNextPage();
				return 0;
			}

			if (strategyEditFan && !volume.on && strategyEditKind == 70 && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && (wParam == 'P' || wParam == 'V'))
			{
				MissionToggleUnderPointer(wParam == 'P' ? 2 : 1);
				return 0;
			}

			// an Edit view ends with Escape (slot 0xd87a45, FUN_0058c010 case 0x1b; S-KC-3). A dialog open over the view takes the Escape itself (FUN_0057c800 0x57cc91: it answers 1 and clears the slot), so the view goes on
			// E on a view with a target (S-K62-5, S-K69-6, S-K72-6, the Track main page 0x534a82): the target's extra request data page (S-KC-8)
			if (strategyEditFan && !volume.on && !requestPageOn && strategyPickField.empty() && !copyCollecting && !ADialogIsOpen() && message == WM_KEYDOWN && wParam == 'E')
			{
				StrategyRequestE();
				return 0;
			}

			// Escape on the page ends it, and only it (INTENG BUG, FIXED: inteng's Mission, Track and Message pages left the Escape slot set and ended the view under them too; StrategyView_RequestEscapeEndsView)
			if (strategyEditFan && requestPageOn && !ADialogIsOpen() && wParam == VK_ESCAPE && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN))
			{
				RequestPageEnd(true);
				return 0;
			}

			if (strategyEditFan && !ADialogIsOpen() && wParam == VK_ESCAPE && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN))
			{
				StrategyViewEnd();
				return 0;
			}

			// A help text over the level waits for a MOUSE BUTTON only ("Click to continue.": FUN_004724f0 loops on the mouse flag _DAT_00d8806a and reads no key): a key does nothing
			if (textPanelText && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN))
			{
				return 0;
			}

			// the letter of the key that opened the window (dialogKeySwallowChar): only the one character message that comes next
			if (message == WM_CHAR && dialogKeySwallowChar != 0)
			{
				int const swallowed = dialogKeySwallowChar;

				dialogKeySwallowChar = 0;

				if (tolower((int)wParam) == swallowed && ADialogIsOpen())
				{
					screenKeySwallowChar = 0;   // the same character message, if a screen key opened the window
					return 0;
				}
			}

			// a dialog of inteng's (dialog_box.h, list_picker.h) takes every key
			if (ADialogIsOpen() && message != WM_SYSKEYDOWN)
			{
				DialogKey(message, wParam);
				return 0;
			}

			// An open prompt takes every key (M-1)
			if (promptFor != kPromptNone && message != WM_SYSKEYDOWN)
			{
				if (message == WM_CHAR && promptSwallow)
				{
					int const swallowed = promptSwallow;

					promptSwallow = 0;

					if (tolower((int)wParam) == swallowed)
					{
						return 0;
					}
				}

				if (!formRows.empty())
				{
					FormKey(message, wParam);
				}
				else
				{
					PromptKey(message, wParam);
				}

				return 0;
			}

			// the character of a screen key already done (see screenKeySwallowChar)
			if (message == WM_CHAR && screenKeySwallowChar != 0)
			{
				int const swallowed = screenKeySwallowChar;

				screenKeySwallowChar = 0;

				if (tolower((int)wParam) == swallowed)
				{
					return 0;
				}
			}

			// The Shape Chooser takes its own keys while it is open (Return: with the shape, Escape: without, Page Up / Page Down: the previous / next shape, S: pick from the list)
			if (chooserActive && message == WM_KEYDOWN && ShapeChooserKey(wParam))
			{
				return 0;
			}

			ui::Event event;
			bool const menuWasOpen = AMenuIsOpen();
			bool const typingHere = fieldsHaveKeys && fieldHost.Showing();   // the keyboard is in the field panel (S3d)
			bool const shiftHeld = ShiftDown();
			bool const ctrlHeld = KeyHeldNow(VK_CONTROL);
			bool const altHeld = KeyHeldNow(VK_MENU);

			if (WindowEvents_FromMessage(message, wParam, shiftHeld, ctrlHeld, altHeld, &event))
			{
				HandleEvent(event);
			}

			// Esc, with no menu open (an open menu took it: menuWasOpen), closes the panel opened last (L-1)
			// Esc in the field panel (S3d) gives up the edit (the panel did, as it got the key) and takes the keyboard out of the panel; it
			// closes nothing
			if (message == WM_KEYDOWN && wParam == VK_ESCAPE && typingHere)
			{
				fieldsHaveKeys = false;
			}
			else if (message == WM_KEYDOWN && wParam == VK_ESCAPE && !menuWasOpen && !(lParam & (1 << 30)) && host::ClickMode_Cancel())
			{
				// A command was waiting for a click (P4): Esc gives it up, and does nothing else
				statusWords = "Cancelled: nothing was done";
			}
			else if (message == WM_KEYDOWN && wParam == VK_ESCAPE && !menuWasOpen && !(lParam & (1 << 30)))
			{
				CloseTheTopPanel();
			}

			// A key of the layout in use does its action (editor_actions.h), read as the key itself, never as the letter it types. Not while
			// a menu has the keys, not with Alt (or AltGr, Ctrl and Alt) held, and only on the key's first press: a key held down sends
			// WM_KEYDOWN again and again, with bit 30 of lParam saying it was down already. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
			bool screenKeyDone = false;

			// A key a SCREEN of inteng reads itself (screen_keys.h): the screen's own key wins over the editor's, as inteng's handler is the one that reads the key there
			if (message == WM_KEYDOWN && !typingHere && !menuWasOpen && !AMenuIsOpen() && wParam <= 0xFF && !(lParam & (1 << 30)) && !altHeld && !ctrlHeld)
			{
				screenKeyDone = RunScreenKey((uint8_t)wParam, shiftHeld);
				screenKeySwallowChar = screenKeyDone && ((wParam >= 'A' && wParam <= 'Z') || (wParam >= '0' && wParam <= '9')) ? tolower((int)wParam) : 0;
			}

			if (!screenKeyDone && message == WM_KEYDOWN && !typingHere && !menuWasOpen && !AMenuIsOpen() && wParam <= 0xFF && !(lParam & (1 << 30)))
			{
				const EditorAction *const action =
					EditorActions_ForKey(kEditorActions, kEditorActionCount, keyLayout, (uint8_t)wParam, shiftHeld, ctrlHeld, altHeld);

				if (action)
				{
					RunAction(action->id);
				}
			}

			// Hierarchy's Ctrl+R / Ctrl+F: the keys below want no Ctrl, so these are read here, with Ctrl held (HierarchyCtrlKey)
			if (message == WM_KEYDOWN && !typingHere && !menuWasOpen && !AMenuIsOpen() && !altHeld && ctrlHeld)
			{
				HierarchyCtrlKey(wParam);
			}

			// The Texture Map's Ctrl keys (TM-A2): inteng's key table gives them as it gives the others (FUN_00557ea0 0x557ee7; texture_map_screen.cpp TextureMapKeyCommandRun),
			// read here because the keys below want no Ctrl
			if (message == WM_KEYDOWN && !typingHere && !menuWasOpen && !AMenuIsOpen() && !altHeld && ctrlHeld && onTextureMap && !textureChooserOn && wParam <= 0xFF && !(lParam & (1 << 30)))
			{
				TextureMapKeyCommandRun((uint8_t)wParam, shiftHeld, true);
			}

			// The camera's speed keys (above): ] and [ as typed, so they are the ones on the keyboard's own layout; Space; and Classic's Shift+A and
			// Shift+Z, on a key's first press
			if (!typingHere && !menuWasOpen && !AMenuIsOpen() && !altHeld && !ctrlHeld)
			{
				if (message == WM_CHAR)
				{
					if ((wParam == ',' || wParam == '.') && WaypointViewsOn())
					{
						// Waypoint mode: the previous / next waypoint of the list is the current one (ours: inteng picks the box nearest the click in its 3D view)
						if (waypointMode.Mode() == 2)
						{
							// Edit Extents: < zooms out and > zooms in (inteng's help of the screen)
							waypointMode.Zoom(wParam == '.' ? 1 : -1);
						}
						else
						{
							waypointMode.Step(wParam == '.' ? 1 : -1);
						}

						snprintf(threeWords, sizeof(threeWords), "%s", waypointMode.Words().c_str());
						statusWords = threeWords;
					}
					else if ((wParam == ',' || wParam == '.') && OnTheTrackScreen() && trackEd.state != host::TrackState::Off)
					{
						// inteng's < and > step the section's extra spin (FUN_004f2380 / FUN_004f2400 with the section's [+0xb4] -/+ 1.0), which only a section with the raw-turn flag has (FUN_004f2470 does nothing else): not built; Next / Previous Section are Page Down / Page Up
						statusWords = "< and > step a spin section's angle in inteng, and no menu there can make a spin section (unfinished in inteng; the stock levels have none): nothing to step. Page Down / Page Up are Next / Previous Section";
					}
					else if ((wParam == ',' || wParam == '.') && ThreeViewsOn() && trackSectionCount > 0)
					{
						// The current section of the track, stepped (ours: inteng's keys for it are not read yet): the views show its start and end markers
						trackSection += wParam == '.' ? 1 : -1;
						trackDirty = true;
						_snprintf_s(trackWords, sizeof(trackWords), _TRUNCATE, "Section %d of %d (, and . step it: ours, inteng's keys are unread)",
							(int)(((int64_t)trackSection % (int64_t)trackSectionCount + (int64_t)trackSectionCount) % (int64_t)trackSectionCount) + 1, (int)trackSectionCount);
						statusWords = trackWords;
					}
					else if (wParam == ']')
					{
						ChangeTheSpeed(1);
					}
					else if (wParam == '[')
					{
						ChangeTheSpeed(-1);
					}
				}
				else if (message == WM_KEYDOWN && !(lParam & (1 << 30)) && !screenKeyDone)   // a key a screen did (screen_keys.h) is not also the editor's
				{
					bool multiTookKey = false;   // Copy Texturing > multiple took this key (T and P at its choice, Esc): the mode's own T (wrap) and the others do not hear it

					if (onTextureMap && textureMultiMode != 0 && (wParam == VK_ESCAPE || (textureMultiMode == 5 && (wParam == 'T' || wParam == 'P'))))
					{
						multiTookKey = true;
						if (wParam == VK_ESCAPE)
						{
							textureMultiMode = 0;
							statusWords = "Texture Map: copying multiple polygons was cancelled";
						}
						else
						{
							TextureMultiDo(wParam == 'P');
						}
					}
					else if (onTextureMap && textureChooserOn && wParam == VK_ESCAPE)
					{
						textureChooserOn = false;
						statusWords = "Texture Map: the texture chooser was cancelled";
					}
					else if (onTextureMap && textureChooserOn)
					{
						// the chooser keeps the keys (its own are not built)
					}
					else if (onTextureMap && wParam == 'S')
					{
						textureSHeld = true;
					}

					if (multiTookKey)
					{
						// done above
					}
					else if (onTextureMap && AnimPlayStop())
					{
						// the key only ended the play
					}
					else if (onTextureMap && !textureChooserOn && !textureAnimPick && !textureAnimCopyPick && !textureCopyObject && !textureCopyPending && !textureInvertPending &&
						!(textureSHeld && (wParam == VK_UP || wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_DOWN)) && TextureMapKeyCommandRun((uint8_t)wParam, shiftHeld, false))
					{
						// inteng's key table took the key (FUN_00557ea0; texture_map_screen.cpp TextureMapKeyCommandRun): the X / Y / E / R, G, U, T, C and L branches below are only
						// reached by keys it gives no command
					}
					else if (HierarchyKey(wParam))
					{
						// the Hierarchy view's keys (Space, comma, period, Home, End)
					}
					else if (PlacementKey(wParam, shiftHeld))
					{
						// Object Placement's keys (inteng's help)
					}
					else if (wParam == VK_INSERT && !onTextureMap && (!OnStrategyScreen() || StrategyInASubLoop()))
					{
						ZoomStart();   // the Movement help's Insert
					}
					else if (wParam == VK_HOME && !onTextureMap && (!OnStrategyScreen() || StrategyInASubLoop()))
					{
						ZoomReset();   // the Movement help's Home: back to the default zoom level
						statusWords = "The zoom is back to the default level";
					}
					else if (wParam == VK_SCROLL && !(lParam & 0x40000000) && !onTextureMap && !(OnStrategyScreen() && (StrategyInASubLoop() || strategyEditFan)) && !volume.on && zoomStage == 0 &&
						!host::ClickMode_Waiting() && !moveRelWaiting && !snapLive && !AutoLinkShow_On())
					{
						// nor while another of inteng's modal waits holds the frame (Insert's zoom FUN_00471cb0, a click-mode's or Move rel. to poly's wait, the live Snap To Vertex, AutoLink's run): WinMain
						// does not get round to 0x58d004 until it ends (a review of the camera close-out, 11-17)
						// Scroll Lock (WinMain 0x58d004-0x58d00d, after every screen's frame): the rotation centre's pick waits for its press (FUN_004720c0, CAM-8). Not where inteng's frame is inside a
						// loop of its own and never gets back to WinMain: Strategy Data's flag page, Stability, the collectors, the pick loop and the volume editor (FUN_00529370's loops) and a kind's Edit
						// view (FUN_0052cbb0's wait). TO CHECK: Texture Map (its menu's "Misc > Screenshot (Scroll Lock)"; the mode is paused), left out until read. NAMED: inteng runs it each frame the
						// key is HELD (a pick made with the key still down starts another); ours, once a press
						rotationPickWaiting = true;
					}
					else if (zoomStage != 0 && wParam == VK_ESCAPE)
					{
						ZoomCancel();   // "(escape cancels process)"
						statusWords = "Zoom cancelled";
					}
					else if (onTextureMap && textureWindowOn && (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN))
					{
						float const step = shiftHeld ? 10.0f : 1.0f;

						TextureWindowMoveNode(textureWinNode, wParam == VK_LEFT ? -step : wParam == VK_RIGHT ? step : 0.0f, wParam == VK_UP ? -step : wParam == VK_DOWN ? step : 0.0f, ctrlHeld || textureWinAllNodes);
						TextureWindowApply();
					}
					else if (onTextureMap && textureWindowOn && (wParam == VK_OEM_PLUS || wParam == VK_OEM_MINUS))
					{
						TextureWindowScale(wParam == VK_OEM_PLUS ? 0xf1 : 0xf3);   // ours, kept by the owner's ruling (board 17:52): = / - step the scale as 9 / 7 do
					}
					else if (onTextureMap && textureWindowOn && wParam == VK_ESCAPE)
					{
						TextureWindowToggle();
					}
					else if (onTextureMap && !textureWindowOn && textureSHeld && (wParam == VK_UP || wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_DOWN))
					{
						// S and an arrow key: arrow selection
						textureArrowAsked = wParam == VK_UP ? 1 : wParam == VK_LEFT ? 2 : wParam == VK_RIGHT ? 3 : 4;
					}
					else if (onTextureMap && (textureAnimPick || textureAnimCopyPick || textureCopyObject) && wParam == VK_ESCAPE)
					{
						textureAnimPick = false;
						textureAnimCopyPick = false;
						textureCopyObject = 0;
						statusWords = "Anim: cancelled";
					}
					else if (onTextureMap && textureCopyPending && wParam == VK_ESCAPE)
					{
						textureCopyPending = false;
						statusWords = "Texture Map: copy texturing cancelled";
					}
					else if (onTextureMap && wParam == VK_ESCAPE && TextureAlignCancel()) {}   // Texture Alignment's click stopped (texture_shared.h)
					else if (onTextureMap && textureInvertPending && (wParam == 'A' || wParam == VK_ESCAPE))
					{
						textureInvertPending = false;

						if (wParam == 'A')
						{
							OpenLevelFile *const open = LevelObjects_OpenForEdit();

							if (open && open->Level())
							{
								TextureInvert(open, nullptr, false, -1, true);
							}
						}
						else
						{
							statusWords = "Texture Map: invert cancelled";
						}
					}
					else if (onTextureMap && (wParam == VK_NEXT || wParam == VK_PRIOR))
					{
						// Next Texture / Prev Texture (inteng's items 0xa and 0xb; the keys are ours)
						TextureMapStepPicture(wParam == VK_NEXT ? 1 : -1);
					}
					else if (wParam >= VK_F1 && wParam <= VK_F12)
					{
						CameraKey((int)(wParam - VK_F1) + 1, shiftHeld);
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && wParam == 'M' && !shiftHeld && !strategyFlagPage)
					{
						// page 0 (FUN_00522980 / FUN_00524e90) reads no M (strategy-data.md S-58): nothing
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && wParam == 'M' && !shiftHeld)
					{
						// the Movement help's "M: Select all modules": every module object joins the selection set (at most the set's 256: named)
						std::vector<host::LevelObject> rows;

						LevelObjects_Read(&rows);
						host::Selection_Clear();

						size_t picked = 0;
						size_t modules = 0;

						for (host::LevelObject const &row : rows)
						{
							int32_t index = 0;

							if (host::ModuleIndexOfId(LevelObjects_Level(), row.id, &index))
							{
								modules++;

								host::Selected one = {};

								one.kind = host::SelectedKind::Object;
								memcpy(one.objectId, row.id, host::kSelectionIdBytes);
								picked += host::Selection_Toggle(one) ? 1 : 0;
							}
						}

						cameraWords = "Selected " + std::to_string((unsigned long long)picked) + " of the level's " + std::to_string((unsigned long long)modules) + " modules" + (picked < modules ? " (the selection holds at most 256)" : "");
						statusWords = cameraWords.c_str();
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Path") == 0 && (wParam == 'L' || wParam == 'P' || (wParam == 'D' && keyLayout == KeyLayout::Classic)))
					{
						// Path mode's keys (FUN_004f9d10: slot 0xd87a8c L = Add selected module to path, 0xd87a84 D = Delete last module, 0xd87a90 P = Properties); D is a movement key in the Modern layout
						PathModeCommand(wParam == 'L' ? 3 : (wParam == 'D' ? 4 : 9));
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && wParam == 'V' && !shiftHeld)
					{
						// the Edit Strategy dialog's "V : Edit Volume" (inteng's string), as a key on the Strategy screen: the same click pick as the item. (S-59, Vasquez 11-17: T no longer does the
						// same with "T : Change Target": inteng reads T only inside the switch kinds' Edit views, which take it before this (52 / 61 / 66 / 68), and does nothing with it on page 0)
						StrategyModeCommand(0x101);
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && wParam == 'E' && (strategyFlagPage || stabilityActive))
					{
						// the flag page (FUN_00525700) and Stability (FUN_00527370) are loops of their own: neither reads E's slot 0xd87a85, and the one reader of it on the screen, page 0's dispatcher
						// FUN_00524e90, is not called while they run (only page 0's frame FUN_00522980 calls it, 0x522a0c): nothing (strategy-data.md S-52)
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && wParam == 'E')
					{
						// Strategy mode's E (FUN_00524e90 0x524ee7: slot 0xd87a85 makes the command 7): Edit Strategy. Shift too: the slot is set whatever Shift is (FUN_0058c010 0x58c182; S-52, Vasquez 11-18)
						StrategyModeCommand(7);
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && wParam == VK_TAB && strategyFromPlacement)
					{
						// Tab in Strategy mode entered from Object Placement: "Press Tab for Object Placement" (0x5eddac), the same as Exit (FUN_00524e90 0x524eca)
						ExitTheMode();
					}
					else if (barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Object Placement") == 0 && wParam == VK_TAB)
					{
						// Object Placement's Tab: "Enter Strategy Mode" (inteng's help): Strategy mode, which Exit and Tab give back Object Placement
						size_t const strategyScreen = ScreenOfName("Strategy Data");

						strategyFromPlacement = true;
						ShowTheScreen(strategyScreen);
						statusWords = "Strategy Data: Tab or Exit goes back to Object Placement";
					}
					else if (OnTheTrackScreen() && trackEd.state == host::TrackState::EndEdit && wParam == 'V' && !shiftHeld)
					{
						// V in the end edit: Snap To Vertex, live, on the flying marker (T-3b); with the marker not flying inteng does nothing
						if (!TrackSnapLiveStart())
						{
							statusWords = "Snap To Vertex: the end marker is not flying (F flies it)";
						}
					}
					else if (OnTheTrackScreen() && wParam == 'T' && shiftHeld)
					{
						UpdateTrackPositions();   // Shift+T in Track Editing too (FUN_004efd50): the edited track first, then back to Object Placement (TB-T2)
					}
					else if (OnTheTrackScreen() && trackEd.state != host::TrackState::Off && wParam == 'O')
					{
						trackShowStart = !trackShowStart;
						trackDirty = true;
						statusWords = trackShowStart ? "The section's start position is shown" : "The section's start position is hidden";
					}
					else if (OnTheTrackScreen() && trackEd.state == host::TrackState::EndEdit && wParam == 'F' && !shiftHeld)
					{
						if (trackFly)
						{
							trackFly = false;
							flyRelAxes = false;
							flyIncBox = false;
							statusWords = "Fly end marker: off, the marker is where it flew to (Finish End Pos Edit takes it)";
						}
						else if (theCamera)
						{
							double const scale = local_scale > 0.0 ? local_scale : 1.0;

							trackFlyOffset[0] = (int32_t)floor((double)threeCursor[0] * scale + 0.5) - theCamera->x;
							trackFlyOffset[1] = (int32_t)floor((double)threeCursor[1] * scale + 0.5) - theCamera->y;
							trackFlyOffset[2] = (int32_t)floor((double)threeCursor[2] * scale + 0.5) - theCamera->z;
							trackFlyBase[0] = theCamera->turn;
							trackFlyBase[1] = theCamera->look;
							trackFlyBase[2] = theCamera->roll;
							trackEd.markerTurn[0] = trackEd.markerTurn[1] = trackEd.markerTurn[2] = 0.0;
							trackFly = true;
							statusWords = "Fly end marker: on (the marker moves and turns with the camera: the movement and turn keys; F again lets it go; X and B switch Move Rel to Axes and Inc Bounding Box)";
						}
					}
					else if (trackFly && (wParam == 'X' || wParam == 'B') && !shiftHeld)
					{
						FlySwitchKey(wParam == 'X');
					}
					else if (OnTheTrackScreen() && trackEd.state != host::TrackState::Off && wParam == 'P')
					{
						// CAM-4: while the marker flies, the turn so far is kept and the swing about the pivot counts from here (inteng flips DAT_00914afc with it, 0x4dc8bd, so the next frames turn
						// the other way; the place the marker has stays)
						float base[4], turned[4];

						if (trackFly && trackEd.state == host::TrackState::EndEdit && host::TrackEdit_MarkerBase(trackEd, base) &&
							host::TrackPlay_TurnedBy(base, trackEd.markerTurn[0], trackEd.markerTurn[1], trackEd.markerTurn[2], turned))
						{
							int32_t const at[3] = { threeCursor[0], threeCursor[1], threeCursor[2] };

							TrackMarkerSnapPut(at, turned, true);
						}

						trackTurnAboutPivot = !trackTurnAboutPivot;
						trackEd.markerAboutPivot = trackTurnAboutPivot;
						statusWords = trackTurnAboutPivot ? "The marker's turn is about the pivot" : "The marker's turn is about the centre (the object stays at its end place)";
					}
					else if (OnTheTrackScreen() && trackEd.state != host::TrackState::Off && (wParam == VK_NEXT || wParam == VK_PRIOR))
					{
						TrackEditCommand(wParam == VK_NEXT ? 0x519 : 0x51a);   // Next / Previous Section: Page Down / Page Up here (inteng's < and > step the section's spin)
					}
					else if (wParam == 'F' && strategyViewBit != 0 && barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0)
					{
						// with a flag in view, F does the flag's Add on the picked modules, Shift+F its Remove (FUN_00525700 0x525779: the slot 0xd87a86 is F, 0xd87a5e the Shift-like slot)
						StrategyModeCommand(shiftHeld ? strategyViewAdd + 1 : strategyViewAdd);
					}
					else if (wParam == 'L' && !shiftHeld && OnStrategyScreen() && !strategyFlagPage)
					{
						// Strategy Data's page 0 (and a kind's view) reads no L (strategy-data.md S-58): nothing
					}
					else if (wParam == 'L' && OnAcousticScreen())
					{
						// Acoustic Mode's L (slot 0xd87a8c, FUN_004f57b0 0x4f5a86): every pick let go of, its red cleared
						AcousticLeave();
					}
					else if (wParam == 'G' && barScreen == host::kIntengHub && SmallModuleStep(shiftHeld))
					{
						// the hub's G / Shift+G (slots 0xd87a87 / 0xd87a5e, FUN_004452b0 0x446b11-0x446c38): Find Small Modules' next / previous one (TB-C4)
					}
					else if (wParam == 'R' && barScreen == host::kIntengHub)
					{
						// the hub's R (slot 0xd87a92, with Shift too: the slot is the key's): the first selected object's shape a quarter turn (TB-C5)
						TurnTheSelectedQuarter();
					}
					else if (wParam == 'L' && !shiftHeld)
					{
						// inteng's "L: Deselect all objects"; with Module Overlaps on the hub works every module against every other again (0x446acc -> FUN_00445070, TB-C11)
						host::Selection_Clear();
						statusWords = "Deselected all objects";

						if (barScreen == host::kIntengHub && moduleOverlapsOn)
						{
							ModuleOverlapsAll();
						}
					}
				}
			}

			// No window was open when this key came (an open one took the key above): one is open now, so this key opened it (Object Placement's E opens Sound Edit, FUN_004d9af0 -> 0x4ed060)
			if (message == WM_KEYDOWN && ADialogIsOpen() && ((wParam >= 'A' && wParam <= 'Z') || (wParam >= '0' && wParam <= '9')))
			{
				dialogKeySwallowChar = tolower((int)wParam);
			}

			// The camera's keys are held only while no menu is open
			// (in Texture Map the mode's own keys win over the camera's: texture_keys.h)
			if (message == WM_KEYDOWN && !typingHere && !AMenuIsOpen() && !(onTextureMap && host::TextureMapKeys_OwnInSubMode((uint8_t)(wParam & 0xFF), textureSubMode)))
			{
				keyDown[wParam & 0xFF] = true;
			}

			// Windows' own system keys go on to it: Alt+F4 closes the window, and Alt+Space opens its menu for the window
			if (message == WM_SYSKEYDOWN)
			{
				break;
			}
			return 0;
		}
		case WM_LBUTTONDOWN:
		{
			// Scroll Lock's pick waiting takes the press before anything else (CAM-8)
			if (RotationPickButton(window, lParam, false))
			{
				return 0;
			}

			if (textureColourPicking) { POINT const at = InThePanels(window, lParam); TextureColourPickPress(at.x, at.y, true); return 0; }   // Select Colour's grid (Shift+K, FUN_00560620)

			// Module Linking's Show All is let go by a mouse button (_DAT_00d8806a, 0x4c1aff); the press then goes on as a click (0x4c36a8) (row ML-M-3)
			if (showAllHeld && OnModuleLinkingScreen())
			{
				ShowAllRelease();
			}

			// the texture chooser takes a press: the cell under it is the texture
			if (onTextureMap && textureChooserOn)
			{
				POINT const chosen = InThePanels(window, lParam);

				TextureChooserClick(chosen.x, chosen.y);
				return 0;
			}

			// a click ends an animation that is playing, and does nothing else (FUN_00541b70 is called on a press)
			if (onTextureMap && AnimPlayStop())
			{
				return 0;
			}

			// A help text over the level is closed by a press, which does nothing else
			if (textPanelText)
			{
				textPanelText = textPanelNext;
				textPanelNext = nullptr;
				return 0;
			}

			// inteng's message / confirm / text / list dialogs (dialog_box.h, list_picker.h) take every press
			if (ADialogIsOpen())
			{
				POINT const press = InThePanels(window, lParam);

				DialogPress(press.x, press.y);
				return 0;
			}

			if (promptFor != kPromptNone)
			{
				if (!formRows.empty())
				{
					POINT const press = InThePanels(window, lParam);

					FormClick(hitPrompt, press.x, press.y, kPanelPassLineHeight + 2);
				}

				return 0;
			}

			// A zoom waiting (Insert): the press is its centre, then its zoom in
			if (zoomStage != 0)
			{
				POINT const zoomAt = InThePanels(window, lParam);

				ZoomPress(zoomAt.x, zoomAt.y, false);
				return 0;
			}

			POINT const at = InThePanels(window, lParam);

			// An open pop-up has this press: on one of its items it picks it; anywhere else it closes, and the press does nothing more
			if (popupBar.IsOpen())
			{
				ui::MenuAnswer const popAnswer = popupBar.Press(at.x, at.y);

				if (popAnswer == ui::MenuAnswer::Picked)
				{
					PopupPicked(popupBar.Picked());
				}

				return 0;
			}

			// Two bars: a press on one bar's strip while the other has a menu open closes that one first, then the press is the strip's own; a press anywhere else goes to the bar
			// with a menu open (which closes or picks), else to the first
			ui::MenuBar *bar = &menuBar;

			if (barRowCount > 1)
			{
				// the row whose strip the press is on, if any
				int onRow = -1;

				for (size_t row = 0; row < barRowCount; row++)
				{
					if (ui::BoxHolds(row == 0 ? hitBar : hitLower[row - 1], at.x, at.y))
					{
						onRow = (int)row;
					}
				}

				// a press on a row's strip closes a menu open on any other row first
				if (onRow >= 0)
				{
					for (size_t row = 0; row < barRowCount; row++)
					{
						if ((int)row != onRow && BarRow(row).IsOpen())
						{
							BarRow(row).Handle(ui::KeyEvent(ui::Key::Escape));
						}
					}

					bar = &BarRow((size_t)onRow);
				}
				else
				{
					for (size_t row = 0; row < barRowCount; row++)
					{
						if (BarRow(row).IsOpen())
						{
							bar = &BarRow(row);
							break;
						}
					}
				}
			}

			ui::MenuAnswer const answer = barVisible || bar->IsOpen() ? bar->Press(at.x, at.y) : ui::MenuAnswer::Nothing;
			AfterMenu(answer, *bar);

			// A press on a button (A8) does what its menu item does; on one that's off it says so
			if (answer == ui::MenuAnswer::Nothing)
			{
				int const tool = host::ToolButtons_At(toolBoxes, toolPlaced, at.x, at.y);
				int const panel = tool >= 0 ? -1 : host::ToolButtons_At(panelBoxes, panelPlaced, at.x, at.y);

				if (tool >= 0 || panel >= 0)
				{
					host::ToolButton const &pressed = tool >= 0 ? toolButtons[tool] : panelButtons[panel];

					fieldsHaveKeys = false;

					if (pressed.off)
					{
						statusWords = "That button is off at the moment (its menu item is off, or nothing is selected)";
					}
					else
					{
						RunAction(pressed.action);
					}

					return 0;
				}
			}

			// A press the menu bar took nothing from goes to the panels first (L-1: they are over the level): on a row of the object list it
			// selects that row's object, even when the row was chosen already, as the selection may be something else by now (Added
			// 2026-11-14 by Melanikus (Dietrich), F3); on any other panel's gray it does nothing. Only a press outside every panel reaches the
			// level, where it is a pick, in the view's own pixels (Added 2026-11-14 by Melanikus (Frost)).
			if (answer == ui::MenuAnswer::Nothing)
			{
				bool onRow = false;

				// A press anywhere takes the keyboard out of the field panel; one on a field of it puts it back in, on that field (S3d)
				fieldsHaveKeys = false;

				if (listOpenedAt && objectList.Press(at.x, at.y, &onRow))
				{
					if (onRow)
					{
						SelectTheChosenObject();
					}
				}
				else if (fieldsOpenedAt && fieldHost.Showing() && ui::BoxHolds(hitFields, at.x, at.y))
				{
					int32_t const field = fieldHost.FieldAt(at.x, at.y);

					if (field >= 0)
					{
						int32_t const before = fieldHost.FocusedField();

						fieldsHaveKeys = true;
						ThePanelAnswered(fieldHost.ClickField((size_t)field), before);
					}
				}
				else if (!OnAPanel(at.x, at.y) && ThreeViewsOn() && threeViewsFitted && threeBox.width >= 64 && threeBox.height >= 48
					&& ui::BoxHolds(threeBox, at.x, at.y)
					&& host::ThreeView_Click(threeViews, (at.x - threeBox.x) * 640 / threeBox.width, (at.y - threeBox.y) * 480 / threeBox.height, threeCursor))
				{
					// A click in one of the three views sets the shared 3D cursor, and is no pick
					_snprintf_s(threeWords, sizeof(threeWords), _TRUNCATE, "Cursor: x %d, y %d, z %d", (int)threeCursor[0], (int)threeCursor[1], (int)threeCursor[2]);
					statusWords = threeWords;

					// Edit Extents (FUN_00585750): the press sizes a new box, sets the aim point (Centre Edit) or takes a face of the current box (waypoint_mode.h BeginDrag); a press near no face
					// takes nothing (inteng picks a waypoint only in the 3D quarter: WaypointBoxClick)
					if (WaypointViewsOn())
					{
						int32_t const sx = (at.x - threeBox.x) * 640 / threeBox.width;
						int32_t const sy = (at.y - threeBox.y) * 480 / threeBox.height;
						int const mask = sy < 240 ? (sx < 320 ? 3 : 6) : (sx < 320 ? 5 : 0);   // top x and y, side z and y, front x and z; the 3D quarter picks nothing
						int32_t file[3];

						host::WaypointView_FileUnits(threeCursor, local_scale, file);

						// a press within 3 pixels of a face of the current waypoint's box takes that face (the drag is MOUSEMOVE's, the release writes it: FUN_00585750); else it picks
						int32_t const pixelFile = threeViews.top.scaleX > 0 ? (int32_t)(threeViews.top.scaleX / (local_scale > 0.0 ? local_scale : 1.0)) : 1;

						if (mask != 0 && waypointMode.BeginDrag(file, mask, pixelFile > 0 ? pixelFile : 1))
						{
							waypointDragMask = mask;
							snprintf(threeWords, sizeof(threeWords), "Dragging a face of waypoint %d: let go to keep it", (int)waypointMode.CurrentWaypoint());
							statusWords = threeWords;
						}
					}
				}
				else if (!OnAPanel(at.x, at.y) && threeMainBox.width > 0 && ui::BoxHolds(threeBox, at.x, at.y) && !ui::BoxHolds(threeMainBox, at.x, at.y))
				{
					// The middle lines of the three views belong to none of them: no pick (inteng's handler sets nothing there either)
				}
				else if (!OnAPanel(at.x, at.y) && window == hWndMain && onTextureMap && textureWindowOn)
				{
					// the texture window has the press: a node near it is taken, and nothing else is picked
					TextureWindowPress(at.x, at.y, textureWinFrameW, textureWinFrameH);
				}
				else if (!OnAPanel(at.x, at.y) && window == hWndMain && onTextureMap && (textureSelectMode == 2 || textureSelectMode == 3) && !texturePlaneMode && !textureInvertPending)
				{
					// Rect (In or On) / Rect (In): the press is the rectangle's first corner (FUN_005590c0); the release picks
					textureRectActive = true;
					textureRectShift = ShiftDown() || (wParam & MK_SHIFT) != 0;
					textureRectX0 = textureRectX1 = GET_X_LPARAM(lParam);
					textureRectY0 = textureRectY1 = GET_Y_LPARAM(lParam);
				}
				else if (!OnAPanel(at.x, at.y) && window == hWndMain)
				{
					pickAsked = true;
					pickShift = ShiftDown() || (wParam & MK_SHIFT) != 0;
					pickCtrl = KeyHeldNow(VK_CONTROL) || (wParam & MK_CONTROL) != 0;
					pickDelete = KeyHeldNow(VK_DELETE);   // Module Linking: Delete + click takes a module off the picks (slot 0x55 = VK 0x2e, FUN_004c3880)
					pickWithS = keyLayout == KeyLayout::Classic && keyDown['S'] && barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Strategy Data") == 0 && strategyFlagPage && !pickShift;   // S + click is the flag page's (FUN_00525700 0x525db2 / 0x525dbf), but Shift is tested first (0x525cba-0x525cc1: Shift down skips the S read): Shift + S + click is Shift + click (0x525cc7, out of the list, red off 0x525da8; S-69); page 0 takes it as a plain click (S-58)
					pickX = GET_X_LPARAM(lParam);
					pickY = GET_Y_LPARAM(lParam);
					pickInBox = threeMainBox.width > 0 && ui::BoxHolds(threeMainBox, at.x, at.y);

					// With the three views shown, the 3D view is the bottom right quarter: a pick there is from the quarter's own pixels
					if (pickInBox)
					{
						pickX = at.x - threeMainBox.x;
						pickY = at.y - threeMainBox.y;
					}
				}
			}
			return 0;
		}
		case WM_LBUTTONUP:
		{
			// Scroll Lock's pick waiting: the release is the wait's own (its press was), and a miss now waits for a new press (0x472119)
			if (rotationPickWaiting)
			{
				rotationPickButtonHeld = false;
				return 0;
			}

			if (textureWinDrag)
			{
				POINT const dropped = InThePanels(window, lParam);

				TextureWindowDragTo(dropped.x, dropped.y, textureWinFrameW, textureWinFrameH);
				textureWinDrag = false;
				TextureWindowApply();
			}

			if (textureRectActive)
			{
				// the other corner: the drag is let go and the frame selects with it
				textureRectActive = false;
				textureRectX1 = GET_X_LPARAM(lParam);
				textureRectY1 = GET_Y_LPARAM(lParam);
				textureRectAsked = true;
			}

			// The button let go: a press on a menu's title, a drag and a release on an item picks it, as Windows' own menus do
			// (MenuBar::Release). (Added 2026-11-14 by Melanikus (Dietrich), F3.)
			POINT const at = InThePanels(window, lParam);

			AfterMenu(menuBar.Release(at.x, at.y), menuBar);

			for (size_t row = 1; row < barRowCount; row++)
			{
				AfterMenu(lowerBar[row - 1].Release(at.x, at.y), lowerBar[row - 1]);
			}

			// A face of a waypoint's box that is being dragged is let go of: each axis put in order, written (Waypoint mode, FUN_00585750)
			if (waypointMode.Dragging())
			{
				host::WaypointContext context;

				context.open = LevelObjects_OpenForEdit();
				context.pickedModule = -1;

				host::WaypointOutcome const done = waypointMode.EndDrag(context);

				waypointDragMask = 0;

				if (done.levelChanged)
				{
					LoadTheObjectListAgain();
				}

				if (!done.say.empty())
				{
					snprintf(threeWords, sizeof(threeWords), "%s", done.say.c_str());
					statusWords = threeWords;
				}
			}

			return 0;
		}
		case WM_KEYUP:
			keyDown[wParam & 0xFF] = false;

			if (wParam == 'S')
			{
				textureSHeld = false;
			}
			return 0;
		case WM_SYSKEYUP:
			// A key let go of while Alt is down comes as WM_SYSKEYUP, not WM_KEYUP: without this it stayed held in the table (Ctrl let go under Alt, then a plain B ran
			// Ctrl+B, the Headlight: ML-M-76 of module-linking-map-dietrich.md). inteng reads its key table from the keys' real state, so a key let go is up. The message
			// still goes on to DefWindowProc, as before. (Added 2026-11-17 by Melanikus (Dietrich).)
			keyDown[wParam & 0xFF] = false;

			if (wParam == 'S')
			{
				textureSHeld = false;
			}
			break;
		case WM_ACTIVATEAPP:
			host::ModuleLinkAvp_AppActive(wParam != 0);   // Link with AVP (0x58ba6f-0x58bc26, module_link_avp.h)
			return 0;
		case WM_KILLFOCUS:
			// A key let go of while another window has the keyboard never reaches this one, so every key counts as up
			memset(keyDown, 0, sizeof(keyDown));
			rightButtonDown = false;
			return 0;
		case WM_MBUTTONDOWN:
			// Scroll Lock's pick waiting: the middle button picks too (bit 4 is not the cancel's bit 2)
			if (RotationPickButton(window, lParam, false))
			{
				return 0;
			}

			break;
		case WM_MBUTTONUP:
			if (rotationPickWaiting)
			{
				rotationPickButtonHeld = false;
				return 0;
			}

			break;
		case WM_RBUTTONDOWN:
			// Scroll Lock's pick waiting takes the press before anything else: the right button gives up (CAM-8)
			if (RotationPickButton(window, lParam, true))
			{
				return 0;
			}

			if (textureColourPicking) { POINT const at = InThePanels(window, lParam); TextureColourPickPress(at.x, at.y, false); return 0; }   // Select Colour's grid: not a colour

			// while AutoLink runs, a right press cancels it (inteng: DAT_00d8806a & 2 at the top of each record, 0x4c899c; AD-4) and does nothing else: no pop-up
			if (AutoLinkShow_On())
			{
				AutoLinkShow_RightPress();
				return 0;
			}

			// the texture windows take a right press on them: the right drag turns or scales the nodes (FUN_005557a0, texture_map_screen.cpp)
			if (onTextureMap && textureWindowOn && window == hWndMain) { POINT const pressed = InThePanels(window, lParam); if (TextureWindowRightPress(pressed.x, pressed.y, textureWinFrameW, textureWinFrameH)) { rightButtonDown = false; return 0; } }

			// Module Linking's Show All is let go by a mouse button, the right one too (row ML-M-3)
			if (showAllHeld && OnModuleLinkingScreen())
			{
				ShowAllRelease();
			}

			// an open dialog takes the right press too: a list's right-click handler (the sound chooser's listen), else nothing
			if (ADialogIsOpen())
			{
				POINT const press = InThePanels(window, lParam);

				host::Dialogs_RightPress(press.x, press.y);
				return 0;
			}

			// A zoom waiting: any button chooses the centre, the right one then zooms out. Its loop is modal (FUN_00471cb0 reads only the mouse and Escape, and clears the mouse word at its end,
			// 0x471e48 / 0x4720ad), so no end test of a loop under it sees the press: not the volume editor's, not an Edit view's (the volume editor runs from a kind's view), not a pick's or a collector's
			if (zoomStage != 0)
			{
				POINT const zoomAt = InThePanels(window, lParam);

				ZoomPress(zoomAt.x, zoomAt.y, true);
				return 0;
			}

			if (volume.on)
			{
				VolumeEnd();   // the right button ends the volume editor (_DAT_00d8806a & 2)
				return 0;
			}

			// Edit Extents' Shift + right drag in one of the three views moves them all (FUN_00585750 0x585bc4-0x5861c8; inteng reads no menu while Shift is down, so no pop-up)
			if (WaypointViewsOn() && (ShiftDown() || (wParam & MK_SHIFT) != 0) && threeBox.width >= 64 && threeBox.height >= 48)
			{
				POINT const press = InThePanels(window, lParam);

				if (ui::BoxHolds(threeBox, press.x, press.y))
				{
					int32_t const sx = (press.x - threeBox.x) * 640 / threeBox.width;
					int32_t const sy = (press.y - threeBox.y) * 480 / threeBox.height;

					waypointPanning = true;
					waypointPanPane = sx / 320 + (sy / 240) * 2;   // DAT_0091bf18
					waypointPanAt[0] = sx;
					waypointPanAt[1] = sy;
					return 0;
				}
			}

			// an Edit view ends with the RIGHT button too (_DAT_00d8806a & 2, e.g. FUN_0052cbb0 0x52ce16; S-KC-3): it opens no pop-up
			if (strategyEditFan && OnStrategyScreen() && !ADialogIsOpen())
			{
				if (requestPageOn)
				{
					RequestPageEnd(false);   // E's page: the right button ends it (every page clears the mouse word at its end), the view goes on
				}
				else if (copyCollecting)
				{
					CopyCollectEnd();   // the Lift's positions collector (FUN_00526bf0): the right button ends it, the view goes on
				}
				else if (!strategyPickField.empty())
				{
					StrategyPickEnd();   // the pick ends on the right button (FUN_00527540 0x52764e), the view goes on
				}
				else if ((strategyEditKind == 72 || strategyEditKind == 80 || strategyEditKind == 76) && trackPage >= 0)
				{
					TrackPageBack();   // a point page (a Hierarchy Object's flag page, 0x533f66): back to the main page (0x534d29), the view goes on
				}
				else
				{
					StrategyViewEnd();
				}

				return 0;
			}

			if (stabilityActive && OnStrategyScreen())
			{
				StabilityEnd();   // the right button ends it too (_DAT_00d8806a & 2)
				return 0;
			}

			if (copyCollecting && OnStrategyScreen())
			{
				CopyCollectEnd();   // the right button ends the collector too (_DAT_00d8806a & 2)
				return 0;
			}

			// With a pop-up open, a right press elsewhere closes it (a press on one of its items picks it)
			if (popupBar.IsOpen())
			{
				POINT const at = InThePanels(window, lParam);

				if (popupBar.Press(at.x, at.y) == ui::MenuAnswer::Picked)
				{
					PopupPicked(popupBar.Picked());
				}

				return 0;
			}

			rightButtonDown = true;
			rightDragged = false;
			rightPressedAt = InThePanels(window, lParam);
			rightRawX = GET_X_LPARAM(lParam);
			rightRawY = GET_Y_LPARAM(lParam);
			lastMouseX = GET_X_LPARAM(lParam);
			lastMouseY = GET_Y_LPARAM(lParam);
			return 0;
		case WM_RBUTTONUP:
			if (textureWinRightDrag) { POINT const let = InThePanels(window, lParam); TextureWindowRightDragTo(let.x, let.y, textureWinFrameW, textureWinFrameH, (wParam & MK_CONTROL) != 0); TextureWindowRightRelease(textureWinFrameW, textureWinFrameH); return 0; }   // the texture windows' right drag ends

			// Edit Extents' Shift + right drag ends with the button (DAT_0091bf08 = 0, 0x585be1)
			if (waypointPanning)
			{
				waypointPanning = false;
				return 0;
			}

			// Move rel. to poly waiting for its click: the right button gives it up (inteng, FUN_00564b60)
			if (rightButtonDown && !rightDragged && moveRelWaiting && host::ClickMode_Waiting())
			{
				host::ClickMode_Cancel();
				moveRelWaiting = false;
				statusWords = "Move rel. to poly: given up";
				rightButtonDown = false;
				return 0;
			}

			// Q waiting for the generator's parent: a RIGHT press ends it with the parent as it was (OP-90, FUN_004e7100 0x4e7177 / 0x4e7184)
			if (rightButtonDown && !rightDragged && parentWaiting && host::ClickMode_Waiting())
			{
				host::ClickMode_Cancel();
				parentWaiting = false;
				statusWords = "Click on parent for generator: given up, the parent is as it was";
				rightButtonDown = false;
				return 0;
			}

			// A right click that didn't drag, on a screen with a second menu table, opens it at the pointer (inteng's pop-up)
			if (rightButtonDown && !rightDragged && !menuBar.IsOpen() && !ALowerMenuIsOpen() && promptFor == kPromptNone && !textPanelText
				&& EditorBar_MakePopup(barScreen, &popupRoom) && popupBar.Load(popupRoom.menus, popupRoom.menuCount))
			{
				popupAt = rightPressedAt;
				popupBar.Handle(ui::KeyEvent(ui::Key::Menu));
				memset(keyDown, 0, sizeof(keyDown));
			}

			rightButtonDown = false;
			return 0;
		case WM_MOUSEMOVE:
			// Scroll Lock's pick waiting with the left or middle button still held: a press that hit nothing picks again where the pointer is now (FUN_004720c0 0x472166 jumps back with the button
			// bits kept, and the pointer is read again each time round, so dragging onto an object picks it); nothing else follows the pointer meanwhile
			if (rotationPickWaiting)
			{
				if ((wParam & (MK_LBUTTON | MK_MBUTTON)) && !(wParam & MK_RBUTTON) && !KeyHeldNow(VK_ESCAPE))
				{
					RotationPickPress(window, lParam);
				}

				return 0;
			}

			if (onTextureMap && textureChooserOn)
			{
				POINT const over = InThePanels(window, lParam);

				textureChooserMouseX = over.x;
				textureChooserMouseY = over.y;
			}

			if (snapLive && window == hWndMain)
			{
				snapPointerX = GET_X_LPARAM(lParam);
				snapPointerY = GET_Y_LPARAM(lParam);
			}

			if (OnAcousticScreen() && window == hWndMain)
			{
				acousticPointerX = GET_X_LPARAM(lParam);
				acousticPointerY = GET_Y_LPARAM(lParam);
			}

			if (onTextureMap && textureFlagList && window == hWndMain)
			{
				textureHoverX = GET_X_LPARAM(lParam);
				textureHoverY = GET_Y_LPARAM(lParam);
				textureHoverSet = true;
			}

			if (zoomStage == 2)
			{
				POINT const zoomAt = InThePanels(window, lParam);

				ZoomMove(zoomAt.x, zoomAt.y);
			}

			if (onTextureMap && textureWindowOn) { POINT const over = InThePanels(window, lParam); TextureWindowMouseAt(over.x, over.y); }   // the scale keys zoom about the mouse (FUN_0055d350)
			if (textureWinRightDrag) { POINT const turned = InThePanels(window, lParam); TextureWindowRightDragTo(turned.x, turned.y, textureWinFrameW, textureWinFrameH, (wParam & MK_CONTROL) != 0); }   // the texture windows' right drag

			if (textureWinDrag)
			{
				POINT const dragged = InThePanels(window, lParam);

				TextureWindowDragTo(dragged.x, dragged.y, textureWinFrameW, textureWinFrameH);
			}

			// Edit Extents' Shift + right drag (FUN_00585750 0x585bed-0x5862cc): the mouse's move since the last one, times the scale, moves the views' centre on the pane's axes (the picture follows the mouse)
			if (waypointPanning && threeBox.width >= 64 && threeBox.height >= 48)
			{
				POINT const where = InThePanels(window, lParam);
				int32_t const sx = (where.x - threeBox.x) * 640 / threeBox.width;
				int32_t const sy = (where.y - threeBox.y) * 480 / threeBox.height;
				int64_t const s = waypointMode.ViewScale() / 100 > 0 ? waypointMode.ViewScale() / 100 : 1;

				if (waypointPanPane == 0)
				{
					waypointViewCentre[0] += (int64_t)(waypointPanAt[0] - sx) * s;
					waypointViewCentre[1] += (int64_t)(waypointPanAt[1] - sy) * s;
				}
				else if (waypointPanPane == 1)
				{
					waypointViewCentre[2] += (int64_t)(sx - waypointPanAt[0]) * s;
					waypointViewCentre[1] += (int64_t)(waypointPanAt[1] - sy) * s;
				}
				else if (waypointPanPane == 2)
				{
					waypointViewCentre[0] += (int64_t)(waypointPanAt[0] - sx) * s;
					waypointViewCentre[2] += (int64_t)(sy - waypointPanAt[1]) * s;
				}

				waypointPanAt[0] = sx;
				waypointPanAt[1] = sy;
			}

			// A face of a waypoint's box taken by a press follows the mouse in its quarter of the three views (the box is drawn from the list as it is now)
			if (waypointMode.Dragging() && threeBox.width >= 64 && threeBox.height >= 48)
			{
				POINT const where = InThePanels(window, lParam);
				int32_t const sx = (where.x - threeBox.x) * 640 / threeBox.width;
				int32_t const sy = (where.y - threeBox.y) * 480 / threeBox.height;
				int const mask = sy < 240 ? (sx < 320 ? 3 : 6) : (sx < 320 ? 5 : 0);
				int32_t scratch[3] = { threeCursor[0], threeCursor[1], threeCursor[2] };

				if (mask == waypointDragMask && host::ThreeView_Click(threeViews, sx, sy, scratch))
				{
					int32_t file[3];

					host::WaypointView_FileUnits(scratch, local_scale, file);
					if (ShiftDown() || (wParam & MK_SHIFT) != 0)
					{
						waypointMode.DragToSnapped(file);   // Shift: onto the nearest side of the other waypoints and of the modules (FUN_00588ac0)
					}
					else
					{
						waypointMode.DragTo(file);
					}
				}
			}

			if (rightButtonDown)
			{
				// What the mouse asks for between two frames is held to a whole turn each way, however many moves come in between
				int const x = GET_X_LPARAM(lParam);
				int const y = GET_Y_LPARAM(lParam);

				if (abs(x - lastMouseX) + abs(y - lastMouseY) > 0 && (abs(x - rightRawX) > 4 || abs(y - rightRawY) > 4))
				{
					rightDragged = true;
				}

				mouseTurn = FlyCamera_HeldTo((long long)mouseTurn + (long long)(x - lastMouseX) * 4, kFlyCameraWholeTurn);
				mouseLook = FlyCamera_HeldTo((long long)mouseLook + (long long)(y - lastMouseY) * 4, kFlyCameraWholeTurn);
				lastMouseX = x;
				lastMouseY = y;
			}
			return 0;
		case kHarnessPicture:
			if (NobodyWatching())
			{
				if (wParam >= 1 && wParam <= 999)
				{
					harnessPictureAsked = (int)wParam;
				}
				else
				{
					printf("AVP_OpenInteng: picture refused, %llu isn't 1 to 999\n", (unsigned long long)wParam);
					fflush(stdout);
				}
			}
			return 0;
		case kHarnessState:
			harnessStateAsked = harnessStateAsked || NobodyWatching();
			return 0;
		case kHarnessItem:
			if (NobodyWatching())
			{
				HarnessRunItem((int32_t)wParam);
			}
			return 0;
		case kHarnessSave:
			if (NobodyWatching())
			{
				SaveTheCopy();
				printf("AVP_OpenInteng: copy: %s\n", statusWords ? statusWords : "");   // said at once: the next frame's own status words may replace these
				fflush(stdout);
				harnessScenarioAsked = true;
			}
			return 0;
		case WM_COPYDATA:
			// the harness's camera-orbit / camera-corner (orbit_command.h): its text, at most 256 bytes, in a -picture run only
			if (NobodyWatching() && lParam && ((COPYDATASTRUCT const *)lParam)->dwData == kOrbitCommandTag && ((COPYDATASTRUCT const *)lParam)->lpData && ((COPYDATASTRUCT const *)lParam)->cbData <= 256)
			{
				std::string orbitWords;

				OrbitCommand_Run(std::string((char const *)((COPYDATASTRUCT const *)lParam)->lpData, ((COPYDATASTRUCT const *)lParam)->cbData), &orbitWords);
				LogString(orbitWords);
				Answer(orbitWords);
				return TRUE;
			}
			break;
		case kHarnessCamera:
			if (NobodyWatching() && theCamera && wParam <= 5)
			{
				// 0 to 3: x, y, z, turn (the scenario's "camera"); 4 and 5: look (signed, a quarter turn either way) and roll (its "camera-look", for the camera turn's acceptance)
				int *const part[6] = { &theCamera->x, &theCamera->y, &theCamera->z, &theCamera->turn, &theCamera->look, &theCamera->roll };

				*part[wParam] = (int)lParam;
				FlyCamera_Hold(theCamera);

				if (wParam == 3 || wParam == 5)
				{
					printf("AVP_OpenInteng: camera: %d %d %d %d look %d roll %d\n", theCamera->x, theCamera->y, theCamera->z, theCamera->turn, theCamera->look, theCamera->roll);
					fflush(stdout);
				}
			}
			return 0;
		case kHarnessFaceLight:
			if (NobodyWatching() && wParam >= 1)
			{
				HarnessFaceLight((size_t)wParam - 1);
			}
			return 0;
		case kHarnessPickMarker:
			if (NobodyWatching())
			{
				if ((size_t)wParam < entryMarkers.size())
				{
					if (entryFlying)
					{
						StopFlyingTheEntryPoint("another entry point was picked");
					}

					entryMarkerPicked = (int)wParam;
					printf("AVP_OpenInteng: pick-marker: marker %d of %zu picked (the entry from module %d to module %d, point %d %d %d)\n", (int)wParam, entryMarkers.size(), (int)entryMarkers[(size_t)wParam].main, (int)entryMarkers[(size_t)wParam].adjacent,
						(int)entryMarkers[(size_t)wParam].point[0], (int)entryMarkers[(size_t)wParam].point[1], (int)entryMarkers[(size_t)wParam].point[2]);
				}
				else
				{
					printf("AVP_OpenInteng: pick-marker: no marker %d (there are %zu)\n", (int)wParam, entryMarkers.size());
				}

				fflush(stdout);
			}
			return 0;
		case kHarnessPickModule:
			if (NobodyWatching())
			{
				HarnessPickModule((int32_t)wParam);
			}
			return 0;
		case kHarnessLocate:
			if (NobodyWatching())
			{
				HarnessLocate(wParam == 0xffff ? -1 : (int32_t)wParam);
			}
			return 0;
		case kHarnessPage:
			if (NobodyWatching() && wParam < 64)
			{
				HarnessShowPage((int)wParam);
			}
			return 0;
		case kHarnessScenarioState:
			harnessScenarioAsked = harnessScenarioAsked || NobodyWatching();
			return 0;
		case kHarnessPickPoly:
			if (NobodyWatching() && onTextureMap)
			{
				std::vector<host::LevelObject> everyObject;
				OpenLevelFile *const here = LevelObjects_OpenForEdit();

				LevelObjects_Read(&everyObject);

				if (here && here->Level() && (size_t)wParam < everyObject.size())
				{
					TexturePolygonClicked(here, everyObject[(size_t)wParam].id, false, -1, (int32_t)((uint32_t)lParam & 0x3fffffffu), (lParam & 0x40000000) != 0, false);
				}

				harnessScenarioAsked = true;
			}
			return 0;
		case kHarnessSelectRow:
			switch (host::HarnessSelect_Row(NobodyWatching(), &objectList, (uint64_t)wParam, lParam != 0))
			{
				case host::HarnessSelectAnswer::Selected:
					harnessPanelAsked = true;
					break;
				case host::HarnessSelectAnswer::Refused:
					printf("AVP_OpenInteng: select refused, row %llu\n", (unsigned long long)wParam);
					fflush(stdout);
					break;
				default:
					break;
			}
			return 0;
		case WM_GETMINMAXINFO:
			// The editor's window is never narrower than its widest bar needs, so no title is past the toolbar's edge (editor_bar.h). The
			// least is of the client area, so the frame's own width is added.
			if (window == editorWindow && lParam && barMinClientWidth > 0)
			{
				RECT least = { 0, 0, barMinClientWidth, 200 };

				AdjustWindowRectEx(&least, (DWORD)GetWindowLongA(window, GWL_STYLE), FALSE, (DWORD)GetWindowLongA(window, GWL_EXSTYLE));
				((MINMAXINFO *)lParam)->ptMinTrackSize.x = least.right - least.left;
				return 0;
			}
			break;
		case 0x02E0:   // WM_DPICHANGED (WinUser.h)
			// Moved to a screen with another scaling: Windows suggests the window's place and size there, which keeps it looking as big as it
			// did. Only the editor's own window takes it; the panels and the view follow its new size on the next frame. (Added 2026-11-14 by
			// Melanikus (Dietrich), F3.)
			if (window == editorWindow && lParam)
			{
				RECT const *const suggested = (const RECT *)lParam;

				SetWindowPos(window, NULL, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
					SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return 0;
		case WM_CLOSE:
			// The game's own wait for a lost device (R_BeginScene, d3_func.cpp:352) pumps the messages and ends only on !bRunning, so
			// closing the window has to say so to the game as well, or the editor would never close while the device was lost.
			// (Modified 2026-11-14 by Melanikus (Dietrich), F3.)
			running = false;
			bRunning = false;
			return 0;
	}
	return DefWindowProcA(window, message, wParam, lParam);
}

// Whether a move's key in the layout in use is held down (Added 2026-11-14 by Melanikus (Dietrich), F3.)
static bool Held(EditorMove move)
{
	uint8_t const key = EditorActions_MoveKey(keyLayout, move);

	if (key != 0 && keyDown[key])
	{
		return true;
	}

	// the arrow keys are the other strafe keys (the manual's General Keys, menu.txt: "1 / Left Arrow" strafe left, "3 / Right Arrow" strafe right, "- / Up Arrow" strafe up, "+ / Down Arrow" strafe down; inteng reads both slots)
	if (keyLayout == KeyLayout::Classic)
	{
		if (move == EditorMove::Left)
		{
			return keyDown[VK_LEFT];
		}

		if (move == EditorMove::Right)
		{
			return keyDown[VK_RIGHT];
		}

		if (move == EditorMove::Up)
		{
			return keyDown[VK_UP];
		}

		if (move == EditorMove::Down)
		{
			return keyDown[VK_DOWN];
		}
	}

	return false;
}

// One frame's move of the camera, from the layout's keys held down and the mouse. The game's angles are 4096ths of a full turn, and its
// distances are millimetres: 4096 is a whole turn, and 100 a step of 10 centimetres.
// ADAPTIVE DEGRADATION (row C-45): each frame while it is on and a sequence is open (FUN_004889d0 0x48a03a -> FUN_00479b80), the distance table (the model's HIDEGDIS; with none, the one
// FUN_00484380 makes: level x 10000) is read from level 9 down, and the first level whose distance is no more than the camera's distance from the world's origin (FUN_004fd830 of the camera's
// place, / 100: mm) becomes the level (FUN_00477fb0); none: no change. The table is read again only when the model's bytes changed (Shift+D writes it).
static void HierarchyAdaptivePick()
{
	static unsigned readGeneration = ~0u;
	static std::string readModel;
	static int32_t distances[10] = { 0, 10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000 };

	if (!onHierarchy || !hierarchyMode.adaptiveDegradation || !hierarchyMode.sequenceSelected || !hierarchyModelOpen || !theCamera)
	{
		return;
	}

	if (hierarchyModelFile.Generation() != readGeneration || hierarchyMode.modelName != readModel)
	{
		bool present = false;

		if (host::HierarchyEdit_ReadDegradation(&hierarchyModelFile, distances, &present) != host::HierarchyEditResult::Done)
		{
			return;
		}

		readGeneration = hierarchyModelFile.Generation();
		readModel = hierarchyMode.modelName;
	}

	double const length = sqrt((double)theCamera->x * theCamera->x + (double)theCamera->y * theCamera->y + (double)theCamera->z * theCamera->z);
	int32_t const distance = length < 2147483647.0 ? (int32_t)length : 2147483647;

	for (int level = 9; level >= 0; level--)
	{
		if (distances[level] <= distance)
		{
			hierarchyDetail = level;
			return;
		}
	}
}

// Hierarchy's Return (C-47): once the frame with "Saving" has been drawn, the changed model is saved as a copy with its collections' text (SaveTheCopy, as Exit's HP-1), and the saved
// words are the state line's; nothing is said otherwise (the Hierarchy screen has no status line)
static void HierarchySaveIfAsked()
{
	if (hierarchySaveAsked != 1)
	{
		return;
	}

	hierarchySaveAsked = 0;

	if (!onHierarchy || !hierarchyModelOpen)
	{
		return;
	}

	SaveTheCopy();
	Answer(savedWords.c_str());
	hierarchyAnswerUntil = GetTickCount() + 5000;
}

// USE CAMERA ROOT (C, command 0x37), traced from inteng.exe. The hierarchy player's section update FUN_0047ef70 (0x47f29a-0x47f2dd) puts the object tied to a section ([section + 4]) at the section's place and matrix every
// frame. FUN_0047e320 (0x47e582-0x47e5ae) ties NO object to the section named "Camera Root" (DAT_006370ec) unless the C flag DAT_006370f0 is on, and then ties the CAMERA object (DAT_00962308): command 0x37 (0x488da5) flips
// the flag and sets or clears [DAT_006370ec + 4] the same way. So with C on the camera is moved to the Camera Root section's place each frame (the gun then shows as the game's HUD camera sees it), and the frame code of
// FUN_004889d0 (0x48a05d) then sets the camera's 3 x 3 matrix to the constant DAT_005d4e28 = identity (read from the exe), whether or not a Camera Root section exists. The text "Use Camera Root" is shown while it is on.
// Ours: the section's world place is the one the hierarchy view's own pose gives (HierarchyView_Sections), the camera's facing is turn 0, look 0, roll 0 (identity).
static void HierarchyCameraRootFrame(FlyCamera *camera)
{
	if (!onHierarchy || !hierarchyCameraRoot || !camera)
	{
		return;
	}

	std::vector<HierarchyViewSection> sections;

	if (HierarchyView_IsShown() && HierarchyView_Sections(&sections))
	{
		for (HierarchyViewSection const &section : sections)
		{
			if (section.name == "Camera Root")
			{
				camera->x = section.world[0];
				camera->y = section.world[1];
				camera->z = section.world[2];
				break;
			}
		}
	}

	camera->turn = 0;
	camera->look = 0;
	camera->roll = 0;
}

// THE TURN KEYS' ANGLES OF A FRAME, inteng's mover FUN_0053db40 for whichever block has it (the camera's, or the flown object's: A-7, reassess/op-moving-dietrich.md): the held turn keys' speeds
// (LevelView_TurnSpeedsOfKeys: a 10 degree matrix a key, in inteng's order, its Euler angles: 4096ths a second); this frame's share of each (FUN_0053c6c0: the speed x the frame's time in 65536ths of a
// second, the part of a 4096th carried to the next frame in carry, the sign put back), x 6 (0x53dd78), and when the frame took over 0.1 s each over 80 held to 81 (0x53dda4-0x53ddeb). angles: x, y, z.
static void TurnKeysOfFrame(int carry[3], int angles[3])
{
	int speeds[3];

	LevelView_TurnSpeedsOfKeys(Held(EditorMove::TurnLeft), Held(EditorMove::TurnRight), Held(EditorMove::RollLeft), Held(EditorMove::RollRight), Held(EditorMove::LookDown), Held(EditorMove::LookUp), speeds);

	int const frameTime = (int)(frameSeconds * 65536.0);

	for (int i = 0; i < 3; i++)
	{
		int const product = speeds[i] * frameTime;
		bool const negative = product < 0;
		unsigned const size = negative ? 0u - (unsigned)product : (unsigned)product;
		int whole = (int)(size >> 16);

		carry[i] -= (int)(size & 0xffff);

		if (carry[i] < 0)
		{
			whole++;
			carry[i] += 0x10000;
		}

		int angle = (negative ? -whole : whole) * 6;

		if (frameTime > 0x1999)
		{
			angle = angle > 0x50 ? 0x51 : (angle < -0x50 ? -0x51 : angle);
		}

		angles[i] = angle;
	}
}

static int cameraTurnCarry[3] = { 0, 0, 0 };   // the camera block's +0x1b8 / +0x1bc / +0x1c0: the part of a 4096th of each axis' turn not turned yet
static int frameViewWidth = 0;                 // the view's width this frame, in pixels (the wide view's turn, MoveTheCamera)

static void MoveTheCamera(FlyCamera *camera, bool moves = true)
{
	// inteng's live mover, FUN_0053db40 (THE CAMERA'S MOVER above). With moves false the keys only TURN the camera (a flying object is moved by them: MoveTheFlyingObject), and the
	// camera does not glide (inteng gives the flown object the mover, slot 2, and the camera slot 0: 0x4ad474 / 0x4df0b7)

	// C held: nothing moves this frame, the glide neither (0x53dc0e: JNZ 0x53e58b, past the glide)
	if (keyDown['C'])
	{
		mouseTurn = 0;
		mouseLook = 0;
		return;
	}

	// THE CAMERA'S TURN KEYS (Bishop 11-17 ~06:00; A-7's mapping, reassess/op-moving-dietrich.md): the camera block has the same mover as a flown object, so its turn is the same: this frame's angles
	// (TurnKeysOfFrame: about 684 of 4096 a second for one key, at any frame rate), then THE WIDE VIEW (0x53de00-0x53de84: the camera block holds the camera object, +0x6c; when its view's +0x34 / +0x38,
	// half the screen's width unless zoomed (FUN_004721a0), is over 512, the y turn becomes y x 512 / it and the x turn x x 512 / it (FUN_0053e5a0, cut toward 0), never 0 for a key that turned: +-1),
	// then the view's matrix turned by them about its OWN axes (MatrixMultiply(+0x38, M, +0x38), MNormalise). Ours keeps the camera as its turn, look and roll: the view's matrix is made of them
	// (LevelView_MatrixOfAngles), turned (LevelView_TurnMatrix), and they are taken back from it (LevelView_AnglesOfMatrix). NAMED: those angles are whole 4096ths, so the view is held to them each
	// frame a key turns it (inteng's matrix is not); a frame no key turns leaves them as they are (inteng squares the matrix again each frame). OUR BUG, FIXED (section 15): the pitch was the wrong way
	// round, numpad 8 looked up and 2 down; inteng's manual (menu.txt: "8 Turn Down / Pitch Down", the help's "2/8 Pitch up/down") and its matrices (0x5f0590, 0x5f05b8) have 8 down, 2 up.
	// Was: 24 of 4096 every frame (the rate was the frame rate's), added to turn / look / roll. The mouse is as it was: its turn and look added to the angles.
	{
		int angles[3];

		TurnKeysOfFrame(cameraTurnCarry, angles);

		// the view's scale (+0x34 / +0x38): half the width, times Insert's zoom (FUN_00471cb0 multiplies the scale, FUN_004721a0 puts half the width back), so a zoom in slows the turn
		// (camera-closeout-drake.md CAM-2, Drake 11-17: the zoom was left out)
		int const half = (int)((double)(frameViewWidth / 2) * (double)zoomScale);

		if (half > 0x200)
		{
			for (int i = 1; i >= 0; i--)
			{
				int const was = angles[i];

				if (was != 0)
				{
					int const scaled = (int)((long long)was * 0x200 / half);

					angles[i] = scaled != 0 ? scaled : (was < 1 ? -1 : 1);
				}
			}
		}

		if (angles[0] != 0 || angles[1] != 0 || angles[2] != 0)
		{
			int32_t view[9];

			LevelView_MatrixOfAngles(camera->turn, camera->look, camera->roll, view);

			// '/' or numpad / held (0x53dec0-0x53ded6, the manual's "Rotate Camera Around Environment"): the camera SWINGS ROUND the rotation centre by the turn, about the world's axes with '/',
			// about its own with numpad / (LevelView_OrbitTurn, CAM-3); else it turns where it is (0x53df78). NOT BUILT HERE: DAT_00914afc's turn about a point the camera carries (CAM-4,
			// Track Editing's, Vasquez). NAMED: while an entry point flies (Waypoint's F, FUN_0058b550: the marker has the mover) inteng would swing the MARKER round the centre; ours turns the
			// camera in place then (the entry fly takes only the camera's place change, which an orbit of the camera would move the marker by wrongly)
			if (!entryFlying && (KeyHeldNow(VK_OEM_2) || KeyHeldNow(VK_DIVIDE)))
			{
				int32_t place[3] = { camera->x, camera->y, camera->z };

				LevelView_OrbitTurn(view, place, rotationCentre, angles, KeyHeldNow(VK_DIVIDE));
				camera->x = place[0];
				camera->y = place[1];
				camera->z = place[2];
				FlyCamera_Hold(camera);
			}
			else
			{
				LevelView_TurnMatrix(view, angles);
			}

			LevelView_AnglesOfMatrix(view, &camera->turn, &camera->look, &camera->roll);
		}
	}

	camera->turn -= mouseTurn;
	camera->look -= mouseLook;
	mouseTurn = 0;
	mouseLook = 0;

	// The camera's own axes, as the game drew it last (level_view.h): the way it faces, its right and its down (the look up or down included)
	int facing[3];
	int toTheRight[3];
	int down[3];
	bool const axes = LevelView_CameraAxes(facing, toTheRight, down);

	if (moves)
	{
		if (!ShiftDown())
		{
			// Shift NOT held (0x53e216-0x53e3ee): A forward, Z back, Left / Right along the right axis, Up / Down along the up axis (Up rises), each by the step; every one of
			// them stops the glide
			int const step = StepAtTheSpeed();
			int const forward = (Held(EditorMove::Forward) ? step : 0) - (Held(EditorMove::Back) ? step : 0);
			int const sideways = (Held(EditorMove::Right) ? step : 0) - (Held(EditorMove::Left) ? step : 0);
			int const downwards = (Held(EditorMove::Down) ? step : 0) - (Held(EditorMove::Up) ? step : 0);

			if (Held(EditorMove::Forward) || Held(EditorMove::Back) || Held(EditorMove::Left) || Held(EditorMove::Right) || Held(EditorMove::Up) || Held(EditorMove::Down))
			{
				cameraGlide = 0;
			}

			if (axes)
			{
				camera->x += MUL_FIXED(forward, facing[0]) + MUL_FIXED(sideways, toTheRight[0]) + MUL_FIXED(downwards, down[0]);
				camera->y += MUL_FIXED(forward, facing[1]) + MUL_FIXED(sideways, toTheRight[1]) + MUL_FIXED(downwards, down[1]);
				camera->z += MUL_FIXED(forward, facing[2]) + MUL_FIXED(sideways, toTheRight[2]) + MUL_FIXED(downwards, down[2]);
			}
		}
		else
		{
			// Shift HELD (0x53e088-0x53e0f6): no step; Shift+A speeds the glide up, Shift+Z down, every frame (inteng's int wraps: so does this one, by unsigned sums)
			if (Held(EditorMove::Forward))
			{
				cameraGlide = (int32_t)((uint32_t)cameraGlide + (uint32_t)kGlidePerFrame);
			}

			if (Held(EditorMove::Back))
			{
				cameraGlide = (int32_t)((uint32_t)cameraGlide - (uint32_t)kGlidePerFrame);
			}
		}

		// Space held: the glide stops (0x53e54d)
		if (keyDown[VK_SPACE])
		{
			cameraGlide = 0;
		}

		// THE GLIDE (FUN_0053c7d0, 0x53e582): along the way the camera faces now, glide x the frame's time (not held to 0.1 s), in inteng's units, 100 to the millimetre; the
		// part of a millimetre left over is carried to the next frame
		static double glideCarry[3] = { 0.0, 0.0, 0.0 };

		if (cameraGlide != 0 && axes)
		{
			double const millimetres = (double)cameraGlide * frameSeconds / 100.0;
			int *const place[3] = { &camera->x, &camera->y, &camera->z };

			for (int i = 0; i < 3; i++)
			{
				glideCarry[i] += millimetres * (double)facing[i] / 65536.0;

				double const whole = glideCarry[i] < 0.0 ? ceil(glideCarry[i]) : floor(glideCarry[i]);

				glideCarry[i] -= whole;
				*place[i] = (int)((double)*place[i] + whole);
			}
		}
	}

	// And held to the camera's limits (level_view.h), so no key held for days (nor a glide) can carry a number past what an int holds
	FlyCamera_Hold(camera);
}

// F, the flying object's keys: inteng's LIVE mover FUN_0053db40 on the flown object's block (slot 2, 0x4df0b7; THE CAMERA'S MOVER above, the same code), each frame in its order (A-5, OP-112):
//  - C held: nothing moves this frame, the glide neither (0x53dc0e: JNZ 0x53e58b, past the glide at 0x53e582); the speed keys [ ] have been read before it (0x53db60-0x53dbc2);
//  - the turn keys turn the object (0x53dc1e-0x53dd11), unless the camera is linked to it (R), when they swing the camera round it;
//  - Shift NOT held (0x53e080): one step per key along a row of the object's matrix (the game's, QuatToMat: row 0 right, row 1 down, row 2 forward, the block's +0x38 / +0x44 / +0x50): Left or numpad 1 -row 0,
//    Right or numpad 3 +row 0, Up or numpad '-' -row 1, Down or numpad '+' +row 1, A +row 2, Z -row 2 (0x53e216-0x53e362); with X ("Move Rel to Axes", DAT_00914af8) along the world's axes (0x53e139-0x53e208).
//    Every one of them stops the glide. The step is the engine's at the FLY speed (F swaps it into speedUnits, OP-110, so StepAtTheSpeed gives it);
//  - Shift HELD (0x53e088-0x53e0f6): no step at all (Shift with an arrow does nothing); Shift+A adds 100000 to the glide each frame, Shift+Z takes 100000 off (an int: it wraps, as inteng's does);
//  - Space held: the glide stops (0x53e54d);
//  - THE GLIDE (FUN_0053c7d0, 0x53e582): along the object's OWN forward axis (+0x50, with X too), glide x the frame's time (not held to 0.1 s), 100 to the millimetre.
// INTENG BUG FIXED, NAMED (section 15): with X, Down adds 0x3e87360 to the world y where every other key adds 0x10000 (0x53e1c6: 81 c1 60 73 e8 03, ADD ECX,0x3e87360; the exe's bytes read), so Down went
// about 1000 times as far as Up; ours steps once, as Up does.
// NOT HERE: Space's turn speeds +0x190.. (always 0 for a flown object: no joystick). The turn keys' +0x1ac.. are worked out again each frame from the keys (A-7), so Space zeroing them changes nothing.
static void MoveTheFlyingObjectFree(bool turns);

// OP-118: with L on (the object collides: every block does on the screen's entry, OP-3, and L flips it), the frame's move goes through the object's own strategy block in the game's ObjectDynamics
// (LevelView_FlyCollideMove: gravity off, the level's polygons and the other objects in its way), so the object stops at a wall as inteng's engine stops its block (inteng: strategy 7, the ellipsoid;
// ours: the game's own collision shape for that object, NAMED). L off: it goes through.
static void MoveTheFlyingObject(bool turns)
{
	int32_t const before[3] = { flyWorld[0], flyWorld[1], flyWorld[2] };

	MoveTheFlyingObjectFree(turns);

	if (flyOn && flyReal && PlacementCurrentCollides() && (flyWorld[0] != before[0] || flyWorld[1] != before[1] || flyWorld[2] != before[2]))
	{
		int32_t reached[3];

		if (LevelView_FlyCollideMove(flyId, before, flyWorld, reached))
		{
			flyWorld[0] = reached[0];
			flyWorld[1] = reached[1];
			flyWorld[2] = reached[2];
		}
	}
}

static void MoveTheFlyingObjectFree(bool turns)
{
	// C held: nothing moves, the glide neither
	if (keyDown['C'])
	{
		mouseTurn = 0;
		mouseLook = 0;
		return;
	}

	int const step = StepAtTheSpeed();

	// A-7 (OP-113), THE TURN (0x53dc1a-0x53df8d): the held turn keys' speeds (LevelView_TurnSpeedsOfKeys: a 10 degree matrix a key, in inteng's order, its Euler angles: 4096ths a second); this frame's share of
	// each (FUN_0053c6c0: the speed x the frame's time in 65536ths of a second, the part of a 4096th carried to the next frame, the sign put back), x 6 (0x53dd78), and when the frame took over 0.1 s each over 80
	// held to 81 (0x53dda4-0x53ddeb); the object's own matrix then turned by those angles about its own axes (LevelView_TurnMatrix). NAMED: the mouse's turn and look are added to the frame's y and x (inteng's
	// mouse look is not read here). A special object (no live copy) is not turned, as before.
	if (turns && flyReal)
	{
		int angles[3];

		TurnKeysOfFrame(flyTurnCarry, angles);
		angles[0] -= mouseLook;
		angles[1] -= mouseTurn;
		mouseTurn = 0;
		mouseLook = 0;

		// the turned angles kept as before: turn (y), look (x), roll (z), for the scenario's state lines only
		flyTurnAcc[0] = (flyTurnAcc[0] + angles[1]) & 4095;
		flyTurnAcc[1] = (flyTurnAcc[1] + angles[0]) & 4095;
		flyTurnAcc[2] = (flyTurnAcc[2] + angles[2]) & 4095;

		// every frame, a turn or none (inteng multiplies and squares the matrix again each frame: 0x53df7b-0x53df88)
		LevelView_TurnMatrix(flyMatrix, angles);
	}
	else if (turns)
	{
		mouseTurn = 0;
		mouseLook = 0;
	}

	// the object's own axes (right, down, forward: the rows of its matrix, turned by this frame's keys first: the mover turns before it moves); a special object (no turn of its own here) has the world's
	double own[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

	if (flyReal)
	{
		for (int i = 0; i < 9; i++)
		{
			own[i] = (double)flyMatrix[i] / 65536.0;
		}
	}

	// the keys' axes: the object's own, or the world's with X
	double const world[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	double const *const axes = flyRelAxes ? world : own;

	if (!ShiftDown())
	{
		double const forward = (Held(EditorMove::Forward) ? step : 0) - (Held(EditorMove::Back) ? step : 0);
		double const sideways = (Held(EditorMove::Right) ? step : 0) - (Held(EditorMove::Left) ? step : 0);
		double const down = (Held(EditorMove::Down) ? step : 0) - (Held(EditorMove::Up) ? step : 0);

		if (Held(EditorMove::Forward) || Held(EditorMove::Back) || Held(EditorMove::Left) || Held(EditorMove::Right) || Held(EditorMove::Up) || Held(EditorMove::Down))
		{
			flyGlide = 0;
		}

		for (int k = 0; k < 3; k++)
		{
			flyWorld[k] = FlyCamera_HeldTo((long long)flyWorld[k] + (long long)floor(forward * axes[6 + k] + sideways * axes[k] + down * axes[3 + k] + 0.5), kFlyCameraMostPlace);
		}
	}
	else
	{
		if (Held(EditorMove::Forward))
		{
			flyGlide = (int32_t)((uint32_t)flyGlide + (uint32_t)kGlidePerFrame);
		}

		if (Held(EditorMove::Back))
		{
			flyGlide = (int32_t)((uint32_t)flyGlide - (uint32_t)kGlidePerFrame);
		}
	}

	// A-7b, numpad '*' (0x53e403, the held key: every frame it is held), after the steps and before the glide: levelled, or with Shift snapped to right angles (FUN_0053d1e0); the glide then goes along the new forward
	if (flyReal && keyDown[VK_MULTIPLY])
	{
		if (ShiftDown())
		{
			LevelView_SquareMatrix(flyMatrix);
		}
		else
		{
			LevelView_LevelMatrix(flyMatrix);
		}

		for (int i = 0; i < 9; i++)
		{
			own[i] = (double)flyMatrix[i] / 65536.0;
		}
	}

	if (keyDown[VK_SPACE])
	{
		flyGlide = 0;
	}

	// THE GLIDE: along the object's own forward axis, glide x the frame's time, 100 to the millimetre; the part of a millimetre left over is carried to the next frame
	if (flyGlide != 0)
	{
		double const millimetres = (double)flyGlide * frameSeconds / 100.0;

		for (int k = 0; k < 3; k++)
		{
			flyGlideCarry[k] += millimetres * own[6 + k];

			double const whole = flyGlideCarry[k] < 0.0 ? ceil(flyGlideCarry[k]) : floor(flyGlideCarry[k]);

			flyGlideCarry[k] -= whole;
			flyWorld[k] = FlyCamera_HeldTo((long long)flyWorld[k] + (long long)whole, kFlyCameraMostPlace);
		}
	}
}

// The folder this program is in, with its last "\", or false if it doesn't fit
static bool ProgramFolder(char *folder, size_t size)
{
	DWORD const length = GetModuleFileNameA(NULL, folder, (DWORD)size);
	if (length == 0 || length >= size)
	{
		return false;
	}
	char *const lastSlash = strrchr(folder, '\\');
	if (!lastSlash)
	{
		return false;
	}
	lastSlash[1] = '\0';
	return true;
}

// A setting from the editor's settings file, or false if it's missing or too long for buf
static bool ReadSetting(const char *settingsFile, const char *name, char *buf, size_t size)
{
	DWORD const length = GetPrivateProfileStringA("Editor", name, "", buf, (DWORD)size, settingsFile);
	return length > 0 && length < size - 1;
}

// INTENG'S MENU FONT (view-state-table.md H-5b, H-5c, H-5e; Bishop's ruling 11-16: Rebellion's font is never carried in our source, it is read at run time from the user's own inteng
// Tools folder): Tools\font16.fnt next to this program first, then in the folder the setting IntengTools names. The file is read as hostile input (menu_font.h); missing or refused,
// the menus keep the game's small font and the status line says why.
static std::string menuFontWords;

static void MenuFontStart()
{
	std::vector<std::string> tries;
	char folder[MAX_PATH];
	char named[MAX_PATH];

	if (ProgramFolder(folder, sizeof(folder)))
	{
		tries.push_back(std::string(folder) + "Tools\\font16.fnt");
	}

	if (layoutSettingsFile[0] && ReadSetting(layoutSettingsFile, "IntengTools", named, sizeof(named)))
	{
		tries.push_back(std::string(named) + "\\font16.fnt");
	}

	std::string last = "not found";

	for (std::string const &path : tries)
	{
		FILE *file = nullptr;

		if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
		{
			continue;
		}

		std::vector<uint8_t> bytes(host::kMenuFontMostBytes + 1);
		size_t const got = fread(bytes.data(), 1, bytes.size(), file);

		fclose(file);

		host::MenuFont font;
		host::MenuFontRefusal why = host::MenuFontRefusal::None;

		if (!host::MenuFont_Read(bytes.data(), got, &font, &why))
		{
			last = std::string("refused, ") + host::MenuFont_Why(why);
			continue;
		}

		if (PanelPass_SetMenuFont(font))
		{
			host::MenuPaint_SetUnderlineRow(font.height - 1);   // the letter's last row (FUN_0046ce70 0x46cffb)
			host::MenuPaint_SetMenuGlyphs(true);                 // glyph 2 marks a sub-menu's row
			return;
		}

		last = "its letters could not be made";
	}

	menuFontWords = "inteng's font (Tools\\font16.fnt): " + last + "; the menus and every text use the game's font";
	statusWords = menuFontWords.c_str();
}

// THE SIZE THE EDITOR STARTS AT (reassess/startup-screen-frost.md SS-1..SS-13; Bishop's order 11-16 from Melanikus, "Set my inteng to start at max resolution", and his rulings). Inteng: 640 x 480,
// then -r, then IntengOptions.ini's Screen Width / Height (read last: the file wins), full screen unless -w (FUN_0058c9e0, FUN_00493ce0, FUN_0058bdf0). Ours: IntengOptions.ini next to this
// program first, then in the folder the setting IntengTools names, read as hostile input (at most 64 KB, inteng_options.h) and NEVER written (it is classic inteng's own file, SS-11); none
// found: inteng's 640 x 480 (SS-12). The size is bounded to the primary monitor (at least 640 x 480); a popup over the whole monitor when the framed window wouldn't fit its work area (inteng's
// full screen, without its display mode change or its exclusive screen: SS-8, SS-10), else a framed window whose client is that size, at the work area's corner (as -w's 0,0). -w: always
// framed. A hidden test run (-picture) reads no IntengOptions.ini and keeps its 1024 x 768 window, which every acceptance's places hang on, unless -options <folder> or -r is given (SS-13).
// Written 2026-11-16 by Melanikus (Frost). Modified 2026-11-18 by Melanikus (Frost), the owner's ruling (SS-14, named): our own AVP_OpenInteng.ini's StartWidth / StartHeight are the size over
// inteng's 640 x 480 and under -r and IntengOptions.ini, and its Windowed=true frames the window as -w does (editor_options.h EditorOptions_Start); the release zip ships 1280 x 720, windowed.
struct StartPlace
{
	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
	int x = CW_USEDEFAULT;
	int y = CW_USEDEFAULT;
	int width = 1024;
	int height = 768;
	std::string words;   // for the log: the size and where it came from
};

static StartPlace ChooseTheStartPlace(bool hidden, host::IntengScreenSize size, bool sizeGiven, bool framed, std::string const &optionsFolder)
{
	StartPlace place;

	if (hidden && !sizeGiven && optionsFolder.empty())
	{
		place.words = "the start size: a hidden test run's 1024 x 768 window (no IntengOptions.ini is read without -options)";
		return place;
	}

	// Our own AVP_OpenInteng.ini's [Editor] StartWidth / StartHeight / Windowed (editor_options.h, the owner's ruling 11-18, SS-14, named): the size over inteng's 640 x 480 and under -r and
	// IntengOptions.ini (read below, it wins as in inteng); Windowed=true frames the window as -w does. Read as hostile input; a value refused is said in the log and not used.
	char const *ownWhy = nullptr;
	host::EditorStart const own = host::EditorOptions_Start(layoutSettingsFile[0] ? layoutSettingsFile : nullptr, &ownWhy);

	if (ownWhy)
	{
		LogString(std::string("settings: ") + ownWhy + " in AVP_OpenInteng.ini is not used: a size is 1 to 16384, Windowed is true or false");
	}

	bool const ownSize = own.sizeGiven && !sizeGiven;
	bool const windowedByOwn = own.windowed && !framed;

	if (ownSize)
	{
		size.width = own.width;
		size.height = own.height;
	}

	framed = framed || own.windowed;

	std::vector<std::string> tries;

	if (hidden)
	{
		if (!optionsFolder.empty())
		{
			tries.push_back(optionsFolder + "\\IntengOptions.ini");
		}
	}
	else
	{
		char folder[MAX_PATH];
		char named[MAX_PATH];

		if (ProgramFolder(folder, sizeof(folder)))
		{
			tries.push_back(std::string(folder) + "IntengOptions.ini");
		}

		if (layoutSettingsFile[0] && ReadSetting(layoutSettingsFile, "IntengTools", named, sizeof(named)))
		{
			tries.push_back(std::string(named) + "\\IntengOptions.ini");
		}
	}

	// the first file that opens decides, as inteng reads one
	std::string from = ownSize ? "no IntengOptions.ini found, AVP_OpenInteng.ini's StartWidth / StartHeight" : "no IntengOptions.ini found, inteng's 640 x 480";

	for (std::string const &path : tries)
	{
		FILE *file = nullptr;

		if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
		{
			continue;
		}

		std::vector<uint8_t> bytes(host::kIntengOptionsMostBytes + 1);
		size_t const got = fread(bytes.data(), 1, bytes.size(), file);

		fclose(file);

		if (got > host::kIntengOptionsMostBytes)
		{
			from = path + " not read: it is bigger than 64 KB";
			break;
		}

		host::IntengOptions_ScreenSize(bytes.data(), got, &size);
		from = path;
		break;
	}

	MONITORINFO monitor;
	POINT const corner = { 0, 0 };
	HMONITOR const primary = MonitorFromPoint(corner, MONITOR_DEFAULTTOPRIMARY);
	RECT whole = { 0, 0, 0, 0 };
	RECT work = { 0, 0, 0, 0 };
	RECT frame = { 0, 0, 0, 0 };

	memset(&monitor, 0, sizeof(monitor));
	monitor.cbSize = sizeof(monitor);

	if (primary && GetMonitorInfoA(primary, &monitor))
	{
		whole = monitor.rcMonitor;
		work = monitor.rcWork;
	}

	AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);

	host::IntengStartWindow const start = host::IntengOptions_StartWindow(size, whole.right - whole.left, whole.bottom - whole.top, work.right - work.left, work.bottom - work.top,
		frame.right - frame.left, frame.bottom - frame.top, framed);

	if (start.popup)
	{
		place.style = WS_POPUP | WS_CLIPCHILDREN;
		place.x = whole.left;
		place.y = whole.top;
		place.width = start.width;
		place.height = start.height;
	}
	else
	{
		RECT outer = { 0, 0, start.width, start.height };

		AdjustWindowRectEx(&outer, WS_OVERLAPPEDWINDOW, FALSE, 0);
		place.x = work.left;
		place.y = work.top;
		place.width = outer.right - outer.left;
		place.height = outer.bottom - outer.top;
	}

	place.words = "the start size: " + std::to_string(size.width) + " x " + std::to_string(size.height) + (sizeGiven ? " (-r, then " : " (") + from + "), " +
		(start.popup ? "a popup over the whole monitor, " : (windowedByOwn ? "a framed window (Windowed=true), its client " : framed ? "a framed window (-w), its client " : "a framed window, its client ")) + std::to_string(start.width) + " x " +
		std::to_string(start.height);
	return place;
}

// INTENG'S OWN EXE, for its binary data (object-placement.md section 8b V-25; Melanikus's ruling via Bishop 11-16: inteng's binary data is never in our source, it is read at run time from his
// own files): inteng.exe next to this program first, then in the folder the setting IntengTools names; only the traced exe (its sha256, inteng_exe.h), every read bounded. It is loaded once
// and shared (IntengExe_Shared: the editor icons here, Drake's light marker). The editor icons are inteng's built-in shapes 2, 3, 4, 7, 8, 9 and 13 (FUN_0042c700's slots, inteng_shapes.h);
// missing or refused, they are our own plain boxes and the status line says why. Added 2026-11-16 by Melanikus (Vasquez).
static std::string intengExeWords;

static void IntengExeStart()
{
	std::vector<std::string> tries;
	char folder[MAX_PATH];
	char named[MAX_PATH];

	if (ProgramFolder(folder, sizeof(folder)))
	{
		tries.push_back(std::string(folder) + "inteng.exe");
	}

	if (layoutSettingsFile[0] && ReadSetting(layoutSettingsFile, "IntengTools", named, sizeof(named)))
	{
		tries.push_back(std::string(named) + "\\inteng.exe");
	}

	static host::IntengExeImage image;
	std::string from;
	host::IntengExeRefusal why = host::IntengExeRefusal::None;

	if (!host::IntengExe_Load(tries, &image, &from, &why))
	{
		std::string const words = std::string(host::IntengExe_Why(why)) + (from.empty() ? std::string() : " (" + from + ")");

		host::IntengExe_SetShared(nullptr, words);
		host::EditorIcon_NotRead(words);
		intengExeWords = std::string("inteng.exe: ") + words + "; the editor icons are our own plain boxes and no placed shape's type is guessed from its name";
		LogString(intengExeWords);
		statusWords = intengExeWords.c_str();
		return;
	}

	host::IntengExe_SetShared(&image, "read from " + from);

	int const wanted[] = { 2, 3, 4, 7, 8, 9, 13 };
	host::IntengShapes shapes;
	host::IntengShapesRefusal refused = host::IntengShapesRefusal::None;

	if (!host::IntengShapes_Read(image, wanted, sizeof(wanted) / sizeof(wanted[0]), &shapes, &refused))
	{
		host::EditorIcon_NotRead(std::string(host::IntengShapes_Why(refused)) + " (" + from + ")");
		intengExeWords = std::string("inteng.exe: ") + host::IntengShapes_Why(refused) + "; the editor icons are our own plain boxes";
		LogString(intengExeWords);
		statusWords = intengExeWords.c_str();
	}
	else
	{
		host::EditorIcon_UseShapes(shapes, from);
		LogString(std::string("editor icons: ") + host::EditorIcon_From());
	}

	// inteng's shape-name table (0x5e7b58, FUN_004f6a20; simple_object_edit.h): a placed shape's type and extra data guessed from its outside file's name (Bishop's ruling V-31: read here, no copy)
	std::vector<host::IntengShapeName> names;
	host::IntengNamesRefusal namesWhy = host::IntengNamesRefusal::None;

	if (!host::IntengShapeNames_Read(image, &names, &namesWhy))
	{
		intengExeWords = std::string("inteng.exe: ") + host::IntengNames_Why(namesWhy) + "; no placed shape's type is guessed from its name";
		LogString(intengExeWords);
		statusWords = intengExeWords.c_str();
		return;
	}

	std::vector<host::SimpleShapeName> rows;

	for (host::IntengShapeName const &one : names)
	{
		host::SimpleShapeName row;

		row.name = one.name;
		row.type = one.type;
		row.extra = one.extra;
		rows.push_back(row);
	}

	host::SimpleObject_SetShapeNames(rows);
	LogString("inteng's shape-name table: " + std::to_string(rows.size()) + " names read from " + from);
}


// The editor stops before it starts, and says why: on the error output, and with someone at the screen in a message box too. Windows'
// "Open with", or a double-click, starts the editor with no console to read, so the reason would go by unseen. The box belongs to no
// window, and the reason is given to it as text, never as a format. With nobody at the screen, there's no box (nobody_watching.h).
static int Stop(const char *why)
{
	fprintf(stderr, "AVP_OpenInteng: %s\n", why);
	if (!NobodyWatching())
	{
		MessageBoxA(NULL, why, "AVP_OpenInteng", MB_OK | MB_ICONWARNING);
	}
	return 1;
}

// DISPLAY SCALING. The editor tells Windows it handles every screen's scaling itself, so Windows never stretches it: its windows, the
// mouse, and the level's and the panels' pictures are then all in the screen's own pixels, on every screen, at any scaling. A program
// that doesn't say so is drawn as if at 100% and stretched by Windows on a screen set to 125% or 150%, such as Melanikus's laptop
// panel. Per-monitor awareness (V2) is Windows 10's, from 1703; on an older Windows only the whole system's kind is there, which still
// stops the stretching on the main screen. The newer call is looked up by name, so the editor still starts where it's missing. It has
// to come before any window is made, so it's main's first step. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
static void NeverStretchedByWindows()
{
	// SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2): Windows' number for that context is -4 (WinUser.h)
	typedef BOOL (WINAPI *SetAwareness)(HANDLE context);
	HMODULE const user32 = GetModuleHandleA("user32.dll");
	SetAwareness const setAwareness = user32 ? (SetAwareness)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext") : nullptr;

	if (setAwareness && setAwareness((HANDLE)(INT_PTR)-4))
	{
		return;
	}

	SetProcessDPIAware();
}

// L-1, NO GRAY BORDER (Melanikus, 2026-11-15: "no gray border, we render as much as the window has to render, the buttons/menus just have a
// gray background"; the panels are "over the view, when open"). The level's view covers the whole of the editor's window. The menu bar, its
// drop-downs and the status line are drawn on top of it, on gray only behind themselves. The object list and the field panel are drawn over
// the level only while they are OPEN: the field panel opens when something is selected, the list from the View menu's Object list, and Esc
// closes the one opened last. They share the left column below the bar (HostFrame_ShareTheColumn). Everything is added to the game's own
// frame, after the level and before the frame is finished (LevelView_Draw), so it is in the shown frame and in every picture. A click goes to
// the topmost panel under it first, and reaches the level only outside them all. (Added 2026-11-15 by Melanikus (Dietrich), L-1.)

static void ToggleTheObjectList()
{
	listOpenedAt = listOpenedAt ? 0 : ++openClock;
}

// Esc: the panel opened last closes, and nothing else changes
static void CloseTheTopPanel()
{
	if (fieldsOpenedAt > listOpenedAt)
	{
		fieldsOpenedAt = 0;
	}
	else
	{
		listOpenedAt = 0;
	}
}

// Whether a press at a place of the window is on one of the panels shown this frame, and so is theirs and never the level's
static bool OnAPanel(int32_t x, int32_t y)
{
	return host::ToolButtons_At(toolBoxes, toolPlaced, x, y) >= 0 || host::ToolButtons_At(panelBoxes, panelPlaced, x, y) >= 0
		|| ui::BoxHolds(hitBar, x, y) || ui::BoxHolds(hitLower[0], x, y) || ui::BoxHolds(hitLower[1], x, y) || ui::BoxHolds(hitLower[2], x, y)
		|| ui::BoxHolds(hitList, x, y) || ui::BoxHolds(hitFields, x, y) || ui::BoxHolds(hitStatus, x, y);
}

// A plain gray box behind something, as one box for the panels' pass
host::PaintBox GrayBehind(int32_t x, int32_t y, int32_t width, int32_t height)
{
	host::PaintBox const box = { x, y, width, height, kPanelPassBackground };
	return box;
}

// ---- THE PROMPTS (M-1, object placement: inteng's Find and Move relative, and the editor's Name object) ----
// A prompt is a line typed over the level: its words, what was typed so far and a hint, on gray, and nothing else takes a key or a press until
// Enter answers it or Esc cancels it. What is typed is from a person, so it is checked as hostile: letters 32 to 126 only, at most 63. The
// answers go through object_place_edit.h, which holds every limit. (Added 2026-11-15 by Melanikus (Dietrich), M-1.)

// What a prompt's status line says, in the buffer the status line reads from
void Answer(const std::string &words)
{
	_snprintf_s(answerWords, sizeof(answerWords), _TRUNCATE, "%s", host::PanelPaint_SafeText(words).c_str());
	statusWords = answerWords;
}

// Hierarchy mode (Construction > Mode > Hierarchy, 0xcf; inteng's state 0xc, FUN_004889d0 is its command switch; p11-shape-handlers.md sections 59 to 63, hierarchy_mode.h).
// WHAT IS DONE: the screen is a mode of its own: entering it clears the mode's state, and its items run by inteng's command numbers (hierarchy_mode.h lists them): select a
// hierarchy, sequence or shape set collection, Copy Hierarchy, the collection commands, Add to / Remove from Collection, the platform speed, angle and switch. Each one that
// changes the level rebuilds it through the game's loader (hierarchy_edit.h) and says so on the status line. The other items stay [wip].
// NAMED DEVIATIONS: (1) inteng draws the selected hierarchy in a view of its own; here nothing is drawn for it. (2) inteng's choosers are lists you click; here the list is shown
// over the level and the choice is typed (its number, or part of its name). (3) Hierarchy names are 1 to 255 plain letters.
bool IsTheHierarchyScreen(size_t screen)
{
	return screen < host::kIntengScreenCount && strcmp(host::kIntengScreens[screen].name, "Hierarchy") == 0;
}

static bool IsTheTrackScreen(size_t screen)
{
	return screen < host::kIntengScreenCount && strcmp(host::kIntengScreens[screen].name, "Track Editing") == 0;
}

// The bars show another page (inteng's menu method +0x18), made again for the window's width
static bool ShowBarPage(int page)
{
	EditorBar_SetPage(page);

	if (!LoadTheMenus())
	{
		LogString("menus: the bars of page " + std::to_string(page) + " of screen " + std::to_string(barScreen) + " couldn't be made, so the editor closed");
		running = false;
		bRunning = false;
		return false;
	}

	return true;
}

static void ShowTrackPageAfter(int32_t command)
{
	int const next = host::TrackPages_After(EditorBar_Page(), command);

	if (next >= 0 && next != EditorBar_Page())
	{
		ShowBarPage(next);
	}
}

static void HarnessShowPage(int page)
{
	ShowBarPage(page);
	harnessScenarioAsked = true;
}


// The selected object's ID, when an object is selected
bool SelectedObjectId(uint8_t *id)
{
	host::Selected const selected = host::Selection_Get();

	if (selected.kind != host::SelectedKind::Object)
	{
		return false;
	}

	memcpy(id, selected.objectId, host::kSelectionIdBytes);
	return true;
}

// The object Light Sourcing's Edge > Soft works on: the selected object, or the module picked in Select Object mode (a module is an object of the file too, found by its name and place)
bool SelectedObjectOrModuleId(uint8_t *id)
{
	if (SelectedObjectId(id))
	{
		return true;
	}

	host::Selected const selected = host::Selection_Get();
	std::string name;
	int32_t world[3];
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	return selected.kind == host::SelectedKind::Module && open && open->Level() && LevelView_ModuleSite(selected.moduleIndex, &name, world) &&
		host::ModuleObjectBySite(open->Level(), name.c_str(), world, local_scale > 0.0 ? local_scale : 1.0, id);
}

std::string HexOf(const uint8_t *id)
{
	char hex[17];

	for (int i = 0; i < 8; i++)
	{
		_snprintf_s(hex + 2 * i, 3, _TRUNCATE, "%02X", (unsigned)id[i]);
	}

	return std::string(hex, 16);
}


// Adjacent Modules, Find Not Done (inteng's FUN_00404a20, command 0x68): the modules that have no adjacent modules. inteng says "All modules have
// adjacent modules" when there are none, and otherwise lists them in a chooser titled "Modules without adjacent modules". Here the status line
// says how many there are and names the one now selected, and each press goes on to the next (the UI is ours, what is listed is inteng's).
// matches FUN_00404a20 (menu-reads-1-full.txt:7332-7420). Its first step, FUN_00404040, writes the CURRENT module's list back, and does nothing while
// no module is current (DAT_005c3868 < 0), which is always so until Set Current is built. (Added 2026-11-15 by Melanikus (Drake).)
static void FindNotDoneAfterTheWrite();

static void FindNotDoneAdjacent()
{
	// FUN_00404a20 0x68 calls the write-back first (FUN_00404040): a module made adjacent just now counts. Its questions come first (AD-6a); the scan once they are answered
	WriteTheAdjacentListThen([](bool ok)
	{
		if (ok)
		{
			FindNotDoneAfterTheWrite();
		}
	}, true);   // the list stays changed (AD-6a-2)
}

static void FindNotDoneAfterTheWrite()
{
	static size_t next = 0;
	std::vector<host::ModuleWithoutAdjacency> none;

	if (!LevelObjects_Level())
	{
		Answer("No level is held to work on");
		return;
	}

	host::FindModulesWithoutAdjacency(LevelObjects_Level(), &none);
	(void)next;

	// FUN_00404a20 0x404f09-0x4052e4: none: the message "All modules have adjacent modules"; else the LIST dialog "Modules without adjacent modules" (rows shown sorted), and on a row: the view goes to the module's place, the current module's list is let go of,
	// the module becomes the current one with its own list loaded (FUN_004045b0), its pick cleared
	if (none.empty())
	{
		DialogMessage("All modules have adjacent modules");
		return;
	}

	std::vector<std::string> rows;

	for (host::ModuleWithoutAdjacency const &one : none)
	{
		rows.push_back(one.name);
	}

	DialogList("Modules without adjacent modules", rows, [none](int32_t choice)
	{
		if (choice < 0 || (size_t)choice >= none.size())
		{
			return;
		}

		host::Selected picked = {};

		picked.kind = host::SelectedKind::Object;
		memcpy(picked.objectId, none[(size_t)choice].id, host::kSelectionIdBytes);
		host::Selection_Set(picked);

		host::FoundObject where;

		if (theCamera && host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(none[(size_t)choice].id).c_str(), &where))
		{
			theCamera->x = (int)(where.x * local_scale);
			theCamera->y = (int)(where.y * local_scale);
			theCamera->z = (int)(where.z * local_scale);
			FlyCamera_Hold(theCamera);
		}

		AdjacentMakeCurrentFromList();   // no second write (AD-6a-2)
	});
}


// Adjacent Modules, Misc, Recalculate Entry Points (inteng's FUN_00403d50, menu-reads-2.txt:60-194): every entry of every module's adjacent-module
// list is worked out again (entry_recalc.h: the data side, as inteng's FUN_00402ae0 does it, AI groups included) and written into the level.
// For each pair that doesn't touch inteng asks "Warning adjacent modules don't touch\nFrom %s To %s\nWould you like to delete the adjacency?"
// (0x5c38b4, FUN_00474a40: Yes / No): Yes takes THAT entry out and the run goes on (FUN_00403d50 0x403f9f, 0x403fd4: FUN_00429820), No works it out
// as any other; an entry naming no object of the level is taken out without a word (0x403fb5-0x403fd4). Ours works the whole run out first, asks
// the questions in the run's order, then writes it all (NAMED: inteng works each entry out after the answers before it; an answer takes out one
// entry only, so a later one comes out the same unless its AI group reads that list). inteng says "Entry points calculated" (0x5c39c8) and, once
// the box is closed, WRITES THE LEVEL FILE (FUN_004719e0(DAT_0060801c, 0) at 0x4054a6, whether anything changed or not): ours saves there too
// (SaveAtIntengsPoint), and the status line carries the save's words. (Added 2026-11-15 by Melanikus (Drake). The save added 2026-11-17 by
// Melanikus (Dietrich), AD-1; the questions and the taking out, AD-2 / AD-5 of reassess/adjacent-closeout-dietrich.md.)
// FUN_00403d50 has three callers, and each goes on its own way once the run is done (AD-6): Adjacent's Recalculate Entry Points (0x405474: the box, then the save),
// Module Linking's Exit + New / All Adjacencies (FUN_004c5a30 0x4c603a: the save, no box) and Waypoint's Recalculate all (FUN_00583d00 0x583fee: the markers made
// again, no save). So the run is started by StartEntryRecalc (editor_shared.h) with what comes after it (done, given the run's words once all is written).
struct EntryRecalcRun
{
	std::vector<host::EntryChange> changes;
	std::vector<host::EntryDelete> deletes;   // the entries naming no object of the level (AD-5)
	std::vector<size_t> asks;                 // the changes whose modules don't touch, in the run's order
	std::vector<bool> takeOut;                // per change: the mapper answered Yes
	host::EntryRecalcCounts counts;
	unsigned generation;                      // the level's swap count when the run was worked out: the chunks it names are still the level's
	std::string who;                          // the command, for the words when the run cannot be written
	std::function<void(std::string const &)> done;   // the caller's way on, with the run's words (AD-6)
};

static void RecalculateEntryPointsFinish(std::shared_ptr<EntryRecalcRun> const &run)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level() || open->Generation() != run->generation)
	{
		Answer(run->who + ": the level changed while it asked; nothing was written");
		return;
	}

	std::vector<host::EntryChange> write;
	std::vector<host::EntryDelete> drop = run->deletes;
	size_t answeredYes = 0;

	for (size_t i = 0; i < run->changes.size(); i++)
	{
		host::EntryChange const &change = run->changes[i];

		if (run->takeOut[i])
		{
			host::EntryDelete const one = { change.adjacency, change.mainIndex, change.adjacentIndex, change.entry };

			drop.push_back(one);
			answeredYes++;
		}
		else if (!change.keep)
		{
			write.push_back(change);
		}
	}

	size_t refused = 0;
	size_t written = 0;
	size_t deleted = 0;

	{
		host::LevelStore store(open);

		written = host::ApplyEntryRecalc(&store, write, &refused);   // in place, before the image is made again for the deletions
	}

	host::AdjacentWriteResult const dropped = host::DeleteAdjacencyEntries(open, drop, &deleted);

	if (deleted > 0)
	{
		LoadTheObjectListAgain();   // the deletions made a new image: the object list is read from it again
	}

	std::string words = "Entry points calculated: " + std::to_string(run->counts.recalculated) + " worked out, " + std::to_string(written) + " changed";

	if (refused > 0)
	{
		words += ", " + std::to_string(refused) + " could not be written";
	}

	if (run->counts.notTouching > 0)
	{
		words += ", " + std::to_string(run->counts.notTouching) + " pairs of modules do not touch (" + std::to_string(answeredYes) + " taken out)";
	}

	if (run->counts.noObject > 0)
	{
		words += ", " + std::to_string(run->counts.noObject) + " naming no module taken out";
	}

	if (run->counts.failedToPlace > 0)
	{
		words += ", " + std::to_string(run->counts.failedToPlace) + " failed to place in the adjacent module";
	}

	if (dropped != host::AdjacentWriteResult::Done || deleted != drop.size())
	{
		words += ", " + std::to_string(drop.size() - deleted) + " could not be taken out";
	}

	if (run->done)
	{
		run->done(words);
	}
}

// FUN_00402ae0 0x4036c9-0x403730 (AD-7): when the worked-out point lies in no member of the adjacent list, the one-button box "Failed to placed entrypoint in adjacent
// module\nFrom %s To %s" (0x5c3874, inteng's own spelling; FUN_00474030 + FUN_0057c800, its answer not read); the point is written all the same (it returns 1)
static void RecalculateEntryPointsAsk(std::shared_ptr<EntryRecalcRun> const &run, size_t k);

static void RecalculateEntryPointsFailedBox(std::shared_ptr<EntryRecalcRun> const &run, size_t k)
{
	host::EntryChange const &pair = run->changes[run->asks[k]];

	DialogMessage("Failed to placed entrypoint in adjacent module\nFrom " + pair.mainName + " To " + pair.adjacentName, [run, k]()
	{
		RecalculateEntryPointsAsk(run, k + 1);
	});
}

// One pair a step, in the run's order: the question for a pair that doesn't touch (0x4032a7: sprintf of 0x5c38b4 with the two modules' names, FUN_00474a40 + FUN_0057c800(100, 100)),
// a Yes (3) taking that entry out (FUN_00402ae0 returns 0 at 0x403448, before the Failed box); then, unless the answer was Yes, the Failed box for a pair that failed to place
// (the two are not exclusive: a No can be followed by the box for the same pair). After the last, the run is written.
static void RecalculateEntryPointsAsk(std::shared_ptr<EntryRecalcRun> const &run, size_t k)
{
	if (k >= run->asks.size())
	{
		RecalculateEntryPointsFinish(run);
		return;
	}

	host::EntryChange const &pair = run->changes[run->asks[k]];

	if (!pair.notTouching)
	{
		RecalculateEntryPointsFailedBox(run, k);   // only the Failed box for this pair
		return;
	}

	DialogConfirm("Warning adjacent modules don't touch\nFrom " + pair.mainName + " To " + pair.adjacentName + "\nWould you like to delete the adjacency?", [run, k](bool yes)
	{
		run->takeOut[run->asks[k]] = yes;

		if (!yes && run->changes[run->asks[k]].failedToPlace)
		{
			RecalculateEntryPointsFailedBox(run, k);
			return;
		}

		RecalculateEntryPointsAsk(run, k + 1);
	});
}

bool StartEntryRecalc(std::string const &who, std::function<void(std::string const &)> const &done, int32_t onlyModule)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !LevelObjects_Level())
	{
		Answer("No level is held to work on");
		return false;
	}

	std::shared_ptr<EntryRecalcRun> const run = std::make_shared<EntryRecalcRun>();
	host::LevelStore store(open);

	if (!host::PlanEntryRecalc(LevelObjects_Level(), &store, &run->changes, &run->counts, &run->deletes, onlyModule))
	{
		Answer(who + ": the entry points could not be worked out");
		return false;
	}

	run->generation = open->Generation();
	run->takeOut.assign(run->changes.size(), false);
	run->who = who;
	run->done = done;

	for (size_t i = 0; i < run->changes.size(); i++)
	{
		if (run->changes[i].notTouching || run->changes[i].failedToPlace)
		{
			run->asks.push_back(i);   // a step: the question, the Failed box (AD-7), or both
		}
	}

	RecalculateEntryPointsAsk(run, 0);
	return true;
}

static void RecalculateEntryPoints()
{
	StartEntryRecalc("Recalculate Entry Points", [](std::string const &words)
	{
		Answer(words);

		// FUN_00404a20 0x405479-0x4054a6: the message box after the work; when it is closed, the level file is written (0x4054a6)
		DialogMessage("Entry points calculated", [words]()
		{
			const std::string saved = SaveAtIntengsPoint();   // const first: Snyk Code's parser skips the whole file on "std::string const" in a lambda given to a call

			Answer(words + "; " + saved);
		});
	});
}


// ---- MODULE LINKING (P5): the current module and its links, exactly as inteng's Module Linking does (FUN_004c1a90, menu-reads-1-full.txt:8409-8940) ----
// inteng keeps the current module's links as a tree in memory (module_links.h) and writes them into the level only when another module is made
// current (FUN_004c10c0's write, here WriteTheLinks): the module's VMDARRAY is rebuilt from the tree when the tree was changed and has at least one
// link. Its list of picked modules is the selection set here (selection.h: Shift+click adds), in the order they were picked. (Added 2026-11-15 by
// Melanikus (Drake), P5.)
std::unique_ptr<host::LinkTree> linkTree;

// For the harness's state line only (the bot reads it, the screen does not show it): the current module's tree, "none" or "module M changed C links N"
static std::string LinkTreeStateWords()
{
	if (!linkTree)
	{
		return "none";
	}

	return "module " + std::to_string(linkTree->RootModule()) + " changed " + std::to_string(linkTree->Changed() ? 1 : 0) + " links " + std::to_string((unsigned long long)linkTree->Count());
}

// MODULE LINKING'S PICKS (inteng's list DAT_00704320, DAT_00704324 how many; row ML-M-61 of module-linking-map-dietrich.md): the modules' object indexes in pick order, changed only by its clicks
// (LinkClick) and let go of by the link commands that let them go (LinkPicksLetGo). Each is drawn 0x7f007f over the tree's colours (FUN_004c17a0 paints the picks last).
std::vector<int32_t> linkPicks;

void LinkPicksLetGo()
{
	linkPicks.clear();
	host::Selection_Clear();
}

// A click on Module Linking (FUN_004c3760): the module it found (none: NOTHING happens, 0x4c3842 / 0x4c37ed). With the Delete key held (FUN_004c3880) the module is taken off the picks;
// else FUN_004c0cb0(module, Shift): without Shift every pick is let go and the module is the one pick; with Shift it is added at the end unless it is picked already (then nothing).
void LinkClick(host::PickReport const &used, bool shift, bool held)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];
	int32_t index = -1;

	if (!open || !open->Level())
	{
		return;
	}

	// The module under the pointer (inteng's FUN_00559720 gives the module record whatever it holds). A module the game gave a STRATEGY (a door, a lift) is picked as an Object by its
	// strategy's ID (pick_game.cpp: the block's SBname), which is the module object's own ID: it is that module. (Until 2026-11-16 only a plain module was taken here, so since e6ef0b4
	// a door could not be picked at all: Melanikus, "I can't select the doors to open/close branch anymore".)
	if (used.found == host::PickFound::Module)
	{
		if (!ModuleObject(open, used.moduleIndex, id) || !host::ModuleIndexOfId(open->Level(), id, &index))
		{
			return;
		}
	}
	else if (used.found == host::PickFound::Object)
	{
		memcpy(id, used.objectId, 8);

		if (!host::ModuleIndexOfId(open->Level(), id, &index))
		{
			return;   // a placed object, not a module: Module Linking picks modules only
		}
	}
	else
	{
		return;
	}

	const std::vector<int32_t>::iterator at = std::find(linkPicks.begin(), linkPicks.end(), index);

	if (held)
	{
		if (at != linkPicks.end())
		{
			linkPicks.erase(at);
		}

		host::Selection_Clear();
		return;
	}

	if (!shift)
	{
		linkPicks.clear();
		linkPicks.push_back(index);
	}
	else if (at == linkPicks.end())
	{
		linkPicks.push_back(index);
	}

	statusWords = host::Selection_UsePick(used);   // the module clicked is the selection too (the field panel, the harness's "selection")
}

// The picked modules, as object indexes, in pick order; an object that is not a module is not one. On Module Linking: its own list (linkPicks).
std::vector<int32_t> PickedModules()
{
	if (OnModuleLinkingScreen())
	{
		return linkPicks;
	}

	std::vector<int32_t> picked;

	for (size_t i = 0; i < host::Selection_SetCount(); i++)
	{
		uint8_t id[host::kSelectionIdBytes];
		int32_t index = 0;

		if (host::Selection_SetMember(i, id) && host::ModuleIndexOfId(LevelObjects_Level(), id, &index))
		{
			picked.push_back(index);
		}
	}

	// A plain click on a module picks it by the GAME's module number (the selection's kind Module), which the set above never holds: its object's place in the file joins the list, as the click means.
	// (Found by the Module Linking scenario: a click then Base > Set Current said "pick a module first" every time.)
	host::Selected const clicked = host::Selection_Get();
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (clicked.kind == host::SelectedKind::Module && open && open->Level())
	{
		uint8_t id[8];
		int32_t index = -1;

		if (ModuleObject(open, clicked.moduleIndex, id) && host::ModuleIndexOfId(open->Level(), id, &index) && std::find(picked.begin(), picked.end(), index) == picked.end())
		{
			picked.insert(picked.begin(), index);
		}
	}

	return picked;
}

// ---- ACOUSTIC MODE (reassess/object-placement.md section 2, AC-1 .. AC-9; the data side is acoustic_mode.h). inteng's is Object Placement's menu page 7 with
// DAT_008f4f30 = 1, so that FUN_004d9af0 runs only FUN_004f57b0 each frame; here it is the screen "Acoustic", entered by Mode > Acoustic Mode (0x10) and left by its
// Mode > Exit (0xa) for Object Placement. Its bar is inteng's page 7 (menu-status.json's Acoustic stand-in: Mode, Module, Default).
int32_t GameModuleOfNumber(int32_t number);


// The MODULE a pick found, by its object's ID (0x4f581e-0x4f5838: a block with a rif object whose definition is not a placed one, flag 0x400); anything else: false
static bool AcousticModuleOfPick(host::PickReport const &used, uint8_t id[8])
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	int32_t index = -1;

	if (!open || !open->Level())
	{
		return false;
	}

	if (used.found == host::PickFound::Module)
	{
		return ModuleObject(open, used.moduleIndex, id) && host::ModuleIndexOfId(open->Level(), id, &index);
	}

	if (used.found == host::PickFound::Object)
	{
		memcpy(id, used.objectId, 8);
		return host::ModuleIndexOfId(open->Level(), id, &index);   // a module the game gave a strategy (a door) is picked as an object; a placed object is no module
	}

	return false;
}

// AC-1 (FUN_004f5620): a flying current object stops flying and none is current; the picks start empty
static void AcousticEnter()
{
	if (flyOn)
	{
		PlacementFlyStop();
	}

	host::Selection_Clear();
	specialMode.haveCurrent = false;
	placedHierarchyCurrent = -1;
	acousticPicks.clear();
	acousticPointerOn = false;
}

// AC-8 (FUN_004f56c0) and any other way out: every pick let go of, its red with it
static void AcousticLeave()
{
	acousticPicks.clear();
	acousticPointerOn = false;
}

// AC-2, a LEFT press on the view (FUN_004f57b0 0x4f57fe-0x4f5a7b): the module under the pointer; without Shift it is added at the end when it is not picked yet (and drawn red),
// with Shift it is taken out of the list (its red cleared); anything that is not a module: nothing
static void AcousticClick(host::PickReport const &used, bool shift)
{
	std::array<uint8_t, 8> id;

	if (!AcousticModuleOfPick(used, id.data()))
	{
		return;
	}

	const std::vector<std::array<uint8_t, 8>>::iterator at = std::find(acousticPicks.begin(), acousticPicks.end(), id);

	if (shift)
	{
		if (at != acousticPicks.end())
		{
			acousticPicks.erase(at);
		}
	}
	else if (at == acousticPicks.end())
	{
		acousticPicks.push_back(id);
	}
}

// The module under the pointer, picked again every frame (FUN_004f57b0 0x4f5bd6: FUN_00559720 at the pointer), for the frame's lines
static void AcousticPointerFrame(int32_t viewWidth, int32_t viewHeight)
{
	acousticPointerOn = acousticPointerX >= 0 && acousticPointerY >= 0 && AcousticModuleOfPick(PickGame_AtClick(acousticPointerX, acousticPointerY, viewWidth, viewHeight), acousticPointerId);
}

// AC-9: the mode's text, at x 16 from y 32 (rows of 16)
void AcousticTextLines(std::vector<HierarchyTextLine> *out)
{
	std::vector<host::AcousticLine> lines;

	out->clear();
	host::Acoustic_FrameLines(LevelObjects_OpenForEdit(), acousticPointerOn ? acousticPointerId : nullptr, &lines);

	for (host::AcousticLine const &line : lines)
	{
		out->push_back({ line.x, line.y / 16, line.words });
	}
}

// FUN_004747f0's start text for a number (0x4749e5): "%f", the zeros at its end taken off, then a point left alone
static std::string AcousticShown(float value)
{
	char shown[64];

	_snprintf_s(shown, sizeof(shown), _TRUNCATE, "%f", (double)value);

	if (strchr(shown, '.'))
	{
		size_t length = strlen(shown);

		while (length > 0 && shown[length - 1] == '0')
		{
			shown[--length] = 0;
		}

		if (length > 0 && shown[length - 1] == '.')
		{
			shown[--length] = 0;
		}
	}

	return shown;
}

// The window's answer read as FUN_00474a10 reads it (sscanf "%f" of the edit's text). INTENG BUG, FIXED (MAPPING-ORDERS section 15): its float is never set before the
// sscanf (0x474a10 PUSH ECX keeps the window's own address there), so a text that is no number gave that address's bits as the reverb; here such a text changes nothing
static bool AcousticTyped(std::string const &typed, float *value)
{
	return sscanf_s(typed.c_str(), "%f", value) == 1;
}

// After a write: a level swapped for a new one (a record made) has its object list made again; a refusal is said
static void AcousticAfter(host::AcousticResult result, unsigned generationBefore)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (open && open->Generation() != generationBefore)
	{
		LoadTheObjectListAgain();
	}

	if (result != host::AcousticResult::Done)
	{
		Answer(std::string("Acoustic Mode: ") + host::AcousticWords(result));
	}
}

static void AcousticSetPicked(host::AcousticField field, int32_t type, float reverb, bool make)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || acousticPicks.empty())
	{
		return;
	}

	unsigned const before = open->Generation();

	AcousticAfter(host::Acoustic_SetModules(open, acousticPicks, field, type, reverb, make, nullptr), before);
}

static void AcousticSetLevel(host::AcousticField field, int32_t type, float reverb)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open)
	{
		return;
	}

	unsigned const before = open->Generation();

	AcousticAfter(host::Acoustic_SetLevel(open, field, type, reverb), before);
}

// The 26 names in their order: the list dialog shows them sorted, and answers with a name's place here, which is its type (FUN_004f5cf0's second list, 0..25)
static std::vector<std::string> AcousticTypeRows()
{
	std::vector<std::string> rows;

	for (int32_t type = 0; type < host::kAcousticTypeCount; type++)
	{
		rows.push_back(host::AcousticTypeName(type));
	}

	return rows;
}

// AC-3 .. AC-7: the items of the Module and Default menus (Exit is the screen change, in the frame)
static void AcousticCommand(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		return;
	}

	switch (command)
	{
	case 0xbb8:
		// Module > Acoustic type (FUN_004f5cf0): only with picked modules; the list "Select Module Acoustics Type" (0x5e95bc); a pick: every picked module's record (made when missing)
		if (!acousticPicks.empty())
		{
			DialogList("Select Module Acoustics Type", AcousticTypeRows(), [](int32_t place)
			{
				if (place >= 0 && place < host::kAcousticTypeCount)
				{
					AcousticSetPicked(host::AcousticField::Type, place, 0.0f, true);
				}
			});
		}

		break;
	case 0xbb9:
		// Module > Reverb (FUN_004f63a0): only with picked modules; the number window starts at the first picked record's reverb that is not -1.0, else 2.0
		if (!acousticPicks.empty())
		{
			DialogLine(std::string(), "Select reverb for module (>1 = default reverb)", AcousticShown(host::Acoustic_ModuleReverbStart(open, acousticPicks)), 200, 0x12, [](std::string const &typed)
			{
				float value = 0.0f;

				if (AcousticTyped(typed, &value))
				{
					AcousticSetPicked(host::AcousticField::Reverb, 0, host::AcousticReverbKept(value), true);
				}
			});
		}

		break;
	case 0xbba:
		// Module > Set To Default Acoustic Type (0x4f58b8): each picked module that HAS a record, type -1; none is made
		AcousticSetPicked(host::AcousticField::Type, -1, 0.0f, false);
		break;
	case 0xbbb:
		// Module > Set To Default Reverb (0x4f592f): each picked module that has a record, reverb -1.0
		AcousticSetPicked(host::AcousticField::Reverb, 0, -1.0f, false);
		break;
	case 0xbbc:
		// Default > Acoustic type (FUN_004f6010): the list "Select Environment Acoustics Type" (0x5e95dc); a pick: the level's record (made when missing)
		DialogList("Select Environment Acoustics Type", AcousticTypeRows(), [](int32_t place)
		{
			if (place >= 0 && place < host::kAcousticTypeCount)
			{
				AcousticSetLevel(host::AcousticField::Type, place, 0.0f);
			}
		});
		break;
	case 0xbbd:
	{
		// Default > Reverb (FUN_004f6560): the level's record made when missing BEFORE the window (0x4f657e), then the window at its reverb; Ok writes the kept number
		unsigned const before = open->Generation();
		host::AcousticResult const made = host::Acoustic_MakeLevelRecord(open);
		host::AcousticRecord record;

		AcousticAfter(made, before);

		if (made != host::AcousticResult::Done || !host::Acoustic_LevelRecord(LevelObjects_OpenForEdit(), &record))
		{
			break;
		}

		DialogLine(std::string(), "Select reverb for environment (>1 = default reverb)", AcousticShown(record.reverb), 200, 0x12, [](std::string const &typed)
		{
			float value = 0.0f;

			if (AcousticTyped(typed, &value))
			{
				AcousticSetLevel(host::AcousticField::Reverb, 0, host::AcousticReverbKept(value));
			}
		});
		break;
	}
	default:
		break;
	}
}

// FUN_004c10c0's write (0x4c10e2-0x4c1430, menu-reads-4.txt) and Set Current's (FUN_004c1a90 0x4c2377-0x4c26a3, row ML-M-19): when the tree was changed and has a link, the current
// module's VMDARRAY is rebuilt from it and the object's module flag (0x200) goes in (0x4c13b0 / 0x4c2611); when it was changed and has NO link, ONLY the flag comes out (0x4c1410 /
// 0x4c267d): the old VMDARRAY is LEFT, as inteng leaves it (Bishop's ruling 2026-11-16, rule 10b: the earlier named deviation that destroyed it is gone), so a module made current again
// loads its old links. Gives false when that was needed and could not be done (the level is then as it was). The level is swapped for a new one, so what was made of the old one is made again.
bool WriteTheLinks()
{
	if (!linkTree || !linkTree->Changed())
	{
		return true;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[host::kSelectionIdBytes];

	if (!open || !host::ModuleIdOfIndex(LevelObjects_Level(), linkTree->RootModule(), id))
	{
		return false;
	}

	if (!linkTree->HasLinks())
	{
		// inteng takes the flag out and leaves the old VMDARRAY (0x4c267d; a module with links and no flag): the same here (row ML-M-19)
		host::BaseObjectResult const flagged = host::SetModuleObjectFlag(open, id, false);

		if (flagged != host::BaseObjectResult::Done)
		{
			Answer(std::string("Module flag not changed: ") + host::BaseObjectWords(flagged));
			return false;
		}

		linkTree->ClearChanged();
		return true;
	}

	std::vector<host::LinkItem> const items = linkTree->Flatten();
	std::vector<host::ModuleLinkItem> written;

	for (host::LinkItem const &item : items)
	{
		host::ModuleLinkItem const one = { item.objectIndex, item.branchNo, item.flags, item.spare };

		written.push_back(one);
	}

	host::ObjectEditResult const result = host::SetModuleLinks(open, id, written.data(), written.size());

	if (result != host::ObjectEditResult::Done)
	{
		Answer(std::string("Module links not written: ") + host::ObjectEditWords(result));
		return false;
	}

	// the module flag goes in after the new chunk (the level was swapped: the ID names the object in the new one)
	host::BaseObjectResult const flagged = host::SetModuleObjectFlag(open, id, true);

	if (flagged != host::BaseObjectResult::Done)
	{
		Answer(std::string("Module links written, module flag not set: ") + host::BaseObjectWords(flagged));
	}

	linkTree->ClearChanged();
	LoadTheObjectListAgain();
	return true;
}

// ---- Object Placement's Alt Locations items (inteng's &Alt Locations, P9): the list of one object's other places, and what each item does to it ----
// The rules are model::AltLocations_* (alt_locations.h: Add new FUN_004df820, Delete Current FUN_004df6b0, Next FUN_004df8c0, Prev FUN_004df990, Change
// shown location FUN_004dfb90, Assign Group Number FUN_004dfa60); the list is read from and written to the object's ALTLOCAT through the edit store
// (alt_locations_edit.h), and the object's place and turn are the current location's after Delete, Next and Prev, as FUN_004dfae0 does. The editor's
// current number for an object lives here, as inteng's lived in its object. NAMED GAPS: inteng's Add new only works for the object kinds 0, 1, 2, 3 and 7
// (FUN_004df820 :8141): here any object does; the object's turn is the file's four floats, not inteng's matrix. (Added 2026-11-15 by Melanikus (Frost).)
static std::map<std::string, model::AltLocations> altLists;
static std::string altWords;

static bool AltListOf(OpenLevelFile *open, uint8_t const *id, model::AltLocations **list)
{
	std::string const key = HexOf(id);
	std::map<std::string, model::AltLocations>::iterator found = altLists.find(key);

	if (found == altLists.end())
	{
		model::AltLocations loaded;
		host::AltLocationsResult const result = host::AlternateLocations_Load(open, id, &loaded);

		if (result != host::AltLocationsResult::Done)
		{
			altWords = std::string("Alt Locations: ") + host::AltLocationsWords(result);
			statusWords = altWords.c_str();
			return false;
		}

		found = altLists.insert(std::make_pair(key, loaded)).first;
	}

	*list = &found->second;
	return true;
}

// The object's list with the shown location, for Object Placement's text (OP-101), read each frame: no status words; a list that does not load is not tried again until the level changes
static model::AltLocations const *PlacementAltShown(OpenLevelFile *open, uint8_t const *id)
{
	static std::string failedKey;
	static unsigned failedFor = 0;
	std::string const key = HexOf(id);
	const std::map<std::string, model::AltLocations>::const_iterator found = altLists.find(key);

	if (found != altLists.end())
	{
		return &found->second;
	}

	if (!open || (key == failedKey && failedFor == open->Generation()))
	{
		return nullptr;
	}

	model::AltLocations loaded;

	if (host::AlternateLocations_Load(open, id, &loaded) != host::AltLocationsResult::Done)
	{
		failedKey = key;
		failedFor = open->Generation();
		return nullptr;
	}

	return &altLists.insert(std::make_pair(key, loaded)).first->second;
}

// Writes the list into the object's ALTLOCAT; the object list is made again from the level. Gives back whether it was written.
static bool AltSave(OpenLevelFile *open, uint8_t const *id, model::AltLocations const &list)
{
	host::AltLocationsResult const result = host::AlternateLocations_Save(open, id, list);

	if (result != host::AltLocationsResult::Done)
	{
		altWords = std::string("Alt Locations: ") + host::AltLocationsWords(result);
		statusWords = altWords.c_str();
		altLists.erase(HexOf(id));
		return false;
	}
	LoadTheObjectListAgain();
	return true;
}

std::string ModuleNameOfIndex(int32_t index)
{
	uint8_t id[host::kSelectionIdBytes];
	host::FoundObject found;

	if (host::ModuleIdOfIndex(LevelObjects_Level(), index, id) && host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found))
	{
		return found.name;
	}

	return "module " + std::to_string(index);
}

// (CONSTRUCTION'S MODES: in construction_modes_screen.cpp)
// (THE SCREEN KEYS: in editor_screen_keys.cpp)
// (STRATEGY DATA'S SELECTION and the screens' picks and tints: in strategy_data_screen.cpp)

static bool AltPutOn(OpenLevelFile *open, uint8_t const *id, model::AltLocation const &location)
{
	host::AltLocationsResult const result = host::AlternateLocations_PutOnObject(open, id, location);

	if (result != host::AltLocationsResult::Done)
	{
		altWords = std::string("Alt Locations: ") + host::AltLocationsWords(result);
		statusWords = altWords.c_str();
		return false;
	}

	return true;
}

static void AltSay(char const *what, model::AltLocations const &list)
{
	altWords = std::string(what) + ": " + std::to_string((unsigned long long)list.list.size()) + " location" + (list.list.size() == 1 ? "" : "s") +
		(list.list.empty() ? std::string() : ", the shown one is " + std::to_string((unsigned long long)list.current + 1));
	statusWords = altWords.c_str();
}

// The number typed into inteng's number window (FUN_00474170) for Assign Group Number / Change shown location: a whole number; one it can't read leaves things as they were
static bool AltTypedWhole(std::string const &typed, int64_t *number)
{
	char *end = nullptr;

	errno = 0;

	long long const read = _strtoi64(typed.c_str(), &end, 10);

	if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0)
	{
		Answer("Type a whole number");
		return false;
	}

	*number = read;
	return true;
}

static void AltLocationCommand(int32_t command)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];
	model::AltLocations *list = nullptr;
	model::AltLocation now;

	// Change shown location (all objects) (0x69, OP-38): FUN_004dfb90 needs no current object; inteng's number window (FUN_00474170: 400 x 200, the words at 18,18, the number at
	// 30,100 300 x 18 starting at 0, Ok / Cancel), then every object with locations is put at that index
	if (command == 0x69)
	{
		DialogLine(std::string(), "Select alternate location index for all objects", "0", 10, 0x12, [](std::string const &typed)
		{
			int64_t number = 0;

			if (AltTypedWhole(typed, &number))
			{
				AltChangeShown(number);
			}
		});
		return;
	}

	int64_t specialGroup = 0;   // a current special object's own items (placement_special_alt.cpp; inteng's current object DAT_008f4f3c is the special one first)

	if (command == 0x66 && SpecialAlt_CurrentGroup(&specialGroup))
	{
		DialogLine(std::string(), "Select alternate location group number\n(0 means no group)", std::to_string((long long)specialGroup), 10, 0x12, [](std::string const &typed) { int64_t number = 0; if (AltTypedWhole(typed, &number)) { AltAssignGroup(number); } });
		return;
	}

	if (command != 0x66 && SpecialAlt_Command(command))
	{
		return;
	}

	if (!open || !SelectedObjectId(id))
	{
		statusWords = "Alt Locations: select an object first";
		return;
	}

	// Assign Group Number (0x66, OP-36): FUN_004dfa60's number window starts at the object's group (+0x38); Ok with a number 0 or more sets it
	if (command == 0x66)
	{
		if (!AltListOf(open, id, &list))
		{
			return;
		}

		DialogLine(std::string(), "Select alternate location group number\n(0 means no group)", std::to_string((long long)list->group), 10, 0x12, [](std::string const &typed)
		{
			int64_t number = 0;

			if (AltTypedWhole(typed, &number))
			{
				AltAssignGroup(number);
			}
		});
		return;
	}

	if (!AltListOf(open, id, &list) || host::AlternateLocations_ObjectNow(open, id, &now) != host::AltLocationsResult::Done)
	{
		return;
	}

	if (command == 0x64)   // Add new
	{
		if (!model::AltLocations_Add(list, now))
		{
			statusWords = "Alt Locations: there are as many locations as a chunk can say";
			return;
		}

		if (AltSave(open, id, *list))
		{
			AltSay("Location added", *list);
		}
	}
	else if (command == 0x65)   // Delete Current
	{
		if (list->list.empty())
		{
			statusWords = "Alt Locations: this object has none";
			return;
		}

		// FUN_004df6b0 puts the NEXT location (or the first, after the last) on the object before it empties a list that is left with one
		bool const haveNext = list->list.size() >= 2;
		size_t const nextAt = list->current + 1 < list->list.size() ? list->current + 1 : 0;
		model::AltLocation const next = haveNext ? list->list[nextAt] : model::AltLocation();

		model::AltLocations_DeleteCurrent(list);

		if (haveNext && !AltPutOn(open, id, next))
		{
			return;
		}

		if (AltSave(open, id, *list))
		{
			AltSay("Location deleted", *list);
		}
	}
	else if (command == 0x67 || command == 0x68)   // Next, Prev
	{
		if (list->list.empty())
		{
			statusWords = "Alt Locations: this object has none";
			return;
		}

		if (command == 0x67)
		{
			model::AltLocations_Next(list, now);
		}
		else
		{
			model::AltLocations_Prev(list, now);
		}

		if (!AltPutOn(open, id, list->list[list->current]))
		{
			return;
		}

		if (AltSave(open, id, *list))
		{
			AltSay(command == 0x67 ? "Next location" : "Previous location", *list);
		}
	}
}

// Assign Group Number: the typed number, taken when it is 0 or more (FUN_004dfa60)
static void AltAssignGroup(int64_t typed)
{
	if (SpecialAlt_AssignGroup(typed))   // the current special object's (placement_special_alt.cpp)
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];
	model::AltLocations *list = nullptr;

	if (!open || !SelectedObjectId(id) || !AltListOf(open, id, &list))
	{
		return;
	}

	if (!model::AltLocations_SetGroup(list, true, typed))
	{
		statusWords = "Alt Locations: the group number has to be 0 or more (0 means no group)";
		return;
	}

	if (list->list.empty())
	{
		// an empty list has no chunk (FUN_004dfc70): the number has nowhere to be kept in the file
		altWords = "Alt Locations: group " + std::to_string((long long)typed) + " set; this object has no locations to write it with";
		statusWords = altWords.c_str();
		return;
	}

	if (AltSave(open, id, *list))
	{
		altWords = "Alt Locations: the group number is " + std::to_string((long long)typed);
		statusWords = altWords.c_str();
	}
}

// Change shown location (all objects): every object with locations shows the typed one (FUN_004dfb90)
static void AltChangeShown(int64_t typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<uint8_t> ids;

	if (!open || typed < 0)
	{
		statusWords = "Alt Locations: type 0 or more";
		return;
	}

	if (host::AlternateLocations_ObjectsWithLocations(open, &ids) != host::AltLocationsResult::Done)
	{
		statusWords = "Alt Locations: no level is open";
		return;
	}

	size_t changed = 0;

	for (size_t at = 0; at + 8 <= ids.size(); at += 8)
	{
		model::AltLocations *list = nullptr;
		model::AltLocation now;

		if (!AltListOf(open, &ids[at], &list) || list->list.empty() ||
		    host::AlternateLocations_ObjectNow(open, &ids[at], &now) != host::AltLocationsResult::Done)
		{
			continue;
		}

		model::AltLocations_JumpTo(list, now, typed);

		if (!AltPutOn(open, &ids[at], list->list[list->current]) || !AltSave(open, &ids[at], *list))
		{
			return;
		}

		changed++;
	}

	changed += SpecialAlt_ChangeShown(open, typed);   // and the special objects' (placement_special_alt.cpp)
	altWords = "Alt Locations: " + std::to_string((unsigned long long)changed) + " objects show their location " + std::to_string((long long)typed);
	statusWords = altWords.c_str();
}

// ---- Construction's Object, Find Small Modules (inteng's case 0x2bd of FUN_004452b0 and FUN_0044be50, small_modules.h) ----
// The prompt for the limit, the search, the chooser, and the view's move to the one chosen. (Added 2026-11-15 by Melanikus (Frost).)
// OUR BUG, FIXED (TB-C4, construction-closeout-frost.md; ours until 2026-11-17 selected every small module found): FUN_0044be50 keeps the small modules in ITS OWN LIST, DAT_00631250 /
// DAT_00631254 (the found list; DAT_0063122c = &DAT_00631250, 0x444735), and never touches the selection (DAT_00631230 / DAT_00631234); the one chosen in the chooser is selected ALONE
// (FUN_00444750(module, 0) at 0x44c31f: with 0 the selection is let go of first) and the list's iterator DAT_00631228 stands on it (0x44c32c-0x44c349; the first one without a choice,
// 0x44c293). Then the hub's G steps to the list's next one and Shift+G to the one before (FUN_004452b0 0x446b11-0x446c38, round the list), each time the view to its place and it
// selected alone. The list is emptied by the next search and when the hub is left (FUN_004476d0's first loop).
extern double local_scale;   // the level's scale (declared again below, where Find uses it)
int32_t smallLimit = 500;                      // DAT_005cf510: the limit, in millimetres (its start is the data word 0x1f4 = 500, read from inteng.exe's data, 0x5cf510)
static std::vector<host::SmallModule> smallFound;      // the found list (DAT_00631250), in the level's order: the chooser's rows
static size_t smallAt = 0;                             // where the list's iterator stands (DAT_00631228)
static std::string smallWords;

// The hub's G / Shift+G (and the chooser): the found list's module at `at` becomes the current one ALONE and the view goes to its place
static void SmallModuleGoTo(size_t at)
{
	if (at >= smallFound.size())
	{
		return;
	}

	smallAt = at;

	host::SmallModule const &chosen = smallFound[at];
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

	smallWords = "Small module " + chosen.name + " (" + std::to_string((unsigned long long)at + 1) + " of " + std::to_string((unsigned long long)smallFound.size()) + "): the view is at its place";
	Answer(smallWords);
}

// G (forward) / Shift+G (back) on the hub, with the found list not empty (0x446b11: DAT_00631254 != 0)
static bool SmallModuleStep(bool back)
{
	if (smallFound.empty())
	{
		return false;
	}

	size_t const count = smallFound.size();

	SmallModuleGoTo(back ? (smallAt + count - 1) % count : (smallAt + 1) % count);
	return true;
}

// The hub is left (FUN_004476d0): the found list goes
static void SmallModulesForget()
{
	smallFound.clear();
	smallAt = 0;
}

// The number typed in the first prompt: the search runs and the chooser opens (or "No small module found.")
static void RunSmallPick(std::string const &typed);

static void RunSmallLimit(std::string const &typed)
{
	char *end = nullptr;

	errno = 0;

	double const metres = strtod(typed.c_str(), &end);

	if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0 || metres != metres)
	{
		Answer("Type a number of metres");
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	smallLimit = host::SmallModules_LimitFromMetres(metres);
	SmallModulesForget();   // FUN_0044be50 empties the found list first (0x44beae: DAT_00631254); the selection is not touched

	if (!open || host::SmallModules_Find(open, smallLimit, &smallFound) != host::SmallModulesResult::Done)
	{
		smallFound.clear();
		Answer("No level is open");
		return;
	}

	if (smallFound.empty())
	{
		DialogMessage("No small module found.");   // inteng's own words (0x5cfccc): the message box (FUN_00474030, 0x44c199)
		return;
	}

	smallAt = 0;   // the iterator on the list's first (0x44c293)

	// the chooser is inteng's list dialog "Small Modules : Select one to go to" (FUN_00475bf0, 0x44c2bc), every name in it, sorted by the dialog
	std::vector<std::string> rows;

	for (host::SmallModule const &one : smallFound)
	{
		rows.push_back(one.name);
	}

	DialogList("Small Modules : Select one to go to", rows, [](int32_t place)
	{
		if (place >= 0)
		{
			RunSmallPick(std::to_string((long long)place + 1));
		}
	});
}

// The number typed in the chooser: that module becomes the current one and the view goes to its place (FUN_0044be50 :0x44c2f1-0x44c351)
static void RunSmallPick(std::string const &typed)
{
	char *end = nullptr;

	errno = 0;

	long long const number = _strtoi64(typed.c_str(), &end, 10);

	if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0 || number < 1 || (unsigned long long)number > smallFound.size())
	{
		Answer("Type the number of a module in the list");
		return;
	}

	promptList.clear();
	SmallModuleGoTo((size_t)number - 1);   // selected alone, the view to it, the iterator on it (0x44c2fd-0x44c349)
}

// Construction's R, "Rotate object 90 degrees" (the hub's key list, 0x1cf54c; FUN_004452b0 0x4472aa-0x44750c, TB-C5): on a press of R (slot 0xd87a92, cleared after use) with something selected,
// the FIRST selected object's shape (FUN_004d0fd0) is turned a quarter turn about y, (x, y, z) -> (-z, y, x), and written (FUN_0050f570, FUN_004bd8e0); the level is changed (DAT_00631264).
// ShapeEdit_TurnQuarter does it (shape_edit.h: inteng's extents bug fixed there, a morphing shape refused). An object with no shape: nothing.
static std::string turnWords;

static void TurnTheSelectedQuarter()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];

	// the first of the selection set, else the one picked: a module a click picked is held by its game module number, its object found by its name and place (as Soft's)
	bool const picked = host::Selection_SetCount() != 0 ? host::Selection_SetMember(0, id) : SelectedObjectOrModuleId(id);

	if (!open || !open->Level() || !picked)
	{
		return;
	}

	std::string why;
	host::ShapeEditResult const result = host::ShapeEdit_TurnQuarter(open, open->Level(), id, &why);

	if (result == host::ShapeEditResult::NoShape || result == host::ShapeEditResult::NoObject)
	{
		return;
	}

	if (result != host::ShapeEditResult::Done)
	{
		turnWords = "Rotate object 90 degrees: not turned (" + (why.empty() ? std::string(host::ShapeEditWords(result)) : why) + ")";
		statusWords = turnWords.c_str();
		return;
	}

	// the game's own loaded shape turned the same way, so the view shows it at once (0x447474-0x447505): a module's by its game module number, any other object's by its ID
	LevelViewShapeOf drawn = {};

	memcpy(drawn.objectId, id, 8);

	for (int32_t m = 0; m < LevelView_ModuleCount(); m++)
	{
		uint8_t moduleId[8];

		if (ModuleObject(open, m, moduleId) && memcmp(moduleId, id, 8) == 0)
		{
			drawn.isModule = true;
			drawn.moduleIndex = m;
			break;
		}
	}

	bool const shown = LevelView_TurnObjectShape(drawn);

	turnWords = std::string("Rotate object 90 degrees: the selected object's shape is turned") + (shown ? "" : " (in the file; the view's shape was not found to turn)");
	statusWords = turnWords.c_str();
	LoadTheObjectListAgain();
}

// Construction's Object, Set Base Module (inteng's case 0x140, object_base.h): the selected object becomes the level's base object. (Added 2026-11-15 by Melanikus (Frost).)
static std::string baseWords;

static void SetTheBaseModule()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];

	if (!open || !SelectedObjectId(id))
	{
		statusWords = "Set Base Module: select one object first";
		return;
	}

	int changed = 0;
	host::BaseObjectResult const result = host::SetBaseObject(open, id, &changed);

	if (result != host::BaseObjectResult::Done)
	{
		baseWords = std::string("Set Base Module: ") + host::BaseObjectWords(result);
		statusWords = baseWords.c_str();
		return;
	}

	baseWords = "The selected object is the base module now (" + std::to_string(changed) + " headers changed; kept in the editor's bytes until Save a copy)";
	statusWords = baseWords.c_str();
}

// Construction's Object, Change Object Type: Module (0x12d) and Placed (0x12e): inteng's handlers run once for EVERY SELECTED OBJECT (object_type_edit.h).
// Placed asks "<name> has module linking data. Do you want to delete it?" for each object that has a MODULEDT, in the selection's order; yes destroys
// the data, anything else leaves THAT object as it was. Here the questions come first, then one change for all the objects (all or nothing).
// Esc answers "no" to the question it is on (inteng's dialog gave an answer that wasn't yes the same way). (Added 2026-11-15 by Melanikus (Frost).)
std::vector<host::ObjectTypeCall> typeCalls;     // the selected objects, with the answers so far
static std::vector<bool> typeHasLinks;                  // which of them hold a MODULEDT
size_t typeAsking = 0;                           // the object the open question is about (an index in typeCalls)
static std::string typeWords;

static void ApplyTheObjectType(host::ObjectTypeWhat what)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || typeCalls.empty())
	{
		statusWords = "Change Object Type: no level is held to work on";
		return;
	}

	std::vector<host::ObjectTypeOutcome> outcomes(typeCalls.size(), host::ObjectTypeOutcome::NoObject);
	host::ObjectTypeResult const result = host::ObjectType_Change(open, what, typeCalls.data(), typeCalls.size(), outcomes.data());

	if (result != host::ObjectTypeResult::Done)
	{
		typeWords = std::string("Change Object Type: ") + host::ObjectTypeWords(result);
		statusWords = typeWords.c_str();
		typeCalls.clear();
		return;
	}

	size_t changed = 0;
	size_t kept = 0;
	size_t missing = 0;

	for (host::ObjectTypeOutcome const outcome : outcomes)
	{
		if (outcome == host::ObjectTypeOutcome::Changed)
		{
			changed++;
		}
		else if (outcome == host::ObjectTypeOutcome::HasModuleData)
		{
			kept++;
		}
		else
		{
			missing++;
		}
	}

	if (changed != 0)
	{
		LoadTheObjectListAgain();
	}

	typeWords = std::string("Change Object Type: ") + std::to_string((unsigned long long)changed) + (what == host::ObjectTypeWhat::Module ? " object(s) are modules now" : " object(s) are placed objects now");

	if (kept != 0)
	{
		typeWords += std::string(", ") + std::to_string((unsigned long long)kept) + (what == host::ObjectTypeWhat::Module ? " left alone (they have module linking data)" : " left as they were (module linking data kept)");
	}

	if (missing != 0)
	{
		typeWords += std::string(", ") + std::to_string((unsigned long long)missing) + " not found in the level";
	}

	if (changed != 0)
	{
		typeWords += " (kept in the editor's bytes until Save a copy)";
	}

	statusWords = typeWords.c_str();
	typeCalls.clear();
}

// The next object that has module data and wasn't asked yet: its question opens. None left: the change is made.
static void AskTheNextTypeObject()
{
	while (typeAsking < typeCalls.size() && !typeHasLinks[typeAsking])
	{
		typeAsking++;
	}

	if (typeAsking >= typeCalls.size())
	{
		ApplyTheObjectType(host::ObjectTypeWhat::Placed);
		return;
	}

	OpenAPrompt(kPromptTypeLinks);
}

static void ChangeTheObjectType(int32_t which)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	size_t const selectedCount = host::Selection_SetCount();

	typeCalls.clear();

	if (!open || selectedCount == 0)
	{
		statusWords = "Change Object Type: select an object first";
		return;
	}

	for (size_t i = 0; i < selectedCount && i < host::kObjectTypeMostCalls; i++)
	{
		host::ObjectTypeCall call;

		memset(&call, 0, sizeof(call));

		if (host::Selection_SetMember(i, call.id))
		{
			typeCalls.push_back(call);
		}
	}

	if (typeCalls.empty())
	{
		statusWords = "Change Object Type: select an object first";
		return;
	}

	if (which == 1)
	{
		ApplyTheObjectType(host::ObjectTypeWhat::Module);
		return;
	}

	// Placed: which objects hold module data, and the question for each
	std::vector<uint8_t> ids;
	std::unique_ptr<bool[]> hasBool(new bool[typeCalls.size()]);

	for (host::ObjectTypeCall const &call : typeCalls)
	{
		ids.insert(ids.end(), call.id, call.id + 8);
	}

	if (!host::ObjectType_HaveModuleData(open, ids.data(), typeCalls.size(), hasBool.get()))
	{
		statusWords = "Change Object Type: the level couldn't be put together from its bytes";
		typeCalls.clear();
		return;
	}

	typeHasLinks.assign(typeCalls.size(), false);

	for (size_t i = 0; i < typeCalls.size(); i++)
	{
		typeHasLinks[i] = hasBool[i];
	}

	typeAsking = 0;
	AskTheNextTypeObject();
}

// Enter on the question: "y" or "yes" (any case) destroys the object's module data; anything else leaves the object as it was
void RunTypeAnswer(std::string const &typed)
{
	if (typeAsking >= typeCalls.size())
	{
		return;
	}

	std::string lower;

	for (char const letter : typed)
	{
		lower += (char)tolower((unsigned char)letter);
	}

	typeCalls[typeAsking].deleteModuleData = lower == "y" || lower == "yes";
	typeAsking++;
	AskTheNextTypeObject();
}

static void StopTheTypeQuestions()
{
	// Esc: "no" to this object, and the questions go on
	RunTypeAnswer(std::string());
}

// Strategy mode (inteng's screen 9, FUN_00524e90): an item's command number comes here (strategy_mode.h; the numbers are strategy-menu-pairs.md's). Delete
// Strategy (0xc, FUN_00528790) asks inteng's question and, only on yes, destroys the current object's strategy chunk. Nothing else is built yet: any other number says
// so. (Added 2026-11-15 by Melanikus (Frost).)
std::string strategyWords;
uint8_t strategyDeleteId[8];

static int32_t ModuleGameIndexOfObject(uint8_t const *id)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	for (int32_t m = 0; open && m < LevelView_ModuleCount(); m++)
	{
		uint8_t other[8];

		if (ModuleObject(open, m, other) && memcmp(other, id, 8) == 0)
		{
			return m;
		}
	}

	return -1;
}

void StabilityBegin()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::vector<uint8_t> const ids = PickedModuleIdBytes();
	uint32_t faces = 0;

	if (!open || ids.empty())
	{
		statusWords = "Stability flags: pick a module first (click, Shift+click for more)";
		return;
	}

	host::StrategyModeResult const got = host::StrategyMode_GetStability(open, ids.data(), &faces);

	if (got != host::StrategyModeResult::Done)
	{
		strategyWords = std::string("Stability flags: ") + host::StrategyModeWords(got);
		statusWords = strategyWords.c_str();
		return;
	}

	int32_t const index = ModuleGameIndexOfObject(ids.data());

	if (index < 0 || !LevelView_ModuleBox(index, stabilityMin, stabilityMax))
	{
		statusWords = "Stability flags: the game has no shape for that module to draw the box of";
		return;
	}

	memcpy(stabilityObject, ids.data(), sizeof(stabilityObject));
	stabilityFaces = faces;
	stabilityPolygons = false;   // FUN_00447f10: the faces start as polylines (strategy-data.md S-75)
	stabilityActive = true;
	strategyEditFan = false;
	copyCollecting = false;
	statusWords = "Stability flags: click a face to turn it red or green; Return ends";
}

static void StabilityToggleLines()
{
	stabilityPolygons = !stabilityPolygons;
}

static void StabilityClick(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	host::PickRay ray;

	if (!PickGame_RayAtClick(x, y, viewWidth, viewHeight, &ray))
	{
		return;
	}

	double tmin = -1.0e30, tmax = 1.0e30;
	int enterAxis = -1, exitAxis = -1;
	bool enterIsMin = true, exitIsMax = true;

	for (int a = 0; a < 3; a++)
	{
		double const d = (double)ray.direction[a], o = (double)ray.from[a], lo = (double)stabilityMin[a], hi = (double)stabilityMax[a];

		if (d == 0.0)
		{
			if (o < lo || o > hi)
			{
				return;
			}

			continue;
		}

		double const t1 = (lo - o) / d, t2 = (hi - o) / d;
		double const nearT = t1 < t2 ? t1 : t2, farT = t1 < t2 ? t2 : t1;

		if (nearT > tmin)
		{
			tmin = nearT;
			enterAxis = a;
			enterIsMin = d > 0.0;
		}

		if (farT < tmax)
		{
			tmax = farT;
			exitAxis = a;
			exitIsMax = d > 0.0;
		}
	}

	if (tmin > tmax || tmax < 0.0 || exitAxis < 0)
	{
		return;
	}

	int const face = (tmin >= 0.0 && enterAxis >= 0) ? 2 * enterAxis + (enterIsMin ? 0 : 1) : 2 * exitAxis + (exitIsMax ? 1 : 0);

	stabilityFaces ^= 1u << face;
}

static void StabilityEnd()
{
	stabilityActive = false;
	DialogConfirm("Would you like to keep the changes?", [](bool yes)   // 0x5ee000, FUN_00474a40
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!yes || !open)
		{
			return;
		}

		host::StrategyModeResult const result = host::StrategyMode_SetStability(open, stabilityObject, stabilityFaces);

		if (result != host::StrategyModeResult::Done)
		{
			strategyWords = std::string("Stability flags: ") + host::StrategyModeWords(result);
			statusWords = strategyWords.c_str();
			return;
		}

		LoadTheObjectListAgain();

		// FUN_00527210 0x527329: Yes saves the level at once (FUN_005282d0, with every switch's Links made again: S-K61-3), as Save does (strategy-data.md S-78)
		StrategySave();
		strategyWords = std::string("Stability flags: the six faces are written into the module's shape; ") + savedWords;
		statusWords = strategyWords.c_str();
	});
}

static void StabilityShapes(std::vector<host::TrackLine> *lines, std::vector<LevelViewTriangle> *triangles)
{
	for (int face = 0; face < 6; face++)
	{
		int const a = face / 2, b = (a + 1) % 3, c = (a + 2) % 3;
		int32_t const plane = (face & 1) ? stabilityMax[a] : stabilityMin[a];
		int32_t const bLo = stabilityMin[b], bHi = stabilityMax[b];
		int32_t const cLo = stabilityMin[c], cHi = stabilityMax[c];
		uint32_t const colour = (stabilityFaces & (1u << face)) ? 0xFFFF0000u : 0xFF00FF00u;
		int32_t const corner[4][2] = { { bLo, cLo }, { bHi, cLo }, { bHi, cHi }, { bLo, cHi } };

		if (stabilityPolygons)
		{
			// the face as two filled triangles
			int const split[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };

			for (auto const &part : split)
			{
				LevelViewTriangle triangle;

				triangle.colour = colour;
				triangle.a[a] = triangle.b[a] = triangle.c[a] = plane;
				triangle.a[b] = corner[part[0]][0]; triangle.a[c] = corner[part[0]][1];
				triangle.b[b] = corner[part[1]][0]; triangle.b[c] = corner[part[1]][1];
				triangle.c[b] = corner[part[2]][0]; triangle.c[c] = corner[part[2]][1];
				triangles->push_back(triangle);
			}

			continue;
		}

		for (int k = 0; k < 4; k++)
		{
			int const from = k, to = (k + 1) % 4;
			host::TrackLine line;

			line.colour = colour;
			line.from[a] = line.to[a] = plane;
			line.from[b] = corner[from][0];
			line.from[c] = corner[from][1];
			line.to[b] = corner[to][0];
			line.to[c] = corner[to][1];
			lines->push_back(line);
		}
	}
}

static void CopyNext(size_t index);
bool StrategyReplaceableWithoutAsking(std::vector<uint8_t> const &payload);
static void CopyDo(size_t index);

// FUN_00528340: the answer kept from the last replace question (2 at the start, 0x528436); "Yes All" (10) and "No All" (0xb) hold for the rest of the list (0x5284a7 / 0x5284b0)
static int32_t copyAnswer = 2;

// Copy's replace question (FUN_00529f30, strategy-data.md S-80): 400 x 200, no title, the words "%s already has a strategy\nDo you want to replace it?" (0x5ee1b4) at (0x12, 0x12) 0x172 x 100,
// four 0x32 x 0x10 buttons at y 0x96: "Yes All" (0x5ee5a8) x 0x28 answers 10, "Yes" x 0x82 answers 3, "No" x 0xdc answers 2, "No All" (0x5ee5a0) x 0x136 answers 0xb (the class's method 0x52a100);
// Escape answers 1 (FUN_0057c800 0x57cca4). 2 and 0xb pass the target by; any other answer (Escape too) replaces its strategy (0x5284fb)
static void CopyQuestion(size_t index, std::string const &name)
{
	host::DialogFormSpec spec;

	spec.title = "";
	spec.width = 400;
	spec.height = 200;

	host::DialogWidget words;

	words.type = host::DialogWidgetType::Label;
	words.box.x = 0x12;
	words.box.y = 0x12;
	words.box.width = 0x172;
	words.box.height = 100;
	words.text = name + " already has a strategy\nDo you want to replace it?";
	spec.widgets.push_back(words);

	struct Answer { char const *label; int32_t x; int32_t answer; };
	static Answer const answers[4] = { { "Yes All", 0x28, 10 }, { "Yes", 0x82, 3 }, { "No", 0xdc, 2 }, { "No All", 0x136, 0xb } };

	for (Answer const &one : answers)
	{
		host::DialogWidget button;

		button.type = host::DialogWidgetType::Button;
		button.box.x = one.x;
		button.box.y = 0x96;
		button.box.width = 0x32;
		button.box.height = 0x10;
		button.text = one.label;
		button.answer = one.answer;
		spec.widgets.push_back(button);
	}

	host::Dialogs_Form(spec, [index](int32_t answer, std::vector<host::DialogWidget> const &)
	{
		copyAnswer = answer;

		if (answer == 2 || answer == 0xb)
		{
			CopyNext(index + 1);
		}
		else
		{
			CopyDo(index);
		}
	});
}

static void CopyFinished()
{
	LoadTheObjectListAgain();
	strategyWords = "Copy Strategy: " + std::to_string((unsigned long long)copyDone) + " object(s) got the strategy (kept in the editor's bytes until Save a copy)";
	statusWords = strategyWords.c_str();
	copyTargets.clear();
}

host::ObjectEditResult WriteStrategyBytes(OpenLevelFile *open, uint8_t const *id, std::vector<uint8_t> const &bytes);   // below, with the views

// One target gets the copy (0x528567): the source kind's own copy (its +0xc; model::CopyStrategyAsTheOldEditor, close-out F-2: a Door, Placed Object or Light Object without the head's Type and
// ExtraData, a switch's links emptied, a Linked Switch kept a Linked Switch, INTENG BUG, FIXED), made from the source's strategy as edited so far and written through WriteStrategyBytes, so a
// VIRTUAL object (where most switches live) is a source or a target as a placed object or a module is (inteng's Copy takes any object that has a wrapper)
static void CopyDo(size_t index)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::ObjectStrategy source;
	std::vector<uint8_t> copy;
	size_t length = 0;
	bool made = false;

	if (open && host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), copyFrom, &source) == host::StrategyFound::Found)
	{
		copy.resize(source.payload.size());   // no kind's copy is longer than its source
		made = model::CopyStrategyAsTheOldEditor(source.payload.data(), source.payload.size(), copy.data(), copy.size(), &length);
		copy.resize(made ? length : 0);
	}

	if (!made || WriteStrategyBytes(open, copyTargets[index].id, copy) != host::ObjectEditResult::Done)
	{
		StrategyMessage("Unable to copy this strategy");   // 0x5ee194: the rest is given up
		LoadTheObjectListAgain();
		copyTargets.clear();
		return;
	}

	copyDone++;
	CopyNext(index + 1);
}

static void CopyNext(size_t index)
{
	for (; index < copyTargets.size(); index++)
	{
		uint8_t const *const id = copyTargets[index].id;

		if (memcmp(id, copyFrom, 8) == 0)
		{
			continue;
		}

		// the target's class's method +8 with the source's kind (0x52846a): it takes the chooser's kinds and more (KindsTheObjectCanTake's note): a Binary Switch 52 on a placed or a virtual object,
		// 53 and 63 on a placed object, 60 on a module. The chooser's list says which class: only a virtual object takes a Mission Objective (70), only a placed object a Placed Object (62), only a
		// module a Light Object (64)
		int32_t kinds[16];
		size_t const count = KindsTheObjectCanTake(id, kinds);
		bool can = false, isVirtual = false, isPlaced = false, isModule = false;

		for (size_t k = 0; k < count; k++)
		{
			can = can || kinds[k] == copyKind;
			isVirtual = isVirtual || kinds[k] == 70;
			isPlaced = isPlaced || kinds[k] == 62;
			isModule = isModule || kinds[k] == 64;
		}

		can = can || ((isVirtual || isPlaced) && copyKind == 52) || (isPlaced && (copyKind == 53 || copyKind == 63)) || (isModule && copyKind == 60);

		if (!can)
		{
			continue;
		}

		host::ObjectStrategy now;

		// the target's strategy as edited so far (the store); one FUN_00521860 calls replaceable goes without a question (0x528497), "No All" passes the rest by, "Yes All" replaces them (0x5284a7 / 0x5284b0)
		if (host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(LevelObjects_OpenForEdit()), id, &now) == host::StrategyFound::Found && !StrategyReplaceableWithoutAsking(now.payload))
		{
			if (copyAnswer == 0xb)
			{
				continue;
			}

			if (copyAnswer != 10)
			{
				host::FoundObject found;
				std::string const name = host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &found) ? found.name : std::string("The object");

				CopyQuestion(index, name);
				return;
			}
		}

		CopyDo(index);
		return;
	}

	CopyFinished();
}

// The collector's Ctrl + click (FUN_00526bf0 0x526e3b-0x526ecc, S-79): only on an object of the level, a record with an object (+0x154); on anything else (a virtual object's icon, which has
// none) the click does nothing at all (0x526e43), not even the plain click's add. The clicked object's shape is FUN_004d0fd0's (Object_Chunk::get_assoc_shape: the header's shape, none without
// a header); every object of the level whose shape is that one (the record table 0x93d120 in its order, from the level's first record DAT_008f588c), the clicked one too, joins the list
// when it is not in it, RED. Every such record has a wrapper (FUN_00521cb0 makes one for each record with an object), so each is a target the copy can deal with. The group is
// LevelObjects_ShapeGroup's (level_objects.h); none (a virtual object's icon) adds nothing.
static void CopyCollectShapeGroup(uint8_t const *clicked)
{
	std::vector<host::LevelObject> group;

	if (!LevelObjects_ShapeGroup(clicked, &group))
	{
		return;
	}

	for (host::LevelObject const &member : group)
	{
		CopyTarget one = {};
		bool listed = false;

		memcpy(one.id, member.id, sizeof(one.id));

		for (CopyTarget const &target : copyTargets)
		{
			listed = listed || memcmp(target.id, one.id, 8) == 0;
		}

		if (!listed)
		{
			copyTargets.push_back(one);
		}
	}
}

static void CopyCollectClick(host::PickReport const &report, bool shift, bool ctrl, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	CopyTarget one = {};
	bool have = false;

	if (open && open->Level())
	{
		if (report.found == host::PickFound::Module)
		{
			have = ModuleObject(open, report.moduleIndex, one.id);
		}
		else if (report.found == host::PickFound::Object)
		{
			memcpy(one.id, report.objectId, sizeof(one.id));
			have = true;
		}
	}

	// an icon nearer than what the level's pick met is what the click took (FUN_00526bf0 takes any object that has a wrapper: a virtual object's V and the like)
	if (open && open->Level())
	{
		bool const levelHit = report.found == host::PickFound::Module || report.found == host::PickFound::Object;

		have = StrategyIconUnderClick(x, y, viewWidth, viewHeight, levelHit ? report.distance : -1, one.id) || have;
	}

	if (!have)
	{
		return;
	}

	// Shift is read first (0x526d7c: Shift takes the clicked one out, whatever Ctrl is); then Ctrl (0x526e33) adds the shape group
	if (!shift && ctrl)
	{
		CopyCollectShapeGroup(one.id);
		return;
	}

	for (size_t i = 0; i < copyTargets.size(); i++)
	{
		if (memcmp(copyTargets[i].id, one.id, 8) == 0)
		{
			if (shift)
			{
				copyTargets.erase(copyTargets.begin() + (long)i);
			}

			return;
		}
	}

	if (!shift)
	{
		copyTargets.push_back(one);
	}
}

// Return or the right button: the collector ends and the targets are dealt with
static void CopyCollectEnd()
{
	copyCollecting = false;

	if (copyPurpose == 1)
	{
		LiftPositionsEnd();   // a Lift's positions (FUN_0052b360), not Copy Strategy's targets
		return;
	}

	if (copyPurpose == 2)
	{
		MissionTargetsEnd();   // a Mission Objective's alteration targets (FUN_00532ea0)
		return;
	}

	if (copyPurpose == 3)
	{
		TrackTargetsEnd();   // a Track Object's point page targets (FUN_00534df0)
		return;
	}

	if (copyPurpose == 4)
	{
		MultiTargetsEnd();   // a Multiple Target Switch's targets (FUN_0052ff10)
		return;
	}

	copyDone = 0;
	copyAnswer = 2;   // 0x528436
	CopyNext(0);
}
// (STRATEGY: in strategy_screen.cpp)

// (PATH MODE: in path_screen.cpp)

// Enter on the name of the new virtual object (FUN_005396d0): the answer makes it; a name already in use gives inteng's "Please try another name" and the question again
static void RunVirtualName(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::VirtualObjectResult const result = open ? host::VirtualObject_Create(open, typed.c_str(), virtualPlace, local_scale > 0.0 ? local_scale : 1.0) : host::VirtualObjectResult::BadCall;

	if (result == host::VirtualObjectResult::NameUsed || result == host::VirtualObjectResult::BadName)
	{
		strategyWords = std::string("Create Virtual Object: ") + host::VirtualObjectWords(result);
		statusWords = strategyWords.c_str();

		if (result == host::VirtualObjectResult::NameUsed)
		{
			// FUN_005396d0 0x5399a1: the box "Please try another name" (0x5f0298, FUN_00474030 + FUN_0057c800), then the name is asked again
			DialogMessage("Please try another name", []() { TextAs(kPromptVirtualName, "Type name for new virtual object", ""); });
			return;
		}

		TextAs(kPromptVirtualName, "Type name for new virtual object", "");   // 0x5f02b0, FUN_00474bc0: a text dialog (Ok / Cancel)
		return;
	}

	if (result != host::VirtualObjectResult::Done)
	{
		strategyWords = std::string("Create Virtual Object: ") + host::VirtualObjectWords(result);
		statusWords = strategyWords.c_str();
		return;
	}

	LoadTheObjectListAgain();

	strategyWords = "Virtual object " + typed + " is in the level (kept in the editor's bytes until Save a copy); the game uses it once a strategy is assigned to it";
	statusWords = strategyWords.c_str();
}


// The words of the prompt's lines
void PromptWords(std::string *title, std::string *hint)
{
	switch (promptFor)
	{
		case kPromptFind:
			*title = "Find: type part of an object's name, or its 16-digit ID";
			break;
		case kPromptFindSelect:
			*title = "Select Object: type part of an object's name, or its 16-digit ID";
			break;
		case kPromptAsc:
		case kPromptAscDoors:
			*title = "Write asc file: type the filename (without .asc extension)";
			break;
		case kPromptWriteC:
			*title = "Write .c file: type the filename for the shape (without .c extension)";
			break;
		case kPromptMove:
			*title = "Move relative: type how far to move it, as three whole numbers: x y z";
			break;
		case kPromptSmallLimit:
			*title = "Search for modules with x or z extents smaller than what?(in m)   now: " + std::to_string(host::SmallModules_MetresOfLimit(smallLimit)) + " m";
			break;
		case kPromptSmallPick:
			*title = "Small Modules : Select one to go to (type its number)";
			break;
		case kPromptPathName:
			*title = "Name of path (optional)";   // inteng's words (0x5e982c)
			break;
		case kPromptPathPick:
			*title = "Select Path: type the number of the path to work on";
			break;
		case kPromptPathDelete:
			*title = "Delete this path?  (type yes or no)";   // inteng's words (0x5e9818)
			break;
		case kPromptPathProps:
			*title = "Path Properties: type Loop path or Go back and forth";   // inteng's words (0x5e98c8, 0x5e98bc, 0x5e98a8)
			break;
		case kPromptExitSave:
			*title = "Would you like to save first?  (type yes or no)";   // inteng's words (0x5edd8c)
			break;
		case kPromptTrackTime:
			*title = "Time for section: type the seconds";   // inteng's words (0x5e9340)
			break;
		case kPromptTrackFlags:
			*title = "Track Properties";   // inteng's dialog (0x5e9498)
			break;
		case kPromptTrackSound:
			*title = trackPromptSound == 0 ? "Track Sound" : (trackPromptSound == 1 ? "Start Sound" : "End Sound");
			break;
		case kPromptTrackDelete:
			*title = "Delete this track?  (type yes or no)";
			break;
		case kPromptDestroyPick:
			*title = "Destroy Virtual Object: type the number of the virtual object";
			break;
		case kPromptDestroyAsk:
			*title = "Delete this object?  (type yes or no)";   // inteng's words (0x5eddec)
			break;
		case kPromptAssignKind:
			*title = "Assign Strategy: type the number of the strategy";
			break;
		case kPromptAssignReplace:
			*title = "Object already has a strategy. Would you like to replace it?  (type yes or no)";   // inteng's words (0x5ee2b4)
			break;
		case kPromptDeleteMulti:
			*title = "Select Strategies To Delete: type the numbers of the groups (for example 1 3)";   // inteng's words (0x5f027c)
			break;
		case kPromptVirtualName:
			*title = "Type name for new virtual object";   // inteng's words (0x5f02b0)
			break;
		case kPromptStability:
			*title = "Stability flags: six letters for the faces, r red (stable) or g green (unstable)";
			break;
		case kPromptStrategyMessage:
			*title = "Type in a message number from 1 to 127";   // inteng's words (0x5edf94); 0 is taken too, as FUN_00525700 does
			break;
		case kPromptStrategyDelete:
		{
			// inteng's words (the string at 0x5ee254, line break kept as a space on one line); the answer is typed here
			std::string question = host::StrategyMode_DeleteQuestion();

			for (char &letter : question)
			{
				letter = letter == '\n' ? ' ' : letter;
			}

			*title = question + "  (type yes or no)";
			break;
		}
		case kPromptStrategyFans:
			*title = std::string(host::StrategyMode_ResetFansQuestion()) + "  (type yes or no)";
			break;
		case kPromptTypeLinks:
		{
			// inteng's words (FUN_00448530: "%s has module linking data. Do you want to delete it?"); the answer is typed here
			host::FoundObject asked;
			bool const found = typeAsking < typeCalls.size() && host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(typeCalls[typeAsking].id).c_str(), &asked);

			*title = (found ? asked.name : std::string("The object")) + " has module linking data. Do you want to delete it?  (type yes or no)";
			break;
		}
		case kPromptCreateName:
			*title = "Name Object: type the name of the new object (a name no other object has)";
			break;
		case kPromptHierarchy:
			*title = hierarchyTitle;
			break;
		case kPromptSpecial:
			*title = specialTitle;
			break;
		case kPromptTears:
			*title = tearAll ? "Fixing tears for ALL objects - type in tolerance level for tear fixing (in mm)" : "Fixing tears for selected objects - type in tolerance level for tear fixing (in mm)";
		case kPromptTextureMap:
			*title = textureAsk >= 4 ? TextureAnimTitle(textureAsk) : textureAsk == 1 ? "Select polygons based on angle away from vertical. Please enter an angle from 0 to 180" : textureAsk == 3 ? "Select number of repeats of texture to use: type the repeats across and down, each 1 to 10 or inf (now " + std::string(textureRepeatX == 11 ? "inf" : std::to_string(textureRepeatX)) + " " + (textureRepeatY == 11 ? "inf" : std::to_string(textureRepeatY)) + "; 1 1 is the picture itself)" : "Arrow Selection Step Size: type the vertical step and the horizontal step (whole numbers, at least 1; now " + std::to_string(textureArrowVertical) + " " + std::to_string(textureArrowHorizontal) + ")";
			break;
		case kPromptChooserPick:
			*title = "Select shape: type its place in the list (1 to the count) or part of its name";
			break;
		case kPromptWaypointExtents:
			*title = "Edit Extents: type the box as six numbers (lowest x y z, highest x y z, in millimetres), or aim x y z, or reset";
			break;
		case kPromptRegenAdjacency:
			*title = "Do you want to regenerate adjacent module data? This will mess up any one-way adjacencies that have been set up. Type yes to do it";
			break;
		case kPromptVisibility:
			*title = "Visible distance in metres: type a whole number (now " + std::to_string(visibleDistance / 100000) + "; 0 keeps every module seen)";
			break;
		case kPromptDeleteAllLinks:
			*title = "Delete all module linking for the whole level? Type yes to do it (inteng's own words for this are not read)";
			break;
		case kPromptSoftEdge:
			*title = "Type in the maximum angle for soft edges (flat is 0).";
			break;
		default:
			*title = "Name object: type its new name";
			break;
	}

	*hint = "Enter: do it     Esc: cancel";
}

// The level's scale, as the game's loader left it (chunk_load.cpp:63): a file place times it is the game's world place (projload.cpp:2248)
extern double local_scale;

// Enter: what was typed is acted on. Whatever happens, the prompt closes, with the answer on the status line.
void RunThePrompt()
{
	int32_t const kind = promptFor;
	std::string const typed = promptText;
	std::string why;
	uint8_t id[8];

	promptFor = kPromptNone;
	promptText.clear();
	formRows.clear();

	if (kind == kPromptAsc || kind == kPromptAscDoors)
	{
		WriteTheAsc(typed, kind == kPromptAscDoors);
		return;
	}

	if (kind == kPromptWriteC)
	{
		WriteTheC(typed);   // write_c_file.h
		return;
	}

	if (kind == kPromptFind || kind == kPromptFindSelect)
	{
		host::FoundObject found;

		if (!host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, typed.c_str(), &found))
		{
			Answer("No object matches \"" + typed + "\"");
			return;
		}

		// The view goes to the object's place. The look and the turn stay as they are. Object Placement's Find doesn't select, as inteng's didn't; Construction's Find Object (0x52) does
		// (FUN_004452b0 0x446636-0x44696e: the view's place = the object's, then FUN_00444750 selects it).
		if (kind == kPromptFindSelect)
		{
			host::Selected picked = {};

			picked.kind = host::SelectedKind::Object;
			memcpy(picked.objectId, found.id, host::kSelectionIdBytes);
			host::Selection_Set(picked);
		}

		if (theCamera)
		{
			theCamera->x = (int)(found.x * local_scale);
			theCamera->y = (int)(found.y * local_scale);
			theCamera->z = (int)(found.z * local_scale);
			FlyCamera_Hold(theCamera);
		}

		Answer("Found " + found.name + (found.matches > 1 ? " (first of " + std::to_string(found.matches) + ")" : std::string()) +
			": the view is at its place");
		return;
	}

	if (kind == kPromptTypeLinks)
	{
		RunTypeAnswer(typed);
		return;
	}

	if (kind == kPromptStrategyDelete)
	{
		RunStrategyDelete(typed);
		return;
	}

	if (kind == kPromptStrategyFans)
	{
		RunStrategyFans(typed);
		return;
	}

	if (kind == kPromptSmallLimit)
	{
		RunSmallLimit(typed);
		return;
	}

	if (kind == kPromptSmallPick)
	{
		RunSmallPick(typed);
		return;
	}

	if (kind == kPromptPathName || kind == kPromptPathPick || kind == kPromptPathDelete || kind == kPromptPathProps)
	{
		promptList.clear();
		RunPathPrompt(kind, typed);
		return;
	}

	if (kind == kPromptExitSave)
	{
		RunExitSavePrompt(typed);
		return;
	}

	if (kind == kPromptTrackTime || kind == kPromptTrackFlags || kind == kPromptTrackSound || kind == kPromptTrackDelete)
	{
		promptList.clear();
		RunTrackPrompt(kind, typed);
		return;
	}

	if (kind == kPromptDestroyPick || kind == kPromptDestroyAsk)
	{
		promptList.clear();
		RunDestroyPrompt(kind, typed);
		return;
	}

	if (kind == kPromptAssignKind || kind == kPromptAssignReplace)
	{
		promptList.clear();
		RunAssignPrompt(kind, typed);
		return;
	}

	if (kind == kPromptDeleteMulti)
	{
		promptList.clear();
		RunDeleteMulti(typed);
		return;
	}

	if (kind == kPromptVirtualName)
	{
		RunVirtualName(typed);
		return;
	}

	if (kind == kPromptStability)
	{
		promptList.clear();
		RunStrategyStability(typed);
		return;
	}

	if (kind == kPromptStrategyMessage)
	{
		RunStrategyMessage(typed);
		return;
	}

	if (kind == kPromptChooserPick)
	{
		ShapeChooserPick(typed);
		return;
	}

	if (kind == kPromptHierarchy)
	{
		HierarchyTypedAnswer(typed);
		return;
	}

	if (kind == kPromptSpecial)
	{
		SpecialTypedAnswer(typed);
		return;
	}

	if (kind == kPromptTears)
	{
		FixTearsDo(typed);
		return;
	}

	if (kind == kPromptTextureMap)
	{
		TextureMapTypedAnswer(typed);
		return;
	}

	if (kind == kPromptCreateName)
	{
		CreateNamedAtTheView(typed);
		return;
	}

	if (kind == kPromptWaypointExtents)
	{
		host::WaypointContext context;

		context.open = LevelObjects_OpenForEdit();
		context.pickedModule = -1;

		host::WaypointOutcome const outcome = waypointMode.ApplyExtentsText(typed, context);

		if (outcome.levelChanged)
		{
			LoadTheObjectListAgain();
		}

		Answer(outcome.say);
		return;
	}

	if (kind == kPromptRegenAdjacency)
	{
		if (_stricmp(typed.c_str(), "yes") == 0 || _stricmp(typed.c_str(), "y") == 0)
		{
			AdjacencyWithMessage(false);   // "Calculating adjacency data" (0x4c1eb1), then FUN_004c5a30(0)
		}
		else
		{
			Answer("Exit + All Adjacencies: the adjacent module data was left as it was (it wants yes)");
		}

		return;
	}

	if (kind == kPromptVisibility)
	{
		// Set Visibility (0xa4, FUN_004c3620): the typed whole number of metres (FUN_00474340) times 100000 is the distance AutoLink keeps modules within. Ours: a
		// number that cannot be read, is below 0 or is above 20000 metres leaves it as it was (inteng's box takes any int; the product would overflow past 21474).
		char *end = nullptr;

		errno = 0;

		long long const metres = _strtoi64(typed.c_str(), &end, 10);

		if (typed.empty() || end == typed.c_str() || *end != '\0' || errno != 0 || metres < 0 || metres > 20000)
		{
			Answer("Set Visibility: type a whole number of metres, 0 to 20000 (0 keeps every module seen)");
			return;
		}

		visibleDistance = (int32_t)(metres * 100000);
		Answer("Visible distance: " + std::to_string(metres) + " metres" + (metres == 0 ? " (no limit)" : ""));
		return;
	}

	if (kind == kPromptDeleteAllLinks)
	{
		// inteng asks "Delete all module linking for th..." and goes on only on its OK (FUN_0057c800 == 3); here the answer is typed
		if (_stricmp(typed.c_str(), "yes") == 0 || _stricmp(typed.c_str(), "y") == 0)
		{
			DeleteModuleLinks(true);
		}
		else
		{
			Answer("Delete Links: nothing deleted (it wants yes)");
		}

		return;
	}


	if (kind == kPromptSoftEdge ? !SelectedObjectOrModuleId(id) : !SelectedObjectId(id))
	{
		Answer("No object is selected any more");
		return;
	}

	if (kind == kPromptSoftEdge)
	{
		// Edge > Soft (FUN_004af810): the typed angle is read as a whole number (sscanf %d) and taken when it is 0 to 180; the vertex normals of the selected object's shape are made again, and the view is lit again
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		int angle = 0;

		if (!open || !host::ShapeSoften_ReadAngle(typed, &angle))
		{
			Answer("Soft: type a whole number from 0 to 180 (flat is 0)");
			return;
		}

		size_t changed = 0;
		std::string reason;
		host::ShapeSoftenResult const result = host::ShapeSoften_ByAngle(open, LevelObjects_Level(), id, angle, local_scale > 0.0 ? local_scale : 1.0, &changed, &reason);

		if (result != host::ShapeSoftenResult::Done)
		{
			Answer(std::string("Soft: ") + host::ShapeSoftenWords(result) + (reason.empty() ? std::string() : " (" + reason + ")"));
			return;
		}

		RelightTheView();
		trackDirty = true;
		Answer("Soft edges at " + std::to_string(angle) + " degrees: " + std::to_string(changed) + " points have a new normal, the view is lit again (kept in the editor's bytes until Save a copy)");
		return;
	}

	if (kind == kPromptMove)
	{
		long long by[3] = { 0, 0, 0 };
		const char *at = typed.c_str();
		bool good = true;

		for (int i = 0; i < 3 && good; i++)
		{
			char *end = nullptr;

			errno = 0;
			by[i] = _strtoi64(at, &end, 10);
			good = end != at && errno == 0;
			at = end;

			while (good && (*at == ' ' || *at == ',' || *at == '\t'))
			{
				at++;
			}
		}

		if (!good || *at != '\0')
		{
			Answer("Move relative wants three whole numbers, such as: 0 0 -500");
			return;
		}

		if (host::ObjectEdit_Move(editedLevel, LevelObjects_Level(), id, by[0], by[1], by[2], &why) != host::ObjectEditAnswer::Done)
		{
			Answer("Not moved: " + why);
			return;
		}

		host::FoundObject now;

		if (host::ObjectEdit_Find(LevelObjects_Level(), editedLevel, HexOf(id).c_str(), &now))
		{
			Answer("Moved " + now.name + " to " + std::to_string(now.x) + " " + std::to_string(now.y) + " " + std::to_string(now.z) +
				" (kept in the editor's bytes; the view shows the level as it was opened)");
		}

		return;
	}

	uint8_t newId[8];

	if (host::ObjectEdit_Rename(editedLevel, LevelObjects_Level(), id, typed.c_str(), newId, &why) != host::ObjectEditAnswer::Done)
	{
		Answer("Not renamed: " + why);
		return;
	}

	// The rename made the level again (RenameObject, a structural edit, 11-17): the list is made again and the field panel lets go of the old level, and the selection
	// follows the object
	LoadTheObjectListAgain();

	host::Selected renamed = {};

	renamed.kind = host::SelectedKind::Object;
	memcpy(renamed.objectId, newId, host::kSelectionIdBytes);
	host::Selection_Set(renamed);
	Answer("Named " + typed + " (its ID is now " + HexOf(newId) + ")");
}

static void PromptKey(UINT message, WPARAM key)
{
	if (message == WM_CHAR)
	{
		if (key >= 32 && key <= 126 && promptText.size() < host::kObjectEditTextMost)
		{
			if ((hierarchyPromptFresh && promptFor == kPromptHierarchy) || (specialPromptFresh && promptFor == kPromptSpecial))
			{
				promptText.clear();
			}

			hierarchyPromptFresh = false;
			specialPromptFresh = false;
			promptText += (char)key;
		}

		return;
	}

	if (message != WM_KEYDOWN)
	{
		return;
	}

	if (key == VK_ESCAPE)
	{
		bool const wasTypeQuestion = promptFor == kPromptTypeLinks;

		if (promptFor == kPromptHierarchy)
		{
			host::HierarchyMode_Cancel(&hierarchyMode);
		}

		if (promptFor == kPromptSpecial)
		{
			host::SpecialMode_Cancel(&specialMode);
		}

		promptFor = kPromptNone;
		promptText.clear();
		promptList.clear();

		if (wasTypeQuestion)
		{
			StopTheTypeQuestions();
		}
	}
	else if (key == VK_RETURN)
	{
		RunThePrompt();
	}
	else if (key == VK_BACK && !promptText.empty())
	{
		hierarchyPromptFresh = false;
		specialPromptFresh = false;
		promptText.pop_back();
	}
}

// What this frame's panels are, as the boxes and lines the panel pass adds to the game's frame (panel_pass.h): the gray behind each, in
// order before every line. titleBoxes gets the menu bar's titles' boxes, for a run's report. lines and boxes have room for
// kPanelPassMostExtras each.
// Object Placement's special objects in the view: each is drawn as inteng's own built-in shape (FUN_004e4ae0's display block with the class's shape slot; SpecialIconTriangles, editor_icons.h, the
// shapes read at run time from his inteng.exe: object-placement.md section 8b V-25), and the owner's addition, its short name beside it while the shape is seen (the label pass below; the panel pass
// holds 64 boxes and lines in all, the nearest ones in front of the camera first). (Added 2026-11-15 by Melanikus (Vasquez); the comment corrected 2026-11-16: the 2D boxes are long gone.)
static std::vector<host::SpecialObject> specialMarkers;
static unsigned specialMarkersFor = ~0u;
static OpenLevelFile *specialMarkersOpen = nullptr;

// An enemy's or a generator's ID by its name (the game's own, AVP_Generator_Chunk::CalculateID: the first of a name, as the game's lookups take the first), read again when the level changes
static std::map<std::string, std::array<uint8_t, 8>> generatorIdsByName;
static unsigned generatorIdsFor = ~0u;
static OpenLevelFile *generatorIdsOpen = nullptr;

static void GeneratorIdsRefresh(OpenLevelFile *open)
{
	if (!open || !open->Level() || (generatorIdsOpen == open && generatorIdsFor == open->Generation()))
	{
		return;
	}

	std::vector<host::GeneratorRef> generators;

	host::ListGenerators(open->Level(), &generators);
	generatorIdsByName.clear();

	for (host::GeneratorRef const &one : generators)
	{
		if (generatorIdsByName.count(one.name) == 0)
		{
			std::array<uint8_t, 8> id;

			memcpy(id.data(), one.id, 8);
			generatorIdsByName[one.name] = id;
		}
	}

	generatorIdsOpen = open;
	generatorIdsFor = open->Generation();
}

// The special objects as boxes in the world (R_SetEditorWorldLines, depth-tested): the twelve edges of a cube round each place, 300 mm (the current one 500 mm), in the kind's colour. The nearest 120 that the view
// sees. The text beside them is the panel pass's. Cleared when the screen is left. (inteng gives each a render block of its own: unread, unfinished-in-inteng.md.)
static std::vector<host::TrackLine> specialWorldLines;
static bool specialWorldLinesDrawn = false;

struct SpecialWorldLinesFlush
{
	~SpecialWorldLinesFlush()
	{
		if (!specialWorldLines.empty())
		{
			LevelView_WorldLines(specialWorldLines.data(), specialWorldLines.size());
			specialWorldLinesDrawn = true;
		}
		else if (specialWorldLinesDrawn)
		{
			LevelView_WorldLines(nullptr, 0);
			specialWorldLinesDrawn = false;
		}
	}
};

static void AddCubeLines(int32_t const centre[3], int32_t half, uint32_t colour)
{
	for (int c = 0; c < 8; c++)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			if ((c & (1 << axis)) != 0)
			{
				continue;
			}

			host::TrackLine line;

			for (int k = 0; k < 3; k++)
			{
				line.from[k] = centre[k] + ((c & (1 << k)) ? half : -half);
				line.to[k] = line.from[k];
			}

			line.to[axis] = centre[axis] + half;
			line.colour = colour;
			specialWorldLines.push_back(line);
		}
	}
}

// the model's names of the volume's two corners, for the kinds that have one (Volume of Death: volume_min / volume_max; Area Switch: trigger_min / trigger_max)
static bool VolumeNames(int32_t kind, char const **lo, char const **hi)
{
	if (kind == 77)
	{
		*lo = "volume_min";
		*hi = "volume_max";
		return true;
	}

	if (kind == 68)
	{
		*lo = "trigger_min";
		*hi = "trigger_max";
		return true;
	}

	return false;
}

// V (FUN_00535970's V key, FUN_00530900's, and the others): the current object's strategy gives the volume, and the editor takes the screen over
void VolumeBegin()
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t selected[8];
	host::ObjectStrategy now;

	// the bytes as edited so far (the store keeps every edit until Save a copy; FindObjectStrategy alone would give the file's own bytes)
	if (!open || !SelectedObjectId(selected) || host::LookUpStrategyBytes(LevelObjects_Level(), host::LevelStore(open), selected, &now) != host::StrategyFound::Found || now.payload.size() < 12)
	{
		statusWords = "Select an object that has a strategy first";
		return;
	}

	int32_t const kind = *(int32_t const *)(now.payload.data() + 8);
	model::StrategyKind const *const kindModel = model::FindStrategyKind(kind);
	char const *loName = nullptr;
	char const *hiName = nullptr;

	if (!kindModel || !VolumeNames(kind, &loName, &hiName))
	{
		statusWords = "V : Edit Volume is for a strategy that has a volume (Area Switch, Volume of Death)";
		return;
	}

	// a Volume of Death and an Area Switch: V is read by their Edit views only (FUN_00535970 0x535b20; FUN_0052f700 0x52fd91 -> FUN_00530900, S-K68-10); on page 0 it does nothing (strategy-data.md S-59)
	if ((kind == 77 || kind == 68) && !(strategyEditFan && strategyEditKind == kind))
	{
		return;
	}

	model::Field const *loField = nullptr;
	model::Field const *hiField = nullptr;

	for (size_t i = 0; i < kindModel->fieldCount; i++)
	{
		if (strcmp(kindModel->fields[i].name, loName) == 0)
		{
			loField = &kindModel->fields[i];
		}

		if (strcmp(kindModel->fields[i].name, hiName) == 0)
		{
			hiField = &kindModel->fields[i];
		}
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	double lo[3], hi[3];

	for (int axis = 0; axis < 3; axis++)
	{
		int32_t a = 0, b = 0;
		char const *why = nullptr;

		if (!loField || !hiField || !model::ReadFieldAxis(*kindModel, *loField, now.payload.data(), now.payload.size(), axis, &a, &why) ||
			!model::ReadFieldAxis(*kindModel, *hiField, now.payload.data(), now.payload.size(), axis, &b, &why))
		{
			statusWords = "Edit Volume: the volume's numbers can't be read";
			return;
		}

		lo[axis] = (a < b ? a : b) * scale;
		hi[axis] = (a < b ? b : a) * scale;
	}

	memcpy(volume.id, selected, sizeof(volume.id));
	memcpy(volume.lo, lo, sizeof(lo));
	memcpy(volume.hi, hi, sizeof(hi));

	// a box that is not empty (some corner number differs, 0x5277fa): FUN_00529280(cube, 0) puts the camera at the cube's place, its centre (FUN_00582f40 0x5830c4), less ten times the camera's
	// FORWARD axis (+0x50..+0x58, 16.16: 0x5292c1-0x529321; S-11, as Find Object's move, StrategyFlyTo: forward / 10 in the game's millimetres), its turn and look kept; the current object stays
	// (strategy-data.md S-VOL-2). Was: the camera AT the centre (its comment read +0x50.. as motion numbers, 0 at rest)
	if (theCamera && (lo[0] != hi[0] || lo[1] != hi[1] || lo[2] != hi[2]))
	{
		int32_t view[9];

		// the camera's OWN forward (inteng's +0x50..+0x58: its live matrix's third row), from its angles: not LevelView_CameraAxes, which reads the last DRAWN frame, and V is pressed in
		// a kind's Edit view, which drew no level (before the owner's box) and still need not have drawn the turn, so a turn made there would be missed (a skeptic's correction to the brief, 11-17)
		LevelView_MatrixOfAngles(theCamera->turn, theCamera->look, theCamera->roll, view);
		theCamera->x = (int)((lo[0] + hi[0]) / 2) - view[6] / 10;
		theCamera->y = (int)((lo[1] + hi[1]) / 2) - view[7] / 10;
		theCamera->z = (int)((lo[2] + hi[2]) / 2) - view[8] / 10;
		FlyCamera_Hold(theCamera);
	}
	volume.kind = kind;
	volume.face = -1;
	volume.module = -1;
	volume.objectPicked = false;
	volume.iconPicked = false;
	volume.last = 0;
	volume.on = true;
	strategyPick = 0;
	copyCollecting = false;
	statusWords = "Edit Volume: Return or the right button ends";
}

// the cube's twelve edges in red, the selected face's four in green (FUN_00583130's wire object; its items' colours are 0xff0000 and the selected face's 0x00ff00)
static void VolumeAddLines()
{
	if (!volume.on)
	{
		return;
	}

	int32_t c[2][3];

	for (int a = 0; a < 3; a++)
	{
		c[0][a] = (int32_t)volume.lo[a];
		c[1][a] = (int32_t)volume.hi[a];
	}

	for (int corner = 0; corner < 8; corner++)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			if ((corner & (1 << axis)) != 0)
			{
				continue;
			}

			host::TrackLine line;

			for (int k = 0; k < 3; k++)
			{
				line.from[k] = c[(corner >> k) & 1][k];
				line.to[k] = line.from[k];
			}

			line.to[axis] = c[1][axis];
			line.colour = 0xFFFF0000u;
			specialWorldLines.push_back(line);
		}
	}

	if (volume.face >= 0 && volume.face < 6)
	{
		int const axis = volume.face / 2;
		int const side = volume.face % 2;
		int const u = (axis + 1) % 3;
		int const v = (axis + 2) % 3;

		for (int along = 0; along < 2; along++)
		{
			for (int other = 0; other < 2; other++)
			{
				host::TrackLine line;

				line.from[axis] = line.to[axis] = c[side][axis];

				// along 0: the edge runs in u at the v value `other`; along 1: it runs in v at the u value `other`
				int const run = along == 0 ? u : v;
				int const fixed = along == 0 ? v : u;

				line.from[run] = c[0][run];
				line.to[run] = c[1][run];
				line.from[fixed] = line.to[fixed] = c[other][fixed];
				line.colour = 0xFF00FF00u;
				specialWorldLines.push_back(line);
			}
		}
	}
}

// each frame: + / - move the selected face (FUN_005277c0 0x527bf2-0x527cf4)
static void VolumeFrame()
{
	if (!volume.on)
	{
		return;
	}

	ULONGLONG const now = GetTickCount64();
	double dt = volume.last ? (double)(now - volume.last) / 1000.0 : 0.0;
	double const longest = 6554.0 / 65536.0;   // 0x199a in 16.16: the frame time is held to 0.1 s

	volume.last = now;

	if (dt > longest)
	{
		dt = longest;
	}

	// the main keyboard's =/+ and - keys only (slots 0xd87afb / 0xd87afd = 0xd87a40 + VK 0xbb / 0xbd, FUN_0058c010 0x58c182); the numpad's + and - (slots 0xd87aab / 0xd87aad) stay the camera's
	bool const plus = keyDown[VK_OEM_PLUS];
	bool const minus = keyDown[VK_OEM_MINUS];

	if (volume.face < 0 || plus == minus || dt <= 0.0 || zoomStage != 0)   // no face moves while Insert's zoom waits (its own loop, S-VOL-11)
	{
		return;
	}

	double const step = (plus ? 1.0 : -1.0) * 2000.0 * dt * (ShiftDown() ? 10.0 : 1.0);   // 200000 inteng units a second = 2000 mm a second
	int const axis = volume.face / 2;

	if (volume.face % 2 == 0)
	{
		double const now_ = volume.lo[axis] - step;

		volume.lo[axis] = now_ >= volume.hi[axis] ? volume.hi[axis] : now_;
	}
	else
	{
		double const now_ = volume.hi[axis] + step;

		volume.hi[axis] = now_ < volume.lo[axis] ? volume.lo[axis] : now_;
	}
}

// Return or the right button: the cube's numbers go into the strategy (truncated to file units as inteng's __ftol does), the editor is let go (FUN_00583560 destroys the cube)
static void VolumeEnd()
{
	if (!volume.on)
	{
		return;
	}

	volume.on = false;
	volume.face = -1;
	volume.module = -1;
	volume.objectPicked = false;
	volume.iconPicked = false;

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	char const *loName = nullptr;
	char const *hiName = nullptr;

	if (!open || !VolumeNames(volume.kind, &loName, &hiName))
	{
		return;
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	host::LevelStore store(open);
	bool good = true;
	char const *why = nullptr;

	for (int axis = 0; axis < 3 && good; axis++)
	{
		host::FieldEdit lowEdit, highEdit;

		lowEdit.field = loName;
		lowEdit.what = host::EditWhat::Axis;
		lowEdit.axis = axis;
		lowEdit.axisValue = (int32_t)(volume.lo[axis] / scale);
		highEdit.field = hiName;
		highEdit.what = host::EditWhat::Axis;
		highEdit.axis = axis;
		highEdit.axisValue = (int32_t)(volume.hi[axis] / scale);

		host::StrategyFound lookup;

		good = host::ApplyStrategyEdit(LevelObjects_Level(), volume.id, volume.kind, lowEdit, &store, &why, &lookup) == host::EditAnswer::Applied &&
			host::ApplyStrategyEdit(LevelObjects_Level(), volume.id, volume.kind, highEdit, &store, &why, &lookup) == host::EditAnswer::Applied;
	}

	static std::string endWords;

	endWords = good ? std::string("The volume is written into the strategy (kept in the editor's bytes until Save a copy)") : std::string("Edit Volume: ") + (why ? why : "the strategy would not take the box");
	statusWords = endWords.c_str();
}

// S, X and Return of the volume editor, and the keys of the screen it shadows (T, V, E: the loop reads none of them); true when the key is taken
static bool VolumeKey(WPARAM key)
{
	if (!volume.on)
	{
		return false;
	}

	// Insert's zoom waiting (S-VOL-11): its own loop FUN_00471cb0 reads only the mouse, the pointer and Escape (0x471cda-0x471f71), and a tapped key's slot is cleared at its key-up
	// (FUN_0058c5e0 0x58c6fe), so the editor's Return, S and X do nothing meanwhile, and Escape gives the zoom up
	if (zoomStage != 0 && (key == VK_RETURN || key == 'S' || key == 'X'))
	{
		return true;
	}

	if (zoomStage != 0 && key == VK_ESCAPE)
	{
		ZoomCancel();
		statusWords = "Zoom cancelled";
		return true;
	}

	if (key == VK_RETURN)
	{
		VolumeEnd();
		return true;
	}

	if (key == 'S' || key == 'X')
	{
		int32_t mins[3], maxs[3];

		if ((volume.module >= 0 && LevelView_ModuleBox(volume.module, mins, maxs)) || (volume.objectPicked && (volume.iconPicked ? StrategyIconBox(volume.object, mins, maxs) : LevelView_ObjectBox(volume.object, mins, maxs))))
		{
			for (int a = 0; a < 3; a++)
			{
				if (key == 'S')
				{
					volume.lo[a] = mins[a];
					volume.hi[a] = maxs[a];
				}
				else
				{
					volume.lo[a] = volume.lo[a] < mins[a] ? volume.lo[a] : mins[a];
					volume.hi[a] = volume.hi[a] > maxs[a] ? volume.hi[a] : maxs[a];
				}
			}
		}

		return true;
	}

	return key == 'T' || key == 'V' || key == 'E' || key == VK_ESCAPE;   // Escape: the editor's loop reads none (S-VOL-11)
}

// a click in the volume editor: no Shift picks a module (FUN_00559720; nothing hit lets it go), Shift picks the cube's face the line meets first (FUN_00559a60)
static void VolumeClick(host::PickReport const &report, bool shift, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight)
{
	if (!shift)
	{
		// FUN_00559720 gives the record under the pointer, module or object (0x527a5b): it is the picked one (strategy-data.md S-VOL-4). Its records are inteng's display blocks, the editor's
		// icons among them (a virtual object, a generator, a bad guy, a sound, a particle generator), so an icon nearer than what the level's pick met is the record, as on the screen's own
		// click (StrategyIconUnderClick; Drake 11-17: the icons could not be picked here). S / X then take the icon's drawn box (StrategyIconBox; inteng's record box, 0x527b51-0x527ba4).
		// NAMED: a placed hierarchy's icon (shape 4, PLACHIER, 0x53b66c) is not drawn on Strategy Data in ours (SpecialIconTriangles' !strategy gate), so it cannot be picked here
		uint8_t icon[8];
		bool const levelHit = report.found == host::PickFound::Module || report.found == host::PickFound::Object;

		if (StrategyIconUnderClick(x, y, viewWidth, viewHeight, levelHit ? report.distance : -1, icon))
		{
			volume.module = -1;
			volume.objectPicked = true;
			volume.iconPicked = true;
			memcpy(volume.object, icon, sizeof(volume.object));
			return;
		}

		if (report.found == host::PickFound::Module)
		{
			volume.module = report.moduleIndex;
			volume.objectPicked = false;
			volume.iconPicked = false;
		}
		else if (report.found == host::PickFound::Object)
		{
			volume.module = -1;
			volume.objectPicked = true;
			volume.iconPicked = false;
			memcpy(volume.object, report.objectId, sizeof(volume.object));
		}
		else if (report.found == host::PickFound::Nothing || report.found == host::PickFound::NoLine)
		{
			volume.module = -1;
			volume.objectPicked = false;
			volume.iconPicked = false;
		}

		return;
	}

	host::PickRay ray;

	if (!PickGame_RayAtClick(x, y, viewWidth, viewHeight, &ray))
	{
		volume.face = -1;
		return;
	}

	int best = -1;
	double bestT = 1e300;

	for (int face = 0; face < 6; face++)
	{
		int const axis = face / 2;
		double const plane = face % 2 == 0 ? volume.lo[axis] : volume.hi[axis];

		if (ray.direction[axis] == 0)
		{
			continue;
		}

		double const t = (plane - ray.from[axis]) / (double)ray.direction[axis];

		if (t <= 0.0 || t >= bestT)
		{
			continue;
		}

		bool inside = true;

		for (int other = 0; other < 3 && inside; other++)
		{
			if (other == axis)
			{
				continue;
			}

			double const at = ray.from[other] + ray.direction[other] * t;

			inside = at >= volume.lo[other] && at <= volume.hi[other];
		}

		if (inside)
		{
			best = face;
			bestT = t;
		}
	}

	volume.face = best;
}

// STRATEGY DATA'S VIEW DISTANCE (strategy-data.md S-53 / S-54 / S-65): the screen's frame FUN_00522980 (0x5229a0-0x5229e0) and the sub-loops' FUN_00529370 (0x52940b-0x52944b): H held (slot 0xd87a88) adds 500000 to
// the shared distance DAT_005fd678 (1,000,000 at the start) each frame, N held (0xd87a8e) takes 500000 away, never below 0; then FUN_00522a30 every frame: a module whose distance from the camera, less its radius (+0x100),
// is not under the distance is not drawn. A kind's Edit view runs its own loop (FUN_0058b700 only): no H / N and no culling pass while it is up.
// THE EDITOR'S ICONS ARE NEVER CULLED (S-65, read 11-18): FUN_00522a30 0x522a48 looks only at records whose shape (+0, the map block's shape, FUN_004b2590 0x4b268a) is at or after DAT_008f588c, the
// level's first shape: FUN_0042c700 puts inteng's 0x13 built-in shapes first and gives back 0x13 (0x42c7ca), which FUN_0046e700 keeps there (0x46ed8e). The icons of the virtual objects (13), generators
// (3), bad guys (7), sounds (8), particle generators (2) and placed hierarchies (4) are built-in shapes (editor_icons.h), so the distance never hides them: SpecialIconTriangles hides them by distance
// only off Strategy, as ours always did. NAMED: a record flagged 0x800000 (+0x64) is ALWAYS hidden (0x522a79: the flag sends it to +0x5c |= 4 whatever its distance); ours has no such flag.
static bool strategyDistanceApplied = false;

static void StrategyViewDistanceEveryFrame()
{
	if (!OnStrategyScreen())
	{
		if (strategyDistanceApplied)
		{
			strategyDistanceApplied = false;
			LevelView_HideBeyond(false, 0);
		}

		return;
	}

	// a kind's Edit view reads neither H nor N; the volume editor, the collector and the object pick it starts are FUN_00529370's loops, where H / N and the hiding go on (S-VOL-11, Drake 11-17;
	// S-57, Bishop 11-18)
	if (strategyEditFan && !StrategyInASubLoop())
	{
		return;
	}

	if (!AMenuIsOpen() && !ADialogIsOpen() && promptFor == kPromptNone && (keyDown['H'] || keyDown['N']))
	{
		int64_t const next = (int64_t)viewDistance + (keyDown['H'] ? 500000 : 0) - (keyDown['N'] ? 500000 : 0);

		viewDistance = ViewDistanceHeld(next);
	}

	LevelView_HideBeyond(true, viewDistance / 100);   // inteng's DAT_005fd678 is 100 to the millimetre (H-6, merged 399a933); the hiding is in millimetres: 1,000,000 = 10 m
	strategyDistanceApplied = true;
}

// THE HUB'S VIEW DISTANCE (row ML-E-11 of module-linking-map-dietrich.md): the hub's frame FUN_004452b0, after its commands, adds 500,000 to the shared distance DAT_005fd678 for
// each frame H is held (slot 0xd87a88, 0x446ada-0x446ae3) and takes 500,000 away for N (0xd87a8e, 0x446af3-0x446af7, never below 0); then EVERY frame, with no switch, FUN_00444b70
// (LAB_00446c46) hides each record whose distance from the camera, less its radius, is at least the distance (the same rule as Module Linking's FUN_004c36f0; 100 to the millimetre,
// row ML-E-7): 1,000,000 = 10 m. (Until 2026-11-16 ours' hub drew every module at any distance and had no H / N: going hub -> Module Linking looked as if the map vanished.)
static bool hubDistanceApplied = false;

static void HubViewDistanceEveryFrame()
{
	// THE HUB'S START (FUN_00447550: the editor's first screen, and every mode's way back to it): the read-only box (TB-C10) once each time the hub begins with a level
	static size_t hubStartScreen = (size_t)-1;
	static File_Chunk *hubStartLevel = nullptr;
	bool const hubStarts = barScreen == host::kIntengHub && LevelObjects_Level() && (hubStartScreen != host::kIntengHub || hubStartLevel != LevelObjects_Level());

	hubStartScreen = barScreen;
	hubStartLevel = LevelObjects_Level();

	if (hubStarts)
	{
		HubReadOnlyBox();
	}

	if (barScreen != host::kIntengHub || !LevelObjects_Level())
	{
		if (hubDistanceApplied)
		{
			hubDistanceApplied = false;
			LevelView_HideBeyond(false, 0);
		}

		return;
	}

	if (!AMenuIsOpen() && !ADialogIsOpen() && promptFor == kPromptNone && (keyDown['H'] || keyDown['N']))
	{
		int64_t const next = (int64_t)viewDistance + (keyDown['H'] ? 500000 : 0) - (keyDown['N'] ? 500000 : 0);

		viewDistance = ViewDistanceHeld(next);
	}

	LevelView_HideBeyond(true, viewDistance / 100);
	hubDistanceApplied = true;
}

static bool OnAPlacementScreen()
{
	return barScreen < host::kIntengScreenCount && (strcmp(host::kIntengScreens[barScreen].name, "Object Placement") == 0 || strcmp(host::kIntengScreens[barScreen].name, "Placement mode 1 (sounds)") == 0);
}

// The EDITOR OBJECTS' ICONS (editor_icons.h): every special object is drawn as inteng's own built-in shape for its kind, filled, flat-coloured, at its place and turn; the selected one flashes (white, then its colours, every
// three frames, as the light markers do). Object Placement draws all five kinds (its sounds screen only the sounds); Strategy Data draws the generators, bad guys, sounds, particle generators and the virtual objects.
void SpecialIconTriangles(std::vector<LevelViewTriangle> *triangles, uint32_t frame, bool strategy, std::vector<IconForPick> *forPick)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		return;
	}

	if (specialMarkersOpen != open || specialMarkersFor != open->Generation())
	{
		specialMarkers.clear();
		host::SpecialObjects_Read(open, &specialMarkers);
		specialMarkersOpen = open;
		specialMarkersFor = open->Generation();
	}

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	bool const soundsOnly = !strategy && strcmp(host::kIntengScreens[barScreen].name, "Object Placement") != 0;
	// Which icon is RED (S-62): the current one; but while Strategy Data's collector runs, its members are and the current is not (FUN_00526bf0 FUN_00529470 on each member;
	// Copy took the source's red off first, 0x5283a5)
	auto const isRed = [strategy](bool current, uint8_t const *id)
	{
		// in the volume editor the ONLY red record is the one it picked (FUN_005277c0 0x527a8b FUN_00529470 on it, an icon too, S-VOL-4): the view's own object lost its red before the view
		// ran (FUN_00528830 0x52897d / FUN_005286b0 0x5286d7), so the subject's icon is not red (a review of S-VOL-4, 11-17: it stayed red)
		if (strategy && volume.on)
		{
			return volume.objectPicked && memcmp(volume.object, id, 8) == 0;
		}

		if (!(strategy && copyCollecting))
		{
			return current;
		}

		for (CopyTarget const &target : copyTargets)
		{
			if (memcmp(target.id, id, 8) == 0)
			{
				return true;
			}
		}

		return false;
	};
	// the triangle hook takes 65536 at most (avpx editor-world-triangles): the objects NEAREST the camera are given first, each whole
	struct Piece { double away; int group; uint8_t id[8]; std::vector<host::IconWorldTriangle> triangles; };
	std::vector<Piece> pieces;
	double const eye[3] = { theCamera ? (double)theCamera->x : 0.0, theCamera ? (double)theCamera->y : 0.0, theCamera ? (double)theCamera->z : 0.0 };

	for (size_t markerAt = 0; markerAt < specialMarkers.size(); markerAt++)
	{
		host::SpecialObject const &o = specialMarkers[markerAt];
		int shape = 0;

		switch (o.kind)
		{
		case host::SpecialKind::Generator: shape = 3; break;
		case host::SpecialKind::BadGuy: shape = 7; break;
		case host::SpecialKind::PlayerStart: shape = strategy ? 0 : 9; break;
		case host::SpecialKind::Sound: shape = 8; break;
		case host::SpecialKind::ParticleGenerator: shape = 2; break;
		}

		if (shape == 0 || (soundsOnly && o.kind != host::SpecialKind::Sound))
		{
			continue;
		}

		double const place[3] = { o.place[0] * scale, o.place[1] * scale, o.place[2] * scale };
		double axes[9];
		bool haveAxes = false;
		float turned[4];

		if (o.kind == host::SpecialKind::Generator || o.kind == host::SpecialKind::BadGuy)
		{
			// about the vertical axis by the heading (4096 to a turn), by the game's own Euler matrix
			float const identity[4] = { 0, 0, 0, 1 };

			haveAxes = host::TrackPlay_TurnedBy(identity, (double)(o.heading & 4095) * 360.0 / 4096.0, 0.0, 0.0, turned) && host::TrackPlay_QuatAxes(turned, axes);
		}
		else if (o.kind == host::SpecialKind::ParticleGenerator || o.kind == host::SpecialKind::PlayerStart)
		{
			haveAxes = host::TrackPlay_QuatAxes(o.turn, axes);
		}

		bool const current = !strategy && specialMode.haveCurrent && specialMode.kind == o.kind && specialMode.number == o.number;

		// Object Placement's view distance (FUN_004dd2f0): an object whose distance from the camera less its radius is not below the distance is hidden; the current object never is
		if (!strategy && placementHideOn && !current)
		{
			double const dx = place[0] - eye[0], dy = place[1] - eye[1], dz = place[2] - eye[2];

			if (sqrt(dx * dx + dy * dy + dz * dz) - host::EditorIcon_Radius(shape) >= (double)PlacementHideDistance())
			{
				continue;
			}
		}

		Piece piece;

		piece.group = (int)markerAt;   // the triangle group is the marker's place in the list: its label asks LevelView_TriangleGroupPixels with the same number
		memcpy(piece.id, o.id, sizeof(piece.id));

		// an enemy or a generator is named by the game's own ID (Strategy Data selects it by that, and finds its strategy in the AVPGENEX of its name)
		if (o.kind == host::SpecialKind::Generator || o.kind == host::SpecialKind::BadGuy)
		{
			GeneratorIdsRefresh(open);

			if (generatorIdsByName.count(o.name))
			{
				memcpy(piece.id, generatorIdsByName[o.name].data(), sizeof(piece.id));
			}
		}

		uint32_t const flash = isRed(current, piece.id) ? 0xFFFF0000u : 0u;   // the current object steady RED (OP-106: 0x4e4f40 sets block +0x64 bit 0x40000000, drawn 0xff0000 by FUN_004a7cd0 0x4a7eca)

		piece.away = (place[0] - eye[0]) * (place[0] - eye[0]) + (place[1] - eye[1]) * (place[1] - eye[1]) + (place[2] - eye[2]) * (place[2] - eye[2]);
		host::EditorIcon_Add(shape, place, haveAxes ? axes : nullptr, flash, &piece.triangles);
		pieces.push_back(piece);
	}

	// the placed hierarchies of Object Placement: the editor's icon shape 4 at the place, turned by the PLACHIDT's turn, red when current, under the view distance rule (no name label: NAMED)
	if (!strategy && !soundsOnly && !forPick)
	{
		// the list PlacementFrame keeps (read again only when the level changes: not each frame)
		std::vector<host::PlacedHierarchyInfo> const &hierarchies = placementHierarchies;

		for (host::PlacedHierarchyInfo const &one : hierarchies)
		{
			double const place[3] = { one.place[0] * scale, one.place[1] * scale, one.place[2] * scale };
			double axes[9];
			bool const haveAxes = host::TrackPlay_QuatAxes(one.turn, axes);
			bool const current = placedHierarchyCurrent == (int)one.number;

			// PH-1: while it is current its model is shown and its own icon is not (FUN_004e9520 0x4e95ac: the object's record +0x64 |= 0x800000)
			if (current && placedHierarchyShown == (int)one.number && HierarchyView_IsShown())
			{
				continue;
			}

			if (placementHideOn && !current)
			{
				double const dx = place[0] - eye[0], dy = place[1] - eye[1], dz = place[2] - eye[2];

				if (sqrt(dx * dx + dy * dy + dz * dz) - host::EditorIcon_Radius(4) >= (double)PlacementHideDistance())
				{
					continue;
				}
			}

			Piece piece;

			piece.group = -1;
			memcpy(piece.id, one.id, sizeof(piece.id));
			piece.away = (place[0] - eye[0]) * (place[0] - eye[0]) + (place[1] - eye[1]) * (place[1] - eye[1]) + (place[2] - eye[2]) * (place[2] - eye[2]);
			host::EditorIcon_Add(4, place, haveAxes ? axes : nullptr, isRed(current, piece.id) ? 0xFFFF0000u : 0u, &piece.triangles);
			pieces.push_back(piece);
		}
	}

	if (strategy)
	{
		std::vector<host::VirtualObjectInfo> virtuals;

		if (host::VirtualObject_List(open, &virtuals))
		{
			host::Selected const selected = host::Selection_Get();

			for (host::VirtualObjectInfo const &v : virtuals)
			{
				double const place[3] = { v.place[0] * scale, v.place[1] * scale, v.place[2] * scale };

				// a virtual object's record is coloured as every record of the screen (S-61 / S-62): the CURRENT one red (+0x64 0x40000000 -> FUN_004a7cd0 0x4a7eca item 0x80 -> FUN_004980f0
				// 0xff0000), else one holding a strategy green (FUN_00522680 0x522935 0x2000000 -> item 0x100 -> 0x00ff00), else its own colours
				bool const current = selected.kind == host::SelectedKind::Object && memcmp(selected.objectId, v.id, sizeof(v.id)) == 0;
				uint32_t const tint = isRed(current, v.id) ? 0xFFFF0000u : (v.hasStrategy ? 0xFF00FF00u : 0u);

				Piece piece;

				piece.group = -1;
				memcpy(piece.id, v.id, sizeof(piece.id));
				piece.away = (place[0] - eye[0]) * (place[0] - eye[0]) + (place[1] - eye[1]) * (place[1] - eye[1]) + (place[2] - eye[2]) * (place[2] - eye[2]);
				host::EditorIcon_Add(13, place, nullptr, tint, &piece.triangles);
				pieces.push_back(piece);
			}
		}
	}

	if (forPick)
	{
		for (Piece const &piece : pieces)
		{
			IconForPick one;

			memcpy(one.id, piece.id, sizeof(one.id));
			one.triangles = piece.triangles;
			forPick->push_back(std::move(one));
		}

		return;
	}

	std::sort(pieces.begin(), pieces.end(), [](Piece const &a, Piece const &b) { return a.away < b.away; });

	for (Piece const &piece : pieces)
	{
		if (triangles->size() + piece.triangles.size() > 65536)
		{
			continue;
		}

		for (host::IconWorldTriangle const &t : piece.triangles)
		{
			LevelViewTriangle one;

			memcpy(one.a, t.a, sizeof(one.a));
			memcpy(one.b, t.b, sizeof(one.b));
			memcpy(one.c, t.c, sizeof(one.c));
			one.colour = t.colour;
			one.group = piece.group;
			triangles->push_back(one);
		}
	}
}

void AddSpecialMarkers(const ui::PanelLayout &layout, host::PaintText *lines, size_t *lineCount, host::PaintBox *boxes, size_t *boxCount)
{
	specialWorldLines.clear();
	SpecialWorldLinesFlush flushLines;

	// the volume editor: the cube, and the face that + / - move
	VolumeFrame();
	VolumeAddLines();

	// the object that is being flown (F) is marked where it is now
	if (flyOn && !flyReal)
	{
		AddCubeLines(flyWorld, 250, 0xFFFFFF00u);
	}

	if (barScreen >= host::kIntengScreenCount)
	{
		return;
	}

	char const *const screen = host::kIntengScreens[barScreen].name;

	if (strcmp(screen, "Track Editing") == 0)
	{
		return;   // the split screen stays on while a track is edited (inteng's 0x52 is in Track Editing's menu object too); the special markers are Object Placement's
	}

	if (strcmp(screen, "Object Placement") != 0 && strcmp(screen, "Placement mode 1 (sounds)") != 0)
	{
		placementSplit = false;     // leaving the screen puts the window back (FUN_004d5d80)
		return;
	}

	if (placementSplit)
	{
		return;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	host::PickCamera camera;

	if (!open || !open->Level() || !PickGame_Camera(layout.view.width, layout.view.height, &camera))
	{
		return;
	}

	if (specialMarkersOpen != open || specialMarkersFor != open->Generation())
	{
		specialMarkers.clear();
		host::SpecialObjects_Read(open, &specialMarkers);
		specialMarkersOpen = open;
		specialMarkersFor = open->Generation();
	}

	struct Seen
	{
		double depth;
		double x, y;
		size_t at;
	};

	std::vector<Seen> seen;
	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	bool const soundsOnly = strcmp(screen, "Object Placement") != 0;

	for (size_t i = 0; i < specialMarkers.size(); i++)
	{
		host::SpecialObject const &o = specialMarkers[i];

		if (soundsOnly && o.kind != host::SpecialKind::Sound)
		{
			continue;
		}

		host::PickCentre centre = { { (int32_t)(o.place[0] * scale), (int32_t)(o.place[1] * scale), (int32_t)(o.place[2] * scale) } };
		double x = 0.0, y = 0.0;

		if (!host::PickCentre_Project(camera, centre, &x, &y) || x < 0 || y < 0 || x >= layout.view.width || y >= layout.view.height)
		{
			continue;
		}

		double const away[3] = { (double)centre.place[0] - camera.place[0], (double)centre.place[1] - camera.place[1], (double)centre.place[2] - camera.place[2] };
		Seen one = { away[0] * away[0] + away[1] * away[1] + away[2] * away[2], x, y, i };

		seen.push_back(one);
	}

	std::sort(seen.begin(), seen.end(), [](Seen const &a, Seen const &b) { return a.depth < b.depth; });

	for (Seen const &one : seen)
	{
		if (*boxCount + 1 > kPanelPassMostExtras || *lineCount + 1 > kPanelPassMostExtras)
		{
			break;
		}

		host::SpecialObject const &o = specialMarkers[one.at];
		bool const current = specialMode.haveCurrent && specialMode.kind == o.kind && specialMode.number == o.number;
		int32_t const half = current ? 6 : 4;
		uint32_t colour = 0xFFFFFFFFu;
		char const *tag = "start";

		switch (o.kind)
		{
		case host::SpecialKind::Generator: colour = 0xFF30D030u; tag = "gen"; break;
		case host::SpecialKind::BadGuy: colour = 0xFFE03030u; tag = "bad"; break;
		case host::SpecialKind::PlayerStart: colour = 0xFFFFFFFFu; tag = "start"; break;
		case host::SpecialKind::Sound: colour = 0xFF30D0E0u; tag = "sound"; break;
		case host::SpecialKind::ParticleGenerator: colour = 0xFFF0A030u; tag = "particle"; break;
		}

		(void)colour;   // the object is drawn as inteng's own icon (SpecialIconTriangles); the cube is gone

		// the name is drawn only when the object's icon is SEEN (an occlusion query on its triangles, a frame late): a wall hides the name as it hides the icon (Melanikus 2026-11-16: "make it have a Z")
		if (LevelView_TriangleGroupPixels((int)one.at) <= 0)
		{
			continue;
		}

		host::PaintText label;

		if (!placementText)
		{
			continue;
		}

		label.text = std::string(tag) + (o.name.empty() || o.kind == host::SpecialKind::PlayerStart ? std::string() : " " + o.name);
		label.x = (int32_t)one.x + half + 3;
		label.y = (int32_t)one.y - 7;
		lines[(*lineCount)++] = label;
	}
}

// (THE PANELS OF THE FRAME: in editor_panels.cpp)

// What the frame adds to the game's own: the panels' boxes and lines this frame (PlanThePanels), at the window's size
struct FramePanels
{
	int32_t width;
	int32_t height;
	PanelExtras extras;
};

static void AddThePanels(void *context)
{
	FramePanels const *const frame = (FramePanels const *)context;

	if (frame)
	{
		if (onTextureMap && textureWindowOn)
		{
			TextureWindowDraw(frame->width, frame->height);
		}

		if (onTextureMap && textureChooserOn)
		{
			TextureChooserDraw(frame->width, frame->height);
		}

		if (onTextureMap && textureFlagList && !textureFlagLines.empty())
		{
			TextureWindowPaint_Lines(frame->width, frame->height, textureFlagLines, 8, 66);   // below the two menu bars
		}

		// AutoLink's display, while a run is on: the owner's grid of pictures, then over it inteng's counter and words (AD-5 .. AD-7); the menu bar and the dialogs over all
		AutoLinkShow_Draw(frame->width, frame->height);
		PanelPass_AddToFrame(frame->width, frame->height, frame->extras);
		TextureColourDraw(frame->width, frame->height);   // Select Colour's grid, over the bars too (Shift+K, FUN_00560620)
	}
}
// (THE PROOF RUNS: in editor_proofs.cpp)

int main(int, char **)
{
	host::TrackEdit_UsePose(host::TrackPlay_PoseAtStartAsFile);   // Stop Track Edit gives the object the pose its track has at the timer start (inteng FUN_004eff90 0x4effba)
	host::TrackEdit_UseRebase(host::TrackPlay_Rebase);   // Paste Track Relative turns and moves the pasted track with the game's maths (track_play_game.h)
	host::TrackEdit_UseTurn(host::TrackPlay_TurnedBy);
	host::TrackEdit_UseObjectPlace(host::TrackPlay_ObjectPlace);   // the object's place at an end of a section is the game's own (track_play_game.h)
	host::SetObjectNameResolver(ObjectByName);   // an object field takes a name as well as its two numbers (record_edit.h)

	NeverStretchedByWindows();

	// The command line as the wide text Windows keeps for it (open_with.h says why main's narrow argv isn't used). Windows gives the list
	// back in memory of its own, which is freed as soon as it's read.
	int argCount = 0;
	wchar_t **const args = CommandLineToArgvW(GetCommandLineW(), &argCount);
	if (!args)
	{
		// Whether -picture was asked for can't be told, so nobody may be at the screen: the reason goes to the error output only
		fprintf(stderr, "AVP_OpenInteng: the command line couldn't be read\n");
		return 1;
	}

	// A run with -picture anywhere on its command line has nobody at the screen, so that's known first, before anything on the line
	// can be refused: a refusal then never shows a box
	for (int i = 1; i < argCount; i++)
	{
		if (wcscmp(args[i], L"-picture") == 0)
		{
			NobodyWatching_Set();
			break;
		}
	}

	// -picture <frames>: a whole number from 1 to 100000, nothing else. -viewmode <word>: one of the View menu's modes, for a proof run
	// (ViewToggles_FromCommandWord, view_toggles.h; added 2026-11-14 by Melanikus (Frost)). And at most one level file.
	long pictureAfter = 0;
	size_t startScreen = host::kIntengHub;
	bool screenGiven = false;
	bool mixedProof = false;   // -mixed: a proof run does Module Linking's Mixed on the fifth frame, with the room the camera is in tinted purple (as the current module is)
	bool showAllProof = false;   // -showall: a proof run does Module Linking's Show All on the fifth frame and says what it painted
	bool strategyAllKinds = false;   // -strategyallkinds: one run that edits a field of every kind Assign Strategy can give
	bool modelProof = false;   // -modelproof: a proof run on a file with no modules: its objects drawn, one picked, Hierarchy opened on its hierarchy, the sequence playing
	bool sClickProof = false;   // -sclickproof: a proof run does Strategy Data's S + click (select by texture) and says whether it works
	bool exitProof = false;   // -exitproof: a proof run does the Strategy leftovers (View Flag, Stop Flag View, Help) and the Exit sweep (Strategy asks, Object Placement saves) and says whether it works
	bool movementProof = false;   // -movementproof: a proof run does the Movement help's keys (roll, zoom, Home) and says whether they work
	bool autoLinkShowFeed = false;   // -autolinkshowfeed: feeds AutoLink's display as the driver will (FeedTheAutoLinkShow), so the display is proven before the driver is in
	bool trackProof = false;   // -trackproof: a proof run does Track Editing's main job (Edit Track, Add Section, the three views, Play, Save a copy, the loader) and says whether it works
	bool strategyProof = false;   // -strategyproof: a proof run does Strategy mode's main job (Assign, Edit, Copy, Save a copy, the loader) and says whether it works
	bool hideProof = false;   // -hideobjects: a proof run hides every object the level places, on the fifth frame
	bool hierarchyProof = false;   // -hierarchyproof: a proof run shows a stock model in front of the camera at one point of one sequence
	std::string hierarchyProofModel, hierarchyProofHierarchy;
	int hierarchyProofSequence = 0, hierarchyProofSub = 0, hierarchyProofPosition = 0;
	host::OpenAt openAt;   // -at x y z turn look [screen] / -shot <png> (open_at.h)
	std::string openAtWhy;
	std::string startGotoObject;   // -gotoobject NAME: the camera is put 2.5 metres in front of the first textured polygon of that object on the first frame (a proof run's way to look at an object)
	bool texMapProof = false;   // -texmapproof: a proof run checks the polygon map against the game's shapes and selects some polygons for the picture
	std::string screenWhy;
	host::IntengScreenSize startSize;   // inteng's 640 x 480, then -r's (startup-screen-frost.md SS-1), then IntengOptions.ini's
	bool startSizeGiven = false;        // -r was given
	bool startFramed = false;           // -w: always a framed window
	std::string optionsFolder;          // -options <folder> (only with -picture): the folder whose IntengOptions.ini a hidden test run reads (SS-13)
	host::ViewToggle startToggles[kCommandWordMostToggles] = { host::ViewToggle::Polygon, host::ViewToggle::Polygon };
	size_t startToggleCount = 0;
	std::wstring levelFile;
	bool noFileProof = false;   // -nofile (only with -picture): a proof run starts as inteng does when it is started with no file (the ini's Level= is not looked at)
	std::string testDocuments;   // -documents <folder> (only with -picture): a hidden test run's Documents folder, where its copies go (Documents\AVP_OpenInteng\edited); without it, the program's own folder
	const char *badCommandLine = nullptr;
	for (int i = 1; i < argCount && !badCommandLine; i++)
	{
		if (wcscmp(args[i], L"-documents") == 0 && i + 1 < argCount)
		{
			char narrow[MAX_PATH] = "";
			BOOL lost = FALSE;
			int const length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, args[i + 1], -1, narrow, sizeof(narrow), NULL, &lost);

			if (length <= 1 || lost)
			{
				badCommandLine = "-documents takes a folder whose name this program can write";
			}

			testDocuments = narrow;
			i++;
			continue;
		}

		if (wcscmp(args[i], L"-picture") == 0 && i + 1 < argCount)
		{
			wchar_t *end = nullptr;
			long const frames = wcstol(args[i + 1], &end, 10);
			if (!end || *end != L'\0' || frames < 1 || frames > 100000)
			{
				badCommandLine = "-picture takes a number of frames from 1 to 100000";
			}
			pictureAfter = frames;
			editorHiddenRun = true;   // a -picture run: nobody at the screen, and no sound (the sound system's volume is 0)
			i++;
		}
		else if (wcscmp(args[i], L"-viewdistance") == 0 && i + 1 < argCount)
		{
			// a proof run: the view distance starts at this many of inteng's units (100 to the millimetre, row ML-E-7; 0 to kViewDistanceMost) instead of 1,000,000
			wchar_t *end = nullptr;
			long long const units = wcstoll(args[i + 1], &end, 10);

			if (!end || *end != L'\0' || units < 0 || units > kViewDistanceMost)
			{
				badCommandLine = "-viewdistance takes inteng's units (100 to the millimetre) from 0 to 2147000000";
			}

			viewDistance = ViewDistanceHeld(units);
			viewDistanceFromCommandLine = true;
			i++;
		}
		else if (wcscmp(args[i], L"-nofile") == 0)
		{
			noFileProof = true;
		}
		else if (wcscmp(args[i], L"-mixed") == 0)
		{
			mixedProof = true;
		}
		else if (wcscmp(args[i], L"-showall") == 0)
		{
			showAllProof = true;
		}
		else if (wcscmp(args[i], L"-strategyallkinds") == 0)
		{
			strategyAllKinds = true;
		}
		else if (wcscmp(args[i], L"-strategyproof") == 0)
		{
			strategyProof = true;
		}
		else if (wcscmp(args[i], L"-trackproof") == 0)
		{
			trackProof = true;
		}
		else if (wcscmp(args[i], L"-autolinkshowfeed") == 0)
		{
			autoLinkShowFeed = true;
		}
		else if (wcscmp(args[i], L"-tracksoundproof") == 0)
		{
			trackProof = true;
			trackSoundOnly = true;
		}
		else if (wcscmp(args[i], L"-trackposeproof") == 0)
		{
			trackProof = true;
			trackPoseOnly = true;
		}
		else if (wcscmp(args[i], L"-exitproof") == 0)
		{
			exitProof = true;
		}
		else if (wcscmp(args[i], L"-modelproof") == 0)
		{
			modelProof = true;
		}
		else if (wcscmp(args[i], L"-sclickproof") == 0)
		{
			sClickProof = true;
		}
		else if (wcscmp(args[i], L"-movementproof") == 0)
		{
			movementProof = true;
		}
		else if (wcscmp(args[i], L"-hideobjects") == 0)
		{
			hideProof = true;
		}
		else if (wcscmp(args[i], L"-texmapproof") == 0)
		{
			texMapProof = true;
		}
		else if (wcscmp(args[i], L"-hierarchyproof") == 0 && i + 5 < argCount)
		{
			// -hierarchyproof <model file name> <hierarchy name> <sequence> <sub sequence> <position 0..65535>: on the fifth frame the model is shown in front of the camera and put at that point of the sequence
			char name[64] = "";

			if (WideCharToMultiByte(CP_ACP, 0, args[i + 1], -1, name, sizeof(name), nullptr, nullptr) == 0)
			{
				badCommandLine = "-hierarchyproof takes a model name, a sequence number, a sub sequence number and a position";
			}

			char hierarchy[64] = "";

			WideCharToMultiByte(CP_ACP, 0, args[i + 2], -1, hierarchy, sizeof(hierarchy), nullptr, nullptr);
			hierarchyProofModel = name;
			hierarchyProofHierarchy = hierarchy;
			hierarchyProofSequence = _wtoi(args[i + 3]);
			hierarchyProofSub = _wtoi(args[i + 4]);
			hierarchyProofPosition = _wtoi(args[i + 5]);
			hierarchyProof = true;
			i += 5;
		}
		else if (wcscmp(args[i], L"-viewmode") == 0 && i + 1 < argCount)
		{
			if (!ViewToggles_FromCommandWord(args[i + 1], startToggles, kCommandWordMostToggles, &startToggleCount))
			{
				badCommandLine = "-viewmode takes one of normal, polygon, polyline, filledpolyline, wire-textured, unlit, headlight, "
					"polyline+unlit, wire-textured+unlit and normal+headlight";
			}
			i++;
		}
		else if (wcscmp(args[i], L"-gotoobject") == 0 && i + 1 < argCount)
		{
			for (wchar_t const *letter = args[i + 1]; *letter; letter++)
			{
				startGotoObject += *letter < 128 ? (char)*letter : '?';
			}

			i++;
		}
		else if (wcscmp(args[i], L"-gotodistance") == 0 && i + 1 < argCount)
		{
			textureGotoDistance = _wtof(args[i + 1]);

			if (!(textureGotoDistance >= 100.0 && textureGotoDistance <= 2000000.0))
			{
				textureGotoDistance = 2500.0;
			}

			i++;
		}
		else if (wcscmp(args[i], L"-screen") == 0 && i + 1 < argCount)
		{
			if (screenGiven)
			{
				badCommandLine = "-screen is given once";
			}
			else if (!host::IntengMenus_ScreenFromCommandWord(args[i + 1], &startScreen, &screenWhy))
			{
				badCommandLine = screenWhy.c_str();
			}

			screenGiven = true;
			i++;
		}
		else if (wcscmp(args[i], L"-w") == 0)
		{
			// inteng's -w (0x58cae5): a framed window, even when the size fills the monitor (Bishop's ruling 11-16, startup-screen-frost.md SS-1)
			startFramed = true;
		}
		else if (wcsncmp(args[i], L"-r", 2) == 0 && (args[i][2] != L'\0' || i + 1 < argCount))
		{
			// inteng's -r<w>x<h> (0x58cc0d: sscanf "%dx%d" of what follows the letter), as one word or two ("-r 1280x720"): the size between inteng's default and IntengOptions.ini, which wins
			wchar_t const *const words = args[i][2] != L'\0' ? args[i] + 2 : args[i + 1];
			char narrow[32] = "";
			size_t n = 0;

			while (words[n] != L'\0' && n + 1 < sizeof(narrow) && words[n] < 0x80)
			{
				narrow[n] = (char)words[n];
				n++;
			}

			if (words[n] != L'\0' || !host::IntengOptions_Resolution(narrow, n, &startSize))
			{
				badCommandLine = "-r takes a size as inteng did: -r<width>x<height>, for example -r1280x720";
			}

			startSizeGiven = true;

			if (args[i][2] == L'\0')
			{
				i++;
			}
		}
		else if (wcscmp(args[i], L"-options") == 0 && i + 1 < argCount)
		{
			char narrow[MAX_PATH] = "";
			BOOL lost = FALSE;
			int const length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, args[i + 1], -1, narrow, sizeof(narrow), NULL, &lost);

			if (length <= 1 || lost)
			{
				badCommandLine = "-options takes a folder whose name this program can read";
			}

			optionsFolder = narrow;
			i++;
		}
		else if (host::OpenAt_Argument(args, argCount, &i, &openAt, &openAtWhy))
		{
			badCommandLine = openAtWhy.empty() ? badCommandLine : openAtWhy.c_str();   // -at / -shot (open_at.h)
		}
		else if (args[i][0] != L'\0' && args[i][0] != L'-' && levelFile.empty())
		{
			levelFile = args[i];
		}
		else
		{
			badCommandLine = "the command line takes one level file, -picture <frames>, -viewmode <mode>, -screen <name>, -r<width>x<height>, -w, <level> -at <x> <y> <z> <turn> <look> [<screen>] and -shot <png>";
		}
	}
	if (!badCommandLine && !openAt.place.screen.empty() && !host::OpenAt_Screen(openAt, screenGiven, &startScreen, &openAtWhy))
	{
		badCommandLine = openAtWhy.c_str();   // the screen -at or the shot names, by -screen's own reader (open_at.h)
	}
	LocalFree(args);
	if (!badCommandLine && !testDocuments.empty() && !editorHiddenRun)
	{
		badCommandLine = "-documents is only for a hidden test run (-picture): the copies of a run with someone at the screen go to their own Documents\\AVP_OpenInteng";
	}
	if (!badCommandLine && !optionsFolder.empty() && !editorHiddenRun)
	{
		badCommandLine = "-options is only for a hidden test run (-picture): with someone at the screen, IntengOptions.ini is read next to this program or in the IntengTools folder";
	}
	if (badCommandLine)
	{
		return Stop(badCommandLine);
	}

	// STARTED WITH NO FILE (inteng: FUN_0058c9e0 -> FUN_0042b8c0 finds no file, FUN_00447ed0: FUN_00474030 message, FUN_0057c800 run it, FUN_0057d2b0, and WinMain returns 0): the words
	// "Please load a rif file by double clicking on it", Ok, and the program ends. Nothing else is read or opened. The ini's Level= is not looked at, as inteng has no such thing; only a
	// -picture run (nobody at the screen, the hidden proofs) may still take its level from there, unless it says -nofile. (Written 2026-11-16 by Melanikus (Dietrich), Bishop's start-up order;
	// startup-trace.md. The box is Windows' for now: inteng's own toolkit window needs the level's view up, which has no level to open.)
	bool const noFile = levelFile.empty() && !openAt.shot && (!pictureAfter || noFileProof);   // the window is made as for any start, and the message is shown in it (NoFileScreen); no level is opened

	// The bar the editor starts with: the hub, or the screen -screen named
	barScreen = startScreen;
	EditorBar_SetPage(IsTheTrackScreen(barScreen) ? host::kTrackPageEntry : 0);   // Track Editing starts on page 2, as when it is entered from another screen

	// One editor at a time in this folder (one_editor.h), known before anything is read or written: the settings, a level copied in,
	// the game's log
	OneEditorAnswer const token = OneEditor_Claim();
	if (token != OneEditorAnswer::Taken)
	{
		return Stop(OneEditor_Words(token));
	}

	// THE EDITOR'S OWN FOLDER FOR THE GAME'S FILES (SAVE-FOLDER-1). The game's code keeps its log and its other files in its save folder,
	// which is the player's own Documents\My Games\Aliens versus Predator, and the editor must never write there (CODING-STANDARD rule 2).
	// So before any of the game's code can ask for that folder, or write a line of its log, the editor gives it its own:
	// <the program's folder>\settings\, made if it isn't there. If the game won't take it, the editor stops: it never falls back to the
	// player's folder.
	// The table of actions and the camera's keys, checked whole (editor_actions.h). A table that isn't sound is a mistake in the editor's
	// own code, so the editor doesn't start. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
	if (const char *const tableWrong = EditorActions_CheckTable(kEditorActions, kEditorActionCount, kEditorMoves, (size_t)EditorMove::Count))
	{
		return Stop(tableWrong);
	}

	char ownFolder[MAX_PATH];
	char settingsFolder[MAX_PATH];
	if (!ProgramFolder(ownFolder, sizeof(ownFolder)) ||
		_snprintf_s(settingsFolder, sizeof(settingsFolder), _TRUNCATE, "%ssettings\\", ownFolder) < 0)
	{
		return Stop("the program's own folder has too long a name");
	}
	if (!CreateDirectoryA(settingsFolder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		return Stop("the editor couldn't make its own settings folder beside the program");
	}
	if (!SaveFolder_UseThisFolder(settingsFolder))
	{
		return Stop("the editor's own settings folder beside the program can't be used (it has to be a plain folder, not a link), so the "
			"editor stopped rather than write into the game's own folder");
	}

	char why[300];
	char folder[MAX_PATH];
	char settingsFile[MAX_PATH];
	char dataFolder[MAX_PATH];
	char levelName[MAX_PATH];
	char picture[MAX_PATH];
	char screenPicture[MAX_PATH];
	char panelsPicture[MAX_PATH];
	if (!ProgramFolder(folder, sizeof(folder)) ||
		_snprintf_s(settingsFile, sizeof(settingsFile), _TRUNCATE, "%sAVP_OpenInteng.ini", folder) < 0 ||
		_snprintf_s(picture, sizeof(picture), _TRUNCATE, "%sfirst-picture.png", folder) < 0 ||
		_snprintf_s(screenPicture, sizeof(screenPicture), _TRUNCATE, "%sfirst-screen.png", folder) < 0 ||
		_snprintf_s(panelsPicture, sizeof(panelsPicture), _TRUNCATE, "%sfirst-panels.png", folder) < 0)
	{
		return Stop("the program's own folder has too long a name");
	}
	if (!ReadSetting(settingsFile, "DataFolder", dataFolder, sizeof(dataFolder)) ||
		(levelFile.empty() && !noFile && !openAt.shot && !ReadSetting(settingsFile, "Level", levelName, sizeof(levelName))))
	{
		return Stop("AVP_OpenInteng.ini beside the program needs [Editor] with DataFolder=, and Level= unless a level file is given (each "
			"shorter than 259 letters)");
	}

	// The key layout (key_layout.h). Anyone can write the setting, so anything but its two words means Classic, and the log says why, in
	// the editor's own words. The settings file is kept for a layout picked later. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
	strcpy_s(layoutSettingsFile, sizeof(layoutSettingsFile), settingsFile);
	{
		char const *optionWhy = nullptr;

		editorOptions = host::EditorOptions_Load(settingsFile, &optionWhy);

		if (optionWhy)
		{
			LogString(std::string("settings: ") + optionWhy + " is not 0 or 1 in the settings file, so its start value is used");
		}
	}
	// The keys are inteng's (Classic: A and Z forward and back, 1 / 3 or Left / Right strafe, - / + or Up / Down strafe up and down, 4 6 8 2 turn and pitch, 7 9 roll), nothing else: the owner's word 11-16 ~10:45, "Let's use that then (A/Z) and
	// remove the wasd layout". A KeyLayout= line in the ini is ignored.
	keyLayout = KeyLayout::Classic;

	// The camera speed is SAVED, as inteng does (FUN_004732a0 writes DAT_005f071c at the end, WriteFile at 0x473336; FUN_00473430 reads it back at the start, 0x4734f3): kept in the ini as CameraSpeed=, from the floor (3) up
	{
		UINT const saved = GetPrivateProfileIntA("Editor", "CameraSpeed", (INT)kSpeedStart, layoutSettingsFile);

		speedUnits = CameraSpeedHeld(saved);
	}

	// THE VIEW DISTANCE is saved too (view-state-table.md H-7: FUN_00473430 reads it FIRST from inteng.ini, ReadFile 0x4734b5 -> DAT_005fd678 0x4734d4; FUN_004732a0 writes it at the
	// end): ViewDistance= in the same file, in inteng's units (100 to the millimetre), 0 to kViewDistanceMost. A -viewdistance on the command line (a proof run) wins and is not kept.
	if (!viewDistanceFromCommandLine)
	{
		int const saved = (int)GetPrivateProfileIntA("Editor", "ViewDistance", (INT)viewDistance, layoutSettingsFile);

		viewDistance = ViewDistanceHeld(saved);
	}


	// The shape folder (shape_import.h): where Quick-create looks for a shape the level doesn't hold. Anyone can write it, so it passes the data
	// folder's check; one that doesn't is left empty and the log says why. (Added 2026-11-15 by Melanikus (Frost).)
	{
		char shapeSetting[MAX_PATH] = "";

		if (ReadSetting(settingsFile, "ShapeFolder", shapeSetting, sizeof(shapeSetting)) && shapeSetting[0] != '\0')
		{
			char checkedShapeFolder[MAX_PATH];

			if (char const *const notUsable = SaveFolder_WhyNotUsable(shapeSetting, checkedShapeFolder, sizeof(checkedShapeFolder)))
			{
				LogString(std::string("ShapeFolder in AVP_OpenInteng.ini can't be used: ") + notUsable + ", so shapes are not loaded from a folder");
			}
			else
			{
				shapeFolder = shapeSetting;
			}
		}
		else
		{
			// No setting: the folder "shape_rifs" beside the program (Bishop's ruling 15 Nov). It need not be there: a missing or empty one is
			// said in the status line when a shape is asked for, with its name. Never a folder outside the program's own.
			shapeFolder = std::string(folder) + "shape_rifs";
		}
	}

	// Textured or Untextured, kept in the same file (inteng keeps its own in inteng.ini, run 25); anything but "0" is textured, inteng's start
	{
		char texturedSetting[8] = "";

		if (ReadSetting(settingsFile, "Textured", texturedSetting, sizeof(texturedSetting)) && texturedSetting[0] == '0')
		{
			ViewToggles_SetTextured(false);
			texturesSaved = false;
		}
	}

	// The data folder is held to the same rules as the editor's own settings folder (save_folder.h, SaveFolder_WhyNotUsable): a drive's
	// letter, a colon and a slash, then a plain folder that's there, with no ".." part, no other colon, no network share or device form,
	// and no link. The editor moves into it below and copies "Open with" levels into it, so a relative one would be read from inside
	// itself (Vasquez's find, 2026-11-14), and a share, a device or a link would send its reads and that copy somewhere else (Apone's
	// cold read, F1). Its checked form is only looked at, never used: the folder is used as it was written.
	char checkedDataFolder[MAX_PATH];
	if (char const *const notUsable = SaveFolder_WhyNotUsable(dataFolder, checkedDataFolder, sizeof(checkedDataFolder)))
	{
		return Stop((std::string("DataFolder in AVP_OpenInteng.ini can't be used: ") + notUsable +
			". It has to be a plain folder on a drive, such as C:\\AVP_OpenInteng\\data").c_str());
	}

	// The data folder has to be the game's: one that holds avp_rifs
	char rifs[MAX_PATH];
	if (_snprintf_s(rifs, sizeof(rifs), _TRUNCATE, "%s\\avp_rifs", dataFolder) < 0)
	{
		return Stop("DataFolder's name is too long");
	}
	DWORD const rifsKind = GetFileAttributesA(rifs);
	if (rifsKind == INVALID_FILE_ATTRIBUTES || !(rifsKind & FILE_ATTRIBUTE_DIRECTORY) || !SetCurrentDirectoryA(dataFolder))
	{
		return Stop("DataFolder isn't the game's folder: it has no avp_rifs folder in it");
	}

	// A run started on the Hierarchy screen is IN the mode as one that moved there is (its models listed from the data folder's avp_huds, the view ready): the hidden scenario runs start so
	if (IsTheHierarchyScreen(startScreen))
	{
		EnterHierarchy();
	}

	// Where a save may write (save_copy.h): the folder for copies, <the user's Documents>\AVP_OpenInteng\ (Melanikus 2026-11-16 ~11:2x: the editor now lives in the game's
	// Tools folder), never the game's folder nor inside it. The Documents folder is Windows' own (the shell's folder API, CSIDL_PERSONAL: the Documents known folder), never a
	// typed path. A hidden test run (-picture) never writes into the user's Documents (a test copy would replace a copy of theirs with the same name): its Documents folder is
	// the one -documents gives, else the program's own folder. Nothing is made here: a save makes the folders. The process is in the game's folder from here on, so every path
	// a save uses is a whole one.
	{
		std::string documents;

		if (editorHiddenRun)
		{
			documents = testDocuments.empty() ? std::string(ownFolder) : testDocuments;
		}
		else
		{
			char shellDocuments[MAX_PATH] = "";

			// a letter Windows can't give in this program's code page comes back as '?', which no path may hold: no Documents folder then
			if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, shellDocuments)) && strchr(shellDocuments, '?') == nullptr)
			{
				documents = shellDocuments;
			}
		}

		std::string const base = host::SaveCopyBase(documents.c_str());

		if (base.empty() || strcpy_s(theCopiesFolder, sizeof(theCopiesFolder), base.c_str()) != 0)
		{
			theCopiesFolder[0] = '\0';
			LogString("the folder for copies: Windows gave no usable Documents folder, so Save a copy, Write Asc File and the screenshots save nothing and say why");
		}
		else
		{
			LogString(std::string("the folder for copies: ") + theCopiesFolder);
		}
	}

	if (strcpy_s(theGameFolder, sizeof(theGameFolder), dataFolder) != 0)
	{
		return Stop("the game's folder has too long a name");
	}

	// -at's level word or -shot's map: a NAME of the data folder's levels, opened by its name (a shot's only with its sha256), so no file is copied in (open_at.h)
	if ((openAt.at || openAt.shot) && !host::OpenAt_Level(openAt, levelFile, dataFolder, levelName, sizeof(levelName), host::IntengExe_Sha256, &openAtWhy))
	{
		return Stop(openAtWhy.c_str());
	}
	if (openAt.at || openAt.shot)
	{
		levelFile.clear();
	}

	// A level file given on the command line is copied into the data folder's avp_rifs if it isn't there yet, and opened by its name
	if (!levelFile.empty() && !OpenWith_TakeLevel(levelFile.c_str(), dataFolder, levelName, sizeof(levelName), why, sizeof(why)))
	{
		return Stop((std::string("can't open that file: ") + why).c_str());
	}

	// OP-137 / OP-137a (inteng FUN_005149e0 -> FUN_00512fc0(0) while the rif is read, so nothing is drawn with a file's old outside shapes): the outside shapes of the editor's OWN copy
	// (avp_rifs\OpenedByEditor\, OpenWith_TakeLevel's) are brought up to date from their files in the shape folder ([Editor] ShapeFolder=, else shape_rifs beside the program: Bishop's
	// ruling OP-137) BEFORE the game loads it for the view, so the view and the held level read the same bytes. A level opened by its name from the data folder is never written.
	if (!levelFile.empty())
	{
		std::string const ownCopy = std::string(dataFolder) + "\\avp_rifs\\" + levelName + ".rif";
		std::string refreshWords;

		if (OutsideShapes_RefreshFile(ownCopy.c_str(), &refreshWords) == OutsideRefresh::Changed)
		{
			outsideShapesWords = refreshWords;
		}
	}

	levelSourcePath = levelFile;   // where the level really is (Load from file reads the module file beside it, as inteng reads it beside the level it opened)

	// The windows (view_surface.h). The editor's own window holds the panels, and the level's view is a child window over the view's
	// box, which is the window the game's renderer draws into: hWndMain, as a window of its own (WindowModeSubWindow), never the whole
	// screen. The editor's window clips its children, so the panels never draw over the view. Both windows share one window procedure:
	// the keyboard stays with the editor's window, and the view only reads the mouse. (Modified 2026-11-14 by Melanikus (Dietrich), F3:
	// the view used to be the editor's window itself.)
	WNDCLASSEXA windowClass;
	memset(&windowClass, 0, sizeof(windowClass));
	windowClass.cbSize = sizeof(windowClass);
	windowClass.lpfnWndProc = EditorWindowProc;
	windowClass.hInstance = GetModuleHandleA(NULL);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = "AVP_OpenInteng";
	WNDCLASSEXA viewClass = windowClass;
	viewClass.lpszClassName = "AVP_OpenInteng_View";
	if (!RegisterClassExA(&windowClass) || !RegisterClassExA(&viewClass))
	{
		return Stop("Windows wouldn't make the editor's window");
	}
	// With nobody at the screen the editor's window is made so that it can't become the active one (WS_EX_NOACTIVATE, as the game makes
	// its own for a test run, win_proj.cpp:593), and it's never shown: the frames are drawn into the view's child window all the same, and
	// saved from there. That's how the F4 stock proof treats its window, and it never took the keyboard in 63 levels; a -picture run that
	// showed its window with ShowWindow(SW_SHOWNOACTIVATE), without WS_EX_NOACTIVATE, did (2026-11-14). Which of the two let it happen
	// wasn't pinned down, so both are done the proof's way. (Joined 2026-11-14 by Melanikus (Hicks) with Dietrich's child window.)
	StartPlace const startPlace = ChooseTheStartPlace(pictureAfter != 0, startSize, startSizeGiven, startFramed, optionsFolder);   // -r, IntengOptions.ini, the monitor (SS-1..SS-13)

	LogString(startPlace.words);
	editorWindow = CreateWindowExA(pictureAfter ? WS_EX_NOACTIVATE : 0, "AVP_OpenInteng", "AVP_OpenInteng", startPlace.style,
		startPlace.x, startPlace.y, startPlace.width, startPlace.height, NULL, NULL, windowClass.hInstance, NULL);
	hWndMain = editorWindow ? CreateWindowExA(0, "AVP_OpenInteng_View", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1, 1, editorWindow, NULL,
		windowClass.hInstance, NULL) : NULL;
	if (!hWndMain)
	{
		return Stop("Windows wouldn't make the editor's window");
	}
	RECT client = { 0, 0, 0, 0 };
	GetClientRect(editorWindow, &client);
	PlaceTheView(client.right - client.left, client.bottom - client.top);
	if (noFile)
	{
		// the game's view is the whole window: the message is shown in the middle of it
		noFileScreenOn = true;
		MoveWindow(hWndMain, 0, 0, client.right - client.left, client.bottom - client.top, TRUE);
	}
	if (!pictureAfter)
	{
		ShowWindow(editorWindow, SW_SHOWNORMAL);
	}
	WindowMode = WindowModeSubWindow;

	// Direct3D couldn't start: the box says why, with the last error the game's own start noted (logString.h), as plain text. The game
	// writes the same line into its log, avp_log.txt.
	if (!InitialiseDirect3D())
	{
		std::string reason = GetLastErrorMessage();
		while (!reason.empty() && (reason.back() == '\n' || reason.back() == '\r'))
		{
			reason.pop_back();
		}
		return Stop((std::string("Direct3D couldn't start in the editor's window. The last error the game noted: ") +
			(reason.empty() ? std::string("none (the game's log, avp_log.txt, may say more)") : reason)).c_str());
	}
	// No panels' swap chain (ViewSurface_Start): the panels are drawn in the game's own frame, over the level (L-1)
	if (!LevelView_Start(why, sizeof(why)))
	{
		ReleaseDirect3D();
		return Stop(why);
	}

	if (noFile)
	{
		int const code = NoFileScreen(client.right - client.left, client.bottom - client.top, pictureAfter ? picture : nullptr);

		ReleaseDirect3D();
		return code;
	}

	// The small font the panels' text and the menu bar are drawn in is loaded by LevelView_Start above, as the game's own start loads it
	// (winmain.cpp:915-934): its picture, its letters' widths, the red grid taken off it, and the menus' two slider pictures. (Joined
	// 2026-11-14 by Melanikus (Hicks): F3 loaded it here too, which would have made it twice.)

	// The menu bar, from the table of actions for the key layout in use (LoadTheMenus, which also switches off the items that can't be
	// used now). A table the bar refuses is a mistake in the editor's own code: it stops here, saying so.
	// The widest bar's titles, with the room the gray behind them takes past the last (PlanThePanels), as the least client width
	// the titles wrap onto up to kEditorBarMostRows rows, so the least width is the one where the widest page of every screen still wraps into that many (and no title is wider than the window)
	MenuFontStart();   // inteng's own menu font, before the bar is measured with it
	IntengExeStart();  // inteng's own exe, for its built-in shapes (the editor icons) and tables (V-25)
	barMinClientWidth = EditorBar_LeastWidth(keyLayout, PanelPass_MenuWidth);

	if (!LoadTheMenus())
	{
		ReleaseDirect3D();
		return Stop("the editor's own table of actions doesn't make a menu bar (editor_actions.cpp)");
	}

	// A -viewmode from the command line: the View menu's own toggles, each used once, in order, before the first frame, and the status
	// line says what the last one did
	for (size_t t = 0; t < startToggleCount && t < kCommandWordMostToggles; t++)
	{
		statusWords = ViewToggles_UseNow(startToggles[t]);
	}

	// A proof run that starts on the Texture Map screen is in the mode, as if its item had been used (the draw mode is Polygon, the previous one kept for the way out)
	if (IsTheTextureMapScreen(barScreen))
	{
		EnterTextureMap();
	}

	FlyCamera camera;
	theCamera = &camera;   // Find moves it (M-1)
	openLevelName = levelName;

	if (!LevelView_Open(levelName, &camera, why, sizeof(why)))
	{
		ReleaseDirect3D();
		return Stop(why);
	}

	statusWords = LevelView_SayMissingPictures(statusWords);   // MISSING PICTURES: the user is told how many (level_view.h)

	// THE FIRST FRAME, as inteng's: the hub (FUN_00447550) ends with FUN_00470550(10), the camera from slot 10, which holds the origin (0, 0, 0) and the identity turn (DAT_00636f90 + 0x78,
	// DAT_005d3b38 + 0x168: nothing but a camera file beside the level, <RIFFNAME>.cam, written by inteng's own Exit, fills them in, and the level's CAMORIGN adds its offset to every slot
	// at FUN_00470950: both still to be read, the camera file by this editor never being written), and the Gouraud switch is OFF (lighting starts off: FUN_004d67f0's DAT_008f4f0c = 0). Every
	// other slot starts the same. (Joined 2026-11-16 by Melanikus (Dietrich), startup-trace.md section 6.)
	{
		FlyCamera const atTheOrigin = { 0, 0, 0, 0, 0, 0 };

		camera = atTheOrigin;

		for (int slot = 0; slot <= 10; slot++)
		{
			cameraSlots[slot] = atTheOrigin;
			cameraSlotSet[slot] = true;
		}

		if (ViewToggles_Now().gouraud == host::Gouraud::On)
		{
			ViewToggles_UseNow(host::ViewToggle::GouraudSwitch);
		}
	}


	// A -picture run says at once where the level put things and where the camera starts (level_view.h), so a run cut short has it too.
	// (Added 2026-11-14 by Melanikus (Dietrich), F3.)
	if (pictureAfter)
	{
		printf("AVP_OpenInteng: start: %s\n", LevelView_StartWords());
		fflush(stdout);
	}

	// The level as the editor holds it (NotHeldWords says what a failure means): its file's bytes, and the chunks the game's own loader
	// made from exactly those bytes (level_file.h). LevelObjects_Hold opens it from the path the game loaded the level from, so the
	// picture and the objects are of one file, and hands it to the lookups (level_objects.h); the stock proof opens its levels through
	// the same call. It's let go of at the end, before the game's level is closed. (Added 2026-11-15 by Melanikus (Dietrich), after
	// checkpoint 1's hidden run found the object list empty: the game's own copy of the level's file is gone once the level is loaded.)
	std::unique_ptr<OpenLevelFile> openLevel(new OpenLevelFile);
	const char *const notHeldWords = NotHeldWords(LevelObjects_Hold(openLevel.get()));
	editedLevel = openLevel.get();

	// OP-137 / OP-137a (inteng FUN_005149e0 -> FUN_00512fc0(0) while the rif is read): the open path refreshes the editor's own copy of the file with OutsideShapes_RefreshFile BEFORE the
	// game loads it for the view, so the view and this held level are both read from the refreshed bytes (the call is just after OpenWith_TakeLevel; the refresh itself is not done here).
	copySavedChanges = openLevel->Changes();   // Move relative and Name object change its bytes (M-1)
	viewChanges = openLevel->Changes();               // the game's view is of these bytes (ShowTheWorkingBytes)

	// OP-137b (Bishop's ruling Q2, 11-17: EVERY open path, as inteng refreshes at every load, FUN_005149e0): a level opened by its NAME from the data folder (Level=, no file on the command
	// line) is never written, so its outside shapes are brought up to date on the HELD level, in memory (ReplaceLevel). Its Changes() then differs from the view's, so the view loads those
	// working bytes on the next frame (ShowTheWorkingBytes); a save writes them, to the copy the user picks. The double-click path did it on the editor's own copy, above.
	if (levelFile.empty() && !notHeldWords)
	{
		std::vector<std::string> refreshed;

		if (OutsideShapesUpdate(openLevel.get(), false, std::string(), nullptr, nullptr, &refreshed) > 0)
		{
			outsideShapesWords = OutsideShapesWordsOf(refreshed);
		}
	}

	// THE CAMERA FILE (inteng, FUN_00470950): the eleven saved camera places of this level, from <RIFFNAME>.cam, read after the first frame's camera is set (so they are used from the next F key or mode start). Ours is read from and
	// written to the editor's own folder (camera_file.h says what is named); written at every slot save and when the program ends (WriteTheCameraFile, FUN_00470610).
	cameraFileName.clear();
	cameraFolder = std::string(ownFolder) + "camera\\";
	cameraFileOffset[0] = cameraFileOffset[1] = cameraFileOffset[2] = 0;

	if (host::CameraFile_Name(LevelObjects_Level(), &cameraFileName))
	{
		host::CameraFileSlot loaded[host::kCameraFileSlots];

		host::CameraFile_Offset(LevelObjects_Level(), cameraFileOffset);

		if (host::CameraFile_Read(cameraFolder + cameraFileName, cameraFileOffset, loaded))
		{
			for (size_t s = 0; s < host::kCameraFileSlots; s++)
			{
				cameraSlots[s] = CameraOfFileSlot(loaded[s]);
				cameraSlotSet[s] = true;
			}

			if (pictureAfter)
			{
				printf("AVP_OpenInteng: camera file: read %s\n", cameraFileName.c_str());
				fflush(stdout);
			}
		}
	}

	// A file of objects with no modules (the HUD weapons, the character models) draws nothing through the game's modules: every object of it is made a display block of the game's own, where the file puts it
	// (hierarchy_view.h, ModelObjects_Build), and the camera starts in front of them all. (Added 2026-11-15 by Melanikus (Frost), the marwep fix.)
	if (pictureAfter)
	{
		printf("AVP_OpenInteng: model objects: the game has %d modules (ModuleArraySize)\n", (int)LevelView_ModuleCount());
		fflush(stdout);
	}

	// inteng starts with LIGHTING OFF in every file (Misc > Lighting, item 0x54, FUN_004d67f0 (0x4d67f0): its flag DAT_008f4f0c is 0 in .bss at the start and FUN_004d68e0 ticks the item from it at 0x4d8409; with the flag 0 the
	// function keeps the textured polygons plain, item types 6 -> 5 and 0x18 -> 7): textures at their full colours, unshaded. The editor's Gouraud switch is that item; a -viewmode on the command line is what a person asked for and is kept.
	// (Bishop 11-16 00:05, Melanikus's rule 10a: no substitutes. Added 2026-11-15 by Melanikus (Frost).)
	if (startToggleCount == 0 && ViewToggles_Now().gouraud == host::Gouraud::On)
	{
		statusWords = ViewToggles_UseNow(host::ViewToggle::GouraudSwitch);
	}

	bool anyModule = false;

	for (int32_t m = 0; m < LevelView_ModuleCount() && !anyModule; m++)
	{
		std::string moduleName;

		anyModule = LevelView_ModuleName(m, &moduleName);   // a module the game has in play (the game's own array always holds its terminator)
	}

	if (!anyModule && LevelObjects_Level())
	{
		std::string said;
		size_t const made = ModelObjects_Build(LevelObjects_Level(), &said);

		LevelView_DrawObjectsOutsideModules(made > 0);
		int32_t mins[3], maxs[3];

		if (pictureAfter)
		{
			printf("AVP_OpenInteng: model objects: %s\n", said.c_str());
			fflush(stdout);
		}

		if (made > 0)
		{
			ModelObjects_Bounds(mins, maxs);

			// WHERE inteng puts the camera when a file opens (traced from inteng.exe): FUN_00447d50 (the file open) loads the file (FUN_0042b8c0), then FUN_00447550 (the hub's set-up) restores the camera from SLOT 10 (FUN_00470550(10): the
			// position at DAT_00636f90 + 10 x 12 and the 3 x 3 matrix at DAT_005d3b38 + 10 x 36 into the camera object, DAT_00962304's +0x68), and only then FUN_00470950 reads the file's saved camera slots (a file named after the RIF, never
			// applied until an F key). The slots start as zero places and identity matrices (inteng.exe's .bss and .data), so a file opened first, with no camera file, shows from 0 0 0, looking along +z, level. The objects of a
			// file are drawn where the file puts them.
			camera.x = 0;
			camera.y = 0;
			camera.z = 0;
			camera.turn = 0;
			camera.look = 0;
			FlyCamera_Hold(&camera);

			if (pictureAfter)
			{
				printf("AVP_OpenInteng: model objects: bounds %d %d %d to %d %d %d; the camera at %d %d %d (inteng's slot 10: the origin, level); Gouraud switched off as inteng's Lighting starts\n", (int)mins[0], (int)mins[1], (int)mins[2], (int)maxs[0], (int)maxs[1], (int)maxs[2], camera.x, camera.y, camera.z);
				fflush(stdout);
			}
		}
	}

	// OP-140 (Bishop's ruling 11-17, a named difference): which outside shapes the open brought up to date; after the start's Gouraud words, and a level not held says so over it
	if (!outsideShapesWords.empty())
	{
		// whatever the start put on the line stays after them: the inteng.exe warning (IntengExeStart), a -viewmode's words (open-path-6), how many pictures are missing
		// (LevelView_SayMissingPictures, which hands back words of its own, so the warning can't be told by its pointer: 11-18, after bishop-int's ad73774b)
		if (statusWords && statusWords[0])
		{
			outsideShapesWords += std::string(" - ") + statusWords;
		}

		statusWords = outsideShapesWords.c_str();
	}

	if (notHeldWords)
	{
		statusWords = notHeldWords;
	}

	// The object list, from the level just opened (level_objects.h). The list copies what it shows at once, as the level's own bytes are
	// only good while it stays open. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
	{
		std::vector<host::LevelObject> found;

		bool const listRead = LevelObjects_Read(&found);
		objectList.Load(found.data(), found.size());
		RotationCentreDefault();   // the file open's hub set-up (FUN_00447d50 -> FUN_00447550 0x447652, CAM-7)
		fieldHost.Clear();
		shownRecord = host::FieldRecord();
		fieldsSawChanges = host::Selection_Changes();
		listSawChanges = host::Selection_Changes();

		// A -picture run says how many rows the list got, its first row's ID and name, and whether the level's tree was read at all, so a
		// list that came up empty (no tree, no objects in it, or every ID 0) is seen at once. (Added 2026-11-14 by Melanikus (Dietrich),
		// after checkpoint 1's hidden run found the list empty.)
		if (pictureAfter)
		{
			char firstId[2 * host::kObjectIdBytes + 1] = "none";
			const uint8_t *const id = objectList.Id(0);

			for (size_t i = 0; id && i < host::kObjectIdBytes; i++)
			{
				_snprintf_s(firstId + i * 2, 3, _TRUNCATE, "%02X", (unsigned)id[i]);
			}

			printf("AVP_OpenInteng: object list: %u rows, first id %s, first name \"%s\", LevelObjects_Read returned %s, the level's file held by the "
				"editor: %s\n", (unsigned)objectList.Count(), firstId, objectList.Count() ? objectList.Name(0).c_str() : "",
				listRead ? "true" : "false", notHeldWords ? notHeldWords : "yes");
			fflush(stdout);
		}

		if (objectList.Cut())
		{
			statusWords = kListCutWords;
		}
	}

	long frames = 0;
	bool pictureSaved = false;
	bool deviceBroken = false;
	bool tookTheKeyboard = false;
	// For a -picture run's report: the sizes the last frame was drawn at, and how many frames waited for the device instead (Added
	// 2026-11-14 by Melanikus (Dietrich), F3.)
	ui::Box drawnView = { 0, 0, 0, 0 };
	int drawnWindowWidth = 0;
	int drawnWindowHeight = 0;
	long framesWaited = 0;
	ScreenPictureReport screenReport = { false, false, 0.0, 0.0 };
	bool screenHidden = false;         // the window wasn't on the screen, so no screen picture could be taken
	bool firstPanelsSaved = false;     // and the last frame's, as first-panels.png
	ui::Box drawnTitles[ui::kMenuMostMenus] = {};
	while (running)
	{
		StateLine_NewFrame();   // the state line's added fields are this frame's (state_line.h)

		// With nobody at the screen, the editor's window must never become the one with the keyboard (nobody_watching.h). If it ever
		// does, the run says so and fails, so that's never missed. (It's the editor's own window that could: the view is its child.)
		if (pictureAfter && GetForegroundWindow() == editorWindow)
		{
			tookTheKeyboard = true;
		}
		MSG message;
		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
		if (!running)
		{
			break;
		}

		// Whether this frame can be drawn (view_surface.h), with the view's child window where the layout puts it. When it can't, while
		// the window is minimised or the device is lost, the processor is given back for about 10 ms, or until a message comes, so the
		// editor never spins a whole core waiting. (Added 2026-11-14 by Melanikus (Dietrich), F3.)
		RECT window = { 0, 0, 0, 0 };
		RECT view = { 0, 0, 0, 0 };
		GetClientRect(editorWindow, &window);
		int const windowWidth = window.right - window.left;
		int const windowHeight = window.bottom - window.top;
		ui::PanelLayout const layout = PlaceTheView(windowWidth, windowHeight);
		GetClientRect(hWndMain, &view);
		frameViewWidth = view.right - view.left;   // the camera's wide-view turn (MoveTheCamera)
		bool const canDraw = ViewSurface_BeforeFrame(view.right - view.left, view.bottom - view.top, windowWidth, windowHeight);
		if (!canDraw)
		{
			framesWaited++;

			// A device that will never come back: say so and close, rather than show a blank window for good
			if (ViewSurface_IsBroken())
			{
				deviceBroken = true;
				break;
			}

			MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
			continue;
		}

		BeginTheFrameStep();   // the frame's distance for the move keys, from the time since the last frame (inteng's speed x frame time)

		// Scroll Lock's pick waiting (CAM-8): inteng's frame stands still inside FUN_004720c0's loop, so nothing moves (the keys held meanwhile act after it, as inteng's held table has them)
		if (ModalWaitOn())
		{
		}
		// Fly (entry points): the keys move the marker picked, the camera stays where it is (FUN_0058b550 swaps the movement state of the camera and the marker)
		else if (entryFlying)
		{
			FlyCamera const before = camera;

			MoveTheCamera(&camera);

			double const scale = local_scale > 0.0 ? local_scale : 1.0;

			entryFlyPlace[0] += (int32_t)((double)(camera.x - before.x) / scale);
			entryFlyPlace[1] += (int32_t)((double)(camera.y - before.y) / scale);
			entryFlyPlace[2] += (int32_t)((double)(camera.z - before.z) / scale);
			camera = before;
			statusWords = "Moving Entry Point";
		}
		else if (flyOn)
		{
			// F: the keys move and turn the flying object, linked or not: it has the mover (slot 2), the camera block slot 0 (0x4df0b1-0x4df0b7); linked (R), the camera follows it in UpdateCameraLink,
			// the orbit's C turned with the object's matrix (V-17)
			MoveTheFlyingObject(true);

		}
		else
		{
			MoveTheCamera(&camera);
		}

		if (!ModalWaitOn())   // Scroll Lock's wait stands the frame still (FUN_004720c0's own loop): no screen frame reads a key or moves anything
		{
			UpdateCameraLink(&camera);
			HierarchyCameraRootFrame(&camera);
		}
		HierarchySaveIfAsked();

		if (chooserActive)
		{
			// the preview: the camera is where FUN_004dd6f0 puts it, 2 x the shape's maxx behind the origin, level
			camera.x = 0;
			camera.y = 0;
			camera.z = chooserCameraZ;
			camera.turn = 0;
			camera.look = 0;
			camera.roll = 0;
			FlyCamera_Hold(&camera);   // the game's own camera is set from this one
			LevelView_HideModules(nullptr, 0, true);
			LevelView_HideObjects(nullptr, 0, true);
			ShapePreview_Spin();   // the preview turns, as inteng's does every frame (FUN_004ddb60 0x4ddba2-0x4ddbc2)
		}

		cameraDrawn = camera;
		ScreenLookPass();      // the screen's entry look: the white wire of the hub, Module Linking, Adjacent Modules, Path and Waypoint
		ViewToggles_Apply();   // the View menu's switches, given to the game before it draws the level (view_toggles_game.h)
		frames++;

		// THE FRAME CAP (view-state-table.md H-8; Bishop's ruling 11-16, 10b: copy inteng). inteng's loop waits, after its text and before it draws (WinMain 0x58d012-0x58d02e: GetTickCount
		// again and again), until 40 ms (0x28) have passed since the last frame: at most 25 frames a second (his screenshot: "Frame Rate = 21", GetTickCount's own steps). What depends
		// on frames runs at inteng's speed then: Light Sourcing's blinking attribute (frame % 3, FUN_004ac430), keys held for a number of frames. NAMED: ours sleeps to that moment
		// instead of spinning.
		{
			static DWORD lastFrameTick = 0;
			DWORD const sinceLast = GetTickCount() - lastFrameTick;

			if (lastFrameTick != 0 && sinceLast < 40)
			{
				Sleep(40 - sinceLast);
			}

			lastFrameTick = GetTickCount();
		}

		// the view distance, kept for the next start whenever it changed (H-7; inteng writes it at the end, FUN_004732a0: kept at once here, as the speed is)
		{
			static int32_t viewDistanceWritten = -1;

			if (viewDistance != viewDistanceWritten && layoutSettingsFile[0] && !viewDistanceFromCommandLine)
			{
				char number[16];

				_snprintf_s(number, sizeof(number), _TRUNCATE, "%d", viewDistance);
				WritePrivateProfileStringA("Editor", "ViewDistance", number, layoutSettingsFile);
				viewDistanceWritten = viewDistance;
			}
		}

		if (mixedProof && pictureAfter && frames == 5)
		{
			ViewToggles_UseNow(host::ViewToggle::Polyline);
			ViewToggles_SetMixed(true);

			int32_t const here[3] = { (int32_t)theCamera->x, (int32_t)theCamera->y, (int32_t)theCamera->z };
			int32_t const room = LevelView_ModuleAt(here);

			if (room >= 0)
			{
				showAllModules.assign(1, room);
				showAllColours.assign(1, 0xFF7F007Fu);
			}

			printf("AVP_OpenInteng: mixed proof: the room under the camera is %d\n", room);
		}

		if (showAllProof && pictureAfter && frames == 5)
		{
			ShowAllTheModules();
			printf("AVP_OpenInteng: %s\n", statusWords ? statusWords : "");
		}

		if (strategyAllKinds && pictureAfter)
		{
			RunTheAllKindsProof(frames);
		}

		if (strategyProof && pictureAfter)
		{
			RunTheStrategyProof(frames);
		}

		AutoLinkFrame();   // AutoLink's run, when one is on (autolink_runner.h): its trees, its display, its end
		DeleteLinksFrame();   // Delete Links' run, when one is on: one module a frame

		if (autoLinkShowFeed)   // in a harness run too (its -picture names the level, so pictureAfter is 0 there)
		{
			FeedTheAutoLinkShow(frames);
		}

		if (trackProof && pictureAfter)
		{
			RunTheTrackProof(frames);
		}

		if (exitProof && pictureAfter)
		{
			RunTheExitProof(frames);
		}

		if (modelProof && pictureAfter)
		{
			RunTheModelProof(frames);
		}

		if (sClickProof && pictureAfter)
		{
			RunTheSClickProof(frames);
		}

		if (movementProof && pictureAfter)
		{
			RunTheMovementProof(frames);
		}

		if (hideProof && pictureAfter && frames == 5)
		{
			LevelView_HideObjects(nullptr, 0, true);
		}

		// -hierarchyproof: the model is shown three metres in front of the camera on the fifth frame, at its point of its sequence
		if (hierarchyProof && pictureAfter && frames == 5)
		{
			int32_t place[3] = { camera.x, camera.y, camera.z };
			std::string why;
			bool const shownIt = ShowTheModelInFront(hierarchyProofModel.c_str(), hierarchyProofHierarchy.c_str(), hierarchyProofSequence, hierarchyProofSub, place, &why);

			if (hierarchyProofPosition < 0)
			{
				HierarchyView_Play(true, true);   // a position below 0 means play it: the picture is taken after as many frames as -picture says, 50 ms each
			}
			else
			{
				HierarchyView_SetPosition(hierarchyProofPosition);
			}
			int32_t const cameraPlace[3] = { camera.x, camera.y, camera.z };

			printf("AVP_OpenInteng: hierarchy proof: model %s sequence %d.%d at %d: %s%s (the camera is in room %d, the model in room %d)\n", hierarchyProofModel.c_str(), hierarchyProofSequence, hierarchyProofSub,
				hierarchyProofPosition, shownIt ? "shown at " : "NOT SHOWN: ", shownIt ? (std::to_string(place[0]) + " " + std::to_string(place[1]) + " " + std::to_string(place[2])).c_str() : why.c_str(),
				(int)LevelView_ModuleAt(cameraPlace), (int)LevelView_ModuleAt(place));

			std::vector<HierarchyViewSequence> sequences;

			{
				std::vector<HierarchyViewSection> sections;

				if (HierarchyView_Sections(&sections))
				{
					printf("AVP_OpenInteng: hierarchy proof: sections (%s):", HierarchyView_HasTurret() ? "has a dum turret" : "no dum turret");

					for (HierarchyViewSection const &one : sections)
					{
						printf(" [%s]", one.name.c_str());
					}

					printf("\n");
				}
			}

			if (HierarchyView_Sequences(&sequences))
			{
				printf("AVP_OpenInteng: hierarchy proof: the hierarchy's sequences (type.sub: frames, length):");

				for (HierarchyViewSequence const &one : sequences)
				{
					printf(" %d.%d: %d, %d;", one.sequence, one.sub, one.frames, one.time);
				}

				printf("\n");
			}
		}

		if (HierarchyView_IsShown() && onHierarchy)
		{
			hierarchyMode.position = HierarchyView_Position();
		}

		if (HierarchyView_IsShown())
		{
			static DWORD lastHierarchyTick = 0;
			DWORD const nowTick = GetTickCount();
			int const elapsed = lastHierarchyTick == 0 ? 0 : (int)(nowTick - lastHierarchyTick);

			lastHierarchyTick = nowTick;

			int const frameMs = pictureAfter ? 50 : (elapsed > 250 ? 250 : elapsed);

			// , / . HELD (HP-17, FUN_004889d0 0x489f9d-0x48a030): each frame a quarter of the frame time (DAT_00d876dc, 16.16 seconds; (t + (t >> 31 & 3)) >> 2), ',' first then '.'.
			// No Shift: FUN_0047bb20 / FUN_0047b9c0 move a STOPPED main's timer (and with Y's switch the stopped turret and deltas); Shift: inteng's [2] (the newest delta, else the turret), running or not.
			// inteng's FUN_0047b9c0 / FUN_0047bb20 add a stopped delta's step to its +0x28 (the running word, 0x47baaa / 0x47bbfd), so it starts running: INTENG BUG, FIXED (section 15, HP-17): its timer moves.
			if (onHierarchy && !AMenuIsOpen() && !ADialogIsOpen() && promptFor == kPromptNone && !ModalWaitOn() && (keyDown[VK_OEM_COMMA] || keyDown[VK_OEM_PERIOD]))
			{
				int32_t const frameTime = (int32_t)(((int64_t)frameMs * 65536) / 1000);
				int32_t const quarter = (frameTime + ((frameTime >> 31) & 3)) >> 2;

				// ',' first, then '.', each its own call as inteng's two tests (0x48a018 / 0x48a026): with both held a stopped delta is already running after ',' (Vasquez's review)
				if (keyDown[VK_OEM_COMMA])
				{
					ShiftDown() ? HierarchyView_ScrubSecond(-quarter, hierarchyMode.deltaSelected) : HierarchyView_ScrubMain(-quarter, hierarchyPlayAll);
				}

				if (keyDown[VK_OEM_PERIOD])
				{
					ShiftDown() ? HierarchyView_ScrubSecond(quarter, hierarchyMode.deltaSelected) : HierarchyView_ScrubMain(quarter, hierarchyPlayAll);
				}
			}

			// every new player starts with "Play commands affect all sequences" off (FUN_00477450 0x477603: +0x88 = 0)
			{
				static uint32_t playersSeen = 0;

				if (hierarchyMode.playersMade != playersSeen)
				{
					playersSeen = hierarchyMode.playersMade;

					if (hierarchyPlayAll)
					{
						hierarchyPlayAll = false;

						if (HierarchyView_ToggleTurretFollows())
						{
							HierarchyView_ToggleTurretFollows();
						}
					}
				}
			}

			// the applied shape sets on the model (HF-0x43, inteng FUN_00477b40 -> FUN_0047e9f0): put on again when the list or the model shown changes
			{
				static std::string appliedKey;
				std::string key = std::to_string(HierarchyView_ShowCount()) + ":";

				for (int32_t const member : hierarchyMode.applied)
				{
					key += std::to_string(member) + ",";
				}

				if (key != appliedKey && hierarchyModelOpen)
				{
					std::vector<model::AlternateSetInfo> alternates;
					std::vector<std::string> names;

					if (!hierarchyMode.applied.empty() && host::HierarchyEdit_AlternateSets(&hierarchyModelFile, &alternates) == host::HierarchyEditResult::Done)
					{
						for (int32_t const member : hierarchyMode.applied)
						{
							for (model::AlternateSetInfo const &one : alternates)
							{
								if (one.number == member)
								{
									names.push_back(one.name);
								}
							}
						}
					}

					HierarchyView_ApplyShapeSets(names);
					appliedKey = key;
				}
			}

			HierarchyView_Frame(frameMs);
			if (onHierarchy && HierarchyView_IsPlaying() && !pictureAfter && GetTickCount() > hierarchyAnswerUntil)
			{
				HierarchyStatus();
			}
		}

		// -texmapproof: on the fifth frame, the game has its blocks in play: the polygon map is checked against them and some polygons are selected for the picture
		if (texMapProof && pictureAfter && frames == 5)
		{
			EnterTextureMap();
			RunThePolygonMapCheck();
			SelectSomePolygonsForTheProof();
			RunTheFlipProof();
		}

		bool const lastFrame = pictureAfter && frames >= pictureAfter;
		// A harness picture asked for since the last frame is saved as this frame is drawn, before it's shown, as first-picture.png is. On
		// a -picture run's last frame, first-picture.png is saved instead, and the harness is told its picture wasn't.
		int const harnessPicture = harnessPictureAsked;
		char harnessPicturePath[MAX_PATH] = "";
		char harnessScreenPath[MAX_PATH] = "";
		char harnessPanelsPath[MAX_PATH] = "";

		harnessPictureAsked = 0;

		bool const harnessPathsMade = harnessPicture && !lastFrame
			&& _snprintf_s(harnessPicturePath, sizeof(harnessPicturePath), _TRUNCATE, "%spicture-%d.png", folder, harnessPicture) >= 0
			&& _snprintf_s(harnessScreenPath, sizeof(harnessScreenPath), _TRUNCATE, "%sscreen-%d.png", folder, harnessPicture) >= 0
			&& _snprintf_s(harnessPanelsPath, sizeof(harnessPanelsPath), _TRUNCATE, "%spanels-%d.png", folder, harnessPicture) >= 0;

		// This frame's panels, worked out first and added to the game's own frame as it is drawn, over the level (L-1)
		host::PaintText panelLines[kPanelPassMostExtras];
		host::PaintBox panelBoxes[kPanelPassMostExtras];
		FramePanels framePanels = { windowWidth, windowHeight, PlanThePanels(layout, panelLines, panelBoxes, drawnTitles) };
		// The selected object is drawn in green (level_view.h)
		{
			uint8_t ids[kLevelViewMostSelected][host::kSelectionIdBytes];
			size_t const count = host::Selection_SetCount() < kLevelViewMostSelected ? host::Selection_SetCount() : kLevelViewMostSelected;

			for (size_t i = 0; i < count; i++)
			{
				host::Selection_SetMember(i, ids[i]);
			}

			LevelView_SelectObjects(ids, count);
		}

		// The three orthographic views, on the track and point screens: over the whole middle of the window, fitted to the level once it is known
		{
			bool const on = ThreeViewsOn() && !(placementSplit && barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Track Editing") == 0);

			threeBox = ui::Box{ 0, layout.view.y, windowWidth, layout.view.height };

			// The lines in the 3D view (Waypoint mode's boxes, the entry point markers) are taken away when that mode is not shown
			static bool worldLinesShown = false;

			RefreshTheEntryMarkers();

			// Scroll Lock's wait stands the screen's frame still (FUN_004720c0's own loop runs only the message pump and its pick): the keys held meanwhile change nothing (no light, no view distance,
			// no scale) until it ends (camera-closeout-drake.md CAM-8)
			if (!ModalWaitOn())
			{
				LightKeysEveryFrame();
			}

			AdjacencyEveryFrame();

			PathTintsEveryFrame();

			if (!ModalWaitOn())
			{
				StrategyViewDistanceEveryFrame();
				HubViewDistanceEveryFrame();
			}

			// Path mode's H and N (FUN_004f9d10 0x4f9e74: slots 0xd87a88 / 0xd87a8e): the view distance +500000 for each frame H is held, -500000 for N (never below 0)
			if (OnThePathScreen() && !AMenuIsOpen() && !ModalWaitOn())
			{
				int64_t const next = (int64_t)viewDistance + (keyDown['H'] ? 500000 : 0) - (keyDown['N'] ? 500000 : 0);

				viewDistance = ViewDistanceHeld(next);
			}

			// Object Placement's H and N, its view distance rule and the data of its text lines (PlacementFrame: OP-80, OP-4 (5)); off that screen it takes its rule away
			if (!ModalWaitOn())
			{
				PlacementFrame();
			}

			static bool trianglesShown = false;

			// the selected object is RED in Object Placement and Strategy Data (inteng's block mark 0x40000000 -> item flag 0x80 -> 0xff0000), green elsewhere
			// while a kind's Edit view runs, the current object is not red (FUN_005286b0 0x5286d7 FUN_00529480 before the view, 0x5286f9 FUN_00529470 after it): its green shows
			// the Construction hub's selection (view-state-table.md H-2a, H-2c: FUN_00444750 0x4449b7 / 0x44498c): ORANGE 0xff7f00 for a module and an object alike, GREEN while Module Overlaps is on
			bool const onTheHub = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5ced30") == 0;
			uint32_t const hubSelection = moduleOverlapsOn ? 0xFF00FF00u : 0xFFFF7F00u;

			// Module Linking draws no selection colour: only the tree's colours (FUN_004c0720) and its picks 0x7f007f (FUN_004c17a0 0x4c18b2). Our selection's green went to a picked DOOR
			// (an object by its ID) before the pick's colour, and the first colour given to a block wins: the door showed green, not 0x7f007f (Dietrich 11-16)
			// Copy Strategy's collector: the source (the selection) is not red while it runs (FUN_00528340 0x5283a5 FUN_00529480; back at 0x5285bd); its members are (FlagViewTints)
			// while a view's OBJECT PICK runs (FUN_00527540) the selection is the pick, and it IS red: 0x52754a / 0x527629 FUN_00529470, off at 0x527620 / 0x52763a / 0x527676 (S-62)
			LevelView_SetSelectedColour(OnModuleLinkingScreen() || (OnStrategyScreen() && ((strategyEditFan && strategyPickField.empty()) || copyCollecting)) ? 0u :
				(OnAPlacementScreen() || OnStrategyScreen() ? 0xFFFF0000u : (onTheHub ? hubSelection : 0xFF00FF00u)));

			bool const iconsWanted = !OnALightScreen() && !(stabilityActive && OnStrategyScreen()) && (OnAPlacementScreen() || OnStrategyScreen()) && !placementSplit;   // (inteng's Edit view's black screen showed none, S-KC-1; the owner's box draws the level, its icons too, round the view's box, which covers what is behind it)

			// HIERARCHY'S PLATFORM FLOOR (hierarchy-closeout-frost.md TB-4; Vasquez 11-17): FUN_0048d230 makes it at the mode's start (0x4887e0), one polygon of four points 100,000 of inteng's units
			// either side across x and 1,000,000 along z at y 0 (1 m and 10 m of the world's), the texture "Platform.rim" (FUN_0057a850 0x57a978: DAT_00915078) loaded through CL_LoadImageOnce,
			// and when that finds no file a flat WHITE polygon (0x48d30e: kind 2, colour 0xffffff); FUN_0048d5c0 shows it while the platform is on (Toggle on/off, DAT_00637118) and hides it
			// when it is off, scrolling the texture's v by the speed. His Tools folder (and the tools' own install) has no Platform.rim, so inteng draws the white floor: ours draws that.
			// NAMED: a Platform.rim is never looked for here (no textured floor, so no scrolling).
			bool const platformWanted = onHierarchy && hierarchyMode.platformOn;
			std::vector<LevelViewTriangle> mergeTriangles;
			bool const mergeWanted = TextureMergeFrame(&mergeTriangles);   // Texture Map's Merge viewing: the shape's polygons in their colours (texture_submodes.cpp)

			if (trianglesShown && !OnALightScreen() && !(stabilityActive && OnStrategyScreen()) && !iconsWanted && !platformWanted && !mergeWanted)
			{
				LevelView_WorldTriangles(nullptr, 0);
				trianglesShown = false;
			}

			// the platform floor: its four points, two triangles, white (Hierarchy is none of the screens below, which draw their own triangles)
			if (platformWanted)
			{
				int32_t const corner[4][3] = { { -1000, 0, 10000 }, { 1000, 0, 10000 }, { 1000, 0, -10000 }, { -1000, 0, -10000 } };
				LevelViewTriangle floor[2];

				for (int i = 0; i < 3; i++)
				{
					floor[0].a[i] = corner[0][i];
					floor[0].b[i] = corner[1][i];
					floor[0].c[i] = corner[2][i];
					floor[1].a[i] = corner[0][i];
					floor[1].b[i] = corner[2][i];
					floor[1].c[i] = corner[3][i];
				}

				floor[0].colour = floor[1].colour = 0xFFFFFFFFu;
				trianglesShown = true;
				LevelView_WorldTriangles(floor, 2);
			}

			if (mergeWanted)
			{
				trianglesShown = true;
				LevelView_WorldTriangles(mergeTriangles.data(), mergeTriangles.size());
			}
			else if (OnALightScreen())
			{
				static uint32_t markerFrame = 0;
				std::vector<LevelViewTriangle> lightTriangles;

				LightMarkerTriangles(&lightTriangles, markerFrame++);
				worldLinesShown = true;
				trianglesShown = true;
				LevelView_WorldLines(nullptr, 0);
				LevelView_WorldTriangles(lightTriangles.data(), lightTriangles.size());
			}
			else if (iconsWanted)
			{
				static uint32_t iconFrame = 0;
				std::vector<LevelViewTriangle> iconTriangles;

				SpecialIconTriangles(&iconTriangles, iconFrame++, OnStrategyScreen());
				trianglesShown = true;
				LevelView_WorldTriangles(iconTriangles.data(), iconTriangles.size());
			}
			else if (stabilityActive && OnStrategyScreen())
			{
				std::vector<host::TrackLine> boxLines;
				std::vector<LevelViewTriangle> boxTriangles;

				StabilityShapes(&boxLines, &boxTriangles);
				worldLinesShown = true;
				trianglesShown = true;
				LevelView_WorldLines(boxLines.data(), boxLines.size());
				LevelView_WorldTriangles(boxTriangles.data(), boxTriangles.size());
			}
			else if (!entryMarkers.empty())
			{
				std::vector<host::TrackLine> markerLines;

				EntryMarkerLines(&markerLines);
				worldLinesShown = true;
				LevelView_WorldLines(markerLines.data(), markerLines.size());
			}
			else if (worldLinesShown && !WaypointWorldOn())
			{
				LevelView_WorldLines(nullptr, 0);
				worldLinesShown = false;
			}

			// inteng's 3-view screen shows the TRACK of the selected object, in the object's own space, centre at 0, 0, 0 (run 18). Its lines are made again when
			// the selection changes or the track does.
			TrackPlayFrame();
			TrackWorldLines();

			// Edit Extents' three views: inteng's are cameras on the same scene (FUN_005846b0), so they show the boxes and links as the 3D view draws them (WaypointView_WorldLines, W-2 / W-3)
			if (on && WaypointViewsOn())
			{
				std::vector<host::TrackLine> wpLines;

				host::WaypointView_WorldLines(waypointMode, local_scale, &wpLines);

				// FUN_005846b0 / FUN_005880e0 (W-5): each view centred on the AI module's master's place (made again each time Edit Extents is entered) and moved by Shift + right drag
				// (WaypointPanDrag), at the mode's scale of inteng's units a pixel (100 to the millimetre: 5000 is 50 mm a pixel)
				if (waypointMode.ViewsMade() != waypointViewsSeen)
				{
					int32_t centreFile[3];
					double const scale = local_scale > 0.0 ? local_scale : 1.0;

					waypointMode.ViewsCentre(centreFile);

					for (int i = 0; i < 3; i++)
					{
						waypointViewCentre[i] = (int64_t)floor((double)centreFile[i] * scale + 0.5);
					}

					waypointViewsSeen = waypointMode.ViewsMade();
				}

				host::ThreeView_Centre(waypointViewCentre, waypointMode.ViewScale() / 100 > 0 ? waypointMode.ViewScale() / 100 : 1, &threeViews);
				threeViewsFitted = true;
				LevelView_ThreeViewLines(wpLines.data(), wpLines.size());
				trackSeenChanges = 0xFFFFFFFFu;   // the track's own lines are made again when a track screen comes back
			}
			else if (on && (host::Selection_Changes() != trackSeenChanges || trackDirty || trackPlaying))
			{
				std::vector<host::TrackLine> trackLines;
				bool const remake = host::Selection_Changes() != trackSeenChanges || trackDirty;
				int32_t maxAbs[3] = { 0, 0, 0 };
				host::Selected const shown = host::Selection_Get();
				host::TrackClip clip;

				if (!remake)
				{
					trackLines = trackPlayBase;   // the track as it was when the play began: only the cross moves
				}
				else
				{
				trackDirty = false;
				trackSeenChanges = host::Selection_Changes();

				if (shown.kind == host::SelectedKind::Object && TrackClipNow(LevelObjects_OpenForEdit(), shown.objectId, &clip) == host::TrackEditResult::Done
					&& clip.track.size() > 12)
				{
					uint8_t const *const payload = (uint8_t const *)clip.track.data() + 12;
					size_t const length = clip.track.size() - 12;

					if (host::TrackView_Section(payload, length, 0, -1, &trackLines, maxAbs, &trackSectionCount))
					{
						host::ThreeView_FitTrack(maxAbs, &threeViews);

						// the current section's start and end markers only, as FUN_004f0da0 shows them; the number wraps round the count as it does
						trackSection = trackSectionCount ? (int32_t)(((int64_t)trackSection % (int64_t)trackSectionCount + (int64_t)trackSectionCount) % (int64_t)trackSectionCount) : 0;
						host::TrackView_Section(payload, length, 6 * threeViews.top.scaleX, trackSection, &trackLines, maxAbs, nullptr, !trackShowStart && OnTheTrackScreen());
					}
				}

				if (trackLines.empty())
				{
					host::ThreeView_FitTrack(maxAbs, &threeViews);
					trackSectionCount = 0;
				}

				if (trackPlayBaseWanted)
				{
					trackPlayBase = trackLines;
					trackPlayBaseWanted = false;
				}
				}

				// the object being played along the track, as the game's own track code places it: a cross in the three views
				if (trackPlaying)
				{
					int32_t half = 6 * threeViews.top.scaleX;

					half = half > 0 ? half : 1000;

					for (int axis = 0; axis < 3; axis++)
					{
						host::TrackLine line = {};

						memcpy(line.from, trackPose.place, sizeof(line.from));
						memcpy(line.to, trackPose.place, sizeof(line.to));
						line.from[axis] -= half;
						line.to[axis] += half;
						line.colour = host::kTrackOffsetColour;
						trackLines.push_back(line);
					}
				}

				threeViewsFitted = true;
				LevelView_ThreeViewLines(trackLines.data(), trackLines.size());
			}

			// Waypoint mode's modes 1 to 3: the 3D view draws the boxes and the links in inteng's colours (waypoint_view.h WaypointView_WorldLines, W-2 / W-3; the avpx hook R_SetEditorWorldLines)
			if (WaypointWorldOn())
			{
				std::vector<host::TrackLine> worldWaypoints;

				host::WaypointView_WorldLines(waypointMode, local_scale, &worldWaypoints);
				worldLinesShown = true;
				LevelView_WorldLines(worldWaypoints.data(), worldWaypoints.size());
			}

			// The current module of the module screens is drawn purple, as inteng draws the picked module there (Construction, Adjacent Modules, Module Linking)
			{
				bool moduleScreen = false;

				if (barScreen < host::kIntengScreenCount)
				{
					char const *const table = host::kIntengScreens[barScreen].table;

					moduleScreen = strcmp(table, "0x5ced30") == 0 || strcmp(table, "0x5c3508") == 0 || strcmp(table, "0x5e2e38") == 0 || strcmp(table, "0x5e2c88") == 0;
				}

				host::Selected const current = host::Selection_Get();

				LevelView_SetCurrentModule(moduleScreen && current.kind == host::SelectedKind::Module ? current.moduleIndex : -1);
				// H-2a: on the hub the selected module is the selection's ORANGE (green with Module Overlaps on), not the module screens' purple
				LevelView_SetCurrentModuleColour(onTheHub ? hubSelection : 0xFF7F007Fu);

				// Module Linking's own view rules (view distance, only the linked modules)
				LinkFollowPass(barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5e2e38") == 0);
				ModuleLinkingViewPass(barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5e2e38") == 0, barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5c3508") == 0);
				TextureMapViewPass(onTextureMap);   // Texture Map's view distance rule (FUN_00544070; texture_map_screen.cpp)
			}


			LevelView_ThreeViews(on, threeViews, threeCursor, threeBox.x, threeBox.y, threeBox.width, threeBox.height);
			WaypointHidePass();
			UpdateModuleTints(barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].table, "0x5c3508") == 0);
			UpdateDummyObject();
			LevelView_SetProjectionScale((onHierarchy && hierarchyMode.viewKind == 1 ? 0.5f : 1.0f) * zoomScale);
			zoomViewBox = threeMainBox.width > 0 ? threeMainBox : layout.view;
			// Hierarchy's detail level is always the mode's own (off at the start, 0x4887da): with Adaptive Degradation on, inteng's pick sets it each frame (HierarchyAdaptivePick, FUN_00479b80),
			// never the game's own pick by the depth to the model
			LevelView_SetAdaptiveDegradation(!onHierarchy);
			HierarchyAdaptivePick();
			HierarchyView_SetDetailLevel(onHierarchy ? hierarchyDetail : -1);
			if (onHierarchy)
			{
				HierarchyView_ApplyDetail();
			}
			UpdateHiddenModules();

			// THE VIEW SWITCH (reassess/hierarchy.md H-VS-1 / H-VS-2 / H-VS-5): a hierarchy chosen in Hierarchy mode makes inteng's player, FUN_0048b550 -> FUN_00477450(hier, 0, list of EVERY record of
			// the level, 0x48b5ea-0x48b606), which sets +0x5c |= 4 (not drawn) and +0x64 |= 0x800000 (never shown again by a view-distance pass) on each of them (0x4776f0-0x477765): every module and every
			// placed object disappears and only the hierarchy's own sections are drawn (FUN_0047dc90), the "animation player" Melanikus saw. Choosing another hierarchy hands the hidden list on (0x48b729-
			// 0x48b779): the level stays hidden. Exit (FUN_004888d0) frees the player; the level's records come back with the next screen. Ours: every module and every object the level places is not drawn
			// while the Hierarchy screen has a hierarchy chosen (re-asserted each frame, after the hidden set); the model's blocks are no placed objects (no strategy block), so they are drawn.
			// NAMED DIFFERENCE: inteng draws the chosen hierarchy's sections in their rest place before a sequence is chosen (the player has no main sequence, [0] = 0); ours shows the model once a
			// sequence is chosen (the game's pose code needs one): between the two dialogs the view is empty.
			{
				static bool levelHiddenForHierarchy = false;
				bool const want = onHierarchy && hierarchyModelOpen && !hierarchyMode.hierarchy.empty();

				// BEFORE A SEQUENCE (HP-31): the chosen hierarchy's sections are the file's own objects at the origin with the identity turn (FUN_0047e320), drawn there until a main sequence places them;
				// once the view shows the model (a sequence chosen) they are hidden with the rest. NAMED: done for a file of objects with no modules (his HUD and weapon files, ModelObjects); in a level
				// with modules the sections' placed objects are not moved.
				{
					static std::string collapsedFor;
					bool const collapseWanted = want && !HierarchyView_IsShown() && ModelObjects_Count() > 0;
					std::string const key = hierarchyMode.modelName + "|" + hierarchyMode.hierarchy;

					if (collapseWanted && collapsedFor != key)
					{
						std::vector<std::string> sectionNames;
						std::vector<host::LevelObject> objects;
						std::vector<std::vector<uint8_t>> ids;

						if (host::HierarchyEdit_SectionObjects(&hierarchyModelFile, hierarchyMode.hierarchy, &sectionNames) == host::HierarchyEditResult::Done && LevelObjects_Read(&objects))
						{
							for (host::LevelObject const &object : objects)
							{
								for (std::string const &name : sectionNames)
								{
									if (object.name && _stricmp(object.name, name.c_str()) == 0)
									{
										ids.push_back(std::vector<uint8_t>(object.id, object.id + 8));
										break;
									}
								}
							}
						}

						ModelObjects_Collapse(ids);
						collapsedFor = key;
					}
					else if (!collapseWanted && !collapsedFor.empty())
					{
						ModelObjects_Uncollapse();
						collapsedFor.clear();
					}
				}

				if (want)
				{
					// the file's objects: placed objects of the level (strategy blocks), and for a file with no modules (his HUD and weapon files) the blocks the editor made for them
					std::vector<const struct displayblock *> fileObjects;

					ModelObjects_Blocks(&fileObjects);
					LevelView_HideModules(nullptr, 0, true);
					LevelView_HideObjects(nullptr, 0, true);
					LevelView_HideBlocks(fileObjects.data(), fileObjects.size());
				}
				else if (levelHiddenForHierarchy)
				{
					LevelView_HideModules(nullptr, 0, false);
					LevelView_HideObjects(nullptr, 0, false);
					LevelView_HideBlocks(nullptr, 0);
					hiddenModulesStale = true;   // Module Linking's hidden set comes back on the next frame
				}

				levelHiddenForHierarchy = want;
			}

			// The bottom right quarter is the 3D view (inteng's layout): the world is drawn into it, so a click in it picks from its own pixels
			bool const split = placementSplit && !on;

			threeMainBox = (on || split) && threeBox.width >= 64 && threeBox.height >= 48
				? ui::Box{ threeBox.x + threeBox.width / 2, threeBox.y + threeBox.height / 2, threeBox.width / 2, threeBox.height / 2 } : ui::Box{ 0, 0, 0, 0 };
			LevelView_MainBox(threeMainBox.width > 0, threeMainBox.x, threeMainBox.y, threeMainBox.width, threeMainBox.height);

			// Toggle Split Screen: three more cameras round the current object (or the view), 5 m (5000 mm, the game's unit) back along z, x and y
			if (split && threeMainBox.width > 0)
			{
				int centre[3] = { camera.x, camera.y, camera.z };

				// OP-45 / OP-46 / OP-40: the place the views were put round (SplitCentreOnCurrent); they don't follow the camera
				if (splitCentreOn)
				{
					centre[0] = splitCentre[0];
					centre[1] = splitCentre[1];
					centre[2] = splitCentre[2];
				}

				int const halfW = threeBox.width / 2;
				int const halfH = threeBox.height / 2;
				LevelViewExtra views[3] =
				{
					{ { centre[0], centre[1], centre[2] - 5000, 0, 0 }, threeBox.x, threeBox.y, halfW, halfH },                   // along +z
					{ { centre[0] - 5000, centre[1], centre[2], 1024, 0 }, threeBox.x + halfW, threeBox.y, halfW, halfH },        // along +x (a quarter turn)
					{ { centre[0], centre[1] - 5000, centre[2], 0, 3072 }, threeBox.x, threeBox.y + halfH, halfW, halfH },        // along +y: down (the look counts up from 0)
				};

				LevelView_SetExtraViews(views, 3);
			}
			else
			{
				LevelView_SetExtraViews(nullptr, 0);
			}
		}

		char const *const askedPicture = lastFrame ? picture : (harnessPathsMade ? harnessPicturePath : nullptr);
		char const *const screenshotTo = !askedPicture && !screenshotPending.empty() ? screenshotPending.c_str() : nullptr;
		char const *const shotTo = !askedPicture && !screenshotTo && !shotPending.empty() ? shotPending.c_str() : nullptr;   // #78
		if ((openAt.at || openAt.shot) && !openAt.applied && LevelObjects_OpenForEdit())
		{
			std::string openedAt;

			host::OpenAt_Apply(&openAt, &openedAt);   // -at / -shot: the camera and the selection, once the level is up (open_at.h)
			LogString(openedAt);
			Answer(openedAt);
		}
		if (!startGotoObject.empty())
		{
			OpenLevelFile *const here = LevelObjects_OpenForEdit();
			std::vector<host::LevelObject> everyObject;

			if (here && here->Level())
			{
				LevelObjects_Read(&everyObject);

				for (host::LevelObject const &one : everyObject)
				{
					model::ShapeModel model;
					std::string why;

					if (one.name && startGotoObject == one.name && host::ShapeEdit_Read(here->Level(), here, one.id, &model, nullptr, &why) == host::ShapeEditResult::Done)
					{
						for (size_t p = 0; p < model.polygons.size(); p++)
						{
							if (model::EngineTypeIsTextured(model.polygons[p].engineType))
							{
								TextureFound found = {};

								memcpy(found.id, one.id, 8);
								found.polygon = (int32_t)p;
								textureFound.assign(1, found);
								textureFoundAt = 0;
								TextureGoto(1);
								break;
							}
						}

						break;
					}
				}

				startGotoObject.clear();
			}
		}

		if (textureAutoMinutes > 0 && onTextureMap && promptFor == kPromptNone && GetTickCount() - textureAutoLast > (DWORD)textureAutoMinutes * 60000u)
		{
			textureAutoLast = GetTickCount();

			if (textureAutoPrompt)
			{
				textureAsk = 14;
				memset(keyDown, 0, sizeof(keyDown));
				promptFor = kPromptTextureMap;
				promptText.clear();
			}
			else
			{
				SaveTheCopy();
			}
		}

		AnimPlayTick();

		// A strategy's Edit view (each kind's method +0x14) fills the WHOLE window black every pass in inteng (FUN_00418b80(&DAT_0091f6c0, 0), e.g. FUN_0052cbb0 0x52cbe7) and draws only its own lines:
		// the level is not drawn while it is up (reassess/strategy-data-kinds.md S-KC-1). THE OWNER'S BOX (11-17, NAMED): ours draws the view, its pages and E's pages in a black box centred in
		// the window (editor_panels.cpp StrategyViewBoxIn) and the level round it, as round Generator Settings. A pick or the Lift's collector draws the screen as usual (S-KC-5); the volume
		// editor (FUN_005277c0) draws the level with its red cube and the picked module red (S-VOL-1)
		// Object Placement's look (OP-3 / OP-44): the call does only what changed since the last frame (the look's start or end, a Lighting flip, a level opened again)
		PlacementLighting_Frame(barScreen < host::kIntengScreenCount ? host::kIntengScreens[barScreen].name : nullptr);
		char const *const savedTo = askedPicture ? askedPicture : (screenshotTo ? screenshotTo : shotTo);
		bool const drawn = LevelView_Draw(camera, savedTo, AddThePanels, &framePanels);

		if (shotTo)
		{
			ShotFinish(drawn, camera, true, windowWidth, windowHeight);
		}

		if (screenshotTo)
		{
			screenshotWords = (drawn ? "Screenshot saved: " : "Screenshot NOT saved: ") + screenshotPending;
			statusWords = screenshotWords.c_str();
			screenshotPending.clear();
		}

		pictureSaved = drawn && lastFrame;
		drawnView = layout.view;
		proofViewWidth = drawnView.width;
		proofViewHeight = drawnView.height;
		drawnWindowWidth = windowWidth;
		drawnWindowHeight = windowHeight;

		if (onTextureMap) TextureSelectionUndo_BeginStep();   // FUN_00543fa0 0x543fa5: every frame of Texture Map starts a selection step (texture_shared.h)

		// A click waiting from before this frame picks from it now, while the game's camera and its list of what's on screen are this
		// frame's (pick_game.h). The place was in the view's pixels then; a view resized since only moves the line a little, and
		// PickGame_AtClick refuses a place outside it. (Added 2026-11-14 by Melanikus (Frost).)
		if (textureArrowAsked)
		{
			int const asked = textureArrowAsked;

			textureArrowAsked = 0;

			if (onTextureMap)
			{
				TextureArrowSelect(asked - 1, (asked == 1 || asked == 4) ? textureArrowVertical : textureArrowHorizontal, view.right - view.left, view.bottom - view.top);
			}
		}

		if (textureRectAsked)
		{
			textureRectAsked = false;

			if (onTextureMap && (textureSelectMode == 2 || textureSelectMode == 3) && (textureRectX1 != textureRectX0 || textureRectY1 != textureRectY0))
			{
				TextureSelectRect(textureRectX0, textureRectY0, textureRectX1, textureRectY1, view.right - view.left, view.bottom - view.top, textureSelectMode == 3, textureRectShift);
			}
		}

		// the live Snap To Vertex: the snap under the pointer, every frame (FUN_004d9af0 0x4da855-0x4da8ce)
		SnapLiveFrame(view.right - view.left, view.bottom - view.top);

		{
			std::string cornerWords;

			OrbitCommand_Frame(view.right - view.left, view.bottom - view.top, &cornerWords);   // the harness's camera-corner trial (orbit_command.h)

			if (!cornerWords.empty())
			{
				LogString(cornerWords);
				Answer(cornerWords);
			}
		}

		// Acoustic Mode: the module under the pointer, every frame (FUN_004f57b0 0x4f5bd6), for its lines
		if (OnAcousticScreen())
		{
			AcousticPointerFrame(view.right - view.left, view.bottom - view.top);
		}

		if (onTextureMap && textureFlagList && textureHoverSet)
		{
			textureHoverSet = false;
			TextureFlagListMake(PickGame_AtClick(textureHoverX, textureHoverY, view.right - view.left, view.bottom - view.top));
		}

		if (pickAsked)
		{
			pickAsked = false;
			host::PickReport const picked = pickInBox ? PickGame_AtClick(pickX, pickY, threeMainBox.width, threeMainBox.height)
				: PickGame_AtClick(pickX, pickY, view.right - view.left, view.bottom - view.top);

			// A command waiting for its click (P4) takes this one, and the click then doesn't change the selection
			// the live Snap To Vertex: the left press keeps the pose shown, and picks nothing else (FUN_004d9af0 0x4da8d6: DAT_008f4f10 = 0; FUN_004d6520 skipped)
			// Scroll Lock's pick first (CAM-8: WinMain's modal FUN_004720c0): the place of what it hit becomes the rotation centre; a press that hit nothing waits on
			if (rotationPickWaiting)
			{
				RotationPickAnswer(picked);
			}
			else if (snapLive)
			{
				SnapLiveEnd(true);
			}
			else if (host::ClickMode_Waiting())
			{
				host::ClickAnswer const answer = host::ClickMode_Click(picked);

				if (answer.done && answer.command == kClickMoveRelPoly)
				{
					MoveRelPolyClicked(answer.report);
				}
				else if (answer.done && answer.command == kClickSnapVertex)
				{
					SnapToVertexClicked(answer.report);
				}
				else if (answer.done && answer.command == kClickParentGenerator)
				{
					ParentClicked(answer.report);
				}
				else if (answer.done && answer.command == kClickRecentrePoly)
				{
					RecentrePolygonClicked(answer.report);
				}
				else
				{
					statusWords = answer.done ? "Picked what the command waited for (no item reads it yet)" : host::ClickMode_Words();
				}
			}
			else if (strategyPick != 0)
			{
				// Strategy mode's T (change target) and V (edit volume): the click is the pick, and the selection stays as it is
				StrategyPickClicked(picked);
			}
			else if (volume.on)
			{
				VolumeClick(picked, pickShift, pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
			}
			else if (onHierarchy && HierarchyView_IsShown())
			{
				hierarchyPickViewW = pickInBox ? threeMainBox.width : view.right - view.left;
				hierarchyPickViewH = pickInBox ? threeMainBox.height : view.bottom - view.top;
				HierarchyClicked(picked);
			}
			else if (stabilityActive && OnStrategyScreen())
			{
				StabilityClick(pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
			}
			else if (copyCollecting && OnStrategyScreen())
			{
				CopyCollectClick(picked, pickShift, pickCtrl, pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
			}
			else if (strategyEditFan && OnStrategyScreen() && !strategyPickField.empty())
			{
				StrategyViewPickClicked(picked, pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
			}
			else if (strategyEditFan && OnStrategyScreen())
			{
				// from the box's top left (x too: the Track, Hierarchy and Multi pages read x bands, strategy_views.h). A left click outside the box does nothing: inteng's page was the whole screen,
				// so it had no outside (NAMED, with the box); the right button ends the view anywhere, as before
				if (strategyViewBox[2] <= 0 || (pickX >= strategyViewBox[0] && pickX < strategyViewBox[0] + strategyViewBox[2] && pickY >= strategyViewBox[1] && pickY < strategyViewBox[1] + strategyViewBox[3]))
				{
					StrategyViewClickAt(pickX - strategyViewLeft, pickY - strategyViewTop);
				}
			}
			else if (OnStrategyScreen() && !pickWithS)
			{
				StrategyListClick(picked, pickShift, pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
			}
			else if (pickWithS)
			{
				// Strategy mode's S + click (FUN_00525700 0x525db2): every module that is mostly the clicked polygon's texture joins the selection
				StrategySelectByTexture(picked);
			}
			else if (onTextureMap)
			{
				texturePickViewW = pickInBox ? threeMainBox.width : view.right - view.left;
				texturePickViewH = pickInBox ? threeMainBox.height : view.bottom - view.top;
				TextureMapClicked(picked, pickShift);
			}
			else if (PickAnEntryMarker(pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top))
			{
				// Waypoint mode, AI modules: the click took an entry point marker (statusWords says which)
			}
			else if (OnWaypointScreen() && (waypointMode.Mode() == 1 || waypointMode.Mode() == 2))
			{
				// Waypoint mode's waypoints: the box nearest the click on the screen (FUN_00584ee0 0x58542d), not the current one or the base; Edit Extents' press in the 3D quarter the same
				// (FUN_00585750 0x585cff)
				WaypointBoxClick(pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
			}
			else if (OnALightScreen() && clickPicksLights)
			{
				// Light Sourcing's Select: Light: a placed light under the pointer, else the light whose centre is nearest it on the screen (FUN_004ab2c0, TB-L3)
				PickTheLight(picked, pickX, pickY, view.right - view.left, view.bottom - view.top);
			}
			else if (OnALightScreen())
			{
				// Light Sourcing's Select: Object: the object whose centre is nearest the pointer on the screen (FUN_004ab200, TB-L4)
				PickTheLightScreenObject(pickX, pickY, view.right - view.left, view.bottom - view.top);
			}
			else
			{
				// Module Linking's Select With Centres (0xcd) on: the click takes the module whose centre is nearest the pointer on the screen (FUN_004c3760)
				// Select by polygons OFF (Construction's Options, 0x1f4, FUN_004449f0; Adjacent Modules' 0xca, FUN_00405690): the click takes the object whose centre is nearest the pointer (named: placed objects are
				// not candidates here, as in Module Linking's)
				bool const byCentres = (selectWithCentres && OnModuleLinkingScreen()) || (!editorOptions.selectByPolygons && !OnALightScreen() && !onTextureMap && !onHierarchy);

				// Object Placement's own click (FUN_004d6520): among the PLACED objects only (modules are not selected there); by polygons the object whose block is under the pointer, else the nearest centre
				bool const onPlacement = barScreen < host::kIntengScreenCount && strcmp(host::kIntengScreens[barScreen].name, "Object Placement") == 0;
				host::PickReport used = picked;

				if (onPlacement)
				{
					// OP-120: one current object, the nearest projected centre among the placed and the special objects, Shift not read (PlacementClickPick)
					PlacementClickUse(PlacementClickPick(pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top));
				}
				else
				{
					if (byCentres)
					{
						used = PickTheNearestCentre(pickX, pickY, pickInBox ? threeMainBox.width : view.right - view.left, pickInBox ? threeMainBox.height : view.bottom - view.top);
					}

					if (OnAcousticScreen())
					{
						AcousticClick(picked, pickShift);   // AC-2: FUN_00559720, the block under the pointer, whatever the Options say
					}
					else if (OnModuleLinkingScreen())
					{
						LinkClick(used, pickShift, pickDelete);   // row ML-M-61
					}
					else
					{
						// With Module Overlaps on, the hub's click selects ONE whatever the Shift (FUN_00444750: DAT_0063126c lets go of the selection first, 0x444760) and shows that
						// module's overlaps only (TB-C11)
						bool const overlapsClick = barScreen == host::kIntengHub && moduleOverlapsOn;

						statusWords = pickShift && !overlapsClick ? host::Selection_UsePickToggling(used) : host::Selection_UsePick(used);

						if (overlapsClick)
						{
							// the picked one's object: a module the line met is held by its game module number, an object by its ID
							uint8_t pickedId[8];
							OpenLevelFile *const overlapsOpen = LevelObjects_OpenForEdit();
							bool const havePicked = (used.found == host::PickFound::Module && overlapsOpen && ModuleObject(overlapsOpen, used.moduleIndex, pickedId)) ||
								(used.found == host::PickFound::Object && (memcpy(pickedId, used.objectId, 8), true));

							if (havePicked)
							{
								ModuleOverlapsPicked(pickedId);
							}
						}

						// Waypoint's AI modules mode: inteng's click (FUN_00583d00 0x58402a): the pick and its colours (Shift is ours, for Entry points > Delete's two modules)
						if (!pickShift && OnWaypointModules())
						{
							OpenLevelFile *const open = LevelObjects_OpenForEdit();

							waypointMode.Click(open, WaypointPickedObject(open));
						}
					}
				}
			}
		}

		// The harness's answers, once the level and the panels are both shown (see THE HARNESS'S QUESTIONS)
		// The panels are in the game's own frame, so they are in its picture: the files the harness and a run's report call the panels'
		// picture are copies of that one (L-1: there is one picture, and the panels are not drawn anywhere else)
		bool panelsSavedNow = false;

		if (drawn && lastFrame)
		{
			panelsSavedNow = CopyFileA(picture, panelsPicture, FALSE) != 0;
		}
		else if (drawn && harnessPathsMade)
		{
			panelsSavedNow = CopyFileA(harnessPicturePath, harnessPanelsPath, FALSE) != 0;
		}

		if (lastFrame)
		{
			firstPanelsSaved = panelsSavedNow;
		}

		if (harnessPicture)
		{
			ScreenPictureReport harnessScreen = { false, false, 0.0, 0.0 };

			bool const harnessHidden = !IsWindowVisible(editorWindow);

			if (harnessPathsMade && !harnessHidden)
			{
				MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);

				ui::Box const patch = { hitBar.x + hitBar.width - 10, hitBar.y + 4, 8, 8 };

				ScreenPicture_Save(d3d.lpD3DDevice, editorWindow, harnessScreenPath, layout.view, patch, kPanelPassBackground, &harnessScreen);
			}

			printf("AVP_OpenInteng: picture %d %s; the screen %d %s\n", harnessPicture, harnessPathsMade && drawn ? "saved" : "NOT SAVED",
				harnessPicture, harnessHidden ? "not taken, the window is hidden" : !harnessScreen.saved ? "NOT SAVED"
				: (harnessScreen.panelsSeen ? "ok" : "NOT THE EDITOR'S WINDOW"));
			printf("AVP_OpenInteng: panels %d %s\n", harnessPicture, harnessPathsMade && panelsSavedNow ? "saved" : "NOT SAVED");
			printf("AVP_OpenInteng: selection: in play %s, green items drawn %d\n", LevelView_SelectedIsInPlay() ? "yes" : "no", LevelView_SelectedItemsDrawn());
			fflush(stdout);
		}

		if (harnessStateAsked)
		{
			// The view mode in -viewmode's own words, the whole of it: the draw mode, then "+unlit" with the Gouraud switch off and
			// "+headlight" with the headlight on, so a mix is never named by only one of its parts. At most 30 letters, inside the 31 the
			// harness reads (drive_editor.cpp). (Modified 2026-11-14 by Melanikus (Frost): inteng's own modes, view-state-table.md.)
			auto const now = ViewToggles_Now();
			const char *const drawWord = now.drawMode == host::DrawMode::Polyline ? "polyline" : now.drawMode == host::DrawMode::FilledPolyline
				? "filledpolyline" : now.drawMode == host::DrawMode::WireTextured ? "wire-textured" : "polygon";
			char modeWord[32];
			_snprintf_s(modeWord, sizeof(modeWord), _TRUNCATE, "%s%s%s", drawWord, now.gouraud == host::Gouraud::Off ? "+unlit" : "",
				now.headlight ? "+headlight" : "");
			char openMenu[16] = "none";
			char litItem[16] = "none";

			harnessStateAsked = false;

			if (menuBar.IsOpen())
			{
				_snprintf_s(openMenu, sizeof(openMenu), _TRUNCATE, "%d", menuBar.OpenMenu());
			}

			if (menuBar.IsOpen() && menuBar.LitItem() >= 0)
			{
				_snprintf_s(litItem, sizeof(litItem), _TRUNCATE, "%d", menuBar.LitItem());
			}

			printf("AVP_OpenInteng: state: menu %s item %s, camera %d %d %d %d %d, view mode %s, view %dx%d at %d,%d, window %dx%d, panels %ux%u\n",
				openMenu, litItem, camera.x, camera.y, camera.z, camera.turn, camera.look, modeWord, layout.view.width, layout.view.height,
				layout.view.x, layout.view.y, windowWidth, windowHeight, 0u, 0u);
			fflush(stdout);
		}

		if (harnessScenarioAsked)
		{
			// OP-26c: whether the current placed object is in the game's view (has its display block), for the harness: "yes" / "no", empty when no placed object is current
			host::ModuleLinkAvp_StateLine();
			StateLine_Add("inplay", host::Selection_Get().kind == host::SelectedKind::Object ? (LevelView_SelectedIsInPlay() ? "yes" : "no") : "");

			std::string titles1, titles2;

			// the titles of the two LOGICAL rows (inteng's row 1 with the editor's own menus, and its row 2), whatever physical rows they wrap into at this width
			for (size_t m = 0; m < barRows.logical[0].menuCount; m++)
			{
				titles1 += (m ? "|" : "") + std::string(barRows.logical[0].menus[m].title);
			}

			for (size_t m = 0; m < barRows.logical[1].menuCount; m++)
			{
				titles2 += (m ? "|" : "") + std::string(barRows.logical[1].menus[m].title);
			}

			harnessScenarioAsked = false;

			host::Selected const nowSelected = host::Selection_Get();
			std::string selectedWords = nowSelected.kind == host::SelectedKind::Module ? "module " + std::to_string(nowSelected.moduleIndex)
				: nowSelected.kind == host::SelectedKind::Object ? "object" : nowSelected.kind == host::SelectedKind::Light ? "light " + std::to_string(nowSelected.lightIndex) : "nothing";

			// The selected object AS THE GAME'S VIEW DRAWS IT: the middle of its block's box in the world, "x <x> y <y> z <z>" in millimetres, "" when no object is selected or it is not in play.
			// The view gap's acceptance reads it: the picture is of the editor's working bytes, not of the opened file (ShowTheWorkingBytes)
			std::string selectedAt;

			if (nowSelected.kind == host::SelectedKind::Object)
			{
				int32_t boxMins[3], boxMaxs[3];

				if (LevelView_ObjectBox(nowSelected.objectId, boxMins, boxMaxs))
				{
					// its box's middle, then its size (width x, height y, depth z: a same-size edit such as a Rescale shows in the size)
					selectedAt = "x " + std::to_string(((int64_t)boxMins[0] + boxMaxs[0]) / 2) + " y " + std::to_string(((int64_t)boxMins[1] + boxMaxs[1]) / 2) + " z " +
						std::to_string(((int64_t)boxMins[2] + boxMaxs[2]) / 2) + " width " + std::to_string((int64_t)boxMaxs[0] - boxMins[0]) + " height " +
						std::to_string((int64_t)boxMaxs[1] - boxMins[1]) + " depth " + std::to_string((int64_t)boxMaxs[2] - boxMins[2]);
				}
			}

			// The Hierarchy view's model: the box of its drawn points on the view's pixels ("x0 y0 x1 y1 viewWidth viewHeight"), "behind" when a point is behind the camera, "" when no model is shown. The scenario
			// verb expect-model-inside reads it: the framing of the model is proven by the numbers, not only by a picture.
			std::string modelBox;

			if (onHierarchy && HierarchyView_IsShown())
			{
				host::PickCamera boxCamera;
				std::vector<int32_t> boxPoints;

				if (PickGame_Camera(layout.view.width, layout.view.height, &boxCamera) && HierarchyView_WorldPoints(&boxPoints))
				{
					double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
					bool behind = false;

					for (size_t i = 0; i + 2 < boxPoints.size(); i += 3)
					{
						int32_t const world[3] = { boxPoints[i], boxPoints[i + 1], boxPoints[i + 2] };
						double px = 0, py = 0, ahead = 0;

						if (!host::PickRay_Project(boxCamera, world, &px, &py, &ahead))
						{
							behind = true;
							continue;
						}

						x0 = px < x0 ? px : x0;
						y0 = py < y0 ? py : y0;
						x1 = px > x1 ? px : x1;
						y1 = py > y1 ? py : y1;
					}

					char box[160];

					_snprintf_s(box, sizeof(box), _TRUNCATE, "%d %d %d %d %d %d", (int)x0, (int)y0, (int)x1, (int)y1, (int)layout.view.width, (int)layout.view.height);
					modelBox = behind ? "behind" : box;
				}
			}

			std::string dialogTitle, dialogRows, dialogRow1;

			// THE RIGHT-CLICK POP-UP CONTRACT of the acceptance harness (FUN_004b7c50 right press -> FUN_004b8350): while a pop-up is open, popup "<entry>|<entry>|..." (the entries of the level shown, in order);
			// popuprow1 "<x> <y> <width> <height>" (the first entry's box, in the layout's pixels as dialogrow1 is); popupstep "<dx> <dy>" (from one entry's box to the next one's: 0 and the entry height).
			// All three are empty when no pop-up is open.
			std::string popupEntries, popupRow1, popupStep;

			if (popupBar.IsOpen() && popupRoom.menuCount == 1)
			{
				for (size_t i = 0; i < popupRoom.menus[0].count; i++)
				{
					std::string entry = popupRoom.menus[0].items[i].name ? popupRoom.menus[0].items[i].name : "";

					// the "  >" after an entry that opens more is ours (the cascade's mark): the entry's own words are what is reported
					if (entry.size() > 2 && entry.compare(entry.size() - 2, 2, " >") == 0)
					{
						entry.resize(entry.size() - 2);
					}

					for (char const letter : entry)
					{
						popupEntries += (letter == '"' || letter == '|') ? '?' : letter;
					}

					popupEntries += i + 1 < popupRoom.menus[0].count ? "|" : "";
				}

				ui::Box const drop = popupBar.DropBox();
				int32_t const entryHeight = host::MenuPaint_ItemHeight(kPanelPassLineHeight);

				popupRow1 = std::to_string(drop.x) + " " + std::to_string(drop.y) + " " + std::to_string(drop.width) + " " + std::to_string(entryHeight);
				popupStep = "0 " + std::to_string(entryHeight);
			}

			// inteng's Hierarchy text of the frame: "<row>:<words>|..." (the words with no quote or bar; the one at x 200 is given as "<row>@200:<words>")
			std::string hierarchyLinesWord;

			for (HierarchyTextLine const &one : hierarchyTextNow)
			{
				hierarchyLinesWord += (hierarchyLinesWord.empty() ? "" : "|") + std::to_string(one.row) + (one.x ? "@" + std::to_string(one.x) : std::string()) + ":";

				for (char const letter : one.text)
				{
					hierarchyLinesWord += (letter == '"' || letter == '|') ? '?' : letter;
				}
			}

			host::DialogsState const dialogNow = host::Dialogs_State();
			StateLine_Add("dialogoverlist", dialogNow.overlist);   // the window's words painted over its open drop-down list (dialogs.cpp WordsOverTheOpenList)

			if (!dialogNow.kind.empty())
			{
				dialogTitle = dialogNow.title;
				dialogRows = dialogNow.rows;
				dialogRow1 = dialogNow.row1;
			}

			// the view mode and the camera, for a scenario to check (expect-field viewmode / camera): the draw mode, then +unlit with the Gouraud switch off and +headlight
			auto const viewNow = ViewToggles_Now();
			// the opened level's name for the game, and the size of the file it is read from (a scenario can tell WHICH file opened: the double-clicked one, not a stock one of the same name)
			long long scenarioLevelBytes = -1;

			{
				WIN32_FILE_ATTRIBUTE_DATA levelFacts;

				if (!openLevelName.empty() && GetFileAttributesExA(("avp_rifs\\" + openLevelName + ".rif").c_str(), GetFileExInfoStandard, &levelFacts))
				{
					scenarioLevelBytes = ((long long)levelFacts.nFileSizeHigh << 32) | levelFacts.nFileSizeLow;
				}
			}

			char scenarioViewMode[48];
			_snprintf_s(scenarioViewMode, sizeof(scenarioViewMode), _TRUNCATE, "%s%s%s", viewNow.drawMode == host::DrawMode::Polyline ? "polyline" : viewNow.drawMode == host::DrawMode::FilledPolyline ? "filledpolyline" : viewNow.drawMode == host::DrawMode::WireTextured ? "wire-textured" : "polygon", viewNow.gouraud == host::Gouraud::Off ? "+unlit" : "", viewNow.headlight ? "+headlight" : "");


			// The list dialog's rows on the page shown ("words@x,y,width,height|..."): the scenario verb click-choice clicks one by its words (a list of many rows is paged: click-button Next shows the next page)
			std::string const choiceBoxes = dialogNow.cells;

			std::string const dialogKindWord = dialogNow.kind;

			// Stability flags: the six faces as letters, r red (stable) or g green, empty when the mode is not on (for expect-field stability)
			// THE BAR FOR THE ACCEPTANCE HARNESS's click-bar: barboxes "<title>~<x> <y> <w> <h>|..." the titles of every physical row, dropboxes "<item>~<x> <y> <w> <h>|..." the items shown in the open drop-down
			std::string barBoxesWord, dropBoxesWord, dropTickedWord;
			// droprows "<words>|<words>|...": the WORDS each shown row of the open drop-down draws, as menu_paint.cpp draws them (the item's name, then its tag right after it when
			// it has one), so a scenario reads the menu as it is on the screen (dropboxes names the items to click by, and leaves a tag out)
			std::string dropRowsWord;
			// textScreenWord: the first line of the full text screen shown over the view (inteng's FUN_004724f0: a Help page, "Click to continue."), quotes taken out; empty when none
			std::string textScreenWord = textPanelText ? std::string(textPanelText).substr(0, std::string(textPanelText).find('\n')) : std::string();

			for (char &c : textScreenWord)
			{
				c = (c == '"' || c == '\r' || c == '\t') ? ' ' : c;
			}
   // dropTickedWord: the open drop-down's ticked items (MenuBar::IsChecked), joined by |
			std::string volumeWord;

			if (volume.on)
			{
				char volumeText[256];

				_snprintf_s(volumeText, sizeof(volumeText), _TRUNCATE, "lo %d %d %d hi %d %d %d", (int)volume.lo[0], (int)volume.lo[1], (int)volume.lo[2], (int)volume.hi[0], (int)volume.hi[1], (int)volume.hi[2]);
				volumeWord = volumeText;
				// the record the left click picked (a level object or an icon: S-VOL-4), by its ID
				volumeWord += volume.objectPicked ? " picked " + HexOf(volume.object) : std::string(" picked none");
			}


			for (size_t row = 0; row < barRowCount && row < barRows.rowCount; row++)
			{
				ui::MenuBar &rowBar = BarRow(row);

				for (size_t m = 0; m < barRows.menuCount[row]; m++)
				{
					ui::Box const t = rowBar.TitleBox(m);

					barBoxesWord += (barBoxesWord.empty() ? "" : "|") + std::string(barRows.menus[row][m].title) + "~" + std::to_string(t.x) + " " + std::to_string(t.y) + " " + std::to_string(t.width) + " " + std::to_string(t.height);
				}

				if (rowBar.IsOpen())
				{
					ui::Box const d = rowBar.DropBox();
					ui::Menu const &openMenu = barRows.menus[row][rowBar.OpenMenu()];

					for (size_t k = 0; k < rowBar.ShownItems(); k++)
					{
						dropBoxesWord += (dropBoxesWord.empty() ? "" : "|") + std::string(openMenu.items[rowBar.FirstShownItem() + k].name) + "~" + std::to_string(d.x) + " " + std::to_string(d.y + (int32_t)k * rowBar.ItemHeight()) + " " + std::to_string(d.width) + " " + std::to_string(rowBar.ItemHeight());

						ui::MenuItem const &drawn = openMenu.items[rowBar.FirstShownItem() + k];

						dropRowsWord += (dropRowsWord.empty() ? "" : "|") + std::string(drawn.name) + (drawn.tag ? drawn.tag : "");

						if (rowBar.IsChecked((size_t)rowBar.OpenMenu(), rowBar.FirstShownItem() + k))
						{
							dropTickedWord += (dropTickedWord.empty() ? "" : "|") + std::string(openMenu.items[rowBar.FirstShownItem() + k].name);
						}
					}

					// the shown CASCADE's rows (BAR-11), each named "<its item>: <the row>" (the name the bar had for it before the cascades, so a scenario's click-bar keeps working)
					int const owner = rowBar.CascadeOf();

					if (owner >= 0)
					{
						ui::Box const c = rowBar.CascadeBox();
						ui::MenuItem const &holder = openMenu.items[owner];

						for (size_t k = 0; k < holder.subCount && c.height > 0; k++)
						{
							std::string const name = std::string(holder.name) + ": " + holder.subItems[k].name;

							dropBoxesWord += "|" + name + "~" + std::to_string(c.x) + " " + std::to_string(c.y + (int32_t)k * rowBar.ItemHeight()) + " " + std::to_string(c.width) + " " + std::to_string(rowBar.ItemHeight());

							if (rowBar.IsCascadeChecked(k))
							{
								dropTickedWord += (dropTickedWord.empty() ? "" : "|") + name;
							}
						}
					}
				}
			}

			std::string stabilityWord;

			for (int face = 0; stabilityActive && face < 6; face++)
			{
				stabilityWord += (stabilityFaces & (1u << face)) ? 'r' : 'g';
			}

			// Object Placement's sounds as the game's sound code holds them (placed_sounds_game.h): how many it holds, has loaded and plays; whether the current sound plays (-1: no current
			// sound), whether Sound Edit's Play plays, whether there is a sound device
			int const currentSounding = specialMode.haveCurrent && specialMode.kind == host::SpecialKind::Sound ? (host::PlacedSounds_SoundingAt(specialMode.number) ? 1 : 0) : -1;
			std::string const placedSoundsWord = "held " + std::to_string(host::PlacedSounds_Held()) + " loaded " + std::to_string(host::PlacedSounds_Loaded()) + " sounding " +
				std::to_string(host::PlacedSounds_Sounding()) + " current " + std::to_string(currentSounding) + " preview " + (host::SoundGame_PreviewSounding() ? "1" : "0") + " device " +
				(host::SoundGame_DeviceOn() ? "1" : "0") + " hierarchy " + std::to_string(host::HierarchySounds_Playing());   // PH-2/3/14: the placed hierarchy's sounds playing

			// the editor's window (startup-screen-frost.md SS-13): its client's width and height, and whether it is inteng's full screen (a popup over the monitor) or a framed window
			RECT windowClient = { 0, 0, 0, 0 };

			GetClientRect(editorWindow, &windowClient);

			std::string const windowWord = std::to_string(windowClient.right - windowClient.left) + " " + std::to_string(windowClient.bottom - windowClient.top) +
				((GetWindowLongA(editorWindow, GWL_STYLE) & WS_POPUP) ? " popup" : " window");

			// the flying object's place while one flies (MoveTheFlyingObject: the world's millimetres), else empty
			std::string const flyAtWords = flyOn ? "x " + std::to_string(flyWorld[0]) + " y " + std::to_string(flyWorld[1]) + " z " + std::to_string(flyWorld[2]) : std::string();

			// the orbit while the camera is linked to the flying object (V-17: pitch and yaw in 4096ths, the distance in mm), else empty
			std::string const orbitWords = flyOn && cameraLinked ? std::string("pitch ") + std::to_string(cameraOrbitPitch) + " yaw " + std::to_string(cameraOrbitYaw) + " distance " + std::to_string(cameraLinkDistance) : std::string();

			// the flying object's turn since F and its own matrix by rows (A-7), while it flies as itself; else empty
			std::string flyTurnedWords, flyMatrixWords;

			if (flyOn && flyReal)
			{
				flyTurnedWords = "x " + std::to_string(flyTurnAcc[1]) + " y " + std::to_string(flyTurnAcc[0]) + " z " + std::to_string(flyTurnAcc[2]);
				flyMatrixWords = "right " + std::to_string(flyMatrix[0]) + " " + std::to_string(flyMatrix[1]) + " " + std::to_string(flyMatrix[2]) + " down " + std::to_string(flyMatrix[3]) + " " +
					std::to_string(flyMatrix[4]) + " " + std::to_string(flyMatrix[5]) + " forward " + std::to_string(flyMatrix[6]) + " " + std::to_string(flyMatrix[7]) + " " + std::to_string(flyMatrix[8]);
			}

			// the camera's turn, look and roll (4096ths), for the camera turn's acceptance
			std::string const camAnglesWords = "turn " + std::to_string(camera.turn) + " look " + std::to_string(camera.look) + " roll " + std::to_string(camera.roll);

			// the rotation centre (CAM-3 / CAM-7 / CAM-8), for the orbit's acceptance: its place; the camera's distance to it (mm); the camera's bearing round it (65536ths of a turn: atan2 of the camera's x over its
			// z from the centre); where the camera SEES it (its place along the camera's own axes, the view matrix times centre - camera: an orbit keeps it, a turn in place moves it); waiting: 1 while
			// Scroll Lock's pick waits
			std::string rotCentreWords;
			{
				int32_t view[9];

				LevelView_MatrixOfAngles(camera.turn, camera.look, camera.roll, view);

				double const d[3] = { (double)rotationCentre[0] - camera.x, (double)rotationCentre[1] - camera.y, (double)rotationCentre[2] - camera.z };
				long long seen[3];

				for (int i = 0; i < 3; i++)
				{
					seen[i] = (long long)floor((view[3 * i] * d[0] + view[3 * i + 1] * d[1] + view[3 * i + 2] * d[2]) / 65536.0 + 0.5);
				}

				long long const distance = (long long)floor(sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + 0.5);
				// in 65536ths of a turn, so a scenario's 16-bit difference (expect-number-moved) wraps where the turn does
				int const bearing = (int)floor(atan2(-d[0], -d[2]) * 65536.0 / (2.0 * 3.14159265358979323846) + 0.5) & 65535;

				rotCentreWords = "x " + std::to_string(rotationCentre[0]) + " y " + std::to_string(rotationCentre[1]) + " z " + std::to_string(rotationCentre[2]) + " dist " + std::to_string(distance) +
					" bearing " + std::to_string(bearing) + " seenx " + std::to_string(seen[0]) + " seeny " + std::to_string(seen[1]) + " seenz " + std::to_string(seen[2]) + " waiting " +
					(rotationPickWaiting ? "1" : "0");
			}

			// Track Editing's two objects of the current section as the game draws them (T-3c): the object itself (the live copy) and the start marker (the second copy), the world's units; empty off the screen
			std::string trackObjectsWords;

			if (OnTheTrackScreen() && trackEd.state != host::TrackState::Off)
			{
				int32_t at[3] = { 0, 0, 0 }, radius = 0;
				bool active = false;

				if (LevelView_FlyCopyState(at, &radius, &active))
				{
					trackObjectsWords = "end " + std::to_string(at[0]) + " " + std::to_string(at[1]) + " " + std::to_string(at[2]);
				}

				if (LevelView_GhostWorld(at))
				{
					trackObjectsWords += (trackObjectsWords.empty() ? "" : " ") + std::string("start ") + std::to_string(at[0]) + " " + std::to_string(at[1]) + " " + std::to_string(at[2]);
				}
			}

			printf("AVP_OpenInteng: scenario: screen \"%s\"; bar1 \"%s\"; bar2 \"%s\"; selection \"%s\"; prompt %d; fieldsfresh \"%d\"; status \"%s\"; selected \"%zu\"; modelbox \"%s\"; choices \"%s\"; dialog \"%s\"; dialogrows \"%s\"; dialogrow1 \"%s\"; dialogkind \"%s\"; dialogbuttons \"%s\"; dialogtext \"%s\"; dialogchecks \"%s\"; dialogfields \"%s\"; dialogcombos \"%s\"; dialogcomborows \"%s\"; popup \"%s\"; popuprow1 \"%s\"; popupstep \"%s\"; viewmode \"%s\"; camera \"%d %d %d %d %d\"; level \"%s\"; levelbytes \"%lld\"; hierarchylines \"%s\"; barshown \"%d\"; stability \"%s\"; camspeed \"%d\"; barboxes \"%s\"; dropboxes \"%s\"; droprows \"%s\"; strategyview \"%s\"; volume \"%s\"; volumeface \"%d\"; volumemodule \"%d\"; screentext \"%s\"; viewdistance \"%d\"; dropticked \"%s\"; textscreen \"%s\"; linktree \"%s\"; placedsounds \"%s\"; pathlines \"%s\"; camerasaved \"%u\"; camerasaveddrawn \"%u\"; lightspin \"%s\"; linkmessage \"%s\"; window \"%s\"; selectedat \"%s\"; flyat \"%s\"; orbit \"%s\"; flyturned \"%s\"; flymatrix \"%s\"; camangles \"%s\"; rotcentre \"%s\"; trackobjects \"%s\"%s\n", barScreen < host::kIntengScreenCount ? host::kIntengScreens[barScreen].name : "?", titles1.c_str(), titles2.c_str(),
				selectedWords.c_str(), (int)promptFor, FieldsFresh() ? 1 : 0, statusWords ? statusWords : "", host::Selection_SetCount(), modelBox.c_str(), choiceBoxes.c_str(), dialogTitle.c_str(), dialogRows.c_str(), dialogRow1.c_str(), dialogKindWord.c_str(), dialogNow.buttons.c_str(), dialogNow.text.c_str(), dialogNow.checks.c_str(), dialogNow.fields.c_str(), dialogNow.combos.c_str(), dialogNow.comborows.c_str(), popupEntries.c_str(), popupRow1.c_str(), popupStep.c_str(), scenarioViewMode, camera.x, camera.y, camera.z, camera.turn, camera.look, openLevelName.c_str(), scenarioLevelBytes, hierarchyLinesWord.c_str(), barVisible ? 1 : 0, stabilityWord.c_str(), speedUnits, barBoxesWord.c_str(), dropBoxesWord.c_str(), dropRowsWord.c_str(), strategyViewWord.c_str(), volumeWord.c_str(), volume.on ? volume.face : -1, volume.on ? volume.module : -1, screenTextNow.c_str(), (int)(viewDistance / 500000), dropTickedWord.c_str(), textScreenWord.c_str(), LinkTreeStateWords().c_str(), placedSoundsWord.c_str(), pathLinesNow.c_str(), cameraSavedCount, cameraSavedDrawn, OnALightScreen() ? lightSpinWord.c_str() : "", linkMessageShown.c_str(), windowWord.c_str(), selectedAt.c_str(), flyAtWords.c_str(), orbitWords.c_str(), flyTurnedWords.c_str(), flyMatrixWords.c_str(), camAnglesWords.c_str(), rotCentreWords.c_str(), trackObjectsWords.c_str(), StateLine_Words().c_str());
			fflush(stdout);

			// the camera, for a run that moves it (hold Z, a turn): where it is, in the game's millimetres, and the frames drawn so far
			if (theCamera)
			{
				printf("AVP_OpenInteng: scenario camera: %d %d %d turn %d look %d after %ld frames\n", theCamera->x, theCamera->y, theCamera->z, theCamera->turn, theCamera->look, frames);
				fflush(stdout);
			}

			// the live Snap To Vertex, for a run that snaps: the place and turn shown now (file units, the file's turn) and the saved ones
			if (snapLive)
			{
				printf("AVP_OpenInteng: scenario snap: now %d %d %d turn %.4f %.4f %.4f %.4f; saved %d %d %d\n", snapNowPlace[0], snapNowPlace[1], snapNowPlace[2], (double)snapNowTurn[0], (double)snapNowTurn[1],
					(double)snapNowTurn[2], (double)snapNowTurn[3], snapSavedPlace[0], snapSavedPlace[1], snapSavedPlace[2]);
				fflush(stdout);
			}

			// the flying object (F), for a run that flies one: where it is, whether its own copy is what flies (real) and how the turn keys have turned it
			if (flyOn)
			{
				int32_t copyWorld[3] = { 0, 0, 0 }, copyRadius = 0;
				bool copyActive = false;
				bool const haveCopy = LevelView_FlyCopyState(copyWorld, &copyRadius, &copyActive);

				printf("AVP_OpenInteng: scenario fly: %d %d %d real %d special %d turned %d %d %d axes %d camlink %d; copy %d at %d %d %d radius %d active %d\n", flyWorld[0], flyWorld[1], flyWorld[2], flyReal ? 1 : 0, flyIsSpecial ? 1 : 0, flyTurnAcc[0], flyTurnAcc[1], flyTurnAcc[2], flyRelAxes ? 1 : 0, cameraLinked ? 1 : 0, haveCopy ? 1 : 0, copyWorld[0], copyWorld[1], copyWorld[2], copyRadius, copyActive ? 1 : 0);
				fflush(stdout);
			}

			// where the level's view and the three views' box are, in the window's own client pixels (what a posted click is given): the panels' pixels scaled to the window
			{
				RECT inside = { 0, 0, 0, 0 };
				D3DPRESENT_PARAMETERS picture;
				IDirect3DSwapChain9 *const panelsChain = ViewSurface_PanelsChain();

				// no panels' swap chain to ask (a hidden run may have none): InThePanels then takes the window's pixels as the panels', so the scale is 1
				bool const haveChain = panelsChain && SUCCEEDED(panelsChain->GetPresentParameters(&picture)) && picture.BackBufferWidth > 0 && picture.BackBufferHeight > 0;

				if (GetClientRect(editorWindow, &inside))
				{
					int64_t const cw = inside.right - inside.left, ch = inside.bottom - inside.top;
					int64_t const pw = haveChain ? (int64_t)picture.BackBufferWidth : cw, ph = haveChain ? (int64_t)picture.BackBufferHeight : ch;
					auto wx = [&](int64_t x) { return pw > 0 ? (long long)(x * cw / pw) : 0LL; };
					auto wy = [&](int64_t y) { return ph > 0 ? (long long)(y * ch / ph) : 0LL; };

					printf("AVP_OpenInteng: scenario-geometry: view %lld %lld %lld %lld; three %lld %lld %lld %lld; threeviews %s\n", wx(layout.view.x), wy(layout.view.y), wx(layout.view.width), wy(layout.view.height),
						wx(threeBox.x), wy(threeBox.y), wx(threeBox.width), wy(threeBox.height), ThreeViewsOn() ? "on" : "off");
					fflush(stdout);
				}
			}
		}

		// The last frame of a -picture run, once the level and the panels are both shown: the window as Windows shows it, as
		// first-screen.png (screen_picture.h). The screen's composer is given a moment to take in both, and the patch it's checked against
		// is the toolbar's right end, past every menu title, where only the panels' own colour is drawn. (Added 2026-11-14 by Melanikus
		// (Dietrich), F3.) A hidden run's window is never on the screen, so there's nothing of it to take: that's said on a line of its own
		// and isn't a failure, so a hidden run's result rests only on what it can prove, the level's and the panels' pictures. A shown run
		// keeps the self-check exactly as it is. (Modified 2026-11-14 by Melanikus (Dietrich), F3, at Apone's ask.)
		if (lastFrame)
		{
			screenHidden = !IsWindowVisible(editorWindow);
		}

		if (lastFrame && !screenHidden)
		{
			MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);

			ui::Box const patch = { hitBar.x + hitBar.width - 10, hitBar.y + 4, 8, 8 };

			ScreenPicture_Save(d3d.lpD3DDevice, editorWindow, screenPicture, layout.view, patch, kPanelPassBackground, &screenReport);
		}

		if (lastFrame)
		{
			running = false;
		}
	}

	// Leaving: the camera file is written (FUN_00470610), into the editor's own folder
	if (!cameraFileName.empty())
	{
		bool const written = WriteTheCameraFile();

		if (pictureAfter)
		{
			printf("AVP_OpenInteng: camera file: %s %s\n", written ? "written" : "NOT written", cameraFileName.c_str());
			fflush(stdout);
		}
	}

	// The editor's hold of the level goes before the game's level does, so no chunk of it is let go of after the game's own teardown
	theCamera = nullptr;
	editedLevel = nullptr;
	LevelObjects_Use(nullptr);
	openLevel.reset();

	// the sounds Object Placement holds go before the game's level does (its teardown lets go of every loaded sound)
	host::SoundGame_PreviewStop();
	host::PlacedSounds_LetGo();
	host::HierarchySounds_LetGo();
	host::TrackPlay_EndSounds();
	LevelView_Close();
	ReleaseDirect3D();
	DestroyWindow(editorWindow);   // and the view's child window with it

	if (deviceBroken)
	{
		return Stop("the graphics device stopped working for good (Direct3D said so), so the editor closed; nothing was saved");
	}

	if (pictureAfter)
	{
		int const dialogs = NobodyWatching_DialogsRefused();
		printf("AVP_OpenInteng: %ld frames of %s; the last one %s; the window %s; %d dialogs refused%s%s\n", frames, levelName,
			pictureSaved ? "is first-picture.png" : "COULD NOT BE SAVED", tookTheKeyboard ? "TOOK THE KEYBOARD" : "never took the keyboard",
			dialogs, dialogs ? ", the last titled " : "", dialogs ? NobodyWatching_LastDialogTitle() : "");
		printf("AVP_OpenInteng: the last frame was drawn in a window %d by %d, with the view's box %d by %d at %d,%d and the device's back buffer "
			"%u by %u; %ld frames waited for the device\n", drawnWindowWidth, drawnWindowHeight, drawnView.width, drawnView.height, drawnView.x,
			drawnView.y, d3d.d3dpp.BackBufferWidth, d3d.d3dpp.BackBufferHeight, framesWaited);
		if (screenHidden)
		{
			printf("AVP_OpenInteng: screen: not taken, the window is hidden\n");
		}
		else
		{
			printf("AVP_OpenInteng: the screen as Windows showed it %s; the panels' own colour %s in the toolbar; in the view's box, a mean "
				"brightness of %.1f with %.1f%% of its pixels above 16\n", screenReport.saved ? "is first-screen.png" : "COULD NOT BE SAVED",
				screenReport.panelsSeen ? "is there" : "IS NOT THERE, SO THE PICTURE ISN'T THE EDITOR'S WINDOW", screenReport.viewMean,
				screenReport.viewLitShare * 100.0);
		}

		printf("AVP_OpenInteng: the panels as drawn %s\n", firstPanelsSaved ? "are first-panels.png" : "COULD NOT BE SAVED");

		for (size_t m = 0; m < barRows.menuCount[0]; m++)
		{
			printf("AVP_OpenInteng: menu %s is hit in the box %d by %d at %d,%d\n", barRows.menus[0][m].title ? barRows.menus[0][m].title : "?", drawnTitles[m].width,
				drawnTitles[m].height, drawnTitles[m].x, drawnTitles[m].y);
		}

		// every row under the first (the bars' second row, and the rows the titles wrap onto)
		for (size_t row = 1; row < barRowCount; row++)
		{
			for (size_t m = 0; m < barRows.menuCount[row]; m++)
			{
				ui::Box const title = lowerBar[row - 1].TitleBox(m);

				printf("AVP_OpenInteng: second bar: menu %s is hit in the box %d by %d at %d,%d\n", barRows.menus[row][m].title ? barRows.menus[row][m].title : "?", title.width, title.height, title.x, title.y);
			}
		}

		bool const screenGood = screenHidden || (screenReport.saved && screenReport.panelsSeen);

		return pictureSaved && screenGood && !tookTheKeyboard && dialogs == 0 ? 0 : 1;
	}
	return 0;
}
