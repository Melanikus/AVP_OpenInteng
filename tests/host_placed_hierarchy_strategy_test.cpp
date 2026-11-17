// The test of src/host/placed_hierarchy_strategy (a placed hierarchy's strategy, kind 76) and strategy_lookup's placed hierarchies, on the stock levels that have one (Invasion's "dropship ken",
// Furyall's "pred ship": each holds a kind 76 strategy with one flag effect). Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USE: host_placed_hierarchy_strategy_test.exe <level> [<level> ...] (tests/host_placed_hierarchy_strategy_test.runargs). Each level is read into memory and changed there only: the file is not written.
// Per level: the placed hierarchies are listed with their names and IDs; the first one's strategy found by its ID; taken out (then none, and taking it out again says so); a new kind 76 put in (the
// creation values); the old one put back (the payload grows again) and read back byte for byte; its name and place kept; a kind the game refuses on a placed hierarchy (62) refused by the game's loader
// with nothing changed; an ID no placed hierarchy has, a payload too short and one over 64 KB refused.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "field_model.h"
#include "placed_hierarchy_strategy.h"
#include "strategy_lookup.h"

using namespace host;

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

static int32_t KindOf(std::vector<uint8_t> const &payload)
{
	int32_t kind = -1;

	if (payload.size() >= 12)
	{
		memcpy(&kind, payload.data() + 8, 4);
	}

	return kind;
}

static void TestLevel(char const *path)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, std::string("the game's loader takes ") + path);
		return;
	}

	std::vector<PlacedHierarchyRef> placed;

	ListPlacedHierarchies(level.Level(), &placed);
	Check(!placed.empty(), std::string(path) + ": its placed hierarchies listed (" + std::to_string(placed.size()) + ")");

	if (placed.empty())
	{
		return;
	}

	PlacedHierarchyRef const first = placed[0];
	uint8_t id[8];
	ObjectStrategy now;

	memcpy(id, first.id, 8);
	Check(FindObjectStrategy(level.Level(), id, &now) == StrategyFound::Found && KindOf(now.payload) == 76, "\"" + first.name + "\": its strategy found by its ID, kind 76");

	std::vector<uint8_t> const original = now.payload;

	// taken out, then nothing to take out
	Check(PlacedHierarchyStrategy_Delete(&level, id) == PlacedHierarchyStrategyResult::Done && FindObjectStrategy(level.Level(), id, &now) == StrategyFound::NoStrategy,
		"Delete: the strategy is taken out; the placed hierarchy is still found, with none");
	Check(PlacedHierarchyStrategy_Delete(&level, id) == PlacedHierarchyStrategyResult::NoStrategy, "Delete again: it has no strategy");

	// a new kind 76: the model's creation values
	uint8_t made[1024];
	size_t madeLength = 0;

	model::MakeNewStrategy(76, made, sizeof(made), &madeLength);
	Check(PlacedHierarchyStrategy_Assign(&level, id, 76) == PlacedHierarchyStrategyResult::Done && FindObjectStrategy(level.Level(), id, &now) == StrategyFound::Found &&
		now.payload == std::vector<uint8_t>(made, made + madeLength), "Assign 76: a new Hierarchy Object, its creation values (" + std::to_string(madeLength) + " bytes)");

	// the old one back: the payload grows (its flag effect and target), read back byte for byte, the name and place kept
	Check(PlacedHierarchyStrategy_Put(&level, id, original.data(), original.size()) == PlacedHierarchyStrategyResult::Done && FindObjectStrategy(level.Level(), id, &now) == StrategyFound::Found &&
		now.payload == original, "Put: the first strategy back, " + std::to_string(original.size()) + " bytes, read back as put");

	PlacedHierarchyRef again;

	Check(FindPlacedHierarchy(level.Level(), id, &again) && again.name == first.name && memcmp(again.place, first.place, sizeof(first.place)) == 0, "its name and place are kept");

	// the game's loader refuses a placed hierarchy holding another kind (avpx hierplace.cpp): nothing changes
	PlacedHierarchyStrategyResult const other = PlacedHierarchyStrategy_Assign(&level, id, 62);

	Check(other == PlacedHierarchyStrategyResult::LoaderRefused && FindObjectStrategy(level.Level(), id, &now) == StrategyFound::Found && now.payload == original,
		std::string("Assign 62 (Placed Object): refused by the game's loader, nothing changed (") + PlacedHierarchyStrategyWords(other) + ")");

	// hostile calls
	uint8_t nobody[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	std::vector<uint8_t> const huge(65537, 0);

	Check(PlacedHierarchyStrategy_Put(&level, nobody, original.data(), original.size()) == PlacedHierarchyStrategyResult::NotFound, "an ID no placed hierarchy has: not found");
	Check(PlacedHierarchyStrategy_Put(&level, id, original.data(), 11) == PlacedHierarchyStrategyResult::BadCall, "a payload shorter than a strategy's head: refused");
	Check(PlacedHierarchyStrategy_Put(&level, id, huge.data(), huge.size()) == PlacedHierarchyStrategyResult::TooBig, "a payload over 64 KB: refused");
	Check(PlacedHierarchyStrategy_Put(nullptr, id, original.data(), original.size()) == PlacedHierarchyStrategyResult::BadCall, "no level: refused");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("  FAILED  give the levels with a placed hierarchy (tests/host_placed_hierarchy_strategy_test.runargs)\n");
		return 1;
	}

	for (int a = 1; a < argc; a++)
	{
		TestLevel(argv[a]);
	}

	if (checksFailed)
	{
		printf("FAILED host_placed_hierarchy_strategy_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_placed_hierarchy_strategy_test\n");
	return 0;
}
