// The test for src/host's window messages turned into the UI kit's events. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng
// (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It opens no window: it hands the translator the messages a window would get. Each check's words say what it proves. The program gives
// back a non-zero result if any check failed.

#include <stdio.h>

#include <string>

#include "window_events.h"

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

// What a message becomes: false when it isn't an event, with the event left as it was
static bool FromWithAlt(UINT message, WPARAM wParam, bool shift, bool ctrl, bool alt, Event *event)
{
	*event = KeyEvent(Key::None);
	return WindowEvents_FromMessage(message, wParam, shift, ctrl, alt, event);
}

static bool From(UINT message, WPARAM wParam, bool shift, bool ctrl, Event *event)
{
	return FromWithAlt(message, wParam, shift, ctrl, false, event);
}

static bool IsKey(const Event &event, Key key, bool shift, bool ctrl)
{
	return event.key == key && event.shift == shift && event.ctrl == ctrl;
}

static bool IsLetter(const Event &event, unsigned char letter, bool shift, bool ctrl)
{
	return event.key == Key::Letter && event.letter == letter && event.shift == shift && event.ctrl == ctrl;
}

static void TestKeys()
{
	puts("Keys that aren't letters, from WM_KEYDOWN:");

	struct { WPARAM virtualKey; Key key; const char *name; } const keys[] =
	{
		{ VK_LEFT, Key::Left, "Left" }, { VK_RIGHT, Key::Right, "Right" }, { VK_UP, Key::Up, "Up" }, { VK_DOWN, Key::Down, "Down" },
		{ VK_HOME, Key::Home, "Home" }, { VK_END, Key::End, "End" }, { VK_PRIOR, Key::PageUp, "Page Up" }, { VK_NEXT, Key::PageDown, "Page Down" },
		{ VK_BACK, Key::Backspace, "Backspace" }, { VK_DELETE, Key::Delete, "Delete" }, { VK_RETURN, Key::Enter, "Enter" },
		{ VK_ESCAPE, Key::Escape, "Esc" }, { VK_TAB, Key::Tab, "Tab" },
	};

	Event event;

	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
	{
		Check(From(WM_KEYDOWN, keys[i].virtualKey, false, false, &event) && IsKey(event, keys[i].key, false, false), std::string(keys[i].name) + " is its key");
	}

	Check(From(WM_KEYDOWN, VK_TAB, true, false, &event) && IsKey(event, Key::Tab, true, false), "Shift+Tab is Tab with Shift");
	Check(From(WM_KEYDOWN, VK_RIGHT, true, true, &event) && IsKey(event, Key::Right, true, true), "and Shift and Ctrl go with any key");

	Check(!From(WM_KEYDOWN, VK_SPACE, false, false, &event) && event.key == Key::None, "Space's key press isn't an event: Space comes as the letter ' '");
	Check(!From(WM_KEYDOWN, 'A', false, false, &event) && event.key == Key::None, "a letter key's press isn't an event: the letter comes from WM_CHAR");
	Check(!From(WM_KEYDOWN, VK_F1, false, false, &event) && !From(WM_KEYDOWN, VK_SHIFT, true, false, &event) && !From(WM_KEYDOWN, 0, false, false, &event)
		&& !From(WM_KEYDOWN, 0xFFFF, false, false, &event), "nor are keys the widgets have no use for, Shift alone among them");
	Check(!From(WM_KEYUP, VK_LEFT, false, false, &event) && event.key == Key::None, "a key let go of is no event");
}

static void TestLetters()
{
	puts("Letters, from WM_CHAR:");

	Event event;
	bool everyByte = true;

	for (unsigned letter = 32; letter <= 255; letter++)
	{
		if (letter == 127)
		{
			continue;
		}

		everyByte = everyByte && From(WM_CHAR, letter, false, false, &event) && IsLetter(event, (unsigned char)letter, false, false);
	}

	Check(everyByte, "every byte from 32 to 255, but 127, is that letter, unchecked: the widgets refuse those the font doesn't have");
	Check(From(WM_CHAR, ' ', false, false, &event) && IsLetter(event, ' ', false, false), "the space is the letter ' '");
	Check(From(WM_CHAR, 'Q', true, false, &event) && IsLetter(event, 'Q', true, false), "a capital typed with Shift keeps Shift");

	bool noControl = true;

	for (unsigned code = 0; code < 32; code++)
	{
		noControl = noControl && !From(WM_CHAR, code, false, false, &event);
	}

	Check(noControl && !From(WM_CHAR, 127, false, false, &event), "no control code is a letter: Backspace, Tab, Enter and Esc come from their key presses, so none counts twice");
	Check(!From(WM_CHAR, 256, false, false, &event) && !From(WM_CHAR, 0x263A, false, false, &event) && !From(WM_CHAR, (WPARAM)-1, false, false, &event),
		"nothing larger than a byte is a letter");
}

