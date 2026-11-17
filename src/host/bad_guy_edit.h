// bad_guy_edit.h: edobtype.dll's "Edit object properties" window for a BAD GUY (resource 129 in the AvP mode, DAT_10022638 = 1; export edit_avp_bad_guy 0x10001280, class 0x10003cd0) as numbers:
// what its controls show for each type, and what OK makes of them. The window itself is drawn by the editor (editor_main.cpp BadGuyWindowShow).
// Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng (A-8, reassess/object-placement.md OP-132; edobtype-dialogs.md section 3). AVP_OpenInteng's own code: see License.txt.
//
// WHAT THE DLL SHOWS (OnInitDialog 0x10003de0, the type's controls FUN_10005400; the tables read from his edobtype.dll):
// - "Type of Object" (1016) over the type combo 1002 (table 0x1001fc28, item data = the type): Alien 1, Marine 7, Predator 2, Facehugger 3, Alien Queen 6, Xenoborg 4, Predator-Alien 5, Pretorian 8,
//   Sentry Gun 9; the pick is the bad guy's type.
// - by the type: Predator (2): "Primary" (1004) over combo 1001 and "Secondary" (1017) over combo 1018, each Pistol 0, Wristblade 1, Plasma Caster 2, Staff 3 (0x10020228), picked from the subtype's
//   bits 0-2 and 3-5. Xenoborg (4): "Active Time 0=default" over edit 1003 (extra 1 x 5) and "Module Range - 0=default" over edit 1020 (extra 2). Sentry Gun (9): "Ammo (30000+ =infinite)" over edit
//   1003 (extra 1 x 256 + extra 2). Any other: "Subtype" over combo 1001 with the type's list (0x1001fbd8 + type x 0xa0: one entry worth 0, the Marine's 18 entries), the subtype picked.
// - "Texturing ID" (1019) over combo 1022: "Random" 0 alone and disabled, except for a Marine: "Random" 0, "Original" 1 and the lines of hnpcmarine.txt / hnpc_civvie.txt (FUN_100063d0, read where the exe's
//   string says; no install holds those files, NAMED: the two entries); the texture picked.
// - "Starts Inactive" (1008, FUN_100059f0) ticked by 0x80000000. OK and Cancel.
// - HIDDEN AS inteng (Bishop's ruling 11-16, a choice): the Game mode boxes 1005-1007 and the Difficulty boxes 1009-1011 (resource style 0x48010003, OnInitDialog 0x10003de0 never shows them); their
//   values pass through DoDataExchange 0x10003d20 unchanged. The explosion buttons, Mass, Integrity, Female, Child: hidden (the simple object's).
// - A pick in the type combo shows that type's controls at once (FUN_10005400 with the new type; the subtype keeps its number, picked when the new list has it).
// WHAT OK DOES (FUN_10005de0, DDX 0x10003d20, the exe's copy back 0x4e5db0): the texture = the picked row's number (none picked: kept); Predator: subtype = (secondary & 7) << 3 | (primary & 7) (none picked:
// 0); Xenoborg: extra 1 = the Active Time / 5, at most 255, extra 2 = the Module Range, at most 255; Sentry Gun: the Ammo, at most 30000, extra 2 its low byte, extra 1 the rest; any other: subtype = the
// picked row's number (none picked: kept); "Starts Inactive" sets / clears 0x80000000; every other flag as it was.
// INTENG BUGS, FIXED (MAPPING-ORDERS section 15; named): the three numbers are read by sscanf "%d" straight into the bytes' globals, so text that is no number left the OLD byte there and it was divided by 5
// (Active Time) or split as an ammo count (Sentry Gun): ours keeps the old value when nothing is read. A number below 0 was stored as a wrapped byte (-5 seconds became 1275): ours takes it as 0.
#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "special_objects_edit.h"

namespace host
{
	typedef std::vector<std::pair<std::string, int32_t> > BadGuyChoices;   // a combo's rows: the words and the number each stands for

	// What the window shows for a type (the bad guy's own numbers in it)
	struct BadGuyControls
	{
		BadGuyChoices types;                 // combo 1002
		int32_t typePick = -1;               // its picked row (-1: none)
		std::string leftLabel;               // 1004: "Subtype", "Primary", "Active Time 0=default", "Ammo (30000+ =infinite)"
		std::string rightLabel;              // 1017: "Secondary", "Module Range - 0=default", or empty (hidden)
		bool leftCombo = false, rightCombo = false, leftEdit = false, rightEdit = false;   // which of 1001 / 1018 / 1003 / 1020 are shown
		BadGuyChoices leftChoices, rightChoices;
		int32_t leftPick = -1, rightPick = -1;
		std::string leftText, rightText;     // the edits' texts ("%d")
		BadGuyChoices textures;              // combo 1022
		int32_t texturePick = -1;
		bool textureLive = false;            // 1022 enabled (a Marine)
		bool startsInactive = false;         // 1008
	};

	// The controls for the bad guy as it is, shown as `type` (its own type when the window opens, another after a pick in the type combo)
	BadGuyControls BadGuy_Controls(SpecialObject const &badGuy, int32_t type);

	// What the window holds at OK
	struct BadGuyTyped
	{
		int32_t type = 0;            // the type combo's picked number (the bad guy's own when nothing is picked)
		int32_t leftPick = -1, rightPick = -1, texturePick = -1;   // the picked rows' numbers (-1: none)
		std::string leftText, rightText;
		bool startsInactive = false;
	};

	// OK: the bad guy with what the window holds (the hidden boxes' flags unchanged)
	SpecialObject BadGuy_Ok(SpecialObject const &badGuy, BadGuyTyped const &typed);
}
