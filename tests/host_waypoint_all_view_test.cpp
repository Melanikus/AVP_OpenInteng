// The test for WaypointView_AllLines (src/host/waypoint_view.cpp): every AI module's alien network drawn at once (the owner's ask 11-19, a named difference). Written 2026-11-19 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_all_view_test.exe <a level .rif with alien networks> (Invasion: tests/host_waypoint_all_view_test.runargs). The level is only read.
// CHECKS, against every master's alien list as the level holds it (Waypoints_Parse): twelve white edges a box; a linked pair one line of two halves, drawn once; a half green, yellow when
// the link is aliens-only (inteng's highlighted colour), red when a one-way link may not be gone along from that side; the lines are APPENDED (what was in the list stays first); the
// counts; an empty list or a null one draws nothing; the level's scale applied.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>
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
#include "waypoint_view.h"

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
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

namespace
{
	// the pairs a list draws, and the halves of each colour they should give
	struct Expect
	{
		size_t pairs = 0;
		size_t green = 0, yellow = 0, red = 0;
	};

	Expect Expected(std::vector<host::WaypointRec> const &list)
	{
		Expect e;

		for (size_t w = 0; w < list.size(); w++)
		{
			for (host::WaypointLinkRec const &link : list[w].links)
			{
				if (link.index < 0 || (size_t)link.index >= list.size())
				{
					continue;
				}

				std::vector<host::WaypointLinkRec> const &back = list[(size_t)link.index].links;

				if ((size_t)link.index < w && std::any_of(back.begin(), back.end(), [&](host::WaypointLinkRec const &b) { return b.index == (int32_t)w; }))
				{
					continue;
				}

				e.pairs++;

				for (int32_t const closedFlag : { host::kLinkOneWayBack, host::kLinkOneWay })
				{
					if (link.flags & closedFlag)
					{
						e.red++;
					}
					else if (link.flags & host::kLinkAlien)
					{
						e.yellow++;
					}
					else
					{
						e.green++;
					}
				}
			}
		}

		return e;
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("host_waypoint_all_view_test: give a level .rif with alien networks\n");
		printf("0 checks, 1 failed\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		printf("1 checks, 1 failed\n");
		return 1;
	}

	std::vector<host::ModuleSnapshot> snapshots;
	std::vector<std::vector<host::WaypointRec>> lists;

	host::ReadModuleSnapshots(&level, &snapshots);

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		host::WaypointSet set;
		std::string why;

		if (snapshot.module && snapshot.masterIndex < 0 && snapshot.hasWaypoint && host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why) && !set.alien.empty())
		{
			lists.push_back(set.alien);
		}
	}

	size_t boxes = 0;
	Expect all;

	for (std::vector<host::WaypointRec> const &list : lists)
	{
		Expect const e = Expected(list);

		boxes += list.size();
		all.pairs += e.pairs;
		all.green += e.green;
		all.yellow += e.yellow;
		all.red += e.red;
	}

	printf("  --  %zu alien networks, %zu boxes, %zu linked pairs (halves: %zu green, %zu yellow, %zu red)\n", lists.size(), boxes, all.pairs, all.green, all.yellow, all.red);
	Check(lists.size() >= 3 && all.pairs > 0, "the level has alien networks with links to draw");

	std::vector<std::vector<host::WaypointRec> const *> pointers;

	for (std::vector<host::WaypointRec> const &list : lists)
	{
		pointers.push_back(&list);
	}

	std::vector<host::WaypointRec> const empty;

	pointers.push_back(&empty);   // an empty list and a null one draw nothing
	pointers.push_back(nullptr);

	// a line already in the list stays first: the lines are appended
	std::vector<host::TrackLine> lines(1);

	lines[0].colour = 0x12345678u;
	lines[0].from[0] = lines[0].from[1] = lines[0].from[2] = 7;
	lines[0].to[0] = lines[0].to[1] = lines[0].to[2] = 8;

	host::WaypointAllCounts const counts = host::WaypointView_AllLines(pointers, 1.0, &lines);

	Check(lines.size() == 1 + 12 * boxes + 2 * all.pairs, "twelve edges a box and two halves a pair, after the line that was there: " + std::to_string(lines.size() - 1) + " of " +
		std::to_string(12 * boxes + 2 * all.pairs));
	Check(lines[0].colour == 0x12345678u && lines[0].from[0] == 7 && lines[0].to[2] == 8, "the line that was there stays first, as it was");
	Check(counts.networks == lists.size() && counts.boxes == boxes && counts.links == all.pairs, "the counts: networks " + std::to_string(counts.networks) + ", boxes " +
		std::to_string(counts.boxes) + ", pairs " + std::to_string(counts.links));

	size_t white = 0, green = 0, yellow = 0, red = 0, other = 0;

	for (size_t i = 1; i < lines.size(); i++)
	{
		uint32_t const c = lines[i].colour;

		white += c == host::kWaypointBoxPlain ? 1 : 0;
		green += c == host::kWaypointLinkPlain ? 1 : 0;
		yellow += c == host::kWaypointLinkHighlighted ? 1 : 0;
		red += c == host::kWaypointLinkClosed ? 1 : 0;
		other += c != host::kWaypointBoxPlain && c != host::kWaypointLinkPlain && c != host::kWaypointLinkHighlighted && c != host::kWaypointLinkClosed ? 1 : 0;
	}

	Check(white == 12 * boxes, "every box edge white (inteng's plain box): " + std::to_string(white));
	Check(green == all.green && yellow == all.yellow && red == all.red && other == 0, "the halves green, YELLOW for an aliens-only link, red where a one-way link may not be gone along: " +
		std::to_string(green) + " / " + std::to_string(yellow) + " / " + std::to_string(red));

	// the first box's twelve edges span its min and max, in the world's units (the level's scale applied)
	std::vector<std::vector<host::WaypointRec> const *> first = { &lists[0] };
	std::vector<host::TrackLine> scaled;

	host::WaypointView_AllLines(first, 2.0, &scaled);

	host::WaypointRec const &box = lists[0][0];
	int32_t low[3] = { INT32_MAX, INT32_MAX, INT32_MAX }, high[3] = { INT32_MIN, INT32_MIN, INT32_MIN };

	for (size_t i = 0; i < 12 && i < scaled.size(); i++)
	{
		for (int k = 0; k < 3; k++)
		{
			low[k] = (std::min)(low[k], (std::min)(scaled[i].from[k], scaled[i].to[k]));
			high[k] = (std::max)(high[k], (std::max)(scaled[i].from[k], scaled[i].to[k]));
		}
	}

	bool spans = scaled.size() >= 12;

	for (int k = 0; k < 3; k++)
	{
		spans = spans && low[k] == (int32_t)(box.min[k] * 2.0) && high[k] == (int32_t)(box.max[k] * 2.0);
	}

	Check(spans, "the first box's edges span its min and max, the level's scale (2) applied");

	// mode 0 (closedRed false: inteng's red is the PICK there): no red line, a closed half in its link's own colour, the same number of lines
	std::vector<host::TrackLine> modeZero;

	host::WaypointView_AllLines(pointers, 1.0, &modeZero, false);

	size_t reds = 0;

	for (host::TrackLine const &line : modeZero)
	{
		reds += line.colour == host::kWaypointLinkClosed ? 1 : 0;
	}

	Check(modeZero.size() == lines.size() - 1 && reds == 0, "closedRed false: the same " + std::to_string(modeZero.size()) + " lines, none red");

	// nothing to draw: nothing appended
	std::vector<std::vector<host::WaypointRec> const *> none = { &empty, nullptr };
	std::vector<host::TrackLine> noLines;
	host::WaypointAllCounts const zero = host::WaypointView_AllLines(none, 1.0, &noLines);

	Check(noLines.empty() && zero.networks == 0 && zero.boxes == 0 && zero.links == 0, "empty and null lists draw nothing");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
