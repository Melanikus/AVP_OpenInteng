// The test of Recentre Shape on a polygon (shape_edit.h, ShapeEdit_RecentreOnPolygon; Texture Map 0xd6) on the game's own levels given on the command line: for objects with shapes that no other object uses, the
// polygon's centre must be at the origin afterwards (within 1 per axis: the centre is cut toward 0), and every point of the shape must be where it was in the WORLD (within 3: the place is moved by the
// turned offset); the level must still build. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_shape_recentre_polygon_test.args builds it; host_snap_vertex_test <level file> [...]. A non-zero result means a check failed.

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
#include "shape_edit.h"

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

	for (LIF<Object_Chunk *> each(&found); !each.done() && ids.size() < 12; each.next())
	{
		model::ShapeModel shape;
		std::vector<int32_t> points, normals;
		uint8_t id[8];

		memcpy(id, &each()->object_data.ID, 8);

		if (FilePick_World(&file, level, id, &shape, &points, &normals) == FilePickResult::Done && !shape.polygons.empty())
		{
			ids.push_back(std::vector<uint8_t>(id, id + 8));
		}
	}

	int done = 0;

	for (std::vector<uint8_t> const & id : ids)
	{
		model::ShapeModel before;
		std::vector<int32_t> worldBefore, normals;

		FilePick_World(&file, level, id.data(), &before, &worldBefore, &normals);

		size_t users = 0;
		std::string why;
		int32_t const polygon = (int32_t)(before.polygons.size() / 2);
		ShapeEditResult const result = ShapeEdit_RecentreOnPolygon(&file, level, id.data(), polygon, &users, &why);

		if (result == ShapeEditResult::Refused)
		{
			continue;     // a morph, or a place the game would refuse: refused is the right answer
		}

		Check(result == ShapeEditResult::Done, "the recentre is done (" + why + ")");

		if (result != ShapeEditResult::Done)
		{
			continue;
		}

		model::ShapeModel after;
		std::vector<int32_t> worldAfter;

		FilePick_World(&file, level, id.data(), &after, &worldAfter, &normals);

		// the polygon's centre is at the origin
		model::ShapePolygon const & poly = after.polygons[(size_t)polygon];
		double sum[3] = { 0, 0, 0 };
		int n = 0;

		for (int k = 0; k < poly.cornerCount; k++)
		{
			bool again = false;

			for (int j = 0; j < k; j++)
			{
				again = again || poly.corners[j] == poly.corners[k];
			}

			if (!again)
			{
				sum[0] += after.points[(size_t)poly.corners[k]].x;
				sum[1] += after.points[(size_t)poly.corners[k]].y;
				sum[2] += after.points[(size_t)poly.corners[k]].z;
				n++;
			}
		}

		bool atOrigin = n > 0;

		for (int axis = 0; axis < 3; axis++)
		{
			atOrigin = atOrigin && fabs(sum[axis] / n) <= 1.0;
		}

		Check(atOrigin, "the polygon's centre is at the origin");

		// the world stays where it was
		bool same = worldBefore.size() == worldAfter.size();
		int32_t worst = 0;

		for (size_t i = 0; same && i < worldBefore.size(); i++)
		{
			int32_t const gap = abs(worldBefore[i] - worldAfter[i]);

			worst = gap > worst ? gap : worst;
		}

		Check(same && worst <= 3, "every point is where it was in the world (worst gap " + std::to_string(worst) + ")");
		done++;

		if (done >= 6)
		{
			break;
		}
	}

	std::vector<char> built;

	file.BuildLevel(built);
	Check(!built.empty(), "the level still builds");
}

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_shape_recentre_polygon_test <level file> [<level file> ...]\n");
		return 2;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i]);
	}

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
