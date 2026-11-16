// verify_adjacent: the check of the Adjacent Modules scenarios (tests/scenarios/adjacent-*.txt): the level the hidden editor run saved as a copy, read with the game's own loader. Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_adjacent.exe <the level the editor opened> <the copy it saved> delete <module> <member>   : the module's list lost exactly that member (the rest in their order, with the entries they had)
//      verify_adjacent.exe <the level the editor opened> <the copy it saved> reorder <module> <member> : the member was taken off and put on: last now, same entries
//      verify_adjacent.exe <the level the editor opened> <the copy it saved> add                      : exactly one module's list gained at least one module on the end (the old entries first, as they were)
// CHECKS (both): both levels are read by the game's loader; the number of objects, shapes, AI chunks, links and so on is what it was (only ADJMDLEP chunks may differ); exactly ONE module's adjacency
// changed; every other module's is byte for byte the same; in the copy every entry of the changed module is at the place Recalculate Entry Points works out (entry_recalc.h: nothing to change).
// Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "level_store.h"
#include "object_edit.h"
#include "entry_recalc.h"

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

// How many times a chunk's 8-letter name stands in the level's unpacked image, counted from the bytes
static size_t CountName(std::vector<char> const &image, char const *name)
{
	size_t count = 0;

	for (size_t at = 0; at + 8 <= image.size(); at++)
	{
		if (image[at] == name[0] && memcmp(image.data() + at, name, 8) == 0)
		{
			count++;
		}
	}

	return count;
}

static char const *const kNames[] = { "RBOBJECT", "REBSHAPE", "AIMODMAS", "AIMODSLA", "WAYPOINT", "VMDARRAY", "SPECLOBJ" };

static bool SameEntry(host::ModuleAdjacencyEntry const &a, host::ModuleAdjacencyEntry const &b)
{
	return a.objectIndex == b.objectIndex && a.flags == b.flags && a.point[0] == b.point[0] && a.point[1] == b.point[1] && a.point[2] == b.point[2];
}

