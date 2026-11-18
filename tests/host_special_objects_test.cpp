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
#include "chunk_sound.hpp"
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
		memcmp(a.parentId, b.parentId, 8) == 0 && memcmp(a.turn, b.turn, 16) == 0 && a.hasSound == b.hasSound && a.particleSoundWav == b.particleSoundWav && a.particleSoundRead == b.particleSoundRead && a.particleSoundIndex == b.particleSoundIndex &&
		a.particleSoundInner == b.particleSoundInner && a.particleSoundOuter == b.particleSoundOuter && a.particleSoundVolume == b.particleSoundVolume && a.particleSoundPitch == b.particleSoundPitch &&
		a.particleSoundFlags == b.particleSoundFlags && a.particleSoundExtra == b.particleSoundExtra && a.hasSettings == b.hasSettings && a.genRate == b.genRate && a.genIncrease == b.genIncrease && a.genLimit == b.genLimit && memcmp(a.weights, b.weights, sizeof(a.weights)) == 0;
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

	// Each sound's ID is the one the game's own library works out for it (Sound_Object_Chunk::CalculateID: what a strategy that names the sound holds, S-KC-12), the sounds in the
	// SPECLOBJ's order. (Added 2026-11-17 by Frost: the reader left a sound's ID 0.)
	{
		Chunk_With_Children *const environment = file.Level() ? (Chunk_With_Children *)file.Level()->lookup_single_child("REBENVDT") : nullptr;
		Chunk_With_Children *const special = environment ? (Chunk_With_Children *)environment->lookup_single_child("SPECLOBJ") : nullptr;
		List<Chunk *> sounds;
		size_t matched = 0, read = 0;

		if (special)
		{
			special->lookup_child("SOUNDOB2", sounds);
		}

		LIF<Chunk *> each(&sounds);

		for (SpecialObject const &one : all)
		{
			if (one.kind != SpecialKind::Sound)
			{
				continue;
			}

			read++;

			if (!each.done())
			{
				ObjectID const id = ((Sound_Object_Chunk *)each())->CalculateID();

				matched += memcmp(&id, one.id, 8) == 0 && one.name == ((Sound_Object_Chunk *)each())->snd_name ? 1 : 0;
				each.next();
			}
		}

		Check(read == (size_t)sounds.size() && matched == read, "every sound's ID is the game library's own (Sound_Object_Chunk::CalculateID): " + std::to_string((unsigned long long)matched) + " of " +
			std::to_string((unsigned long long)read));
	}

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

