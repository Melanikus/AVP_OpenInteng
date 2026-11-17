// verify_autolink: the check of an AutoLink scenario's saved copy against the level it opened, read back through the game's own loader (rows ML-A-1..7 of
// AvP-docs\engine\inteng\reassess\module-linking-map-dietrich.md). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". (verify_links is the hand-made links' check: this one is AutoLink's.)
//
// USE: verify_autolink.exe <the level the editor opened> <the copy it saved> <a module's name> <same-set | same-branches | same-items>
// Prints the module's links in both (pre-order, "name/branch_no" as FUN_004c46c0 writes them), what only one holds, and whether the items are the same one for one. CHECKS: both levels are
// read by the game's loader; the module is in both; with same-set, the copy's links hold exactly the modules the original's do (the order and the branches may differ, and are printed); with
// same-branches, that and each module with a branch (a door: the modules seen through it) holds the same modules in its branch; with same-items, the items are the same, one for one; every
// OTHER module's links are item for item the original's (the scenario ran that one module). Gives back 0 only if every check passed.

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
#include "autolink_scene.h"
#include "module_links.h"

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

static std::string NameOf(std::map<int32_t, std::string> const &names, int32_t index)
{
	std::map<int32_t, std::string>::const_iterator const found = names.find(index);

	return found != names.end() ? found->second : "#" + std::to_string(index);
}

static std::string Words(std::vector<host::LinkItem> const &items, std::map<int32_t, std::string> const &names)
{
	std::string out;

	for (host::LinkItem const &item : items)
	{
		out += (out.empty() ? "" : " ") + NameOf(names, item.objectIndex) + "/" + std::to_string(item.branchNo);
	}

	return out.empty() ? "(none)" : out;
}

// Each module that has a branch, and the modules inside that branch (its nested branches too): FUN_004c46c0's branch_no is the item's 1-based place plus the nodes under it, i.e. the
// 0-based index of the first item after the branch, so item i holds the items i + 1 .. branch_no - 1 (FUN_004c40f0 reads branch_no - i - 1 of them). (Until 22:10 this stopped one item
// short: the last item of every branch was left out of the printed and compared sets.)
static std::map<std::string, std::set<std::string> > Branches(std::vector<host::LinkItem> const &items, std::map<int32_t, std::string> const &names)
{
	std::map<std::string, std::set<std::string> > out;

	for (size_t i = 0; i < items.size(); i++)
	{
		if (items[i].branchNo <= 0)
		{
			continue;
		}

		std::set<std::string> &inside = out[NameOf(names, items[i].objectIndex)];

		for (size_t j = i + 1; j < (size_t)items[i].branchNo && j < items.size(); j++)
		{
			inside.insert(NameOf(names, items[j].objectIndex));
		}
	}

	return out;
}

static std::string BranchWords(std::map<std::string, std::set<std::string> > const &branches)
{
	std::string out;

	for (auto const &one : branches)
	{
		std::string inside;

		for (std::string const &name : one.second)
		{
			inside += (inside.empty() ? "" : " ") + name;
		}

		out += (out.empty() ? "" : "; ") + one.first + ": [" + inside + "]";
	}

	return out.empty() ? "(none)" : out;
}

