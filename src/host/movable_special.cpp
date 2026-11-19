// movable_special.cpp: movable.h's adapter for Object Placement's SPECIAL OBJECTS (special_objects_edit.h): the player start and the particle generator with their turn, the sound,
// the AVP generator and the bad guy with the place only. Each is read with SpecialObjects_Read and written whole again with SpecialObject_Write, so every other field and child is
// kept, and the write goes through the game's loader, which checks the changed level; nothing is changed if it refuses. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH ONE: the ref's kind and its number among the objects of that kind (SpecialObject::number, the file's order).
// DECIDED P2-SPECIAL-WRITER: the plan names SpecialMode_MoveCurrent for the move-only kinds. That is "read the mode's current object, put the place in, SpecialObject_Write"
// (special_mode.cpp), and it needs the Object Placement mode's state. So the same writer is called here without it. The generator's and bad guy's heading stays as it is.
//
// THE TURN, as movable.h gives it (x y z w, OBJHEAD1's way):
// - a particle generator's PARGENDA holds it the same way, and the game reads both alike, x y z negated, then QuatToMat (object_setup.cpp:3992-3998 and 899-905). As it is.
// - a player start's AVPSTART holds a 3 x 3 of 16.16 numbers, row by row (mat11 mat12 mat13 mat21 ...), which the game takes as the player's matrix as it is
//   (projload.cpp:170-178, player.cpp:341), turned to and from a turn by movable_matrix.h (the editor's own pairing; the light's adapter uses the same).
// - the other three hold none: the identity is given, and a turn given is not used.

#include "movable.h"

#include <string.h>

#include <vector>

#include "movable_matrix.h"
#include "special_objects_edit.h"

namespace host
{
	namespace
	{
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

		// The object the ref names. NothingOpen with no level; NoSuchThing when there is no such object.
		MovableResult Find(OpenLevelFile *file, MovableRef const &ref, SpecialObject *out)
		{
			SpecialKind kind;

			if (!SpecialKindOf(ref.kind, &kind))
			{
				return MovableResult::KindRefused;
			}

			if (!file)
			{
				return MovableResult::NothingOpen;
			}

			std::vector<SpecialObject> all;
			SpecialResult const read = SpecialObjects_Read(file, &all);

			if (read == SpecialResult::BadCall)
			{
				return MovableResult::NothingOpen;
			}

			if (read != SpecialResult::Done || ref.number < 0)
			{
				return MovableResult::NoSuchThing;
			}

			for (SpecialObject const &each : all)
			{
				if (each.kind == kind && each.number == (size_t)ref.number)
				{
					*out = each;
					return MovableResult::Done;
				}
			}

			return MovableResult::NoSuchThing;
		}
	}

	MovableResult MovableSpecial_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		SpecialObject object;
		MovableResult const found = Find(file, ref, &object);

		if (found != MovableResult::Done)
		{
			return found;
		}

		MovablePose read;

		memcpy(read.place, object.place, sizeof(read.place));

		if (object.kind == SpecialKind::PlayerStart)
		{
			Movable_TurnOfMatrix(object.matrix, read.turn);
		}
		else if (object.kind == SpecialKind::ParticleGenerator)
		{
			memcpy(read.turn, object.turn, sizeof(read.turn));
		}

		*pose = read;
		return MovableResult::Done;
	}

	MovableResult MovableSpecial_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		SpecialObject object;
		MovableResult const found = Find(file, ref, &object);

		if (found != MovableResult::Done)
		{
			return found;
		}

		memcpy(object.place, pose.place, sizeof(object.place));

		if (object.kind == SpecialKind::PlayerStart)
		{
			Movable_MatrixOfTurn(pose.turn, object.matrix);
		}
		else if (object.kind == SpecialKind::ParticleGenerator)
		{
			memcpy(object.turn, pose.turn, sizeof(object.turn));
		}

		switch (SpecialObject_Write(file, object))
		{
			case SpecialResult::Done: return MovableResult::Done;
			case SpecialResult::BadCall: return MovableResult::NothingOpen;
			case SpecialResult::NotFound: case SpecialResult::NoEnvironment: return MovableResult::NoSuchThing;
			default: return MovableResult::WriterRefused;
		}
	}
}
