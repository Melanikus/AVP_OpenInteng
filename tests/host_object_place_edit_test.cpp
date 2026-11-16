// The test for src/host/object_place_edit: finding an object by what was typed, moving it, and naming it, through the edit store (OpenLevelFile), on
// levels it writes itself. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (M-1, object placement). AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS: Find by a part of a name (capitals or not), by a 16-digit ID, and nothing for no match, nothing typed, or too much typed; the
// first of two matches is given, with how many there were. Move changes the place in the header's bytes only (the chunk keeps its size), is
// held to the game's FarthestPlace, 2^24, either way, whatever number is typed, and changes nothing when it refuses. Name gives the new name
// and the ID that comes from it, refuses a name already used (in capitals or not), one the game's file-name rule refuses, empty or one that
// doesn't fit the room the old name leaves, keeps the chunk's size with 0 after the name, and changes nothing when it refuses.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <initializer_list>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_place_edit.h"

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

// ---- Making level files as the game reads them (see host_level_objects_test.cpp)

typedef std::vector<char> Bytes;

static void PutNumber(Bytes &out, int value)
{
	char four[4];
	memcpy(four, &value, sizeof(four));
	out.insert(out.end(), four, four + 4);
}

static Bytes MakeChunk(const char *name, Bytes const &inside)
{
	Bytes out(name, name + 8);
	PutNumber(out, (int)(12 + inside.size()));
	out.insert(out.end(), inside.begin(), inside.end());
	return out;
}

static Bytes Join(std::initializer_list<Bytes> parts)
{
	Bytes out;

	for (Bytes const &part : parts)
	{
		out.insert(out.end(), part.begin(), part.end());
	}

	return out;
}

static Bytes NameBytes(const char *name)
{
	Bytes out(name, name + strlen(name) + 1);

	while (out.size() % 4 != 0)
	{
		out.push_back(0);
	}

	return out;
}

// An OBJHEAD1 chunk: the flags, the lock user, the place (bytes 20 to 31), 28 more bytes of numbers (all 0), then the name
static Bytes ObjectHeader(const char *name, int x, int y, int z)
{
	Bytes inside;
	PutNumber(inside, 0);
	inside.resize(20, 0);
	PutNumber(inside, x);
	PutNumber(inside, y);
	PutNumber(inside, z);
	inside.resize(60, 0);
	Bytes const words = NameBytes(name);
	inside.insert(inside.end(), words.begin(), words.end());
	return MakeChunk("OBJHEAD1", inside);
}

struct ObjectSpec
{
	const char *name;
	int x, y, z;
};

static Bytes LevelBytes(const char *levelName, std::vector<ObjectSpec> const &objects)
{
	Bytes inside = Join({ MakeChunk("REBENVDT", MakeChunk("RIFFNAME", NameBytes(levelName))), MakeChunk("REBSHAPE", MakeChunk("SHPHEAD1", Bytes(68, 0))) });

	for (ObjectSpec const &object : objects)
	{
		inside = Join({ inside, MakeChunk("RBOBJECT", ObjectHeader(object.name, object.x, object.y, object.z)) });
	}

	return MakeChunk("REBINFF2", inside);
}

static bool WriteTheFile(std::string const &path, Bytes const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD written = 0;
	bool const whole = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, 0) && written == bytes.size();

	CloseHandle(file);
	return whole;
}

static std::string HexOf(const uint8_t *id)
{
	char hex[17];

	for (int i = 0; i < 8; i++)
	{
		snprintf(hex + 2 * i, 3, "%02X", (unsigned)id[i]);
	}

	return std::string(hex, 16);
}

// ---- The tests

using host::FoundObject;
using host::ObjectEditAnswer;

