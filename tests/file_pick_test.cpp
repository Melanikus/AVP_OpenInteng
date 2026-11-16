// The test for the FILE-shape pick (file_pick.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt.
// No window, none of the game: shapes are built by hand as the shape model's numbers.
//
// WHAT IT TESTS: a ray through the centre of each face of a cube gives that face's number and the centre as the hit point (from outside and from inside); a quad is
// one polygon (a ray near each of its two halves gives the same number); the nearest of two polygons wins; a polygon behind the ray's start is not hit; a ray
// parallel to the plane is not hit; the hit point's numbers are cut toward 0; a place and a turn move the polygons (checked against the shape maths' own turn);
// a pentagon works; a shape with no normals takes the corners' own; and hostile numbers (a ray with no direction or a NaN, a corner that doesn't exist, a polygon of
// 2 corners) give "no hit", never a crash.

#include <limits>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "chunk.hpp"
#include "file_pick.h"
#include "shape_maths.h"

using namespace host;

// ---- Stand-ins for the parts of the game the chunk code calls (the shape edit store links the chunk code): files are read by Windows' own

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static model::ShapePolygon Polygon(int a, int b, int c, int d = -1, int e = -1)
{
	model::ShapePolygon polygon;

	memset(&polygon, 0, sizeof(polygon));
	polygon.corners[0] = a;
	polygon.corners[1] = b;
	polygon.corners[2] = c;
	polygon.corners[3] = d;
	polygon.corners[4] = e;
	polygon.cornerCount = e >= 0 ? 5 : (d >= 0 ? 4 : 3);
	polygon.normalIndex = -1;
	return polygon;
}

static void AddPoint(model::ShapeModel *shape, int32_t x, int32_t y, int32_t z)
{
	model::ShapePoint point = { x, y, z };

	shape->points.push_back(point);
}

// A polygon with its own normal added
static void AddPolygon(model::ShapeModel *shape, model::ShapePolygon polygon, float nx, float ny, float nz)
{
	model::ShapeNormal normal = { nx, ny, nz };

	polygon.normalIndex = (int32_t)shape->polygonNormals.size();
	shape->polygonNormals.push_back(normal);
	shape->polygons.push_back(polygon);
}

static float const kNoTurn[4] = { 0, 0, 0, 1 };
static int32_t const kNoPlace[3] = { 0, 0, 0 };

static FilePickRay Ray(double fx, double fy, double fz, double dx, double dy, double dz)
{
	FilePickRay ray = { { fx, fy, fz }, { dx, dy, dz } };

	return ray;
}

static bool Is3(int32_t const *got, int32_t a, int32_t b, int32_t c)
{
	return got[0] == a && got[1] == b && got[2] == c;
}

// A cube from -1000 to 1000, its 6 faces as quads, numbered 0..5 as: -x, +x, -y, +y, -z, +z (the corner order is a plain walk round each face)
static model::ShapeModel Cube()
{
	model::ShapeModel shape;

	for (int i = 0; i < 8; i++)
	{
		AddPoint(&shape, (i & 1) ? 1000 : -1000, (i & 2) ? 1000 : -1000, (i & 4) ? 1000 : -1000);
	}

	AddPolygon(&shape, Polygon(0, 4, 6, 2), -1, 0, 0);
	AddPolygon(&shape, Polygon(1, 3, 7, 5), 1, 0, 0);
	AddPolygon(&shape, Polygon(0, 1, 5, 4), 0, -1, 0);
	AddPolygon(&shape, Polygon(2, 6, 7, 3), 0, 1, 0);
	AddPolygon(&shape, Polygon(0, 2, 3, 1), 0, 0, -1);
	AddPolygon(&shape, Polygon(4, 5, 7, 6), 0, 0, 1);
	return shape;
}

