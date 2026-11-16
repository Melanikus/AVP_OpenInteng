// The test for Delete Links (AutoLink, Module 0xa7 and Environment 0xa8) and for the module flag that the writing of a module's links keeps (Drake's
// SetModuleObjectFlag, src/host/object_base.cpp, with ModuleData_DeleteLinks, SetModuleLinks and LinkTree), written by someone who did not write them.
// It runs on a REAL level (the first program argument): the level is opened with the game's loader, the edits are made through the editor's own host
// functions in the order the editor makes them, the level is saved as a COPY (never the opened file) and loaded again with the game's loader, and the
// answers are read with the game's own chunk library, not with the code under test. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_delete_links_test.exe <a level .rif> <a folder it may write its copies into (made if it isn't there)>. With no arguments it says so and passes.
// The level is only read; the copies are the only files written.
//
// THE ORDER THE EDITOR USES (editor_main.cpp, DeleteModuleLinks and WriteTheLinks): Delete Links takes a module X: ModuleData_DeleteLinks(X) destroys the
// VMDARRAY, a new empty LinkTree for X is marked changed; the next write of that tree has no link, so it takes the module flag 0x200 out (WriteTheLinks).
// A tree that keeps links is written with SetModuleLinks and then the flag is put in. This test does exactly those calls.
//
//  1. Delete Links on a module X that has links: ModuleData_DeleteLinks says Done and how many it took out; the new tree has no link (HasLinks false);
//     taking the flag out says Done;
//  2. in the copy, X still has its MODULEDT, it holds no VMDARRAY, and X's header flags are what they were with the 0x200 bit taken out and nothing else changed;
//  3. a module Y that keeps its links is written back (its items loaded into a LinkTree, flattened, SetModuleLinks, then the flag put in): in the copy it still
//     has links, the same number, to the same modules, and its flag has 0x200;
//  4. every OTHER module's links are item for item what they were, and every other object's header flags are what they were;
//  5. the number of objects is as it was; asked again for X, ModuleData_DeleteLinks says NothingToDelete and changes nothing;
//  6. the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
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
#include "module_data_edit.h"
#include "object_edit.h"
#include "object_base.h"

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

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

// What the game's own chunk library says about one level: for every object its header flags and, for the modules, the items of its VMDARRAY
struct Snapshot
{
	std::map<std::string, int> flagsOfObject;                       // by 8-byte ID in hex
	std::map<int32_t, std::vector<host::LinkItem>> linksOfModule;   // by object index; only modules that have a VMDARRAY
	std::map<int32_t, std::string> idOfModule;
	std::set<int32_t> modulesWithMODULEDT;
	size_t objects = 0;
};

static std::string HexOf(uint8_t const *id)
{
	char text[17];

	for (int at = 0; at < 8; at++)
	{
		_snprintf_s(text + 2 * at, 3, _TRUNCATE, "%02X", id[at]);
	}

	return std::string(text, 16);
}

static Snapshot Take(OpenLevelFile &open)
{
	Snapshot now;
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	std::set<int32_t> modules;

	host::ModuleIndexes(open.Level(), &modules);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		now.objects++;

		Object_Header_Chunk *const header = object->get_header();

		now.flagsOfObject[HexOf((uint8_t const *)&object->object_data.ID)] = header ? header->flags : -1;

		if (modules.count(object->object_data.index_num) != 0)
		{
			now.idOfModule[object->object_data.index_num] = HexOf((uint8_t const *)&object->object_data.ID);

			if (object->lookup_single_child("MODULEDT"))
			{
				now.modulesWithMODULEDT.insert(object->object_data.index_num);
			}
		}
	}

	for (int32_t const index : modules)
	{
		std::vector<host::LinkItem> items;

		if (host::ReadModuleLinks(open.Level(), index, &items))
		{
			now.linksOfModule[index] = items;
		}
	}

	return now;
}

static bool SameItems(std::vector<host::LinkItem> const &a, std::vector<host::LinkItem> const &b)
{
	return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size() * sizeof(host::LinkItem)) == 0);
}

