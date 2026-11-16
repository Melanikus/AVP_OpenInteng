// The test of Path mode's data effects (src/host/path_mode.cpp): read the paths of a level, make a path through the module graph, change its flags, delete it, and have the
// game's own loader take the saved level. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS, on the game's own levels, given on the command line. The files are only read: every edit is made in memory (OpenLevelFile::ReplaceLevel) and the one save goes to
// a folder of the test's own.
// - The paths a level holds are read; the next free number is made as inteng makes it.
// - A path is written: from an empty working path the first module becomes the one node, a second module a few steps away adds the modules between (flags 1) and itself (flags 0);
//   the level then holds the path with those nodes, exactly once.
// - Properties: "Go back and forth" sets bit 2, "Loop path" clears it, bit 1 is never touched; the path is written again under the same number and there is still one chunk of it.
// - Delete Module takes the last node and the route's nodes before it; a path with no node is deleted from the level.
// - A number the game's loader refuses (256) is refused with the level's bytes and Generation() as they were.
// - The edited level is SAVED, and a second OpenLevelFile reads it through the game's own loader and finds the same paths.
//
// HOW IT RUNS: tests/host_path_mode_test.args builds it; run as host_path_mode_test <level file> [<level file> ...]. A non-zero result means a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "path_mode.h"

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

// ---- Stand-ins for the parts of the game the chunk code calls (as tests/level_file_test.cpp)

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

typedef std::vector<char> Bytes;

static Bytes Built(OpenLevelFile * file)
{
	Bytes bytes;

	if (!file->BuildLevel(bytes))
	{
		bytes.clear();
	}

	return bytes;
}

static size_t CountNumber(std::vector<PathInfo> const & paths, int32_t number)
{
	size_t count = 0;

	for (PathInfo const & each : paths)
	{
		count += each.number == number ? 1 : 0;
	}

	return count;
}

static PathInfo const * Find(std::vector<PathInfo> const & paths, int32_t number)
{
	for (PathInfo const & each : paths)
	{
		if (each.number == number)
		{
			return &each;
		}
	}

	return nullptr;
}

static void TestFile(std::string const & path, std::string const & folder)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	Bytes const original = Built(&file);
	std::vector<PathInfo> before;

	Check(!original.empty() && PathMode_ReadPaths(&file, &before), "the level builds and its paths are read");
	printf("  (%zu paths in the level)\n", before.size());

	int32_t number = -1;

	Check(PathMode_NextNumber(&file, &number) && number >= 0, "the next free number is made");
	Check(CountNumber(before, number) == 0, "the next free number is one no path of the level has (inteng's own loop gives 2 on Breakout, which a path has): " + std::to_string(number));

	// the module graph, and two modules a few steps apart
	PathGraph graph;
	std::vector<int32_t> numbers;
	std::vector<std::string> names;

	if (!PathMode_BuildGraph(&file, &graph, &numbers, &names) || graph.neighbours.size() < 3)
	{
		printf("  (no module graph to make a path through)\n");
		return;
	}

	int const first = 0;
	int second = -1;

	// a module at the end of a route of at least one module between: breadth-first from the first until a module two steps away is found
	{
		std::vector<int> depth(graph.neighbours.size(), -1);
		std::vector<int> queue(1, first);

		depth[(size_t)first] = 0;

		for (size_t at = 0; at < queue.size(); at++)
		{
			for (int const next : graph.neighbours[(size_t)queue[at]])
			{
				if (next >= 0 && (size_t)next < depth.size() && depth[(size_t)next] < 0)
				{
					depth[(size_t)next] = depth[(size_t)queue[at]] + 1;
					queue.push_back(next);

					if (second < 0 && depth[(size_t)next] == 2)
					{
						second = next;
					}
				}
			}
		}
	}

	if (second < 0)
	{
		printf("  (no two modules two steps apart)\n");
		return;
	}

	std::vector<PathNode> nodes;

	Check(PathMode_AddModule(&nodes, graph, numbers, -1, first) && nodes.size() == 1 && nodes[0].module == numbers[(size_t)first] && nodes[0].flags == 0,
		"the first module becomes the one node");
	Check(PathMode_AddModule(&nodes, graph, numbers, first, second) && nodes.size() >= 3 && nodes.back().module == numbers[(size_t)second] && nodes.back().flags == 0 &&
		(nodes[1].flags & 1) != 0, "a module two steps away adds the module between (flags 1) and itself (flags 0)");

	// writing the path
	unsigned const generation = file.Generation();
	std::string const name = "Zz test path";
	PathModeResult const written = PathMode_Write(&file, number, name.c_str(), 0, 0, nodes.data(), nodes.size());

	Check(written == PathModeResult::Done, std::string("the path is written: ") + PathModeWords(written));
	Check(file.Generation() == generation + 1, "the level was rebuilt once");

	std::vector<PathInfo> after;

	Check(PathMode_ReadPaths(&file, &after) && CountNumber(after, number) == 1, "the level holds the path once");
	Check(after.size() == before.size() + 1, "Create Path keeps every path the level had and adds one");

	PathInfo const * const made = Find(after, number);

	Check(made && made->name == name && made->nodes.size() == nodes.size() && made->nodes.back().module == nodes.back().module, "with its name and nodes");

	// properties
	int32_t const back = PathMode_FlagsAfterProperties(0x1, false);

	Check((back & 2) != 0 && (back & 1) != 0, "Go back and forth sets bit 2 and leaves bit 1");
	Check((PathMode_FlagsAfterProperties(0x3, true) & 2) == 0 && (PathMode_FlagsAfterProperties(0x3, true) & 1) != 0, "Loop path clears bit 2 and leaves bit 1");
	Check(PathMode_Write(&file, number, name.c_str(), back, 0, nodes.data(), nodes.size()) == PathModeResult::Done, "the path is written again with its flags");

	after.clear();
	Check(PathMode_ReadPaths(&file, &after) && CountNumber(after, number) == 1 && Find(after, number) && Find(after, number)->flags == back, "still one chunk, with the flags");

	// a number the loader refuses
	Bytes const withPath = Built(&file);
	unsigned const generationNow = file.Generation();

	Check(PathMode_Write(&file, 256, "x", 0, 0, nodes.data(), nodes.size()) == PathModeResult::BadNumber && file.Generation() == generationNow && Built(&file) == withPath,
		"a number past 255 is refused, nothing changed");

	// saved, and read by the game's own loader
	std::string const saved = folder + "\\saved.rif";

	if (file.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened, "the saved level is taken by the game's own loader");

		std::vector<PathInfo> reread;

		Check(PathMode_ReadPaths(&second, &reread) && CountNumber(reread, number) == 1 && Find(reread, number) && Find(reread, number)->nodes.size() == nodes.size(),
			"and holds the path");

		DeleteFileA(saved.c_str());
		DeleteFileA((saved + ".bak").c_str());
	}
	else
	{
		Check(false, "the edited level is saved");
	}

	// Delete Module, then a path with no node is deleted
	std::vector<PathNode> shorter = nodes;

	PathMode_DeleteLast(&shorter);
	Check(shorter.size() < nodes.size(), "Delete Module takes the last node (and the route's nodes before it)");
	Check(PathMode_Write(&file, number, name.c_str(), back, 0, shorter.data(), shorter.size()) == PathModeResult::Done, "the shorter path is written");

	after.clear();
	Check(PathMode_ReadPaths(&file, &after) && CountNumber(after, number) == (shorter.empty() ? 0u : 1u), "the level holds it once, or not at all with no node");
	Check(PathMode_Write(&file, number, name.c_str(), back, 0, nullptr, 0) == PathModeResult::Done || PathMode_Write(&file, number, name.c_str(), back, 0, nullptr, 0) == PathModeResult::NothingToDo,
		"Delete Path with no node");

	after.clear();
	Check(PathMode_ReadPaths(&file, &after) && CountNumber(after, number) == 0, "the path is gone");
}

