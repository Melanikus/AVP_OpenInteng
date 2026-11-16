// The test for writing an EMPTY link tree (the editor's WriteTheLinks when the last link of a module was deleted): host::DropModuleLinks (src/host/object_edit.cpp) then the module flag out
// (host::SetModuleObjectFlag, src/host/object_base.cpp): the module has no VMDARRAY and no flag 0x200 afterwards, so its links and its flag agree. A NAMED DEVIATION from inteng, which
// leaves the old VMDARRAY (AvP-docs\director\plans\drake-deviations.md; Apone's check, Bishop's ruling). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_empty_links_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: a module of the level that has links and the flag; after the two steps it has neither; every other module has the links and the flag it had; the game's loader takes the result
// (the level is saved as a COPY and read again, the module still has neither); a module with no links gives "nothing to drop" and changes nothing; the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "module_links.h"
#include "object_base.h"
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

// A module's flag 0x200 as the level's object header holds it
static bool FlagOf(OpenLevelFile &level, int32_t index)
{
	uint8_t id[8];

	if (!host::ModuleIdOfIndex(level.Level(), index, id))
	{
		return false;
	}

	List<Object_Chunk *> all;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0 && each()->get_header())
		{
			std::vector<unsigned char> payload;
			uint32_t flags = 0;

			if (level.PayloadOf(each()->get_header(), &payload) && payload.size() >= 4)
			{
				memcpy(&flags, payload.data(), 4);
			}

			return (flags & 0x200) != 0;
		}
	}

	return false;
}

struct State
{
	bool links;
	std::vector<host::LinkItem> items;
	bool flag;
};

static std::map<int32_t, State> StatesOf(OpenLevelFile &level)
{
	std::map<int32_t, State> states;
	std::set<int32_t> modules;

	host::ModuleIndexes(level.Level(), &modules);

	for (int32_t const module : modules)
	{
		State state;

		state.links = host::ReadModuleLinks(level.Level(), module, &state.items);
		state.flag = FlagOf(level, module);
		states[module] = state;
	}

	return states;
}

static bool SameState(State const &a, State const &b)
{
	if (a.links != b.links || a.flag != b.flag || a.items.size() != b.items.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.items.size(); i++)
	{
		if (a.items[i].objectIndex != b.items[i].objectIndex || a.items[i].branchNo != b.items[i].branchNo)
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_empty_links_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_empty_links_test (0 ok)\n");
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

	std::map<int32_t, State> const before = StatesOf(level);
	int32_t target = -1, noLinks = -1;

	for (std::map<int32_t, State>::const_iterator it = before.begin(); it != before.end(); ++it)
	{
		if (it->second.links && it->second.flag && target < 0)
		{
			target = it->first;
		}

		if (!it->second.links && noLinks < 0)
		{
			noLinks = it->first;
		}
	}

	Check(target >= 0, "the level has a module with links and the flag: " + std::to_string(target));

	if (target < 0)
	{
		return 1;
	}

	// the two steps of the editor's write of an empty tree
	bool dropped = false;
	host::ObjectEditResult const drop = host::DropModuleLinks(&level, target, &dropped);

	Check(drop == host::ObjectEditResult::Done && dropped, std::string("the module's VMDARRAY is destroyed and the game's loader takes the level: ") + host::ObjectEditWords(drop));

	uint8_t id[8];

	Check(host::ModuleIdOfIndex(level.Level(), target, id), "the module is found again in the new level");
	Check(host::SetModuleObjectFlag(&level, id, false) == host::BaseObjectResult::Done, "the module flag 0x200 comes out");

	std::map<int32_t, State> const after = StatesOf(level);

	Check(!after.at(target).links && !after.at(target).flag, "the module has no VMDARRAY and no flag: its links and its flag agree");

	size_t others = 0, same = 0;

	for (std::map<int32_t, State>::const_iterator it = before.begin(); it != before.end(); ++it)
	{
		if (it->first != target)
		{
			others++;
			same += SameState(it->second, after.at(it->first)) ? 1 : 0;
		}
	}

	Check(same == others, "every other module has the links and the flag it had: " + std::to_string(same) + " of " + std::to_string(others));

	// a module with no links: nothing to drop
	if (noLinks >= 0)
	{
		bool again = true;

		Check(host::DropModuleLinks(&level, noLinks, &again) == host::ObjectEditResult::Done && !again, "a module with no links: nothing is dropped and nothing is changed");
	}

	// the copy
	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "empty-links-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		std::map<int32_t, State> const reread = StatesOf(reopened);

		Check(!reread.at(target).links && !reread.at(target).flag, "read again from the copy the module still has no VMDARRAY and no flag");
	}

	DeleteFileA(copyPath.c_str());

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_empty_links_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_empty_links_test\n");
	return 0;
}
