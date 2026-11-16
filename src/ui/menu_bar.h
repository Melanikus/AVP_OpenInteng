// The menu bar's model: a row of menus along the top of the editor (File, View, Mode, Help), each dropping down a list of items, by the
// keyboard and the mouse. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It's the editor's own widget (R-INT-6: no Win32 menus), drawn by the panels with the game's menu text; this is its model, window-free,
// tested like the rest of the kit. Its menus and items are the editor's own table of actions (src/host/editor_actions.h), never anything
// from a level.
//
// THE RULES.
// - The menus are checked as a whole when they're loaded, and refused as a whole if anything is wrong: 1 to kMenuMostMenus menus, a
//   title of 1 to kMenuNameMost font letters, 0 to kMenuMostItems items each, an item's name the same, its keys (shown beside it, such as
//   "Ctrl+O") none or 1 to kMenuKeysMost font letters, and each item's action a number above 0, used once in the whole bar.
// - Closed, only Key::Menu means anything (F10, or Alt pressed and let go alone): it opens the first menu. Every other event is left for
//   the widget with focus.
// - Open, it takes every key, and only it: Left and Right go to the menu before or after, round from the end to the start; Up and Down
//   light the item before or after, passing over the ones that are off, round too; Home and End the first and the last that's on;
//   Enter or Space picks the lit item if it's on; Esc or Key::Menu closes it. It never touches any other widget, so an edit in the field
//   with focus is still there, just as it was, when the menu closes.
// - The mouse: a press on a title opens that menu, or closes it if it's open; on an item that's on, picks it; on an item that's off, picks
//   nothing, leaves the menu open and says which item it was (Refused), so the editor can say why it's off; anywhere else, while a menu
//   is open, closes it. Its places come from LayOut, which the view gives the sizes it measured.
// - And the gesture Windows' own menus take: a press on a title, a drag down, and the button let go on an item that's on picks that
//   item (Release). Let go anywhere else, the menu stays open and nothing is picked, as after a click on its title. (Added 2026-11-14 by
//   Melanikus (Dietrich): with only presses, Melanikus's press on View, drag and release on Polyline did nothing.)
// - A drop-down goes under its title, moved left if need be to stay inside its room (the list panel's column, so it never reaches over
//   the level's view, which the panels can't draw over), and it's cut to the room: when the room is smaller than it, it shows as many
//   whole items as fit, and scrolls so the lit one is always among them. A room of no size shows nothing, and no press lands in it. Every
//   sum is done in 64 bits, so no place or size can wrap round.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "panel_layout.h"
#include "ui_event.h"

namespace ui
{
	// The most menus on one bar, and the most letters in a title or an item's name. inteng's biggest bars set them: module linking's
	// tables have 9 and 8 menus, with the editor's own after them, and its longest name with the tag the editor puts after an item that
	// isn't built yet is 47 letters ("Recentre Shape: Move centre to bottom [Blender]"). (Modified 2026-11-15 by Melanikus (Frost), A6:
	// they were 8 and 40.)
	size_t const kMenuMostMenus = 16;   // was 12: Waypoint's second row, page 3, has 14 titles (Modified 2026-11-15 by Melanikus (Apone), the code-built menus)
	size_t const kMenuMostItems = 96;   // was 32: Texture Map's longest drop-down has 61 items (the drop-down scrolls)
	size_t const kMenuNameMost = 64;   // was 48: "Repeating Texture Options: Select Number of Repeats" is 51 letters and the tag after it 6
	size_t const kMenuKeysMost = 24;
	size_t const kMenuTagMost = 24;   // the longest tag after an item's name, " [unfinished in inteng]" being 23 (Added 2026-11-15, A6; raised from 12 for it)

	struct MenuItem
	{
		const char *name;     // the item's words: 1 to kMenuNameMost font letters
		const char *keys;     // the keys that do it too, shown beside it, or null for none
		int32_t action;       // what it does, as the editor's table numbers it: above 0, each once in the whole bar
		const char *tag = nullptr;   // words shown right after the name and never cut, such as " [wip]" for an item that isn't built
		                             // yet, or null for none: 1 to kMenuTagMost font letters. Null unless set, so an item made field by field
		                             // (ui_menu_bar_test's TestSmallRooms makes ten on the stack) has none. (Added 2026-11-15 by Melanikus
		                             // (Frost), A6.)
		int32_t access = -1;         // the place in name of the letter drawn underlined (inteng's '&': FUN_0046ce70 0x46ceac, view-state-table.md H-5c), -1 for none
		bool subMenu = false;        // the item opens a menu of its own: its row shows inteng's sub-menu mark (glyph 2 of the menu font, FUN_004b6f70 0x4b71c5; H-5b)
	};

