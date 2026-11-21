// See waypoint_box_gizmo.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_box_gizmo.h"

#include <math.h>
#include <string.h>

#include "waypoint_net_check.h"
#include "waypoint_pick.h"
#include "waypoint_room.h"   // avpx/3dc: the stock room sum

namespace host
{
	namespace
	{
		// the most steps a face is pushed out to find room, or an aim point moved in to find a place inside (a few are enough at any scale a level uses)
		int const kMostSteps = 4096;

		bool Real(double v)
		{
			uint64_t bits;

			memcpy(&bits, &v, sizeof(bits));
			return (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
		}

		bool Within(int64_t v)
		{
			return v >= -(int64_t)kWaypointBoxMostPlace && v <= (int64_t)kWaypointBoxMostPlace;
		}

		bool BoxWithin(WaypointRec const &box)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if (!Within(box.min[axis]) || !Within(box.max[axis]) || !Within(box.centre[axis]))
				{
					return false;
				}
			}

			return true;
		}

		// inteng's FUN_00586e90: the aim point is the middle of the box (on every axis), unless it is the user's
		void AimFollows(WaypointRec *box)
		{
			if (box->flags & kWaypointFlagCentreByUser)
			{
				return;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				box->centre[axis] = (int32_t)(((int64_t)box->min[axis] + box->max[axis]) / 2);
			}
		}

		// one axis of the checker's inside test (waypoint_net_check.cpp Holds: the loader's scaled numbers, the highest 1 less)
		bool AxisHolds(WaypointRec const &box, int axis, int32_t value, double scale)
		{
			int32_t low = 0, top = 0, at = 0;

			return WaypointScaledCorner(box.min[axis], scale, &low) && WaypointScaledCorner(box.max[axis], scale, &top) && top != INT32_MIN &&
				WaypointScaledCorner(value, scale, &at) && at >= low && at <= top - 1;
		}

		bool RoomOn(WaypointRec const &box, int axis, double scale)
		{
			int32_t room = 0;

			return box.min[axis] <= box.max[axis] && WaypointRoom(box.min[axis], box.max[axis], box.centre[axis], scale, &room) && room >= 1;
		}

		// a world point pulled toward the eye to kGizmoNear ahead, along its own line of sight (Gizmo_Lines' pull)
		void Pulled(PickCamera const &camera, double const point[3], double ahead, int32_t out[3])
		{
			double const pull = kGizmoNear / ahead;

			for (int i = 0; i < 3; i++)
			{
				double const eye = (double)camera.place[i];

				out[i] = (int32_t)floor(eye + (point[i] - eye) * pull + 0.5);
			}
		}

