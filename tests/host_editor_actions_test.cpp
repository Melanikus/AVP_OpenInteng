// The test for src/host's table of actions: every command the menu bar offers, its keys in the two key layouts, and the camera's keys.
// Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own
// code". (Modified 2026-11-14 by Melanikus (Dietrich), F3: the two key layouts.)
//
// It runs none of the editor. The editor's own table is checked as the editor checks it at start; then tables with one thing wrong each
// prove the check refuses that thing. A table whose two layouts differ proves the menus and the keys follow the layout asked for, which
// the editor's own table can't (its lines have the same key in both layouts). Each check's words say what it proves. The program gives
// back a non-zero result if any check failed.

#include <windows.h>   // only for the keys' numbers, VK_...
#include <stdio.h>
#include <string.h>

#include <string>

#include "editor_actions.h"

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

static size_t const kMoveCount = (size_t)EditorMove::Count;

static Menu menus[(size_t)EditorMenu::Count];
static MenuItem items[64];
static EditorKeyWords words[64];

// The item for this action in the menus made last, or null
static const MenuItem *ItemFor(int32_t action)
{
	for (size_t m = 0; m < (size_t)EditorMenu::Count; m++)
	{
		for (size_t i = 0; i < menus[m].count; i++)
		{
			if (menus[m].items[i].action == action)
			{
				return &menus[m].items[i];
			}
		}
	}

	return 0;
}

static bool KeysAre(int32_t action, const char *keys)
{
	const MenuItem *const item = ItemFor(action);

	return item && (keys ? item->keys && strcmp(item->keys, keys) == 0 : item->keys == 0);
}

static void TestTheTable()
{
	puts("The editor's own table:");

	Check(EditorActions_CheckTable(kEditorActions, kEditorActionCount, kEditorMoves, kMoveCount) == 0,
		"the table and the camera's keys pass the check the editor runs at start");

	for (KeyLayout const layout : { KeyLayout::Classic, KeyLayout::ModernWasd })
	{
		std::string const in = layout == KeyLayout::Classic ? " (Classic inteng)" : " (Modern WASD)";

		Check(EditorActions_MakeMenus(kEditorActions, kEditorActionCount, layout, menus, (size_t)EditorMenu::Count, items, 64, words, 64),
			"the menus are made from the table" + in);

		MenuBar bar;
		Check(bar.Load(menus, (size_t)EditorMenu::Count), "and the menu bar takes them: their words, keys and numbers pass its checks" + in);
		Check(strcmp(menus[0].title, "File") == 0 && strcmp(menus[1].title, "View") == 0 && strcmp(menus[2].title, "Mode") == 0
			&& strcmp(menus[3].title, "Help") == 0, "the menus are File, View, Mode and Help, in that order" + in);

		size_t total = 0;
		for (size_t m = 0; m < (size_t)EditorMenu::Count; m++)
		{
			total += menus[m].count;
		}
		Check(total == kEditorActionCount, "every line of the table is an item, once" + in);

		Check(KeysAre(kActionOpenLevel, 0) && KeysAre(kActionQuit, "Alt+F4") && KeysAre(kActionViewPolyline, "8")
			&& KeysAre(kActionViewPolygon, "7") && KeysAre(kActionViewFilledPolyline, "9") && KeysAre(kActionViewGouraud, "B")
			&& KeysAre(kActionViewHeadlight, "Ctrl+B") && KeysAre(kActionHelpKeys, 0),
			"each item shows its key's words: inteng's 8, 7, 9, B and Ctrl+B for the draw modes; Windows' Alt+F4 is only shown" + in);
	}

	Check(menus[1].count >= 2 && menus[1].items[menus[1].count - 2].action == kActionViewKeysClassic
		&& menus[1].items[menus[1].count - 1].action == kActionViewKeysModern, "the two key layouts are the View menu's last two items");

	Check(!EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count - 1, items, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count, items, kEditorActionCount - 1, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count, items, 64, words, kEditorActionCount - 1)
		&& !EditorActions_MakeMenus(0, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count, items, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, 0, 4, items, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, 4, 0, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, 4, items, 64, 0, 64),
		"too little room for menus, items or words, or none, or no table, makes nothing");
}

