// See uv_project.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "uv_project.h"

#include <math.h>

namespace model
{
	namespace
	{
		// the x87's fistp in its normal mode: nearest, ties to even (FUN_004fd740)
		int32_t RoundHalfEven(double x)
		{
			double const floor_ = floor(x);
			double const fraction = x - floor_;
			double result = floor_;

			if (fraction > 0.5 || (fraction == 0.5 && fmod(floor_, 2.0) != 0.0))
			{
				result = floor_ + 1.0;
			}

			return (int32_t)result;
		}

		// 32-bit wrap for the game's whole-number adds and subtracts (no signed overflow here)
		int32_t Add(int32_t a, int32_t b)
		{
			return (int32_t)((uint32_t)a + (uint32_t)b);
		}

		int32_t Sub(int32_t a, int32_t b)
		{
			return (int32_t)((uint32_t)a - (uint32_t)b);
		}

		Fix3 Difference(Fix3 const &a, Fix3 const &b)
		{
			Fix3 r;

			r.x = Sub(a.x, b.x);
			r.y = Sub(a.y, b.y);
			r.z = Sub(a.z, b.z);
			return r;
		}

		bool TexelOk(float value)
		{
			return value >= -kMostTexel && value <= kMostTexel;
		}
	}

	int32_t Fixed16Multiply(int32_t a, int32_t b)
	{
		int64_t const product = (int64_t)a * (int64_t)b;

		// (low 32 bits >> 16) | (high 32 bits << 16): bits 16..47 of the product
		return (int32_t)(uint32_t)(((uint64_t)product >> 16) & 0xffffffffu);
	}

	int32_t FixedDot(Fix3 const &a, Fix3 const &b)
	{
		return Add(Add(Fixed16Multiply(a.x, b.x), Fixed16Multiply(a.y, b.y)), Fixed16Multiply(a.z, b.z));
	}

	Fix3 FixedCross(Fix3 const &a, Fix3 const &b)
	{
		Fix3 r;

		r.x = Sub(Fixed16Multiply(a.y, b.z), Fixed16Multiply(a.z, b.y));
		r.y = Sub(Fixed16Multiply(a.z, b.x), Fixed16Multiply(a.x, b.z));
		r.z = Sub(Fixed16Multiply(a.x, b.y), Fixed16Multiply(a.y, b.x));
		return r;
	}

	bool FixedNormalise(Fix3 *v)
	{
		double const x = (double)(float)v->x;
		double const y = (double)(float)v->y;
		double const z = (double)(float)v->z;
		double const length = sqrt(x * x + y * y + z * z);

		if (!(length > 0.0))
		{
			return false;
		}

		// each quotient is stored as a 32-bit float (_DAT_008f5824) before the rounding
		v->x = RoundHalfEven((double)(float)((x * 65536.0) / length));
		v->y = RoundHalfEven((double)(float)((y * 65536.0) / length));
		v->z = RoundHalfEven((double)(float)((z * 65536.0) / length));
		return true;
	}

	bool FixedDifferenceNormalised(Fix3 const &a, Fix3 const &b, Fix3 *out)
	{
		Fix3 d = Difference(a, b);

		if (!FixedNormalise(&d))
		{
			return false;
		}

		*out = d;
		return true;
	}

	bool TriangleIsOriented(Fix3 const &p0, Fix3 const &p1, Fix3 const &p2, Fix3 const &normal)
	{
		Fix3 const first = Difference(p1, p0);
		Fix3 const second = Difference(p2, p0);
		Fix3 across = FixedCross(first, normal);

		if (!FixedNormalise(&across))
		{
			return false;
		}

		return FixedDot(across, second) > 0;
	}

	UvModelResult ProjectionBasis(Fix3 const points[3], bool oriented, PlaneBasis *out)
	{
		if (!points || !out)
		{
			return UvModelResult::BadCall;
		}

		Fix3 axis1 = {};
		Fix3 axis2 = {};
		int best = 0;
		int32_t bestDot = 2000000000;
		bool perpendicular = false;   // a pair of edges at exactly 90 degrees was found: the search stopped there

		// not oriented: axis 1 along the edge to the corner before, axis 2 along the edge to the next; oriented: the other way round (FUN_00570590 0x5708a9 and 0x5707c3)
		for (int i = 0; i < 3; i++)
		{
			Fix3 const &from = points[i];
			Fix3 const &toFirst = points[(i + (oriented ? 1 : 2)) % 3];
			Fix3 const &toSecond = points[(i + (oriented ? 2 : 1)) % 3];

			if (!FixedDifferenceNormalised(toFirst, from, &axis1) || !FixedDifferenceNormalised(toSecond, from, &axis2))
			{
				return UvModelResult::BadPlane;
			}

			int32_t dot = FixedDot(axis1, axis2);

			if (dot == 0)
			{
				perpendicular = true;
				break;
			}

			if (dot < 0)
			{
				dot = Sub(0, dot);
			}

			if (dot < bestDot)
			{
				best = i;
				bestDot = dot;
			}
		}

		Fix3 normal = FixedCross(axis1, axis2);

		if (!FixedNormalise(&normal))
		{
			return UvModelResult::BadPlane;
		}

		if (!perpendicular)
		{
			// axis 1 again, from the chosen corner: (best + 2) % 3 when oriented, (best + 1) % 3 when not (0x57086b and 0x57094c)
			if (!FixedDifferenceNormalised(points[(best + (oriented ? 2 : 1)) % 3], points[best], &axis1))
			{
				return UvModelResult::BadPlane;
			}
		}

		axis2 = FixedCross(normal, axis1);

		if (!FixedNormalise(&axis2))
		{
			return UvModelResult::BadPlane;
		}

		out->axis1 = axis1;
		out->axis2 = axis2;
		out->origin = points[0];
		return UvModelResult::Ok;
	}

