// shape_resized.h: IS THIS SHAPE A RESIZED COPY OF A STOCK ONE? (Vasquez 2026-11-24, Bishop's card for beta.0; the owner: "Yes, same pickup"). Written 2026-11-24
// by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: typed placement (shape_identity.h) knows a shape only by its file's exact SHA-256. A level can hold a stock pickup at ANOTHER SIZE: its SHPEXTFN says
// "rescale 0.8", its points are the stock file's times 0.8, and so its bytes, and its SHA-256, are not the stock file's (Invasion's own medbox01, Armour01,
// pulse, Smart and Flam01: shape_extract.h ShapeExtractResult::Rescaled). It is still the same pickup. This says so, and only that.
//
// WHAT IS THE SAME, every part of it, or the answer is "not the same":
//   - one shape in each (a fragment file, more than one part, is never compared), and the same shape flags (but the outside-file mark, below);
//   - the same number of points (at most kShapeResizedMostPoints) and of polygons (at most kShapeResizedMostPolygons), so a shape that is only PART of the
//     stock one, or more than it, is never taken for it;
//   - each polygon, in the same order: the same corners (the same point numbers, in the same order), the same item type and flags, the same picture (by its
//     texture NAME, any capitals; or both drawn shaded) and the same places in it (within kShapeResizedTexelSlack);
//   - and the points: ONE scale, the same for x, y and z and greater than 0, between kShapeResizedLeastScale and kShapeResizedMostScale, takes every stock point
//     to the copy's within the tolerance below. A mirrored copy (a negative scale on any axis), a copy stretched on one axis more than another, a copy moved or
//     turned: not the same.
//   - and AT THE STOCK SHAPE'S OWN SIZE (a scale within kShapeResizedLeastChange of 1) nothing was rescaled, so nothing was rounded: every point must be
//     EXACTLY where the stock one is (0 mm), whatever the file's other bytes (another file of the shape: the old tools' package's Pulse.RIF is Steam's
//     pulse.rif point for point; Bishop's (a), 2026-11-24). An edited shape at the stock size (a point moved 1 mm) is never taken for it.
// THE TOLERANCE, per coordinate: points are whole millimetres in a file, so a copy made at scale s holds round(s x t) where the stock file holds round(t), t the
// shape as it was made: they differ by at most 0.5 + 0.5 x s. Allowed: kShapeResizedRoundingMm x (1 + s) + kShapeResizedSlackMm. A stock shape whose points all
// sit within kShapeResizedLeastRadiusMm of its middle is too small for one scale to mean anything: never compared.
// THE SCALE: the least-squares one through the origin (the sum of copy x stock over the sum of stock x stock, over every coordinate): the copy's own points decide
// it, never a number the file says (its SHPEXTFN's rescale is only reported beside it).
// THE WORK IS BOUNDED: the counts are checked first, and the compare is one pass over the points and one over the polygons, each at most 16,384.
// Pure: no file, no Windows.
#pragma once

#include <stddef.h>

#include <string>

#include "shape_thumb.h"

namespace host
{
	size_t const kShapeResizedMostPoints = kShapeThumbMostPoints;       // 16,384
	size_t const kShapeResizedMostPolygons = kShapeThumbMostPolygons;   // 16,384
	double const kShapeResizedLeastScale = 1.0 / 64;
	double const kShapeResizedMostScale = 64.0;
	double const kShapeResizedRoundingMm = 0.5;       // each file rounds to whole millimetres: half of one, for the copy and for the stock file (x the scale)
	double const kShapeResizedSlackMm = 0.01;         // the float arithmetic's
	double const kShapeResizedTexelSlack = 0.01;      // a place in a picture: the same whole texels in both files
	double const kShapeResizedLeastRadiusMm = 16.0;
	double const kShapeResizedLeastChange = 0.001;    // a scale within 0.1% of 1 is the stock shape's own size: no rescale, so no rounding to allow for
	// THE OUTSIDE-FILE MARK (avpx chunk_shape.hpp SHAPE_FLAG_EXTERNALFILE): every outside shape a LEVEL holds has it, and the file made of it doesn't (the extractor
	// takes it off: shape_extract.cpp). It says where the shape's data came from, not anything the game draws, so the shape flags are compared without it
	// (measured 2026-11-24: Invasion's own five, drawn from the level, differed from Steam's files by it alone, so none was typed)
	int32_t const kShapeResizedOutsideMark = 0x1000;

	struct ShapeResizedAnswer
	{
		bool same = false;     // the copy is the stock shape at one uniform scale
		bool sameSize = false; // that scale is the stock's own size (1): then every point exactly equal, nothing allowed for rounding (another FILE of the shape)
		double scale = 0;      // that scale (when same; when not, the best one found, or 0 when none was worked out)
		double worstMm = 0;    // the farthest any copy coordinate sits from the stock one times the scale (when the points were compared)
		double allowedMm = 0;  // what the tolerance allowed at that scale
		std::string why;       // when not the same, why, in plain words ("another number of points: 120 and 96")
	};

	// Is `copy` the `stock` shape at one uniform scale (above)? Both as ShapeThumb_Read made them.
	ShapeResizedAnswer ShapeResized_Compare(ShapeThumbModel const &copy, ShapeThumbModel const &stock);

	// THE SAME SURFACE, ANOTHER POLYGON LIST (Vasquez 2026-11-24; the owner: "dedupe those", Bishop's approval): a stock pickup's conflict variant is often the
	// same shape with its triangle pairs merged into quads (MEASURED: pulse.rif 142 triangles, pulse_als-dm 16 triangles and 63 quads, every point equal; so too
	// smrtam01, sadar and pboost). `merged` is the same surface as `split` when, every part of it the same:
	//   - one shape in each, the same shape flags (but the outside-file mark), the same points EXACTLY (each coordinate equal), at most kShapeResizedMostPoints;
	//   - every polygon of `merged` is ONE triangle of `split` (the same corners, turning the same way, from any corner) or a quad that is TWO of its triangles
	//     cut along either diagonal, each piece turning as the quad does; and every piece takes the same places in the same picture (by its texture NAME, any
	//     capitals) with the same item type and flags;
	//   - every triangle of `split` is used exactly once. `split` holds triangles only (a quad in it: not compared).
	// So a part of the shape, a polygon more, another picture or a face turned the other way is never the same. Bounded: the counts first, then one sort.
	// False with *why in plain words when not.
	bool ShapeResized_SameSurface(ShapeThumbModel const &merged, ShapeThumbModel const &split, std::string *why);

	// WHAT TWO FILES OF ONE SURFACE SHARE, as bytes, so the picker groups the files to compare without comparing every pair: the shape flags (but the outside-file
	// mark), how many triangles it is in all (a quad is two), its points exactly, and its pictures' names in small letters, sorted. Empty for a file of more
	// than one shape or with no point (never folded). The same key is not the same surface: ShapeResized_SameSurface decides
	std::string ShapeResized_SurfaceKey(ShapeThumbModel const &model);
}
