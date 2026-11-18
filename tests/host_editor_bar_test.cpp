// The test for src/host's editor bar (editor_bar.h): every screen's bar is made and loads into the menu bar, inteng's items show their
// words alone (no tag after a name, as inteng draws them), the editor's own menu and the hub's Screens menu are where they should be, every pick does what it says, the widest bar is
// measured, and titles past the toolbar's edge are named. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A6). AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <set>
#include <string>

#include "editor_bar.h"
#include "menu_paint.h"

using host::kIntengScreenCount;
using host::kIntengScreens;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// Whether a text is these words; a null text is never any words
static bool Is(const char *text, const char *words)
{
	return text && strcmp(text, words) == 0;
}

// Every letter six pixels wide
static int SixWide(int)
{
	return 6;
}

static EditorBarRoom room;

// The item with this number in the bar made last, or null
static const ui::MenuItem *ItemFor(int32_t action)
{
	for (size_t m = 0; m < room.menuCount; m++)
	{
		for (size_t i = 0; i < room.menus[m].count; i++)
		{
			if (room.menus[m].items[i].action == action)
			{
				return &room.menus[m].items[i];
			}
		}
	}

	return nullptr;
}

static void TestEveryBar()
{
	puts("Every screen's bar:");

	bool allMade = true;
	bool allLoad = true;
	std::string first;

	for (KeyLayout const layout : { KeyLayout::Classic, KeyLayout::ModernWasd })
	{
		for (size_t s = 0; s < kIntengScreenCount; s++)
		{
			ui::MenuBar bar;
			bool const made = EditorBar_Make(s, layout, &room);

			allMade = allMade && made;
			allLoad = allLoad && made && bar.Load(room.menus, room.menuCount);
			if (!allLoad && first.empty())
			{
				first = kIntengScreens[s].name;
			}
		}
	}

	Check(allMade, "every screen's bar is made, in both key layouts");
	Check(allLoad, "and loads into the menu bar: every title, item, key and tag passes its checks" + (first.empty() ? std::string() : " (not: " + first + ")"));
	Check(!EditorBar_Make(kIntengScreenCount, KeyLayout::Classic, &room) && !EditorBar_Make(0, KeyLayout::Classic, nullptr),
		"a screen that isn't one, or no room, makes nothing");
}

// Every row of a made room and of its cascades: counts them, and those of an item inteng_menus tags as not built (so the check below reads some), and keeps the first row that shows a tag
static void RowsOfRoom(const EditorBarRoom &made, size_t *rows, size_t *notBuilt, std::string *shownTag)
{
	for (size_t m = 0; m < made.menuCount; m++)
	{
		for (size_t i = 0; i < made.menus[m].count; i++)
		{
			const ui::MenuItem &item = made.menus[m].items[i];

			for (size_t k = 0; k <= item.subCount; k++)
			{
				const ui::MenuItem &row = k == 0 ? item : item.subItems[k - 1];
				const host::IntengItem *const inteng = host::IntengMenus_Find(row.action);

				(*rows)++;
				*notBuilt += inteng && inteng->tag != host::IntengTag::None ? 1 : 0;

				if (row.tag && shownTag->empty())
				{
					*shownTag = std::string(row.name) + row.tag;
				}
			}
		}
	}
}

