// The test of Object Placement's special objects (src/host/special_objects_edit.cpp) on the game's own levels given on the command line: every AVPGENER, AVPSTART, SOUNDOB2 and PARGENER is read
// and written over itself (the fields must read back the same), then one object of each kind is made, written, read back and deleted, the level is saved and taken by the game's own loader.
// Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_special_objects_test.args builds it; host_special_objects_test <level file> [...]. A non-zero result means a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "special_objects_edit.h"
#include "crash_guard.h"

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const & what)
{
	checksRun++;

	if (!good)
	{
		printf("  FAILED  %s\n", what.c_str());
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char * name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char * message, int)
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

static size_t Count(std::vector<SpecialObject> const & all, SpecialKind kind)
{
	size_t n = 0;

	for (SpecialObject const & each : all)
	{
		n += each.kind == kind ? 1 : 0;
	}

	return n;
}

static bool Same(SpecialObject const & a, SpecialObject const & b)
{
	return a.kind == b.kind && a.name == b.name && memcmp(a.place, b.place, 12) == 0 && a.heading == b.heading && a.type == b.type && a.flags == b.flags && a.texture == b.texture &&
		a.subType == b.subType && a.extra1 == b.extra1 && a.extra2 == b.extra2 && memcmp(a.matrix, b.matrix, 36) == 0 && memcmp(a.module, b.module, 8) == 0 && a.inner == b.inner &&
		a.outer == b.outer && a.volume == b.volume && a.pitch == b.pitch && a.soundFlags == b.soundFlags && a.probability == b.probability && a.wav == b.wav && a.particleType == b.particleType &&
		a.particleFlags == b.particleFlags && a.time == b.time && a.particleProbability == b.particleProbability && a.speed == b.speed && a.quantity == b.quantity &&
		memcmp(a.parentId, b.parentId, 8) == 0 && memcmp(a.turn, b.turn, 16) == 0 && a.hasSound == b.hasSound && a.hasSettings == b.hasSettings && a.genRate == b.genRate && a.genIncrease == b.genIncrease && a.genLimit == b.genLimit && memcmp(a.weights, b.weights, sizeof(a.weights)) == 0;
}

static void TestFile(std::string const & path, std::string const & folder)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	std::vector<SpecialObject> all;

	Check(SpecialObjects_Read(&file, &all) == SpecialResult::Done, "the special objects are read");

	std::vector<char> original;

	file.BuildLevel(original);

	// every object written over itself reads back the same
	for (SpecialObject const & each : all)
	{
		if (SpecialObject_Write(&file, each) != SpecialResult::Done)
		{
			Check(false, "an object of the level is written over itself: " + each.name);
			return;
		}
	}

	std::vector<SpecialObject> again;

	Check(SpecialObjects_Read(&file, &again) == SpecialResult::Done && again.size() == all.size(), "the same number of objects after writing each over itself");

	for (size_t i = 0; i < all.size() && i < again.size(); i++)
	{
		if (!Same(all[i], again[i]))
		{
			Check(false, "object " + std::to_string((unsigned long long)i) + " (" + all[i].name + ") reads back the same");
			return;
		}

		if (all[i].kind == SpecialKind::ParticleGenerator)
		{
			uint8_t id[8];

			ParticleGeneratorId(all[i].name, id);
			Check(memcmp(id, all[i].id, 8) == 0, "particle generator " + all[i].name + " holds md5(PARTICLE GEN + name)");
		}
	}

	std::vector<char> after;

	file.BuildLevel(after);

	if (!all.empty())
	{
		printf("  (%zu objects: %zu generators, %zu bad guys, %zu starts, %zu sounds, %zu particle generators; written over themselves the level is %s)\n", all.size(), Count(all, SpecialKind::Generator),
			Count(all, SpecialKind::BadGuy), Count(all, SpecialKind::PlayerStart), Count(all, SpecialKind::Sound), Count(all, SpecialKind::ParticleGenerator),
			after == original ? "byte for byte the same" : "changed only in padding bytes or sizes");
	}

	// one of each kind: made, found, written, read back, deleted
	SpecialKind const kinds[5] = { SpecialKind::Generator, SpecialKind::BadGuy, SpecialKind::PlayerStart, SpecialKind::Sound, SpecialKind::ParticleGenerator };
	SpecialPlace where;

	where.place[0] = 1234;
	where.place[1] = -567;
	where.place[2] = 8910;
	where.heading = 321;

	for (SpecialKind const kind : kinds)
	{
		std::vector<SpecialObject> before;
		size_t number = 99999;

		SpecialObjects_Read(&file, &before);

		std::string const name = kind == SpecialKind::PlayerStart ? std::string("Default name") : "Zz test " + std::to_string((int)kind);
		SpecialResult const made = SpecialObject_Create(&file, kind, where, 1.0, name, &number);

		Check(made == SpecialResult::Done, std::string("Create: ") + SpecialWords(made));

		std::vector<SpecialObject> now;

		SpecialObjects_Read(&file, &now);
		Check(now.size() == before.size() + 1 && Count(now, kind) == Count(before, kind) + 1 && number == Count(before, kind), "one object more of that kind, numbered after the others");

		SpecialObject mine;
		bool found = false;

		for (SpecialObject const & each : now)
		{
			if (each.kind == kind && each.number == number)
			{
				mine = each;
				found = true;
			}
		}

		Check(found && (kind == SpecialKind::PlayerStart || mine.name == name) && memcmp(mine.place, where.place, 12) == 0, "it reads back with its name and place (kind " + std::to_string((int)kind) + ", found " + std::to_string((int)found) + ", name \"" + mine.name + "\")");

		if (kind == SpecialKind::Generator || kind == SpecialKind::BadGuy)
		{
			Check(mine.flags == 0x3801 && mine.heading == 321 && mine.type == (kind == SpecialKind::BadGuy ? 1 : 0), "with inteng's start values (flags 0x3801, type 0 or 1)");
		}

		if (kind == SpecialKind::Sound)
		{
			Check(mine.inner == 5000 && mine.outer == 10000 && mine.volume == 127 && mine.pitch == 0 && mine.wav.empty(), "a sound starts with 5000 / 10000 mm, volume 127 (scale 1)");
			mine.wav = "step.wav";
			mine.volume = 100;
		}

		if (kind == SpecialKind::ParticleGenerator)
		{
			Check(mine.time == 50 && mine.particleProbability == 50, "a particle generator starts at time 50, probability 50");
			mine.speed = 7;
		}

		if (kind == SpecialKind::Generator)
		{
			mine.flags |= 0x8000000u;
			mine.hasSettings = true;
			mine.genRate = 300;
			mine.genIncrease = 25;
			mine.genLimit = 9;
			mine.weights[0] = 5;
			mine.weights[14] = 7;
		}

		mine.place[0] = 4321;
		mine.name = name + "b";

		Check(SpecialObject_Write(&file, mine) == SpecialResult::Done, "Edit: the object is written");

		SpecialObjects_Read(&file, &now);

		bool edited = false;

		for (SpecialObject const & each : now)
		{
			if (each.kind == kind && each.number == number)
			{
				edited = (kind == SpecialKind::PlayerStart || each.name == mine.name) && each.place[0] == 4321 && (kind != SpecialKind::Sound || (each.wav == "step.wav" && each.volume == 100)) && (kind != SpecialKind::ParticleGenerator || each.speed == 7) && (kind != SpecialKind::Generator || (each.hasSettings && each.genRate == 300 && each.genIncrease == 25 && each.genLimit == 9 && each.weights[0] == 5 && each.weights[14] == 7));
			}
		}

		Check(edited, "and reads back edited");

		std::string const saved = folder + "\\special_saved.rif";

		if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
		{
			OpenLevelFile second;
			std::vector<SpecialObject> reread;

			Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && SpecialObjects_Read(&second, &reread) == SpecialResult::Done && reread.size() == now.size(), "a saved copy is taken by the game's own loader with all its objects");
			DeleteFileA(saved.c_str());
			DeleteFileA((saved + ".bak").c_str());
		}

		Check(SpecialObject_Delete(&file, kind, number) == SpecialResult::Done, "Delete takes it away");
		SpecialObjects_Read(&file, &now);
		Check(now.size() == before.size() && Count(now, kind) == Count(before, kind), "and the level holds the objects it held before");
		Check(SpecialObject_Delete(&file, kind, 99999) == SpecialResult::NotFound, "an object that is not there is refused");
	}

	Check(SpecialObject_Create(&file, SpecialKind::Generator, where, 1.0, std::string(70, 'x'), nullptr) == SpecialResult::BadName, "a name longer than 63 is refused");

	// EMPTY NAMES (inteng gives a new object none): each kind made with an empty name must read back, write, save and load through the game, and be deleted, without a crash
	for (SpecialKind const kind : kinds)
	{
		std::vector<SpecialObject> before;
		size_t number = 99999;

		SpecialObjects_Read(&file, &before);

		SpecialResult made = SpecialResult::BadValue;

		// HOSTILE INPUT, with its reason: the game's loader reads a chunk's EMPTY name as no name at all (a null pointer, CopyOptionalNameFromChunk), and a particle generator's data
		// chunk (and a dummy object's, and a power cable's) worked out its size with strlen of that null pointer: the program stopped. Their size is now worked out as 0 name bytes.
		bool const survived = Survives([&] { made = SpecialObject_Create(&file, kind, where, 1.0, std::string(), &number); });

		Check(survived, std::string("a level with an object of kind ") + std::to_string((int)kind) + " that has an EMPTY name is made and loaded without stopping the program (a null name is sized as 0 bytes)");

		if (!survived)
		{
			return;
		}

		Check(made == SpecialResult::Done, std::string("an empty name is taken (kind ") + std::to_string((int)kind) + "): " + SpecialWords(made));

		std::vector<SpecialObject> now;

		SpecialObjects_Read(&file, &now);
		Check(now.size() == before.size() + 1 && Count(now, kind) == Count(before, kind) + 1, "it reads back as one more of that kind");

		for (SpecialObject const & each : now)
		{
			if (each.kind == kind && each.number == number)
			{
				SpecialObject mine = each;

				Check(SpecialObject_Write(&file, mine) == SpecialResult::Done, "and is written back");
			}
		}

		std::string const saved = folder + "\\special_saved_empty.rif";


		if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
		{
			OpenLevelFile second;
			std::vector<SpecialObject> reread;

			Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && SpecialObjects_Read(&second, &reread) == SpecialResult::Done && reread.size() == now.size(), "a saved copy with it is taken by the game's own loader");
			DeleteFileA(saved.c_str());
			DeleteFileA((saved + ".bak").c_str());
		}

		Check(SpecialObject_Delete(&file, kind, number) == SpecialResult::Done, "and is deleted");
	}
}

