// See surface_place.h. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "surface_place.h"

#include <math.h>
#include <string.h>

#include "surface_turn.h"   // CLOSEST: the axis and the turn (Frost 11-19, the owner's order)

namespace host
{
	namespace
	{
		double const kOne = 65536.0;
		double const kMostPlace = 16777216.0;   // 2^24, the camera's and the level's bound (level_view.h kFlyCameraMostPlace)

		// c = a x b
		void Cross(double const a[3], double const b[3], double c[3])
		{
			c[0] = a[1] * b[2] - a[2] * b[1];
			c[1] = a[2] * b[0] - a[0] * b[2];
			c[2] = a[0] * b[1] - a[1] * b[0];
		}

		double Dot(double const a[3], double const b[3])
		{
			return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
		}

		// a whole number, the nearest (halves away from 0)
		int32_t Nearest(double value)
		{
			return (int32_t)(value >= 0.0 ? floor(value + 0.5) : -floor(-value + 0.5));
		}

		// A float that is a real number (not infinite, not "not a number"), judged by its bits: /fp:fast lets the compiler assume no number is ever NaN (as movable.cpp's)
		bool IsReal(float value)
		{
			uint32_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7f800000u) != 0x7f800000u;
		}

		// a quaternion x y z w (real numbers) scaled to length 1; false for one of no length, or one so long its squares would not be real
		bool ToUnit(double q[4])
		{
			double const length = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

			if (!(length > 1e-9) || !(length < 1e30))
			{
				return false;
			}

			for (int i = 0; i < 4; i++)
			{
				q[i] /= length;
			}

			return true;
		}
	}

	bool SurfacePlace_Pose(SurfacePlaceInput const &in, SurfacePlacePose *out)
	{
		double n[3] = { in.hitNormal[0] / kOne, in.hitNormal[1] / kOne, in.hitNormal[2] / kOne };
		double const length = sqrt(Dot(n, n));

		if (!(length > 1e-6))
		{
			return false;
		}

		for (int i = 0; i < 3; i++)
		{
			n[i] /= length;
		}

		SurfacePlacePose pose;

		for (int i = 0; i < 4; i++)
		{
			pose.turn[i] = in.turn[i];
		}

		if (!in.canTurn)
		{
			// a move-only kind: the hit point, nothing else
			for (int i = 0; i < 3; i++)
			{
				pose.place[i] = in.hitPoint[i];
			}

			*out = pose;
			return true;
		}

		double q[4] = { in.turn[0], in.turn[1], in.turn[2], in.turn[3] };

		if (!IsReal(in.turn[0]) || !IsReal(in.turn[1]) || !IsReal(in.turn[2]) || !IsReal(in.turn[3]) || !ToUnit(q))
		{
			return false;
		}

		if (in.align == SurfaceAlign::Closest)
		{
			// CLOSEST (the default): the axis pointing most out of the surface from the drag's start pose, turned onto the normal by the smallest rotation (surface_turn.h)
			int const axis = SurfaceAxis_Closest(in.turn, n, in.previousAxis, kSurfaceAxisMarginDegrees);

			if (axis < 0 || !SurfaceTurn_AxisOnto(in.turn, axis, n, pose.turn))
			{
				return false;
			}

			pose.axis = axis;
		}
		else
		{
		double const x = q[0], y = q[1], z = q[2], w = q[3];

		// UP ("up to surface", the 11-18 rule): the object's up, R (0, -1, 0) (minus R's middle column), and its x axis, R (1, 0, 0) (R's first column)
		double const up[3] = { -2.0 * (x * y - z * w), -(1.0 - 2.0 * (x * x + z * z)), -2.0 * (y * z + x * w) };
		double const xAxis[3] = { 1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w), 2.0 * (x * z - y * w) };
		double const c = Dot(up, n);

		// the smallest turn r taking up onto n: (up x n, 1 + up . n), to length 1 (none at all when up is n already); up opposite n: half a turn about the object's x axis, (x axis, 0)
		double r[4];

		if (c < -1.0 + 1e-9)
		{
			r[0] = xAxis[0];
			r[1] = xAxis[1];
			r[2] = xAxis[2];
			r[3] = 0.0;
		}
		else
		{
			double v[3];

			Cross(up, n, v);
			r[0] = v[0];
			r[1] = v[1];
			r[2] = v[2];
			r[3] = 1.0 + c;
		}

		if (!ToUnit(r))
		{
			return false;
		}

		// r after the object's own: r q (R(r q) = R(r) R(q)), of length 1 as both are
		pose.turn[0] = (float)(r[3] * x + r[0] * w + (r[1] * z - r[2] * y));
		pose.turn[1] = (float)(r[3] * y + r[1] * w + (r[2] * x - r[0] * z));
		pose.turn[2] = (float)(r[3] * z + r[2] * w + (r[0] * y - r[1] * x));
		pose.turn[3] = (float)(r[3] * w - (r[0] * x + r[1] * y + r[2] * z));
		pose.axis = 3;   // -y, the object's up
		}

		// the place: the hit point, pushed out along the normal by the box's extent on the side that MEETS the surface (Bishop's order 11-19, the owner's): the snapped axis points out of
		// the surface, so the box's face on its other side meets it. For +x, +y or +z (0, 2, 4) that is the box's least on the axis (pushed out by minus it), for -x, -y or -z (1, 3, 5)
		// its most. UP's -y gives the base, the largest y (y is down), as the 11-18 rule did
		if (pose.axis < 0 || pose.axis > 5)
		{
			return false;
		}

		int const side = pose.axis / 2;
		double const push = pose.axis % 2 == 0 ? -(double)in.boxMin[side] : (double)in.boxMax[side];

		for (int i = 0; i < 3; i++)
		{
			double const place = (double)in.hitPoint[i] + n[i] * push;

			if (!(place > -kMostPlace && place < kMostPlace))
			{
				return false;
			}

			pose.place[i] = Nearest(place);
		}

		*out = pose;
		return true;
	}

	bool SurfacePlace_Live(SurfaceLiveInput const &in, SurfacePlacePose *out)
	{
		uint64_t bits;

		memcpy(&bits, &in.scale, sizeof(bits));

		// a real number (judged by its bits: /fp:fast) inside the bounds
		if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull || !(in.scale >= 0.001 && in.scale <= 1000.0))
		{
			return false;
		}

		SurfacePlaceInput world = {};
		int64_t facing = 0;

		for (int i = 0; i < 3; i++)
		{
			if (in.hitNormal[i] == INT32_MIN)
			{
				return false;
			}

			facing += (int64_t)in.hitNormal[i] * ((int64_t)in.hitWorld[i] - (int64_t)in.cameraWorld[i]);
		}

		// a face seen from behind (its normal along the line of sight): turned round, so "out of the surface" is towards the camera
		for (int i = 0; i < 3; i++)
		{
			world.hitPoint[i] = in.hitWorld[i];
			world.hitNormal[i] = facing > 0 ? -in.hitNormal[i] : in.hitNormal[i];
			world.turn[i] = in.turn[i];
		}

		world.turn[3] = in.turn[3];

		for (int i = 0; i < 3; i++)
		{
			world.boxMin[i] = in.boxMinWorld[i];
			world.boxMax[i] = in.boxMaxWorld[i];
		}

		world.canTurn = in.canTurn;
		world.align = in.align;
		world.previousAxis = in.previousAxis;

		SurfacePlacePose pose;

		if (!SurfacePlace_Pose(world, &pose))
		{
			return false;
		}

		// the place in the file's units
		for (int i = 0; i < 3; i++)
		{
			double const place = (double)pose.place[i] / in.scale;

			if (!(place > -kMostPlace && place < kMostPlace))
			{
				return false;
			}

			pose.place[i] = Nearest(place);
		}

		*out = pose;
		return true;
	}
}
