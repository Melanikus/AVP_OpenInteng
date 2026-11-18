// camera_orbit.cpp: the camera on a sphere round an object's box (camera_orbit.h says what and why). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "camera_orbit.h"

#include <math.h>

namespace host
{
	namespace
	{
		double const kPi = 3.14159265358979323846;
		double const kMostPlace = 16777216.0;   // 2^24: the editor camera's limit (level_view.h kFlyCameraMostPlace)
		double const kMostBoxSide = 33554432.0; // 2^25: a box two camera limits across

		int32_t Round(double value)
		{
			return (int32_t)floor(value + 0.5);
		}
	}

	bool CameraOrbit_Place(int32_t const mins[3], int32_t const maxs[3], double around, double up, double distance, OrbitCamera *out, double *usedDistance)
	{
		if (!mins || !maxs || !out || !isfinite(around) || !isfinite(up) || !isfinite(distance) || up < -89.0 || up > 89.0 || distance < 0.0 || distance > kOrbitMostDistance)
		{
			return false;
		}

		double centre[3];
		double halfDiagonalSquared = 0.0;

		for (int k = 0; k < 3; k++)
		{
			double const side = (double)maxs[k] - (double)mins[k];

			if (side < 0.0 || side > kMostBoxSide)
			{
				return false;
			}

			centre[k] = ((double)mins[k] + (double)maxs[k]) / 2.0;
			halfDiagonalSquared += (side / 2.0) * (side / 2.0);
		}

		double const taken = distance > 0.0 ? distance : fmin(kOrbitMostDefaultDistance, fmax(kOrbitLeastDistance, kOrbitDistancePerHalfDiagonal * sqrt(halfDiagonalSquared)));
		double const a = fmod(around, 360.0) * kPi / 180.0;
		double const u = up * kPi / 180.0;
		double const across = taken * cos(u);   // the part along the ground
		// the camera on the far side of the way it looks: turned a, it looks along (-sin a, 0, cos a) (turn 1024 looks along -x), so it stands at the centre minus that, and up (y down: minus) by the rest
		double const place[3] = { centre[0] + across * sin(a), centre[1] - taken * sin(u), centre[2] - across * cos(a) };

		for (int k = 0; k < 3; k++)
		{
			if (!(fabs(place[k]) <= kMostPlace))
			{
				return false;
			}

			out->place[k] = Round(place[k]);
		}

		int32_t turn = Round(fmod(around, 360.0) * 4096.0 / 360.0) % 4096;

		out->turn = turn < 0 ? turn + 4096 : turn;
		out->look = Round(up * 4096.0 / 360.0);   // looking down at the centre from above: positive (the fly camera's look, seen 11-18)

		if (usedDistance)
		{
			*usedDistance = taken;
		}

		return true;
	}
}
