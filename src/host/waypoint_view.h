// waypoint_view.h: the lines Waypoint mode's three views draw: each waypoint's box, the links between waypoints, the current and the base waypoint in their own colours. Written 2026-11-15
// by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// NAMED: inteng draws a waypoint as a box in the 3D view (FUN_00583130, FUN_00586e90); here the boxes and links are lines in the three orthographic views (three_view.h), the way the track
// screens show a track, and a waypoint is picked by a click in one of them. The colours are ours (inteng's are not read: the colour numbers it passes depend on its display mode).
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
}
