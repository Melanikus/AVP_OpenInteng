// See pick_ray.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <math.h>
#include <string.h>

#include "pick_ray.h"

namespace host
{
	bool PickRay_ScaleUsable(float scale)
	{
		uint32_t bits = 0;
		memcpy(&bits, &scale, sizeof(bits));

		uint32_t const sign = bits >> 31;
		uint32_t const exponent = (bits >> 23) & 0xFF;

		// The sign bit off (above 0, or a nought), and the exponent neither all ones (not a number, or endless) nor all noughts (a
		// nought, or one of the smallest numbers, which are too small to divide by)
		return sign == 0 && exponent != 0xFF && exponent != 0;
	}

	bool PickRay_Project(const PickCamera &camera, const int32_t world[3], double *pixelX, double *pixelY, double *ahead)
	{
		if (!pixelX || !pixelY || !ahead || camera.viewWidth < 1 || camera.viewWidth > kPickMostSide || camera.viewHeight < 1 || camera.viewHeight > kPickMostSide ||
			!PickRay_ScaleUsable(camera.xScale) || !PickRay_ScaleUsable(camera.yScale))
		{
			return false;
		}

		double const from[3] = { (double)world[0] - camera.place[0], (double)world[1] - camera.place[1], (double)world[2] - camera.place[2] };
		double view[3];

		// RotateVector: each view number is one COLUMN of the matrix times the place (the matrix in 65536ths)
		for (int column = 0; column < 3; column++)
		{
			view[column] = ((double)camera.turn[0][column] * from[0] + (double)camera.turn[1][column] * from[1] + (double)camera.turn[2][column] * from[2]) / (double)kPickOne;
		}

		if (!(view[2] > 0.0))
		{
			return false;
		}

		double const across = view[0] * (double)camera.xScale / view[2];
		double const up = -view[1] * (double)camera.yScale / view[2];

		*pixelX = (across + 1.0) * 0.5 * (double)camera.viewWidth;
		*pixelY = (1.0 - up) * 0.5 * (double)camera.viewHeight;
		*ahead = view[2];
		return true;
	}

	PickRayMade PickRay_FromClick(const PickCamera &camera, int32_t clickX, int32_t clickY, PickRay *ray)
	{
		if (camera.viewWidth < 1 || camera.viewWidth > kPickMostSide || camera.viewHeight < 1 || camera.viewHeight > kPickMostSide)
		{
			return PickRayMade::ViewSizeRefused;
		}

		if (clickX < 0 || clickX >= camera.viewWidth || clickY < 0 || clickY >= camera.viewHeight)
		{
			return PickRayMade::ClickOutsideTheView;
		}

		if (!PickRay_ScaleUsable(camera.xScale) || !PickRay_ScaleUsable(camera.yScale))
		{
			return PickRayMade::ScaleRefused;
		}

		for (int row = 0; row < 3; row++)
		{
			for (int column = 0; column < 3; column++)
			{
				if (camera.turn[row][column] < -kPickOne || camera.turn[row][column] > kPickOne)
				{
					return PickRayMade::TurnRefused;
				}
			}
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (camera.place[axis] < -kPickMostPlace || camera.place[axis] > kPickMostPlace)
			{
				return PickRayMade::PlaceRefused;
			}
		}

		// The click's place on the screen, from -1 to 1 each way: across from the left, and up from the bottom, as the card counts them.
		// Direct3D 9 puts a pixel's middle on its whole number, so pixel 0 is the screen's left edge, -1.
		double const across = 2.0 * (double)clickX / (double)camera.viewWidth - 1.0;
		double const up = 1.0 - 2.0 * (double)clickY / (double)camera.viewHeight;

		// The direction in the camera's view, one unit ahead: the projection undone. The game's own view counts down where the card
		// counts up (d3d_render.cpp:1915), so up turns back into down here. Each scale is a real number at least 1.2e-38 (checked above),
		// and the click's numbers are at most 1 each way, so each is at most about 8.5e37: nothing here can overflow a double.
		double const view[3] = { across / (double)camera.xScale, -up / (double)camera.yScale, 1.0 };

		// Back into the world: RotateVector makes each view number from one COLUMN of the matrix, so each world number comes from one
		// ROW of it. The matrix is in 65536ths, but the length is made 65536 next, so the 65536 needn't be taken off here.
		double world[3];
		for (int row = 0; row < 3; row++)
		{
			world[row] = (double)camera.turn[row][0] * view[0] + (double)camera.turn[row][1] * view[1] + (double)camera.turn[row][2] * view[2];
		}

		double const length = sqrt(world[0] * world[0] + world[1] * world[1] + world[2] * world[2]);

		// A matrix of noughts, or one whose rows cancel this direction out, gives a line that points nowhere. (length is never "not a
		// number" here: every number that went into it is a whole number or a checked scale, and none is endless.)
		if (!(length >= 1e-9))
		{
			return PickRayMade::NoDirection;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			ray->from[axis] = camera.place[axis];

			// Rounded to the nearest whole number, and held to the game's 1 each way, which a 65536 long line can't pass anyway
			double const part = world[axis] / length * (double)kPickOne;
			double const rounded = part < 0.0 ? -floor(-part + 0.5) : floor(part + 0.5);
			ray->direction[axis] = rounded > (double)kPickOne ? kPickOne : (rounded < -(double)kPickOne ? -kPickOne : (int32_t)rounded);
		}

		return PickRayMade::Made;
	}
}
