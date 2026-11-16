// shape_maths.h: the numbers inteng works out about a shape: its radius, its extents, the point Recentre aims at, and moving a whole shape.
// P11, slice 3. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT IS. Pure functions over model::ShapeModel (shape_model.h). Each says in its comment which inteng function it copies and where that
// was read (AvP-docs\engine\inteng\p11-shape-handlers.md, the file:line is the one in re-tools\projects\vasquez-inteng\out). Nothing here
// touches a file or the game's classes.
//
// THE RULES.
// - The numbers are hostile (a level file made them): every sum is done in 64 bits and checked against the 32-bit range BEFORE anything is
//   changed, and a function that changes a shape changes it all or not at all (the shape is as it was on any answer but Ok).
// - Where inteng rounds, so does this: the divisions by 2 truncate toward zero (the compiler's __ftol and C integer division), the radius is
//   the longest point cast to a float (not rounded up).
// - Where inteng's own numbers are not yet read (the SIGN of the offset Recentre applies, the length helper of the normal maths), this file
//   has NO function for them: nothing here is a guess shipped as if it were read.
#pragma once

#include <stdint.h>

#include "shape_model.h"

namespace model
{
	enum class ShapeMathsResult
	{
		Ok,
		NoPoints,    // a shape with no points has no extents or centre to work out
		Overflow,    // a number would leave the 32-bit range
		SameShape,   // a weld was asked between a shape and itself (see WeldShapePoints)
		BadShape,    // a polygon names a point the shape doesn't have, or has fewer than 3 corners
	};

	// What an answer means, in plain words for the status line: static text, never null.
	char const *ShapeMathsWords(ShapeMathsResult result);

	// The longest distance of a point from the origin, as a float. Copies FUN_0042fa00 (sqrt of the sum of squares, in extended precision) kept
	// over all points the way FUN_00473c20 and FUN_00448cf0 do (out\menu-reads-5.txt, out\menu-reads-8.txt:432-446): the largest value wins and
	// it is cast to float without rounding up. 0 for no points.
	// Difference, named: inteng's sqrt is x87 extended and ours is double; the two can differ in the last bit of the float only when a point is
	// farther than about 2^25 units from the origin, far outside the game's levels.
	float ShapeRadiusOf(std::vector<ShapePoint> const &points);

	// The extents (max x, min x, max y, min y, max z, min z) of the points. NoPoints for none; *out is untouched then.
	ShapeMathsResult ShapeExtentsOf(std::vector<ShapePoint> const &points, ShapeExtents *out);

	// The point Recentre's "Move centre to top" (top true) or "to bottom" (false) names: the middle of the extents on x and z, each sum halved
	// by truncation toward zero, and y the extents' max y for top, min y for bottom. Copies FUN_004452b0's command code for 0x14a / 0x14b
	// (out\menu-reads-1-full.txt:6362-6372). The extents are the SHAPE'S (the header's), not worked out again, as inteng uses its record's.
	// The halving is by the double 2.0: read in run 12, `PUSH 0x40000000` at 0x446538 with the low dword `PUSH 0` at 0x446540
	// (out\raw-listing-7.txt:3; p11-shape-handlers.md section 18).
	ShapePoint RecentreTarget(ShapeExtents const &extents, bool top);

	// The offset Recentre adds to every point for a target point: the target NEGATED (FUN_004452b0 calls FUN_0042f960 with the double -1.0,
	// 0x446590: out\raw-listing-4.txt:75-77, read in run 9). Overflow for a target of -2^31, which has no negative in 32 bits.
	ShapeMathsResult RecentreOffsetFor(ShapePoint target, ShapePoint *offset);

	// Moves every point by q, the extents by q and the radius to the longest new point. Copies FUN_00473b40 and FUN_00473c20's first half
	// (out\menu-reads-5.txt). All or nothing: Overflow leaves *shape as it was. The point normals and polygon normals are NOT touched: a move
	// doesn't change a direction.
	ShapeMathsResult TranslateShape(ShapeModel *shape, ShapePoint q);

	// The radius about the shape's CENTRE: the longest point-to-centre distance, float, from 0, the centre being (max + min) / 2.0 per axis
	// truncated toward zero from the shape's own extents. Copies FUN_0050f570 0x50fc36-0x50fcd4 (out\menu-reads-5.txt, section 17 of
	// p11-shape-handlers.md): inteng's rebuild works this out after every shape edit. WHICH radius (this or the origin's) ends in SHPHEAD1 is not
	// read yet, so no function here writes either into the header.
	float ShapeRadiusAboutCentre(ShapeModel const &shape);

