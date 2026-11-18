// host_camera_orbit_test.cpp: the test of src/host/camera_orbit.cpp, the camera orbiting a test's subject (the owner, 11-18: "like orbiting the subject, not a straight shot"). Written 2026-11-18
// by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// CHECKS: a one-metre box, worked by hand: the default 45 round and 45 up stands the camera above the box's +x -z corner side, 2.5 half diagonals off, turned 512 and looking 512 down; for
// every around and up of a grid, the camera is at the distance asked from the centre and the way it faces (its turn and look, the game's axes) points at the centre within a tick; around
// 0, 90, 180 and 270 land on the box's four sides (turn 1024 looks along -x, Bishop 11-18 a701208a, measured); and each refusal: a box the wrong way round, too big, an up past 89, a negative or too long distance, not-a-number, a place past 2^24.

#include <math.h>
#include <stdio.h>

#include <string>

#include "camera_orbit.h"

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

static std::string Said(OrbitCamera const &c)
{
	return std::to_string(c.place[0]) + " " + std::to_string(c.place[1]) + " " + std::to_string(c.place[2]) + " turn " + std::to_string(c.turn) + " look " + std::to_string(c.look);
}

int main()
{
	double const pi = 3.14159265358979323846;
	int32_t const mins[3] = { 0, 0, 0 }, maxs[3] = { 1000, 1000, 1000 };
	OrbitCamera c = {};
	double used = 0;

	// by hand: the half diagonal is sqrt(3) x 500 = 866.03, the distance 2.5 times it = 2165.06; the part along the ground 2165.06 x cos 45 = 1530.93, so x = 500 + 1530.93 x sin 45 = 1582.53, z = 500 - 1530.93 x cos 45 = -582.53,
	// y = 500 - 2165.06 x sin 45 = -1030.93 (up is minus y)
	bool const placed = CameraOrbit_Place(mins, maxs, kOrbitDefaultAround, kOrbitDefaultUp, 0, &c, &used);

	Check(placed && fabs(used - 2165.0635) < 0.01, "the default distance is 2.5 half diagonals (" + std::to_string(used) + ")");
	Check(c.place[0] == 1583 && c.place[1] == -1031 && c.place[2] == -583 && c.turn == 512 && c.look == 512, "45 round and 45 up: above the box's +x -z side, turned 512, looking 512 down (" + Said(c) + ")");

	// a small box: at least a metre
	int32_t const tiny[3] = { 10, 10, 10 };

	bool const tinyPlaced = CameraOrbit_Place(mins, tiny, 45, 45, 0, &c, &used);

	Check(tinyPlaced && used == kOrbitLeastDistance, "a box of a centimetre: the distance is a metre (" + std::to_string(used) + ")");

	// a big box (8 m): at most four metres
	int32_t const big[3] = { 8000, 8000, 8000 };
	bool const bigPlaced = CameraOrbit_Place(mins, big, 45, 45, 0, &c, &used);

	Check(bigPlaced && used == kOrbitMostDefaultDistance, "a box of 8 metres: the distance is four metres (" + std::to_string(used) + ")");

	// every around and up of a grid: at the distance, and facing the centre
	bool atDistance = true, facing = true;
	std::string first;

	for (int around = -720; around <= 720 && (atDistance && facing); around += 15)
	{
		for (int up = -85; up <= 85 && (atDistance && facing); up += 10)
		{
			if (!CameraOrbit_Place(mins, maxs, around, up, 5000, &c, &used))
			{
				atDistance = false;
				first = "refused at " + std::to_string(around) + " " + std::to_string(up);
				break;
			}

			double const to[3] = { 500.0 - c.place[0], 500.0 - c.place[1], 500.0 - c.place[2] };
			double const length = sqrt(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
			double const t = c.turn * 2.0 * pi / 4096.0, l = c.look * 2.0 * pi / 4096.0;
			// the way the camera faces: turn about the up axis from +z toward -x (1024 looks along -x), look positive down (+y)
			double const faces[3] = { -sin(t) * cos(l), sin(l), cos(t) * cos(l) };
			double const along = (to[0] * faces[0] + to[1] * faces[1] + to[2] * faces[2]) / length;

			atDistance = fabs(length - 5000.0) < 1.5;
			facing = along > cos(2.0 * pi / 4096.0 * 1.5);

			if (!atDistance || !facing)
			{
				first = "at " + std::to_string(around) + " round, " + std::to_string(up) + " up: " + Said(c) + ", " + std::to_string(length) + " away, facing " + std::to_string(along);
			}
		}
	}

	Check(atDistance, "every around and up of the grid: the camera 5000 from the centre" + (first.empty() ? std::string() : " (" + first + ")"));
	Check(facing, "and it faces the centre, within a tick and a half" + (first.empty() ? std::string() : " (" + first + ")"));

	// the four sides
	OrbitCamera side[4] = {};
	bool sides = true;

	for (int k = 0; k < 4; k++)
	{
		sides = sides && CameraOrbit_Place(mins, maxs, k * 90.0, 0, 3000, &side[k], nullptr);
	}

	Check(sides && side[0].place[2] == -2500 && side[0].turn == 0 && side[1].place[0] == 3500 && side[1].turn == 1024 && side[2].place[2] == 3500 && side[2].turn == 2048 &&
		side[3].place[0] == -2500 && side[3].turn == 3072 && side[0].look == 0, "0, 90, 180, 270 round, level: on the -z, +x, +z and -x sides, turned 0, 1024, 2048, 3072 (" + Said(side[0]) + " / " + Said(side[1]) + " / " +
		Said(side[2]) + " / " + Said(side[3]) + ")");

	// the refusals
	int32_t const wrong[3] = { 1000, -5, 1000 };
	int32_t const huge[3] = { 40000000, 10, 10 };
	int32_t const far[3] = { 16777000, 0, 0 };

	Check(!CameraOrbit_Place(mins, wrong, 45, 45, 0, &c, nullptr), "a box the wrong way round is refused");
	Check(!CameraOrbit_Place(mins, huge, 45, 45, 0, &c, nullptr), "a box past 2^25 across is refused");
	Check(!CameraOrbit_Place(mins, maxs, 45, 90, 0, &c, nullptr) && !CameraOrbit_Place(mins, maxs, 45, -90, 0, &c, nullptr), "an up of 90 or -90 (straight down or up) is refused");
	Check(!CameraOrbit_Place(mins, maxs, 45, 45, -1, &c, nullptr) && !CameraOrbit_Place(mins, maxs, 45, 45, kOrbitMostDistance + 1, &c, nullptr), "a negative distance, or one past a kilometre, is refused");
	Check(!CameraOrbit_Place(mins, maxs, NAN, 45, 0, &c, nullptr) && !CameraOrbit_Place(mins, maxs, 45, INFINITY, 0, &c, nullptr), "a number that is not finite is refused");
	Check(!CameraOrbit_Place(far, far, 90, 0, 5000, &c, nullptr), "a camera place past 2^24 is refused (16,777,000 and 5 m on its +x side)");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
