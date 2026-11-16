// module_overlaps.h: Construction's Miscellaneous > Module Overlaps (command 0x56): the modules whose boxes overlap, drawn in the colour of how much. Written 2026-11-15 by Melanikus (Hicks) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_004476d0 / FUN_00445110 / FUN_00445070 / FUN_00444c80, dump-all-1.txt:53520; AvP-docs\engine\inteng\apone-reads-4.md): on entering the mode every module is drawn in the plain colour (24-bit 0xFFFFFF), then
// every object that is not hidden, has a shape and is NOT a placed object (class flag 0x400 clear: a module) is compared with every other such object. For two of them the box of each (the shape's extents, six numbers, plus
// the object's place, each part divided by 100 as a whole number first) is worked out and the OVERLAP on each axis is min(max of the two) - max(min of the two); the smallest of the three is o. With o above 0 the OTHER
// object's polygons are given a colour by o, a worse colour never giving way to a better one:
//     o above 1000  -> red     0xFF0000  (the biggest overlap)
//     101 to 1000   -> purple  0x7F007F
//     3 to 100      -> orange  0xFF7E00
//     1 or 2        -> yellow  0xFFFF00  (the modules only touch)
// Leaving the mode puts the normal view back. (The 24-bit colours are the ones the palette masks of 0x444f33..0x445042 work out to: plain 0xf8 + 0xffff07, 0xfc + 0xfffe04, 0xfb + 0xff7e05, 0xff + 0x7eff80, 0xfa + 0xfeff06.)
//
// NAMED DEVIATIONS: the view draws a module's colour as a TINT over its textures (the same call Module Linking uses), not as the plain colour of every polygon, and a module that overlaps nothing is left as it is
// (inteng draws it plain white); at most kLevelViewMostSelected modules are tinted; a hidden object and the current module depth (DAT_008f588c) are not looked at (every module of the level is compared).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class OpenLevelFile;

namespace host
{
	// One module's box: the shape's extents as the file holds them (max x, min x, max y, min y, max z, min z) and the object's place
	struct ModuleBox
	{
		int32_t objectIndex;   // the object's own number in the level (index_num), which the game's module number is looked up by (level_chunks.h GameModuleNumbers)
		int32_t extents[6];
		int32_t place[3];
	};

	enum class OverlapSeverity
	{
		None = 0,
		Touching,   // o is 1 or 2
		Small,      // 3 to 100
		Medium,     // 101 to 1000
		Large,      // above 1000
	};

	// How much two boxes overlap (the three axes must all overlap by more than 0; the smallest overlap decides)
	OverlapSeverity ModuleOverlaps_Between(ModuleBox const &a, ModuleBox const &b);

	// The worst overlap each box has with any other (the order of the boxes does not matter: a worse colour is never replaced by a better one)
	std::vector<OverlapSeverity> ModuleOverlaps_Worst(std::vector<ModuleBox> const &boxes);

	// The colour of a severity as 0xRRGGBB (0 for None)
	uint32_t ModuleOverlaps_Rgb(OverlapSeverity severity);

	// The boxes of the open level's modules: every object that has a shape and is not a placed object. False when no level is open or one shape could not be read (then *boxes holds what was read).
	bool ModuleOverlaps_ReadBoxes(OpenLevelFile *file, std::vector<ModuleBox> *boxes);
}
