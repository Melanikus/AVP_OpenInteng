// verify_copyover: the check of a scenario that used Light Sourcing's O with PLACED lights (inteng's FUN_004acf30 0x4ad34e-0x4ad3f5; light-closeout-frost.md TB-L1), read back through the game's own
// loader. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The rules: src/host/light_keys.h LightCopyOver.
//
// USE: verify_copyover.exe <opened level> <copy> <set light A (from 1)> <lamp A> <set light B (from 1)> <lamp B> <lamp C>
// The scenario: C on lamp A, O on set light A (a placed light over a light of the set); C on set light B, O on lamp B (a light of the set over a placed light); C on lamp A, O on lamp C (placed over
// placed); then Exit (the write-back takes 0x300 out of every engine flags word). CHECKS, the opened level's numbers against the copy's:
//  - set light A: lamp A's brightness, spread, range, UP colour (T was not pressed), engine flags (& ~0x300), local flags | 4; its number, place and turn its own;
//  - lamp B: set light B's brightness, spread, range, its colour as the UP colour, engine flags (& ~0x300), local flags | 4; its down colour, times, types, flags and extra words its own;
//  - lamp C: lamp A's 15 numbers (engine & ~0x300, local | 4), its extra words its own;
//  - lamp A and set light B as they were.
// Gives back 0 only if every check passed.

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
#include "light_edit.h"

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

static int32_t W(std::vector<unsigned char> const &b, size_t at)
{
	int32_t v = 0;

	if (at + 4 <= b.size())
	{
		memcpy(&v, b.data() + at, 4);
	}

	return v;
}

static std::vector<unsigned char> SetLight(OpenLevelFile &f, int fromOne)
{
	std::vector<unsigned char> b;
	Chunk *const c = fromOne >= 1 ? host::FindLightChunk(f.Level(), (size_t)(fromOne - 1)) : nullptr;

	if (c)
	{
		f.PayloadOf(c, &b);
	}

	return b;
}

static std::vector<unsigned char> Lamp(OpenLevelFile &f, char const *name)
{
	std::vector<unsigned char> b;
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			Chunk *const c = host::FindPlacedLightChunk(f.Level(), (uint8_t const *)&each()->object_data.ID);

			if (c)
			{
				f.PayloadOf(c, &b);
			}
		}
	}

	return b;
}

static uint32_t const NoPreLight = 0x300;

int main(int argc, char **argv)
{
	if (argc != 8)
	{
		printf("verify_copyover: <opened level> <copy> <set light A> <lamp A> <set light B> <lamp B> <lamp C>\n");
		return 2;
	}

	OpenLevelFile opened, copy;

	Check(opened.Open(argv[1]) == LevelOpenResult::Opened && opened.Level() != nullptr, std::string("the game's loader reads the opened level: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	if (checksFailed)
	{
		printf("FAILED verify_copyover\n");
		return 1;
	}

	int const a = atoi(argv[3]), b = atoi(argv[5]);
	std::vector<unsigned char> const setA0 = SetLight(opened, a), setA1 = SetLight(copy, a), setB0 = SetLight(opened, b), setB1 = SetLight(copy, b);
	std::vector<unsigned char> const lampA0 = Lamp(opened, argv[4]), lampA1 = Lamp(copy, argv[4]), lampB0 = Lamp(opened, argv[6]), lampB1 = Lamp(copy, argv[6]), lampC0 = Lamp(opened, argv[7]),
		lampC1 = Lamp(copy, argv[7]);

	Check(setA0.size() >= 84 && setA1.size() == setA0.size() && setB0.size() >= 84 && setB1.size() == setB0.size(), "both levels hold the two lights of the set");
	Check(lampA0.size() >= 64 && lampA1.size() == lampA0.size() && lampB0.size() >= 64 && lampB1.size() == lampB0.size() && lampC0.size() >= 64 && lampC1.size() == lampC0.size(), "and the three lamps");

	if (checksFailed)
	{
		printf("FAILED verify_copyover\n");
		return 1;
	}

	// set light A: lamp A's numbers (STDLIGHT: brightness 52, spread 56, range 60, colour 64, engine 68, local 72; PLOBJLIT: 0, 4, 8, up 12, down 16, engine 20, local 24)
	Check(W(setA1, 52) == W(lampA0, 0) && W(setA1, 56) == W(lampA0, 4) && W(setA1, 60) == W(lampA0, 8), "set light " + std::to_string(a) + " has " + argv[4] + "'s brightness, spread and range (" +
		std::to_string(W(setA1, 52)) + " " + std::to_string(W(setA1, 56)) + " " + std::to_string(W(setA1, 60)) + ")");
	Check(W(setA1, 64) == W(lampA0, 12), "its UP colour (0x" + std::to_string(W(setA1, 64)) + ")");
	Check((uint32_t)W(setA1, 68) == ((uint32_t)W(lampA0, 20) & ~NoPreLight) && (uint32_t)W(setA1, 72) == ((uint32_t)W(lampA0, 24) | 4), "its engine flags and its local flags with 4 in");
	Check(memcmp(setA1.data(), setA0.data(), 52) == 0 && memcmp(setA1.data() + 76, setA0.data() + 76, setA1.size() - 76) == 0, "set light " + std::to_string(a) + "'s number, place and turn are its own");

	// lamp B: set light B's numbers
	Check(W(lampB1, 0) == W(setB0, 52) && W(lampB1, 4) == W(setB0, 56) && W(lampB1, 8) == W(setB0, 60), std::string(argv[6]) + " has set light " + std::to_string(b) + "'s brightness, spread and range");
	Check(W(lampB1, 12) == W(setB0, 64) && W(lampB1, 16) == W(lampB0, 16), "its colour as the UP colour, the down colour its own");
	Check((uint32_t)W(lampB1, 20) == ((uint32_t)W(setB0, 68) & ~NoPreLight) && (uint32_t)W(lampB1, 24) == ((uint32_t)W(setB0, 72) | 4), "its engine flags and its local flags with 4 in");
	Check(memcmp(lampB1.data() + 28, lampB0.data() + 28, lampB1.size() - 28) == 0, std::string(argv[6]) + "'s times, types, flags and extra words are its own");

	// lamp C: lamp A's 15 numbers
	std::vector<unsigned char> wanted(lampA0.begin(), lampA0.begin() + 60);
	int32_t const engine = (int32_t)((uint32_t)W(lampA0, 20) & ~NoPreLight), local = (int32_t)((uint32_t)W(lampA0, 24) | 4);

	memcpy(wanted.data() + 20, &engine, 4);
	memcpy(wanted.data() + 24, &local, 4);
	Check(memcmp(lampC1.data(), wanted.data(), 60) == 0, std::string(argv[7]) + " holds " + argv[4] + "'s 15 numbers (local flags with 4 in)");
	Check(memcmp(lampC1.data() + 60, lampC0.data() + 60, lampC1.size() - 60) == 0, "and its own extra words");

	// the copied ones
	Check(lampA1 == lampA0, std::string(argv[4]) + " is as it was");
	Check(setB1 == setB0, "set light " + std::to_string(b) + " is as it was");

	if (checksFailed)
	{
		printf("FAILED verify_copyover (%d)\n", checksFailed);
		return 1;
	}

	printf("PASSED verify_copyover\n");
	return 0;
}
