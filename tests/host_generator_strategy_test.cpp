// The test of an enemy's and a generator's strategy (src/host/generator_strategy.cpp, and the lookup's generators, strategy_lookup.cpp ListGenerators / FindObjectStrategy): Strategy Data's
// Assign, Delete and list edits on kinds 69 Enemy and 79 Generator (reassess/strategy-data-kinds.md S-KC-6, S-K69-0, S-K79-0). Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_generator_strategy_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: the level's enemies and generators are listed with the game's own IDs and types; an enemy with no strategy is given a new Enemy (69) and a generator a new Generator (79), each found
// again by its ID with its kind; given bytes replace it; Delete takes it out and a second Delete says there is none; every other one's strategy stays byte for byte; the copy saved goes through
// the game's loader and holds the strategies; hostile calls (no ID, an ID no one has, a strategy over 64 KB, a kind the editor can't make) change nothing; the opened file is unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "generator_strategy.h"
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

static std::string Key(uint8_t const *id)
{
	return std::string((char const *)id, 8);
}

// Every enemy's and generator's strategy bytes, by ID (none: not in the map)
static std::map<std::string, std::vector<uint8_t>> Held(File_Chunk *level)
{
	std::vector<GeneratorRef> all;
	std::map<std::string, std::vector<uint8_t>> held;

	ListGenerators(level, &all);

	for (GeneratorRef const &one : all)
	{
		ObjectStrategy found;

		if (FindObjectStrategy(level, one.id, &found) == StrategyFound::Found && held.count(Key(one.id)) == 0)
		{
			held[Key(one.id)] = found.payload;
		}
	}

	return held;
}

static bool OthersUnchanged(File_Chunk *level, std::map<std::string, std::vector<uint8_t>> before, std::string const &a, std::string const &b)
{
	std::map<std::string, std::vector<uint8_t>> now = Held(level);

	before.erase(a);
	before.erase(b);
	now.erase(a);
	now.erase(b);
	return now == before;
}

