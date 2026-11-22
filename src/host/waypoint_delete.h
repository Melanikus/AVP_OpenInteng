// waypoint_delete.h: DELETE WAYPOINTS (the owner 11-21, the last patch for alpha.3: "be able to select and delete Waypoint mapping by module, group of modules or
// envronment"; his choices: "Waypoint mode's bar", "Remove all, confirm says so"). Two items on Waypoint mode's bar, beside Edit / Add / Remove Module (which manage an
// AI module's members, not its waypoints): Delete Waypoints (the picked AI module, or the Ctrl-selected ones) and Delete All Waypoints (every AI module of the level).
// Each asks first, with the count. What goes is each AI module's ALIEN list, the one Alien auto waypoint writes. Its ground list (the marines' and predators' routes)
// is kept. An AI module left with neither list loses its WAYPOINT chunk, as Rebellion's own files have none for the modules with no network (measured on the 63 stock
// levels: tests/host_waypoint_delete_level_test.cpp; the write is WaypointMode::DeleteAlienLists).
// Nothing is lost until the file is saved. Pure here: which AI modules, how many waypoints, the words. The command is waypoint_delete_editor.cpp.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "object_edit.h"

namespace host
{
	int32_t const kWaypointDeleteWaypoints = 0x7336;      // ours: Waypoint mode's bar, "Delete Waypoints": the picked AI module, or the Ctrl-selected ones
	int32_t const kWaypointDeleteAllWaypoints = 0x7337;   // ours: "Delete All Waypoints": every AI module of the level

	struct WaypointDeleteHead
	{
		int32_t head = -1;        // the AI module's head (its master, or a module of none): the module whose WAYPOINT chunk holds its lists
		size_t alien = 0;         // its alien waypoints
		size_t ground = 0;        // its ground waypoints (kept)
	};

	struct WaypointDeletePlan
	{
		std::vector<WaypointDeleteHead> heads;   // the AI modules with alien waypoints to delete, in the file's order (or the selection's)
		size_t alien = 0;                        // their alien waypoints, all together
		size_t unreadable = 0;                   // AI modules whose WAYPOINT chunk could not be read: left as they are
	};

	// The AI modules to delete from, of the level's modules (ReadModuleSnapshots): all, every AI module; else the AI modules holding the modules named (the Ctrl set or the
	// pick), each once. Only those with an alien waypoint are kept
	WaypointDeletePlan WaypointDelete_Plan(std::vector<ModuleSnapshot> const &snapshots, std::vector<int32_t> const &modules, bool all);

	// The question asked first (the owner's "confirm says so"), and the end line. One AI module: by its number; more: how many. Rebellion's networks are hand-made, so
	// the question says that what goes may be his
	std::string WaypointDelete_Question(WaypointDeletePlan const &plan, bool all);
	std::string WaypointDelete_Done(size_t waypoints, size_t modules);

	// The command (waypoint_delete_editor.cpp, Waypoint mode's command list): false when it is not one of the two; pickedModule: the module picked in the view, -1 for none
	bool WaypointDeleteCommand(int32_t command, int32_t pickedModule);
}
