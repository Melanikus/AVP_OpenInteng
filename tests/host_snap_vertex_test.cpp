// The test of Snap To Vertex's data side (move_rel_poly_edit.h, SnapToVertex_Target / SnapToVertex_MoveObject) on the game's own levels given on the command line: for pairs of objects with shapes,
// the vertex of the first nearest a point is found, the second is moved by it, and one of the second's own world vertices must then lie exactly on that vertex; the level must still be taken by the game's
// own loader. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_snap_vertex_test.args builds it; host_snap_vertex_test <level file> [...]. A non-zero result means a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "chunk_object.hpp"
#include "level_file.h"
#include "file_pick.h"
#include "move_rel_poly_edit.h"

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const & what)
{
	checksRun++;

	if (!good)
	{
		printf("  FAILED  %s\n", what.c_str());
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char * name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char * message, int)
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

using namespace host;

static void TestFile(std::string const & path)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	File_Chunk * const level = file.Level();
	List<Object_Chunk *> found;
	std::vector<std::vector<uint8_t>> ids;

	level->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done() && ids.size() < 40; each.next())
	{
		model::ShapeModel shape;
		std::vector<int32_t> points, normals;
		uint8_t id[8];

		memcpy(id, &each()->object_data.ID, 8);

		if (FilePick_World(&file, level, id, &shape, &points, &normals) == FilePickResult::Done && points.size() >= 9)
		{
			ids.push_back(std::vector<uint8_t>(id, id + 8));
		}
	}

	if (ids.size() < 2)
	{
		printf("  (fewer than two objects with shapes)\n");
		return;
	}

	int done = 0;
	std::vector<size_t> snapped;   // the pairs the click's snap moved, for the live snap below

	for (size_t k = 0; k + 1 < ids.size() && done < 6; k += 2)
	{
		model::ShapeModel shape;
		std::vector<int32_t> targetPoints, normals;

		FilePick_World(&file, level, ids[k].data(), &shape, &targetPoints, &normals);

		// a point a little way from the target's first point
		double const point[3] = { targetPoints[0] + 50.0, targetPoints[1] - 30.0, targetPoints[2] + 10.0 };
		int32_t vertex[3];

		if (SnapToVertex_Target(&file, level, ids[k].data(), point, vertex) != MoveRelPolyEditResult::Done)
		{
			Check(false, "a target vertex is found");
			continue;
		}

		// it is one of the target's world points, and no other is nearer
		bool among = false;
		double best = 0.0;
		bool any = false;

		for (size_t i = 0; i + 2 < targetPoints.size(); i += 3)
		{
			double const dx = targetPoints[i] - point[0], dy = targetPoints[i + 1] - point[1], dz = targetPoints[i + 2] - point[2];
			double const d = dx * dx + dy * dy + dz * dz;

			among = among || (targetPoints[i] == vertex[0] && targetPoints[i + 1] == vertex[1] && targetPoints[i + 2] == vertex[2]);
			best = !any || d < best ? d : best;
			any = true;
		}

		Check(among, "the vertex found is one of the target's points");

		{
			double const dx = vertex[0] - point[0], dy = vertex[1] - point[1], dz = vertex[2] - point[2];

			Check(dx * dx + dy * dy + dz * dz <= best, "and no point of the target is nearer the click");
		}

		MoveRelPolyEditResult const moved = SnapToVertex_MoveObject(&file, level, ids[k + 1].data(), vertex);

		if (moved == MoveRelPolyEditResult::TooFar)
		{
			continue;     // the new place left the game's range: refused, as it should be
		}

		Check(moved == MoveRelPolyEditResult::Done, "the second object is moved");

		std::vector<int32_t> movedPoints;

		FilePick_World(&file, level, ids[k + 1].data(), &shape, &movedPoints, &normals);

		// the moved object has a vertex on the target's vertex (the places are whole numbers, so the match is exact)
		bool onIt = false;

		for (size_t i = 0; i + 2 < movedPoints.size(); i += 3)
		{
			onIt = onIt || (movedPoints[i] == vertex[0] && movedPoints[i + 1] == vertex[1] && movedPoints[i + 2] == vertex[2]);
		}

		Check(onIt, "one of the moved object's own vertices lies on the target vertex");
		snapped.push_back(k);
		done++;
	}

	// THE LIVE SNAP (SnapToVertex_Pose, OP-43): for each pair the click's snap moved, from where it put the second object, the same point snaps it nowhere new (its vertex is on T already) and leaves
	// its turn; with align on the target's first polygon the turn is a whole turn (a quaternion of length 1); nothing is written either way
	for (size_t const k : snapped)
	{
		model::ShapeModel shape;
		std::vector<int32_t> targetPoints, normals;

		FilePick_World(&file, level, ids[k].data(), &shape, &targetPoints, &normals);

		double const point[3] = { targetPoints[0] + 50.0, targetPoints[1] - 30.0, targetPoints[2] + 10.0 };
		int32_t before[3], place[3];
		float turn[4];

		if (ObjectPlace_Get(&file, level, ids[k + 1].data(), before) != MoveRelPolyEditResult::Done)
		{
			continue;
		}

		MoveRelPolyEditResult const plain = SnapToVertex_Pose(&file, level, ids[k + 1].data(), ids[k].data(), point, 0, false, place, turn);

		if (plain == MoveRelPolyEditResult::TooFar)
		{
			continue;
		}

		Check(plain == MoveRelPolyEditResult::Done && place[0] == before[0] && place[1] == before[1] && place[2] == before[2], "the live snap from the snapped place moves nothing");

		MoveRelPolyEditResult const aligned = SnapToVertex_Pose(&file, level, ids[k + 1].data(), ids[k].data(), point, 0, true, place, turn);

		if (aligned != MoveRelPolyEditResult::TooFar)
		{
			double const length = sqrt((double)turn[0] * turn[0] + (double)turn[1] * turn[1] + (double)turn[2] * turn[2] + (double)turn[3] * turn[3]);

			Check(aligned == MoveRelPolyEditResult::Done && fabs(length - 1.0) < 0.001, "the live snap with align gives a whole turn");
		}

		int32_t after[3];

		Check(ObjectPlace_Get(&file, level, ids[k + 1].data(), after) == MoveRelPolyEditResult::Done && memcmp(after, before, sizeof(after)) == 0, "and writes nothing");
	}

	{
		double const origin[3] = { 0.0, 0.0, 0.0 };
		int32_t place[3];
		float turn[4];

		Check(SnapToVertex_Pose(&file, level, ids[0].data(), ids[0].data(), origin, 0, false, place, turn) == MoveRelPolyEditResult::BadCall,
			"the live snap onto the object itself is refused (FUN_004f7000 0x4f7043: another block)");
	}

	// an object's place read and set again
	{
		int32_t place[3];

		Check(ObjectPlace_Get(&file, level, ids[0].data(), place) == MoveRelPolyEditResult::Done, "an object's place is read");

		int32_t const was0 = place[0];

		place[0] += 10;
		Check(ObjectPlace_Set(&file, level, ids[0].data(), place) == MoveRelPolyEditResult::Done, "and set");
		place[0] = 0;
		Check(ObjectPlace_Get(&file, level, ids[0].data(), place) == MoveRelPolyEditResult::Done && place[0] == was0 + 10, "and reads back");
		place[0] = 2000000000;
		Check(ObjectPlace_Set(&file, level, ids[0].data(), place) == MoveRelPolyEditResult::TooFar, "a place past 2^24 is refused");
	}

	std::vector<char> built;

	file.BuildLevel(built);
	Check(!built.empty(), "the level still builds after the moves");
}

