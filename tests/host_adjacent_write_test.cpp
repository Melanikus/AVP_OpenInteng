// The test for Adjacent Modules' WRITE-BACK (src/host/adjacent_write.*, inteng's FUN_00404040): the list of a module's adjacent modules goes into its ADJMDLEP chunk, an entry the old chunk has is
// kept, a new one gets its entry point worked out, an empty list takes the chunk out. Run on a REAL level (the first argument); the level is saved as a COPY (never the opened file) and loaded again
// with the game's own loader. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_adjacent_write_test.exe <a level .rif> <a folder it may write its copy into (made if it isn't there)>. With no arguments it says so and passes.
//
// WHAT IT CHECKS (against reads made here from the file, not against the code under test):
//  1. DELETE: the last member of a module's list taken off: the chunk in the written level names the others in the same order, the other modules' adjacency is as it was;
//  2. a second write-back of the same list changes nothing it worked out (every entry is already at its computed place: the entry-point side is stable);
//  3. ADD: a module that touches the current one put on the end: the chunk has one entry more, the old ones first in their order, the new one has its flags and point worked out (not 0 and 0);
//  4. EMPTY: an empty list takes the chunk out; written again nothing is to take out; the other modules are as they were;
//  5. the written level is saved as a copy, loaded again by the game's loader and read the same; the opened file is byte for byte what it was;
//  6. hostile: no level, a module that is none, a member that names no object: refused or harmless, never a broken level.

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
#include "adjacent_write.h"

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

static std::vector<host::ModuleSnapshot> Snapshots(OpenLevelFile *open)
{
	std::vector<host::ModuleSnapshot> all;

	if (host::ReadModuleSnapshots(open, &all) != host::ObjectEditResult::Done)
	{
		all.clear();
	}

	return all;
}

static host::ModuleSnapshot const *Find(std::vector<host::ModuleSnapshot> const &all, int32_t index)
{
	for (host::ModuleSnapshot const &one : all)
	{
		if (one.index == index)
		{
			return &one;
		}
	}

	return nullptr;
}

// A list made on the spot is gone when its line ends, and a pointer Find gave into it would be read after it was freed: that was this test's crash
// (exit 139 after its 31 checks, 11-18 to 11-27; the address sanitizer found it reading freed memory in the INFO block at the end). So such a list
// can't be given to Find at all: keep it in a list with a name first. (Added 2026-11-27 by Melanikus (Drake).)
static host::ModuleSnapshot const *Find(std::vector<host::ModuleSnapshot> const &&all, int32_t index) = delete;

static std::vector<int32_t> Members(host::ModuleSnapshot const &one)
{
	std::vector<int32_t> out;

	for (host::ModuleAdjacencyEntry const &entry : one.adjacency)
	{
		out.push_back(entry.objectIndex);
	}

	return out;
}

static bool SameEntry(host::ModuleAdjacencyEntry const &a, host::ModuleAdjacencyEntry const &b)
{
	return a.objectIndex == b.objectIndex && a.flags == b.flags && a.point[0] == b.point[0] && a.point[1] == b.point[1] && a.point[2] == b.point[2];
}

