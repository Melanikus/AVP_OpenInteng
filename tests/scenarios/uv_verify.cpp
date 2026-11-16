// uv_verify.cpp: the Copy From / To Multiple and Copy Object scenarios' check. Usage: uv_verify.exe <level> <object row A> <object row B> <same|differ> <polygon numbers...>: for each polygon number, the texture coordinates and the picture number of that polygon in object A and in object B (rows are the
// object list's order, as tests\scenarios\shape_pairs prints them) are all equal ("same") or all not equal ("differ"; at least one coordinate or the picture differs for each polygon). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
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
		printf("usage: uv_verify <level> <row A> <row B> <same|differ> <polygon>...\n");
		return 2;
	}

	OpenLevelFile file;

	Check(file.Open(argv[1]) == LevelOpenResult::Opened, "the level opens");

	if (failed)
	{
		return 1;
	}

	int const rowA = atoi(argv[2]), rowB = atoi(argv[3]);
	bool const same = strcmp(argv[4], "same") == 0;
	uint8_t ids[2][8];
	bool have[2] = { false, false };
	List<Object_Chunk *> list;
	int row = -1;

	file.Level()->list_objects(&list);

	for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		row++;

		if (object && row == rowA)
		{
			memcpy(ids[0], &object->object_data.ID, 8);
			have[0] = true;
		}

		if (object && row == rowB)
		{
			memcpy(ids[1], &object->object_data.ID, 8);
			have[1] = true;
		}
	}

	Check(have[0] && have[1], "both object rows exist");

	if (failed)
	{
		return 1;
	}

	for (int i = 5; i < argc; i++)
	{
		int const polygon = atoi(argv[i]);
		model::PolygonUv a, b;
		std::string why;
		bool const read = host::UvEdit_ReadPolygon(&file, file.Level(), ids[0], polygon, &a, &why) == host::UvEditResult::Done && host::UvEdit_ReadPolygon(&file, file.Level(), ids[1], polygon, &b, &why) == host::UvEditResult::Done && a.textured && b.textured;

		Check(read, "polygon " + std::to_string(polygon) + " is textured in both");

		if (!read)
		{
			continue;
		}

		bool equal = a.uv.cornerCount == b.uv.cornerCount && (a.colour & 0xfff) == (b.colour & 0xfff);

		for (int c = 0; equal && c < a.uv.cornerCount; c++)
		{
			equal = fabs(a.uv.corners[c].u - b.uv.corners[c].u) < 0.01 && fabs(a.uv.corners[c].v - b.uv.corners[c].v) < 0.01;
		}

		printf("    polygon %d: A uv", polygon);

		for (int c = 0; c < a.uv.cornerCount; c++)
		{
			printf(" %g %g", a.uv.corners[c].u, a.uv.corners[c].v);
		}

		printf(" picture %d; B uv", a.colour & 0xfff);

		for (int c = 0; c < b.uv.cornerCount; c++)
		{
			printf(" %g %g", b.uv.corners[c].u, b.uv.corners[c].v);
		}

		printf(" picture %d\n", b.colour & 0xfff);
		Check(equal == same, "polygon " + std::to_string(polygon) + (same ? " has the same coordinates and picture in both" : " differs between the two"));
	}

	printf("%s\n", failed ? "SOME CHECKS FAILED" : "All checks passed");
	return failed ? 1 : 0;
}