static void TestCube()
{
	puts("Rays through the centres of a cube's faces:");

	model::ShapeModel const shape = Cube();
	double const centre[6][3] = { { -1000, 0, 0 }, { 1000, 0, 0 }, { 0, -1000, 0 }, { 0, 1000, 0 }, { 0, 0, -1000 }, { 0, 0, 1000 } };
	double const outward[6][3] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };

	for (int face = 0; face < 6; face++)
	{
		FilePickHit hit;
		FilePickRay const fromOutside = Ray(centre[face][0] + outward[face][0] * 5000, centre[face][1] + outward[face][1] * 5000, centre[face][2] + outward[face][2] * 5000,
			-outward[face][0], -outward[face][1], -outward[face][2]);
		bool const ok = FilePick_Shape(shape, kNoPlace, kNoTurn, fromOutside, &hit);

		Check(ok && hit.found && hit.polygon == face, "face " + std::to_string(face) + " from outside gives polygon " + std::to_string(face));
		Check(Is3(hit.point, (int32_t)centre[face][0], (int32_t)centre[face][1], (int32_t)centre[face][2]), "  its hit point is the face's centre");
		Check(fabs(hit.normal[0] - outward[face][0]) < 1e-9 && fabs(hit.normal[1] - outward[face][1]) < 1e-9 && fabs(hit.normal[2] - outward[face][2]) < 1e-9,
			"  its plane's normal is the face's, 1 long");
		Check(fabs(hit.distance - 5000.0) < 1e-6 && hit.cornerCount == 4, "  the distance along the ray is 5000, 4 corners");

		// from the middle of the cube, outward: the face is looked at from the inside
		FilePickRay const fromInside = Ray(0, 0, 0, outward[face][0], outward[face][1], outward[face][2]);
		bool const ok2 = FilePick_Shape(shape, kNoPlace, kNoTurn, fromInside, &hit);

		Check(ok2 && hit.found && hit.polygon == face, "  from inside the same polygon");
	}
}

static void TestQuadIsOne()
{
	puts("A quad is one polygon, not two triangles:");

	model::ShapeModel const shape = Cube();
	FilePickHit hit;

	// the +z face, quad 4 5 7 6 = (-1000,-1000), (1000,-1000), (1000,1000), (-1000,1000): the two halves of a split along either diagonal
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(900, -900, 3000, 0, 0, -1), &hit);
	Check(hit.found && hit.polygon == 5, "near one corner: polygon 5");
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(-900, 900, 3000, 0, 0, -1), &hit);
	Check(hit.found && hit.polygon == 5, "near the opposite corner: polygon 5 again");
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(900, 900, 3000, 0, 0, -1), &hit);
	Check(hit.found && hit.polygon == 5, "near a third corner: polygon 5 again");
}

static void TestMisses()
{
	puts("Rays that hit nothing:");

	model::ShapeModel const shape = Cube();
	FilePickHit hit;

	bool ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(1500, 0, 3000, 0, 0, -1), &hit);
	Check(ok && !hit.found, "a ray beside the cube");

	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 3000, 0, 0, 1), &hit);
	Check(ok && !hit.found, "a ray pointing away from the cube");

	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 2000, 1, 0, 0), &hit);
	Check(ok && !hit.found, "a ray parallel to a face's plane, above the cube");

	// one flat quad, a ray in its plane: denominator exactly 0
	model::ShapeModel flat;

	AddPoint(&flat, -1000, -1000, 0);
	AddPoint(&flat, 1000, -1000, 0);
	AddPoint(&flat, 1000, 1000, 0);
	AddPoint(&flat, -1000, 1000, 0);
	AddPolygon(&flat, Polygon(0, 1, 2, 3), 0, 0, 1);
	ok = FilePick_Shape(flat, kNoPlace, kNoTurn, Ray(-3000, 0, 0, 1, 0, 0), &hit);
	Check(ok && !hit.found, "a ray lying in the polygon's own plane (denominator exactly 0)");

	model::ShapeModel const none = model::ShapeModel();

	ok = FilePick_Shape(none, kNoPlace, kNoTurn, Ray(0, 0, 100, 0, 0, -1), &hit);
	Check(ok && !hit.found, "a shape with no polygons");
}

