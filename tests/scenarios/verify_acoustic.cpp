// verify_acoustic: the check of an Acoustic Mode scenario's saved copy, read back through the game's own loader and its own chunk classes
// (Module_Acoustics_Chunk, Environment_Acoustics_Chunk), not through the code under test. Written 2026-11-17 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_acoustic.exe <the level the editor opened> <the copy it saved> [module <an object's name> <type> <reverb>] ... [level <type> <reverb>]
// CHECKS: both levels are read by the game's loader; each named module's MODACOUS holds that type and reverb (a reverb as typed, compared to 0.001);
// the level's ENVACOUS, when named, the same; every OTHER object's MODACOUS is the original's (there or not, its three words), and the level's
// ENVACOUS too when it is not named; the copy has as many objects as the original. Gives back 0 only if every check passed.

#include <math.h>
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

struct Record
{
	bool held = false;
	int env = 0;
	float reverb = 0.0f;
	int spare = 0;
};

static Record ModuleRecord(Object_Chunk *object)
{
	Record out;
	Chunk *const data = object->lookup_single_child("MODULEDT");
	Chunk_With_Children *const holder = data ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	Module_Acoustics_Chunk *const record = holder ? dynamic_cast<Module_Acoustics_Chunk *>(holder->lookup_single_child("MODACOUS")) : nullptr;

	if (record)
	{
		out.held = true;
		out.env = record->env_index;
		out.reverb = record->reverb;
		out.spare = record->spare;
	}

	return out;
}

static Record LevelRecord(File_Chunk *level)
{
	Record out;
	Chunk *const data = level->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = data ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	Environment_Acoustics_Chunk *const record = holder ? dynamic_cast<Environment_Acoustics_Chunk *>(holder->lookup_single_child("ENVACOUS")) : nullptr;

	if (record)
	{
		out.held = true;
		out.env = record->env_index;
		out.reverb = record->reverb;
		out.spare = record->spare;
	}

	return out;
}

static bool Same(Record const &a, Record const &b)
{
	return a.held == b.held && (!a.held || (a.env == b.env && memcmp(&a.reverb, &b.reverb, 4) == 0 && a.spare == b.spare));
}

static std::string Said(Record const &r)
{
	char words[96];

	if (!r.held)
	{
		return "none";
	}

	snprintf(words, sizeof(words), "type %d, reverb %.3f, spare %d", r.env, (double)r.reverb, r.spare);
	return words;
}

// Every object's record in the level's order, by name (the first object of a name; the scenario names modules by their names)
static void Records(File_Chunk *level, std::vector<std::string> *names, std::vector<Record> *records)
{
	List<Object_Chunk *> objects;

	level->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		names->push_back(std::string(each()->object_data.o_name, strnlen(each()->object_data.o_name, sizeof(each()->object_data.o_name))));
		records->push_back(ModuleRecord(each()));
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("  FAILED  usage: verify_acoustic <original> <copy> [module <name> <type> <reverb>] ... [level <type> <reverb>]\n");
		return 1;
	}

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level(), std::string("the game's loader reads the original: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level(), std::string("the game's loader reads the copy: ") + argv[2]);

	if (!original.Level() || !copy.Level())
	{
		printf("FAILED verify_acoustic (%d checks failed)\n", checksFailed);
		return 1;
	}

	std::vector<std::string> namesBefore, namesAfter;
	std::vector<Record> before, after;

	Records(original.Level(), &namesBefore, &before);
	Records(copy.Level(), &namesAfter, &after);
	Check(namesBefore == namesAfter, "the copy has the original's objects, in its order (" + std::to_string(namesAfter.size()) + ")");

	if (namesBefore != namesAfter)
	{
		printf("FAILED verify_acoustic (%d checks failed)\n", checksFailed);
		return 1;
	}

	std::vector<bool> named(after.size(), false);
	bool levelNamed = false;

	for (int a = 3; a < argc; )
	{
		std::string const word = argv[a];

		if (word == "module" && a + 3 < argc)
		{
			std::string const name = argv[a + 1];
			int const type = atoi(argv[a + 2]);
			float const reverb = (float)atof(argv[a + 3]);
			size_t at = 0;

			while (at < namesAfter.size() && namesAfter[at] != name)
			{
				at++;
			}

			if (at == namesAfter.size())
			{
				Check(false, "the copy holds an object named \"" + name + "\"");
			}
			else
			{
				named[at] = true;
				Check(after[at].held && after[at].env == type && fabs(after[at].reverb - reverb) < 0.001f,
					"in the copy \"" + name + "\" holds type " + argv[a + 2] + ", reverb " + argv[a + 3] + " (it holds " + Said(after[at]) + "; the original " + Said(before[at]) + ")");
			}

			a += 4;
		}
		else if (word == "level" && a + 2 < argc)
		{
			Record const r = LevelRecord(copy.Level());

			levelNamed = true;
			Check(r.held && r.env == atoi(argv[a + 1]) && fabs(r.reverb - (float)atof(argv[a + 2])) < 0.001f,
				std::string("in the copy the level holds type ") + argv[a + 1] + ", reverb " + argv[a + 2] + " (it holds " + Said(r) + "; the original " + Said(LevelRecord(original.Level())) + ")");
			a += 3;
		}
		else
		{
			Check(false, "a word this tool doesn't take, or too few after it: " + word);
			break;
		}
	}

	size_t others = 0, differing = 0;

	for (size_t i = 0; i < after.size(); i++)
	{
		if (!named[i])
		{
			others++;

			if (!Same(before[i], after[i]))
			{
				differing++;
				printf("  (\"%s\": the original %s, the copy %s)\n", namesAfter[i].c_str(), Said(before[i]).c_str(), Said(after[i]).c_str());
			}
		}
	}

	Check(differing == 0, "every other object's MODACOUS (" + std::to_string(others) + ") is the original's");

	if (!levelNamed)
	{
		Check(Same(LevelRecord(original.Level()), LevelRecord(copy.Level())), "the level's ENVACOUS is the original's");
	}

	printf("%s verify_acoustic (%d checks failed)\n", checksFailed ? "FAILED" : "passed", checksFailed);
	return checksFailed ? 1 : 0;
}
