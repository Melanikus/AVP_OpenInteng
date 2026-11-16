// The test of Hierarchy mode's view items (src/host/hierarchy_mode.cpp): Adaptive Degradation 0x45, Dummy Objects 0x5c, Dummy Object Size 0x5d, Alien 0x3c, Marine 0x3d and Help 0x38. None of them touches the
// file, so no level is needed. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_hierarchy_view_items_test.args builds it; host_hierarchy_view_items_test. A non-zero result means a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "hierarchy_mode.h"

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const & what)
{
	checksRun++;

	if (!good)
	{
		printf("  FAILED  %s\n", what.c_str());
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char * name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char * message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

using namespace host;

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	HierarchyMode mode;

	Check(HierarchyMode_Does(0x45) && HierarchyMode_Does(0x5c) && HierarchyMode_Does(0x5d) && HierarchyMode_Does(0x3c) && HierarchyMode_Does(0x3d) && HierarchyMode_Does(0x38), "the mode runs the six items");

	HierarchyStep step = HierarchyMode_Command(nullptr, &mode, 0x45);

	Check(mode.adaptiveDegradation && !step.prompt && step.answer.find("Adaptive Degradation: on") == 0, "Adaptive Degradation turns on: " + step.answer);
	HierarchyMode_Command(nullptr, &mode, 0x45);
	Check(!mode.adaptiveDegradation, "and off");

	step = HierarchyMode_Command(nullptr, &mode, 0x5c);
	Check(mode.dummyObjects && step.answer.find("Dummy Objects: on") == 0 && step.answer.find("400 mm") != std::string::npos, "Dummy Objects turns on with the cube of 400 mm: " + step.answer);
	HierarchyMode_Command(nullptr, &mode, 0x5c);
	Check(!mode.dummyObjects, "and off");

	step = HierarchyMode_Command(nullptr, &mode, 0x5d);
	Check(step.prompt && step.defaultText == "400" && step.title == "Dummy object size in mm", "Dummy Object Size asks in mm with 400 offered");
	step = HierarchyMode_Typed(nullptr, &mode, "250");
	Check(mode.dummyObjectSize == 25000 && step.answer.find("250 mm") != std::string::npos, "250 mm is kept as 25000");
	HierarchyMode_Command(nullptr, &mode, 0x5d);
	HierarchyMode_Typed(nullptr, &mode, "0");
	Check(mode.dummyObjectSize == 25000, "a size of 0 changes nothing");
	HierarchyMode_Command(nullptr, &mode, 0x5d);
	HierarchyMode_Typed(nullptr, &mode, "abc");
	Check(mode.dummyObjectSize == 25000, "and so does a word");

	HierarchyMode_Command(nullptr, &mode, 0x3c);
	Check(mode.viewKind == 1, "Alien");
	HierarchyMode_Command(nullptr, &mode, 0x3d);
	Check(mode.viewKind == 2, "Marine");

	step = HierarchyMode_Command(nullptr, &mode, 0x38);
	Check(step.prompt && mode.list.size() > 20 && mode.list[0] == "H  Select hierarchy", "Help shows the first page");
	step = HierarchyMode_Typed(nullptr, &mode, "");
	Check(step.prompt && mode.list.size() > 8 && mode.list[0] == "Boned object commands", "Enter shows the second page");
	step = HierarchyMode_Typed(nullptr, &mode, "");
	Check(!step.prompt && mode.list.empty(), "and the next Enter closes it");

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
