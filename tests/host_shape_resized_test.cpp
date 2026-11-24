// The test for A RESIZED COPY OF A STOCK SHAPE (Vasquez 2026-11-24, Bishop's card for beta.0; the owner: "Yes, same pickup"): src/host/shape_resized.h
// ShapeResized_Compare says "the same" only for the stock shape at ONE uniform scale greater than 0, every other part of it the same, and says why not for
// every near miss. Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// USE: host_shape_resized_test.exe (no files: the shapes are made here). The stock shape is a box of 8 whole-millimetre points, about 300 mm across, of 6
// four-cornered polygons, 5 with a picture and 1 shaded.
//   (1) THE SAME: at 0.8, 2.5, 1/3 and 0.77, each point rounded to whole millimetres as a file holds it; a texture name in other capitals. and itself
//       at its own size (every point exactly equal: another file of the shape, the old tools' package's; Bishop's (a))
//   (2) THE TOLERANCE, both sides of it: one point off by just under what is allowed (the same), and by just over (not)
//   (3) THE NEAR MISSES the card names, each not the same: mirrored on one axis, every axis negated, stretched on one axis, moved, turned a quarter;
//       and an EDITED shape at the stock size (one point moved 1 mm: measured in the acceptance, it fit scale 1.000 within the rounding before the rule)
//   (4) ANOTHER SHAPE: other counts of points or polygons (a part of it, or more), polygons in another order, a polygon's corners in another order,
//       another picture, shaded instead of a picture, other places in the picture, another item type or flags, other shape flags, a two-part file
//   (5) HOSTILE OR PAST THE BOUNDS, each refused with words and nothing read past either shape: more than 16,384 points, a corner naming a point past the
//       list, a point that isn't a number or infinite, a place in a picture that isn't a number (read by the float's bits: /fp:fast), no point, a scale of 100, a stock shape too small to tell a scale by
// RED FIRST: the acceptance acceptance-picker-resized (Invasion's own medbox01, Armour01, pulse, Smart and Flam01 static before this card); these checks are
// the new call's own.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "shape_resized.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The stock shape: a box 300 x 200 x 120 mm with one corner at the origin's side, its points whole millimetres
static host::ShapeThumbModel Stock()
{
	static float const corners[8][3] = { { -150, -100, -60 }, { 150, -100, -60 }, { 150, 100, -60 }, { -150, 100, -60 },
		{ -150, -100, 60 }, { 150, -100, 60 }, { 150, 100, 60 }, { -150, 100, 60 } };
	static int const faces[6][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 1, 2, 6, 5 }, { 2, 3, 7, 6 }, { 3, 0, 4, 7 } };
	host::ShapeThumbModel m;

	m.parts = 1;
	m.shapeFlags = 0x40;

	for (int p = 0; p < 8; p++)
	{
		for (int a = 0; a < 3; a++)
		{
			m.points.push_back(corners[p][a]);
		}
	}

	host::ShapeThumbPicture picture;
	picture.index = 0;
	picture.name = "Common\\Pikup.rim";
	picture.path = "Common\\Pikup.rim";
	m.pictures.push_back(picture);

	for (int f = 0; f < 6; f++)
	{
		host::ShapeThumbPolygon polygon;
		polygon.cornerCount = 4;
		polygon.picture = f == 5 ? -1 : 0;   // the last one shaded
		polygon.engineType = 2;
		polygon.flags = f == 1 ? 0x10 : 0;

		for (int c = 0; c < 4; c++)
		{
			polygon.corners[c] = faces[f][c];
			polygon.texels[c][0] = (float)(c == 1 || c == 2 ? 31 : 0);
			polygon.texels[c][1] = (float)(c >= 2 ? 31 : 0);
		}

		m.polygons.push_back(polygon);
	}

	return m;
}

// The stock shape at a scale, each coordinate rounded to whole millimetres as a file holds it
static host::ShapeThumbModel Scaled(double scale)
{
	host::ShapeThumbModel m = Stock();

	for (float &v : m.points)
	{
		v = (float)floor(v * scale + 0.5);
	}

	return m;
}

