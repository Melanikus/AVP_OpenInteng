// texture_quad.h: the Texture Map's shared maths on a polygon and the one the merge data paired it with (SHPMRGDT): the pair as ONE quad (inteng FUN_0055cdf0), which
// way the polygon's corners turn about its normal (FUN_00558d30), and the move that keeps a polygon's texture coordinates on the picture (FUN_00566b90). Several
// Texture Map commands call these (the window, Copy Texturing, cycle / reverse, the arrows, the sub-modes: TM-A, texture-map-closeout-frost.md). Written 2026-11-17 by
// Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Inteng's Texture Map holds TRIANGLES only (a quad of the game is two triangles and their pairing, DAT_00914d3c). So TextureQuadOf answers for a polygon of three corners;
// a polygon of the file with four of its own is not one it answers (NotATriangle), and the caller takes its four corners as they are.
#pragma once

#include <stdint.h>

#include "shape_model.h"
#include "uv_model.h"

namespace model
{
	// FUN_0055cdf0 (shape, polygon, out[12]): the polygon and its partner as one quad of four corners.
	// - With no partner: points = the polygon's corners 2, 1, 0 and -1 (in that order), ownCorners = 2, 1, 0, -1, partnerCorners all -1.
	// - With one: points[0] is the polygon's corner that the partner hasn't (ownCorners[0] is its place 0..2), points[2] the partner's corner that the polygon hasn't
	//   (partnerCorners[2] its place), and points[1] and points[3] the two they share, in the order TextureTurnsForward gives (true: the polygon's corners after
	//   the unshared one, +1 then +2; false: +2 then +1), with their places in both. ownCorners[2] and partnerCorners[0] are -1.
	// Point numbers here are the shape's (inteng's items hold each times 3).
	struct TextureQuad
	{
		int32_t points[4];           // out[0..3]
		int32_t ownCorners[4];       // out[4..7]: each corner's place in the polygon (0..2), -1 where it isn't the polygon's
		int32_t partnerCorners[4];   // out[8..11]: its place in the partner, -1 where it isn't the partner's
	};

	enum class TextureQuadResult
	{
		Ok,              // the pair as one quad, or the polygon alone when it has no partner (partner -1)
		Alone,           // the partner can't be used, so *out is the polygon alone: a number outside the shape, the polygon itself, a partner that is not a triangle, or one with
		                 // all three of the polygon's corners. INTENG BUG, FIXED, the last: inteng's unshared corner (local_14) is a stack word nothing writes, read as a place in
		                 // the polygon's item (0x55cefc)
		NoPolygon,       // the polygon is not one of the shape's: *out is left as it was
		NotATriangle,    // the polygon has more or fewer than three corners of its own: *out is left as it was
	};

	TextureQuadResult TextureQuadOf(ShapeModel const &shape, int32_t polygon, int32_t partner, TextureQuad *out);

	// FUN_00558d30: true when the polygon's corners turn one way about its normal. a = corner 1 - corner 0, b = corner 2 - corner 0 (the points as whole numbers), n = the
	// polygon's normal in 65536ths as inteng's own copy_to_shapeheader makes sh_normals (FUN_004ff010 0x4ffa4f-0x4ffa8a: the file's float times 65536.0f, then __ftol,
	// which cuts toward 0 into 64 bits and keeps the low 32, so a part of 32768 or more wraps; one past 64 bits is 0. A shape with no normals counts each as 0, 0, 0);
	// c = a x n in 16.16 products (FUN_004b5650, FUN_00499b30), made 65536 long (FUN_004fd750); true when c . b (16.16 products, FUN_004b3170) is above 0. False for a
	// polygon that is not one of the shape's or has fewer than three corners.
	bool TextureTurnsForward(ShapeModel const &shape, int32_t polygon);

	// FUN_004fd750 as the x87 does it (the listing, 0x4fd758-0x4fd7df): the three whole numbers as floats (x kept whole, y and z stored as 32-bit floats), the length the
	// square root of (z.fz + fy.fy) + x.x stored as a float, each part times 65536 over that length stored as a float and rounded to the nearest whole number (ties to
	// even, FUN_004fd740). A part that is not a number or doesn't fit is 0x80000000, the x87's answer. The sums are done in double precision (the precision the x87 ran at
	// in inteng is not read: DECIDED, the run-time library's own setting).
	void TextureNormaliseTo65536(int32_t v[3]);

	// FUN_00566b90's move on the coordinates of a textured polygon (own) and, when the two are one quad, its partner (partner, else null): the smallest u over their
	// corners and the smallest v are brought into [0, width) and [0, height) by whole picture sizes, and those corners of both lists are moved by the same amount.
	// ownCorners / partnerCorners: how many of each list's corners the polygon has (its own corner count; a list may hold more than its polygon uses). Inteng's
	// record holds a triangle's three (FUN_0055d770 0x55d9ca-0x55d9ee copies 0x18 bytes), so a 4th corner of a triangle's list is neither looked at nor moved;
	// DECIDED: the file's own quad (four corners) has all four looked at and moved. Each coordinate is first made a whole number cut toward zero, as uv_model's
	// commands do (inteng's are whole numbers: __ftol). *moved says whether anything changed; with nothing to move the lists are left exactly as they were,
	// fractions too. BadWindow for a width or height of 0 or less (INTENG BUG, FIXED: inteng divides by it, 0x566cbc / 0x566ce2); BadTexel when a corner is past
	// +-kMostTexel or a moved one would be, the lists left as they were; BadCall for a null own or moved, or a negative corner count.
	UvModelResult TextureWrapUv(UvList *own, int ownCorners, UvList *partner, int partnerCorners, int32_t width, int32_t height, bool *moved);
}
