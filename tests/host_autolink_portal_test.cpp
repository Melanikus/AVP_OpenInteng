// The oracle for the first step of AutoLink: host::AutoLink_ModulesMeet (src/host/autolink_portal.cpp, inteng's FUN_004c4750) on REAL levels. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_autolink_portal_test.exe <a level .rif>. With no argument it says so and passes. The level is only read.
// THE ORACLE: the levels' own ADJMDLEP chunks list, for each module, the modules that touch it (the old tools made them; the entry points between them were checked by entry_recalc). A pair
// the stock level calls adjacent should MEET by FUN_004c4750's box and wall test. The numbers printed are the match: of all adjacent pairs, how many meet; and of the pairs the
// level does NOT call adjacent (a sample), how many meet (the false positives). The test fails only on a hard fault (no module read, a crash); the match is a report, not a pass mark.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "entry_recalc.h"
#include "level_store.h"
#include "shape_edit.h"
#include "autolink_portal.h"

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

// A room-like box for the boundary checks: its 8 corners from the extents (maxX, minX, maxY, minY, maxZ, minZ), its faces turned INWARD (a module is seen from inside), the face on
// (openAxis, openSide) left out: the doorway. Added 2026-11-16 by Melanikus (Vasquez).
static void OpenBox(int32_t const extents[6], int openAxis, int openSide, host::PortalShape *shape)
{
	for (int i = 0; i < 8; i++)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			shape->points.push_back((i >> axis) & 1 ? extents[2 * axis] : extents[2 * axis + 1]);
		}
	}

	for (int axis = 0; axis < 3; axis++)
	{
		for (int side = 0; side < 2; side++)   // 1: the max side
		{
			if (axis == openAxis && side == openSide)
			{
				continue;
			}

			std::vector<int32_t> corners;

			for (int i = 0; i < 8; i++)
			{
				if (((i >> axis) & 1) == side)
				{
					corners.push_back(i);
				}
			}

			shape->polygons.push_back(corners);

			for (int n = 0; n < 3; n++)
			{
				shape->normals.push_back(n == axis ? (side ? -65536 : 65536) : 0);
			}
		}
	}
}

// THE NUMBERS OF autolink_rules.h AT THEIR EDGES (reassess/autolink-meet-vasquez.md AM-4): two open boxes 10 m long and 40 m wide meeting on x, A open on its +x side, B on its -x side
static void BoundaryChecks()
{
	int32_t const ext[6] = { 5000, -5000, 20000, -20000, 20000, -20000 };
	host::PortalShape aShape, bShape;

	OpenBox(ext, 0, 1, &aShape);
	OpenBox(ext, 0, 0, &bShape);

	auto const meet = [&](int32_t bx, int32_t by, host::Portal *portal)
	{
		host::PortalModule a, b;

		memset(&a, 0, sizeof(a));
		memset(&b, 0, sizeof(b));
		memcpy(a.extents, ext, sizeof(ext));
		memcpy(b.extents, ext, sizeof(ext));
		a.shape = &aShape;
		b.shape = &bShape;
		b.place[0] = bx;
		b.place[1] = by;
		return host::AutoLink_ModulesMeet(a, b, portal);
	};

	host::Portal p;

	Check(meet(10000, 0, &p) && p.axis == 0 && p.direction == 1 && p.lo[1] == -20000 && p.hi[1] == 20000 && p.lo[2] == -20000 && p.hi[2] == 20000,
		"boxes touching on x meet: the contact axis x, B on A's upper side, the portal the whole 40 m x 40 m side");
	Check(!meet(10000, 10001, &p), "the y overlap 29999: not wide (kMeetWide, `29999 < overlap`), one wide axis is too few (kMeetWideAxes 2): no meet");
	Check(meet(10000, 10000, &p) && p.lo[1] == -10000 && p.hi[1] == 20000, "the y overlap 30000: wide: they meet, the portal the overlap on y");
	Check(meet(11000, 0, &p) && p.planeA == 5000 && p.planeB == 6000, "1 m apart (the x overlap -1000, kMeetGap): they still meet, each module on its own contact plane");
	Check(!meet(11001, 0, &p), "1001 apart (the x overlap -1001): no meet");
}