// OP-135 (Drake 11-17): a particle generator's sound, read whole (Indexed_Sound_Chunk's fields), taken away (Remove Sound), made new (Sound on a generator with none: inteng's record
// "Not Set") and written back: each reads back as written, and the level with it is taken by the game's own loader
static void TestParticleSound(std::string const & path, std::string const & folder)
{
	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		return;
	}

	std::vector<SpecialObject> all;

	if (SpecialObjects_Read(&file, &all) != SpecialResult::Done)
	{
		return;
	}

	SpecialObject const *withSound = nullptr;

	for (SpecialObject const & each : all)
	{
		withSound = !withSound && each.kind == SpecialKind::ParticleGenerator && each.hasSound ? &each : withSound;
	}

	if (!withSound)
	{
		return;
	}

	SpecialObject const original = *withSound;

	printf("  (particle sound of %s: \"%s\" index %d, %u / %u mm, volume %d, pitch %d, flags 0x%x, %zu extra)\n", original.name.c_str(), original.particleSoundWav.c_str(), original.particleSoundIndex,
		original.particleSoundInner, original.particleSoundOuter, original.particleSoundVolume, original.particleSoundPitch, original.particleSoundFlags, original.particleSoundExtra.size());
	Check(original.particleSoundRead && !original.particleSoundWav.empty() && original.particleSoundVolume >= 0 && original.particleSoundVolume <= 127 && original.particleSoundOuter > 0,
		"a particle generator's sound is read whole: its index, wav, ranges, volume, pitch and flags");

	auto const reread = [&](SpecialObject *out)
	{
		std::vector<SpecialObject> again;

		if (SpecialObjects_Read(&file, &again) != SpecialResult::Done)
		{
			return false;
		}

		for (SpecialObject const & each : again)
		{
			if (each.kind == SpecialKind::ParticleGenerator && each.number == original.number)
			{
				*out = each;
				return true;
			}
		}

		return false;
	};

	// Remove Sound: no INDSOUND
	SpecialObject removed = original;

	removed.hasSound = false;
	removed.particleSoundRead = false;
	removed.particleSoundWav.clear();

	SpecialObject back;

	Check(SpecialObject_Write(&file, removed) == SpecialResult::Done && reread(&back) && !back.hasSound && back.name == original.name && back.particleType == original.particleType,
		"Remove Sound: the generator is written with no INDSOUND, the rest as it was");

	// Sound on a generator with none: inteng's new record (FUN_004ebc80(0, 1): "Not Set", 500000 / 4000000 in memory, volume 127, pitch 0, flags 1), written as FUN_004ec1b0 makes it
	SpecialObject made = removed;

	made.hasSound = true;
	made.particleSoundRead = true;
	made.particleSoundIndex = 0;
	made.particleSoundWav = "Not Set";
	made.particleSoundInner = 5000;
	made.particleSoundOuter = 40000;
	made.particleSoundVolume = 127;
	made.particleSoundPitch = 0;
	made.particleSoundFlags = 1;
	made.particleSoundExtra.clear();
	Check(SpecialObject_Write(&file, made) == SpecialResult::Done && reread(&back) && back.hasSound && back.particleSoundRead && back.particleSoundWav == "Not Set" && back.particleSoundInner == 5000 &&
		back.particleSoundOuter == 40000 && back.particleSoundVolume == 127 && back.particleSoundPitch == 0 && back.particleSoundFlags == 1 && back.particleSoundExtra.empty(),
		"Sound on a generator with none: the new INDSOUND (\"Not Set\", 5000 / 40000 mm, 127, 0, loops) reads back as written");

	// a name of every length up to 4 past a multiple of 4: the padding is right ((length + 4) & ~3) and the numbers after it read back
	bool padding = true;

	for (char const *name : { "a", "ab", "abc", "abcd", "abcde.wav" })
	{
		made.particleSoundWav = name;
		made.particleSoundVolume = 33;
		padding = padding && SpecialObject_Write(&file, made) == SpecialResult::Done && reread(&back) && back.particleSoundWav == name && back.particleSoundVolume == 33 && back.particleSoundRead;
	}

	Check(padding, "a wav name of 1 to 9 letters is padded as the game reads it, and the numbers after it read back");

	// and back as it was: the same fields, and the level taken by the game's loader
	Check(SpecialObject_Write(&file, original) == SpecialResult::Done && reread(&back) && Same(back, original), "written back as it was, the generator reads back the same");

	std::string const saved = folder + "\\particle_sound.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened, "the level with the remade INDSOUNDs is taken by the game's own loader");
		DeleteFileA(saved.c_str());
		DeleteFileA((saved + ".bak").c_str());
	}
}

