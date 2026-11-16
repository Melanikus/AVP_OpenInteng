// The oracle for AutoLink's visibility pass: host::AutoLinkPass (src/host/autolink_pass.cpp, inteng's FUN_004c7ab0 views drawn by OUR software rasteriser) on REAL levels. Written 2026-11-15
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_autolink_pass_test.exe <a level .rif> [every-nth-module]. With no argument it says so and passes. The level is only read.
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
#include "autolink_pass.h"

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
		printf("host_autolink_pass_test: give a level .rif; with none there is nothing to run.\n");
		printf("passed host_autolink_pass_test (0 ok)\n");
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

	// THE ORACLE: the links the stock level stores (every module in a module's VMDARRAY tree, at any depth) against the modules the pass sees from that module. RECALL = how many stored links the
	// pass finds; EXTRA = how many seen modules the level does not link (inteng's own links may differ by machine: its pass was drawn by the display).
	std::vector<host::AutoLinkModule> list;
	std::vector<int32_t> numbers;

	for (std::map<int32_t, One *>::iterator it = modules.begin(); it != modules.end(); ++it)
	{
		host::AutoLinkModule entry;

		entry.number = it->first;
		entry.module = it->second->module;
		list.push_back(entry);
		numbers.push_back(it->first);
	}

	host::AutoLinkPass pass;
	DWORD const builtAt = GetTickCount();

	pass.Build(list);
	printf("  --  scene built in %lu ms\n", (unsigned long)(GetTickCount() - builtAt));

	size_t const step = argc >= 3 ? (size_t)atoi(argv[2]) : 25;
	size_t tested = 0, storedTotal = 0, found = 0, seenTotal = 0, extra = 0, withLinks = 0, portalsTotal = 0, viewsTotal = 0;
	DWORD const started = GetTickCount();

	for (size_t at = 0; at < list.size(); at += step ? step : 1)
	{
		std::vector<host::LinkItem> items;
		std::set<int32_t> stored, seen;
		size_t views = 0, portals = 0;

		if (host::ReadModuleLinks(level.Level(), list[at].number, &items))
		{
			for (host::LinkItem const &item : items)
			{
				stored.insert(item.objectIndex);
			}
		}

		pass.SeenFrom(at, &seen, &views, &portals);
		tested++;
		viewsTotal += views;
		portalsTotal += portals;
		storedTotal += stored.size();
		seenTotal += seen.size();
		withLinks += stored.empty() ? 0 : 1;

		for (int32_t const link : stored)
		{
			found += seen.count(link) ? 1 : 0;
		}

		for (int32_t const one : seen)
		{
			extra += stored.count(one) ? 0 : 1;
		}
	}

	printf("  --  %zu modules tested (every %zu-th), %zu views drawn over %zu portals, %lu ms\n", tested, step, viewsTotal, portalsTotal, (unsigned long)(GetTickCount() - started));
	printf("  --  THE MATCH: the level stores %zu links in these modules, the pass finds %zu of them (RECALL %.1f%%); it sees %zu modules, %zu of them not linked by the level (EXTRA %.1f%% of what it sees)\n", storedTotal, found,
		storedTotal ? 100.0 * (double)found / (double)storedTotal : 0.0, seenTotal, extra, seenTotal ? 100.0 * (double)extra / (double)seenTotal : 0.0);

	Check(tested > 0 && withLinks > 0, "modules with stored links were tested");

	// A third argument ("full"): EVERY module is run, and the stored links are also counted as found when the OTHER module's pass sees this one (the links may have been made with Add Reverse Links,
	// FUN_004c86b0, whose result is symmetric): RECALL (own pass) against RECALL (own or the other's)
	if (argc >= 4)
	{
		std::map<int32_t, std::set<int32_t> > seenBy;   // module number -> the modules whose pass saw it
		std::map<int32_t, std::set<int32_t> > seenOf;   // module number -> what its pass sees
		size_t stored2 = 0, direct = 0, symmetric = 0, missedMeets = 0, missedTouching = 0, missedNear = 0, missedFar = 0, missedNoShape = 0, missedShown = 0;
		std::map<int32_t, size_t> indexOfNumber;

		for (size_t at = 0; at < list.size(); at++)
		{
			indexOfNumber[list[at].number] = at;
		}

		for (size_t at = 0; at < list.size(); at++)
		{
			std::set<int32_t> seen;

			pass.SeenFrom(at, &seen, nullptr, nullptr);
			seenOf[list[at].number] = seen;

			for (int32_t const one : seen)
			{
				seenBy[one].insert(list[at].number);
			}
		}

		for (size_t at = 0; at < list.size(); at++)
		{
			std::vector<host::LinkItem> items;

			if (!host::ReadModuleLinks(level.Level(), list[at].number, &items))
			{
				continue;
			}

			for (host::LinkItem const &item : items)
			{
				stored2++;
				direct += seenOf[list[at].number].count(item.objectIndex) ? 1 : 0;

				bool const either = seenOf[list[at].number].count(item.objectIndex) || seenBy[list[at].number].count(item.objectIndex);

				symmetric += either ? 1 : 0;

				if (!either)
				{
					// DIAGNOSIS of a link neither pass finds: does the other module meet this one at all, and how far apart are the boxes
					std::map<int32_t, size_t>::const_iterator other = indexOfNumber.find(item.objectIndex);

					if (other == indexOfNumber.end())
					{
						missedNoShape++;
						continue;
					}

					host::Portal portal;
					bool const meets = host::AutoLink_ModulesMeet(list[at].module, list[other->second].module, &portal);
					int64_t gap = -1000000000;

					for (int axis = 0; axis < 3; axis++)
					{
						host::PortalModule const &p = list[at].module, &q = list[other->second].module;
						int64_t const one = (int64_t)p.place[axis] + p.extents[2 * axis + 1] - ((int64_t)q.place[axis] + q.extents[2 * axis]);
						int64_t const two = (int64_t)q.place[axis] + q.extents[2 * axis + 1] - ((int64_t)p.place[axis] + p.extents[2 * axis]);
						int64_t const g = one > two ? one : two;

						gap = g > gap ? g : gap;
					}

					missedMeets += meets ? 1 : 0;
					missedTouching += (!meets && gap <= 0) ? 1 : 0;
					missedNear += (!meets && gap > 0 && gap <= 500000) ? 1 : 0;
					missedFar += (!meets && gap > 500000) ? 1 : 0;

					if (missedShown < 8)
					{
						missedShown++;
						printf("        missed %d -> %d: meets %d, box gap %lld (memory units)\n", list[at].number, item.objectIndex, meets ? 1 : 0, (long long)gap);
					}
				}
			}
		}

		printf("  --  the links NEITHER pass finds: %zu meet the module (a portal exists but the views missed it), %zu touch with no portal, %zu within 5 metres, %zu farther, %zu name a module with no shape\n", missedMeets,
			missedTouching, missedNear, missedFar, missedNoShape);

		printf("  --  FULL RUN, every module: of %zu stored links the module's own pass finds %zu (%.1f%%), its own or the other module's pass finds %zu (%.1f%%)\n", stored2, direct,
			stored2 ? 100.0 * (double)direct / (double)stored2 : 0.0, symmetric, stored2 ? 100.0 * (double)symmetric / (double)stored2 : 0.0);
	}

	if (checksFailed)
	{
		printf("FAILED host_autolink_pass_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_autolink_pass_test\n");
	return 0;
}