static bool IdFromHex(std::string const &hex, uint8_t *id)
{
	for (int at = 0; at < 8; at++)
	{
		unsigned int value = 0;

		if (hex.size() != 16 || sscanf_s(hex.substr((size_t)at * 2, 2).c_str(), "%2x", &value) != 1)
		{
			return false;
		}

		id[at] = (uint8_t)value;
	}

	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_delete_links_test <a level .rif> <a folder for its copies>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	std::string const levelPath = argv[1];
	std::string const copyFolder = argv[2];
	std::vector<char> original;

	Check(ReadWholeFile(levelPath, &original), "the level is read (it is only read, never written)");

	if (original.empty())
	{
		return 1;
	}

	OpenLevelFile pristine;

	if (pristine.Open(levelPath.c_str()) != LevelOpenResult::Opened || !pristine.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	Snapshot const before = Take(pristine);

	// X: a module with links, to be deleted; Y: another module with links that has the flag (so "keeps it" can be seen), to be written back
	int32_t x = -1, y = -1;

	for (auto const &entry : before.linksOfModule)
	{
		bool const flagged = (before.flagsOfObject.at(before.idOfModule.at(entry.first)) & 0x200) != 0;

		if (x < 0 && !entry.second.empty() && flagged && before.modulesWithMODULEDT.count(entry.first) != 0)
		{
			x = entry.first;
		}
		else if (y < 0 && !entry.second.empty() && flagged && before.modulesWithMODULEDT.count(entry.first) != 0)
		{
			y = entry.first;
		}
	}

	Check(x >= 0 && y >= 0, "the level has two modules with links and the flag 0x200: X = " + std::to_string(x) + ", Y = " + std::to_string(y));

	if (x < 0 || y < 0)
	{
		return 1;
	}

	std::string const idX = before.idOfModule.at(x);
	std::string const idY = before.idOfModule.at(y);
	uint8_t xId[8], yId[8];

	IdFromHex(idX, xId);
	IdFromHex(idY, yId);

	printf("  (X is module %d with %zu links, flags 0x%X; Y is module %d with %zu links, flags 0x%X; %zu objects)\n", (int)x, before.linksOfModule.at(x).size(),
		(unsigned)before.flagsOfObject.at(idX), (int)y, before.linksOfModule.at(y).size(), (unsigned)before.flagsOfObject.at(idY), before.objects);

	OpenLevelFile edit;

	if (edit.Open(levelPath.c_str()) != LevelOpenResult::Opened || !edit.Level())
	{
		Check(false, "the game's loader takes the level for the edits");
		return 1;
	}

	// 1. Delete Links on X, in the editor's order
	size_t removed = 0;
	host::ModuleDataResult const deleted = host::ModuleData_DeleteLinks(&edit, xId, &removed);

	Check(deleted == host::ModuleDataResult::Done && removed >= 1, "Delete Links on X: ModuleData_DeleteLinks is Done and took out " + std::to_string(removed) + " VMDARRAY");

	host::LinkTree emptyTree(x);

	emptyTree.MarkChanged();
	Check(emptyTree.Changed() && !emptyTree.HasLinks(), "the new tree for X is changed and has no link");

	host::BaseObjectResult const flagOut = host::SetModuleObjectFlag(&edit, xId, false);

	Check(flagOut == host::BaseObjectResult::Done, std::string("taking the flag out of X is Done") + (flagOut == host::BaseObjectResult::Done ? "" : std::string(": ") + host::BaseObjectWords(flagOut)));

	// 3. Y written back as WriteTheLinks writes a tree that keeps links. Its flag is taken out first, so that putting it in is a change that can be seen
	// (in stock every module with links has it already)
	Check(host::SetModuleObjectFlag(&edit, yId, false) == host::BaseObjectResult::Done, "the flag is taken out of Y first, so that the write has to put it back");

	std::vector<host::LinkItem> yItems;

	Check(host::ReadModuleLinks(edit.Level(), y, &yItems) && !yItems.empty(), "Y's links are read from the edited level");

	std::set<int32_t> modules;

	host::ModuleIndexes(edit.Level(), &modules);

	host::LinkTree yTree(y);

	yTree.Load(yItems.data(), (int)yItems.size(), [&modules](int32_t index) { return modules.count(index) != 0; });
	yTree.MarkChanged();
	Check(yTree.HasLinks(), "Y's tree has links");

	std::vector<host::LinkItem> const flat = yTree.Flatten();
	std::vector<host::ModuleLinkItem> written;

	for (host::LinkItem const &item : flat)
	{
		host::ModuleLinkItem const one = { item.objectIndex, item.branchNo, item.flags, item.spare };

		written.push_back(one);
	}

	host::ObjectEditResult const wrote = host::SetModuleLinks(&edit, yId, written.data(), written.size());

	Check(wrote == host::ObjectEditResult::Done, std::string("SetModuleLinks on Y is Done") + (wrote == host::ObjectEditResult::Done ? "" : std::string(": ") + host::ObjectEditWords(wrote)));

	host::BaseObjectResult const flagIn = host::SetModuleObjectFlag(&edit, yId, true);

	Check(flagIn == host::BaseObjectResult::Done, "putting the flag into Y is Done");

	std::string const copyPath = CopyPathIn(copyFolder, "delete-links-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(edit.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (!after.Level())
	{
		return 1;
	}

	Snapshot const now = Take(after);

	// 2. X
	Check(now.modulesWithMODULEDT.count(x) != 0 && now.linksOfModule.count(x) == 0, "in the copy X still has its MODULEDT and it holds no VMDARRAY");

	int const xBefore = before.flagsOfObject.at(idX);
	int const xAfter = now.flagsOfObject.count(idX) ? now.flagsOfObject.at(idX) : -1;

	Check(xAfter == (xBefore & ~0x200), "X's header flags are what they were with the 0x200 bit taken out: " + std::to_string(xBefore) + " to " + std::to_string(xAfter) + " (decimal)");

	// 3. Y
	bool const yHasLinks = now.linksOfModule.count(y) != 0;
	int const yAfter = now.flagsOfObject.count(idY) ? now.flagsOfObject.at(idY) : -1;

	Check(yHasLinks && now.linksOfModule.at(y).size() == yItems.size(), "in the copy Y still has links, the same number: " + std::to_string(yHasLinks ? now.linksOfModule.at(y).size() : 0) + " of " + std::to_string(yItems.size()));

	bool sameModules = yHasLinks;

	if (yHasLinks)
	{
		std::multiset<int32_t> a, b;

		for (host::LinkItem const &item : yItems)
		{
			a.insert(item.objectIndex);
		}

		for (host::LinkItem const &item : now.linksOfModule.at(y))
		{
			b.insert(item.objectIndex);
		}

		sameModules = a == b;
	}

	Check(sameModules, "Y's links go to the same modules as before");
	Check((yAfter & 0x200) != 0 && yAfter == before.flagsOfObject.at(idY), "Y keeps the flag 0x200, and its flags are what they were");

	// 4. everything else
	size_t othersChanged = 0, flagsChanged = 0;

	for (auto const &entry : before.linksOfModule)
	{
		if (entry.first == x || entry.first == y)
		{
			continue;
		}

		othersChanged += (now.linksOfModule.count(entry.first) == 0 || !SameItems(entry.second, now.linksOfModule.at(entry.first))) ? 1 : 0;
	}

	for (auto const &entry : before.flagsOfObject)
	{
		if (entry.first != idX && (now.flagsOfObject.count(entry.first) == 0 || now.flagsOfObject.at(entry.first) != entry.second))
		{
			flagsChanged++;
		}
	}

	Check(othersChanged == 0, "every OTHER module's links are item for item what they were: " + std::to_string(before.linksOfModule.size() - 2) + " modules compared, " + std::to_string(othersChanged) + " changed");
	Check(flagsChanged == 0, "every other object's header flags are what they were: " + std::to_string(before.flagsOfObject.size() - 1) + " objects compared, " + std::to_string(flagsChanged) + " changed");

	// 5. the number of objects, and X asked again
	Check(now.objects == before.objects, "the number of objects is as it was: " + std::to_string(before.objects));

	std::vector<char> imageBefore;
	std::vector<char> imageAfter;
	size_t again = 99;

	Check(edit.BuildLevel(imageBefore), "the edited level is made again (before asking twice)");
	Check(host::ModuleData_DeleteLinks(&edit, xId, &again) == host::ModuleDataResult::NothingToDelete && again == 0 && edit.BuildLevel(imageAfter) && imageAfter == imageBefore,
		"asked again for X, it says NothingToDelete and changes nothing");

	DeleteFileA(copyPath.c_str());

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
