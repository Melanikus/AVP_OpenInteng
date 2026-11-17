// See vertex_pick.h. Written 2026-11-17 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "vertex_pick.h"

#include <math.h>

namespace host
{
	namespace
	{
		// The line against one triangle (the sums of Moller and Trumbore, in doubles): true when it crosses it ahead of where it starts, from either side
		bool HitsTriangle(PickRay const &ray, double const a[3], double const b[3], double const c[3])
		{
			double const d[3] = { (double)ray.direction[0], (double)ray.direction[1], (double)ray.direction[2] };
			double const o[3] = { (double)ray.from[0], (double)ray.from[1], (double)ray.from[2] };
			double const e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
			double const e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
			double const p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
			double const det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];

			if (fabs(det) < 1e-9)
			{
				return false;
			}

			double const inv = 1.0 / det;
			double const t0[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] };
			double const u = (t0[0] * p[0] + t0[1] * p[1] + t0[2] * p[2]) * inv;

			if (u < 0.0 || u > 1.0)
			{
				return false;
			}

			double const q[3] = { t0[1] * e1[2] - t0[2] * e1[1], t0[2] * e1[0] - t0[0] * e1[2], t0[0] * e1[1] - t0[1] * e1[0] };
			double const v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;

			if (v < 0.0 || u + v > 1.0)
			{
				return false;
			}

			double const t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;

			return t > 0.0;
		}

		bool PointAt(std::vector<int32_t> const &points, int32_t index, double out[3])
		{
			if (index < 0 || (size_t)index * 3 + 2 >= points.size())
			{
				return false;
			}

			for (int i = 0; i < 3; i++)
			{
				out[i] = (double)points[(size_t)index * 3 + (size_t)i];
			}

			return true;
		}
	}

	bool VertexPick_RayHitsFaces(PickRay const &ray, std::vector<int32_t> const &points, std::vector<int32_t> const &faces)
	{
		for (size_t f = 0; f + 4 <= faces.size(); f += 4)
		{
			double a[3], b[3], c[3], d[3];

			if (!PointAt(points, faces[f], a) || !PointAt(points, faces[f + 1], b) || !PointAt(points, faces[f + 2], c))
			{
				continue;
			}

			if (HitsTriangle(ray, a, b, c))
			{
				return true;
			}

			// a quad: its second triangle
			if (PointAt(points, faces[f + 3], d) && HitsTriangle(ray, a, c, d))
			{
				return true;
			}
		}

		return false;
	}

	std::vector<int32_t> VertexPick_Points(PickCamera const &camera, int32_t clickX, int32_t clickY, std::vector<int32_t> const &points, double nearAhead)
	{
		std::vector<int32_t> taken;
		size_t const count = points.size() / 3;
		int64_t best = INT64_MAX;
		int32_t nearest = -1;

		for (size_t i = 0; i < count; i++)
		{
			int32_t const world[3] = { points[i * 3], points[i * 3 + 1], points[i * 3 + 2] };
			double x = 0.0, y = 0.0, ahead = 0.0;

			// 0x4853c4: a point not beyond the view's near value is skipped
			if (!PickRay_Project(camera, world, &x, &y, &ahead) || !(ahead > nearAhead))
			{
				continue;
			}

			// FUN_004fd830: the length as a float, made a whole number (FUN_004fd740); the strictly smaller wins (0x48544d), the first on a tie
			double const dx = x - (double)clickX, dy = y - (double)clickY;
			int64_t const length = (int64_t)llround((double)sqrtf((float)(dx * dx + dy * dy)));

			if (length < best)
			{
				best = length;
				nearest = (int32_t)i;
			}
		}

		if (nearest < 0)
		{
			return taken;
		}

		taken.push_back(nearest);

		// 0x4854aa-0x485567: every other point within 1 of it on each axis (the float differences cut toward zero lie in -1 .. 1)
		for (size_t j = 0; j < count; j++)
		{
			if ((int32_t)j == nearest)
			{
				continue;
			}

			bool close = true;

			for (int axis = 0; axis < 3 && close; axis++)
			{
				double const difference = (double)points[j * 3 + (size_t)axis] - (double)points[(size_t)nearest * 3 + (size_t)axis];
				int64_t const cut = (int64_t)difference;   // __ftol: toward zero

				close = cut > -2 && cut < 2;
			}

			if (close)
			{
				taken.push_back((int32_t)j);
			}
		}

		return taken;
	}
}
