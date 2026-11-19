// movable_waypoint.cpp: movable.h's adapter for a WAYPOINT BOX of the alien list (the place only; Waypoints_Translate, new, then WaypointMode::Write).
// NOT BUILT YET: Dietrich (P4) writes it here, in this file only. Until then it answers NotBuilt, and the gizmo shows nothing for this kind. Stub written 2026-11-18 by
// Melanikus (Drake) for AVP_OpenInteng, with the interface. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "movable.h"

namespace host
{
	MovableResult MovableWaypoint_Get(OpenLevelFile *, MovableRef const &, MovablePose *)
	{
		return MovableResult::NotBuilt;
	}

	MovableResult MovableWaypoint_Set(OpenLevelFile *, MovableRef const &, MovablePose const &)
	{
		return MovableResult::NotBuilt;
	}
}
