// waypoint_all.h: every AI module's alien waypoint list at once, as the level holds it, for THE WAYPOINT DISPLAY (the owner 11-19: "as I enter inteng to see and be able to
// edit all alien paths and volumes across all modules"; Vasquez draws it in waypoint_view.cpp). Written 2026-11-19 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// AS THE GAME'S LOADER TAKES THEM (avpx/3dc/avp/win95/projload.cpp 2497; Frost's waypoint_net_level.h reads them the same way): an AI module's list is its master's WAYPOINT chunk
// (or a lone module's: a module that is no other module's slave), its alien list; a slave's WAYPOINT is never read. The chunk is read by Waypoints_Parse (waypoint_model.h), which
// measures every count against the bytes.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "waypoint_model.h"

class OpenLevelFile;

namespace host
{
	struct AlienNet
	{
		int32_t master;                  // the AI module's master (or the lone module): its object index
		std::string name;                // its name
		std::vector<WaypointRec> alien;  // its alien list (the file's units)
	};

	// Every AI module's alien list as the level holds it: masters in the file's order, each master's name; a master with no WAYPOINT or no alien boxes is left out; an unreadable
	// one (a WAYPOINT Waypoints_Parse refuses) is left out and counted in *unreadable. Read once for each state of the level: the open, its level, Generation() (a level swapped in:
	// every waypoint edit that changes a chunk's size), Changes() (a same-size edit, which Generation() misses) and a fingerprint of its objects' indexes and names (a new Open starts
	// both counts at 0 again, its level maybe where the last one was); the same state gives the list already read, so a frame's call only walks the object list. No level open, or a
	// level ReadModuleSnapshots can't read: an empty list. One caller at a time (the editor's own thread).
	std::vector<AlienNet> const &WaypointAll_AlienNets(OpenLevelFile *open, size_t *unreadable = nullptr);

	// How many times WaypointAll_AlienNets has read a level (for the tests: a call with the same state reads nothing)
	size_t WaypointAll_Reads();
}