static void TestNearest()
{
	puts("The nearest polygon wins:");

	model::ShapeModel shape;

	for (int level = 0; level < 2; level++)
	{
		int const z = level == 0 ? 0 : 500;
		int const base = (int)shape.points.size();

		AddPoint(&shape, -1000, -1000, z);
		AddPoint(&shape, 1000, -1000, z);
		AddPoint(&shape, 1000, 1000, z);
		AddPoint(&shape, -1000, 1000, z);
		AddPolygon(&shape, Polygon(base, base + 1, base + 2, base + 3), 0, 0, 1);
	}

	FilePickHit hit;

	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 2000, 0, 0, -1), &hit);
	Check(hit.found && hit.polygon == 1 && Is3(hit.point, 0, 0, 500), "from above, the polygon at z = 500 (polygon 1) and not the one below it");
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, -2000, 0, 0, 1), &hit);
	Check(hit.found && hit.polygon == 0 && Is3(hit.point, 0, 0, 0), "from below, the polygon at z = 0 (polygon 0)");
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 250, 0, 0, 1), &hit);
	Check(hit.found && hit.polygon == 1, "from between them, looking up: polygon 1, the one in front");
}

static void TestTruncation()
{
	puts("The hit point is cut toward 0:");

	model::ShapeModel flat;

	AddPoint(&flat, -1000, -1000, 0);
	AddPoint(&flat, 1000, -1000, 0);
	AddPoint(&flat, 1000, 1000, 0);
	AddPoint(&flat, -1000, 1000, 0);
	AddPolygon(&flat, Polygon(0, 1, 2, 3), 0, 0, 1);

	FilePickHit hit;

	FilePick_Shape(flat, kNoPlace, kNoTurn, Ray(-123.7, 50.9, 100, 0, 0, -1), &hit);
	Check(hit.found && Is3(hit.point, -123, 50, 0), "(-123.7, 50.9, 0) gives (-123, 50, 0)");
	Check(Is3(hit.planePoint, -1000, -1000, 0), "the plane's point is the polygon's first corner");
}

static void TestPlaceAndTurn()
{
	puts("A place and a turn move the polygons:");

	model::ShapeModel flat;

	AddPoint(&flat, 1000, -500, -500);
	AddPoint(&flat, 1000, 500, -500);
	AddPoint(&flat, 1000, 500, 500);
	AddPoint(&flat, 1000, -500, 500);
	AddPolygon(&flat, Polygon(0, 1, 2, 3), 1, 0, 0);

	// the same shape moved by a place alone
	int32_t const place[3] = { 10000, 2000, -300 };
	FilePickHit hit;

	FilePick_Shape(flat, place, kNoTurn, Ray(20000, 2000, -300, -1, 0, 0), &hit);
	Check(hit.found && hit.polygon == 0 && Is3(hit.point, 11000, 2000, -300), "a place of (10000, 2000, -300): the centre is at x = 11000");

	// turned a quarter turn about z: wherever the shape maths puts the centre (1000, 0, 0), a ray at it from the polygon's turned normal hits it
	float const quarter[4] = { 0, 0, 0.70710678f, 0.70710678f };
	model::ShapePoint const centre = { 1000, 0, 0 };
	model::ShapePoint turned;

	Check(model::RotateByTurn(quarter, centre, &turned) == model::ShapeMathsResult::Ok, "the shape maths turns (1000, 0, 0)");

	FilePick_Shape(flat, kNoPlace, quarter, Ray(turned.x * 4.0, turned.y * 4.0, turned.z * 4.0, -turned.x, -turned.y, -turned.z), &hit);
	Check(hit.found && hit.polygon == 0, "a ray at the turned centre hits the polygon");
	Check(hit.found && abs(hit.point[0] - turned.x) <= 1 && abs(hit.point[1] - turned.y) <= 1 && abs(hit.point[2] - turned.z) <= 1, "  at the turned centre");
	Check(hit.found && fabs(hit.normal[0] * hit.normal[0] + hit.normal[1] * hit.normal[1] + hit.normal[2] * hit.normal[2] - 1.0) < 1e-6, "  with a normal 1 long");
	Check(hit.found && fabs(hit.normal[0] * turned.x + hit.normal[1] * turned.y + hit.normal[2] * turned.z - 1000.0) < 2.0, "  the normal is the turned one (it points along the turned centre)");

	// the unturned ray misses the turned polygon
	FilePick_Shape(flat, kNoPlace, quarter, Ray(4000, 0, 0, -1, 0, 0), &hit);
	Check(!hit.found || hit.point[1] != 0, "a ray along the old x axis doesn't hit the turned polygon at its old place");
}

