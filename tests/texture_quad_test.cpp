// The test for src/model/texture_quad.cpp: the Texture Map's shared maths, inteng's FUN_0055cdf0 (a polygon and its partner as one quad), FUN_00558d30 (which way a
// polygon's corners turn about its normal), FUN_004fd750 (a vector made 65536 long) and FUN_00566b90 (a polygon's texture coordinates brought onto the picture).
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng (TM-A's shared APIs, texture-map-closeout-frost.md). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// Every expected number below is worked out by hand from inteng's code (dump-full-4.txt FUN_0055cdf0 0x55ce05-0x55cf9d, FUN_00558d30 0x558d3d-0x558ddc,
// listing-all-3.txt FUN_004fd750 0x4fd758-0x4fd7df, FUN_00566b90 0x566ba4-0x566d4f):
//  1. the square P0 (0,0,0) P1 (100,0,0) P2 (100,100,0) P3 (0,100,0) as triangles A = 0 1 2 and B = 0 2 3, normal +z: A turns "false" (c = a x n = (0, -100, 0),
//     made (0, -65536, 0), c . b = -100), so A with B is the quad 1 0 3 2, places in A 1 0 -1 2, in B -1 0 2 1; with the normal -z A turns "true" and the quad is
//     1 2 3 0, places 1 2 -1 0 and -1 1 2 0; A alone is 2 1 0 -1;
//  2. a partner that can't be used gives A alone: outside the shape, A itself, one of four corners, and one with all three of A's corners (INTENG BUG, FIXED);
//  3. (3, 4, 0) made 65536 long is (39322, 52429, 0); (0, 0, 0) is 0x80000000 three times, as the x87 gives for 0 / 0;
//  4. the wrap: the smallest u brought into [0, width), the smallest v into [0, height), every corner of both lists moved by the same whole picture sizes.

#include <stdio.h>
#include <string.h>

#include <string>

#include "texture_quad.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string Four(int32_t const v[4])
{
	return std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]) + " " + std::to_string(v[3]);
}

static bool Is(int32_t const v[4], int32_t a, int32_t b, int32_t c, int32_t d)
{
	return v[0] == a && v[1] == b && v[2] == c && v[3] == d;
}

static model::ShapePolygon Polygon(int32_t a, int32_t b, int32_t c, int32_t d = -1)
{
	model::ShapePolygon polygon = {};

	polygon.engineType = 7;
	polygon.corners[0] = a;
	polygon.corners[1] = b;
	polygon.corners[2] = c;
	polygon.corners[3] = d;
	polygon.corners[4] = -1;
	polygon.cornerCount = d < 0 ? 3 : 4;
	return polygon;
}

// The square: A = 0 1 2, B = 0 2 3, C = 0 1 2 (all of A's corners), D = 0 1 2 3 (four of its own)
static model::ShapeModel Square(float normalZ)
{
	model::ShapeModel shape = {};

	shape.points = { { 0, 0, 0 }, { 100, 0, 0 }, { 100, 100, 0 }, { 0, 100, 0 } };
	shape.polygons = { Polygon(0, 1, 2), Polygon(0, 2, 3), Polygon(0, 1, 2), Polygon(0, 1, 2, 3) };
	shape.polygonNormals.assign(4, model::ShapeNormal{ 0.0f, 0.0f, normalZ });
	return shape;
}

