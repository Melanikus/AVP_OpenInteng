// editor_bar.h: the menu bar of each of the old editor's screens, and what a pick on it does. A screen's bar is inteng's own menus for it
// (inteng_menus.h), each item with its tag, then the editor's own menu, "View (ours)", and on the hub also "Screens (ours)", which opens
// any screen. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A6, the all-menus build). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// Window-free and tested: the editor's loop makes the bar of the screen in use, loads it into its menu bar, and hands each pick here.
// - An item inteng's screen has does what its work says (Quit, go to a screen, back to the hub, the keys). A tagged item does nothing but
//   say why, so every item can be chosen, and every one gives Melanikus "clear feedback". (His words: "add all menu items and [wip]
//   besides them".)
// - The editor's own items are its table's (editor_actions.h), but for two that inteng's own items now do: Quit (2), which construction's
//   File, Quit does, and Help, Keys (301), which construction's Keys - Movement and placement's Keys do. They're left off the bar.
// - "Back to Construction" is ours, on every screen but the hub. Some of inteng's screens have no Exit of their own.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "editor_actions.h"
#include "inteng_menus.h"
#include "menu_bar.h"
#include "panel_paint.h"

// The most items one bar holds. The hub has the most: its 32, the editor's own 13 and Screens' 22, 67 in all.
size_t const kEditorBarMostItems = 128;

// The editor's own items this file adds: "Back to Construction", and the Screens menu's, one for each screen in kIntengScreens' order
int32_t const kActionBackToHub = 303;
int32_t const kActionScreenFirst = 0x00F00000;

// Where a bar is made: its menus, its items and the keys' words beside them. All of it has to stay where it is while the menu bar uses it.
struct EditorBarRoom
{
	ui::Menu menus[ui::kMenuMostMenus];
	ui::MenuItem items[kEditorBarMostItems];
	EditorKeyWords words[kEditorBarMostItems];
	size_t menuCount;
	ui::MenuItem subItems[kEditorBarMostItems];   // the cascades' rows (inteng's kind-2 records, reassess/dialog-look-drake.md BAR-10): each cascade item's subItems point in here
};

// THE RIGHT-CLICK POP-UP (inteng's second menu table of a screen, opened by the RIGHT mouse button at the pointer; Vasquez's read, board 03:04,
// FUN_004b7c50 and the menu object's method +0xc). Module Linking's table at 0x5e2e38 has one, 0x5e2c88: its eight link commands. Which other screens
// have one is not read yet, so only the ones read are in the list in editor_bar.cpp.
size_t const kEditorNoPopup = (size_t)-1;

// The place in kIntengScreens of the table that is screen's pop-up, or kEditorNoPopup for a screen that has none
size_t EditorBar_PopupScreen(size_t screen);

// WHAT THE SECOND TABLE IS (Vasquez, 2026-11-15, FUN_004b7c50, FUN_004b8aa0, FUN_004b8b90, FUN_004b8350): inteng's menu object keeps, for each page, TWO tables: the bar's (method +0x10 stores it at +0x4c, the
// bar shows it) and a second one (method +0x14 stores it at +0x50, which FUN_004b8c80 makes the object's +0x28 when the page is chosen). The per-frame method FUN_004b7c50 shows the bar on the left button at
// the top of the window and, on the RIGHT button, the second table through method +0xc (FUN_004b8350: a pop-up at the pointer that takes the mouse until a pick: each level lists the table's entries, an entry with
// items under it opens them to the side). So every table a screen registers with +0x14 is its RIGHT-CLICK POP-UP, not a second bar: Module Linking (0x5e2c88), Hierarchy (FUN_00485580 0x488723), Waypoint (FUN_00581650),
// Path, Strategy Data, Texture Map and the Track Editing pages' second tables (the code-built ones are IntengScreen::secondRow of the page shown). This editor still draws them as a second bar too (Apone's
// row 2); the pop-up opens the same items at the pointer: the top level lists the table's entries, an entry with several items opens them as a second pop-up in the same place.
int32_t const kEditorPopupGroupBase = 0x7e000000;   // a picked action from this number is "open the group m = action - base", not a command

// Makes screen's pop-up in *room: group < 0 the top level, group >= 0 the items of the table's entry number group (one menu holding them, each with its tag's words). Gives back false, making nothing that counts,
// for no room, a screen with no pop-up, a group it does not have, or a table that wouldn't fit.
bool EditorBar_MakePopupLevel(size_t screen, int32_t group, EditorBarRoom *room);

// Makes screen's pop-up in *room: one menu holding every item of its pop-up table, each with its tag's words. Gives back false, making nothing
// that counts, for no room, a screen with no pop-up, or a table that wouldn't fit.
bool EditorBar_MakePopup(size_t screen, EditorBarRoom *room);

// Makes the screen's bar in *room, with the keys' words of layout. Gives back false, making nothing that counts, for no room, a screen
// that isn't one of kIntengScreens, or a bar that wouldn't fit the menu bar. The test makes every screen's, so the editor's own never
// fails.
bool EditorBar_Make(size_t screen, KeyLayout layout, EditorBarRoom *room);