	UvModelResult ProjectCorners(bool tile, PlaneBasis const &plane, WindowCorners const &window, TextureWindow const &rect, TileSettings const &tiles,
		std::vector<Fix3> const &worldPoints, std::vector<UvCorner> *out)
	{
		if (!out)
		{
			return UvModelResult::BadCall;
		}

		// the window's two sides from its first corner (FUN_00570590 0x5705f1-0x5706a0)
		double const du01 = (double)(float)(window.u[1] - window.u[0]);
		double const dv01 = (double)(float)(window.v[1] - window.v[0]);
		double const du03 = (double)(float)(window.u[3] - window.u[0]);
		double const dv03 = (double)(float)(window.v[3] - window.v[0]);
		double const length01 = sqrt(du01 * du01 + dv01 * dv01);
		double const length03 = sqrt(du03 * du03 + dv03 * dv03);

		if (!(length01 > 0.0) || !(length03 > 0.0))
		{
			return UvModelResult::BadWindow;
		}

		if (tile && (tiles.uScale == 0 || tiles.vScale == 0))
		{
			return UvModelResult::BadWindow;
		}

		double const c1 = (double)(float)(du01 / length01);   // along side 1: the u and v parts
		double const s1 = (double)(float)(dv01 / length01);
		double const c3 = (double)(float)(du03 / length03);   // along side 3
		double const s3 = (double)(float)(dv03 / length03);

		int32_t minU = 10000, maxU = -10000, minV = 10000, maxV = -10000;

		for (int j = 0; j < 4; j++)
		{
			if (window.u[j] < minU) minU = window.u[j];
			if (window.u[j] > maxU) maxU = window.u[j];
			if (window.v[j] < minV) minV = window.v[j];
			if (window.v[j] > maxV) maxV = window.v[j];
		}

		double const centreU = (double)(float)(((double)(float)maxU + (double)(float)minU) * 0.5);
		double const centreV = (double)(float)(((double)(float)maxV + (double)(float)minV) * 0.5);

		// tile mode takes the projection of the origin off (0x5709ca-0x5709f6): floats of whole numbers, taken back as whole numbers
		int32_t offsetU = 0, offsetV = 0;

		if (tile)
		{
			offsetU = (int32_t)(float)FixedDot(plane.axis1, plane.origin);
			offsetV = (int32_t)(float)FixedDot(plane.axis2, plane.origin);
		}

		std::vector<int32_t> us(worldPoints.size()), vs(worldPoints.size());
		int32_t selMinU = 2000000000, selMaxU = -2000000000, selMinV = 2000000000, selMaxV = -2000000000;

		for (size_t i = 0; i < worldPoints.size(); i++)
		{
			int32_t u = FixedDot(plane.axis1, worldPoints[i]);
			int32_t v = FixedDot(plane.axis2, worldPoints[i]);

			if (tile)
			{
				u = Sub(u, offsetU);
				v = Sub(v, offsetV);
			}
			else
			{
				if (u < selMinU) selMinU = u;
				if (u > selMaxU) selMaxU = u;
				if (v < selMinV) selMinV = v;
				if (v > selMaxV) selMaxV = v;
			}

			us[i] = u;
			vs[i] = v;
		}

		// wrap's scale (0x570c2c-0x570c70): the window's side plus a half over the spread of the selection, 0 for no spread
		double const half = 0.5;
		double const scaleU = selMinU == selMaxU ? 0.0 : (double)(float)((length01 + half) / (double)(float)(selMaxU - selMinU));
		double const scaleV = selMinV == selMaxV ? 0.0 : (double)(float)((length03 + half) / (double)(float)(selMaxV - selMinV));
		int32_t const middleU = (selMaxU + selMinU) / 2;
		int32_t const middleV = (selMaxV + selMinV) / 2;

		std::vector<UvCorner> made(worldPoints.size());

		for (size_t i = 0; i < worldPoints.size(); i++)
		{
			int32_t newU;
			int32_t newV;

			if (!tile)
			{
				double const x = ((double)(float)us[i] - (double)(float)middleU) * scaleU;
				double const y = ((double)(float)vs[i] - (double)(float)middleV) * scaleV;

				newU = RoundHalfTowardZero((double)(float)(x * c1 + y * c3 + centreU));
				newV = RoundHalfTowardZero((double)(float)(x * s1 + y * s3 + centreV));

				// held to the window's rectangle: at most right - 1 and bottom - 1, then at least left and top (0x570d84-0x570dc6)
				if (newU > rect.right - 1) newU = rect.right - 1;
				if (newV > rect.bottom - 1) newV = rect.bottom - 1;
				if (newU < rect.left) newU = rect.left;
				if (newV < rect.top) newV = rect.top;
			}
			else
			{
				double const u = (double)(float)us[i];
				double const v = (double)(float)vs[i];

				newU = RoundHalfTowardZero((double)(float)(((u * c1 + v * c3) / (double)(float)tiles.uScale) + (double)(float)tiles.uOffset));
				newV = RoundHalfTowardZero((double)(float)(((u * s1 + v * s3) / (double)(float)tiles.vScale) + (double)(float)tiles.vOffset));
			}

			made[i].u = (float)newU;
			made[i].v = (float)newV;

			if (!TexelOk(made[i].u) || !TexelOk(made[i].v))
			{
				return UvModelResult::BadTexel;
			}
		}

		out->swap(made);
		return UvModelResult::Ok;
	}
}
