// The test of StrategyMode_HasEach (strategy_mode.h): for every object of a stock level, the same answer as StrategyMode_Has, which builds and walks the whole level again for each
// object (Strategy Data's green marks asked it for every object each time the level changed: 3 s and more on Derelict after a Create Virtual Object, the owner's "3 second lock",
// Vasquez's probe 2026-11-24). Also an ID no object has (NoObject) and no level (BadCall). The two times are printed, not checked (a busy machine moves them). Needs the stock levels
// (its .runargs: Derelict, Genshd1). Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <string>
#include <vector>

#include "chunk_object.hpp"
#include "level_file.h"
#include "strategy_mode.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as tests/host_strategy_stock_roundtrip_test). The game's loader opens a level with no sharing (mishchnk.cpp: share 0),
// so two tests reading one stock level at the same time would refuse each other; an open for reading only is shared for reading here, which changes nothing the loader reads.
HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (!(access & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)))
	{
		share |= FILE_SHARE_READ;
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

static void OneLevel(char const *path)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, std::string("the level opens: ") + path);
		return;
	}

	// every object of the level by its ID (the library's own list), then an ID none has
	std::vector<uint8_t> ids;
	List<Object_Chunk *> found;

	level.Level()->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		if (Object_Chunk *const object = each())
		{
			uint8_t const *const id = (uint8_t const *)&object->object_data.ID;

			ids.insert(ids.end(), id, id + 8);
		}
	}

	size_t const objects = ids.size() / 8;
	uint8_t const none[8] = { 0xfe, 0xed, 0xfa, 0xce, 0x12, 0x34, 0x56, 0x78 };

	ids.insert(ids.end(), none, none + 8);

	std::vector<host::StrategyModeResult> one, each;
	auto const oneFrom = std::chrono::steady_clock::now();

	for (size_t i = 0; i < objects + 1; i++)
	{
		one.push_back(host::StrategyMode_Has(&level, ids.data() + i * 8));
	}

	double const oneMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - oneFrom).count();
	auto const eachFrom = std::chrono::steady_clock::now();

	host::StrategyMode_HasEach(&level, ids.data(), objects + 1, &each);

	double const eachMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - eachFrom).count();
	size_t same = 0, done = 0, without = 0;

	for (size_t i = 0; i < one.size() && i < each.size(); i++)
	{
		same += one[i] == each[i] ? 1 : 0;
		done += each[i] == host::StrategyModeResult::Done ? 1 : 0;
		without += each[i] == host::StrategyModeResult::NoStrategy ? 1 : 0;
	}

	std::string const name = strrchr(path, '\\') ? strrchr(path, '\\') + 1 : (strrchr(path, '/') ? strrchr(path, '/') + 1 : path);

	printf("  (%s: %zu objects; one by one %.0f ms, all at once %.0f ms)\n", name.c_str(), objects, oneMs, eachMs);
	Check(objects > 100, name + ": the level has its objects (" + std::to_string(objects) + ")");
	Check(each.size() == objects + 1 && same == objects + 1, name + ": every answer the same as StrategyMode_Has's (" + std::to_string(same) + " of " + std::to_string(objects + 1) + ")");
	Check(done > 0 && without > 0, name + ": some objects hold a strategy (" + std::to_string(done) + ") and some don't (" + std::to_string(without) + ")");
	Check(!each.empty() && each.back() == host::StrategyModeResult::NoObject, name + ": an ID no object has: NoObject");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	std::vector<host::StrategyModeResult> answers;
	uint8_t const id[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	host::StrategyMode_HasEach(nullptr, id, 1, &answers);
	Check(answers.size() == 1 && answers[0] == host::StrategyModeResult::BadCall, "no level: BadCall, one answer for the one ID");

	if (argc < 3)
	{
		printf("  FAILED  usage: host_strategy_mode_has_test <Derelict .rif> <Genshd1 .rif>\n");
		return 1;
	}

	OneLevel(argv[1]);
	OneLevel(argv[2]);

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
