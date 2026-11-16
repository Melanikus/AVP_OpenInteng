// The test for Exit + New / All Adjacencies (host::Adjacency_Generate, src/host/adjacency_generate.cpp: inteng's FUN_004c5a30) on REAL levels. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_adjacency_generate_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: "New" on a stock level (every module has an ADJMDLEP) renews no module; "All" is done and the game's loader takes it; every entry the level held with flags bit 0 (set by hand) is still
// in its module's list; the level is saved as a COPY, loaded again, holds what the run made; the opened file is byte for byte what it was. REPORTED (not a pass mark): how many of the stored
// adjacencies the run finds (RECALL) and how many it makes that the level did not hold (EXTRA), because the meet test is OUR port (autolink_portal.h) and the links are the level's own VMDARRAYs;
// how many entries have no entry back (inteng's rule is only partly symmetric); how many lists differ in a second run.

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
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "object_edit.h"
#include "adjacency_generate.h"

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

static double LevelScale(OpenLevelFile const &open)
{
	Chunk *const data = open.Level()->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = data && !data->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	Chunk *const scale = holder ? holder->lookup_single_child("ENVSDSCL") : nullptr;
	std::vector<unsigned char> bytes;
	double value = 1.0;

	if (scale && open.PayloadOf(scale, &bytes) && bytes.size() >= 8)
	{
		memcpy(&value, bytes.data(), 8);
	}

	return value > 0.0 ? value : 1.0;
}

typedef std::map<int32_t, std::vector<host::ModuleAdjacencyEntry> > Lists;

static Lists ListsOf(OpenLevelFile &level)
{
	Lists lists;
	std::vector<host::ModuleSnapshot> snapshots;

	host::ReadModuleSnapshots(&level, &snapshots);

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (snapshot.hasAdjacency && !lists.count(snapshot.index))
		{
			lists[snapshot.index] = snapshot.adjacency;
		}
	}

	return lists;
}

