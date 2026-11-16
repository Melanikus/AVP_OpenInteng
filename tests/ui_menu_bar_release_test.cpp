// The test of the menu bar's release (src/ui/menu_bar.h, MenuBar::Release): Windows' own menu gesture, a press on a title, a drag and a
// release on an item. Written 2026-11-14 by Melanikus (Apone) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHY: Melanikus, 2026-11-14 21:35: "clicking polyline first time didn't change anything, i had to siwtch modes and come back". Hicks's
// probe found it: a press on View, a drag down and a release on Polyline picked nothing, and the menu stayed open. This tries the
// gesture on every row of every menu, at each row's top, middle and bottom and at its left and right, and every release that must pick
// nothing, from the rules menu_bar.h writes down. Each check's words say what it proves. The program gives back a non-zero result if
// any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "menu_bar.h"

using namespace ui;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// File, View (with one item that's off), Mode (no items) and Help, numbered as the editor's table numbers them
static const MenuItem kFile[] = { { "Open level...", "Ctrl+O", 1 }, { "Save", "Ctrl+S", 2 }, { "Quit", "Alt+F4", 3 } };
static const MenuItem kView[] = { { "Polyline", "8", 101 }, { "Fully lit", 0, 102 }, { "Back faces", 0, 103 }, { "Flat shaded", "7", 106 } };
static const MenuItem kHelp[] = { { "Keys", "F1", 30 } };
static const Menu kBar[] = { { "File", kFile, 3 }, { "View", kView, 4 }, { "Mode", 0, 0 }, { "Help", kHelp, 1 } };
static size_t const kMenus = 4;
static int32_t const kItemHeight = 19;

// The bar as the editor lays it out: titles along a 32-pixel toolbar, the drop-down in the list's column under it
static void Ready(MenuBar *bar)
{
	bar->Load(kBar, kMenus);
	bar->SetEnabled(103, false);   // Back faces, off while the game can't show them
	Box const toolbar = { 0, 0, 800, 32 };
	Box const room = { 0, 32, 200, 600 };
	int32_t const titles[kMenus] = { 30, 40, 40, 40 };
	int32_t const drops[kMenus] = { 160, 160, 160, 160 };
	bar->LayOut(toolbar, room, titles, drops, kItemHeight);
}

static int32_t TitleMiddleX(const MenuBar &bar, size_t menu)
{
	Box const t = bar.TitleBox(menu);
	return t.x + t.width / 2;
}

static int32_t TitleMiddleY(const MenuBar &bar, size_t menu)
{
	Box const t = bar.TitleBox(menu);
	return t.y + t.height / 2;
}

// A row's place in the open drop-down: its top, middle or bottom pixel, at the drop-down's left, middle or right
static void RowPlace(const MenuBar &bar, size_t row, int part, int across, int32_t *x, int32_t *y)
{
	Box const drop = bar.DropBox();
	int32_t const top = drop.y + (int32_t)row * kItemHeight;
	*y = part == 0 ? top : (part == 1 ? top + kItemHeight / 2 : top + kItemHeight - 1);
	*x = across == 0 ? drop.x : (across == 1 ? drop.x + drop.width / 2 : drop.x + drop.width - 1);
}

// Press on a menu's title, so its menu opens, ready for a release
static bool PressTitle(MenuBar *bar, size_t menu)
{
	MenuAnswer const answer = bar->Press(TitleMiddleX(*bar, menu), TitleMiddleY(*bar, menu));
	return (answer == MenuAnswer::Opened || answer == MenuAnswer::Moved) && bar->IsOpen() && bar->OpenMenu() == (int)menu;
}

