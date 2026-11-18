// The test for ShapeMergeEdit_Objects (src/host/shape_merge_edit.cpp, Construction > Object > Merge Objects) with SHARED shapes and the second object's shape (TB-C8b / C8h of
// AvP-docs/engine/inteng/reassess/construction-closeout-frost.md): on a REAL level (the first program argument) it merges, through the edit store, saves a COPY (never the opened file),
// loads the copy with the game's own loader, and checks the shapes. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng (made from host_shape_merge_edit_test.cpp).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_merge_shared_test.exe <a level .rif> <a folder it may write its copies into>. With no arguments it says so and passes, so the test list can hold it.
//
// WHAT inteng DOES (FUN_00449cf0): a shape with more than one user is first given to the object as a COPY of its own (FUN_004d1090 takes the object off the shape, FUN_0050e880
// copies the shape, FUN_004d0ff0 puts the object on the copy: 0x449dba-0x449ddb for the first object, 0x449e9b-0x449ebc for the second), so the other users keep the shape as it was.
// The second object's shape (or its copy) is DELETED with the second object at the end (local_178, 0x44a834). The library numbers a new shape one past the highest (chunk_shape.cpp
// prepare_for_output: file_id_num -1 becomes ++max_id).
//
// WHAT IT CHECKS:
//  CASE 1, the first object's shape is shared, the second's is its own:
//   1. the merge is done (before TB-C8b it was refused: "another object uses the first shape too");
//   2. the first object now uses a NEW shape, numbered one past the level's highest, used by it alone, with the polygons of both;
//   3. the old shape is still there with its other users, its polygons as they were, and the first object's name is off its list;
//   4. the second object and its shape are gone.
//  CASE 2, the second object's shape is shared, the first's is its own:
//   5. the merge is done; the second's shape is still there for its other users, with its polygons, and the second object's name is off its list.
//  And the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "shape_merge.h"
#include "shape_merge_edit.h"
#include "object_edit.h"

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

static int32_t IntIn(Bytes const &bytes, size_t at)
{
	int32_t value = -1;

	if (bytes.size() >= at + 4)
	{
		memcpy(&value, bytes.data() + at, 4);
	}

	return value;
}

// A shape's number, from its SHPHEAD1's bytes (payload offset 20, shape_model.h); -1 when it can't be read
static int32_t NumberOfShape(OpenLevelFile const &open, Shape_Chunk *shape)
{
	Bytes bytes;
	Chunk *const head = shape ? shape->lookup_single_child("SHPHEAD1") : nullptr;

	return head && open.PayloadOf(head, &bytes) ? IntIn(bytes, 20) : -1;
}

// The number of the shape an object uses, from its OBJHEAD1's bytes (shape_id_no, payload offset 56); -1 when it can't be read
static int32_t ShapeNumberOfObject(OpenLevelFile const &open, Object_Chunk *object)
{
	Bytes bytes;
	Chunk *const head = object ? object->get_header() : nullptr;

	return head && open.PayloadOf(head, &bytes) ? IntIn(bytes, 56) : -1;
}

static Shape_Chunk *ShapeNumbered(OpenLevelFile const &open, int32_t number)
{
	List<Shape_Chunk *> shapes;

	open.Level()->list_shapes(&shapes);

	for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
	{
		if (NumberOfShape(open, each()) == number)
		{
			return each();
		}
	}

	return nullptr;
}

static int32_t HighestShapeNumber(OpenLevelFile const &open)
{
	List<Shape_Chunk *> shapes;
	int32_t highest = -1;

	open.Level()->list_shapes(&shapes);

	for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
	{
		int32_t const number = NumberOfShape(open, each());

		highest = number > highest ? number : highest;
	}

	return highest;
}

// The IDs of the objects a shape lists as its users
static std::vector<std::string> UserIds(Shape_Chunk *shape)
{
	std::vector<std::string> ids;

	if (shape)
	{
		List<Object_Chunk *> users = shape->list_assoc_objs();   // a copy: the iterator takes a list it may change

		for (LIF<Object_Chunk *> each(&users); !each.done(); each.next())
		{
			ids.push_back(std::string((char const *)&each()->object_data.ID, 8));
		}
	}

	std::sort(ids.begin(), ids.end());
	return ids;
}

