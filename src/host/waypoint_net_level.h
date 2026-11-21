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
		uint64_t waypointHash = 0;            // its master's WAYPOINT chunk's bytes, hashed (FNV-1a): the same as another file's = the same network (verify_waypoint_nets' stock)
	};

	// The level's scale (ENVSDSCL), 1 when it has none
	double WaypointNets_Scale(OpenLevelFile *open);

	// Every AI module of the level whose master holds alien waypoints, in the masters' order, each with its exits and its problems. False, with the reason, when the level cannot be read,
	// or a WAYPOINT chunk cannot be (the game's own reader refuses it too).
	bool WaypointNets_CheckLevel(OpenLevelFile *open, std::vector<WaypointNetModule> *out, std::string *why);

	// THE REPORT after an edit by the waypoint-box gizmo (waypoint_box_gizmo.h; Bishop's order 11-18: "a REPORT, never a refusal"): what the checker finds, as one part of the status line:
	// "checker: nothing found", "checker: box 14: its centre is outside it", or "checker: 5 found: <the first kReportNamed>; and 2 more". more: the problems past the ones kept.
	size_t const kWaypointReportNamed = 3;
	std::string WaypointNets_ReportWords(std::vector<WaypointProblem> const &problems, size_t more);

	// The report for the AI module whose master is `master` (an object index), from the level as it is now (the written copy, read as the stock loader reads it): "checker: no alien
	// waypoints" when its master holds none, "checker: the level could not be read: <why>" when the level cannot be
	std::string WaypointNets_ReportModule(OpenLevelFile *open, int32_t master);
}
