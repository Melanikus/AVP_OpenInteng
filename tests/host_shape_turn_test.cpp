// The test for ShapeEdit_TurnQuarter (src/host/shape_edit.cpp, Construction's R "Rotate object 90 degrees", FUN_004452b0 0x4472aa-0x44750c, TB-C5 of
// AvP-docs/engine/inteng/reassess/construction-closeout-frost.md): on a REAL level (the first program argument) it turns one shape through the edit store, saves a COPY (never the
// opened file), loads the copy with the game's own loader, and checks the numbers. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng (made from host_shape_recentre_test.cpp).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_turn_test.exe <a level .rif> <a folder it may write its copy into (made if it isn't there)>. With no arguments it says so and passes, so the test list can hold it.
//
// WHAT IT CHECKS, after the reload of the copy:
//  1. every point (x, y, z) of the shape is (-z, y, x) of what it was, every point normal and polygon normal the same, the radius and the polygons as they were;
//  2. the extents are turned whole: max x = -min z, min x = -max z, max z = max x, min z = min x (so max x is never below min x: inteng's own turn of the two triples, each by
//     itself, gives max x = -max z and min x = -min z, the wrong way round: INTENG BUG, FIXED), y as it was;
//  3. every user of the shape keeps its place and turn;
//  4. a shape with a SHPMORPH is refused and nothing of it changes;
//  5. the file that was opened is byte for byte what it was.

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
#include "shape_edit.h"
#include "shape_maths.h"

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

typedef std::vector<unsigned char> Bytes;

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

struct UserBefore
{
	uint8_t id[8];
	int32_t place[3];
	float turn[4];
};

// The users of a shape, in the level's own order, read from their header chunks' bytes (the place at 20, the turn at 32, as the editor reads them)
static bool UsersOf(OpenLevelFile const &open, Shape_Chunk const *owned, std::vector<UserBefore> *users)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const user = each();

		if (!user || user->get_assoc_shape() != owned)
		{
			continue;
		}

		Bytes bytes;
		Object_Header_Chunk *const header = user->get_header();

		if (!header || !open.PayloadOf(header, &bytes) || bytes.size() < 48)
		{
			return false;
		}

		UserBefore entry;

		memcpy(entry.id, &user->object_data.ID, 8);
		memcpy(entry.place, &bytes[20], sizeof(entry.place));
		memcpy(entry.turn, &bytes[32], sizeof(entry.turn));
		users->push_back(entry);
	}

	return true;
}

static Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
{
	List<Object_Chunk *> all;

	level->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
		{
			return each();
		}
	}

	return nullptr;
}

