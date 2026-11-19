// movable_placed_object.cpp: movable.h's adapter for a PLACED OBJECT (a shape object, its OBJHEAD1): the place and the turn as the object's own header holds them,
// read with TrackObjectPlace and written with TrackSetObjectPose (track_edit.h), the writer a track's let-go already uses (inteng's FUN_004eff90 / FUN_004f0870). The
// write goes through the game's loader, which checks the changed level; nothing is changed if it refuses. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "movable.h"

#include "track_edit.h"

namespace host
{
	namespace
	{
		bool NamesSomething(MovableRef const &ref)
		{
			for (size_t i = 0; i < kSelectionIdBytes; i++)
			{
				if (ref.id[i] != 0)
				{
					return true;
				}
			}

			return false;
		}
	}

	MovableResult MovablePlacedObject_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		if (!NamesSomething(ref))
		{
			return MovableResult::NoSuchThing;
		}

		MovablePose read;

		if (!TrackObjectPlace(file, ref.id, read.place, read.turn))
		{
			return MovableResult::NoSuchThing;
		}

		*pose = read;
		return MovableResult::Done;
	}

	MovableResult MovablePlacedObject_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		if (!NamesSomething(ref))
		{
			return MovableResult::NoSuchThing;
		}

		switch (TrackSetObjectPose(file, ref.id, pose.place, pose.turn))
		{
			case TrackEditResult::Done: return MovableResult::Done;
			case TrackEditResult::NothingOpen: return MovableResult::NothingOpen;
			case TrackEditResult::NoName: case TrackEditResult::NoSuchObject: return MovableResult::NoSuchThing;
			default: return MovableResult::WriterRefused;
		}
	}
}
