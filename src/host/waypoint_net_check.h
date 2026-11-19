// waypoint_net_check.h: THE CHECKER of an AI module's alien waypoints (the owner's alien auto-plot, AvP-docs/engine/inteng/reassess/alien-autoplot-design.md, section 6). The stock game checks
// nothing when it loads waypoints, and some mistakes crash it or make its aliens stand still, so the editor checks a network before it is written. Written 2026-11-18 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A pure function: one AI module's alien list (waypoint_model.h: what its master's WAYPOINT holds, in the file's units) and the module's exit points, in, the problems out. No level, no
// window. Each rule is the stock game's own (avpx/3dc/avp/bh_waypoint.cpp, avp/win95/projload.cpp, 3dc/waypoint_room.h):
//   - TOO MANY: at most 99 boxes. The route search's ring has 100 places and goes wrong without a word when it wraps (bh_waypoint.cpp WAYVOLUME_QUEUE_LENGTH, 679-686, 825-830).
//   - NO ROOM: min <= max on every axis, and the room an alien aims for (waypoint_room.h WaypointRoom: three quarters of the box, after the loader's scale and its 1 off the highest
//     corner) is 1 or more on every axis.
//   - LARGE TARGET INTO A SMALL BOX: a link with 0x8 (linkflag_largetarget) sits in the list of the box it leads INTO (the step from V to W uses W's link to V: Global_Route.first_link),
//     and the stock game divides by that box's room there (GetTargetPositionInWaypoint): a room of 0 crashes it.
//   - LINK OUTSIDE THE LIST: a link's index must be one of the list's (stock reads past it into other memory).
//   - CANNOT REACH: an alien steps from V to W only when W lists V without 0x1 (the backwards search, bh_waypoint.cpp 752-776); every box must reach every other so (aliens take the
//     0x4 links too, Global_NPC_Type 1).
//   - EXIT OUTSIDE: every exit point of the module (the place an NPC heads for to leave it, waypoint_net_level.h) must lie in a box, or no route leads out.
//   - CENTRE OUTSIDE / CENTRE IN ANOTHER BOX: a box's centre (where aliens aim) must be in its own box and in no other (an alien that stands at a centre must be in that box alone).
// INSIDE A BOX is the game's own test (WaypointContainsPoint): min <= p <= max - 1 on each axis, every number first scaled as the loader scales it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "waypoint_model.h"

namespace host
{
	size_t const kWaypointsMostPerModule = 99;
	int32_t const kWaypointLinkOneWay = 1;        // linkflag_oneway
	int32_t const kWaypointLinkLargeTarget = 8;   // linkflag_largetarget

	enum class WaypointProblemKind
	{
		TooMany,              // box: -1
		NoRoom,               // box, axis (0 x, 1 y, 2 z)
		LargeTargetSmallBox,  // box (the one the 0x8 link is in, and leads into), axis
		LinkOutside,          // box, other (the index it gives)
		CannotReach,          // box, other: box cannot reach other (one of them is box 0)
		ExitOutside,          // other: the exit's place in the exits given
		CentreOutside,        // box
		CentreInAnother,      // box, other (the box its centre is also in)
		BadScale,             // the scale or a number the sums cannot use
	};

	struct WaypointProblem
	{
		WaypointProblemKind kind;
		int32_t box = -1;
		int32_t other = -1;
		int axis = -1;
		std::string words;    // for the status line and the reports
	};

	struct WaypointPoint
	{
		int32_t place[3];     // the file's units
	};

	// The problems of one AI module's alien list, in the order of the rules above, with every box's problems in the box's order. scale is the level's (local_scale); 1 for most.
	// Every problem is listed, so a list of several thousand problems is possible for a hostile list: at most `most` are kept (the rest counted in *more when it is given).
	std::vector<WaypointProblem> Waypoints_Check(std::vector<WaypointRec> const &alien, std::vector<WaypointPoint> const &exits, double scale, size_t most = 1000, size_t *more = nullptr);

	// The words of a kind, short ("too many boxes", "no room", ...): static text, never null
	char const *WaypointProblemName(WaypointProblemKind kind);
}
