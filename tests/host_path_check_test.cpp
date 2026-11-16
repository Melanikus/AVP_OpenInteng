// The check of Path mode's data (Create Path and Delete Path as the menu commands do them: PathMode_Write on the route Add Module makes), made by someone who built none of it (the non-builder's check for the Path MVP).
// It runs on a REAL level (the program arguments after the folder: the first level that has a path already and a route of three modules): each scenario is done on a freshly opened level, the level is SAVED AS A COPY
// and loaded again with the game's loader, and what the scenario promises is read back from the COPY. The AVPPATH2 chunks are counted from the unpacked image's bytes, not through the code under test. What must NOT change is
// checked as hard as what must: every other chunk-name count, every module's adjacency / links / AI chunks / WAYPOINT chunk, every path the level had. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_path_check_test.exe <a folder it may write its copies into> <a level .rif> [more levels]. With fewer arguments it says so and passes. The levels are only read; the copies are the only files written.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
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
#include "path_mode.h"

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

static char const *const kNames[] = { "RBOBJECT", "REBSHAPE", "MODULEDT", "AIMODMAS", "AIMODSLA", "WAYPOINT", "ADJMDLEP", "VMDARRAY", "SPECLOBJ", "AVPPATH2" };
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


static bool SameNode(host::PathNode const &a, host::PathNode const &b)
{
	return a.module == b.module && memcmp(a.place, b.place, sizeof(a.place)) == 0 && a.flags == b.flags && a.spare == b.spare;
}

static bool SamePath(host::PathInfo const &a, host::PathInfo const &b)
{
	if (a.number != b.number || a.flags != b.flags || a.spare != b.spare || a.name != b.name || a.nodes.size() != b.nodes.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.nodes.size(); i++)
	{
		if (!SameNode(a.nodes[i], b.nodes[i]))
		{
			return false;
		}
	}

	return true;
}

struct Context
{
	std::string levelPath, folder;
	std::vector<char> original;
	std::vector<size_t> counts;
	std::vector<host::ModuleSnapshot> snapshots;
	std::vector<host::PathInfo> paths;
	std::vector<host::PathNode> route;   // the path the scenarios write
	int32_t newNumber;
};

