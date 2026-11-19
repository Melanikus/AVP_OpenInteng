// host_surface_turn_test.cpp: placement mode's turn by the CLOSEST CARDINAL AXIS (src/host/surface_turn.h; the owner's order 11-19): the axis chosen (the largest dot with the normal,
// from the drag's start pose), the smallest rotation taking it onto the normal (the rest of the pose kept), the margin that keeps the previous axis near a tie, the bound that makes a
// half turn impossible, and the refusals. Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>

#include "surface_turn.h"

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

static double const kPi = 3.14159265358979323846;

// a turn of `degrees` about a unit axis, x y z w
static void TurnAbout(double ax, double ay, double az, double degrees, float out[4])
{
	double const half = degrees * kPi / 360.0;

	out[0] = (float)(ax * sin(half));
	out[1] = (float)(ay * sin(half));
	out[2] = (float)(az * sin(half));
	out[3] = (float)cos(half);
}

// the angle between two turns, in degrees (2 acos |<a, b>|)
static double AngleBetween(float const a[4], float const b[4])
{
	double d = fabs((double)a[0] * b[0] + (double)a[1] * b[1] + (double)a[2] * b[2] + (double)a[3] * b[3]);

	d = d > 1.0 ? 1.0 : d;
	return 2.0 * acos(d) * 180.0 / kPi;
}

static bool Near(double const a[3], double const b[3], double tolerance)
{
	return fabs(a[0] - b[0]) < tolerance && fabs(a[1] - b[1]) < tolerance && fabs(a[2] - b[2]) < tolerance;
}

static std::string N(double v)
{
	char text[48];

	sprintf_s(text, "%.4f", v);
	return text;
}

