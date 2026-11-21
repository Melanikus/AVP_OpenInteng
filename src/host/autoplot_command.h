// autoplot_command.h: WAYPOINT MODE'S AUTO-PLOT ALIEN WAYS (the owner's addition 11-18; Bishop's design AvP-docs/engine/inteng/reassess/alien-autoplot-design.md): mode 0's "Auto-plot AI
// module" (the picked module's AI module) and "Auto-plot all AI modules" (every one), our menu items (menu-status.json "extras" on Waypoint's first bar). Each AI module's goals are its exit
// points (its entry points, ADJMDLEP: where an NPC heads to leave it) and a few places a marine stands; AvPx's own pathfinder (autoplot_grid.h, Drake's) finds the ways between them, an
// alien's (floor is lava) and a marine's on the floor; the generator (autoplot_plan.h) makes them stock alien waypoints; the checker (waypoint_net_check.h, Frost's) refuses a network the
// stock game would choke on; what passes is WRITTEN at once over the AI module's alien list (WaypointMode::AutoPlotWrite: the game's loader checks it). Existing alien waypoints are only
// replaced after a Yes. The searches run on a worker (the grid takes one caller at a time); the frame shows the progress and writes on the editor's main thread.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <vector>

#include "autoplot_volumes.h"

namespace host
{
	int32_t const kWaypointAutoPlotModule = 0x7330;   // ours: the picked module's AI module
	int32_t const kWaypointAutoPlotAll = 0x7331;      // ours: every AI module
	int32_t const kWaypointAutoPlotSelected = 0x7332; // ours: the AI modules selected in Waypoint mode 0 (Vasquez's Ctrl+click set; the owner 11-19, through Bishop: "select multiple
	                                                  // modules and apply to them"), planned as one set so their lanes join across the doorways between them
	size_t const kAutoPlotMostSpotsPerAiModule = 6;   // marine stand spots added to the exits as goals
	size_t const kAutoPlotMostPairs = 40;             // goal pairs searched per AI module (each twice: alien and floor)
	int32_t const kAutoPlotStationReach = 6000;       // a climb station's wall at most this far from where the alien lands (Drake: chain A's side walls 3.5-3.9 m off the lane)
	uint32_t const kAutoPlotAirduct = 0x80000000u;    // MODULEFLAG_AIRDUCT, the bit Strategy mode's Add Flag > Airduct (0xe) sets (strategy_mode.h)
}

// Waypoint mode's command for one of the two (true when it was one of them); the rest of the run is AutoPlotCommand_Frame's. pickedModule: the picked module's object index as
// the Entry points markers take it (the view's pick, the selection, the mode's current module), -1 for none
bool AutoPlotCommand(int32_t command, int32_t pickedModule);

// Each frame on the Waypoint screen: the grid's and the worker's progress said, a finished run's networks checked and written
void AutoPlotCommand_Frame();

// A run is going (the grid building or the worker searching)
bool AutoPlotCommand_Busy();

// Auto-plot these modules' AI modules (any module's object index, each taken to the head of the AI module that holds it, once, in the order given), or EVERY AI module when the
// list is empty: the same run and status line as Auto-plot selected / all (Vasquez's "Alien auto waypoint" menu, the owner 11-20: "a drop down with environment or modules and
// accept a string of multiple modules"). What AutoPlotCommand gives back
bool AutoPlotCommand_Run(std::vector<int32_t> const &modules);

// The same, with the owner's counts for every AI module of the run (autoplot_volumes.h AutoPlotVolumeCounts: climb-ins and get-outs; Vasquez's plot dialog)
bool AutoPlotCommand_Run(std::vector<int32_t> const &modules, host::AutoPlotVolumeCounts const &counts);

// The counts of the run going (or the last one): the defaults unless AutoPlotCommand_Run gave them
host::AutoPlotVolumeCounts AutoPlotCommand_RunCounts();
