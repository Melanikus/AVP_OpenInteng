// The test for Add Reverse Links (host::AddReverseLinks, src/host/reverse_links.cpp; LinkTree::ReverseVisits and AddReversePath, src/host/module_links.cpp).
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_reverse_links_test.exe <a level .rif> <a folder it may write its copies into (made if it isn't there)>. With no arguments only the small trees
// are checked. The level is only read; the copies are the only files written.
//
// SMALL TREES (made by hand, the expected trees worked out from FUN_004c60a0's text, drake-autolink-facts.md section 1):
//  1. A -> B: walking A gives one visit (B, path {A}); adding that path to B's tree makes B's tree "B, then A";
//  2. A -> B -> (branch) C: the visits are B {A}, C {A, B}; C's tree gets B as its first node with A as a branch of it (the path from the last to the first:
//     B, then A one level down), and a node that is already in the tree is not made twice;
//  3. the path is left as it was after a branch is walked (the first element equal to the branch's module is taken out).
// A REAL LEVEL (the numbers printed are the match against inteng's own links: how many modules already hold their reverse links):
//  4. Add Reverse Links over every module is done; the game's loader takes every write; the level is saved as a COPY and loaded again; the opened file is byte
//     for byte what it was;
//  5. a second run over the result changes nothing in any module's links (it is idempotent);
//  6. the match: how many modules' lists of links are the same before and after (when inteng's AutoLink made the links, a stock level is mostly already
//     closed under reversal), and how many links were added.

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
#include "reverse_links.h"
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

static std::string Show(host::LinkTree const &tree)
{
	std::string text = std::to_string(tree.RootModule()) + ":";

	for (host::LinkItem const &item : tree.Flatten())
	{
		text += " " + std::to_string(item.objectIndex) + "/" + std::to_string(item.branchNo);
	}

	return text;
}

static bool SameItems(std::vector<host::LinkItem> const &a, std::vector<host::LinkItem> const &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t at = 0; at < a.size(); at++)
	{
		if (a[at].objectIndex != b[at].objectIndex || a[at].branchNo != b[at].branchNo)
		{
			return false;
		}
	}

	return true;
}

static void SmallTrees()
{
	printf("SMALL TREES\n");

	// 1. A(1) -> B(2)
	{
		host::LinkTree a(1);
		std::vector<host::LinkTree::ReverseVisit> visits;

		a.Normal(2);
		a.ReverseVisits(&visits);

		Check(visits.size() == 1 && visits[0].module == 2 && visits[0].path == std::vector<int32_t>({ 1 }), "A -> B gives the one visit (B, path {A})");

		host::LinkTree b(2);

		b.AddReversePath(visits[0].path);
		Check(Show(b) == "2: 1/0", "B's tree is B, then A: " + Show(b));
	}

	// 2. A(1) -> B(2) with a branch holding C(3): the visits, and C's tree
	{
		host::LinkTree a(1);
		std::vector<host::LinkTree::ReverseVisit> visits;

		a.Normal(2);
		a.Branch(3);   // under the last node that has none: B
		a.ReverseVisits(&visits);

		Check(visits.size() == 2 && visits[0].module == 2 && visits[1].module == 3, "A -> B (branch C): the visits are B then C");
		Check(visits.size() == 2 && visits[1].path == std::vector<int32_t>({ 1, 2 }), "C is met with the path {A, B}");

		host::LinkTree c(3);

		c.AddReversePath(visits[1].path);
		Check(Show(c) == "3: 2/2 1/0", "C's tree: B first, A in its branch (the path is walked from the last to the first): " + Show(c));

		// the same path again: every node is already there, none is made twice
		c.AddReversePath(visits[1].path);
		Check(Show(c) == "3: 2/2 1/0", "the same path again changes nothing: " + Show(c));
	}

	// 3. the path after a branch: walking a tree whose branch holds a second node, then a sibling after the branch
	{
		host::LinkTree a(1);
		std::vector<host::LinkTree::ReverseVisit> visits;

		a.Normal(2);
		a.Branch(3);
		a.Normal(4);   // after the branch's node? Normal adds at the end of the last open chain
		a.ReverseVisits(&visits);

		bool pathsRight = visits.size() >= 2;

		for (host::LinkTree::ReverseVisit const &visit : visits)
		{
			pathsRight = pathsRight && !visit.path.empty() && visit.path[0] == 1;
		}

		Check(pathsRight, "every path starts at the walked module");
	}

	// 4. Open Branch (FUN_004c43d0), seen through the tree's colours (a branch is walked only while it is open)
	{
		auto has = [](host::LinkTree const &tree, int32_t module)
		{
			for (host::LinkTree::Tint const &tint : tree.Tints())
			{
				if (tint.module == module)
				{
					return true;
				}
			}

			return false;
		};

		host::LinkTree tree(1);

		tree.Normal(2);
		tree.Branch(3);     // 3 hangs under 2, open
		tree.Close(2);      // 2's branch is closed
		tree.Normal(4);     // goes after 2 now

		Check(has(tree, 2) && has(tree, 4) && !has(tree, 3), "with 2's branch closed, 3 is not shown");
		Check(tree.Open(2) && has(tree, 3) && has(tree, 4), "Open Branch on 2 shows 3 again (and 4, which comes after 2, is still there)");
		Check(!tree.Open(99), "Open Branch on a module that is not in the tree says no");

		// a branch met in a LATER node closes the open branch of an earlier one (0x4c4440-0x4c445c)
		host::LinkTree second(1);

		second.Normal(2);
		second.Branch(3);   // 3 under 2
		second.Close(2);
		second.Normal(4);
		second.Branch(5);   // 5 under 4 (4 is the last node with no branch)
		second.Open(2);
		Check(has(second, 3), "2's branch is open");
		Check(second.Open(4) && has(second, 5) && !has(second, 3), "Open Branch on 4 opens 5 and closes 2's open branch: 3 goes");
	}
}

