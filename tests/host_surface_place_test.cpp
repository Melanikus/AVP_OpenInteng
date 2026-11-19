// The test for src/host/surface_place: placement mode's pose (an object dragged over walls and floors, its base flat on the surface; the owner's addition 11-18, Hicks's plan section 5).
// Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the pose is pure maths, and a sign slip (the world is y DOWN) or a turn read the wrong way round would stand an object on its head or sink it into the floor, which no picture shows
// plainly. The turn is the file's x y z w, and every check turns the shape's points with inteng's own model::RotateByTurn (FUN_00430e80), which is how the game draws them (surface_place.h
// says why), so the checks hold for what is seen, not only for this file's own sums. On a floor, a wall, a ceiling and two slopes it checks: the object's up (its local -y) comes out along
// the surface's normal; the base's middle (local (0, the box's largest y, 0)) lands on the hit point; the turn is of length 1; an upright object on a floor keeps its turn exactly; on a
// wall the smallest turn leaves the front alone. Then a move-only kind (the hit point only), and the refusals (a normal or a turn of no length, a turn not a number, a place past 2^24).
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "shape_maths.h"
#include "surface_place.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

static float const kUpright[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

// a quarter turn about the world's y (the object faces another way on the floor)
static float const kQuarterAboutY[4] = { 0.0f, 0.70710678f, 0.0f, 0.70710678f };

// a long local vector, so the whole numbers RotateByTurn gives back carry the direction to 1 part in a million
static double const kLong = 1000000.0;

static host::SurfacePlaceInput Input(int32_t const hit[3], int32_t const normal[3], float const turn[4], bool canTurn)
{
	host::SurfacePlaceInput in;

	memset(&in, 0, sizeof(in));

	for (int i = 0; i < 3; i++)
	{
		in.hitPoint[i] = hit[i];
		in.hitNormal[i] = normal[i];
	}

	// a crate's box: 300 either side, 800 above its place, its base 500 below it (y down)
	in.boxMin[0] = -300;
	in.boxMin[1] = -800;
	in.boxMin[2] = -300;
	in.boxMax[0] = 300;
	in.boxMax[1] = 500;
	in.boxMax[2] = 300;

	for (int i = 0; i < 4; i++)
	{
		in.turn[i] = turn[i];
	}

	in.canTurn = canTurn;
	return in;
}

// local (x, y, z) turned by the turn as inteng and the game turn a shape's point
static bool Turned(float const turn[4], double x, double y, double z, double out[3])
{
	model::ShapePoint local;
	model::ShapePoint world;

	local.x = (int32_t)x;
	local.y = (int32_t)y;
	local.z = (int32_t)z;

	if (model::RotateByTurn(turn, local, &world) != model::ShapeMathsResult::Ok)
	{
		return false;
	}

	out[0] = world.x;
	out[1] = world.y;
	out[2] = world.z;
	return true;
}

static bool Near(double a, double b, double tolerance)
{
	return fabs(a - b) <= tolerance;
}

// the same turn to float rounding (q, not -q: the pose keeps the old one's signs)
static bool SameTurn(float const a[4], float const b[4])
{
	for (int i = 0; i < 4; i++)
	{
		if (!Near(a[i], b[i], 1e-6))
		{
			return false;
		}
	}

	return true;
}

static std::string Words(int32_t const v[3])
{
	return std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]);
}

static std::string Words(float const q[4])
{
	char words[96];

	snprintf(words, sizeof(words), "%.5f %.5f %.5f %.5f", (double)q[0], (double)q[1], (double)q[2], (double)q[3]);
	return words;
}