static void TestPentagonAndNoNormals()
{
	puts("A pentagon, and a shape with no normals:");

	model::ShapeModel shape;

	// a regular-ish pentagon in the z = 0 plane, with NO normal of its own (the normal array stays empty)
	AddPoint(&shape, 0, 1000, 0);
	AddPoint(&shape, 950, 310, 0);
	AddPoint(&shape, 590, -810, 0);
	AddPoint(&shape, -590, -810, 0);
	AddPoint(&shape, -950, 310, 0);
	shape.polygons.push_back(Polygon(0, 1, 2, 3, 4));

	FilePickHit hit;

	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 900, 0, 0, -1), &hit);
	Check(hit.found && hit.polygon == 0 && hit.cornerCount == 5 && Is3(hit.point, 0, 0, 0), "a ray through a pentagon's middle hits it (5 corners)");
	Check(fabs(fabs(hit.normal[2]) - 1.0) < 1e-9, "the normal comes from the corners: along z");
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(900, 900, 900, 0, 0, -1), &hit);
	Check(!hit.found, "a ray outside the pentagon's corner doesn't");
	FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(-800, 600, 900, 0, 0, -1), &hit);
	Check(!hit.found, "nor one in the notch between two corners, outside it");
}

static void TestHostile()
{
	puts("Hostile numbers give no hit, never a crash:");

	model::ShapeModel shape = Cube();
	FilePickHit hit;

	bool ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 2000, 0, 0, 0), &hit);
	Check(!ok && !hit.found, "a ray with no direction");

	double const nan = std::numeric_limits<double>::quiet_NaN();

	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(nan, 0, 2000, 0, 0, -1), &hit);
	Check(!ok && !hit.found, "a ray starting at NaN");
	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 2000, 0, nan, -1), &hit);
	Check(!ok && !hit.found, "a ray going to NaN");
	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 2000, 0, 0, std::numeric_limits<double>::infinity()), &hit);
	Check(!ok && !hit.found, "a ray going to infinity");
	Check(!FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 2000, 0, 0, -1), nullptr), "no place for the answer");

	// a corner that doesn't exist, a polygon of 2 corners, a polygon of no area, a negative corner: each skipped, the good polygons still hit
	shape.polygons[5].corners[2] = 99;
	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 3000, 0, 0, -1), &hit);
	Check(ok && hit.found && hit.polygon == 4, "the +z face with a corner number that does not exist is skipped: the ray goes on to the -z face (polygon 4)");
	shape.polygons[5].corners[2] = 7;
	shape.polygons[4].cornerCount = 2;
	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 0, 0, 0, -1), &hit);
	Check(ok && !hit.found, "a polygon of 2 corners is skipped");
	shape.polygons[4].cornerCount = 4;
	shape.polygons[4].corners[1] = -5;
	ok = FilePick_Shape(shape, kNoPlace, kNoTurn, Ray(0, 0, 0, 0, 0, -1), &hit);
	Check(ok && !hit.found, "a polygon with a negative corner number is skipped");

	// a polygon of no area (3 points on a line), with a normal of 0: no plane, skipped
	model::ShapeModel line;

	AddPoint(&line, 0, 0, 0);
	AddPoint(&line, 100, 0, 0);
	AddPoint(&line, 200, 0, 0);
	AddPolygon(&line, Polygon(0, 1, 2), 0, 0, 0);
	ok = FilePick_Shape(line, kNoPlace, kNoTurn, Ray(100, 0, 500, 0, 0, -1), &hit);
	Check(ok && !hit.found, "a polygon with no area and no normal is skipped");
}

int main()
{
	TestCube();
	TestQuadIsOne();
	TestMisses();
	TestNearest();
	TestTruncation();
	TestPlaceAndTurn();
	TestPentagonAndNoNormals();
	TestHostile();

	if (checksFailed)
	{
		printf("%d check(s) FAILED\n", checksFailed);
		return 1;
	}

	puts("all good");
	return 0;
}
