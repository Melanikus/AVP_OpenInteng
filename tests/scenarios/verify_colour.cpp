// verify_colour.cpp: the Texture Map colour scenarios' check (Shift+K / Ctrl+K, TM-B). Usage: verify_colour.exe <copy before> <copy after> <count> <colour word, hex>: the game's own loader
// takes both; every shape has the same points and polygons in both; EXACTLY <count> polygons differ, and each of them is now a plain polygon (engine type 2) of that colour word with its
// corners, normal and flags as they were; every other polygon is number for number the same. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"

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

// every shape of a level by its number
static bool ReadShapes(char const *path, std::map<int32_t, model::ShapeModel> *shapes)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		return false;
	}

	List<Object_Chunk *> all;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		model::ShapeModel shape;
		std::string why;

		if (object && object->get_assoc_shape() && host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &why) == host::ShapeEditResult::Done)
		{
			(*shapes)[shape.number] = shape;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		printf("usage: verify_colour.exe <copy before> <copy after> <count> <colour word, hex>\n");
		return 2;
	}

	long const wanted = atol(argv[3]);
	int32_t const colour = (int32_t)strtoul(argv[4], nullptr, 16);
	std::map<int32_t, model::ShapeModel> before, after;

	Check(ReadShapes(argv[1], &before) && !before.empty(), "the game's loader takes the copy before");
	Check(ReadShapes(argv[2], &after) && !after.empty(), "the game's loader takes the copy after");
	Check(before.size() == after.size(), "the same shapes (" + std::to_string(before.size()) + " and " + std::to_string(after.size()) + ")");

	long changed = 0, plainOfColour = 0, kept = 0;
	size_t geometry = 0;

	for (auto const &one : before)
	{
		auto const other = after.find(one.first);

		if (other == after.end() || other->second.polygons.size() != one.second.polygons.size() || other->second.points.size() != one.second.points.size() ||
			(!one.second.points.empty() && memcmp(one.second.points.data(), other->second.points.data(), one.second.points.size() * sizeof(one.second.points[0])) != 0))
		{
			geometry++;
			continue;
		}

		for (size_t p = 0; p < one.second.polygons.size(); p++)
		{
			model::ShapePolygon const &was = one.second.polygons[p], &now = other->second.polygons[p];

			if (memcmp(&was, &now, sizeof(was)) == 0)
			{
				continue;
			}

			changed++;
			plainOfColour += now.engineType == 2 && now.colour == colour ? 1 : 0;
			kept += now.flags == was.flags && now.normalIndex == was.normalIndex && now.cornerCount == was.cornerCount && memcmp(now.corners, was.corners, sizeof(was.corners)) == 0 ? 1 : 0;
			printf("  shape %d polygon %zu: type %d colour %08X -> type %d colour %08X\n", one.first, p, was.engineType, (unsigned)was.colour, now.engineType, (unsigned)now.colour);
		}
	}

	Check(geometry == 0, "every shape's points and polygon count are as they were (" + std::to_string(geometry) + " differ)");
	Check(changed == wanted, "exactly " + std::to_string(wanted) + " polygons changed (" + std::to_string(changed) + ")");
	Check(plainOfColour == changed, "each is a plain polygon (type 2) of the colour " + std::string(argv[4]) + " (" + std::to_string(plainOfColour) + " of " + std::to_string(changed) + ")");
	Check(kept == changed, "each kept its corners, normal and flags (" + std::to_string(kept) + " of " + std::to_string(changed) + ")");

	printf(failed ? "SOME CHECKS FAILED\n" : "ALL CHECKS PASSED\n");
	return failed ? 1 : 0;
}