		bool Projected(PickCamera const &camera, double const point[3], double *x, double *y, double *ahead)
		{
			int32_t world[3];

			for (int i = 0; i < 3; i++)
			{
				if (!Real(point[i]) || !(point[i] > -(double)kPickMostPlace && point[i] < (double)kPickMostPlace))
				{
					return false;
				}

				world[i] = (int32_t)floor(point[i] + 0.5);
			}

			return PickRay_Project(camera, world, x, y, ahead) && *ahead > 1.0;
		}
	}

	char const *WaypointBoxGizmo_HandleName(WaypointBoxHandle handle)
	{
		switch (handle)
		{
			case WaypointBoxHandle::MoveX: return "move x";
			case WaypointBoxHandle::MoveY: return "move y";
			case WaypointBoxHandle::MoveZ: return "move z";
			case WaypointBoxHandle::Ball: return "centre ball";
			case WaypointBoxHandle::MinX: return "min x";
			case WaypointBoxHandle::MaxX: return "max x";
			case WaypointBoxHandle::MinY: return "min y";
			case WaypointBoxHandle::MaxY: return "max y";
			case WaypointBoxHandle::MinZ: return "min z";
			case WaypointBoxHandle::MaxZ: return "max z";
			default: return "none";
		}
	}

	int WaypointBoxGizmo_FaceOf(WaypointBoxHandle handle)
	{
		int const h = (int)handle;

		return h >= (int)WaypointBoxHandle::MinX && h <= (int)WaypointBoxHandle::MaxZ ? h - (int)WaypointBoxHandle::MinX : -1;
	}

	int WaypointBoxGizmo_ArrowAxis(WaypointBoxHandle handle)
	{
		switch (handle)
		{
			case WaypointBoxHandle::MoveX: return 0;
			case WaypointBoxHandle::MoveY: return 1;
			case WaypointBoxHandle::MoveZ: return 2;
			default: return -1;
		}
	}

	void WaypointBoxGizmo_Middle(WaypointRec const &box, double scale, double out[3])
	{
		double lo[3], hi[3];

		WaypointPick_WorldBox(box, scale, false, lo, hi);

		for (int i = 0; i < 3; i++)
		{
			out[i] = (lo[i] + hi[i]) / 2.0;
		}
	}

	void WaypointBoxGizmo_Aim(WaypointRec const &box, double scale, double out[3])
	{
		int32_t aim[3];

		WaypointPick_AimWorld(box, scale, aim);

		for (int i = 0; i < 3; i++)
		{
			out[i] = (double)aim[i];
		}
	}

	void WaypointBoxGizmo_FacePoint(WaypointRec const &box, double scale, int face, double out[3])
	{
		double lo[3], hi[3];

		WaypointPick_WorldBox(box, scale, false, lo, hi);

		for (int i = 0; i < 3; i++)
		{
			out[i] = (lo[i] + hi[i]) / 2.0;
		}

		if (face >= 0 && face < 6)
		{
			out[face / 2] = face % 2 ? hi[face / 2] : lo[face / 2];
		}
	}

	WaypointBoxHandle WaypointBoxGizmo_Hit(PickCamera const &camera, GizmoFrame const &frame, WaypointRec const &box, double scale, double x, double y)
	{
		GizmoHandle const gizmo = Gizmo_Hit(camera, frame, false, x, y);

		if (gizmo == GizmoHandle::Centre)
		{
			return WaypointBoxHandle::Ball;
		}

		// the face nearest within kWaypointFacePixels; on a tie the face met first keeps it
		WaypointBoxHandle best = WaypointBoxHandle::None;
		double bestPixels = kWaypointFacePixels + 1e-9;

		for (int face = 0; face < 6; face++)
		{
			double point[3], px = 0.0, py = 0.0, ahead = 0.0;

			WaypointBoxGizmo_FacePoint(box, scale, face, point);

			if (!Projected(camera, point, &px, &py, &ahead))
			{
				continue;
			}

			double const pixels = sqrt((px - x) * (px - x) + (py - y) * (py - y));

			if (pixels < bestPixels)
			{
				bestPixels = pixels;
				best = (WaypointBoxHandle)((int)WaypointBoxHandle::MinX + face);
			}
		}

		if (best != WaypointBoxHandle::None)
		{
			return best;
		}

		switch (gizmo)
		{
			case GizmoHandle::MoveX: return WaypointBoxHandle::MoveX;
			case GizmoHandle::MoveY: return WaypointBoxHandle::MoveY;
			case GizmoHandle::MoveZ: return WaypointBoxHandle::MoveZ;
			default: return WaypointBoxHandle::None;
		}
	}

	void WaypointBoxGizmo_Lines(PickCamera const &camera, GizmoFrame const &frame, WaypointRec const &box, double scale, WaypointBoxHandle hot, std::vector<TrackLine> *lines)
	{
		GizmoHandle const gizmoHot = hot == WaypointBoxHandle::Ball ? GizmoHandle::Centre : hot == WaypointBoxHandle::MoveX ? GizmoHandle::MoveX :
			hot == WaypointBoxHandle::MoveY ? GizmoHandle::MoveY : hot == WaypointBoxHandle::MoveZ ? GizmoHandle::MoveZ : GizmoHandle::None;

		Gizmo_Lines(camera, frame, false, gizmoHot, lines);

		if (lines->empty())
		{
			return;   // no frame to draw at
		}

		uint32_t const colours[3] = { kGizmoColourX, kGizmoColourY, kGizmoColourZ };
		double const eye[3] = { (double)camera.place[0], (double)camera.place[1], (double)camera.place[2] };

		if (!PickRay_ScaleUsable(camera.xScale) || camera.viewWidth <= 0)
		{
			return;
		}

		for (int face = 0; face < 6; face++)
		{
			double point[3], px = 0.0, py = 0.0, ahead = 0.0;

			WaypointBoxGizmo_FacePoint(box, scale, face, point);

			if (!Projected(camera, point, &px, &py, &ahead))
			{
				continue;
			}

			// a diamond square to the line of sight, kWaypointFacePixels across on the screen at its own distance
			double sight[3] = { point[0] - eye[0], point[1] - eye[1], point[2] - eye[2] };
			double const length = sqrt(sight[0] * sight[0] + sight[1] * sight[1] + sight[2] * sight[2]);

			if (!(length > 1e-6))
			{
				continue;
			}

			for (int i = 0; i < 3; i++)
			{
				sight[i] /= length;
			}

			double const helper[3] = { fabs(sight[0]) < 0.9 ? 1.0 : 0.0, fabs(sight[0]) < 0.9 ? 0.0 : 1.0, 0.0 };
			double u[3] = { sight[1] * helper[2] - sight[2] * helper[1], sight[2] * helper[0] - sight[0] * helper[2], sight[0] * helper[1] - sight[1] * helper[0] };
			double const lu = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);

			if (!(lu > 1e-9))
			{
				continue;
			}

			for (int i = 0; i < 3; i++)
			{
				u[i] /= lu;
			}

			double const v[3] = { sight[1] * u[2] - sight[2] * u[1], sight[2] * u[0] - sight[0] * u[2], sight[0] * u[1] - sight[1] * u[0] };
			double const pixel = ahead / ((double)camera.xScale * (double)camera.viewWidth / 2.0);   // the world's millimetres a pixel there
			double const size = kWaypointFacePixels * pixel;
			uint32_t const colour = hot == (WaypointBoxHandle)((int)WaypointBoxHandle::MinX + face) ? kGizmoColourHot : colours[face / 2];
			double corner[4][3];

			for (int i = 0; i < 3; i++)
			{
				corner[0][i] = point[i] + u[i] * size;
				corner[1][i] = point[i] + v[i] * size;
				corner[2][i] = point[i] - u[i] * size;
				corner[3][i] = point[i] - v[i] * size;
			}

			for (int c = 0; c < 4; c++)
			{
				TrackLine line;

				Pulled(camera, corner[c], ahead, line.from);
				Pulled(camera, corner[(c + 1) % 4], ahead, line.to);
				line.colour = colour;
				lines->push_back(line);
			}
		}
	}

	bool WaypointBox_HasRoom(WaypointRec const &box, double scale)
	{
		if (!BoxWithin(box))
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (!RoomOn(box, axis, scale))
			{
				return false;
			}
		}

		return true;
	}

	bool WaypointBox_Moved(WaypointRec const &start, int axis, int32_t by, double scale, WaypointRec *out)
	{
		if (axis < 0 || axis > 2 || !out)
		{
			return false;
		}

		WaypointRec moved = start;
		int64_t const min = (int64_t)start.min[axis] + by, max = (int64_t)start.max[axis] + by, centre = (int64_t)start.centre[axis] + by;

		if (!Within(min) || !Within(max) || !Within(centre))
		{
			return false;
		}

		moved.min[axis] = (int32_t)min;
		moved.max[axis] = (int32_t)max;
		moved.centre[axis] = (int32_t)centre;

		if (!WaypointBox_HasRoom(moved, scale))
		{
			return false;
		}

		*out = moved;
		return true;
	}

	bool WaypointBox_HoldAimInside(WaypointRec *box, double scale)
	{
		if (!box || !BoxWithin(*box))
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			int64_t const low = box->min[axis], high = (int64_t)box->max[axis] - 1;

			if (high < low)
			{
				return false;
			}

			int32_t at = (int32_t)(box->centre[axis] < low ? low : (box->centre[axis] > high ? high : box->centre[axis]));
			int32_t const middle = (int32_t)(((int64_t)box->min[axis] + box->max[axis]) / 2);

			// where the level's scale makes the file's numbers not enough: a step toward the middle at a time
			for (int step = 0; step < kMostSteps && !AxisHolds(*box, axis, at, scale) && at != middle; step++)
			{
				at += at < middle ? 1 : -1;
			}

			box->centre[axis] = at;
		}

		return Waypoint_Holds(*box, box->centre, scale);
	}

	bool WaypointBox_FaceMoved(WaypointRec const &start, int face, int32_t by, double scale, WaypointRec *out)
	{
		if (face < 0 || face > 5 || !out)
		{
			return false;
		}

		int const axis = face / 2;
		bool const high = face % 2 != 0;
		int64_t value = (high ? (int64_t)start.max[axis] : (int64_t)start.min[axis]) + by;

		// never past the other face
		if (high && value < start.min[axis])
		{
			value = start.min[axis];
		}

		if (!high && value > start.max[axis])
		{
			value = start.max[axis];
		}

		for (int step = 0; step < kMostSteps; step++)
		{
			if (!Within(value))
			{
				return false;
			}

			WaypointRec made = start;

			(high ? made.max : made.min)[axis] = (int32_t)value;
			AimFollows(&made);

			if (RoomOn(made, axis, scale) && (!(made.flags & kWaypointFlagCentreByUser) || WaypointBox_HoldAimInside(&made, scale)) && WaypointBox_HasRoom(made, scale))
			{
				*out = made;
				return true;
			}

			value += high ? 1 : -1;   // no room yet: the face a step out
		}

		return false;
	}

	bool WaypointBox_AimMoved(WaypointRec const &start, int axis, int32_t by, double scale, WaypointRec *out)
	{
		if (axis < 0 || axis > 2 || !out)
		{
			return false;
		}

		int64_t const at = (int64_t)start.centre[axis] + by;

		if (!Within(at))
		{
			return false;
		}

		WaypointRec made = start;

		made.centre[axis] = (int32_t)at;
		made.flags |= kWaypointFlagCentreByUser;

		if (!WaypointBox_HoldAimInside(&made, scale) || !WaypointBox_HasRoom(made, scale))
		{
			return false;
		}

		*out = made;
		return true;
	}

	int32_t WaypointBox_FileStep(double worldMm, double scale)
	{
		if (!Real(worldMm) || !Real(scale) || !(scale > 0.0))
		{
			return 0;
		}

		double const file = floor(worldMm / scale + 0.5);

		return file > (double)kWaypointBoxMostPlace ? kWaypointBoxMostPlace : (file < -(double)kWaypointBoxMostPlace ? -kWaypointBoxMostPlace : (int32_t)file);
	}
}
