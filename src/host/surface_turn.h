// surface_turn.h: THE TURN of placement mode (the gizmo's centre ball, surface_place.h), by the CLOSEST CARDINAL AXIS (the owner, 11-19: "lights (and I assume all other assets) are
// snapping their 'UP' to the normal ... this causes lights (3d modelled in a way the up axis is not aligned with the light direction) to flip and try to align the wrong axis. We want
// the 'Closest cardinal axis' (in dot product/angle), on the object's local coordinates to be the side that snaps"). Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Pure maths, no level and no window. TWO STEPS, each its own function, so a later rule changes only the first (the owner: a weapon lying on a floor and slid onto a wall meets it with
// another axis, "we can deal with that" later; a likely rule then is to keep the axis that met the first surface: give that axis to the second step instead of choosing one):
//   1. THE AXIS: of the object's six local directions turned by R0 (the pose the drag STARTED from, so nothing drifts), the one that points most out of the surface (the largest dot
//      with the normal n). Near a tie it keeps the previous frame's axis unless another wins by more than kSurfaceAxisMarginDegrees, so it does not flicker between two.
//   2. THE TURN: the smallest rotation Q taking that axis onto n (about axis x n, by the angle between them), applied after R0: the pose is Q R0. The rest of the object's pose is kept:
//      on a floor an upright object stays upright; onto a wall it turns only by the angle its closest side was off the wall's normal.
// The largest dot of six axes is always 1/sqrt(3) or more, so the axis is never opposite n and Q is never a half turn.
// THE NUMBERS: the world is the game's, y DOWN; n points OUT of the surface (towards the camera that saw it). The turn is the file's, x y z w, as surface_place.h says: a point of the
// shape at local p is drawn at the place plus R p, R the turn's own rotation matrix (model::TurnMatrix).
// A placed light (a placed object's PLOBJLIT) shines from the object itself, so the same Q turns its light with its model.

#pragma once

namespace host
{
	// The six local directions, as numbered here: 0 +x, 1 -x, 2 +y, 3 -y (up: y is down), 4 +z, 5 -z
	int const kSurfaceAxisCount = 6;

	// The margin by which another axis must be nearer the normal than the kept one before it takes over (the owner's "ties and flicker"; Bishop: "a few degrees")
	double const kSurfaceAxisMarginDegrees = 3.0;

	// The words of an axis for the status and state lines: "+x" .. "-z"; "none" for one out of range
	char const *SurfaceAxis_Name(int axis);

	// STEP 1: the axis of the turn R0 (file x y z w, any length but 0) that points most along n (unit, or any length but 0), keeping `previous` (0 to 5; -1 for none) unless another one's
	// angle to n is smaller by more than marginDegrees. On an exact tie with no previous, the lowest number. -1 when R0 or n has no length or isn't real numbers.
	int SurfaceAxis_Closest(float const turn0[4], double const normal[3], int previous, double marginDegrees);

	// STEP 2: the turn Q R0, Q the smallest rotation taking R0's `axis` (0 to 5) onto n: out of length 1. False, with out untouched, for an axis out of range, a turn or normal of no
	// length or not real numbers, or an axis more than 90 degrees from n (no step 1 gives one).
	bool SurfaceTurn_AxisOnto(float const turn0[4], int axis, double const normal[3], float out[4]);

	// The direction R0 turns a local axis (0 to 5) to, in the world (unit length); false for an axis out of range or a turn of no length
	bool SurfaceAxis_World(float const turn[4], int axis, double out[3]);
}
