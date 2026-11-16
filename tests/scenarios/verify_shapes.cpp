// verify_shapes.cpp: the check program of the Construction scenarios for Fix tears and Recentre Shape (tests/scenarios/construction-fix-tears.txt, texture-map-recentre.txt): a level edited in the running
// editor and saved by Save a copy is read back through the game's own loader and compared with the original, object by object, by where the shapes' points are in the WORLD. Written 2026-11-15 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_shapes <the original level> <the saved copy> tears|recentre
//   tears:    the copy loads, and at least one object has a point that is somewhere else in the world (the weld moved it), none by more than the tolerance given as the fourth argument (default 5) on any axis
//   recentre: the copy loads, at least one shape's own points differ from the original's (the shape moved) and every object's points are where they were in the world (within 3: the place is rounded)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "chunk_object.hpp"
#include "level_file.h"
#include "file_pick.h"
#include "shape_model.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

using namespace host;

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

struct Held
{
	std::vector<int32_t> world;
	std::vector<model::ShapePoint> local;
};

static bool Collect(OpenLevelFile *file, std::map<std::string, Held> *out)
{
	List<Object_Chunk *> found;

	file->Level()->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		uint8_t id[8];

		memcpy(id, &each()->object_data.ID, 8);

		model::ShapeModel shape;
		Held held;

		std::vector<int32_t> normals;

		if (FilePick_World(file, file->Level(), id, &shape, &held.world, &normals) != FilePickResult::Done)
		{
			continue;
		}

		held.local = shape.points;
		(*out)[std::string((char const *)id, 8)] = held;
	}

	return !out->empty();
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 4)
	{
		printf("usage: verify_shapes <original> <copy> tears|recentre [tolerance]\n");
		return 2;
	}

	OpenLevelFile original, copy;

	if (original.Open(argv[1]) != LevelOpenResult::Opened)
	{
		Check(false, std::string("the original opens: ") + argv[1]);
		return 1;
	}

	Check(copy.Open(argv[2]) == LevelOpenResult::Opened, std::string("the saved copy is taken by the game's own loader: ") + argv[2]);

	if (failed)
	{
		return 1;
	}

	std::map<std::string, Held> before, after;

	Check(Collect(&original, &before) && Collect(&copy, &after), "the shapes of both levels are read");
	Check(before.size() == after.size(), "the same objects with shapes in both (" + std::to_string((unsigned long long)before.size()) + ")");

	bool const tears = strcmp(argv[3], "tears") == 0;
	int32_t const tolerance = argc > 4 ? atoi(argv[4]) : 5;
	size_t movedWorld = 0, movedLocal = 0, tooFar = 0, worldShifted = 0;

	for (auto const &each : before)
	{
		auto const other = after.find(each.first);

		if (other == after.end() || other->second.world.size() != each.second.world.size())
		{
			continue;
		}

		bool anyWorld = false;
		bool anyLocal = false;
		int32_t worst = 0;

		for (size_t i = 0; i < each.second.world.size(); i++)
		{
			int32_t const gap = abs(each.second.world[i] - other->second.world[i]);

			anyWorld = anyWorld || gap != 0;
			worst = gap > worst ? gap : worst;
		}

		for (size_t i = 0; i < each.second.local.size() && i < other->second.local.size(); i++)
		{
			anyLocal = anyLocal || each.second.local[i].x != other->second.local[i].x || each.second.local[i].y != other->second.local[i].y || each.second.local[i].z != other->second.local[i].z;
		}

		movedWorld += anyWorld ? 1 : 0;
		movedLocal += anyLocal ? 1 : 0;
		tooFar += worst > 3 * tolerance ? 1 : 0;
		worldShifted += worst > 3 ? 1 : 0;
	}

	printf("  (%zu objects with points somewhere else in the world, %zu with their own points changed)\n", movedWorld, movedLocal);

	if (tears)
	{
		Check(movedWorld > 0, "the weld moved points: at least one object has a point somewhere else");
		Check(tooFar == 0, "and no point moved by more than three times the tolerance (" + std::to_string((int)tolerance) + ": a point can be welded more than once) on any axis");
	}
	else
	{
		Check(movedLocal > 0, "the shape moved: at least one shape's own points changed");
		Check(worldShifted == 0, "and every point is where it was in the world (within 3)");
	}

	return failed ? 1 : 0;
}
