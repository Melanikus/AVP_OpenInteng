// The test of Delete (Object Placement 0x16): host::DeleteObject takes a placed object out of the open level whether or not other objects name it, as inteng's Delete does (the placed object's method +0x24 at 0x4e0480:
// it never asks who names the object; Melanikus's ruling 2026-11-15). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_delete_object_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: a PLACED object whose 8-byte ID is held by other bytes of the level (a name another object holds) is taken out and the answer is Done; the object is gone from the level and the level has one object less;
// the name the others held is still in the level (a dangling name); a module is still refused; the level saved as a COPY goes through the game's loader and reopens without the object; the opened file is unchanged.

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
#include "level_chunks.h"
#include "object_edit.h"

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

static size_t Occurrences(std::vector<char> const &bytes, unsigned char const *id)
{
	size_t count = 0;

	for (size_t i = 0; i + 8 <= bytes.size(); i++)
	{
		if (memcmp(bytes.data() + i, id, 8) == 0)
		{
			count++;
		}
	}

	return count;
}

static size_t ObjectsIn(OpenLevelFile &level)
{
	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);
	return all.size();
}

static bool Has(OpenLevelFile &level, unsigned char const *id)
{
	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
		{
			return true;
		}
	}

	return false;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_delete_object_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_delete_object_test (0 ok)\n");
		return 0;
	}

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

	std::vector<char> image;
	Check(level.BuildLevel(image), "the level is built from what the editor read");

	// a placed object (no MODULEDT) whose ID is held by more than its own chunk, and a module
	unsigned char target[8] = { 0 };
	unsigned char module[8] = { 0 };
	bool haveTarget = false, haveModule = false;
	size_t namedBefore = 0;
	size_t baseline = (size_t)-1;
	std::set<int32_t> modules;
	host::ModuleIndexes(level.Level(), &modules);

	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		unsigned char id[8];
		memcpy(id, &object->object_data.ID, 8);

		if (modules.count((int32_t)object->object_data.index_num))
		{
			if (!haveModule)
			{
				memcpy(module, id, 8);
				haveModule = true;
			}

			continue;
		}

		size_t const found = Occurrences(image, id);

		if (found < baseline)
		{
			baseline = found;
		}

		if (found > namedBefore)
		{
			memcpy(target, id, 8);
			namedBefore = found;
		}
	}

	// an object's ID is made from its name and is not in its own chunk, so any bytes that hold it are another object's name for it
	haveTarget = namedBefore > baseline;

	Check(haveTarget, "the level has a placed object that other objects name");

	if (!haveTarget)
	{
		printf("FAILED host_delete_object_test (no placed object here is named by another)\n");
		return 1;
	}

	size_t const objectsBefore = ObjectsIn(level);
	Check(haveModule && host::DeleteObject(&level, module) == host::ObjectEditResult::IsAModule, "a module is still refused");
	Check(ObjectsIn(level) == objectsBefore, "and the level has all its objects");

	host::ObjectEditResult const result = host::DeleteObject(&level, target);
	Check(result == host::ObjectEditResult::Done, std::string("the placed object another object names is taken out: ") + host::ObjectEditWords(result));
	Check(ObjectsIn(level) == objectsBefore - 1 && !Has(level, target), "the level has one object less, and not that one");

	std::vector<char> after;
	Check(level.BuildLevel(after), "the changed level is built");
	size_t const namedAfter = Occurrences(after, target);
	Check(namedAfter >= 1 && namedAfter == namedBefore, "the name the others held is still in the level, every one of them: a dangling name (" + std::to_string(namedAfter) + " of " + std::to_string(namedBefore) + ")");

	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "delete-object-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;
	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		Check(ObjectsIn(reopened) == objectsBefore - 1 && !Has(reopened, target), "read again from the copy the object is gone and the others are there");
	}

	DeleteFileA(copyPath.c_str());
	DeleteFileA((copyPath + ".bak").c_str());

	std::vector<char> originalAfter;
	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_delete_object_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_delete_object_test\n");
	return 0;
}
