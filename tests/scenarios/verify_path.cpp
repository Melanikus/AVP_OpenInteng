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

// The check of the Path scenario's copy (tests/scenarios/path-main-job.txt): what a hidden editor run did through clicks and menu items (Create Path with a name, a click on a module and Add Module, the camera
// moved, another click and Add Module) read back from the COPY it saved, with the library and not through the editor's own code, against the level it opened. Written 2026-11-15 by Melanikus (Apone)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// USE: verify_path <the opened level .rif> <the copy .rif> <the new path's name> [the flags the path should hold]. Gives back 0 when every check passed.
int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 4)
	{
		printf("  verify_path <the opened level .rif> <the copy .rif> <the path's name>: nothing to check\n");
		return 2;
	}

	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the editor's copy");

	if (checksFailed)
	{
		return 1;
	}

	std::vector<size_t> countsBefore = CountsOf(before), countsAfter = CountsOf(after);
	size_t const pathAt = kNameCount - 1;   // AVPPATH2, counted from the bytes

	Check(countsAfter[pathAt] == countsBefore[pathAt] + 1, "one more AVPPATH2 chunk in the bytes: " + std::to_string(countsBefore[pathAt]) + " to " + std::to_string(countsAfter[pathAt]));
	countsBefore[pathAt] = countsAfter[pathAt] = 0;
	Check(countsBefore == countsAfter, "every other chunk-name count is the same (objects, shapes, modules, AI chunks, adjacency, links)");

	std::vector<host::ModuleSnapshot> const was = SnapshotsOf(before), now = SnapshotsOf(after);
	bool same = was.size() == now.size();

	for (size_t i = 0; same && i < was.size(); i++)
	{
		same = was[i].module == now[i].module && was[i].index == now[i].index && was[i].hasAdjacency == now[i].hasAdjacency && was[i].hasLinks == now[i].hasLinks && was[i].isMaster == now[i].isMaster
			&& was[i].masterIndex == now[i].masterIndex && was[i].hasWaypoint == now[i].hasWaypoint && was[i].waypoint == now[i].waypoint && was[i].adjacency.size() == now[i].adjacency.size() && was[i].links.size() == now[i].links.size();
	}

	Check(same, "every module's adjacency, links, AI chunks and WAYPOINT chunk are as they were (" + std::to_string(now.size()) + " objects)");

	std::vector<host::PathInfo> pathsBefore, pathsAfter;

	Check(host::PathMode_ReadPaths(&before, &pathsBefore) && host::PathMode_ReadPaths(&after, &pathsAfter), "the level's paths are read from both files");
	Check(pathsAfter.size() == pathsBefore.size() + 1, "one more path: " + std::to_string(pathsBefore.size()) + " to " + std::to_string(pathsAfter.size()));

	bool others = true;
	host::PathInfo const *made = nullptr;

	for (host::PathInfo const &path : pathsAfter)
	{
		bool old = false;

		for (host::PathInfo const &was : pathsBefore)
		{
			old = old || (was.number == path.number && SamePath(was, path));
		}

		if (!old)
		{
			if (made)
			{
				others = false;   // a second new path
			}

			made = &path;
		}
	}

	Check(others && made != nullptr, "every path the level had is there as it was, and exactly one is new");

	if (!made)
	{
		return 1;
	}

	printf("  (the new path: number %d, name \"%s\", %zu nodes)\n", made->number, made->name.c_str(), made->nodes.size());
	Check(made->name == argv[3], "the new path has the name that was typed");
	Check(made->nodes.size() >= 2, "it has at least two nodes (two modules picked, and the modules the route brings between them)");

	// each node is a module of the level, and each is next to the one before it in the module graph read from the copy
	host::PathGraph graph;
	std::vector<int32_t> numbers;
	std::vector<std::string> names;

	Check(host::PathMode_BuildGraph(&after, &graph, &numbers, &names), "the module graph is read from the copy");

	bool real = true, adjacent = true;
	int previous = -1;

	for (host::PathNode const &node : made->nodes)
	{
		int at = -1;

		for (size_t m = 0; m < numbers.size(); m++)
		{
			if (numbers[m] == node.module)
			{
				at = (int)m;
				break;
			}
		}

		real = real && at >= 0;

		if (previous >= 0 && at >= 0)
		{
			bool next = false;

			for (int const neighbour : graph.neighbours[(size_t)previous])
			{
				next = next || neighbour == at;
			}

			adjacent = adjacent && next;
		}

		previous = at;
	}

	Check(real, "every node's module is a module of the level");
	Check(adjacent, "each node's module is next to the one before it in the level's module graph");

	// the route is a SHORTEST one (the editor's route search is breadth first, FUN_004f8c10): between two nodes the person put in (flags bit 1 clear) the modules the route put in (bit 1 set) are as many as the breadth-first distance in the graph read from the copy
	// says, counted here by a walk of this program's own
	{
		auto const distance = [&](int from, int to)
		{
			std::vector<int> seen(numbers.size(), -1);
			std::vector<int> queue(1, from);

			seen[(size_t)from] = 0;

			for (size_t head = 0; head < queue.size(); head++)
			{
				for (int const next : graph.neighbours[(size_t)queue[head]])
				{
					if (next >= 0 && (size_t)next < seen.size() && seen[(size_t)next] < 0)
					{
						seen[(size_t)next] = seen[(size_t)queue[head]] + 1;
						queue.push_back(next);
					}
				}
			}

			return seen[(size_t)to];
		};

		bool shortest = true;
		int lastExplicit = -1;
		int route = 0;

		for (host::PathNode const &node : made->nodes)
		{
			int at = -1;

			for (size_t m = 0; m < numbers.size(); m++)
			{
				if (numbers[m] == node.module)
				{
					at = (int)m;
					break;
				}
			}

			if (at < 0)
			{
				continue;
			}

			if ((node.flags & 1) != 0)
			{
				route++;
				continue;
			}

			if (lastExplicit >= 0)
			{
				int const wanted = distance(lastExplicit, at);

				shortest = shortest && wanted >= 0 && route + 1 == wanted;
			}

			lastExplicit = at;
			route = 0;
		}

		Check(shortest, "between two modules the person picked, the modules the route put in make a shortest route in the module graph (a breadth-first walk of this program's own)");
	}

	if (argc >= 5)
	{
		int const wantFlags = atoi(argv[4]);

		Check(made->flags == wantFlags, "the path's flags are " + std::to_string(wantFlags) + " (" + std::to_string(made->flags) + ")");
	}
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
