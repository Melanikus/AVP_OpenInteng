// host_gizmo_test.cpp: the mouse gizmo's maths (src/host/gizmo.h): the frame's size on the screen, the hit test, an arrow's drag, a ring's drag and its angle, Shift's
// 15-degree snap, the moved place and the turned matrix, the lines and their pull toward the camera, and the refusals. A camera at the world's middle looking along +z
// (the identity turn) at a 1024 x 768 view; the expected pixels come from PickRay_Project, the game's own drawing, so the checks hold whatever its signs are. THE
// HIT looks from the owner's cube corner (45 degrees on every axis, 11-18), where no ring lies edge-on on an arrow's line, and each of its checks wins by 2 px or
// more (THE MARGIN, measured here): looking straight down an axis, three sat on exact ties that a CI machine's last bits decided (PR #87, the owner 11-21).
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
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

// THE CUBE CORNER (the owner's test camera, 11-18: "45 degrees on every axis, up and to the side, like a cube corner"; 11-21 for this test): the same view, looking
// along (1, 1, 1) at the object 4 m ahead, so it sees every axis at the same slant and no ring edge-on on an arrow's line. The matrix's columns are the camera's
// right (1, 0, -1), down (-1, 2, -1) and ahead (1, 1, 1), each made 1 long (PickRay_Project: each view number is one column times the place)
static host::PickCamera CornerCamera(int32_t const object[3])
{
	host::PickCamera c = Camera();
	double const r[3] = { 1.0 / sqrt(2.0), 0.0, -1.0 / sqrt(2.0) }, d[3] = { -1.0 / sqrt(6.0), 2.0 / sqrt(6.0), -1.0 / sqrt(6.0) }, f[3] = { 1.0 / sqrt(3.0), 1.0 / sqrt(3.0), 1.0 / sqrt(3.0) };

	for (int row = 0; row < 3; row++)
	{
		c.turn[row][0] = (int32_t)floor(r[row] * host::kPickOne + 0.5);
		c.turn[row][1] = (int32_t)floor(d[row] * host::kPickOne + 0.5);
		c.turn[row][2] = (int32_t)floor(f[row] * host::kPickOne + 0.5);
		c.place[row] = (int32_t)floor(object[row] - f[row] * 4000.0 + 0.5);
	}

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

static double ToSegment(double x, double y, double ax, double ay, double bx, double by)
{
	double const dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
	double t = length2 > 0.0 ? ((x - ax) * dx + (y - ay) * dy) / length2 : 0.0;

	t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
	return sqrt((x - ax - t * dx) * (x - ax - t * dx) + (y - ay - t * dy) * (y - ay - t * dy));
}

// THE MARGIN (the owner 11-21, "it's probably an angle of the camera issue again": PR #87's CI failed "12 px out along the x arrow" on one runner, an exact
// arrow/ring tie that the last bits of cos and sin decided). Each handle's distance on the screen from (x, y), measured here, not by the product: the centre
// point; each arrow, its centre to its tip; each ring, a fine circle of the frame's radius about its axis (the product's 48 straight pieces lie under 0.15 px
// inside it). [0] the centre, [1..3] move x/y/z, [4..6] turn x/y/z
static void HandleDistances(host::PickCamera const &camera, host::GizmoFrame const &frame, double x, double y, double out[7])
{
	double cx = 0.0, cy = 0.0;

	Pixel(camera, frame.centre[0], frame.centre[1], frame.centre[2], &cx, &cy);
	out[0] = sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));

	for (int a = 0; a < 3; a++)
	{
		double tx = 0.0, ty = 0.0;

		Pixel(camera, frame.centre[0] + frame.axis[a][0] * frame.length, frame.centre[1] + frame.axis[a][1] * frame.length, frame.centre[2] + frame.axis[a][2] * frame.length, &tx, &ty);
		out[1 + a] = ToSegment(x, y, cx, cy, tx, ty);

		// the ring about axis a: two directions square to it and to each other
		double const *n = frame.axis[a];
		double const pick[3] = { fabs(n[0]) < 0.9 ? 1.0 : 0.0, fabs(n[0]) < 0.9 ? 0.0 : 1.0, 0.0 };
		double u[3] = { n[1] * pick[2] - n[2] * pick[1], n[2] * pick[0] - n[0] * pick[2], n[0] * pick[1] - n[1] * pick[0] };
		double const ul = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);

		u[0] /= ul;
		u[1] /= ul;
		u[2] /= ul;

		double const v[3] = { n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0] };
		double best = 1e9, px = 0.0, py = 0.0;

		for (int k = 0; k <= 720; k++)
		{
			double const t = 2.0 * 3.14159265358979323846 * k / 720.0, c = cos(t), s = sin(t);
			double qx = 0.0, qy = 0.0;

			Pixel(camera, frame.centre[0] + frame.radius * (u[0] * c + v[0] * s), frame.centre[1] + frame.radius * (u[1] * c + v[1] * s),
				frame.centre[2] + frame.radius * (u[2] * c + v[2] * s), &qx, &qy);

			if (k > 0)
			{
				double const d = ToSegment(x, y, px, py, qx, qy);
				best = d < best ? d : best;
			}

			px = qx;
			py = qy;
		}

		out[4 + a] = best;
	}
}