// Every module but `except` has the adjacency it had
static bool OthersAsTheyWere(std::vector<host::ModuleSnapshot> const &before, std::vector<host::ModuleSnapshot> const &after, int32_t except)
{
	if (before.size() != after.size())
	{
		return false;
	}

	for (size_t i = 0; i < before.size(); i++)
	{
		if (before[i].index == except)
		{
			continue;
		}

		if (before[i].index != after[i].index || before[i].hasAdjacency != after[i].hasAdjacency || before[i].adjacency.size() != after[i].adjacency.size())
		{
			return false;
		}

		for (size_t e = 0; e < before[i].adjacency.size(); e++)
		{
			if (!SameEntry(before[i].adjacency[e], after[i].adjacency[e]))
			{
				return false;
			}
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_adjacent_write_test <a level .rif> <a folder for its copy>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	std::string const levelPath = argv[1];
	std::string const copyFolder = argv[2];
	std::vector<char> original;

	Check(ReadWholeFile(levelPath, &original), "the level is read (it is only read, never written)");

	OpenLevelFile open;

	if (original.empty() || open.Open(levelPath.c_str()) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	std::vector<host::ModuleSnapshot> const start = Snapshots(&open);

	// the module worked on: one with at least 3 adjacent modules, whose neighbours are all modules
	host::ModuleSnapshot const *mine = nullptr;

	for (host::ModuleSnapshot const &one : start)
	{
		if (one.module && one.hasAdjacency && one.adjacency.size() >= 3)
		{
			mine = &one;
			break;
		}
	}

	Check(mine != nullptr, "the level has a module with at least 3 adjacent modules");

	if (!mine)
	{
		return 1;
	}

	int32_t const current = mine->index;
	std::vector<int32_t> const oldList = Members(*mine);

	printf("  info  module %d, %zu adjacent modules\n", current, oldList.size());

	for (int32_t const member : oldList)
	{
		printf("  info  module %d is on its list\n", (int)member);
	}

	// 1. DELETE the last member
	{
		std::vector<int32_t> list = oldList;

		list.pop_back();

		host::AdjacentWriteCounts counts;
		host::AdjacentWriteResult const result = host::AdjacentWriteBack(&open, current, list, &counts);

		Check(result == host::AdjacentWriteResult::Done, std::string("1. the list without its last member is written: ") + host::AdjacentWriteWords(result));

		std::vector<host::ModuleSnapshot> const after = Snapshots(&open);
		host::ModuleSnapshot const *const now = Find(after, current);

		Check(now && now->hasAdjacency && Members(*now) == list, "1. the chunk names the others in the same order");
		Check(counts.kept == list.size() && counts.made == 0 && counts.entries == list.size(), "1. every entry was taken over from the old chunk: " + std::to_string(counts.kept) + " kept");
		Check(OthersAsTheyWere(start, after, current), "1. every other module's adjacency is as it was");

		// 2. again: nothing is worked out any more
		host::AdjacentWriteCounts again;

		Check(host::AdjacentWriteBack(&open, current, list, &again) == host::AdjacentWriteResult::Done, "2. the same list is written again");
		Check(again.recalculated == 0, "2. no entry point changes the second time: " + std::to_string(again.recalculated));

		std::vector<host::ModuleSnapshot> const twice = Snapshots(&open);
		host::ModuleSnapshot const *const stays = Find(twice, current);

		Check(stays && now && stays->adjacency.size() == now->adjacency.size(), "2. the chunk has the same number of entries");

		bool same = stays && now;

		for (size_t e = 0; same && e < now->adjacency.size(); e++)
		{
			same = SameEntry(now->adjacency[e], stays->adjacency[e]);
		}

		Check(same, "2. the entries are byte for byte what the first write-back left");
	}

	// 3. ADD a module that touches: try the neighbours of the module's neighbours until one gets an entry point
	{
		std::vector<host::ModuleSnapshot> const before = Snapshots(&open);
		host::ModuleSnapshot const *const now = Find(before, current);
		std::vector<int32_t> const base = Members(*now);
		std::vector<int32_t> candidates(1, oldList.back());   // the module taken off in 1. goes back first: its stock entry is the oracle

		for (int32_t const member : base)
		{
			host::ModuleSnapshot const *const other = Find(before, member);

			for (size_t e = 0; other && e < other->adjacency.size(); e++)
			{
				int32_t const candidate = other->adjacency[e].objectIndex;
				bool listed = candidate == current;

				for (int32_t const known : base)
				{
					listed = listed || known == candidate;
				}

				for (int32_t const known : candidates)
				{
					listed = listed || known == candidate;
				}

				const host::ModuleSnapshot *const target = Find(before, candidate);

				if (!listed && target && target->module)
				{
					candidates.push_back(candidate);
				}
			}
		}

		for (host::ModuleSnapshot const &other : before)
		{
			bool listed = !other.module || other.index == current;

			for (int32_t const known : base)
			{
				listed = listed || known == other.index;
			}

			for (int32_t const known : candidates)
			{
				listed = listed || known == other.index;
			}

			if (!listed)
			{
				candidates.push_back(other.index);
			}
		}

		Check(candidates.size() > 1, "3. there are modules to try: " + std::to_string(candidates.size()));

		bool worked = false;
		int32_t added = -1;

		for (size_t c = 0; c < candidates.size() && c < 400 && !worked; c++)
		{
			std::vector<int32_t> list = base;

			list.push_back(candidates[c]);

			host::AdjacentWriteCounts counts;

			if (host::AdjacentWriteBack(&open, current, list, &counts) != host::AdjacentWriteResult::Done)
			{
				continue;
			}

			std::vector<host::ModuleSnapshot> const after = Snapshots(&open);
			host::ModuleSnapshot const *const written = Find(after, current);

			if (written && written->adjacency.size() == list.size() && counts.made == 1 && (written->adjacency.back().flags != 0 || written->adjacency.back().point[0] != 0 || written->adjacency.back().point[1] != 0 || written->adjacency.back().point[2] != 0))
			{
				worked = true;
				added = candidates[c];

				bool oldFirst = Members(*written).size() == list.size();

				for (size_t e = 0; oldFirst && e < list.size(); e++)
				{
					oldFirst = written->adjacency[e].objectIndex == list[e];
				}

				Check(oldFirst, "3. module " + std::to_string(added) + " is on the end, the old ones first in their order");
				Check(counts.kept == base.size() && counts.made == 1, "3. " + std::to_string(counts.kept) + " kept, " + std::to_string(counts.made) + " new");
				if (added == oldList.back())
				{
					host::ModuleAdjacencyEntry const &stock = mine->adjacency.back();

					printf("  info  the module taken off in 1. is back: stock entry flags %x point %d %d %d\n", (unsigned)stock.flags, stock.point[0], stock.point[1], stock.point[2]);
					Check(SameEntry(stock, written->adjacency.back()), "3. the entry worked out for the module that was taken off is the stock level's entry");
				}

				printf("  info  the new entry: flags %x, point %d %d %d (%zu entries worked out different)\n", (unsigned)written->adjacency.back().flags, written->adjacency.back().point[0], written->adjacency.back().point[1], written->adjacency.back().point[2], counts.recalculated);
				Check(OthersAsTheyWere(before, after, current), "3. every other module's adjacency is as it was");

				host::AdjacentWriteCounts again;

				Check(host::AdjacentWriteBack(&open, current, list, &again) == host::AdjacentWriteResult::Done && again.recalculated == 0, "3. the new entry is at its computed place: a second write-back changes none");
			}
		}

		Check(worked, "3. a module put on the list gets its flags and entry point worked out");
	}

	// 5. the written level goes through the game's loader as a copy
	{
		std::vector<host::ModuleSnapshot> const edited = Snapshots(&open);
		std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "adjacent-write-copy.rif";

		CreateDirectoryA(copyFolder.c_str(), NULL);
		DeleteFileA(copyPath.c_str());
		Check(open.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "5. the level is saved as a COPY: " + copyPath);

		OpenLevelFile copy;

		Check(copy.Open(copyPath.c_str()) == LevelOpenResult::Opened && copy.Level(), "5. the game's loader takes the copy");

		std::vector<host::ModuleSnapshot> const reread = Snapshots(&copy);

		Check(OthersAsTheyWere(edited, reread, -12345) && edited.size() == reread.size(), "5. every module's adjacency in the copy is what the editor held");

		std::vector<char> originalAfter;

		Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "5. the opened file is byte for byte what it was");
		DeleteFileA(copyPath.c_str());
	}

	// 4. EMPTY list takes the chunk out
	{
		std::vector<host::ModuleSnapshot> const before = Snapshots(&open);
		host::AdjacentWriteCounts counts;
		host::AdjacentWriteResult const result = host::AdjacentWriteBack(&open, current, std::vector<int32_t>(), &counts);

		Check(result == host::AdjacentWriteResult::Done && counts.removed, "4. an empty list is written: the chunk is taken out");

		std::vector<host::ModuleSnapshot> const after = Snapshots(&open);
		host::ModuleSnapshot const *const now = Find(after, current);

		Check(now && !now->hasAdjacency, "4. the module has no ADJMDLEP any more");
		Check(OthersAsTheyWere(before, after, current), "4. every other module's adjacency is as it was");

		host::AdjacentWriteCounts again;

		Check(host::AdjacentWriteBack(&open, current, std::vector<int32_t>(), &again) == host::AdjacentWriteResult::Done && !again.removed, "4. written again there is nothing to take out");
	}

	// 6. hostile
	{
		host::AdjacentWriteCounts counts;
		std::vector<int32_t> one(1, oldList[0]);

		Check(host::AdjacentWriteBack(nullptr, current, one, &counts) == host::AdjacentWriteResult::NothingOpen, "6. no level: refused");
		Check(host::AdjacentWriteBack(&open, -5, one, &counts) == host::AdjacentWriteResult::NoSuchModule, "6. a module number that is none: refused");
		Check(host::AdjacentWriteBack(&open, 1 << 30, one, nullptr) == host::AdjacentWriteResult::NoSuchModule, "6. a huge module number: refused, no counts asked");

		std::vector<char> imageBefore;
		std::vector<char> imageAfter;
		std::vector<int32_t> odd(1, 0x7ffffff0);

		Check(open.BuildLevel(imageBefore), "6. the level's image is made");

		host::AdjacentWriteResult const result = host::AdjacentWriteBack(&open, current, odd, &counts);

		Check(open.BuildLevel(imageAfter), "6. the level's image is made after a member that names no object (" + std::string(host::AdjacentWriteWords(result)) + ")");

		if (result != host::AdjacentWriteResult::Done)
		{
			Check(imageBefore == imageAfter, "6. refused: the level is what it was");
		}
		else
		{
			std::vector<host::ModuleSnapshot> const after = Snapshots(&open);

			Check(!after.empty() && Find(after, current) != nullptr, "6. taken: the level still reads");
		}
	}

	// INFO for the scenarios: modules that touch the current one although they are not on its list, found by trying them
	{
		std::vector<host::ModuleSnapshot> const nowAll = Snapshots(&open);
		size_t found = 0;

		for (host::ModuleSnapshot const &other : nowAll)
		{
			bool stockNeighbour = false;

			for (int32_t const known : oldList)
			{
				stockNeighbour = stockNeighbour || known == other.index;
			}

			if (!other.module || other.index == current || found >= 4 || stockNeighbour)
			{
				continue;
			}

			std::vector<int32_t> list(1, other.index);
			host::AdjacentWriteCounts counts;

			if (host::AdjacentWriteBack(&open, current, list, &counts) == host::AdjacentWriteResult::Done)
			{
				std::vector<host::ModuleSnapshot> const afterThisOne = Snapshots(&open);
				host::ModuleSnapshot const *const written = Find(afterThisOne, current);

				if (written && written->adjacency.size() == 1 && (written->adjacency[0].flags != 0 || written->adjacency[0].point[0] != 0 || written->adjacency[0].point[2] != 0))
				{
					printf("  info  module %d touches module %d: flags %x point %d %d %d\n", (int)other.index, (int)current, (unsigned)written->adjacency[0].flags, written->adjacency[0].point[0], written->adjacency[0].point[1], written->adjacency[0].point[2]);
					found++;
				}
			}
		}
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
