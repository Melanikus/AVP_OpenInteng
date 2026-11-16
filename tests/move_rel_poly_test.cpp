// The test for Move rel. to poly's sums (move_rel_poly.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt. No window, none of the game. The expected numbers are worked by hand from inteng's FUN_005633e0 as read (p11-shape-handlers.md 18, 19, 42, 43).

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "move_rel_poly.h"

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

static bool Is3(const int32_t *got, int32_t a, int32_t b, int32_t c)
{
	return got[0] == a && got[1] == b && got[2] == c;
}

int main()
{
	puts("Move rel. to poly's sums:");

	int32_t const identity[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };

	// a polygon at z = 1000 facing +z, corners (0,0,1000), (3000,0,1000), (0,3000,1000): each term divided by 3 before it is summed
	MoveRelPolyTarget flat;
	memset(&flat, 0, sizeof(flat));
	flat.cornerCount = 3;
	int32_t const corners[3][3] = { { 0, 0, 1000 }, { 3000, 0, 1000 }, { 0, 3000, 1000 } };
	memcpy(flat.corners, corners, sizeof(corners));
	flat.normal[2] = 65536;
	memcpy(flat.matrix, identity, sizeof(identity));

	MoveRelPolyResult out;

	Check(MoveRelPoly_Make(flat, 100000, nullptr, &out) && Is3(out.place, 1000, 1000, 100999),
		"the point is the average with each term divided first (z: 333 + 333 + 333 = 999), then moved back along the negated normal by 1 metre (100000)");
	Check(Is3(out.matrix, -65536, 0, 0) && Is3(out.matrix + 3, 0, 65536, 0) && Is3(out.matrix + 6, 0, 0, -65536),
		"the matrix: row 2 the negated normal, row 0 the axis (n.z, 0, -n.x) at length 65536, row 1 their cross product");

	int32_t const click[3] = { 5, 6, 7 };
	Check(MoveRelPoly_Make(flat, 100000, click, &out) && Is3(out.place, 5, 6, 100007), "a click's point replaces the polygon's point, and the move back still applies");
	Check(MoveRelPoly_Make(flat, 0, nullptr, &out) && Is3(out.place, 1000, 1000, 999), "a distance of 0 leaves it on the point");

	// a polygon facing +x, the target turned a quarter: m0..m8 = 0,1,0, -1,0,0, 0,0,1 (x' = -y, y' = x)
	MoveRelPolyTarget turned = flat;
	int32_t const quarter[9] = { 0, 65536, 0, -65536, 0, 0, 0, 0, 65536 };
	memcpy(turned.matrix, quarter, sizeof(quarter));
	turned.normal[0] = 65536;
	turned.normal[1] = 0;
	turned.normal[2] = 0;
	turned.place[0] = 100;
	turned.place[1] = 200;
	turned.place[2] = 300;
	Check(MoveRelPoly_Make(turned, 0, nullptr, &out) && Is3(out.place, -1000 + 100, 1000 + 200, 999 + 300), "the target's matrix turns the point (x' = -y, y' = x) and its place is added");
	Check(Is3(out.matrix + 6, 0, -65536, 0), "the negated normal is turned too");
	Check(Is3(out.matrix, 65536, 0, 0) && Is3(out.matrix + 3, 0, 0, 65536), "with n.x and n.z both 0 the axis is (65536, 0, 0)");

	// a slanted normal: (0.6, 0, 0.8) in 16.16 is (39322, 0, 52429): the axis (n.z, 0, -n.x) scaled to 65536
	MoveRelPolyTarget slanted = flat;
	slanted.normal[0] = -39322;
	slanted.normal[2] = -52429;
	Check(MoveRelPoly_Make(slanted, 0, nullptr, &out) && Is3(out.matrix + 6, 39322, 0, 52429), "a normal's sign is flipped by the negation");
	int64_t const length2 = (int64_t)out.matrix[0] * out.matrix[0] + (int64_t)out.matrix[2] * out.matrix[2];
	Check(out.matrix[1] == 0 && length2 > 65535LL * 65535 && length2 < 65537LL * 65537, "row 0 has length 65536 (to the last digit)");

	Check(!MoveRelPoly_Make(flat, 0, nullptr, nullptr), "no place to write to: refused");
	MoveRelPolyTarget none = flat;
	none.cornerCount = 0;
	MoveRelPolyTarget many = flat;
	many.cornerCount = kMoveRelPolyMostCorners + 1;
	Check(!MoveRelPoly_Make(none, 0, nullptr, &out) && !MoveRelPoly_Make(many, 0, nullptr, &out), "no corners, or too many, is refused");

	Check(MoveRelPoly_DistanceFromMetres(1.5) == 150000 && MoveRelPoly_DistanceFromMetres(-2.0) == -200000 && MoveRelPoly_DistanceFromMetres(0.0) == 0 && MoveRelPoly_DistanceFromMetres(0.29) == 29000 && MoveRelPoly_DistanceFromMetres(1.1) == 110000,
		"metres become the int: times 100000, truncated, with slack for the double's last digit (0.29 m is 29000)");
	Check(MoveRelPoly_DistanceFromMetres(1e30) == INT32_MAX && MoveRelPoly_DistanceFromMetres(-1e30) == INT32_MIN && MoveRelPoly_DistanceFromMetres(nan("")) == 0,
		"a number too big is held to the range of an int");
	Check(MoveRelPoly_MetresFromDistance(150000) > 1.4999999 && MoveRelPoly_MetresFromDistance(150000) < 1.5000001, "and the default shown is the int times 0.00001");
	Check(MoveRelPoly_Multiply(65536, 65536) == 65536 && MoveRelPoly_Multiply(-65536, 100000) == -100000 && MoveRelPoly_Multiply(1, -1) == -1,
		"the 16.16 multiply shifts the 64-bit product down 16 (arithmetic: -1 >> 16 is -1)");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks ok");
	return checksFailed ? 1 : 0;
}
