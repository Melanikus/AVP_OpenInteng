// flag_verify.cpp: the Texture Map flags scenario's check. Usage: flag_verify.exe <the level before> <the level after (a saved copy)> <mask, hex> <on|off> <count>: exactly <count> polygons' flag words differ between the two files, each by exactly <mask> (set for "on", cleared for "off"),
// and no polygon's type, corners or colour word differs anywhere. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
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

static void Collect(OpenLevelFile &file, std::map<std::string, model::ShapeModel> *out)
{
	List<Object_Chunk *> list;

	file.Level()->list_objects(&list);

	std::set<Shape_Chunk *> seen;

	for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!shape || seen.count(shape))
		{
			continue;
		}

		seen.insert(shape);

		model::ShapeModel model;
		std::string why;

		if (host::ShapeEdit_Read(file.Level(), &file, (uint8_t const *)&object->object_data.ID, &model, nullptr, &why) == host::ShapeEditResult::Done)
		{
			(*out)[std::string((char const *)&object->object_data.ID, 8)] = model;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 6)
	{
		printf("usage: flag_verify <before> <after> <mask hex> <on|off> <count>\n");
		return 2;
	}

	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened, "the level before opens");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened, "the saved copy opens (the game's loader takes it)");

	if (failed)
	{
		return 1;
	}

	uint32_t const mask = (uint32_t)strtoul(argv[3], nullptr, 16);
	bool const on = strcmp(argv[4], "on") == 0;
	size_t const want = (size_t)atoi(argv[5]);

	std::map<std::string, model::ShapeModel> was, now;

	Collect(before, &was);
	Collect(after, &now);

	size_t changed = 0, otherChanges = 0, wrongBits = 0;

	for (auto const &entry : was)
	{
		auto const there = now.find(entry.first);

		if (there == now.end() || there->second.polygons.size() != entry.second.polygons.size())
		{
			otherChanges++;
			continue;
		}

		for (size_t p = 0; p < entry.second.polygons.size(); p++)
		{
			model::ShapePolygon const &a = entry.second.polygons[p], &b = there->second.polygons[p];

			if (a.engineType != b.engineType || a.colour != b.colour || a.cornerCount != b.cornerCount || memcmp(a.corners, b.corners, sizeof(a.corners)) != 0)
			{
				otherChanges++;
			}

			if (a.flags != b.flags)
			{
				changed++;

				uint32_t const diff = (uint32_t)a.flags ^ (uint32_t)b.flags;

				wrongBits += (diff == mask && ((on && ((uint32_t)b.flags & mask) == mask) || (!on && ((uint32_t)b.flags & mask) == 0))) ? 0 : 1;
			}
		}
	}

	printf("  %zu polygons' flag words changed\n", changed);
	Check(changed == want, std::to_string(changed) + " polygons changed their flags (wanted " + std::to_string(want) + ")");
	Check(wrongBits == 0, std::string("each changed by exactly the mask, ") + (on ? "set" : "cleared"));
	Check(otherChanges == 0, "no polygon's type, corners or colour changed anywhere");
	printf("%s\n", failed ? "SOME CHECKS FAILED" : "All checks passed");
	return failed ? 1 : 0;
}
