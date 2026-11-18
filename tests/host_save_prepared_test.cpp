// The test of inteng's SAVE on real levels (Bishop's audit 2026-11-15): a save does what inteng's write_file does (File_Chunk::write_file, mishchnk.cpp:391): prepare_for_output() on the
// WHOLE tree (every object header's version number goes up by one, the environment's and the sprites' too, the shape headers get their numbers), then the library writes the whole tree.
// A save with NOTHING changed on a stock level is therefore not the file's bytes: it is what inteng would write. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_save_prepared_test.exe <a level .rif> <a folder it may write its copies into> [<a second level> <an object in it> <another object in it>]. With no arguments it says so
// and passes. The levels are only read; the copies are the only files written.
// CHECKS: a save of the level with nothing changed (as a COPY) is exactly what the game's own library writes after prepare_for_output() (done here straight on the library, not through
// level_file.cpp); it differs from the file (the version numbers went up: at least one byte for every object header); after the save the level is built as the file just written, and
// an edit's rebuild of it is taken; the game's loader takes the copy and it holds the same objects; a second save of the copy is the library's prepare of the copy again (the numbers
// go up once more); the opened file is byte for byte what it was. With the second level: one object taken out, a save, the other taken out, a save again, all made (Frost, 2026-11-17).

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "huffman.hpp"
#include "level_file.h"
#include "object_edit.h"
#include "object_make.h"
#include "object_number.h"
#include "object_place_edit.h"
#include "shape_chooser.h"

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

static bool WriteWholeFile(std::string const &path, std::vector<char> const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD put = 0;
	bool const good = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, 0) && put == bytes.size();
	return CloseHandle(file) && good;
}

// The level inside a file: the file itself, or what the game's checked unpacker makes of a packed one (empty if it can't)
static std::vector<char> LevelIn(std::vector<char> const &file)
{
	if (file.size() < sizeof(HuffmanPackage) || memcmp(file.data(), COMPRESSED_RIF_IDENTIFIER, 8) != 0)
	{
		return file;
	}

	std::vector<char> level;
	char *const unpacked = HuffmanDecompressChecked((HuffmanPackage const *)file.data(), (int)file.size());

	if (unpacked)
	{
		int size;
		memcpy(&size, file.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(size));
		level.assign(unpacked, unpacked + size);
		free(unpacked);
	}

	return level;
}

static std::string scratchFolder;

// What the game's own library writes for a level after prepare_for_output(): straight on the library, in a file of this test's own. Empty if the library refuses the level.
static std::vector<char> AsIntengWritesIt(std::vector<char> const &level)
{
	std::string const path = scratchFolder + "prepared.tmp";
	std::vector<char> out;

	if (!WriteWholeFile(path, level))
	{
		return out;
	}

	auto *const before = Parent_File;

	{
		std::unique_ptr<File_Chunk> const tree(new File_Chunk(path.c_str()));

		if (tree->error_code == 0)
		{
			tree->prepare_for_output();
			size_t const size = tree->size_chunk();
			std::vector<char> block(size, 0);
			tree->fill_data_block(block.data());
			out.swap(block);
		}
	}

	Parent_File = before;
	DeleteFileA(path.c_str());
	return out;
}

// How many objects of the level have an object header (each has a version number that prepare_for_output() puts up by one)
static size_t HeadersIn(OpenLevelFile &level)
{
	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);

	size_t count = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->get_header())
		{
			count++;
		}
	}

	return count;
}

// Is bytes [from, to) a run of chunks end to end (each an 8-character printable name, then a size that fits)? Then the chunk it is the inside of holds chunks.
static bool HoldsChunks(std::vector<char> const &bytes, size_t from, size_t to)
{
	if (to - from < 12)
	{
		return false;
	}

	for (size_t at = from; at < to; )
	{
		if (to - at < 12)
		{
			return false;
		}

		for (int c = 0; c < 8; c++)
		{
			unsigned char const letter = (unsigned char)bytes[at + c];

			if (letter < 0x20 || letter > 0x7e)
			{
				return false;
			}
		}

		int32_t size;
		memcpy(&size, &bytes[at + 8], 4);

		if (size < 12 || (size_t)size > to - at)
		{
			return false;
		}

		at += (size_t)size;
	}

	return true;
}

