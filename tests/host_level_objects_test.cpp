// The test for src/host/level_objects: the object list, the headers and the strategy lookups read the level the EDITOR holds (OpenLevelFile,
// src/io/level_file.h), never the game's own copy, because the game lets go of its copy as soon as a level is loaded (InitNPCs,
// npcsetup.cpp:657-664). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng, after checkpoint 1's hidden run found the object
// list empty. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS, on level files it makes itself (no file the game comes with is used, and no game is started):
// - With no level held, the lookups find nothing, and say so: the list is empty and false, a header isn't found, and there's no level.
// - LevelObjects_Hold opens a level file in an OpenLevelFile by the path the game side gives (LevelView_FilePath), and the lookups then
//   read it: every object, in the file's own order, each with its name and the 8-byte ID the game works out from the level's name and
//   its own (chunk_object.cpp:368-382, worked out again here with the game's own MD5); a header by its ID, the first of two objects
//   that share one; nothing for an ID the level doesn't have.
// - The chunks it reads are the chunks of the bytes held: the file's size and MD5 sum, which the game's loader worked out from what it
//   read, are those of the file the test wrote.
// - A second level held in the same OpenLevelFile replaces the first: the lookups read the new one, and none of the old one is found.
// - A level the loader refuses, a file that isn't there, and a path the game side doesn't have each give back why, and leave no level held
//   at all, not the one held before.
// - LevelObjects_Use with nothing makes the lookups find nothing, though the OpenLevelFile still holds its level; and an OpenLevelFile
//   can be deleted after that, with nothing left pointing at it.
//
// WHAT IT CAN'T SHOW: that the game really lets go of its copy (that's the game's own code, npcsetup.cpp, and the editor's -picture run
// and the stock proof show the list full in the real editor), and that a file swapped in the middle of an open is refused (that's
// OpenLevelFile's own check, which tests/level_file_test.cpp on Hicks's branch holds to).
//
// HOW IT RUNS: built with tests/host_level_objects_test.args, which has the game's real chunk library and level_file.cpp in it. The
// program gives back a non-zero result if any check failed. tests/takeouts/host_level_objects_test.json breaks level_objects.cpp one way
// at a time, to prove a check here fails each time. This test links no env_rif (system.cpp's, which the first version of level_objects.cpp
// read), so a level_objects.cpp that read it again wouldn't link.

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
#include "md5.h"
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

// ---- Stand-ins for the parts of the game the chunk code calls. The loader opens files through avp_CreateFile: here, Windows' own.

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

// The game's memory code stops the game through these if the computer runs out of memory. Here that stops the test, as a failure.
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

// The game side's answer to "where did the game load the open level from?" (level_view.cpp's LevelView_FilePath). Empty: no level is open.
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

	memcpy(path, gamePath.c_str(), gamePath.size() + 1);
	return true;
}

// ---- Making level files as the game reads them: every chunk is an 8-byte name, then a 4-byte size, lowest byte first, that counts these
// 12 bytes too, then what it holds (avpx/3dc/win95/chunk.hpp and mishchnk.cpp). A level file is one chunk named REBINFF2.

typedef std::vector<char> Bytes;

static void PutNumber(Bytes &out, int value)
{
	char four[4];
	memcpy(four, &value, sizeof(four));
	out.insert(out.end(), four, four + 4);
}

// name must have 8 bytes to read
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

// A name, its zero, and zeros after it up to a multiple of 4, as the game writes names
static Bytes NameBytes(const char *name)
{
	Bytes out(name, name + strlen(name) + 1);

	while (out.size() % 4 != 0)
	{
		out.push_back(0);
	}

	return out;
}

// An OBJHEAD1 chunk: the flags, 56 more bytes of numbers (all 0), then the object's name (chunk_object.cpp:392-446)
static Bytes ObjectHeader(const char *name, int flags)
{
	Bytes inside;
	PutNumber(inside, flags);
	inside.resize(60, 0);
	Bytes const words = NameBytes(name);
	inside.insert(inside.end(), words.begin(), words.end());
	return MakeChunk("OBJHEAD1", inside);
}

struct ObjectSpec
{
	const char *name;
	int flags;
};

// A shape with no points and no polygons, number 0 in the file: just its header (SHPHEAD1, 68 bytes of numbers: the flags, 16 bytes for the
// user who locked it, the shape's number in the file, its counts of points and polygons, its radius and box, its version, and the count of
// the object names it has, chunk_shape.cpp:1497-1560; all 0 but the number). The game's loader refuses a level with an object that names
// no shape in the file (mishchnk.cpp, File_Chunk::post_input_processing), so every object of these levels names this one (its shape_id_no,
// the last of the 60 bytes of numbers in ObjectHeader, is 0).
static Bytes EmptyShape()
{
	return MakeChunk("REBSHAPE", MakeChunk("SHPHEAD1", Bytes(68, 0)));
}