static void Same(host::ShapeThumbModel const &copy, std::string const &what, double scale = 0)
{
	host::ShapeResizedAnswer const a = host::ShapeResized_Compare(copy, Stock());
	char numbers[160];

	snprintf(numbers, sizeof numbers, " (scale %.4f, worst %.3f mm of %.3f allowed)", a.scale, a.worstMm, a.allowedMm);
	Check(a.same && (scale == 0 || fabs(a.scale - scale) < 0.002), what + ": the same" + numbers + (a.same ? "" : ", said: " + a.why));
}

static void NotSame(host::ShapeThumbModel const &copy, std::string const &what, std::string const &words, host::ShapeThumbModel const &stock = Stock())
{
	host::ShapeResizedAnswer const a = host::ShapeResized_Compare(copy, stock);

	Check(!a.same && a.why.find(words) != std::string::npos, what + ": not the same, said \"" + a.why + "\" (wanted \"" + words + "\")");
}

// The stock box (six quads) with each quad cut into two triangles: along its 0-2 diagonal (cut 0), its 1-3 one (cut 1), or the polygon's number's own
// (cut 2: even 0-2, odd 1-3); each piece turning as the quad does, with its places and picture
static host::ShapeThumbModel Split(host::ShapeThumbModel const &quads, int cut)
{
	host::ShapeThumbModel m = quads;

	m.polygons.clear();

	for (size_t i = 0; i < quads.polygons.size(); i++)
	{
		host::ShapeThumbPolygon const &q = quads.polygons[i];
		bool const along02 = cut == 0 || (cut == 2 && i % 2 == 0);
		int const halves[2][3] = { { 0, 1, along02 ? 2 : 3 }, { along02 ? 0 : 1, 2, 3 } };

		for (auto const &h : halves)
		{
			host::ShapeThumbPolygon t = q;

			t.cornerCount = 3;

			for (int k = 0; k < 3; k++)
			{
				t.corners[k] = q.corners[h[k]];
				t.texels[k][0] = q.texels[h[k]][0];
				t.texels[k][1] = q.texels[h[k]][1];
			}

			t.corners[3] = t.corners[4] = -1;
			m.polygons.push_back(t);
		}
	}

	return m;
}

static void Surface(bool want, host::ShapeThumbModel const &merged, host::ShapeThumbModel const &split, std::string const &what, std::string const &words = "")
{
	std::string why;
	bool const got = host::ShapeResized_SameSurface(merged, split, &why);

	Check(got == want && (want || why.find(words) != std::string::npos), what + (want ? ": the same surface" : ": not the same surface") + (why.empty() ? "" : ", said \"" +
		why + "\"") + (want || words.empty() ? "" : " (wanted \"" + words + "\")"));
}

