// verify_waypoint_boxes: a saved copy against the level it was made from, through the game's own loader: which AI modules' alien boxes changed. For the waypoint display's editing
// across modules (Vasquez 2026-11-19, the owner's "see and be able to edit all alien paths and volumes across all modules"): a box moved in each of the named AI modules, and
// nothing else in any network. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_waypoint_boxes.exe <the saved copy> <the original> <master> [<master> ...]
// CHECKS: both levels read by the game's loader; the same AI modules hold alien networks, with the same box counts; in each named master EXACTLY ONE box differs (its min, max or aim
// point) and its links are the same; in every other master nothing differs at all. Prints which box of each changed and how. Gives back 0 when every check holds, 1 when one fails.
// The files are HOSTILE INPUT: read by the game's loader and Waypoints_Parse, which measures every count against the bytes.

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
#include "object_edit.h"
#include "waypoint_model.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
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
	exit(1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

namespace
{
	// every master's alien list, by its object index
	bool AlienLists(char const *path, std::map<int32_t, std::vector<host::WaypointRec>> *out)
	{
		OpenLevelFile level;

		if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
		{
			return false;
		}

		std::vector<host::ModuleSnapshot> snapshots;

		if (host::ReadModuleSnapshots(&level, &snapshots) != host::ObjectEditResult::Done)
		{
			return false;
		}

		for (host::ModuleSnapshot const &snapshot : snapshots)
		{
			host::WaypointSet set;
			std::string why;

			if (snapshot.module && snapshot.masterIndex < 0 && snapshot.hasWaypoint && host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why) &&
				!set.alien.empty())
			{
				(*out)[snapshot.index] = set.alien;
			}
		}

		return true;
	}

	bool SameLinks(host::WaypointRec const &a, host::WaypointRec const &b)
	{
		if (a.links.size() != b.links.size())
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

		return true;
	}

	bool SamePlace(host::WaypointRec const &a, host::WaypointRec const &b)
	{
		for (int k = 0; k < 3; k++)
		{
			if (a.min[k] != b.min[k] || a.max[k] != b.max[k] || a.centre[k] != b.centre[k])
			{
				return false;
			}
		}

		return true;
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 4)
	{
		printf("verify_waypoint_boxes: give the saved copy, the original, then one or more AI modules' masters\n");
		return 1;
	}

	std::map<int32_t, std::vector<host::WaypointRec>> copy, original;

	Check(AlienLists(argv[1], &copy), std::string("the game's loader reads the saved copy (") + argv[1] + ")");
	Check(AlienLists(argv[2], &original), std::string("and the original (") + argv[2] + ")");

	std::set<int32_t> named;

	for (int i = 3; i < argc; i++)
	{
		named.insert((int32_t)strtol(argv[i], nullptr, 10));
	}

	bool sameNetworks = copy.size() == original.size();

	for (std::pair<int32_t const, std::vector<host::WaypointRec>> const &each : original)
	{
		sameNetworks = sameNetworks && copy.count(each.first) && copy[each.first].size() == each.second.size();
	}

	Check(sameNetworks, "the same " + std::to_string(original.size()) + " AI modules hold alien networks, each with as many boxes as before");

	size_t othersChanged = 0;
	std::string firstOther;

	for (std::pair<int32_t const, std::vector<host::WaypointRec>> const &each : original)
	{
		std::vector<host::WaypointRec> const &before = each.second;
		std::vector<host::WaypointRec> const &after = copy[each.first];
		size_t moved = 0, relinked = 0;
		std::string how;

		for (size_t b = 0; b < before.size() && b < after.size(); b++)
		{
			if (!SamePlace(before[b], after[b]))
			{
				moved++;

				char line[256];

				snprintf(line, sizeof line, "box %zu: min %d %d %d max %d %d %d -> min %d %d %d max %d %d %d", b, before[b].min[0], before[b].min[1], before[b].min[2], before[b].max[0],
					before[b].max[1], before[b].max[2], after[b].min[0], after[b].min[1], after[b].min[2], after[b].max[0], after[b].max[1], after[b].max[2]);
				how += (how.empty() ? "" : "; ") + std::string(line);
			}

			relinked += SameLinks(before[b], after[b]) ? 0 : 1;
		}

		if (named.count(each.first))
		{
			Check(moved == 1 && relinked == 0, "AI module " + std::to_string(each.first) + ": exactly one box moved, links the same (" + (how.empty() ? std::string("none moved") : how) + ")");
		}
		else if (moved || relinked)
		{
			othersChanged++;
			firstOther = firstOther.empty() ? "AI module " + std::to_string(each.first) + ": " + how : firstOther;
		}
	}

	for (int32_t const master : named)
	{
		Check(original.count(master) != 0, "AI module " + std::to_string(master) + " holds an alien network");
	}

	Check(othersChanged == 0, "no other network changed" + (firstOther.empty() ? std::string() : " (" + firstOther + ")"));

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
