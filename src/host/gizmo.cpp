// gizmo.cpp: see gizmo.h. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "gizmo.h"

#include <math.h>
#include <string.h>

namespace host
{
	namespace
	{
		// a real number (not infinite, not "not a number"), judged by its bits: the editor is built with /fp:fast, which lets the compiler assume v == v
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

		void Cross(double const a[3], double const b[3], double out[3])
		{
			double const x = a[1] * b[2] - a[2] * b[1];
			double const y = a[2] * b[0] - a[0] * b[2];
			double const z = a[0] * b[1] - a[1] * b[0];

			out[0] = x;
			out[1] = y;
			out[2] = z;
		}

		bool Unit(double v[3])
		{
			double const length = sqrt(Dot(v, v));

			if (!(length > 1e-9))
			{
				return false;
			}

			v[0] /= length;
			v[1] /= length;
			v[2] /= length;
			return true;
		}

		// two directions 1 long, square to the axis and to each other: a ring's plane
		void RingPlane(double const axis[3], double u[3], double v[3])
		{
			double const helper[3] = { fabs(axis[0]) < 0.9 ? 1.0 : 0.0, fabs(axis[0]) < 0.9 ? 0.0 : 1.0, 0.0 };

			Cross(axis, helper, u);
			Unit(u);
			Cross(axis, u, v);
		}

		bool Project(PickCamera const &camera, double const point[3], double *x, double *y)
		{
			int32_t world[3];

			for (int i = 0; i < 3; i++)
			{
				if (!(point[i] > -(double)kPickMostPlace && point[i] < (double)kPickMostPlace))
				{
					return false;
				}

				world[i] = (int32_t)floor(point[i] + 0.5);
			}

			double ahead = 0.0;

			return PickRay_Project(camera, world, x, y, &ahead) && ahead > 0.0;
		}

		// the pointer's distance to the segment from a to b, on the screen
		double ToSegment(double px, double py, double ax, double ay, double bx, double by)
		{
			double const dx = bx - ax, dy = by - ay;
			double const length2 = dx * dx + dy * dy;
			double t = length2 > 1e-12 ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0.0;

			t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);

			double const cx = ax + t * dx - px, cy = ay + t * dy - py;

			return sqrt(cx * cx + cy * cy);
		}

		bool MouseLine(PickCamera const &camera, int32_t x, int32_t y, double from[3], double direction[3])
		{
			PickRay ray;

			if (PickRay_FromClick(camera, x, y, &ray) != PickRayMade::Made)
			{
				return false;
			}

			for (int i = 0; i < 3; i++)
			{
				from[i] = ray.from[i];
				direction[i] = (double)ray.direction[i] / (double)kPickOne;
			}

			return true;
		}