// The checks every turning pose must pass: up out along the (unit) normal, the turn of length 1, the base's middle on the hit point
static void CheckPose(char const *where, host::SurfacePlaceInput const &in, host::SurfacePlacePose const &pose, double const unitNormal[3])
{
	double up[3];
	double base[3];
	bool const turned = Turned(pose.turn, 0.0, -kLong, 0.0, up) && Turned(pose.turn, 0.0, (double)in.boxMax[1], 0.0, base);

	Check(turned && Near(up[0], unitNormal[0] * kLong, 2.0) && Near(up[1], unitNormal[1] * kLong, 2.0) && Near(up[2], unitNormal[2] * kLong, 2.0),
		std::string(where) + ": the object's up (its local -y, turned as the game draws it) comes out along the surface's normal (turn " + Words(pose.turn) + ")");

	double const length = sqrt((double)pose.turn[0] * pose.turn[0] + (double)pose.turn[1] * pose.turn[1] + (double)pose.turn[2] * pose.turn[2] + (double)pose.turn[3] * pose.turn[3]);

	Check(Near(length, 1.0, 1e-5), std::string(where) + ": the new turn is of length 1");

	bool onHit = turned;

	for (int i = 0; i < 3; i++)
	{
		onHit = onHit && Near(pose.place[i] + base[i], (double)in.hitPoint[i], 1.5);
	}

	Check(onHit, std::string(where) + ": the base's middle rests on the hit point, not in the surface nor off it (place " + Words(pose.place) + ")");

	// THE HEADING: the smallest turn goes about k = (the old up) x (the normal), so the part of the object that pointed along k before still does (with the old up along the normal or
	// against it there is no such k: the floor's and the ceiling's own checks say what is kept)
	double const oldLength = sqrt((double)in.turn[0] * in.turn[0] + (double)in.turn[1] * in.turn[1] + (double)in.turn[2] * in.turn[2] + (double)in.turn[3] * in.turn[3]);
	float const old[4] = { (float)(in.turn[0] / oldLength), (float)(in.turn[1] / oldLength), (float)(in.turn[2] / oldLength), (float)(in.turn[3] / oldLength) };
	float const back[4] = { -old[0], -old[1], -old[2], old[3] };   // the old turn undone
	double oldUp[3];

	if (!Turned(old, 0.0, -kLong, 0.0, oldUp))
	{
		Check(false, std::string(where) + ": the old up can be turned");
		return;
	}

	double k[3] = { oldUp[1] * unitNormal[2] - oldUp[2] * unitNormal[1], oldUp[2] * unitNormal[0] - oldUp[0] * unitNormal[2], oldUp[0] * unitNormal[1] - oldUp[1] * unitNormal[0] };
	double const kLength = sqrt(k[0] * k[0] + k[1] * k[1] + k[2] * k[2]);

	if (kLength < 0.01 * kLong)
	{
		return;
	}

	double local[3], now[3];

	for (int i = 0; i < 3; i++)
	{
		k[i] = k[i] / kLength * kLong;
	}

	bool const kept = Turned(back, k[0], k[1], k[2], local) && Turned(pose.turn, local[0], local[1], local[2], now) && Near(now[0], k[0], 4.0) && Near(now[1], k[1], 4.0) && Near(now[2], k[2], 4.0);

	Check(kept, std::string(where) + ": the heading is kept: what pointed along the smallest turn's axis (old up x normal) still does");
}

static void TestTheTurnsMeaning()
{
	puts("The turn is the file's, read as the game draws it:");

	double x[3];

	Check(Turned(kQuarterAboutY, kLong, 0.0, 0.0, x) && Near(x[0], 0.0, 2.0) && Near(x[2], -kLong, 2.0), "a quarter turn about y (0, 0.707, 0, 0.707) sends the shape's +x to the world's -z (RotateByTurn)");
}

static void TestFloor()
{
	puts("A floor (its normal up, -y; the world is y down):");

	int32_t const hit[3] = { 1000, 2000, -3000 };
	int32_t const normal[3] = { 0, -65536, 0 };
	double const unit[3] = { 0.0, -1.0, 0.0 };
	host::SurfacePlaceInput in = Input(hit, normal, kUpright, true);
	host::SurfacePlacePose pose;

	Check(host::SurfacePlace_Pose(in, &pose), "floor: a pose is given");
	CheckPose("floor", in, pose, unit);
	Check(SameTurn(pose.turn, kUpright), "floor: an upright object keeps its turn (" + Words(pose.turn) + ")");
	Check(pose.place[0] == 1000 && pose.place[1] == 1500 && pose.place[2] == -3000, "floor: its place is 500 above the hit point (y 2000 - 500 = 1500), its base 500 below its place");

	in = Input(hit, normal, kQuarterAboutY, true);
	Check(host::SurfacePlace_Pose(in, &pose), "floor, turned a quarter: a pose is given");
	Check(SameTurn(pose.turn, kQuarterAboutY), "floor, turned a quarter: its heading is kept (the turn is the old one)");

	float const longTurn[4] = { 0.0f, 0.0f, 0.0f, 2.0f };

	in = Input(hit, normal, longTurn, true);
	Check(host::SurfacePlace_Pose(in, &pose) && SameTurn(pose.turn, kUpright), "floor, a turn twice too long (0 0 0 2): the same turn comes back of length 1 (" + Words(pose.turn) + ")");
}

