// The test of inteng's SAVE on real levels (Bishop's audit 2026-11-15): a save does what inteng's write_file does (File_Chunk::write_file, mishchnk.cpp:391): prepare_for_output() on the
// WHOLE tree (every object header's version number goes up by one, the environment's and the sprites' too, the shape headers get their numbers), then the library writes the whole tree.
// A save with NOTHING changed on a stock level is therefore not the file's bytes: it is what inteng would write. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_save_prepared_test.exe <a level .rif> <a folder it may write its copies into>. With no arguments it says so and passes. The level is only read; the copies are the only files written.
// CHECKS: a save of the level with nothing changed (as a COPY) is exactly what the game's own library writes after prepare_for_output() (done here straight on the library, not through
// level_file.cpp); it differs from the file (the version numbers went up: at least one byte for every object header); the game's loader takes the copy and it holds the same objects;
// a second save of the copy is the library's prepare of the copy again (the numbers go up once more); the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "huffman.hpp"
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
