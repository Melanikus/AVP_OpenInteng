// See move_rel_poly.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "move_rel_poly.h"

#include <math.h>
#include <string.h>

namespace host
{
	namespace
	{
		// Not a number, tested on its bits: the build is /fp:fast, under which value != value may be taken as never true
		bool NotANumber(double value)
		{
			uint64_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7FF0000000000000ull) == 0x7FF0000000000000ull && (bits & 0x000FFFFFFFFFFFFFull) != 0;
		}

		int32_t Held(int64_t value)
		{
			return value > INT32_MAX ? INT32_MAX : (value < INT32_MIN ? INT32_MIN : (int32_t)value);
		}

		// FUN_004b3860: a point turned by a matrix, columns of the matrix going with the point's parts
		void Turn(int32_t const m[9], int32_t const v[3], int32_t out[3])
		{
			out[0] = Held((int64_t)MoveRelPoly_Multiply(m[0], v[0]) + MoveRelPoly_Multiply(m[3], v[1]) + MoveRelPoly_Multiply(m[6], v[2]));
			out[1] = Held((int64_t)MoveRelPoly_Multiply(m[1], v[0]) + MoveRelPoly_Multiply(m[4], v[1]) + MoveRelPoly_Multiply(m[7], v[2]));
			out[2] = Held((int64_t)MoveRelPoly_Multiply(m[2], v[0]) + MoveRelPoly_Multiply(m[5], v[1]) + MoveRelPoly_Multiply(m[8], v[2]));
		}

		// The round to an int of the x87's current mode: to nearest, ties to even (FUN_004fd740: FLD float, FISTP dword)
		int32_t Round(double value)
		{
			if (NotANumber(value))
			{
				return INT32_MIN;   // what FISTP stores for a number it can't (the "integer indefinite")
			}

			double const rounded = nearbyint(value);

			return rounded > 2147483647.0 || rounded < -2147483648.0 ? INT32_MIN : (int32_t)rounded;
		}

		// FUN_004fd750: an int vector scaled to length 65536 (65536.0 * component / length), each rounded. A vector of no length divides by 0, as inteng's does:
		// the numbers that give are what the x87 gives, 0/0 an "indefinite", and Round turns them into the same indefinite int.
		void ScaleTo65536(int32_t v[3])
		{
			double const length = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);

			for (int axis = 0; axis < 3; axis++)
			{
				v[axis] = Round(65536.0 * v[axis] / length);
			}
		}

		// FUN_004fd620(p0 = 0, p1 = a, p2 = c): the cross product of a and c, normalised, each component times 65536 and rounded
		void CrossNormalised(int32_t const a[3], int32_t const c[3], int32_t out[3])
		{
			double cross[3] = {
				(double)c[2] * a[1] - (double)c[1] * a[2],
				(double)c[0] * a[2] - (double)c[2] * a[0],
				(double)c[1] * a[0] - (double)c[0] * a[1],
			};
			double const length = sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

			for (int axis = 0; axis < 3; axis++)
			{
				out[axis] = Round(cross[axis] / length * 65536.0);
			}
		}
	}

	int32_t MoveRelPoly_Multiply(int32_t a, int32_t b)
	{
		return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
	}

	int32_t MoveRelPoly_DistanceFromMetres(double metres)
	{
		if (NotANumber(metres))
		{
			return 0;
		}

		// metres times 100000, truncated toward 0; a millionth of a unit is added away from 0 first, so a number typed in metres such as 1.5 or 0.29 isn't lost to the
		// double's last digit (1.5 / 0.00001 is 149999.99999999997): ours, named in move_rel_poly.h
		double scaled = metres * 100000.0;

		scaled += scaled < 0.0 ? -1e-6 : 1e-6;

		return scaled >= 2147483647.0 ? INT32_MAX : (scaled <= -2147483648.0 ? INT32_MIN : (int32_t)scaled);
	}

	double MoveRelPoly_MetresFromDistance(int32_t distance)
	{
		return (double)distance * 0.00001;
	}

	bool MoveRelPoly_Make(const MoveRelPolyTarget &target, int32_t distance, const int32_t *hit, MoveRelPolyResult *out)
	{
		if (!out || target.cornerCount < 1 || target.cornerCount > kMoveRelPolyMostCorners)
		{
			return false;
		}

		// 1. the average of the corners, each term divided by the count before it is summed
		int32_t centre[3] = { 0, 0, 0 };
		int32_t const count = (int32_t)target.cornerCount;

		for (size_t i = 0; i < target.cornerCount; i++)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				centre[axis] += target.corners[i][axis] / count;
			}
		}

		// 2. turned by the target's matrix, and its place added
		int32_t turned[3];

		Turn(target.matrix, centre, turned);

		int32_t place[3];

		for (int axis = 0; axis < 3; axis++)
		{
			place[axis] = Held((int64_t)turned[axis] + target.place[axis]);
		}

		// 3. the normal, negated, turned the same way
		int32_t negated[3] = { -target.normal[0], -target.normal[1], -target.normal[2] };
		int32_t n[3];

		Turn(target.matrix, negated, n);

		// 4. the matrix: row 2 is n; row 0 the axis; row 1 the cross product of n and the axis
		int32_t axis[3];

		if (n[0] == 0 && n[2] == 0)
		{
			axis[0] = 0x10000;
			axis[1] = 0;
			axis[2] = 0;
		}
		else
		{
			axis[0] = n[2];
			axis[1] = 0;
			axis[2] = -n[0];
			ScaleTo65536(axis);
		}

		int32_t row1[3];

		CrossNormalised(n, axis, row1);

		for (int i = 0; i < 3; i++)
		{
			out->matrix[i] = axis[i];
			out->matrix[3 + i] = row1[i];
			out->matrix[6 + i] = n[i];
		}

		// 5. a hit point replaces the point; 6. then back along n by the distance
		for (int a = 0; a < 3; a++)
		{
			int32_t const start = hit ? hit[a] : place[a];

			out->place[a] = Held((int64_t)start - MoveRelPoly_Multiply(n[a], distance));
		}

		return true;
	}
}