// Where two levels' chunk trees differ in which chunks they hold: for each holder, the count of each name, then the n-th chunk of a name in one against the n-th in the
// other. Each difference is printed as an info line (the first `most`), as "<path>: <name> held <n>, prepared <m>".
static void ChunkCountsDiffer(std::vector<char> const &held, size_t heldFrom, size_t heldTo, std::vector<char> const &prepared, size_t preparedFrom, size_t preparedTo,
	std::string const &path, int *most)
{
	std::map<std::string, std::vector<std::pair<size_t, size_t> > > inHeld, inPrepared;

	for (size_t at = heldFrom; at + 12 <= heldTo; )
	{
		int32_t size;
		memcpy(&size, &held[at + 8], 4);
		inHeld[std::string(&held[at], 8)].push_back(std::make_pair(at, at + (size_t)size));
		at += (size_t)size;
	}

	for (size_t at = preparedFrom; at + 12 <= preparedTo; )
	{
		int32_t size;
		memcpy(&size, &prepared[at + 8], 4);
		inPrepared[std::string(&prepared[at], 8)].push_back(std::make_pair(at, at + (size_t)size));
		at += (size_t)size;
	}

	std::set<std::string> names;

	for (auto const &one : inHeld)
	{
		names.insert(one.first);
	}

	for (auto const &one : inPrepared)
	{
		names.insert(one.first);
	}

	for (std::string const &name : names)
	{
		std::vector<std::pair<size_t, size_t> > const &a = inHeld[name], &b = inPrepared[name];

		if (a.size() != b.size())
		{
			if (*most > 0)
			{
				printf("  info  %s: %s held %zu, prepared %zu\n", path.c_str(), name.c_str(), a.size(), b.size());
				(*most)--;
			}

			continue;
		}

		for (size_t i = 0; i < a.size(); i++)
		{
			if (HoldsChunks(held, a[i].first + 12, a[i].second) && HoldsChunks(prepared, b[i].first + 12, b[i].second))
			{
				ChunkCountsDiffer(held, a[i].first + 12, a[i].second, prepared, b[i].first + 12, b[i].second, path + "/" + name + "#" + std::to_string(i), most);
			}
			else if (a[i].second - a[i].first != b[i].second - b[i].first && *most > 0)
			{
				// a chunk of numbers whose size the preparing changed: the editor's (held) chunk no longer pairs with the prepared bytes (level_file.cpp Pair: size_chunk() != bytes)
				printf("  info  %s/%s#%zu: held %zu bytes, prepared %zu bytes\n", path.c_str(), name.c_str(), i, a[i].second - a[i].first, b[i].second - b[i].first);
				(*most)--;
			}
		}
	}
}

// The ID of the level's object of that name (the first in the level's order); false when none has it
static bool IdNamed(OpenLevelFile &level, char const *name, uint8_t id[8])
{
	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			memcpy(id, &each()->object_data.ID, 8);
			return true;
		}
	}

	return false;
}

// S-35a (Drake 11-17): after an edit that adds, renames or takes out an object, a save leaves the level held AS THE FILE IT WROTE, the prepared bytes, as inteng's own tree is after its
// save: the edit kept the shape's header list of its objects' names as the save makes it (Shape_Header_Chunk::prepare_for_output), so the editor's tree pairs with the prepared
// bytes. Before, it paired only with its own image (Frost's fallback), and the next save's version numbers went up once less than inteng's.
static void HeldIsTheFileWritten(OpenLevelFile &level, std::string const &path, char const *edit)
{
	std::vector<char> file, held;

	Check(ReadWholeFile(path, &file) && level.BuildLevel(held) && !held.empty() && held == LevelIn(file),
		std::string("S-35a: after ") + edit + " and a save, the level held is the file just written (it pairs with the prepared bytes, not only with its own image)");
}

