// verify_objects: the check of a scenario's saved copy for the objects it must have taken out or kept, read back through the game's own loader (as verify_strategy). Written 2026-11-17
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_objects.exe <the copy it saved> gone|kept <an object's name> [gone|kept <name>] ...
// CHECKS: the copy is read by the game's loader; for "gone <name>" no object of the copy (module or placed object) has that exact name; for "kept <name>" one has.
// Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
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

// Whether an object of the level has exactly this name
static bool HasObjectNamed(OpenLevelFile &level, char const *name)
{
	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && object->object_data.o_name && strcmp(object->object_data.o_name, name) == 0)
		{
			return true;
		}
	}

	return false;
}

int main(int argc, char **argv)
{
	if (argc < 4 || (argc - 2) % 2 != 0)
	{
		printf("verify_objects: <copy> gone|kept <name> [gone|kept <name>] ...\n");
		return 2;
	}

	OpenLevelFile copy;

	Check(copy.Open(argv[1]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[1]);

	if (checksFailed)
	{
		printf("FAILED verify_objects\n");
		return 1;
	}

	for (int a = 2; a + 1 < argc; a += 2)
	{
		std::string const what = argv[a];
		bool const there = HasObjectNamed(copy, argv[a + 1]);

		if (what == "gone")
		{
			Check(!there, std::string("the copy has no object named \"") + argv[a + 1] + "\"");
		}
		else if (what == "kept")
		{
			Check(there, std::string("the copy has an object named \"") + argv[a + 1] + "\"");
		}
		else
		{
			Check(false, "a word the check knows (gone or kept), not \"" + what + "\"");
		}
	}

	printf("%s verify_objects\n", checksFailed ? "FAILED" : "PASSED");
	return checksFailed ? 1 : 0;
}