	struct Menu
	{
		const char *title;    // the menu's words on the bar
		const MenuItem *items;
		size_t count;         // 0 to kMenuMostItems: a menu with none shows an empty drop-down, and picks nothing
		int32_t titleAccess = -1;   // the place in title of the letter drawn underlined, -1 for none
	};

	enum class MenuAnswer
	{
		Nothing,   // the event means nothing to the bar (while it's closed, it's for the widget with focus)
		Opened,    // a menu opened
		Moved,     // another menu, or another item, is lit
		Closed,    // the menu closed, with nothing picked
		Picked,    // an item was picked, and the menu closed: Picked() says which action
		Refused    // a press, or the release of a drag, was on an item that's off: nothing is picked, the menu stays open, and Refused() says
		           // which action, so the editor can say why it's off
	};

	class MenuBar
	{
	public:
		MenuBar();

		// Loads the bar's menus, as the rules say. Gives back false, and leaves the bar with no menus, which then means nothing to any
		// event, for any list the rules refuse. The menus have to stay where they are while the bar is used: they're the editor's table.
		bool Load(const Menu *menus, size_t count);

		// Every item that does this action is checked (its state is on: the draw mode in use, a switch that is on) or not. A checked item is shown
		// highlighted. It changes nothing else: a checked item can be picked as any other. (Added 2026-11-15 by Melanikus (Dietrich).)
		void SetChecked(int32_t action, bool checked);
		bool IsChecked(size_t menu, size_t item) const;

		// Every item that does this action is on or off. An item that's off is shown, but can't be lit or picked.
		void SetEnabled(int32_t action, bool enabled);

		MenuAnswer Handle(const Event &event);

		// Where the bar and its drop-down go: the bar's own box, the room a drop-down may use, each menu's title width and drop-down
		// width as the view measured them (count of each: as many as the menus loaded), and one item's height. Sizes below 0 count as 0.
		void LayOut(const Box &bar, const Box &room, const int32_t *titleWidths, const int32_t *dropWidths, int32_t itemHeight);

		// A press of the mouse's button at a place in the window
		MenuAnswer Press(int32_t x, int32_t y);

		// The mouse's button let go at a place in the window. Only the release that ends the press which opened the open menu, on its
		// title, counts, and only once:
		// - over an item of that menu that's on, it picks that item, and the menu closes: Picked;
		// - over an item that's off, over the menu's own title or another's, or anywhere else, it picks nothing, and the menu stays open,
		//   as after a click on its title: Nothing.
		// Every other release means Nothing: with no menu open, a menu opened by a key, a press since that wasn't on a title, a key since
		// the press, or a release already counted. So a plain click on a title still opens its menu and leaves it open, and a click on an
		// item still picks it on the press.
		MenuAnswer Release(int32_t x, int32_t y);

		// The pointer moved to a place in the window: with a menu open, the shown row under it becomes the lit (pointed) one, as inteng's menu loop does with
		// the row under the mouse (FUN_004b7dd0 -> FUN_004b7590, view-state-table.md H-5d). Anywhere else the lit row stays. True when the lit row changed.
		bool PointAt(int32_t x, int32_t y);

		bool Loaded() const { return menus != 0; }
		bool IsOpen() const { return open >= 0; }

		// The open menu, and its lit item, or -1 for none
		int OpenMenu() const { return open; }
		int LitItem() const { return lit; }

		// The action the last Picked answer picked, or 0
		int32_t Picked() const { return picked; }

		// The action of the item that's off the last Refused answer was about, or 0
		int32_t Refused() const { return refused; }

		bool IsEnabled(size_t menu, size_t item) const;

		// For the view: a title's box, the open drop-down's box (empty when none is open or the room has no size), and which of its items
		// show in it: ShownItems of them, from FirstShownItem
		Box TitleBox(size_t menu) const;
		Box DropBox() const;
		size_t FirstShownItem() const { return first; }
		int32_t ItemHeight() const { return itemHeight; }   // one drop-down row's height, for the acceptance harness's click-bar
		size_t ShownItems() const;

	private:
		void OpenMenuAt(int menu);
		MenuAnswer Close();
		int NextEnabled(int from, int step) const;
		void KeepLitShown();

		const Menu *menus;   // null when there are none
		size_t count;
		bool enabled[kMenuMostMenus][kMenuMostItems];
		bool checked[kMenuMostMenus][kMenuMostItems];
		int open;
		int lit;
		size_t first;
		int32_t picked;
		int32_t refused;
		bool openedByPress;   // the open menu was opened by the last press, on its title, and nothing has ended that press yet

		Box bar;
		Box room;
		int32_t titleWidths[kMenuMostMenus];
		int32_t dropWidths[kMenuMostMenus];
		int32_t itemHeight;
	};
}
