// host_movable_light_test.cpp: movable.h's LIGHT adapter (src/host/movable_light.cpp, with light_keys.h LightPose_Set / LightPose_Get and movable_matrix.h) on a
// stock level's Light Sourcing lights: a light's place and turn read (its matrix, through the turn and back, to 2 / 65536), a new place and a quarter turn
// written in one write through the game's loader (the matrix the game's QuatToMat of the turn, its third row the new direction), the changed flag put in and
// every other number kept, the other lights not moved, a saved copy taken by the loader, and a light that is none or a place past 2^24 refused with nothing
// changed. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_movable_light_test.args builds it; host_movable_light_test <level file> [...]. A non-zero result means a check failed.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"

#include "level_file.h"
#include "light_edit.h"
#include "light_keys.h"
#include "movable.h"
#include "movable_matrix.h"

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

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

static host::MovableRef RefOf(int32_t light)
{
	host::MovableRef ref;

	ref.kind = host::MovableKind::Light;
	ref.number = light;
	return ref;
}

static bool MatrixNear(int32_t const a[9], int32_t const b[9], int32_t most = 2)
{
	for (int i = 0; i < 9; i++)
	{
		if (abs(a[i] - b[i]) > most)
		{
			return false;
		}
	}

	return true;
}

static int32_t MostApart(int32_t const a[9], int32_t const b[9])
{
	int32_t most = 0;

	for (int i = 0; i < 9; i++)
	{
		most = abs(a[i] - b[i]) > most ? abs(a[i] - b[i]) : most;
	}

	return most;
}

// How far a stock matrix is from a turn, in 1/65536: its rows' lengths from 1, the rows' dot products from 0, and the third row from the first two's cross
// product. A quaternion holds only a true turn, so the turn read from such a matrix gives back the nearest true one, up to about this far.
static int32_t OwnError(int32_t const m[9])
{
	double r[3][3];

	for (int i = 0; i < 9; i++)
	{
		r[i / 3][i % 3] = (double)m[i] / 65536.0;
	}

	double worst = 0.0;

	for (int i = 0; i < 3; i++)
	{
		double const length = sqrt(r[i][0] * r[i][0] + r[i][1] * r[i][1] + r[i][2] * r[i][2]);

		worst = fabs(length - 1.0) > worst ? fabs(length - 1.0) : worst;

		for (int j = i + 1; j < 3; j++)
		{
			double const dot = r[i][0] * r[j][0] + r[i][1] * r[j][1] + r[i][2] * r[j][2];

			worst = fabs(dot) > worst ? fabs(dot) : worst;
		}
	}

	double const cross[3] = { r[0][1] * r[1][2] - r[0][2] * r[1][1], r[0][2] * r[1][0] - r[0][0] * r[1][2], r[0][0] * r[1][1] - r[0][1] * r[1][0] };

	for (int c = 0; c < 3; c++)
	{
		worst = fabs(cross[c] - r[2][c]) > worst ? fabs(cross[c] - r[2][c]) : worst;
	}

	return (int32_t)ceil(worst * 65536.0);
}

static std::string Words(int32_t const m[9])
{
	std::string out;

	for (int i = 0; i < 9; i++)
	{
		out += (i ? " " : "") + std::to_string(m[i]);
	}

	return out;
}