static void TestTheGestureOnEveryRow()
{
	puts("Press on a title, drag, release on a row:");

	size_t tried = 0, picked = 0;
	std::string first;
	for (size_t m = 0; m < kMenus; m++)
	{
		for (size_t row = 0; row < kBar[m].count; row++)
		{
			bool const on = kBar[m].items[row].action != 103;
			for (int part = 0; part < 3; part++)
			{
				for (int across = 0; across < 3; across++)
				{
					MenuBar bar;
					Ready(&bar);
					if (!PressTitle(&bar, m))
					{
						if (first.empty()) first = std::string("the press on ") + kBar[m].title + " didn't open it";
						continue;
					}
					int32_t x, y;
					RowPlace(bar, row, part, across, &x, &y);
					MenuAnswer const answer = bar.Release(x, y);
					tried++;
					bool const good = on ? (answer == MenuAnswer::Picked && bar.Picked() == kBar[m].items[row].action && !bar.IsOpen())
						: (answer == MenuAnswer::Refused && bar.Refused() == 103 && bar.Picked() == 0 && bar.IsOpen());
					if (good && on) picked++;
					if (!good && first.empty())
					{
						first = std::string(kBar[m].title) + " > " + kBar[m].items[row].name + " at part " + std::to_string(part) + ", across " + std::to_string(across);
					}
				}
			}
		}
	}

	Check(first.empty() && tried == 9 * 8, "a release on each of the 8 rows, at its top, middle and bottom and its left, middle and right (72 releases): "
		"each row that's on picks exactly its own action and closes the menu, and Back faces, which is off, picks nothing, leaves it open and says it was Back faces"
		+ (first.empty() ? std::string() : " (the first that didn't: " + first + ")"));
	Check(picked == 9 * 7, "and all 63 releases on the 7 rows that are on picked");
}

static void TestReleasesThatPickNothing()
{
	puts("Releases that pick nothing:");

	MenuBar bar;
	int32_t x = 0, y = 0;

	Ready(&bar);
	PressTitle(&bar, 1);
	Check(bar.Release(TitleMiddleX(bar, 1), TitleMiddleY(bar, 1)) == MenuAnswer::Nothing && bar.IsOpen() && bar.OpenMenu() == 1 && bar.Picked() == 0,
		"a press and a release on the same title (a plain click) opens the menu and leaves it open, with nothing picked");
	RowPlace(bar, 0, 1, 1, &x, &y);
	Check(bar.Release(x, y) == MenuAnswer::Nothing && bar.IsOpen() && bar.Picked() == 0, "a second release, with no press between, picks nothing: one release ends one press");
	Check(bar.Press(x, y) == MenuAnswer::Picked && bar.Picked() == 101 && !bar.IsOpen(), "and a plain click on Polyline after that still picks it, on the press, as before");

	Ready(&bar);
	PressTitle(&bar, 1);
	Check(bar.Release(TitleMiddleX(bar, 0), TitleMiddleY(bar, 0)) == MenuAnswer::Nothing && bar.IsOpen() && bar.OpenMenu() == 1,
		"a release on another menu's title picks nothing, and View stays open");

	Ready(&bar);
	PressTitle(&bar, 1);
	Check(bar.Release(500, 400) == MenuAnswer::Nothing && bar.IsOpen() && bar.Picked() == 0, "a release out in the level's view picks nothing, and the menu stays open");

	Ready(&bar);
	PressTitle(&bar, 1);
	Box const drop = bar.DropBox();
	Check(bar.Release(drop.x + 10, drop.y + drop.height) == MenuAnswer::Nothing && bar.IsOpen(), "a release just below the last row picks nothing");
	Ready(&bar);
	PressTitle(&bar, 1);
	Check(bar.Release(drop.x + drop.width, drop.y + 5) == MenuAnswer::Nothing && bar.IsOpen(), "and so does one just right of the drop-down");

	Ready(&bar);
	PressTitle(&bar, 2);
	Check(bar.Release(TitleMiddleX(bar, 2), 40) == MenuAnswer::Nothing && bar.Picked() == 0, "Mode, with no items, picks nothing on a release under it");

	Ready(&bar);
	PressTitle(&bar, 1);
	Check(bar.Release(INT32_MIN, INT32_MAX) == MenuAnswer::Nothing && bar.Picked() == 0, "a release at the ends of the numbers picks nothing, and nothing wraps");
}

