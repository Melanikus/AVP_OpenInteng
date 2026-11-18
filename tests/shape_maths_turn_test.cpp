// The test for model::RotateByTurn and model::TurnMatrix (src/model/shape_maths.cpp): an object's turn (the four floats of OBJHEAD1) as inteng turns a point with it.
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng (TB-C8h of AvP-docs/engine/inteng/reassess/construction-closeout-frost.md). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (read in the raw listing, out\listing-all-1b.txt; the decompiler's float casts at dump-full-1.txt:40460-40470 are not in the code):
// - FUN_00430d70 makes the 3x3 matrix in DOUBLE: it loads the four floats, keeps 2x, 2y, 2z and every product as doubles (FSTP double, 0x430d7c-0x430e6b), and
//   m0 = 1-(Zz+Yy), m1 = Yx-Zw, m2 = Zx+Yw, m3 = Yx+Zw, m4 = 1-(Zz+Xx), m5 = Zy-Xw, m6 = Zx-Yw, m7 = Zy+Xw, m8 = 1-(Yy+Xx) (X = 2x, Y = 2y, Z = 2z).
// - FUN_00430e80 turns a whole-number point: each row (v.x*m0 + v.y*m1) + v.z*m2 in double, truncated toward zero by __ftol (0x430e8c-0x430eee).
// WHAT IT CHECKS:
//  1. a quarter turn about y, (0, 0.70710677, 0, 0.70710677) the way a file holds it, takes (1500, 0, 1000) to x' = 1000, z' = -1499: inteng's doubles give
//     m2 = 0.99999996577..., so 1000 * m2 + 1500 * m0 = 1000.0000171 (ours with float products made m2 = 0.99999994 and gave 999), and z' = 1500 * m6 + 1000 * m8 = -1499.99991,
//     which __ftol truncates toward zero to -1499;
//  2. TurnMatrix gives the listing's nine numbers for that turn, bit for bit as doubles;
//  3. the turn (0, 0, 0, 1) leaves points as they are; a turn that is not a finite number is refused (Overflow).

#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>

#include "shape_maths.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string Point(model::ShapePoint const &p)
{
	return std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.z);
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// 1. the quarter turn about y
	float const quarter[4] = { 0.0f, 0.70710677f, 0.0f, 0.70710677f };
	model::ShapePoint const v = { 1500, 0, 1000 };
	model::ShapePoint turned = { 0, 0, 0 };
	model::ShapeMathsResult const result = model::RotateByTurn(quarter, v, &turned);

	Check(result == model::ShapeMathsResult::Ok && turned.x == 1000 && turned.y == 0 && turned.z == -1499,
		"(1500, 0, 1000) turned a quarter about y is (1000, 0, -1499), as inteng's doubles and truncation give: " + Point(turned));

	// 2. the matrix, from the listing's own steps in double
	double const x = (double)quarter[0], y = (double)quarter[1], z = (double)quarter[2], w = (double)quarter[3];
	double const X = x + x, Y = y + y, Z = z + z;
	double const wanted[9] =
	{
		1.0 - (Z * z + Y * y), Y * x - Z * w, x * Z + Y * w,
		Y * x + Z * w, 1.0 - (Z * z + X * x), Z * y - X * w,
		x * Z - Y * w, Z * y + X * w, 1.0 - (Y * y + X * x),
	};
	double m[9] = { 0 };

	model::TurnMatrix(quarter, m);
	Check(memcmp(m, wanted, sizeof(m)) == 0, "TurnMatrix gives the listing's nine doubles (m2 = " + std::to_string(m[2]) + ")");
	Check(m[2] != (double)0.99999994f, "m2 is not rounded to a float (0.99999994)");

	// 3. no turn, and a turn that isn't a number
	float const none[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	model::ShapePoint const some = { -123, 4567, 89 };
	model::ShapePoint same = { 0, 0, 0 };

	bool const noneOk = model::RotateByTurn(none, some, &same) == model::ShapeMathsResult::Ok;   // turned before the words are made

	Check(noneOk && same.x == some.x && same.y == some.y && same.z == some.z,
		"the turn (0, 0, 0, 1) leaves a point as it is: " + Point(same));

	float bad[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	bad[1] = std::numeric_limits<float>::quiet_NaN();
	Check(model::RotateByTurn(bad, some, &same) == model::ShapeMathsResult::Overflow, "a turn that is not a number is refused");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