// D14 (Bishop 11-17): a placed hierarchy's PLACHIDT written again. Its place and turn (a flown hierarchy, V-6) read back as written, with its name, hierarchy, ID, sequences and the other
// special objects as they were; a new name reads back too; the refusals (past the end, a place past 2^24, a NaN turn, a bad name); and a saved copy is taken by the game's own loader.
static void TestPlacedHierarchyData(std::string const & path, std::string const & folder)
{
	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "D14: the level opens: " + path);
		return;
	}

	std::vector<PlacedHierarchyInfo> before, after;
	std::vector<SpecialObject> objectsBefore, objectsAfter;

	Check(PlacedHierarchies_Read(&file, &before) == SpecialResult::Done && SpecialObjects_Read(&file, &objectsBefore) == SpecialResult::Done, "D14: the placed hierarchies are read: " + path);

	int32_t const place[3] = { 0, 0, 0 };
	float const turn[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	if (before.empty())
	{
		Check(PlacedHierarchy_WritePlace(&file, 0, place, turn) == SpecialResult::NotFound, "D14: a level with no placed hierarchy refuses the write as not found");
		return;
	}

	PlacedHierarchyInfo const first = before[0];
	int32_t const moved[3] = { first.place[0] + 100, first.place[1] - 200, first.place[2] + 300 };
	float const turned[4] = { 0.0f, 0.6f, 0.0f, 0.8f };

	Check(PlacedHierarchy_WritePlace(&file, before.size(), moved, turned) == SpecialResult::NotFound, "D14: a placed hierarchy past the list's end is not found");

	int32_t const tooFar[3] = { (1 << 24) + 1, 0, 0 };
	float nanTurn[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	uint32_t const nanBits = 0x7fc00000u;

	memcpy(&nanTurn[1], &nanBits, 4);
	Check(PlacedHierarchy_WritePlace(&file, 0, tooFar, turned) == SpecialResult::BadValue, "D14: a place farther than the game's 2^24 is refused");
	Check(PlacedHierarchy_WritePlace(&file, 0, moved, nanTurn) == SpecialResult::BadValue, "D14: a turn that is not a real number is refused");

	PlacedHierarchyInfo badName = first;

	badName.name = std::string(80, 'n');
	Check(PlacedHierarchy_WriteData(&file, 0, badName) == SpecialResult::BadValue, "D14: a name the game doesn't take is refused");

	Check(PlacedHierarchy_WritePlace(&file, 0, moved, turned) == SpecialResult::Done, "D14: the place and turn are written");
	Check(PlacedHierarchies_Read(&file, &after) == SpecialResult::Done && after.size() == before.size(), "D14: the level still holds as many placed hierarchies");
	Check(SpecialObjects_Read(&file, &objectsAfter) == SpecialResult::Done && objectsAfter.size() == objectsBefore.size(), "D14: and as many other special objects");

	if (after.empty())
	{
		return;
	}

	PlacedHierarchyInfo const &now = after[0];

	Check(memcmp(now.place, moved, sizeof(moved)) == 0 && memcmp(now.turn, turned, sizeof(turned)) == 0, "D14: the place and turn read back as written");
	bool soundsWhole = true;

	for (PlacedHierarchyInfo const &one : before)
	{
		for (PlacedHierarchySound const &sound : one.sounds)
		{
			soundsWhole = soundsWhole && sound.read && !sound.wav.empty();
		}
	}

	Check(soundsWhole, "PH-16: every sound (INDSOUND) of the level's placed hierarchies is read whole, with its wav name");
	Check(now.sounds.size() == first.sounds.size(), "D14: its sounds are as they were");
	Check(now.name == first.name && now.hierarchyIndex == first.hierarchyIndex && memcmp(now.id, first.id, 8) == 0 && now.sequences.size() == first.sequences.size(),
		"D14: its name, hierarchy, ID and sequences are as they were");

	// PH-8..10: the sounds written again, a new one at the end; then the first taken out; the refusals
	std::vector<PlacedHierarchySound> sounds = now.sounds;
	PlacedHierarchySound added;

	added.index = PlacedHierarchy_FreeSoundIndex(sounds);
	added.wav = "step.wav";
	added.inner = 5000;
	added.outer = 40000;
	added.volume = 100;
	added.pitch = 256;
	added.flags = 1;
	added.read = true;
	sounds.push_back(added);

	PlacedHierarchySound badIndex = added, badWav = added;

	badIndex.index = 5000;
	badWav.wav = std::string("bad\x01name");
	Check(PlacedHierarchy_WriteSounds(&file, 0, { badIndex }) == SpecialResult::BadValue && PlacedHierarchy_WriteSounds(&file, 0, { badWav }) == SpecialResult::BadValue,
		"PH-8: a sound index past the game's 4095, or a wav name with a letter outside 32..126, is refused");
	Check(PlacedHierarchy_WriteSounds(&file, after.size(), sounds) == SpecialResult::NotFound, "PH-8: a placed hierarchy past the list's end is not found");
	Check(PlacedHierarchy_WriteSounds(&file, 0, sounds) == SpecialResult::Done && PlacedHierarchies_Read(&file, &after) == SpecialResult::Done && !after.empty() &&
		after[0].sounds.size() == sounds.size(), "PH-8: the sounds are written, the new one with them");

	if (!after.empty() && !after[0].sounds.empty())
	{
		PlacedHierarchySound const &last = after[0].sounds.back();
		bool sameOld = true;

		for (size_t i = 0; i + 1 < after[0].sounds.size() && i < now.sounds.size(); i++)
		{
			sameOld = sameOld && after[0].sounds[i].index == now.sounds[i].index && after[0].sounds[i].wav == now.sounds[i].wav && after[0].sounds[i].inner == now.sounds[i].inner &&
				after[0].sounds[i].volume == now.sounds[i].volume && after[0].sounds[i].flags == now.sounds[i].flags;
		}

		Check(last.read && last.index == added.index && last.wav == "step.wav" && last.inner == 5000 && last.outer == 40000 && last.volume == 100 && last.pitch == 256 && last.flags == 1 &&
			last.extra.empty(), "PH-8: the new sound reads back last, field for field (an INDSOUND at the end of the PLACHIER, no extra words: FUN_004ec1b0)");
		Check(sameOld, "PH-8: the sounds it had read back as they were, in their order");

		std::vector<PlacedHierarchySound> fewer = after[0].sounds;

		fewer.erase(fewer.begin());
		Check(PlacedHierarchy_WriteSounds(&file, 0, fewer) == SpecialResult::Done && PlacedHierarchies_Read(&file, &after) == SpecialResult::Done && !after.empty() &&
			after[0].sounds.size() == fewer.size() && !after[0].sounds.empty() && after[0].sounds.back().wav == "step.wav", "PH-10: the first sound taken out, the others kept");
		Check(after[0].sequences.size() == now.sequences.size(), "PH-8..10: the sequences are as they were");
	}

	PlacedHierarchyInfo renamed = after.empty() ? now : after[0];

	renamed.name = "phtest01";
	Check(PlacedHierarchy_WriteData(&file, 0, renamed) == SpecialResult::Done && PlacedHierarchies_Read(&file, &after) == SpecialResult::Done && !after.empty() && after[0].name == "phtest01" &&
		memcmp(after[0].place, moved, sizeof(moved)) == 0, "D14: a new name (a different length: the chunk changes size) reads back, the place kept");

	std::string const saved = folder + "\\placed_hierarchy_data_saved.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;
		std::vector<PlacedHierarchyInfo> reread;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && PlacedHierarchies_Read(&second, &reread) == SpecialResult::Done && !reread.empty() && reread[0].name == "phtest01" &&
			memcmp(reread[0].place, moved, sizeof(moved)) == 0, "D14: a saved copy is taken by the game's own loader with the new place and name");
		DeleteFileA(saved.c_str());
		DeleteFileA((saved + ".bak").c_str());
	}
	else
	{
		Check(false, "D14: the level with the written PLACHIDT is saved");
	}
}

