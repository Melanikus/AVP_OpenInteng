// See editor_actions.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". (Modified 2026-11-14 by Melanikus (Dietrich), F3: the two key layouts. Modified 2026-11-25 by Melanikus
// (Frost): one layout again, inteng's: the Modern WASD layout's code is gone, editor_actions.h.)

#include "editor_actions.h"
#include "view_toggles.h"

#include <windows.h>   // only for the keys' numbers, VK_...
#include <stdio.h>
#include <string.h>

const char kEditorAboutWords[] = "AVP_OpenInteng: the map editor for Aliens versus Predator, drawn with the game's own engine (AvPx)";
const char kEditorOpenNotYetWords[] = "Opening another level isn't here yet: the editor opens the level AVP_OpenInteng.ini names";

static void OpenLevel(EditorHostState *host)
{
	// Hicks's side puts the level opening here. Until then, the mapper is told what the editor does instead.
	host->statusWords = kEditorOpenNotYetWords;
}

// Save a copy: the loop does it (it holds the open level and the editor's folders), and says where it went
static void SaveCopy(EditorHostState *host)
{
	host->saveCopy = true;
}

// Level settings: the loop shows them in the field panel (it holds the open level)
static void LevelSettings(EditorHostState *host)
{
	host->levelSettings = true;
}

static void Quit(EditorHostState *host)
{
	host->running = false;
}

// The View menu's items (Frost's, view_toggles.h, built to view-state-table.md): each uses its item, and the status line says what's on
// or off now. (Modified 2026-11-14 by Melanikus (Frost): inteng's own draw modes and Gouraud switch.)
static void ViewPolygon(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::Polygon);
}

static void ViewPolyline(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::Polyline);
}

static void ViewFilledPolyline(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::FilledPolyline);
}

static void ViewWireTextured(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::WireTextured);
}

static void ViewGouraud(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::GouraudSwitch);
}

static void ViewHeadlight(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::Headlight);
}

// inteng's Gouraud switch has three states (Vasquez's read of DAT_00914d6c): 0 textures unshaded, 1 (command 0xcd) every textured polygon shaded, 2 (command
// 0xcc) only the polygons that were gouraud in the level stay shaded and the rest are drawn as in 0. The Gouraud item above is states 0 and 1 (no B, B); this is
// state 2, in the menu so it is not forgotten, off until it is built.
static void ViewGouraudLevelOnly(EditorHostState *host)
{
	host->statusWords = "Gouraud, the level's own only (inteng's third state, command 0xcc): not built yet";
}

static void ViewTextures(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::Textures);
}

static void ViewBackFaces(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::BackFaces);
}

static void ViewBrighter(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::Brighter);
}

static void ViewDarker(EditorHostState *host)
{
	host->statusWords = ViewToggles_UseNow(host::ViewToggle::Darker);
}

// The object list, over the level while it is open: the loop opens it, or closes it if it is open (L-1)
static void ViewObjectList(EditorHostState *host)
{
	host->toggleObjectList = true;
}

// A new name for the selected object: the loop opens its prompt (M-1)
static void ViewNameObject(EditorHostState *host)
{
	host->prompt = kPromptName;
}

// Help, Keys: the camera's keys, made from the table of moves
static void HelpKeys(EditorHostState *host)
{
	host->statusWords = EditorActions_KeysWords();
}

static void HelpAbout(EditorHostState *host)
{
	host->statusWords = kEditorAboutWords;
}