// The screen's SECOND bar, under the first (inteng's code-built screens show two rows of menus: IntengScreen::secondRow, page 0), made in *room with the same item words and tags and none of
// the editor's own items. Gives back true with room->menuCount 0 for a screen that has no second row, and false (making nothing that counts) for no room, a screen that isn't one of
// kIntengScreens, or a row that wouldn't fit the menu bar.
bool EditorBar_MakeSecond(size_t screen, EditorBarRoom *room);

// WHERE THE BARS GO. With a second bar the toolbar is two bars tall: EditorBar_Sizes doubles the toolbar's height in the panel sizes, and EditorBar_Place splits the laid-out toolbar into the first bar
// (on top) and the second (under it), each bar's drop-down room starting at the bar's own foot and reaching the foot of the window's view. The editor and the click tests both use these, so what the
// tests click is what the editor draws.
struct EditorBarPlace
{
	ui::Box bar1;
	ui::Box bar2;        // empty without a second bar
	ui::Box dropRoom1;
	ui::Box dropRoom2;
};

ui::PanelSizes EditorBar_Sizes(ui::PanelSizes sizes, bool second);
EditorBarPlace EditorBar_Place(const ui::PanelLayout &layout, bool second);
// The page the code-built screens show on both bars (inteng's menu method +0x18): 0 is the page a screen starts with; a page a row does not have shows that row empty. Both bars made after this are on it.
void EditorBar_SetPage(int32_t page);
int32_t EditorBar_Page();
// The right-click pop-up's page alone (inteng's menu method +0x1c): on Object Placement a page other than 0 is that page of FUN_004d68e0's built pages (6: the current placed hierarchy's, PH-1)
void EditorBar_SetPopupPage(int32_t page);
int32_t EditorBar_PopupPage();

// TITLES THAT WRAP (Bishop 11-15: "when a row's titles don't fit, WRAP onto another row under it, never off the edge"). A screen's bars are two LOGICAL rows (inteng's row 1 with the editor's own menus, and
// its row 2); when their titles are wider than the window, each logical row is cut into PHYSICAL rows, a menu never split and a logical row always starting a new physical row, so every title is in the window and
// can be clicked at every window width the editor allows. kEditorBarMostRows physical rows at most; EditorBar_LeastWidth is the window width the widest page of every screen needs for that.
size_t const kEditorBarMostRows = 4;
int32_t const kEditorBarEdge = 20;   // room kept past the last title of a row: a title fits when the titles before it, its own, and this are within the width

struct EditorBarRows
{
	EditorBarRoom logical[2];                                       // the bars as EditorBar_Make and EditorBar_MakeSecond make them (the menus' items stay in these)
	ui::Menu menus[kEditorBarMostRows][ui::kMenuMostMenus];         // each physical row's menus: copies of logical menus, their items still those of the logical rooms
	size_t menuCount[kEditorBarMostRows];
	size_t rowCount;                                                // 1 to kEditorBarMostRows
};

// Makes the screen's bars on the page now shown (EditorBar_SetPage), wrapped to width. False, with nothing that counts made, when a bar can't be made or more than kEditorBarMostRows rows would be needed.
bool EditorBar_MakeRows(size_t screen, KeyLayout layout, int32_t width, host::PaintLetterWidth widthOf, EditorBarRows *rows);

// The toolbar's height for that many physical rows, and where each row and its drop-down room go (the drop-down room of a row starts at the row's own foot)
ui::PanelSizes EditorBar_SizesRows(ui::PanelSizes sizes, size_t rows);

struct EditorBarPlaceRows
{
	ui::Box bar[kEditorBarMostRows];        // a row there is not is an empty box
	ui::Box dropRoom[kEditorBarMostRows];
};

EditorBarPlaceRows EditorBar_PlaceRows(const ui::PanelLayout &layout, size_t rows);

// The least window width at which every page of every screen wraps into kEditorBarMostRows physical rows or fewer, and no title is wider than the window. The editor keeps its window at least this wide.
int32_t EditorBar_LeastWidth(KeyLayout layout, host::PaintLetterWidth widthOf);

// Does what the item numbered action does, through *host, as the editor's actions do: host->screen is the screen in use, changed when the
// item goes to another. Gives back false, changing nothing, for no host or a number no item of any bar has.
bool EditorBar_Run(int32_t action, EditorHostState *host);

// How wide the widest bar's titles are together, as widthOf measures them (MenuPaint_Measure): the narrowest the toolbar can be with no
// title past its edge. The editor keeps its window at least this wide.
int32_t EditorBar_WidestTitles(KeyLayout layout, host::PaintLetterWidth widthOf);

// The titles bar has laid out past the toolbar's edge (TitleBox of no width), named in words for the status line: "Menus past the
// window's edge: <title>, <title>", at most size - 1 letters, always ended by a 0. Nothing is cut: an empty text.
void EditorBar_CutOff(const ui::MenuBar &bar, const ui::Menu *menus, size_t count, char *words, size_t size);