static bool SameAdjacency(host::ModuleSnapshot const &a, host::ModuleSnapshot const &b)
{
	if (a.hasAdjacency != b.hasAdjacency || a.adjacency.size() != b.adjacency.size())
	{
		return false;
	}

	for (size_t e = 0; e < a.adjacency.size(); e++)
	{
		if (!SameEntry(a.adjacency[e], b.adjacency[e]))
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 4)
	{
		printf("usage: verify_adjacent <the level the editor opened> <the copy it saved> delete <module> <member> | add\n");
		return 2;
	}

	bool const deleting = strcmp(argv[3], "delete") == 0;
	bool const reordering = strcmp(argv[3], "reorder") == 0;
	bool const moved = strcmp(argv[3], "marker") == 0;
	int32_t const wantModule = (deleting || reordering) && argc >= 6 ? atoi(argv[4]) : -1;
	int32_t const wantMember = (deleting || reordering) && argc >= 6 ? atoi(argv[5]) : -1;
	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the editor's copy");

	if (!before.Level() || !after.Level())
	{
		return 1;
	}

	std::vector<char> imageBefore, imageAfter;

	Check(before.BuildLevel(imageBefore) && after.BuildLevel(imageAfter), "both levels are made into images");

	for (char const *name : kNames)
	{
		size_t const was = CountName(imageBefore, name);
		size_t const now = CountName(imageAfter, name);

		Check(was == now, std::string(name) + " chunks: " + std::to_string(was) + " before, " + std::to_string(now) + " after");
	}

	std::vector<host::ModuleSnapshot> was, now;

	Check(host::ReadModuleSnapshots(&before, &was) == host::ObjectEditResult::Done && host::ReadModuleSnapshots(&after, &now) == host::ObjectEditResult::Done, "the modules of both levels are read");
	Check(was.size() == now.size(), "the level has as many objects: " + std::to_string(now.size()));

	std::vector<size_t> changed;

	for (size_t i = 0; i < was.size() && i < now.size(); i++)
	{
		if (was[i].index != now[i].index || !SameAdjacency(was[i], now[i]))
		{
			changed.push_back(i);
		}
	}

	if (moved)
	{
		// an entry point marker was moved (Waypoint, Fly): the entries between two AI modules got one point and the hand-set bit; the lists themselves (the modules, their order) are as they were
		Check(!changed.empty(), "some modules' adjacency changed: " + std::to_string(changed.size()));

		size_t entriesChanged = 0;
		bool shape = true;
		bool flagged = true;
		int32_t point[3] = { 0, 0, 0 };
		bool same = true;
		bool first = true;

		for (size_t i : changed)
		{
			shape = shape && was[i].adjacency.size() == now[i].adjacency.size();

			for (size_t e = 0; shape && e < was[i].adjacency.size(); e++)
			{
				shape = was[i].adjacency[e].objectIndex == now[i].adjacency[e].objectIndex;

				if (!SameEntry(was[i].adjacency[e], now[i].adjacency[e]))
				{
					entriesChanged++;
					flagged = flagged && (now[i].adjacency[e].flags & 1) != 0 && (now[i].adjacency[e].flags | 1) == (was[i].adjacency[e].flags | 1);

					if (first)
					{
						point[0] = now[i].adjacency[e].point[0]; point[1] = now[i].adjacency[e].point[1]; point[2] = now[i].adjacency[e].point[2];
						first = false;
					}

					printf("  info  module %d entry to module %d: point %d %d %d -> %d %d %d, flags %x -> %x\n", (int)was[i].index, (int)was[i].adjacency[e].objectIndex, was[i].adjacency[e].point[0], was[i].adjacency[e].point[1], was[i].adjacency[e].point[2],
						now[i].adjacency[e].point[0], now[i].adjacency[e].point[1], now[i].adjacency[e].point[2], (unsigned)was[i].adjacency[e].flags, (unsigned)now[i].adjacency[e].flags);
				}
			}
		}

		Check(shape, "every changed module has the same modules in the same order");
		Check(entriesChanged >= 1, "entries changed: " + std::to_string(entriesChanged));
		Check(flagged, "every entry that changed has the hand-set bit and its other flags as they were");
		(void)same;
		printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
		return checksFailed ? 1 : 0;
	}

	Check(changed.size() == 1, "exactly one module's adjacency changed: " + std::to_string(changed.size()));

	if (changed.size() != 1)
	{
		return 1;
	}

	host::ModuleSnapshot const &oldOne = was[changed[0]];
	host::ModuleSnapshot const &newOne = now[changed[0]];

	printf("  info  module %d: %zu adjacent modules before, %zu after\n", (int)oldOne.index, oldOne.adjacency.size(), newOne.adjacency.size());

	if (reordering)
	{
		// the member was taken off and put on again: it is now the LAST one, and every module has the entry (flags and point) it had, the put-on one's worked out again to the same place
		Check(oldOne.index == wantModule, "the module that changed is the current one the scenario made: " + std::to_string(wantModule));

		std::vector<host::ModuleAdjacencyEntry> expected;
		host::ModuleAdjacencyEntry moved = { 0, 0, { 0, 0, 0 } };
		bool hadIt = false;

		for (host::ModuleAdjacencyEntry const &entry : oldOne.adjacency)
		{
			if (entry.objectIndex == wantMember)
			{
				hadIt = true;
				moved = entry;
			}
			else
			{
				expected.push_back(entry);
			}
		}

		expected.push_back(moved);
		Check(hadIt, "the module that was taken off and put on was on the list: " + std::to_string(wantMember));
		Check(newOne.adjacency.size() == expected.size() && expected.size() >= 2 && expected.back().objectIndex == wantMember, "the list is as long as before with that module on the end");

		bool same = newOne.adjacency.size() == expected.size();

		for (size_t e = 0; same && e < expected.size(); e++)
		{
			same = SameEntry(expected[e], newOne.adjacency[e]);
		}

		Check(same, "every module has the entry it had: the put-on one's flags and point, worked out again, are the stock level's");
	}
	else if (deleting)
	{
		Check(oldOne.index == wantModule, "the module that changed is the current one the scenario made: " + std::to_string(wantModule));

		std::vector<host::ModuleAdjacencyEntry> expected;
		bool hadIt = false;

		for (host::ModuleAdjacencyEntry const &entry : oldOne.adjacency)
		{
			if (entry.objectIndex == wantMember)
			{
				hadIt = true;
			}
			else
			{
				expected.push_back(entry);
			}
		}

		Check(hadIt, "the module taken off the list was on it: " + std::to_string(wantMember));
		Check(newOne.hasAdjacency == !expected.empty() && newOne.adjacency.size() == expected.size(), "the list is the old one without it: " + std::to_string(expected.size()) + " modules");

		bool same = newOne.adjacency.size() == expected.size();

		for (size_t e = 0; same && e < expected.size(); e++)
		{
			same = SameEntry(expected[e], newOne.adjacency[e]);
		}

		Check(same, "the others come in their order with the entry (flags and point) they had");
	}
	else
	{
		bool grew = newOne.adjacency.size() > oldOne.adjacency.size();
		bool oldFirst = grew;

		for (size_t e = 0; oldFirst && e < oldOne.adjacency.size(); e++)
		{
			oldFirst = oldOne.adjacency[e].objectIndex == newOne.adjacency[e].objectIndex;
		}

		Check(grew, "the list got longer");
		Check(oldFirst, "the old modules come first in their order, the new ones on the end");

		for (size_t e = oldOne.adjacency.size(); grew && e < newOne.adjacency.size(); e++)
		{
			host::ModuleAdjacencyEntry const &entry = newOne.adjacency[e];

			printf("  info  new entry for module %d: flags %x point %d %d %d\n", (int)entry.objectIndex, (unsigned)entry.flags, entry.point[0], entry.point[1], entry.point[2]);
		}
	}

	// every entry of the changed module is where Recalculate Entry Points puts it
	host::LevelStore const store(&after);
	std::vector<host::EntryChange> changes;
	host::EntryRecalcCounts counts = {};

	Check(host::PlanEntryRecalc(after.Level(), &store, &changes, &counts), "Recalculate Entry Points is planned on the copy");

	size_t mine = 0;

	for (host::EntryChange const &change : changes)
	{
		mine += change.mainIndex == newOne.index ? 1 : 0;
	}

	Check(mine == 0, "in the copy every entry of the changed module is at its computed place: " + std::to_string(mine) + " would change");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