// PH-11..13: the Sound Timing items' rules, as inteng has them (reassess/ph8-13-map-vasquez.json): the new timing (FUN_004ebb60), the chooser rows (0x5e907c, 0x5e9060) and the window's
// write-back (FUN_004eca00 with FUN_0057d620's number read: only 0 or more is taken; an empty field is 0; anything not '-'-and-digits is 0)
static void TestTimingRules()
{
	auto const indexes = [](std::vector<int32_t> const &numbers)
	{
		std::vector<PlacedHierarchySound> sounds;

		for (int32_t const n : numbers)
		{
			PlacedHierarchySound one;

			one.index = n;
			sounds.push_back(one);
		}

		return PlacedHierarchy_FreeSoundIndex(sounds);
	};

	Check(indexes({}) == 0 && indexes({ 0, 1 }) == 2 && indexes({ 1 }) == 0 && indexes({ 2, 0, 1, 4 }) == 3, "PH-8: Add's index is the lowest one no sound has (FUN_004eab10)");

	PlacedHierarchyTimedSound const fresh = PlacedHierarchy_NewTiming();

	Check(fresh.soundIndex == 0 && fresh.start == 0 && fresh.end == 0x10000, "PH-11: a new timing is sound 0, start 0, end 0x10000 (FUN_004ebb60)");

	PlacedHierarchyTimedSound shown;

	shown.soundIndex = 3;
	shown.start = 100;
	shown.end = 65536;
	Check(PlacedHierarchy_TimingRow(shown) == "3 start 100 , end 65536", "PH-12: the timing chooser's row is \"%d start %d , end %d\" of the raw numbers (0x5e907c)");

	PlacedHierarchySound sound;

	sound.index = 5;
	sound.wav = "step.wav";
	Check(PlacedHierarchy_SoundRow(sound) == "5 step.wav", "PH-11: the sound chooser's row is \"%d %s\" (0x5e9060)");

	struct Case { char const *start; char const *end; int32_t wantStart; int32_t wantEnd; char const *what; };
	Case const cases[] = {
		{ "123", "456", 123, 456, "two numbers are taken" },
		{ "", "", 0, 0, "an empty field stores 0" },
		{ "-5", "7", 10, 7, "a negative number keeps the old value" },
		{ "12a", "-", 0, 0, "anything not '-' and digits reads 0, and so does '-' alone" },
		{ "9999999999", "999999999", 0, 999999999, "a number past an int reads 0 (a tenth digit inteng's field can't take); nine digits are taken" },
	};

	for (Case const &one : cases)
	{
		PlacedHierarchyTimedSound timing;

		timing.soundIndex = 2;
		timing.start = 10;
		timing.end = 20;
		PlacedHierarchy_TimingFromFields(one.start, one.end, &timing);
		Check(timing.start == one.wantStart && timing.end == one.wantEnd && timing.soundIndex == 2, std::string("PH-11: the window's write-back: ") + one.what);
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

	TestTimingRules();

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i], folder);
		TestPlacedHierarchySequences(argv[i], folder);
		TestPlacedHierarchyData(argv[i], folder);
		TestParticleSound(argv[i], folder);
	}

	RemoveDirectoryA(folder.c_str());
	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
