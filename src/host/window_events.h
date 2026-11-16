// window_events.h: the editor's window messages, turned into the UI kit's events (src/ui/ui_event.h). Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The widgets never see the window: the host hands each event to the widget with focus. This decides which messages are events, and
// makes sure each key press becomes one event, never two.
// - A typed letter comes only from WM_CHAR, as the byte Windows typed (the window is an ANSI one, so it's one byte, 0 to 255). The
//   widgets check every byte themselves: one the font doesn't have is refused there, never here.
// - A key that isn't a letter (the arrows, Home, End, Page Up and Down, Backspace, Delete, Enter, Esc, Tab) comes only from WM_KEYDOWN.
//   Windows also sends some of those as WM_CHAR control codes (Backspace, Tab, Enter, Esc); those are left out, so none counts twice.
// - The space comes from WM_CHAR, as the letter ' ', which every widget that takes Space also takes.
// - With Ctrl held, a letter key comes from WM_KEYDOWN as that letter with Ctrl (Ctrl+A selects everything), and the control code
//   Windows sends for it as WM_CHAR is left out. The host itself takes Ctrl+V, to paste.
// - AltGr is typing, never Ctrl: Windows reports it as Ctrl and Alt held together, so with Alt held too, Ctrl counts as not held. That's
//   how `@ \ { [ ] } |`, and a Portuguese keyboard's AltGr+Q and AltGr+W, reach a text field as letters, and how AltGr and a letter never
//   fires a shortcut. (Apone's review W1, 2026-11-14.)
// - Alt's keys (WM_SYSKEYDOWN, WM_SYSCHAR) are Windows' own, for its menus: never an event.
// - Anything else is no event at all. A message's repeat count is one event: a held key repeats by its own messages.
// Shift, Ctrl and Alt are given by the host, as it reads them when the message comes, so this is plain sums a test can drive.

#pragma once

#include <windows.h>

#include "ui_event.h"

// Turns one window message into an event. Gives back false, with *event untouched, when the message isn't one. F10 (a WM_SYSKEYDOWN) is
// the menu bar's key, Key::Menu.
bool WindowEvents_FromMessage(UINT message, WPARAM wParam, bool shiftHeld, bool ctrlHeld, bool altHeld, ui::Event *event);

// THE MENU BAR'S OTHER KEY: Alt pressed and let go alone, as in most programs, is Key::Menu too. This remembers, between messages,
// whether Alt is down and whether anything else was pressed while it was: Alt with a key (Alt+F4) or a mouse button is no tap. AltGr is
// typing, and Windows sends its Alt as a WM_KEYUP, never a WM_SYSKEYUP, so it's no tap either.
struct AltTap
{
	bool down;
	bool alone;
};

// Notes one message. Gives back true when it ends a lone tap of Alt: the host then hands the menu bar Key::Menu. Focus leaving the window
// forgets the tap, as its WM_SYSKEYUP may never come.
bool WindowEvents_AltTapEnds(AltTap *tap, UINT message, WPARAM wParam);

// Whether the editor swallows a WM_SYSCOMMAND, rather than hand it to Windows: only SC_KEYMENU with lParam 0, which is Alt alone or F10,
// and would open Windows' own menu for the window instead of the editor's bar. Alt+Space (lParam ' ') still opens Windows' menu, so the
// keyboard can still move and close the window, and Alt+F4 is SC_CLOSE, which is never swallowed.
bool WindowEvents_SwallowSysCommand(WPARAM wParam, LPARAM lParam);