static bool Refused(Shape_Chunk *shape)
{
	char const *const names[] = { "ANIMSEQU", "TEXTANIM", "SHPMRGDT", "SHPMORPH", "SHPPCINF" };

	for (char const *const name : names)
	{
		if (shape->lookup_single_child(name))
		{
			return true;
		}
	}

	return false;
}

// What a case needs to know about the level before its merge
struct Before
{
	uint8_t first[8];
	uint8_t second[8];
	int32_t shapeOfFirst;
	int32_t shapeOfSecond;
	int32_t highest;
	size_t polygonsFirst;
	size_t polygonsSecond;
	std::vector<std::string> usersOfShared;   // the users of the shared shape, the merged object's ID taken out
};

// Merges the first pair the edit takes, the first chosen from `firsts` and the second from `seconds`; false when none is taken
static bool MergeAPair(OpenLevelFile *open, std::vector<Object_Chunk *> const &firsts, std::vector<Object_Chunk *> const &seconds, bool firstShared, Before *was, std::string const &what)
{
	host::ShapeMergeEditResult result = host::ShapeMergeEditResult::BadCall;
	std::string why, lastWhy;
	size_t tried = 0;

	for (size_t a = 0; a < firsts.size() && result != host::ShapeMergeEditResult::Done && tried < 40; a++)
	{
		for (size_t b = 0; b < seconds.size() && result != host::ShapeMergeEditResult::Done && tried < 40; b++)
		{
			if (firsts[a] == seconds[b] || firsts[a]->get_assoc_shape() == seconds[b]->get_assoc_shape())
			{
				continue;
			}

			model::ShapeModel shapeA, shapeB;

			memcpy(was->first, &firsts[a]->object_data.ID, 8);
			memcpy(was->second, &seconds[b]->object_data.ID, 8);

			if (host::ShapeEdit_Read(open->Level(), open, was->first, &shapeA, nullptr, &why) != host::ShapeEditResult::Done ||
				host::ShapeEdit_Read(open->Level(), open, was->second, &shapeB, nullptr, &why) != host::ShapeEditResult::Done)
			{
				continue;
			}

			Object_Chunk *const shared = firstShared ? firsts[a] : seconds[b];

			was->shapeOfFirst = ShapeNumberOfObject(*open, firsts[a]);
			was->shapeOfSecond = ShapeNumberOfObject(*open, seconds[b]);
			was->highest = HighestShapeNumber(*open);
			was->polygonsFirst = shapeA.polygons.size();
			was->polygonsSecond = shapeB.polygons.size();
			was->usersOfShared = UserIds(shared->get_assoc_shape());

			std::string const mine((char const *)&shared->object_data.ID, 8);
			was->usersOfShared.erase(std::remove(was->usersOfShared.begin(), was->usersOfShared.end(), mine), was->usersOfShared.end());

			tried++;
			why.clear();
			result = host::ShapeMergeEdit_Objects(open, was->first, was->second, &why);   // after a success every Chunk * held is gone: the loops end at once

			if (result != host::ShapeMergeEditResult::Done)
			{
				lastWhy = std::string(host::ShapeMergeEditWords(result)) + (why.empty() ? "" : " (" + why + ")");
			}
		}
	}

	Check(result == host::ShapeMergeEditResult::Done, what + " (pairs tried: " + std::to_string(tried) + ")" + (result == host::ShapeMergeEditResult::Done ? "" : ": " + lastWhy));
	return result == host::ShapeMergeEditResult::Done;
}

