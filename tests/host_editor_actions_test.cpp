// The test for src/host's table of actions: every command the menu bar offers, its key, and the camera's keys.
// Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own
// code". (Modified 2026-11-14 by Melanikus (Dietrich), F3: the two key layouts. Modified 2026-11-25 by Melanikus (Frost): one layout,
// inteng's, the Modern WASD layout's code removed; its keys freed.)
//
// It runs none of the editor. The editor's own table is checked as the editor checks it at start; then tables with one thing wrong each
// prove the check refuses that thing. A table with another key on one line proves the menus and the keys follow the line's key. Each
// check's words say what it proves. The program gives
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

	Check(EditorActions_MakeMenus(kEditorActions, kEditorActionCount, menus, (size_t)EditorMenu::Count, items, 64, words, 64),
		"the menus are made from the table");

	MenuBar bar;
	Check(bar.Load(menus, (size_t)EditorMenu::Count), "and the menu bar takes them: their words, keys and numbers pass its checks");
	Check(strcmp(menus[0].title, "File") == 0 && strcmp(menus[1].title, "View") == 0 && strcmp(menus[2].title, "Mode") == 0
		&& strcmp(menus[3].title, "Help") == 0, "the menus are File, View, Mode and Help, in that order");

	size_t total = 0;
	for (size_t m = 0; m < (size_t)EditorMenu::Count; m++)
	{
		total += menus[m].count;
	}
	Check(total == kEditorActionCount, "every line of the table is an item, once");

	Check(KeysAre(kActionOpenLevel, 0) && KeysAre(kActionQuit, "Alt+F4") && KeysAre(kActionViewPolyline, "8")
		&& KeysAre(kActionViewPolygon, "7") && KeysAre(kActionViewFilledPolyline, "9") && KeysAre(kActionViewGouraud, "B")
		&& KeysAre(kActionViewHeadlight, "Ctrl+B") && KeysAre(kActionHelpKeys, 0),
		"each item shows its key's words: inteng's 8, 7, 9, B and Ctrl+B for the draw modes; Windows' Alt+F4 is only shown");

	// THE MODERN (WASD) LAYOUT IS GONE (Frost 2026-11-25, the owner: "WASD mode is off"): no action picks a layout any more
	bool noLayoutItem = true;
	for (size_t a = 0; a < kEditorActionCount; a++)
	{
		noLayoutItem = noLayoutItem && kEditorActions[a].id != 151 && kEditorActions[a].id != 152 && strstr(kEditorActions[a].name, "WASD") == 0;
	}
	Check(noLayoutItem, "no item picks a key layout (151 and 152 retired) or names WASD");

	Check(!EditorActions_MakeMenus(kEditorActions, kEditorActionCount, menus, (size_t)EditorMenu::Count - 1, items, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, menus, (size_t)EditorMenu::Count, items, kEditorActionCount - 1, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, menus, (size_t)EditorMenu::Count, items, 64, words, kEditorActionCount - 1)
		&& !EditorActions_MakeMenus(0, kEditorActionCount, menus, (size_t)EditorMenu::Count, items, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, 0, 4, items, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, menus, 4, 0, 64, words, 64)
		&& !EditorActions_MakeMenus(kEditorActions, kEditorActionCount, menus, 4, items, 64, 0, 64),
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

	Check(EditorActions_MoveKey(EditorMove::Forward) == 'A' && EditorActions_MoveKey(EditorMove::Back) == 'Z',
		"A forward and Z back, as inteng's keys.txt has them");
	Check(EditorActions_MoveKey(EditorMove::Left) == VK_NUMPAD1 && EditorActions_MoveKey(EditorMove::Right) == VK_NUMPAD3,
		"the keypad's 1 and 3 sideways");
	Check(EditorActions_MoveKey(EditorMove::Up) == VK_SUBTRACT && EditorActions_MoveKey(EditorMove::Down) == VK_ADD,
		"the keypad's minus up and plus down (\"-/+ Move up/down\")");
	Check(EditorActions_MoveKey(EditorMove::TurnLeft) == VK_NUMPAD4 && EditorActions_MoveKey(EditorMove::TurnRight) == VK_NUMPAD6
		&& EditorActions_MoveKey(EditorMove::LookUp) == VK_NUMPAD2 && EditorActions_MoveKey(EditorMove::LookDown) == VK_NUMPAD8,
		"the keypad's 4 and 6 turn, 2 looks up and 8 down (\"2/8 Pitch up/down\")");
	Check(EditorActions_MoveKey(EditorMove::Count) == 0 && EditorActions_MoveKey((EditorMove)200) == 0,
		"a move that isn't one has no key");

	// the keys the Modern (WASD) layout held are freed (Frost 2026-11-25): none of W, S, D, R, F or the arrows is a move's key now
	bool noModernKey = true, noMainRowDigit = true;
	for (size_t m = 0; m < kMoveCount; m++)
	{
		uint8_t const key = EditorActions_MoveKey((EditorMove)m);
		noMainRowDigit = noMainRowDigit && !(key >= '0' && key <= '9');
		noModernKey = noModernKey && key != 'W' && key != 'S' && key != 'D' && key != 'R' && key != 'F' && key != VK_LEFT && key != VK_RIGHT && key != VK_UP && key != VK_DOWN;
	}
	Check(noMainRowDigit, "the camera never takes a main-row digit, which are the draw modes'");
	Check(noModernKey, "no move holds W, S, D, R, F or an arrow (the Modern layout's): only inteng's keys");
}