static std::set<int32_t> IndexesOf(std::vector<host::ModuleAdjacencyEntry> const &list)
{
	std::set<int32_t> out;

	for (host::ModuleAdjacencyEntry const &entry : list)
	{
		out.insert(entry.objectIndex);
	}

	return out;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_adjacency_generate_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_adjacency_generate_test (0 ok)\n");
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

	double const scale = LevelScale(level);
	Lists const before = ListsOf(level);
	size_t handSet = 0;

	for (Lists::const_iterator it = before.begin(); it != before.end(); ++it)
	{
		for (host::ModuleAdjacencyEntry const &entry : it->second)
		{
			handSet += (entry.flags & 1) ? 1 : 0;
		}
	}

	printf("  --  the level holds %zu adjacency lists, %zu entries set by hand\n", before.size(), handSet);

	// 1. New: a stock level's modules all have an ADJMDLEP, so nothing is listed that has one
	host::AdjacencyCounts onlyNew, everything;
	std::string why;
	host::AdjacencyResult const newResult = host::Adjacency_Generate(&level, true, scale, &onlyNew, &why);

	Check(newResult == host::AdjacencyResult::Done, std::string("Exit + New Adjacencies is done") + (newResult == host::AdjacencyResult::Done ? "" : ": " + why));
	printf("  --  New: %zu listed, %zu with no module data, %zu renewed, %zu made, %zu with no adjacent module, %zu reverse entries, %zu written\n", onlyNew.listed, onlyNew.noModuleData, onlyNew.renewed, onlyNew.made,
		onlyNew.noneFound, onlyNew.reverseAdded, onlyNew.written);
	Check(onlyNew.renewed == 0, "New renews no module (every module with an ADJMDLEP is left out of its list)");

	// 2. All
	DWORD const started = GetTickCount();
	host::AdjacencyResult const allResult = host::Adjacency_Generate(&level, false, scale, &everything, &why);
	DWORD const took = GetTickCount() - started;

	Check(allResult == host::AdjacencyResult::Done, std::string("Exit + All Adjacencies is done and the game's loader takes it") + (allResult == host::AdjacencyResult::Done ? "" : ": " + why));
	printf("  --  All: %zu listed, %zu with no module data, %zu renewed, %zu made (%zu entries), %zu with no adjacent module, %zu hand-set kept, %zu reverse entries, %zu written, %zu entry points worked out, %lu ms\n",
		everything.listed, everything.noModuleData, everything.renewed, everything.made, everything.entries, everything.noneFound, everything.keptByHand, everything.reverseAdded, everything.written,
		everything.recalculated, (unsigned long)took);

	Lists const after = ListsOf(level);
	size_t handKept = 0, handLost = 0, storedEntries = 0, found = 0, nowEntries = 0, extra = 0, asymmetric = 0;

	for (Lists::const_iterator it = before.begin(); it != before.end(); ++it)
	{
		Lists::const_iterator now = after.find(it->first);
		std::set<int32_t> const nowSet = now != after.end() ? IndexesOf(now->second) : std::set<int32_t>();

		for (host::ModuleAdjacencyEntry const &entry : it->second)
		{
			storedEntries++;
			found += nowSet.count(entry.objectIndex) ? 1 : 0;

			if (entry.flags & 1)
			{
				(nowSet.count(entry.objectIndex) ? handKept : handLost)++;
			}
		}
	}

	for (Lists::const_iterator it = after.begin(); it != after.end(); ++it)
	{
		Lists::const_iterator was = before.find(it->first);
		std::set<int32_t> const wasSet = was != before.end() ? IndexesOf(was->second) : std::set<int32_t>();

		for (host::ModuleAdjacencyEntry const &entry : it->second)
		{
			nowEntries++;
			extra += wasSet.count(entry.objectIndex) ? 0 : 1;

			Lists::const_iterator other = after.find(entry.objectIndex);

			asymmetric += (other != after.end() && IndexesOf(other->second).count(it->first) == 0) ? 1 : 0;
		}
	}

	printf("  --  THE MATCH with the stored adjacency: %zu stored entries, the run holds %zu of them (RECALL %.1f%%); it holds %zu entries, %zu not stored before (EXTRA %.1f%%); %zu entries have no entry back\n",
		storedEntries, found, storedEntries ? 100.0 * (double)found / (double)storedEntries : 0.0, nowEntries, extra, nowEntries ? 100.0 * (double)extra / (double)nowEntries : 0.0, asymmetric);
	Check(handLost == 0, "every entry set by hand that the level held is still in its list: " + std::to_string(handKept) + " kept");

	// 3. a second All: how many lists hold other modules than the first run's (reported)
	host::AdjacencyCounts again;

	Check(host::Adjacency_Generate(&level, false, scale, &again, &why) == host::AdjacencyResult::Done, "a second Exit + All Adjacencies is done");

	Lists const second = ListsOf(level);
	size_t differ = 0;

	for (Lists::const_iterator it = after.begin(); it != after.end(); ++it)
	{
		Lists::const_iterator two = second.find(it->first);

		differ += (two == second.end() || IndexesOf(two->second) != IndexesOf(it->second)) ? 1 : 0;
	}

	printf("  --  a second run: %zu of %zu lists differ in which modules they hold\n", differ, after.size());

	// 4. the copy
	std::string const copyFolder = argv[2];
	std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "adjacency-test-copy.rif";

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		Lists const reread = ListsOf(reopened);
		bool same = reread.size() == second.size();

		for (Lists::const_iterator it = second.begin(); it != second.end() && same; ++it)
		{
			Lists::const_iterator two = reread.find(it->first);

			same = two != reread.end() && two->second.size() == it->second.size();

			for (size_t i = 0; same && i < it->second.size(); i++)
			{
				same = two->second[i].objectIndex == it->second[i].objectIndex && two->second[i].flags == it->second[i].flags && memcmp(two->second[i].point, it->second[i].point, 12) == 0;
			}
		}

		Check(same, "the copy, read again, holds exactly the lists the run made (indexes, flags, entry points)");
	}

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");
	DeleteFileA(copyPath.c_str());

	if (checksFailed)
	{
		printf("FAILED host_adjacency_generate_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_adjacency_generate_test\n");
	return 0;
}
