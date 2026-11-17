// The test of src/host/autolink_records: AutoLink's records numbered as inteng's Module Linking holds them (reassess/autolink-meet-vasquez.md AM-13, AM-24): the stand-in box of a door
// (FUN_00447f10), then on stock levels the modules in the file's order with each door's stand-in after them, last door first. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_autolink_records_test.exe <level> [<level> ...] (tests/host_autolink_records_test.runargs). The levels are only read.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "autolink_records.h"

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

// The level's scale: the ENVSDSCL double inside REBENVDT, 1.0 when there is none
static double LevelScale(OpenLevelFile const &open)
{
	Chunk *const data = open.Level()->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = data && !data->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	Chunk *const scale = holder ? holder->lookup_single_child("ENVSDSCL") : nullptr;
	std::vector<unsigned char> bytes;
	double value = 1.0;

	if (scale && open.PayloadOf(scale, &bytes) && bytes.size() >= 8)
	{
		memcpy(&value, bytes.data(), 8);
	}

	return value > 0.0 ? value : 1.0;
}

// FUN_00447f10's box over extents (maxX, minX, maxY, minY, maxZ, minZ) = (10, -20, 30, -40, 50, -60)
static void TestBox()
{
	int32_t const door[6] = { 10, -20, 30, -40, 50, -60 };
	int32_t boxExtents[6] = { 0 };
	PortalShape box;

	AutoLink_StandInBox(door, &box, boxExtents);

	bool corners = box.points.size() == 24;

	for (int i = 0; i < 8 && corners; i++)
	{
		corners = box.points[(size_t)i * 3] == ((i & 1) ? 10 : -20) && box.points[(size_t)i * 3 + 1] == ((i & 2) ? 30 : -40) && box.points[(size_t)i * 3 + 2] == ((i & 4) ? 50 : -60);
	}

	Check(corners, "8 corners: point i takes each axis's maximum when its bit is set (x bit 0, y bit 1, z bit 2)");

	// every face's corners lie on its side, and its normal points out of the box
	bool faces = box.polygons.size() == 6 && box.normals.size() == 18;

	for (size_t f = 0; f < box.polygons.size() && faces; f++)
	{
		int axis = -1, sign = 0;

		for (int n = 0; n < 3; n++)
		{
			if (box.normals[f * 3 + (size_t)n] != 0)
			{
				axis = n;
				sign = box.normals[f * 3 + (size_t)n] > 0 ? 1 : -1;
			}
		}

		faces = axis >= 0 && box.polygons[f].size() == 4;

		for (int32_t const corner : box.polygons[f])
		{
			faces = faces && box.points[(size_t)corner * 3 + (size_t)axis] == (sign > 0 ? door[2 * axis] : door[2 * axis + 1]);
		}
	}

	Check(faces, "6 quads, each on its side of the box with a normal of +-65536 pointing out (-x, +x, -y, +y, -z, +z)");
	Check(boxExtents[0] == 11 && boxExtents[1] == -21 && boxExtents[2] == 31 && boxExtents[3] == -41 && boxExtents[4] == 51 && boxExtents[5] == -61,
		"its own extents: the door's widened by 1 on each side (0x44831b-0x448333)");
}

static void TestLevel(char const *path)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, std::string("the game's loader takes ") + path);
		return;
	}

	AutoLinkRecords records;

	AutoLink_Records(&level, LevelScale(level), &records);

	size_t const modules = records.scene.list.size();
	size_t doors = 0;

	for (size_t k = 0; k < modules; k++)
	{
		doors += records.isDoor[k] ? 1 : 0;
	}

	Check(modules > 0 && records.records.size() == modules + doors && records.objectIndexOf.size() == records.records.size() && records.standsFor.size() == records.records.size(),
		std::string(path) + ": " + std::to_string(modules) + " modules and " + std::to_string(doors) + " doors: " + std::to_string(records.records.size()) + " records");

	// the modules first, in the file's order (their object numbers as AutoLinkScene lists them), none a stand-in
	bool firstModules = true;

	for (size_t k = 0; k < modules; k++)
	{
		firstModules = firstModules && records.standsFor[k] == -1 && records.objectIndexOf[k] == records.scene.list[k].number;
	}

	Check(firstModules, "records 0 .. modules - 1: the modules in the file's order");

	// then the stand-ins, last door first, each at its door's place, standing for it
	bool standIns = true;
	int32_t previous = (int32_t)modules;

	for (size_t r = modules; r < records.records.size(); r++)
	{
		int32_t const door = records.standsFor[r];

		standIns = standIns && door >= 0 && door < previous && records.isDoor[(size_t)door] && records.objectIndexOf[r] == records.objectIndexOf[(size_t)door] &&
		           memcmp(records.records[r].place, records.records[(size_t)door].place, sizeof(records.records[r].place)) == 0 && records.records[r].shape &&
		           records.records[r].shape->polygons.size() == 6 && records.records[r].extents[0] == records.records[(size_t)door].extents[0] + 1;
		previous = door;
	}

	Check(standIns, "records modules .. : one stand-in per door, in descending order of the doors, at the door's place, a 6-face box 1 wider");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	TestBox();

	if (argc < 2)
	{
		printf("  FAILED  give stock levels (tests/host_autolink_records_test.runargs)\n");
		return 1;
	}

	for (int a = 1; a < argc; a++)
	{
		TestLevel(argv[a]);
	}

	if (checksFailed)
	{
		printf("FAILED host_autolink_records_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_records_test\n");
	return 0;
}
