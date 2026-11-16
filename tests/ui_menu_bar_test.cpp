// The test for src/ui's menu bar. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

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

static const MenuItem kFile[] = { { "Open level...", "Ctrl+O", 1 }, { "Save", "Ctrl+S", 2 }, { "Quit", "Alt+F4", 3 } };
static const MenuItem kView[] = { { "Wireframe", 0, 10 }, { "Lights", 0, 11 } };
static const MenuItem kHelp[] = { { "Keys", "F1", 30 }, { "About", 0, 31 } };

// File, View, Mode (no items yet) and Help, as the editor's table makes them
static const Menu kBar[] = { { "File", kFile, 3 }, { "View", kView, 2 }, { "Mode", 0, 0 }, { "Help", kHelp, 2 } };

static bool Loads(const Menu *menus, size_t count)
{
	MenuBar bar;

	return bar.Load(menus, count) && bar.Loaded();
}

// One more menu than the bar takes, each with items of its own, so only the count can refuse the last one: the action numbers are all
// different (a shared list would be refused for its numbers first, and a missing count check would never be seen)
static MenuItem many[kMenuMostMenus + 1][kMenuMostItems + 1];
static Menu manyMenus[kMenuMostMenus + 1];

static void TestLoading()
{
	puts("Loading:");

	Check(Loads(kBar, 4), "File, View, Mode with no items, and Help load");
	Check(!Loads(0, 0) && !Loads(kBar, 0) && !Loads(0, 4), "no menus, or menus counted with no list, are refused");

	for (size_t m = 0; m < kMenuMostMenus + 1; m++)
	{
		for (size_t i = 0; i < kMenuMostItems + 1; i++)
		{
			many[m][i].name = "an item";
			many[m][i].keys = 0;
			many[m][i].action = (int32_t)(1 + m * 100 + i);
		}

		manyMenus[m].title = "Menu";
		manyMenus[m].items = many[m];
		manyMenus[m].count = kMenuMostItems;
	}

	Check(Loads(manyMenus, kMenuMostMenus), "12 menus of 32 items each, the most, load");
	Check(!Loads(manyMenus, kMenuMostMenus + 1), "13 menus are refused");
	manyMenus[0].count = kMenuMostItems + 1;
	Check(!Loads(manyMenus, 1), "and a menu of 33 items");
	manyMenus[0].count = kMenuMostItems;

	Menu menu = { "File", kFile, 3 };
	menu.title = 0;
	Check(!Loads(&menu, 1), "a menu with no title is refused");
	menu.title = "";
	Check(!Loads(&menu, 1), "and one with an empty title");
	std::string const longest(kMenuNameMost, 't');
	std::string const tooLong(kMenuNameMost + 1, 't');
	menu.title = longest.c_str();
	Check(Loads(&menu, 1), "a title of 48 letters loads");
	menu.title = tooLong.c_str();
	Check(!Loads(&menu, 1), "and one of 49 is refused");
	menu.title = "Fi\tle";
	Check(!Loads(&menu, 1), "and one with a letter the font doesn't have");

	MenuItem tagged[] = { { "Open", 0, 1, " [wip]" } };
	Menu taggedMenu = { "File", tagged, 1 };
	std::string const longestTag(kMenuTagMost, 't');
	std::string const tooLongTag(kMenuTagMost + 1, 't');

	Check(Loads(&taggedMenu, 1), "an item with a tag loads");
	tagged[0].tag = longestTag.c_str();
	Check(Loads(&taggedMenu, 1), "a tag of 12 letters loads");
	tagged[0].tag = tooLongTag.c_str();
	Check(!Loads(&taggedMenu, 1), "and one of 13 is refused");
	tagged[0].tag = "[w\x01p]";
	Check(!Loads(&taggedMenu, 1), "and one with a letter the font doesn't have");
	tagged[0].tag = "";
	Check(!Loads(&taggedMenu, 1), "and an empty one: no tag is null, never empty");
	menu.title = "File";
	menu.items = 0;
	Check(!Loads(&menu, 1), "items counted with no list are refused");

	MenuItem item = { "Open", 0, 1 };
	Menu one = { "File", &item, 1 };
	Check(Loads(&one, 1), "an item with no keys loads");
	item.name = 0;
	Check(!Loads(&one, 1), "an item with no name is refused");
	item.name = tooLong.c_str();
	Check(!Loads(&one, 1), "and one with a name too long");
	item.name = "Open";
	item.keys = "";
	Check(!Loads(&one, 1), "an item's keys can't be empty: no keys is null");
	std::string const keysTooLong(kMenuKeysMost + 1, 'k');
	item.keys = keysTooLong.c_str();
	Check(!Loads(&one, 1), "and they can't be longer than 24 letters");
	item.keys = "Ctrl+\x81";
	Check(!Loads(&one, 1), "or hold a byte the font doesn't have");
	item.keys = 0;
	item.action = 0;
	Check(!Loads(&one, 1), "an action of 0 is refused");
	item.action = -1;
	Check(!Loads(&one, 1), "and one below 0");

	MenuItem const twiceInOne[] = { { "A", 0, 5 }, { "B", 0, 5 } };
	Menu const sameMenu = { "File", twiceInOne, 2 };
	Check(!Loads(&sameMenu, 1), "the same action twice in one menu is refused");
	MenuItem const first[] = { { "A", 0, 5 } };
	MenuItem const second[] = { { "B", 0, 6 }, { "C", 0, 5 } };
	Menu const twiceInTwo[] = { { "File", first, 1 }, { "View", second, 2 } };
	Check(!Loads(twiceInTwo, 2), "and the same action in two menus");

	MenuBar bar;
	bar.Load(kBar, 4);
	Check(!bar.Load(&sameMenu, 1) && !bar.Loaded() && bar.Handle(KeyEvent(Key::Menu)) == MenuAnswer::Nothing, "a refused load leaves no menus, so the bar means nothing");
}