static void TestKeyWords()
{
	puts("A key's words:");

	char w[kEditorKeyWordsRoom];
	EditorKey const ctrlO = { 'O', true, false };
	EditorKey const seven = { '7', false, false };
	EditorKey const pad7 = { VK_NUMPAD7, false, false };
	EditorKey const plus = { VK_ADD, false, false };
	EditorKey const minus = { VK_SUBTRACT, false, false };
	EditorKey const left = { VK_LEFT, false, false };
	EditorKey const both = { 'B', true, true };
	EditorKey const longest = { VK_NUMPAD9, true, true };
	EditorKey const none = { 0, false, false };
	EditorKey const f5 = { VK_F5, false, false };

	Check(EditorActions_KeyWords(ctrlO, w, sizeof(w)) && strcmp(w, "Ctrl+O") == 0, "Ctrl+O");
	Check(EditorActions_KeyWords(seven, w, sizeof(w)) && strcmp(w, "7") == 0 && EditorActions_KeyWords(pad7, w, sizeof(w)) && strcmp(w, "Num 7") == 0,
		"the main row's 7 is \"7\" and the keypad's is \"Num 7\": two keys");
	Check(EditorActions_KeyWords(plus, w, sizeof(w)) && strcmp(w, "Num +") == 0 && EditorActions_KeyWords(minus, w, sizeof(w)) && strcmp(w, "Num -") == 0,
		"the keypad's plus and minus");
	Check(EditorActions_KeyWords(left, w, sizeof(w)) && strcmp(w, "Left") == 0, "an arrow");
	Check(EditorActions_KeyWords(both, w, sizeof(w)) && strcmp(w, "Ctrl+Shift+B") == 0, "Ctrl and Shift together");
	Check(EditorActions_KeyWords(longest, w, sizeof(w)) && strcmp(w, "Ctrl+Shift+Num 9") == 0, "the longest words fit their room");
	Check(!EditorActions_KeyWords(none, w, sizeof(w)) && w[0] == '\0' && !EditorActions_KeyWords(f5, w, sizeof(w)) && w[0] == '\0',
		"no key, or a key the editor has no name for, has no words");
	Check(!EditorActions_KeyWords(ctrlO, w, 6) && w[0] == '\0' && EditorActions_KeyWords(ctrlO, w, 7) && strcmp(w, "Ctrl+O") == 0,
		"words that don't fit their room aren't given, cut; exactly room enough is enough");

	char canary[2] = { 'x', 'y' };
	Check(!EditorActions_KeyWords(ctrlO, canary, 0) && canary[0] == 'x' && !EditorActions_KeyWords(ctrlO, 0, 8), "no room, or none at all, is never written to");
}

