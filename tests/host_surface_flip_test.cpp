// The test for placement mode's FLIP (src/host/surface_turn.h SurfaceFlip_Turn, SurfaceFlip_DownAxis, SurfaceFlip_Words, SurfaceTurn_Then). Written 2026-11-24 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The owner: "I want "F" for "Flip" each press rotates 90% around an axis, after
// 4 presses goes to next axis ... (first in up axis) then left/right/front/back and down", "So 6x "faces" of a cube times 4x angles". Every direction is read through
// SurfaceAxis_World, the slide's own (and tested) way of turning a local axis, so the flips mean what the slide will draw.
//   (1) EACH FACE DOWN: flip k puts its named side down (+y, y is down) and turns its own up the other way: upright +y, left side -x, right +x, front +z, back -z, upside down -y
//   (2) THE QUARTER TURNS: inside a face the down side stays down, and each step turns the object a quarter of a turn about up, the same way round every time
//   (3) ALL 24 DIFFERENT, every one a whole turn of a cube (each axis onto an axis), 1 long; the order the owner gave (4 upright first)
//   (4) ROUND THE 24: 24 is 0 again, 25 is 1, -1 is 23; any whole number gives one of them
//   (5) THEN: SurfaceTurn_Then(a, b) is R(a) R(b) (b first), checked against the axes; a turn of no length or not a number refused, out untouched
//   (6) THE WORDS: "upright, 0" .. "upside down, 270"
// RED FIRST: these functions don't exist before this card (the test doesn't build on bishop-int 6e8f9150).
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>

#include "surface_turn.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// the world direction of local axis `axis` under turn t, rounded to whole numbers (a cube's turn takes every axis onto an axis)
static void Axis(float const t[4], int axis, int out[3])
{
	double d[3] = {};

	SurfaceAxis_World(t, axis, d);

	for (int i = 0; i < 3; i++)
	{
		out[i] = (int)floor(d[i] + 0.5);
	}
}

static bool Same(int const a[3], int const b[3])
{
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

// how far each of the turn's axes is from the nearest whole axis (0 for a cube's turn)
static double OffAxis(float const t[4])
{
	double worst = 0;

	for (int axis = 0; axis < kSurfaceAxisCount; axis++)
	{
		double d[3] = {};

		SurfaceAxis_World(t, axis, d);

		for (int i = 0; i < 3; i++)
		{
			double const off = fabs(d[i] - floor(d[i] + 0.5));

			worst = off > worst ? off : worst;
		}
	}

	return worst;
}

static std::string Words(int const v[3])
{
	return "(" + std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]) + ")";
}

