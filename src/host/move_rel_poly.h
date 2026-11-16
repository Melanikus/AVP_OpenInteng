// move_rel_poly.h: the sums of Move rel. to poly (Object Placement and Track Editing, command 0x51), exactly as inteng's FUN_005633e0 does them. Plain numbers,
// no window, no game: a test can drive it. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT COPIES (AvP-docs\engine\inteng, p11-shape-handlers.md 15, 18, 19 and 42-43; out\menu-reads-11.txt:87-). FUN_005633e0(target object, polygon, distance, moving
// object, optional hit point) sets the MOVING object's place and matrix:
//   1. the point: the average of the polygon's listed corners, each term divided by the count with INTEGER division (truncating toward 0) before the terms are
//      summed; the list is the polygon's first 3 corners, plus the corners of the polygon it was merged from (SHPMRGDT) that are not in it already;
//   2. that point is turned by the TARGET object's matrix (FUN_004b3860: x' = m0 x + m3 y + m6 z, y' = m1 x + m4 y + m7 z, z' = m2 x + m5 y + m8 z, each product a
//      16.16 multiply, (a * b) >> 16 on 64 bits) and the target's place is added;
//   3. the polygon's normal is NEGATED and turned the same way: n;
//   4. the moving object's matrix is made from n: row 2 = n; the axis is (65536, 0, 0) when n.x and n.z are both 0, else (n.z, 0, -n.x) scaled to length 65536
//      (FUN_004fd750); row 0 = the axis; row 1 = the cross product of n and the axis, normalised, each component times 65536 and rounded (FUN_004fd620);
//   5. an optional hit point (the click's point on the polygon's plane: FUN_00473680) REPLACES the point of step 2;
//   6. the place moves BACK along n by the distance: each axis of the place -= (n component * distance) >> 16 (FUN_00551f60).
// The places are inteng's own ints, the unit of 1e-5 m: the prompt's distance in metres is the int distance times 0.00001 (_DAT_005ac358).
//
// NAMED DEVIATIONS from the old program's arithmetic: inteng computes the normalise and the cross product in x87 floats (single-precision stores, extended
// precision between them); these use double, so a result can differ from inteng's by 1 in the last place of a 16.16 number where the extended and the double
// roundings part. The cross product's y and z follow the standard cyclic form of the x term the notes give (p11 18). The distance typed in metres becomes the int
// by multiplying by 100000 and truncating toward 0 (inteng's way back is "not shown" in the read), with a millionth of a unit of slack away from 0 so a typed 1.5 or
// 0.29 is not lost to the double's last digit.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace host
{
	// The most corners the list can hold: 3, plus up to 4 of a merged-from polygon's (a polygon has 3 or 4 corners in a level's shapes)
	size_t const kMoveRelPolyMostCorners = 8;

	struct MoveRelPolyTarget
	{
		int32_t corners[kMoveRelPolyMostCorners][3];   // the polygon's listed corners, in the target shape's own space, already without repeats
		size_t cornerCount;                            // 1 to kMoveRelPolyMostCorners
		int32_t normal[3];                             // the polygon's normal as the shape holds it (16.16)
		int32_t matrix[9];                             // the target object's matrix (16.16), row by row as inteng holds it: m0..m8
		int32_t place[3];                              // the target object's place
	};

	struct MoveRelPolyResult
	{
		int32_t place[3];
		int32_t matrix[9];
	};

	// The sums above. hit is the click's point (3 ints) or null. Gives back false, with nothing in *out, for no corners or too many.
	bool MoveRelPoly_Make(const MoveRelPolyTarget &target, int32_t distance, const int32_t *hit, MoveRelPolyResult *out);

	// The prompt's number: a distance typed in metres to the int the move takes (times 100000, truncated toward 0, held to the range of an int), and back
	// (the int times 0.00001: the default shown). A value that isn't a number gives 0.
	int32_t MoveRelPoly_DistanceFromMetres(double metres);
	double MoveRelPoly_MetresFromDistance(int32_t distance);

	// The 16.16 multiply inteng uses everywhere (FUN_00499b30 = FUN_00551f60): (int)(((int64)a * (int64)b) >> 16)
	int32_t MoveRelPoly_Multiply(int32_t a, int32_t b);
}
