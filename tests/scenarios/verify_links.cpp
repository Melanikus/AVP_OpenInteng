#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
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
#include "waypoint_model.h"
#include "waypoint_mode.h"

#include "arg_number.h"

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

static char const *const kNames[] = { "RBOBJECT", "REBSHAPE", "MODULEDT", "AIMODMAS", "AIMODSLA", "WAYPOINT", "ADJMDLEP", "VMDARRAY", "SPECLOBJ" };
static size_t const kNameCount = sizeof(kNames) / sizeof(kNames[0]);

static std::vector<size_t> CountsOf(OpenLevelFile &level)
{
	std::vector<char> image;
	std::vector<size_t> counts(kNameCount, 0);

	if (level.BuildLevel(image))
	{
		for (size_t i = 0; i < kNameCount; i++)
		{
			counts[i] = CountName(image, kNames[i]);
		}
	}

	return counts;
}

static std::vector<host::ModuleSnapshot> SnapshotsOf(OpenLevelFile &level)
{
	std::vector<host::ModuleSnapshot> snapshots;

	host::ReadModuleSnapshots(&level, &snapshots);
	return snapshots;
}


static host::ModuleSnapshot const *SnapshotAt(std::vector<host::ModuleSnapshot> const &all, int32_t index)
{
	for (host::ModuleSnapshot const &snapshot : all)
	{
		if (snapshot.module && snapshot.index == index)
		{
			return &snapshot;
		}
	}

	return nullptr;
}

// The check of the Module Linking scenario's copy (tests/scenarios/module-linking-main-job.txt): what a hidden editor run did through clicks and menu items (Set Current on the module the camera is in, a pick and
// Link > Normal, a pick and Link > Branch, Exit) read back from the COPY it saved, with the library and not through the editor's own code, against the level it opened.
// Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// USE: verify_links <the opened level .rif> <the copy .rif> <how many links the current module gained>. Gives back 0 when every check passed.
int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 4)
	{
		printf("  verify_links <the opened level .rif> <the copy .rif> <links gained>: nothing to check\n");
		return 2;
	}

	// the count is bounded before it is used (arg_number.h)
	long gainedNumber = 0;

	if (!ArgNumber("how many links the current module gained", argv[3], 0, 100000, &gainedNumber))
	{
		return 2;
	}

	size_t const gained = (size_t)gainedNumber;
	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the editor's copy");

	if (checksFailed)
	{
		return 1;
	}

	std::vector<size_t> const countsBefore = CountsOf(before), countsAfter = CountsOf(after);

	Check(countsBefore == countsAfter, "the chunk-name counts are the same (objects, shapes, modules, AI chunks, adjacency, links, paths)");

	std::vector<host::ModuleSnapshot> const was = SnapshotsOf(before), now = SnapshotsOf(after);

	Check(was.size() == now.size(), "the level has as many objects: " + std::to_string(now.size()));

	std::vector<int32_t> changed;
	bool otherChunksSame = was.size() == now.size();

	for (size_t i = 0; i < was.size() && i < now.size(); i++)
	{
		if (!was[i].module)
		{
			continue;
		}

		otherChunksSame = otherChunksSame && now[i].module && was[i].index == now[i].index && was[i].hasAdjacency == now[i].hasAdjacency && was[i].isMaster == now[i].isMaster && was[i].masterIndex == now[i].masterIndex
			&& was[i].hasWaypoint == now[i].hasWaypoint && was[i].waypoint == now[i].waypoint && was[i].adjacency.size() == now[i].adjacency.size();

		bool linksSame = was[i].hasLinks == now[i].hasLinks && was[i].links.size() == now[i].links.size();

		for (size_t k = 0; linksSame && k < was[i].links.size(); k++)
		{
			linksSame = was[i].links[k].objectIndex == now[i].links[k].objectIndex && was[i].links[k].branchNo == now[i].links[k].branchNo && was[i].links[k].flags == now[i].links[k].flags && was[i].links[k].spare == now[i].links[k].spare;
		}

		if (!linksSame)
		{
			changed.push_back(was[i].index);
		}
	}

	Check(otherChunksSame, "every module's adjacency and AI chunks are as they were");
	Check(changed.size() == 1, "exactly one module's links changed (the current module): " + std::to_string(changed.size()));

	if (changed.size() != 1)
	{
		return 1;
	}

	host::ModuleSnapshot const *const oldOne = SnapshotAt(was, changed[0]);
	host::ModuleSnapshot const *const newOne = SnapshotAt(now, changed[0]);

	if (!oldOne || !newOne)
	{
		return 1;
	}

	printf("  (module %d: %zu links before, %zu after)\n", changed[0], oldOne->links.size(), newOne->links.size());
	Check(newOne->hasLinks && newOne->links.size() == oldOne->links.size() + gained, "the current module has " + std::to_string(gained) + " more links: " + std::to_string(oldOne->links.size()) + " to " + std::to_string(newOne->links.size()));

	// every old link target is still a target; the new targets are modules the module did not link before, and differ from each other
	std::set<int32_t> oldTargets, newTargets, added;

	for (host::ModuleLinkItem const &item : oldOne->links)
	{
		oldTargets.insert(item.objectIndex);
	}

	for (host::ModuleLinkItem const &item : newOne->links)
	{
		newTargets.insert(item.objectIndex);

		if (!oldTargets.count(item.objectIndex))
		{
			added.insert(item.objectIndex);
		}
	}

	bool keptOld = true;

	for (int32_t const target : oldTargets)
	{
		keptOld = keptOld && newTargets.count(target) != 0;
	}

	Check(keptOld, "every module the current module linked before is still linked");
	// a Branch Link may lead to a module the Normal Link before it already reached (the editor's tree allows it), so the new links lead to one or more new modules, never more than the links gained
	Check(added.size() >= 1 && added.size() <= gained, "the new links lead to " + std::to_string(added.size()) + " modules it did not link before (1 to " + std::to_string(gained) + ")");

	bool real = true;

	for (int32_t const target : added)
	{
		host::ModuleSnapshot const *const module = SnapshotAt(now, target);

		real = real && module != nullptr && target != changed[0];
	}

	Check(real, "each of them is another module of the level");
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
