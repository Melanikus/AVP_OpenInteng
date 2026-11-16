// The test for the numbers of inteng's Write Asc File (asc_export.h): the offset, the vertex transform, the faces' edge flags and the marker box.
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "asc_export.h"
#include "save_copy.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static AscShape Triangle()
{
	AscShape shape;
	AscPoint const points[3] = { { -5, 2, -3 }, { 4, -7, 1 }, { 0, 0, 9 } };

	shape.points.assign(points, points + 3);

	AscPolygon polygon = { { 0, 1, 2 }, -1 };

	shape.polygons.push_back(polygon);
	return shape;
}

int main()
{
	puts("The offset:");
	{
		AscShape const shape = Triangle();
		std::vector<AscShape const *> shapes = { &shape, nullptr };
		AscOffset const offset = AscOffsetOf(shapes);

		// the largest negated coordinate, never below 0: x -> 5, y -> 7, z -> 3
		Check(offset.x == 5 && offset.y == 7 && offset.z == 3, "the offset is the largest negated x, y and z");

		AscShape positive;
		AscPoint const p = { 1, 2, 3 };

		positive.points.push_back(p);
		std::vector<AscShape const *> only = { &positive };
		AscOffset const none = AscOffsetOf(only);

		Check(none.x == 0 && none.y == 0 && none.z == 0, "all coordinates positive: an offset of 0");
	}

	puts("The vertices:");
	{
		AscShape const shape = Triangle();
		AscOffset const offset = { 5, 7, 3 };
		int32_t const place[3] = { 10, 20, 30 };
		AscObject const object = MakeAscObject(shape, place, offset, 0.5);

		// point 0 = (-5, 2, -3): x' = (-5+5+10 + .5) * .5 = 5.25; y' (the z) = (-3+3+30 + .5) * .5 = 15.25; z' (the negated y) = -((2+7+20 + .5) * .5) = -14.75
		Check(object.vertices.size() == 3, "a vertex for each point");
		Check(object.vertices[0].x == 5.25 && object.vertices[0].y == 15.25 && object.vertices[0].z == -14.75, "X is the x, Y is the z, Z is the negated y");
	}

	puts("The faces' edges:");
	{
		AscShape shape;
		AscPoint const p = { 0, 0, 0 };

		for (int i = 0; i < 5; i++)
		{
			shape.points.push_back(p);
		}

		// polygon 0 = (0,1,2); polygon 1 = (2,0,3) merged with 0: corner 3 is the only one polygon 0 lacks... polygon 1's corner 3 is not in 0's
		AscPolygon const a = { { 0, 1, 2 }, -1 };
		AscPolygon const b = { { 2, 0, 3 }, 0 };
		AscPolygon const c = { { 0, 1, 2 }, 0 };    // all corners shared with polygon 0: nothing is joined away
		AscPolygon const d = { { 3, 4, 0 }, 0 };    // two corners polygon 0 lacks: it breaks out, nothing is joined away
		AscPolygon const e = { { 0, 1, 2 }, 99 };   // a merge number past the shape: as if none

		shape.polygons = { a, b, c, d, e };

		int32_t const place[3] = { 0, 0, 0 };
		AscOffset const offset = { 0, 0, 0 };
		AscObject const object = MakeAscObject(shape, place, offset, 1.0);

		Check(object.faces.size() == 5, "a face for each polygon");
		Check(object.faces[0].ab == 1 && object.faces[0].bc == 1 && object.faces[0].ca == 1, "no merge data: all three edges 1");
		// polygon 1's corner index 2 (point 3) is the one 0 lacks, so the edge (2 + 1) % 3 = 0, which is AB, is 0
		Check(object.faces[1].ab == 0 && object.faces[1].bc == 1 && object.faces[1].ca == 1, "one corner the other lacks: the edge opposite it is 0");
		Check(object.faces[2].ab == 1 && object.faces[2].bc == 1 && object.faces[2].ca == 1, "all corners shared: all 1");
		Check(object.faces[3].ab == 1 && object.faces[3].bc == 1 && object.faces[3].ca == 1, "two corners the other lacks: all 1");
		Check(object.faces[4].ab == 1 && object.faces[4].bc == 1 && object.faces[4].ca == 1, "a merge number outside the shape is ignored");
		Check(object.faces[1].a == 2 && object.faces[1].b == 0 && object.faces[1].c == 3, "the corners are A, B, C");
	}

	puts("The marker box:");
	{
		AscOffset const offset = { 0, 0, 0 };
		AscMarker const marker = MakeAscMarker(offset, 1.0);

		// the constants decode to +-1.2832 and +-2.5664 (0x3ff487b1c0010c6f and 0x400487b1c0010c6f)
		Check(marker.vertices[0].x < -1.28 && marker.vertices[0].x > -1.29 && marker.vertices[1].x > 1.28 && marker.vertices[1].x < 1.29, "the marker's x is -1.283 then 1.283");
		Check(marker.vertices[0].y == 0 && marker.vertices[0].z > 2.56 && marker.vertices[0].z < 2.57 && marker.vertices[2].z < -2.56, "its y is 0 and its z is 2.566 then -2.566");

		AscOffset const moved = { 2, 3, 4 };
		AscMarker const shifted = MakeAscMarker(moved, 10.0);

		// X = offset.x * scale + x; Y = offset.z * scale + y; Z = z - offset.y * scale
		Check(shifted.vertices[0].x == 20.0 + marker.vertices[0].x && shifted.vertices[0].y == 40.0 && shifted.vertices[0].z == marker.vertices[0].z - 30.0,
			"the offset moves X by the x, Y by the z and Z by the negated y, times the scale");
	}

	puts("Where an export goes:");
	{
		std::string path;
		std::string folder;
		char const *why = nullptr;

		Check(SaveExportName("my level.1", ".asc") == "my_level_1.asc", "a typed name is made safe and gets the extension");
		Check(SaveExportName("", ".asc") == "export.asc" && SaveExportName(nullptr, ".asc") == "export.asc", "no name gives \"export\"");
		Check(SaveExportPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", "plan", ".asc", "C:\\Editor\\", "C:\\Games\\AvP", &path, &folder, &why) &&
			path == "C:\\Editor\\edited\\plan.asc" && folder == "C:\\Editor\\edited\\", "an export goes to <editor folder>\\edited\\<name>.asc");
		Check(!SaveExportPlace("C:\\Games\\AvP\\avp_rifs\\Invasion.RIF", "plan", ".asc", "C:\\Games\\AvP\\editor\\", "C:\\Games\\AvP", &path, &folder, &why) &&
			path.empty(), "never inside the game's folder");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