// A level: its environment (REBENVDT, holding the level's name, RIFFNAME), the shape above, and its objects (RBOBJECT, each holding a header)
static Bytes LevelBytes(const char *levelName, std::vector<ObjectSpec> const &objects)
{
	Bytes inside = Join({ MakeChunk("REBENVDT", MakeChunk("RIFFNAME", NameBytes(levelName))), EmptyShape() });

	for (ObjectSpec const &object : objects)
	{
		inside = Join({ inside, MakeChunk("RBOBJECT", ObjectHeader(object.name, object.flags)) });
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

// An object's ID as the game works it out: the level's name and the object's own, joined and run through MD5, the first 7 bytes of the
// sum and then a 0 (Object_Chunk::CalculateID, chunk_object.cpp:368-382)
static void IdOf(const char *levelName, const char *objectName, uint8_t *id)
{
	std::string const joined = std::string(levelName) + objectName;
	char sum[16];

	md5_buffer(joined.c_str(), (unsigned int)joined.size(), sum);
	memcpy(id, sum, 8);
	id[7] = 0;
}

static bool IdIsAllZero(uint8_t const *id)
{
	for (int i = 0; i < 8; i++)
	{
		if (id[i] != 0)
		{
			return false;
		}
	}

	return true;
}

// ---- The levels of the test

static std::string folder;       // where the test's files go: a folder of its own, made inside the folder the test runs in
static std::string pathA;
static std::string pathB;
static std::string pathBad;
static Bytes bytesA;

static const char *const nameA = "lvlA";
static const char *const nameB = "lvlB";
static int const flagsDoor = 0x00108800;   // none of them is the game's locked (0x1) or base object (0x100) flag, which the loader acts on
static int const flagsFirstWall = 0x00000200;
static int const flagsSecondWall = 0x00000400;

// Five objects: a door, two walls with the same name (so the same ID), one with no name, and a lamp
static std::vector<ObjectSpec> const objectsA = {
	{ "DoorHole", flagsDoor }, { "Wall", flagsFirstWall }, { "", 0 }, { "Wall", flagsSecondWall }, { "Lamp", 0 } };
static std::vector<ObjectSpec> const objectsB = { { "Crate", 0x00000200 }, { "Vent", 0 } };

static const char *const namesA[] = { "DoorHole", "Wall", "", "Wall", "Lamp" };

// How an open went, in words, for a check's text
static const char *Said(LevelOpenResult result)
{
	switch (result)
	{
	case LevelOpenResult::Opened:
		return "Opened";
	case LevelOpenResult::NoPath:
		return "NoPath";
	case LevelOpenResult::CantRead:
		return "CantRead";
	case LevelOpenResult::LoaderRefused:
		return "LoaderRefused";
	case LevelOpenResult::ChangedWhileOpening:
		return "ChangedWhileOpening";
	case LevelOpenResult::DoesntPair:
		return "DoesntPair";
	case LevelOpenResult::TooManyChunks:
		return "TooManyChunks";
	}

	return "an unknown result";
}

// ---- The tests

static void TestNothingHeld()
{
	puts("1. with no level held, the lookups find nothing");

	LevelObjects_Use(nullptr);

	std::vector<host::LevelObject> rows(3);   // three rows that aren't a level's, to see them emptied
	host::ObjectHeader header;
	header.name = "kept";
	header.flags = 7;
	uint8_t const someId[8] = { 1, 2, 3, 4, 5, 6, 7, 0 };

	Check(!LevelObjects_Read(&rows) && rows.empty(), "the list is false and empty, and what was in it is gone");
	Check(!LevelObjects_FindHeader(someId, &header) && header.name == "kept" && header.flags == 7, "no header is found, and the one asked into stays as it was");
	Check(LevelObjects_Level() == nullptr, "there's no level");

	OpenLevelFile open;

	gamePath.clear();
	Check(LevelObjects_Hold(&open) == LevelOpenResult::NoPath, "with no path from the game side, holding a level gives back NoPath");
	Check(open.Level() == nullptr && LevelObjects_Level() == nullptr && !LevelObjects_Read(&rows), "and nothing is held, and the lookups still find nothing");
	Check(LevelObjects_Hold(nullptr) == LevelOpenResult::NoPath && LevelObjects_Level() == nullptr, "no OpenLevelFile at all gives back NoPath too");
}

static void TestAHeldLevel(OpenLevelFile *open)
{
	puts("2. a level held in the editor's OpenLevelFile is what the lookups read");

	gamePath = pathA;
	LevelOpenResult const held = LevelObjects_Hold(open);
	Check(held == LevelOpenResult::Opened, std::string("the level file is held (Opened); the open gave ") + Said(held));
	Check(open->Level() != nullptr && LevelObjects_Level() == open->Level(), "the lookups' level is the OpenLevelFile's");

	std::vector<host::LevelObject> rows(3);   // three rows that aren't this level's, to see them emptied
	Check(LevelObjects_Read(&rows), "the list reads");
	Check(rows.size() == 5, "the list has the level's 5 objects (it has " + std::to_string(rows.size()) + ")");

	bool namesRight = rows.size() == 5;
	bool idsRight = rows.size() == 5;

	for (size_t i = 0; i < rows.size() && i < 5; i++)
	{
		uint8_t expected[8];

		IdOf(nameA, namesA[i], expected);
		namesRight = namesRight && rows[i].name && strcmp(rows[i].name, namesA[i]) == 0;
		idsRight = idsRight && memcmp(rows[i].id, expected, 8) == 0;
	}

	Check(namesRight, "the names are the file's own, in the file's own order");
	Check(idsRight, "each ID is the game's own, from the level's name and the object's own");

	if (rows.size() == 5)
	{
		Check(!IdIsAllZero(rows[0].id) && !IdIsAllZero(rows[2].id), "no ID is 0, so the checks above mean something");
		Check(memcmp(rows[1].id, rows[3].id, 8) == 0 && memcmp(rows[0].id, rows[1].id, 8) != 0, "the two walls have one ID, and the door has another");
	}

	host::ObjectHeader header;
	uint8_t door[8], wall[8], nameless[8];

	IdOf(nameA, "DoorHole", door);
	IdOf(nameA, "Wall", wall);
	IdOf(nameA, "", nameless);

	Check(LevelObjects_FindHeader(door, &header) && header.name == "DoorHole" && header.flags == (uint32_t)flagsDoor &&
		memcmp(header.id, door, 8) == 0, "a header is found by its ID, with its name, its flags and its ID");
	Check(LevelObjects_FindHeader(wall, &header) && header.name == "Wall" && header.flags == (uint32_t)flagsFirstWall,
		"of two objects with one ID, the first in the file is the one");
	Check(LevelObjects_FindHeader(nameless, &header) && header.name.empty() && header.flags == 0, "an object with no name has a header with none");

	header.name = "kept";
	header.flags = 7;
	uint8_t const unknown[8] = { 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0 };

	Check(!LevelObjects_FindHeader(unknown, &header) && header.name == "kept" && header.flags == 7,
		"an ID the level doesn't have finds nothing, and leaves the header as it was");
	Check(!LevelObjects_FindHeader(nullptr, &header), "no ID finds nothing");
}

static void TestTheSameBytes(OpenLevelFile *open)
{
	puts("3. the chunks are those of the bytes held");

	File_Chunk *const level = open->Level();

	if (!level)
	{
		Check(false, "a level is held to read the loader's sum from");
		return;
	}

	// The sum the game's loader worked out from the very reads that made these chunks (level_file_sum.h), against the file the test wrote
	struct MD5Context context;
	unsigned char expected[16];

	MD5Init(&context);
	MD5Update(&context, (md5byte const *)bytesA.data(), (unsigned)bytesA.size());
	MD5Final(expected, &context);

	Check(level->file_sum.bytes == bytesA.size(), "the loader read as many bytes as the file the test wrote");
	Check(memcmp(level->file_sum.md5, expected, 16) == 0, "and their MD5 sum is that file's");
}

static void TestAnotherLevelReplacesIt(OpenLevelFile *open)
{
	puts("4. a second level held in the same OpenLevelFile replaces the first");

	gamePath = pathB;
	Check(LevelObjects_Hold(open) == LevelOpenResult::Opened, "the second level file is held");

	std::vector<host::LevelObject> rows;
	bool const read = LevelObjects_Read(&rows);
	Check(read && rows.size() == 2, "the list has the second level's 2 objects (it has " + std::to_string(rows.size()) + ")");
	Check(rows.size() == 2 && strcmp(rows[0].name, "Crate") == 0 && strcmp(rows[1].name, "Vent") == 0, "and they are its own");

	host::ObjectHeader header;
	uint8_t door[8], crate[8];

	IdOf(nameA, "DoorHole", door);
	IdOf(nameB, "Crate", crate);

	Check(!LevelObjects_FindHeader(door, &header), "none of the first level's objects is found any more");
	Check(LevelObjects_FindHeader(crate, &header) && header.name == "Crate", "the second level's are");
	Check(LevelObjects_Level() == open->Level() && LevelObjects_Level() != nullptr, "the lookups' level is the OpenLevelFile's, as it is now");

	// A level opened in the OpenLevelFile later, by itself and not through LevelObjects_Hold (as the edit path will), is read too: the lookups
	// ask the OpenLevelFile at each call, and don't keep what it held when they were given it
	bool const again = open->Open(pathA.c_str()) == LevelOpenResult::Opened && LevelObjects_Read(&rows);
	Check(again && rows.size() == 5 && LevelObjects_Level() == open->Level(), "a level opened in it later, not through LevelObjects_Hold, is the one read");
}

static void TestARefusedOpen(OpenLevelFile *open)
{
	puts("5. a level that can't be held leaves no level held, not the one held before");

	std::vector<host::LevelObject> rows;

	gamePath = pathB;
	Check(LevelObjects_Hold(open) == LevelOpenResult::Opened && LevelObjects_Read(&rows) && rows.size() == 2, "a level is held first");

	gamePath = pathBad;
	LevelOpenResult const refused = LevelObjects_Hold(open);

	Check(refused != LevelOpenResult::Opened, "a file that isn't a level isn't held");
	Check(open->Level() == nullptr && LevelObjects_Level() == nullptr, "no level is held at all after it");
	Check(!LevelObjects_Read(&rows) && rows.empty(), "the list is false and empty");

	gamePath = folder + "\\there_is_no_such_file.rif";
	Check(LevelObjects_Hold(open) != LevelOpenResult::Opened && LevelObjects_Level() == nullptr, "a file that isn't there isn't held");

	gamePath = pathA;
	Check(LevelObjects_Hold(open) == LevelOpenResult::Opened && LevelObjects_Read(&rows) && rows.size() == 5, "and a level can be held again after a refusal");
}

static void TestLetGo()
{
	puts("6. the lookups can be let go of, and the OpenLevelFile deleted after that");

	OpenLevelFile *const open = new OpenLevelFile;

	gamePath = pathA;
	Check(LevelObjects_Hold(open) == LevelOpenResult::Opened, "a level is held");

	LevelObjects_Use(nullptr);

	std::vector<host::LevelObject> rows;
	host::ObjectHeader header;
	uint8_t door[8];

	IdOf(nameA, "DoorHole", door);

	Check(open->Level() != nullptr, "the OpenLevelFile still holds its level");
	Check(!LevelObjects_Read(&rows) && !LevelObjects_FindHeader(door, &header) && LevelObjects_Level() == nullptr, "and the lookups find nothing");

	LevelObjects_Use(open);
	Check(LevelObjects_Read(&rows) && rows.size() == 5, "given it again, they read it again, without opening anything");

	Check(LevelObjects_Hold(nullptr) == LevelOpenResult::NoPath && LevelObjects_Level() == nullptr && !LevelObjects_Read(&rows),
		"holding no OpenLevelFile at all hands the lookups none, whatever they read before");

	LevelObjects_Use(nullptr);
	delete open;
	Check(!LevelObjects_Read(&rows) && LevelObjects_Level() == nullptr, "after it's deleted, the lookups still find nothing, and nothing was read from it");
}

int main()
{
	folder = "level_objects_test_" + std::to_string(GetCurrentProcessId());
	pathA = folder + "\\a.rif";
	pathB = folder + "\\b.rif";
	pathBad = folder + "\\bad.rif";

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		puts("  FAILED  the test's folder can't be made");
		return 1;
	}

	bytesA = LevelBytes(nameA, objectsA);

	bool const written = WriteTheFile(pathA, bytesA) && WriteTheFile(pathB, LevelBytes(nameB, objectsB)) &&
		WriteTheFile(pathBad, Bytes{ 'n', 'o', 't', ' ', 'a', ' ', 'l', 'e', 'v', 'e', 'l', ' ', 'f', 'i', 'l', 'e' });

	Check(written, "the test's level files are written");

	if (written)
	{
		TestNothingHeld();

		{
			OpenLevelFile open;

			TestAHeldLevel(&open);
			TestTheSameBytes(&open);
			TestAnotherLevelReplacesIt(&open);
			TestARefusedOpen(&open);
			LevelObjects_Use(nullptr);
		}

		TestLetGo();
	}

	DeleteFileA(pathA.c_str());
	DeleteFileA(pathB.c_str());
	DeleteFileA(pathBad.c_str());
	RemoveDirectoryA(folder.c_str());

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