static void TestOnItsSide()
{
	puts("An object lying on its side (a quarter about z: its up along +x), onto a wall facing +z:");

	float const onItsSide[4] = { 0.0f, 0.0f, 0.70710678f, 0.70710678f };
	int32_t const hit[3] = { 600, -200, 4000 };
	int32_t const normal[3] = { 0, 0, 65536 };
	double const unit[3] = { 0.0, 0.0, 1.0 };
	host::SurfacePlaceInput const in = Input(hit, normal, onItsSide, true);
	host::SurfacePlacePose pose;
	double up[3];

	Check(Turned(onItsSide, 0.0, -kLong, 0.0, up) && Near(up[0], kLong, 2.0), "on its side: its up is along +x before (RotateByTurn)");
	Check(host::SurfacePlace_Pose(in, &pose), "on its side, onto the wall: a pose is given");
	CheckPose("on its side, onto the wall", in, pose, unit);
}

static void TestWall()
{
	puts("A wall facing +x:");

	int32_t const hit[3] = { -5000, 1200, 700 };
	int32_t const normal[3] = { 65536, 0, 0 };
	double const unit[3] = { 1.0, 0.0, 0.0 };
	host::SurfacePlaceInput in = Input(hit, normal, kUpright, true);
	host::SurfacePlacePose pose;

	Check(host::SurfacePlace_Pose(in, &pose), "wall: a pose is given");
	CheckPose("wall", in, pose, unit);
	Check(pose.place[0] == -4500 && pose.place[1] == 1200 && pose.place[2] == 700, "wall: its place is 500 out from the wall along +x");

	double front[3];

	Check(Turned(pose.turn, 0.0, 0.0, kLong, front) && Near(front[0], 0.0, 2.0) && Near(front[1], 0.0, 2.0) && Near(front[2], kLong, 2.0),
		"wall: the smallest turn (about the z axis): the object's front (local +z) is untouched");

	// the same wall with the object turned a quarter first: up still out of the wall, the base on it
	in = Input(hit, normal, kQuarterAboutY, true);
	Check(host::SurfacePlace_Pose(in, &pose), "wall, turned a quarter first: a pose is given");
	CheckPose("wall, turned a quarter first", in, pose, unit);
}

static void TestCeiling()
{
	puts("A ceiling (its normal down, +y):");

	int32_t const hit[3] = { 0, -4000, 0 };
	int32_t const normal[3] = { 0, 65536, 0 };
	double const unit[3] = { 0.0, 1.0, 0.0 };
	host::SurfacePlaceInput in = Input(hit, normal, kUpright, true);
	host::SurfacePlacePose pose;

	Check(host::SurfacePlace_Pose(in, &pose), "ceiling: a pose is given");
	CheckPose("ceiling", in, pose, unit);

	double x[3], z[3];

	Check(Turned(pose.turn, kLong, 0.0, 0.0, x) && Turned(pose.turn, 0.0, 0.0, kLong, z) && Near(x[0], kLong, 2.0) && Near(z[2], -kLong, 2.0),
		"ceiling: an upright object (up exactly opposite the normal) takes half a turn about its own x axis (x kept, z reversed)");
	Check(pose.place[0] == 0 && pose.place[1] == -3500 && pose.place[2] == 0, "ceiling: its place is 500 below the ceiling");

	// turned a quarter first, its own x axis is the world's -z: the half turn is about that, not the world's x
	in = Input(hit, normal, kQuarterAboutY, true);
	Check(host::SurfacePlace_Pose(in, &pose), "ceiling, turned a quarter first: a pose is given");
	CheckPose("ceiling, turned a quarter first", in, pose, unit);
	Check(Turned(pose.turn, kLong, 0.0, 0.0, x) && Near(x[0], 0.0, 2.0) && Near(x[1], 0.0, 2.0) && Near(x[2], -kLong, 2.0),
		"ceiling, turned a quarter first: the half turn is about the object's own x axis (its +x still along the world's -z)");
}