static void TestCtrl()
{
	puts("Ctrl:");

	Event event;

	Check(From(WM_KEYDOWN, 'A', false, true, &event) && IsLetter(event, 'a', false, true), "Ctrl+A is the letter a with Ctrl, as the text field takes it");
	Check(From(WM_KEYDOWN, 'V', false, true, &event) && IsLetter(event, 'v', false, true), "and Ctrl+V the letter v with Ctrl, which the host takes to paste");
	Check(From(WM_KEYDOWN, 'Z', true, true, &event) && IsLetter(event, 'z', true, true), "the last letter, with Shift too");
	Check(!From(WM_CHAR, 1, false, true, &event) && !From(WM_CHAR, 'a', false, true, &event), "and the code Windows types for it is left out, so Ctrl+A counts once");
	Check(!From(WM_KEYDOWN, '@', false, true, &event) && !From(WM_KEYDOWN, '[', false, true, &event), "the keys just outside A to Z aren't letters with Ctrl");
}

// AltGr comes from Windows as Ctrl and Alt held together (Apone's review W1)
static void TestAltGr()
{
	puts("AltGr:");

	Event event;
	const char *const typed = "@\\{[]}|";
	bool allTyped = true;

	for (const char *at = typed; *at; at++)
	{
		allTyped = allTyped && FromWithAlt(WM_CHAR, (unsigned char)*at, false, true, true, &event) && IsLetter(event, (unsigned char)*at, false, false);
	}

	Check(allTyped, "a letter typed with AltGr, Ctrl and Alt as Windows says, is that letter with no Ctrl: @ \\ { [ ] } | all reach a text field");
	Check(FromWithAlt(WM_CHAR, '/', true, true, true, &event) && IsLetter(event, '/', true, false), "and keeps Shift, as AltGr+Q gives on a Portuguese keyboard");
	Check(!FromWithAlt(WM_KEYDOWN, 'Q', false, true, true, &event) && !FromWithAlt(WM_KEYDOWN, 'W', false, true, true, &event) && event.key == Key::None,
		"AltGr's letter keys aren't Ctrl and a letter, so no shortcut fires: the letter comes from WM_CHAR");
	Check(FromWithAlt(WM_KEYDOWN, VK_LEFT, false, true, true, &event) && IsKey(event, Key::Left, false, false), "a key with AltGr held is that key, with no Ctrl");
	Check(FromWithAlt(WM_KEYDOWN, 'A', false, true, false, &event) && IsLetter(event, 'a', false, true), "Ctrl alone is still Ctrl");
}

static void TestMenuKey()
{
	puts("The menu bar's key:");

	Event event;

	Check(!From(WM_SYSKEYDOWN, VK_F10, false, false, &event) && !From(WM_KEYDOWN, VK_F10, false, false, &event) && !From(WM_SYSKEYDOWN, VK_F9, false, false, &event), "F10 is no menu key, nor is any other F key (inteng's F-keys are camera places)");

	AltTap tap = { false, false };

	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU) && WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "Alt pressed and let go alone is a tap");
	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "and letting go again with no press before it isn't");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	Check(WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "Alt held long enough to repeat is still a tap when it's let go alone");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_F4);
	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "Alt with a key pressed while it's down, Alt+F4 say, is no tap");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_F4);
	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "and Alt's own repeat after the key doesn't make it a tap again");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	WindowEvents_AltTapEnds(&tap, WM_LBUTTONDOWN, 0);
	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "and nor is Alt with a click");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	WindowEvents_AltTapEnds(&tap, WM_KEYDOWN, 'Q');
	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "nor Alt with a key Windows sends as a plain key");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	Check(!WindowEvents_AltTapEnds(&tap, WM_KEYUP, VK_MENU), "AltGr's Alt, which comes back as a plain WM_KEYUP, is no tap: AltGr is typing");

	WindowEvents_AltTapEnds(&tap, WM_SYSKEYDOWN, VK_MENU);
	WindowEvents_AltTapEnds(&tap, WM_KILLFOCUS, 0);
	Check(!WindowEvents_AltTapEnds(&tap, WM_SYSKEYUP, VK_MENU), "a tap that focus left in the middle of is forgotten");

	Check(WindowEvents_SwallowSysCommand(SC_KEYMENU, 0) && WindowEvents_SwallowSysCommand(SC_KEYMENU | 0x3, 0),
		"Windows' own key menu, for Alt alone or F10, is swallowed, whatever its low four bits say");
	Check(!WindowEvents_SwallowSysCommand(SC_KEYMENU, ' '), "Alt+Space isn't: Windows' menu for the window still opens, so the keyboard can move and close it");
	Check(!WindowEvents_SwallowSysCommand(SC_CLOSE, 0) && !WindowEvents_SwallowSysCommand(SC_MOVE, 0) && !WindowEvents_SwallowSysCommand(SC_KEYMENU, 'f'),
		"and nor is Alt+F4's close, or any other command, or Alt and a letter");
}

static void TestNotEvents()
{
	puts("Other messages:");

	Event event;

	Check(!From(WM_SYSKEYDOWN, VK_LEFT, false, false, &event) && !From(WM_SYSCHAR, 'a', false, false, &event), "Alt's keys are Windows' own, never an event");
	Check(!From(WM_LBUTTONDOWN, 0, false, false, &event) && !From(WM_MOUSEMOVE, 0, false, false, &event) && !From(WM_PAINT, 0, false, false, &event),
		"the mouse and painting aren't key events: the host handles them");
	Check(!From(WM_DEADCHAR, 'a', false, false, &event) && event.key == Key::None, "and a dead key's message is no letter, so an accent is typed once, with its letter");
}

int main()
{
	TestKeys();
	TestLetters();
	TestCtrl();
	TestAltGr();
	TestMenuKey();
	TestNotEvents();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