// THE TABLE. A line here is a command and its menu item. Frost's View draw modes and toggles go in the View range (101 on; 151 and 152, the
// key layouts' two, are RETIRED with the Modern layout); the editor's modes go in the Mode range (201 on). A key is { the key, Ctrl held, Shift held }: { 'O', true } is
// Ctrl+O. A line no key does is { 0 }, and is only in its menu. The draw modes' keys are inteng's own (keys.txt and the view state table,
// AvP-docs\engine\inteng\view-state-table.md, 6a): the main row's 7 Polygon, 8 Polyline and 9 Filled polyline, B the Gouraud switch and
// Ctrl+B the headlight. They're the same in Modern WASD, where none of them is a camera key.
const EditorAction kEditorActions[] =
{
	// number                  menu              words                  key            Windows' keys  what it does
	{ kActionOpenLevel,          EditorMenu::File, "Open level...",       { 0 },         0,        OpenLevel },
	{ kActionQuit,               EditorMenu::File, "Quit",                { 0 },         "Alt+F4", Quit },
	{ kActionSaveCopy,           EditorMenu::File, "Save a copy",         { 0 },         0,        SaveCopy },
	{ kActionViewPolygon,        EditorMenu::View, "Polygon",             { '7' },       0,        ViewPolygon },
	{ kActionViewPolyline,       EditorMenu::View, "Polyline",            { '8' },       0,        ViewPolyline },
	{ kActionViewFilledPolyline, EditorMenu::View, "Filled polyline",     { '9' },       0,        ViewFilledPolyline },
	{ kActionViewWireTextured,   EditorMenu::View, "Wireframe, textured", { 0 },         0,        ViewWireTextured },
	{ kActionViewGouraud,        EditorMenu::View, "Gouraud",             { 'B' },       0,        ViewGouraud },
	{ kActionViewHeadlight,      EditorMenu::View, "Headlight",           { 'B', true }, 0,        ViewHeadlight },
	{ kActionViewBackFaces,      EditorMenu::View, "Back faces",          { 0 },         0,        ViewBackFaces },
	{ kActionViewBrighter,       EditorMenu::View, "Brighter",            { 0 },         0,        ViewBrighter },
	{ kActionViewDarker,         EditorMenu::View, "Darker",              { 0 },         0,        ViewDarker },
	{ kActionViewObjectList,     EditorMenu::View, "Object list",          { 0 },         0,        ViewObjectList },
	{ kActionViewNameObject,     EditorMenu::View, "Name object...",       { 0 },         0,        ViewNameObject },
	{ kActionViewTextures,       EditorMenu::View, "Textures",            { 0 },         0,        ViewTextures },
	{ kActionViewGouraudLevelOnly, EditorMenu::View, "Gouraud: level's own only [wip]", { 0 },         0,        ViewGouraudLevelOnly },
	{ kActionLevelSettings,      EditorMenu::Mode, "Level settings",     { 0 },         0,        LevelSettings },
	{ kActionHelpKeys,           EditorMenu::Help, "Keys",                { 0 },         0,        HelpKeys },
	{ kActionHelpAbout,          EditorMenu::Help, "About",               { 0 },         0,        HelpAbout },
};

const size_t kEditorActionCount = sizeof(kEditorActions) / sizeof(kEditorActions[0]);

// THE CAMERA'S KEYS, held down to move. Classic is inteng's own keys.txt, "Movement" ("NumLock must be on for the keys to work"): "2/8
// Pitch up/down", "4/6 Turn left/right", "1/3 Move left/right", "-/+ Move up/down (+,- on numeric keypad)", "A Move forwards", "Z Move
// backwards". So the keypad's minus goes up and its plus goes down. inteng's other movement keys aren't here yet: 7/9 roll, / held to
// turn the level instead of the view, Shift+A and Shift+Z to speed up and slow down, ] and [ for the speed, Space to stop, and the
// F-keys' camera places. Modern is the editor's own: W S A D, R and F, and the arrows.
const EditorMoveKeys kEditorMoves[] =
{
	// move                  words         key
	{ EditorMove::Forward,   "forward",    'A' },
	{ EditorMove::Back,      "back",       'Z' },
	{ EditorMove::Left,      "left",       VK_NUMPAD1 },
	{ EditorMove::Right,     "right",      VK_NUMPAD3 },
	{ EditorMove::Up,        "up",         VK_SUBTRACT },
	{ EditorMove::Down,      "down",       VK_ADD },
	{ EditorMove::TurnLeft,  "turn left",  VK_NUMPAD4 },
	{ EditorMove::TurnRight, "turn right", VK_NUMPAD6 },
	{ EditorMove::LookUp,    "look up",    VK_NUMPAD2 },
	{ EditorMove::LookDown,  "look down",  VK_NUMPAD8 },
	{ EditorMove::RollLeft,  "roll left",  VK_NUMPAD7 },
	{ EditorMove::RollRight, "roll right", VK_NUMPAD9 },
};

