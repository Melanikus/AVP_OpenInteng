// autolink_portal.h: where two modules MEET, EXACTLY as inteng's FUN_004c4750 works it out (AutoLink's first step for a pair of modules; AvP-docs\engine\inteng\drake-autolink-facts.md
// sections 2 and 6; dump-all-2.txt L61121, 4651 bytes). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT DOES (every step is the decompile's, with its address in portal.cpp):
//  1. The two modules' boxes (place + the shape's extents) are overlapped per axis. Fewer than two axes with an overlap above 29999 (30 m), or the least overlap below -1000 (the boxes more
//     than 1 m apart), and they do not meet (0). The axis of the least overlap is the CONTACT axis and the direction says on which side of A the module B lies.
//  2. The shapes' points within 1000 of the contact plane are listed (A's and B's), and every polygon that has EXACTLY TWO corners among them and whose normal points strongly along an axis
//     (a component beyond +-10000 of 65536) tightens the rectangle of the contact on the other axes (the polygon's corners are placed by the module's place); then two more passes per module
//     narrow the rectangle on the axis where a polygon overlaps most (the ratio tests, in double).
//  3. The rectangle must be wider than 1000 on both axes that are not the contact axis; else they do not meet.
// The answer is the portal: the rectangle (lo / hi per axis), the contact plane of each module, the contact axis and the direction. AutoLink puts its sample views in the middle of it.
//
// INPUT: every length in the UNITS INTENG'S MEMORY HOLDS: the file's number x 100 x the level's scale, truncated (host::AutoLinkPlace), for the module's place, its extents AND its shape's points (CHECKED:
// with that unit 93.9 to 99.3% of the stock levels' own adjacent pairs meet and under 0.3% of the other pairs do; with the file's numbers as they are only 0 to 3%: the 30 m and 1 m limits are in these units), each
// polygon's corner point numbers, and each polygon's normal as 16.16 whole numbers (the file's float x 65536, truncated: host::AutoLinkNormal). NAMED: a polygon with more than 4 corners
// is read with its first 4 only (inteng's slots are an array of 4 and its loop reads past it for more: a quirk that is not copied).
#pragma once

#include <stdint.h>

#include <vector>

namespace host
{
	struct PortalShape
	{
		std::vector<int32_t> points;                  // x, y, z of each point, in order
		std::vector<std::vector<int32_t> > polygons;  // each polygon's corner point numbers (indexes of points, not times 3)
		std::vector<int32_t> normals;                 // 3 per polygon: the normal as 16.16 whole numbers
	};

	struct PortalModule
	{
		int32_t place[3];                             // in inteng's memory units
		int32_t extents[6];                           // the shape's maxX, minX, maxY, minY, maxZ, minZ (the order of the header and of the object's +0x104 to +0x118)
		PortalShape const *shape;                     // the module's shape
	};

	struct Portal
	{
		int32_t lo[3];                                // the rectangle's lower bound per axis (the contact axis's too)
		int32_t hi[3];
		int32_t planeA;                               // A's contact plane coordinate on the contact axis
		int32_t planeB;                               // B's
		int32_t axis;                                 // 0, 1 or 2
		int32_t direction;                            // -1: B lies on A's lower side, 1: on its upper side
	};

	// A length as inteng's memory holds it: truncated(file number x 100 x the level's scale) (FUN_0042f960 with _DAT_00607bd8 = 100 x the scale), for places, extents and points alike
	int32_t AutoLinkPlace(int32_t file, double levelScale);

	// A normal's number as the 16.16 whole number inteng holds: truncated(x 65536)
	int32_t AutoLinkNormal(float component);

	// FUN_004c4750(A, B, out): true when the modules meet, *out is then the portal. False for the same module twice (A == B by pointer is the caller's test).
	bool AutoLink_ModulesMeet(PortalModule const &a, PortalModule const &b, Portal *out);
}
