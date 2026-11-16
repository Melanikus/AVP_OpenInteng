// The test for Module Overlaps (src/host/module_overlaps.*). Written 2026-11-15 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_module_overlaps_test.exe <stock level .rif> ...  With no arguments it says so and passes.
//  1. the numbers (worked by hand from FUN_00444c80): boxes that overlap by more than 1000 on every axis are Large, 101..1000 Medium, 3..100 Small, 1..2 Touching, 0 or less on any axis None; the smallest axis decides; every
//     part of an edge is divided by 100 as a whole number first (so 99 mm of overlap is 0); the answer for a pair is the same both ways; the worst of several is kept whatever the order;
//  2. on each stock level: the boxes of the modules are read (every object with a shape and no placed-object flag), there are some, every module has a box with extents that go from min to max, and the worst-overlap list has one
//     entry per box and agrees with a direct pair-by-pair count.

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
#include "module_overlaps.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("  the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("  FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

static host::ModuleBox Box(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t z0, int32_t z1, int32_t px = 0, int32_t py = 0, int32_t pz = 0)
{
	host::ModuleBox box = {};

	box.extents[0] = x1;   // max x
	box.extents[1] = x0;   // min x
	box.extents[2] = y1;
	box.extents[3] = y0;
	box.extents[4] = z1;
	box.extents[5] = z0;
	box.place[0] = px;
	box.place[1] = py;
	box.place[2] = pz;
	return box;
}

static void Numbers()
{
	printf("THE NUMBERS\n");

	using host::OverlapSeverity;

	// units are file units; every part is divided by 100 first, so a box 0..200000 is 0..2000 and two such boxes 100000 apart overlap by 1000
	host::ModuleBox const base = Box(0, 200000, 0, 200000, 0, 200000);

	Check(host::ModuleOverlaps_Between(base, Box(100000, 300000, 0, 200000, 0, 200000)) == OverlapSeverity::Medium, "an overlap of exactly 1000 is Medium (the red one is above 1000)");
	Check(host::ModuleOverlaps_Between(base, Box(99900, 300000, 0, 200000, 0, 200000)) == OverlapSeverity::Large, "an overlap of 1001 is Large");
	Check(host::ModuleOverlaps_Between(base, Box(190000, 300000, 0, 200000, 0, 200000)) == OverlapSeverity::Small, "an overlap of 100 is Small (3 to 100)");
	Check(host::ModuleOverlaps_Between(base, Box(199800, 300000, 0, 200000, 0, 200000)) == OverlapSeverity::Touching, "an overlap of 2 is Touching");
	Check(host::ModuleOverlaps_Between(base, Box(199700, 300000, 0, 200000, 0, 200000)) == OverlapSeverity::Small, "an overlap of 3 is Small");
	Check(host::ModuleOverlaps_Between(base, Box(200000, 300000, 0, 200000, 0, 200000)) == OverlapSeverity::None, "boxes that only share a face (overlap 0) are None");
	Check(host::ModuleOverlaps_Between(base, Box(100000, 300000, 300000, 400000, 0, 200000)) == OverlapSeverity::None, "an axis that does not overlap makes it None whatever the others are");

	// the smallest axis: x overlaps 1000 (Medium range), y 150 (Medium), z 2000: the smallest, 150, is Medium
	Check(host::ModuleOverlaps_Between(base, Box(100000, 300000, 185000, 300000, 0, 200000)) == OverlapSeverity::Medium, "the smallest of the three axes decides");

	// the place is added to the extents
	Check(host::ModuleOverlaps_Between(Box(0, 100000, 0, 100000, 0, 100000, 0, 0, 0), Box(0, 100000, 0, 100000, 0, 100000, 50000, 0, 0)) == OverlapSeverity::Medium, "the object's place moves its box");

	// whole-number division: 99 of overlap is 0 hundreds
	Check(host::ModuleOverlaps_Between(Box(0, 99, 0, 99, 0, 99), Box(1, 99, 0, 99, 0, 99)) == OverlapSeverity::None, "small boxes divide away to nothing (each part by 100 as a whole number)");

	// symmetry and the worst of several
	std::vector<host::ModuleBox> boxes = { base, Box(100000, 300000, 0, 200000, 0, 200000), Box(199800, 300000, 0, 200000, 0, 200000), Box(900000, 950000, 900000, 950000, 900000, 950000) };
	std::vector<host::OverlapSeverity> worst = host::ModuleOverlaps_Worst(boxes);

	Check(host::ModuleOverlaps_Between(boxes[0], boxes[1]) == host::ModuleOverlaps_Between(boxes[1], boxes[0]), "the answer for a pair is the same both ways");
	Check(worst.size() == 4 && worst[0] == OverlapSeverity::Medium && worst[1] == OverlapSeverity::Large && worst[2] == OverlapSeverity::Large && worst[3] == OverlapSeverity::None, "the worst overlap each box has with any other is kept");

	std::vector<host::ModuleBox> reversed(boxes.rbegin(), boxes.rend());
	std::vector<host::OverlapSeverity> worstBack = host::ModuleOverlaps_Worst(reversed);
	bool same = true;

	for (size_t i = 0; i < boxes.size(); i++)
	{
		same = same && worst[i] == worstBack[boxes.size() - 1 - i];
	}

	Check(same, "the order of the boxes does not change what each gets");
	Check(host::ModuleOverlaps_Rgb(OverlapSeverity::Large) == 0xFF0000u && host::ModuleOverlaps_Rgb(OverlapSeverity::Medium) == 0x7F007Fu && host::ModuleOverlaps_Rgb(OverlapSeverity::Small) == 0xFF7E00u &&
		host::ModuleOverlaps_Rgb(OverlapSeverity::Touching) == 0xFFFF00u && host::ModuleOverlaps_Rgb(OverlapSeverity::None) == 0, "the colours are inteng's palette masks worked out");
}

