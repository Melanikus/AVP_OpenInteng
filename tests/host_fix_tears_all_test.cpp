// The test of Fix tears (All) (src/host/fix_tears_all.cpp) on the game's own levels given on the command line: every pair is welded, the level still builds, and a second run finds nothing more to weld.
// Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_fix_tears_all_test.args builds it; host_fix_tears_all_test <level file> [...]. A non-zero result means a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "fix_tears_all.h"

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

static void TestFile(std::string const & path)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	FixTearsAllCounts first, second;

	Check(FixTearsAll(&file, file.Level(), 5, &first), "the weld runs");
	printf("  (%zu objects, %zu pairs welded, %zu points moved, %zu pairs refused%s%s)\n", first.objects, first.pairs, first.welded, first.refused, first.firstWhy.empty() ? "" : ": ", first.firstWhy.c_str());

	std::vector<char> built;

	file.BuildLevel(built);
	Check(!built.empty(), "the level still builds");

	Check(FixTearsAll(&file, file.Level(), 5, &second), "a second run");

	std::vector<char> again;

	file.BuildLevel(again);
	Check(again == built, "the second run changes nothing (the points are already together)");
}

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_fix_tears_all_test <level file> [<level file> ...]\n");
		return 2;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i]);
	}

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