static void TestKeys()
{
	puts("The keyboard:");

	MenuBar bar;
	bar.Load(kBar, 4);

	Check(!bar.IsOpen() && bar.Handle(LetterEvent('a')) == MenuAnswer::Nothing && bar.Handle(KeyEvent(Key::Down)) == MenuAnswer::Nothing,
		"closed, every key but the menu key is left for the widget with focus");
	Check(bar.Handle(KeyEvent(Key::Menu)) == MenuAnswer::Opened && bar.IsOpen() && bar.OpenMenu() == 0 && bar.LitItem() == 0, "the menu key opens the first menu, its first item lit");
	Check(bar.Handle(KeyEvent(Key::Down)) == MenuAnswer::Moved && bar.LitItem() == 1, "Down lights the next item");
	bar.Handle(KeyEvent(Key::Down));
	Check(bar.Handle(KeyEvent(Key::Down)) == MenuAnswer::Moved && bar.LitItem() == 0, "and goes round from the last to the first");
	Check(bar.Handle(KeyEvent(Key::Up)) == MenuAnswer::Moved && bar.LitItem() == 2, "Up goes round the other way");
	Check(bar.Handle(KeyEvent(Key::Home)) == MenuAnswer::Moved && bar.LitItem() == 0 && bar.Handle(KeyEvent(Key::End)) == MenuAnswer::Moved && bar.LitItem() == 2,
		"Home and End light the first and the last");
	Check(bar.Handle(KeyEvent(Key::End)) == MenuAnswer::Nothing, "and End on the last changes nothing");
	Check(bar.Handle(KeyEvent(Key::Right)) == MenuAnswer::Moved && bar.OpenMenu() == 1 && bar.LitItem() == 0, "Right goes to the next menu, its first item lit");
	Check(bar.Handle(KeyEvent(Key::Left)) == MenuAnswer::Moved && bar.Handle(KeyEvent(Key::Left)) == MenuAnswer::Moved && bar.OpenMenu() == 3,
		"Left goes back, and round from the first menu to the last");
	Check(bar.Handle(KeyEvent(Key::Tab)) == MenuAnswer::Nothing && bar.IsOpen() && bar.Handle(LetterEvent('x')) == MenuAnswer::Nothing && bar.IsOpen(),
		"open, it takes every key, and the ones it has no use for change nothing");

	bar.Handle(KeyEvent(Key::Down));
	Check(bar.Handle(KeyEvent(Key::Enter)) == MenuAnswer::Picked && bar.Picked() == 31 && !bar.IsOpen(), "Enter picks the lit item, About, and closes the menu");
	Check(bar.Handle(LetterEvent('a')) == MenuAnswer::Nothing && bar.Picked() == 0, "and the next event picks nothing");

	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.Handle(KeyEvent(Key::Space)) == MenuAnswer::Picked && bar.Picked() == 1, "Space picks too");
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.Handle(LetterEvent(' ')) == MenuAnswer::Picked && bar.Picked() == 1, "and so does a typed space");
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.Handle(LetterEvent(' ', true)) == MenuAnswer::Nothing && bar.IsOpen(), "but not Ctrl and a space");
	Check(bar.Handle(KeyEvent(Key::Escape)) == MenuAnswer::Closed && !bar.IsOpen() && bar.Picked() == 0, "Esc closes it with nothing picked");
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.Handle(KeyEvent(Key::Menu)) == MenuAnswer::Closed && !bar.IsOpen(), "and so does the menu key");

	// The Mode menu has no items yet
	bar.Handle(KeyEvent(Key::Menu));
	bar.Handle(KeyEvent(Key::Right));
	bar.Handle(KeyEvent(Key::Right));
	Check(bar.OpenMenu() == 2 && bar.LitItem() == -1 && bar.Handle(KeyEvent(Key::Down)) == MenuAnswer::Nothing && bar.Handle(KeyEvent(Key::Enter)) == MenuAnswer::Nothing
		&& bar.IsOpen(), "a menu with no items lights none and picks nothing");
	bar.Handle(KeyEvent(Key::Escape));

	MenuBar single;
	Menu const onlyFile = { "File", kFile, 3 };
	single.Load(&onlyFile, 1);
	single.Handle(KeyEvent(Key::Menu));
	Check(single.Handle(KeyEvent(Key::Left)) == MenuAnswer::Nothing && single.Handle(KeyEvent(Key::Right)) == MenuAnswer::Nothing && single.OpenMenu() == 0,
		"with one menu, Left and Right have nowhere to go");
}

