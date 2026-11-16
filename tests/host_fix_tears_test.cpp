// The test for Fix tears (ShapeEdit_FixTears, src/host/shape_edit.cpp; the weld of command 0x5a, FUN_0044ad20), written by someone who did not write it. It
// runs on a REAL level (the first program argument): the level is opened with the game's loader, two objects are chosen by this program, the weld is done,
// the level is saved as a COPY (never the opened file) and loaded again with the game's loader, and the numbers are checked against sums made HERE from
// the rules (the rule: a point of the first object's shape whose world place, its point plus its object's place, is within the tolerance on every axis of
// a point of the second object's shape, the first such point in order, goes to that point's world place). Written 2026-11-15 by Melanikus (Apone) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_fix_tears_test.exe <a level .rif> <a folder it may write its copies into (made if it isn't there)>. With no arguments it says so and passes.
// The level is only read; the copies are the only files written.
//
// THE WELD (two objects of the level that this program finds: shapes of their own, no morph, some points within the tolerance and some not):
//  1. it says Done and how many points it moved, and that number is the one made here;
//  2. in the copy, every point of the FIRST object's shape that has a partner within the tolerance is at the same world place as its partner, and every
//     other point has not moved; at least one point did move;
//  3. the numbers of points and polygons, the polygons themselves and the level's size are as they were;
//  4. the first shape's normals, extents and radius are made again: the extents are the smallest and largest of the points, the radius is the longest
//     distance of a point from the origin, every polygon normal has the length 1 and stands square to the polygon's first two edges;
//  5. the SECOND object's shape is as it was, and so is every other shape, and every object's place;
//  6. the opened file is byte for byte what it was.
// THE REFUSALS (each on its own freshly opened level: the answer is Refused and the level as the next save would write it is byte for byte what it was):
//  7. the shape to be moved is used by other objects too;
//  8. the first object's shape has a SHPMORPH; the second object's shape has a SHPMORPH (when the level has a shape with one);
//  9. the two objects use the same shape; and the same object twice.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"

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

// What this program knows of one object that has a shape
struct Candidate
{
	uint8_t id[8];
	model::ShapeModel shape;
	size_t users = 0;
	bool morph = false;
	model::ShapePoint place = { 0, 0, 0 };
	Shape_Chunk *shapeChunk = nullptr;
};

static std::vector<Candidate> Candidates(OpenLevelFile &open)
{
	std::vector<Candidate> found;
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape())
		{
			continue;
		}

		Candidate candidate;
		std::string why;

		memcpy(candidate.id, &object->object_data.ID, 8);

		if (host::ShapeEdit_Read(open.Level(), &open, candidate.id, &candidate.shape, &candidate.users, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		candidate.shapeChunk = object->get_assoc_shape();
		candidate.morph = candidate.shapeChunk->lookup_single_child("SHPMORPH") != nullptr;
		candidate.place.x = object->object_data.location.x;
		candidate.place.y = object->object_data.location.y;
		candidate.place.z = object->object_data.location.z;
		found.push_back(candidate);
	}

	return found;
}

// The rule, made here: for each point of `moved`, in order, the first point of `reference` whose world place is within the tolerance on every axis
// (bounds included) is its partner; a point with a partner goes to the partner's world place.
static size_t ExpectedWeld(Candidate const &moved, Candidate const &reference, int32_t tolerance, std::vector<model::ShapePoint> *expected, std::vector<int> *partner)
{
	size_t count = 0;

	*expected = moved.shape.points;
	partner->assign(moved.shape.points.size(), -1);

	for (size_t at = 0; at < moved.shape.points.size(); at++)
	{
		int64_t const wx = (int64_t)moved.shape.points[at].x + moved.place.x;
		int64_t const wy = (int64_t)moved.shape.points[at].y + moved.place.y;
		int64_t const wz = (int64_t)moved.shape.points[at].z + moved.place.z;

		for (size_t other = 0; other < reference.shape.points.size(); other++)
		{
			int64_t const dx = (int64_t)reference.shape.points[other].x + reference.place.x - wx;
			int64_t const dy = (int64_t)reference.shape.points[other].y + reference.place.y - wy;
			int64_t const dz = (int64_t)reference.shape.points[other].z + reference.place.z - wz;

			if (dx >= -tolerance && dx <= tolerance && dy >= -tolerance && dy <= tolerance && dz >= -tolerance && dz <= tolerance)
			{
				(*expected)[at].x = (int32_t)(wx + dx - moved.place.x);
				(*expected)[at].y = (int32_t)(wy + dy - moved.place.y);
				(*expected)[at].z = (int32_t)(wz + dz - moved.place.z);
				(*partner)[at] = (int)other;
				count++;
				break;
			}
		}
	}

	return count;
}

