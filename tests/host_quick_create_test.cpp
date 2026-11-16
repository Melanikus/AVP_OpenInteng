// The test for Object Placement's Quick-create items (src/host/quick_create.cpp and object_make.cpp, Frost's P3): on a REAL level (the first program
// argument) it makes every Quick-create item's object through the edit store, saves a COPY (never the opened file), loads the copy with the game's own
// loader, and checks what is there. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng, because the maker had only the tests that make their own
// small levels. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_quick_create_test.exe <a level .rif> <a folder it may write its copy into (made if it isn't there)>. With no arguments it says so and passes,
// so the test list can hold it. The level is only read; the copy is the only file written.
//
// WHAT IT CHECKS:
//  1. each of the 22 items either makes its object (Done) or says the level holds no such shape (NoSuchShape), and nothing else; a refused item leaves
//     the level as it was (the same number of objects); at least one item is Done on the level given;
//  2. after the copy is reloaded by the game's loader: the number of objects is the old number plus the number made; every object made is there under
//     the ID it was given, with the name it was given, at the place it was given, with the shape's header naming it as one of its users;
//  3. the names are the shape's name and a number ("pulse0", "pulse1"): the first name no object of the level had;
//  4. the file that was opened is byte for byte what it was.

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
#include "object_make.h"
#include "quick_create.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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

typedef std::vector<unsigned char> Bytes;

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

static size_t CountObjects(OpenLevelFile const &open)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	size_t count = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		count += each() ? 1 : 0;
	}

	return count;
}

struct Made
{
	int32_t command;
	uint8_t id[8];
	std::string name;
	int32_t place[3];
	std::string shapeName;
};

// What the file holds for the object with this ID: its header's place and name, read from the bytes the editor holds (the place at 20, the name at 60)
static bool HeaderOf(OpenLevelFile const &open, uint8_t const *id, int32_t place[3], std::string *name)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || memcmp(&object->object_data.ID, id, 8) != 0)
		{
			continue;
		}

		Bytes bytes;
		Object_Header_Chunk *const header = object->get_header();

		if (!header || !open.PayloadOf(header, &bytes) || bytes.size() < 61)
		{
			return false;
		}

		memcpy(place, &bytes[20], 3 * sizeof(int32_t));

		size_t end = 60;

		while (end < bytes.size() && bytes[end] != 0)
		{
			end++;
		}

		*name = std::string((char const *)&bytes[60], end - 60);
		return true;
	}

	return false;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_quick_create_test <a level .rif> <a folder for its copy>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	std::string const levelPath = argv[1];
	std::string const copyFolder = argv[2];
	std::vector<char> original;

	Check(ReadWholeFile(levelPath, &original), "the level is read (it is only read, never written)");

	OpenLevelFile open;

	if (original.empty() || open.Open(levelPath.c_str()) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	size_t const objectsBefore = CountObjects(open);
	std::vector<Made> made;
	size_t noShape = 0;
	bool everyAnswerPlain = true;
	bool refusedLeavesAlone = true;

	// The 22 items' command numbers, as inteng's table has them (the Weapon, Ammo, Light and Other lists)
	int32_t const commands[] = { 0x3e9, 0x3eb, 0x3ec, 0x3ed, 0x3ee, 0x3ef, 0x3f8, 0x3f9, 0x44d, 0x455, 0x44f, 0x450, 0x451, 0x452, 0x453, 0x456, 0x460, 0x461, 0x462,
		0x4b1, 0x4b3, 0x4b4 };

	for (int32_t command : commands)
	{
		char const *const shape = host::QuickCreate_ShapeName(command);

		if (!shape)
		{
			everyAnswerPlain = false;
			continue;
		}

		size_t const before = CountObjects(open);
		int32_t place[3] = { 1000 * (int32_t)made.size() + 5000, -2000, 7000 };
		int32_t const matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
		Made entry;
		std::string name;

		memset(entry.id, 0, sizeof(entry.id));

		host::ObjectMakeResult const result = host::CreateObjectFromShape(&open, shape, place, matrix, entry.id, &name);

		if (result == host::ObjectMakeResult::Done)
		{
			entry.command = command;
			entry.name = name;
			memcpy(entry.place, place, sizeof(place));
			entry.shapeName = shape;
			made.push_back(entry);
			everyAnswerPlain = everyAnswerPlain && CountObjects(open) == before + 1;
		}
		else if (result == host::ObjectMakeResult::NoSuchShape)
		{
			noShape++;
			refusedLeavesAlone = refusedLeavesAlone && CountObjects(open) == before;
		}
		else
		{
			everyAnswerPlain = false;
			printf("  (command 0x%x, shape %s: %s)\n", (unsigned)command, shape, host::ObjectMakeWords(result));
		}
	}

	printf("  (%zu items made an object, %zu said the level holds no such shape)\n", made.size(), noShape);
	Check(everyAnswerPlain, "each item made its object (one more object each) or said the level has no such shape, and nothing else");
	Check(refusedLeavesAlone, "an item that said no such shape left the level's objects as they were");
	Check(made.size() >= 1, "at least one item was made on this level");

	if (made.empty())
	{
		return 1;
	}

	// The copy, never the opened file
	std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "quick-create-test-copy.rif";

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(open.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;
	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	// Reloaded by the game's loader
	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (!after.Level())
	{
		return 1;
	}

	Check(CountObjects(after) == objectsBefore + made.size(), "the copy has the old number of objects plus those made: " + std::to_string(objectsBefore) + " + " + std::to_string(made.size()));

	bool allThere = true;
	bool namesRight = true;
	std::string firstBad;

	for (Made const &entry : made)
	{
		int32_t place[3] = { 0, 0, 0 };
		std::string name;

		if (!HeaderOf(after, entry.id, place, &name))
		{
			allThere = false;
			firstBad = "command 0x" + std::to_string(entry.command) + ": no object with its ID";
			break;
		}

		if (memcmp(place, entry.place, sizeof(place)) != 0)
		{
			allThere = false;
			firstBad = "command " + std::to_string(entry.command) + ": its place is not the one given";
		}

		// the shape's name and a counter: "pulse0", "pulse1" ...
		bool const startsWithShape = name.size() > entry.shapeName.size() && _strnicmp(name.c_str(), entry.shapeName.c_str(), entry.shapeName.size()) == 0;
		bool digitsAfter = startsWithShape;

		for (size_t at = entry.shapeName.size(); digitsAfter && at < name.size(); at++)
		{
			digitsAfter = name[at] >= '0' && name[at] <= '9';
		}

		namesRight = namesRight && name == entry.name && digitsAfter;
	}

	Check(allThere && firstBad.empty(), "every object made is in the copy under its ID, at the place it was given" + (firstBad.empty() ? std::string() : " (" + firstBad + ")"));
	Check(namesRight, "every object made has the name it was given: the shape's name and a number");

	DeleteFileA(copyPath.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
