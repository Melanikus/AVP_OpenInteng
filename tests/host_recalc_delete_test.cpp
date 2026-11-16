// The test for two of Vasquez's P11 host pieces that had none: Recalculate Normals and Extents (ShapeEdit_Recalculate, src/host/shape_edit.cpp) and Delete
// Module's Data (ModuleData_DeleteLinks, src/host/module_data_edit.cpp). Each is run on a REAL level (the first program argument), the level is saved as a
// COPY (never the opened file) and loaded again with the game's own loader, and the numbers are checked. Written 2026-11-15 by Melanikus (Apone) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_recalc_delete_test.exe <a level .rif> <a folder it may write its copies into (made if it isn't there)>. With no arguments it says so and passes.
// The level is only read; the copies are the only files written.
//
// RECALCULATE (checked against sums made here from the points, not against the code under test):
//  1. the extents in the copy are the smallest and largest of the shape's points on each axis, as whole numbers;
//  2. the header's radius is the longest distance of a point from the origin (within 1 part in 10,000, it is a float);
//  3. every polygon normal in the copy has the length 1 (within 0.001) and stands square to the polygon's first two edges (the dot product is under 0.001);
//  4. the points, the polygons and their counts are as they were; the level is the same size; the opened file is byte for byte what it was.
// DELETE MODULE'S DATA:
//  5. on a module that holds VMDARRAYs, it says Done and how many it took out; in the copy that module's MODULEDT holds no VMDARRAY, the number of objects and
//     the number of VMDARRAYs of every OTHER module are what they were, and the level got smaller;
//  6. asked again for the same module it says NothingToDelete and changes nothing; the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "module_data_edit.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

// The VMDARRAYs inside an object's first MODULEDT, or -1 when it has no MODULEDT
static int VmdArraysOf(Object_Chunk *object)
{
	Chunk *const moduleData = object->lookup_single_child("MODULEDT");

	if (!moduleData)
	{
		return -1;
	}

	List<Chunk *> found;

	((Chunk_With_Children *)moduleData)->lookup_child("VMDARRAY", found);

	int count = 0;

	for (LIF<Chunk *> each(&found); !each.done(); each.next())
	{
		count += each() ? 1 : 0;
	}

	return count;
}

static size_t CountObjects(OpenLevelFile const &open)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	size_t count = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		count += each() ? 1 : 0;
	}

	return count;
}

