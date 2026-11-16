#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
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


static bool SameWaypoint(host::WaypointRec const &a, host::WaypointRec const &b)
{
	if (memcmp(a.min, b.min, sizeof(a.min)) || memcmp(a.max, b.max, sizeof(a.max)) || memcmp(a.centre, b.centre, sizeof(a.centre)) || a.flags != b.flags || a.spare2 != b.spare2
		|| a.links.size() != b.links.size() || a.moduleLinks.size() != b.moduleLinks.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.links.size(); i++)
	{
		if (a.links[i].index != b.links[i].index || a.links[i].flags != b.links[i].flags)
		{
			return false;
		}
	}

	for (size_t i = 0; i < a.moduleLinks.size(); i++)
	{
		if (a.moduleLinks[i].name != b.moduleLinks[i].name || a.moduleLinks[i].flags != b.moduleLinks[i].flags)
		{
			return false;
		}
	}

	return true;
}

static bool HasLink(host::WaypointRec const &wp, int32_t to)
{
	for (host::WaypointLinkRec const &link : wp.links)
	{
		if (link.index == to)
		{
			return true;
		}
	}

	return false;
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

static bool SetOf(host::ModuleSnapshot const *snapshot, host::WaypointSet *set)
{
	std::string why;

	*set = host::WaypointSet();
	return snapshot && snapshot->hasWaypoint && host::Waypoints_Parse(snapshot->waypoint.data(), snapshot->waypoint.size(), set, &why);
}

// The check of the Waypoint scenario's copy (tests/scenarios/waypoint-main-job.txt): what a hidden editor run did through the menus and the views (Create Waypoint, Set Base, a click on waypoint 0, Add Link,
// the right face of waypoint 0's box dragged) read back from the COPY it saved, with the library and not through the editor's own code, against the level it opened. Only an AI module's WAYPOINT chunk may
// differ. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// USE: verify_waypoint <the opened level .rif> <the copy .rif> [dragged]. Gives back 0 when every check passed; "dragged" also expects waypoint 0's box to have grown on its high x face (and no other face).
int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  verify_waypoint <the opened level .rif> <the copy .rif> [dragged]: nothing to check\n");
		return 2;
	}

	bool const dragged = argc >= 4 && strcmp(argv[3], "dragged") == 0;
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

	// the modules whose WAYPOINT chunk differs
	std::vector<int32_t> changed;
	bool otherChunksSame = was.size() == now.size();

	for (size_t i = 0; i < was.size() && i < now.size(); i++)
	{
		if (!was[i].module)
		{
			continue;
		}

		otherChunksSame = otherChunksSame && now[i].module && was[i].index == now[i].index && was[i].hasAdjacency == now[i].hasAdjacency && was[i].hasLinks == now[i].hasLinks && was[i].isMaster == now[i].isMaster
			&& was[i].masterIndex == now[i].masterIndex && was[i].hasWaypoint == now[i].hasWaypoint && was[i].adjacency.size() == now[i].adjacency.size() && was[i].links.size() == now[i].links.size();

		if (was[i].waypoint != now[i].waypoint)
		{
			changed.push_back(was[i].index);
		}
	}

	Check(otherChunksSame, "every module's adjacency, links and AI chunks are as they were");
	Check(changed.size() == 1, "exactly one module's WAYPOINT chunk changed: " + std::to_string(changed.size()));

	if (changed.size() != 1)
	{
		return 1;
	}

	host::WaypointSet setBefore, setAfter;

	Check(SetOf(SnapshotAt(was, changed[0]), &setBefore) && SetOf(SnapshotAt(now, changed[0]), &setAfter), "the changed module's WAYPOINT chunk is read from both files with the library: module " + std::to_string(changed[0]));

	bool const alien = setAfter.alien.size() != setBefore.alien.size();
	std::vector<host::WaypointRec> const &listBefore = alien ? setBefore.alien : setBefore.ground;
	std::vector<host::WaypointRec> const &listAfter = alien ? setAfter.alien : setAfter.ground;
	size_t const n = listBefore.size();

	Check((alien ? setAfter.ground : setAfter.alien).size() == (alien ? setBefore.ground : setBefore.alien).size(), "the other list has as many waypoints");
	Check(listAfter.size() == n + 1, "the " + std::string(alien ? "alien" : "ground") + " list has one more waypoint: " + std::to_string(n) + " to " + std::to_string(listAfter.size()));

	if (listAfter.size() != n + 1 || n == 0)
	{
		return 1;
	}

	host::WaypointRec const &made = listAfter[n];

	Check(made.max[0] > made.min[0] && made.max[1] > made.min[1] && made.max[2] > made.min[2], "the new waypoint's box has room on every axis");
	Check(HasLink(made, 0) && HasLink(listAfter[0], (int32_t)n), "the new waypoint and waypoint 0 are linked both ways (Set Base, a click on waypoint 0, Add Link)");
	Check(listAfter[0].links.size() == listBefore[0].links.size() + 1 || HasLink(listBefore[0], (int32_t)n), "waypoint 0 has exactly one link more");

	// every other waypoint as it was
	bool others = true;

	for (size_t i = 1; i < n; i++)
	{
		others = others && SameWaypoint(listAfter[i], listBefore[i]);
	}

	Check(others, "waypoints 1 to " + std::to_string(n - 1) + " are as they were, field for field and link for link");

	// waypoint 0: its links but one are as they were; the box depends on the drag
	host::WaypointRec const &first = listAfter[0];
	host::WaypointRec const &firstWas = listBefore[0];

	if (!dragged)
	{
		Check(memcmp(first.min, firstWas.min, sizeof(first.min)) == 0 && memcmp(first.max, firstWas.max, sizeof(first.max)) == 0, "waypoint 0's box is as it was");
	}
	else
	{
		bool otherFaces = first.min[0] == firstWas.min[0] && first.min[1] == firstWas.min[1] && first.min[2] == firstWas.min[2] && first.max[1] == firstWas.max[1] && first.max[2] == firstWas.max[2];

		printf("  (waypoint 0's box before: %d %d %d to %d %d %d; after: %d %d %d to %d %d %d)\n", firstWas.min[0], firstWas.min[1], firstWas.min[2], firstWas.max[0], firstWas.max[1], firstWas.max[2], first.min[0], first.min[1], first.min[2], first.max[0], first.max[1], first.max[2]);
		Check(first.max[0] != firstWas.max[0], "the dragged face moved: waypoint 0's high x face is not where it was");
		Check(otherFaces, "the other five faces of waypoint 0's box did not move");
		Check(first.max[0] > first.min[0], "the box still has room on x");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
