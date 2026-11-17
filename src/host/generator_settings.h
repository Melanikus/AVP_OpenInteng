// generator_settings.h: edobtype.dll's "Generator Settings" window (resource 133; export edit_generator 0x100015f0) as numbers: what its controls show for a generator, and what OK makes of what is
// typed, in the DLL's own order with MFC's own words for a refusal. The window itself (the layout of resource 133) is drawn by the editor (editor_main.cpp GeneratorSettingsWindowShow).
// Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng (A-8, reassess/object-placement.md OP-131; edobtype-dialogs.md section 2). AVP_OpenInteng's own code: see License.txt.
//
// WHAT THE DLL DOES (re-read 11-16 on edobtype-dump-1 / edobtype-listing-1):
// - The constructor 0x10001ed0 copies the exe's struct (the generator's flags, its 15 weights, the two rates, Max NPCs) into the window's members; the flags get 0x8000000 (the advanced generator) at once;
//   the two rates are shown x 0.01 as floats ((float)(int)rate * 0.01, 0x1001a488); the boxes: Marine 0x800, Alien 0x1000, Predator 0x2000, Starts inactive 0x80000000, Multiplayer start 0x40000000 set; Easy /
//   Medium / Hard ticked when their NOT bits 0x100000 / 0x200000 / 0x400000 are clear; "Use global generator times" ticked when 0x4000000 is clear; "Use own max npc setting" ticked when 0x2000000 is set.
// - DoDataExchange 0x100020f0 reads the controls in ITS order (the members' order, not the window's): Alien 1039, Civ Flamer 1035, Civ Molotov 1038, Civ Pistol 1034, Civ Shotgun 1033, Civ Unarmed 1036,
//   Marine Flamer 1028, Grenade 1031, Minigun 1032, Pulse Rifle 1027, Sadar 1030, Smartgun 1029 (each DDX_Text int, DDV_MinMaxInt 0 .. 1000), the Alien / Marine / Predator boxes, Praetorian 1041, Pred Alien
//   1040 (0 .. 1000), the two boxes 1047 / 1048, the rates 1042 / 1043 (DDX_Text float, DDV_MinMaxFloat 0 .. 50), Marine Pistol 1049 (0 .. 1000), Max NPCs 1051 (DDX_Text BYTE, DDV 0 .. 255), the boxes
//   1052 / 1059, Easy 1009, Hard 1011, Medium 1010. The FIRST control that fails says MFC's words (the DLL's string table: 61712 "Please enter an integer.", 61713 "Please enter a number.", 61714 "Please enter
//   an integer between %1 and %2.", 61715 "Please enter a number between %1 and %2.", 61718 "Please enter an integer between 0 and 255.") and the window stays.
// - OK 0x10002410 writes the members back: the weights, the rates ftol(float x 100.0) (0x1001a560), Max NPCs, and the flags from the boxes.
// INTENG BUGS, FIXED (MAPPING-ORDERS section 15; named):
// - OK writes "Use global generator times" on 0x2000000 and "Use own max npc setting" on 0x4000000, the other way round from what the constructor read (0x1000257d / 0x1000259f against 0x10002054 /
//   0x10002060): every OK swapped the two settings. Ours reads and writes each on the bit it was read from (Melanikus 11-15, unfinished-in-inteng.md).
// - OK truncates the rate (ftol(0.29f x 100) = 28): a typed rate came back one hundredth lower. Ours rounds to the nearest hundredth.
#pragma once

#include <stdint.h>

#include <string>

#include "special_objects_edit.h"

namespace host
{
	// What the window's controls hold: the edits' texts and the boxes' ticks, by the controls they are
	struct GeneratorSettingsTyped
	{
		std::string weights[15];   // in SpecialObject::weights' order: Pulse Rifle 1027, Flamer 1028, Smartgun 1029, Sadar 1030, Grenade 1031, Minigun 1032, Civ Shotgun 1033, Civ Pistol 1034,
		                           // Civ Flamer 1035, Civ Unarmed 1036, Civ Molotov 1038, Alien 1039, Pred Alien 1040, Praetorian 1041, Marine Pistol 1049
		std::string rate;          // 1042 Initial generation rate per minute
		std::string increase;      // 1043 Increase in rate per minute
		std::string maxNpcs;       // 1051 Max NPCs
		bool useGlobalTimes = false;    // 1047
		bool startsInactive = false;    // 1048
		bool marine = false, alien = false, predator = false;   // 1044, 1045, 1046
		bool useOwnMax = false;         // 1052
		bool multiplayerStart = false;  // 1059
		bool easy = false, medium = false, hard = false;         // 1009, 1010, 1011
	};

	// The window as the DLL fills it for this generator (the constructor and DDX's show direction: ints "%d", the rates as MFC's float text "%.*g" with 6 digits, Max NPCs "%u")
	GeneratorSettingsTyped GeneratorSettings_Shown(SpecialObject const &generator);

	// OK: the controls read in DoDataExchange's order. A refusal gives false, MFC's words in *message and the refused control's number in *control (the window stays as typed). Else true and *generator
	// takes the values (weights, the two rates, Max NPCs, the flags with 0x8000000; hasSettings, as the save then writes a GENEXSET).
	bool GeneratorSettings_Ok(GeneratorSettingsTyped const &typed, SpecialObject *generator, std::string *message, int32_t *control);

	// MFC's own readers, as DDX_Text uses them: _AfxSimpleScanf "%d" / "%u" (spaces and tabs around one number strtol / strtoul reads whole; a 0 read when the first letter is not '0' is no number; "%u" takes no
	// '-') and _AfxSimpleFloatParse; of an edit's text MFC reads the first 63 letters for a whole number (DDX_TextWithFormat's szT[64]) and the first 31 for a float (_AfxTextFloatFormat's szBuffer[32])
	bool GeneratorSettings_ScanInt(std::string const &text, bool unsignedForm, long *value);
	bool GeneratorSettings_ScanFloat(std::string const &text, double *value);
}