int main()
{
	int const down[3] = { 0, 1, 0 };   // +y: y is down
	struct { int axis; char const *name; } const faces[6] = { { 2, "upright (+y down)" }, { 1, "on its left side (-x down)" }, { 0, "on its right side (+x down)" },
		{ 4, "on its front (+z down)" }, { 5, "on its back (-z down)" }, { 3, "upside down (-y down)" } };

	// ---- (1) each face down, its own up the other way
	for (int face = 0; face < 6; face++)
	{
		for (int q = 0; q < 4; q++)
		{
			int const k = face * 4 + q;
			float t[4];
			int got[3], upGot[3];

			SurfaceFlip_Turn(k, t);
			Axis(t, faces[face].axis, got);
			Axis(t, faces[face].axis ^ 1, upGot);   // the opposite side (0<->1, 2<->3, 4<->5)

			int const up[3] = { 0, -1, 0 };

			Check(Same(got, down) && Same(upGot, up) && SurfaceFlip_DownAxis(k) == faces[face].axis, "(1) flip " + std::to_string(k) + ": " + faces[face].name +
				": that side goes " + Words(got) + ", the other " + Words(upGot) + ", down axis " + SurfaceAxis_Name(SurfaceFlip_DownAxis(k)));
		}
	}

	// ---- (2) the quarter turns about up: in each face, the axis that lay along +x before a step lies along the next one round up after it, the same way every time
	for (int face = 0; face < 6; face++)
	{
		bool turnsRight = true;
		std::string seen;

		for (int q = 0; q < 4; q++)
		{
			float a[4], b[4];
			int side = 0;

			SurfaceFlip_Turn(face * 4 + q, a);
			SurfaceFlip_Turn(face * 4 + (q + 1) % 4, b);

			// the local axis lying along world +x under flip a (it is a horizontal one: the down axis is along y)
			for (int axis = 0; axis < kSurfaceAxisCount; axis++)
			{
				int d[3];

				Axis(a, axis, d);
				side = d[0] == 1 ? axis : side;
			}

			int after[3];

			Axis(b, side, after);
			seen += (seen.empty() ? "" : " ") + Words(after);
			turnsRight = turnsRight && after[0] == 0 && after[1] == 0 && after[2] == 1;   // +x goes to +z each step: a quarter turn about y, always the same way
		}

		Check(turnsRight, "(2) " + std::string(faces[face].name) + ": each F a quarter turn about up, the same way round (+x goes to " + seen + ")");
	}

	// ---- (3) all 24 different, each a cube's turn, 1 long
	float all[kSurfaceFlipCount][4];
	bool cube = true, unit = true;

	for (int k = 0; k < kSurfaceFlipCount; k++)
	{
		SurfaceFlip_Turn(k, all[k]);
		cube = cube && OffAxis(all[k]) < 1e-5;
		unit = unit && fabs(sqrt(all[k][0] * all[k][0] + all[k][1] * all[k][1] + all[k][2] * all[k][2] + all[k][3] * all[k][3]) - 1.0) < 1e-5;
	}

	int sameAs = -1, other = -1;

	for (int a = 0; a < kSurfaceFlipCount && sameAs < 0; a++)
	{
		for (int b = a + 1; b < kSurfaceFlipCount; b++)
		{
			bool same = true;

			for (int axis = 0; axis < kSurfaceAxisCount; axis++)
			{
				int da[3], db[3];

				Axis(all[a], axis, da);
				Axis(all[b], axis, db);
				same = same && Same(da, db);
			}

			if (same)
			{
				sameAs = a;
				other = b;
				break;
			}
		}
	}

	Check(kSurfaceFlipCount == 24, "(3) 24 flips (6 faces x 4 quarter turns)");
	Check(sameAs < 0, "(3) all 24 different" + (sameAs < 0 ? std::string() : ": " + std::to_string(sameAs) + " and " + std::to_string(other) + " are the same"));
	Check(cube && unit, "(3) each a whole turn of a cube (every axis onto an axis), 1 long");

	float identity[4];
	SurfaceFlip_Turn(0, identity);
	Check(OffAxis(identity) < 1e-6 && fabs(identity[3]) > 0.99999f, "(3) flip 0 is no turn: as the slide puts it (the owner: first about up)");

	// ---- (4) round the 24
	struct { int k, as; } const round[] = { { 24, 0 }, { 25, 1 }, { 47, 23 }, { 48, 0 }, { -1, 23 }, { -24, 0 }, { -25, 23 }, { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() % 24 },
		{ std::numeric_limits<int>::min(), ((std::numeric_limits<int>::min() % 24) + 24) % 24 } };

	for (auto const &r : round)
	{
		float a[4], b[4];

		SurfaceFlip_Turn(r.k, a);
		SurfaceFlip_Turn(r.as, b);
		Check(memcmp(a, b, sizeof(a)) == 0 && SurfaceFlip_DownAxis(r.k) == SurfaceFlip_DownAxis(r.as) && SurfaceFlip_Words(r.k) == SurfaceFlip_Words(r.as),
			"(4) flip " + std::to_string(r.k) + " is flip " + std::to_string(r.as));
	}

	// ---- (5) then: R(a) R(b), b first
	float a[4], b[4], ab[4], ba[4];

	SurfaceFlip_Turn(5, a);    // on its left side, a quarter turn
	SurfaceFlip_Turn(13, b);   // on its front, a quarter turn
	Check(SurfaceTurn_Then(a, b, ab) && SurfaceTurn_Then(b, a, ba), "(5) two flips put together");

	bool composed = true;

	for (int axis = 0; axis < kSurfaceAxisCount; axis++)
	{
		double viaB[3], want[3], got[3];
		int wantAxis = -1;

		SurfaceAxis_World(b, axis, viaB);   // b first: where b sends the axis ...

		for (int other2 = 0; other2 < kSurfaceAxisCount; other2++)   // ... is a whole axis; then a sends that one
		{
			double d[3];

			SurfaceAxis_World(identity, other2, d);
			wantAxis = fabs(d[0] - viaB[0]) + fabs(d[1] - viaB[1]) + fabs(d[2] - viaB[2]) < 1e-5 ? other2 : wantAxis;
		}

		SurfaceAxis_World(a, wantAxis < 0 ? 0 : wantAxis, want);
		SurfaceAxis_World(ab, axis, got);
		composed = composed && wantAxis >= 0 && fabs(got[0] - want[0]) + fabs(got[1] - want[1]) + fabs(got[2] - want[2]) < 1e-5;
	}

	Check(composed, "(5) Then(a, b) turns each axis by b, then by a");
	Check(memcmp(ab, ba, sizeof(ab)) != 0, "(5) the order matters: Then(a, b) is not Then(b, a) for these two");

	float none[4] = { 0, 0, 0, 0 }, nan[4] = { 0, 0, 0, std::numeric_limits<float>::quiet_NaN() }, kept[4] = { 7, 7, 7, 7 };

	Check(!SurfaceTurn_Then(none, a, kept) && !SurfaceTurn_Then(a, nan, kept) && kept[0] == 7 && kept[3] == 7, "(5) a turn of no length, or not a number: refused, out untouched");

	// ---- (6) the words
	Check(SurfaceFlip_Words(0) == "upright, 0" && SurfaceFlip_Words(1) == "upright, 90" && SurfaceFlip_Words(7) == "on its left side, 270" &&
		SurfaceFlip_Words(8) == "on its right side, 0" && SurfaceFlip_Words(14) == "on its front, 180" && SurfaceFlip_Words(17) == "on its back, 90" &&
		SurfaceFlip_Words(23) == "upside down, 270", "(6) the words: " + SurfaceFlip_Words(0) + " / " + SurfaceFlip_Words(7) + " / " + SurfaceFlip_Words(23));

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
