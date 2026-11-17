// The check of Waypoint mode (Create Waypoint, Set Base, Add Link, Delete Waypoint, Add / Remove Can Crawl, run through the menu commands of host::WaypointMode), made by someone who
// built none of it (the non-builder's check for the Waypoint MVP). It runs on a REAL level (the program arguments after the folder: the first level that has an AI module whose master
// holds at least two waypoints in one list is used): each scenario is done on a freshly opened level, the level is SAVED AS A COPY and loaded again with the game's loader, and what the
// scenario promises is read back from the COPY's bytes. The point of this check is what must NOT change: every other module's adjacency, links and WAYPOINT chunk stay byte for byte as
// they were, the other list of the same module is the same, and the chunk-name counts (objects, shapes, modules, AI chunks) are the same. Written 2026-11-15 by Melanikus (Apone) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_check_test.exe <a folder it may write its copies into> <a level .rif> [more levels]. With fewer arguments it says so and passes. The levels are only read; the
// copies are the only files written.
//
// SCENARIOS: (1) Create Waypoint, Set Base, Step to another waypoint, Add Link: the copy has one more waypoint with room in its box, linked both ways with the one picked; (2) Create
// Waypoint then Delete Waypoint: the copy's WAYPOINT chunk holds the same waypoints as the original (fields and links); (3) Add Can Crawl then Remove Can Crawl on the current waypoint:
// the flag is set in the copy after the first, and the second puts the original back.

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

static host::ModuleSnapshot const *SnapshotOf(std::vector<host::ModuleSnapshot> const &all, int32_t index)
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

