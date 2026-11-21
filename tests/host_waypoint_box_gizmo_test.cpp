// host_waypoint_box_gizmo_test.cpp: the waypoint-box gizmo's sums (src/host/waypoint_box_gizmo.h): its handles' names, the box's points in the world at the level's scale, the hit
// test (the centre ball, a face, an arrow), the lines (the gizmo's own and the six face diamonds, pulled toward the camera), and the three edits: the whole box moved with its aim
// point, a face moved (never past the other one, the room the stock game needs kept, the aim point as inteng's Edit Extents has it), the aim point moved (the user's, held inside the
// box by the checker's own test), with the refusals. A camera at the world's middle looking along +z at a 1024 x 768 view; the expected pixels come from PickRay_Project, the game's
// own drawing. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "waypoint_box_gizmo.h"
#include "waypoint_net_check.h"
#include "waypoint_room.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static host::PickCamera Camera()
{
	host::PickCamera c = {};

	c.turn[0][0] = c.turn[1][1] = c.turn[2][2] = host::kPickOne;
	c.xScale = 1.0f;
	c.yScale = 1.3333333f;
	c.viewWidth = 1024;
	c.viewHeight = 768;
	return c;
}

static bool Pixel(host::PickCamera const &camera, double const p[3], double *px, double *py, double *ahead = nullptr)
{
	int32_t const world[3] = { (int32_t)floor(p[0] + 0.5), (int32_t)floor(p[1] + 0.5), (int32_t)floor(p[2] + 0.5) };
	double a = 0.0;

	bool const made = host::PickRay_Project(camera, world, px, py, &a);

	if (ahead)
	{
		*ahead = a;
	}

	return made;
}

static host::WaypointRec Box(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1)
{
	host::WaypointRec box = {};

	box.min[0] = x0;
	box.min[1] = y0;
	box.min[2] = z0;
	box.max[0] = x1;
	box.max[1] = y1;
	box.max[2] = z1;

	for (int i = 0; i < 3; i++)
	{
		box.centre[i] = (int32_t)(((int64_t)box.min[i] + box.max[i]) / 2);
	}

	box.links.push_back(host::WaypointLinkRec{ 3, 4 });
	box.moduleLinks.push_back(host::WaypointModuleLink{ "next", 1 });
	return box;
}

static std::string Three(int32_t const v[3])
{
	return std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]);
}

static bool RoomAll(host::WaypointRec const &box, double scale)
{
	for (int axis = 0; axis < 3; axis++)
	{
		int32_t room = 0;

		if (!WaypointRoom(box.min[axis], box.max[axis], box.centre[axis], scale, &room) || room < 1)
		{
			return false;
		}
	}

	return true;
}

