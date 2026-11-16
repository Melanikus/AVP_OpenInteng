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
		done++;
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

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

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
