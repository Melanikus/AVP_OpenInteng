// bar_letter.h: M-3 of reassess/bible-check.md (the Bible's MN-3: "Ctrl + the underlined letter opens the menu"). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES. While its bar is up and waiting (FUN_004b7dd0: the pointer at the top, kept while Control is held, reassess/dialog-look-drake.md BAR-3), every key pressed is
// looked up among the bar's titles (FUN_004b7740 at 0x4b7f6e): its code against each title's underlined letter, which the bar kept as a capital when it was made (FUN_004b77e0
// 0x4b788b: a to z less 0x20). A match ends the wait with that title (0x4b7f73 EBX), and the title is opened as a press on it opens it (0x4b7fb0-0x4b7ff8, BAR-4: a title with no
// items gives its own command at once). A letter's held flag is cleared whether it matched or not (0x4b7758), so it does nothing else.
// OURS: the same lookup; a matching letter opens the title through the bar's own press, and is taken. NAMED: a letter that names no title is NOT taken (the screens' own Control
// keys, Texture Map's Ctrl+K and the others, keep working: inteng's loop would eat them while its bar waits); and only the first row's titles are looked up (the bar's own menu
// object, +0x24), as the press on a title is.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "menu_bar.h"

namespace host
{
	// The title of these whose underlined letter the key is (a Windows key code, 'A' to 'Z'; the title's letter in either case), or -1: no such title, or not a letter key
	int32_t BarLetter_Title(ui::Menu const *menus, size_t count, uint32_t key);
}