static void TestRunning()
{
	puts("What each action does:");

	EditorHostState host = {};

	host.running = true;
	EditorActions_Find(kActionQuit)->run(&host);
	Check(!host.running, "Quit stops the editor");

	host.running = true;
	EditorActions_Find(kActionHelpKeys)->run(&host);
	Check(host.running && host.statusWords && strcmp(host.statusWords,
		"inteng's keys, with NumLock on: A forward, Z back, Num 1 left, Num 3 right, Num - up, Num + down, Num 4 turn left, Num 6 turn right, "
		"Num 2 look up, Num 8 look down, Num 7 roll left, Num 9 roll right; the mouse with its right button held turns and looks. Menus: F10 or Alt") == 0,
		"Help, Keys says inteng's keys, made from the table of moves");
	Check(EditorActions_Find(151) == 0 && EditorActions_Find(152) == 0, "the two key layouts' items are gone (the Modern WASD layout removed, 11-25)");

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

	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, 'O', false, true, false) == 0 && open != nullptr, "Ctrl+O is no key any more: inteng's hub has no Open (Bishop 11-16)");
	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, '8', false, false, false) == EditorActions_Find(kActionViewPolyline)
		&& EditorActions_ForKey(kEditorActions, kEditorActionCount, VK_NUMPAD8, false, false, false) == 0,
		"the main row's 8 is Polyline; the keypad's 8 is no action");
	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, 'B', false, false, false) == EditorActions_Find(kActionViewGouraud)
		&& EditorActions_ForKey(kEditorActions, kEditorActionCount, 'B', false, true, false) == EditorActions_Find(kActionViewHeadlight),
		"B is Gouraud and Ctrl+B the headlight");
	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, 'O', false, false, false) == 0
		&& EditorActions_ForKey(kEditorActions, kEditorActionCount, 'O', true, true, false) == 0,
		"a plain O, or Ctrl+Shift+O, is no action: a key's Ctrl and Shift are part of it");
	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, 'O', false, true, true) == 0
		&& EditorActions_ForKey(kEditorActions, kEditorActionCount, 'O', false, false, true) == 0,
		"with Alt held, or AltGr (Ctrl and Alt), no key is an action");
	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, 'V', false, true, false) == 0
		&& EditorActions_ForKey(kEditorActions, kEditorActionCount, 0, false, false, false) == 0
		&& EditorActions_ForKey(0, kEditorActionCount, 'O', false, true, false) == 0, "Ctrl+V, no key, and no table are no action");

	Check(EditorActions_ForKey(kEditorActions, kEditorActionCount, 'A', false, false, false) == 0,
		"a camera key is held, not an action");
}

// An action's key is the one its line holds: the menus and the keys follow it
static void TestTheKeyIsFollowed()
{
	puts("An action's key is its line's:");

	EditorAction other[64];
	memcpy(other, kEditorActions, sizeof(EditorAction) * kEditorActionCount);
	EditorKey const x = { 'X', false, false };
	other[2].key = x;

	Check(EditorActions_CheckTable(other, kEditorActionCount, kEditorMoves, kMoveCount) == 0, "(a table with X on one line is sound)");
	Check(EditorActions_MakeMenus(other, kEditorActionCount, menus, (size_t)EditorMenu::Count, items, 64, words, 64)
		&& KeysAre(other[2].id, "X"), "its menus show X");
	Check(EditorActions_ForKey(other, kEditorActionCount, 'X', false, false, false) == &other[2], "and X does it");
	Check(&EditorActions_KeyIn(other[2]) == &other[2].key, "an action's key is its line's");
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
	Fresh(); table[0].key = ctrlO; table[2].key = ctrlO;   // (the real table's Ctrl+O is gone: the first line is given it here)
	Check(Refused("share a key"), "two actions on one key");
	Fresh(); table[3].key = plainO;
	Check(Sound(), "O and Ctrl+O are two keys");
	Fresh(); table[3].key = x;
	Check(Sound(), "a free key on a line is sound");
	Fresh(); table[3].key = a;
	Check(Refused("camera holds"), "an action on the forward key, A");
	Fresh(); table[3].key = ctrlA;
	Check(Refused("camera holds"), "and on Ctrl+A: a held key counts whatever else is held");
	Fresh(); table[3].key = w;
	Check(Sound(), "W is free: no move holds it (the Modern layout's forward key, gone)");
	Fresh(); table[3].key = ctrlV;
	Check(Refused("Ctrl+V"), "Ctrl+V, the paste's");
	Fresh(); table[3].key = ctrlShiftV;
	Check(Refused("Ctrl+V"), "Ctrl+Shift+V too");
	Fresh(); table[3].key = plainV;
	Check(Sound(), "a plain V is free");
	Fresh(); table[3].key = f5;
	Check(Refused("no name"), "a key the editor has no name for");
	Fresh(); table[1].key = ctrlQ;
	Check(Refused("Windows' keys"), "an action that shows Windows' keys and has a key of its own");

	Fresh(); moves[3].move = EditorMove::Left;
	Check(Refused("out of order"), "a move out of order");
	Fresh(); moves[0].name = "";
	Check(Refused("no words"), "a move with no words");
	Fresh(); moves[1].key = moves[0].key;
	Check(Refused("share a key"), "two moves on one key");
	Fresh(); moves[4].key = 0;
	Check(Refused("no key"), "a move with no key");
	Fresh(); moves[4].key = VK_F5;
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
	TestTheKeyIsFollowed();
	TestTheCheckRefuses();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
