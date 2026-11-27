// verify_envgen: the check of a level settings scenario's saved copy (Strategy mode's "AvP Environment Settings..." and "AvP Generator
// Properties...", src/host/level_settings.h), read back through the game's own loader and its own chunk classes (AVP_Environment_Settings_Chunk,
// Global_Generator_Data_Chunk), not through the code under test. Written 2026-11-27 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_envgen.exe <the level the editor opened> <the copy it saved> [<field> <value>] ...
//   environment fields: red green blue spears (numbers), stars pistol caster disc hook medicomp jetpack (0 or 1)
//   generator fields: enemy max perminute change hive (numbers)
// CHECKS: both levels are read by the game's loader, and the copy has the original's objects in its order; in the copy's REBENVDT, every named
// field holds its value; every field NOT named holds the original's (or, when the original has no such chunk, the game's own default); the spare
// kit bits and the generators' two spares are the original's; a chunk with no field named is there in the copy exactly when it was in the
// original (nothing made that wasn't asked for). Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "environment_chunk.hpp"
#include "mishchnk.hpp"
#include "avp_chunk.hpp"
#include "level_file.h"

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

// The level's two settings as the game's classes hold them, by the names this program takes; held: whether the level has the chunk (when it
// hasn't, the values are the game's defaults: avp_chunk.cpp:608-627 and the generator chunk's constructor)
struct Settings
{
	bool envHeld = false;
	bool genHeld = false;
	std::map<std::string, long long> env;
	std::map<std::string, long long> gen;
	unsigned spareBits = 0;
	long long spare1 = 0, spare2 = 0;
};

static Settings Read(File_Chunk *level)
{
	Settings out;
	Chunk *const data = level->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = data ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	AVP_Environment_Settings_Chunk *const env = holder ? dynamic_cast<AVP_Environment_Settings_Chunk *>(holder->lookup_single_child("AVPENVIR")) : nullptr;
	Global_Generator_Data_Chunk *const gen = holder ? dynamic_cast<Global_Generator_Data_Chunk *>(holder->lookup_single_child("GLOGENDC")) : nullptr;

	out.env = { { "red", 200 }, { "green", 200 }, { "blue", 200 }, { "spears", 30 }, { "stars", 0 }, { "pistol", 1 }, { "caster", 1 }, { "disc", 1 },
	            { "hook", 0 }, { "medicomp", 1 }, { "jetpack", 0 } };
	out.gen = { { "enemy", 0 }, { "max", 25 }, { "perminute", 4 }, { "change", 2 }, { "hive", 60 } };

	if (env && env->settings)
	{
		AVP_Environment_Settings const &s = *env->settings;

		out.envHeld = true;
		out.env = { { "red", s.sky_colour_red }, { "green", s.sky_colour_green }, { "blue", s.sky_colour_blue }, { "spears", s.predator_num_spears },
		            { "stars", s.stars_in_sky }, { "pistol", s.predator_pistol }, { "caster", s.predator_plasmacaster }, { "disc", s.predator_disc },
		            { "hook", s.predator_grappling_hook }, { "medicomp", s.predator_medicomp }, { "jetpack", s.marine_jetpack } };
		out.spareBits = s.spare_bits;
	}

	if (gen)
	{
		out.genHeld = true;
		out.gen = { { "enemy", gen->EnemyGenerated }, { "max", gen->MaxNPCSOnLevel }, { "perminute", gen->NPCSPerMinute }, { "change", gen->NPCAcceleration },
		            { "hive", gen->HiveStateChangeTime } };
		out.spare1 = gen->spare1;
		out.spare2 = gen->spare2;
	}

	return out;
}

static std::vector<std::string> Names(File_Chunk *level)
{
	std::vector<std::string> names;
	List<Object_Chunk *> objects;

	level->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		names.push_back(std::string(each()->object_data.o_name, strnlen(each()->object_data.o_name, sizeof(each()->object_data.o_name))));
	}

	return names;
}

int main(int argc, char **argv)
{
	if (argc < 3 || (argc - 3) % 2 != 0)
	{
		printf("  FAILED  usage: verify_envgen <original> <copy> [<field> <value>] ...\n");
		return 1;
	}

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level(), std::string("the game's loader reads the original: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level(), std::string("the game's loader reads the copy: ") + argv[2]);

	if (!original.Level() || !copy.Level())
	{
		printf("FAILED verify_envgen (%d checks failed)\n", checksFailed);
		return 1;
	}

	std::vector<std::string> const before = Names(original.Level());
	std::vector<std::string> const after = Names(copy.Level());

	Check(before == after, "the copy has the original's objects, in its order (" + std::to_string(after.size()) + ")");

	Settings const was = Read(original.Level());
	Settings const now = Read(copy.Level());
	std::map<std::string, long long> wantedEnv = was.env, wantedGen = was.gen;
	bool envNamed = false, genNamed = false;

	for (int a = 3; a + 1 < argc; a += 2)
	{
		std::string const field = argv[a];
		long long const value = atoll(argv[a + 1]);

		if (wantedEnv.count(field))
		{
			wantedEnv[field] = value;
			envNamed = true;
		}
		else if (wantedGen.count(field))
		{
			wantedGen[field] = value;
			genNamed = true;
		}
		else
		{
			Check(false, "\"" + field + "\" is a field this program knows");
		}
	}

	Check(now.envHeld == (was.envHeld || envNamed), std::string("the copy ") + (now.envHeld ? "holds" : "holds no") + " AVPENVIR (the original " +
	      (was.envHeld ? "holds" : "holds none") + (envNamed ? ", and a field of it was named" : "") + ")");
	Check(now.genHeld == (was.genHeld || genNamed), std::string("the copy ") + (now.genHeld ? "holds" : "holds no") + " GLOGENDC (the original " +
	      (was.genHeld ? "holds" : "holds none") + (genNamed ? ", and a field of it was named" : "") + ")");

	for (auto const &one : wantedEnv)
	{
		long long const held = now.env.at(one.first);

		Check(held == one.second, "AVPENVIR " + one.first + ": " + std::to_string(held) + " (wanted " + std::to_string(one.second) + ")");
	}

	for (auto const &one : wantedGen)
	{
		long long const held = now.gen.at(one.first);

		Check(held == one.second, "GLOGENDC " + one.first + ": " + std::to_string(held) + " (wanted " + std::to_string(one.second) + ")");
	}

	Check(now.spareBits == was.spareBits && now.spare1 == was.spare1 && now.spare2 == was.spare2, "the spare kit bits and the generators' two spares are the original's");

	if (checksFailed != 0)
	{
		printf("FAILED verify_envgen (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("verify_envgen: every check passed\n");
	return 0;
}