static void TestOff()
{
	puts("Items that are off:");

	MenuBar bar;
	bar.Load(kBar, 4);
	bar.SetEnabled(2, false);

	Check(bar.IsEnabled(0, 0) && !bar.IsEnabled(0, 1) && bar.IsEnabled(0, 2) && !bar.IsEnabled(9, 0) && !bar.IsEnabled(0, 9), "Save is off, and the rest of File on");
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.Handle(KeyEvent(Key::Down)) == MenuAnswer::Moved && bar.LitItem() == 2, "Down passes over an item that's off");
	Check(bar.Handle(KeyEvent(Key::Up)) == MenuAnswer::Moved && bar.LitItem() == 0, "and so does Up");
	bar.SetEnabled(1, false);
	Check(bar.LitItem() == 2, "turning off the lit item lights the next that's on");
	bar.SetEnabled(3, false);
	Check(bar.LitItem() == -1 && bar.Handle(KeyEvent(Key::Enter)) == MenuAnswer::Nothing && bar.Handle(KeyEvent(Key::Down)) == MenuAnswer::Nothing,
		"with every item off, none is lit and nothing can be picked");
	bar.SetEnabled(1, true);
	bar.SetEnabled(3, true);
	Check(bar.LitItem() == -1 && bar.Handle(KeyEvent(Key::Up)) == MenuAnswer::Moved && bar.LitItem() == 2,
		"with none lit, Up lights the last item that's on, Quit");
	bar.SetEnabled(1, false);
	bar.SetEnabled(3, false);
	bar.Handle(KeyEvent(Key::Escape));
	bar.SetEnabled(1, true);
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.LitItem() == 0, "and turned back on, it's lit when the menu opens");
	Check(bar.Load(kBar, 4) && bar.IsEnabled(0, 1), "a new load turns every item back on");
}

static void LayOutAsBuilt(MenuBar &bar, const Box &room)
{
	Box const toolbar = { 0, 0, 1024, 32 };
	int32_t const titles[] = { 50, 60, 60, 50 };
	int32_t const drops[] = { 180, 200, 240, 300 };

	bar.LayOut(toolbar, room, titles, drops, 20);
}

