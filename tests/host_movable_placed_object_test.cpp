// host_movable_placed_object_test.cpp: movable.h's PLACED OBJECT adapter on the stock levels: an object's place and turn read, a new pose written through the game's
// loader (TrackSetObjectPose) and read back, the other objects left where they were, and an ID no object has refused. Written 2026-11-18 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"

#include "level_file.h"
#include "movable.h"

// the game's file and message hooks, as host_track_editor_test has them: a level is only ever read here (the writer works on the bytes in memory)
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

void avp_MessageBox(const char *, int)
{
}

void avp_exit(int code)
{
	exit(code);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

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

static host::MovableRef RefOf(std::vector<uint8_t> const &id)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::PlacedObject;
	memcpy(ref.id, id.data(), 8);
	return ref;
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

int main(int argc, char **argv)
{
	int levels = 0;

	// every write puts the whole level together again: the first three levels that open are enough for the adapter (the writer itself is host_track_editor_test's)
	for (int a = 1; a < argc && levels < 3; a++)
	{
		OpenLevelFile f;

		if (f.Open(argv[a]) != LevelOpenResult::Opened)
		{
			continue;
		}

		levels++;

		std::string const where = argv[a];
		List<Object_Chunk *> objects;
		std::vector<std::vector<uint8_t> > ids;

		f.Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done() && ids.size() < 2; each.next())
		{
			if (each())
			{
				ids.push_back(std::vector<uint8_t>((uint8_t const *)&each()->object_data.ID, (uint8_t const *)&each()->object_data.ID + 8));
			}
		}

		if (ids.size() < 2)
		{
			Check(false, where + ": the level holds two objects to move");
			continue;
		}

		host::MovablePose first, other;

		Check(host::MovablePlacedObject_Get(&f, RefOf(ids[0]), &first) == host::MovableResult::Done, where + ": the first object's pose is read");
		Check(host::MovablePlacedObject_Get(&f, RefOf(ids[1]), &other) == host::MovableResult::Done, where + ": and the second's");

		// moved 250 mm across and 100 up (y is down), and turned a quarter about the up axis
		host::MovablePose moved = first;

		moved.place[0] += 250;
		moved.place[1] -= 100;
		moved.turn[0] = 0.0f;
		moved.turn[1] = 0.70710678f;
		moved.turn[2] = 0.0f;
		moved.turn[3] = 0.70710678f;

		Check(host::MovablePlacedObject_Set(&f, RefOf(ids[0]), moved) == host::MovableResult::Done, where + ": the new pose is written (the game's loader took the level)");

		host::MovablePose back, otherBack;

		Check(host::MovablePlacedObject_Get(&f, RefOf(ids[0]), &back) == host::MovableResult::Done && back.place[0] == moved.place[0] && back.place[1] == moved.place[1] &&
			back.place[2] == moved.place[2] && SameTurn(back.turn, moved.turn), where + ": read back: the new place and the quarter turn (" + std::to_string(back.place[0]) + " " +
			std::to_string(back.place[1]) + " " + std::to_string(back.place[2]) + ")");
		Check(host::MovablePlacedObject_Get(&f, RefOf(ids[1]), &otherBack) == host::MovableResult::Done && memcmp(otherBack.place, other.place, sizeof(other.place)) == 0 &&
			SameTurn(otherBack.turn, other.turn), where + ": the other object is where it was");

		// back where it was: the level as it was read
		Check(host::MovablePlacedObject_Set(&f, RefOf(ids[0]), first) == host::MovableResult::Done && host::MovablePlacedObject_Get(&f, RefOf(ids[0]), &back) ==
			host::MovableResult::Done && memcmp(back.place, first.place, sizeof(first.place)) == 0, where + ": and written back to its first place");

		std::vector<uint8_t> const nobody(8, 0xEE), zero(8, 0);
		host::MovablePose untouched;

		untouched.place[0] = 12345;
		Check(host::MovablePlacedObject_Get(&f, RefOf(nobody), &untouched) == host::MovableResult::NoSuchThing && untouched.place[0] == 12345, where + ": an ID no object has: NoSuchThing");
		Check(host::MovablePlacedObject_Set(&f, RefOf(nobody), moved) == host::MovableResult::NoSuchThing, where + ": and it is not written");
		Check(host::MovablePlacedObject_Get(&f, RefOf(zero), &untouched) == host::MovableResult::NoSuchThing && host::MovablePlacedObject_Set(&f, RefOf(zero), moved) ==
			host::MovableResult::NoSuchThing, where + ": an all-0 ID names nothing");
	}

	Check(levels > 0, "at least one stock level opened (" + std::to_string(levels) + ")");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
