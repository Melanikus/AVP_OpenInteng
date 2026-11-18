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
#include "object_number.h"
#include "quick_create.h"
#include "shape_import.h"

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
	int32_t number;         // V-27: the lowest number no object had when it was made
};

// What the file holds for the object with this ID: its header's place and name, read from the bytes the editor holds (the place at 20, the name at 60)
static bool HeaderOf(OpenLevelFile const &open, uint8_t const *id, int32_t place[3], std::string *name, std::string *lock = nullptr, int32_t *number = nullptr)
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

		if (lock)
		{
			*lock = std::string((char const *)&bytes[4], 16);   // the lock user, 4..19
		}

		if (number)
		{
			memcpy(number, &bytes[48], sizeof(int32_t));   // the number (index_num), 48
		}

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

// V-27: the lowest number no object of the level has, worked out here on its own from every header's number (at 48)
static int32_t LowestFreeOf(OpenLevelFile const &open)
{
	List<Object_Chunk *> all;
	std::vector<bool> taken;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		Bytes bytes;
		Object_Header_Chunk *const header = object ? object->get_header() : nullptr;

		if (!header || !open.PayloadOf(header, &bytes) || bytes.size() < 52)
		{
			continue;
		}

		int32_t number = 0;

		memcpy(&number, &bytes[48], sizeof(number));

		if (number >= 0 && number < 1000000)
		{
			if ((size_t)number >= taken.size())
			{
				taken.resize((size_t)number + 1, false);
			}

			taken[(size_t)number] = true;
		}
	}

	int32_t free = 0;

	while ((size_t)free < taken.size() && taken[(size_t)free])
	{
		free++;
	}

	return free;
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

	// OP-141 (a), open-path-2 of the 11-17 review: a shape's name is the file <folder>\<name>.rif, and Windows opens a DEVICE for "NUL.rif" or "COM1.rif" in any folder. ShapeImport_Read
	// refuses such a name (BadName) before anything is opened: NUL would open and read empty, COM1 would open a serial port. The copy folder is a plain folder with no such files.
	{
		char const *const devices[] = { "NUL", "nul", "CON", "PRN", "AUX", "COM1", "com9", "LPT1", "LPT9" };
		bool allRefused = true;
		std::string notRefused;

		for (char const *const device : devices)
		{
			std::vector<char> bytes;

			if (host::ShapeImport_Read(copyFolder.c_str(), device, &bytes, nullptr) != host::ShapeImportResult::BadName || !bytes.empty())
			{
				allRefused = false;
				notRefused += std::string(" ") + device;
			}
		}

		std::vector<char> none;

		Check(allRefused, "a device's name (NUL, CON, PRN, AUX, COM1-9, LPT1-9, any capitals) is refused before a file is opened" + (notRefused.empty() ? std::string() : ": not" + notRefused));
		Check(host::ShapeImport_Read(copyFolder.c_str(), "COM10", &none, nullptr) == host::ShapeImportResult::NotFound && host::ShapeImport_Read(copyFolder.c_str(), "NULL", &none, nullptr) ==
			host::ShapeImportResult::NotFound, "names that only start like a device (COM10, NULL) are plain names: looked for, and not there");
	}

	// V-27: a new object's number is inteng's FUN_004bebc0's: the first empty slot of the file's object table (every object at its own number), or its size when it has no gap
	Check(host::ObjectNumber_LowestFree({}) == 0 && host::ObjectNumber_LowestFree({ 0, 1, 2 }) == 3, "no object: 0; numbers 0, 1, 2 with no gap: 3 (the table's size)");
	Check(host::ObjectNumber_LowestFree({ 0, 2, 3 }) == 1 && host::ObjectNumber_LowestFree({ 5 }) == 0,
		"a gap is filled first: 0, 2, 3 gives 1 and a lone 5 gives 0 (the first empty slot, not the greatest + 1)");
	Check(host::ObjectNumber_LowestFree({ 3, 1, 0, 1, -1 }) == 2, "out of order, a repeat and a negative (which holds no slot): 3, 1, 0, 1, -1 gives 2");
	Check(host::ObjectNumber_LowestFree({ 0, 0x7fffffff }) == 1, "a hostile greatest number (2^31-1) is no limit: 0, 2^31-1 gives 1");

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
		entry.number = LowestFreeOf(open);

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
	bool locksPlayer = true;
	bool numbersLowest = true;
	std::string firstBad;
	std::string firstLock;
	std::string firstNumber;
	std::string const player("Player\0\0\0\0\0\0\0\0\0\0", 16);

	for (Made const &entry : made)
	{
		int32_t place[3] = { 0, 0, 0 };
		std::string name;
		std::string lock;
		int32_t number = -1;

		if (!HeaderOf(after, entry.id, place, &name, &lock, &number))
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

		// V-27: the save's lock call (FUN_004bd890) sets the lock user DAT_005ce6f4, the static "Player", padded with zeros (code_004d1460's strncpy of 16)
		if (lock != player && firstLock.empty())
		{
			locksPlayer = false;
			firstLock = "command 0x" + std::to_string(entry.command) + ": the lock user's first letter is " + std::to_string((int)(unsigned char)lock[0]);
		}

		if (number != entry.number && firstNumber.empty())
		{
			numbersLowest = false;
			firstNumber = "command 0x" + std::to_string(entry.command) + ": " + std::to_string(number) + ", not " + std::to_string(entry.number);
		}
	}

	Check(allThere && firstBad.empty(), "every object made is in the copy under its ID, at the place it was given" + (firstBad.empty() ? std::string() : " (" + firstBad + ")"));
	Check(namesRight, "every object made has the name it was given: the shape's name and a number");
	Check(locksPlayer, "every object made has inteng's lock user, \"Player\" and ten zeros" + (firstLock.empty() ? std::string() : " (" + firstLock + ")"));
	Check(numbersLowest, "every object made has the lowest number no object had when it was made" + (firstNumber.empty() ? std::string() : " (" + firstNumber + ")"));

	DeleteFileA(copyPath.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
