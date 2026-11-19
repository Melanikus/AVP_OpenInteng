// waypoint_net_level.h: the alien waypoint checker (waypoint_net_check.h) on a whole level: every AI module's alien list and its exit points, taken from the level as the stock game's loader
// takes them, and each checked. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng (the owner's alien auto-plot, AvP-docs/engine/inteng/reassess/alien-autoplot-design.md).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// AS THE LOADER TAKES THEM (avpx/3dc/avp/win95/projload.cpp):
//   - THE NETWORK: an AI module's is its master's WAYPOINT (or a lone module's), its alien list; a slave's WAYPOINT is not read (2497). A module with no alien waypoint has no network to check.
//   - THE EXITS: the AI module's ADJMDLEP entries, its master's and then its slaves', that lead to another AI module and have neither the inside-me nor the inside-it flag (bits 2 and 4);
//     the FIRST one to each neighbouring AI module only (2508-2598). Its point is where an NPC in this AI module heads for to go there. They are entry_markers.h's markers, kept one per
//     neighbour.
//   - THE SCALE: the first REBENVDT's ENVSDSCL, 1 when there is none (local_scale).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "waypoint_net_check.h"

class OpenLevelFile;

namespace host
{
	struct WaypointNetModule
	{
		int32_t master = -1;                  // the AI module's master (or the lone module): its object index
		size_t boxes = 0;                     // its alien list's boxes
		std::vector<WaypointPoint> exits;     // its exit points, one per neighbouring AI module
		std::vector<int32_t> exitTo;          // the neighbouring AI module (its master's object index) of each
		std::vector<WaypointProblem> problems;
		size_t more = 0;                      // problems found past the ones kept
	};

	// The level's scale (ENVSDSCL), 1 when it has none
	double WaypointNets_Scale(OpenLevelFile *open);

	// Every AI module of the level whose master holds alien waypoints, in the masters' order, each with its exits and its problems. False, with the reason, when the level cannot be read,
	// or a WAYPOINT chunk cannot be (the game's own reader refuses it too).
	bool WaypointNets_CheckLevel(OpenLevelFile *open, std::vector<WaypointNetModule> *out, std::string *why);
}
