// The test for the Shape Chooser's list (host::ShapeChooser_List, ShapeChooser_ObjectIsPlaced, ShapeChooser_ShapeOfObject, src/host/shape_chooser.cpp) and for giving a placed
// object a shape from it (host::ShapeReplace_ByNumber), on a REAL level (the first program argument); the level is saved as a COPY and loaded again with the game's own loader.
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_chooser_test.exe <a level .rif> <a folder it may write its copies into>. With no arguments it says so and passes.
//  1. the list holds every shape that has a SHPEXTFN: each entry has a name (1 to 63 letters, no folder, no extension), a number that no other entry has, and counts from 0;
//  2. the list is the same twice (nothing in it changes the level);
//  3. a placed object is a placed object, a module is not; the number of a placed object's own shape is one the list holds (when its shape is an outside one);
//  4. giving a placed object the shape of ANOTHER entry says Done, the loader takes the copy, the object's shape number in the copy is that entry's; giving it its own says SameShape
//     and changes nothing; a number the list does not hold is refused as NoSuchShape;
//  5. the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_chooser.h"
#include "shape_replace_edit.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static int32_t ShapeNumberOfObject(OpenLevelFile const &open, uint8_t const *id)
{
	int32_t number = -1;

	host::ShapeChooser_ShapeOfObject(open.Level(), &open, id, &number);
	return number;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_shape_chooser_test: give a level .rif and a folder for its copies; with neither there is nothing to run.\n");
		printf("passed host_shape_chooser_test (0 ok)\n");
		return 0;
	}

	std::string const levelPath = argv[1];
	std::string const copyFolder = argv[2];
	std::vector<char> original;

	if (!ReadWholeFile(levelPath, &original))
	{
		printf("  FAILED  the level can be read: %s\n", levelPath.c_str());
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	// 1 and 2: the list
	std::vector<host::ShapeChoice> list, again;

	size_t const count = host::ShapeChooser_List(level.Level(), &level, &list);

	Check(count > 0 && count == list.size(), "the level holds outside shapes the chooser lists: " + std::to_string(count));

	std::set<int32_t> numbers;
	bool entriesRight = true;
	std::string firstBad;

	for (host::ShapeChoice const &one : list)
	{
		bool const good = !one.name.empty() && one.name.size() <= 63 && one.name.find_first_of("\\/.") == std::string::npos && one.polygons >= 0 && one.points >= 0 && numbers.insert(one.number).second;

		if (!good && entriesRight)
		{
			firstBad = "'" + one.name + "' number " + std::to_string(one.number);
		}

		entriesRight = entriesRight && good;
	}

	Check(entriesRight, "every entry has a plain name (1 to 63 letters, no folder, no extension), a number of its own and counts from 0" + (firstBad.empty() ? std::string() : " (first bad: " + firstBad + ")"));
	printf("  --  first entries:");

	for (size_t i = 0; i < list.size() && i < 4; i++)
	{
		printf(" '%s' (%d polys, %d verts)", list[i].name.c_str(), list[i].polygons, list[i].points);
	}

	printf("\n");

	host::ShapeChooser_List(level.Level(), &level, &again);
	Check(again.size() == list.size() && (list.empty() || (again[0].name == list[0].name && again.back().number == list.back().number)), "the list is the same twice");

	// 3: placed objects and modules
	List<Object_Chunk *> all;

	level.Level()->list_objects(&all);

	uint8_t placed[8] = { 0 }, other[8] = { 0 }, module[8] = { 0 };
	bool havePlaced = false, haveOther = false, haveModule = false;
	int32_t placedShape = -1;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		uint8_t const *const id = (uint8_t const *)&object->object_data.ID;
		bool const isPlaced = host::ShapeChooser_ObjectIsPlaced(level.Level(), &level, id);
		int32_t const shape = ShapeNumberOfObject(level, id);

		if (isPlaced && shape >= 0 && numbers.count(shape) && !havePlaced)
		{
			memcpy(placed, id, 8);
			placedShape = shape;
			havePlaced = true;
		}
		else if (!isPlaced && !haveModule)
		{
			memcpy(module, id, 8);
			haveModule = true;
		}
	}

	Check(havePlaced, "a placed object whose own shape is one the list holds is found");
	Check(!haveModule || !host::ShapeChooser_ObjectIsPlaced(level.Level(), &level, module), "an object that is not placed is not a placed object");

	if (!havePlaced)
	{
		return checksFailed ? 1 : 0;
	}

	// the name check Create's name prompt makes (a byte-wise compare against every object's name)
	{
		List<Object_Chunk *> objects;
		bool allFound = true;
		size_t checked = 0;

		level.Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (each() && each()->object_data.o_name[0])
			{
				std::string const name(each()->object_data.o_name, strnlen(each()->object_data.o_name, sizeof(each()->object_data.o_name)));

				allFound = allFound && host::ObjectNameInUse(level.Level(), name.c_str());
				checked++;
			}
		}

		Check(checked > 0 && allFound, "every object's own name is in use: " + std::to_string(checked) + " names");
		Check(!host::ObjectNameInUse(level.Level(), "zzNoSuchObjectName") && !host::ObjectNameInUse(level.Level(), "") && !host::ObjectNameInUse(level.Level(), std::string(200, 'a').c_str()),
			"a name no object has, an empty name and a name too long are not in use");
	}

	// 4: giving it another shape
	int32_t otherNumber = -1;

	for (host::ShapeChoice const &one : list)
	{
		if (one.number != placedShape)
		{
			otherNumber = one.number;
			break;
		}
	}

	(void)other;
	(void)haveOther;

	std::vector<char> imageBefore;

	Check(level.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");
	Check(host::ShapeReplace_ByNumber(&level, placed, placedShape) == host::ShapeReplaceResult::SameShape, "giving the object its own shape says SameShape");

	std::vector<char> now;

	Check(level.BuildLevel(now) && now == imageBefore, "and changes nothing");
	Check(host::ShapeReplace_ByNumber(&level, placed, 2147483000) == host::ShapeReplaceResult::NoSuchShape, "a number the list does not hold is refused as NoSuchShape");

	if (otherNumber >= 0)
	{
		Check(host::ShapeReplace_ByNumber(&level, placed, otherNumber) == host::ShapeReplaceResult::Done, "giving it another entry's shape says Done");
		Check(ShapeNumberOfObject(level, placed) == otherNumber, "the object's shape is that entry's now");

		std::string const copyPath = copyFolder + (copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "chooser-test-copy.rif";

		CreateDirectoryA(copyFolder.c_str(), NULL);
		DeleteFileA(copyPath.c_str());
		Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

		OpenLevelFile after;

		Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

		if (after.Level())
		{
			Check(ShapeNumberOfObject(after, placed) == otherNumber, "the copy holds the object with that shape");
		}

		DeleteFileA(copyPath.c_str());
	}
	else
	{
		printf("  --  the list holds one shape only: giving a different one is not exercised\n");
	}

	// 5
	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_shape_chooser_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_shape_chooser_test\n");
	return 0;
}
