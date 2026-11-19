// host_movable_special_test.cpp: movable.h's SPECIAL OBJECTS adapter (src/host/movable_special.cpp) on the stock levels: for each of the five kinds (player start, particle
// generator, sound, AVP generator, bad guy; one is made with SpecialObject_Create when the level has none) the place and turn read, a new pose written through
// SpecialObject_Write and read back, every other field kept, the other objects left where they were, a saved copy taken by the game's own loader with the new pose, and a
// number no object of the kind has refused. The player start's matrix is checked against the game's own QuatToMat (avpx/3dc/maths.cpp:2374) of the turn with x y z negated, as
// the game reads every other kind's turn (object_setup.cpp:899-902, 3992-3995). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_movable_special_test.args builds it; host_movable_special_test <level file> [...]. A non-zero result means a check failed.

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

using namespace host;

struct KindCase
{
	MovableKind movable;
	SpecialKind special;
	char const *name;
	bool turns;
};

static KindCase const kCases[] =
{
	{ MovableKind::PlayerStart, SpecialKind::PlayerStart, "player start", true },
	{ MovableKind::ParticleGenerator, SpecialKind::ParticleGenerator, "particle generator", true },
	{ MovableKind::Sound, SpecialKind::Sound, "sound", false },
	{ MovableKind::AvpGenerator, SpecialKind::Generator, "AVP generator", false },
	{ MovableKind::BadGuy, SpecialKind::BadGuy, "bad guy", false },
};

static bool SameTurn(float const a[4], float const b[4], double tolerance)
{
	// a turn and its negative are the same turn
	double same = 0.0, opposite = 0.0;

	for (int i = 0; i < 4; i++)
	{
		same += fabs((double)a[i] - (double)b[i]);
		opposite += fabs((double)a[i] + (double)b[i]);
	}

	return same < tolerance || opposite < tolerance;
}

static bool IsIdentity(float const t[4])
{
	return t[0] == 0.0f && t[1] == 0.0f && t[2] == 0.0f && t[3] == 1.0f;
}

static MovableRef RefOf(MovableKind kind, int32_t number)
{
	MovableRef ref;

	ref.kind = kind;
	ref.number = number;
	return ref;
}

// Every field but the place, the matrix and the quaternion turn
static bool SameButPose(SpecialObject const &a, SpecialObject const &b)
{
	return a.kind == b.kind && a.number == b.number && a.name == b.name && a.heading == b.heading && a.type == b.type && a.flags == b.flags && a.texture == b.texture &&
		a.subType == b.subType && a.extra1 == b.extra1 && a.extra2 == b.extra2 && memcmp(a.module, b.module, 8) == 0 && a.inner == b.inner && a.outer == b.outer &&
		a.volume == b.volume && a.pitch == b.pitch && a.soundFlags == b.soundFlags && a.probability == b.probability && a.wav == b.wav && a.particleType == b.particleType &&
		a.particleFlags == b.particleFlags && a.time == b.time && a.particleProbability == b.particleProbability && a.speed == b.speed && a.quantity == b.quantity &&
		memcmp(a.id, b.id, 8) == 0 && memcmp(a.parentId, b.parentId, 8) == 0 && a.hasSound == b.hasSound && a.particleSoundWav == b.particleSoundWav &&
		a.hasSettings == b.hasSettings && a.genRate == b.genRate && a.genIncrease == b.genIncrease && a.genLimit == b.genLimit && memcmp(a.weights, b.weights, sizeof(a.weights)) == 0;
}

static bool FindOne(std::vector<SpecialObject> const &all, SpecialKind kind, size_t number, SpecialObject *out)
{
	for (SpecialObject const &each : all)
	{
		if (each.kind == kind && each.number == number)
		{
			*out = each;
			return true;
		}
	}

	return false;
}

static size_t CountOf(std::vector<SpecialObject> const &all, SpecialKind kind)
{
	size_t n = 0;

	for (SpecialObject const &each : all)
	{
		n += each.kind == kind ? 1 : 0;
	}

	return n;
}

