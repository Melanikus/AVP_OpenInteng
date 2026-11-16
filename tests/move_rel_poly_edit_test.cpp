// The test for the turns of Move rel. to poly (move_rel_poly_edit.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt. No window, none of the game, no level: the conversions between the file's turn (four floats) and inteng's matrix (nine 16.16 ints).
//
// WHAT IT TESTS: the int matrix made from a file turn (FUN_004b3c80 on the conjugate quaternion) turns a point, by inteng's own rule (FUN_004b3860: x' = m0 x + m3 y +
// m6 z, ...), exactly as the game's own rotation of the file's floats does (the shape maths' RotateByTurn = FUN_00430d70 + FUN_00430e80): that is what shows on screen,
// so it is what the loader's conversion must give. And the matrix back to a turn (FUN_004de070) gives a turn that rotates the same way, in every branch of its code
// (a positive trace, and the three largest-diagonal cases of a half-turn about x, y, z). Also: the words of every answer, and a call with no level.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "chunk.hpp"
#include "move_rel_poly.h"
#include "move_rel_poly_edit.h"
#include "shape_maths.h"

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

// ---- Stand-ins for the parts of the game the chunk code calls (the edit module links the chunk code)

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// FUN_004b3860: a point turned by inteng's matrix
static void IntengTurn(int32_t const m[9], int32_t const v[3], int32_t out[3])
{
	out[0] = MoveRelPoly_Multiply(m[0], v[0]) + MoveRelPoly_Multiply(m[3], v[1]) + MoveRelPoly_Multiply(m[6], v[2]);
	out[1] = MoveRelPoly_Multiply(m[1], v[0]) + MoveRelPoly_Multiply(m[4], v[1]) + MoveRelPoly_Multiply(m[7], v[2]);
	out[2] = MoveRelPoly_Multiply(m[2], v[0]) + MoveRelPoly_Multiply(m[5], v[1]) + MoveRelPoly_Multiply(m[8], v[2]);
}

static void Unit(float q[4], double x, double y, double z, double w)
{
	double const length = sqrt(x * x + y * y + z * z + w * w);

	q[0] = (float)(x / length);
	q[1] = (float)(y / length);
	q[2] = (float)(z / length);
	q[3] = (float)(w / length);
}

// The largest gap between inteng's turn of the three axes' points by the matrix, and the game's rotation of the same points by the turn
static int32_t WorstGap(int32_t const m[9], float const turn[4])
{
	int32_t const points[5][3] = { { 100000, 0, 0 }, { 0, 100000, 0 }, { 0, 0, 100000 }, { 70000, -30000, 50000 }, { -123456, 654321, -42424 } };
	int32_t worst = 0;

	for (int p = 0; p < 5; p++)
	{
		int32_t byMatrix[3];
		model::ShapePoint byTurn;

		IntengTurn(m, points[p], byMatrix);

		if (model::RotateByTurn(turn, { points[p][0], points[p][1], points[p][2] }, &byTurn) != model::ShapeMathsResult::Ok)
		{
			return INT32_MAX;
		}

		int32_t const gaps[3] = { abs(byMatrix[0] - byTurn.x), abs(byMatrix[1] - byTurn.y), abs(byMatrix[2] - byTurn.z) };

		for (int g = 0; g < 3; g++)
		{
			if (gaps[g] > worst)
			{
				worst = gaps[g];
			}
		}
	}

	return worst;
}

static void TestTurns()
{
	puts("The int matrix turns a point as the game's rotation of the file's turn does:");

	float turns[12][4];
	int count = 0;

	Unit(turns[count++], 0, 0, 0, 1);                  // no turn
	Unit(turns[count++], 0, 0, 0.70710678, 0.70710678);  // a quarter about z
	Unit(turns[count++], 0.70710678, 0, 0, 0.70710678);  // a quarter about x
	Unit(turns[count++], 0, 0.70710678, 0, 0.70710678);  // a quarter about y
	Unit(turns[count++], 0.2, -0.5, 0.3, 0.8);
	Unit(turns[count++], -0.6, 0.1, 0.7, 0.2);
	Unit(turns[count++], 0.3, 0.3, 0.3, 0.3);
	Unit(turns[count++], 0, 0, 1, 0);                  // a half turn about z
	Unit(turns[count++], 0, 1, 0, 0);                  // about y
	Unit(turns[count++], 1, 0, 0, 0);                  // about x
	Unit(turns[count++], 0.1, 0.9, -0.2, -0.3);
	Unit(turns[count++], -0.4, -0.4, 0.5, 0.65);

	int32_t worst = 0;

	for (int t = 0; t < count; t++)
	{
		int32_t m[9];

		MoveRelPolyEdit_TurnToMatrix(turns[t], m);

		int32_t const gap = WorstGap(m, turns[t]);

		if (gap > worst)
		{
			worst = gap;
		}

		Check(gap <= 40, "turn " + std::to_string(t) + ": the matrix and the rotation agree within 40 units (the points go to 654321: the ints hold 1/65536 and truncate) (gap " + std::to_string(gap) + ")");
	}

	printf("  (the worst gap over all: %d)\n", worst);
}