// A placed hierarchy's sequences (PLHISEQU) are written as inteng's save writes them (reassess/object-placement.md PH-4..PH-7, PH-16; FUN_004e96d0 with FUN_004eb9b0): every one replaced, in the
// order given, the PLACHIER's other children kept, and refused outside what the game's reader takes (index 0..4095). Added 2026-11-16 by Melanikus (Vasquez).
static void TestPlacedHierarchySequences(std::string const & path, std::string const & folder)
{
	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		return;
	}

	std::vector<PlacedHierarchyInfo> before;
	std::vector<SpecialObject> objectsBefore;

	Check(PlacedHierarchies_Read(&file, &before) == SpecialResult::Done && SpecialObjects_Read(&file, &objectsBefore) == SpecialResult::Done, "the placed hierarchies are read: " + path);

	std::vector<PlacedHierarchySequence> mine(2);

	mine[0].index = 3;
	mine[0].sequence = 22;
	mine[0].sub = 0;
	mine[0].milliseconds = 2500;
	mine[0].flags = 2 | 4;
	mine[0].sounds.resize(2);
	mine[0].sounds[0].soundIndex = 1;
	mine[0].sounds[0].start = 100;
	mine[0].sounds[0].end = 65536;
	mine[0].sounds[1].soundIndex = 0;
	mine[0].sounds[1].start = 0;
	mine[0].sounds[1].end = 38000;
	mine[1].index = 0;
	mine[1].sequence = 7;
	mine[1].sub = 2;
	mine[1].milliseconds = 1000;
	mine[1].flags = 1;

	if (before.empty())
	{
		Check(PlacedHierarchy_WriteSequences(&file, 0, mine) == SpecialResult::NotFound, "a level with no placed hierarchy refuses the write as not found");
		return;
	}

	Check(PlacedHierarchy_WriteSequences(&file, before.size(), mine) == SpecialResult::NotFound, "a placed hierarchy past the list's end is not found");

	std::vector<PlacedHierarchySequence> bad = mine;

	bad[1].index = 4096;
	Check(PlacedHierarchy_WriteSequences(&file, 0, bad) == SpecialResult::BadValue, "an index above 4095 (the game's reader's limit) is refused");
	bad[1].index = -1;
	Check(PlacedHierarchy_WriteSequences(&file, 0, bad) == SpecialResult::BadValue, "a negative index is refused");
	bad = mine;
	bad[0].milliseconds = 0;
	Check(PlacedHierarchy_WriteSequences(&file, 0, bad) == SpecialResult::BadValue, "a time of 0 is refused");

	Check(PlacedHierarchy_WriteSequences(&file, 0, mine) == SpecialResult::Done, "the sequences are written");

	std::vector<PlacedHierarchyInfo> after;
	std::vector<SpecialObject> objectsAfter;

	Check(PlacedHierarchies_Read(&file, &after) == SpecialResult::Done && after.size() == before.size(), "the level still holds as many placed hierarchies");
	Check(SpecialObjects_Read(&file, &objectsAfter) == SpecialResult::Done && objectsAfter.size() == objectsBefore.size(), "and as many other special objects");

	bool same = !after.empty() && after[0].sequences.size() == mine.size() && after[0].name == before[0].name && memcmp(after[0].place, before[0].place, 12) == 0;

	for (size_t i = 0; same && i < mine.size(); i++)
	{
		PlacedHierarchySequence const &a = after[0].sequences[i];
		PlacedHierarchySequence const &b = mine[i];

		same = a.index == b.index && a.sequence == b.sequence && a.sub == b.sub && a.milliseconds == b.milliseconds && a.flags == b.flags && a.sounds.size() == b.sounds.size();

		for (size_t k = 0; same && k < a.sounds.size(); k++)
		{
			same = a.sounds[k].soundIndex == b.sounds[k].soundIndex && a.sounds[k].start == b.sounds[k].start && a.sounds[k].end == b.sounds[k].end;
		}
	}

	Check(same, "they read back in the order given, with their sounds, and the placed hierarchy keeps its name and place");

	std::string const saved = folder + "\\placed_hierarchy_saved.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;
		std::vector<PlacedHierarchyInfo> reread;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && PlacedHierarchies_Read(&second, &reread) == SpecialResult::Done && !reread.empty() && reread[0].sequences.size() == mine.size(),
			"a saved copy is taken by the game's own loader with the sequences");
		DeleteFileA(saved.c_str());
		DeleteFileA((saved + ".bak").c_str());
	}
}

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_special_objects_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "special_objects_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i], folder);
		TestPlacedHierarchySequences(argv[i], folder);
	}

	RemoveDirectoryA(folder.c_str());
	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
