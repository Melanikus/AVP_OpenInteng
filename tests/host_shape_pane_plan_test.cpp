// The test for the live panes' frame worked out before anything is drawn (src/host/shape_pane_plan.cpp). Written 2026-11-23 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//   (1) THE PLAN, shelf by shelf: three 96 x 96 panes in a 200 x 200 atlas at (0, 0), (96, 0), then a new shelf at (0, 96)
//   (2) THE FRAME'S TRIANGLES: of 150,000, 60,000 and 40,000 with 200,000 the most, the second isn't drawn (saying so) and takes no room, the third is
//   (3) NOT DRAWN, saying why: a box of no size, one wider than the atlas, and the fifth 96 x 96 pane when the atlas holds four
//   (4) THE TURNED CAMERA: the store's still eye turned by ShapePane_TurnEye is where ShapeThumb_CameraTurned puts it, at several turns
//   (5) THE PANE'S MATRIX: the middle lands in the middle of the glass, in front of the camera; a square pane's matrix is the still pictures' view times
//       their lens; a pane twice as wide as high sees twice as far across (fieldOfView across its shorter side)
//   (6) NO MATRIX: the eye at the middle, a box of no size
// RED FIRST: on the stand-in that plans nothing and makes no matrix, (1), (2), (3), (4) and (5) fail.
#include <math.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "shape_pane_plan.h"
#include "shape_thumb.h"
#include "shape_thumb_render.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static host::ShapePaneAsk Ask(int handle, int32_t width, int32_t height, uint32_t triangles)
{
	host::ShapePaneAsk a;
	a.handle = handle;
	a.box[0] = 10;
	a.box[1] = 20;
	a.box[2] = width;
	a.box[3] = height;
	a.triangles = triangles;
	return a;
}

static std::string Region(host::ShapePanePlace const &p)
{
	return p.drawn ? "at " + std::to_string(p.region[0]) + ", " + std::to_string(p.region[1]) + " (" + std::to_string(p.region[2]) + " x " + std::to_string(p.region[3]) +
		")" : "not drawn: " + p.why;
}

static bool At(host::ShapePanePlace const &p, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	return p.drawn && p.region[0] == x && p.region[1] == y && p.region[2] == w && p.region[3] == h && p.why.empty();
}

// a place times the matrix, divided by its w: x and y on the glass (-1 to 1), z its depth (0 to 1)
static void OnGlass(float const m[4][4], float const p[3], float out[3], float *w)
{
	float r[4];

	for (int c = 0; c < 4; c++)
	{
		r[c] = p[0] * m[0][c] + p[1] * m[1][c] + p[2] * m[2][c] + m[3][c];
	}

	*w = r[3];
	out[0] = r[0] / r[3];
	out[1] = r[1] / r[3];
	out[2] = r[2] / r[3];
}