static void TestFind(OpenLevelFile *open)
{
	puts("1. Find");

	FoundObject found;
	File_Chunk *const level = open->Level();

	Check(host::ObjectEdit_Find(level, open, "door", &found) && found.name == "DoorHole" && found.matches == 1 && found.x == 100 && found.y == -200 && found.z == 300,
		"a part of a name, in small letters, finds the object, its name and its place");
	Check(host::ObjectEdit_Find(level, open, "WALL", &found) && found.name == "Wall" && found.matches == 2, "the first of two matches is given, with how many there were");

	FoundObject byId;
	std::string const hex = HexOf(found.id);

	Check(host::ObjectEdit_Find(level, open, hex.c_str(), &byId) && memcmp(byId.id, found.id, 8) == 0, "16 hexadecimal digits find an object by its ID");

	FoundObject untouched;
	untouched.name = "kept";
	Check(!host::ObjectEdit_Find(level, open, "nothing like this", &untouched) && untouched.name == "kept", "no match finds nothing and leaves the answer as it was");
	Check(!host::ObjectEdit_Find(level, open, "", &untouched), "nothing typed finds nothing");
	Check(!host::ObjectEdit_Find(level, open, std::string(64, 'a').c_str(), &untouched), "more than 63 letters find nothing");
	Check(!host::ObjectEdit_Find(nullptr, open, "door", &untouched) && !host::ObjectEdit_Find(level, open, nullptr, &untouched) && !host::ObjectEdit_Find(level, open, "door", nullptr),
		"no level, nothing typed at all, or no place for the answer find nothing");
}

static void TestMove(OpenLevelFile *open)
{
	puts("2. Move relative");

	File_Chunk *const level = open->Level();
	FoundObject door;
	std::string why;

	host::ObjectEdit_Find(level, open, "door", &door);

	Check(host::ObjectEdit_Move(open, level, door.id, 10, -20, 30, &why) == ObjectEditAnswer::Done, "a move by 10, -20, 30 is done");

	FoundObject now;

	Check(host::ObjectEdit_Find(level, open, "door", &now) && now.x == 110 && now.y == -220 && now.z == 330, "the object is at its new place");
	Check(host::ObjectEdit_Move(open, level, door.id, (1 << 24) - 110, 0, 0, &why) == ObjectEditAnswer::Done &&
		host::ObjectEdit_Find(level, open, "door", &now) && now.x == (1 << 24), "a place exactly at the game's farthest, 16777216, is allowed");
	Check(host::ObjectEdit_Move(open, level, door.id, 1, 0, 0, &why) == ObjectEditAnswer::Refused && !why.empty() && host::ObjectEdit_Find(level, open, "door", &now) && now.x == (1 << 24),
		"one more is refused, with a reason, and the place is as it was");
	Check(host::ObjectEdit_Move(open, level, door.id, 0, 0, -(1LL << 25), &why) == ObjectEditAnswer::Refused, "a move that puts a side below -16777216 is refused");
	Check(host::ObjectEdit_Move(open, level, door.id, 1LL << 62, 0, 0, &why) == ObjectEditAnswer::Refused &&
		host::ObjectEdit_Move(open, level, door.id, 0, -(1LL << 62), 0, &why) == ObjectEditAnswer::Refused &&
		host::ObjectEdit_Move(open, level, door.id, 0, 0, 0x7FFFFFFFFFFFFFFFLL, &why) == ObjectEditAnswer::Refused, "a number of 64 bits can't wrap round into a place");

	uint8_t const unknown[8] = { 1, 2, 3, 4, 5, 6, 7, 0 };

	Check(host::ObjectEdit_Move(open, level, unknown, 1, 1, 1, &why) == ObjectEditAnswer::NoObject, "an ID no object has is NoObject");
	Check(host::ObjectEdit_Move(nullptr, level, door.id, 1, 1, 1, &why) == ObjectEditAnswer::BadCall && host::ObjectEdit_Move(open, level, nullptr, 1, 1, 1, &why) == ObjectEditAnswer::BadCall,
		"no store, or no ID, is a bad call");
}

