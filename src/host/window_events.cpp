// See window_events.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "window_events.h"

// The key a WM_KEYDOWN's virtual key is, for the keys that aren't letters, or Key::None
static ui::Key KeyFor(WPARAM virtualKey)
{
	switch (virtualKey)
	{
		case VK_LEFT: return ui::Key::Left;
		case VK_RIGHT: return ui::Key::Right;
		case VK_UP: return ui::Key::Up;
		case VK_DOWN: return ui::Key::Down;
		case VK_HOME: return ui::Key::Home;
		case VK_END: return ui::Key::End;
		case VK_PRIOR: return ui::Key::PageUp;
		case VK_NEXT: return ui::Key::PageDown;
		case VK_BACK: return ui::Key::Backspace;
		case VK_DELETE: return ui::Key::Delete;
		case VK_RETURN: return ui::Key::Enter;
		case VK_ESCAPE: return ui::Key::Escape;
		case VK_TAB: return ui::Key::Tab;
		default: return ui::Key::None;
	}
}

bool WindowEvents_FromMessage(UINT message, WPARAM wParam, bool shiftHeld, bool ctrlHeldAsReported, bool altHeld, ui::Event *event)
{
	// AltGr comes as Ctrl and Alt together, and it's typing, so Ctrl with Alt is no Ctrl
	bool const ctrlHeld = ctrlHeldAsReported && !altHeld;

	if (message == WM_KEYDOWN)
	{
		// Ctrl and a letter key: that letter, with Ctrl, in small letters (Windows' virtual keys for letters are the capitals)
		if (ctrlHeld && wParam >= 'A' && wParam <= 'Z')
		{
			*event = ui::LetterEvent((unsigned char)(wParam - 'A' + 'a'), true);
			event->shift = shiftHeld;
			return true;
		}

		ui::Key const key = KeyFor(wParam);

		if (key == ui::Key::None)
		{
			return false;
		}

		*event = ui::KeyEvent(key, shiftHeld, ctrlHeld);
		return true;
	}

	// F10 is no menu key: in inteng it is camera slot 9 (F-keys restore, Shift+F-keys save), and Windows sends it as a system key, which the editor's window handles as the F key it is (Bishop 11-16)

	if (message == WM_CHAR)
	{
		// Only a byte is a letter: an ANSI window's WM_CHAR is one, and anything larger isn't one of its letters. Control codes (below
		// 32, and 127) come with a WM_KEYDOWN already, or are Ctrl's codes for its letters, so they're left out.
		if (wParam > 255 || wParam < 32 || wParam == 127 || ctrlHeld)
		{
			return false;
		}

		*event = ui::LetterEvent((unsigned char)wParam, false);
		event->shift = shiftHeld;
		return true;
	}

	return false;
}

bool WindowEvents_AltTapEnds(AltTap *tap, UINT message, WPARAM wParam)
{
	bool const isAlt = wParam == VK_MENU;

	switch (message)
	{
		case WM_SYSKEYDOWN:
			// Alt's own press starts a tap (its repeats while held change nothing); any other key while Alt is down spoils it
			if (isAlt && !tap->down)
			{
				tap->down = true;
				tap->alone = true;
			}
			else if (!isAlt)
			{
				tap->alone = false;
			}
			return false;

		case WM_KEYDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			tap->alone = false;
			return false;

		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			if (!isAlt)
			{
				return false;
			}

			// Only a system key's letting go is a tap: AltGr's Alt comes back as a WM_KEYUP. (A tap is only ever alone while Alt is down:
			// both start together, and both end together.)
			bool const ends = message == WM_SYSKEYUP && tap->alone;

			tap->down = false;
			tap->alone = false;
			return ends;
		}

		case WM_KILLFOCUS:
			tap->down = false;
			tap->alone = false;
			return false;

		default:
			return false;
	}
}

bool WindowEvents_SwallowSysCommand(WPARAM wParam, LPARAM lParam)
{
	// The low four bits of a system command are Windows' own, so they're left out before it's compared (Microsoft's documentation)
	return (wParam & 0xFFF0) == SC_KEYMENU && lParam == 0;
}
