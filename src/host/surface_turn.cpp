// See surface_turn.h. Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "surface_turn.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace host
{
	namespace
	{
		double const kPi = 3.14159265358979323846;

		// a real number (not infinite, not "not a number"), judged by its bits: the editor is built with /fp:fast
		bool Real(double v)
		{
			uint64_t bits;

			memcpy(&bits, &v, sizeof(bits));
			return (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
		}

		double Dot(double const a[3], double const b[3])
		{
			return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
		}

		// a quaternion of real numbers made 1 long; false for one of no length or not real
		bool UnitTurn(float const in[4], double q[4])
		{
			for (int i = 0; i < 4; i++)
			{
				q[i] = (double)in[i];

				if (!Real(q[i]))
				{
					return false;
				}
			}

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

		// a direction made 1 long; false for one of no length or not real
		bool UnitVector(double const in[3], double out[3])
		{
			if (!Real(in[0]) || !Real(in[1]) || !Real(in[2]))
			{
				return false;
			}

			double const length = sqrt(Dot(in, in));

			if (!(length > 1e-9) || !(length < 1e30))
			{
				return false;
			}

			for (int i = 0; i < 3; i++)
			{
				out[i] = in[i] / length;
			}

			return true;
		}

		// R e for the unit turn q (x y z w) and local axis `axis`: R's columns (surface_place.h's R: row 1 is 1 - 2(y^2 + z^2), 2(xy - zw), 2(xz + yw)), signed
		void AxisOf(double const q[4], int axis, double out[3])
		{
			double const x = q[0], y = q[1], z = q[2], w = q[3];
			double const columns[3][3] = {
				{ 1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w), 2.0 * (x * z - y * w) },
				{ 2.0 * (x * y - z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + x * w) },
				{ 2.0 * (x * z + y * w), 2.0 * (y * z - x * w), 1.0 - 2.0 * (x * x + y * y) },
			};
			double const sign = axis % 2 == 0 ? 1.0 : -1.0;

			for (int i = 0; i < 3; i++)
			{
				out[i] = sign * columns[axis / 2][i];
			}
		}

		double AngleOf(double cosine)
		{
			return acos(cosine > 1.0 ? 1.0 : (cosine < -1.0 ? -1.0 : cosine));
		}
	}

	char const *SurfaceAxis_Name(int axis)
	{
		static char const *const names[kSurfaceAxisCount] = { "+x", "-x", "+y", "-y", "+z", "-z" };

		return axis >= 0 && axis < kSurfaceAxisCount ? names[axis] : "none";
	}

	bool SurfaceAxis_World(float const turn[4], int axis, double out[3])
	{
		double q[4];

		if (axis < 0 || axis >= kSurfaceAxisCount || !UnitTurn(turn, q))
		{
			return false;
		}

		AxisOf(q, axis, out);
		return true;
	}

	int SurfaceAxis_Closest(float const turn0[4], double const normal[3], int previous, double marginDegrees)
	{
		double q[4], n[3];

		if (!UnitTurn(turn0, q) || !UnitVector(normal, n) || !Real(marginDegrees))
		{
			return -1;
		}

		int best = -1;
		double bestDot = -2.0, dots[kSurfaceAxisCount];

		for (int axis = 0; axis < kSurfaceAxisCount; axis++)
		{
			double a[3];

			AxisOf(q, axis, a);
			dots[axis] = Dot(a, n);

			if (dots[axis] > bestDot)
			{
				bestDot = dots[axis];
				best = axis;
			}
		}

		// keep the previous frame's axis unless the best one is nearer the normal by more than the margin
		if (previous >= 0 && previous < kSurfaceAxisCount && previous != best)
		{
			double const margin = (marginDegrees > 0.0 ? marginDegrees : 0.0) * kPi / 180.0;

			if (AngleOf(dots[previous]) - AngleOf(bestDot) <= margin)
			{
				return previous;
			}
		}

		return best;
	}

	bool SurfaceTurn_AxisOnto(float const turn0[4], int axis, double const normal[3], float out[4])
	{
		double q[4], n[3], a[3];

		if (axis < 0 || axis >= kSurfaceAxisCount || !UnitTurn(turn0, q) || !UnitVector(normal, n))
		{
			return false;
		}

		AxisOf(q, axis, a);

		double const c = Dot(a, n);

		if (!(c >= 0.0))
		{
			return false;   // more than 90 degrees off: no closest axis is
		}

		// Q, the smallest turn taking a onto n: (a x n, 1 + a . n), made 1 long (c >= 0, so its length is 1 or more: never a division by about 0)
		double r[4] = { a[1] * n[2] - a[2] * n[1], a[2] * n[0] - a[0] * n[2], a[0] * n[1] - a[1] * n[0], 1.0 + c };
		double const length = sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);

		for (int i = 0; i < 4; i++)
		{
			r[i] /= length;
		}

		// Q R0, the product r q (R(r q) = R(r) R(q)), as surface_place.cpp composes it
		double const x = q[0], y = q[1], z = q[2], w = q[3];
		double made[4] = {
			r[3] * x + r[0] * w + (r[1] * z - r[2] * y),
			r[3] * y + r[1] * w + (r[2] * x - r[0] * z),
			r[3] * z + r[2] * w + (r[0] * y - r[1] * x),
			r[3] * w - (r[0] * x + r[1] * y + r[2] * z),
		};
		double const madeLength = sqrt(made[0] * made[0] + made[1] * made[1] + made[2] * made[2] + made[3] * made[3]);

		if (!(madeLength > 1e-9))
		{
			return false;
		}

		for (int i = 0; i < 4; i++)
		{
			out[i] = (float)(made[i] / madeLength);
		}

		return true;
	}

	namespace
	{
		// Flip k's place round the 24 (0 to 23) for any whole number: the remainder made positive (no number can overflow here: |k % 24| < 24)
		int FlipIndex(int k)
		{
			return ((k % kSurfaceFlipCount) + kSurfaceFlipCount) % kSurfaceFlipCount;
		}

		// the Hamilton product r q: R(r q) = R(r) R(q), q done first (surface_place.cpp's own formula)
		void Times(double const r[4], double const q[4], double out[4])
		{
			out[0] = r[3] * q[0] + r[0] * q[3] + (r[1] * q[2] - r[2] * q[1]);
			out[1] = r[3] * q[1] + r[1] * q[3] + (r[2] * q[0] - r[0] * q[2]);
			out[2] = r[3] * q[2] + r[2] * q[3] + (r[0] * q[1] - r[1] * q[0]);
			out[3] = r[3] * q[3] - (r[0] * q[0] + r[1] * q[1] + r[2] * q[2]);
		}
	}

	void SurfaceFlip_Turn(int k, float out[4])
	{
		int const index = FlipIndex(k), face = index / 4, quarters = index % 4;
		double const h = sqrt(0.5);   // cos 45 = sin 45

		// THE FACE DOWN, a turn T taking that local axis onto +y (y is down): about z by -90 or +90 for the left (-x) and right (+x) sides, about x by -90 or +90 for the
		// front (+z) and back (-z), half a turn about x for upside down (-y); none for upright (+y already down). A turn by a about the unit axis u is (u sin a/2, cos a/2).
		static double const faces[6][4] = {
			{ 0, 0, 0, 1 },     // upright
			{ 0, 0, -h, h },    // on its left side: about z by -90 (-x onto +y)
			{ 0, 0, h, h },     // on its right side: about z by +90 (+x onto +y)
			{ -h, 0, 0, h },    // on its front: about x by -90 (+z onto +y)
			{ h, 0, 0, h },     // on its back: about x by +90 (-z onto +y)
			{ 1, 0, 0, 0 },     // upside down: about x by 180 (-y onto +y)
		};

		// THE QUARTER TURNS U about up (-y), a quarter turn each: about -y by 90 per step (+x towards +z), so U = (0, -sin(q 45), 0, cos(q 45))
		static double const turns[4][4] = {
			{ 0, 0, 0, 1 }, { 0, -h, 0, h }, { 0, -1, 0, 0 }, { 0, -h, 0, -h },
		};

		double made[4];

		Times(turns[quarters], faces[face], made);   // the face put down first, then turned about up

		for (int i = 0; i < 4; i++)
		{
			out[i] = (float)made[i];
		}
	}

	int SurfaceFlip_DownAxis(int k)
	{
		static int const down[6] = { 2, 1, 0, 4, 5, 3 };   // +y, -x, +x, +z, -z, -y (the axes' own numbers, above)

		return down[FlipIndex(k) / 4];
	}

	std::string SurfaceFlip_Words(int k)
	{
		static char const *const faces[6] = { "upright", "on its left side", "on its right side", "on its front", "on its back", "upside down" };
		int const index = FlipIndex(k);

		return std::string(faces[index / 4]) + ", " + std::to_string((index % 4) * 90);
	}

	bool SurfaceTurn_Then(float const a[4], float const b[4], float out[4])
	{
		double qa[4], qb[4], made[4];

		if (!UnitTurn(a, qa) || !UnitTurn(b, qb))
		{
			return false;
		}

		Times(qa, qb, made);

		double const length = sqrt(made[0] * made[0] + made[1] * made[1] + made[2] * made[2] + made[3] * made[3]);

		if (!(length > 1e-9))
		{
			return false;
		}

		for (int i = 0; i < 4; i++)
		{
			out[i] = (float)(made[i] / length);
		}

		return true;
	}
}
