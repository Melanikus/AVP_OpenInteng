// See waypoint_pick.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "waypoint_pick.h"

#include <math.h>

namespace host
{
	namespace
	{
		double World(int64_t file, double scale)
		{
			// as the view draws it (waypoint_view.cpp World): toward zero, held inside an int
			double const value = (double)file * scale;

			return value > 2147483000.0 ? 2147483000.0 : (value < -2147483000.0 ? -2147483000.0 : (double)(int32_t)value);
		}

		int32_t File(double world, double scale)
		{
			double const value = floor(world / scale);

			return value > 2147483000.0 ? 2147483000 : (value < -2147483000.0 ? -2147483000 : (int32_t)value);
		}

		double Scale(double levelScale)
		{
			return levelScale > 0.0 ? levelScale : 1.0;
		}
	}

	void WaypointPick_WorldBox(WaypointRec const &wp, double levelScale, bool minimisedCube, double lo[3], double hi[3])
	{
		double const scale = Scale(levelScale);

		for (int i = 0; i < 3; i++)
		{
			double const aim = World(wp.centre[i], scale);

			lo[i] = minimisedCube ? aim - kWaypointPickMinimisedHalf : World(wp.min[i], scale);
			hi[i] = minimisedCube ? aim + kWaypointPickMinimisedHalf : World(wp.max[i], scale);

			if (lo[i] > hi[i])
			{
				double const swap = lo[i];

				lo[i] = hi[i];
				hi[i] = swap;
			}
		}
	}

	bool WaypointPick_RayEnters(double const from[3], double const direction[3], double const lo[3], double const hi[3], double *distance)
	{
		double const length = sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);

		if (!(length > 0.0))
		{
			return false;
		}

		double enter = -1e300, leave = 1e300;
		bool inside = true;

		for (int i = 0; i < 3; i++)
		{
			double const d = direction[i] / length;

			inside = inside && from[i] >= lo[i] && from[i] <= hi[i];

			if (fabs(d) < 1e-12)
			{
				if (from[i] < lo[i] || from[i] > hi[i])
				{
					return false;
				}

				continue;
			}

			double a = (lo[i] - from[i]) / d;
			double b = (hi[i] - from[i]) / d;

			if (a > b)
			{
				double const swap = a;

				a = b;
				b = swap;
			}

			enter = a > enter ? a : enter;
			leave = b < leave ? b : leave;
		}

		if (inside || enter > leave || enter < 0.0)
		{
			return false;
		}

		*distance = enter;
		return true;
	}

	int32_t WaypointPick_BoxOnRay(std::vector<WaypointRec> const &list, double levelScale, bool minimised, int32_t current, double const from[3], double const direction[3], double wallAt)
	{
		int32_t best = -1;
		double bestDistance = 0.0;

		for (size_t w = 0; w < list.size(); w++)
		{
			double lo[3], hi[3], distance = 0.0;

			WaypointPick_WorldBox(list[w], levelScale, minimised && (int32_t)w != current, lo, hi);

			if (!WaypointPick_RayEnters(from, direction, lo, hi, &distance) || (wallAt >= 0.0 && distance > wallAt + kWaypointPickPastWall))
			{
				continue;
			}

			if (best < 0 || distance < bestDistance)
			{
				best = (int32_t)w;
				bestDistance = distance;
			}
		}

		return best;
	}

	bool WaypointPick_IsFloor(int32_t const normal[3])
	{
		double const x = normal[0], y = normal[1], z = normal[2];
		double const length = sqrt(x * x + y * y + z * z);

		// up is -y; within 45 degrees: -y / length at least cos 45
		return length > 0.0 && -y / length >= 0.70710678;
	}

	void WaypointPick_AimWorld(WaypointRec const &wp, double levelScale, int32_t out[3])
	{
		double const scale = Scale(levelScale);

		for (int i = 0; i < 3; i++)
		{
			out[i] = (int32_t)World(wp.centre[i], scale);
		}
	}

	double WaypointPick_SegmentPixels(double ax, double ay, double bx, double by, double px, double py)
	{
		double const dx = bx - ax, dy = by - ay;
		double const length2 = dx * dx + dy * dy;
		double t = length2 > 0.0 ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0.0;

		t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);

		double const x = ax + t * dx - px, y = ay + t * dy - py;

		return sqrt(x * x + y * y);
	}

	bool WaypointPick_NearLink(double ax, double ay, double bx, double by, double px, double py, double *pixels)
	{
		double const toLine = WaypointPick_SegmentPixels(ax, ay, bx, by, px, py);
		double const toA = sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
		double const toB = sqrt((px - bx) * (px - bx) + (py - by) * (py - by));

		if (toLine > kWaypointPickLinkPixels || toA < kWaypointPickLinkEnds || toB < kWaypointPickLinkEnds)
		{
			return false;
		}

		if (pixels)
		{
			*pixels = toLine;
		}

		return true;
	}

	int32_t WaypointPick_NextLinkFlags(int32_t flags)
	{
		static int32_t const cycle[] = { 0, 1, 2, 4, 5, 6, 8, 9, 10 };
		size_t const count = sizeof(cycle) / sizeof(cycle[0]);

		for (size_t i = 0; i < count; i++)
		{
			if (cycle[i] == flags)
			{
				return cycle[(i + 1) % count];
			}
		}

		return 0;
	}

	WaypointRec WaypointPick_NewBox(int32_t const hit[3], double levelScale)
	{
		double const scale = Scale(levelScale);
		WaypointRec made;
		double const lo[3] = { (double)hit[0] - kWaypointNewHalfWidth, (double)hit[1] - kWaypointNewHeight, (double)hit[2] - kWaypointNewHalfWidth };
		double const hi[3] = { (double)hit[0] + kWaypointNewHalfWidth, (double)hit[1], (double)hit[2] + kWaypointNewHalfWidth };

		for (int i = 0; i < 3; i++)
		{
			made.min[i] = File(lo[i], scale);
			made.max[i] = File(hi[i], scale);
			made.centre[i] = (int32_t)(((int64_t)made.min[i] + made.max[i]) / 2);
		}

		made.flags = 0;
		made.spare2 = 0;
		return made;
	}
}