static bool BoxIs(const Box &box, int32_t x, int32_t y, int32_t width, int32_t height)
{
	return box.x == x && box.y == y && box.width == width && box.height == height;
}

static void TestMouse()
{
	puts("The mouse and the boxes:");

	MenuBar bar;
	Box const listColumn = { 0, 32, 260, 716 };
	bar.Load(kBar, 4);
	LayOutAsBuilt(bar, listColumn);

	Check(BoxIs(bar.TitleBox(0), 0, 0, 50, 32) && BoxIs(bar.TitleBox(1), 50, 0, 60, 32) && BoxIs(bar.TitleBox(3), 170, 0, 50, 32) && BoxIs(bar.TitleBox(4), 0, 0, 0, 0),
		"the titles go left to right along the bar, each as wide as measured, and there's no fifth");
	Check(BoxIs(bar.DropBox(), 0, 0, 0, 0) && bar.ShownItems() == 0, "with no menu open there's no drop-down");

	Check(bar.Press(60, 10) == MenuAnswer::Opened && bar.OpenMenu() == 1 && BoxIs(bar.DropBox(), 50, 32, 200, 40), "a press on View opens it, under its title");
	Check(bar.Press(5, 10) == MenuAnswer::Moved && bar.OpenMenu() == 0 && BoxIs(bar.DropBox(), 0, 32, 180, 60), "a press on File goes to it");
	Check(bar.Press(5, 10) == MenuAnswer::Closed && !bar.IsOpen(), "and a press on the open menu's title closes it");

	bar.Press(5, 10);
	Check(bar.Press(10, 32 + 45) == MenuAnswer::Picked && bar.Picked() == 3 && !bar.IsOpen(), "a press on the third item picks it, Quit");
	bar.Press(5, 10);
	bar.SetEnabled(2, false);
	Check(bar.Press(10, 32 + 25) == MenuAnswer::Refused && bar.Refused() == 2 && bar.Picked() == 0 && bar.IsOpen(),
		"a press on an item that's off picks nothing, leaves the menu open, and says which item it was, Save, for the editor to say why");
	Check(bar.Press(500, 400) == MenuAnswer::Closed && bar.Refused() == 0 && !bar.IsOpen(), "a press anywhere else closes it, and forgets the item that was off");
	Check(bar.Press(500, 400) == MenuAnswer::Nothing, "and with nothing open, a press elsewhere is no business of the bar's");
	bar.SetEnabled(2, true);

	// Help's 300 wide drop-down, under a title at 170, can't fit the 260 room: it's held to the room, at its left edge
	bar.Press(175, 10);
	Check(bar.OpenMenu() == 3 && BoxIs(bar.DropBox(), 0, 32, 260, 40), "a drop-down wider than the room is cut to it, and moved left to stay inside");
	bar.Press(120, 10);
	Check(bar.OpenMenu() == 2 && BoxIs(bar.DropBox(), 0, 0, 0, 0), "Mode, with no items, shows no drop-down");
	bar.Handle(KeyEvent(Key::Escape));

	// inteng's menu loop (reassess/dialog-look-drake.md BAR-4, BAR-5): the first drop-down needs a press; with one open, the pointer on another title opens that one, no press
	Check(!bar.PointAt(60, 10) && !bar.IsOpen(), "BAR-4: with nothing open, the pointer on a title opens nothing (FUN_004b7dd0 leaves its wait only on a press, 0x4b7e7c)");
	bar.Press(5, 10);
	Check(bar.PointAt(60, 10) && bar.OpenMenu() == 1 && bar.LitItem() == -1 && BoxIs(bar.DropBox(), 50, 32, 200, 40),
		"BAR-5: with File open, the pointer on View opens View's drop-down, nothing lit in it (0x4b81de-0x4b82da)");
	Check(!bar.PointAt(60, 12) && bar.OpenMenu() == 1, "the pointer moving on the same title changes nothing");
	Check(bar.Release(60, 32 + 25) == MenuAnswer::Picked && bar.Picked() == 11, "the press that opened File goes on: let go on View's second item, it picks Lights");
	bar.Handle(KeyEvent(Key::Escape));
}

