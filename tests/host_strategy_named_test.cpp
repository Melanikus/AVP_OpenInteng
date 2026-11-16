// The test of the strategy edits on an object other objects NAME (Strategy mode's Copy 0x6, Assign 0x8, Delete 0xc: inteng's FUN_00528340, FUN_00528830, FUN_00528790 never ask who names the
// object; Bishop's ruling 2026-11-16, reassess/strategy-data-kinds.md S-KC-15). Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY THERE IS NO REFUSAL: the names stay in the level and the game follows them by name each time a level starts (AssignAllSBNames, avpx bh_types.cpp): a name that finds nothing is logged and left null,
// and every use of what a name finds is null-safe and checks its kind (RequestState / GetState switch on the kind, a switch door's partner and a lift's controller are checked, a link switch checks
// both, every save-game block checks the kind). A level from anyone can already name any object from any strategy, so nothing new can happen here.
//
// USE: host_strategy_named_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: an object with a strategy whose 8-byte ID other bytes of the level hold (a name another object holds) is given another object's strategy (Copy), a new one (Assign) and none (Delete), each
// answered Done; after each, every OTHER object's strategy is byte for byte what it was (the names they hold stay); the level saved as a COPY goes through the game's loader, the object is there with
// no strategy and the others' strategies are unchanged; the opened file is unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_edit.h"
#include "strategy_lookup.h"

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

static size_t Occurrences(std::vector<char> const &bytes, unsigned char const *id)
{
	size_t count = 0;

	for (size_t i = 0; i + 8 <= bytes.size(); i++)
	{
		if (memcmp(bytes.data() + i, id, 8) == 0)
		{
			count++;
		}
	}

	return count;
}

static std::string Key(unsigned char const *id)
{
	return std::string((char const *)id, 8);
}

// Every object's ID, and the strategy bytes of each object that has one
static void Strategies(OpenLevelFile &level, std::vector<std::string> *ids, std::map<std::string, std::vector<uint8_t>> *held)
{
	ids->clear();
	held->clear();

	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (!each())
		{
			continue;
		}

		unsigned char id[8];
		memcpy(id, &each()->object_data.ID, 8);
		ids->push_back(Key(id));

		host::ObjectStrategy found;

		if (host::FindObjectStrategy(level.Level(), id, &found) == host::StrategyFound::Found)
		{
			(*held)[Key(id)] = found.payload;
		}
	}
}

// Every object but `target` holds the strategy it held before, byte for byte
static bool OthersUnchanged(OpenLevelFile &level, std::map<std::string, std::vector<uint8_t>> const &before, std::string const &target)
{
	std::vector<std::string> ids;
	std::map<std::string, std::vector<uint8_t>> now;

	Strategies(level, &ids, &now);
	now.erase(target);

	std::map<std::string, std::vector<uint8_t>> expected = before;

	expected.erase(target);
	return now == expected;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_strategy_named_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_strategy_named_test (0 ok)\n");
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

	std::vector<char> image;
	Check(level.BuildLevel(image), "the level is built from what the editor read");

	std::vector<std::string> ids;
	std::map<std::string, std::vector<uint8_t>> before;

	Strategies(level, &ids, &before);

	// an object's ID is made from its name and is not in its own chunk, so bytes beyond the fewest any object's ID has are another object's name for it: the target is the object WITH a strategy
	// named most often; the source of the copy is another object with a strategy
	size_t baseline = (size_t)-1;
	size_t namedBefore = 0;
	std::string target;
	std::string source;

	for (std::string const &id : ids)
	{
		size_t const found = Occurrences(image, (unsigned char const *)id.data());

		baseline = found < baseline ? found : baseline;

		if (before.count(id) && found > namedBefore)
		{
			namedBefore = found;
			target = id;
		}
	}

	for (auto const &each : before)
	{
		if (each.first != target)
		{
			source = each.first;
			break;
		}
	}

	bool const haveTarget = !target.empty() && namedBefore > baseline && !source.empty();

	Check(haveTarget, "the level has an object with a strategy that other objects name (" + std::to_string(namedBefore) + " places hold its ID, the fewest " + std::to_string(baseline) + ")");

	if (!haveTarget)
	{
		printf("FAILED host_strategy_named_test (no object with a strategy here is named by another)\n");
		return 1;
	}

	unsigned char const *const targetId = (unsigned char const *)target.data();
	unsigned char const *const sourceId = (unsigned char const *)source.data();

	// Copy (0x6): the target is given the source's strategy, in place of its own
	host::ObjectEditResult result = host::CopyStrategy(&level, sourceId, targetId);
	Check(result == host::ObjectEditResult::Done, std::string("Copy: the named object is given another's strategy: ") + host::ObjectEditWords(result));
	{
		host::ObjectStrategy now;
		Check(host::FindObjectStrategy(level.Level(), targetId, &now) == host::StrategyFound::Found && now.payload == before[source], "and holds the source's bytes");
	}
	Check(OthersUnchanged(level, before, target), "every other object's strategy is as it was: the names they hold stay");

	// Assign (0x8): a new Door (50) in place of what it holds
	result = host::AssignStrategy(&level, targetId, 50);
	Check(result == host::ObjectEditResult::Done, std::string("Assign: the named object is given a new strategy: ") + host::ObjectEditWords(result));
	Check(OthersUnchanged(level, before, target), "every other object's strategy is as it was");

	// Delete (0xc): the strategy taken out
	result = host::DeleteStrategy(&level, targetId);
	Check(result == host::ObjectEditResult::Done, std::string("Delete: the named object's strategy is taken out: ") + host::ObjectEditWords(result));
	{
		host::ObjectStrategy none;
		Check(host::FindObjectStrategy(level.Level(), targetId, &none) == host::StrategyFound::NoStrategy, "it has no strategy now");
	}
	Check(OthersUnchanged(level, before, target), "every other object's strategy is as it was");

	std::vector<char> after;
	Check(level.BuildLevel(after), "the changed level is built");
	Check(Occurrences(after, targetId) >= baseline + 1, "the names the others hold for it are still in the level (" + std::to_string(Occurrences(after, targetId)) + ")");

	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "strategy-named-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;
	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		host::ObjectStrategy none;
		Check(host::FindObjectStrategy(reopened.Level(), targetId, &none) == host::StrategyFound::NoStrategy, "read again from the copy the object is there with no strategy");
		Check(OthersUnchanged(reopened, before, target), "and every other object's strategy is as it was");
	}

	DeleteFileA(copyPath.c_str());
	DeleteFileA((copyPath + ".bak").c_str());

	std::vector<char> originalAfter;
	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_strategy_named_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_strategy_named_test\n");
	return 0;
}
