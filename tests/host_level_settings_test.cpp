// The test of the level's own settings, editable (src/host/level_settings.cpp; design-vasquez-oi-envgen.md). Written 2026-11-27 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_level_settings_test.exe <Temple .rif> <Base .rif> <Area52 .rif> <a folder for its copies>. The levels are only read; the copies are
// written into <folder>\level_settings_test_<process id> (it stops if that is there already) and deleted at the end.
// THE CASES:
//  1. the game's defaults for a level with neither chunk;
//  2. AVPENVIR as the game reads it: under 24 bytes it is refused, by name (the game's minimum, Drake's PR #313); a size the model refuses
//     is refused, by name; a whole block is read as it is;
//  3. every range, refused with its own field's reason, nothing changed by a refusal; the edges allowed; the kit's spare bits kept;
//  4. Temple read as the game reads it (Dietrich's survey, director/ccmain-reads-dietrich.md: sky 50/50/60, kit 0x57, spears 30; marines,
//     3, 4, 0, 60);
//  5. Temple edited and written through the game's loader, saved, and read again: every change there, nothing else of the two chunks changed,
//     the same objects;
//  6. Base (neither chunk): the defaults; both chunks made (added last in REBENVDT) with one change each; read again after a save;
//  7. Area52 (hive 32,000 s, more than the form takes): kept as the map holds it when only Max NPCs is changed;
//  8. the writer refuses a payload of the wrong size, and nothing is written.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_settings.h"
#include "field_model.h"

static int checksFailed = 0;
static int checksDone = 0;