static void TestTheMoves()
{
	puts("The camera's keys:");

	Check(EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Forward) == 'A' && EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Back) == 'Z',
		"Classic: A forward and Z back, as inteng's keys.txt has them");
	Check(EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Left) == VK_NUMPAD1 && EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Right) == VK_NUMPAD3,
		"Classic: the keypad's 1 and 3 sideways");
	Check(EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Up) == VK_SUBTRACT && EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Down) == VK_ADD,
		"Classic: the keypad's minus up and plus down (\"-/+ Move up/down\")");
	Check(EditorActions_MoveKey(KeyLayout::Classic, EditorMove::TurnLeft) == VK_NUMPAD4 && EditorActions_MoveKey(KeyLayout::Classic, EditorMove::TurnRight) == VK_NUMPAD6
		&& EditorActions_MoveKey(KeyLayout::Classic, EditorMove::LookUp) == VK_NUMPAD2 && EditorActions_MoveKey(KeyLayout::Classic, EditorMove::LookDown) == VK_NUMPAD8,
		"Classic: the keypad's 4 and 6 turn, 2 looks up and 8 down (\"2/8 Pitch up/down\")");
	Check(EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::Forward) == 'W' && EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::Back) == 'S'
		&& EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::Left) == 'A' && EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::Right) == 'D'
		&& EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::Up) == 'R' && EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::Down) == 'F',
		"Modern: W S A D, R up and F down");
	Check(EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::TurnLeft) == VK_LEFT && EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::TurnRight) == VK_RIGHT
		&& EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::LookUp) == VK_UP && EditorActions_MoveKey(KeyLayout::ModernWasd, EditorMove::LookDown) == VK_DOWN,
		"Modern: the arrows turn and look");
	Check(EditorActions_MoveKey(KeyLayout::Classic, EditorMove::Count) == 0 && EditorActions_MoveKey(KeyLayout::ModernWasd, (EditorMove)200) == 0,
		"a move that isn't one has no key");

	bool noMainRowDigit = true;
	for (size_t m = 0; m < kMoveCount; m++)
	{
		uint8_t const key = EditorActions_MoveKey(KeyLayout::Classic, (EditorMove)m);
		noMainRowDigit = noMainRowDigit && !(key >= '0' && key <= '9');
	}
	Check(noMainRowDigit, "Classic's camera never takes a main-row digit, which are the draw modes'");
}

static void TestRunning()
{
	puts("What each action does:");

	EditorHostState host = { true, 0, KeyLayout::Classic };

	EditorActions_Find(kActionQuit)->run(&host);
	Check(!host.running, "Quit stops the editor");

	host.running = true;
	EditorActions_Find(kActionHelpKeys)->run(&host);
	Check(host.running && host.layout == KeyLayout::Classic && host.statusWords && strcmp(host.statusWords,
		"Classic inteng keys, with NumLock on: A forward, Z back, Num 1 left, Num 3 right, Num - up, Num + down, Num 4 turn left, Num 6 turn right, "
		"Num 2 look up, Num 8 look down, Num 7 roll left, Num 9 roll right; the mouse with its right button held turns and looks. Menus: F10 or Alt") == 0,
		"Help, Keys in Classic says Classic's keys, made from the table of moves, and changes no layout");

	host.layout = KeyLayout::ModernWasd;
	EditorActions_Find(kActionHelpKeys)->run(&host);
	Check(host.statusWords && strcmp(host.statusWords,
		"Modern WASD keys: W forward, S back, A left, D right, R up, F down, Left turn left, Right turn right, Up look up, Down look down, Num 7 roll left, Num 9 roll right; "
		"the mouse with its right button held turns and looks. Menus: F10 or Alt") == 0, "Help, Keys in Modern says Modern's");

	EditorActions_Find(kActionViewKeysClassic)->run(&host);
	Check(host.layout == KeyLayout::Classic && host.statusWords == kEditorClassicNowWords && host.running, "Classic inteng keys picks Classic, and says so");
	EditorActions_Find(kActionViewKeysModern)->run(&host);
	Check(host.layout == KeyLayout::ModernWasd && host.statusWords == kEditorModernNowWords, "Modern WASD keys picks Modern, and says so");

	EditorActions_Find(kActionHelpAbout)->run(&host);
	Check(host.statusWords == kEditorAboutWords, "Help, About says what the editor is");
	EditorActions_Find(kActionOpenLevel)->run(&host);
	Check(host.running && host.statusWords == kEditorOpenNotYetWords, "Open level says it isn't here yet, and what the editor does instead");
	Check(EditorActions_Find(0) == 0 && EditorActions_Find(-1) == 0 && EditorActions_Find(99999) == 0, "a number no action has finds none");
}

