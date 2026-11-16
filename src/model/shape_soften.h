// shape_soften.h: inteng's Edge > Soft (Light Sourcing, command 0x50: FUN_004af810, FUN_004aff80, FUN_004afd80, FUN_004afde0, FUN_004b0020) as a function on a shape's numbers: the vertex normals are made again from
// the polygon normals, a point's normal being the average of the normals of the polygons that meet at it, where "meet" is cut at an angle. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-lighting-facts.md ("Edge > Soft").
//
// WHAT inteng DOES (the decompile): the typed angle (0 to 180, whole degrees) gives a threshold cos(angle x (1/360) x 6.28318...) (_DAT_005ab4a0 = 0x3f66c16c16c16c17 is 1/360 and _DAT_005adba0 = 0x401921fb542fe938 is 2 pi: the cosine of the angle itself). Every corner of every polygon (polygon by polygon, corner by corner) becomes a record with the
// corner's point (as three doubles), the polygon's normal (three floats made doubles) and the point's number. Corners are put in GROUPS, kept in a list in the order they were made: a corner goes into the FIRST
// group that takes it, else it starts a new group at the end. A group takes a corner when the group's position (the average of its members' positions) is no more than 1000 / (100 x the level's scale) shape units
// from the corner's, and either the group already holds a corner of the same point (no angle test), or every member's polygon normal has a dot product with the corner's polygon normal of at least the threshold.
// A group's position is made again as each corner joins. At the end each group's polygon normals are summed in the order they joined, made a unit vector (a zero sum stays zero) and given, as floats, to the point of
// every member, groups in the order made, so a point in two groups ends with the LATER group's.
//
// NAMED: x87 extended precision in inteng, long double here for the sums, products and the cosine.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "shape_model.h"

namespace model
{
	size_t const kSoftenMostAngle = 180;

	// inteng's tolerance (_DAT_005ac998)
	double const kSoftenTolerance = 1000.0;

	// The threshold of an angle in whole degrees, as inteng works it out
	double SoftenThreshold(int angleDegrees);

	// Makes shape->pointNormals again from the polygon normals. levelScale is the level's scale (1 when it has none): inteng's _DAT_00607bd8 is 100 x that. Gives false, changing nothing, when the angle is not 0 to
	// 180, the shape has no vertex normals to give, or the polygon normals are not one for each polygon. *changed (if not null) is how many points got a different normal than they had.
	bool SoftenShape(ShapeModel *shape, int angleDegrees, double levelScale, size_t *changed);
}