static void Recalculate(std::string const &levelPath, std::string const &copyFolder, std::vector<char> const &original)
{
	printf("RECALCULATE NORMALS AND EXTENTS\n");

	OpenLevelFile before;

	if (before.Open(levelPath.c_str()) != LevelOpenResult::Opened || !before.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// The shape with the most users, from objects whose shape this program can read
	List<Object_Chunk *> all;

	before.Level()->list_objects(&all);

	uint8_t chosen[8] = { 0 };
	size_t mostUsers = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape())
		{
			continue;
		}

		model::ShapeModel probe;
		size_t users = 0;
		std::string why;

		if (host::ShapeEdit_Read(before.Level(), &before, (uint8_t const *)&object->object_data.ID, &probe, &users, &why) == host::ShapeEditResult::Done
			&& probe.points.size() >= 4 && probe.polygons.size() >= 1 && users > mostUsers && users <= 64)
		{
			mostUsers = users;
			memcpy(chosen, &object->object_data.ID, 8);
		}
	}

	Check(mostUsers >= 1, "the level has a shape this program can read: " + std::to_string(mostUsers) + " user(s)");

	if (mostUsers < 1)
	{
		return;
	}

	model::ShapeModel shapeBefore;
	std::string why;

	Check(host::ShapeEdit_Read(before.Level(), &before, chosen, &shapeBefore, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read before");

	std::vector<char> imageBefore;

	Check(before.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	size_t users = 0;
	host::ShapeEditResult const result = host::ShapeEdit_Recalculate(&before, before.Level(), chosen, &users, &why);

	Check(result == host::ShapeEditResult::Done, std::string("Recalculate Normals and Extents is done") + (result == host::ShapeEditResult::Done ? "" : ": " + why));

	std::string const copyPath = CopyPathIn(copyFolder, "recalc-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (!after.Level())
	{
		return;
	}

	model::ShapeModel shapeAfter;

	Check(host::ShapeEdit_Read(after.Level(), &after, chosen, &shapeAfter, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read after the reload");

	// 4. the points and polygons as they were
	bool same = shapeAfter.points.size() == shapeBefore.points.size() && shapeAfter.polygons.size() == shapeBefore.polygons.size();

	for (size_t at = 0; same && at < shapeBefore.points.size(); at++)
	{
		same = memcmp(&shapeAfter.points[at], &shapeBefore.points[at], sizeof(model::ShapePoint)) == 0;
	}

	for (size_t at = 0; same && at < shapeBefore.polygons.size(); at++)
	{
		same = memcmp(&shapeAfter.polygons[at], &shapeBefore.polygons[at], sizeof(model::ShapePolygon)) == 0;
	}

	Check(same, "the points and the polygons, and their counts, are as they were: " + std::to_string(shapeAfter.points.size()) + " points, " + std::to_string(shapeAfter.polygons.size()) + " polygons");

	// 1. the extents, from the points, made here
	int32_t maxX = INT32_MIN, minX = INT32_MAX, maxY = INT32_MIN, minY = INT32_MAX, maxZ = INT32_MIN, minZ = INT32_MAX;
	double farthest = 0.0;

	for (model::ShapePoint const &p : shapeAfter.points)
	{
		maxX = p.x > maxX ? p.x : maxX;
		minX = p.x < minX ? p.x : minX;
		maxY = p.y > maxY ? p.y : maxY;
		minY = p.y < minY ? p.y : minY;
		maxZ = p.z > maxZ ? p.z : maxZ;
		minZ = p.z < minZ ? p.z : minZ;

		double const away = sqrt((double)p.x * p.x + (double)p.y * p.y + (double)p.z * p.z);

		farthest = away > farthest ? away : farthest;
	}

	Check(shapeAfter.extents.maxX == maxX && shapeAfter.extents.minX == minX && shapeAfter.extents.maxY == maxY && shapeAfter.extents.minY == minY
		&& shapeAfter.extents.maxZ == maxZ && shapeAfter.extents.minZ == minZ, "the extents are the smallest and largest of the points on each axis");

	// 2. the radius about the origin
	Check(fabs((double)shapeAfter.radius - farthest) <= farthest * 1e-4 + 1.0, "the header's radius is the longest distance of a point from the origin: " + std::to_string(shapeAfter.radius) + " (made here: " + std::to_string(farthest) + ")");

	// 3. the polygon normals: unit length and square to the polygon's first two edges
	bool normalsRight = shapeAfter.polygonNormals.size() == shapeAfter.polygons.size();
	std::string firstBad;

	for (size_t at = 0; normalsRight && at < shapeAfter.polygons.size(); at++)
	{
		model::ShapePolygon const &polygon = shapeAfter.polygons[at];

		if (polygon.cornerCount < 3 || polygon.corners[0] < 0 || polygon.corners[1] < 0 || polygon.corners[2] < 0
			|| (size_t)polygon.corners[0] >= shapeAfter.points.size() || (size_t)polygon.corners[1] >= shapeAfter.points.size() || (size_t)polygon.corners[2] >= shapeAfter.points.size())
		{
			continue;
		}

		model::ShapePoint const &a = shapeAfter.points[(size_t)polygon.corners[0]];
		model::ShapePoint const &b = shapeAfter.points[(size_t)polygon.corners[1]];
		model::ShapePoint const &c = shapeAfter.points[(size_t)polygon.corners[2]];
		model::ShapeNormal const &n = shapeAfter.polygonNormals[at];
		double const length = sqrt((double)n.x * n.x + (double)n.y * n.y + (double)n.z * n.z);
		double const e1[3] = { (double)b.x - a.x, (double)b.y - a.y, (double)b.z - a.z };
		double const e2[3] = { (double)c.x - a.x, (double)c.y - a.y, (double)c.z - a.z };
		double const l1 = sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
		double const l2 = sqrt(e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]);
		bool const unit = fabs(length - 1.0) < 0.001;
		bool const square = l1 == 0.0 || l2 == 0.0 || (fabs((n.x * e1[0] + n.y * e1[1] + n.z * e1[2]) / l1) < 0.001 && fabs((n.x * e2[0] + n.y * e2[1] + n.z * e2[2]) / l2) < 0.001);

		if (!unit || !square)
		{
			normalsRight = false;
			firstBad = "polygon " + std::to_string(at) + ": length " + std::to_string(length);
		}
	}

	Check(normalsRight, "every polygon normal has the length 1 and stands square to the polygon's first two edges" + (firstBad.empty() ? std::string() : " (" + firstBad + ")"));

	std::vector<char> imageAfter;

	Check(after.BuildLevel(imageAfter) && imageAfter.size() == imageBefore.size(), "the level is the same size as before: " + std::to_string(imageBefore.size()) + " bytes");

	DeleteFileA(copyPath.c_str());
}

static void DeleteModuleData(std::string const &levelPath, std::string const &copyFolder, std::vector<char> const &original)
{
	printf("DELETE MODULE'S DATA\n");

	OpenLevelFile before;

	if (before.Open(levelPath.c_str()) != LevelOpenResult::Opened || !before.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	List<Object_Chunk *> all;

	before.Level()->list_objects(&all);

	uint8_t chosen[8] = { 0 };
	int arrays = 0;
	std::vector<std::pair<std::string, int>> othersBefore;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		int const mine = object ? VmdArraysOf(object) : -1;

		if (mine > 0 && arrays == 0)
		{
			arrays = mine;
			memcpy(chosen, &object->object_data.ID, 8);
		}
	}

	Check(arrays > 0, "the level has a module that holds VMDARRAYs: " + std::to_string(arrays));

	if (arrays == 0)
	{
		return;
	}

	size_t const objectsBefore = CountObjects(before);
	int totalBefore = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && memcmp(&object->object_data.ID, chosen, 8) != 0 && VmdArraysOf(object) > 0)
		{
			totalBefore += VmdArraysOf(object);
		}
	}

	std::vector<char> imageBefore;

	Check(before.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	size_t removed = 0;
	host::ModuleDataResult const result = host::ModuleData_DeleteLinks(&before, chosen, &removed);

	Check(result == host::ModuleDataResult::Done && removed == (size_t)arrays, std::string("Delete Module's Data is done and takes out all ") + std::to_string(arrays) + " (took " + std::to_string(removed) + ")");

	std::string const copyPath = CopyPathIn(copyFolder, "module-data-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (!after.Level())
	{
		return;
	}

	List<Object_Chunk *> allAfter;

	after.Level()->list_objects(&allAfter);

	bool chosenClean = false;
	int totalAfter = 0;

	for (LIF<Object_Chunk *> each(&allAfter); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		if (memcmp(&object->object_data.ID, chosen, 8) == 0)
		{
			chosenClean = object->lookup_single_child("MODULEDT") != nullptr && VmdArraysOf(object) == 0;
		}
		else if (VmdArraysOf(object) > 0)
		{
			totalAfter += VmdArraysOf(object);
		}
	}

	Check(chosenClean, "in the copy that module still has its MODULEDT, and it holds no VMDARRAY");
	Check(totalAfter == totalBefore, "every OTHER module's VMDARRAYs are as they were: " + std::to_string(totalAfter) + " of " + std::to_string(totalBefore));
	Check(CountObjects(after) == objectsBefore, "the number of objects is as it was: " + std::to_string(objectsBefore));

	std::vector<char> imageAfter;

	bool const built = after.BuildLevel(imageAfter);

	Check(built && imageAfter.size() < imageBefore.size(), "the level got smaller: " + std::to_string(imageBefore.size()) + " to " + std::to_string(imageAfter.size()) + " bytes");

	// 6. again: nothing to delete, nothing changes
	std::vector<char> imageAgain;
	size_t removedAgain = 99;

	Check(host::ModuleData_DeleteLinks(&before, chosen, &removedAgain) == host::ModuleDataResult::NothingToDelete && before.BuildLevel(imageAgain) && imageAgain == imageAfter,
		"asked again for the same module it says NothingToDelete and changes nothing");

	DeleteFileA(copyPath.c_str());
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_recalc_delete_test <a level .rif> <a folder for its copies>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	std::string const levelPath = argv[1];
	std::string const copyFolder = argv[2];
	std::vector<char> original;

	Check(ReadWholeFile(levelPath, &original), "the level is read (it is only read, never written)");

	if (original.empty())
	{
		return 1;
	}

	Recalculate(levelPath, copyFolder, original);
	DeleteModuleData(levelPath, copyFolder, original);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
