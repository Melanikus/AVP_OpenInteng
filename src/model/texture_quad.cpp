// texture_quad.cpp: the Texture Map's shared maths on a merged pair (see texture_quad.h). Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_quad.h"

#include <math.h>
#include <string.h>

namespace model
{
	namespace
	{
		// FUN_00499b30: the 16.16 product, the 64-bit product's bits 16..47
		int32_t MulFixed(int32_t a, int32_t b)
		{
			return (int32_t)(uint32_t)(((int64_t)a * (int64_t)b) >> 16);
		}

		// FIST with the x87 rounding to the nearest (ties to even): a value that is not a number or doesn't fit gives the x87's 0x80000000
		int32_t Fist(double value)
		{
			if (!(value > -2147483648.5 && value < 2147483647.5))
			{
				return INT32_MIN;
			}

			double const rounded = nearbyint(value);

			if (!(rounded >= -2147483648.0 && rounded <= 2147483647.0))
			{
				return INT32_MIN;
			}

			return (int32_t)rounded;
		}

		// __ftol (0x59008c-0x5900ae): FISTP qword with the rounding set to cut toward zero, and the LOW 32 bits of that 64-bit number given back. A value that is
		// not a number or doesn't fit 64 bits is the x87's 0x8000000000000000, whose low 32 bits are 0
		int32_t Ftol(double value)
		{
			double const cut = trunc(value);

			if (!(cut >= -9223372036854775808.0 && cut < 9223372036854775808.0))
			{
				return 0;
			}

			return (int32_t)(uint32_t)(uint64_t)(int64_t)cut;
		}

		int32_t Minus(int32_t a, int32_t b)
		{
			return (int32_t)((uint32_t)a - (uint32_t)b);
		}

		void Alone(ShapePolygon const &polygon, TextureQuad *out)
		{
			// 0x55ce2b-0x55ce78: the corners 2, 1, 0
			out->points[0] = polygon.corners[2];
			out->points[1] = polygon.corners[1];
			out->points[2] = polygon.corners[0];
			out->points[3] = -1;
			out->ownCorners[0] = 2;
			out->ownCorners[1] = 1;
			out->ownCorners[2] = 0;
			out->ownCorners[3] = -1;

			for (int i = 0; i < 4; i++)
			{
				out->partnerCorners[i] = -1;
			}
		}

		bool IsOne(ShapeModel const &shape, int32_t polygon)
		{
			return polygon >= 0 && (size_t)polygon < shape.polygons.size();
		}

		// Inteng's 65536ths of a normal (its copy_to_shapeheader, FUN_004ff010 0x4ffa4f-0x4ffa8a: FLD float, FMUL 65536.0f, __ftol)
		int32_t NormalPart(float part)
		{
			return Ftol((double)part * 65536.0);
		}

		// The u (0) or v (1) of a corner as inteng's whole number: cut toward 0 (FUN_0059008c, as uv_model's commands)
		int32_t Whole(UvCorner const &corner, int which)
		{
			return Ftol((double)(which == 0 ? corner.u : corner.v));
		}

		// 0x566c9e-0x566d03: how far the smallest coordinate moves, by whole picture sizes, to be in [0, size)
		int32_t MoveOnto(int32_t smallest, int32_t size)
		{
			int32_t rest = 0;

			if (smallest < size)
			{
				if (smallest >= 0)
				{
					return 0;
				}

				rest = smallest % size;

				if (rest < 0)
				{
					rest += size;
				}
			}
			else
			{
				rest = smallest % size;
			}

			return rest - smallest;
		}
	}

