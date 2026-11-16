// small_modules.h: inteng's Construction > Object > Find Small Modules (case 0x2bd of FUN_004452b0 and FUN_0044be50, out\menu-reads-1-full.txt:6104-6117
// and out\menu-reads-2.txt:868-1110): the search over the level's modules. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES:
//  1. A prompt: "Search for modules with x or z extents smaller than what?(in m)", showing the present limit (DAT_005cf510 times DAT_005ac368). A confirmed
//     number becomes the limit (the typed number converted by __ftol: the factor is not shown) and FUN_0044be50 runs with it.
//  2. FUN_0044be50: the selection is let go of. Then every object that is NOT a placed object (the word at +0x2c, the header's flags, has 0x400 clear) is
//     tested: its shape's extents (two points, the shape's max and min) give a size on each axis, and the object is small when the smaller of its x and z
//     sizes is at most the limit (y is not looked at). The small ones are the new selection and a list for the chooser.
//  3. None: the message "No small module found." Otherwise a chooser titled "Small Modules : Select one to go to" lists their names; the one chosen
//     becomes the current object and the VIEW goes to its place (view place = its place, FUN_00444750 with it).
// This file is step 2's test and the list; the prompt, the chooser, the selection and the view are in editor_main.cpp.
//
// NAMED GAPS (the reads don't show them): the factor between the typed metres and the limit's units (the prompt says metres and a shape's points are in
// millimetres, so a typed number of metres is 1000 times in the limit: INFERRED); the limit's value when the editor starts (DAT_005cf510's start: here 0).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;

namespace host
{
	struct SmallModule
	{
		uint8_t id[8];       // the object's ID
		std::string name;    // its name: what the chooser lists
		int32_t place[3];    // where the view goes
		int32_t size[3];     // its shape's extent on each axis (max - min), in millimetres
	};

	enum class SmallModulesResult
	{
		Done,
		NothingOpen,   // no level is open
	};

	// The limit a typed number of metres makes: whole millimetres, held to 0 to 2^30 (a number the game could never meet is no use)
	int32_t SmallModules_LimitFromMetres(double metres);

	// The number of metres a limit stands for, for the prompt's words
	double SmallModules_MetresOfLimit(int32_t limit);

	// Step 2: every object that isn't a placed object and whose shape's smaller x or z extent is at most `limit`, in the level's order.
	SmallModulesResult SmallModules_Find(OpenLevelFile *file, int32_t limit, std::vector<SmallModule> *found);
}
