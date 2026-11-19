// movable.cpp: see movable.h. The kind table, the checks every pose goes through, and the call to the kind's adapter. Written 2026-11-18 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "movable.h"

#include <math.h>
#include <string.h>

namespace host
{
	namespace
	{
		struct KindRow
		{
			char const *name;
			bool canRotate;
			bool inLevel;   // the pose is in the level's bytes (Movable_PoseInLevel)
			MovableResult (*get)(OpenLevelFile *, MovableRef const &, MovablePose *);
			MovableResult (*set)(OpenLevelFile *, MovableRef const &, MovablePose const &);
		};

		// In MovableKind's order (the static_assert below holds it to the enum's length)
		KindRow const kKinds[] =
		{
			{ "nothing", false, true, nullptr, nullptr },
			{ "placed object", true, true, MovablePlacedObject_Get, MovablePlacedObject_Set },
			{ "placed hierarchy", true, true, MovablePlacedHierarchy_Get, MovablePlacedHierarchy_Set },
			{ "player start", true, true, MovableSpecial_Get, MovableSpecial_Set },
			{ "particle generator", true, true, MovableSpecial_Get, MovableSpecial_Set },
			{ "sound", false, true, MovableSpecial_Get, MovableSpecial_Set },
			{ "AVP generator", false, true, MovableSpecial_Get, MovableSpecial_Set },
			{ "bad guy", false, true, MovableSpecial_Get, MovableSpecial_Set },
			{ "virtual object", false, true, MovableVirtualObject_Get, MovableVirtualObject_Set },
			{ "light", true, true, MovableLight_Get, MovableLight_Set },
			{ "entry point", false, true, MovableEntryPoint_Get, MovableEntryPoint_Set },
			{ "waypoint box", false, false, MovableWaypoint_Get, MovableWaypoint_Set },
			{ "track section end", true, false, MovableTrackEnd_Get, MovableTrackEnd_Set },
		};

		static_assert(sizeof(kKinds) / sizeof(kKinds[0]) == (size_t)MovableKind::Count, "one row of kKinds for every MovableKind");

		KindRow const *RowOf(MovableKind kind)
		{
			size_t const k = (size_t)kind;

			return k > 0 && k < (size_t)MovableKind::Count ? &kKinds[k] : nullptr;
		}

		// A float that is a real number (not infinite, not "not a number"), judged by its bits: /fp:fast lets the compiler assume no number is ever NaN
		bool RealNumber(float value)
		{
			uint32_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7f800000u) != 0x7f800000u;
		}
	}

	char const *Movable_ResultWords(MovableResult result)
	{
		switch (result)
		{
			case MovableResult::Done: return "done";
			case MovableResult::NothingOpen: return "no level is open";
			case MovableResult::NoSuchThing: return "it is not in the level any more";
			case MovableResult::NotBuilt: return "the mouse can't move this kind yet";
			case MovableResult::PlaceRefused: return "the place is too far from the world's middle";
			case MovableResult::TurnRefused: return "the turn is not a usable turn";
			case MovableResult::KindRefused: return "nothing movable is selected";
			case MovableResult::WriterRefused: return "the level wouldn't take it, so nothing was changed";
		}

		return "an unknown answer";
	}

	bool Movable_CanRotate(MovableKind kind)
	{
		KindRow const *const row = RowOf(kind);

		return row != nullptr && row->canRotate;
	}

	bool Movable_PoseInLevel(MovableKind kind)
	{
		KindRow const *const row = RowOf(kind);

		return row == nullptr || row->inLevel;
	}

	char const *Movable_KindName(MovableKind kind)
	{
		KindRow const *const row = RowOf(kind);

		return row ? row->name : "nothing";
	}

	MovableRef Movable_FromSelection(Selected const &selection)
	{
		MovableRef ref;

		if (selection.kind == SelectedKind::Object)
		{
			bool any = false;

			for (size_t i = 0; i < kSelectionIdBytes; i++)
			{
				any = any || selection.objectId[i] != 0;
			}

			if (any)
			{
				ref.kind = MovableKind::PlacedObject;
				memcpy(ref.id, selection.objectId, kSelectionIdBytes);
				ref.module = selection.moduleIndex;
			}
		}

		return ref;
	}

	MovableResult Movable_CheckPose(MovablePose const &pose, MovablePose *usable)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			if (pose.place[axis] < -kMovableMostPlace || pose.place[axis] > kMovableMostPlace)
			{
				return MovableResult::PlaceRefused;
			}
		}

		double length2 = 0.0;

		for (int i = 0; i < 4; i++)
		{
			if (!RealNumber(pose.turn[i]))   // any real length is made 1 below (a float's square fits a double many times over)
			{
				return MovableResult::TurnRefused;
			}

			length2 += (double)pose.turn[i] * (double)pose.turn[i];
		}

		if (!(length2 > 1e-6))
		{
			return MovableResult::TurnRefused;
		}

		double const length = sqrt(length2);

		if (usable)
		{
			*usable = pose;

			for (int i = 0; i < 4; i++)
			{
				usable->turn[i] = (float)((double)pose.turn[i] / length);
			}
		}

		return MovableResult::Done;
	}

	MovableResult Movable_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		KindRow const *const row = RowOf(ref.kind);

		if (!row || !row->get)
		{
			return MovableResult::KindRefused;
		}

		if (!file)
		{
			return MovableResult::NothingOpen;
		}

		MovablePose read;
		MovableResult const got = row->get(file, ref, &read);

		if (got != MovableResult::Done)
		{
			return got;
		}

		// what the level holds is hostile too: a pose the checks refuse is given to no one
		MovablePose usable;
		MovableResult const checked = Movable_CheckPose(read, &usable);

		if (checked != MovableResult::Done)
		{
			return checked;
		}

		if (!row->canRotate)
		{
			usable.turn[0] = usable.turn[1] = usable.turn[2] = 0.0f;
			usable.turn[3] = 1.0f;
		}

		if (pose)
		{
			*pose = usable;
		}

		return MovableResult::Done;
	}

	MovableResult Movable_GetFirst(OpenLevelFile *file, MovableRef const &first, MovableRef const &second, MovableRef *used, MovablePose *pose)
	{
		MovablePose read;
		MovableResult const one = first.kind == MovableKind::None ? MovableResult::KindRefused : Movable_Get(file, first, &read);

		if (one == MovableResult::Done)
		{
			*used = first;
			*pose = read;
			return one;
		}

		if (second.kind == MovableKind::None)
		{
			return one;
		}

		MovableResult const two = Movable_Get(file, second, &read);

		if (two == MovableResult::Done)
		{
			*used = second;
			*pose = read;
		}

		return two;
	}

	MovableResult Movable_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		KindRow const *const row = RowOf(ref.kind);

		if (!row || !row->set)
		{
			return MovableResult::KindRefused;
		}

		if (!file)
		{
			return MovableResult::NothingOpen;
		}

		MovablePose usable;
		MovableResult const checked = Movable_CheckPose(pose, &usable);

		if (checked != MovableResult::Done)
		{
			return checked;
		}

		if (!row->canRotate)
		{
			usable.turn[0] = usable.turn[1] = usable.turn[2] = 0.0f;
			usable.turn[3] = 1.0f;
		}

		return row->set(file, ref, usable);
	}
}
