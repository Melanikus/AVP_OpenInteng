// verify_level_same: "nothing changed in the level": two level files compared chunk by chunk, every byte, but the version counters every save
// moves on (tests/level_same.h says which and why). The one shared check for a scenario's "change nothing" step, and for the command line.
// Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_level_same.exe <a.rif> <b.rif> [--ignore <CHUNKID>]... [--drop <CHUNKID>]...
//   Each file packed (REBCRIF1) or not (REBINFF2). --ignore skips every chunk of that name (1 to 8 letters, digits or _): it must still be at
//   the same place in both files, but its size and everything inside it are not compared. --drop leaves every chunk of that name out of both
//   walks, so one file may hold it and the other not (Delete Waypoints takes a module's WAYPOINT out: --drop WAYPOINT; Dietrich 11-21).
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

// A level's path from the command line, made whole (GetFullPathNameA) and checked to be an existing file, not a folder, before anything opens it; one that
// climbs out of a folder ("..") or is longer than a path can be is refused. What is opened is the whole path, not the argument. (Added 2026-11-18 by
// Melanikus (Drake): Snyk Code, a command line's path read unchecked.)
static bool LevelSameWholePath(char const *given, std::string *out)
{
	if (!given || !given[0] || strlen(given) >= MAX_PATH || strstr(given, "..") != nullptr)
	{
		return false;
	}

	char full[MAX_PATH] = "";
	DWORD const length = GetFullPathNameA(given, MAX_PATH, full, nullptr);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	DWORD const kind = GetFileAttributesA(full);

	if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		return false;
	}

	*out = full;
	return true;
}

// A chunk's name for --ignore, already checked by IsChunkName (1 to 8 letters, digits or _), made again letter by letter from the table of
// the letters a name may have: what reaches the comparison is the table's letters, never the command line's own text. (Added 2026-11-18
// by Melanikus (Drake): Snyk Code followed the argument into level_same.cpp's arithmetic.)
static std::string IgnoredChunkName(char const *text)
{
	static char const kLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
	std::string name;

	for (size_t n = 0; n < 8 && text[n] != '\0'; n++)
	{
		for (size_t k = 0; k + 1 < sizeof(kLetters); k++)
		{
			if (kLetters[k] == text[n])
			{
				name.push_back(kLetters[k]);
				break;
			}
		}
	}

	return name;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	std::vector<std::string> files;
	std::vector<std::string> ignored;
	std::vector<std::string> dropped;

	for (int n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "--ignore") == 0)
		{
			if (n + 1 >= argc || !IsChunkName(argv[n + 1]))
			{
				printf("REFUSED --ignore takes a chunk's name: 1 to 8 letters, digits or _\n");
				return 2;
			}

			ignored.push_back(IgnoredChunkName(argv[++n]));
		}
		else if (strcmp(argv[n], "--drop") == 0)
		{
			if (n + 1 >= argc || !IsChunkName(argv[n + 1]))
			{
				printf("REFUSED --drop takes a chunk's name: 1 to 8 letters, digits or _\n");
				return 2;
			}

			dropped.push_back(IgnoredChunkName(argv[++n]));
		}
		else
		{
			std::string level;

			if (!LevelSameWholePath(argv[n], &level))
			{
				printf("REFUSED a level: not an existing file's path, or one with \"..\" in it\n");
				return 2;
			}

			files.push_back(level);
		}
	}

	if (files.size() != 2)
	{
		printf("REFUSED usage: verify_level_same <a.rif> <b.rif> [--ignore <CHUNKID>]... [--drop <CHUNKID>]...\n");
		return 2;
	}

	return PrintLevelSameReport(CompareLevelFiles(files[0].c_str(), files[1].c_str(), ignored, dropped));
}