// FUN_0053d1e0, FUN_004f6ee0 and the align of FUN_004f7000, worked by hand from the code (no level needed)
static void TestSnapMaths()
{
	int32_t const one = 65536;
	int32_t const identity[9] = { one, 0, 0, 0, one, 0, 0, 0, one };
	int32_t m[9];

	memcpy(m, identity, sizeof(m));
	SnapVertex_SquareUp(m);
	Check(memcmp(m, identity, sizeof(m)) == 0, "SquareUp leaves the identity alone");

	int32_t const nearly[9] = { 65000, 3000, -2000, -2500, 64000, 7000, 2000, -7000, 64500 };

	memcpy(m, nearly, sizeof(m));
	SnapVertex_SquareUp(m);
	Check(memcmp(m, identity, sizeof(m)) == 0, "SquareUp makes a slightly turned matrix the identity");

	int32_t zeros[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int32_t const zerosSquared[9] = { -one, 0, 0, 0, -one, 0, 0, 0, one };

	SnapVertex_SquareUp(zeros);
	Check(memcmp(zeros, zerosSquared, sizeof(zeros)) == 0, "SquareUp of zeros: row 0 x (a 0 is -1), row 1 y (-1), row 2 their product (the code's ties)");

	int32_t const up[3] = { 0, one, 0 };
	int32_t const upFrame[9] = { 0, 0, one, 0, one, 0, -one, 0, 0 };

	SnapVertex_NormalMatrix(up, m);
	Check(memcmp(m, upFrame, sizeof(m)) == 0, "NormalMatrix of up: (0, 0, 1), the normal, (-1, 0, 0) (the axis z: x and z tie at 0, z is taken)");

	memcpy(m, identity, sizeof(m));
	SnapVertex_Align(up, m);
	Check(memcmp(m, identity, sizeof(m)) == 0, "an unturned object aligned to a floor stays unturned");

	int32_t const down[3] = { 0, -one, 0 };

	memcpy(m, identity, sizeof(m));
	SnapVertex_Align(down, m);
	Check(memcmp(m, identity, sizeof(m)) == 0, "and to a ceiling");

	int32_t const slope[3] = { 0, 46341, 46341 };
	int32_t slopeFrame[9];

	SnapVertex_NormalMatrix(slope, slopeFrame);
	memcpy(m, identity, sizeof(m));
	SnapVertex_Align(slope, m);
	Check(memcmp(m, slopeFrame, sizeof(m)) == 0 && m[3] == 0 && m[4] == 46341 && m[5] == 46341, "aligned to a 45 degree slope it takes the slope's frame: its row 1 is the normal");

	int32_t const none[3] = { 0, 0, 0 };

	memcpy(m, nearly, sizeof(m));
	SnapVertex_Align(none, m);
	Check(memcmp(m, nearly, sizeof(m)) == 0, "a zero normal changes nothing (inteng's would divide by 0)");
}

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestSnapMaths();

	if (argc < 2)
	{
		printf("usage: host_snap_vertex_test <level file> [<level file> ...]\n");
		return 2;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i]);
	}

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