// Every polygon normal has the length 1 (within 0.001) and stands square to the polygon's first two edges (the dot product with each unit edge is under 0.001)
// A polygon whose first two edges have no area between them (a weld that puts two corners at one place makes these) has no direction to be square to: it is
// left out and counted in *flat.
static bool NormalsAreRight(std::vector<model::ShapePoint> const &points, std::vector<model::ShapePolygon> const &polygons, std::vector<model::ShapeNormal> const &normals, std::string *firstBad,
	size_t *flat)
{
	*flat = 0;

	if (normals.size() != polygons.size())
	{
		*firstBad = "the number of normals is not the number of polygons";
		return false;
	}

	for (size_t at = 0; at < polygons.size(); at++)
	{
		model::ShapePolygon const &polygon = polygons[at];

		if (polygon.cornerCount < 3 || polygon.corners[0] < 0 || polygon.corners[1] < 0 || polygon.corners[2] < 0 || (size_t)polygon.corners[0] >= points.size()
			|| (size_t)polygon.corners[1] >= points.size() || (size_t)polygon.corners[2] >= points.size())
		{
			continue;
		}

		model::ShapePoint const &a = points[(size_t)polygon.corners[0]];
		model::ShapePoint const &b = points[(size_t)polygon.corners[1]];
		model::ShapePoint const &c = points[(size_t)polygon.corners[2]];
		model::ShapeNormal const &n = normals[at];
		double const length = sqrt((double)n.x * n.x + (double)n.y * n.y + (double)n.z * n.z);
		double const e1[3] = { (double)b.x - a.x, (double)b.y - a.y, (double)b.z - a.z };
		double const e2[3] = { (double)c.x - a.x, (double)c.y - a.y, (double)c.z - a.z };
		double const l1 = sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
		double const l2 = sqrt(e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]);
		double const cross[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };

		if (cross[0] == 0.0 && cross[1] == 0.0 && cross[2] == 0.0)
		{
			(*flat)++;
			continue;
		}

		bool const unit = fabs(length - 1.0) < 0.001;
		bool const square = l1 == 0.0 || l2 == 0.0 || (fabs((n.x * e1[0] + n.y * e1[1] + n.z * e1[2]) / l1) < 0.001 && fabs((n.x * e2[0] + n.y * e2[1] + n.z * e2[2]) / l2) < 0.001);

		if (!unit || !square)
		{
			*firstBad = "polygon " + std::to_string(at) + ": length " + std::to_string(length);
			return false;
		}
	}

	return true;
}

// The extents of some points, made here
static model::ShapeExtents ExtentsOfPoints(std::vector<model::ShapePoint> const &points)
{
	model::ShapeExtents e = { INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX };

	for (model::ShapePoint const &p : points)
	{
		e.maxX = p.x > e.maxX ? p.x : e.maxX;
		e.minX = p.x < e.minX ? p.x : e.minX;
		e.maxY = p.y > e.maxY ? p.y : e.maxY;
		e.minY = p.y < e.minY ? p.y : e.minY;
		e.maxZ = p.z > e.maxZ ? p.z : e.maxZ;
		e.minZ = p.z < e.minZ ? p.z : e.minZ;
	}

	return e;
}

static bool ShapesAreTheSame(model::ShapeModel const &a, model::ShapeModel const &b)
{
	if (a.points.size() != b.points.size() || a.polygons.size() != b.polygons.size() || a.polygonNormals.size() != b.polygonNormals.size()
		|| a.pointNormals.size() != b.pointNormals.size() || memcmp(&a.extents, &b.extents, sizeof(a.extents)) != 0 || memcmp(&a.radius, &b.radius, sizeof(a.radius)) != 0)
	{
		return false;
	}

	return (a.points.empty() || memcmp(a.points.data(), b.points.data(), a.points.size() * sizeof(model::ShapePoint)) == 0)
		&& (a.polygons.empty() || memcmp(a.polygons.data(), b.polygons.data(), a.polygons.size() * sizeof(model::ShapePolygon)) == 0)
		&& (a.polygonNormals.empty() || memcmp(a.polygonNormals.data(), b.polygonNormals.data(), a.polygonNormals.size() * sizeof(model::ShapeNormal)) == 0)
		&& (a.pointNormals.empty() || memcmp(a.pointNormals.data(), b.pointNormals.data(), a.pointNormals.size() * sizeof(model::ShapeNormal)) == 0);
}

