// waypoint_view.h: the lines Waypoint mode's three views draw: each waypoint's box, the links between waypoints, the current and the base waypoint in their own colours. Written 2026-11-15
// by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Modes 1 to 3 draw in the 3D view as inteng does (WaypointView_WorldLines below; Vasquez 11-17). WaypointView_Lines is for Edit Extents' three orthographic views (three_view.h) only, with
// our colours: inteng's three views (FUN_005846b0) are W-5 of reassess/waypoint-closeout-vasquez.md, to build.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "track_view.h"
#include "waypoint_mode.h"

namespace host
{
	uint32_t const kWaypointBoxColour = 0xFF40C040u;       // ours: a waypoint's box
	uint32_t const kWaypointCurrentColour = 0xFFFFE000u;   // the current waypoint
	uint32_t const kWaypointBaseColour = 0xFFFF4040u;      // the base waypoint
	uint32_t const kWaypointLinkColour = 0xFFFFFFFFu;      // a plain link
	uint32_t const kWaypointAlienLinkColour = 0xFF40E0FFu; // a link only aliens use
	uint32_t const kWaypointDeviationColour = 0xFFFFA000u; // a link that allows a deviation
	uint32_t const kWaypointHighlightColour = 0xFFFF40FFu;  // a link with the flag Highlight link flag chose

	// The lines of the active list (mode 1), in the world's units (the file's number times the level's scale), and the box that holds them all (*mins, *maxs; zeros for an empty list).
	// A one-way link is drawn from its waypoint to the middle of the way only. Gives the number of waypoints drawn.
	size_t WaypointView_Lines(WaypointMode const &mode, double levelScale, std::vector<TrackLine> *lines, int32_t mins[3], int32_t maxs[3]);

	// The world's place of a click's cursor as the file's units, for the picking (the inverse of the scale)
	void WaypointView_FileUnits(int32_t const world[3], double levelScale, int32_t file[3]);

	// THE 3D VIEW OF MODES 1 TO 3, as inteng draws it (Vasquez 2026-11-17, reassess/waypoint-closeout-vasquez.md W-2 / W-3; true colour):
	// - each waypoint of the active list is a wire box (FUN_00583130, from its min and max): the current one RED (0x585518), the base MAGENTA 0x7f007f (0x5851c0), the others 0xffffffff
	//   (white); with minimise on, every box but the current one's is a cube 20,000 of inteng's units each way round its aim point (FUN_00586e90), 200 of the world's (inteng's unit is the world's x 100);
	// - each link is one line of two halves between the two aim points (FUN_005835a0), made once for the pair; its colours FUN_00586570's (see the .cpp).
	// The other list's boxes are hidden (FUN_005876c0: +0x5c | 4), and so are the modules outside the AI module (the editor: FUN_00583b40).
	uint32_t const kWaypointBoxPlain = 0xFFFFFFFFu;         // (DAT_00d876d8 != 0) + -2 = -1
	uint32_t const kWaypointBoxCurrent = 0xFFFF0000u;       // (& 0xfeff06) + 0xfa
	uint32_t const kWaypointBoxBase = 0xFF7F007Fu;          // (& 0x7eff80) + 0xff
	uint32_t const kWaypointLinkPlain = 0xFF00FF00u;        // (& 0xfe03) + 0xfd
	uint32_t const kWaypointLinkHighlighted = 0xFFFFFF00u;  // (& 0xfffe04) + 0xfc
	uint32_t const kWaypointLinkClosed = 0xFFFF0000u;       // (& 0xfeff06) + 0xfa: the half a one-way link may not be gone along from
	int32_t const kWaypointMinimisedHalf = 200;             // 20,000 of inteng's units, in the world's
	int32_t const kWaypointAimMarkerHalf = 100;             // Centre Edit's marker: 10,000 of inteng's units each way
	uint32_t const kWaypointAimMarker = 0xFF00FF00u;        // (& 0xfe03) + 0xfd
	uint32_t const kWaypointModuleLink = 0xFF00FF00u;       // W-4: a module link's line, FUN_005835a0's own green on both halves (0x5837eb / 0x583808), no FUN_00586570 after

	void WaypointView_WorldLines(WaypointMode const &mode, double levelScale, std::vector<TrackLine> *lines);

	// Where the block of the waypoint's box is (its centre, the world's units): FUN_0046ddb0 measures the click to it (the pick of mode 1, FUN_00584ee0 0x58542d)
	void WaypointView_BoxCentre(WaypointMode const &mode, size_t index, double levelScale, int32_t out[3]);
}
