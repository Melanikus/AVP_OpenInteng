// verify_snap: the check of a Snap To Vertex scenario's saved copy (acceptance-object-snap-vertex), read back through the game's own loader. Written 2026-11-17 by Melanikus (Vasquez)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_snap.exe <the level the editor opened> <the copy it saved> <the moved object's name> [place <x> <y> <z> [turned]]
// ("place x y z", for Move relative's read-back: the object's place must be exactly that, and the vertex check is left out; "turned" after it, for Move rel. to poly's: its turn must have changed.)
// CHECKS: both levels are read by the game's loader; the copy has the original's objects in its order; the named object's place (OBJHEAD1, as the game's Object_Chunk holds it) has
// moved and its turn is the original's (a snap without Shift turns nothing); one of its world vertices (its shape's points turned by its turn and moved by its place: file_pick.h's
// FilePick_World, which the editor's own snap does not change) IS a world vertex of another object; every other object's place and turn are the original's. Gives back 0 only if every
// check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "file_pick.h"
#include "shape_model.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// what the game's chunk code calls
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

struct Held
{
	std::string name;
	uint8_t id[8];
	int place[3];
	float turn[4];
};

static void Objects(File_Chunk *level, std::vector<Held> *out)
{
	List<Object_Chunk *> objects;

	level->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Held one;

		one.name.assign(each()->object_data.o_name, strnlen(each()->object_data.o_name, sizeof(each()->object_data.o_name)));
		memcpy(one.id, &each()->object_data.ID, 8);
		one.place[0] = each()->object_data.location.x;
		one.place[1] = each()->object_data.location.y;
		one.place[2] = each()->object_data.location.z;
		one.turn[0] = each()->object_data.orientation.x;
		one.turn[1] = each()->object_data.orientation.y;
		one.turn[2] = each()->object_data.orientation.z;
		one.turn[3] = each()->object_data.orientation.w;
		out->push_back(one);
	}
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("  FAILED  usage: verify_snap <original> <copy> <the moved object's name>\n");
		return 1;
	}

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level(), std::string("the game's loader reads the original: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level(), std::string("the game's loader reads the copy: ") + argv[2]);

	if (!original.Level() || !copy.Level())
	{
		printf("FAILED verify_snap (%d checks failed)\n", checksFailed);
		return 1;
	}

	std::vector<Held> before, after;

	Objects(original.Level(), &before);
	Objects(copy.Level(), &after);

	bool sameObjects = before.size() == after.size();

	for (size_t i = 0; sameObjects && i < before.size(); i++)
	{
		sameObjects = before[i].name == after[i].name;
	}

	Check(sameObjects, "the copy has the original's objects, in its order (" + std::to_string(after.size()) + ")");

	if (!sameObjects)
	{
		printf("FAILED verify_snap (%d checks failed)\n", checksFailed);
		return 1;
	}

	std::string const name = argv[3];
	size_t moved = after.size();

	for (size_t i = 0; i < after.size() && moved == after.size(); i++)
	{
		moved = after[i].name == name ? i : moved;
	}

	Check(moved < after.size(), "the copy holds an object named \"" + name + "\"");

	if (moved == after.size())
	{
		printf("FAILED verify_snap (%d checks failed)\n", checksFailed);
		return 1;
	}

	char words[200];

	snprintf(words, sizeof(words), "\"%s\" moved: %d %d %d -> %d %d %d", name.c_str(), before[moved].place[0], before[moved].place[1], before[moved].place[2], after[moved].place[0],
		after[moved].place[1], after[moved].place[2]);
	Check(memcmp(before[moved].place, after[moved].place, sizeof(before[moved].place)) != 0, words);

	// "place x y z" (Move relative's read-back): its place is exactly that, instead of the vertex check; "place x y z turned" (Move rel. to poly's): its turn has changed too
	bool const placeAsked = argc >= 8 && strcmp(argv[4], "place") == 0;
	bool const turnedAsked = placeAsked && argc >= 9 && strcmp(argv[8], "turned") == 0;

	if (turnedAsked)
	{
		Check(memcmp(before[moved].turn, after[moved].turn, sizeof(before[moved].turn)) != 0, "and its turn has changed");
	}
	else
	{
		Check(memcmp(before[moved].turn, after[moved].turn, sizeof(before[moved].turn)) == 0, "and its turn is the original's");
	}

	if (placeAsked)
	{
		int const wanted[3] = { atoi(argv[5]), atoi(argv[6]), atoi(argv[7]) };

		snprintf(words, sizeof(words), "its place is %d %d %d", wanted[0], wanted[1], wanted[2]);
		Check(memcmp(after[moved].place, wanted, sizeof(wanted)) == 0, words);
	}

	// its world vertices, and every other object's
	model::ShapeModel shape;
	std::vector<int32_t> points, normals;

	Check(host::FilePick_World(&copy, copy.Level(), after[moved].id, &shape, &points, &normals) == host::FilePickResult::Done, "its shape is read in the copy");

	std::set<std::tuple<int32_t, int32_t, int32_t>> others;

	for (size_t i = 0; i < after.size(); i++)
	{
		std::vector<int32_t> theirs, theirNormals;
		model::ShapeModel theirShape;

		if (i == moved || memcmp(after[i].id, after[moved].id, 8) == 0 || host::FilePick_World(&copy, copy.Level(), after[i].id, &theirShape, &theirs, &theirNormals) != host::FilePickResult::Done)
		{
			continue;
		}

		for (size_t p = 0; p + 2 < theirs.size(); p += 3)
		{
			others.insert(std::make_tuple(theirs[p], theirs[p + 1], theirs[p + 2]));
		}
	}

	bool onAVertex = false;

	for (size_t p = 0; p + 2 < points.size() && !onAVertex; p += 3)
	{
		onAVertex = others.count(std::make_tuple(points[p], points[p + 1], points[p + 2])) != 0;
	}

	if (!placeAsked)
	{
		Check(onAVertex, "one of its world vertices is a world vertex of another object");
	}

	size_t differing = 0;

	for (size_t i = 0; i < after.size(); i++)
	{
		if (i != moved && (memcmp(before[i].place, after[i].place, sizeof(before[i].place)) != 0 || memcmp(before[i].turn, after[i].turn, sizeof(before[i].turn)) != 0))
		{
			differing++;
			printf("  (\"%s\" moved too)\n", after[i].name.c_str());
		}
	}

	Check(differing == 0, "every other object's place and turn (" + std::to_string(after.size() - 1) + ") are the original's");

	printf("%s verify_snap (%d checks failed)\n", checksFailed ? "FAILED" : "passed", checksFailed);
	return checksFailed ? 1 : 0;
}