int main()
{
	float const identity[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	double const floorUp[3] = { 0.0, -1.0, 0.0 };   // out of a floor: up, -y (y is down)
	double const wallZ[3] = { 0.0, 0.0, -1.0 };
	float made[4];

	printf("THE AXES\n");
	Check(strcmp(host::SurfaceAxis_Name(0), "+x") == 0 && strcmp(host::SurfaceAxis_Name(3), "-y") == 0 && strcmp(host::SurfaceAxis_Name(5), "-z") == 0 &&
		strcmp(host::SurfaceAxis_Name(6), "none") == 0 && strcmp(host::SurfaceAxis_Name(-1), "none") == 0, "the six axes' names, none past them");
	{
		double up[3];
		float quarterX[4];

		TurnAbout(1, 0, 0, 90, quarterX);
		Check(host::SurfaceAxis_World(identity, 3, up) && Near(up, floorUp, 1e-12), "the identity's -y is the world's up, (0, -1, 0)");

		double const minusZ[3] = { 0.0, 1.0, 0.0 };   // a quarter turn about x takes local -z to +y (R(0, 0, -1) = (0, sin 90, -cos 90))

		Check(host::SurfaceAxis_World(quarterX, 5, up) && Near(up, minusZ, 1e-6), "turned 90 degrees about x, local -z points down (+y)");
	}

	printf("ON A FLOOR AND ONTO A WALL\n");
	{
		// (each call made before its Check: MSVC makes the message first, from the numbers before it)
		int const onFloor = host::SurfaceAxis_Closest(identity, floorUp, -1, host::kSurfaceAxisMarginDegrees);
		bool const floorMade = host::SurfaceTurn_AxisOnto(identity, onFloor, floorUp, made);
		double const floorTurned = AngleBetween(made, identity);

		Check(onFloor == 3 && floorMade && floorTurned < 1e-3, "an upright object on a floor: its -y meets it, and it stays upright (turned " + N(floorTurned) + " degrees)");

		int const onWall = host::SurfaceAxis_Closest(identity, wallZ, -1, host::kSurfaceAxisMarginDegrees);
		bool const wallMade = host::SurfaceTurn_AxisOnto(identity, onWall, wallZ, made);
		double const wallTurned = AngleBetween(made, identity);

		Check(onWall == 5 && wallMade && wallTurned < 1e-3,
			"an upright object slid onto a wall facing -z: its -z meets it, NOT turned (" + N(wallTurned) + " degrees; the old rule turned its up onto the wall: 90)");
	}
	{
		float quarterX[4];

		TurnAbout(1, 0, 0, 90, quarterX);

		int const axis = host::SurfaceAxis_Closest(quarterX, floorUp, -1, host::kSurfaceAxisMarginDegrees);
		bool const quarterMade = host::SurfaceTurn_AxisOnto(quarterX, axis, floorUp, made);
		double const quarterTurned = AngleBetween(made, quarterX);

		// the turn is given in floats (sin 45 to 7 places), so its +z is off the up by about 0.02 degrees: within 0.05 is kept
		Check(axis == 4 && quarterMade && quarterTurned < 0.05,
			std::string("an object turned 90 degrees about x, slid over a floor: its ") + host::SurfaceAxis_Name(axis) + " meets it, and it keeps that turn (turned " + N(quarterTurned) + " degrees)");
	}
	{
		float tilted[4];
		double meets[3];

		TurnAbout(0, 0, 1, 30, tilted);

		int const axis = host::SurfaceAxis_Closest(tilted, floorUp, -1, host::kSurfaceAxisMarginDegrees);
		bool const turned = host::SurfaceTurn_AxisOnto(tilted, axis, floorUp, made) && host::SurfaceAxis_World(made, axis, meets);

		Check(axis == 3 && turned && Near(meets, floorUp, 1e-5) && fabs(AngleBetween(made, tilted) - 30.0) < 1e-3,
			"tilted 30 degrees about z onto a floor: its -y meets it exactly, by a turn of exactly 30 degrees (" + N(AngleBetween(made, tilted)) + ")");
	}
	{
		// a long weapon lying along z, its heading 20 degrees about y, slid onto a wall facing +x: the side nearest the wall's normal meets it, by 20 degrees about y only
		float lying[4];
		double const wallX[3] = { 1.0, 0.0, 0.0 };
		double meets[3], up[3];

		TurnAbout(0, 1, 0, 20, lying);

		int const axis = host::SurfaceAxis_Closest(lying, wallX, -1, host::kSurfaceAxisMarginDegrees);
		bool const turned = host::SurfaceTurn_AxisOnto(lying, axis, wallX, made) && host::SurfaceAxis_World(made, axis, meets) && host::SurfaceAxis_World(made, 3, up);

		Check(axis == 0 && turned && Near(meets, wallX, 1e-5) && Near(up, floorUp, 1e-5) && fabs(AngleBetween(made, lying) - 20.0) < 1e-3,
			"a weapon lying along z, heading 20 degrees, slid onto a wall facing +x: its +x meets it, turned 20 degrees about y, still lying flat (its up still up)");
	}

	printf("THE SMALLEST TURN, ALWAYS (random poses and surfaces)\n");
	{
		unsigned seed = 12345u;
		auto next = [&seed]() { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffff) / 65535.0 * 2.0 - 1.0; };
		bool bound = true, exact = true, smallest = true;
		double worst = 1.0;

		for (int i = 0; i < 2000; i++)
		{
			float q[4] = { (float)next(), (float)next(), (float)next(), (float)next() };
			double n[3] = { next(), next(), next() };
			double const length = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

			if (length < 0.1)
			{
				continue;
			}

			for (int k = 0; k < 3; k++)
			{
				n[k] /= length;
			}

			int const axis = host::SurfaceAxis_Closest(q, n, -1, host::kSurfaceAxisMarginDegrees);
			double a[3], meets[3];

			if (axis < 0 || !host::SurfaceAxis_World(q, axis, a) || !host::SurfaceTurn_AxisOnto(q, axis, n, made) || !host::SurfaceAxis_World(made, axis, meets))
			{
				exact = false;
				continue;
			}

			double const dot = a[0] * n[0] + a[1] * n[1] + a[2] * n[2];
			double qn[4] = { q[0], q[1], q[2], q[3] };
			double const ql = sqrt(qn[0] * qn[0] + qn[1] * qn[1] + qn[2] * qn[2] + qn[3] * qn[3]);
			float unit[4] = { (float)(qn[0] / ql), (float)(qn[1] / ql), (float)(qn[2] / ql), (float)(qn[3] / ql) };

			worst = dot < worst ? dot : worst;
			bound = bound && dot >= 1.0 / sqrt(3.0) - 1e-9;
			exact = exact && Near(meets, n, 1e-4);
			smallest = smallest && fabs(AngleBetween(made, unit) - acos(dot > 1.0 ? 1.0 : dot) * 180.0 / kPi) < 0.02;
		}

		Check(bound, "the chosen axis's dot with the normal is never under 1/sqrt(3) (the least of 2000: " + N(worst) + "), so it is never opposite and Q never a half turn");
		Check(exact, "the chosen axis lands exactly on the normal");
		Check(smallest, "the turn made is exactly the angle between the axis and the normal: the smallest one");
	}

	printf("NEAR A TIE: THE PREVIOUS AXIS KEPT UNLESS ANOTHER WINS BY THE MARGIN\n");
	{
		double const r = 1.0 / sqrt(2.0);
		double const tie[3] = { 0.0, -r, -r };   // 45 degrees from both -y and -z

		Check(host::SurfaceAxis_Closest(identity, tie, -1, 3.0) == 3, "an exact tie with none before: the lower number (-y)");
		Check(host::SurfaceAxis_Closest(identity, tie, 5, 3.0) == 5, "an exact tie with -z before: -z kept");

		// tilted toward -y by 1 degree: -y is 44 degrees off, -z 46 (2 apart: within the margin) / by 2 degrees: 43 and 47 (4 apart: past it)
		double const t1 = (45.0 - 1.0) * kPi / 180.0, t2 = (45.0 - 2.0) * kPi / 180.0;
		double const by1[3] = { 0.0, -cos(t1), -sin(t1) }, by2[3] = { 0.0, -cos(t2), -sin(t2) };

		Check(host::SurfaceAxis_Closest(identity, by1, 5, 3.0) == 5, "-y nearer by 2 degrees: -z kept (no flicker)");
		Check(host::SurfaceAxis_Closest(identity, by2, 5, 3.0) == 3, "-y nearer by 4 degrees: -y takes over");
		Check(host::SurfaceAxis_Closest(identity, by1, -1, 3.0) == 3 && host::SurfaceAxis_Closest(identity, by1, 3, 3.0) == 3, "with none before, or -y before: -y");
		Check(host::SurfaceAxis_Closest(identity, floorUp, 7, 3.0) == 3 && host::SurfaceAxis_Closest(identity, floorUp, 5, 3.0) == 3,
			"a previous axis out of range is none; one 45 degrees worse than the best is never kept");
	}

	printf("REFUSALS\n");
	{
		double const none[3] = { 0.0, 0.0, 0.0 };
		double const nan[3] = { std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0 };
		float const zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float const bad[4] = { std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 1.0f };
		float kept[4] = { 7.0f, 7.0f, 7.0f, 7.0f };

		Check(host::SurfaceAxis_Closest(identity, none, -1, 3.0) == -1 && host::SurfaceAxis_Closest(identity, nan, -1, 3.0) == -1 &&
			host::SurfaceAxis_Closest(zero, floorUp, -1, 3.0) == -1 && host::SurfaceAxis_Closest(bad, floorUp, -1, 3.0) == -1, "no normal, a normal not a number, a turn of no length or not real: -1");
		Check(!host::SurfaceTurn_AxisOnto(identity, 6, floorUp, kept) && !host::SurfaceTurn_AxisOnto(identity, -1, floorUp, kept) &&
			!host::SurfaceTurn_AxisOnto(identity, 2, floorUp, kept) && !host::SurfaceTurn_AxisOnto(zero, 3, floorUp, kept) && !host::SurfaceTurn_AxisOnto(identity, 3, none, kept) &&
			kept[0] == 7.0f, "an axis out of range, one opposite the normal (+y onto a floor), no turn or no normal: false, nothing written");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