static bool SaveAndReload(OpenLevelFile *open, std::string const &copyFolder, char const *name, OpenLevelFile *after, std::string *copyPath)
{
	*copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + name;

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath->c_str());
	Check(open->SaveAs(copyPath->c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + *copyPath);
	Check(after->Open(copyPath->c_str()) == LevelOpenResult::Opened, "the game's loader takes the copy");
	return after->Level() != nullptr;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_shape_merge_shared_test <a level .rif> <a folder for its copies>: no level was named, so nothing was tested here\n");
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

	for (int which = 1; which <= 2; which++)
	{
		bool const firstShared = which == 1;

		printf("CASE %d: the %s object's shape is shared\n", which, firstShared ? "first" : "second");

		OpenLevelFile open;
		Check(open.Open(levelPath.c_str()) == LevelOpenResult::Opened, "the game's loader takes the level, and the editor pairs it");

		if (!open.Level())
		{
			return 1;
		}

		// The objects (not modules) Merge Objects takes, by whether their shape has one user or more. A stock level gives every placed object a shape of its own, so one is COPIED
		// first, as Object Placement's Create Duplicate does (CreateObjectLike: the copy uses the same shape, as inteng's): then that shape has two users.
		std::vector<Object_Chunk *> own, shared;
		bool copied = false;

		for (int pass = 0; pass < 2; pass++)
		{
			List<Object_Chunk *> all;

			own.clear();
			shared.clear();
			open.Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

				if (!shape || object->lookup_single_child("MODULEDT") || Refused(shape) || shape->lookup_single_child("SHPEXTFL"))
				{
					continue;
				}

				(shape->list_assoc_objs().size() == 1 ? own : shared).push_back(object);
			}

			if (pass == 0 && !own.empty())
			{
				uint8_t like[8], made[8];

				memcpy(like, &own.back()->object_data.ID, 8);
				copied = host::CreateObjectLike(&open, like, "mergeSharedCopy", nullptr, made) == host::ObjectEditResult::Done;   // every Chunk * held is gone after it
				Check(copied, "an object is copied, so its shape has two users (CreateObjectLike)");
			}
		}

		Check(!own.empty() && !shared.empty(), "the level has objects with a shape of their own (" + std::to_string(own.size()) + ") and with a shared shape (" + std::to_string(shared.size()) + ")");

		Before was = {};

		if (own.empty() || shared.empty() || !MergeAPair(&open, firstShared ? shared : own, firstShared ? own : shared, firstShared, &was, "Merge Objects takes the pair"))
		{
			continue;   // the next case still runs
		}

		OpenLevelFile after;
		std::string copyPath;

		if (!SaveAndReload(&open, copyFolder, firstShared ? "merge-shared-1.rif" : "merge-shared-2.rif", &after, &copyPath))
		{
			return 1;
		}

		Object_Chunk *const objectA = ObjectWithId(after.Level(), was.first);

		Check(objectA != nullptr && objectA->get_assoc_shape() != nullptr, "the first object is in the copy, with a shape");
		Check(ObjectWithId(after.Level(), was.second) == nullptr, "the second object is gone");

		if (!objectA || !objectA->get_assoc_shape())
		{
			return 1;
		}

		model::ShapeModel merged;
		std::string why;

		bool const mergedRead = host::ShapeEdit_Read(after.Level(), &after, was.first, &merged, nullptr, &why) == host::ShapeEditResult::Done;   // read before the words are made

		Check(mergedRead && merged.polygons.size() == was.polygonsFirst + was.polygonsSecond,
			"the first object's shape has the polygons of both: " + std::to_string(merged.polygons.size()) + " = " + std::to_string(was.polygonsFirst) + " + " + std::to_string(was.polygonsSecond));

		Shape_Chunk *const sharedNow = ShapeNumbered(after, firstShared ? was.shapeOfFirst : was.shapeOfSecond);

		Check(sharedNow != nullptr, "the shared shape (number " + std::to_string(firstShared ? was.shapeOfFirst : was.shapeOfSecond) + ") is still in the level for its other users");
		Check(UserIds(sharedNow) == was.usersOfShared, "its users are the ones it had, less the merged object (" + std::to_string(UserIds(sharedNow).size()) + " of " + std::to_string(was.usersOfShared.size()) + ")");

		// The shared shape's polygons as they were: read through one of its other users
		if (sharedNow && !was.usersOfShared.empty())
		{
			model::ShapeModel kept;

			bool const keptRead = host::ShapeEdit_Read(after.Level(), &after, (uint8_t const *)was.usersOfShared.front().data(), &kept, nullptr, &why) == host::ShapeEditResult::Done;

			Check(keptRead && kept.polygons.size() == (firstShared ? was.polygonsFirst : was.polygonsSecond),
				"its polygons are as they were: " + std::to_string(kept.polygons.size()));
		}

		if (firstShared)
		{
			int32_t const nowA = ShapeNumberOfObject(after, objectA);

			Check(nowA == was.highest + 1, "the first object uses a NEW shape, numbered one past the highest: " + std::to_string(nowA) + " (the highest was " + std::to_string(was.highest) + ")");
			Check(UserIds(objectA->get_assoc_shape()) == std::vector<std::string>(1, std::string((char const *)was.first, 8)), "the new shape's only user is the first object");
			Check(ShapeNumbered(after, was.shapeOfSecond) == nullptr, "the second object's own shape (number " + std::to_string(was.shapeOfSecond) + ") is gone with it");
		}
		else
		{
			Check(ShapeNumberOfObject(after, objectA) == was.shapeOfFirst, "the first object keeps its own shape (number " + std::to_string(was.shapeOfFirst) + ")");
		}

		DeleteFileA(copyPath.c_str());
	}

	// CASES 3 and 4: the two objects use ONE shape (an object and its copy; inteng's FUN_00449cf0 gives the first a copy, 0x449dba-0x449ddb, merges the original into it and
	// deletes the original at the end, 0x44a830, when the second was its last user; with a third user the original stays for it)
	for (int copies = 1; copies <= 2; copies++)
	{
		printf("CASE %d: the two objects use one shape, which %s\n", copies + 2, copies == 1 ? "they alone use" : "a third object uses too");

		OpenLevelFile open;
		Check(open.Open(levelPath.c_str()) == LevelOpenResult::Opened, "the game's loader takes the level, and the editor pairs it");

		if (!open.Level())
		{
			return 1;
		}

		// an object with a shape of its own that Merge Objects takes, then its copies
		uint8_t first[8] = { 0 }, second[8] = { 0 }, third[8] = { 0 };
		bool found = false;
		{
			List<Object_Chunk *> all;

			open.Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done() && !found; each.next())
			{
				Shape_Chunk *const shape = each()->get_assoc_shape();

				if (shape && !each()->lookup_single_child("MODULEDT") && !Refused(shape) && !shape->lookup_single_child("SHPEXTFL") && shape->list_assoc_objs().size() == 1)
				{
					memcpy(first, &each()->object_data.ID, 8);
					found = true;
				}
			}
		}

		Check(found && host::CreateObjectLike(&open, first, "mergeSameCopy1", nullptr, second) == host::ObjectEditResult::Done, "the object is copied: the copy uses its shape");

		if (copies == 2)
		{
			Check(host::CreateObjectLike(&open, first, "mergeSameCopy2", nullptr, third) == host::ObjectEditResult::Done, "it is copied again: three objects use the shape");
		}

		model::ShapeModel before;
		std::string why;
		int32_t const oldNumber = ShapeNumberOfObject(open, ObjectWithId(open.Level(), first));
		int32_t const highest = HighestShapeNumber(open);
		bool const readBefore = host::ShapeEdit_Read(open.Level(), &open, first, &before, nullptr, &why) == host::ShapeEditResult::Done;
		host::ShapeMergeEditResult const result = host::ShapeMergeEdit_Objects(&open, first, second, &why);

		Check(readBefore && result == host::ShapeMergeEditResult::Done, std::string("Merge Objects takes an object and its copy: ") + host::ShapeMergeEditWords(result) + (why.empty() ? "" : " (" + why + ")"));

		if (result != host::ShapeMergeEditResult::Done)
		{
			continue;
		}

		OpenLevelFile after;
		std::string copyPath;

		if (!SaveAndReload(&open, copyFolder, copies == 1 ? "merge-same-1.rif" : "merge-same-2.rif", &after, &copyPath))
		{
			return 1;
		}

		Object_Chunk *const objectA = ObjectWithId(after.Level(), first);
		model::ShapeModel merged;
		bool const mergedRead = objectA && host::ShapeEdit_Read(after.Level(), &after, first, &merged, nullptr, &why) == host::ShapeEditResult::Done;

		Check(ObjectWithId(after.Level(), second) == nullptr, "the copy is gone");
		Check(mergedRead && merged.polygons.size() == 2 * before.polygons.size(), "the first object's shape has the polygons of both: " + std::to_string(merged.polygons.size()) + " = 2 x " +
			std::to_string(before.polygons.size()));
		Check(objectA && ShapeNumberOfObject(after, objectA) == highest + 1, "the first object uses its own new shape, numbered one past the highest (" + std::to_string(highest + 1) + ")");

		Shape_Chunk *const old = ShapeNumbered(after, oldNumber);

		if (copies == 1)
		{
			Check(old == nullptr, "the shape the two used is gone: nobody uses it any more");
		}
		else
		{
			Check(old != nullptr && UserIds(old) == std::vector<std::string>(1, std::string((char const *)third, 8)), "the shape stays for the third object alone");
		}

		DeleteFileA(copyPath.c_str());
	}

	std::vector<char> originalAfter;
	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
