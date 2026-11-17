// The test for ShapeMergeEdit_Objects (src/host/shape_merge_edit.cpp, Construction > Object > Merge Objects) with TEXTURED shapes (TB-C8a of
// AvP-docs/engine/inteng/reassess/construction-closeout-frost.md): on a REAL level (the first program argument) it finds two objects that Merge Objects takes and at least one of which has
// UV lists (SHPUVCRD), merges the second into the first through the edit store, saves a COPY (never the opened file), loads the copy with the game's own loader, and checks the numbers.
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_merge_edit_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes, so the test list can hold it.
//
// WHAT IT CHECKS, after the reload of the copy (inteng FUN_00449cf0 / FUN_00448cf0 0x448f57-0x449029):
//  1. the merge is done (before TB-C8a every textured shape was refused);
//  2. the first shape's UV lists are its own, then the second's: the count is the sum, the bytes are the two lists one after the other;
//  3. each polygon of the second shape's part with a UV list names its old list plus the first's count; the first shape's own polygons are as they were;
//  4. the second object is gone; the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// A shape's SHPUVCRD payload from the store's bytes (empty when it has none)
static Bytes UvPayload(OpenLevelFile const &open, Shape_Chunk *shape)
{
	Bytes bytes;
	Chunk *const chunk = shape ? shape->lookup_single_child("SHPUVCRD") : nullptr;

	if (chunk)
	{
		open.PayloadOf(chunk, &bytes);
	}

	return bytes;
}

static int32_t CountOf(Bytes const &uv)
{
	int32_t count = 0;

	if (uv.size() >= 4)
	{
		memcpy(&count, uv.data(), 4);
	}

	return count;
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

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_shape_merge_edit_test <a level .rif> <a folder for its copy>: no level was named, so nothing was tested here\n");
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

	// The candidates: objects (not modules) with a shape of their own (one user), none of the chunks Merge Objects still refuses, and UV lists
	List<Object_Chunk *> all;
	std::vector<Object_Chunk *> candidates;

	before.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!shape || object->lookup_single_child("MODULEDT") || Refused(shape) || CountOf(UvPayload(before, shape)) <= 0 || shape->list_assoc_objs().size() != 1)
		{
			continue;
		}

		candidates.push_back(object);
	}

	Check(candidates.size() >= 2, "the level has at least two textured objects Merge Objects can take: " + std::to_string(candidates.size()));

	if (candidates.size() < 2)
	{
		return 1;
	}

	// The first pair the merge takes (another object may name the second: then the next pair is tried)
	uint8_t first[8] = { 0 }, second[8] = { 0 };
	model::ShapeModel shapeA, shapeB;
	Bytes uvA, uvB;
	host::ShapeMergeEditResult result = host::ShapeMergeEditResult::BadCall;
	std::string why;
	size_t tried = 0;

	for (size_t a = 0; a < candidates.size() && result != host::ShapeMergeEditResult::Done && tried < 40; a++)
	{
		for (size_t b = 0; b < candidates.size() && result != host::ShapeMergeEditResult::Done && tried < 40; b++)
		{
			if (a == b)
			{
				continue;
			}

			memcpy(first, &candidates[a]->object_data.ID, 8);
			memcpy(second, &candidates[b]->object_data.ID, 8);

			if (host::ShapeEdit_Read(before.Level(), &before, first, &shapeA, nullptr, &why) != host::ShapeEditResult::Done ||
				host::ShapeEdit_Read(before.Level(), &before, second, &shapeB, nullptr, &why) != host::ShapeEditResult::Done)
			{
				continue;
			}

			uvA = UvPayload(before, candidates[a]->get_assoc_shape());
			uvB = UvPayload(before, candidates[b]->get_assoc_shape());
			tried++;
			why.clear();
			result = host::ShapeMergeEdit_Objects(&before, first, second, &why);   // after a success every Chunk * held is gone: the loops end at once
		}
	}

	Check(result == host::ShapeMergeEditResult::Done, std::string("Merge Objects takes two textured objects (pairs tried: ") + std::to_string(tried) + ")" +
		(result == host::ShapeMergeEditResult::Done ? "" : ": " + std::string(host::ShapeMergeEditWords(result)) + (why.empty() ? "" : " (" + why + ")")));

	if (result != host::ShapeMergeEditResult::Done)
	{
		printf("SOME CHECKS FAILED\n");
		return 1;
	}

	// The copy: never the opened file
	std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "merge-test-copy.rif";

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;
	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	OpenLevelFile after;
	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened, "the game's loader takes the copy");

	if (!after.Level())
	{
		return 1;
	}

	Object_Chunk *const objectA = ObjectWithId(after.Level(), first);

	Check(objectA != nullptr, "the first object is in the copy");
	Check(ObjectWithId(after.Level(), second) == nullptr, "the second object is gone");

	if (!objectA || !objectA->get_assoc_shape())
	{
		return 1;
	}

	// 2. the UV lists: the first's, then the second's
	Bytes const uvNow = UvPayload(after, objectA->get_assoc_shape());
	int32_t const countA = CountOf(uvA), countB = CountOf(uvB);
	Bytes wanted(4);
	int32_t const total = countA + countB;

	memcpy(wanted.data(), &total, 4);
	wanted.insert(wanted.end(), uvA.begin() + 4, uvA.end());
	wanted.insert(wanted.end(), uvB.begin() + 4, uvB.end());

	Check(CountOf(uvNow) == total, "the merged shape has the UV lists of both: " + std::to_string(CountOf(uvNow)) + " = " + std::to_string(countA) + " + " + std::to_string(countB));
	Check(uvNow == wanted, "its lists are the first's, then the second's, byte for byte");

	// 3. the polygons: the first's as they were, the second's with their list numbers moved up by the first's count
	model::ShapeModel merged;

	Check(host::ShapeEdit_Read(after.Level(), &after, first, &merged, nullptr, &why) == host::ShapeEditResult::Done, "the merged shape is read after the reload");
	Check(merged.polygons.size() == shapeA.polygons.size() + shapeB.polygons.size(), "it has the polygons of both: " + std::to_string(merged.polygons.size()));

	bool firstSame = merged.polygons.size() >= shapeA.polygons.size();

	for (size_t at = 0; firstSame && at < shapeA.polygons.size(); at++)
	{
		firstSame = merged.polygons[at].colour == shapeA.polygons[at].colour && merged.polygons[at].engineType == shapeA.polygons[at].engineType;
	}

	Check(firstSame, "the first shape's polygons keep their colours (their UV list numbers)");

	bool secondMoved = merged.polygons.size() == shapeA.polygons.size() + shapeB.polygons.size();
	size_t textured = 0;

	for (size_t at = 0; secondMoved && at < shapeB.polygons.size(); at++)
	{
		model::ShapePolygon const &was = shapeB.polygons[at];
		model::ShapePolygon const &now = merged.polygons[shapeA.polygons.size() + at];

		if (model::PolygonTypeHasUvList(was.engineType))
		{
			textured++;
			secondMoved = ((uint32_t)now.colour >> 16) == ((uint32_t)was.colour >> 16) + (uint32_t)countA && ((uint32_t)now.colour & 0xFFFFu) == ((uint32_t)was.colour & 0xFFFFu);
		}
		else
		{
			secondMoved = now.colour == was.colour;
		}
	}

	Check(secondMoved, "each of the second shape's " + std::to_string(textured) + " textured polygons names its old UV list + " + std::to_string(countA) + ", the others keep their colours");

	DeleteFileA(copyPath.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