static void TestWhatEndsTheGesture()
{
	puts("What ends the gesture:");

	MenuBar bar;
	int32_t x = 0, y = 0;

	Ready(&bar);
	bar.Handle(KeyEvent(Key::Menu));
	RowPlace(bar, 0, 1, 1, &x, &y);
	Check(bar.IsOpen() && bar.Release(x, y) == MenuAnswer::Nothing && bar.IsOpen(), "a menu opened by its key picks nothing on a release: there was no press");

	Ready(&bar);
	PressTitle(&bar, 1);
	bar.Handle(KeyEvent(Key::Down));
	RowPlace(bar, 0, 1, 1, &x, &y);
	Check(bar.Release(x, y) == MenuAnswer::Nothing && bar.IsOpen(), "a key between the press and the release ends the gesture: the release picks nothing");

	Ready(&bar);
	PressTitle(&bar, 1);
	bar.Handle(KeyEvent(Key::Escape));
	RowPlace(bar, 0, 1, 1, &x, &y);
	Check(!bar.IsOpen() && bar.Release(x, y) == MenuAnswer::Nothing && !bar.IsOpen(), "closed with Escape, a release picks nothing and opens nothing");

	Ready(&bar);
	PressTitle(&bar, 1);
	Ready(&bar);   // a new bar is loaded while the press is held
	RowPlace(bar, 0, 1, 1, &x, &y);
	Check(bar.Release(x, y) == MenuAnswer::Nothing, "loading the bar again ends the gesture");

	Ready(&bar);
	PressTitle(&bar, 1);
	bar.Release(TitleMiddleX(bar, 1), TitleMiddleY(bar, 1));   // the plain click that opens it
	PressTitle(&bar, 0);                                      // a press on File's title moves to File
	RowPlace(bar, 1, 1, 1, &x, &y);
	Check(bar.Release(x, y) == MenuAnswer::Picked && bar.Picked() == 2, "a press on another title moves to that menu, and the release on its row picks it (File > Save)");

	Ready(&bar);
	PressTitle(&bar, 1);
	bar.Release(TitleMiddleX(bar, 1), TitleMiddleY(bar, 1));
	bar.Press(TitleMiddleX(bar, 1), TitleMiddleY(bar, 1));   // a second press on View's own title closes it
	RowPlace(bar, 0, 1, 1, &x, &y);
	Check(!bar.IsOpen() && bar.Release(x, y) == MenuAnswer::Nothing, "a press on the open menu's own title closes it, and the release after picks nothing");
}

static void TestNoRowsNoDivision()
{
	puts("A bar with no room for rows:");

	MenuBar bar;
	bar.Load(kBar, kMenus);
	Box const toolbar = { 0, 0, 800, 32 };
	Box const room = { 0, 32, 200, 600 };
	int32_t const titles[kMenus] = { 30, 40, 40, 40 };
	int32_t const drops[kMenus] = { 160, 160, 160, 160 };
	bar.LayOut(toolbar, room, titles, drops, 0);
	PressTitle(&bar, 1);
	Check(bar.Release(20, 40) == MenuAnswer::Nothing && bar.Picked() == 0, "with rows of no height, a release picks nothing (and divides by nothing)");
	bar.LayOut(toolbar, room, titles, drops, -19);
	PressTitle(&bar, 1);
	Check(bar.Release(20, 40) == MenuAnswer::Nothing, "and so with rows of a height below 0");

	MenuBar empty;
	Check(empty.Release(10, 10) == MenuAnswer::Nothing && empty.Picked() == 0, "a bar with no menus loaded picks nothing on a release");
}

int main()
{
	TestTheGestureOnEveryRow();
	TestReleasesThatPickNothing();
	TestWhatEndsTheGesture();
	TestNoRowsNoDivision();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