static void TestSlope()
{
	puts("Slopes:");

	int32_t const hit[3] = { 250, 3000, 9000 };
	int32_t const normal[3] = { 0, -46341, 46341 };
	double const unit[3] = { 0.0, -0.70710678, 0.70710678 };
	host::SurfacePlaceInput in = Input(hit, normal, kQuarterAboutY, true);
	host::SurfacePlacePose pose;

	Check(host::SurfacePlace_Pose(in, &pose), "slope (45 degrees, facing up and +z), the object turned a quarter: a pose is given");
	CheckPose("slope", in, pose, unit);

	// a 35-degree slope with a normal not of length 1 (the cast's normal is taken as a direction only), and a turn not of length 1 (the file's floats as they come)
	int32_t const longNormal[3] = { 3 * 37590, 3 * -53684, 0 };
	double const length = sqrt(37590.0 * 37590.0 + 53684.0 * 53684.0);
	double const longUnit[3] = { 37590.0 / length, -53684.0 / length, 0.0 };
	float const longTurn[4] = { 0.0f, 0.0f, 0.0f, 2.0f };

	in = Input(hit, longNormal, longTurn, true);
	Check(host::SurfacePlace_Pose(in, &pose), "slope, a normal three times too long and a turn twice too long: a pose is given");
	CheckPose("slope, a normal three times too long and a turn twice too long", in, pose, longUnit);
}

static void TestMoveOnly()
{
	puts("A kind that holds no turn (a sound, a generator, a bad guy, a virtual object):");

	int32_t const hit[3] = { -5000, 1200, 700 };
	int32_t const normal[3] = { 65536, 0, 0 };
	host::SurfacePlaceInput in = Input(hit, normal, kQuarterAboutY, false);
	host::SurfacePlacePose pose;

	Check(host::SurfacePlace_Pose(in, &pose), "move only: a pose is given");
	Check(pose.place[0] == -5000 && pose.place[1] == 1200 && pose.place[2] == 700, "move only: its place is the hit point itself (no push by a box)");
	Check(memcmp(pose.turn, kQuarterAboutY, sizeof(pose.turn)) == 0, "move only: its turn is untouched, on a wall too");
}

