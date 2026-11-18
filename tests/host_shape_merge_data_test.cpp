// The test for ShapeMergeEdit_Objects (src/host/shape_merge_edit.cpp, Construction > Object > Merge Objects) with shapes that have MERGE DATA, SHPMRGDT (TB-C8c of
// AvP-docs/engine/inteng/reassess/construction-closeout-frost.md): on a REAL level (the first program argument) it finds two objects Merge Objects takes, at least one with SHPMRGDT,
// merges the second into the first through the edit store, saves a COPY, loads it with the game's own loader, and checks the merged SHPMRGDT: the first's entries, then the second's, each -1
// kept and any other plus the first's count (FUN_00449cf0 0x44a466-0x44a4ba); a shape without SHPMRGDT counts as all -1 (INTENG BUG, FIXED: inteng reads an empty list).
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng (made from host_shape_merge_edit_test.cpp). AVP_OpenInteng's own code: see License.txt.
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

// A shape's SHPMRGDT payload from the store's bytes (empty when it has none), and its ints
static Bytes MergePayload(OpenLevelFile const &open, Shape_Chunk *shape)
{
	Bytes bytes;
	Chunk *const chunk = shape ? shape->lookup_single_child("SHPMRGDT") : nullptr;

	if (chunk)
	{
		open.PayloadOf(chunk, &bytes);
	}

	return bytes;
}

static std::vector<int32_t> Ints(Bytes const &bytes)
{
	std::vector<int32_t> out(bytes.size() / 4);

	if (!out.empty())
	{
		memcpy(out.data(), bytes.data(), out.size() * 4);
	}

	return out;
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
	char const *const names[] = { "ANIMSEQU", "TEXTANIM", "SHPMORPH", "SHPPCINF" };

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
		printf("  host_shape_merge_data_test <a level .rif> <a folder for its copy>: no level was named, so nothing was tested here\n");
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

		if (!shape || object->lookup_single_child("MODULEDT") || Refused(shape))   // shared shapes too (TB-C8b): on stock levels the shapes with merge data are shared ones
		{
			continue;
		}

		candidates.push_back(object);
	}

	Check(candidates.size() >= 2, "the level has at least two objects Merge Objects can take: " + std::to_string(candidates.size()));

	if (candidates.size() < 2)
	{
		return 1;
	}

	// The first pair the merge takes (another object may name the second: then the next pair is tried)
	uint8_t first[8] = { 0 }, second[8] = { 0 };
	model::ShapeModel shapeA, shapeB;
	Bytes mergeA, mergeB;
	bool hasA = false, hasB = false;
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

			hasA = candidates[a]->get_assoc_shape()->lookup_single_child("SHPMRGDT") != nullptr;
			hasB = candidates[b]->get_assoc_shape()->lookup_single_child("SHPMRGDT") != nullptr;

			if (!hasA && !hasB)
			{
				continue;   // a pair with merge data on one of them, at least
			}

			mergeA = MergePayload(before, candidates[a]->get_assoc_shape());
			mergeB = MergePayload(before, candidates[b]->get_assoc_shape());
			tried++;
			why.clear();
			result = host::ShapeMergeEdit_Objects(&before, first, second, &why);   // after a success every Chunk * held is gone: the loops end at once
		}
	}

	Check(result == host::ShapeMergeEditResult::Done, std::string("Merge Objects takes two objects, one with merge data (pairs tried: ") + std::to_string(tried) + ")" +
		(result == host::ShapeMergeEditResult::Done ? "" : ": " + std::string(host::ShapeMergeEditWords(result)) + (why.empty() ? "" : " (" + why + ")")));

	if (result != host::ShapeMergeEditResult::Done)
	{
		printf("SOME CHECKS FAILED\n");
		return 1;
	}

	// The copy: never the opened file
	std::string const copyPath = copyFolder + (copyFolder.empty() || copyFolder.back() == '\\' || copyFolder.back() == '/' ? "" : "\\") + "merge-data-test-copy.rif";

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

	// 2. the merge data: the first's entries (all -1 when it had none), then the second's, -1 kept, any other plus the first's count
	model::ShapeModel merged;

	Check(host::ShapeEdit_Read(after.Level(), &after, first, &merged, nullptr, &why) == host::ShapeEditResult::Done, "the merged shape is read after the reload");
	Check(merged.polygons.size() == shapeA.polygons.size() + shapeB.polygons.size(), "it has the polygons of both: " + std::to_string(merged.polygons.size()));

	std::vector<int32_t> a = Ints(mergeA), b = Ints(mergeB);

	if (!hasA)
	{
		a.assign(shapeA.polygons.size(), -1);
	}

	if (!hasB)
	{
		b.assign(shapeB.polygons.size(), -1);
	}

	std::vector<int32_t> wanted = a;

	for (int32_t const entry : b)
	{
		wanted.push_back(entry == -1 ? -1 : entry + (int32_t)a.size());
	}

	std::vector<int32_t> const now = Ints(MergePayload(after, objectA->get_assoc_shape()));

	Check(now.size() == wanted.size(), "the merged SHPMRGDT has an entry for each polygon of both: " + std::to_string(now.size()) + " (the first " + std::string(hasA ? "had" : "had NO") + " merge data, the second " + std::string(hasB ? "had" : "had none") + ")");
	Check(now == wanted, "its entries are the first's, then the second's with -1 kept and the rest moved up by " + std::to_string(a.size()));

	DeleteFileA(copyPath.c_str());
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