static void TestBack()
{
	puts("The matrix back to a turn (FUN_004de070) rotates the same way:");

	float turns[8][4];
	int count = 0;

	Unit(turns[count++], 0, 0, 0, 1);                    // a positive trace
	Unit(turns[count++], 0.2, -0.5, 0.3, 0.8);           // a positive trace, off the axes
	Unit(turns[count++], 1, 0, 0, 0);                    // a half turn about x: trace -1, x is the largest diagonal
	Unit(turns[count++], 0, 1, 0, 0);                    // about y
	Unit(turns[count++], 0, 0, 1, 0);                    // about z
	Unit(turns[count++], 0.1, 0.9, -0.2, -0.05);         // near a half turn, trace below 0
	Unit(turns[count++], -0.7, 0.1, 0.65, -0.1);
	Unit(turns[count++], 0.5, 0.5, 0.5, 0.5);

	for (int t = 0; t < count; t++)
	{
		int32_t m[9];
		float back[4];

		MoveRelPolyEdit_TurnToMatrix(turns[t], m);
		MoveRelPolyEdit_MatrixToTurn(m, back);

		double const length = sqrt((double)back[0] * back[0] + (double)back[1] * back[1] + (double)back[2] * back[2] + (double)back[3] * back[3]);

		Check(fabs(length - 1.0) < 1e-3, "turn " + std::to_string(t) + ": the turn that comes back is 1 long (" + std::to_string(length) + ")");

		int32_t const gap = WorstGap(m, back);

		Check(gap <= 60, "turn " + std::to_string(t) + ": the turn that comes back rotates as the matrix does, within 60 units (gap " + std::to_string(gap) + ")");
	}
}

static void TestIdentity()
{
	puts("The simplest turns:");

	int32_t m[9];
	float none[4] = { 0, 0, 0, 1 };

	MoveRelPolyEdit_TurnToMatrix(none, m);
	Check(m[0] == 65536 && m[4] == 65536 && m[8] == 65536 && m[1] == 0 && m[2] == 0 && m[3] == 0 && m[5] == 0 && m[6] == 0 && m[7] == 0, "no turn is the identity matrix");

	float back[4];

	MoveRelPolyEdit_MatrixToTurn(m, back);
	Check(back[0] == 0.0f && back[1] == 0.0f && back[2] == 0.0f && fabs(back[3] - 1.0f) < 1e-6, "and the identity matrix is no turn");
}

static void TestWords()
{
	puts("Words and bad calls:");

	MoveRelPolyEditResult const all[] = { MoveRelPolyEditResult::Done, MoveRelPolyEditResult::BadCall, MoveRelPolyEditResult::SameObject, MoveRelPolyEditResult::NoObject,
		MoveRelPolyEditResult::NoShape, MoveRelPolyEditResult::BadPolygon, MoveRelPolyEditResult::TooFar, MoveRelPolyEditResult::Refused };
	bool every = true;

	for (MoveRelPolyEditResult result : all)
	{
		char const *const words = MoveRelPolyEdit_Words(result);

		every = every && words && words[0] != 0;
	}

	Check(every, "every answer has words");

	uint8_t const id[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint8_t const other[8] = { 8, 7, 6, 5, 4, 3, 2, 1 };

	Check(MoveRelPolyEdit_Do(nullptr, nullptr, id, other, 0, 0, nullptr, 1.0, nullptr) == MoveRelPolyEditResult::BadCall, "no level: BadCall");
}

int main()
{
	TestIdentity();
	TestTurns();
	TestBack();
	TestWords();

	if (checksFailed)
	{
		printf("%d check(s) FAILED\n", checksFailed);
		return 1;
	}

	puts("all good");
	return 0;
}
