// verify_align: the Texture Alignment scenario's check (acceptance-texture-align.txt). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_align.exe <level> <object name> <polygon a> <polygon b> <same|differ>
//   The object's shape (the first object with that name), polygons a and b: at every point a and b both have (the same point number of the shape), their texture
//   coordinates, as whole numbers, are all equal ("same") or at least one differs ("differ"). There must be at least one such point.
// Gives back 0 when it holds, 1 when it doesn't, 2 for a bad call.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "uv_edit.h"

#include "arg_number.h"

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("  the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("  FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

int main(int argc, char **argv)
{
	if (argc < 6)
	{
		printf("usage: verify_align <level> <object name> <polygon a> <polygon b> <same|differ>\n");
		return 2;
	}

	long a = 0, b = 0;

	if (!ArgNumber("polygon a", argv[3], 0, 65535, &a) || !ArgNumber("polygon b", argv[4], 0, 65535, &b) || strlen(argv[2]) > 200)
	{
		return 2;
	}

	bool const same = strcmp(argv[5], "same") == 0;
	OpenLevelFile file;

	Check(file.Open(argv[1]) == LevelOpenResult::Opened, "the level opens");

	if (failed)
	{
		return 1;
	}

	uint8_t id[8];
	bool have = false;
	List<Object_Chunk *> list;

	file.Level()->list_objects(&list);

	for (LIF<Object_Chunk *> each(&list); !each.done() && !have; each.next())
	{
		Object_Chunk *const object = each();

		if (object && strlen(argv[2]) < sizeof(object->object_data.o_name) && strncmp(object->object_data.o_name, argv[2], sizeof(object->object_data.o_name)) == 0)
		{
			memcpy(id, &object->object_data.ID, 8);
			have = true;
		}
	}

	Check(have, std::string("an object is named ") + argv[2]);

	if (failed)
	{
		return 1;
	}

	model::ShapeModel shape;
	model::PolygonUv uvA, uvB;
	std::string why;

	Check(host::ShapeEdit_Read(file.Level(), &file, id, &shape, nullptr, &why) == host::ShapeEditResult::Done && (size_t)a < shape.polygons.size() &&
		(size_t)b < shape.polygons.size(), "its shape has both polygons");
	Check(!failed && host::UvEdit_ReadPolygon(&file, file.Level(), id, (int32_t)a, &uvA, &why) == host::UvEditResult::Done && uvA.textured &&
		host::UvEdit_ReadPolygon(&file, file.Level(), id, (int32_t)b, &uvB, &why) == host::UvEditResult::Done && uvB.textured, "both are textured");

	if (failed)
	{
		return 1;
	}

	model::ShapePolygon const &pa = shape.polygons[(size_t)a];
	model::ShapePolygon const &pb = shape.polygons[(size_t)b];
	int shared = 0, differing = 0;

	for (int i = 0; i < pa.cornerCount && i < uvA.uv.cornerCount; i++)
	{
		for (int j = 0; j < pb.cornerCount && j < uvB.uv.cornerCount; j++)
		{
			if (pa.corners[i] != pb.corners[j])
			{
				continue;
			}

			shared++;

			double const ua = trunc(uvA.uv.corners[i].u), va = trunc(uvA.uv.corners[i].v), ub = trunc(uvB.uv.corners[j].u), vb = trunc(uvB.uv.corners[j].v);

			printf("    point %d: polygon %ld has %g %g, polygon %ld has %g %g\n", pa.corners[i], a, ua, va, b, ub, vb);
			differing += (ua != ub || va != vb) ? 1 : 0;
		}
	}

	Check(shared > 0, "the two polygons share " + std::to_string(shared) + " points");

	if (same)
	{
		Check(differing == 0, "their coordinates are the same at every point they share");
	}
	else
	{
		Check(differing > 0, "their coordinates differ at " + std::to_string(differing) + " of the points they share");
	}

	printf(failed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failed ? 1 : 0;
}