	TextureQuadResult TextureQuadOf(ShapeModel const &shape, int32_t polygon, int32_t partner, TextureQuad *out)
	{
		if (!out || !IsOne(shape, polygon))
		{
			return TextureQuadResult::NoPolygon;
		}

		ShapePolygon const &own = shape.polygons[(size_t)polygon];

		if (own.cornerCount != 3)
		{
			return TextureQuadResult::NotATriangle;
		}

		if (partner == -1)
		{
			Alone(own, out);
			return TextureQuadResult::Ok;
		}

		if (!IsOne(shape, partner) || partner == polygon || shape.polygons[(size_t)partner].cornerCount != 3)
		{
			Alone(own, out);
			return TextureQuadResult::Alone;
		}

		ShapePolygon const &other = shape.polygons[(size_t)partner];

		// 0x55ce83-0x55cec4: each of the polygon's corners' place in the partner (the last that matches), -1 where the partner hasn't it; the last corner the partner hasn't
		int32_t placeInPartner[3] = { -1, -1, -1 };
		int32_t unshared = -1;

		for (int k = 0; k < 3; k++)
		{
			for (int j = 0; j < 3; j++)
			{
				if (own.corners[k] == other.corners[j])
				{
					placeInPartner[k] = j;
				}
			}

			if (placeInPartner[k] == -1)
			{
				unshared = k;
			}
		}

		if (unshared == -1)
		{
			// INTENG BUG, FIXED (TextureQuadResult::Alone): every corner shared leaves inteng's local_14 unwritten
			Alone(own, out);
			return TextureQuadResult::Alone;
		}

		// 0x55cec6-0x55cedb: the partner's first place that none of the polygon's corners is at
		int32_t partnersOwn = 0;

		for (; partnersOwn < 3; partnersOwn++)
		{
			bool found = false;

			for (int k = 0; k < 3; k++)
			{
				found = found || placeInPartner[k] == partnersOwn;
			}

			if (!found)
			{
				break;
			}
		}

		if (partnersOwn == 3)
		{
			// only when all three are shared, which is refused above
			Alone(own, out);
			return TextureQuadResult::Alone;
		}

		// 0x55ceec-0x55cf93
		bool const forward = TextureTurnsForward(shape, polygon);
		int32_t const second = forward ? (unshared + 1) % 3 : (unshared + 2) % 3;
		int32_t const fourth = forward ? (unshared + 2) % 3 : (unshared + 1) % 3;

		out->points[0] = own.corners[unshared];
		out->points[1] = own.corners[second];
		out->points[2] = other.corners[partnersOwn];
		out->points[3] = own.corners[fourth];
		out->ownCorners[0] = unshared;
		out->ownCorners[1] = second;
		out->ownCorners[2] = -1;
		out->ownCorners[3] = fourth;
		out->partnerCorners[0] = -1;
		out->partnerCorners[1] = placeInPartner[second];
		out->partnerCorners[2] = partnersOwn;
		out->partnerCorners[3] = placeInPartner[fourth];
		return TextureQuadResult::Ok;
	}

	bool TextureTurnsForward(ShapeModel const &shape, int32_t polygon)
	{
		if (!IsOne(shape, polygon))
		{
			return false;
		}

		ShapePolygon const &own = shape.polygons[(size_t)polygon];

		if (own.cornerCount < 3)
		{
			return false;
		}

		for (int k = 0; k < 3; k++)
		{
			if (own.corners[k] < 0 || (size_t)own.corners[k] >= shape.points.size())
			{
				return false;
			}
		}

		ShapePoint const &p0 = shape.points[(size_t)own.corners[0]];
		ShapePoint const &p1 = shape.points[(size_t)own.corners[1]];
		ShapePoint const &p2 = shape.points[(size_t)own.corners[2]];

		// FUN_004b3830 twice: corner 1 - corner 0, corner 2 - corner 0
		int32_t const a[3] = { Minus(p1.x, p0.x), Minus(p1.y, p0.y), Minus(p1.z, p0.z) };
		int32_t const b[3] = { Minus(p2.x, p0.x), Minus(p2.y, p0.y), Minus(p2.z, p0.z) };
		int32_t n[3] = { 0, 0, 0 };

		if ((size_t)polygon < shape.polygonNormals.size())
		{
			ShapeNormal const &normal = shape.polygonNormals[(size_t)polygon];

			n[0] = NormalPart(normal.x);
			n[1] = NormalPart(normal.y);
			n[2] = NormalPart(normal.z);
		}

		// FUN_004b5650: a x n
		int32_t c[3] = { Minus(MulFixed(a[1], n[2]), MulFixed(a[2], n[1])), Minus(MulFixed(a[2], n[0]), MulFixed(a[0], n[2])), Minus(MulFixed(a[0], n[1]), MulFixed(a[1], n[0])) };

		TextureNormaliseTo65536(c);

		// FUN_004b3170
		uint32_t const dot = (uint32_t)MulFixed(c[2], b[2]) + (uint32_t)MulFixed(c[0], b[0]) + (uint32_t)MulFixed(c[1], b[1]);

		return (int32_t)dot > 0;
	}

