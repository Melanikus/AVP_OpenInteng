// See autolink_portal.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Every step below carries the address of the decompile it copies (dump-all-2.txt, FUN_004c4750 at 004c4750). The names follow the decompile's arrays: S4 = local_98[4..6]
// (A's "upper" slot, starts as A's maximum), S10 = local_98[10..12] (A's "lower" slot, starts as A's minimum), S13 = local_98[13..15] (B's upper, starts as B's maximum), S7 =
// local_98[7..9] (B's lower, starts as B's minimum).

// CHECKED line by line on dump-full-2 (61547-62369) by Vasquez 2026-11-16: every step matches (reassess/autolink-meet-vasquez.md AM-1..AM-9); its numbers are autolink_rules.h's.

#include "autolink_portal.h"
#include "autolink_rules.h"

#include <math.h>

namespace host
{
	namespace
	{
		int32_t const kWide = autolink_rules::kMeetWide;           // 0x4c48c1: an overlap above this is "wide" (30 m)
		int32_t const kNearPlane = autolink_rules::kPlaneNear;     // 0x4c4a94 / 0x4c4b83: a point within 0x3e9 - 1 of the contact plane
		int32_t const kFacing = autolink_rules::kNormalAlong;      // 0x4c4d5c, 0x4c4d81: a normal component beyond +-10000 (of 65536)
		int32_t const kMostCorners = 4;    // inteng's slots are 4 (named in the header)

		int32_t Abs(int64_t value)
		{
			return (int32_t)(value < 0 ? -value : value);
		}

		bool InList(std::vector<int32_t> const &list, int32_t pointNumber)
		{
			for (size_t i = 0; i < list.size(); i++)
			{
				if (list[i] == pointNumber)
				{
					return true;
				}
			}

			return false;
		}

		// The polygon's corners that are in the list of points on the contact plane (0x4c4c0e-0x4c4cd8): their number found, the corners found (slot per corner, -1 for none)
		// and how many corners the polygon has (capped at 4)
		size_t FoundCorners(PortalShape const &shape, size_t polygon, std::vector<int32_t> const &onPlane, int32_t slots[4], size_t *cornerCount)
		{
			size_t found = 0;

			slots[0] = slots[1] = slots[2] = slots[3] = -1;
			*cornerCount = 0;

			std::vector<int32_t> const &corners = shape.polygons[polygon];

			for (size_t j = 0; j < corners.size() && j < (size_t)kMostCorners; j++)
			{
				(*cornerCount)++;

				if (InList(onPlane, corners[j]))
				{
					slots[j] = corners[j];
					found++;
				}
			}

			return found;
		}

		bool PointOk(PortalShape const &shape, int32_t pointNumber)
		{
			return pointNumber >= 0 && (size_t)pointNumber * 3 + 2 < shape.points.size();
		}

		bool ShapeOk(PortalShape const &shape)
		{
			if (shape.normals.size() < shape.polygons.size() * 3)
			{
				return false;
			}

			for (size_t k = 0; k < shape.polygons.size(); k++)
			{
				for (size_t j = 0; j < shape.polygons[k].size(); j++)
				{
					if (!PointOk(shape, shape.polygons[k][j]))
					{
						return false;
					}
				}
			}

			return true;
		}

		// The first two passes (0x4c4c0e-0x4c4dde for A, 0x4c4e1b-0x4c5006 for B): a polygon with exactly two corners on the plane and a normal along an axis tightens the rectangle
		void TightenByWalls(PortalModule const &m, std::vector<int32_t> const &onPlane, int32_t lowerSlot[3], int32_t upperSlot[3])
		{
			PortalShape const &shape = *m.shape;

			for (size_t k = 0; k < shape.polygons.size(); k++)
			{
				int32_t slots[4];
				size_t corners = 0;

				if (FoundCorners(shape, k, onPlane, slots, &corners) != 2 || corners == 0)
				{
					continue;
				}

				for (size_t s = 0; s < corners; s++)
				{
					if (slots[s] == -1)
					{
						continue;
					}

					for (int a = 0; a < 3; a++)
					{
						int32_t const n = shape.normals[k * 3 + (size_t)a];
						int64_t const world = (int64_t)m.place[a] + shape.points[(size_t)slots[s] * 3 + (size_t)a];

						if (n < kFacing + 1)
						{
							if (n < -kFacing)
							{
								// the lower slot keeps the larger (0x4c4d81-0x4c4da1)
								if (world > lowerSlot[a])
								{
									lowerSlot[a] = (int32_t)world;
								}
							}
						}
						else
						{
							// the upper slot keeps the smaller (0x4c4d65-0x4c4d77)
							if (world < upperSlot[a])
							{
								upperSlot[a] = (int32_t)world;
							}
						}
					}
				}
			}
		}
	}

