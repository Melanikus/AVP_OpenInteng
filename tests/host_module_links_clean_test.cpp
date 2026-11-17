// The test of Module Linking's start taking out the links that name no module (src/host/module_links_clean.cpp; inteng's FUN_004c90c0, row ML-E-12 of
// AvP-docs\engine\inteng\reassess\module-linking-map-dietrich.md). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USE: host_module_links_clean_test.exe <Invasion .rif> <a folder it may write into> [every stock level, for the count]. The levels are only read; the folder gets the copies.
// THE CASES: (1) the rule on items, each step with its address; (2) on Invasion, gARAGDOR's links get two items that name no module (an object index past the level's, and a PLACED
// object's), put in by the editor's own SetModuleLinks; the pass takes exactly those out, the branch numbers renumbered as FUN_004c90c0 does, and the copy saved is taken by the loader
// (it is also left as dangling-links-orig.rif, before the pass, for acceptance-module-linking-clean); (3) how many such links the stock levels hold (printed, not checked).

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
#include "autolink_scene.h"
#include "module_links_clean.h"
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

// what the game's chunk code calls
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

static host::LinkItem Item(int32_t object, int32_t branchNo)
{
	host::LinkItem one = { branchNo, 0, 0, object };

	return one;
}

static std::string Words(std::vector<host::LinkItem> const &items)
{
	std::string out;

	for (host::LinkItem const &item : items)
	{
		out += (out.empty() ? "" : " ") + std::to_string(item.objectIndex) + "/" + std::to_string(item.branchNo);
	}

	return out.empty() ? "(none)" : out;
}