static void Check(bool good, std::string const &what)
{
	checksDone++;

	if (!good)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own (as host_acoustic_mode_test's)

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

typedef host::LevelSettingsKind Kind;
Kind const Env = Kind::Environment;
Kind const Gen = Kind::Generators;

static int32_t Get(Kind kind, std::vector<uint8_t> const &settings, char const *field)
{
	int32_t value = -123456;

	if (!host::LevelSettings_Get(kind, settings, field, &value))
	{
		return -123456;
	}

	return value;
}

static int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
{
	int32_t value = 0;

	if (at + 4 <= bytes.size())
	{
		memcpy(&value, bytes.data() + at, 4);
	}

	return value;
}

static void PutInt(std::vector<uint8_t> *bytes, size_t at, int32_t value)
{
	memcpy(bytes->data() + at, &value, 4);
}

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	FILE *f = fopen(path.c_str(), "rb");

	if (!f)
	{
		return false;
	}

	out->clear();
	char block[65536];
	size_t got;

	while ((got = fread(block, 1, sizeof(block), f)) > 0)
	{
		out->insert(out->end(), block, block + got);
	}

	fclose(f);
	return true;
}

static size_t ObjectCount(OpenLevelFile *file)
{
	List<Object_Chunk *> objects;

	file->Level()->list_objects(&objects);
	return (size_t)objects.size();
}

// The field's own reason, from the model's table: what a refusal has to give
static char const *ReasonOf(char const *chunk, char const *field)
{
	model::ChunkKind const *const kind = model::FindChunkKind(chunk);
	model::Field const *const found = kind ? model::FindChunkField(*kind, field) : 0;

	return found ? found->why : "(no such field)";
}

static void RefusedWithItsReason(Kind kind, char const *chunk, char const *field, int32_t value)
{
	std::vector<uint8_t> settings = host::LevelSettings_Defaults(kind);
	std::vector<uint8_t> const before = settings;
	char const *why = 0;
	bool const set = host::LevelSettings_Set(kind, &settings, field, value, &why);

	Check(!set && why == ReasonOf(chunk, field) && settings == before,
	      std::string(field) + " " + std::to_string(value) + ": refused with its own reason (" + (why ? why : "none") + "), nothing changed");
}

static void AllowedAndWritten(Kind kind, char const *field, int32_t value)
{
	std::vector<uint8_t> settings = host::LevelSettings_Defaults(kind);
	char const *why = 0;
	bool const set = host::LevelSettings_Set(kind, &settings, field, value, &why);

	Check(set && Get(kind, settings, field) == value, std::string(field) + " " + std::to_string(value) + ": allowed, and written");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// ---- 1. the game's defaults (avp_chunk.cpp:608-627 and the generator chunk's own constructor; npcsetup.cpp: aliens)
	std::vector<uint8_t> const envDefaults = host::LevelSettings_Defaults(Env);
	std::vector<uint8_t> const genDefaults = host::LevelSettings_Defaults(Gen);

	Check(envDefaults.size() == 24 && IntAt(envDefaults, 0) == 24 && IntAt(envDefaults, 4) == 200 && IntAt(envDefaults, 8) == 200 &&
	      IntAt(envDefaults, 12) == 200 && IntAt(envDefaults, 16) == 0x17 && IntAt(envDefaults, 20) == 30,
	      "a level with no AVPENVIR: 24 bytes that say 24; sky 200, 200, 200; pistol, caster, disc and medicomp (0x17); 30 spears");
	Check(genDefaults.size() == 28 && IntAt(genDefaults, 0) == 0 && IntAt(genDefaults, 4) == 25 && IntAt(genDefaults, 8) == 4 &&
	      IntAt(genDefaults, 12) == 2 && IntAt(genDefaults, 16) == 60 && IntAt(genDefaults, 20) == 0 && IntAt(genDefaults, 24) == 0,
	      "a level with no GLOGENDC: aliens, 25, 4, 2, 60 s, and the spares 0");
	Check(strcmp(host::LevelSettings_ChunkName(Env), "AVPENVIR") == 0 && strcmp(host::LevelSettings_ChunkName(Gen), "GLOGENDC") == 0,
	      "the chunks' names");

	// ---- 2. AVPENVIR as the game reads it. Under 24 bytes it is REFUSED (Bishop 11-27: the game's minimum is 24 from Drake's PR #313, and
	// all 1,133 AVPENVIRs in the Steam, Fox-install, community and AvP-maps RIFs are 24 bytes that say 24), by name, for each short size
	{
		char const *why = 0;
		std::vector<uint8_t> read;

		for (size_t size : { (size_t)4, (size_t)16, (size_t)20, (size_t)23 })
		{
			std::vector<uint8_t> shorter(size, 0);

			PutInt(&shorter, 0, (int32_t)size);
			why = 0;
			Check(!host::LevelSettings_AsTheGameReads(Env, shorter, &read, &why) && why && strstr(why, "24") != 0,
			      "a " + std::to_string(size) + "-byte AVPENVIR that says " + std::to_string(size) + " is refused, saying why: " + (why ? why : "no reason"));
		}

		std::vector<uint8_t> whole = envDefaults;

		PutInt(&whole, 16, (int32_t)0xffffff80u | 0x41);
		PutInt(&whole, 20, 50);
		Check(host::LevelSettings_AsTheGameReads(Env, whole, &read, &why) && read == whole, "a whole 24-byte block is read as it is, its spare bits too");

		std::vector<uint8_t> lies = envDefaults;

		PutInt(&lies, 0, 28);
		why = 0;
		Check(!host::LevelSettings_AsTheGameReads(Env, lies, &read, &why) && why && why[0] != 0,
		      std::string("24 bytes that say 28 are refused, by name: ") + (why ? why : "no reason"));

		std::vector<uint8_t> three(3, 0);

		why = 0;
		Check(!host::LevelSettings_AsTheGameReads(Env, three, &read, &why) && why && why[0] != 0, "a block too short to hold its own size is refused, by name");

		std::vector<uint8_t> shortGen(27, 0);

		why = 0;
		Check(!host::LevelSettings_AsTheGameReads(Gen, shortGen, &read, &why) && why && why[0] != 0, "a 27-byte GLOGENDC is refused, by name");

		std::vector<uint8_t> badEnemy = genDefaults;

		PutInt(&badEnemy, 0, 2);
		why = 0;
		Check(!host::LevelSettings_AsTheGameReads(Gen, badEnemy, &read, &why) && why == ReasonOf("GLOGENDC", "EnemyGenerated"),
		      "an enemy that is neither aliens nor marines is refused with the loader's reason");
	}

	// ---- 3. every range, with its own reason
	RefusedWithItsReason(Env, "AVPENVIR", "sky_colour_red", 256);
	RefusedWithItsReason(Env, "AVPENVIR", "sky_colour_green", -1);
	RefusedWithItsReason(Env, "AVPENVIR", "sky_colour_blue", 1000);
	RefusedWithItsReason(Env, "AVPENVIR", "predator_num_spears", 32768);
	RefusedWithItsReason(Env, "AVPENVIR", "predator_num_spears", -1);
	RefusedWithItsReason(Gen, "GLOGENDC", "MaxNPCSOnLevel", 256);
	RefusedWithItsReason(Gen, "GLOGENDC", "MaxNPCSOnLevel", -1);
	RefusedWithItsReason(Gen, "GLOGENDC", "NPCSPerMinute", 256);
	RefusedWithItsReason(Gen, "GLOGENDC", "NPCAcceleration", 256);
	RefusedWithItsReason(Gen, "GLOGENDC", "HiveStateChangeTime", 4682);
	RefusedWithItsReason(Gen, "GLOGENDC", "HiveStateChangeTime", 32000);
	RefusedWithItsReason(Gen, "GLOGENDC", "HiveStateChangeTime", 0);
	RefusedWithItsReason(Gen, "GLOGENDC", "EnemyGenerated", 2);
	AllowedAndWritten(Env, "sky_colour_red", 0);
	AllowedAndWritten(Env, "sky_colour_red", 255);
	AllowedAndWritten(Env, "predator_num_spears", 0);
	AllowedAndWritten(Env, "predator_num_spears", 32767);
	AllowedAndWritten(Gen, "MaxNPCSOnLevel", 255);
	AllowedAndWritten(Gen, "NPCSPerMinute", 0);
	AllowedAndWritten(Gen, "NPCAcceleration", 255);
	AllowedAndWritten(Gen, "HiveStateChangeTime", 1);
	AllowedAndWritten(Gen, "HiveStateChangeTime", 4681);
	AllowedAndWritten(Gen, "EnemyGenerated", 1);
	{
		int32_t lowest = 0, highest = 0;

		Check(host::LevelSettings_Range(Gen, "HiveStateChangeTime", &lowest, &highest) && lowest == 1 && highest == 4681 &&
		      host::LevelSettings_Range(Env, "predator_num_spears", &lowest, &highest) && lowest == 0 && highest == 32767,
		      "the ranges shown beside the boxes are the table's: hive 1 to 4681, spears 0 to 32767");
	}
	{
		std::vector<uint8_t> settings = envDefaults;
		char const *why = 0;

		PutInt(&settings, 16, (int32_t)0xabcdef80u | 0x17);
		Check(host::LevelSettings_Set(Env, &settings, "kit", 0x5f, &why) && (uint32_t)IntAt(settings, 16) == (0xabcdef80u | 0x5f) &&
		      Get(Env, settings, "kit") == 0x5f, "the kit: its seven bits set (0x5f), the spare bits kept as they were");
	}

	if (argc < 5)
	{
		printf("  FAILED  usage: host_level_settings_test <Temple .rif> <Base .rif> <Area52 .rif> <folder>\n");
		return 1;
	}

	std::string const folder = std::string(argv[4]) + "\\level_settings_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), NULL))
	{
		printf("  FAILED  the test couldn't make its folder %s (is it there already?)\n", folder.c_str());
		return 1;
	}

	std::vector<std::string> made;

	// ---- 4. Temple, as the game reads it
	{
		std::vector<char> original, after;

		Check(ReadWholeFile(argv[1], &original), "Temple is read (only read, never written)");

		OpenLevelFile level;

		if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
		{
			Check(false, "the game's loader takes Temple");
		}
		else
		{
			std::vector<uint8_t> env, gen;
			bool hadEnv = false, hadGen = false;
			char const *why = 0;

			Check(host::LevelSettings_Read(&level, Env, &env, &hadEnv, &why) == host::LevelSettingsResult::Done && hadEnv &&
			      Get(Env, env, "sky_colour_red") == 50 && Get(Env, env, "sky_colour_green") == 50 && Get(Env, env, "sky_colour_blue") == 60 &&
			      Get(Env, env, "kit") == 0x57 && Get(Env, env, "predator_num_spears") == 30, "Temple's AVPENVIR: sky 50, 50, 60; kit 0x57; 30 spears");
			Check(host::LevelSettings_Read(&level, Gen, &gen, &hadGen, &why) == host::LevelSettingsResult::Done && hadGen &&
			      Get(Gen, gen, "EnemyGenerated") == 1 && Get(Gen, gen, "MaxNPCSOnLevel") == 3 && Get(Gen, gen, "NPCSPerMinute") == 4 &&
			      Get(Gen, gen, "NPCAcceleration") == 0 && Get(Gen, gen, "HiveStateChangeTime") == 60, "Temple's GLOGENDC: marines, 3, 4, 0, 60 s");

			// ---- 5. edited, written through the loader, saved, read again
			size_t const objectsBefore = ObjectCount(&level);
			std::vector<uint8_t> envWanted = env, genWanted = gen;

			Check(host::LevelSettings_Set(Env, &envWanted, "sky_colour_red", 7, &why) && host::LevelSettings_Set(Env, &envWanted, "kit", 0x56, &why) &&
			      host::LevelSettings_Set(Env, &envWanted, "predator_num_spears", 12, &why) && host::LevelSettings_Set(Gen, &genWanted, "EnemyGenerated", 0, &why) &&
			      host::LevelSettings_Set(Gen, &genWanted, "HiveStateChangeTime", 4681, &why), "the changes are allowed: red 7, the pistol off (0x56), 12 spears, aliens, 4681 s");
			Check(host::LevelSettings_Write(&level, Env, envWanted, &why) == host::LevelSettingsResult::Done, "the environment is written through the game's loader");
			Check(host::LevelSettings_Write(&level, Gen, genWanted, &why) == host::LevelSettingsResult::Done, "the generators are written through the game's loader");
			Check(level.HasChanges(), "the level now has changes to save");

			std::string const copy = folder + "\\temple.rif";

			made.push_back(copy);
			Check(level.SaveAs(copy.c_str()) == LevelSaveResult::Saved, "the copy is saved");

			OpenLevelFile again;

			if (again.Open(copy.c_str()) != LevelOpenResult::Opened || !again.Level())
			{
				Check(false, "the game's loader takes the copy");
			}
			else
			{
				std::vector<uint8_t> envAgain, genAgain;

				Check(host::LevelSettings_Read(&again, Env, &envAgain, &hadEnv, &why) == host::LevelSettingsResult::Done && envAgain == envWanted,
				      "in the copy: the environment is exactly what was written (every other byte of it as it was)");
				Check(host::LevelSettings_Read(&again, Gen, &genAgain, &hadGen, &why) == host::LevelSettingsResult::Done && genAgain == genWanted,
				      "in the copy: the generators are exactly what was written (the spares kept)");
				Check(ObjectCount(&again) == objectsBefore, "the copy holds the same objects");
			}

			Check(ReadWholeFile(argv[1], &after) && after == original, "Temple's own file is byte for byte as it was");
		}
	}

	// ---- 6. Base: neither chunk
	{
		OpenLevelFile level;

		if (level.Open(argv[2]) != LevelOpenResult::Opened || !level.Level())
		{
			Check(false, "the game's loader takes Base");
		}
		else
		{
			std::vector<uint8_t> env, gen;
			bool hadEnv = true, hadGen = true;
			char const *why = 0;

			Check(host::LevelSettings_Read(&level, Env, &env, &hadEnv, &why) == host::LevelSettingsResult::Done && !hadEnv && env == envDefaults,
			      "Base holds no AVPENVIR: the game's defaults are shown");
			Check(host::LevelSettings_Read(&level, Gen, &gen, &hadGen, &why) == host::LevelSettingsResult::Done && !hadGen && gen == genDefaults,
			      "Base holds no GLOGENDC: the game's defaults are shown");
			Check(host::LevelSettings_Set(Env, &env, "sky_colour_blue", 90, &why) && host::LevelSettings_Set(Gen, &gen, "MaxNPCSOnLevel", 9, &why),
			      "one change each: blue 90, max 9");
			Check(host::LevelSettings_Write(&level, Env, env, &why) == host::LevelSettingsResult::Done &&
			      host::LevelSettings_Write(&level, Gen, gen, &why) == host::LevelSettingsResult::Done, "both chunks are made, through the game's loader");

			std::string const copy = folder + "\\base.rif";

			made.push_back(copy);
			Check(level.SaveAs(copy.c_str()) == LevelSaveResult::Saved, "the copy is saved");

			OpenLevelFile again;

			if (again.Open(copy.c_str()) != LevelOpenResult::Opened || !again.Level())
			{
				Check(false, "the game's loader takes the copy");
			}
			else
			{
				std::vector<uint8_t> envAgain, genAgain;

				Check(host::LevelSettings_Read(&again, Env, &envAgain, &hadEnv, &why) == host::LevelSettingsResult::Done && hadEnv && envAgain == env &&
				      Get(Env, envAgain, "sky_colour_blue") == 90 && Get(Env, envAgain, "kit") == 0x17, "in the copy: an AVPENVIR, the defaults with blue 90");
				Check(host::LevelSettings_Read(&again, Gen, &genAgain, &hadGen, &why) == host::LevelSettingsResult::Done && hadGen && genAgain == gen &&
				      Get(Gen, genAgain, "MaxNPCSOnLevel") == 9 && Get(Gen, genAgain, "HiveStateChangeTime") == 60, "in the copy: a GLOGENDC, the defaults with max 9");
			}
		}
	}

	// ---- 7. Area52: a hive time the form wouldn't take, kept
	{
		OpenLevelFile level;

		if (level.Open(argv[3]) != LevelOpenResult::Opened || !level.Level())
		{
			Check(false, "the game's loader takes Area52");
		}
		else
		{
			std::vector<uint8_t> gen;
			bool had = false;
			char const *why = 0;

			Check(host::LevelSettings_Read(&level, Gen, &gen, &had, &why) == host::LevelSettingsResult::Done && had &&
			      Get(Gen, gen, "HiveStateChangeTime") == 32000, "Area52's hive time is read as the map holds it: 32000 s, more than 4681");
			Check(host::LevelSettings_Set(Gen, &gen, "MaxNPCSOnLevel", 5, &why) && host::LevelSettings_Write(&level, Gen, gen, &why) == host::LevelSettingsResult::Done,
			      "only Max NPCs is changed (5), and it's written");

			std::vector<uint8_t> genNow;

			Check(host::LevelSettings_Read(&level, Gen, &genNow, &had, &why) == host::LevelSettingsResult::Done && Get(Gen, genNow, "MaxNPCSOnLevel") == 5 &&
			      Get(Gen, genNow, "HiveStateChangeTime") == 32000, "the map's own 32000 s is kept: only a changed field is checked");

			// ---- 8. a payload of the wrong size is never written
			unsigned const generation = level.Generation();
			std::vector<uint8_t> wrong(27, 0);

			Check(host::LevelSettings_Write(&level, Gen, wrong, &why) == host::LevelSettingsResult::BadCall && level.Generation() == generation,
			      "a 27-byte GLOGENDC is refused, and the level is left as it was");

			std::vector<uint8_t> lies = envDefaults;

			PutInt(&lies, 0, 28);
			Check(host::LevelSettings_Write(&level, Env, lies, &why) == host::LevelSettingsResult::BadCall && level.Generation() == generation,
			      "an AVPENVIR that says 28 in 24 bytes is refused, and the level is left as it was");
			Check(host::LevelSettings_Write(0, Env, envDefaults, &why) == host::LevelSettingsResult::BadCall, "no level: refused");
		}
	}

	for (std::string const &one : made)
	{
		DeleteFileA(one.c_str());
	}

	RemoveDirectoryA(folder.c_str());

	printf("%d checks, %d failed\n", checksDone, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
