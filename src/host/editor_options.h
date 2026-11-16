// editor_options.h: the three switches of Construction's Options menu that inteng keeps in its settings file, and ours in AVP_OpenInteng.ini. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Facts: FUN_004452b0's cases 0x1f4, 0x1f5 and 0x1f6 (dump-all-1.txt:54064-54075) and the settings writer / reader FUN_004732a0 / FUN_00473430
// (dump-all-2.txt:6687-6689, 6812-6820), which keep DAT_00631268, DAT_00636f88 and DAT_00636f84 in inteng.ini (14 numbers in a row, each read only when 4 bytes come back).
//
//   Select by polygons (0x1f4, and Adjacent Modules' 0xca): DAT_00631268. On: a click in Construction takes the object the click's line meets (FUN_00444aa0, the hit test). Off: it takes the object whose CENTRE is
//       nearest the pointer on the screen (FUN_004449f0, FUN_0046ddb0). The same switch picks the module under the pointer in Adjacent Modules (FUN_00405690).
//   Prefer .BMP over .RIM (0x1f5): DAT_00636f88. On: FUN_0046df30 (the picture loader) tries the picture's name with its ending made .BMP before it takes the .RIM.
//   Use Polygon View Distance (0x1f6): DAT_00636f84. FUN_00474ee0, every frame: on, the view's flags get the haze bit (0x100) and its two haze numbers (VDB_H1, VDB_H2: +0x154, +0x158) the view distance
//       (DAT_005fd678); off, the haze bit is cleared.
//
// OURS (named): the file is our own AVP_OpenInteng.ini, [Editor] SelectByPolygons=, PreferBmp=, PolygonViewDistance=; a value is read only when it is exactly 0 or 1 (anything else is refused and the default
// stays: the file is input from outside), and a switch is written only when it is changed. inteng's start value of all three is 0 (its program data is zero where nothing sets it); SelectByPolygons starts ON here,
// because the editor's click has always been the hit test and Melanikus's tests and the other modes rely on it: the difference is named, and the switch works both ways.
#pragma once

#include <stddef.h>

namespace host
{
	struct EditorOptions
	{
		bool selectByPolygons;
		bool preferBmp;
		bool polygonViewDistance;
	};

	// The start values (see above)
	EditorOptions EditorOptions_Defaults();

	// Reads the three keys of [Editor] in the settings file. A key that is missing keeps its default; a key that holds anything but 0 or 1 keeps its default too, and *why names the first such key (null: not wanted).
	// With no file name, the defaults.
	EditorOptions EditorOptions_Load(char const *settingsFile, char const **why);

	// Writes one switch ("SelectByPolygons", "PreferBmp" or "PolygonViewDistance") as 0 or 1. False for another key, no file name, or a write the system refused.
	bool EditorOptions_Save(char const *settingsFile, char const *key, bool on);
}
