// verify_bone_offsets.cpp: the check program of the bones' point offsets (TB-3: inteng's FUN_0041eae0 after Attach shape to bones and after Shift + V; reassess/hierarchy-closeout-frost.md). Written
// 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_bone_offsets.exe prepare <a model file> <the copy to write>
//        No file of his (nor any stock one) holds the reference sequence (10000, 0) that Attach needs, which the art tools make: the copy's first hierarchy has its first one-frame sequence given
//        those numbers (HierarchyEdit_RenumberSequence, as the unit test does). The model file is only read.
//      verify_bone_offsets.exe check <a saved model file>
//        Every object of the file attached to a hierarchy: each SHPBONDT point record's +4..+0xc is the point's place in its bone's own frame in the reference pose. A turn and a move keep distances,
//        so two points of one bone are as far apart in their offsets as in the world (HierarchyEdit_SkinPoints), each number cut toward zero: within 4 mm. Offsets left at 0, 0, 0 (ours before TB-3)
//        are not. Gives back 0 only if the file loads, at least one object is attached, and every compared pair keeps its distance.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "hierarchy_edit.h"

// ---- Stand-ins for the parts of the game the chunk code calls (as the unit tests')

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

using namespace host;

static int Prepare(char const *from, char const *to)
{
	OpenLevelFile file;

	if (file.Open(from) != LevelOpenResult::Opened || !file.Level())
	{
		printf("  FAILED  the game's loader reads the model: %s\n", from);
		return 1;
	}

	std::vector<std::string> names;

	if (HierarchyEdit_Names(&file, &names) != HierarchyEditResult::Done || names.empty())
	{
		printf("  FAILED  the model has a hierarchy\n");
		return 1;
	}

	std::vector<SequenceInfo> sequences;

	HierarchyEdit_Sequences(&file, names.front(), &sequences);

	for (SequenceInfo const &sequence : sequences)
	{
		std::vector<FrameInfo> frames;
		size_t changed = 0;

		if (HierarchyEdit_RootFrames(&file, names.front(), sequence.number, sequence.subNumber, &frames) != HierarchyEditResult::Done || frames.size() != 1 ||
			HierarchyEdit_RenumberSequence(&file, names.front(), sequence.number, sequence.subNumber, 10000, 0, &changed) != HierarchyEditResult::Done)
		{
			continue;
		}

		DeleteFileA(to);

		if (file.SaveAs(to) != LevelSaveResult::Saved)
		{
			printf("  FAILED  the copy could not be written: %s\n", to);
			return 1;
		}

		printf("  ok  %s: the one-frame sequence (%d, %d) of %s given the reference numbers (10000, 0) in %zu places: %s\n", from, (int)sequence.number, (int)sequence.subNumber, names.front().c_str(),
			changed, to);
		return 0;
	}

	printf("  FAILED  the hierarchy %s has no one-frame sequence to be the reference\n", names.front().c_str());
	return 1;
}

static int CheckOffsets(char const *path)
{
	OpenLevelFile file;

	if (file.Open(path) != LevelOpenResult::Opened || !file.Level())
	{
		printf("  FAILED  the game's loader reads the saved model: %s\n", path);
		return 1;
	}

	std::vector<std::string> hierarchies, objects;

	HierarchyEdit_Names(&file, &hierarchies);
	HierarchyEdit_ObjectNames(&file, &objects);

	size_t attached = 0, pairs = 0;
	int failed = 0;

	for (std::string const &object : objects)
	{
		for (std::string const &hierarchy : hierarchies)
		{
			BoneAttachment one;

			if (HierarchyEdit_ReadBoneAttachment(&file, object, hierarchy, &one) != HierarchyEditResult::Done || !one.present)
			{
				continue;
			}

			attached++;

			std::vector<int32_t> world;

			if (HierarchyEdit_SkinPoints(&file, object, &world) != HierarchyEditResult::Done || one.offsets.size() != one.bones.size())
			{
				printf("  FAILED  %s / %s: its points and offsets are read\n", object.c_str(), hierarchy.c_str());
				failed++;
				continue;
			}

			size_t here = 0, nonZero = 0;
			std::string wrong;

			for (size_t b = 0; b < one.bones.size() && wrong.empty(); b++)
			{
				std::vector<int32_t> const &points = one.bones[b].second;

				for (size_t k = 0; k < points.size() && k < one.offsets[b].size(); k++)
				{
					nonZero += (one.offsets[b][k][0] != 0 || one.offsets[b][k][1] != 0 || one.offsets[b][k][2] != 0) ? 1 : 0;
				}

				for (size_t k = 1; k < points.size() && k < one.offsets[b].size() && wrong.empty(); k++)
				{
					size_t const i = (size_t)points[k - 1], j = (size_t)points[k];

					if (i * 3 + 2 >= world.size() || j * 3 + 2 >= world.size())
					{
						wrong = "a point past the shape's";
						break;
					}

					double w2 = 0.0, o2 = 0.0;

					for (int axis = 0; axis < 3; axis++)
					{
						double const w = (double)world[i * 3 + axis] - (double)world[j * 3 + axis];
						double const o = (double)one.offsets[b][k - 1][axis] - (double)one.offsets[b][k][axis];

						w2 += w * w;
						o2 += o * o;
					}

					if (sqrt(w2) <= 8.0)
					{
						continue;
					}

					here++;

					if (fabs(sqrt(w2) - sqrt(o2)) > 4.0)
					{
						wrong = one.bones[b].first + ": points " + std::to_string((long long)i) + " and " + std::to_string((long long)j) + " are " + std::to_string((long long)sqrt(w2)) +
							" mm apart, their offsets " + std::to_string((long long)sqrt(o2));
					}
				}
			}

			pairs += here;
			printf("  %s  %s / %s: %zu bones, %zu offsets not 0, %zu pairs of one bone keep their distance in their offsets%s\n", wrong.empty() && here > 0 ? "ok" : "FAILED", object.c_str(),
				hierarchy.c_str(), one.bones.size(), nonZero, here, wrong.empty() ? "" : (" (not: " + wrong + ")").c_str());
			failed += wrong.empty() && here > 0 ? 0 : 1;
		}
	}

	if (attached == 0)
	{
		printf("  FAILED  no object of %s is attached to a hierarchy\n", path);
		return 1;
	}

	printf(failed ? "FAILED verify_bone_offsets\n" : "ok verify_bone_offsets (%zu attached, %zu pairs)\n", attached, pairs);
	return failed ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc == 4 && strcmp(argv[1], "prepare") == 0)
	{
		return Prepare(argv[2], argv[3]);
	}

	if (argc == 3 && strcmp(argv[1], "check") == 0)
	{
		return CheckOffsets(argv[2]);
	}

	printf("verify_bone_offsets: prepare <model> <copy> | check <saved model>\n");
	return 2;
}
