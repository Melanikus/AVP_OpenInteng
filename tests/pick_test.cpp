// The test for src/host's click-picking: the line from the camera through a click (pick_ray), and the editor's one selection with the
// status line's words (selection). Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It needs no window and none of the game. The line is checked against the game's own drawing, done forwards here with the game's own
// sums: MUL_FIXED (mathline.cpp:322) and RotateVector (maths.cpp:730-751), copied below, then the card's projection as the game sets it
// (d3d_render.cpp:1915 and :1578). A line that goes back through the clicked pixel when it's drawn forwards is the right line. Each
// check's words say what it proves. The program gives back a non-zero result if any check failed. tests/takeouts/pick_test.json proves
// each check can fail.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits>
#include <string>

#include "pick_ray.h"
#include "selection.h"

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

// ---- The game's own forward sums, copied ----

// MUL_FIXED, mathline.cpp:322: the 64-bit product moved down 16 places, its low 32 bits kept
static int32_t GameMulFixed(int32_t a, int32_t b)
{
	return (int32_t)((((int64_t)a * b) >> 16) & 0xFFFFFFFF);
}

// RotateVector, maths.cpp:730-751: each of the view's numbers is one column of the matrix times the vector. m[row][column] is the
// game's mat<row+1><column+1>.
static void GameRotate(const int32_t m[3][3], const int32_t v[3], int32_t out[3])
{
	out[0] = GameMulFixed(m[0][0], v[0]) + GameMulFixed(m[1][0], v[1]) + GameMulFixed(m[2][0], v[2]);
	out[1] = GameMulFixed(m[0][1], v[0]) + GameMulFixed(m[1][1], v[1]) + GameMulFixed(m[2][1], v[2]);
	out[2] = GameMulFixed(m[0][2], v[0]) + GameMulFixed(m[1][2], v[1]) + GameMulFixed(m[2][2], v[2]);
}

// The card's place for something in the game's view: the game hands it y the other way round (d3d_render.cpp:1915), the projection
// multiplies across by xScale and down by yScale and divides by the distance ahead (d3d_render.cpp:1578), and Direct3D 9 spreads -1 to 1
// over the view's pixels with a pixel's middle on its whole number. False for something behind the camera.
static bool ToPixel(const PickCamera &camera, const int32_t view[3], double *x, double *y)
{
	if (view[2] <= 0)
	{
		return false;
	}

	double const across = (double)camera.xScale * (double)view[0] / (double)view[2];
	double const up = (double)camera.yScale * -(double)view[1] / (double)view[2];
	*x = (across + 1.0) * (double)camera.viewWidth / 2.0;
	*y = (1.0 - up) * (double)camera.viewHeight / 2.0;
	return true;
}

// ---- Cameras to test with ----

// A turn the way a camera turns: round the up axis (turn), then up or down (look), then a lean (roll), each in degrees, as a matrix in
// 65536ths whose columns are the view's across, down and ahead in the world
static void MakeTurn(double turnDegrees, double lookDegrees, double rollDegrees, int32_t out[3][3])
{
	double const pi = 3.14159265358979323846;
	double const t = turnDegrees * pi / 180.0, l = lookDegrees * pi / 180.0, r = rollDegrees * pi / 180.0;

	// Round y, then round x, then round z: three plain turns multiplied together
	double const yTurn[3][3] = { { cos(t), 0, sin(t) }, { 0, 1, 0 }, { -sin(t), 0, cos(t) } };
	double const xTurn[3][3] = { { 1, 0, 0 }, { 0, cos(l), -sin(l) }, { 0, sin(l), cos(l) } };
	double const zTurn[3][3] = { { cos(r), -sin(r), 0 }, { sin(r), cos(r), 0 }, { 0, 0, 1 } };
	double both[3][3];
	double all[3][3];

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			both[i][j] = yTurn[i][0] * xTurn[0][j] + yTurn[i][1] * xTurn[1][j] + yTurn[i][2] * xTurn[2][j];
		}
	}

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			all[i][j] = both[i][0] * zTurn[0][j] + both[i][1] * zTurn[1][j] + both[i][2] * zTurn[2][j];
			out[i][j] = (int32_t)floor(all[i][j] * 65536.0 + 0.5);
		}
	}
}