// Does one scenario on a fresh open, saves a COPY, loads it again: *after is the reloaded level. The always-true checks are made here.
static bool Run(Context const &c, std::string const &name, std::function<bool(OpenLevelFile &)> const &edit, OpenLevelFile *after, size_t pathChunksExpected)
{
	printf("%s\n", name.c_str());

	OpenLevelFile level;

	if (level.Open(c.levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return false;
	}

	Check(edit(level), "the commands say they are done");

	std::string const copyPath = CopyPathIn(c.folder, "path-check-copy.rif");

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

	std::vector<size_t> counts = CountsOf(*after);

	DeleteFileA(copyPath.c_str());

	// the path chunks (the last name of the list) are the only count that moves, and by what the scenario says
	size_t const pathAt = kNameCount - 1;
	bool const pathCountOk = counts[pathAt] == pathChunksExpected;

	counts[pathAt] = c.counts[pathAt];
	Check(counts == c.counts, "every other chunk-name count is as it was (objects, shapes, modules, AI chunks, adjacency, links, SPECLOBJ)");
	Check(pathCountOk, "the AVPPATH2 chunks counted from the bytes: " + std::to_string(c.counts[pathAt]) + " to " + std::to_string(pathChunksExpected));

	// every module's adjacency, links, AI chunks and WAYPOINT chunk byte for byte
	std::vector<host::ModuleSnapshot> const snapshots = SnapshotsOf(*after);
	bool same = snapshots.size() == c.snapshots.size();

	for (size_t i = 0; same && i < snapshots.size(); i++)
	{
		host::ModuleSnapshot const &was = c.snapshots[i];
		host::ModuleSnapshot const &now = snapshots[i];

		same = now.module == was.module && now.index == was.index && now.hasAdjacency == was.hasAdjacency && now.hasLinks == was.hasLinks && now.isMaster == was.isMaster
			&& now.masterIndex == was.masterIndex && now.hasWaypoint == was.hasWaypoint && now.waypoint == was.waypoint && now.adjacency.size() == was.adjacency.size() && now.links.size() == was.links.size();
	}

	Check(same, "every module's adjacency, links, AI chunks and WAYPOINT chunk are as they were (" + std::to_string(snapshots.size()) + " objects)");
	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_path_check_test <a folder for its copies> <a level .rif> [more levels]: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	Context c;

	c.folder = argv[1];
	c.newNumber = -1;

	// the first level that has a path already and a module with a chain of three modules (a route of a, b, c) to build a path from
	host::PathGraph graph;
	std::vector<int32_t> numbers;
	std::vector<std::string> names;
	int start = -1, finish = -1;

	for (int at = 2; at < argc && c.levelPath.empty(); at++)
	{
		OpenLevelFile level;
		std::vector<host::PathInfo> paths;

		if (level.Open(argv[at]) != LevelOpenResult::Opened || !level.Level() || !host::PathMode_ReadPaths(&level, &paths) || paths.empty())
		{
			continue;
		}

		// each level starts from nothing: the graph and the lists are filled by the call, and what an earlier level left in them is not this level's
		graph = host::PathGraph();
		numbers.clear();
		names.clear();
		start = finish = -1;

		if (!host::PathMode_BuildGraph(&level, &graph, &numbers, &names))
		{
			continue;
		}

		// a module whose neighbour's neighbour is a third module: the route start..finish has a module between
		for (size_t a = 0; a < graph.neighbours.size() && finish < 0; a++)
		{
			for (int b : graph.neighbours[a])
			{
				for (int d : graph.neighbours[(size_t)b])
				{
					if (d != (int)a && d != b && std::find(graph.neighbours[a].begin(), graph.neighbours[a].end(), d) == graph.neighbours[a].end())
					{
						start = (int)a;
						finish = d;
						break;
					}
				}

				if (finish >= 0)
				{
					break;
				}
			}
		}

		if (finish >= 0)
		{
			c.levelPath = argv[at];
			c.paths = paths;
			c.snapshots = SnapshotsOf(level);
			host::PathMode_NextNumber(&level, &c.newNumber);
		}
	}

	Check(!c.levelPath.empty(), "a level with a path and a three-module route was found among the arguments");

	if (c.levelPath.empty())
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

	// the route, as the editor makes it: Add Module on the start, then on the finish (the route's modules between come with it)
	{
		std::vector<host::PathNode> path;

		Check(host::PathMode_AddModule(&path, graph, numbers, -1, start) && path.size() == 1, "Add Module on the first module starts the path with that one node");
		Check(host::PathMode_AddModule(&path, graph, numbers, start, finish) && path.size() >= 3, "Add Module on a module two steps away brings the module between: " + std::to_string(path.size()) + " nodes");

		bool distinct = !path.empty() && path.front().module == numbers[(size_t)start] && path.back().module == numbers[(size_t)finish];

		Check(distinct, "the path starts at the first module and ends at the one picked");
		c.route = path;
	}

	size_t const was = c.counts.back();

	// PathMode_NextNumber is inteng's own loop (FUN_004fb300): it scans the level's paths once in file order and never looks back, so on a level whose path numbers are not in order it can give a number a path
	// of the level already has (Breakout: 2), and the editor then REPLACES that path, as inteng does. This test is about adding, so it makes its path with a number no path uses, and says when the loop gave one that is used.
	{
		int32_t most = -1;
		bool used = false;

		for (host::PathInfo const &path : c.paths)
		{
			most = path.number > most ? path.number : most;
			used = used || path.number == c.newNumber;
		}

		if (used)
		{
			printf("  (inteng's next-free loop gives number %d, which a path of this level already has: the editor would replace that path; the test adds with number %d)\n", c.newNumber, most + 1);
			c.newNumber = most + 1;
		}
	}

	printf("  (%zu paths in the level, %zu AVPPATH2 in the bytes, the new path gets number %d, route of %zu nodes: modules %d to %d)\n", c.paths.size(), was, c.newNumber, c.route.size(), start, finish);
	Check(c.paths.size() == was, "the paths the library reads are as many as the AVPPATH2 chunks counted from the bytes");

	// 1. Create a path
	{
		OpenLevelFile after;

		if (Run(c, "CREATE PATH (a name, the route's nodes), written into the level", [&](OpenLevelFile &level)
		{
			return host::PathMode_Write(&level, c.newNumber, "AponeRoute", 0, 0, c.route.data(), c.route.size()) == host::PathModeResult::Done;
		}, &after, was + 1))
		{
			std::vector<host::PathInfo> paths;

			Check(host::PathMode_ReadPaths(&after, &paths), "the copy's paths are read");
			Check(paths.size() == c.paths.size() + 1, "one more path in the copy: " + std::to_string(c.paths.size()) + " to " + std::to_string(paths.size()));

			host::PathInfo const *made = nullptr;
			bool others = true;

			for (host::PathInfo const &path : paths)
			{
				if (path.number == c.newNumber)
				{
					made = &path;
				}
			}

			for (host::PathInfo const &old : c.paths)
			{
				bool found = false;

				for (host::PathInfo const &path : paths)
				{
					found = found || (path.number == old.number && SamePath(path, old));
				}

				others = others && found;
			}

			Check(others, "every path the level had is there, field for field and node for node");
			Check(made != nullptr && made->name == "AponeRoute" && made->nodes.size() == c.route.size(), "the new path has the name and as many nodes as were written");

			bool nodesGood = made != nullptr && made->nodes.size() == c.route.size();

			for (size_t i = 0; nodesGood && i < c.route.size(); i++)
			{
				nodesGood = made->nodes[i].module == c.route[i].module;
			}

			Check(nodesGood, "the nodes are the route's modules, in the order of the route");

			// the modules of the new path are modules of the level, and each is next to the one before it (the graph the route search uses)
			bool adjacent = nodesGood;

			for (size_t i = 1; adjacent && i < made->nodes.size(); i++)
			{
				int a = -1, b = -1;

				for (size_t m = 0; m < numbers.size(); m++)
				{
					a = numbers[m] == made->nodes[i - 1].module && a < 0 ? (int)m : a;
					b = numbers[m] == made->nodes[i].module && b < 0 ? (int)m : b;
				}

				adjacent = a >= 0 && b >= 0 && std::find(graph.neighbours[(size_t)a].begin(), graph.neighbours[(size_t)a].end(), b) != graph.neighbours[(size_t)a].end();
			}

			Check(adjacent, "each node's module is next to the one before it in the module graph the code reads from the level");
		}
	}

	// 2. Delete the level's first path (count 0)
	{
		OpenLevelFile after;
		host::PathInfo const &gone = c.paths[0];

		if (Run(c, "DELETE PATH (the level's first path, by number)", [&](OpenLevelFile &level)
		{
			return host::PathMode_Write(&level, gone.number, nullptr, 0, 0, nullptr, 0) == host::PathModeResult::Done;
		}, &after, was - 1))
		{
			std::vector<host::PathInfo> paths;

			Check(host::PathMode_ReadPaths(&after, &paths) && paths.size() + 1 == c.paths.size(), "one path fewer in the copy: " + std::to_string(c.paths.size()) + " to " + std::to_string(paths.size()));

			bool keptOthers = true;

			for (size_t i = 1; i < c.paths.size(); i++)
			{
				bool found = false;

				for (host::PathInfo const &path : paths)
				{
					found = found || (path.number == c.paths[i].number && SamePath(path, c.paths[i]));
				}

				keptOthers = keptOthers && found;
			}

			bool goneFound = false;

			for (host::PathInfo const &path : paths)
			{
				goneFound = goneFound || path.number == gone.number;
			}

			Check(keptOthers && !goneFound, "the others are as they were and the deleted number is gone");
		}
	}

	// 3. Create then delete the same path: the level's paths are as they were
	{
		OpenLevelFile after;

		if (Run(c, "CREATE PATH then DELETE IT by number", [&](OpenLevelFile &level)
		{
			return host::PathMode_Write(&level, c.newNumber, "AponeRoute", 0, 0, c.route.data(), c.route.size()) == host::PathModeResult::Done
				&& host::PathMode_Write(&level, c.newNumber, nullptr, 0, 0, nullptr, 0) == host::PathModeResult::Done;
		}, &after, was))
		{
			std::vector<host::PathInfo> paths;
			bool back = host::PathMode_ReadPaths(&after, &paths) && paths.size() == c.paths.size();

			for (size_t i = 0; back && i < paths.size(); i++)
			{
				back = SamePath(paths[i], c.paths[i]);
			}

			Check(back, "the level's paths are as they were, in the same order");
		}
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
