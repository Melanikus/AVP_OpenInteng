// key_reader.h: whether a key is held now, for the editor's own key reads (Shift, Ctrl, Alt, Delete, + and -). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A key is held when the editor's own table of keys says so (the key messages the window gets fill it, the posted ones of a scenario too), or, ONLY when no scenario drives the
// editor, when the real keyboard says so (GetKeyState). While a scenario drives the editor (a -picture run: nobody_watching.h) the real keyboard is not read, so a person
// using the computer meanwhile (a Shift held for a screenshot) cannot change what the scenario's keys do (Bishop 2026-11-16). Outside a scenario nothing changes: the real
// keyboard is read as inteng reads it.
#pragma once

namespace host
{
	// The real keyboard's answer for a key (GetKeyState's high bit by default); a test puts its own here. Null puts the default back.
	void KeyReader_SetRealReader(bool (*reader)(int vk));

	// table: the editor's 256 entries, true while that key's message said down; scenarioDriven: a scenario drives the editor now
	bool KeyReader_Held(int vk, bool const *table, bool scenarioDriven);
}