static void TestFile(std::string const &path, std::string const &folder, int *withLights)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	int32_t place[3], matrix[9], place1[3], matrix1[9];

	if (host::LightPose_Get(&file, 0, place, matrix) != host::LightEditResult::Done)
	{
		printf("  (no light here)\n");
		return;
	}

	(*withLights)++;

	bool const hasSecond = host::LightPose_Get(&file, 1, place1, matrix1) == host::LightEditResult::Done;
	host::LightData before;

	Check(host::CopyLight(&file, 0, &before) == host::LightEditResult::Done, "light 0's numbers are read");

	host::MovablePose got;

	Check(host::MovableLight_Get(&file, RefOf(0), &got) == host::MovableResult::Done && memcmp(got.place, place, sizeof(place)) == 0,
		"light 0: the pose's place is the STDLIGHT's (" + std::to_string(place[0]) + " " + std::to_string(place[1]) + " " + std::to_string(place[2]) + ")");

	int32_t again[9];
	int32_t const own = OwnError(matrix);
	int32_t const allowed = 2 + 2 * own;

	host::Movable_MatrixOfTurn(got.turn, again);
	Check(MatrixNear(again, matrix, allowed), "its turn gives its matrix back: " + std::to_string(MostApart(again, matrix)) + " / 65536 apart at most, allowed " +
		std::to_string(allowed) + " (2, and twice the stock matrix's own distance from a true turn, " + std::to_string(own) + ") (" + Words(matrix) + ")");

	// a new place and a quarter turn about the vertical axis
	host::MovablePose moved;

	moved.place[0] = place[0] + 1000;
	moved.place[1] = place[1] - 500;
	moved.place[2] = place[2] + 250;
	moved.turn[0] = 0.0f;
	moved.turn[1] = 0.70710678f;
	moved.turn[2] = 0.0f;
	moved.turn[3] = 0.70710678f;

	Check(host::MovableLight_Set(&file, RefOf(0), moved) == host::MovableResult::Done, "a new place and turn are written (the game's loader took the level)");

	int32_t nowPlace[3] = {}, nowMatrix[9] = {};
	// the game's QuatToMat of (0, -0.7071, 0, 0.7071): mat13 = 2 w y = -1, mat22 1, mat31 = -2 w y = 1, the rest 0. Its third row, the light's direction: +x
	int32_t const want[9] = { 0, 0, -65536, 0, 65536, 0, 65536, 0, 0 };
	bool const readNow = host::LightPose_Get(&file, 0, nowPlace, nowMatrix) == host::LightEditResult::Done;

	Check(readNow && memcmp(nowPlace, moved.place, sizeof(nowPlace)) == 0 && MatrixNear(nowMatrix, want),
		"the STDLIGHT holds the new place and the game's QuatToMat of the turn, its third row (the direction) +x (" + Words(nowMatrix) + ")");

	host::LightData after;

	Check(host::CopyLight(&file, 0, &after) == host::LightEditResult::Done && after.size() == before.size() && memcmp(after.data(), before.data(), 4) == 0 &&
		memcmp(after.data() + 52, before.data() + 52, 20) == 0 && memcmp(after.data() + 76, before.data() + 76, before.size() - 76) == 0,
		"its number, brightness, spread, range, colour, engine flags and pads are kept");

	int32_t localBefore = 0, localAfter = 0;

	memcpy(&localBefore, before.data() + 72, 4);
	memcpy(&localAfter, after.data() + 72, 4);
	Check(localAfter == (localBefore | 4), "its local flags gain the changed flag (4), as L's move gives");

	host::MovablePose back;

	Check(host::MovableLight_Get(&file, RefOf(0), &back) == host::MovableResult::Done && memcmp(back.place, moved.place, sizeof(back.place)) == 0, "read back: the new place");

	int32_t second[3], secondMatrix[9];

	Check(!hasSecond || (host::LightPose_Get(&file, 1, second, secondMatrix) == host::LightEditResult::Done && memcmp(second, place1, sizeof(second)) == 0 &&
		memcmp(secondMatrix, matrix1, sizeof(secondMatrix)) == 0), "light 1 did not move or turn");

	std::string const saved = folder + "\\movable_light_copy.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile copy;
		int32_t copyPlace[3], copyMatrix[9];

		Check(copy.Open(saved.c_str()) == LevelOpenResult::Opened && host::LightPose_Get(&copy, 0, copyPlace, copyMatrix) == host::LightEditResult::Done &&
			memcmp(copyPlace, moved.place, sizeof(copyPlace)) == 0 && MatrixNear(copyMatrix, want), "a saved copy is taken by the game's own loader with the new place and turn");
		DeleteFileA(saved.c_str());
	}
	else
	{
		Check(false, "a copy is saved");
	}

	// refusals, the level as it was after each
	unsigned const generation = file.Generation();
	host::MovablePose untouched;
	host::MovablePose tooFar = moved;

	untouched.place[0] = 12345;
	tooFar.place[0] = (1 << 24) + 1;
	Check(host::MovableLight_Get(&file, RefOf(1 << 20), &untouched) == host::MovableResult::NoSuchThing && untouched.place[0] == 12345 &&
		host::MovableLight_Set(&file, RefOf(1 << 20), moved) == host::MovableResult::NoSuchThing, "a light number past the set: NoSuchThing");
	Check(host::MovableLight_Get(&file, RefOf(-1), &untouched) == host::MovableResult::NoSuchThing, "light -1: NoSuchThing");
	Check(host::MovableLight_Set(&file, RefOf(0), tooFar) == host::MovableResult::WriterRefused, "a place past 2^24: refused (WriterRefused)");
	Check(file.Generation() == generation, "and the refusals changed nothing");

	// and back as it was
	Check(host::MovableLight_Set(&file, RefOf(0), got) == host::MovableResult::Done && host::LightPose_Get(&file, 0, nowPlace, nowMatrix) == host::LightEditResult::Done &&
		memcmp(nowPlace, place, sizeof(place)) == 0 && MatrixNear(nowMatrix, matrix, allowed), "written back: its place, and its matrix as near as above");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_movable_light_test <level file> [<level file> ...]\n");
		return 2;
	}

	std::string const folder = "movable_light_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	int withLights = 0;

	for (int a = 1; a < argc; a++)
	{
		TestFile(argv[a], folder, &withLights);
	}

	RemoveDirectoryA(folder.c_str());

	host::MovablePose none;

	Check(host::MovableLight_Get(nullptr, RefOf(0), &none) == host::MovableResult::NothingOpen, "no level open: NothingOpen");
	Check(withLights > 0, "at least one level has lights (" + std::to_string(withLights) + ")");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