int main()
{
	host::PickCamera const camera = Camera();

	printf("THE HANDLES\n");
	{
		host::WaypointBoxHandle const all[] = { host::WaypointBoxHandle::None, host::WaypointBoxHandle::MoveX, host::WaypointBoxHandle::MoveY, host::WaypointBoxHandle::MoveZ,
			host::WaypointBoxHandle::Ball, host::WaypointBoxHandle::MinX, host::WaypointBoxHandle::MaxX, host::WaypointBoxHandle::MinY, host::WaypointBoxHandle::MaxY,
			host::WaypointBoxHandle::MinZ, host::WaypointBoxHandle::MaxZ };
		char const *const names[] = { "none", "move x", "move y", "move z", "centre ball", "min x", "max x", "min y", "max y", "min z", "max z" };
		bool named = true;

		for (size_t h = 0; h < sizeof(all) / sizeof(all[0]); h++)
		{
			named = named && strcmp(host::WaypointBoxGizmo_HandleName(all[h]), names[h]) == 0;
		}

		Check(named && strcmp(host::WaypointBoxGizmo_HandleName((host::WaypointBoxHandle)200), "none") == 0, "each handle's name (drag-gizmo's), none past the list");
		Check(host::WaypointBoxGizmo_FaceOf(host::WaypointBoxHandle::MinX) == 0 && host::WaypointBoxGizmo_FaceOf(host::WaypointBoxHandle::MaxZ) == 5 &&
			host::WaypointBoxGizmo_FaceOf(host::WaypointBoxHandle::Ball) == -1 && host::WaypointBoxGizmo_FaceOf(host::WaypointBoxHandle::MoveY) == -1, "faces 0 (min x) to 5 (max z); -1 for the rest");
		Check(host::WaypointBoxGizmo_ArrowAxis(host::WaypointBoxHandle::MoveX) == 0 && host::WaypointBoxGizmo_ArrowAxis(host::WaypointBoxHandle::MoveZ) == 2 &&
			host::WaypointBoxGizmo_ArrowAxis(host::WaypointBoxHandle::MinX) == -1, "arrows 0 to 2; -1 for the rest");
	}

	printf("THE BOX IN THE WORLD (the file's numbers times the scale, toward zero)\n");
	{
		host::WaypointRec box = Box(-1000, -500, 3000, 1000, 1500, 5000);
		double middle[3], aim[3], face[3];

		box.centre[0] = 250;
		host::WaypointBoxGizmo_Middle(box, 1.0, middle);
		host::WaypointBoxGizmo_Aim(box, 1.0, aim);
		Check(middle[0] == 0.0 && middle[1] == 500.0 && middle[2] == 4000.0 && aim[0] == 250.0 && aim[1] == 500.0 && aim[2] == 4000.0, "the middle of the box, and its aim point, at scale 1");

		host::WaypointBoxGizmo_FacePoint(box, 1.0, 1, face);
		Check(face[0] == 1000.0 && face[1] == 500.0 && face[2] == 4000.0, "max x's middle: the highest x, the middle of the others");
		host::WaypointBoxGizmo_FacePoint(box, 1.0, 2, face);
		Check(face[0] == 0.0 && face[1] == -500.0 && face[2] == 4000.0, "min y's middle");

		host::WaypointRec odd = Box(-3, 0, 0, 3, 10, 10);

		host::WaypointBoxGizmo_FacePoint(odd, 2.5, 0, face);
		Check(face[0] == -7.0, "at scale 2.5, min x -3 is -7 in the world (toward zero, as the 3D view draws it): " + std::to_string(face[0]));
	}

	printf("THE HIT\n");
	host::WaypointRec const box = Box(-1000, -1000, 3000, 1000, 1000, 5000);   // 2 m each way, its middle 4 m ahead
	host::GizmoFrame frame;
	int32_t const middleWorld[3] = { 0, 0, 4000 };

	Check(host::Gizmo_Frame(camera, middleWorld, nullptr, false, &frame), "Drake's frame at the box's middle");
	{
		double centre[3] = { 0, 0, 4000 }, cx = 0, cy = 0;

		Pixel(camera, centre, &cx, &cy);
		Check(host::WaypointBoxGizmo_Hit(camera, frame, box, 1.0, cx, cy) == host::WaypointBoxHandle::Ball, "the middle is the centre ball (before min z's face, whose middle is there too)");

		double maxX[3], fx = 0, fy = 0;

		host::WaypointBoxGizmo_FacePoint(box, 1.0, 1, maxX);
		Pixel(camera, maxX, &fx, &fy);
		Check(host::WaypointBoxGizmo_Hit(camera, frame, box, 1.0, fx + 3.0, fy - 3.0) == host::WaypointBoxHandle::MaxX, "4 px from max x's middle: max x");
		Check(host::WaypointBoxGizmo_Hit(camera, frame, box, 1.0, fx + 9.0, fy) == host::WaypointBoxHandle::None, "9 px from it, and nothing else near: none");

		double arrow[3] = { frame.length * 0.5, 0.0, 4000.0 }, ax = 0, ay = 0;

		Pixel(camera, arrow, &ax, &ay);
		Check(host::WaypointBoxGizmo_Hit(camera, frame, box, 1.0, ax, ay + 2.0) == host::WaypointBoxHandle::MoveX, "the x arrow's middle (no face there): move x");

		double maxY[3], yx = 0, yy = 0;

		host::WaypointBoxGizmo_FacePoint(box, 1.0, 3, maxY);
		Pixel(camera, maxY, &yx, &yy);
		Check(host::WaypointBoxGizmo_Hit(camera, frame, box, 1.0, yx, yy) == host::WaypointBoxHandle::MaxY, "max y's middle: max y");

		host::PickCamera behind = camera;

		behind.place[2] = 6000;   // past the box: every face behind the camera
		Check(host::WaypointBoxGizmo_Hit(behind, frame, box, 1.0, fx, fy) != host::WaypointBoxHandle::MaxX, "a face behind the camera is never hit");
	}

	printf("THE LINES\n");
	{
		std::vector<host::TrackLine> gizmoOnly, lines;

		host::Gizmo_Lines(camera, frame, false, host::GizmoHandle::None, &gizmoOnly);
		host::WaypointBoxGizmo_Lines(camera, frame, box, 1.0, host::WaypointBoxHandle::MaxX, &lines);
		Check(!gizmoOnly.empty() && lines.size() == gizmoOnly.size() + 6 * 4, "the gizmo's lines (no rings) and a 4-line diamond for each face: " + std::to_string(lines.size()));

		bool sameStart = lines.size() >= gizmoOnly.size();

		for (size_t i = 0; i < gizmoOnly.size() && sameStart; i++)
		{
			sameStart = memcmp(&lines[i], &gizmoOnly[i], sizeof(host::TrackLine)) == 0;
		}

		Check(sameStart, "the first lines are exactly Gizmo_Lines' (the same look as every other kind's gizmo)");

		size_t hot = 0, red = 0, green = 0, blue = 0;
		bool pulled = true;

		for (size_t i = gizmoOnly.size(); i < lines.size(); i++)
		{
			hot += lines[i].colour == host::kGizmoColourHot ? 1 : 0;
			red += lines[i].colour == host::kGizmoColourX ? 1 : 0;
			green += lines[i].colour == host::kGizmoColourY ? 1 : 0;
			blue += lines[i].colour == host::kGizmoColourZ ? 1 : 0;

			double const p[3] = { (double)lines[i].from[0], (double)lines[i].from[1], (double)lines[i].from[2] };
			double px = 0, py = 0, ahead = 0;

			pulled = pulled && Pixel(camera, p, &px, &py, &ahead) && fabs(ahead - host::kGizmoNear) < 25.0;
		}

		Check(hot == 4 && red == 4 && green == 8 && blue == 8, "the hot face (max x) yellow, the others in their axis's colour: " + std::to_string(hot) + " " + std::to_string(red) + " " +
			std::to_string(green) + " " + std::to_string(blue));
		Check(pulled, "every face diamond is pulled to about kGizmoNear ahead (nearer than any wall), as Gizmo_Lines pulls its own");

		double maxX[3], fx = 0, fy = 0, dx = 0, dy = 0;

		host::WaypointBoxGizmo_FacePoint(box, 1.0, 1, maxX);
		Pixel(camera, maxX, &fx, &fy);

		host::TrackLine const &first = lines[gizmoOnly.size() + 4];   // max x's first line
		double const corner[3] = { (double)first.from[0], (double)first.from[1], (double)first.from[2] };

		Pixel(camera, corner, &dx, &dy);
		Check(fabs(sqrt((dx - fx) * (dx - fx) + (dy - fy) * (dy - fy)) - host::kWaypointFacePixels) < 1.0, "a diamond's corner is kWaypointFacePixels from its face's middle on the screen");
	}

	printf("THE WHOLE BOX MOVED\n");
	{
		host::WaypointRec moved;

		Check(host::WaypointBox_Moved(box, 0, 250, 1.0, &moved) && moved.min[0] == -750 && moved.max[0] == 1250 && moved.centre[0] == 250 && moved.min[1] == -1000 &&
			moved.max[2] == 5000 && moved.links.size() == 1 && moved.links[0].index == 3 && moved.moduleLinks.size() == 1 && moved.flags == box.flags,
			"x by 250: lowest, highest and aim point all 250 on; the others, the links, the module links and the flags as they were");

		host::WaypointRec outside = box;

		outside.centre[0] = outside.min[0] - 46;   // Invasion module 54 box 14's kind: the aim point 46 below the lowest x
		Check(host::WaypointBox_Moved(outside, 1, -300, 1.0, &moved) && moved.centre[0] == outside.min[0] - 46 && moved.centre[1] == outside.centre[1] - 300,
			"a box whose aim point is outside moves with it: still 46 below its lowest x (the checker reports it; a move never changes it)");

		host::WaypointRec far = box, untouched = box;

		far.max[2] = host::kWaypointBoxMostPlace - 10;
		Check(!host::WaypointBox_Moved(far, 2, 11, 1.0, &untouched) && untouched.min[0] == box.min[0], "moved past kWaypointBoxMostPlace: refused, nothing written to the answer");
		Check(!host::WaypointBox_Moved(box, 3, 10, 1.0, &moved) && !host::WaypointBox_Moved(box, -1, 10, 1.0, &moved) && !host::WaypointBox_Moved(box, 0, 10, 1.0, nullptr),
			"an axis that is none, or no answer: refused");
	}

	printf("A FACE MOVED\n");
	{
		host::WaypointRec made;

		Check(host::WaypointBox_FaceMoved(box, 1, 500, 1.0, &made) && made.max[0] == 1500 && made.min[0] == -1000 && made.centre[0] == 250 && made.centre[1] == 0 && made.centre[2] == 4000,
			"max x out by 500: the aim point is the box's middle again (not the user's: inteng's FUN_00586e90)");
		Check(host::WaypointBox_FaceMoved(box, 4, -1000, 1.0, &made) && made.min[2] == 2000 && made.centre[2] == 3500, "min z by -1000, its aim point the new middle");

		// (each call made before its Check: the message's numbers would be made first otherwise, from the box before it)
		bool const pastMin = host::WaypointBox_FaceMoved(box, 1, -5000, 1.0, &made);

		Check(pastMin && made.max[0] >= made.min[0] && made.min[0] == -1000 && RoomAll(made, 1.0) && made.max[0] - made.min[0] <= 8,
			"max x dragged 5 m past min x: held just past it, with the room the stock game needs (" + std::to_string(made.max[0] - made.min[0]) + " wide)");

		bool const pastMax = host::WaypointBox_FaceMoved(box, 0, 9000, 1.0, &made);

		Check(pastMax && made.min[0] <= made.max[0] && made.max[0] == 1000 && RoomAll(made, 1.0) && made.max[0] - made.min[0] <= 8,
			"min x dragged far past max x: held just short of it, with room (" + std::to_string(made.max[0] - made.min[0]) + " wide)");

		host::WaypointRec users = box;

		users.flags |= host::kWaypointFlagCentreByUser;
		users.centre[0] = -900;
		Check(host::WaypointBox_FaceMoved(users, 1, 200, 1.0, &made) && made.centre[0] == -900 && (made.flags & host::kWaypointFlagCentreByUser),
			"the user's aim point stays where it is when it is still inside");
		Check(host::WaypointBox_FaceMoved(users, 0, 400, 1.0, &made) && made.min[0] == -600 && made.centre[0] == -600 && host::Waypoint_Holds(made, made.centre, 1.0),
			"the user's aim point left outside by min x moving in: held inside, at the new lowest x");
		Check(host::WaypointBox_FaceMoved(users, 1, -1950, 1.0, &made) && host::Waypoint_Holds(made, made.centre, 1.0) && made.centre[0] == made.max[0] - 1,
			"and by max x moving in past it: at the new highest x less 1 (the game's inside: max - 1)");

		host::WaypointRec scaled;

		bool const onOther = host::WaypointBox_FaceMoved(box, 3, -2000, 0.5, &scaled);

		Check(onOther && RoomAll(scaled, 0.5) && host::WaypointBox_HasRoom(scaled, 0.5) && scaled.max[1] - scaled.min[1] <= 16,
			"at scale 0.5 a face dragged onto the other keeps the room the stock game needs at that scale (" + std::to_string(scaled.max[1] - scaled.min[1]) + " of the file's units)");
		Check(!host::WaypointBox_FaceMoved(box, 6, 10, 1.0, &made) && !host::WaypointBox_FaceMoved(box, -1, 10, 1.0, &made), "a face that is none: refused");
	}

	printf("THE AIM POINT MOVED\n");
	{
		host::WaypointRec made, outside = box;

		outside.centre[0] = outside.min[0] - 46;
		Check(host::WaypointBox_AimMoved(outside, 2, 0, 1.0, &made) && made.centre[0] == outside.min[0] && host::Waypoint_Holds(made, made.centre, 1.0) &&
			(made.flags & host::kWaypointFlagCentreByUser), "box 14's kind, the aim point moved along z by 0: held inside on x too (at the lowest x), the user's now");
		Check(host::WaypointBox_AimMoved(box, 0, 10000, 1.0, &made) && made.centre[0] == 999 && made.min[0] == -1000 && made.max[0] == 1000,
			"far along x: at the highest x less 1, the box as it was");
		Check(host::WaypointBox_AimMoved(box, 1, -300, 1.0, &made) && made.centre[1] == -300 && made.centre[0] == 0, "300 along -y: there, inside");

		bool scaledInside = true;

		for (double scale : { 0.5, 1.5, 2.5, 3.0 })
		{
			host::WaypointRec small = Box(-7, -7, -7, 9, 9, 9);

			scaledInside = scaledInside && host::WaypointBox_AimMoved(small, 0, 50, scale, &made) && host::Waypoint_Holds(made, made.centre, scale) &&
				host::WaypointBox_AimMoved(small, 1, -50, scale, &made) && host::Waypoint_Holds(made, made.centre, scale);
		}

		Check(scaledInside, "at the scales 0.5, 1.5, 2.5 and 3 an aim point dragged past a side is held inside by the checker's own test");

		host::WaypointRec flat = box;

		flat.max[0] = flat.min[0];
		Check(!host::WaypointBox_AimMoved(flat, 0, 5, 1.0, &made), "a box with no room on an axis: no place inside, refused");
	}

	printf("THE ROOM, AND THE FILE'S STEP\n");
	{
		host::WaypointRec flat = box, crossed = box, huge = box;

		flat.max[1] = flat.min[1] + 1;
		crossed.max[2] = crossed.min[2] - 10;
		huge.min[0] = -host::kWaypointBoxMostPlace - 1;
		Check(host::WaypointBox_HasRoom(box, 1.0) && !host::WaypointBox_HasRoom(flat, 1.0) && !host::WaypointBox_HasRoom(crossed, 1.0) && !host::WaypointBox_HasRoom(huge, 1.0),
			"room: a 2 m box has it; 1 wide, lowest past highest, or a number past kWaypointBoxMostPlace have not");
		Check(host::WaypointBox_FileStep(100.0, 2.0) == 50 && host::WaypointBox_FileStep(-99.0, 1.0) == -99 && host::WaypointBox_FileStep(10.0, 0.0) == 0 &&
			host::WaypointBox_FileStep(std::numeric_limits<double>::quiet_NaN(), 1.0) == 0 && host::WaypointBox_FileStep(1e300, 1.0) == host::kWaypointBoxMostPlace &&
			host::WaypointBox_FileStep(-1e300, 1.0) == -host::kWaypointBoxMostPlace, "a drag's millimetres as the file's units: over the scale; 0 for no scale or no number; held at the most");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
