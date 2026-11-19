// movable_light.cpp: movable.h's adapter for a Light Sourcing LIGHT (a STDLIGHT of the light set NORMALLT): its place and its turn read with LightPose_Get and
// written in one write with the new LightPose_Set (light_keys.h), beside L's LightPlace_Set: the changed flag put in as L does, every other number kept. The write
// goes through the game's loader, which checks the changed level; nothing is changed if it refuses. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH ONE: ref.number is the light's place in NORMALLT's list, in the file's order (light_edit.h: the number every Light Sourcing key takes).
// THE TURN: a STDLIGHT holds a 3 x 3 of 16.16 numbers, row by row, its third row the light's direction (light_maths.h; the game itself never reads it: inteng's
// prelighting does). D drops a light with the camera's matrix (LightDefaultDrop), as a player start is made with it, so the light pairs its matrix with a turn
// as the player start does (movable_matrix.h).

#include "movable.h"

#include <string.h>

#include "light_edit.h"
#include "light_keys.h"
#include "movable_matrix.h"

namespace host
{
	namespace
	{
		MovableResult ResultOf(LightEditResult result)
		{
			switch (result)
			{
				case LightEditResult::Done: return MovableResult::Done;
				case LightEditResult::NothingOpen: return MovableResult::NothingOpen;
				case LightEditResult::NoLightSet: case LightEditResult::NoSuchLight: return MovableResult::NoSuchThing;
				default: return MovableResult::WriterRefused;
			}
		}
	}

	MovableResult MovableLight_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		if (!file)
		{
			return MovableResult::NothingOpen;
		}

		if (ref.number < 0)
		{
			return MovableResult::NoSuchThing;
		}

		MovablePose read;
		int32_t matrix[9];
		MovableResult const got = ResultOf(LightPose_Get(file, (size_t)ref.number, read.place, matrix));

		if (got != MovableResult::Done)
		{
			return got;
		}

		Movable_TurnOfMatrix(matrix, read.turn);
		*pose = read;
		return MovableResult::Done;
	}

	MovableResult MovableLight_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		if (!file)
		{
			return MovableResult::NothingOpen;
		}

		if (ref.number < 0)
		{
			return MovableResult::NoSuchThing;
		}

		int32_t matrix[9];

		Movable_MatrixOfTurn(pose.turn, matrix);
		return ResultOf(LightPose_Set(file, (size_t)ref.number, pose.place, matrix));
	}
}