static PickCamera MakeCamera(double turn, double look, double roll, float yScale, int32_t width, int32_t height)
{
	PickCamera camera;
	camera.place[0] = 12000;
	camera.place[1] = -3000;
	camera.place[2] = 45000;
	MakeTurn(turn, look, roll, camera.turn);
	camera.yScale = yScale;
	camera.xScale = yScale / ((float)width / (float)height);   // as UpdateProjectionMatrix makes it: yScale over the view's shape
	camera.viewWidth = width;
	camera.viewHeight = height;
	return camera;
}

// A float made from its bits, for the numbers no sum gives on purpose
static float FromBits(uint32_t bits)
{
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

// A seeded run of numbers, the same every time, so a failure can be found again (xorshift)
static uint32_t seed = 0x2545F491u;

static uint32_t Next()
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

static int32_t Between(int32_t least, int32_t most)
{
	return least + (int32_t)(Next() % (uint32_t)(most - least + 1));
}

static double Length(const int32_t v[3])
{
	return sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
}

// ---- The line ----

static void TestPlainSense()
{
	puts("The line, in plain sense:");

	PickCamera const camera = MakeCamera(0, 0, 0, 1.0f, 800, 600);
	PickRay ray;

	Check(PickRay_FromClick(camera, 400, 300, &ray) == PickRayMade::Made && ray.direction[0] == 0 && ray.direction[1] == 0
		&& ray.direction[2] == kPickOne, "a camera turned nowhere, clicked in the middle, looks straight ahead: (0, 0, 65536)");
	Check(ray.from[0] == 12000 && ray.from[1] == -3000 && ray.from[2] == 45000, "and the line starts at the camera's place");

	Check(PickRay_FromClick(camera, 400, 0, &ray) == PickRayMade::Made && ray.direction[1] < 0 && ray.direction[2] > 0,
		"a click at the top looks up, which is the game's y going down, below 0");
	Check(PickRay_FromClick(camera, 799, 300, &ray) == PickRayMade::Made && ray.direction[0] > 0, "and one at the right looks right");

	// The top edge's pixel is at -1 up, so up / yScale is 1: the line goes 45 degrees up when yScale is 1
	Check(PickRay_FromClick(camera, 400, 0, &ray) == PickRayMade::Made && abs(ray.direction[1] + ray.direction[2]) <= 2,
		"with a yScale of 1, the top edge is 45 degrees up, as the projection says");

	PickCamera behind = MakeCamera(180, 0, 0, 1.0f, 800, 600);
	Check(PickRay_FromClick(behind, 400, 300, &ray) == PickRayMade::Made && ray.direction[2] < -65530, "a camera turned round looks back along z");
}

static void TestRoundTrip()
{
	puts("The line, drawn forwards with the game's own sums, goes back through the click:");

	struct Turn { double turn, look, roll; };
	Turn const turns[] = { { 0, 0, 0 }, { 37, 0, 0 }, { 180, 0, 0 }, { -95, 30, 0 }, { 10, -80, 0 }, { 250, 89, 5 }, { 1, 2, 3 } };
	struct Size { int32_t width, height; float yScale; };
	Size const sizes[] = { { 800, 600, 1.0f }, { 1920, 1080, 1.7320508f }, { 1, 1, 1.0f }, { 640, 2000, 0.6f }, { 16384, 16384, 1.0f }, { 333, 77, 2.4142135f } };

	double worstBySize[6] = { 0 };
	int lines = 0;
	bool allMade = true;

	for (const Turn &turn : turns)
	{
		for (size_t which = 0; which < 6; which++)
		{
			Size const &size = sizes[which];
			PickCamera const camera = MakeCamera(turn.turn, turn.look, turn.roll, size.yScale, size.width, size.height);
			int32_t const clicks[][2] = { { 0, 0 }, { size.width - 1, 0 }, { 0, size.height - 1 }, { size.width - 1, size.height - 1 },
				{ size.width / 2, size.height / 2 }, { size.width / 3, (size.height * 2) / 3 } };

			for (const auto &click : clicks)
			{
				PickRay ray;

				if (PickRay_FromClick(camera, click[0], click[1], &ray) != PickRayMade::Made)
				{
					allMade = false;
					continue;
				}

				int32_t view[3];
				double x = 0, y = 0;
				GameRotate(camera.turn, ray.direction, view);

				if (!ToPixel(camera, view, &x, &y))
				{
					allMade = false;
					continue;
				}

				double const off = fabs(x - click[0]) > fabs(y - click[1]) ? fabs(x - click[0]) : fabs(y - click[1]);
				worstBySize[which] = off > worstBySize[which] ? off : worstBySize[which];
				lines++;
			}
		}
	}

	printf("  (the worst by view: 800x600 %.4f, 1920x1080 %.4f, 1x1 %.4f, 640x2000 %.4f, 16384x16384 %.4f, 333x77 %.4f)\n", worstBySize[0],
		worstBySize[1], worstBySize[2], worstBySize[3], worstBySize[4], worstBySize[5]);

	// Every view but the largest (sizes[4]) is a real window's size, or smaller
	double worstReal = 0.0;

	for (size_t which = 0; which < 6; which++)
	{
		if (which != 4 && worstBySize[which] > worstReal)
		{
			worstReal = worstBySize[which];
		}
	}

	char words[320];
	_snprintf(words, sizeof(words) - 1, "at the corners, the middle and a third of the way, for 7 turns and the 5 views up to 2000 pixels, each line lands"
		" within a fifth of a pixel of its click (the worst, %.4f)", worstReal);
	words[sizeof(words) - 1] = '\0';
	Check(allMade && lines == 7 * 6 * 6 && worstReal < 0.2, words);

	_snprintf(words, sizeof(words) - 1, "and on the largest view, 16384 pixels across, within 2 pixels: a pixel there is so small that a direction 65536 long,"
		" turned by the game's whole-number sums, which drop what's under a 65536th, moves by about one (the worst, %.4f)", worstBySize[4]);
	words[sizeof(words) - 1] = '\0';
	Check(worstBySize[4] < 2.0, words);

	// A place in the world, drawn to its pixel; a click on that pixel makes a line that, drawn forwards, lands within a pixel of the place
	double worstPlace = 0.0;
	int places = 0;
	bool placesGood = true;

	for (const Turn &turn : turns)
	{
		PickCamera const camera = MakeCamera(turn.turn, turn.look, turn.roll, 1.7320508f, 1920, 1080);

		for (int i = 0; i < 1000; i++)
		{
			int32_t const relative[3] = { Between(-60000, 60000), Between(-60000, 60000), Between(-60000, 60000) };
			int32_t view[3];
			double x = 0, y = 0;
			GameRotate(camera.turn, relative, view);

			if (!ToPixel(camera, view, &x, &y) || x < 0 || y < 0 || x > camera.viewWidth - 1 || y > camera.viewHeight - 1)
			{
				continue;
			}

			int32_t const clickX = (int32_t)floor(x + 0.5);
			int32_t const clickY = (int32_t)floor(y + 0.5);
			PickRay ray;
			double lineX = 0, lineY = 0;
			int32_t lineView[3];

			if (PickRay_FromClick(camera, clickX, clickY, &ray) != PickRayMade::Made)
			{
				placesGood = false;
				continue;
			}

			GameRotate(camera.turn, ray.direction, lineView);
			placesGood = placesGood && ToPixel(camera, lineView, &lineX, &lineY);
			double const off = fabs(lineX - x) > fabs(lineY - y) ? fabs(lineX - x) : fabs(lineY - y);
			worstPlace = off > worstPlace ? off : worstPlace;
			places++;
		}
	}

	_snprintf(words, sizeof(words) - 1, "and %d places in the world, clicked where the game draws them, each get a line within a pixel of them (the worst,"
		" %.4f)", places, worstPlace);
	words[sizeof(words) - 1] = '\0';
	Check(placesGood && places >= 200 && worstPlace <= 1.0, words);
}

static void TestRefusals()
{
	puts("The line refuses what it can't use, and writes nothing then:");

	PickCamera const good = MakeCamera(20, 10, 0, 1.0f, 800, 600);
	PickRay const untouched = { { 7, 7, 7 }, { 7, 7, 7 } };

	auto refused = [&](const PickCamera &camera, int32_t x, int32_t y, PickRayMade why) {
		PickRay ray = untouched;
		return PickRay_FromClick(camera, x, y, &ray) == why && memcmp(&ray, &untouched, sizeof(ray)) == 0;
	};

	bool sizes = true;
	int32_t const badSides[] = { 0, -1, kPickMostSide + 1, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };

	for (int32_t side : badSides)
	{
		PickCamera wide = good;
		wide.viewWidth = side;
		PickCamera tall = good;
		tall.viewHeight = side;
		sizes = sizes && refused(wide, 0, 0, PickRayMade::ViewSizeRefused) && refused(tall, 0, 0, PickRayMade::ViewSizeRefused);
	}

	Check(sizes, "a side of the view of 0, -1, 16385 or the largest and smallest numbers there are");

	PickCamera smallest = good;
	smallest.viewWidth = 1;
	smallest.viewHeight = 1;
	PickCamera largest = good;
	largest.viewWidth = kPickMostSide;
	largest.viewHeight = kPickMostSide;
	PickRay ray;
	Check(PickRay_FromClick(smallest, 0, 0, &ray) == PickRayMade::Made && PickRay_FromClick(largest, kPickMostSide - 1, kPickMostSide - 1, &ray) == PickRayMade::Made,
		"but a view of 1 by 1, and one of 16384 by 16384 clicked at its last pixel, each make a line");

	bool clicks = true;
	int32_t const badX[] = { -1, 800, 801, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };
	int32_t const badY[] = { -1, 600, 601, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };

	for (int32_t x : badX)
	{
		clicks = clicks && refused(good, x, 0, PickRayMade::ClickOutsideTheView);
	}

	for (int32_t y : badY)
	{
		clicks = clicks && refused(good, 0, y, PickRayMade::ClickOutsideTheView);
	}

	Check(clicks, "a click left of, right of, above or below the view, even at the largest and smallest numbers");

	uint32_t const badScaleBits[] = { 0x00000000u, 0x80000000u, 0xBF800000u, 0x7FC00000u, 0xFFC00000u, 0x7F800000u, 0xFF800000u, 0x00000001u, 0x007FFFFFu };
	bool scales = true;

	for (uint32_t bits : badScaleBits)
	{
		PickCamera across = good;
		across.xScale = FromBits(bits);
		PickCamera down = good;
		down.yScale = FromBits(bits);
		scales = scales && refused(across, 1, 1, PickRayMade::ScaleRefused) && refused(down, 1, 1, PickRayMade::ScaleRefused);
	}

	Check(scales, "a projection number of 0, -0, -1, not a number, endless either way, or one of the numbers too small to divide by");

	PickCamera tiny = good;
	tiny.xScale = FromBits(0x00800000u);   // the smallest ordinary float, about 1.2e-38
	PickCamera huge = good;
	huge.yScale = FromBits(0x7F7FFFFFu);   // the largest, about 3.4e38
	Check(PickRay_FromClick(tiny, 0, 0, &ray) == PickRayMade::Made && PickRay_FromClick(huge, 799, 599, &ray) == PickRayMade::Made
		&& Length(ray.direction) > 65530 && Length(ray.direction) < 65542, "but the smallest and largest ordinary ones still make a line 65536 long");

	bool turns = true;
	int32_t const badTurn[] = { kPickOne + 1, -kPickOne - 1, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };

	for (int row = 0; row < 3; row++)
	{
		for (int column = 0; column < 3; column++)
		{
			for (int32_t value : badTurn)
			{
				PickCamera bent = good;
				bent.turn[row][column] = value;
				turns = turns && refused(bent, 5, 5, PickRayMade::TurnRefused);
			}
		}
	}

	Check(turns, "a matrix number past 65536 either way, at each of its nine places");

	PickCamera edge = good;
	edge.turn[0][0] = kPickOne;
	edge.turn[2][2] = -kPickOne;
	Check(PickRay_FromClick(edge, 5, 5, &ray) == PickRayMade::Made, "but 65536 and -65536 themselves are a matrix's numbers");

	bool places = true;
	int32_t const badPlace[] = { kPickMostPlace + 1, -kPickMostPlace - 1, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };

	for (int axis = 0; axis < 3; axis++)
	{
		for (int32_t value : badPlace)
		{
			PickCamera distant = good;
			distant.place[axis] = value;
			places = places && refused(distant, 5, 5, PickRayMade::PlaceRefused);
		}
	}

	PickCamera atEdge = good;
	atEdge.place[0] = kPickMostPlace;
	atEdge.place[1] = -kPickMostPlace;
	Check(places && PickRay_FromClick(atEdge, 5, 5, &ray) == PickRayMade::Made && ray.from[0] == kPickMostPlace,
		"a camera place past 16,777,216 either way on any axis, though at it is still a place");

	PickCamera zero = good;
	memset(zero.turn, 0, sizeof(zero.turn));
	PickCamera flat = good;
	memset(flat.turn, 0, sizeof(flat.turn));
	flat.turn[0][0] = kPickOne;   // every view direction goes to across only, and the middle click has none across
	Check(refused(zero, 400, 300, PickRayMade::NoDirection) && refused(flat, 400, 300, PickRayMade::NoDirection),
		"a matrix of noughts, or one that turns this click's direction into nothing, gives no line");
}

static void TestHostileSweep()
{
	puts("Hostile cameras and clicks:");

	int made = 0;
	int refusedCount = 0;
	bool allGood = true;

	for (int i = 0; i < 20000; i++)
	{
		PickCamera camera;

		for (int axis = 0; axis < 3; axis++)
		{
			camera.place[axis] = (i % 7 == 0) ? (int32_t)Next() : Between(-(kPickMostPlace + 100), kPickMostPlace + 100);
		}

		for (int row = 0; row < 3; row++)
		{
			for (int column = 0; column < 3; column++)
			{
				camera.turn[row][column] = (i % 11 == 0) ? (int32_t)Next() : Between(-kPickOne - 2, kPickOne + 2);
			}
		}

		camera.xScale = (i % 3 == 0) ? FromBits(Next()) : (float)Between(1, 4000) / 1000.0f;
		camera.yScale = (i % 5 == 0) ? FromBits(Next()) : (float)Between(1, 4000) / 1000.0f;
		camera.viewWidth = (i % 13 == 0) ? (int32_t)Next() : Between(-3, kPickMostSide + 3);
		camera.viewHeight = (i % 17 == 0) ? (int32_t)Next() : Between(-3, kPickMostSide + 3);
		// Clicks a little either side of the view, worked out only from a side that is a view's, so nothing here overflows
		int32_t const acrossRoom = (camera.viewWidth > 0 && camera.viewWidth <= kPickMostSide) ? camera.viewWidth + 3 : 3;
		int32_t const downRoom = (camera.viewHeight > 0 && camera.viewHeight <= kPickMostSide) ? camera.viewHeight + 3 : 3;
		int32_t const x = (i % 19 == 0) ? (int32_t)Next() : Between(-3, acrossRoom);
		int32_t const y = (i % 23 == 0) ? (int32_t)Next() : Between(-3, downRoom);

		PickRay ray = { { 7, 7, 7 }, { 7, 7, 7 } };
		PickRayMade const answer = PickRay_FromClick(camera, x, y, &ray);

		if (answer == PickRayMade::Made)
		{
			made++;
			double const length = Length(ray.direction);
			bool inRange = true;

			for (int axis = 0; axis < 3; axis++)
			{
				inRange = inRange && ray.direction[axis] >= -kPickOne && ray.direction[axis] <= kPickOne && ray.from[axis] == camera.place[axis];
			}

			allGood = allGood && inRange && length > 65533.0 && length < 65539.0;
		}
		else
		{
			refusedCount++;
			allGood = allGood && ray.from[0] == 7 && ray.direction[2] == 7;
		}
	}

	char words[220];
	_snprintf(words, sizeof(words) - 1, "20,000 seeded cameras, some with any bits at all for their numbers: %d lines, each 65536 long from the camera's place,"
		" and %d refused with nothing written", made, refusedCount);
	words[sizeof(words) - 1] = '\0';
	Check(allGood && made > 1000 && refusedCount > 1000, words);
}

// ---- The selection ----

static void TestSelection()
{
	puts("The selection:");

	Selected const now = Selection_Get();
	Check(now.kind == SelectedKind::Nothing && Selection_Changes() == 0, "the editor starts with nothing selected, and no change counted");

	Selected module = { SelectedKind::Module, 3, { 9, 9, 9, 9, 9, 9, 9, 9 } };
	Check(Selection_Set(module) && Selection_Get().kind == SelectedKind::Module && Selection_Get().moduleIndex == 3 && Selection_Changes() == 1,
		"selecting module 3 selects it, and counts one change");

	Selected const kept = Selection_Get();
	Check(kept.objectId[0] == 0 && kept.objectId[7] == 0, "and keeps no ID for a module, whatever the ID field held");

	Check(Selection_Set(module) && Selection_Changes() == 1, "selecting it again changes nothing, and counts nothing");

	module.moduleIndex = 4;
	Check(Selection_Set(module) && Selection_Get().moduleIndex == 4 && Selection_Changes() == 2, "another module is a change");

	Selected badModule = { SelectedKind::Module, -1, { 0 } };
	Selected badModule2 = { SelectedKind::Module, std::numeric_limits<int32_t>::min(), { 0 } };
	Check(!Selection_Set(badModule) && !Selection_Set(badModule2) && Selection_Get().moduleIndex == 4 && Selection_Changes() == 2,
		"a module numbered below 0 is refused, and the selection stays as it was");

	Selected nullObject = { SelectedKind::Object, 0, { 0 } };
	Check(!Selection_Set(nullObject) && Selection_Get().kind == SelectedKind::Module, "an object whose ID is all 0, the game's \"no name\", is refused");

	Selected object = { SelectedKind::Object, 77, { 1, 2, 3, 4, 5, 6, 7, 8 } };
	Check(Selection_Set(object) && Selection_Get().kind == SelectedKind::Object && memcmp(Selection_Get().objectId, object.objectId, 8) == 0
		&& Selection_Changes() == 3, "an object is selected by its 8-byte ID");
	Check(Selection_Get().moduleIndex == 0, "and keeps no module number, whatever that field held");

	Check(Selection_Set(object) && Selection_Changes() == 3, "the same ID again changes nothing");

	object.objectId[7] = 9;
	Check(Selection_Set(object) && Selection_Changes() == 4, "and an ID that differs only in its last byte is another object");

	Selected oddKind = object;
	oddKind.kind = (SelectedKind)7;
	Check(!Selection_Set(oddKind) && Selection_Get().objectId[7] == 9, "a kind that isn't one of the three is refused");

	Selection_Clear();
	Check(Selection_Get().kind == SelectedKind::Nothing && Selection_Changes() == 5, "clearing selects nothing, and counts a change");
	Selection_Clear();
	Check(Selection_Changes() == 5, "and clearing again counts nothing");
}

// ---- What the status line says ----

static std::string Words(const PickReport &report, size_t size = kSelectionMostWords)
{
	// Filled with X and ended at its last place, so words that were never ended show as X letters, and nothing reads past the end
	char words[kSelectionMostWords];
	memset(words, 'X', sizeof(words));
	words[sizeof(words) - 1] = '\0';
	Selection_Words(report, words, size);
	return std::string(words);
}

static void TestWords()
{
	puts("What the status line says:");

	PickReport module = { PickFound::Module, 12, { 'A', 'B', 1, 0xFF, 'z' }, { 0 }, 3456 };
	Check(Words(module) == "Picked: module 12 (AB??z), 3.4 m away", "a module: its number, its name with its odd bytes as ?, and how far away");

	PickReport shortName = { PickFound::Module, 0, { 'A', 0, 'B', 'C', 'D' }, { 0 }, 0 };
	PickReport noName = { PickFound::Module, 5, { 0 }, { 0 }, 999 };
	Check(Words(shortName) == "Picked: module 0 (A), 0.0 m away" && Words(noName) == "Picked: module 5 (no name), 0.9 m away",
		"a name stops at its first 0, and one with no letters says so");

	PickReport level = { PickFound::Module, 16, { 'g', 'A', 'R', 'A', 'G', 'D', 'O', 'R' }, { 0 }, 22600 };
	Check(Words(level) == "Picked: module 16 (gARAGDOR), 22.6 m away", "a module's name as the level gives it, longer than the game's 5 bytes (the owner's \"(?)\", 11-17)");

	PickReport longest = { PickFound::Module, 2147483647, { 0 }, { 0 }, 10000000 };
	memset(longest.moduleName, 'L', sizeof(longest.moduleName));
	Check(Words(longest) == "Picked: module 2147483647 (" + std::string(kSelectionModuleNameBytes, 'L') + "), 10000.0 m away",
		"a name filling all its bytes with no 0 ends there, and the longest words still fit");

	PickReport object = { PickFound::Object, 0, { 0 }, { 0x01, 0xAB, 0x00, 0xFF, 0x10, 0x20, 0x7F, 0x80 }, 10000000 };
	Check(Words(object) == "Picked: object 01AB00FF10207F80, 10000.0 m away", "an object: its ID as 16 hex digits");

	PickReport farOff = object;
	farOff.distance = std::numeric_limits<int32_t>::max();
	PickReport behind = object;
	behind.distance = std::numeric_limits<int32_t>::min();
	Check(Words(farOff) == "Picked: object 01AB00FF10207F80, 10000.0 m away" && Words(behind) == "Picked: object 01AB00FF10207F80, 0.0 m away",
		"a distance is held to 0 to 10,000 metres, the most the game's line of sight looks");

	PickReport other = { PickFound::Nothing, 0, { 0 }, { 0 }, 0 };
	bool othersSay = Words(other) == "Nothing there to pick";
	other.found = PickFound::NotInTheFile;
	othersSay = othersSay && Words(other).find("doesn't name it") != std::string::npos;
	other.found = PickFound::NumberRefused;
	othersSay = othersSay && Words(other).find("isn't in the level's list") != std::string::npos;
	other.found = PickFound::NoLine;
	othersSay = othersSay && Words(other).find("no line could be made") != std::string::npos;
	other.found = (PickFound)99;
	othersSay = othersSay && Words(other).find("no line could be made") != std::string::npos;
	Check(othersSay, "a miss, something the game made, a refused number, no line, and a kind of answer that isn't one, each say so");

	Check(Words(module, 10) == "Picked: m" && Words(module, 1).empty(), "words that don't fit are cut, and always end");

	char untouched[4] = { 'Q', 'Q', 'Q', 0 };
	Selection_Words(module, untouched, 0);
	Selection_Words(module, nullptr, 50);
	Check(strcmp(untouched, "QQQ") == 0, "and with no room at all, nothing is written");
}

static void TestUsePick()
{
	puts("A pick, used:");

	Selection_Clear();
	uint32_t const start = Selection_Changes();

	PickReport module = { PickFound::Module, 8, { 'R', 'M', '0', '8', 0 }, { 0 }, 1500 };
	const char *said = Selection_UsePick(module);
	Check(Selection_Get().kind == SelectedKind::Module && Selection_Get().moduleIndex == 8 && strcmp(said, "Picked: module 8 (RM08), 1.5 m away") == 0
		&& Selection_Changes() == start + 1, "a module hit selects it, and says so");

	PickReport miss = { PickFound::Nothing, 0, { 0 }, { 0 }, 0 };
	said = Selection_UsePick(miss);
	Check(Selection_Get().moduleIndex == 8 && strcmp(said, "Nothing there to pick") == 0, "a miss leaves the selection as it was");

	PickReport made = { PickFound::NotInTheFile, 0, { 0 }, { 0 }, 0 };
	said = Selection_UsePick(made);
	Check(Selection_Get().moduleIndex == 8 && strstr(said, "doesn't name it") != 0, "and so does something the game made");

	PickReport object = { PickFound::Object, 0, { 0 }, { 9, 8, 7, 6, 5, 4, 3, 2 }, 2000 };
	said = Selection_UsePick(object);
	Check(Selection_Get().kind == SelectedKind::Object && Selection_Get().objectId[0] == 9 && strncmp(said, "Picked: object 0908070605040302", 31) == 0,
		"an object hit selects it by its ID");

	PickReport nullObject = { PickFound::Object, 0, { 0 }, { 0 }, 2000 };
	said = Selection_UsePick(nullObject);
	Check(Selection_Get().objectId[0] == 9 && strstr(said, "Not picked") != 0 && strstr(said, "Picked:") == 0,
		"an object hit whose ID is all 0 selects nothing, and the words never say Picked");

	PickReport badModule = { PickFound::Module, -5, { 'X', 0 }, { 0 }, 2000 };
	said = Selection_UsePick(badModule);
	Check(Selection_Get().kind == SelectedKind::Object && strstr(said, "Not picked") != 0, "and the same for a module numbered below 0");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestPlainSense();
	TestRoundTrip();
	TestRefusals();
	TestHostileSweep();
	TestSelection();
	TestWords();
	TestUsePick();

	printf("%s: %d check(s) failed\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed", checksFailed);
	return checksFailed ? 1 : 0;
}
