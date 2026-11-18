// verify_polyflags: two saves of one level hold the same polygon flags and types in every object's shape, read through the game's chunk library (as verify_texmap). Written 2026-11-17 by
// Melanikus (Drake) for AVP_OpenInteng (Object Placement's lighting, OP-3 / OP-44: the look sets the game's own item bits, iflag_nolight, and none of it may reach the file). AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_polyflags.exe <copy A> <copy B>
// CHECKS: both copies open; they hold the same objects (by ID); for every object whose shape reads (ShapeEdit_Read), the same number of polygons, and each polygon's flags word and engine type are
// the same in both. Two saves are never the same BYTES (each save runs prepare_for_output: version numbers go up, see verify_texmap's baseline), so this compares what the polygons hold.
// Gives back 0 only if every check passed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

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

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("usage: verify_polyflags <copy A> <copy B>\n");
		return 2;
	}

	OpenLevelFile a, b;

	Check(a.Open(argv[1]) == LevelOpenResult::Opened, "copy A opens (the game's loader takes it)");
	Check(b.Open(argv[2]) == LevelOpenResult::Opened, "copy B opens");

	if (failed)
	{
		return 1;
	}

	std::map<std::string, Object_Chunk *> objectsA, objectsB;

	for (int pass = 0; pass < 2; pass++)
	{
		OpenLevelFile &file = pass == 0 ? a : b;
		List<Object_Chunk *> list;

		file.Level()->list_objects(&list);

		for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
		{
			if (each())
			{
				(pass == 0 ? objectsA : objectsB)[std::string((char const *)&each()->object_data.ID, 8)] = each();
			}
		}
	}

	Check(objectsA.size() == objectsB.size(), "the same number of objects: " + std::to_string(objectsA.size()) + " and " + std::to_string(objectsB.size()));

	size_t shapes = 0, polygons = 0, countsDiffer = 0, flagsDiffer = 0, missing = 0;
	std::string firstDiffer;

	for (auto const &entry : objectsA)
	{
		if (!objectsB.count(entry.first))
		{
			missing++;
			continue;
		}

		uint8_t id[8];

		memcpy(id, entry.first.data(), 8);

		model::ShapeModel sa, sb;
		std::string why;

		if (host::ShapeEdit_Read(a.Level(), &a, id, &sa, nullptr, &why) != host::ShapeEditResult::Done || host::ShapeEdit_Read(b.Level(), &b, id, &sb, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		shapes++;

		if (sa.polygons.size() != sb.polygons.size())
		{
			countsDiffer++;
			continue;
		}

		for (size_t p = 0; p < sa.polygons.size(); p++)
		{
			polygons++;

			if (sa.polygons[p].flags != sb.polygons[p].flags || sa.polygons[p].engineType != sb.polygons[p].engineType)
			{
				if (flagsDiffer++ == 0)
				{
					char line[160];

					_snprintf_s(line, sizeof(line), _TRUNCATE, "the first: object %s, polygon %zu, flags 0x%x / 0x%x, type %d / %d", entry.second->object_data.o_name, p, (unsigned)sa.polygons[p].flags,
						(unsigned)sb.polygons[p].flags, (int)sa.polygons[p].engineType, (int)sb.polygons[p].engineType);
					firstDiffer = line;
				}
			}
		}
	}

	printf("  %zu shapes, %zu polygons compared\n", shapes, polygons);
	Check(missing == 0, "every object of copy A is in copy B (" + std::to_string(missing) + " missing)");
	Check(shapes > 0, "some shape was read");
	Check(countsDiffer == 0, "every shape has the same number of polygons in both (" + std::to_string(countsDiffer) + " differ)");
	Check(flagsDiffer == 0, "every polygon's flags and type are the same in both (" + std::to_string(flagsDiffer) + " differ" + (firstDiffer.empty() ? std::string() : "; " + firstDiffer) + ")");
	return failed ? 1 : 0;
}
