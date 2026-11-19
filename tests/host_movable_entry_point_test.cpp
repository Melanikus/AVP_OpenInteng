// host_movable_entry_point_test.cpp: movable.h's ENTRY POINT adapter (src/host/movable_entry_point.cpp) on a stock level: a marker's place read (the same as EntryMarkers_Of's), a new
// place written through EntryMarkers_Move (the Fly's stop, FUN_0058a6b0) and read back as the level now holds it, the place it already has written as nothing, no turn,
// a saved copy taken by the game's own loader, and a marker or module that is none refused. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_movable_entry_point_test.args builds it; host_movable_entry_point_test <level file> [...]. A non-zero result means a check failed.

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
#include "entry_markers.h"
#include "object_edit.h"

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

static host::MovableRef RefOf(int32_t module, int32_t number)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::EntryPoint;
	ref.module = module;
	ref.number = number;
	return ref;
}

static bool IsIdentity(float const t[4])
{
	return t[0] == 0.0f && t[1] == 0.0f && t[2] == 0.0f && t[3] == 1.0f;
}

static std::string Words(int32_t const p[3])
{
	return std::to_string(p[0]) + " " + std::to_string(p[1]) + " " + std::to_string(p[2]);
}

static void TestFile(std::string const &path, std::string const &folder, int *withMarkers)
{
	printf("%s\n", path.c_str());

	OpenLevelFile open;

	if (open.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	std::vector<host::ModuleSnapshot> modules;

	Check(host::ReadModuleSnapshots(&open, &modules) == host::ObjectEditResult::Done, "the modules are read");

	// a module whose AI module has markers: the first with adjacent modules (as host_entry_markers_test finds one)
	int32_t module = -1;
	std::vector<host::EntryMarker> markers;

	for (host::ModuleSnapshot const &one : modules)
	{
		if (one.module && one.hasAdjacency && one.adjacency.size() >= 2 && host::EntryMarkers_Of(&open, one.index, &markers) && !markers.empty())
		{
			module = one.index;
			break;
		}
	}

	if (module < 0)
	{
		printf("  (no entry point marker here)\n");
		return;
	}

	(*withMarkers)++;

	host::EntryMarker const first = markers[0];
	host::MovablePose got;

	Check(host::MovableEntryPoint_Get(&open, RefOf(module, 0), &got) == host::MovableResult::Done && memcmp(got.place, first.point, sizeof(got.place)) == 0 && IsIdentity(got.turn),
		"module " + std::to_string(module) + " marker 0: the pose is the entry's point (" + Words(first.point) + "), no turn");

	if (markers.size() > 1)
	{
		Check(host::MovableEntryPoint_Get(&open, RefOf(module, (int32_t)markers.size() - 1), &got) == host::MovableResult::Done &&
			memcmp(got.place, markers.back().point, sizeof(got.place)) == 0, "the last marker's pose is its point");
	}

	// its own place again: nothing to write, and nothing changes
	host::MovablePose same;

	memcpy(same.place, first.point, sizeof(same.place));
	Check(host::MovableEntryPoint_Set(&open, RefOf(module, 0), same) == host::MovableResult::Done, "the place it has: Done");

	std::vector<host::EntryMarker> still;

	Check(host::EntryMarkers_Of(&open, module, &still) && still.size() == markers.size() && memcmp(still[0].point, first.point, sizeof(first.point)) == 0 && still[0].flags == first.flags,
		"and the entry is as it was (its point and flags)");

	// far away (as host_entry_markers_test moves one): the writer holds the point inside a box of the other AI module, so the place read back is the one the entries got
	host::MovablePose moved;

	moved.place[0] = first.point[0] + 5000000;
	moved.place[1] = first.point[1] - 3000000;
	moved.place[2] = first.point[2] + 4000000;
	moved.turn[1] = 0.70710678f;
	moved.turn[3] = 0.70710678f;   // a turn given to a kind with none is not used

	Check(host::MovableEntryPoint_Set(&open, RefOf(module, 0), moved) == host::MovableResult::Done, "a far place is written (the game's loader took the level)");

	std::vector<host::EntryMarker> after;
	host::MovablePose back;

	Check(host::EntryMarkers_Of(&open, module, &after) && after.size() == markers.size() && memcmp(after[0].point, first.point, sizeof(first.point)) != 0 &&
		(after[0].flags & 1) != 0, "the entry got a new point and the hand-set bit (" + (after.empty() ? std::string("none") : Words(after[0].point)) + ")");
	Check(host::MovableEntryPoint_Get(&open, RefOf(module, 0), &back) == host::MovableResult::Done && !after.empty() && memcmp(back.place, after[0].point, sizeof(back.place)) == 0 &&
		IsIdentity(back.turn), "read back: the place the entry now holds, no turn");

	std::string const saved = folder + "\\movable_entry_point_copy.rif";

	if (open.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;
		std::vector<host::EntryMarker> reread;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && host::EntryMarkers_Of(&second, module, &reread) && !reread.empty() && !after.empty() &&
			memcmp(reread[0].point, after[0].point, sizeof(after[0].point)) == 0, "a saved copy is taken by the game's own loader with the new point");
		DeleteFileA(saved.c_str());
	}
	else
	{
		Check(false, "a copy is saved");
	}

	// refusals
	host::MovablePose untouched;

	untouched.place[0] = 12345;
	Check(host::MovableEntryPoint_Get(&open, RefOf(module, (int32_t)markers.size()), &untouched) == host::MovableResult::NoSuchThing && untouched.place[0] == 12345 &&
		host::MovableEntryPoint_Set(&open, RefOf(module, (int32_t)markers.size()), moved) == host::MovableResult::NoSuchThing, "a marker number past the list: NoSuchThing, nothing written");
	Check(host::MovableEntryPoint_Get(&open, RefOf(module, -1), &untouched) == host::MovableResult::NoSuchThing, "marker number -1: NoSuchThing");
	Check(host::MovableEntryPoint_Get(&open, RefOf(-1, 0), &untouched) == host::MovableResult::NoSuchThing &&
		host::MovableEntryPoint_Get(&open, RefOf(1 << 30, 0), &untouched) == host::MovableResult::NoSuchThing, "a module that is none: NoSuchThing");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_movable_entry_point_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "movable_entry_point_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	int withMarkers = 0;

	for (int a = 1; a < argc; a++)
	{
		TestFile(argv[a], folder, &withMarkers);
	}

	RemoveDirectoryA(folder.c_str());

	host::MovablePose none;

	Check(host::MovableEntryPoint_Get(nullptr, RefOf(0, 0), &none) == host::MovableResult::NothingOpen, "no level open: NothingOpen");
	Check(withMarkers > 0, "at least one level has entry point markers (" + std::to_string(withMarkers) + ")");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
