// shape_merge.h: Merge Objects (command 0x58) for the numbers of two shapes. P11, slice 6. Written 2026-11-15 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. Pure code over model::ShapeModel (shape_model.h). It copies what inteng does to the two shapes' numbers when it merges the second
// object into the first: FUN_00449cf0 decides the point map (out\menu-reads-9.txt:316-810) and FUN_00448cf0 builds the merged numbers
// (out\menu-reads-8.txt:147-462); AvP-docs\engine\inteng\p11-shape-handlers.md sections 10 and 12 say each step in words. It knows nothing
// about the other things a merge also does (the TEXTANIM and ANIMSEQU chunks, SHPMRGDT, the UV list chunks, removing the second object,
// moving the first object's place by the centre): those are the host's, and this gives the host what it needs for them (the point map, the
// centre, how many polygons the first shape had).
//
// THE RULES.
// - The numbers are hostile: every count is checked against kMostShapePoints / kMostShapePolygons before anything is made, every sum is
//   done in 64 bits and checked against the 32-bit range, and the first shape is changed only when the answer is Ok.
// - Where inteng's own numbers or order matter they are copied: the map matches a point of the second shape to the FIRST point of the first
//   shape it equals EXACTLY (not "near": inteng's test is |difference| < 1 on whole numbers); the extents start from +-10000000 (inteng's
//   own start, which is not the 1e9 of Recalculate); the centre is the middle of those extents, halved by truncation toward zero.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "shape_maths.h"
#include "shape_model.h"

namespace model
{
	enum class ShapeMergeResult
	{
		Ok,
		TooBig,       // the merged shape would have more points or polygons than kMostShapePoints / kMostShapePolygons
		Overflow,     // a number would leave the 32-bit range
		BadShape,     // a polygon names a point its shape doesn't have, or the normals aren't one per polygon
	};

	// What an answer means, in plain words for the status line: static text, never null.
	char const *ShapeMergeWords(ShapeMergeResult result);

	// Whether a polygon of this engine type has a UV list (its colour's high 16 bits name one). Copies FUN_0042bce0 (out\menu-reads-9.txt:36-89):
	// the types 5, 6, 7, 0x15, 0x16, 0x17, 0x18 and 0x19.
	bool PolygonTypeHasUvList(int32_t engineType);

	// What a merge works out for the host besides the merged numbers.
	struct ShapeMergeOutcome
	{
		std::vector<int32_t> pointMap;   // for each point of the second shape, its number in the merged shape (before the re-centring: the same numbers)
		ShapePoint centre;               // the middle the merged points were moved back by (the first object's place has to move by it)
		size_t firstPolygons;            // how many polygons the first shape had: the second's polygons start there, and TEXTANIM's sequences are offset by it
	};

	// Merges `second` into `first`. placeFirst / placeSecond are the two objects' places (millimetres, translation only: inteng ignores rotation
	// here). uvListsInFirst is how many UV lists the first shape has (the second's polygons' UV list numbers are moved up by it).
	// firstHasAnimation is true when the first shape has an ANIMSEQU chunk: inteng then looks for NO equal points and adds every point of the
	// second shape (out\menu-reads-9.txt:0x449fe5-0x44a0cd).
	// What `first` holds afterwards: the first shape's points (all, unchanged in order) then the second's that matched none; the polygons of
	// both (the second's normal number made its new polygon number, its UV list number moved up, its corners renumbered through the map), the
	// polygon normals of both as they were, the point normals worked out again (every polygon adds its normal to each corner, each made unit,
	// a length of 0 becoming (1, 0, 0) as FUN_00449114 does), every point moved back by the centre, the extents and the radius (about the origin,
	// as FUN_00448cf0 leaves it) of the moved points. `first`'s flags and number are kept.
	// All or nothing: on any answer but Ok, `first` and *outcome are as they were.
	ShapeMergeResult MergeShapes(ShapeModel *first, ShapePoint placeFirst, ShapeModel const &second, ShapePoint placeSecond, size_t uvListsInFirst,
		bool firstHasAnimation, ShapeMergeOutcome *outcome);
}
