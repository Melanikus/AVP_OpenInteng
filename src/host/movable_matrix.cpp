// See movable_matrix.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "movable_matrix.h"

#include <math.h>

namespace host
{
	namespace
	{
		int32_t Fixed(double value)
		{
			// a unit turn's numbers are -1 .. 1; held to that so a rounding can never pass an int
			double const held = value < -1.0 ? -1.0 : value > 1.0 ? 1.0 : value;

			return (int32_t)lround(held * 65536.0);
		}
	}

	// The game's QuatToMat (maths.cpp:2374-2468) of (-x, -y, -z, w), row by row
	void Movable_MatrixOfTurn(float const turn[4], int32_t matrix[9])
	{
		double const x = -(double)turn[0], y = -(double)turn[1], z = -(double)turn[2], w = (double)turn[3];

		matrix[0] = Fixed(1.0 - 2.0 * (y * y + z * z));   // mat11
		matrix[1] = Fixed(2.0 * (x * y - w * z));         // mat12
		matrix[2] = Fixed(2.0 * (x * z + w * y));         // mat13
		matrix[3] = Fixed(2.0 * (x * y + w * z));         // mat21
		matrix[4] = Fixed(1.0 - 2.0 * (x * x + z * z));   // mat22
		matrix[5] = Fixed(2.0 * (y * z - w * x));         // mat23
		matrix[6] = Fixed(2.0 * (x * z - w * y));         // mat31
		matrix[7] = Fixed(2.0 * (y * z + w * x));         // mat32
		matrix[8] = Fixed(1.0 - 2.0 * (x * x + y * y));   // mat33
	}

	void Movable_TurnOfMatrix(int32_t const matrix[9], float turn[4])
	{
		double m[9];

		for (int i = 0; i < 9; i++)
		{
			m[i] = (double)matrix[i] / 65536.0;
		}

		double const m11 = m[0], m12 = m[1], m13 = m[2], m21 = m[3], m22 = m[4], m23 = m[5], m31 = m[6], m32 = m[7], m33 = m[8];
		double const trace = m11 + m22 + m33;
		double x = 0.0, y = 0.0, z = 0.0, w = 1.0, s = 0.0;

		if (trace > 0.0)
		{
			s = sqrt(trace + 1.0) * 2.0;
			w = 0.25 * s;
			x = (m32 - m23) / s;
			y = (m13 - m31) / s;
			z = (m21 - m12) / s;
		}
		else if (m11 >= m22 && m11 >= m33)
		{
			double const d = 1.0 + m11 - m22 - m33;

			s = sqrt(d > 0.0 ? d : 0.0) * 2.0;

			if (s > 1e-6)
			{
				x = 0.25 * s;
				w = (m32 - m23) / s;
				y = (m12 + m21) / s;
				z = (m13 + m31) / s;
			}
		}
		else if (m22 >= m33)
		{
			double const d = 1.0 + m22 - m11 - m33;

			s = sqrt(d > 0.0 ? d : 0.0) * 2.0;

			if (s > 1e-6)
			{
				y = 0.25 * s;
				w = (m13 - m31) / s;
				x = (m12 + m21) / s;
				z = (m23 + m32) / s;
			}
		}
		else
		{
			double const d = 1.0 + m33 - m11 - m22;

			s = sqrt(d > 0.0 ? d : 0.0) * 2.0;

			if (s > 1e-6)
			{
				z = 0.25 * s;
				w = (m21 - m12) / s;
				x = (m13 + m31) / s;
				y = (m23 + m32) / s;
			}
		}

		if (!(s > 1e-6))
		{
			x = y = z = 0.0;
			w = 1.0;
		}

		turn[0] = (float)-x;
		turn[1] = (float)-y;
		turn[2] = (float)-z;
		turn[3] = (float)w;
	}
}

namespace host
{
	bool Movable_MatrixAxes(int32_t const matrix[9], double axes[9])
	{
		double rows[9];

		for (int r = 0; r < 3; r++)
		{
			double const x = (double)matrix[r * 3] / 65536.0, y = (double)matrix[r * 3 + 1] / 65536.0, z = (double)matrix[r * 3 + 2] / 65536.0;
			double const length = sqrt(x * x + y * y + z * z);

			// 16.16 numbers can't be more than 2^15 long; a row about 0 long is no axis
			if (!(length > 1e-3))
			{
				return false;
			}

			rows[r * 3] = x / length;
			rows[r * 3 + 1] = y / length;
			rows[r * 3 + 2] = z / length;
		}

		for (int i = 0; i < 9; i++)
		{
			axes[i] = rows[i];
		}

		return true;
	}
}
