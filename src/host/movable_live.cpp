// movable_live.cpp: see movable_live.h. Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "movable_live.h"

#include <math.h>
#include <string.h>

#include <string>

#include "state_line.h"

namespace host
{
	namespace
	{
		bool active = false;
		MovableRef liveRef;
		MovablePose livePose;

		std::string drawnWhat;
		double drawnAt[3] = { 0.0, 0.0, 0.0 };
		bool drawnTurned = false;
		double drawnFacing[3] = { 0.0, 0.0, 0.0 };

		// The special kind of a movable kind (movable_special.cpp's mapping): false for the kinds that are no special object
		bool SpecialKindOf(MovableKind kind, SpecialKind *out)
		{
			switch (kind)
			{
				case MovableKind::PlayerStart: *out = SpecialKind::PlayerStart; return true;
				case MovableKind::ParticleGenerator: *out = SpecialKind::ParticleGenerator; return true;
				case MovableKind::Sound: *out = SpecialKind::Sound; return true;
				case MovableKind::AvpGenerator: *out = SpecialKind::Generator; return true;
				case MovableKind::BadGuy: *out = SpecialKind::BadGuy; return true;
				default: return false;
			}
		}

		bool HasId(MovableRef const &ref)
		{
			for (uint8_t const b : ref.id)
			{
				if (b)
				{
					return true;
				}
			}

			return false;
		}
	}

	void MovableLive_Show(MovableRef const &ref, MovablePose const &pose)
	{
		active = ref.kind != MovableKind::None;
		liveRef = ref;
		livePose = pose;
	}

	void MovableLive_Clear()
	{
		active = false;
	}

	bool MovableLive_PoseOfSpecial(SpecialKind kind, size_t number, MovablePose *pose)
	{
		SpecialKind liveKind;

		if (!active || !pose || !SpecialKindOf(liveRef.kind, &liveKind) || liveKind != kind || liveRef.number < 0 || (size_t)liveRef.number != number)
		{
			return false;
		}

		*pose = livePose;
		return true;
	}

	bool MovableLive_PoseOfHierarchy(uint8_t const id[8], size_t number, MovablePose *pose)
	{
		if (!active || !pose || liveRef.kind != MovableKind::PlacedHierarchy)
		{
			return false;
		}

		// by its ID when the ref has one (movable_placed_hierarchy.cpp), else by its number
		bool const same = HasId(liveRef) ? (id && memcmp(id, liveRef.id, sizeof(liveRef.id)) == 0) : (liveRef.number >= 0 && (size_t)liveRef.number == number);

		if (!same)
		{
			return false;
		}

		*pose = livePose;
		return true;
	}

	bool MovableLive_PoseOfVirtual(uint8_t const id[8], MovablePose *pose)
	{
		if (!active || !pose || !id || liveRef.kind != MovableKind::VirtualObject || memcmp(id, liveRef.id, sizeof(liveRef.id)) != 0)
		{
			return false;
		}

		*pose = livePose;
		return true;
	}

	void MovableLive_ForgetDrawn()
	{
		drawnWhat.clear();
	}

	void MovableLive_NoteDrawn(char const *what, double const world[3], double const *axes)
	{
		drawnWhat = what ? what : "";
		drawnTurned = axes != nullptr;

		for (int i = 0; i < 3; i++)
		{
			drawnAt[i] = world[i];
			drawnFacing[i] = axes ? axes[6 + i] : 0.0;
		}
	}

	void MovableLive_StateLine()
	{
		std::string value;

		if (!drawnWhat.empty())
		{
			value = drawnWhat;

			for (int i = 0; i < 3; i++)
			{
				value += " " + std::to_string((long long)floor(drawnAt[i] + 0.5));
			}

			if (drawnTurned)
			{
				value += " facing";

				for (int i = 0; i < 3; i++)
				{
					value += " " + std::to_string((long long)floor(drawnFacing[i] * 1000.0 + 0.5));
				}
			}
		}

		StateLine_Add("currenticon", value);
	}
}
