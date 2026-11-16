// autolink_pass.h: AutoLink's visibility pass for one module, in inteng's METHOD with OUR renderer (owner-intent item 15: a NAMED DEVIATION): the sample views of FUN_004c7ab0 drawn
// as flat module-ID pictures by a small software rasteriser, read back with FUN_004c6fb0's 3 x 3 test. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-autolink-facts.md sections 2, 6, 7.
//
// WHAT inteng DOES (the decompile): for the current module A, every other module B that MEETS A (autolink_portal.h) gives a PORTAL; the sample views stand in the middle of the portal's
// rectangle (on the contact axis: the contact plane's middle, 10000 units back into A) and look through it along the contact axis; there are FIVE views (straight, then four turns by a
// matrix of sin 0x5798 / cos 0xf08f about the camera's own axes: about 70 degrees to each side), and for a contact on the x or z axis two more views, moved 500 units beyond the portal's
// side. Every view is drawn 320 x 240 with every module's polygons flat in the module's number + 1 (a z-buffer decides), read back, and a module counts as SEEN only where a
// 3 x 3 block of one number exists. The seen modules go into A's tree.
//
// WHAT THIS DOES: the same pass, drawn by host::AutoLinkRaster (double precision, a pinhole of 160 pixels, centre (160, 120), the near plane at 1 unit, no back-face culling, so more is
// seen rather than less: Melanikus's rule, a missing link can make a module vanish in game and an extra one only costs speed).
//
// NAMED (each a difference from inteng): our renderer, not inteng's (inteng's pixels depend on the machine's display path: FUN_004684e0 mode 0..3); the lens (160 pixels) and the near plane
// are read from FUN_0057f520's arguments (0xa0, 0x78, 0xa0), not proven; the two extra views' turn (hidden argument of FUN_004b34b0 at 0x4c82d1 / 0x4c8460) is guessed as the 70-degree turns; the
// modules inteng leaves out of the pass (a SHPMORPH, a strategy of kind 1000: FUN_004c6380 / FUN_004c98b0) are not; the distance cut is a separate step (DAT_00704378).
#pragma once

#include <stdint.h>

#include <set>
#include <vector>

#include "autolink_portal.h"

namespace host
{
	// One module of the level for the pass: its number (the object index the links name), its place / extents / shape as autolink_portal.h wants them
	struct AutoLinkModule
	{
		int32_t number;
		PortalModule module;
	};

	class AutoLinkPass
	{
	public:
		AutoLinkPass();

		// Takes the modules (kept by pointer: they must outlive the pass) and makes the world-space polygons the rasteriser draws.
		void Build(std::vector<AutoLinkModule> const &modules);

		// The modules seen from module `from` (an index into the vector given to Build) through its portals into every module that meets it: their numbers, not including `from`'s own.
		// *views (not null) gets how many views were drawn, *portals how many modules meet.
		void SeenFrom(size_t from, std::set<int32_t> *seen, size_t *views, size_t *portals) const;

		// One picture: the modules a camera at place (memory units) with the turn (nine 16.16 whole numbers, the camera's right, up and forward vectors as rows) sees by the 3 x 3 test
		void SeenByCamera(int32_t const place[3], int32_t const turn[9], std::set<int32_t> *seen) const;

	private:
		struct Polygon
		{
			int32_t id;                   // module number + 1
			std::vector<double> corners;  // x, y, z per corner
		};

		struct Range
		{
			size_t first, end;   // the module's polygons in `polygons`
			double lo[3], hi[3]; // its box in the level, for the cull
		};

		std::vector<AutoLinkModule> modules;
		std::vector<Polygon> polygons;
		std::vector<Range> ranges;
	};
}