static bool SetOf(std::vector<host::ModuleSnapshot> const &all, int32_t index, host::WaypointSet *set)
{
	host::ModuleSnapshot const *snapshot = SnapshotOf(all, index);
	std::string why;

	*set = host::WaypointSet();
	return snapshot && snapshot->hasWaypoint && host::Waypoints_Parse(snapshot->waypoint.data(), snapshot->waypoint.size(), set, &why);
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

static bool SameList(std::vector<host::WaypointRec> const &a, std::vector<host::WaypointRec> const &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (!SameWaypoint(a[i], b[i]))
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

struct Context
{
	std::string levelPath, folder;
	std::vector<char> original;
	std::vector<size_t> counts;
	std::vector<host::ModuleSnapshot> snapshots;
	int32_t master;
	bool alien;          // the list the scenarios work on is the alien one
	host::WaypointSet before;
};

// Does one scenario on a fresh open, saves a COPY, loads it again: *after is the reloaded level, *snapshots what it holds. The always-true checks are made here.
static bool Run(Context const &c, std::string const &name, std::function<bool(OpenLevelFile &)> const &edit, OpenLevelFile *after, std::vector<host::ModuleSnapshot> *snapshots)
{
	printf("%s\n", name.c_str());

	OpenLevelFile level;

	if (level.Open(c.levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return false;
	}

	bool const done = edit(level);

	Check(done, "the commands say they are done");

	std::string const copyPath = CopyPathIn(c.folder, "waypoint-check-copy.rif");

	CreateDirectoryA(c.folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());

	if (level.SaveAs(copyPath.c_str()) != LevelSaveResult::Saved)
	{
		Check(false, "the level is saved as a COPY");
		return false;
	}

	std::vector<char> originalAfter;

	Check(ReadWholeFile(c.levelPath, &originalAfter) && originalAfter == c.original, "the opened file is byte for byte what it was");

	if (after->Open(copyPath.c_str()) != LevelOpenResult::Opened || !after->Level())
	{
		Check(false, "the game's loader takes the copy");
		DeleteFileA(copyPath.c_str());
		return false;
	}

	Check(true, "the game's loader takes the copy");

	std::vector<size_t> const counts = CountsOf(*after);

	DeleteFileA(copyPath.c_str());
	Check(counts == c.counts, "the chunk-name counts are as they were (objects, shapes, modules, AI chunks, adjacency, links, paths)");

	*snapshots = SnapshotsOf(*after);

	// every module but the master: adjacency, links and WAYPOINT chunk byte for byte
	bool others = snapshots->size() == c.snapshots.size();
	size_t checked = 0;

	for (size_t i = 0; others && i < snapshots->size(); i++)
	{
		host::ModuleSnapshot const &was = c.snapshots[i];
		host::ModuleSnapshot const &now = (*snapshots)[i];

		if (!was.module || was.index == c.master)
		{
			continue;
		}

		checked++;
		others = now.module && now.index == was.index && now.hasAdjacency == was.hasAdjacency && now.hasLinks == was.hasLinks && now.isMaster == was.isMaster && now.masterIndex == was.masterIndex
			&& now.hasWaypoint == was.hasWaypoint && now.waypoint == was.waypoint && now.adjacency.size() == was.adjacency.size() && now.links.size() == was.links.size();
	}

	Check(others, "every other module's adjacency, links, AI chunks and WAYPOINT chunk are as they were (" + std::to_string(checked) + " modules)");
	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_waypoint_check_test <a folder for its copies> <a level .rif> [more levels]: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	Context c;

	c.folder = argv[1];
	c.master = -1;
	c.alien = false;

	// the first level with a master that has two or more waypoints in one list
	for (int at = 2; at < argc && c.master < 0; at++)
	{
		OpenLevelFile level;

		if (level.Open(argv[at]) != LevelOpenResult::Opened || !level.Level())
		{
			continue;
		}

		std::vector<host::ModuleSnapshot> const snapshots = SnapshotsOf(level);

		for (host::ModuleSnapshot const &snapshot : snapshots)
		{
			host::WaypointSet set;

			if (!snapshot.module || !snapshot.isMaster || snapshot.masterIndex >= 0 || !SetOf(snapshots, snapshot.index, &set))
			{
				continue;
			}

			if (set.ground.size() >= 2 || set.alien.size() >= 2)
			{
				c.levelPath = argv[at];
				c.master = snapshot.index;
				c.alien = set.ground.size() < 2;
				c.before = set;
				c.snapshots = snapshots;
				break;
			}
		}
	}

	Check(c.master >= 0, "a level with an AI module whose master has two waypoints in a list was found among the arguments");

	if (c.master < 0)
	{
		return 1;
	}

	Check(ReadWholeFile(c.levelPath, &c.original), "the level is read (it is only read, never written): " + c.levelPath);

	{
		OpenLevelFile pristine;

		if (pristine.Open(c.levelPath.c_str()) != LevelOpenResult::Opened)
		{
			return 1;
		}

		c.counts = CountsOf(pristine);
	}

	std::vector<host::WaypointRec> const &was = c.alien ? c.before.alien : c.before.ground;
	std::vector<host::WaypointRec> const &otherListWas = c.alien ? c.before.ground : c.before.alien;
	int const showCommand = c.alien ? 3 : 4;
	size_t const n = was.size();

	printf("  (master module %d, the %s list, %zu waypoints in it, %zu in the other)\n", c.master, c.alien ? "alien" : "ground", n, otherListWas.size());

	// the commands that open the list: Edit Module (0x19) on the master, then Show Alien / Ground Waypoints
	auto open = [&](host::WaypointMode &mode, OpenLevelFile &level) -> bool
	{
		host::WaypointContext context;

		context.open = &level;
		context.pickedModule = c.master;
		mode.Enter();
		mode.Command(0x19, context);

		if (mode.CurrentModule() != c.master)
		{
			return false;
		}

		// inteng's Show Waypoints opens the PICKED module's AI module and Edit Module let the pick go (FUN_00583b40, 0x583e80; waypoint-closeout-vasquez.md W-8): a click on the master again
		mode.Click(&level, c.master);
		mode.Command(showCommand, context);
		return mode.Mode() == 1 && mode.List().size() == n;
	};

	// Create Waypoint as inteng (waypoint-closeout-vasquez.md W-5): an empty box at the master's place, into Edit Extents; a drag in the top view sizes x and y, one in the side view z (1000 each
	// way from the place), then Finish Extent Edit back to mode 1
	auto create = [&](host::WaypointMode &mode, host::WaypointContext const &context)
	{
		mode.Command(5, context);

		if (mode.Mode() != 2 || mode.CurrentWaypoint() < 0)
		{
			return;
		}

		host::WaypointRec const fresh = mode.List()[(size_t)mode.CurrentWaypoint()];
		int32_t const corner[3] = { fresh.min[0] + 1000, fresh.min[1] + 1000, fresh.min[2] + 1000 };

		mode.BeginDrag(fresh.min, 3, 10);
		mode.DragTo(corner);
		mode.EndDrag(context);
		mode.BeginDrag(fresh.min, 6, 10);
		mode.DragTo(corner);
		mode.EndDrag(context);
		mode.Command(0x13, context);
	};

	// 1. Create Waypoint, Set Base (the current one is cleared), Step to the first waypoint of the list, Add Link
	{
		OpenLevelFile after;
		std::vector<host::ModuleSnapshot> snapshots;

		if (Run(c, "CREATE WAYPOINT, SET BASE, STEP TO THE FIRST, ADD LINK", [&](OpenLevelFile &level)
		{
			host::WaypointMode mode;
			host::WaypointContext context;

			context.open = &level;
			context.pickedModule = c.master;

			if (!open(mode, level))
			{
				return false;
			}

			create(mode, context);
			mode.Command(7, context);
			mode.Step(1);
			host::WaypointOutcome const out = mode.Command(8, context);
			printf("  (Add Link says: %s; list %zu, base %d, current %d)\n", out.say.c_str(), mode.List().size(), (int)mode.BaseWaypoint(), (int)mode.CurrentWaypoint());

			return mode.List().size() == n + 1 && mode.BaseWaypoint() == (int32_t)n && mode.CurrentWaypoint() == 0 && out.say.find("Link between") != std::string::npos;
		}, &after, &snapshots))
		{
			host::WaypointSet set;

			Check(SetOf(snapshots, c.master, &set), "the copy's master still has a WAYPOINT chunk the library parses");

			std::vector<host::WaypointRec> const &list = c.alien ? set.alien : set.ground;
			std::vector<host::WaypointRec> const &other = c.alien ? set.ground : set.alien;

			Check(list.size() == n + 1, "one more waypoint in the list: " + std::to_string(n) + " to " + std::to_string(list.size()));
			Check(SameList(other, otherListWas), "the other list of the module is as it was");

			bool same = list.size() == n + 1;

			for (size_t i = 1; same && i < n; i++)
			{
				same = SameWaypoint(list[i], was[i]);
			}

			Check(same, "the waypoints other than the one linked are as they were (box, aim point, flags, links, module links)");

			if (list.size() == n + 1)
			{
				host::WaypointRec const &made = list[n];

				Check(made.max[0] > made.min[0] && made.max[1] > made.min[1] && made.max[2] > made.min[2], "the new waypoint's box has room on every axis (the game's loader would refuse it otherwise)");
				Check(HasLink(made, 0) && HasLink(list[0], (int32_t)n), "the new waypoint and the first one are linked both ways in the copy's bytes");
				Check(list[0].links.size() == was[0].links.size() + 1, "the first waypoint has exactly one link more");
				Check(memcmp(list[0].min, was[0].min, sizeof(was[0].min)) == 0 && memcmp(list[0].max, was[0].max, sizeof(was[0].max)) == 0 && list[0].flags == was[0].flags, "linking did not move or re-flag the first waypoint");
			}
		}
	}

	// 2. Create then Delete
	{
		OpenLevelFile after;
		std::vector<host::ModuleSnapshot> snapshots;

		if (Run(c, "CREATE WAYPOINT, then DELETE WAYPOINT", [&](OpenLevelFile &level)
		{
			host::WaypointMode mode;
			host::WaypointContext context;

			context.open = &level;
			context.pickedModule = c.master;

			if (!open(mode, level))
			{
				return false;
			}

			create(mode, context);

			bool const grew = mode.List().size() == n + 1 && mode.CurrentWaypoint() == (int32_t)n;

			mode.Command(6, context);
			return grew && mode.List().size() == n;
		}, &after, &snapshots))
		{
			host::WaypointSet set;

			Check(SetOf(snapshots, c.master, &set), "the copy's master has a WAYPOINT chunk");
			Check(SameList(c.alien ? set.alien : set.ground, was), "the list holds the waypoints it held, field for field and link for link");
			Check(SameList(c.alien ? set.ground : set.alien, otherListWas), "the other list is as it was");
		}
	}

	// 3. Add Can Crawl then Remove Can Crawl on the last waypoint of the list
	{
		OpenLevelFile after;
		std::vector<host::ModuleSnapshot> snapshots;

		if (Run(c, "ADD CAN CRAWL on the last waypoint", [&](OpenLevelFile &level)
		{
			host::WaypointMode mode;
			host::WaypointContext context;

			context.open = &level;
			context.pickedModule = c.master;

			if (!open(mode, level))
			{
				return false;
			}

			mode.Step(-1);
			return mode.CurrentWaypoint() == (int32_t)n - 1 && (mode.Command(0x16, context), true);
		}, &after, &snapshots))
		{
			host::WaypointSet set;

			Check(SetOf(snapshots, c.master, &set), "the copy's master has a WAYPOINT chunk");

			std::vector<host::WaypointRec> const &list = c.alien ? set.alien : set.ground;
			bool others = list.size() == n;

			for (size_t i = 0; others && i + 1 < n; i++)
			{
				others = SameWaypoint(list[i], was[i]);
			}

			Check(others, "the other waypoints are as they were");

			if (list.size() == n)
			{
				Check((list[n - 1].flags & host::kWaypointCanCrawl) != 0, "the last waypoint has Can Crawl in the copy's bytes");
				Check((list[n - 1].flags & ~host::kWaypointCanCrawl) == (was[n - 1].flags & ~host::kWaypointCanCrawl), "no other flag of it changed");
			}
		}

		if (Run(c, "ADD CAN CRAWL then REMOVE CAN CRAWL on the last waypoint", [&](OpenLevelFile &level)
		{
			host::WaypointMode mode;
			host::WaypointContext context;

			context.open = &level;
			context.pickedModule = c.master;

			if (!open(mode, level))
			{
				return false;
			}

			mode.Step(-1);
			mode.Command(0x16, context);
			mode.Command(0x17, context);
			return mode.CurrentWaypoint() == (int32_t)n - 1;
		}, &after, &snapshots))
		{
			host::WaypointSet set;

			Check(SetOf(snapshots, c.master, &set), "the copy's master has a WAYPOINT chunk");
			Check(SameList(c.alien ? set.alien : set.ground, was) || ((was[n - 1].flags & host::kWaypointCanCrawl) != 0 && (c.alien ? set.alien : set.ground).size() == n),
				"the list is as it was (when the waypoint had Can Crawl to begin with, Remove takes it off and nothing else changes)");
		}
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
