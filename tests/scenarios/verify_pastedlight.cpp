// verify_pastedlight: the check of a scenario that pasted a PLACED light in Light Sourcing (C on a placed light, V at the camera: inteng's FUN_004abb00), read back through the game's own loader.
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The rules: AvP-docs\engine\inteng\reassess\light-create-frost.md.
//
// USE: verify_pastedlight.exe <the level the editor opened> <the copy it saved> <the copied light's object> <the new object's name> <x> <y> <z>
// CHECKS: both levels are read by the game's loader; the copy has an object of the new name, with:
//  - the copied object's shape (the same shape number);
//  - a header whose flags are the placed bit 0x400 and the copied object's load bits (game modes, platform load set, PC / PSX / Saturn load, not-difficulty): INTENG BUG, FIXED (inteng: 0x400 only,
//    so the game never loaded it);
//  - its place x y z (the file's units: the camera's place divided by the level's scale);
//  - a Placed Object strategy (kind 62, 52 bytes): the copied one's Type, ExtraData and mass, its integrity with bit 31 put in, flags 1, every other word 0;
//  - a PLOBJLIT with the copied light's 15 numbers and no extra words;
// and the copied object is as it was in the opened level (its strategy and its light). Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_light.hpp"
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

// The bytes of the object's AVPSTRAT (inside its OBJPRJDT) after the chunk's heading; empty when it has none
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
	if (argc != 8)
	{
		printf("verify_pastedlight: <opened level> <copy> <copied light's object> <new object's name> <x> <y> <z>\n");
		return 2;
	}

	OpenLevelFile opened, copy;

	Check(opened.Open(argv[1]) == LevelOpenResult::Opened && opened.Level() != nullptr, std::string("the game's loader reads the opened level: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	if (checksFailed)
	{
		printf("FAILED verify_pastedlight\n");
		return 1;
	}

	Object_Chunk *const before = ObjectNamed(opened, argv[3]);
	Object_Chunk *const source = ObjectNamed(copy, argv[3]);
	Object_Chunk *const made = ObjectNamed(copy, argv[4]);

	Check(before && source && before->get_header() && source->get_header(), std::string("both levels hold the copied light's object \"") + argv[3] + "\"");
	Check(made && made->get_header(), std::string("the copy has the new object \"") + argv[4] + "\", with a header");

	if (!before || !source || !made || !before->get_header() || !source->get_header() || !made->get_header())
	{
		printf("FAILED verify_pastedlight\n");
		return 1;
	}

	uint32_t const loadBits = 0x800 | 0x1000 | 0x2000 | 0x4000 | 0x8000 | 0x10000 | 0x20000 | 0x100000 | 0x200000 | 0x400000;
	uint32_t const wantedFlags = 0x400u | ((uint32_t)source->get_header()->flags & loadBits);

	// the shape's number: the header's word at +56 (after the flags, the lock user, the place, the turn, the number and the version_no)
	std::vector<unsigned char> sourceHead, madeHead;

	copy.PayloadOf(source->get_header(), &sourceHead);
	copy.PayloadOf(made->get_header(), &madeHead);
	Check(sourceHead.size() >= 60 && madeHead.size() >= 60 && Word(madeHead, 56) == Word(sourceHead, 56), "the new object uses the copied object's shape (number " + std::to_string((long long)Word(sourceHead, 56)) + ")");
	Check((uint32_t)made->get_header()->flags == wantedFlags, "its header's flags are 0x400 and the copied object's load bits (" + std::to_string((unsigned long long)wantedFlags) + "; it holds " +
		std::to_string((unsigned long long)(uint32_t)made->get_header()->flags) + ")");
	Check((wantedFlags & 0x8000) != 0, "so the game loads it (PC load 0x8000 is among them)");

	ChunkVectorInt const &at = made->object_data.location;

	Check(at.x == atoi(argv[5]) && at.y == atoi(argv[6]) && at.z == atoi(argv[7]), "its place is " + std::string(argv[5]) + " " + argv[6] + " " + argv[7] + " (it is " + std::to_string((long long)at.x) + " " +
		std::to_string((long long)at.y) + " " + std::to_string((long long)at.z) + ")");

	// the strategy: 52 bytes of kind 62, Type / ExtraData / mass the copied one's, integrity with bit 31, flags 1, the rest 0
	std::vector<unsigned char> const was = StrategyOf(copy, source);
	std::vector<unsigned char> const now = StrategyOf(copy, made);
	bool restZero = true;

	for (size_t word : { 0, 4, 12, 16, 40, 44, 48 })
	{
		restZero = restZero && Word(now, word) == 0;
	}

	Check(now.size() == 52 && Word(now, 8) == 62, "its strategy is a Placed Object (kind 62), 52 bytes");
	Check(was.size() >= 52 && Word(now, 20) == Word(was, 20) && Word(now, 24) == Word(was, 24) && Word(now, 28) == Word(was, 28), "Type, ExtraData and mass are the copied strategy's");
	Check((uint32_t)Word(now, 32) == ((uint32_t)Word(was, 32) | 0x80000000u), "its integrity is the copied one's with bit 31 in");
	Check(Word(now, 36) == 1 && restZero, "flags 1, every other word 0");

	// the PLOBJLIT: the copied light's 15 numbers, no extra words
	Placed_Object_Light_Chunk *const newLight = (Placed_Object_Light_Chunk *)made->lookup_single_child("PLOBJLIT");
	Placed_Object_Light_Chunk *const sourceLight = (Placed_Object_Light_Chunk *)source->lookup_single_child("PLOBJLIT");
	Placed_Object_Light_Chunk *const beforeLight = (Placed_Object_Light_Chunk *)before->lookup_single_child("PLOBJLIT");

	// what is compared, for a failure's reading: the two strategies' words and the two lights' 15 numbers
	printf("  info  strategy, copied / new:");

	for (size_t word = 0; word < 52; word += 4)
	{
		printf(" %d/%d", Word(was, word), Word(now, word));
	}

	printf("\n");

	if (newLight && sourceLight)
	{
		int const *const a = (int const *)&sourceLight->light;
		int const *const b = (int const *)&newLight->light;

		printf("  info  light, copied / new:");

		for (int word = 0; word < 15; word++)
		{
			printf(" %d/%d", a[word], b[word]);
		}

		printf("\n");
	}

	// the 15 numbers the copied light's, but the local flags with 4 (changed) in (the new light's record: the copied flags | 4, 0x4abf6b; the write-back writes them, 0x4adcfe)
	bool sameLight = newLight && sourceLight;

	if (sameLight)
	{
		Placed_Object_Light_Data wanted = sourceLight->light;

		wanted.local_light_flags |= 4;
		sameLight = memcmp(&newLight->light, &wanted, sizeof(Placed_Object_Light_Data)) == 0;
	}

	Check(sameLight, "its PLOBJLIT holds the copied light's 15 numbers, its local flags with 4 (changed) in");
	Check(newLight && newLight->num_extra_data == 0, "and no extra words");

	// the copied object as it was
	Check(sourceLight && beforeLight && memcmp(&sourceLight->light, &beforeLight->light, sizeof(Placed_Object_Light_Data)) == 0 && was == StrategyOf(opened, before),
		"the copied object's light and strategy are as they were");

	printf("%s verify_pastedlight\n", checksFailed ? "FAILED" : "PASSED");
	return checksFailed ? 1 : 0;
}
