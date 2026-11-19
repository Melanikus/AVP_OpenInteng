// screen_keys.h: the KEYS inteng's screens read themselves (not the menus' items), as a table: which key does which inteng command on which screen. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The keys' sources: inteng's key flags (the exe's key array: the flag of a key is 0xd87a40 plus its code, so 'N' is
// 0xd87a8e), each read from the screen's own handler (AvP-docs\engine\inteng\inteng-menus.md, the "key" column, and the dumps named below); the help texts (inteng-help-keys.md) list more keys than the
// handlers read: a help line with no key flag in its handler is not in the table (it is marked "not in AvP's code" in drake-help-keys.md).
//
//  Adjacent Modules (FUN_00404a20):  N 0x32 Add, D 0x34 Delete, S 0x64 Set Current, 8 0xc8 Polyline, 9 0xc9 Filled Polyline (J and M change the view distance while it is on: the view's own pass).
//  Module Linking (FUN_004c1a90):    H 0x96 Hide, U 0x98 Un-hide all, L 0x9a Un-hide last, S 0xc8 Set Current, Shift+S (the continuous base: kScreenKeyLinkFollow), N 0x32 Normal, B 0x34 Branch, C 0x40 Close
//                                    Branch, O 0x42 Open Branch, D 0x36 Delete, 7 0x48 Filled, 8 0x46 Polyline, 9 0x47 Filled Polyline, 0 0x4a Mixed, V 0x4b Only Draw Linked Modules.
//  Waypoint (FUN_00583d00 / FUN_00584ee0 / FUN_00585750): by the mode of the screen, see the table in screen_keys.cpp.
//  Light Sourcing (FUN_004aab60):    S switches the selected light on, X off (kScreenKeyLightOn / kScreenKeyLightOff).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace host
{
	// Commands that are no menu item's: the editor does them itself
	int32_t const kScreenKeyWaypointDelete = 0x7201;   // Waypoint mode 1, Delete (ours, the owner's addition P4, 11-18): the link selected by mouse (inteng's Delete Link 0x10), else the current waypoint (Delete Waypoint 6)
	int32_t const kScreenKeyLinkFollow = 0x7001;   // Module Linking, Shift+S: the base follows the module the camera is in (DAT_00704384; FUN_004c96f0 every frame)
	int32_t const kScreenKeyLightOn = 0x7101;      // Light Sourcing, S: the selected light on (FUN_004ac240(light, 1))
	int32_t const kScreenKeyLightOff = 0x7102;      // Light Sourcing, X: the selected light off (FUN_004ac240(light, 0))
	int32_t const kScreenKeyLightCopy = 0x7104;        // C: the selected light is the clipboard's
	int32_t const kScreenKeyLightDrop = 0x7105;        // D: a new light at the camera (FUN_004ab860)
	int32_t const kScreenKeyLightPaste = 0x7106;       // V: the clipboard light at the camera (FUN_004ac080)
	int32_t const kScreenKeyLightRemove = 0x7107;      // R: the selected light is taken out
	int32_t const kScreenKeyLightOverwrite = 0x7108;   // O: the clipboard's numbers over the selected light
	int32_t const kScreenKeyLightInvisible = 0x7109;   // I: the selected light's invisible flag flipped
	int32_t const kScreenKeyLightUpdate = 0x710a;      // U: the selected light's changed flag taken out
	int32_t const kScreenKeyLightAttrPrev = 0x710b;    // <: the attribute before
	int32_t const kScreenKeyLightAttrNext = 0x710c;    // >: the attribute after
	int32_t const kScreenKeyLightRelight = 0x710d;     // F: the lighting worked out again (FUN_004fe890)
	int32_t const kScreenKeyLightReturn = 0x710e;      // Return: the write-back of Exit, and the copy saved when it changed (FUN_004aab60 0x4aaf88)
	int32_t const kScreenKeyLightViewMore = 0x7111;    // H (held): the view distance 500000 more every frame (FUN_004aab60 0x4aac17)
	int32_t const kScreenKeyLightViewLess = 0x7112;    // N (held): 500000 less
	int32_t const kScreenKeyLightFly = 0x710f;         // L: fly light (NOT BUILT)
	int32_t const kScreenKeyLightPlacedColour = 0x7110;       // T: a placed-object light's colour edits are its up or its down colour (FUN_004acf30 0x4ad4bf)
	int32_t const kScreenKeyLightPlacedProperties = 0x7113;   // P: a placed-object light's properties (FUN_004b0470)
	int32_t const kScreenKeyLightHeadlight = 0x7103;   // Light Sourcing, B: the headlight on and off (slot 0xd87a82)

	// What a key does on a screen: the command, or 0 for none. table is the screen's inteng table ("0x5c3508"), or "" for a code-built screen, which is given by name ("Waypoint"); mode is the Waypoint
	// screen's mode (0 AI modules, 1 waypoints, 2 extents, 3 linking), -1 elsewhere; key is the key's code (a capital letter, a digit, VK_DELETE 0x2e); shift is whether Shift is held.
	int32_t ScreenKeys_Command(char const *table, char const *name, int mode, uint8_t key, bool shift);

	// The table's rows, for the test and the document: the screen's name, the key, shift, the command, what inteng's handler does
	struct ScreenKeyRow
	{
		char const *table;
		char const *name;
		int mode;
		uint8_t key;
		bool shift;
		int32_t command;
		char const *words;
	};

	size_t ScreenKeys_Count();
	ScreenKeyRow const &ScreenKeys_Row(size_t index);
}