static void TestKeys()
{
	puts("The keys, read as keys:");

	const EditorAction *const open = EditorActions_Find(kActionOpenLevel);

	for (KeyLayout const layout : { KeyLayout::Classic, KeyLayout::ModernWasd })
	{
		std::string const in = layout == KeyLayout::Classic ? " (Classic)" : " (Modern)";

		Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'O', false, true, false) == 0 && open != nullptr, "Ctrl+O is no key any more: inteng's hub has no Open (Bishop 11-16)" + in);
		Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, '8', false, false, false) == EditorActions_Find(kActionViewPolyline)
			&& EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, VK_NUMPAD8, false, false, false) == 0,
			"the main row's 8 is Polyline; the keypad's 8 is no action" + in);
		Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'B', false, false, false) == EditorActions_Find(kActionViewGouraud)
			&& EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'B', false, true, false) == EditorActions_Find(kActionViewHeadlight),
			"B is Gouraud and Ctrl+B the headlight" + in);
		Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'O', false, false, false) == 0
			&& EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'O', true, true, false) == 0,
			"a plain O, or Ctrl+Shift+O, is no action: a key's Ctrl and Shift are part of it" + in);
		Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'O', false, true, true) == 0
			&& EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'O', false, false, true) == 0,
			"with Alt held, or AltGr (Ctrl and Alt), no key is an action" + in);
		Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 'V', false, true, false) == 0
			&& EditorActions_ForKey(kEditorActions, kEditorActionCount, layout, 0, false, false, false) == 0
			&& EditorActions_ForKey(0, kEditorActionCount, layout, 'O', false, true, false) == 0, "Ctrl+V, no key, and no table are no action" + in);
	}

	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, KeyLayout::Classic, 'A', false, false, false) == 0,
		"a camera key is held, not an action");
}

// A table whose two layouts differ, so the menus and the keys must follow the layout asked for
static void TestTheLayoutIsFollowed()
{
	puts("The layout asked for is the one used:");

	EditorAction differ[64];
	memcpy(differ, kEditorActions, sizeof(EditorAction) * kEditorActionCount);
	EditorKey const x = { 'X', false, false };
	EditorKey const y = { 'Y', false, false };
	differ[2].classic = x;
	differ[2].modern = y;

	Check(EditorActions_CheckTable(differ, kEditorActionCount, kEditorMoves, kMoveCount) == 0, "(a table with X in Classic and Y in Modern on one line is sound)");
	Check(EditorActions_MakeMenus(differ, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count, items, 64, words, 64)
		&& KeysAre(differ[2].id, "X"), "its menus for Classic show X");
	Check(EditorActions_MakeMenus(differ, kEditorActionCount, KeyLayout::ModernWasd, menus, (size_t)EditorMenu::Count, items, 64, words, 64)
		&& KeysAre(differ[2].id, "Y"), "and for Modern show Y");
	Check(EditorActions_ForKey(differ, kEditorActionCount, KeyLayout::Classic, 'X', false, false, false) == &differ[2]
		&& EditorActions_ForKey(differ, kEditorActionCount, KeyLayout::Classic, 'Y', false, false, false) == 0
		&& EditorActions_ForKey(differ, kEditorActionCount, KeyLayout::ModernWasd, 'Y', false, false, false) == &differ[2]
		&& EditorActions_ForKey(differ, kEditorActionCount, KeyLayout::ModernWasd, 'X', false, false, false) == 0, "and X works only in Classic, Y only in Modern");
	Check(&EditorActions_KeyIn(differ[2], KeyLayout::Classic) == &differ[2].classic && &EditorActions_KeyIn(differ[2], KeyLayout::ModernWasd) == &differ[2].modern,
		"an action's key in a layout is that layout's");
}

static EditorAction table[64];
static EditorMoveKeys moves[(size_t)EditorMove::Count];

static void Fresh()
{
	memcpy(table, kEditorActions, sizeof(EditorAction) * kEditorActionCount);
	memcpy(moves, kEditorMoves, sizeof(moves));
}

static bool Refused(const char *because)
{
	const char *const why = EditorActions_CheckTable(table, kEditorActionCount, moves, kMoveCount);

	return why && strstr(why, because) != 0;
}

static bool Sound()
{
	return EditorActions_CheckTable(table, kEditorActionCount, moves, kMoveCount) == 0;
}

