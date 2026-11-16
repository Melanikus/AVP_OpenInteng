// asc_export.h: the numbers inteng's Write Asc File puts in a .asc: for each object its vertices and faces, and the little marker box at the end.
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P7, level export). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// WHAT IS HERE AND WHAT IS NOT. Everything below is the number-making of FUN_00506f90 (Write Asc File) and FUN_00506b00 (one object), as read in
// re-tools\projects\vasquez-inteng\out (menu-reads-2.txt:11242-11623, menu-reads-5.txt, the object part). It makes numbers, never text: the
// words and spacing of each line (header, "Tri-mesh, Vertices: %d     Faces: %d", "Vertex %d: ...", "Face %d: ...") are NOT here, because which
// string each sprintf call uses is still being read (Vasquez: the raw bytes before the calls at 0x507054, 0x506b2a, 0x506b9e, 0x506d42 and the
// face loop). No .asc is written until they are, and the menu items stay [wip].
//
// WHAT FUN_00506f90 DOES, as far as it is read:
//   1. An offset: over every object's shape, the largest of the NEGATED x, of the negated y and of the negated z of its points, never less than 0
//      (local_4fc, local_4f8, local_4f4 start at 0). Every point is moved by it, so what is written is never to the negative side.
//   2. For each object, in the level's order, whose shape header has flag 0x1000 CLEAR: its shape, or (with "doors closed", param_3) the first
//      sub-shape of its SHPMORPH when it has one, is written by FUN_00506b00 with the object's place and the offset.
//   3. A marker box, "hudson": four fixed vertices moved by the same offset, and two fixed faces.
// WHAT FUN_00506b00 DOES, per object:
//   - each point of the shape: ((point + offset + place) + 0.5) * scale, written as X = the x, Y = the z, Z = the NEGATED y (the y and z are
//     swapped and the new z is negated);
//   - each polygon: its first three corners A, B, C, and for each of its three edges AB, BC, CA a 1, except that when the shape has merge data
//     (SHPMRGDT) naming another polygon for this one, and exactly one of this polygon's three corners is not among the other's first three, the
//     edge opposite that corner is 0 (the edge that was joined away).
// The scale (_DAT_005ae9d0) is not read yet: the caller gives it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// The shape of one object as the writer needs it: the points in the shape's own units and each polygon's first three corners (point numbers)
	// with the polygon the shape's merge data names for it (-1 for none, or when the shape has no SHPMRGDT)
	struct AscPoint
	{
		int32_t x, y, z;
	};

	struct AscPolygon
	{
		int32_t corner[3];
		int32_t mergedWith;   // the merge data's number for this polygon: a polygon of the same shape, or -1
	};

	struct AscShape
	{
		std::vector<AscPoint> points;
		std::vector<AscPolygon> polygons;
	};

	struct AscVertex
	{
		double x, y, z;
	};

	struct AscFace
	{
		int32_t a, b, c;        // the corners
		int32_t ab, bc, ca;     // 1, or 0 for the edge joined away
	};

	struct AscObject
	{
		std::vector<AscVertex> vertices;
		std::vector<AscFace> faces;
	};

	struct AscOffset
	{
		int32_t x, y, z;
	};

	// Step 1: the offset over the shapes of all the objects (every one, as inteng's loop has no test of the shape's flag). Null shapes are skipped.
	AscOffset AscOffsetOf(std::vector<AscShape const *> const &shapes);

	// FUN_00506b00's numbers for one object: place is its place (three whole numbers), offset the step 1 offset, scale the writer's. A polygon whose
	// merge number or corner is outside the shape is written as if it had none (a hostile level never reads past a shape).
	AscObject MakeAscObject(AscShape const &shape, int32_t const place[3], AscOffset const &offset, double scale);

	// Step 3: the marker box's four vertices (its X, Y and Z as FUN_00506f90 writes them), for the offset and the scale.
	struct AscMarker
	{
		AscVertex vertices[4];
	};

	AscMarker MakeAscMarker(AscOffset const &offset, double scale);

	// The shape header flag that keeps a shape out of the file (step 2): 0x1000
	uint32_t const kAscSkipShapeFlag = 0x1000;
}
