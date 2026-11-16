// key_layout.h: the editor's two key layouts, and the setting in AVP_OpenInteng.ini that remembers which one the mapper picked. Written
// 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Melanikus asked for two layouts, and that the editor "defaults to classic" (keys-and-modes-2026-11-14.md):
// - Classic inteng: the keys of the old editor, inteng, from its own keys.txt. The camera is on the numeric keypad (with NumLock on), A
//   and Z move forward and back, and the main row's 7, 8 and 9 pick how the level is drawn. The keypad's 7 and the main row's 7 are
//   different keys here, so the editor reads every layout key as the key itself (WM_KEYDOWN's virtual key), never as the letter it types.
// - Modern WASD: W, S, A and D move, R and F go up and down, and the arrows turn and look.
// Which key does what in each is the table of actions' (editor_actions.h). This file is only the setting.
//
// THE SETTING IS INPUT FROM OUTSIDE THE CODE. Anyone can write anything in the .ini. It's read as one line of at most
// kKeyLayoutSettingRoom - 1 letters, and only the two exact words below count, letter for letter. Anything else (no line, an empty one,
// a longer one, another word, other letters or bytes, the right word with something after it) means Classic, and the caller is given the
// reason to log. Nothing read from the line is ever shown, logged or used as anything but those two words. (Windows' own reading,
// GetPrivateProfileString, has already taken off any spaces around the word, and one pair of quotes around it, before the editor sees it.)
//
// Writing it changes only that one line: Windows' own .ini writing (WritePrivateProfileString) keeps every other line of the file as it
// was. The words written are always one of the two below, never anything read.

#pragma once

#include <stddef.h>

enum class KeyLayout
{
	Classic,      // the editor's default
	ModernWasd,
};

// The setting's two words, exactly as the .ini holds them: [Editor] KeyLayout=Classic or KeyLayout=ModernWASD
extern const char kKeyLayoutClassicWord[];
extern const char kKeyLayoutModernWord[];

// How many bytes the setting is read into, with its 0. A line that fills them may have been cut, so it never counts.
size_t const kKeyLayoutSettingRoom = 32;

// The layout a setting's text names. text holds length letters as they were read; room is how many bytes they were read into, so a
// reading that filled it (length room - 1 or more) counts as too long. Gives back null when the text is one of the two words exactly,
// with *layout that one; otherwise the reason, in the editor's own words, with *layout Classic.
const char *KeyLayout_FromSetting(const char *text, size_t length, size_t room, KeyLayout *layout);

// Reads [Editor] KeyLayout= from the settings file at settingsFile, as KeyLayout_FromSetting says. Gives back null, or the reason the
// layout is Classic. With no settingsFile, nothing is read (Windows would read its own win.ini instead).
const char *KeyLayout_Load(const char *settingsFile, KeyLayout *layout);

// Writes [Editor] KeyLayout= into the settings file at settingsFile: the layout's word, and nothing else changed. Gives back false if
// Windows couldn't write it (the file is read-only, say). With no settingsFile, nothing is written (Windows would write its own win.ini).
bool KeyLayout_Save(const char *settingsFile, KeyLayout layout);