static void TheQuad()
{
	puts("1. a polygon and its partner as one quad (FUN_0055cdf0), and which way it turns (FUN_00558d30)");

	model::ShapeModel const up = Square(1.0f);
	model::ShapeModel const down = Square(-1.0f);
	model::TextureQuad quad;

	Check(!model::TextureTurnsForward(up, 0), "A with the normal +z turns false (c . b = -100)");
	Check(model::TextureTurnsForward(down, 0), "A with the normal -z turns true (c . b = 100)");

	// __ftol keeps the low 32 bits of the 64-bit cut (0x5900ab): 40000 x 65536 = 2621440000 is -1673527296, c = (947912704, 0, 0) made (65536, 0, 0), c . b = 1
	model::ShapeModel wide = {};
	wide.points = { { 0, 0, 0 }, { 0, 131072, 0 }, { 1, 0, 0 } };
	wide.polygons = { Polygon(0, 1, 2) };
	wide.polygonNormals = { model::ShapeNormal{ 0.0f, 0.0f, 40000.0f } };
	Check(model::TextureTurnsForward(wide, 0), "a normal part of 40000 wraps as __ftol's low 32 bits: true (the review's case)");

	// past 64 bits __ftol gives 0x8000000000000000, low 32 bits 0: n = 0, c = 0 made 0x80000000 x3, c . b = -6553600
	model::ShapeModel huge = Square(1.0e15f);
	Check(!model::TextureTurnsForward(huge, 0), "a normal part of 1e15 is 0 (past 64 bits): false");

	memset(&quad, 0x55, sizeof(quad));
	Check(model::TextureQuadOf(up, 0, 1, &quad) == model::TextureQuadResult::Ok, "A with B (+z): Ok");
	Check(Is(quad.points, 1, 0, 3, 2), "its points are 1 0 3 2 (they are " + Four(quad.points) + ")");
	Check(Is(quad.ownCorners, 1, 0, -1, 2), "their places in A are 1 0 -1 2 (they are " + Four(quad.ownCorners) + ")");
	Check(Is(quad.partnerCorners, -1, 0, 2, 1), "their places in B are -1 0 2 1 (they are " + Four(quad.partnerCorners) + ")");

	memset(&quad, 0x55, sizeof(quad));
	Check(model::TextureQuadOf(down, 0, 1, &quad) == model::TextureQuadResult::Ok, "A with B (-z): Ok");
	Check(Is(quad.points, 1, 2, 3, 0), "its points are 1 2 3 0 (they are " + Four(quad.points) + ")");
	Check(Is(quad.ownCorners, 1, 2, -1, 0), "their places in A are 1 2 -1 0 (they are " + Four(quad.ownCorners) + ")");
	Check(Is(quad.partnerCorners, -1, 1, 2, 0), "their places in B are -1 1 2 0 (they are " + Four(quad.partnerCorners) + ")");

	memset(&quad, 0x55, sizeof(quad));
	Check(model::TextureQuadOf(up, 0, -1, &quad) == model::TextureQuadResult::Ok, "A alone: Ok");
	Check(Is(quad.points, 2, 1, 0, -1) && Is(quad.ownCorners, 2, 1, 0, -1) && Is(quad.partnerCorners, -1, -1, -1, -1),
		"A alone is its corners 2 1 0, places 2 1 0, none in a partner (" + Four(quad.points) + " / " + Four(quad.ownCorners) + " / " + Four(quad.partnerCorners) + ")");

	puts("2. a partner that can't be used");

	int32_t const refused[4] = { 7, 0, 3, 2 };
	char const *const why[4] = { "a partner outside the shape", "A itself", "a partner of four corners", "a partner with all three of A's corners (INTENG BUG, FIXED)" };

	for (int i = 0; i < 4; i++)
	{
		memset(&quad, 0x55, sizeof(quad));
		bool const alone = model::TextureQuadOf(up, 0, refused[i], &quad) == model::TextureQuadResult::Alone;
		Check(alone && Is(quad.points, 2, 1, 0, -1) && Is(quad.ownCorners, 2, 1, 0, -1) && Is(quad.partnerCorners, -1, -1, -1, -1), std::string(why[i]) + ": A alone");
	}

	memset(&quad, 0x55, sizeof(quad));
	Check(model::TextureQuadOf(up, 3, -1, &quad) == model::TextureQuadResult::NotATriangle && quad.points[0] == 0x55555555, "a polygon of four corners of its own: NotATriangle, nothing written");
	Check(model::TextureQuadOf(up, 4, -1, &quad) == model::TextureQuadResult::NoPolygon && model::TextureQuadOf(up, -1, -1, &quad) == model::TextureQuadResult::NoPolygon,
		"a polygon outside the shape: NoPolygon");
	Check(!model::TextureTurnsForward(up, 4) && !model::TextureTurnsForward(up, -1), "and it doesn't turn either way");
}

static void TheLength()
{
	puts("3. a vector made 65536 long (FUN_004fd750)");

	int32_t v[3] = { 3, 4, 0 };
	model::TextureNormaliseTo65536(v);
	Check(v[0] == 39322 && v[1] == 52429 && v[2] == 0, "(3, 4, 0) is (39322, 52429, 0) (it is " + std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]) + ")");

	int32_t zero[3] = { 0, 0, 0 };
	model::TextureNormaliseTo65536(zero);
	Check(zero[0] == INT32_MIN && zero[1] == INT32_MIN && zero[2] == INT32_MIN, "(0, 0, 0) is 0x80000000 three times, the x87's answer to 0 / 0");

	int32_t down[3] = { 0, 0, -7 };
	model::TextureNormaliseTo65536(down);
	Check(down[0] == 0 && down[1] == 0 && down[2] == -65536, "(0, 0, -7) is (0, 0, -65536)");
}

static model::UvList List(float u0, float v0, float u1, float v1, float u2, float v2)
{
	model::UvList list = {};

	list.cornerCount = 3;
	list.corners[0] = { u0, v0 };
	list.corners[1] = { u1, v1 };
	list.corners[2] = { u2, v2 };
	return list;
}

static bool ListIs(model::UvList const &list, float u0, float v0, float u1, float v1, float u2, float v2)
{
	return list.cornerCount == 3 && list.corners[0].u == u0 && list.corners[0].v == v0 && list.corners[1].u == u1 && list.corners[1].v == v1 && list.corners[2].u == u2 &&
		list.corners[2].v == v2;
}