static void TestNoTags()
{
	puts("Every row as inteng draws it:");

	// every page any screen has (Object Placement's pop-up pages are Track Editing's second-row pages), and page 0
	std::set<int32_t> pages = { 0 };

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		for (size_t p = 0; p < kIntengScreens[s].pageCount; p++)
		{
			pages.insert(kIntengScreens[s].pages[p].page);
		}
	}

	size_t rows = 0, notBuilt = 0;
	std::string shownTag;

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		for (int32_t const page : pages)
		{
			EditorBar_SetPage(page);
			EditorBar_SetPopupPage(page);

			if (EditorBar_Make(s, KeyLayout::Classic, &room))
			{
				RowsOfRoom(room, &rows, &notBuilt, &shownTag);
			}

			if (EditorBar_MakeSecond(s, &room))
			{
				RowsOfRoom(room, &rows, &notBuilt, &shownTag);
			}

			for (int32_t group = -1; group < (int32_t)ui::kMenuMostItems; group++)
			{
				if (EditorBar_MakePopupLevel(s, group, &room))
				{
					RowsOfRoom(room, &rows, &notBuilt, &shownTag);
				}
			}
		}
	}

	EditorBar_SetPage(0);
	EditorBar_SetPopupPage(0);

	Check(rows > 0 && notBuilt > 0 && shownTag.empty(), "no row of any screen's bars, pages, cascades or pop-up shows a tag after its name (" + std::to_string(rows) + " rows, " +
		std::to_string(notBuilt) + " of them items with nothing built behind them" + (shownTag.empty() ? std::string() : "; not: \"" + shownTag + "\"") + ")");
}

static void TestTheHub()
{
	puts("The hub's bar:");

	Check(EditorBar_Make(host::kIntengHub, KeyLayout::Classic, &room) && room.menuCount == 6, "the hub has inteng's 6 menus and nothing of ours (no View (ours), no Screens (ours))");
	Check(room.menuCount == 6 && Is(room.menus[0].title, "File") && Is(room.menus[5].title, "Options"), "File first, Options last");
	Check(ItemFor(kActionBackToHub) == nullptr && ItemFor(2) == nullptr && ItemFor(301) == nullptr,
		"the hub has no way back to itself, and the editor's old Quit and Help, Keys are left off: inteng's own do them");

	const ui::MenuItem *const open = ItemFor(kActionOpenLevel);
	const ui::MenuItem *const quit = ItemFor(0x10001);
	const ui::MenuItem *const write = ItemFor(0x10002);

	Check(open == nullptr, "the editor's own Open level is not on the bar (Ctrl+O still does it)");
	Check(quit && Is(quit->name, "Quit") && quit->tag == nullptr && write && Is(write->name, "Write C File") && write->tag == nullptr,
		"inteng's Quit, which works, and Write C File, which isn't built, both show their name alone, as inteng draws every item (no \" [wip]\" after it)");
}

static void TestAScreen()
{
	puts("Another screen's bar:");

	Check(EditorBar_Make(6, KeyLayout::Classic, &room) && room.menuCount == 5, "object placement has inteng's 5 menus and nothing of ours");
	Check(ItemFor(kActionBackToHub) == nullptr, "and no Back to Construction: inteng's Exit is the way back");
}

