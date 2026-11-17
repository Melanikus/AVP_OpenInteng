// vertex_pick.h: Hierarchy's VERTEX SELECTION click (V, "Vertex Selection Mode"), as inteng's FUN_00485290 does it, in plain sums a test can drive. Written 2026-11-17 by Melanikus (Vasquez)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Facts: AvP-docs\engine\inteng\reassess\hierarchy-closeout-frost.md TB-2.
//
// WHAT inteng DOES (FUN_004889d0 0x489dd4-0x489efa calls FUN_00485290(the vertex object, x, y, list), then for every number in the list sets the point's selection mark (FUN_0046dc90), or with
// Shift clears it (FUN_0046dce0)):
//   1. FUN_00559720(x, y): the object whose POLYGONS are under the pointer, nearest along the click's line. Unless that is the vertex object (the skin, the player's +0x7c), the list stays EMPTY.
//   2. Every point of the object's shape, turned and moved as the object is drawn and put through the view (FUN_004b3860, FUN_004b3800, FUN_004b4ba0): one whose depth is not beyond the view
//      block's +0x3c (0x140 for the marine's lens, 0xa0 for the alien's: inteng's units, 100 to the millimetre) is skipped; the others are projected (x * ProjX / depth + the screen's middle,
//      FUN_004888b0) and measured to the pointer (FUN_004fd830, a whole number). The SMALLEST distance wins, strictly (the first on a tie).
//   3. That point, then every other point within 1 of it on each axis (its duplicates: the float differences cut toward zero lie in -1 .. 1, 0x4854ef-0x48553c).
// OURS: the polygons are the skin's in the reference pose where the view draws its points (named in editor_main.cpp); the projection is the game's (pick_ray.h).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "pick_ray.h"

namespace host
{
	// Whether the line crosses one of the faces (each four point numbers, the fourth -1 for a triangle; points three whole numbers each, the world's millimetres), ahead of where it starts
	bool VertexPick_RayHitsFaces(PickRay const &ray, std::vector<int32_t> const &points, std::vector<int32_t> const &faces);

	// The point numbers the click takes (steps 2 and 3 above), the nearest first; empty when no point is ahead of the camera by more than nearAhead (the world's millimetres)
	std::vector<int32_t> VertexPick_Points(PickCamera const &camera, int32_t clickX, int32_t clickY, std::vector<int32_t> const &points, double nearAhead);
}