	// Recalculate Normals and Extents for one shape. Copies FUN_0044baa0 (out\menu-reads-5.txt:1476-, FUN_00567610, FUN_0042f7a0, FUN_0042f820,
	// FUN_0042fa80, FUN_0042fb00; p11-shape-handlers.md sections 8 and 10):
	//  - polygon normal i = (p1 - p0) x (p2 - p0) of the polygon's first three corners, taken in double, made unit length, narrowed to float;
	//    a cross product of length 0 is left as 0 (FUN_0042f820 returns without dividing);
	//  - point normals start at 0, then every polygon in order adds its normal (a float add) to each of its corners, then each is made unit
	//    (divided by sqrt(x*x + y*y + z*z)); a point normal of length 0 stays 0 (in Merge such a normal becomes (1,0,0): not this item);
	//  - extents = max and min of every coordinate, the running values starting at -1000000000 for the maxes and 1000000000 for the mins (inteng's
	//    own starting values, so a shape with no points gets those, and a coordinate beyond 1e9 does not move a min).
	// The radius is NOT touched (see ShapeRadiusAboutCentre). The shape's polygon and point counts are not changed.
	// NAMED DEVIATIONS: (1) inteng does the length, division and sqrt in x87 extended precision, this in double: a normal can differ in the
	// last bit of a float. (2) A point difference above 2^30 in any axis is refused (Overflow): inteng's x87 would take it, but its normals
	// are then meaningless and the editor is for levels in the game's range. All or nothing.
	ShapeMathsResult RecalculateNormalsAndExtents(ShapeModel *shape);

	// Fix tears: welds shape b's points to shape a's. Copies FUN_0044abd0 (out\menu-reads-7.txt:1492-, re-read in full 2026-11-15): for each
	// point of b in order, the point's world place is point + placeB (translation only, no rotation); each point of a in order is tried, its
	// world place being point + placeA; the FIRST a point for which the world difference is within the tolerance on EACH axis, bounds included
	// (-tolerance <= d <= tolerance: a box, not a sphere), is the match, and b's point becomes (a point + placeA) - placeB. A negative
	// tolerance matches nothing. *welded is how many b points matched (inteng rewrites a point that already sat on its match too).
	// Only b's points change; its extents, radius and normals are the rebuild's business (inteng's callers FUN_0044ad20 / FUN_0044b1f0 rebuild
	// the shape after: AvP-docs\engine\inteng\p11-shape-handlers.md section 6). All or nothing: Overflow leaves b as it was.
	// NAMED DEVIATION: inteng, given the SAME shape twice, reads points it has just changed; this refuses it (SameShape), because the answer
	// would depend on the order and nothing in the game asks for it.
	ShapeMathsResult WeldShapePoints(ShapeModel *b, ShapePoint placeB, ShapeModel const &a, ShapePoint placeA, int32_t tolerance, size_t *welded);

	// Turns a whole-number vector by an object's turn (the four floats x, y, z, w the file holds in OBJHEAD1 after the place), the way inteng does:
	// the quaternion becomes a 3x3 matrix of doubles by FUN_00430d70 (out\menu-reads-19.txt:2-47: m0 = 1 - 2(y^2 + z^2), m1 = 2xy - 2zw, ... with
	// the float products it makes) and the vector is multiplied by it as M x v, row by row, each result truncated toward zero by FUN_00430e80
	// (out\raw-listing-13.txt, p11-shape-handlers.md section 31). Overflow when a result leaves the 32-bit range or a float isn't finite.
	// NAMED DEVIATION: inteng adds the three products in x87 extended precision; this does it in double, so a result a hair under a whole number
	// can truncate one unit differently.
	ShapeMathsResult RotateByTurn(float const turn[4], ShapePoint v, ShapePoint *out);

	// Rescale's numbers (Object Placement, Rescale; FUN_004e1340 calls FUN_00430210 on the CHUNK-side record, menu-reads-34.txt:8-57; Apone's run 11,
	// apone-reads-3.md): every coordinate of every point, and the six extents numbers, become the old number times factor, truncated toward zero (__ftol);
	// the radius (a float) becomes (float)(radius x factor). So the numbers come out RELATIVE to what the shape holds now (the live table inteng also
	// keeps is absolute from a backup, which only the program's memory has). Overflow when any result leaves the 32-bit range or the factor is not a
	// finite number above 0 (inteng accepts 0: a shape with every point at 0; refused here, a named deviation). The shape is not changed unless the
	// answer is Ok. NAMED DEVIATION: inteng multiplies in x87 extended precision, this in double, so a product a hair under a whole number can
	// truncate one unit differently. NOT here: the centre (FUN_0042f960 on record 7..9) and record[10], which FUN_0050f570's rebuild sets again, and
	// SHPCENTR, which no host function writes yet.
	ShapeMathsResult RescaleShape(ShapeModel *shape, double factor);
}