// PathMode_NearestStep (FUN_004f9650 with FUN_004f89f0's distances): an Enemy's / Generator's path start, on graphs made here (no level needed)
static void TestNearestStep()
{
	auto steps = [](std::vector<int32_t> const &modules) { std::vector<PathNode> out; for (int32_t m : modules) { PathNode n = {}; n.module = m; out.push_back(n); } return out; };
	PathGraph chain;

	// 0 - 1 - 2 - 3, and 0 - 4; module i has the number 10 + i; 5 stands alone
	chain.neighbours = { { 1, 4 }, { 0, 2 }, { 1, 3 }, { 2 }, { 0 }, {} };
	std::vector<int32_t> const numbers = { 10, 11, 12, 13, 14, 15 };

	Check(PathMode_NearestStep(chain, numbers, 0, steps({ 12, 14 })) == 4, "the nearest of the path's modules by the breadth-first distance (14, one away, not 12, two away)");
	Check(PathMode_NearestStep(chain, numbers, 0, steps({ 11, 14 })) == 1, "two at the same distance: the first in the modules' order");
	Check(PathMode_NearestStep(chain, numbers, 3, steps({ 10, 13 })) == 3, "the start itself when it is on the path (distance 0)");
	Check(PathMode_NearestStep(chain, numbers, 0, steps({ 15 })) == -1, "a path no step of which can be reached: none");
	Check(PathMode_NearestStep(chain, numbers, 0, steps({})) == -1, "no steps: none");
	Check(PathMode_NearestStep(chain, numbers, -1, steps({ 12 })) == -1 && PathMode_NearestStep(chain, numbers, 6, steps({ 12 })) == -1, "a start outside the graph: none");
	Check(PathMode_NearestStep(chain, { 10, 11 }, 0, steps({ 11 })) == -1, "numbers that don't match the graph: none");

	// two modules with one number: a step names the FIRST (FUN_004beb90), even when the other is nearer
	PathGraph twins;

	twins.neighbours = { { 2 }, { 2 }, { 0, 1 } };
	Check(PathMode_NearestStep(twins, { 10, 11, 11 }, 0, steps({ 11 })) == 1, "a step names the first module with its number, two away, not its twin one away");

	// a hostile neighbour index is passed over
	PathGraph bad;

	bad.neighbours = { { -3, 99, 1 }, { 0 } };
	Check(PathMode_NearestStep(bad, { 10, 11 }, 0, steps({ 11 })) == 1, "a neighbour index outside the graph is passed over");
}

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestNearestStep();

	if (argc < 2)
	{
		// no level named (the suite runs every test with no arguments): only the checks above, which need none
		printf("usage: host_path_mode_test <level file> [<level file> ...]: no level was named, so only the graphs made here were tested\n");
		printf("%d checks, %d failed\n", checksRun, checksFailed);
		return checksFailed ? 1 : 0;
	}

	std::string const folder = "path_mode_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i], folder);
	}

	RemoveDirectoryA(folder.c_str());
	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
