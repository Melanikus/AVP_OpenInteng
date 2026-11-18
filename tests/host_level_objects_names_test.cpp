// The test that the object list's rows keep their names (level_objects.h LevelObjects_Read; host::LevelObject's name is a pointer). A virtual object's row points at a copy
// of its name the list keeps; that copy must stay as it is while the rows are in use, so a later read (of another level, or by another feature in the middle of a loop over
// the first rows) must not take it away. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (Bishop's sweep for borrowed pointers into strings changed later:
// before, one list of names was emptied and filled again by every read, so the rows of the read before pointed at the next level's names, or at freed memory).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_level_objects_names_test.exe <Invasion.RIF> <Hangar.RIF> (tests/host_level_objects_names_test.runargs): two stock levels with virtual objects (60 and 21), only read.
// THE CASE: Invasion's rows read and their names copied; Hangar held in a second OpenLevelFile (Invasion's stays open, so its objects' own names are still there) and read;
// every one of Invasion's rows still reads the name it had. Then Invasion read again: the same names.

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
#include "level_objects.h"
#include "level_view.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as tests/host_level_objects_test.cpp's). The loader opens files through avp_CreateFile: here, Windows' own.

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

// The game side's answer to "where did the game load the open level from?": the level the test holds next
static std::string gamePath;

bool LevelView_FilePath(char *path, size_t size)
{
	if (!path || !size)
	{
		return false;
	}

	path[0] = '\0';

	if (gamePath.empty() || gamePath.size() >= size)
	{
		return false;
	}

	// a byte at a time, never past size - 1 (the path comes from the command line)
	size_t n = 0;

	for (; n < gamePath.size() && n + 1 < size; n++)
	{
		path[n] = gamePath[n];
	}

	path[n] = '\0';
	return n == gamePath.size();
}

static std::vector<std::string> NamesOf(std::vector<host::LevelObject> const &rows)
{
	std::vector<std::string> names;

	for (host::LevelObject const &row : rows)
	{
		names.push_back(row.name ? row.name : "(none)");
	}

	return names;
}

// how many of the rows read their name as it was copied (and the first that doesn't)
static size_t Same(std::vector<host::LevelObject> const &rows, std::vector<std::string> const &names, std::string *first)
{
	size_t same = 0;

	for (size_t i = 0; i < rows.size() && i < names.size(); i++)
	{
		std::string const now = rows[i].name ? rows[i].name : "(none)";

		if (now == names[i])
		{
			same++;
		}
		else if (first->empty())
		{
			*first = "row " + std::to_string((unsigned long long)i) + " was \"" + names[i] + "\", reads \"" + now.substr(0, 40) + "\"";
		}
	}

	return same;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 3)
	{
		printf("  FAILED  usage: host_level_objects_names_test <Invasion.RIF> <Hangar.RIF> (tests/host_level_objects_names_test.runargs)\n");
		return 1;
	}

	puts("the object list's rows keep their names while they are in use (LevelObjects_Read):");

	OpenLevelFile invasion, hangar;
	std::vector<host::LevelObject> rowsA, rowsB, rowsC;

	gamePath = argv[1];
	Check(LevelObjects_Hold(&invasion) == LevelOpenResult::Opened && LevelObjects_Read(&rowsA), "Invasion is held and its rows read");

	std::vector<std::string> const namesA = NamesOf(rowsA);
	size_t const objectsA = rowsA.size();

	Check(objectsA > 60, "Invasion's rows: " + std::to_string((unsigned long long)objectsA) + " (its 60 virtual objects among them)");

	gamePath = argv[2];

	bool const hangarRead = LevelObjects_Hold(&hangar) == LevelOpenResult::Opened && LevelObjects_Read(&rowsB);

	Check(hangarRead, "Hangar is held in a second OpenLevelFile and its rows read (" + std::to_string((unsigned long long)rowsB.size()) + ")");

	std::string first;
	size_t const same = Same(rowsA, namesA, &first);

	Check(same == objectsA, "every one of Invasion's rows still reads its own name after Hangar's read: " + std::to_string((unsigned long long)same) + " of " +
		std::to_string((unsigned long long)objectsA) + (first.empty() ? std::string() : " (" + first + ")"));

	gamePath = argv[1];
	Check(LevelObjects_Hold(&invasion) == LevelOpenResult::Opened && LevelObjects_Read(&rowsC) && NamesOf(rowsC) == namesA, "Invasion held and read again: the same names in the same order");

	first.clear();
	Check(Same(rowsB, NamesOf(rowsB), &first) == rowsB.size(), "and Hangar's rows read their own names still");

	LevelObjects_Use(nullptr);
	printf("host_level_objects_names_test: %d failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