// How clearly the expected handle wins at (x, y), in pixels: for a handle, how much nearer it is than every other one (and inside the hit's reach, and outside the
// centre ball's); for the centre ball, how far inside it; for none, how far outside every handle's reach. Under 2 px the check sits on (or near) a tie, and the
// machine's last bits can decide it
static double Margin(host::PickCamera const &camera, host::GizmoFrame const &frame, bool canRotate, double x, double y, host::GizmoHandle expected)
{
	double d[7];

	HandleDistances(camera, frame, x, y, d);

	int const last = canRotate ? 6 : 3;

	if (expected == host::GizmoHandle::Centre)
	{
		return host::kGizmoCentrePixels - d[0];
	}

	if (expected == host::GizmoHandle::None)
	{
		double nearest = d[0] - host::kGizmoCentrePixels;

		for (int i = 1; i <= last; i++)
		{
			nearest = (std::min)(nearest, d[i] - host::kGizmoHitPixels);
		}

		return nearest;
	}

	int const want = host::Gizmo_IsTurn(expected) ? 4 + host::Gizmo_AxisOf(expected) : 1 + host::Gizmo_AxisOf(expected);
	double margin = (std::min)(host::kGizmoHitPixels - d[want], d[0] - host::kGizmoCentrePixels);

	for (int i = 1; i <= last; i++)
	{
		margin = i == want ? margin : (std::min)(margin, d[i] - d[want]);
	}

	return margin;
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

	printf("THE HIT (the cube-corner camera: the owner's 45 degrees on every axis, so no ring lies edge-on on an arrow's line)\n");
	{
		host::PickCamera const corner = CornerCamera(place);
		host::GizmoFrame cf;

		bool const framed = host::Gizmo_Frame(corner, place, nullptr, false, &cf);   // (first: a check's words are made before or after its test, as the compiler likes)

		Check(framed && fabs(cf.ahead - 4000.0) < 2.0, "the cube-corner camera sees the object 4 m ahead (" + N(cf.ahead) + ")");

		double cx, cy, ax[3], ay[3];

		Pixel(corner, cf.centre[0], cf.centre[1], cf.centre[2], &cx, &cy);

		for (int a = 0; a < 3; a++)   // each arrow's middle on the screen
		{
			Pixel(corner, cf.centre[0] + cf.axis[a][0] * cf.length / 2, cf.centre[1] + cf.axis[a][1] * cf.length / 2, cf.centre[2] + cf.axis[a][2] * cf.length / 2, &ax[a], &ay[a]);
		}

		// every hit check also says how clearly its handle wins, and wants 2 px: a check on a tie passes or fails by the machine's last bits (PR #87's CI)
		auto const hit = [&](bool canRotate, double x, double y, host::GizmoHandle expected, std::string const &what)
		{
			host::GizmoHandle const got = host::Gizmo_Hit(corner, cf, canRotate, x, y);
			double const margin = Margin(corner, cf, canRotate, x, y, expected);

			Check(got == expected && margin >= 2.0, what + " (" + host::Gizmo_HandleName(got) + ", by " + N(margin) + " px)");
		};

		// a place `off` px square to arrow a's line at its middle, on the side where `expected` wins more clearly (the view decides which side is open)
		auto const beside = [&](int a, double off, bool canRotate, host::GizmoHandle expected, double *x, double *y)
		{
			double const dx = ax[a] - cx, dy = ay[a] - cy, l = sqrt(dx * dx + dy * dy), nx = -dy / l, ny = dx / l;
			double const m1 = Margin(corner, cf, canRotate, ax[a] + off * nx, ay[a] + off * ny, expected), m2 = Margin(corner, cf, canRotate, ax[a] - off * nx, ay[a] - off * ny, expected);

			*x = ax[a] + (m1 >= m2 ? off : -off) * nx;
			*y = ay[a] + (m1 >= m2 ? off : -off) * ny;
		};
		double x, y;

		beside(0, 3.0, true, host::GizmoHandle::MoveX, &x, &y);
		hit(true, x, y, host::GizmoHandle::MoveX, "3 px off the x arrow's middle: move x");
		beside(1, 3.0, true, host::GizmoHandle::MoveY, &x, &y);
		hit(true, x, y, host::GizmoHandle::MoveY, "beside the y arrow: move y");
		{
			// (30 px square to the arrow's middle lands on the x ring in this view: 30 px past its tip, along it, every ring is far off too)
			double tx, ty;

			Pixel(corner, cf.centre[0] + cf.axis[0][0] * cf.length, cf.centre[1] + cf.axis[0][1] * cf.length, cf.centre[2] + cf.axis[0][2] * cf.length, &tx, &ty);

			double const dx = tx - cx, dy = ty - cy, l = sqrt(dx * dx + dy * dy);

			hit(true, tx + 30 * dx / l, ty + 30 * dy / l, host::GizmoHandle::None, "30 px off the x arrow, past its tip: nothing");
		}
		hit(true, cx, cy, host::GizmoHandle::Centre, "on the centre, where all three arrows start: the centre ball (before the arrows)");
		hit(false, cx + 6, cy, host::GizmoHandle::Centre, "6 px off it (inside its 9), for a kind with no turn: the centre ball");
		{
			double const dx = ax[0] - cx, dy = ay[0] - cy, l = sqrt(dx * dx + dy * dy);

			hit(true, cx + 12 * dx / l, cy + 12 * dy / l, host::GizmoHandle::MoveX, "12 px out along the x arrow: the arrow again");
		}

		// on the z ring, half way round between the x and the y arrows (the ring about z lies in the x-y plane)
		double rx, ry;
		double const h = cf.radius / sqrt(2.0);

		Pixel(corner, cf.centre[0] + h * (cf.axis[0][0] + cf.axis[1][0]), cf.centre[1] + h * (cf.axis[0][1] + cf.axis[1][1]), cf.centre[2] + h * (cf.axis[0][2] + cf.axis[1][2]), &rx, &ry);
		hit(true, rx, ry, host::GizmoHandle::TurnZ, "on the z ring, between the x and y arrows: turn z");
		hit(false, rx, ry, host::GizmoHandle::None, "a kind with no turn has no rings");
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
