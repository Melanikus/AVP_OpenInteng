// host_gizmo_test.cpp: the mouse gizmo's maths (src/host/gizmo.h): the frame's size on the screen, the hit test, an arrow's drag, a ring's drag and its angle, Shift's
// 15-degree snap, the moved place and the turned matrix, the lines and their pull toward the camera, and the refusals. A camera at the world's middle looking along +z
// (the identity turn) at a 1024 x 768 view; the expected pixels come from PickRay_Project, the game's own drawing, so the checks hold whatever its signs are.
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "gizmo.h"

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

static bool Pixel(host::PickCamera const &camera, double x, double y, double z, double *px, double *py)
{
	int32_t const world[3] = { (int32_t)floor(x + 0.5), (int32_t)floor(y + 0.5), (int32_t)floor(z + 0.5) };
	double ahead = 0.0;

	return host::PickRay_Project(camera, world, px, py, &ahead);
}

static std::string N(double v)
{
	char text[48];

	sprintf_s(text, "%.3f", v);
	return text;
}

int main()
{
	host::PickCamera const camera = Camera();
	int32_t const place[3] = { 0, 0, 4000 };
	host::GizmoFrame frame;
	double const pi = 3.14159265358979323846;

	printf("THE FRAME\n");
	Check(host::Gizmo_Frame(camera, place, nullptr, false, &frame), "an object 4 m ahead has a frame");
	Check(fabs(frame.ahead - 4000.0) < 1.0, "its distance ahead (" + N(frame.ahead) + ")");
	Check(fabs(frame.length - 90.0 * 4000.0 / 512.0) < 0.01 && fabs(frame.radius - 70.0 * 4000.0 / 512.0) < 0.01, "an arrow 90 px and a ring 70 px on the screen (" + N(frame.length) + " mm)");
	{
		double cx, cy, tx, ty;

		Pixel(camera, 0, 0, 4000, &cx, &cy);
		Pixel(camera, frame.length, 0, 4000, &tx, &ty);
		Check(fabs(fabs(tx - cx) - 90.0) < 0.6 && fabs(ty - cy) < 0.6, "the x arrow's tip, drawn by the game's sums, is 90 px across (" + N(tx - cx) + ")");
	}
	{
		int32_t const behind[3] = { 0, 0, -4000 };
		host::GizmoFrame none;
		host::PickCamera flat = camera;
		int32_t const zero[9] = { 0 };

		flat.xScale = 0.0f;
		Check(!host::Gizmo_Frame(camera, behind, nullptr, false, &none), "behind the camera: no frame");
		Check(!host::Gizmo_Frame(flat, place, nullptr, false, &none), "a refused camera (scale 0): no frame");
		Check(!host::Gizmo_Frame(camera, place, zero, true, &none) && !host::Gizmo_Frame(camera, place, nullptr, true, &none), "local axes from a matrix of noughts, or none: no frame");

		int32_t const quarter[9] = { 0, 65536, 0, -65536, 0, 0, 0, 0, 65536 };   // turned a quarter about z: its x is the world's y
		host::GizmoFrame local;

		Check(host::Gizmo_Frame(camera, place, quarter, true, &local) && fabs(local.axis[0][1] - 1.0) < 1e-9 && fabs(local.axis[1][0] + 1.0) < 1e-9,
			"local axes are the matrix's rows (the object's x along the world's y)");
	}

	printf("THE HIT\n");
	{
		double cx, cy, mx, my, yx, yy;

		Pixel(camera, 0, 0, 4000, &cx, &cy);
		Pixel(camera, frame.length / 2, 0, 4000, &mx, &my);
		Pixel(camera, 0, frame.length / 2, 4000, &yx, &yy);
		Check(host::Gizmo_Hit(camera, frame, true, mx, my + 3) == host::GizmoHandle::MoveX, "3 px off the x arrow's middle: move x");
		Check(host::Gizmo_Hit(camera, frame, true, yx + 3, yy) == host::GizmoHandle::MoveY, "beside the y arrow: move y");
		Check(host::Gizmo_Hit(camera, frame, true, mx, my + 30) == host::GizmoHandle::None, "30 px off: nothing");
		Check(host::Gizmo_Hit(camera, frame, true, cx, cy) == host::GizmoHandle::Centre && host::Gizmo_Hit(camera, frame, false, cx + 6, cy - 6) == host::GizmoHandle::Centre,
			"on the centre, where all three arrows start, and 8.5 px off it: the centre ball (before the arrows), for a kind with no turn too");
		Check(host::Gizmo_Hit(camera, frame, true, cx + (mx > cx ? 12 : -12), cy) == host::GizmoHandle::MoveX, "12 px out along the x arrow: the arrow again");

		double const d = 70.0 / sqrt(2.0);

		Check(host::Gizmo_Hit(camera, frame, true, cx + d, cy + d) == host::GizmoHandle::TurnZ, "on the ring that faces the camera, between the arrows: turn z");
		Check(host::Gizmo_Hit(camera, frame, false, cx + d, cy + d) == host::GizmoHandle::None, "a kind with no turn has no rings");
	}

	printf("AN ARROW'S DRAG\n");
	{
		double cx, cy;

		Pixel(camera, 0, 0, 4000, &cx, &cy);

		int32_t const x0 = (int32_t)floor(cx), y0 = (int32_t)floor(cy);
		double a0 = 0.0, a1 = 0.0;

		Check(host::Gizmo_AlongAxis(camera, frame, 0, x0, y0, &a0) && host::Gizmo_AlongAxis(camera, frame, 0, x0 + 64, y0, &a1), "the mouse's line meets the x axis");
		Check(fabs(fabs(a1 - a0) - 64.0 * 4000.0 / 512.0) < 0.5, "64 px along the arrow: 500 mm along the axis (" + N(a1 - a0) + ")");

		double b0 = 0.0, b1 = 0.0;

		Check(host::Gizmo_AlongAxis(camera, frame, 0, x0, y0, &b0) && host::Gizmo_AlongAxis(camera, frame, 0, x0, y0 + 64, &b1) && fabs(b1 - b0) < 0.5,
			"64 px square to the arrow: no move along it (" + N(b1 - b0) + ")");
		Check(!host::Gizmo_AlongAxis(camera, frame, 3, x0, y0, &a0) && !host::Gizmo_AlongAxis(camera, frame, -1, x0, y0, &a0), "an axis past 0 .. 2: refused");
		Check(!host::Gizmo_AlongAxis(camera, frame, 0, -5, y0, &a0), "a pointer outside the view: no line");

		int32_t moved[3];
		double const axisX[3] = { 1.0, 0.0, 0.0 };

		host::Gizmo_Moved(place, axisX, 250.4, moved);
		Check(moved[0] == 250 && moved[1] == 0 && moved[2] == 4000, "the place moved 250 mm along x");
		host::Gizmo_Moved(place, axisX, 1e12, moved);
		Check(moved[0] == host::kPickMostPlace, "a huge move is held at the world's limit");
		host::Gizmo_Moved(place, axisX, std::numeric_limits<double>::quiet_NaN(), moved);
		Check(moved[0] == 0 && moved[2] == 4000, "a move of not-a-number leaves the place as it was");
	}

	printf("A RING'S DRAG\n");
	{
		double cx, cy;

		Pixel(camera, 0, 0, 4000, &cx, &cy);

		double v0[3], v1[3];
		int32_t const r = 70;

		Check(host::Gizmo_OnRingPlane(camera, frame, 2, (int32_t)floor(cx) + r, (int32_t)floor(cy), v0) && host::Gizmo_OnRingPlane(camera, frame, 2, (int32_t)floor(cx),
			(int32_t)floor(cy) + r, v1), "the mouse's line meets the z ring's plane, twice");

		double const angle = host::Gizmo_AngleAbout(frame.axis[2], v0, v1);

		Check(fabs(fabs(angle) - pi / 2) < 0.02, "a quarter way round the ring on the screen: a quarter turn (" + N(angle * 180.0 / pi) + " degrees)");

		// THE OBJECT FOLLOWS THE MOUSE: a row pointing where the press was, turned by that angle, points where the pointer is now
		double const length0 = sqrt(v0[0] * v0[0] + v0[1] * v0[1] + v0[2] * v0[2]), length1 = sqrt(v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2]);
		int32_t const toPress[9] = { (int32_t)(v0[0] / length0 * 65536), (int32_t)(v0[1] / length0 * 65536), (int32_t)(v0[2] / length0 * 65536), 0, 0, 65536, 65536, 0, 0 };
		int32_t turned[9];

		host::Gizmo_Turned(toPress, frame.axis[2], angle, turned);

		double const follow = (turned[0] * v1[0] + turned[1] * v1[1] + turned[2] * v1[2]) / 65536.0 / length1;

		Check(follow > 0.999, "turned by it, the row toward the press points toward the pointer now (" + N(follow) + ")");
		Check(!host::Gizmo_OnRingPlane(camera, frame, 0, (int32_t)floor(cx), (int32_t)floor(cy) + 1, v0) || true, "(the x ring's plane is edge-on here: a meet or none, never a crash)");
	}

	printf("THE TURN AND THE SNAP\n");
	{
		int32_t const identity[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
		double const z[3] = { 0.0, 0.0, 1.0 }, zero[3] = { 0.0, 0.0, 0.0 };
		int32_t out[9];

		host::Gizmo_Turned(identity, z, pi / 2, out);
		Check(out[0] == 0 && out[1] == 65536 && out[3] == -65536 && out[4] == 0 && out[8] == 65536, "a quarter about z: x goes to y, y to minus x, z stays");
		host::Gizmo_Turned(identity, z, std::numeric_limits<double>::quiet_NaN(), out);
		Check(memcmp(out, identity, sizeof(out)) == 0, "not-a-number: the matrix as it was");
		host::Gizmo_Turned(identity, zero, 1.0, out);
		Check(memcmp(out, identity, sizeof(out)) == 0, "an axis of no length: the matrix as it was");

		double const degree = pi / 180.0, step = host::kGizmoSnapRadians;

		Check(fabs(host::Gizmo_Snap(7 * degree, step)) < 1e-9 && fabs(host::Gizmo_Snap(8 * degree, step) - 15 * degree) < 1e-9, "Shift: 7 degrees snaps to 0, 8 to 15");
		Check(fabs(host::Gizmo_Snap(-22 * degree, step) + 15 * degree) < 1e-9 && fabs(host::Gizmo_Snap(-23 * degree, step) + 30 * degree) < 1e-9, "-22 to -15, -23 to -30");
		Check(fabs(host::Gizmo_Snap(1.0, 0.0) - 1.0) < 1e-12, "a step of 0: no snap");
	}

	printf("THE LINES\n");
	{
		std::vector<host::TrackLine> lines;

		host::Gizmo_Lines(camera, frame, true, host::GizmoHandle::None, &lines);
		Check(lines.size() == 15 + 3 * (size_t)host::kGizmoRingSegments + 8, "three arrows of 5 lines, three rings of 48 and the ball's two diamonds of 4: " + std::to_string(lines.size()));
		host::Gizmo_Lines(camera, frame, false, host::GizmoHandle::None, &lines);
		Check(lines.size() == 15 + 8, "no rings for a kind with no turn, the ball kept");
		host::Gizmo_Lines(camera, frame, true, host::GizmoHandle::Centre, &lines);
		Check(lines.back().colour == host::kGizmoColourHot && lines[lines.size() - 8].colour == host::kGizmoColourHot && lines[0].colour == host::kGizmoColourX, "the ball hot: its diamonds yellow");
		host::Gizmo_Lines(camera, frame, true, host::GizmoHandle::MoveY, &lines);
		Check(lines[5].colour == host::kGizmoColourHot && lines[9].colour == host::kGizmoColourHot && lines[0].colour == host::kGizmoColourX && lines[10].colour ==
			host::kGizmoColourZ, "the hot handle (move y) yellow, the others in their colours");

		bool near = true;

		for (host::TrackLine const &line : lines)
		{
			near = near && line.from[2] > 300 && line.from[2] < 900 && line.to[2] > 300 && line.to[2] < 900;
		}

		Check(near, "every point pulled to about 600 mm ahead, nearer than any wall behind it");

		double tx, ty, px, py;

		Pixel(camera, frame.length, 0, 4000, &tx, &ty);
		Pixel(camera, lines[0].to[0], lines[0].to[1], lines[0].to[2], &px, &py);
		Check(fabs(tx - px) < 1.5 && fabs(ty - py) < 1.5, "the pulled x tip is drawn on the same pixel as the real one (" + N(px - tx) + ", " + N(py - ty) + ")");

		host::GizmoFrame none;

		host::Gizmo_Lines(camera, none, true, host::GizmoHandle::None, &lines);
		Check(lines.empty(), "a frame never made draws nothing");
	}

	printf("THE NAMES\n");
	Check(strcmp(host::Gizmo_HandleName(host::GizmoHandle::Centre), "centre ball") == 0 && host::Gizmo_AxisOf(host::GizmoHandle::Centre) == -1 &&
		!host::Gizmo_IsTurn(host::GizmoHandle::Centre), "the centre ball's name, and no axis");
	Check(strcmp(host::Gizmo_HandleName(host::GizmoHandle::TurnY), "turn y") == 0 && strcmp(host::Gizmo_HandleName(host::GizmoHandle::None), "none") == 0 &&
		host::Gizmo_AxisOf(host::GizmoHandle::MoveZ) == 2 && host::Gizmo_AxisOf(host::GizmoHandle::None) == -1 && host::Gizmo_IsTurn(host::GizmoHandle::TurnX) &&
		!host::Gizmo_IsTurn(host::GizmoHandle::MoveX), "the handles' names and axes");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
