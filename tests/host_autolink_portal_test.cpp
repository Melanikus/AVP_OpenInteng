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

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("host_autolink_portal_test: give a level .rif; with none there is nothing to run.\n");
		printf("passed host_autolink_portal_test (0 ok)\n");
		return 0;
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
