// The test for ShapeEdit_Recentre (src/host/shape_edit.cpp, Recentre Shape, "Move centre to top"): on a REAL level (the first program argument),
// it recentres one shape through the edit store, saves a COPY (never the opened file), loads the copy with the game's own loader, and checks the
// numbers. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng (P11, Vasquez's slice 7, which had no test). AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_recentre_test.exe <a level .rif> <a folder it may write its copy into (made if it isn't there)>. With no arguments it says
// so and passes, so the test list can hold it. The level is only read; the copy is the only file written.
//
// WHAT IT CHECKS, after the reload of the copy:
//  1. every object that uses the shape was moved (the count Recentre gives is the count of users found by hand);
//  2. every point of the shape is where it was, minus the target (the middle of the extents on x and z, the top on y), and the extents and
//     the number of points and polygons are as they were, minus the target for the extents;
//  3. every user's place is its old place minus (its own turn applied to the offset, the target negated), and its turn is unchanged;
//  4. nothing else changed: the two levels (before and after) are the same size, and the bytes that differ are no more than the points, the
//     header's radius and extents, and the users' places can account for (and not none);
//  5. the file that was opened is byte for byte what it was.
// A level, a shape and its users are chosen by the program itself: the first shape with the most users (up to 2 or more if there is one).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
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

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_shape_recentre_test <a level .rif> <a folder for its copy>: no level was named, so nothing was tested here\n");
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

	// The shape with the most users, from the first object that has a shape this program can read; a tie goes to the first
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
			&& probe.points.size() >= 3 && users > mostUsers && users <= 64)
		{
			mostUsers = users;
			memcpy(chosen, &object->object_data.ID, 8);
		}
	}

	Check(mostUsers >= 1, "the level has a shape this program can read, with " + std::to_string(mostUsers) + " user(s)");

	if (mostUsers < 1)
	{
		return 1;
	}

	model::ShapeModel shapeBefore;
	size_t usersBefore = 0;
	std::string why;

	Check(host::ShapeEdit_Read(before.Level(), &before, chosen, &shapeBefore, &usersBefore, &why) == host::ShapeEditResult::Done, "the shape is read before");

	Object_Chunk *const chosenObject = ObjectWithId(before.Level(), chosen);
	Shape_Chunk *const owned = chosenObject ? chosenObject->get_assoc_shape() : nullptr;
	std::vector<UserBefore> usersAt;

	bool const usersRead = owned && UsersOf(before, owned, &usersAt) && usersAt.size() == usersBefore;

	Check(usersRead, "the users' places and turns are read by hand: " + std::to_string(usersAt.size()));

	std::vector<char> imageBefore;
	Check(before.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	// The edit
	size_t moved = 0;
	host::ShapeEditResult const result = host::ShapeEdit_Recentre(&before, before.Level(), chosen, true, &moved, &why);

	Check(result == host::ShapeEditResult::Done, std::string("Recentre, Move centre to top, is done") + (result == host::ShapeEditResult::Done ? "" : ": " + why));
	Check(moved == usersAt.size(), "every user was moved: " + std::to_string(moved) + " of " + std::to_string(usersAt.size()));

	// The copy: never the opened file
	std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "recentre-test-copy.rif";

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
	size_t usersAfter = 0;

	Check(host::ShapeEdit_Read(after.Level(), &after, chosen, &shapeAfter, &usersAfter, &why) == host::ShapeEditResult::Done, "the shape is read after the reload");
	Check(usersAfter == usersBefore, "the shape has as many users as before");
	Check(shapeAfter.points.size() == shapeBefore.points.size() && shapeAfter.polygons.size() == shapeBefore.polygons.size(),
		"the numbers of points and of polygons are as they were");

	// 2. every point minus the target; the extents minus the target
	model::ShapePoint const target = model::RecentreTarget(shapeBefore.extents, true);
	bool pointsMoved = shapeAfter.points.size() == shapeBefore.points.size();

	for (size_t at = 0; pointsMoved && at < shapeBefore.points.size(); at++)
	{
		pointsMoved = shapeAfter.points[at].x == shapeBefore.points[at].x - target.x && shapeAfter.points[at].y == shapeBefore.points[at].y - target.y
			&& shapeAfter.points[at].z == shapeBefore.points[at].z - target.z;
	}

	Check(pointsMoved, "every point of the shape is where it was, minus the target (" + std::to_string(target.x) + ", " + std::to_string(target.y) + ", " + std::to_string(target.z) + ")");
	Check(shapeAfter.extents.maxX == shapeBefore.extents.maxX - target.x && shapeAfter.extents.minX == shapeBefore.extents.minX - target.x
		&& shapeAfter.extents.maxY == shapeBefore.extents.maxY - target.y && shapeAfter.extents.minY == shapeBefore.extents.minY - target.y
		&& shapeAfter.extents.maxZ == shapeBefore.extents.maxZ - target.z && shapeAfter.extents.minZ == shapeBefore.extents.minZ - target.z,
		"the extents are as they were, minus the target");
	Check(shapeAfter.extents.maxY == 0, "Move centre to top puts the top of the shape at 0 on y");

	bool polygonsSame = shapeAfter.polygons.size() == shapeBefore.polygons.size();

	for (size_t at = 0; polygonsSame && at < shapeBefore.polygons.size(); at++)
	{
		polygonsSame = memcmp(&shapeAfter.polygons[at], &shapeBefore.polygons[at], sizeof(model::ShapePolygon)) == 0;
	}

	Check(polygonsSame, "the polygons are as they were");

	// 3. every user's place minus its own turn applied to the offset; its turn unchanged
	Object_Chunk *const chosenAfter = ObjectWithId(after.Level(), chosen);
	std::vector<UserBefore> usersNow;

	bool const usersFound = chosenAfter && chosenAfter->get_assoc_shape() && UsersOf(after, chosenAfter->get_assoc_shape(), &usersNow) && usersNow.size() == usersAt.size();

	Check(usersFound, "the users are found again in the copy: " + std::to_string(usersNow.size()));

	model::ShapePoint const offset = { -target.x, -target.y, -target.z };
	bool placesMoved = usersNow.size() == usersAt.size();
	bool turnsSame = placesMoved;
	std::string firstBad;

	for (size_t at = 0; placesMoved && at < usersAt.size(); at++)
	{
		model::ShapePoint turned;

		if (model::RotateByTurn(usersAt[at].turn, offset, &turned) != model::ShapeMathsResult::Ok)
		{
			placesMoved = false;
			firstBad = "a turn the maths refuses";
			break;
		}

		int64_t const want[3] = { (int64_t)usersAt[at].place[0] - turned.x, (int64_t)usersAt[at].place[1] - turned.y, (int64_t)usersAt[at].place[2] - turned.z };

		for (int axis = 0; axis < 3; axis++)
		{
			if ((int64_t)usersNow[at].place[axis] != want[axis] && firstBad.empty())
			{
				firstBad = "user " + std::to_string(at) + ", axis " + std::to_string(axis) + ": " + std::to_string(usersNow[at].place[axis]) + " is not " + std::to_string(want[axis]);
			}
		}

		turnsSame = turnsSame && memcmp(usersNow[at].turn, usersAt[at].turn, sizeof(usersAt[at].turn)) == 0 && memcmp(usersNow[at].id, usersAt[at].id, 8) == 0;
	}

	Check(placesMoved && firstBad.empty(), "every user's place is its old place minus its own turn applied to the offset" + (firstBad.empty() ? std::string() : " (" + firstBad + ")"));
	Check(turnsSame, "every user's turn and ID are unchanged");

	// 4. nothing else changed: the same size, and the bytes that differ are no more than those numbers can account for
	std::vector<char> imageAfter;
	Check(after.BuildLevel(imageAfter), "the copy as the next save would write it is made (after)");
	Check(imageAfter.size() == imageBefore.size(), "the level is the same size as before: " + std::to_string(imageBefore.size()) + " bytes");

	size_t differing = 0;

	size_t const common = imageBefore.size() < imageAfter.size() ? imageBefore.size() : imageAfter.size();

	for (size_t at = 0; at < common; at++)
	{
		differing += imageBefore[at] != imageAfter[at] ? 1 : 0;
	}

	// inteng's own save runs prepare_for_output on the whole tree (every object's version_no +1, the shape headers' version and name lists, the zero padding after names), so a level built again differs from
	// the one built before in those bytes whatever is edited. A BASELINE measures it: the unedited level built a second time, against the first build. What the edit may change is the numbers above PLUS the
	// baseline (twice: the save runs it on the edited level and the build of the copy runs it again).
	std::vector<char> imageBaseline;
	size_t baseline = 0;

	if (before.BuildLevel(imageBaseline) && imageBaseline.size() == imageBefore.size())
	{
		for (size_t at = 0; at < imageBefore.size(); at++)
		{
			baseline += imageBefore[at] != imageBaseline[at] ? 1 : 0;
		}
	}

	size_t const accountedFor = shapeBefore.points.size() * 12 + 28 + usersAt.size() * 12 + baseline * 2;

	printf("  (%zu bytes differ; the points, the header's radius and extents, %zu place(s) and the save's own prepare_for_output (a baseline of %zu bytes, twice) can account for up to %zu)\n", differing, usersAt.size(), baseline, accountedFor);
	Check(differing > 0 && differing <= accountedFor, "only the numbers that were meant to move have changed");

	DeleteFileA(copyPath.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
