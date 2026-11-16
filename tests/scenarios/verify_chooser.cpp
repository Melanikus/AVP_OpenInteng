// verify_chooser.cpp: the check program of acceptance-shape-chooser.txt (Vasquez, 2026-11-16): the level saved by Save a copy after Create > With shape > Choose is read back through the game's own loader. The copy has
// exactly one object more than the original, that object is named as the scenario typed, and its shape is the one the chooser showed (the outside file's name, as the chooser lists it).
// USAGE: verify_chooser <original> <copy> <object name> <shape name>
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "chunk_object.hpp"
#include "level_file.h"
#include "shape_chooser.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

using namespace host;

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

static size_t CountObjects(OpenLevelFile *file)
{
	List<Object_Chunk *> found;

	file->Level()->list_objects(&found);
	return (size_t)found.size();
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 5)
	{
		printf("usage: verify_chooser <original> <copy> <object name> <shape name>\n");
		return 2;
	}

	OpenLevelFile original, copy;

	if (original.Open(argv[1]) != LevelOpenResult::Opened)
	{
		Check(false, std::string("the original opens: ") + argv[1]);
		return 1;
	}

	Check(copy.Open(argv[2]) == LevelOpenResult::Opened, std::string("the saved copy is taken by the game's own loader: ") + argv[2]);

	if (failed)
	{
		return 1;
	}

	if (strcmp(argv[3], "replace") == 0)
	{
		// Replace shape: the same objects, and exactly ONE has another shape: the one the chooser showed (argv[4])
		std::vector<ShapeChoice> list;
		int32_t wanted = -1;

		ShapeChooser_List(copy.Level(), &copy, &list);

		for (ShapeChoice const &one : list)
		{
			if (one.name == argv[4])
			{
				wanted = one.number;
			}
		}

		Check(wanted >= 0, std::string("the shape ") + argv[4] + " is one the chooser lists");
		Check(CountObjects(&copy) == CountObjects(&original), "the copy has the same number of objects");

		List<Object_Chunk *> found;
		size_t changed = 0, changedToWanted = 0;

		copy.Level()->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			uint8_t id[8];
			int32_t was = -1, now = -1;

			memcpy(id, &each()->object_data.ID, 8);

			bool const had = ShapeChooser_ShapeOfObject(original.Level(), &original, id, &was);
			bool const has = ShapeChooser_ShapeOfObject(copy.Level(), &copy, id, &now);

			if (had != has || (had && was != now))
			{
				changed++;
				changedToWanted += has && now == wanted ? 1 : 0;
			}
		}

		Check(changed == 1 && changedToWanted == 1, "exactly one object has another shape and it is the chosen one (" + std::to_string((unsigned long long)changed) + " changed, " + std::to_string((unsigned long long)changedToWanted) + " to " + argv[4] + ")");
		printf(failed ? "FAILED\n" : "all passed\n");
		return failed ? 1 : 0;
	}

	Check(CountObjects(&copy) == CountObjects(&original) + 1, "the copy has exactly one object more (" + std::to_string((unsigned long long)CountObjects(&original)) + " -> " + std::to_string((unsigned long long)CountObjects(&copy)) + ")");

	List<Object_Chunk *> found;
	uint8_t id[8] = {};
	bool seen = false;

	copy.Level()->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && strncmp(object->object_data.o_name, argv[3], sizeof(object->object_data.o_name)) == 0)
		{
			memcpy(id, &object->object_data.ID, 8);
			seen = true;
		}
	}

	Check(seen, std::string("an object named ") + argv[3] + " is in the copy");

	std::vector<ShapeChoice> list;
	int32_t wanted = -1;

	ShapeChooser_List(copy.Level(), &copy, &list);

	for (ShapeChoice const &one : list)
	{
		if (one.name == argv[4])
		{
			wanted = one.number;
		}
	}

	Check(wanted >= 0, std::string("the shape ") + argv[4] + " is one the chooser lists");

	int32_t has = -1;

	bool const hasOne = seen && ShapeChooser_ShapeOfObject(copy.Level(), &copy, id, &has);

	Check(hasOne && has == wanted, "the new object's shape is that shape (number " + std::to_string((long long)wanted) + ", the object has " + std::to_string((long long)has) + ")");
	printf(failed ? "FAILED\n" : "all passed\n");
	return failed ? 1 : 0;
}
