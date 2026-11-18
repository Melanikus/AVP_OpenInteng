// verify_typelight: the check of a scenario that gave a placed object the Type "Placed light" (16) in Object Placement's Edit and left the mode: inteng's save FUN_004e1f90 (0x4e2cd5-0x4e2dd8)
// makes such an object a DEFAULT placed light (a PLOBJLIT); read back through the game's own loader.
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The rules: AvP-docs\engine\inteng\reassess\light-closeout-frost.md
// TB-L2 and src/host/simple_object_edit.h.
//
// USE: verify_typelight.exe <the level the editor opened> <the copy it saved> <the object> <the range the default light has: 1,000,000 / (100 x the level's scale), cut toward 0; or "scale" to work it out
// from the copy's ENVSDSCL>
// CHECKS: both levels are read by the game's loader; the object had no PLOBJLIT in the opened level; in the copy it has a Placed Object strategy (kind 62) of Type 16, and ONE PLOBJLIT
// (its place among the object's chunks: host_placed_light_type_test) holding exactly inteng's default: brightness 0x10000, spread 0, the range given, up colour 0xffffff, down colour 0, engine flags 4, local flags 0x50, the five times 0, light type 0, on/off type 0,
// flags 1, no extra words. Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_light.hpp"
#include "environment_chunk.hpp"
#include "mishchnk.hpp"
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

static Object_Chunk *ObjectNamed(OpenLevelFile &level, char const *name)
{
	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			return each();
		}
	}

	return nullptr;
}

static std::vector<unsigned char> StrategyOf(OpenLevelFile &level, Object_Chunk *object)
{
	std::vector<unsigned char> bytes;
	Chunk_With_Children *const project = object ? (Chunk_With_Children *)object->lookup_single_child("OBJPRJDT") : nullptr;
	Chunk *const strategy = project ? project->lookup_single_child("AVPSTRAT") : nullptr;

	if (strategy)
	{
		level.PayloadOf(strategy, &bytes);
	}

	return bytes;
}

static int32_t Word(std::vector<unsigned char> const &bytes, size_t at)
{
	int32_t value = 0;

	if (at + 4 <= bytes.size())
	{
		memcpy(&value, bytes.data() + at, 4);
	}

	return value;
}

int main(int argc, char **argv)
{
	if (argc != 5)
	{
		printf("verify_typelight: <opened level> <copy> <object> <range>\n");
		return 2;
	}

	OpenLevelFile opened, copy;

	Check(opened.Open(argv[1]) == LevelOpenResult::Opened && opened.Level() != nullptr, std::string("the game's loader reads the opened level: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	if (checksFailed)
	{
		printf("FAILED verify_typelight\n");
		return 1;
	}

	Object_Chunk *const before = ObjectNamed(opened, argv[3]);
	Object_Chunk *const after = ObjectNamed(copy, argv[3]);

	Check(before && after, std::string("both levels hold \"") + argv[3] + "\"");

	if (!before || !after)
	{
		printf("FAILED verify_typelight\n");
		return 1;
	}

	Check(before->lookup_single_child("PLOBJLIT") == nullptr, "it had no placed light in the opened level");

	std::vector<unsigned char> const strategy = StrategyOf(copy, after);

	Check(strategy.size() >= 52 && Word(strategy, 8) == 62 && Word(strategy, 20) == 16, "in the copy it holds a Placed Object strategy (kind 62) of Type 16, Placed light (kind " + std::to_string(Word(strategy, 8)) + ", Type " +
		std::to_string(Word(strategy, 20)) + ")");

	List<Chunk *> lights;
	after->lookup_child("PLOBJLIT", lights);
	Check(lights.size() == 1, "and ONE placed light (PLOBJLIT): " + std::to_string(lights.size()));

	Chunk *first = nullptr;

	for (LIF<Chunk *> each(&lights); !each.done() && !first; each.next())
	{
		first = each();
	}

	std::vector<unsigned char> light;

	if (first)
	{
		copy.PayloadOf(first, &light);
	}

	// "scale": the range worked out here from the copy's own ENVSDSCL (inteng's DAT_00607bd8 = 100 x the scale, 100.0 when there is none)
	int32_t range = atoi(argv[4]);

	if (strcmp(argv[4], "scale") == 0)
	{
		double scale = 1.0;
		Chunk_With_Children *const envd = dynamic_cast<Chunk_With_Children *>(copy.Level()->lookup_single_child("REBENVDT"));
		Environment_Scale_Chunk const *const s = envd ? dynamic_cast<Environment_Scale_Chunk const *>(envd->lookup_single_child("ENVSDSCL")) : nullptr;

		if (s && s->scale > 0.0)
		{
			scale = s->scale;
		}

		range = (int32_t)(1000000.0 / (100.0 * scale));
		printf("  (the level's scale %g: the default range %d)\n", scale, (int)range);
	}
	int32_t const wanted[16] = { 0x10000, 0, range, 0xffffff, 0, 4, 0x50, 0, 0, 0, 0, 0, 0, 0, 1, 0 };
	char const *const names[16] = { "brightness", "spread", "range", "up colour", "down colour", "engine flags", "local flags", "fade up time", "fade down time", "up time", "down time", "start time",
		"light type", "on/off type", "flags", "extra words" };

	Check(light.size() == 64, "its bytes are the 15 numbers and the count: " + std::to_string(light.size()));

	for (size_t i = 0; i < 16; i++)
	{
		Check(Word(light, 4 * i) == wanted[i], std::string(names[i]) + " " + std::to_string(Word(light, 4 * i)) + " (inteng's default: " + std::to_string(wanted[i]) + ")");
	}

	Placed_Object_Light_Chunk *const loaded = dynamic_cast<Placed_Object_Light_Chunk *>(first);

	Check(loaded && loaded->light.brightness == 0x10000 && loaded->light.range == range && loaded->light.flags == 1 && loaded->num_extra_data == 0, "and the game's loader made it a placed light with those numbers");

	if (checksFailed)
	{
		printf("FAILED verify_typelight (%d)\n", checksFailed);
		return 1;
	}

	printf("PASSED verify_typelight\n");
	return 0;
}