static void TestRename(OpenLevelFile *open)
{
	puts("3. Name object");

	File_Chunk *const level = open->Level();
	FoundObject wall;
	FoundObject lamp;
	std::string why;
	uint8_t newId[8];

	host::ObjectEdit_Find(level, open, "wall", &wall);
	host::ObjectEdit_Find(level, open, "lamp", &lamp);

	Check(host::ObjectEdit_Rename(open, level, lamp.id, "wall", newId, &why) == ObjectEditAnswer::Refused && why == "The name of the object has already been used.",
		"a name another object has is refused, in capitals or not, with the game's own words");
	Check(host::ObjectEdit_Rename(open, level, lamp.id, "", newId, &why) == ObjectEditAnswer::Refused, "no name is refused");
	Check(host::ObjectEdit_Rename(open, level, lamp.id, "bad/name", newId, &why) == ObjectEditAnswer::Refused, "a name the game's file-name rule refuses is refused");
	Check(host::ObjectEdit_Rename(open, level, lamp.id, "LampLampLampLamp", newId, &why) == ObjectEditAnswer::Refused, "a name that doesn't fit the room the old one leaves is refused");
	Check(host::ObjectEdit_Rename(open, level, lamp.id, std::string(70, 'a').c_str(), newId, &why) == ObjectEditAnswer::Refused, "a name of more than 63 letters is refused");

	FoundObject same;

	Check(host::ObjectEdit_Find(level, open, "lamp", &same) && memcmp(same.id, lamp.id, 8) == 0, "after the refusals the object is as it was");

	Check(host::ObjectEdit_Rename(open, level, lamp.id, "Torch", newId, &why) == ObjectEditAnswer::Done, "a free name that fits is done");

	FoundObject torch;

	Check(host::ObjectEdit_Find(level, open, "torch", &torch) && memcmp(torch.id, newId, 8) == 0 && memcmp(newId, lamp.id, 8) != 0, "it's found by its new name, with a new ID");
	Check(!host::ObjectEdit_Find(level, open, "lamp", &same), "and no longer by the old one");

	List<Object_Chunk *> all;
	level->list_objects(&all);
	Object_Header_Chunk *head = nullptr;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (memcmp(&each()->object_data.ID, newId, 8) == 0)
		{
			head = each()->get_header();
		}
	}

	std::vector<unsigned char> bytes;

	Check(head && open->PayloadOf(head, &bytes) && bytes.size() == 72 - 12 + 8, "the header's bytes keep their size (60 and the 8 the old name's room was)");
	Check(bytes.size() == 68 && memcmp(&bytes[60], "Torch\0\0\0", 8) == 0, "with the new name and 0s after it");
	Check(host::ObjectEdit_Rename(open, level, torch.id, "Lamp", newId, &why) == ObjectEditAnswer::Done, "a name can be given back");
}

int main()
{
	std::string const folder = "object_edit_test_" + std::to_string(GetCurrentProcessId());
	std::string const path = folder + "\\a.rif";

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		puts("  FAILED  the test's folder can't be made");
		return 1;
	}

	std::vector<ObjectSpec> const objects = { { "DoorHole", 100, -200, 300 }, { "Wall", 0, 0, 0 }, { "Wall", 5, 5, 5 }, { "Lamp", 1, 2, 3 } };
	bool const written = WriteTheFile(path, LevelBytes("lvlE", objects));

	Check(written, "the test's level file is written");

	if (written)
	{
		OpenLevelFile open;
		LevelOpenResult const result = open.Open(path.c_str());

		Check(result == LevelOpenResult::Opened, "it is held in the editor's own open");

		if (result == LevelOpenResult::Opened)
		{
			TestFind(&open);
			TestMove(&open);
			TestRename(&open);
		}
	}

	DeleteFileA(path.c_str());
	RemoveDirectoryA(folder.c_str());
	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