static std::string Words(model::UvList const &list)
{
	std::string words;

	for (int i = 0; i < list.cornerCount; i++)
	{
		char one[64];
		snprintf(one, sizeof(one), "%s(%g, %g)", i ? " " : "", list.corners[i].u, list.corners[i].v);
		words += one;
	}

	return words;
}

static void TheWrap()
{
	puts("4. the coordinates brought onto the picture (FUN_00566b90)");

	{
		model::UvList own = List(70, -5, 80, 10, 90, 20);
		bool moved = false;

		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 64, 32, &moved) == model::UvModelResult::Ok && moved, "u from 70 and v from -5 on a 64 x 32 picture: moved");
		Check(ListIs(own, 6, 27, 16, 42, 26, 52), "u by -64, v by +32: (6, 27) (16, 42) (26, 52) (it is " + Words(own) + ")");
	}

	{
		model::UvList own = List(10, 0, 20, 0, 30, 0);
		model::UvList partner = List(-70, 40, 5, 40, 5, 40);
		bool moved = false;

		Check(model::TextureWrapUv(&own, 3, &partner, 3, 64, 32, &moved) == model::UvModelResult::Ok && moved, "with a partner whose u goes down to -70: moved");
		Check(ListIs(own, 138, 0, 148, 0, 158, 0) && ListIs(partner, 58, 40, 133, 40, 133, 40),
			"both by u +128, v as it was (the smallest v, 0, is on the picture): " + Words(own) + " / " + Words(partner));
	}

	{
		model::UvList own = List(0.5f, 0, 10, 5, 63, 31);
		bool moved = true;

		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 64, 32, &moved) == model::UvModelResult::Ok && !moved && ListIs(own, 0.5f, 0, 10, 5, 63, 31),
			"already on the picture: nothing moved, the fraction kept");
	}

	{
		model::UvList own = List(64.7f, 1, 65.2f, 1, 70.9f, 1);
		bool moved = false;

		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 64, 64, &moved) == model::UvModelResult::Ok && moved && ListIs(own, 0, 1, 1, 1, 6, 1),
			"u from 64.7 (64 cut toward 0): every corner made whole and moved by -64: " + Words(own));
	}

	{
		model::UvList own = List(70, 0, 80, 0, 90, 0);
		model::UvList const before = own;
		bool moved = true;

		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 0, 32, &moved) == model::UvModelResult::BadWindow && !moved && ListIs(own, 70, 0, 80, 0, 90, 0),
			"a picture 0 wide: BadWindow, nothing moved (INTENG BUG, FIXED: inteng divides by it)");
		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 64, -1, &moved) == model::UvModelResult::BadWindow && ListIs(own, 70, 0, 80, 0, 90, 0), "a picture -1 high: BadWindow");
		(void)before;
	}

	{
		model::UvList own = List(-16000, 0, 16383, 0, 0, 0);
		bool moved = true;

		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 64, 32, &moved) == model::UvModelResult::BadTexel && !moved && ListIs(own, -16000, 0, 16383, 0, 0, 0),
			"a move that takes a corner past 16383: BadTexel, nothing moved");
	}

	{
		model::UvList own = {};
		model::UvList partner = {};
		bool moved = true;

		Check(model::TextureWrapUv(&own, 3, &partner, 3, 64, 32, &moved) == model::UvModelResult::Ok && !moved, "two empty lists: nothing to move");
		Check(model::TextureWrapUv(nullptr, 3, nullptr, 0, 64, 32, &moved) == model::UvModelResult::BadCall, "no list: BadCall");
	}

	{
		// A triangle whose list has a 4th corner: inteng's record holds 3 (u, v) pairs, so the 4th is neither the smallest nor moved (the review's case)
		model::UvList own = List(10, 0, 20, 0, 30, 0);
		bool moved = true;

		own.cornerCount = 4;
		own.corners[3] = { -100, 0 };
		Check(model::TextureWrapUv(&own, 3, nullptr, 0, 64, 64, &moved) == model::UvModelResult::Ok && !moved && own.corners[3].u == -100 && own.corners[0].u == 10,
			"a triangle's list with a 4th corner at u -100: nothing moved, the 4th left as it is");

		model::UvList quad = own;
		Check(model::TextureWrapUv(&quad, 4, nullptr, 0, 64, 64, &moved) == model::UvModelResult::Ok && moved && quad.corners[3].u == 28 && quad.corners[0].u == 138,
			"the file's own quad (four corners, DECIDED): all four looked at and moved by +128 (u " + std::to_string((int)quad.corners[0].u) + " ... " +
			std::to_string((int)quad.corners[3].u) + ")");
	}
}

int main()
{
	TheQuad();
	TheLength();
	TheWrap();

	printf("texture_quad_test: %s, %d failed\n", checksFailed ? "FAILED" : "passed", checksFailed);
	return checksFailed ? 1 : 0;
}
