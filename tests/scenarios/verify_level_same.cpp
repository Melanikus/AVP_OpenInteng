// verify_level_same: "nothing changed in the level": two level files compared chunk by chunk, every byte, but the version counters every save
// moves on (tests/level_same.h says which and why). The one shared check for a scenario's "change nothing" step, and for the command line.
// Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_level_same.exe <a.rif> <b.rif> [--ignore <CHUNKID>]...
//   Each file packed (REBCRIF1) or not (REBINFF2). --ignore skips every chunk of that name (1 to 8 letters, digits or _): it must still be at
//   the same place in both files, but its size and everything inside it are not compared.
// PRINTS AND GIVES BACK: "SAME (n chunks, m version counters ignored)" and 0; or "DIFFERENT (...)" and up to 20 lines, each a chunk's path
//   (REBINFF2/REBSHAPE#12/SHPPOLYS: #n counts from 1 among the chunks of that name in one place, given when there are several), the offset
//   in what it holds and the two values, and 1; or "REFUSED <file>: <why>" and 2 for a file it won't read (not a level, a size that doesn't
//   fit, chunks too deep, more chunks inside one chunk or in all than the game's loader is given, the game's loader refuses it) and for a
//   call it doesn't take.
// In a scenario: verify C:/Users/Melanikus/source/AvP-builds-excluded/<folder>/verify_level_same.exe {save1} {save2}
//   (run-shards.sh builds it from this tree's tests/scenarios/verify_level_same.args, whatever folder the line names).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "level_same.h"

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

// The game's code stops only when it can't go on (no memory): a file it won't read, so 2
void avp_exit(int code)
{
	printf("REFUSED the game's code stopped the program (code %d)\n", code);
	exit(2);
}

// the game's log: why its loader refused a file, said where the scenario's record shows it
void LogErrorStringC(const char *line)
{
	printf("  the game's loader says: %s\n", line ? line : "");
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static bool IsChunkName(char const *text)
{
	size_t const length = text ? strlen(text) : 0;

	if (length < 1 || length > 8)
	{
		return false;
	}

	for (size_t n = 0; n < length; n++)
	{
		char const letter = text[n];

		if (!((letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '_'))
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	std::vector<char const *> files;
	std::vector<std::string> ignored;

	for (int n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "--ignore") == 0)
		{
			if (n + 1 >= argc || !IsChunkName(argv[n + 1]))
			{
				printf("REFUSED --ignore takes a chunk's name: 1 to 8 letters, digits or _\n");
				return 2;
			}

			ignored.push_back(argv[++n]);
		}
		else
		{
			files.push_back(argv[n]);
		}
	}

	if (files.size() != 2)
	{
		printf("REFUSED usage: verify_level_same <a.rif> <b.rif> [--ignore <CHUNKID>]...\n");
		return 2;
	}

	return PrintLevelSameReport(CompareLevelFiles(files[0], files[1], ignored));
}