static void TestPicks()
{
	puts("What each pick does:");

	EditorHostState host = { true, nullptr, KeyLayout::Classic, host::kIntengHub };

	Check(EditorBar_Run(0x1000a, &host) && host.screen == 6 && Is(host.statusWords, "Object Placement") && host.running,
		"construction's Object Placement goes to object placement's screen, and the status line names it");

	EditorBar_Make(6, KeyLayout::Classic, &room);

	const ui::MenuItem *exit = nullptr;

	for (size_t i = 0; i < room.menus[0].count; i++)
	{
		exit = Is(room.menus[0].items[i].name, "Exit") ? &room.menus[0].items[i] : exit;
	}

	// A mode's Exit is the mode's Exit now (the editor saves what changed, says "Camera position saved" and shows the hub: editor_main.cpp ExitTheMode): the bar hands it over
	Check(exit && EditorBar_Run(exit->action, &host) && host.exitMode, "placement's Exit is the mode's Exit: the bar asks the editor for it");
	host.screen = host::kIntengHub;
	host.statusWords = nullptr;
	// (Frost 11-18: Write C File, 0x10002, was the example; it is built now, so a still-[wip] item is: Light Sourcing's Acoustic type, 0xc1c03)
	Check(EditorBar_Run(0xc1c03, &host) && host.screen == host::kIntengHub && Is(host.statusWords, host::IntengMenus_Why(host::IntengTag::Wip)),
		"a [wip] item does nothing but say why, on the status line");
	host.prompt = 0;
	Check(EditorBar_Run(0x10002, &host) && host.screen == host::kIntengHub && host.prompt == kPromptWriteC,
		"Write C File asks the editor for the file name (kPromptWriteC; write_c_file.h decides whether one object is selected)");
	host.prompt = 0;
	Check(EditorBar_Run(kActionScreenFirst + 12, &host) && host.screen == 12 && Is(host.statusWords, kIntengScreens[12].name),
		"a Screens item opens that screen");
	Check(EditorBar_Run(kActionBackToHub, &host) && host.screen == host::kIntengHub, "Back to Construction goes back to the hub");

	host.statusWords = nullptr;
	host.showMovementKeys = false;
	// Construction's Keys - Movement shows the camera keys' text screen (FUN_005573c0; TB-C6): the bar asks for it, the hub draws no status words
	Check(EditorBar_Run(0x10021, &host) && host.showMovementKeys && host.statusWords == nullptr,
		"construction's Keys - Movement asks for the camera keys' text screen");
	Check(EditorBar_Run(kActionViewPolygon, &host) && host.statusWords != nullptr && host.screen == host::kIntengHub,
		"the editor's own items still do what they did");
	Check(EditorBar_Run(0x10001, &host) && !host.running, "construction's Quit closes the editor");

	host.running = true;
	host.statusWords = nullptr;
	Check(!EditorBar_Run(2, &host) && !EditorBar_Run(301, &host) && !EditorBar_Run(kActionScreenFirst + (int32_t)kIntengScreenCount, &host)
		&& !EditorBar_Run(0, &host) && !EditorBar_Run(-5, &host) && host.running && host.statusWords == nullptr && host.screen == host::kIntengHub,
		"a number no item of any bar has does nothing, and the two left off the bar are refused too");
	Check(!EditorBar_Run(0x10001, nullptr), "no host does nothing");
}

static void TestWidth()
{
	puts("The widest bar, and titles past the edge:");

	int32_t const widest = EditorBar_WidestTitles(KeyLayout::Classic, SixWide);
	bool atLeast = true;

	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		int32_t titles[ui::kMenuMostMenus];
		int32_t drops[ui::kMenuMostMenus];
		int64_t sum = 0;

		EditorBar_Make(s, KeyLayout::Classic, &room);
		host::MenuPaint_Measure(room.menus, room.menuCount, SixWide, titles, drops);

		for (size_t m = 0; m < room.menuCount; m++)
		{
			sum += titles[m];
		}

		atLeast = atLeast && widest >= sum;
	}

	Check(widest > 0 && atLeast, "the widest bar's titles are measured, and no screen's are wider");

	ui::MenuBar bar;
	int32_t titles[ui::kMenuMostMenus];
	int32_t drops[ui::kMenuMostMenus];
	char words[256];

	EditorBar_Make(4, KeyLayout::Classic, &room);
	bar.Load(room.menus, room.menuCount);
	host::MenuPaint_Measure(room.menus, room.menuCount, SixWide, titles, drops);
	bar.LayOut(ui::Box{ 0, 0, widest, 32 }, ui::Box{ 0, 32, 260, 700 }, titles, drops, host::MenuPaint_ItemHeight(15));
	EditorBar_CutOff(bar, room.menus, room.menuCount, words, sizeof(words));
	Check(words[0] == '\0', "on a toolbar as wide as the widest bar, no title is past its edge, and nothing is said");

	bar.LayOut(ui::Box{ 0, 0, 120, 32 }, ui::Box{ 0, 32, 260, 700 }, titles, drops, host::MenuPaint_ItemHeight(15));
	EditorBar_CutOff(bar, room.menus, room.menuCount, words, sizeof(words));
	Check(strncmp(words, "Menus past the window's edge: ", 30) == 0 && strlen(words) > 30,
		"on a narrow toolbar, the titles past its edge are named");

	char few[12];

	EditorBar_CutOff(bar, room.menus, room.menuCount, few, sizeof(few));
	Check(strlen(few) == 11, "the words are cut to the room given, and always end in a 0");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestEveryBar();
	TestNoTags();
	TestTheHub();
	TestAScreen();
	TestPicks();
	TestWidth();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