static_assert(sizeof(kEditorMoves) / sizeof(kEditorMoves[0]) == (size_t)EditorMove::Count, "the camera's keys have one line for each move");

// The menus' titles, one for each EditorMenu, in its order
static const char *const kMenuTitles[(size_t)EditorMenu::Count] = { "File", "View", "Mode", "Help" };

// A key's own name, as the menu and Help, Keys show it, into name, which has room for the longest ("Right", "Num 9"): only the keys the
// editor's tables can use. Gives back false for a key with no name here.
static bool KeyName(uint8_t key, char *name, size_t size)
{
	int written = -1;

	if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
	{
		written = snprintf(name, size, "%c", (char)key);
	}
	else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
	{
		written = snprintf(name, size, "Num %d", key - VK_NUMPAD0);
	}
	else if (key == VK_ADD || key == VK_SUBTRACT)
	{
		written = snprintf(name, size, "Num %c", key == VK_ADD ? '+' : '-');
	}
	else if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN)
	{
		written = snprintf(name, size, "%s", key == VK_LEFT ? "Left" : key == VK_RIGHT ? "Right" : key == VK_UP ? "Up" : "Down");
	}

	return written > 0;
}

bool EditorActions_KeyWords(const EditorKey &key, char *words, size_t size)
{
	if (!words || size == 0)
	{
		return false;
	}

	words[0] = '\0';

	char name[16];

	if (key.virtualKey == 0 || !KeyName(key.virtualKey, name, sizeof(name)))
	{
		return false;
	}

	int const written = snprintf(words, size, "%s%s%s", key.ctrl ? "Ctrl+" : "", key.shift ? "Shift+" : "", name);

	if (written <= 0 || (size_t)written >= size)
	{
		words[0] = '\0';
		return false;
	}

	return true;
}

const EditorKey &EditorActions_KeyIn(const EditorAction &action)
{
	return action.key;
}

uint8_t EditorActions_MoveKey(EditorMove move)
{
	// The table is in EditorMove's order (EditorActions_CheckTable proves it), so a move's line is its number
	return (size_t)move < (size_t)EditorMove::Count ? kEditorMoves[(size_t)move].key : 0;
}

const char *EditorActions_KeysWords()
{
	// Long enough for every move's key and words; anything longer would only be cut, never written past the end
	static char words[320];

	words[0] = '\0';
	strncat_s(words, sizeof(words), "inteng's keys, with NumLock on:", _TRUNCATE);

	for (size_t m = 0; m < (size_t)EditorMove::Count; m++)
	{
		char key[kEditorKeyWordsRoom];
		EditorKey const held = { kEditorMoves[m].key, false, false };

		if (EditorActions_KeyWords(held, key, sizeof(key)))
		{
			strncat_s(words, sizeof(words), m == 0 ? " " : ", ", _TRUNCATE);
			strncat_s(words, sizeof(words), key, _TRUNCATE);
			strncat_s(words, sizeof(words), " ", _TRUNCATE);
			strncat_s(words, sizeof(words), kEditorMoves[m].name, _TRUNCATE);
		}
	}

	strncat_s(words, sizeof(words), "; the mouse with its right button held turns and looks. Menus: F10 or Alt", _TRUNCATE);
	return words;
}