static void TestSmallRooms()
{
	puts("Rooms smaller than a drop-down:");

	MenuBar bar;
	MenuItem items[10];
	for (int i = 0; i < 10; i++)
	{
		items[i].name = "item";
		items[i].keys = 0;
		items[i].action = 100 + i;
	}
	Menu const tall = { "File", items, 10 };
	Box const toolbar = { 0, 0, 1024, 32 };
	int32_t const titles[] = { 50 };
	int32_t const drops[] = { 180 };
	Box room = { 0, 32, 260, 45 };

	bar.Load(&tall, 1);
	bar.LayOut(toolbar, room, titles, drops, 20);
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.ShownItems() == 2 && BoxIs(bar.DropBox(), 0, 32, 180, 40) && bar.FirstShownItem() == 0, "a room 45 high shows the 2 whole items that fit, 40 high");
	bar.Handle(KeyEvent(Key::Down));
	bar.Handle(KeyEvent(Key::Down));
	Check(bar.LitItem() == 2 && bar.FirstShownItem() == 1, "lighting the third scrolls so it's shown");
	bar.Handle(KeyEvent(Key::End));
	Check(bar.LitItem() == 9 && bar.FirstShownItem() == 8, "End shows the last two");
	bar.Handle(KeyEvent(Key::Home));
	Check(bar.FirstShownItem() == 0, "and Home the first two");
	bar.Handle(KeyEvent(Key::Down));
	Check(bar.Press(10, 32 + 25) == MenuAnswer::Picked && bar.Picked() == 101, "a press on the second row picks the item shown there");

	bar.Handle(KeyEvent(Key::Menu));
	bar.Handle(KeyEvent(Key::End));
	Check(bar.Press(10, 32 + 25) == MenuAnswer::Picked && bar.Picked() == 109, "and scrolled, the row is the one shown, not the one first in the list");

	int32_t const heights[] = { 0, 19, -5 };
	bool noneShown = true;
	for (size_t h = 0; h < sizeof(heights) / sizeof(heights[0]); h++)
	{
		room.height = heights[h];
		bar.LayOut(toolbar, room, titles, drops, 20);
		bar.Handle(KeyEvent(Key::Menu));
		noneShown = noneShown && bar.ShownItems() == 0 && BoxIs(bar.DropBox(), 0, 0, 0, 0) && bar.Press(10, 32) == MenuAnswer::Closed;
	}
	Check(noneShown, "a room of no height, one too short for one item, or one below 0, shows nothing, and no press lands in it");

	room.height = 400;
	room.width = 0;
	bar.LayOut(toolbar, room, titles, drops, 20);
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.ShownItems() == 0 && BoxIs(bar.DropBox(), 0, 0, 0, 0), "and so does a room of no width");
	bar.Handle(KeyEvent(Key::Escape));

	room.width = 260;
	bar.LayOut(toolbar, room, titles, drops, 0);
	bar.Handle(KeyEvent(Key::Menu));
	Check(bar.ShownItems() == 0 && bar.Press(10, 40) == MenuAnswer::Closed, "items of no height show nothing either");

	int32_t const negative[] = { -50 };
	Box const narrowBar = { 0, 0, 30, 32 };
	bar.LayOut(narrowBar, room, negative, negative, 20);
	Check(BoxIs(bar.TitleBox(0), 0, 0, 0, 32), "a width below 0 counts as 0");
	bar.LayOut(narrowBar, room, titles, drops, 20);
	Check(BoxIs(bar.TitleBox(0), 0, 0, 30, 32), "and a title past the bar's end is cut where the bar ends");
	bar.LayOut(toolbar, room, 0, 0, 20);
	Check(BoxIs(bar.TitleBox(0), 0, 0, 0, 32), "no widths at all count as 0");

	Box const farBar = { INT32_MAX - 10, 0, 100, 32 };
	bar.LayOut(farBar, room, titles, drops, 20);
	Box const far = bar.TitleBox(0);
	Check(far.x == INT32_MAX - 10 && far.width >= 0, "a bar at the far end of the numbers doesn't wrap round");
}

int main()
{
	TestLoading();
	TestKeys();
	TestOff();
	TestMouse();
	TestSmallRooms();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