// An object taken out of the level, a save, a second object taken out, a save again: both edits and both saves are made, and the second copy has neither object. Until
// 2026-11-17 the save paired the editor's tree with the prepared bytes only; once an object was taken out they no longer paired, the level was left unpaired, and the second
// delete and the second save were refused. The info lines say where the prepared level's chunks differ from the level as held. (Added 2026-11-17 by Frost, Bishop's report
// from close-out F-1.)
static void DeletedThenSaved(char const *levelPath, char const *first, char const *second)
{
	puts("an object taken out, a save, another taken out, a save again");

	OpenLevelFile work;
	uint8_t id[8];

	Check(work.Open(levelPath) == LevelOpenResult::Opened && work.Level(), std::string("the game's loader takes ") + levelPath);

	if (!work.Level() || !IdNamed(work, first, id))
	{
		Check(false, std::string("the level has an object named ") + first);
		return;
	}

	Check(host::DeleteObject(&work, id) == host::ObjectEditResult::Done, std::string(first) + " is taken out");

	std::vector<char> held;

	if (work.BuildLevel(held))
	{
		std::vector<char> const prepared = AsIntengWritesIt(held);
		int most = 12;

		if (prepared.size() >= 12)
		{
			ChunkCountsDiffer(held, 12, held.size(), prepared, 12, prepared.size(), "level", &most);
		}

		printf("  info  held %zu bytes, prepared %zu bytes%s\n", held.size(), prepared.size(), most == 12 ? ", the same chunks in each holder, each the same size" : "");
	}

	std::string const firstSave = scratchFolder + "save-prepared-deleted-1.rif", secondSave = scratchFolder + "save-prepared-deleted-2.rif";

	DeleteFileA(firstSave.c_str());
	DeleteFileA(secondSave.c_str());
	Check(work.SaveAs(firstSave.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY");
	HeldIsTheFileWritten(work, firstSave, "the delete");
	Check(IdNamed(work, second, id) && host::DeleteObject(&work, id) == host::ObjectEditResult::Done, std::string("after the save, ") + second + " is taken out too");
	Check(work.SaveAs(secondSave.c_str()) == LevelSaveResult::Saved, "and the level is saved again");

	OpenLevelFile back;
	uint8_t ignore[8];

	Check(back.Open(secondSave.c_str()) == LevelOpenResult::Opened && back.Level() && !IdNamed(back, first, ignore) && !IdNamed(back, second, ignore),
		std::string("the game's loader takes the second copy, and it has neither ") + first + " nor " + second);

	for (std::string const &path : { firstSave, secondSave })
	{
		DeleteFileA(path.c_str());
		DeleteFileA((path + ".bak").c_str());
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_save_prepared_test: give a level .rif and a folder for the copies; with none there is nothing to run.\n");
		printf("passed host_save_prepared_test (0 ok)\n");
		return 0;
	}

	std::string const folder = argv[2];
	CreateDirectoryA(folder.c_str(), NULL);
	scratchFolder = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\");

	std::vector<char> original;

	if (!ReadWholeFile(argv[1], &original))
	{
		printf("  FAILED  the level can be read: %s\n", argv[1]);
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	size_t const headers = HeadersIn(level);
	std::vector<char> built;
	Check(level.BuildLevel(built), "the level is built from what the editor read");
	Check(headers > 0, "the level has objects with headers");

	std::string const copyPath = scratchFolder + "save-prepared-copy.rif";
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY with nothing changed");

	std::vector<char> copy;
	Check(ReadWholeFile(copyPath, &copy), "the copy can be read");
	copy = LevelIn(copy); // the level inside it, when the file is packed (the copy is packed again when the file was)

	std::vector<char> const wanted = AsIntengWritesIt(built);
	Check(!wanted.empty() && copy == wanted, "the copy is exactly what the game's library writes after prepare_for_output() on the whole tree (inteng's write_file)");

	if (copy != wanted)
	{
		size_t first = 0, count = 0;

		for (size_t i = 0; i < copy.size() && i < wanted.size(); i++)
		{
			if (copy[i] != wanted[i])
			{
				if (!count)
				{
					first = i;
				}

				count++;
			}
		}

		printf("  info  copy %zu bytes, wanted %zu bytes, first difference at %zu, %zu bytes differ\n", copy.size(), wanted.size(), first, count);
	}

	size_t differing = 0;

	for (size_t i = 0; i < copy.size() && i < built.size(); i++)
	{
		if (copy[i] != built[i])
		{
			differing++;
		}
	}

	Check(copy != built && differing >= headers, "it is not the file's bytes: the version numbers went up, at least one byte for every object header");
	printf("  info  level %zu bytes, copy %zu bytes, %zu bytes differ, %zu object headers\n", original.size(), copy.size(), differing, headers);

	// After the save the level IS the file just written (inteng's own tree is the prepared one after its save), and an edit's rebuild (ReplaceLevel) of it is taken
	{
		std::vector<char> again;
		Check(level.BuildLevel(again) && again == copy, "after the save the level is built as the file just written (the prepared bytes)");
		Check(level.ReplaceLevel(again) == LevelOpenResult::Opened, "an edit's rebuild of the level (ReplaceLevel) is taken after the save");
	}

	OpenLevelFile reopened;
	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		Check(HeadersIn(reopened) == headers, "the copy holds the same objects");

		std::string const secondPath = scratchFolder + "save-prepared-second.rif";
		DeleteFileA(secondPath.c_str());
		Check(reopened.SaveAs(secondPath.c_str()) == LevelSaveResult::Saved, "the copy is saved again with nothing changed");

		std::vector<char> second;
		Check(ReadWholeFile(secondPath, &second) && (second = LevelIn(second)) == AsIntengWritesIt(copy) && second != copy,
			"it is the library's prepare of the copy: the numbers go up once more with each save, as inteng's do");
		DeleteFileA(secondPath.c_str());
		DeleteFileA((secondPath + ".bak").c_str());
	}

	DeleteFileA(copyPath.c_str());
	DeleteFileA((copyPath + ".bak").c_str());

	// 11-17 (Drake): an object COPIED (Object Placement's Create Duplicate of a shape with no outside file, CreateObjectLike). Preparing the level grows the shape's header (the list
	// of the objects that use it), so the tree held stopped pairing with the bytes the first save wrote and every later save was refused (NothingOpen): Object Placement's Exit
	// saves, so his Save a copy after it failed. Frost's 657abcb pairs the level with the image it holds when the prepared bytes do not pair: this is the P path's red-first case.
	{
		OpenLevelFile edited;
		uint8_t like[8] = {};
		bool haveLike = false;

		if (edited.Open(argv[1]) == LevelOpenResult::Opened && edited.Level())
		{
			List<Object_Chunk *> all;

			edited.Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done() && !haveLike; each.next())
			{
				if (each() && each()->get_header() && (each()->get_header()->flags & 0x400) != 0)
				{
					memcpy(like, &each()->object_data.ID, 8);
					haveLike = true;
				}
			}
		}

		uint8_t madeId[8] = {};
		int32_t const place[3] = { 1000, 2000, 3000 };

		// V-27b: the numbers before the copy, worked out here on their own (every header's number at 48)
		std::vector<int32_t> numbersBefore;

		if (edited.Level())
		{
			List<Object_Chunk *> all;

			edited.Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				std::vector<unsigned char> bytes;
				int32_t number = -1;

				if (each() && each()->get_header() && edited.PayloadOf(each()->get_header(), &bytes) && bytes.size() >= 52)
				{
					memcpy(&number, &bytes[48], sizeof(number));
					numbersBefore.push_back(number);
				}
			}
		}

		int32_t lowestBefore = 0;

		while (std::find(numbersBefore.begin(), numbersBefore.end(), lowestBefore) != numbersBefore.end())
		{
			lowestBefore++;
		}

		Check(haveLike && host::CreateObjectLike(&edited, like, "savePairA", place, madeId) == host::ObjectEditResult::Done, "a placed object is copied (CreateObjectLike)");

		// V-27b: the copy is a new object as inteng's save makes one (object_number.h): the lock user "Player" and ten zeros, and the lowest number no object had
		{
			std::string lock;
			int32_t number = -1;
			List<Object_Chunk *> all;

			if (edited.Level())
			{
				edited.Level()->list_objects(&all);
			}

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				std::vector<unsigned char> bytes;

				if (each() && memcmp(&each()->object_data.ID, madeId, 8) == 0 && each()->get_header() && edited.PayloadOf(each()->get_header(), &bytes) && bytes.size() >= 52)
				{
					lock = std::string((char const *)&bytes[4], 16);
					memcpy(&number, &bytes[48], sizeof(number));
				}
			}

			Check(lock == std::string("Player\0\0\0\0\0\0\0\0\0\0", 16), "the copy's lock user is inteng's \"Player\" and ten zeros (its first letter: " +
				std::to_string(lock.empty() ? -1 : (int)(unsigned char)lock[0]) + ")");
			Check(number == lowestBefore, "the copy's number is the lowest no object had: " + std::to_string(number) + ", wanted " + std::to_string(lowestBefore));
		}

		std::string const firstPath = scratchFolder + "save-prepared-dup1.rif", againPath = scratchFolder + "save-prepared-dup2.rif", thirdPath = scratchFolder + "save-prepared-dup3.rif";
		DeleteFileA(firstPath.c_str());
		DeleteFileA(againPath.c_str());
		DeleteFileA(thirdPath.c_str());
		Check(edited.SaveAs(firstPath.c_str()) == LevelSaveResult::Saved, "the level with the copied object is saved");
		HeldIsTheFileWritten(edited, firstPath, "the copy (CreateObjectLike)");

		LevelSaveResult const again = edited.SaveAs(againPath.c_str());

		Check(again == LevelSaveResult::Saved, "and saved AGAIN in the same sitting (it was refused, NothingOpen: SaveAs answered " +
			std::to_string((int)again) + ")");
		Check(edited.SaveAs(thirdPath.c_str()) == LevelSaveResult::Saved, "and a third time");

		OpenLevelFile back;

		Check(back.Open(thirdPath.c_str()) == LevelOpenResult::Opened && back.Level(), "the game's loader takes the third save");

		for (std::string const &path : { firstPath, againPath, thirdPath })
		{
			DeleteFileA(path.c_str());
			DeleteFileA((path + ".bak").c_str());
		}
	}

	// 11-17 (Drake): P's OTHER branch, a placed object whose shape HAS an outside file (Invasion's first: Flam01; Genshd1's locker02 is "Cabinet"): a new object made from the shape
	// (CreateObjectFromShape, named "<shape><n>"), renamed (svB), saved three times. The shape's header still lists the made name and the object's chunks were changed in their
	// bytes, so the editor's tree pairs with neither the prepared bytes nor its own image: SaveAs's step 7 makes the level held the one written. Red on 406f11e (the
	// second save answered NothingOpen).
	{
		OpenLevelFile edited;
		uint8_t like[8] = {};
		std::string shapeName;

		if (edited.Open(argv[1]) == LevelOpenResult::Opened && edited.Level())
		{
			std::vector<host::ShapeChoice> shapes;

			host::ShapeChooser_List(edited.Level(), &edited, &shapes);

			List<Object_Chunk *> all;

			edited.Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done() && shapeName.empty(); each.next())
			{
				int32_t number = -1;

				if (each() && each()->get_header() && (each()->get_header()->flags & 0x400) != 0 &&
					host::ShapeChooser_ShapeOfObject(edited.Level(), &edited, (uint8_t const *)&each()->object_data.ID, &number))
				{
					for (host::ShapeChoice const &one : shapes)
					{
						if (one.number == number && !one.name.empty())
						{
							shapeName = one.name;
							memcpy(like, &each()->object_data.ID, 8);
						}
					}
				}
			}
		}

		int32_t const place[3] = { 1000, 2000, 3000 };
		int32_t const matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
		uint8_t newId[8] = {}, madeId[8] = {};
		std::string autoName, why;
		bool const made = !shapeName.empty() && host::CreateObjectFromShape(&edited, shapeName.c_str(), place, matrix, newId, &autoName, "") == host::ObjectMakeResult::Done;

		Check(made, "a placed object is made from the shape of a placed one with an outside file (" + shapeName + ", made as " + autoName + ")");
		host::ObjectEditAnswer const renamed = made ? host::ObjectEdit_Rename(&edited, edited.Level(), newId, "svB", madeId, &why) : host::ObjectEditAnswer::BadCall;

		Check(renamed == host::ObjectEditAnswer::Done, "and renamed svB (shorter than the made name: a header never changes its size), as P names it (answer " + std::to_string((int)renamed) + (why.empty() ? std::string() : ": " + why) + ")");

		std::string const paths[3] = { scratchFolder + "save-prepared-shape1.rif", scratchFolder + "save-prepared-shape2.rif", scratchFolder + "save-prepared-shape3.rif" };

		for (int round = 0; round < 3; round++)
		{
			DeleteFileA(paths[round].c_str());

			LevelSaveResult const saved = edited.SaveAs(paths[round].c_str());

			Check(saved == LevelSaveResult::Saved, "saved, round " + std::to_string(round + 1) + " of 3 (SaveAs answered " + std::to_string((int)saved) + "; the second was refused, NothingOpen)");

			if (round == 0)
			{
				HeldIsTheFileWritten(edited, paths[0], "a make from a shape with an outside file and its rename (P)");
			}
		}

		OpenLevelFile back;

		Check(back.Open(paths[2].c_str()) == LevelOpenResult::Opened && back.Level(), "the game's loader takes the third save");

		for (std::string const &path : paths)
		{
			DeleteFileA(path.c_str());
			DeleteFileA((path + ".bak").c_str());
		}
	}

	// a second level and two of its objects (the .runargs: Fall, Pboost0, pulse0, the objects acceptance-save-then-edit deletes)
	if (argc >= 6)
	{
		DeletedThenSaved(argv[3], argv[4], argv[5]);
	}

	std::vector<char> originalAfter;
	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_save_prepared_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_save_prepared_test\n");
	return 0;
}
