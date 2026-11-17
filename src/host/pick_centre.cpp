// See pick_centre.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "pick_centre.h"

#include <math.h>

namespace host
{
	bool PickCentre_Project(PickCamera const &camera, PickCentre const &centre, double *outX, double *outY)
	{
		// the same checks as the click's line (PickRay_FromClick), so a camera it refuses is refused here
		if (camera.viewWidth < 1 || camera.viewWidth > kPickMostSide || camera.viewHeight < 1 || camera.viewHeight > kPickMostSide)
		{
			return false;
		}

		if (!PickRay_ScaleUsable(camera.xScale) || !PickRay_ScaleUsable(camera.yScale))
		{
			return false;
		}

		for (int row = 0; row < 3; row++)
		{
			for (int column = 0; column < 3; column++)
			{
				if (camera.turn[row][column] < -kPickOne || camera.turn[row][column] > kPickOne)
				{
					return false;
				}
			}
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (camera.place[axis] < -kPickMostPlace || camera.place[axis] > kPickMostPlace || centre.place[axis] < -kPickMostPlace || centre.place[axis] > kPickMostPlace)
			{
				return false;
			}
		}

		// the centre in the camera's view: the camera's place taken off, then each view number from one COLUMN of the matrix (RotateVector), in 65536ths
		double const away[3] = { (double)centre.place[0] - (double)camera.place[0], (double)centre.place[1] - (double)camera.place[1], (double)centre.place[2] - (double)camera.place[2] };
		double view[3];

		for (int column = 0; column < 3; column++)
		{
			view[column] = ((double)camera.turn[0][column] * away[0] + (double)camera.turn[1][column] * away[1] + (double)camera.turn[2][column] * away[2]) / (double)kPickOne;
		}

		// not beyond the near value: never taken
		if (!(view[2] >= (double)kPickCentreNear))
		{
			return false;
		}

		// the projection: across by xScale, down by yScale, over the depth (the game's view counts down where the card counts up), spread over the view's pixels
		double const across = view[0] * (double)camera.xScale / view[2];
		double const up = -view[1] * (double)camera.yScale / view[2];
		double const x = (across + 1.0) * 0.5 * (double)camera.viewWidth;
		double const y = (1.0 - up) * 0.5 * (double)camera.viewHeight;
		*outX = x;
		*outY = y;
		return true;
	}


	double PickCentre_Distance(PickCamera const &camera, int32_t clickX, int32_t clickY, PickCentre const &centre)
	{
		double x = 0.0, y = 0.0;

		if (!PickCentre_Project(camera, centre, &x, &y))
		{
			return -1.0;
		}

		double const dx = (double)clickX - x;
		double const dy = (double)clickY - y;

		return sqrt(dy * dy + dx * dx);
	}

	int PickCentre_Nearest(PickCamera const &camera, int32_t clickX, int32_t clickY, PickCentre const *candidates, size_t count, bool zeroTaken, double farthest)
	{
		if (!candidates)
		{
			return -1;
		}

		double best = farthest;
		int taken = -1;

		for (size_t i = 0; i < count; i++)
		{
			double const distance = PickCentre_Distance(camera, clickX, clickY, candidates[i]);

			if ((zeroTaken ? distance >= 0.0 : distance > 0.0) && distance < best)
			{
				best = distance;
				taken = (int)i;
			}
		}

		return taken;
	}
}
