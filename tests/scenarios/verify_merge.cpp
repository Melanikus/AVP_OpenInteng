// verify_merge.cpp: the check of Texture Map's Merge save (TM-B 2c, acceptance-texture-map-merge-save.txt): one object's shape's SHPMRGDT numbers in the level the editor opened and in the copy
// it saved, read with the game's own loader. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_merge.exe <the level the editor opened> <the copy it saved> <object name> <a> <b> parted | paired | same
//   parted: polygons a and b were each other's partner before and are -1 both after; paired: each other's partner after; same: a and b as before. In every case every other number is as it was.
// Exit 0 only when every check holds. HOSTILE INPUT: both files are read by the game's chunk library; every size is checked before a number is read.
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

// The named object's shape's SHPMRGDT numbers; false when there is no such object, shape or chunk, or its bytes are not whole numbers
static bool MergeNumbers(OpenLevelFile &file, char const *name, std::vector<int32_t> *numbers)
{
	List<Object_Chunk *> objects;

	numbers->clear();
	file.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || strncmp(object->object_data.o_name, name, sizeof(object->object_data.o_name)) != 0)
		{
			continue;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();
		Chunk *const merge = shape ? shape->lookup_single_child("SHPMRGDT") : nullptr;
		std::vector<unsigned char> bytes;

		if (!merge || !file.PayloadOf(merge, &bytes) || bytes.size() % 4 != 0)
		{
			return false;
		}

		numbers->resize(bytes.size() / 4);

		if (!bytes.empty())
		{
			memcpy(numbers->data(), bytes.data(), bytes.size());
		}

		return true;
	}

	return false;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 7)
	{
		printf("usage: verify_merge <the level the editor opened> <the copy it saved> <object name> <a> <b> parted | paired | same\n");
		return 2;
	}

	char const *const name = argv[3];
	int32_t const a = atoi(argv[4]), b = atoi(argv[5]);
	std::string const how = argv[6];
	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the editor's copy");

	if (!before.Level() || !after.Level())
	{
		return 1;
	}

	std::vector<int32_t> was, now;

	Check(MergeNumbers(before, name, &was) && MergeNumbers(after, name, &now), std::string(name) + "'s shape has its SHPMRGDT in both");
	Check(was.size() == now.size() && a >= 0 && b >= 0 && (size_t)a < was.size() && (size_t)b < was.size(), "as many numbers in both (" + std::to_string(was.size()) + "), and both polygons in them");

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED\n");
		return 1;
	}

	if (how == "parted")
	{
		Check(was[(size_t)a] == b && was[(size_t)b] == a, std::to_string(a) + " and " + std::to_string(b) + " were each other's partner before");
		Check(now[(size_t)a] == -1 && now[(size_t)b] == -1, "and are parted (-1 both) in the copy");
	}
	else if (how == "paired")
	{
		Check(now[(size_t)a] == b && now[(size_t)b] == a, std::to_string(a) + " and " + std::to_string(b) + " are each other's partner in the copy");
	}
	else
	{
		Check(now[(size_t)a] == was[(size_t)a] && now[(size_t)b] == was[(size_t)b], std::to_string(a) + " and " + std::to_string(b) + " are as they were");
	}

	size_t same = 0, others = 0;

	for (size_t i = 0; i < was.size(); i++)
	{
		if ((int32_t)i != a && (int32_t)i != b)
		{
			others++;
			same += now[i] == was[i] ? 1 : 0;
		}
	}

	Check(same == others, "every other number is as it was (" + std::to_string(same) + " of " + std::to_string(others) + ")");
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
