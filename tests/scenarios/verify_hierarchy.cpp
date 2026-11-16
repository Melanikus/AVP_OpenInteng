// verify_hierarchy.cpp: the check program of the Hierarchy scenario (tests/scenarios/hierarchy-main-job.txt): a model edited in the running editor and saved by Save a copy is read back
// through the game's own loader and compared with the original model. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USAGE: verify_hierarchy <the original model .rif> <the saved copy> <hierarchy name> <sequence number> <sub number> <wav name>
// It passes (exit 0) when, in the copy, against the original: (1) the sound list holds one sound more, named <wav name>, with the volume 100 and the loop bit clear; (2) exactly one root
// frame of the sequence differs from the original, by gaining keyframe flag 6 and that sound's index; (3) the copy's sequence has the same frames count as the original. Lines say what was found.

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

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		// no arguments (the suite runs every .args with none): nothing to check, and it says so, as the other check programs do
		printf("usage: verify_hierarchy <original model> <saved copy> <hierarchy> <sequence> <sub> <wav name>: no files were named, so nothing was checked here\n");
		printf("All checks passed\n");
		return 0;
	}

	if (argc < 7)
	{
		printf("usage: verify_hierarchy <original model> <saved copy> <hierarchy> <sequence> <sub> <wav name>\n");
		return 2;
	}

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened, "the original model opens");
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened, "the saved copy is taken by the game's own loader");

	std::string const hierarchy = argv[3];
	int32_t const number = atoi(argv[4]);
	int32_t const sub = atoi(argv[5]);
	std::string const wav = argv[6];

	std::vector<SoundInfo> before, after;

	HierarchyEdit_Sounds(&original, &before);
	HierarchyEdit_Sounds(&copy, &after);
	Check(after.size() == before.size() + 1, "the copy has one sound more (" + std::to_string((unsigned long long)before.size()) + " -> " + std::to_string((unsigned long long)after.size()) + ")");

	int32_t added = 0;

	for (SoundInfo const &each : after)
	{
		bool known = false;

		for (SoundInfo const &old : before)
		{
			known = known || old.index == each.index;
		}

		if (!known)
		{
			added = each.index;
			Check(each.wav == wav && each.volume == 100 && (each.flags & 1) == 0, "the new sound " + std::to_string(each.index) + " is " + each.wav + ", volume " + std::to_string(each.volume));
		}
	}

	Check(added > 0, "a new sound index was found");

	std::vector<FrameInfo> a, b;

	Check(HierarchyEdit_RootFrames(&original, hierarchy, number, sub, &a) == HierarchyEditResult::Done && HierarchyEdit_RootFrames(&copy, hierarchy, number, sub, &b) == HierarchyEditResult::Done, "both have the root frames of the sequence");
	Check(a.size() == b.size() && !a.empty(), "the frame counts are the same (" + std::to_string((unsigned long long)a.size()) + ")");

	size_t differing = 0;

	for (size_t i = 0; i < a.size() && i < b.size(); i++)
	{
		if (a[i].flags != b[i].flags)
		{
			differing++;
			Check((b[i].flags & (1u << 6)) != 0 && (int32_t)((b[i].flags >> 24) & 0x7f) == added, "frame " + std::to_string((unsigned long long)i + 1) + " gained keyframe flag 6 and sound " + std::to_string(added));
		}
	}

	Check(differing == 1, "exactly one frame differs (" + std::to_string((unsigned long long)differing) + ")");
	printf("%s\n", failed ? "FAILED" : "all passed");
	return failed ? 1 : 0;
}