static void TestKind(OpenLevelFile *file, KindCase const &kc, std::string const &folder, std::string const &level)
{
	std::string const what = level + " " + kc.name;
	std::vector<SpecialObject> all;

	if (SpecialObjects_Read(file, &all) != SpecialResult::Done)
	{
		Check(false, what + ": the special objects are read");
		return;
	}

	if (CountOf(all, kc.special) == 0)
	{
		SpecialPlace where;
		size_t made = 0;

		where.place[0] = 1000;
		where.place[1] = -2000;
		where.place[2] = 3000;
		Check(SpecialObject_Create(file, kc.special, where, 1.0, kc.special == SpecialKind::PlayerStart ? "" : "movabletest", &made) == SpecialResult::Done && made == 0,
			what + ": none in the level, so one is made");
		all.clear();
		SpecialObjects_Read(file, &all);
	}

	SpecialObject before;

	if (!FindOne(all, kc.special, 0, &before))
	{
		Check(false, what + ": the first one is found");
		return;
	}

	MovablePose got;

	Check(MovableSpecial_Get(file, RefOf(kc.movable, 0), &got) == MovableResult::Done && memcmp(got.place, before.place, sizeof(got.place)) == 0,
		what + ": the pose's place is the chunk's (" + std::to_string(before.place[0]) + " " + std::to_string(before.place[1]) + " " + std::to_string(before.place[2]) + ")");

	if (kc.special == SpecialKind::ParticleGenerator)
	{
		Check(SameTurn(got.turn, before.turn, 1e-6), what + ": the turn is the PARGENDA's quaternion as it is");
	}
	else if (!kc.turns)
	{
		Check(IsIdentity(got.turn), what + ": no turn: the identity");
	}

	MovablePose moved;

	moved.place[0] = before.place[0] + 1000;
	moved.place[1] = before.place[1] - 500;
	moved.place[2] = before.place[2] + 250;

	// a quarter turn about the vertical axis (unit length, as Movable_Set gives it)
	moved.turn[0] = 0.0f;
	moved.turn[1] = 0.70710678f;
	moved.turn[2] = 0.0f;
	moved.turn[3] = 0.70710678f;

	Check(MovableSpecial_Set(file, RefOf(kc.movable, 0), moved) == MovableResult::Done, what + ": the new pose is written (the game's loader took the level)");

	MovablePose back;

	Check(MovableSpecial_Get(file, RefOf(kc.movable, 0), &back) == MovableResult::Done && memcmp(back.place, moved.place, sizeof(back.place)) == 0, what + ": read back: the new place");

	std::vector<SpecialObject> after;
	SpecialObject now;

	Check(SpecialObjects_Read(file, &after) == SpecialResult::Done && after.size() == all.size() && FindOne(after, kc.special, 0, &now) && SameButPose(now, before),
		what + ": every other field is kept");

	if (kc.turns)
	{
		Check(SameTurn(back.turn, moved.turn, 1e-3), what + ": read back: the new turn");
	}
	else
	{
		Check(IsIdentity(back.turn) && memcmp(now.matrix, before.matrix, sizeof(now.matrix)) == 0 && memcmp(now.turn, before.turn, sizeof(now.turn)) == 0,
			what + ": no turn: the identity, and nothing turned in the file");
	}

	if (kc.special == SpecialKind::PlayerStart)
	{
		// the game's QuatToMat of (0, -0.7071, 0, 0.7071): mat11 0, mat13 = 2 w y = -1, mat22 1, mat31 = -2 w y = 1, mat33 0
		int32_t const want[9] = { 0, 0, -65536, 0, 65536, 0, 65536, 0, 0 };
		bool closeEnough = true;

		for (int i = 0; i < 9; i++)
		{
			closeEnough = closeEnough && abs(now.matrix[i] - want[i]) <= 2;
		}

		Check(closeEnough, what + ": the matrix is the game's QuatToMat of the turn with x y z negated, row by row (" + std::to_string(now.matrix[0]) + " " + std::to_string(now.matrix[2]) +
			" " + std::to_string(now.matrix[6]) + ")");
	}

	if (kc.special == SpecialKind::ParticleGenerator)
	{
		Check(SameTurn(now.turn, moved.turn, 1e-6), what + ": the PARGENDA holds the turn as it was given");
	}

	// the others of every kind did not move
	bool othersStill = true;

	for (SpecialObject const &each : all)
	{
		if (each.kind == kc.special && each.number == 0)
		{
			continue;
		}

		SpecialObject later;

		othersStill = othersStill && FindOne(after, each.kind, each.number, &later) && memcmp(later.place, each.place, sizeof(each.place)) == 0;
	}

	Check(othersStill, what + ": every other special object is where it was");

	std::string const saved = folder + "\\movable_special_copy.rif";

	if (file->SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;
		std::vector<SpecialObject> reread;
		SpecialObject there;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && SpecialObjects_Read(&second, &reread) == SpecialResult::Done &&
			FindOne(reread, kc.special, 0, &there) && memcmp(there.place, moved.place, sizeof(moved.place)) == 0, what + ": a saved copy is taken by the game's own loader with the new place");
		DeleteFileA(saved.c_str());
	}
	else
	{
		Check(false, what + ": a copy is saved");
	}

	// refusals
	int32_t const past = (int32_t)CountOf(after, kc.special);
	MovablePose untouched;

	untouched.place[0] = 12345;
	Check(MovableSpecial_Get(file, RefOf(kc.movable, past), &untouched) == MovableResult::NoSuchThing && untouched.place[0] == 12345 &&
		MovableSpecial_Set(file, RefOf(kc.movable, past), moved) == MovableResult::NoSuchThing, what + ": a number past the kind's list: NoSuchThing, nothing written");
	Check(MovableSpecial_Get(file, RefOf(kc.movable, -1), &untouched) == MovableResult::NoSuchThing, what + ": number -1: NoSuchThing");

	// and back where it was (the player start: the turn read from its own matrix gives that matrix again)
	Check(MovableSpecial_Set(file, RefOf(kc.movable, 0), got) == MovableResult::Done, what + ": the old pose is written back");

	std::vector<SpecialObject> last;
	SpecialObject restored;

	if (SpecialObjects_Read(file, &last) == SpecialResult::Done && FindOne(last, kc.special, 0, &restored))
	{
		bool closeEnough = memcmp(restored.place, before.place, sizeof(before.place)) == 0;

		for (int i = 0; i < 9; i++)
		{
			closeEnough = closeEnough && abs(restored.matrix[i] - before.matrix[i]) <= 2;
		}

		Check(closeEnough && SameTurn(restored.turn, before.turn, 1e-6), what + ": it is where it was, turned as it was (the matrix to 2 / 65536)");
	}
	else
	{
		Check(false, what + ": read again");
	}
}

static void TestFile(std::string const &path, std::string const &folder)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	for (KindCase const &kc : kCases)
	{
		TestKind(&file, kc, folder, path);
	}

	// a kind that is not a special one
	MovablePose none;

	Check(MovableSpecial_Get(&file, RefOf(MovableKind::Light, 0), &none) == MovableResult::KindRefused && MovableSpecial_Set(&file, RefOf(MovableKind::Light, 0), none) ==
		MovableResult::KindRefused, path + ": a kind that is not a special object: KindRefused");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_movable_special_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "movable_special_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	for (int a = 1; a < argc; a++)
	{
		TestFile(argv[a], folder);
	}

	RemoveDirectoryA(folder.c_str());

	MovablePose none;

	Check(MovableSpecial_Get(nullptr, RefOf(MovableKind::Sound, 0), &none) == MovableResult::NothingOpen, "no level open: NothingOpen");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