static bool NormalTurned(model::ShapeNormal const &before, model::ShapeNormal const &after)
{
	return after.x == -before.z && after.y == before.y && after.z == before.x;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_shape_turn_test <a level .rif> <a folder for its copy>: no level was named, so nothing was tested here\n");
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

	OpenLevelFile before;
	Check(before.Open(levelPath.c_str()) == LevelOpenResult::Opened, "the game's loader takes the level, and the editor pairs it");

	if (!before.Level())
	{
		return 1;
	}

	// The shape: the first one this program can read, with no morph, whose x and z extents differ (so a turn shows in them); and the first shape with a morph, for 4
	List<Object_Chunk *> all;

	before.Level()->list_objects(&all);

	uint8_t chosen[8] = { 0 };
	uint8_t morphing[8] = { 0 };
	bool haveChosen = false;
	bool haveMorphing = false;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape())
		{
			continue;
		}

		uint8_t const *const id = (uint8_t const *)&object->object_data.ID;

		if (host::ShapeEdit_HasMorph(before.Level(), id))
		{
			if (!haveMorphing)
			{
				memcpy(morphing, id, 8);
				haveMorphing = true;
			}

			continue;
		}

		model::ShapeModel probe;
		std::string why;

		if (!haveChosen && host::ShapeEdit_Read(before.Level(), &before, id, &probe, nullptr, &why) == host::ShapeEditResult::Done && probe.points.size() >= 3 &&
			(probe.extents.maxX != probe.extents.maxZ || probe.extents.minX != probe.extents.minZ) && probe.extents.maxZ != -probe.extents.minZ)
		{
			memcpy(chosen, id, 8);
			haveChosen = true;
		}
	}

	Check(haveChosen, "the level has a shape this program can read, with no morph and x and z extents that differ");

	if (!haveChosen)
	{
		return 1;
	}

	model::ShapeModel shapeBefore;
	std::string why;

	Check(host::ShapeEdit_Read(before.Level(), &before, chosen, &shapeBefore, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read before");

	Object_Chunk *const chosenObject = ObjectWithId(before.Level(), chosen);
	Shape_Chunk *const owned = chosenObject ? chosenObject->get_assoc_shape() : nullptr;
	std::vector<UserBefore> usersAt;

	bool const usersRead = owned && UsersOf(before, owned, &usersAt) && !usersAt.empty();   // read before the words are made (an argument's order is the compiler's)

	Check(usersRead, "the users' places and turns are read by hand: " + std::to_string(usersAt.size()));

	// 4. a morphing shape is refused, its numbers as they were
	if (haveMorphing)
	{
		model::ShapeModel morphBefore, morphAfter;
		std::string morphWhy;
		bool const readBefore = host::ShapeEdit_Read(before.Level(), &before, morphing, &morphBefore, nullptr, &morphWhy) == host::ShapeEditResult::Done;
		host::ShapeEditResult const refused = host::ShapeEdit_TurnQuarter(&before, before.Level(), morphing, &morphWhy);
		bool const readAfter = host::ShapeEdit_Read(before.Level(), &before, morphing, &morphAfter, nullptr, &morphWhy) == host::ShapeEditResult::Done;

		Check(refused == host::ShapeEditResult::Refused, "a shape with a SHPMORPH is refused");
		Check(!readBefore || (readAfter && morphAfter.points.size() == morphBefore.points.size() &&
			(morphBefore.points.empty() || memcmp(morphAfter.points.data(), morphBefore.points.data(), morphBefore.points.size() * sizeof(model::ShapePoint)) == 0)),
			"and its points are as they were");
	}
	else
	{
		printf("  (the level has no shape with a SHPMORPH: check 4 not made)\n");
	}

	// The edit
	host::ShapeEditResult const result = host::ShapeEdit_TurnQuarter(&before, before.Level(), chosen, &why);

	Check(result == host::ShapeEditResult::Done, std::string("Rotate object 90 degrees is done") + (result == host::ShapeEditResult::Done ? "" : ": " + why));

	// The copy: never the opened file
	std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "turn-test-copy.rif";

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;
	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	// The reload, by the game's own loader
	OpenLevelFile after;
	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened, "the game's loader takes the copy");

	if (!after.Level())
	{
		return 1;
	}

	model::ShapeModel shapeAfter;

	Check(host::ShapeEdit_Read(after.Level(), &after, chosen, &shapeAfter, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read after the reload");
	Check(shapeAfter.points.size() == shapeBefore.points.size() && shapeAfter.polygons.size() == shapeBefore.polygons.size() &&
		shapeAfter.pointNormals.size() == shapeBefore.pointNormals.size() && shapeAfter.polygonNormals.size() == shapeBefore.polygonNormals.size(),
		"the numbers of points, polygons and normals are as they were");

	// 1. the points and the normals turned, the radius and the polygons as they were
	bool pointsTurned = shapeAfter.points.size() == shapeBefore.points.size();

	for (size_t at = 0; pointsTurned && at < shapeBefore.points.size(); at++)
	{
		pointsTurned = shapeAfter.points[at].x == -shapeBefore.points[at].z && shapeAfter.points[at].y == shapeBefore.points[at].y && shapeAfter.points[at].z == shapeBefore.points[at].x;
	}

	Check(pointsTurned, "every point (x, y, z) is (-z, y, x) of what it was: " + std::to_string(shapeBefore.points.size()) + " points");

	bool normalsTurned = shapeAfter.pointNormals.size() == shapeBefore.pointNormals.size() && shapeAfter.polygonNormals.size() == shapeBefore.polygonNormals.size();

	for (size_t at = 0; normalsTurned && at < shapeBefore.pointNormals.size(); at++)
	{
		normalsTurned = NormalTurned(shapeBefore.pointNormals[at], shapeAfter.pointNormals[at]);
	}

	for (size_t at = 0; normalsTurned && at < shapeBefore.polygonNormals.size(); at++)
	{
		normalsTurned = NormalTurned(shapeBefore.polygonNormals[at], shapeAfter.polygonNormals[at]);
	}

	Check(normalsTurned, "every point normal and polygon normal is turned the same way");
	Check(shapeAfter.radius == shapeBefore.radius, "the radius is as it was");

	bool polygonsSame = shapeAfter.polygons.size() == shapeBefore.polygons.size();

	for (size_t at = 0; polygonsSame && at < shapeBefore.polygons.size(); at++)
	{
		polygonsSame = memcmp(&shapeAfter.polygons[at], &shapeBefore.polygons[at], sizeof(model::ShapePolygon)) == 0;
	}

	Check(polygonsSame, "the polygons are as they were");

	// 2. the extents turned whole
	model::ShapeExtents const &was = shapeBefore.extents;
	model::ShapeExtents const &now = shapeAfter.extents;

	Check(now.maxX == -was.minZ && now.minX == -was.maxZ && now.maxZ == was.maxX && now.minZ == was.minX && now.maxY == was.maxY && now.minY == was.minY,
		"the extents are turned whole: max x " + std::to_string(now.maxX) + " = -(min z " + std::to_string(was.minZ) + "), min x " + std::to_string(now.minX) + " = -(max z " +
		std::to_string(was.maxZ) + "), z from x, y as it was");
	Check(now.maxX >= now.minX && now.maxZ >= now.minZ, "max is not below min on x and z (inteng's own turn gives max x = -max z, min x = -min z: INTENG BUG, FIXED)");

	// 3. every user keeps its place and turn
	Object_Chunk *const chosenAfter = ObjectWithId(after.Level(), chosen);
	std::vector<UserBefore> usersNow;
	bool const usersFound = chosenAfter && chosenAfter->get_assoc_shape() && UsersOf(after, chosenAfter->get_assoc_shape(), &usersNow) && usersNow.size() == usersAt.size();

	Check(usersFound, "the users are found again in the copy: " + std::to_string(usersNow.size()));

	bool usersSame = usersFound;

	for (size_t at = 0; usersSame && at < usersAt.size(); at++)
	{
		usersSame = memcmp(usersNow[at].id, usersAt[at].id, 8) == 0 && memcmp(usersNow[at].place, usersAt[at].place, sizeof(usersAt[at].place)) == 0 &&
			memcmp(usersNow[at].turn, usersAt[at].turn, sizeof(usersAt[at].turn)) == 0;
	}

	Check(usersSame, "every user keeps its place, turn and ID");

	DeleteFileA(copyPath.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
