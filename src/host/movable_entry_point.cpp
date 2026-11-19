// movable_entry_point.cpp: movable.h's adapter for Waypoint mode's ENTRY POINT markers (entry_markers.h): a marker's place read from EntryMarkers_Of and written with
// EntryMarkers_Move, the writer Entry points > Fly's stop already uses (construction_modes_screen.cpp StopFlyingTheEntryPoint, inteng's FUN_0058a6b0). An entry point holds no
// turn. The write goes through the game's loader, which checks the changed level; nothing is changed if it refuses. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH ONE: ref.module is the picked module (its object index, as Waypoint mode counts it) and ref.number the marker's place in EntryMarkers_Of's list for it (inteng's order).
// THE PLACE THAT IS KEPT: the writer holds the new point inside the nearest module box of the other AI module and gives every entry of the pair that point (entry_markers.h),
// so a Get after a Set gives the place the entries now hold, which can differ from the one asked for. The list of markers is the level's again after a Set: the screen that
// keeps its own list makes it again (as the Fly's stop does).
// DECIDED P2-ENTRY-SAME: the place the marker already has is Done with nothing written (the writer itself would say NothingMoved, which here means no entry of the pair).

#include "movable.h"

#include <string.h>

#include <vector>

#include "entry_markers.h"

namespace host
{
	namespace
	{
		MovableResult Find(OpenLevelFile *file, MovableRef const &ref, EntryMarker *out)
		{
			if (!file)
			{
				return MovableResult::NothingOpen;
			}

			std::vector<EntryMarker> markers;

			if (ref.module < 0 || ref.number < 0 || !EntryMarkers_Of(file, ref.module, &markers) || (size_t)ref.number >= markers.size())
			{
				return MovableResult::NoSuchThing;
			}

			*out = markers[(size_t)ref.number];
			return MovableResult::Done;
		}
	}

	MovableResult MovableEntryPoint_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		EntryMarker marker;
		MovableResult const found = Find(file, ref, &marker);

		if (found != MovableResult::Done)
		{
			return found;
		}

		MovablePose read;

		memcpy(read.place, marker.point, sizeof(read.place));
		*pose = read;
		return MovableResult::Done;
	}

	MovableResult MovableEntryPoint_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		EntryMarker marker;
		MovableResult const found = Find(file, ref, &marker);

		if (found != MovableResult::Done)
		{
			return found;
		}

		if (memcmp(marker.point, pose.place, sizeof(marker.point)) == 0)
		{
			return MovableResult::Done;
		}

		size_t written = 0;

		switch (EntryMarkers_Move(file, marker, pose.place, &written))
		{
			case EntryMoveResult::Done: return MovableResult::Done;
			case EntryMoveResult::NoLevel: return MovableResult::NothingOpen;
			case EntryMoveResult::NothingMoved: return MovableResult::NoSuchThing;
			default: return MovableResult::WriterRefused;
		}
	}
}
