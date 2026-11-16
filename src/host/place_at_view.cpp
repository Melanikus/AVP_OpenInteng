// See place_at_view.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdint.h>

#include "place_at_view.h"

namespace host
{
	namespace
	{
		// The game's MUL_FIXED for a distance and a unit-length axis (65536 = 1): the 64-bit product moved down 16 places, so no number can wrap
		int64_t Along(int32_t distance, int32_t axis)
		{
			return ((int64_t)distance * (int64_t)axis) >> 16;
		}
	}

	bool PlaceAtView_Make(const int32_t camera[3], const ViewAxes &axes, ViewOffset where, int32_t distance, int32_t place[3])
	{
		if (!camera || !place || distance < 0 || distance > kPlaceAtViewFarthest)
		{
			return false;
		}

		int64_t moved[3] = { camera[0], camera[1], camera[2] };

		switch (where)
		{
			case ViewOffset::OnCamera:
				break;
			case ViewOffset::InFront:
			case ViewOffset::Behind:
			{
				int64_t const sign = where == ViewOffset::InFront ? 1 : -1;

				for (int axis = 0; axis < 3; axis++)
				{
					moved[axis] += sign * Along(distance, axes.forward[axis]);
				}

				break;
			}
			case ViewOffset::Right:
			case ViewOffset::Left:
			{
				int64_t const sign = where == ViewOffset::Right ? 1 : -1;

				for (int axis = 0; axis < 3; axis++)
				{
					moved[axis] += sign * Along(distance, axes.right[axis]);
				}

				break;
			}
			case ViewOffset::Above:
				moved[1] -= distance;   // the game's y points down
				break;
			case ViewOffset::Under:
				moved[1] += distance;
				break;
			default:
				return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (moved[axis] > kPlaceAtViewFarthest || moved[axis] < -(int64_t)kPlaceAtViewFarthest)
			{
				return false;
			}
		}

		for (int axis = 0; axis < 3; axis++)
		{
			place[axis] = (int32_t)moved[axis];
		}

		return true;
	}
}