static int32_t KindOf(File_Chunk *level, uint8_t const *id)
{
	ObjectStrategy found;
	int32_t kind = -1;

	if (FindObjectStrategy(level, id, &found) == StrategyFound::Found && found.payload.size() >= 12)
	{
		memcpy(&kind, found.payload.data() + 8, 4);
	}

	return kind;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_generator_strategy_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_generator_strategy_test (0 ok)\n");
		return 0;
	}

	std::vector<char> original;

	if (!ReadWholeFile(argv[1], &original))
	{
		printf("  FAILED  the level can be read: %s\n", argv[1]);
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	std::vector<GeneratorRef> all;

	ListGenerators(level.Level(), &all);

	// an enemy and a generator WITHOUT a strategy (the ones Assign has to make a holder for), and one of each kind for the rest
	GeneratorRef enemy = {}, generator = {};
	bool haveEnemy = false, haveGenerator = false;

	for (GeneratorRef const &one : all)
	{
		ObjectStrategy found;
		GeneratorRef first;
		bool const firstOfName = FindGenerator(level.Level(), one.id, &first) && first.generator == one.generator;
		bool const none = FindObjectStrategy(level.Level(), one.id, &found) == StrategyFound::NoStrategy;

		if (firstOfName && none && one.type != 0 && !haveEnemy)
		{
			enemy = one;
			haveEnemy = true;
		}
		else if (firstOfName && none && one.type == 0 && !haveGenerator)
		{
			generator = one;
			haveGenerator = true;
		}
	}

	printf("  (%zu enemies and generators; an enemy with no strategy: %s, a generator with none: %s)\n", all.size(), haveEnemy ? enemy.name.c_str() : "none", haveGenerator ? generator.name.c_str() : "none");
	Check(!all.empty() && (haveEnemy || haveGenerator), "the level has enemies or generators, and one with no strategy");

	if (all.empty() || (!haveEnemy && !haveGenerator))
	{
		printf("FAILED host_generator_strategy_test\n");
		return 1;
	}

	std::map<std::string, std::vector<uint8_t>> const before = Held(level.Level());
	std::string const enemyKey = haveEnemy ? Key(enemy.id) : std::string();
	std::string const generatorKey = haveGenerator ? Key(generator.id) : std::string();

	// hostile calls first: nothing changes
	uint8_t nobody[8] = { 1, 2, 3, 4, 5, 6, 7, 0 };
	std::vector<uint8_t> huge(65537, 0);
	uint8_t const *const someone = haveEnemy ? enemy.id : generator.id;

	Check(GeneratorStrategy_Assign(nullptr, someone, 69) == GeneratorStrategyResult::BadCall && GeneratorStrategy_Assign(&level, nullptr, 69) == GeneratorStrategyResult::BadCall, "no level or no ID is a bad call");
	Check(GeneratorStrategy_Assign(&level, nobody, 69) == GeneratorStrategyResult::NotFound, "an ID no enemy or generator has is not found");
	Check(GeneratorStrategy_Put(&level, someone, huge.data(), huge.size()) == GeneratorStrategyResult::TooBig, "a strategy over 64 KB is refused");
	Check(GeneratorStrategy_Put(&level, someone, huge.data(), 11) == GeneratorStrategyResult::BadCall, "and one shorter than a strategy's first three numbers");
	Check(GeneratorStrategy_Assign(&level, someone, 123456) == GeneratorStrategyResult::BadKind, "a kind the editor can't make is refused");
	Check(GeneratorStrategy_Delete(&level, someone) == GeneratorStrategyResult::NoStrategy, "Delete on one with no strategy says so");
	Check(Held(level.Level()) == before, "and none of that changed a strategy");

	if (haveEnemy)
	{
		GeneratorStrategyResult const made = GeneratorStrategy_Assign(&level, enemy.id, 69);
		Check(made == GeneratorStrategyResult::Done, std::string("Assign > Enemy on the enemy ") + enemy.name + ": " + GeneratorStrategyWords(made));
		Check(KindOf(level.Level(), enemy.id) == 69, "it is found again by its ID, an Enemy (69)");
	}

	if (haveGenerator)
	{
		GeneratorStrategyResult const made = GeneratorStrategy_Assign(&level, generator.id, 79);
		Check(made == GeneratorStrategyResult::Done, std::string("Assign > Generator on the generator ") + generator.name + ": " + GeneratorStrategyWords(made));
		Check(KindOf(level.Level(), generator.id) == 79, "it is found again by its ID, a Generator (79)");
	}

	Check(OthersUnchanged(level.Level(), before, enemyKey, generatorKey), "every other enemy's and generator's strategy is as it was");

	// given bytes replace it (an Enemy whose Mission is 2, Guard)
	if (haveEnemy)
	{
		ObjectStrategy now;

		if (FindObjectStrategy(level.Level(), enemy.id, &now) == StrategyFound::Found && now.payload.size() >= 24)
		{
			std::vector<uint8_t> changed = now.payload;
			int32_t const guard = 2;

			memcpy(changed.data() + 20, &guard, 4);
			Check(GeneratorStrategy_Put(&level, enemy.id, changed.data(), changed.size()) == GeneratorStrategyResult::Done, "given bytes are put in its place");

			ObjectStrategy again;
			Check(FindObjectStrategy(level.Level(), enemy.id, &again) == StrategyFound::Found && again.payload == changed, "and read back as given");
		}
	}

	// saved as a copy and read again by the game's loader
	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "generator-strategy-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	{
		OpenLevelFile reopened;

		Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

		if (reopened.Level())
		{
			Check((!haveEnemy || KindOf(reopened.Level(), enemy.id) == 69) && (!haveGenerator || KindOf(reopened.Level(), generator.id) == 79), "read again from the copy, the new strategies are there");
			Check(OthersUnchanged(reopened.Level(), before, enemyKey, generatorKey), "and every other one is as it was");
		}
	}

	// Delete takes it out; a second Delete says there is none
	uint8_t const *const taken = haveEnemy ? enemy.id : generator.id;

	Check(GeneratorStrategy_Delete(&level, taken) == GeneratorStrategyResult::Done, "Delete takes the strategy out");

	ObjectStrategy none;

	Check(FindObjectStrategy(level.Level(), taken, &none) == StrategyFound::NoStrategy, "and it has none now");
	Check(GeneratorStrategy_Delete(&level, taken) == GeneratorStrategyResult::NoStrategy, "a second Delete says there is none");

	DeleteFileA(copyPath.c_str());
	DeleteFileA((copyPath + ".bak").c_str());

	std::vector<char> originalAfter;
	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_generator_strategy_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_generator_strategy_test\n");
	return 0;
}
