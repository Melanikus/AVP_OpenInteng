// verify_shapes.cpp: the check program of the Construction scenarios for Fix tears and Recentre Shape (tests/scenarios/construction-fix-tears.txt, texture-map-recentre.txt): a level edited in the running
// editor and saved by Save a copy is read back through the game's own loader and compared with the original, object by object, by where the shapes' points are in the WORLD. Written 2026-11-15 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_shapes <the original level> <the saved copy> tears|recentre | scaled <object name> bigger|smaller
//   tears:    the copy loads, and at least one object has a point that is somewhere else in the world (the weld moved it), none by more than the tolerance given as the fourth argument (default 5) on any axis
//   recentre: the copy loads, at least one shape's own points differ from the original's (the shape moved) and every object's points are where they were in the world (within 3: the place is rounded)
//   scaled:   (Object Placement's '=' and '-' keys, OP-14; added 2026-11-17 by Drake) the named object's own points are the original's times ONE ratio (the frames the key was held decide it), each
//             within what the truncations allow (each step cuts toward zero), the ratio above 1 (bigger) or below 1 (smaller); every other object's own points are as they were

#include <math.h>
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

static std::map<std::string, std::string> idOfName;   // an object's name -> its 8-byte ID (the first of a name)

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

		if (each()->object_data.o_name && idOfName.count(each()->object_data.o_name) == 0)
		{
			idOfName[each()->object_data.o_name] = std::string((char const *)id, 8);
		}

		(*out)[std::string((char const *)id, 8)] = held;
	}

	return !out->empty();
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 4)
	{
		printf("usage: verify_shapes <original> <copy> tears|recentre [tolerance] | scaled <object name> bigger|smaller\n");
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

	if (strcmp(argv[3], "scaled") == 0)
	{
		if (argc < 6)
		{
			Check(false, "scaled takes an object's name and bigger or smaller");
			return 1;
		}

		bool const bigger = strcmp(argv[5], "bigger") == 0;
		auto const named = idOfName.find(argv[4]);

		Check(named != idOfName.end(), std::string("the object ") + argv[4] + " has a shape in the original");

		if (named == idOfName.end() || after.count(named->second) == 0 || before.count(named->second) == 0)
		{
			Check(false, "and in the copy");
			return 1;
		}

		std::vector<model::ShapePoint> const &was = before[named->second].local;
		std::vector<model::ShapePoint> const &now = after[named->second].local;
		double ratio = 0.0;
		int32_t largest = 0;

		Check(was.size() == now.size() && !was.empty(), "its shape has as many points as before (" + std::to_string((unsigned long long)was.size()) + ")");

		// the ratio from the coordinate of largest size (the least cut by the truncations)
		for (size_t i = 0; i < was.size() && i < now.size(); i++)
		{
			int32_t const c[3] = { was[i].x, was[i].y, was[i].z };
			int32_t const d[3] = { now[i].x, now[i].y, now[i].z };

			for (int k = 0; k < 3; k++)
			{
				if (abs(c[k]) > largest)
				{
					largest = abs(c[k]);
					ratio = (double)d[k] / (double)c[k];
				}
			}
		}

		// each step cuts toward zero (less than 1 off), and a later step multiplies what an earlier one cut: grown by R = (10/7)^n in all, the cuts add up to less than
		// 1 + 10/7 + ... + (10/7)^(n-1) = (R - 1) x 7/3; 8 more for the shrinking steps and the ratio's own rounding
		double const tolerance = 8.0 + ((ratio > 1.0 ? ratio : 1.0) - 1.0) * 7.0 / 3.0;
		size_t off = 0;

		for (size_t i = 0; i < was.size() && i < now.size(); i++)
		{
			off += fabs((double)now[i].x - was[i].x * ratio) > tolerance || fabs((double)now[i].y - was[i].y * ratio) > tolerance || fabs((double)now[i].z - was[i].z * ratio) > tolerance ? 1 : 0;
		}

		printf("  (the ratio: %.6f, from a coordinate of %d; points within %.1f of it)\n", ratio, (int)largest, tolerance);
		Check(bigger ? ratio > 1.0 : (ratio > 0.0 && ratio < 1.0), std::string("the shape is ") + (bigger ? "bigger" : "smaller") + " than in the original");
		Check(off == 0, "every point is the original's times that ratio, within what the truncations allow (" + std::to_string((unsigned long long)off) + " are not)");

		size_t othersChanged = 0;

		for (auto const &each : before)
		{
			auto const other = after.find(each.first);

			if (each.first == named->second || other == after.end())
			{
				continue;
			}

			bool same = other->second.local.size() == each.second.local.size();

			for (size_t i = 0; same && i < each.second.local.size(); i++)
			{
				same = each.second.local[i].x == other->second.local[i].x && each.second.local[i].y == other->second.local[i].y && each.second.local[i].z == other->second.local[i].z;
			}

			othersChanged += same ? 0 : 1;
		}

		Check(othersChanged == 0, "every other object's own points are as they were (" + std::to_string((unsigned long long)othersChanged) + " are not)");
		return failed ? 1 : 0;
	}

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