	void TextureNormaliseTo65536(int32_t v[3])
	{
		if (!v)
		{
			return;
		}

		double const x = (double)v[0];
		float const fy = (float)v[1];
		double const z = (double)v[2];
		float const fz = (float)v[2];

		// 0x4fd764-0x4fd77e: (z . fz + fy . fy) + x . x, its square root stored as a float
		double const sum = (z * (double)fz + (double)fy * (double)fy) + x * x;
		float const length = (float)sqrt(sum);

		// 0x4fd782-0x4fd7d4: each part x 65536 / the length, stored as a float, then FIST
		v[0] = Fist((double)(float)(x * 65536.0 / (double)length));
		v[1] = Fist((double)(float)((double)fy * 65536.0 / (double)length));
		v[2] = Fist((double)(float)((double)fz * 65536.0 / (double)length));
	}

	UvModelResult TextureWrapUv(UvList *own, int ownCorners, UvList *partner, int partnerCorners, int32_t width, int32_t height, bool *moved)
	{
		if (!own || !moved || ownCorners < 0 || (partner && partnerCorners < 0))
		{
			return UvModelResult::BadCall;
		}

		*moved = false;

		if (width <= 0 || height <= 0)
		{
			return UvModelResult::BadWindow;
		}

		UvList *const lists[2] = { own, partner };
		int used[2] = { ownCorners, partner ? partnerCorners : 0 };

		for (int l = 0; l < 2; l++)
		{
			UvList const *const list = lists[l];

			if (!list)
			{
				continue;
			}

			if (list->cornerCount < 0 || list->cornerCount > kMostUvCorners)
			{
				return UvModelResult::BadCornerCount;
			}

			// only the corners the polygon has (inteng's record holds three: FUN_0055d770 0x55d9ca-0x55d9ee copies 0x18 bytes of the list)
			used[l] = used[l] < list->cornerCount ? used[l] : list->cornerCount;

			for (int i = 0; i < used[l]; i++)
			{
				float const u = list->corners[i].u, v = list->corners[i].v;

				if (!(u >= -kMostTexel && u <= kMostTexel && v >= -kMostTexel && v <= kMostTexel))
				{
					return UvModelResult::BadTexel;   // the texel rule, as the readers keep it: so every sum below stays small
				}
			}
		}

		// 0x566c44-0x566c94: the smallest u and v over both (100000000 to start with, as inteng's)
		int32_t smallestU = 100000000, smallestV = 100000000;
		bool any = false;

		for (int l = 0; l < 2; l++)
		{
			for (int i = 0; lists[l] && i < used[l]; i++)
			{
				int32_t const u = Whole(lists[l]->corners[i], 0);
				int32_t const v = Whole(lists[l]->corners[i], 1);

				smallestU = u <= smallestU ? u : smallestU;
				smallestV = v <= smallestV ? v : smallestV;
				any = true;
			}
		}

		if (!any)
		{
			return UvModelResult::Ok;
		}

		int32_t const du = MoveOnto(smallestU, width);
		int32_t const dv = MoveOnto(smallestV, height);

		if (du == 0 && dv == 0)
		{
			return UvModelResult::Ok;
		}

		// 0x566d05-0x566d4f: every corner moved by the same; nothing is changed if one would go past the texel rule
		UvList made[2];

		for (int l = 0; l < 2; l++)
		{
			if (!lists[l])
			{
				continue;
			}

			made[l] = *lists[l];

			for (int i = 0; i < used[l]; i++)
			{
				int64_t const u = (int64_t)Whole(made[l].corners[i], 0) + du;
				int64_t const v = (int64_t)Whole(made[l].corners[i], 1) + dv;

				if (u < -(int64_t)kMostTexel || u > (int64_t)kMostTexel || v < -(int64_t)kMostTexel || v > (int64_t)kMostTexel)
				{
					return UvModelResult::BadTexel;
				}

				made[l].corners[i].u = (float)u;
				made[l].corners[i].v = (float)v;
			}
		}

		for (int l = 0; l < 2; l++)
		{
			if (lists[l])
			{
				*lists[l] = made[l];
			}
		}

		*moved = true;
		return UvModelResult::Ok;
	}
}