// Looks for two objects to weld: shapes of their own, no morph, and with the tolerance at least one point of the first has a partner that moves it and
// at least one point has none
static bool FindPair(std::vector<Candidate> const &all, int32_t tolerance, size_t *movedAt, size_t *referenceAt, size_t *expectedCount)
{
	for (size_t m = 0; m < all.size(); m++)
	{
		Candidate const &moved = all[m];

		if (moved.users != 1 || moved.morph || moved.shape.points.size() < 4 || moved.shape.points.size() > 4000)
		{
			continue;
		}

		for (size_t r = 0; r < all.size(); r++)
		{
			Candidate const &reference = all[r];

			if (r == m || reference.users != 1 || reference.morph || reference.shapeChunk == moved.shapeChunk || reference.shape.points.empty() || reference.shape.points.size() > 4000)
			{
				continue;
			}

			// a quick no: the boxes of the two shapes, in the world, do not come within the tolerance of each other
			int64_t const grow = tolerance;

			if ((int64_t)moved.shape.extents.minX + moved.place.x - grow > (int64_t)reference.shape.extents.maxX + reference.place.x
				|| (int64_t)reference.shape.extents.minX + reference.place.x - grow > (int64_t)moved.shape.extents.maxX + moved.place.x
				|| (int64_t)moved.shape.extents.minY + moved.place.y - grow > (int64_t)reference.shape.extents.maxY + reference.place.y
				|| (int64_t)reference.shape.extents.minY + reference.place.y - grow > (int64_t)moved.shape.extents.maxY + moved.place.y
				|| (int64_t)moved.shape.extents.minZ + moved.place.z - grow > (int64_t)reference.shape.extents.maxZ + reference.place.z
				|| (int64_t)reference.shape.extents.minZ + reference.place.z - grow > (int64_t)moved.shape.extents.maxZ + moved.place.z)
			{
				continue;
			}

			std::vector<model::ShapePoint> expected;
			std::vector<int> partner;
			size_t const count = ExpectedWeld(moved, reference, tolerance, &expected, &partner);
			size_t differ = 0;

			for (size_t at = 0; at < expected.size(); at++)
			{
				differ += memcmp(&expected[at], &moved.shape.points[at], sizeof(model::ShapePoint)) != 0 ? 1 : 0;
			}

			std::string notUsed;
			model::ShapeExtents const newExtents = ExtentsOfPoints(expected);
			bool const extentsMove = memcmp(&newExtents, &moved.shape.extents, sizeof(newExtents)) != 0;
			size_t flatNotUsed = 0;
			bool const normalsGoStale = !NormalsAreRight(expected, moved.shape.polygons, moved.shape.polygonNormals, &notUsed, &flatNotUsed);

			if (differ >= 1 && count < moved.shape.points.size() && extentsMove && normalsGoStale)
			{
				*movedAt = m;
				*referenceAt = r;
				*expectedCount = count;
				return true;
			}
		}
	}

	return false;
}

