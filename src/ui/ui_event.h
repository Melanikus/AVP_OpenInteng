// One thing a person did, as the editor's widgets are told it. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY THIS EXISTS. Every widget has a model, which holds its state and its rules, and a view, which draws it. The models never see the
// window: the window turns each key press and typed letter into one of these, and hands it to the widget that has focus. So the tests
// drive the models with exactly what the window will send, and every rule can be tried without a window.

#pragma once

namespace ui
{
	// Which key. Letter means a typed letter, and the letter is in Event::letter.
	enum class Key
	{
		None,
		Letter,
		Left,
		Right,
		Up,
		Down,
		Home,
		End,
		PageUp,
		PageDown,
		Backspace,
		Delete,
		Enter,
		Escape,
		Tab,
		Space,
		Menu        // the menu bar's key: F10, or Alt pressed and let go alone (src/host/window_events.h)
	};

	struct Event
	{
		Key key;                 // which key, or Letter
		unsigned char letter;    // with Letter: the byte the window was given, from 0 to 255, never yet checked
		bool shift;              // Shift was held
		bool ctrl;               // Ctrl was held. A letter typed with Ctrl is a shortcut, never a letter in the text
	};

	inline Event KeyEvent(Key key, bool shift = false, bool ctrl = false)
	{
		Event const event = { key, 0, shift, ctrl };
		return event;
	}

	inline Event LetterEvent(unsigned char letter, bool ctrl = false)
	{
		Event const event = { Key::Letter, letter, false, ctrl };
		return event;
	}
}