static void TestTheCheckRefuses()
{
	puts("The check refuses a table with one thing wrong:");

	EditorKey const ctrlO = { 'O', true, false };
	EditorKey const plainO = { 'O', false, false };
	EditorKey const x = { 'X', false, false };
	EditorKey const w = { 'W', false, false };
	EditorKey const a = { 'A', false, false };
	EditorKey const ctrlA = { 'A', true, false };
	EditorKey const ctrlV = { 'V', true, false };
	EditorKey const ctrlShiftV = { 'V', true, true };
	EditorKey const plainV = { 'V', false, false };
	EditorKey const f5 = { VK_F5, false, false };
	EditorKey const ctrlQ = { 'Q', true, false };

	Fresh(); table[1].id = table[0].id;
	Check(Refused("share a number"), "two actions with one number");
	Fresh(); table[0].id = 0;
	Check(Refused("above 0"), "a number of 0");
	Fresh(); table[0].menu = EditorMenu::Count;
	Check(Refused("four menus"), "an action in no menu");
	Fresh(); table[0].name = "";
	Check(Refused("no words"), "an action with no words");
	Fresh(); table[0].run = 0;
	Check(Refused("does nothing"), "an action that does nothing");
	Fresh(); table[0].classic = ctrlO; table[2].classic = ctrlO;   // (the real table's Ctrl+O is gone: the first line is given it here)
	Check(Refused("share a key"), "two actions on one key in Classic");
	Fresh(); table[0].modern = ctrlO; table[2].modern = ctrlO;
	Check(Refused("share a key"), "two actions on one key in Modern only");
	Fresh(); table[3].classic = plainO;
	Check(Sound(), "O and Ctrl+O are two keys");
	Fresh(); table[3].classic = x; table[4].modern = x;
	Check(Sound(), "one key in Classic and the same in Modern, on two lines, is sound: the layouts are separate");
	Fresh(); table[3].classic = a;
	Check(Refused("camera holds"), "an action on Classic's forward key, A");
	Fresh(); table[3].classic = ctrlA;
	Check(Refused("camera holds"), "and on Ctrl+A: a held key counts whatever else is held");
	Fresh(); table[3].modern = a;
	Check(Refused("camera holds"), "an action on Modern's left key, A");
	Fresh(); table[3].classic = w;
	Check(Sound(), "W is free in Classic: only Modern's camera holds it");
	Fresh(); table[3].classic = ctrlV;
	Check(Refused("Ctrl+V"), "Ctrl+V, the paste's");
	Fresh(); table[3].classic = ctrlShiftV;
	Check(Refused("Ctrl+V"), "Ctrl+Shift+V too");
	Fresh(); table[3].classic = plainV;
	Check(Sound(), "a plain V is free");
	Fresh(); table[3].classic = f5;
	Check(Refused("no name"), "a key the editor has no name for");
	Fresh(); table[1].classic = ctrlQ;
	Check(Refused("Windows' keys"), "an action that shows Windows' keys and has a key of its own");

	Fresh(); moves[3].move = EditorMove::Left;
	Check(Refused("out of order"), "a move out of order");
	Fresh(); moves[0].name = "";
	Check(Refused("no words"), "a move with no words");
	Fresh(); moves[1].classic = moves[0].classic;
	Check(Refused("share a key"), "two moves on one key");
	Fresh(); moves[4].modern = 0;
	Check(Refused("no key"), "a move with no key in a layout");
	Fresh(); moves[4].classic = VK_F5;
	Check(Refused("no key"), "a move on a key the editor has no name for");

	Fresh();
	Check(EditorActions_CheckTable(table, kEditorActionCount, moves, kMoveCount - 1) != 0, "the camera's keys short of a move");
	Check(EditorActions_CheckTable(table, 0, moves, kMoveCount) != 0 && EditorActions_CheckTable(0, kEditorActionCount, moves, kMoveCount) != 0
		&& EditorActions_CheckTable(table, kEditorActionCount, 0, kMoveCount) != 0, "no table, or an empty one, or no moves");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheTable();
	TestKeyWords();
	TestTheMoves();
	TestRunning();
	TestKeys();
	TestTheLayoutIsFollowed();
	TestTheCheckRefuses();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
