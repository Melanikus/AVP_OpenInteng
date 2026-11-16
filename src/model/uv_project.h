// uv_project.h: the Texture Map commands WRAP (0x2e) and TILE (0x2f): new texture coordinates for the corners of the selected polygons, by projecting their world
// points onto the plane of a chosen polygon. P11. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT COPIES. inteng's FUN_00570590 (p11-shape-handlers.md sections 51, 55 and 58; its decompile is at out\dump-all-4.txt, "== FUN_00570590"; the helper
// functions below are named with their addresses). Everything here is PURE: world points go in, texture coordinates come out; nothing touches a file or a
// class of the game. The caller (the host) makes the world points the way inteng does (FUN_0055a900: each shape point turned by the object's matrix with
// FUN_004b3860 and moved by its place with FUN_004b3800) and gives the numbers that live in inteng's screen state.
//
// THE FIXED-POINT NUMBERS. inteng's vectors are three whole numbers; an AXIS is a vector of length 65536 (1.0 = 0x10000) made by FUN_004fd750, and a product
// of an axis and a point is (a x b) >> 16 (FUN_00499b30), so a dot product is in the point's own units.
//
// NAMED DEVIATIONS (cards in unfinished-in-inteng.md):
// 1. inteng's float maths (lengths, the scale, the final mix) runs on the x87 in extended precision, with a few values stored as 32-bit floats (the argument
//    of FUN_00570ef0 is one). Here a double stands for the extended precision and the same values are narrowed to float where the code stores them: a result
//    that falls exactly on a half can differ by one in the last place.
// 2. The chosen polygon's NORMAL (FUN_00558d30 reads inteng's own table of whole-number normals) is an INPUT here: how inteng turns the file's float normals into
//    whole numbers is not read.
// 3. A polygon's type being forced to 7 when it is not a textured one (FUN_00570590 0x570a79) is the caller's.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "uv_model.h"

namespace model
{
	struct Fix3
	{
		int32_t x, y, z;
	};

	// FUN_00499b30: (a x b) >> 16, the high word kept (the low 32 bits of the 64-bit product shifted right 16)
	int32_t Fixed16Multiply(int32_t a, int32_t b);

	// FUN_004b3170: Fixed16Multiply of the three pairs, added (32-bit wrap, as the game's adds)
	int32_t FixedDot(Fix3 const &a, Fix3 const &b);

	// FUN_004b5650: the cross product a x b with Fixed16Multiply: (a.y b.z - a.z b.y, a.z b.x - a.x b.z, a.x b.y - a.y b.x)
	Fix3 FixedCross(Fix3 const &a, Fix3 const &b);

	// FUN_004fd750: each component c becomes ROUND(float(c x 65536.0 / length)), the length being the square root of the sum of the three squares of the float
	// numbers, the division stored as a float and rounded to nearest, ties to even (FUN_004fd740). False for a length of zero (inteng would divide by zero and
	// get 0x80000000s): *v is then left as it was.
	bool FixedNormalise(Fix3 *v);

	// FUN_004b3240: the normalised difference a - b
	bool FixedDifferenceNormalised(Fix3 const &a, Fix3 const &b, Fix3 *out);

	// FUN_00558d30(module, polygon): from the polygon's first three world points p0, p1, p2 and its normal n (inteng's whole-number normal): TRUE when
	// dot(normalise(cross(p1 - p0, n)), p2 - p0) is above 0. False when a length is zero.
	bool TriangleIsOriented(Fix3 const &p0, Fix3 const &p1, Fix3 const &p2, Fix3 const &normal);

	// The plane of the chosen polygon: axis1 and axis2 (length 65536 each) and the origin (its first world point).
	struct PlaneBasis
	{
		Fix3 axis1, axis2, origin;
	};

	// FUN_00570590 0x57078c-0x5709a9: from the chosen polygon's three world points (the order of its first three corners) and FUN_00558d30's answer `oriented`.
	// The corner whose two edges are the most nearly perpendicular is chosen (the smallest |dot| of the two edge axes; a dot of exactly 0 stops the search at once),
	// axis 1 is the normalised edge from it to the next (oriented: its third) corner, the normal is the normalised cross of the two edge axes of the LAST pair
	// examined, and axis 2 is the normalised cross of the normal and axis 1. BadPlane when a length is zero (an edge of no length: inteng divides by zero).
	UvModelResult ProjectionBasis(Fix3 const points[3], bool oriented, PlaneBasis *out);

	// inteng's texture window as FUN_00570590 reads it: the four corner points of the window (state +0x52: pairs of whole numbers) and the rectangle the
	// results are held to (state +8, +0xc, +0x10, +0x14: left, top, right, bottom, not yet lowered by 1).
	struct WindowCorners
	{
		int32_t u[4], v[4];
	};

	// the settings of the tile mode: DAT_005fd694 and DAT_005fd698 (the U and V scales, never 0), DAT_00914d20 and DAT_00914d24 (the U and V offsets)
	struct TileSettings
	{
		int32_t uScale, vScale, uOffset, vOffset;
	};

	// WRAP (tile false, mode 1) or TILE (tile true, mode 2): the world point of every corner of every selected polygon in, the new (u, v) of each corner out
	// (same order). WRAP fits the points' spread onto the window (the window's own two sides give the directions and the lengths, +0.5, the scale) and holds each
	// result to [left, right - 1] x [top, bottom - 1]; TILE divides the projection by the scales and adds the offsets, with no limit. BadWindow when a window
	// side has no length or a tile scale is 0 (inteng asserts); BadTexel when a result is beyond +-16383.
	UvModelResult ProjectCorners(bool tile, PlaneBasis const &plane, WindowCorners const &window, TextureWindow const &rect, TileSettings const &tiles,
		std::vector<Fix3> const &worldPoints, std::vector<UvCorner> *out);
}
