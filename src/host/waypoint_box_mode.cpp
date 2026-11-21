// waypoint_box_mode.cpp: WaypointMode's two calls for the waypoint-box gizmo (waypoint_mode.h MouseShowBox, MouseSetBox; waypoint_box_gizmo.h), in a file of their own so the gizmo's
// work never edits waypoint_mode.cpp. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_mode.h"

#include <string.h>

#include "level_file.h"

namespace host
{
	namespace
	{
		WaypointOutcome BoxSaid(std::string const &words)
		{
			WaypointOutcome out;

			out.say = words;
			out.page = -1;
			out.leaveScreen = false;
			out.levelChanged = false;
			out.askExtents = false;
			return out;
		}
	}

	bool WaypointMode::MouseShowBox(int32_t index, WaypointRec const &box)
	{
		if (mode != 1 || !alien || index < 0 || (size_t)index >= set.alien.size())
		{
			return false;
		}

		WaypointRec &wp = set.alien[(size_t)index];

		memcpy(wp.min, box.min, sizeof(wp.min));
		memcpy(wp.max, box.max, sizeof(wp.max));
		memcpy(wp.centre, box.centre, sizeof(wp.centre));
		wp.flags = box.flags;
		return true;
	}

	WaypointOutcome WaypointMode::MouseSetBox(OpenLevelFile *open, int32_t index, WaypointRec const &box, std::string const &what)
	{
		if (mode != 1 || !alien || !open || !open->Level() || current < 0 || index < 0 || (size_t)index >= set.alien.size())
		{
			return BoxSaid("A waypoint's box is moved by mouse on the alien list of an AI module only");
		}

		WaypointSet const backup = set;
		std::string why;

		MouseShowBox(index, box);

		if (!Write(open, &why))
		{
			set = backup;
			return BoxSaid("Waypoint " + std::to_string((long long)index) + ": not written: " + why);
		}

		WaypointOutcome out = BoxSaid("Waypoint " + std::to_string((long long)index) + ": " + what);

		out.levelChanged = true;
		return out;
	}
}