int main()
{
	printf("host_shape_pane_plan_test: the live panes' frame, worked out before anything is drawn\n");

	// ---- (1) shelf by shelf
	std::vector<host::ShapePaneAsk> three = { Ask(1, 96, 96, 100), Ask(2, 96, 96, 100), Ask(3, 96, 96, 100) };
	std::vector<host::ShapePanePlace> const shelves = host::ShapePane_Plan(three, 200, host::kShapePanesTrianglesAFrame);

	Check(shelves.size() == 3 && At(shelves[0], 0, 0, 96, 96) && At(shelves[1], 96, 0, 96, 96) && At(shelves[2], 0, 96, 96, 96),
		"(1) three 96 x 96 panes in a 200 x 200 atlas: " + (shelves.size() == 3 ? Region(shelves[0]) + "; " + Region(shelves[1]) + "; " + Region(shelves[2]) : std::string("no places")));

	// ---- (2) the frame's triangles
	std::vector<host::ShapePaneAsk> heavy = { Ask(1, 96, 96, 150000), Ask(2, 96, 96, 60000), Ask(3, 96, 96, 40000) };
	std::vector<host::ShapePanePlace> const budget = host::ShapePane_Plan(heavy, 200, 200000);

	Check(budget.size() == 3 && At(budget[0], 0, 0, 96, 96) && !budget[1].drawn && budget[1].why.find("200,000") != std::string::npos && At(budget[2], 96, 0, 96, 96),
		"(2) 150,000 + 60,000 would pass 200,000: the second isn't drawn and takes no room, the third (40,000) is: " +
		(budget.size() == 3 ? Region(budget[0]) + "; " + Region(budget[1]) + "; " + Region(budget[2]) : std::string("no places")));

	// ---- (3) not drawn, saying why
	std::vector<host::ShapePaneAsk> odd = { Ask(1, 0, 96, 10), Ask(2, 96, -5, 10), Ask(3, 201, 96, 10) };
	std::vector<host::ShapePanePlace> const refused = host::ShapePane_Plan(odd, 200, 200000);

	Check(refused.size() == 3 && !refused[0].drawn && !refused[0].why.empty() && !refused[1].drawn && !refused[1].why.empty() && !refused[2].drawn &&
		refused[2].why.find("atlas") != std::string::npos, "(3) no width, a height below nothing, and wider than the atlas: none drawn, each saying why (" +
		(refused.size() == 3 ? refused[0].why + "; " + refused[1].why + "; " + refused[2].why : std::string("no places")) + ")");

	std::vector<host::ShapePaneAsk> five(5, Ask(1, 96, 96, 10));
	std::vector<host::ShapePanePlace> const full = host::ShapePane_Plan(five, 200, 200000);

	Check(full.size() == 5 && full[3].drawn && At(full[3], 96, 96, 96, 96) && !full[4].drawn && full[4].why.find("room") != std::string::npos,
		"(3) a 200 x 200 atlas holds four 96 x 96 panes: the fifth isn't drawn, there's no room left (" + (full.size() == 5 ? Region(full[4]) : std::string("no places")) + ")");

	// ---- (4) the turned camera, as ShapeThumb_CameraTurned turns the still one
	host::ShapeThumbModel model;
	model.centre[0] = 100.0f;
	model.centre[1] = -50.0f;
	model.centre[2] = 300.0f;
	model.radius = 400.0f;

	float eye[3], at[3];
	host::ShapeThumb_Camera(model, 0.7f, eye, at);
	bool same = true;
	std::string turns;

	for (float turn : { 0.0f, 0.5f, 2.0f, -1.0f, 6.0f })
	{
		float theirs[3], theirAt[3], mine[3];

		host::ShapeThumb_CameraTurned(model, 0.7f, turn, theirs, theirAt);
		host::ShapePane_TurnEye(eye, at, turn, mine);

		bool const here = fabsf(mine[0] - theirs[0]) < 0.01f && fabsf(mine[1] - theirs[1]) < 0.01f && fabsf(mine[2] - theirs[2]) < 0.01f;
		same = same && here;
		turns += (turns.empty() ? "" : "; ") + std::to_string(turn) + (here ? " same" : " differs");
	}

	Check(same, "(4) the still eye turned round the middle is where ShapeThumb_CameraTurned puts it (" + turns + ")");

	// ---- (5) the pane's matrix
	float square[4][4], wide[4][4];
	bool const squareMade = host::ShapePane_Matrix(eye, at, 0.7f, 96, 96, square);
	bool const wideMade = host::ShapePane_Matrix(eye, at, 0.7f, 192, 96, wide);
	float middle[3], w = 0.0f;

	if (squareMade)
	{
		OnGlass(square, at, middle, &w);
	}

	Check(squareMade && fabsf(middle[0]) < 1e-4f && fabsf(middle[1]) < 1e-4f && middle[2] > 0.0f && middle[2] < 1.0f && w > 0.0f,
		"(5) the middle lands in the middle of the glass, in front of the camera");

	// the still pictures' own view and lens (shape_thumb_render.cpp: near at 0.3 of the distance, at least 1; far twice the distance and 1)
	float view[4][4], lens[4][4], still[4][4];
	float const distance = sqrtf((at[0] - eye[0]) * (at[0] - eye[0]) + (at[1] - eye[1]) * (at[1] - eye[1]) + (at[2] - eye[2]) * (at[2] - eye[2]));
	bool const stillMade = host::ShapeThumb_ViewMatrix(eye, at, view) &&
		host::ShapeThumb_LensMatrix(0.7f, distance * 0.3f > 1.0f ? distance * 0.3f : 1.0f, distance * 2.0f + 1.0f, lens);
	bool sameAsStill = stillMade && squareMade;

	for (int r = 0; r < 4 && stillMade; r++)
	{
		for (int c = 0; c < 4; c++)
		{
			still[r][c] = view[r][0] * lens[0][c] + view[r][1] * lens[1][c] + view[r][2] * lens[2][c] + view[r][3] * lens[3][c];
			sameAsStill = sameAsStill && fabsf(still[r][c] - square[r][c]) <= 1e-4f * (fabsf(still[r][c]) + 1.0f);
		}
	}

	Check(sameAsStill, "(5) a square pane's matrix is the still pictures' view times their lens");

	// a place a little right of the middle, as the camera sees it: on the wide pane it lands half as far across
	float right[3] = { at[0] + 50.0f * view[0][0], at[1] + 50.0f * view[1][0], at[2] + 50.0f * view[2][0] };
	float onSquare[3], onWide[3], ws = 0.0f, ww = 0.0f;

	if (squareMade && wideMade)
	{
		OnGlass(square, right, onSquare, &ws);
		OnGlass(wide, right, onWide, &ww);
	}

	Check(squareMade && wideMade && onSquare[0] > 0.0f && fabsf(onWide[0] * 2.0f - onSquare[0]) < 1e-4f && fabsf(onWide[1] - onSquare[1]) < 1e-4f,
		"(5) a pane twice as wide as high sees twice as far across: the same place lands half as far from the middle (" +
		(squareMade && wideMade ? std::to_string(onSquare[0]) + " on the square one, " + std::to_string(onWide[0]) + " on the wide one" : std::string("not made")) + ")");

	// ---- (6) no matrix
	float none[4][4];

	Check(!host::ShapePane_Matrix(at, at, 0.7f, 96, 96, none) && !host::ShapePane_Matrix(eye, at, 0.7f, 0, 96, none) && !host::ShapePane_Matrix(eye, at, 0.7f, 96, 0, none),
		"(6) no matrix for the eye at the middle, or a box of no size");

	// an eye or a middle that isn't a number (Drake's NIT on 0ae66bf9: this test builds /fp:fast, as the editor does, under which a compiler may drop a test
	// that a NaN fails, so the numbers are read from their bits); built from bits here too, so no arithmetic of the test's own can be folded away
	uint32_t const nanBits = 0x7FC00000u, infinityBits = 0x7F800000u;
	float notANumber = 0.0f, infinity = 0.0f;
	memcpy(&notANumber, &nanBits, sizeof(notANumber));
	memcpy(&infinity, &infinityBits, sizeof(infinity));
	float const nanEye[3] = { notANumber, eye[1], eye[2] };
	float const infiniteAt[3] = { at[0], infinity, at[2] };
	bool const madeFromNaN = host::ShapePane_Matrix(nanEye, at, 0.7f, 96, 96, none);
	bool const madeFromInfinity = host::ShapePane_Matrix(eye, infiniteAt, 0.7f, 96, 96, none);

	Check(!madeFromNaN && !madeFromInfinity, std::string("(6) no matrix for an eye that isn't a number, or a middle at infinity (") + (madeFromNaN ? "made from a NaN eye" : "NaN eye refused") +
		"; " + (madeFromInfinity ? "made from an infinite middle" : "infinite middle refused") + ")");

	printf(checksFailed ? "host_shape_pane_plan_test: %d FAILED\n" : "host_shape_pane_plan_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