static void Weld(std::string const &levelPath, std::string const &copyFolder, std::vector<char> const &original)
{
	printf("THE WELD\n");

	OpenLevelFile before;

	if (before.Open(levelPath.c_str()) != LevelOpenResult::Opened || !before.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	std::vector<Candidate> const all = Candidates(before);
	size_t movedAt = 0, referenceAt = 0, expectedCount = 0;
	int32_t tolerance = 0;
	bool found = false;
	int32_t const tries[] = { 20, 100, 500, 2000, 8000 };

	for (int32_t const attempt : tries)
	{
		if (FindPair(all, attempt, &movedAt, &referenceAt, &expectedCount))
		{
			tolerance = attempt;
			found = true;
			break;
		}
	}

	Check(found, "the level has two objects with shapes of their own, no morph, some points within the tolerance of the other's and some not, and the weld changes the extents and makes the stored normals wrong, so a missing rebuild shows: " + std::to_string(all.size()) + " objects looked at");

	if (!found)
	{
		return;
	}

	Candidate const &moved = all[movedAt];
	Candidate const &reference = all[referenceAt];
	std::vector<model::ShapePoint> expected;
	std::vector<int> partner;
	size_t const expectedAgain = ExpectedWeld(moved, reference, tolerance, &expected, &partner);

	printf("  (the pair: %zu points to move, %zu points of the other, tolerance %d, %zu of the first have a partner)\n", moved.shape.points.size(),
		reference.shape.points.size(), (int)tolerance, expectedAgain);

	std::vector<char> imageBefore;

	Check(before.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	size_t welded = 999999;
	std::string why;
	host::ShapeEditResult const result = host::ShapeEdit_FixTears(&before, before.Level(), moved.id, reference.id, tolerance, &welded, &why);

	Check(result == host::ShapeEditResult::Done, std::string("Fix tears is done") + (result == host::ShapeEditResult::Done ? "" : ": " + why));
	Check(welded == expectedCount, "it says it moved " + std::to_string(welded) + " points; made here: " + std::to_string(expectedCount));

	std::string const copyPath = CopyPathIn(copyFolder, "fix-tears-test-copy.rif");

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

	std::vector<Candidate> const allAfter = Candidates(after);

	Check(allAfter.size() == all.size(), "the same number of objects with a shape: " + std::to_string(allAfter.size()));

	if (allAfter.size() != all.size())
	{
		return;
	}

	model::ShapeModel const &shape = allAfter[movedAt].shape;

	// 3. counts, polygons, size
	bool polygonsSame = shape.polygons.size() == moved.shape.polygons.size() && shape.points.size() == moved.shape.points.size();

	for (size_t at = 0; polygonsSame && at < moved.shape.polygons.size(); at++)
	{
		polygonsSame = memcmp(&shape.polygons[at], &moved.shape.polygons[at], sizeof(model::ShapePolygon)) == 0;
	}

	Check(polygonsSame, "the numbers of points and polygons, and the polygons, are as they were: " + std::to_string(shape.points.size()) + " points, " + std::to_string(shape.polygons.size()) + " polygons");

	std::vector<char> imageAfter;

	Check(after.BuildLevel(imageAfter) && imageAfter.size() == imageBefore.size(), "the level is the same size as before: " + std::to_string(imageBefore.size()) + " bytes");

	if (!polygonsSame)
	{
		return;
	}

	// 2. the points: a partner means the same world place, none means not moved
	size_t atTheSamePlace = 0, notMovedRight = 0, wrong = 0, didMove = 0;
	std::string firstWrong;

	for (size_t at = 0; at < shape.points.size(); at++)
	{
		model::ShapePoint const &p = shape.points[at];
		model::ShapePoint const &was = moved.shape.points[at];

		didMove += memcmp(&p, &was, sizeof(p)) != 0 ? 1 : 0;

		if (partner[at] >= 0)
		{
			model::ShapePoint const &q = reference.shape.points[(size_t)partner[at]];
			bool const here = (int64_t)p.x + moved.place.x == (int64_t)q.x + reference.place.x && (int64_t)p.y + moved.place.y == (int64_t)q.y + reference.place.y
				&& (int64_t)p.z + moved.place.z == (int64_t)q.z + reference.place.z;

			atTheSamePlace += here ? 1 : 0;

			if (!here)
			{
				wrong++;
				firstWrong = firstWrong.empty() ? "point " + std::to_string(at) + " has a partner and is not at its world place" : firstWrong;
			}
		}
		else if (memcmp(&p, &was, sizeof(p)) == 0)
		{
			notMovedRight++;
		}
		else
		{
			wrong++;
			firstWrong = firstWrong.empty() ? "point " + std::to_string(at) + " has no partner and moved" : firstWrong;
		}
	}

	Check(wrong == 0 && atTheSamePlace == expectedCount && atTheSamePlace + notMovedRight == shape.points.size(),
		"every point with a partner is at its partner's world place (" + std::to_string(atTheSamePlace) + ") and every other point has not moved (" + std::to_string(notMovedRight) + ")" + (firstWrong.empty() ? std::string() : ": " + firstWrong));
	Check(didMove >= 1, "at least one point moved: " + std::to_string(didMove));

	// 4. the normals, extents and radius, made again
	int32_t maxX = INT32_MIN, minX = INT32_MAX, maxY = INT32_MIN, minY = INT32_MAX, maxZ = INT32_MIN, minZ = INT32_MAX;
	double farthest = 0.0;

	for (model::ShapePoint const &p : shape.points)
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

	Check(shape.extents.maxX == maxX && shape.extents.minX == minX && shape.extents.maxY == maxY && shape.extents.minY == minY && shape.extents.maxZ == maxZ
		&& shape.extents.minZ == minZ, "the extents are the smallest and largest of the points on each axis");
	Check(fabs((double)shape.radius - farthest) <= farthest * 1e-4 + 1.0, "the header's radius is the longest distance of a point from the origin: " + std::to_string(shape.radius) + " (made here: " + std::to_string(farthest) + ")");

	std::string firstBad;
	size_t flat = 0;
	bool const normalsRight = NormalsAreRight(shape.points, shape.polygons, shape.polygonNormals, &firstBad, &flat);

	Check(normalsRight, "every polygon normal has the length 1 and stands square to the polygon's first two edges (" + std::to_string(flat) + " polygons with no area left out)" + (firstBad.empty() ? std::string() : " (" + firstBad + ")"));

	// 5. the other shape and every other shape, and every place
	Check(ShapesAreTheSame(allAfter[referenceAt].shape, reference.shape), "the SECOND object's shape is as it was");

	size_t otherChanged = 0, placeChanged = 0;

	for (size_t at = 0; at < all.size(); at++)
	{
		if (at != movedAt && !ShapesAreTheSame(all[at].shape, allAfter[at].shape))
		{
			otherChanged++;
		}

		if (memcmp(&all[at].place, &allAfter[at].place, sizeof(all[at].place)) != 0)
		{
			placeChanged++;
		}
	}

	Check(otherChanged == 0, "every other shape is as it was: " + std::to_string(otherChanged) + " changed");
	Check(placeChanged == 0, "every object's place is as it was: " + std::to_string(placeChanged) + " changed");

	DeleteFileA(copyPath.c_str());
}

// One refusal, on a freshly opened level: the answer and the level's image
static void Refusal(std::string const &levelPath, uint8_t const *movedId, uint8_t const *referenceId, std::string const &what)
{
	OpenLevelFile level;

	if (level.Open(levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, what + ": the game's loader takes the level");
		return;
	}

	std::vector<char> imageBefore;
	std::vector<char> imageAfter;
	size_t welded = 999999;
	std::string why;

	if (!level.BuildLevel(imageBefore))
	{
		Check(false, what + ": the level as the next save would write it is made");
		return;
	}

	host::ShapeEditResult const result = host::ShapeEdit_FixTears(&level, level.Level(), movedId, referenceId, 500, &welded, &why);
	bool const built = level.BuildLevel(imageAfter);

	Check(result == host::ShapeEditResult::Refused && welded == 0 && built && imageAfter == imageBefore, what + ": refused (" + why + "), nothing moved, the level is byte for byte as it was");
}

static void Refusals(std::string const &levelPath)
{
	printf("THE REFUSALS\n");

	OpenLevelFile open;

	if (open.Open(levelPath.c_str()) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	std::vector<Candidate> const all = Candidates(open);
	size_t plain = all.size();   // an object with a shape of its own and no morph
	size_t shared = all.size();  // an object whose shape other objects use too (no morph)
	size_t sharedPartner = all.size();  // another object of that same shape
	size_t morph = all.size();   // an object whose shape has a morph

	for (size_t at = 0; at < all.size(); at++)
	{
		if (all[at].users == 1 && !all[at].morph && plain == all.size())
		{
			plain = at;
		}

		if (all[at].users > 1 && !all[at].morph && shared == all.size())
		{
			shared = at;
		}

		if (all[at].morph && morph == all.size())
		{
			morph = at;
		}
	}

	for (size_t at = 0; shared != all.size() && at < all.size(); at++)
	{
		if (at != shared && all[at].shapeChunk == all[shared].shapeChunk)
		{
			sharedPartner = at;
			break;
		}
	}

	Check(plain != all.size(), "the level has an object with a shape of its own");

	if (plain == all.size())
	{
		return;
	}

	// 7. the shape to be moved is shared
	if (shared != all.size())
	{
		Refusal(levelPath, all[shared].id, all[plain].id, "the shape to be moved is used by other objects too");
	}
	else
	{
		Check(false, "the level has an object whose shape other objects use too");
	}

	// 8. a morph
	if (morph != all.size())
	{
		Refusal(levelPath, all[morph].id, all[plain].id, "the first object's shape has a SHPMORPH");
		Refusal(levelPath, all[plain].id, all[morph].id, "the second object's shape has a SHPMORPH");
	}
	else
	{
		printf("  NOT TESTED  this level has no shape with a SHPMORPH, so those two refusals were not tried on it (name a level that has one)\n");
	}

	// 9. the same shape twice, and the same object twice
	if (sharedPartner != all.size())
	{
		Refusal(levelPath, all[shared].id, all[sharedPartner].id, "two different objects that use the same shape");
	}
	else
	{
		Check(false, "the level has two objects that use the same shape");
	}

	Refusal(levelPath, all[plain].id, all[plain].id, "the same object twice");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_fix_tears_test <a level .rif> <a folder for its copies>: no level was named, so nothing was tested here\n");
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

	Weld(levelPath, copyFolder, original);
	Refusals(levelPath);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
