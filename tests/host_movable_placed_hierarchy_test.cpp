// host_movable_placed_hierarchy_test.cpp: movable.h's PLACED HIERARCHY adapter (src/host/movable_placed_hierarchy.cpp) on the stock levels that hold one (Invasion, Furyall,
// odobenus): its place and turn read by ID and by number, a new pose written through PlacedHierarchy_WritePlace and read back, its name, index, sequences and sounds kept,
// a saved copy taken by the game's own loader with the new pose, and an ID or number no placed hierarchy has refused. Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_movable_placed_hierarchy_test.args builds it; host_movable_placed_hierarchy_test <level file> [...]. A non-zero result means a check failed.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"

#include "level_file.h"
#include "movable.h"
#include "special_objects_edit.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

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

static bool SameTurn(float const a[4], float const b[4])
{
	// a turn and its negative are the same turn
	double same = 0.0, opposite = 0.0;

	for (int i = 0; i < 4; i++)
	{
		same += fabs((double)a[i] - (double)b[i]);
		opposite += fabs((double)a[i] + (double)b[i]);
	}

	return same < 1e-4 || opposite < 1e-4;
}

static host::MovableRef ById(host::PlacedHierarchyInfo const &info)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::PlacedHierarchy;
	memcpy(ref.id, info.id, sizeof(ref.id));
	return ref;
}

static host::MovableRef ByNumber(int32_t number)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::PlacedHierarchy;
	ref.number = number;
	return ref;
}

static bool SamePose(host::MovablePose const &pose, host::PlacedHierarchyInfo const &info)
{
	return memcmp(pose.place, info.place, sizeof(pose.place)) == 0 && SameTurn(pose.turn, info.turn);
}

static void TestFile(std::string const &path, std::string const &folder, int *withOne)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	std::vector<host::PlacedHierarchyInfo> all;

	Check(host::PlacedHierarchies_Read(&file, &all) == host::SpecialResult::Done, "the placed hierarchies are read");

	if (all.empty())
	{
		printf("  (no placed hierarchy here)\n");
		return;
	}

	(*withOne)++;

	host::PlacedHierarchyInfo const before = all[0];
	host::MovablePose got;

	Check(host::MovablePlacedHierarchy_Get(&file, ById(before), &got) == host::MovableResult::Done && SamePose(got, before),
		"by its ID: the pose is the PLACHIDT's place and turn (" + before.name + ")");
	Check(host::MovablePlacedHierarchy_Get(&file, ByNumber(0), &got) == host::MovableResult::Done && SamePose(got, before), "by its number (an all-zero ID): the same pose");

	// a new place and a quarter turn about the vertical axis
	host::MovablePose moved;

	moved.place[0] = before.place[0] + 1000;
	moved.place[1] = before.place[1] - 500;
	moved.place[2] = before.place[2] + 250;
	moved.turn[0] = 0.0f;
	moved.turn[1] = 0.70710678f;
	moved.turn[2] = 0.0f;
	moved.turn[3] = 0.70710678f;

	Check(host::MovablePlacedHierarchy_Set(&file, ById(before), moved) == host::MovableResult::Done, "the new pose is written (the game's loader took the level)");

	host::MovablePose back;

	Check(host::MovablePlacedHierarchy_Get(&file, ById(before), &back) == host::MovableResult::Done && memcmp(back.place, moved.place, sizeof(back.place)) == 0 &&
		SameTurn(back.turn, moved.turn), "read back: the new place and turn");

	std::vector<host::PlacedHierarchyInfo> after;

	Check(host::PlacedHierarchies_Read(&file, &after) == host::SpecialResult::Done && after.size() == all.size() && after[0].name == before.name &&
		after[0].hierarchyIndex == before.hierarchyIndex && memcmp(after[0].id, before.id, sizeof(before.id)) == 0 && after[0].sequences.size() == before.sequences.size() &&
		after[0].sounds.size() == before.sounds.size(), "its name, hierarchy index, ID, sequences and sounds are kept");

	for (size_t i = 1; i < all.size() && i < after.size(); i++)
	{
		Check(memcmp(after[i].place, all[i].place, sizeof(all[i].place)) == 0 && SameTurn(after[i].turn, all[i].turn), "placed hierarchy " + std::to_string(i) + " did not move");
	}

	std::string const saved = folder + "\\movable_placed_hierarchy_copy.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;
		std::vector<host::PlacedHierarchyInfo> reread;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && host::PlacedHierarchies_Read(&second, &reread) == host::SpecialResult::Done && !reread.empty() &&
			memcmp(reread[0].place, moved.place, sizeof(moved.place)) == 0 && SameTurn(reread[0].turn, moved.turn), "a saved copy is taken by the game's own loader with the new pose");
		DeleteFileA(saved.c_str());
	}
	else
	{
		Check(false, "a copy is saved");
	}

	// refusals
	host::MovableRef nobody = ById(before);

	nobody.id[0] ^= 0x5a;
	nobody.id[6] ^= 0xa5;

	host::MovablePose untouched;

	untouched.place[0] = 12345;
	Check(host::MovablePlacedHierarchy_Get(&file, nobody, &untouched) == host::MovableResult::NoSuchThing && untouched.place[0] == 12345, "an ID no placed hierarchy has: NoSuchThing");
	Check(host::MovablePlacedHierarchy_Set(&file, nobody, moved) == host::MovableResult::NoSuchThing, "and it is not written");
	Check(host::MovablePlacedHierarchy_Get(&file, ByNumber((int32_t)all.size()), &untouched) == host::MovableResult::NoSuchThing &&
		host::MovablePlacedHierarchy_Set(&file, ByNumber((int32_t)all.size()), moved) == host::MovableResult::NoSuchThing, "a number past the list: NoSuchThing");
	Check(host::MovablePlacedHierarchy_Get(&file, ByNumber(-1), &untouched) == host::MovableResult::NoSuchThing, "no ID and no number: NoSuchThing");

	// and back where it was
	host::MovablePose original;

	memcpy(original.place, before.place, sizeof(original.place));
	memcpy(original.turn, before.turn, sizeof(original.turn));
	Check(host::MovablePlacedHierarchy_Set(&file, ByNumber(0), original) == host::MovableResult::Done && host::MovablePlacedHierarchy_Get(&file, ById(before), &back) ==
		host::MovableResult::Done && SamePose(back, before), "written back by number: the pose it had");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_movable_placed_hierarchy_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "movable_placed_hierarchy_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	int withOne = 0;

	for (int a = 1; a < argc; a++)
	{
		TestFile(argv[a], folder, &withOne);
	}

	RemoveDirectoryA(folder.c_str());

	// null level: nothing open
	host::MovablePose none;

	Check(host::MovablePlacedHierarchy_Get(nullptr, ByNumber(0), &none) == host::MovableResult::NothingOpen, "no level open: NothingOpen");
	Check(withOne > 0, "at least one level holds a placed hierarchy (" + std::to_string(withOne) + ")");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