int main()
{
	printf("host_shape_resized_test: a resized copy of a stock shape (shape_resized.h)\n");

	// (1) THE SAME
	// at its own size, every point exactly where the stock one is: the same shape (another file of it: the old tools' package's, Bishop's (a))
	{
		host::ShapeResizedAnswer const a = host::ShapeResized_Compare(Stock(), Stock());
		Check(a.same && a.sameSize && a.scale == 1.0 && a.worstMm == 0.0 && a.allowedMm == 0.0, "(1) itself, at its own size: the same, sameSize, scale exactly 1, 0 mm of 0 allowed");
		Check(!host::ShapeResized_Compare(Scaled(0.8), Stock()).sameSize, "(1) at 0.8: not sameSize");
	}
	Same(Scaled(0.8), "(1) at 0.8, rounded to whole mm", 0.8);
	Same(Scaled(2.5), "(1) at 2.5, rounded to whole mm", 2.5);
	Same(Scaled(1.0 / 3), "(1) at 1/3, rounded to whole mm", 1.0 / 3);
	Same(Scaled(0.77), "(1) at 0.77, rounded to whole mm (115.5 is 116: half a millimetre off)", 0.77);
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.pictures[0].name = "COMMON\\PIKUP.RIM";
		Same(c, "(1) its texture name in other capitals", 0.8);
	}

	// (2) THE TOLERANCE: at 2, a coordinate may sit 0.5 x (1 + 2) + 0.01 = 1.51 mm from the stock one times 2. Points 0 and 1 (x -150 and +150) are both moved
	// the same way, so the least-squares scale stays 2 exactly and each sits off by just what is added
	{
		host::ShapeThumbModel under = Scaled(2.0), over = Scaled(2.0);
		under.points[0] += 1.45f;
		under.points[3] += 1.45f;
		over.points[0] += 1.60f;
		over.points[3] += 1.60f;
		Same(under, "(2) at 2, two coordinates 1.45 mm off (1.51 allowed)", 2.0);
		NotSame(over, "(2) at 2, two coordinates 1.60 mm off (1.51 allowed)", "no one scale takes every point there");
	}

	// (3) THE NEAR MISSES
	{
		host::ShapeThumbModel mirroredZ = Scaled(0.8), mirrored = Scaled(0.8), negated = Scaled(0.8), stretched = Scaled(0.8), moved = Scaled(0.8), turned = Scaled(0.8);

		for (size_t k = 0; k < mirrored.points.size(); k += 3)
		{
			mirrored.points[k] = -mirrored.points[k];                                         // x only
			mirroredZ.points[k + 2] = -mirroredZ.points[k + 2];                               // z only
			stretched.points[k] = (float)floor(stretched.points[k] * 1.05 + 0.5);            // x 5% longer
			moved.points[k] += 10;                                                            // 10 mm along x
			float const x = turned.points[k], z = turned.points[k + 2];
			turned.points[k] = -z;                                                            // a quarter turn round y
			turned.points[k + 2] = x;
		}

		for (float &v : negated.points)
		{
			v = -v;
		}

		// a mirror is refused one way or the other: on x (the box's longest side) it turns the scale's sum below 0, so no scale fits at all; on z (its shortest)
		// the sum stays above 0, and the points don't fit the scale it gives
		NotSame(mirrored, "(3) mirrored on x", "no scale greater than 0 fits");
		NotSame(mirroredZ, "(3) mirrored on z", "no one scale takes every point there");
		NotSame(negated, "(3) every axis negated (a scale of -0.8)", "no scale greater than 0 fits");
		NotSame(stretched, "(3) stretched 5% on x", "no one scale takes every point there");
		NotSame(moved, "(3) moved 10 mm", "no one scale takes every point there");
		NotSame(turned, "(3) turned a quarter round y", "no one scale takes every point there");

		host::ShapeThumbModel edited = Stock();
		edited.points[0] += 1;
		NotSame(edited, "(3) an edited shape at the stock size: its first point moved 1 mm", "at the stock shape's own size every point must be exactly where its is");

		// at the stock size nothing is allowed for rounding: a point a quarter of a millimetre off is refused too (at 0.8 the same would be taken)
		host::ShapeThumbModel quarter = Stock();
		quarter.points[4] += 0.25f;
		NotSame(quarter, "(3) at the stock size, one point 0.25 mm off", "at the stock shape's own size every point must be exactly where its is");
	}

	// (4) ANOTHER SHAPE
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.points.push_back(0); c.points.push_back(0); c.points.push_back(0);
		NotSame(c, "(4) one point more", "another number of points: 9 and 8");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.polygons.pop_back();
		NotSame(c, "(4) one polygon fewer (a part of it)", "another number of polygons: 5 and 6");
		NotSame(Scaled(0.8), "(4) the stock shape a polygon fewer than the copy (the copy more than it)", "another number of polygons: 6 and 5", c);
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		std::swap(c.polygons[0], c.polygons[1]);
		NotSame(c, "(4) two polygons in another order", "polygon 0");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		std::swap(c.polygons[2].corners[0], c.polygons[2].corners[1]);
		NotSame(c, "(4) a polygon's corners in another order", "polygon 2 joins other points");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.pictures[0].name = "Common\\Pikup2.rim";
		NotSame(c, "(4) another texture name", "polygon 0 is drawn with another picture");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.polygons[3].picture = -1;
		NotSame(c, "(4) a polygon shaded instead of its picture", "polygon 3 is drawn with another picture");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.polygons[4].texels[2][0] = 30;
		NotSame(c, "(4) another place in the picture", "polygon 4 takes other places in its picture");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8), d = Scaled(0.8);
		c.polygons[1].engineType = 3;
		d.polygons[1].flags = 0;
		NotSame(c, "(4) another item type", "polygon 1 has another item type or other flags");
		NotSame(d, "(4) other polygon flags", "polygon 1 has another item type or other flags");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8), d = Scaled(0.8);
		c.shapeFlags = 0x41;
		d.parts = 2;
		NotSame(c, "(4) other shape flags", "other shape flags (0x41 and 0x40)");
		NotSame(d, "(4) a file of two shapes", "a file of more than one shape is never compared");
	}
	{
		// the outside-file mark (0x1000) is where a shape came from: a level's copy has it, the file doesn't, and it is the same shape; any other bit isn't
		host::ShapeThumbModel c = Scaled(0.8), d = Scaled(0.8);
		c.shapeFlags = 0x40 | host::kShapeResizedOutsideMark;
		d.shapeFlags = 0x40 | host::kShapeResizedOutsideMark | 0x2;
		Same(c, "(4) the level's copy with the outside-file mark, the file without it", 0.8);
		NotSame(d, "(4) the mark and one other flag", "other shape flags (0x1042 and 0x40)");
	}

	// (5) HOSTILE OR PAST THE BOUNDS
	{
		host::ShapeThumbModel big = Stock();
		big.points.assign((host::kShapeResizedMostPoints + 1) * 3, 1.0f);
		NotSame(big, "(5) 16,385 points in both", "more than 16384 points", big);
	}
	{
		host::ShapeThumbModel c = Scaled(0.8), s = Stock();
		c.polygons[0].corners[3] = 99;
		s.polygons[0].corners[3] = 99;
		NotSame(c, "(5) a corner naming point 99 of 8, in both", "polygon 0 names a point the shape doesn't have", s);
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.points[4] = std::numeric_limits<float>::quiet_NaN();
		NotSame(c, "(5) a point that isn't a number", "a point that isn't a number");
	}
	{
		// (Frost's SHOULD 1 on a844d4dc: under /fp:fast a NaN fails every comparison, so "off by more than the slack" was never true for it; now read by its bits)
		host::ShapeThumbModel c = Scaled(0.8), d = Scaled(0.8), e = Scaled(0.8);
		c.polygons[2].texels[1][0] = std::numeric_limits<float>::quiet_NaN();
		d.points[7] = std::numeric_limits<float>::infinity();
		e.polygons[4].texels[3][1] = -std::numeric_limits<float>::infinity();
		NotSame(c, "(5) a place in the picture that isn't a number (NaN)", "polygon 2 has a place in its picture that isn't a number");
		NotSame(d, "(5) a point at infinity", "a point that isn't a number");
		NotSame(e, "(5) a place in the picture at -infinity", "polygon 4 has a place in its picture that isn't a number");
	}
	{
		host::ShapeThumbModel c = Scaled(0.8);
		c.points.clear();
		NotSame(c, "(5) no point", "a shape with no whole points");
	}
	NotSame(Scaled(100), "(5) a scale of 100", "a scale past what is compared");
	{
		host::ShapeThumbModel tiny = Stock(), copy = Stock();

		for (size_t k = 0; k < tiny.points.size(); k++)
		{
			tiny.points[k] = tiny.points[k] / 50;   // 6 x 4 x 2.4 mm
			copy.points[k] = tiny.points[k] * 2;
		}

		NotSame(copy, "(5) a stock shape 6 mm across", "the stock shape is too small to tell a scale by", tiny);
	}

	// (6) THE SAME SURFACE, ANOTHER POLYGON LIST (the fold: a stock pickup's variant with its triangle pairs merged into quads)
	{
		host::ShapeThumbModel const quads = Stock(), cut02 = Split(Stock(), 0), cut13 = Split(Stock(), 1), mixed = Split(Stock(), 2);

		Surface(true, quads, cut02, "(6) six quads against their twelve triangles cut 0-2");
		Surface(true, quads, cut13, "(6) against them cut 1-3");
		Surface(true, quads, mixed, "(6) against them cut either way, polygon by polygon");
		Surface(true, cut02, cut02, "(6) the triangles against themselves");

		host::ShapeThumbModel fewer = cut02, more = cut02, turned = cut02, otherPlace = cut02, moved = cut02, otherName = cut02, sameNameCaps = cut02;
		fewer.polygons.pop_back();
		more.polygons.push_back(more.polygons[0]);
		std::swap(turned.polygons[3].corners[1], turned.polygons[3].corners[2]);   // a face turned the other way
		std::swap(turned.polygons[3].texels[1][0], turned.polygons[3].texels[2][0]);
		std::swap(turned.polygons[3].texels[1][1], turned.polygons[3].texels[2][1]);
		otherPlace.polygons[5].texels[2][1] = 30;
		moved.points[7] += 1;
		otherName.pictures[0].name = "Common\\Other.rim";
		sameNameCaps.pictures[0].name = "COMMON\\PIKUP.RIM";

		Surface(false, quads, fewer, "(6) a triangle missing (a part of the shape)", "another number of triangles in all: 12 and 11");
		Surface(false, quads, more, "(6) a triangle more", "another number of triangles in all: 12 and 13");
		Surface(false, quads, turned, "(6) a face turned the other way", "is no two triangles of the other");
		Surface(false, quads, otherPlace, "(6) one corner's place in the picture another", "is no two triangles of the other");
		Surface(false, quads, moved, "(6) a point 1 mm elsewhere", "a point is somewhere else");
		Surface(false, quads, otherName, "(6) another picture", "is no two triangles of the other");
		Surface(true, quads, sameNameCaps, "(6) the picture's name in other capitals");
		Surface(false, cut02, quads, "(6) the one to compare with holds quads", "holds a polygon of more than three corners");

		host::ShapeThumbModel five = quads, nan = quads, past = quads, flags = quads, mark = quads;
		five.polygons[0].cornerCount = 5;
		five.polygons[0].corners[4] = 1;
		nan.polygons[2].texels[0][0] = std::numeric_limits<float>::quiet_NaN();
		past.polygons[1].corners[2] = 99;
		flags.shapeFlags = 0x41;
		mark.shapeFlags = 0x40 | host::kShapeResizedOutsideMark;
		Surface(false, five, cut02, "(6) a polygon of five corners", "five corners");
		Surface(false, nan, cut02, "(6) a place that isn't a number", "isn't a number");
		Surface(false, past, cut02, "(6) a corner naming point 99 of 8", "a point the shape doesn't have");
		Surface(false, flags, cut02, "(6) other shape flags", "other shape flags");
		Surface(true, mark, cut02, "(6) the level's outside-file mark alone");

		// THE KEY the picker groups files by: the quads and their triangles share it; a point elsewhere, another picture, a triangle more do not; two parts: none
		host::ShapeThumbModel twoParts = quads;
		twoParts.parts = 2;
		std::string const key = host::ShapeResized_SurfaceKey(quads);
		Check(!key.empty() && key == host::ShapeResized_SurfaceKey(cut02) && key == host::ShapeResized_SurfaceKey(mixed) && key == host::ShapeResized_SurfaceKey(mark) &&
			key == host::ShapeResized_SurfaceKey(sameNameCaps), "(6) the key: the quads, their triangles either way, the mark alone, the name's capitals: one key");
		Check(key != host::ShapeResized_SurfaceKey(moved) && key != host::ShapeResized_SurfaceKey(otherName) && key != host::ShapeResized_SurfaceKey(more) &&
			key != host::ShapeResized_SurfaceKey(flags) && host::ShapeResized_SurfaceKey(twoParts).empty(),
			"(6) the key: a point elsewhere, another picture, a triangle more, other flags: another key; a file of two shapes: none");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed == 0)
	{
		printf("All checks passed\n");
		return 0;
	}

	printf("SOME CHECKS FAILED\n");
	return 1;
}
