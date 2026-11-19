// host_movable_virtual_object_test.cpp: movable.h's VIRTUAL OBJECT adapter (src/host/movable_virtual_object.cpp, with virtual_object.h VirtualObject_SetPlace) on Vaults: a
// virtual object's place read (its VOBJPROP's), a new place written through the game's loader and read back, its name, size, ID and strategy kept, the others not moved, a
// saved copy taken by the loader, no turn, and an unknown ID or a place the loader refuses (past 2^24) changing nothing. Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_movable_virtual_object_test.args builds it; host_movable_virtual_object_test <level file> [...]. A non-zero result means a check failed.

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
#include "virtual_object.h"

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

static host::MovableRef RefOf(uint8_t const *id)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::VirtualObject;
	memcpy(ref.id, id, sizeof(ref.id));
	return ref;
}

static bool IsIdentity(float const t[4])
{
	return t[0] == 0.0f && t[1] == 0.0f && t[2] == 0.0f && t[3] == 1.0f;
}

static bool SameButPlace(host::VirtualObjectInfo const &a, host::VirtualObjectInfo const &b)
{
	return memcmp(a.id, b.id, 8) == 0 && a.name == b.name && a.size == b.size && a.hasStrategy == b.hasStrategy;
}

static std::string Words(int32_t const p[3])
{
	return std::to_string(p[0]) + " " + std::to_string(p[1]) + " " + std::to_string(p[2]);
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

	std::vector<host::VirtualObjectInfo> all;

	Check(host::VirtualObject_List(&file, &all), "the virtual objects are read");

	if (all.empty())
	{
		printf("  (no virtual object here)\n");
		return;
	}

	(*withOne)++;

	host::VirtualObjectInfo const before = all[0];
	host::MovablePose got;

	Check(host::MovableVirtualObject_Get(&file, RefOf(before.id), &got) == host::MovableResult::Done && memcmp(got.place, before.place, sizeof(got.place)) == 0 && IsIdentity(got.turn),
		before.name + ": the pose is its VOBJPROP's place (" + Words(before.place) + "), no turn");

	host::MovablePose moved;

	moved.place[0] = before.place[0] + 1000;
	moved.place[1] = before.place[1] - 500;
	moved.place[2] = before.place[2] + 250;

	Check(host::MovableVirtualObject_Set(&file, RefOf(before.id), moved) == host::MovableResult::Done, "the new place is written (the game's loader took the level)");

	host::MovablePose back;

	Check(host::MovableVirtualObject_Get(&file, RefOf(before.id), &back) == host::MovableResult::Done && memcmp(back.place, moved.place, sizeof(back.place)) == 0 && IsIdentity(back.turn),
		"read back: the new place");

	std::vector<host::VirtualObjectInfo> after;

	Check(host::VirtualObject_List(&file, &after) && after.size() == all.size() && SameButPlace(after[0], before), "its name, size, ID and strategy are kept");

	bool othersStill = after.size() == all.size();

	for (size_t i = 1; i < all.size() && othersStill; i++)
	{
		othersStill = memcmp(after[i].place, all[i].place, sizeof(all[i].place)) == 0 && SameButPlace(after[i], all[i]);
	}

	Check(othersStill, "the other " + std::to_string(all.size() - 1) + " virtual objects did not move");

	std::string const saved = folder + "\\movable_virtual_object_copy.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;
		std::vector<host::VirtualObjectInfo> reread;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && host::VirtualObject_List(&second, &reread) && !reread.empty() &&
			memcmp(reread[0].place, moved.place, sizeof(moved.place)) == 0 && SameButPlace(reread[0], before), "a saved copy is taken by the game's own loader with the new place");
		DeleteFileA(saved.c_str());
	}
	else
	{
		Check(false, "a copy is saved");
	}

	// refusals: the level as it was after each (its generation: it was not made again)
	uint8_t unknown[8];

	memset(unknown, 0xEE, sizeof(unknown));

	uint8_t const zero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned const generation = file.Generation();
	host::MovablePose untouched;

	untouched.place[0] = 12345;
	Check(host::MovableVirtualObject_Get(&file, RefOf(unknown), &untouched) == host::MovableResult::NoSuchThing && untouched.place[0] == 12345 &&
		host::MovableVirtualObject_Set(&file, RefOf(unknown), moved) == host::MovableResult::NoSuchThing, "an ID no virtual object has: NoSuchThing");
	Check(host::MovableVirtualObject_Get(&file, RefOf(zero), &untouched) == host::MovableResult::NoSuchThing, "an all-zero ID: NoSuchThing");

	host::MovablePose tooFar;

	tooFar.place[0] = (1 << 24) + 1;
	tooFar.place[1] = before.place[1];
	tooFar.place[2] = before.place[2];
	Check(host::MovableVirtualObject_Set(&file, RefOf(before.id), tooFar) == host::MovableResult::WriterRefused, "a place past 2^24: the game's loader refuses it, WriterRefused");
	Check(file.Generation() == generation && host::MovableVirtualObject_Get(&file, RefOf(before.id), &back) == host::MovableResult::Done &&
		memcmp(back.place, moved.place, sizeof(back.place)) == 0, "and the refusals changed nothing");
	Check(host::VirtualObject_SetPlace(&file, nullptr, moved.place) == host::VirtualObjectResult::BadCall && host::VirtualObject_SetPlace(nullptr, before.id, moved.place) ==
		host::VirtualObjectResult::BadCall, "VirtualObject_SetPlace with no ID or no level: BadCall");

	// and back where it was
	host::MovablePose original;

	memcpy(original.place, before.place, sizeof(original.place));
	Check(host::MovableVirtualObject_Set(&file, RefOf(before.id), original) == host::MovableResult::Done && host::MovableVirtualObject_Get(&file, RefOf(before.id), &back) ==
		host::MovableResult::Done && memcmp(back.place, before.place, sizeof(back.place)) == 0, "written back: the place it had");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_movable_virtual_object_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "movable_virtual_object_test_" + std::to_string(GetCurrentProcessId());

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

	host::MovablePose none;
	uint8_t const any[8] = { 1, 2, 3, 4, 5, 6, 7, 0 };

	Check(host::MovableVirtualObject_Get(nullptr, RefOf(any), &none) == host::MovableResult::NothingOpen, "no level open: NothingOpen");
	Check(withOne > 0, "at least one level holds a virtual object (" + std::to_string(withOne) + ")");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