static void TestRefused()
{
	puts("Refused, the pose untouched:");

	int32_t const hit[3] = { 1, 2, 3 };
	int32_t const none[3] = { 0, 0, 0 };
	int32_t const wall[3] = { 65536, 0, 0 };
	float const zeroTurn[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float const notANumber[4] = { 0.0f, 0.0f, 0.0f, (float)nan("") };
	host::SurfacePlacePose pose;
	host::SurfacePlacePose const before = { { 11, 22, 33 }, { 1.0f, 2.0f, 3.0f, 4.0f } };

	pose = before;
	Check(!host::SurfacePlace_Pose(Input(hit, none, kUpright, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a normal of no length is refused");
	pose = before;
	Check(!host::SurfacePlace_Pose(Input(hit, none, kUpright, false), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a normal of no length is refused for a move-only kind too");
	pose = before;
	Check(!host::SurfacePlace_Pose(Input(hit, wall, zeroTurn, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a turn of no length (all zeros) is refused");
	pose = before;
	Check(!host::SurfacePlace_Pose(Input(hit, wall, notANumber, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a turn that is not a number is refused");

	float const infinite[4] = { (float)HUGE_VAL, 0.0f, 0.0f, 1.0f };

	pose = before;
	Check(!host::SurfacePlace_Pose(Input(hit, wall, infinite, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a turn with an infinite number is refused");

	int32_t const edge[3] = { 0, -16777000, 0 };
	int32_t const up[3] = { 0, -65536, 0 };

	pose = before;
	Check(!host::SurfacePlace_Pose(Input(edge, up, kUpright, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a place pushed past 2^24 is refused");
}

static host::SurfaceLiveInput Live(int32_t const hit[3], int32_t const normal[3], int32_t const camera[3], double scale, int32_t base, float const turn[4], bool canTurn)
{
	host::SurfaceLiveInput in;

	memset(&in, 0, sizeof(in));

	for (int i = 0; i < 3; i++)
	{
		in.hitWorld[i] = hit[i];
		in.hitNormal[i] = normal[i];
		in.cameraWorld[i] = camera[i];
	}

	for (int i = 0; i < 4; i++)
	{
		in.turn[i] = turn[i];
	}

	in.scale = scale;
	in.baseWorld = base;
	in.canTurn = canTurn;
	return in;
}

static void TestLive()
{
	puts("One frame of the live drag (the game's world units in, the file's out):");

	int32_t const hit[3] = { 1000, 2000, -3000 };
	int32_t const up[3] = { 0, -65536, 0 };
	int32_t const down[3] = { 0, 65536, 0 };
	int32_t const above[3] = { 1000, -500, 0 };   // the camera above the floor (y down: a smaller y)
	host::SurfacePlacePose pose;

	Check(host::SurfacePlace_Live(Live(hit, up, above, 1.0, 500, kUpright, true), &pose) && pose.place[0] == 1000 && pose.place[1] == 1500 && pose.place[2] == -3000 &&
		SameTurn(pose.turn, kUpright), "a floor seen from above, scale 1: the place 500 above the hit, upright");
	Check(host::SurfacePlace_Live(Live(hit, down, above, 1.0, 500, kUpright, true), &pose) && pose.place[1] == 1500 && SameTurn(pose.turn, kUpright),
		"the same floor with its normal given the other way (a face seen from behind): turned round to face the camera, the same pose");

	int32_t const hit2[3] = { 2000, 4000, -6000 };
	int32_t const above2[3] = { 2000, -1000, 0 };

	Check(host::SurfacePlace_Live(Live(hit2, up, above2, 2.0, 1000, kUpright, true), &pose) && pose.place[0] == 1000 && pose.place[1] == 1500 && pose.place[2] == -3000,
		"scale 2: the world's place (2000, 3000, -6000) is the file's (1000, 1500, -3000) (" + Words(pose.place) + ")");

	int32_t const wallHit[3] = { -5000, 1200, 700 };
	int32_t const wallNormal[3] = { 65536, 0, 0 };
	int32_t const leftOfIt[3] = { -10000, 1200, 700 };   // the camera on the wall's -x side: its +x normal faces away
	double upNow[3];

	Check(host::SurfacePlace_Live(Live(wallHit, wallNormal, leftOfIt, 1.0, 500, kUpright, true), &pose) && Turned(pose.turn, 0.0, -kLong, 0.0, upNow) && Near(upNow[0], -kLong, 2.0) &&
		pose.place[0] == -5500, "a wall seen from its back: the object stands out towards the camera (up along -x, the place 500 on that side)");

	int32_t const onPlane[3] = { -5000, 9000, 700 };   // the camera in the wall's own plane: no side to choose, the normal as given

	Check(host::SurfacePlace_Live(Live(wallHit, wallNormal, onPlane, 1.0, 500, kUpright, true), &pose) && pose.place[0] == -4500, "the camera in the face's own plane: the normal as given");
	Check(host::SurfacePlace_Live(Live(hit2, wallNormal, above2, 2.0, 1000, kQuarterAboutY, false), &pose) && pose.place[0] == 1000 && pose.place[1] == 2000 && pose.place[2] == -3000 &&
		memcmp(pose.turn, kQuarterAboutY, sizeof(pose.turn)) == 0, "move only, scale 2: the hit point in the file's units, the turn untouched");

	host::SurfacePlacePose const before = { { 11, 22, 33 }, { 1.0f, 2.0f, 3.0f, 4.0f } };
	int32_t const broken[3] = { INT32_MIN, 0, 0 };
	double const scales[5] = { 0.0, -1.0, 0.0009, 1001.0, nan("") };
	bool allRefused = true;

	for (double const scale : scales)
	{
		pose = before;
		allRefused = allRefused && !host::SurfacePlace_Live(Live(hit, up, above, scale, 500, kUpright, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0;
	}

	Check(allRefused, "a scale of 0, below 0, past its bounds (1/1000 .. 1000) or not a number: refused, the pose untouched");
	pose = before;
	Check(!host::SurfacePlace_Live(Live(hit, broken, above, 1.0, 500, kUpright, true), &pose) && memcmp(&pose, &before, sizeof(pose)) == 0, "a normal with a part of -2^31 (no way round): refused");
}

int main()
{
	TestTheTurnsMeaning();
	TestFloor();
	TestOnItsSide();
	TestWall();
	TestCeiling();
	TestSlope();
	TestMoveOnly();
	TestRefused();
	TestLive();
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