int main(int argc, char **argv)
{
	// ---- (1) the rule: the modules are 1..9; 50 and 60 name none
	auto const isModule = [](int32_t index) { return index >= 1 && index <= 9; };

	{
		std::vector<host::LinkItem> items = { Item(1, 0), Item(50, 0), Item(2, 0) };
		size_t dropped = 0;

		bool const changed = host::ModuleLinks_DropDangling(&items, isModule, &dropped);

		Check(changed && dropped == 1 && Words(items) == "1/0 2/0", "a link to no module is taken out, the rest packed down: " + Words(items));
	}

	{
		// 1 holds a branch of items 1..2 (branch_no 3: the index of the first item after it): 50 and 2; then 3. 50 is dropped when one item is kept: every branch_no above 1 is
		// lowered by one (0x4c9244-0x4c925e): 1's becomes 2, so its branch is 2 alone, and 3 follows it
		std::vector<host::LinkItem> items = { Item(1, 3), Item(50, 0), Item(2, 0), Item(3, 0) };

		bool const changed = host::ModuleLinks_DropDangling(&items, isModule, nullptr);

		Check(changed && Words(items) == "1/2 2/0 3/0", "the branch numbers after the dropped item are lowered: " + Words(items));
	}

	{
		std::vector<host::LinkItem> items = { Item(50, 0), Item(60, 0) };
		size_t dropped = 0;

		Check(host::ModuleLinks_DropDangling(&items, isModule, &dropped) && items.empty() && dropped == 2, "every link to no module: none kept (the VMDARRAY goes)");
	}

	{
		// nothing dropped, but a branch_no past the end (a damaged file): held to the count (0x4c928e-0x4c92a2), and that is a change
		std::vector<host::LinkItem> items = { Item(1, 9), Item(2, 0) };

		bool const changed = host::ModuleLinks_DropDangling(&items, isModule, nullptr);

		Check(changed && Words(items) == "1/2 2/0", "a branch number past the end is held to the count kept: " + Words(items));
	}

	{
		std::vector<host::LinkItem> items = { Item(1, 2), Item(2, 0), Item(3, 0) };

		bool const changed = host::ModuleLinks_DropDangling(&items, isModule, nullptr);

		Check(!changed && Words(items) == "1/2 2/0 3/0", "links that all name modules: nothing changes");
	}

	if (argc < 3)
	{
		printf("  FAILED  usage: host_module_links_clean_test <Invasion .rif> <folder> [stock levels]\n");
		return 1;
	}

	// ---- (2) on Invasion: two dangling links put into gARAGDOR's, then the pass
	{
		OpenLevelFile level;

		Check(level.Open(argv[1]) == LevelOpenResult::Opened && level.Level(), std::string("the level opens: ") + argv[1]);

		if (checksFailed)
		{
			printf("%d checks failed\n", checksFailed);
			return 1;
		}

		host::AutoLinkScene scene;
		std::set<int32_t> modules;
		int32_t garage = -1, placed = -1, past = 0;

		host::AutoLinkScene_Build(&level, 1.0, &scene);
		host::ModuleIndexes(level.Level(), &modules);

		for (auto const &one : scene.names)
		{
			garage = one.second == "gARAGDOR" && garage < 0 ? one.first : garage;
		}

		// a placed object: an object index that is not a module
		uint8_t id[8];

		for (int32_t index = 0; index < 100000 && placed < 0; index++)
		{
			placed = host::ObjectIdOfIndex(level.Level(), index, id) && !modules.count(index) ? index : placed;
		}

		for (int32_t const m : modules)
		{
			past = m >= past ? m + 1000 : past;
		}

		std::vector<host::LinkItem> stock;

		Check(garage >= 0 && placed >= 0 && host::ReadModuleLinks(level.Level(), garage, &stock) && stock.size() == 7,
			"the scene: gARAGDOR (object " + std::to_string(garage) + ") with its 7 stock links, a placed object (" + std::to_string(placed) + ")");

		// put in: after the first stock item, a link to the placed object; at the end, a link past the level's objects. Branch numbers past the first insert shift up by one.
		std::vector<host::ModuleLinkItem> planted;
		std::vector<host::LinkItem> expected;

		for (size_t i = 0; i < stock.size(); i++)
		{
			host::ModuleLinkItem const one = { stock[i].objectIndex, stock[i].branchNo > 1 ? stock[i].branchNo + 1 : stock[i].branchNo, stock[i].flags, stock[i].spare };

			planted.push_back(one);
			expected.push_back(stock[i]);

			if (i == 0)
			{
				host::ModuleLinkItem const toPlaced = { placed, 0, 0, 0 };

				planted.push_back(toPlaced);
			}
		}

		host::ModuleLinkItem const toNothing = { past, 0, 0, 0 };

		planted.push_back(toNothing);

		uint8_t garageId[8];

		Check(host::ModuleIdOfIndex(level.Level(), garage, garageId) && host::SetModuleLinks(&level, garageId, planted.data(), planted.size()) == host::ObjectEditResult::Done,
			"9 links written into gARAGDOR, two of them to no module");

		std::string const folder = argv[2];
		std::string const before = folder + "\\dangling-links-orig.rif", after = folder + "\\dangling-links-cleaned.rif";

		CreateDirectoryA(folder.c_str(), nullptr);
		DeleteFileA(before.c_str());
		Check(level.SaveAs(before.c_str()) == LevelSaveResult::Saved, "the level with the dangling links saved as a COPY: " + before);

		host::ModuleLinksCleaned done;
		std::string why;

		Check(host::ModuleLinks_CleanLevel(&level, &done, &why), "the pass ran: " + why);
		Check(done.modules == 1 && done.itemsDropped == 2 && done.emptied == 0, "it changed one module, two links taken out (" + std::to_string(done.modules) + " modules, " +
			std::to_string(done.itemsDropped) + " links)");

		std::vector<host::LinkItem> now;

		host::ReadModuleLinks(level.Level(), garage, &now);
		Check(Words(now) == Words(expected), "gARAGDOR's links are the stock ones again, branch numbers too: " + Words(now) + " (stock " + Words(expected) + ")");

		host::ModuleLinksCleaned again;

		Check(host::ModuleLinks_CleanLevel(&level, &again, &why) && again.modules == 0, "a second pass changes nothing");

		DeleteFileA(after.c_str());
		Check(level.SaveAs(after.c_str()) == LevelSaveResult::Saved, "the cleaned level saved as a COPY");

		OpenLevelFile reopened;
		std::vector<host::LinkItem> read;

		Check(reopened.Open(after.c_str()) == LevelOpenResult::Opened && reopened.Level() && host::ReadModuleLinks(reopened.Level(), garage, &read) && Words(read) == Words(expected),
			"the game's loader takes the copy, with gARAGDOR's stock links");
	}

	// ---- (3) the stock levels: how many links name no module (a count, not a check)
	for (int a = 3; a < argc; a++)
	{
		OpenLevelFile level;

		if (level.Open(argv[a]) != LevelOpenResult::Opened || !level.Level())
		{
			printf("  (%s: not opened)\n", argv[a]);
			continue;
		}

		std::set<int32_t> modules;
		size_t links = 0, dangling = 0, withDangling = 0;

		host::ModuleIndexes(level.Level(), &modules);

		for (int32_t const m : modules)
		{
			std::vector<host::LinkItem> items;
			size_t here = 0;

			host::ReadModuleLinks(level.Level(), m, &items);
			links += items.size();

			for (host::LinkItem const &item : items)
			{
				here += modules.count(item.objectIndex) ? 0 : 1;
			}

			dangling += here;
			withDangling += here ? 1 : 0;
		}

		char const *const slash = strrchr(argv[a], '\\');

		printf("  (%s: %zu links, %zu name no module, in %zu modules)\n", slash ? slash + 1 : argv[a], links, dangling, withDangling);
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