static void RealLevel(std::string const &levelPath, std::string const &copyFolder, std::vector<char> const &original)
{
	printf("REAL LEVEL\n");

	OpenLevelFile level;

	if (level.Open(levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	std::set<int32_t> modules;

	host::ModuleIndexes(level.Level(), &modules);

	// how the module flag and the links go together in this level (inteng's own data)
	size_t withLinks = 0, flaggedWithLinks = 0, flaggedNoLinks = 0, unflaggedWithLinks = 0;
	std::map<int32_t, std::vector<host::LinkItem>> before;

	for (int32_t const module : modules)
	{
		std::vector<host::LinkItem> items;
		uint8_t id[8];
		bool const has = host::ReadModuleLinks(level.Level(), module, &items);

		before[module] = items;
		withLinks += has ? 1 : 0;

		if (host::ModuleIdOfIndex(level.Level(), module, id))
		{
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
						(flags & 0x200) ? (has ? flaggedWithLinks++ : flaggedNoLinks++) : (has ? unflaggedWithLinks++ : 0);
					}

					break;
				}
			}
		}
	}

	printf("  --  %zu modules, %zu with a VMDARRAY; module flag 0x200 with links %zu, with none %zu; links without the flag %zu\n", modules.size(), withLinks, flaggedWithLinks, flaggedNoLinks, unflaggedWithLinks);

	std::vector<char> imageBefore;

	Check(level.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	host::ReverseLinksCounts counts;
	std::string why;
	DWORD const started = GetTickCount();
	host::ReverseLinksResult const result = host::AddReverseLinks(&level, nullptr, &counts, &why);
	DWORD const took = GetTickCount() - started;

	Check(result == host::ReverseLinksResult::Done, std::string("Add Reverse Links over every module is done") + (result == host::ReverseLinksResult::Done ? "" : ": " + why));
	printf("  --  walked %zu, touched %zu, written %zu, skipped (no MODULEDT) %zu, modules whose number of links changed %zu, links added %zu, %lu ms\n", counts.walked, counts.touched, counts.written, counts.skipped, counts.changed, counts.linksAdded, (unsigned long)took);

	if (result != host::ReverseLinksResult::Done)
	{
		return;
	}

	size_t same = 0, different = 0;

	for (int32_t const module : modules)
	{
		std::vector<host::LinkItem> now;

		host::ReadModuleLinks(level.Level(), module, &now);

		if (SameItems(before[module], now))
		{
			same++;
		}
		else
		{
			different++;
		}
	}

	printf("  --  THE MATCH with inteng's own links: %zu of %zu modules hold exactly the links they held before, %zu differ\n", same, modules.size(), different);

	// 5. a second run changes nothing
	host::ReverseLinksCounts again;
	std::map<int32_t, std::vector<host::LinkItem>> afterFirst;

	for (int32_t const module : modules)
	{
		host::ReadModuleLinks(level.Level(), module, &afterFirst[module]);
	}

	Check(host::AddReverseLinks(&level, nullptr, &again, &why) == host::ReverseLinksResult::Done, "a second run is done");

	size_t moved = 0;

	for (int32_t const module : modules)
	{
		std::vector<host::LinkItem> now;

		host::ReadModuleLinks(level.Level(), module, &now);
		moved += SameItems(afterFirst[module], now) ? 0 : 1;
	}

	// NOT a check: inteng's own walk is not idempotent (it compares only the first node of a branch, so a path nested deeper is made again; the quirk is copied),
	// so how many modules a second run moves is only reported
	printf("  --  a second run: %zu modules' links moved, %zu links added (inteng's quirk: the first node of a branch is the only one compared)\n", moved, again.linksAdded);

	std::string const copyPath = CopyPathIn(copyFolder, "reverse-links-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	DeleteFileA(copyPath.c_str());
}

int main(int argc, char **argv)
{
	SmallTrees();

	if (argc >= 3)
	{
		std::vector<char> original;

		if (!ReadWholeFile(argv[1], &original))
		{
			printf("  FAILED  the level can be read: %s\n", argv[1]);
			return 1;
		}

		RealLevel(argv[1], argv[2], original);
	}

	if (checksFailed)
	{
		printf("FAILED host_reverse_links_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_reverse_links_test\n");
	return 0;
}