const char *EditorActions_CheckTable(const EditorAction *actions, size_t actionCount, const EditorMoveKeys *moves, size_t moveCount)
{
	char words[kEditorKeyWordsRoom];

	if (!actions || actionCount == 0)
	{
		return "the table of actions is empty";
	}

	if (!moves || moveCount != (size_t)EditorMove::Count)
	{
		return "the camera's keys don't have one line for each move";
	}

	for (size_t m = 0; m < moveCount; m++)
	{
		if (moves[m].move != (EditorMove)m || !moves[m].name || !moves[m].name[0])
		{
			return "a line of the camera's keys is out of order, or has no words";
		}

		EditorKey const held = { moves[m].key, false, false };

		if (!EditorActions_KeyWords(held, words, sizeof(words)))
		{
			return "a move of the camera has no key, or one the editor has no name for";
		}

		for (size_t n = 0; n < m; n++)
		{
			if (moves[n].key == held.virtualKey)
			{
				return "two of the camera's moves share a key";
			}
		}
	}

	for (size_t a = 0; a < actionCount; a++)
	{
		EditorAction const &action = actions[a];

		if (action.id <= 0)
		{
			return "an action's number isn't above 0";
		}

		if ((size_t)action.menu >= (size_t)EditorMenu::Count)
		{
			return "an action isn't in one of the four menus";
		}

		if (!action.name || !action.name[0] || !action.run)
		{
			return "an action has no words, or does nothing";
		}

		for (size_t b = 0; b < a; b++)
		{
			if (actions[b].id == action.id)
			{
				return "two actions share a number";
			}
		}

		EditorKey const &key = action.key;

		if (key.virtualKey == 0)
		{
			continue;
		}

		if (action.windowsKeys)
		{
			return "an action shows Windows' keys and has a key of its own as well";
		}

		if (!EditorActions_KeyWords(key, words, sizeof(words)))
		{
			return "an action's key is one the editor has no name for";
		}

		if (key.virtualKey == 'V' && key.ctrl)
		{
			return "an action takes Ctrl+V, which stays the paste's";
		}

		for (size_t m = 0; m < moveCount; m++)
		{
			if (moves[m].key == key.virtualKey)
			{
				return "an action takes a key the camera holds";
			}
		}

		for (size_t b = 0; b < a; b++)
		{
			EditorKey const &other = actions[b].key;

			if (other.virtualKey == key.virtualKey && other.ctrl == key.ctrl && other.shift == key.shift)
			{
				return "two actions share a key";
			}
		}
	}

	return 0;
}

bool EditorActions_MakeMenus(const EditorAction *actions, size_t actionCount, ui::Menu *menus, size_t menuRoom,
	ui::MenuItem *items, size_t itemRoom, EditorKeyWords *words, size_t wordsRoom)
{
	size_t const menuCount = (size_t)EditorMenu::Count;

	if (!actions || !menus || !items || !words || menuRoom < menuCount || itemRoom < actionCount || wordsRoom < actionCount)
	{
		return false;
	}

	size_t used = 0;

	for (size_t m = 0; m < menuCount; m++)
	{
		menus[m].title = kMenuTitles[m];
		menus[m].items = items + used;
		menus[m].count = 0;

		for (size_t a = 0; a < actionCount; a++)
		{
			if ((size_t)actions[a].menu == m)
			{
				// The words beside the item: its key's, or else the keys Windows does for it, or none
				bool const hasKey = EditorActions_KeyWords(actions[a].key, words[used].text, sizeof(words[used].text));

				items[used].name = actions[a].name;
				items[used].keys = hasKey ? words[used].text : actions[a].windowsKeys;
				items[used].action = actions[a].id;
				items[used].tag = 0;   // the editor's own items have none (an item array may come from the stack)
				used++;
				menus[m].count++;
			}
		}
	}

	return true;
}

const EditorAction *EditorActions_Find(int32_t id)
{
	for (size_t a = 0; a < kEditorActionCount; a++)
	{
		if (kEditorActions[a].id == id)
		{
			return &kEditorActions[a];
		}
	}

	return 0;
}

const EditorAction *EditorActions_ForKey(const EditorAction *actions, size_t actionCount, uint8_t virtualKey, bool shiftHeld,
	bool ctrlHeld, bool altHeld)
{
	if (!actions || altHeld || virtualKey == 0)
	{
		return 0;
	}

	for (size_t a = 0; a < actionCount; a++)
	{
		EditorKey const &key = actions[a].key;

		if (key.virtualKey == virtualKey && key.ctrl == ctrlHeld && key.shift == shiftHeld)
		{
			return &actions[a];
		}
	}

	return 0;
}