		void RingPoint(GizmoFrame const &frame, int axis, int k, double out[3])
		{
			double u[3], v[3];

			RingPlane(frame.axis[axis], u, v);

			double const t = 2.0 * 3.14159265358979323846 * (double)k / (double)kGizmoRingSegments;

			for (int i = 0; i < 3; i++)
			{
				out[i] = frame.centre[i] + frame.radius * (u[i] * cos(t) + v[i] * sin(t));
			}
		}
	}

	char const *Gizmo_HandleName(GizmoHandle handle)
	{
		switch (handle)
		{
			case GizmoHandle::MoveX: return "move x";
			case GizmoHandle::MoveY: return "move y";
			case GizmoHandle::MoveZ: return "move z";
			case GizmoHandle::TurnX: return "turn x";
			case GizmoHandle::TurnY: return "turn y";
			case GizmoHandle::TurnZ: return "turn z";
			case GizmoHandle::Centre: return "centre ball";
			default: return "none";
		}
	}

	bool Gizmo_IsTurn(GizmoHandle handle)
	{
		return handle == GizmoHandle::TurnX || handle == GizmoHandle::TurnY || handle == GizmoHandle::TurnZ;
	}

	int Gizmo_AxisOf(GizmoHandle handle)
	{
		switch (handle)
		{
			case GizmoHandle::MoveX: case GizmoHandle::TurnX: return 0;
			case GizmoHandle::MoveY: case GizmoHandle::TurnY: return 1;
			case GizmoHandle::MoveZ: case GizmoHandle::TurnZ: return 2;
			default: return -1;
		}
	}

	bool Gizmo_Frame(PickCamera const &camera, int32_t const place[3], int32_t const matrix[9], bool local, GizmoFrame *frame)
	{
		GizmoFrame made;
		double x = 0.0, y = 0.0;

		for (int i = 0; i < 3; i++)
		{
			made.centre[i] = place[i];
		}

		if (!PickRay_Project(camera, place, &x, &y, &made.ahead) || !(made.ahead > 1.0) || !PickRay_ScaleUsable(camera.xScale))
		{
			return false;
		}

		if (local)
		{
			if (!matrix)
			{
				return false;
			}

			for (int row = 0; row < 3; row++)
			{
				for (int i = 0; i < 3; i++)
				{
					made.axis[row][i] = (double)matrix[row * 3 + i] / (double)kPickOne;
				}

				if (!(sqrt(Dot(made.axis[row], made.axis[row])) > 0.5) || !Unit(made.axis[row]))
				{
					return false;
				}
			}
		}

		double const perPixel = made.ahead / ((double)camera.xScale * (double)camera.viewWidth / 2.0);

		made.length = kGizmoArrowPixels * perPixel;
		made.radius = kGizmoRingPixels * perPixel;
		*frame = made;
		return true;
	}

	GizmoHandle Gizmo_Hit(PickCamera const &camera, GizmoFrame const &frame, bool canRotate, double x, double y)
	{
		GizmoHandle best = GizmoHandle::None;
		double bestDistance = kGizmoHitPixels + 1e-9;   // strictly nearer wins everywhere below: on a tie the handle met first (x, y, z; arrows before rings) keeps it
		double cx = 0.0, cy = 0.0;

		if (!Project(camera, frame.centre, &cx, &cy))
		{
			return GizmoHandle::None;
		}

		// the centre ball before the arrows, which all start at the centre (Vasquez's placement, P3)
		if (sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) <= kGizmoCentrePixels)
		{
			return GizmoHandle::Centre;
		}

		GizmoHandle const moves[3] = { GizmoHandle::MoveX, GizmoHandle::MoveY, GizmoHandle::MoveZ };
		GizmoHandle const turns[3] = { GizmoHandle::TurnX, GizmoHandle::TurnY, GizmoHandle::TurnZ };

		for (int a = 0; a < 3; a++)
		{
			double tip[3], tx = 0.0, ty = 0.0;

			for (int i = 0; i < 3; i++)
			{
				tip[i] = frame.centre[i] + frame.axis[a][i] * frame.length;
			}

			if (Project(camera, tip, &tx, &ty))
			{
				double const d = ToSegment(x, y, cx, cy, tx, ty);

				if (d < bestDistance)
				{
					bestDistance = d;
					best = moves[a];
				}
			}
		}

		if (!canRotate)
		{
			return best;
		}

		for (int a = 0; a < 3; a++)
		{
			double px = 0.0, py = 0.0;
			bool havePrevious = false;

			for (int k = 0; k <= kGizmoRingSegments; k++)
			{
				double point[3], qx = 0.0, qy = 0.0;

				RingPoint(frame, a, k, point);

				bool const seen = Project(camera, point, &qx, &qy);

				if (seen && havePrevious)
				{
					double const d = ToSegment(x, y, px, py, qx, qy);

					if (d < bestDistance)
					{
						bestDistance = d;
						best = turns[a];
					}
				}

				havePrevious = seen;
				px = qx;
				py = qy;
			}
		}

		return best;
	}

	bool Gizmo_AlongAxis(PickCamera const &camera, GizmoFrame const &frame, int axis, int32_t x, int32_t y, double *along)
	{
		double from[3], direction[3];

		if (axis < 0 || axis > 2 || !MouseLine(camera, x, y, from, direction))
		{
			return false;
		}

		double const *const a = frame.axis[axis];
		double const w0[3] = { frame.centre[0] - from[0], frame.centre[1] - from[1], frame.centre[2] - from[2] };
		double const b = Dot(a, direction);
		double const c = Dot(direction, direction);
		double const d = Dot(a, w0);
		double const e = Dot(direction, w0);
		double const denominator = c - b * b;

		if (!(c > 1e-12) || !(denominator > 1e-6 * c))   // the line nearly along the axis: no closest point to speak of
		{
			return false;
		}

		*along = (b * e - c * d) / denominator;
		return true;
	}

	bool Gizmo_OnRingPlane(PickCamera const &camera, GizmoFrame const &frame, int axis, int32_t x, int32_t y, double out[3])
	{
		double from[3], direction[3];

		if (axis < 0 || axis > 2 || !MouseLine(camera, x, y, from, direction))
		{
			return false;
		}

		double const *const a = frame.axis[axis];
		double const facing = Dot(a, direction);
		double const length = sqrt(Dot(direction, direction));

		if (!(fabs(facing) > 1e-4 * length))   // the line nearly in the ring's plane
		{
			return false;
		}

		double const toCentre[3] = { frame.centre[0] - from[0], frame.centre[1] - from[1], frame.centre[2] - from[2] };
		double const s = Dot(a, toCentre) / facing;

		if (!(s > 0.0))
		{
			return false;
		}

		for (int i = 0; i < 3; i++)
		{
			out[i] = from[i] + s * direction[i] - frame.centre[i];
		}

		return true;
	}

	double Gizmo_AngleAbout(double const axis[3], double const from[3], double const to[3])
	{
		double cross[3];

		Cross(from, to, cross);
		return atan2(Dot(axis, cross), Dot(from, to));
	}

	double Gizmo_Snap(double radians, double step)
	{
		return step > 0.0 ? floor(radians / step + 0.5) * step : radians;
	}

	void Gizmo_Moved(int32_t const place[3], double const axis[3], double mm, int32_t out[3])
	{
		bool const usable = Real(mm) && Real(axis[0]) && Real(axis[1]) && Real(axis[2]);

		for (int i = 0; i < 3; i++)
		{
			double v = usable ? (double)place[i] + axis[i] * mm : (double)place[i];   // not a real number: the place as it was

			v = v > (double)kPickMostPlace ? (double)kPickMostPlace : v;
			v = v < -(double)kPickMostPlace ? -(double)kPickMostPlace : v;
			out[i] = (int32_t)floor(v + 0.5);
		}
	}

	void Gizmo_Turned(int32_t const matrix[9], double const axis[3], double radians, int32_t out[9])
	{
		double a[3] = { axis[0], axis[1], axis[2] };

		if (!Real(radians) || !Real(a[0]) || !Real(a[1]) || !Real(a[2]) || !Unit(a))
		{
			for (int i = 0; i < 9; i++)
			{
				out[i] = matrix[i];
			}

			return;
		}

		double const c = cos(radians), s = sin(radians);

		for (int row = 0; row < 3; row++)
		{
			double v[3], across[3], turned[3];

			for (int i = 0; i < 3; i++)
			{
				v[i] = (double)matrix[row * 3 + i] / (double)kPickOne;
			}

			Cross(a, v, across);

			double const along = Dot(a, v);

			for (int i = 0; i < 3; i++)
			{
				turned[i] = v[i] * c + across[i] * s + a[i] * along * (1.0 - c);
			}

			if (!Unit(turned))
			{
				for (int i = 0; i < 3; i++)
				{
					out[row * 3 + i] = matrix[row * 3 + i];
				}

				continue;
			}

			for (int i = 0; i < 3; i++)
			{
				out[row * 3 + i] = (int32_t)floor(turned[i] * (double)kPickOne + 0.5);
			}
		}
	}

	void Gizmo_Lines(PickCamera const &camera, GizmoFrame const &frame, bool canRotate, GizmoHandle hot, std::vector<TrackLine> *lines)
	{
		lines->clear();

		if (!(frame.ahead > 1.0))
		{
			return;
		}

		double const pull = kGizmoNear / frame.ahead;   // every point along its own line of sight: the same picture, nearer than any wall
		uint32_t const colours[3] = { kGizmoColourX, kGizmoColourY, kGizmoColourZ };
		auto add = [&](double const p[3], double const q[3], uint32_t colour)
		{
			TrackLine line;

			for (int i = 0; i < 3; i++)
			{
				double const eye = (double)camera.place[i];

				line.from[i] = (int32_t)floor(eye + (p[i] - eye) * pull + 0.5);
				line.to[i] = (int32_t)floor(eye + (q[i] - eye) * pull + 0.5);
			}

			line.colour = colour;
			lines->push_back(line);
		};

		GizmoHandle const moves[3] = { GizmoHandle::MoveX, GizmoHandle::MoveY, GizmoHandle::MoveZ };
		GizmoHandle const turns[3] = { GizmoHandle::TurnX, GizmoHandle::TurnY, GizmoHandle::TurnZ };

		for (int a = 0; a < 3; a++)
		{
			uint32_t const colour = hot == moves[a] ? kGizmoColourHot : colours[a];
			double tip[3], u[3], v[3];

			RingPlane(frame.axis[a], u, v);

			for (int i = 0; i < 3; i++)
			{
				tip[i] = frame.centre[i] + frame.axis[a][i] * frame.length;
			}

			add(frame.centre, tip, colour);

			// the head: four lines back from the tip
			double const back = frame.length * 0.18, wide = frame.length * 0.07;
			double const *const sides[4] = { u, v, u, v };
			double const signs[4] = { 1.0, 1.0, -1.0, -1.0 };

			for (int h = 0; h < 4; h++)
			{
				double foot[3];

				for (int i = 0; i < 3; i++)
				{
					foot[i] = tip[i] - frame.axis[a][i] * back + sides[h][i] * wide * signs[h];
				}

				add(tip, foot, colour);
			}
		}

		for (int a = 0; a < 3 && canRotate; a++)
		{
			uint32_t const colour = hot == turns[a] ? kGizmoColourHot : colours[a];
			double previous[3];

			RingPoint(frame, a, 0, previous);

			for (int k = 1; k <= kGizmoRingSegments; k++)
			{
				double point[3];

				RingPoint(frame, a, k, point);
				add(previous, point, colour);

				for (int i = 0; i < 3; i++)
				{
					previous[i] = point[i];
				}
			}
		}

		// the centre ball: two diamonds square to the line of sight (kGizmoCentrePixels and half that), white, yellow when hot
		double const eye[3] = { (double)camera.place[0], (double)camera.place[1], (double)camera.place[2] };
		double sight[3] = { frame.centre[0] - eye[0], frame.centre[1] - eye[1], frame.centre[2] - eye[2] };
		double u[3], v[3];

		if (Unit(sight))
		{
			RingPlane(sight, u, v);

			double const pixel = frame.length / kGizmoArrowPixels;   // the world's millimetres per pixel at the centre
			uint32_t const colour = hot == GizmoHandle::Centre ? kGizmoColourHot : 0xFFFFFFFFu;

			for (double size : { kGizmoCentrePixels, kGizmoCentrePixels / 2.0 })
			{
				double corner[4][3];

				for (int i = 0; i < 3; i++)
				{
					corner[0][i] = frame.centre[i] + u[i] * size * pixel;
					corner[1][i] = frame.centre[i] + v[i] * size * pixel;
					corner[2][i] = frame.centre[i] - u[i] * size * pixel;
					corner[3][i] = frame.centre[i] - v[i] * size * pixel;
				}

				for (int c = 0; c < 4; c++)
				{
					add(corner[c], corner[(c + 1) % 4], colour);
				}
			}
		}
	}
}