static void OnLevel(char const *path)
{
	printf("LEVEL %s\n", path);

	OpenLevelFile open;

	if (open.Open(path) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	std::vector<host::ModuleBox> boxes;

	Check(host::ModuleOverlaps_ReadBoxes(&open, &boxes) && !boxes.empty(), "the modules' boxes are read (" + std::to_string(boxes.size()) + ")");

	bool ordered = true;

	for (host::ModuleBox const &box : boxes)
	{
		ordered = ordered && box.extents[0] >= box.extents[1] && box.extents[2] >= box.extents[3] && box.extents[4] >= box.extents[5];
	}

	Check(ordered, "every box goes from its min to its max on each axis");

	std::vector<host::OverlapSeverity> worst = host::ModuleOverlaps_Worst(boxes);
	size_t direct = 0, listed = 0;

	for (size_t i = 0; i < boxes.size(); i++)
	{
		bool any = false;

		for (size_t j = 0; j < boxes.size() && !any; j++)
		{
			any = j != i && host::ModuleOverlaps_Between(boxes[i], boxes[j]) != host::OverlapSeverity::None;
		}

		direct += any ? 1 : 0;
		listed += worst[i] != host::OverlapSeverity::None ? 1 : 0;
	}

	Check(worst.size() == boxes.size() && direct == listed, "the list agrees with a pair-by-pair count (" + std::to_string(listed) + " of " + std::to_string(boxes.size()) + " modules overlap another)");
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("host_module_overlaps_test: give stock levels (.rif); with none there is nothing to run.\n");
		printf("passed host_module_overlaps_test (0 ok)\n");
		return 0;
	}

	Numbers();

	for (int i = 1; i < argc; i++)
	{
		OnLevel(argv[i]);
	}

	if (checksFailed)
	{
		printf("FAILED host_module_overlaps_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_module_overlaps_test\n");
	return 0;
}
