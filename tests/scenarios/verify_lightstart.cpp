// verify_lightstart: the check of a scenario that changed a light of the set in one visit of Light Sourcing and left a second visit unchanged (inteng's start takes the changed flag out:
// FUN_004ae5d0 0x4aedb8; light-closeout-frost.md TB-L8), read back through the game's own loader. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USE: verify_lightstart.exe <opened level> <copy> <the light of the set changed (from 1)>
// CHECKS: in the copy the light's spread is larger than in the opened level (the first visit's change was kept), and its local flags are the opened level's without 4 (changed): the second visit's start
// took it out. Gives back 0 only if every check passed.

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

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		printf("verify_lightstart: <opened level> <copy> <light (from 1)>\n");
		return 2;
	}

	OpenLevelFile opened, copy;

	Check(opened.Open(argv[1]) == LevelOpenResult::Opened && opened.Level() != nullptr, std::string("the game's loader reads the opened level: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	int const n = atoi(argv[3]);
	Chunk *const before = checksFailed == 0 && n >= 1 ? host::FindLightChunk(opened.Level(), (size_t)(n - 1)) : nullptr;
	Chunk *const after = checksFailed == 0 && n >= 1 ? host::FindLightChunk(copy.Level(), (size_t)(n - 1)) : nullptr;
	std::vector<unsigned char> b, a;

	Check(before && after && opened.PayloadOf(before, &b) && copy.PayloadOf(after, &a) && b.size() >= 84 && a.size() >= 84, "both levels hold light " + std::to_string(n) + " of the set");

	if (checksFailed)
	{
		printf("FAILED verify_lightstart\n");
		return 1;
	}

	// STDLIGHT: spread 56, local flags 72
	Check(W(a, 56) > W(b, 56), "its spread is the larger one the first visit made: " + std::to_string(W(b, 56)) + " -> " + std::to_string(W(a, 56)));
	Check((W(a, 72) & 4) == 0, "its changed flag (4) is OUT: the second visit's start took it out (local flags 0x" + std::to_string(W(a, 72)) + ")");
	Check(W(a, 72) == (W(b, 72) & ~4), "its other local flags are the opened level's");

	if (checksFailed)
	{
		printf("FAILED verify_lightstart (%d)\n", checksFailed);
		return 1;
	}

	printf("PASSED verify_lightstart\n");
	return 0;
}