int main(int argc, char **argv)
{
	BoundaryChecks();

	if (argc < 2)
	{
		printf("host_autolink_portal_test: give a level .rif for the oracle; with none only the boundary checks ran.\n");
		printf(checksFailed ? "FAILED host_autolink_portal_test (%d checks failed)\n" : "passed host_autolink_portal_test (%d failed)\n", checksFailed);
		return checksFailed ? 1 : 0;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	double const scale = LevelScale(level);
	std::set<int32_t> moduleNumbers;

	host::ModuleIndexes(level.Level(), &moduleNumbers);

	// Every module that has a shape the model reads: its place, extents, points, corners and normals
	struct One
	{
		host::PortalShape shape;
		host::PortalModule module;
	};

	std::map<int32_t, One *> modules;
	List<Object_Chunk *> all;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !moduleNumbers.count((int32_t)object->object_data.index_num))
		{
			continue;
		}

		model::ShapeModel shape;
		std::string why;

		if (host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		One *const one = new One();

		for (model::ShapePoint const &p : shape.points)
		{
			one->shape.points.push_back(host::AutoLinkPlace(p.x, scale));
			one->shape.points.push_back(host::AutoLinkPlace(p.y, scale));
			one->shape.points.push_back(host::AutoLinkPlace(p.z, scale));
		}

		for (size_t k = 0; k < shape.polygons.size(); k++)
		{
			std::vector<int32_t> corners;

			for (int c = 0; c < shape.polygons[k].cornerCount; c++)
			{
				corners.push_back(shape.polygons[k].corners[c]);
			}

			one->shape.polygons.push_back(corners);

			model::ShapeNormal const normal = k < shape.polygonNormals.size() ? shape.polygonNormals[k] : model::ShapeNormal();

			one->shape.normals.push_back(host::AutoLinkNormal(normal.x));
			one->shape.normals.push_back(host::AutoLinkNormal(normal.y));
			one->shape.normals.push_back(host::AutoLinkNormal(normal.z));
		}

		one->module.place[0] = host::AutoLinkPlace(object->object_data.location.x, scale);
		one->module.place[1] = host::AutoLinkPlace(object->object_data.location.y, scale);
		one->module.place[2] = host::AutoLinkPlace(object->object_data.location.z, scale);
		one->module.extents[0] = host::AutoLinkPlace(shape.extents.maxX, scale);
		one->module.extents[1] = host::AutoLinkPlace(shape.extents.minX, scale);
		one->module.extents[2] = host::AutoLinkPlace(shape.extents.maxY, scale);
		one->module.extents[3] = host::AutoLinkPlace(shape.extents.minY, scale);
		one->module.extents[4] = host::AutoLinkPlace(shape.extents.maxZ, scale);
		one->module.extents[5] = host::AutoLinkPlace(shape.extents.minZ, scale);
		one->module.shape = &one->shape;
		modules[(int32_t)object->object_data.index_num] = one;
	}

	Check(modules.size() > 0, "modules with a shape the model reads: " + std::to_string(modules.size()) + " of " + std::to_string(moduleNumbers.size()) + " (level scale " + std::to_string(scale) + ")");

	// the oracle: the adjacent pairs the level lists
	host::LevelStore store(&level);
	size_t adjacentPairs = 0, adjacentMeet = 0, adjacentNoShape = 0, notAdjacentTested = 0, notAdjacentMeet = 0;
	std::map<int32_t, std::set<int32_t> > adjacent;

	for (std::map<int32_t, One *>::iterator it = modules.begin(); it != modules.end(); ++it)
	{
		std::vector<int32_t> entries;

		host::AdjacentModulesOf(level.Level(), &store, it->first, &entries);

		for (int32_t const other : entries)
		{
			adjacent[it->first].insert(other);

			std::map<int32_t, One *>::iterator found = modules.find(other);

			if (found == modules.end())
			{
				adjacentNoShape++;
				continue;
			}

			host::Portal portal;

			adjacentPairs++;
			adjacentMeet += host::AutoLink_ModulesMeet(it->second->module, found->second->module, &portal) ? 1 : 0;

			// with a second argument the first few pairs are shown with every number the test uses (to tell a wrong unit from a wrong step)
			if (argc >= 3 && adjacentPairs <= 4)
			{
				host::PortalModule const &x = it->second->module;
				host::PortalModule const &y = found->second->module;

				printf("  --  pair %d / %d: A place %d %d %d ext %d %d %d %d %d %d | B place %d %d %d ext %d %d %d %d %d %d | file places A %d %d %d\n", it->first, other, x.place[0], x.place[1], x.place[2], x.extents[0],
					x.extents[1], x.extents[2], x.extents[3], x.extents[4], x.extents[5], y.place[0], y.place[1], y.place[2], y.extents[0], y.extents[1], y.extents[2], y.extents[3], y.extents[4],
					y.extents[5], 0, 0, 0);

				for (int i = 0; i < 3; i++)
				{
					int const top = (y.place[i] + y.extents[2 * i] <= x.place[i] + x.extents[2 * i]) ? y.place[i] + y.extents[2 * i] : x.place[i] + x.extents[2 * i];
					int const bottom = (y.place[i] + y.extents[2 * i + 1] < x.place[i] + x.extents[2 * i + 1]) ? x.place[i] + x.extents[2 * i + 1] : y.place[i] + y.extents[2 * i + 1];

					printf("        axis %d overlap %d\n", i, top - bottom);
				}
			}
		}
	}

	printf("  --  THE MATCH: of %zu adjacent pairs the level lists (%zu more name a module with no shape), %zu MEET by FUN_004c4750's test (%.1f%%)\n", adjacentPairs, adjacentNoShape, adjacentMeet,
		adjacentPairs ? 100.0 * (double)adjacentMeet / (double)adjacentPairs : 0.0);

	// the pairs the level does not list: every 7th pair of a sample, to count the false positives
	size_t counter = 0;

	for (std::map<int32_t, One *>::iterator a = modules.begin(); a != modules.end(); ++a)
	{
		for (std::map<int32_t, One *>::iterator b = modules.begin(); b != modules.end(); ++b)
		{
			if (a == b || adjacent[a->first].count(b->first) || adjacent[b->first].count(a->first) || (++counter % 7) != 0)
			{
				continue;
			}

			host::Portal portal;

			notAdjacentTested++;
			notAdjacentMeet += host::AutoLink_ModulesMeet(a->second->module, b->second->module, &portal) ? 1 : 0;
		}
	}

	printf("  --  pairs the level does not list as adjacent (a sample): %zu tested, %zu MEET (%.2f%%)\n", notAdjacentTested, notAdjacentMeet,
		notAdjacentTested ? 100.0 * (double)notAdjacentMeet / (double)notAdjacentTested : 0.0);

	Check(adjacentPairs > 0, "the level lists adjacent pairs to check against");

	if (checksFailed)
	{
		printf("FAILED host_autolink_portal_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_portal_test\n");
	return 0;
}