	int32_t AutoLinkPlace(int32_t file, double levelScale)
	{
		double const value = (double)file * (100.0 * levelScale);

		if (!(value > -2147483648.0 && value < 2147483647.0))
		{
			return value < 0 ? INT32_MIN : INT32_MAX;
		}

		return (int32_t)value;
	}

	int32_t AutoLinkNormal(float component)
	{
		double const value = (double)component * 65536.0;

		if (!(value > -2147483648.0 && value < 2147483647.0))
		{
			return value < 0 ? INT32_MIN : INT32_MAX;
		}

		return (int32_t)value;
	}

	bool AutoLink_ModulesMeet(PortalModule const &a, PortalModule const &b, Portal *out)
	{
		if (!out || !a.shape || !b.shape || !ShapeOk(*a.shape) || !ShapeOk(*b.shape))
		{
			return false;
		}

		// The boxes (0x4c479b-0x4c4834): place + the extents
		int32_t aMax[3], aMin[3], bMax[3], bMin[3];

		for (int i = 0; i < 3; i++)
		{
			aMax[i] = a.place[i] + a.extents[2 * i];
			aMin[i] = a.place[i] + a.extents[2 * i + 1];
			bMax[i] = b.place[i] + b.extents[2 * i];
			bMin[i] = b.place[i] + b.extents[2 * i + 1];
		}

		// The overlap per axis (0x4c4858-0x4c4895)
		int32_t overlap[3];

		for (int i = 0; i < 3; i++)
		{
			int32_t const top = bMax[i] <= aMax[i] ? bMax[i] : aMax[i];
			int32_t const bottom = bMin[i] < aMin[i] ? aMin[i] : bMin[i];

			overlap[i] = top - bottom;
		}

		// The wide axes and the contact axis (0x4c489e-0x4c4907)
		int wide = 0;
		int32_t least = overlap[0];
		int axis = 0;

		for (int i = 0; i < 3; i++)
		{
			if (overlap[i] > kWide)
			{
				wide++;
			}

			if (overlap[i] < least)
			{
				least = overlap[i];
				axis = i;
			}
		}

		if (wide < autolink_rules::kMeetWideAxes || least < autolink_rules::kMeetGap)
		{
			return false;
		}

		// The direction and the contact planes (0x4c4924-0x4c4963)
		int32_t planeA = aMin[axis];
		int32_t planeB = bMax[axis];
		int32_t direction = -1;

		if (planeB - planeA != least)
		{
			direction = 1;
			planeA = aMax[axis];
			planeB = bMin[axis];
		}

		// The points near the contact plane, A's and B's (0x4c4a64-0x4c4b4b)
		std::vector<int32_t> aOn, bOn;

		for (size_t i = 0; i * 3 + 2 < a.shape->points.size(); i++)
		{
			int64_t const away = ((int64_t)a.shape->points[i * 3 + (size_t)axis] - planeA) + a.place[axis];

			if (Abs(away) < kNearPlane + 1)
			{
				aOn.push_back((int32_t)i);
			}
		}

		for (size_t i = 0; i * 3 + 2 < b.shape->points.size(); i++)
		{
			int64_t const away = ((int64_t)b.place[axis] - planeB) + b.shape->points[i * 3 + (size_t)axis];

			if (Abs(away) < kNearPlane + 1)
			{
				bOn.push_back((int32_t)i);
			}
		}

		// The slots (0x4c4a16 and the start): A's upper = A's maximum, A's lower = A's minimum, B's upper = B's maximum, B's lower = B's minimum
		int32_t s4[3], s10[3], s13[3], s7[3];

		for (int i = 0; i < 3; i++)
		{
			s4[i] = aMax[i];
			s10[i] = aMin[i];
			s13[i] = bMax[i];
			s7[i] = bMin[i];
		}

		TightenByWalls(a, aOn, s10, s4);
		TightenByWalls(b, bOn, s7, s13);

		// The merge (0x4c5023-0x4c50d8)
		for (int i = 0; i < 3; i++)
		{
			if (s4[i] == aMax[i])
			{
				s4[i] = aMin[i];
			}

			if (s10[i] == aMin[i])
			{
				s10[i] = aMax[i];
			}

			if (s13[i] == bMax[i])
			{
				s13[i] = bMin[i];
			}

			if (s7[i] == bMin[i])
			{
				s7[i] = bMax[i];
			}

			int32_t const upper = s4[i] <= s13[i] ? s13[i] : s4[i];
			int32_t const lower = s7[i] <= s10[i] ? s7[i] : s10[i];

			s4[i] = s13[i] = upper;
			s10[i] = s7[i] = lower;
		}

		// The narrowing passes (0x4c50e2-0x4c53a8 for A, 0x4c53b7-0x4c5679 for B): the polygon with two or more corners on the plane narrows the rectangle on the axis it overlaps most
		for (int pass = 0; pass < 2; pass++)
		{
			PortalModule const &m = pass == 0 ? a : b;
			std::vector<int32_t> const &onPlane = pass == 0 ? aOn : bOn;
			PortalShape const &shape = *m.shape;

			for (size_t k = 0; k < shape.polygons.size(); k++)
			{
				int32_t slots[4];
				size_t corners = 0;

				if (FoundCorners(shape, k, onPlane, slots, &corners) < 2)
				{
					continue;
				}

				int64_t const box = autolink_rules::kCornerBoxStart;
				int64_t pMin[3] = { box, box, box };
				int64_t pMax[3] = { -box, -box, -box };

				for (size_t s = 0; s < corners; s++)
				{
					if (slots[s] == -1)
					{
						continue;
					}

					for (int i = 0; i < 3; i++)
					{
						int64_t const world = (int64_t)m.place[i] + shape.points[(size_t)slots[s] * 3 + (size_t)i];

						if (world <= pMin[i])
						{
							pMin[i] = world;
						}

						if (world >= pMax[i])
						{
							pMax[i] = world;
						}
					}
				}

				int best = -1;
				double bestRatio = autolink_rules::kNarrowRatioStart;
				int64_t clipMin[3] = { 0, 0, 0 };
				int64_t clipMax[3] = { 0, 0, 0 };

				for (int i = 0; i < 3; i++)
				{
					if (i == axis)
					{
						continue;
					}

					int32_t const upperSlot = pass == 0 ? s10[i] : s7[i];    // A: the lower slot s10 is the one called "hi" below, B: s7
					int32_t const lowerSlot = pass == 0 ? s4[i] : s13[i];

					if (upperSlot == lowerSlot)
					{
						continue;
					}

					clipMin[i] = pMin[i] <= lowerSlot ? lowerSlot : pMin[i];
					clipMax[i] = upperSlot <= pMax[i] ? upperSlot : pMax[i];

					double const span = (double)((int64_t)upperSlot - lowerSlot);
					double first;
					double second;

					if (pass == 0)
					{
						first = (double)(clipMax[i] - lowerSlot) / span;
						second = (double)((int64_t)upperSlot - clipMin[i]) / span;
					}
					else
					{
						first = (double)((int64_t)s10[i] - clipMin[i]) / span;
						second = (double)(clipMax[i] - (int64_t)s4[i]) / span;
					}

					double const ratio = first < second ? first : second;

					if (pass == 0 ? (bestRatio > ratio) : (ratio < bestRatio))
					{
						best = i;
						bestRatio = ratio;
					}
				}

				if (best != -1)
				{
					int32_t &hiSlot = pass == 0 ? s10[best] : s7[best];
					int32_t &loSlot = pass == 0 ? s4[best] : s13[best];

					if (hiSlot != loSlot)
					{
						int64_t const cmin = clipMin[best];
						int64_t const cmax = clipMax[best];

						if (cmax - loSlot < (int64_t)hiSlot - cmin)
						{
							loSlot = (int32_t)(cmax <= loSlot ? loSlot : cmax);
						}
						else
						{
							hiSlot = (int32_t)(cmin < hiSlot ? cmin : hiSlot);
						}
					}
				}
			}
		}

		// The last tests (0x4c5694-0x4c56e4): on each axis but the contact axis, the rectangle must be wider than 1000
		for (int i = 0; i < 3; i++)
		{
			if (i == axis)
			{
				continue;
			}

			int32_t const upper = s13[i] < s4[i] ? s4[i] : s13[i];
			int32_t const lower = s7[i] <= s10[i] ? s7[i] : s10[i];

			s4[i] = upper;
			s10[i] = lower;

			if ((int64_t)lower - autolink_rules::kPortalWide <= upper)
			{
				return false;
			}
		}

		for (int i = 0; i < 3; i++)
		{
			out->lo[i] = s4[i];
			out->hi[i] = s10[i];
		}

		out->planeA = planeA;
		out->planeB = planeB;
		out->axis = axis;
		out->direction = direction;
		return true;
	}
}