static bool SameItems(std::vector<host::LinkItem> const &a, std::vector<host::LinkItem> const &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i].objectIndex != b[i].objectIndex || a[i].branchNo != b[i].branchNo || a[i].flags != b[i].flags || a[i].spare != b[i].spare)
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		printf("verify_autolink: <original> <copy> <module name> <same-set | same-branches | same-items>\n");
		return 2;
	}

	std::string const name = argv[3], mode = argv[4];

	if (mode != "same-set" && mode != "same-items" && mode != "same-branches" && !(name == "*" && (mode == "report" || mode == "clean")))
	{
		printf("verify_autolink: the mode is same-set, same-branches or same-items (or * report, * clean), not %s\n", mode.c_str());
		return 2;
	}

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level() != nullptr, std::string("the game's loader reads the original: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	if (checksFailed)
	{
		printf("FAILED verify_autolink\n");
		return 1;
	}

	host::AutoLinkScene before, after;

	host::AutoLinkScene_Build(&original, 1.0, &before);
	host::AutoLinkScene_Build(&copy, 1.0, &after);

	// "* clean": Module Linking's start took out the links that name no module (FUN_004c90c0, row ML-E-12). The original has some; the copy has none; a module that had some keeps
	// exactly its other links (the same object numbers, in the same order); every other module's links are the original's, item for item.
	if (name == "*" && mode == "clean")
	{
		std::set<int32_t> modules;
		size_t danglingBefore = 0, danglingAfter = 0, keptWrong = 0, otherChanged = 0, touched = 0;
		std::string firstWrong;

		host::ModuleIndexes(original.Level(), &modules);

		for (int32_t const one : modules)
		{
			std::vector<host::LinkItem> a, b;
			std::vector<int32_t> wanted, got;
			size_t here = 0;

			host::ReadModuleLinks(original.Level(), one, &a);
			host::ReadModuleLinks(copy.Level(), one, &b);

			for (host::LinkItem const &item : a)
			{
				here += modules.count(item.objectIndex) ? 0 : 1;

				if (modules.count(item.objectIndex))
				{
					wanted.push_back(item.objectIndex);
				}
			}

			for (host::LinkItem const &item : b)
			{
				danglingAfter += modules.count(item.objectIndex) ? 0 : 1;
				got.push_back(item.objectIndex);
			}

			danglingBefore += here;

			if (here)
			{
				touched++;
				printf("  %s: the original's %s\n  %*s  the copy's     %s\n", NameOf(before.names, one).c_str(), Words(a, before.names).c_str(), (int)NameOf(before.names, one).size(), "",
					Words(b, after.names).c_str());

				if (wanted != got)
				{
					keptWrong++;
					firstWrong = firstWrong.empty() ? NameOf(before.names, one) : firstWrong;
				}
			}
			else if (!SameItems(a, b))
			{
				otherChanged++;
				firstWrong = firstWrong.empty() ? NameOf(before.names, one) : firstWrong;
			}
		}

		Check(danglingBefore > 0, "the original holds links that name no module: " + std::to_string(danglingBefore) + " in " + std::to_string(touched) + " modules");
		Check(danglingAfter == 0, "the copy holds none: " + std::to_string(danglingAfter));
		Check(keptWrong == 0, "each module that had some keeps exactly its other links, in order" + (keptWrong ? " (not " + firstWrong + ")" : std::string()));
		Check(otherChanged == 0, "every other module's links are the original's" + (otherChanged ? " (" + std::to_string(otherChanged) + " changed, the first " + firstWrong + ")" : std::string()));
		printf(checksFailed ? "FAILED verify_autolink\n" : "ok verify_autolink\n");
		return checksFailed ? 1 : 0;
	}

	// "* report": every module of the level, the copy's links against the original's (a measure, not a check: AutoLink > Environment against the stock links)
	if (name == "*")
	{
		std::set<int32_t> modules;
		size_t sameItems = 0, sameSet = 0, sameBranches = 0, withLinks = 0, missingAll = 0, extraAll = 0, shown = 0;

		host::ModuleIndexes(original.Level(), &modules);

		for (int32_t const one : modules)
		{
			std::vector<host::LinkItem> a, b;

			host::ReadModuleLinks(original.Level(), one, &a);
			host::ReadModuleLinks(copy.Level(), one, &b);

			if (a.empty() && b.empty())
			{
				continue;
			}

			withLinks++;

			std::set<std::string> setA, setB;

			for (host::LinkItem const &item : a)
			{
				setA.insert(NameOf(before.names, item.objectIndex));
			}

			for (host::LinkItem const &item : b)
			{
				setB.insert(NameOf(after.names, item.objectIndex));
			}

			std::string missing, extra;

			for (std::string const &n : setA)
			{
				missing += setB.count(n) ? "" : (missing.empty() ? "" : " ") + n;
				missingAll += setB.count(n) ? 0 : 1;
			}

			for (std::string const &n : setB)
			{
				extra += setA.count(n) ? "" : (extra.empty() ? "" : " ") + n;
				extraAll += setA.count(n) ? 0 : 1;
			}

			bool const items = SameItems(a, b), set = missing.empty() && extra.empty(), branches = set && Branches(a, before.names) == Branches(b, after.names);

			sameItems += items ? 1 : 0;
			sameSet += set ? 1 : 0;
			sameBranches += branches ? 1 : 0;

			if (!set && shown < 12)
			{
				shown++;
				printf("  %s: stock only [%s], ours only [%s]\n", NameOf(before.names, one).c_str(), missing.c_str(), extra.c_str());
			}
		}

		printf("  modules with links in either: %zu; the same modules: %zu; and the same branches: %zu; the same items one for one: %zu; links in the stock only: %zu; in ours only: %zu\n",
			withLinks, sameSet, sameBranches, sameItems, missingAll, extraAll);
		printf("ok verify_autolink report\n");
		return 0;
	}

	int32_t index = -1;

	for (auto const &one : before.names)
	{
		index = one.second == name && index < 0 ? one.first : index;
	}

	Check(index >= 0 && NameOf(after.names, index) == name, "the module " + name + " is in both (object " + std::to_string(index) + ")");

	if (checksFailed)
	{
		printf("FAILED verify_autolink\n");
		return 1;
	}

	std::vector<host::LinkItem> was, now;

	host::ReadModuleLinks(original.Level(), index, &was);
	host::ReadModuleLinks(copy.Level(), index, &now);
	printf("  the original's links of %s: %s\n", name.c_str(), Words(was, before.names).c_str());
	printf("  the copy's links of %s:     %s\n", name.c_str(), Words(now, after.names).c_str());

	std::set<std::string> wasSet, nowSet;

	for (host::LinkItem const &item : was)
	{
		wasSet.insert(NameOf(before.names, item.objectIndex));
	}

	for (host::LinkItem const &item : now)
	{
		nowSet.insert(NameOf(after.names, item.objectIndex));
	}

	std::string missing, extra;

	for (std::string const &one : wasSet)
	{
		missing += nowSet.count(one) ? "" : (missing.empty() ? "" : " ") + one;
	}

	for (std::string const &one : nowSet)
	{
		extra += wasSet.count(one) ? "" : (extra.empty() ? "" : " ") + one;
	}

	printf("  in the original only: %s\n  in the copy only: %s\n", missing.empty() ? "(none)" : missing.c_str(), extra.empty() ? "(none)" : extra.c_str());

	bool const itemsSame = SameItems(was, now);
	std::map<std::string, std::set<std::string> > const wasBranches = Branches(was, before.names), nowBranches = Branches(now, after.names);

	printf("  the items one for one: %s\n", itemsSame ? "the same" : "different (order or branches)");
	printf("  the original's branches: %s\n  the copy's branches:     %s\n", BranchWords(wasBranches).c_str(), BranchWords(nowBranches).c_str());

	if (mode == "same-items")
	{
		Check(itemsSame, "the copy's items are the original's, one for one");
	}
	else
	{
		Check(missing.empty() && extra.empty(), "the copy's links hold exactly the original's modules");
	}

	if (mode == "same-branches")
	{
		Check(wasBranches == nowBranches, "each module with a branch (a door: what is seen through it) holds the same modules in it as the original's");
	}

	// every other module untouched
	std::set<int32_t> modules;
	size_t changed = 0;
	std::string firstChanged;

	host::ModuleIndexes(original.Level(), &modules);

	for (int32_t const other : modules)
	{
		std::vector<host::LinkItem> a, b;

		if (other == index)
		{
			continue;
		}

		host::ReadModuleLinks(original.Level(), other, &a);
		host::ReadModuleLinks(copy.Level(), other, &b);

		if (!SameItems(a, b))
		{
			changed++;
			firstChanged = firstChanged.empty() ? NameOf(before.names, other) : firstChanged;
		}
	}

	Check(changed == 0, "every other module's links are the original's (" + std::to_string(modules.empty() ? 0 : modules.size() - 1) + " modules" +
		(changed ? ", " + std::to_string(changed) + " changed, the first " + firstChanged : std::string()) + ")");

	printf(checksFailed ? "FAILED verify_autolink\n" : "ok verify_autolink\n");
	return checksFailed ? 1 : 0;
}
