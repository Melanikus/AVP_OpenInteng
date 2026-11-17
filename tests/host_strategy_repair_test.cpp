// The test of Find Errors on a Mission Objective (kind 70; FUN_005335f0; row S-K70-R of AvP-docs\engine\inteng\reassess\strategy-data-kinds.md, SKD-70-7 of strategy-kinds-dietrich.md): every
// alteration whose target no object answers to is removed, the LAST entry moved into its place, the count shrunk. Written 2026-11-16 by Melanikus (Dietrich, then Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_strategy_repair_test.exe <a stock level .rif> <a folder it may write its copy into> <a stock level with a Track Object (Destructable), Area52>. The levels are only read; the copy is the only file written.
// THE CASE: no stock level has a Mission Objective with alterations (AvP-docs\tools\drake\kind-survey, 2026-11-16: every one has 0), so the test gives the level's first Mission Objective four by the
// editor's own SetStrategyBytes: an ID no object has, a virtual object, another unknown ID, another virtual object (U1, V1, U2, V2). inteng's walk (FUN_005335f0 0x533626-0x53363e): U1 is
// rejected, the last entry V2 takes its place and is looked at in turn (kept), V1 is kept, U2 is rejected and is the last, so nothing moves: the list is V2, V1. A second pass changes nothing; the
// request targets after the list are as they were; the copy is taken by the game's loader.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_edit.h"
#include "strategy_lookup.h"
#include "level_store.h"
#include "strategy_repair.h"
#include "virtual_object.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as host_strategy_named_test)

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

static int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
{
	int32_t value = 0;

	if (at + 4 <= bytes.size())
	{
		memcpy(&value, bytes.data() + at, 4);
	}

	return value;
}

// The kind-70 strategy bytes of a virtual object, or false
static bool MissionBytes(OpenLevelFile &level, uint8_t const *id, std::vector<uint8_t> *out)
{
	host::ObjectStrategy found;

	if (host::FindObjectStrategy(level.Level(), id, &found) != host::StrategyFound::Found || found.payload.size() < 44 || IntAt(found.payload, 8) != 70)
	{
		return false;
	}

	*out = found.payload;
	return true;
}

// One alteration entry: the target's ID and its word
static std::vector<uint8_t> Entry(uint8_t const *id, int32_t word)
{
	std::vector<uint8_t> one(12);

	memcpy(one.data(), id, 8);
	memcpy(one.data() + 8, &word, 4);
	return one;
}

// THE TRACK CASE (FUN_00534450, rows S-K72-R / S-K80-R): the level's first placed object holding that kind (72, or 80) gets four point effects: A = unknown, virtual; B = unknown; C = none;
// D = virtual (and, for 80, an unknown destruct target). inteng's walk leaves A = the virtual (the last target moved into the unknown's place), B and C deleted (C had no target at all),
// D kept, in their order; for 80 the destruct target is the null ID and its request word as it was. A second pass changes nothing.
static void TrackCase(char const *path, int32_t kind)
{
	std::string const label = "kind " + std::to_string((long long)kind) + ": ";
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, label + "the level opens: " + path);
		return;
	}

	std::vector<host::VirtualObjectInfo> virtuals;
	host::VirtualObject_List(&level, &virtuals);

	List<Object_Chunk *> objects;
	level.Level()->list_objects(&objects);

	uint8_t subject[8] = { 0 };
	std::vector<uint8_t> before;
	bool found = false;

	for (LIF<Object_Chunk *> each(&objects); !each.done() && !found; each.next())
	{
		host::ObjectStrategy strategy;
		uint8_t id[8];

		if (!each())
		{
			continue;
		}

		memcpy(id, &each()->object_data.ID, 8);

		if (host::FindObjectStrategy(level.Level(), id, &strategy) == host::StrategyFound::Found && strategy.payload.size() >= 24 && IntAt(strategy.payload, 8) == kind)
		{
			memcpy(subject, id, 8);
			before = strategy.payload;
			found = true;
		}
	}

	Check(found && virtuals.size() >= 2, label + "the level has a placed object holding it, and two virtual objects");

	if (!found || virtuals.size() < 2)
	{
		return;
	}

	// the bytes after the effects, as they are (the kind's own words)
	size_t at = 24;
	int32_t const count = IntAt(before, 20);
	bool walked = count >= 0;

	for (int32_t i = 0; walked && i < count; i++)
	{
		int32_t const n = IntAt(before, at + 8);

		walked = at + 12 <= before.size() && n >= 0 && (size_t)n <= (before.size() - at - 12) / 16;
		at += walked ? 12 + 16 * (size_t)n : 0;
	}

	Check(walked, label + "its effects are read (" + std::to_string((long long)count) + ")");

	if (!walked)
	{
		return;
	}

	std::vector<uint8_t> tail(before.begin() + (std::ptrdiff_t)at, before.end());
	uint8_t unknown1[8], unknown2[8], unknown3[8];

	memset(unknown1, 0xEE, 8);
	memset(unknown2, 0xDD, 8);
	memset(unknown3, 0xCC, 8);

	auto const target = [](uint8_t const *id, int32_t request, int32_t direction)
	{
		std::vector<uint8_t> one(16);

		memcpy(one.data(), id, 8);
		memcpy(one.data() + 8, &request, 4);
		memcpy(one.data() + 12, &direction, 4);
		return one;
	};

	auto const effect = [](int32_t point, std::vector<std::vector<uint8_t>> const &targets)
	{
		std::vector<uint8_t> one(12, 0);
		int32_t const n = (int32_t)targets.size();

		memcpy(one.data(), &point, 4);
		memcpy(one.data() + 8, &n, 4);

		for (std::vector<uint8_t> const &t : targets)
		{
			one.insert(one.end(), t.begin(), t.end());
		}

		return one;
	};

	std::vector<uint8_t> const tA1 = target(unknown1, 1, 0), tA2 = target(virtuals[0].id, 3, 1), tB = target(unknown2, 0, 0), tD = target(virtuals[1].id, 2, 0);
	std::vector<uint8_t> planted(before.begin(), before.begin() + 24);
	int32_t const four = 4;

	memcpy(planted.data() + 20, &four, 4);

	for (std::vector<uint8_t> const &one : { effect(0, { tA1, tA2 }), effect(1, { tB }), effect(2, {}), effect(3, { tD }) })
	{
		planted.insert(planted.end(), one.begin(), one.end());
	}

	std::vector<uint8_t> plantedTail = tail;

	if (kind == 80 && plantedTail.size() >= 12)
	{
		memcpy(plantedTail.data() + plantedTail.size() - 8, unknown3, 8);   // the destruct target: an ID no object has
	}

	planted.insert(planted.end(), plantedTail.begin(), plantedTail.end());
	Check(host::SetStrategyBytes(&level, subject, planted.data(), planted.size()) == host::ObjectEditResult::Done, label + "the four effects are put in (a placed object: SetStrategyBytes)");

	// then an edit the way the editor's field edits are made (SetPayload through the store: the parsed class still holds the bytes before it): D's pad word becomes 0x55. The repair must
	// rebuild from the bytes as they are NOW, or it writes the edit away (found 2026-11-16: the pass read the class, FindObjectStrategy)
	host::ObjectStrategy plantedNow;
	std::vector<uint8_t> withEdit = planted;
	size_t const dPad = 24 + (12 + 32) + (12 + 16) + 12 + 4;   // A (two targets), B (one), C (none), then D's point and its pad word
	int32_t const mark = 0x55;

	memcpy(withEdit.data() + dPad, &mark, 4);

	host::LevelStore store(&level);

	Check(host::FindObjectStrategy(level.Level(), subject, &plantedNow) == host::StrategyFound::Found && store.SetPayload(plantedNow.chunk, withEdit),
	      label + "a field edit through the store: D's pad word 0x55");

	host::StrategyRepair_Run(&level);

	host::ObjectStrategy now;
	std::vector<uint8_t> expected(before.begin(), before.begin() + 24);
	int32_t const two = 2;

	memcpy(expected.data() + 20, &two, 4);

	for (std::vector<uint8_t> const &one : { effect(0, { tA2 }), effect(3, { tD }) })
	{
		expected.insert(expected.end(), one.begin(), one.end());
	}

	std::vector<uint8_t> expectedTail = plantedTail;

	if (kind == 80 && expectedTail.size() >= 12)
	{
		memset(expectedTail.data() + expectedTail.size() - 8, 0, 8);   // the null ID; the request word before it as it was
	}

	expected.insert(expected.end(), expectedTail.begin(), expectedTail.end());
	memcpy(expected.data() + 24 + (12 + 16) + 4, &mark, 4);   // D's pad word, after A (one target left): the field edit kept

	bool const read = host::FindObjectStrategy(level.Level(), subject, &now) == host::StrategyFound::Found;

	Check(read && IntAt(now.payload, 20) == 2, label + "two effects are left: " + std::to_string((long long)(read ? IntAt(now.payload, 20) : -1)));
	Check(read && now.payload == expected, label + "A keeps its virtual target (moved into the unknown's place), B (its only target unknown) and C (no target) are gone, D kept, in order" +
	      (kind == 80 ? "; the destruct target is the null ID, its request word as it was" : ""));
	Check(host::StrategyRepair_Run(&level) == 0, label + "a second pass changes nothing");
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("  FAILED  usage: host_strategy_repair_test <level .rif> <folder for the copy>\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the level opens: %s\n", argv[1]);
		return 1;
	}

	std::vector<host::VirtualObjectInfo> virtuals;
	uint8_t subject[8] = { 0 };
	std::vector<uint8_t> before;
	std::vector<uint8_t const *> others;
	bool found = false;

	host::VirtualObject_List(&level, &virtuals);

	for (host::VirtualObjectInfo const &one : virtuals)
	{
		std::vector<uint8_t> bytes;

		if (!found && MissionBytes(level, one.id, &bytes) && IntAt(bytes, 40) == 0)
		{
			memcpy(subject, one.id, 8);
			before = bytes;
			found = true;
		}
		else if (others.size() < 2)
		{
			others.push_back(one.id);
		}
	}

	Check(found && others.size() == 2, "the level has a Mission Objective with no alterations and two other virtual objects");

	if (!found || others.size() != 2)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	// U1, V1, U2, V2 (IDs no object has: 0xEE.. and 0xDD..), the words 3, 1, 2, 3
	uint8_t unknown1[8], unknown2[8];

	memset(unknown1, 0xEE, 8);
	memset(unknown2, 0xDD, 8);

	std::vector<uint8_t> const u1 = Entry(unknown1, 3), v1 = Entry(others[0], 1), u2 = Entry(unknown2, 2), v2 = Entry(others[1], 3);
	std::vector<uint8_t> planted(before.begin(), before.begin() + 44);
	int32_t const four = 4;

	memcpy(planted.data() + 40, &four, 4);

	for (std::vector<uint8_t> const *entry : { &u1, &v1, &u2, &v2 })
	{
		planted.insert(planted.end(), entry->begin(), entry->end());
	}

	planted.insert(planted.end(), before.begin() + 44, before.end());   // the request targets and the rest, as they were

	// object_edit.h's SetStrategyBytes reaches placed objects and modules only: a virtual object's ID is not one of them (the write every stock Mission Objective needs)
	Check(host::SetStrategyBytes(&level, subject, planted.data(), planted.size()) == host::ObjectEditResult::NoSuchObject, "SetStrategyBytes does not reach a virtual object (NoSuchObject)");

	// VirtualObject_SetStrategyBytes' refusals, each for its reason, the level unchanged
	std::vector<uint8_t> const huge(65537, 0);

	Check(host::VirtualObject_SetStrategyBytes(&level, subject, planted.data(), 11) == host::VirtualObjectResult::BadBytes, "refused: 11 bytes are shorter than a strategy's header (BadBytes)");
	Check(host::VirtualObject_SetStrategyBytes(&level, subject, huge.data(), huge.size()) == host::VirtualObjectResult::BadBytes, "refused: 65537 bytes are more than a strategy chunk the editor makes (BadBytes)");
	Check(host::VirtualObject_SetStrategyBytes(&level, unknown1, planted.data(), planted.size()) == host::VirtualObjectResult::NotFound, "refused: no virtual object has the ID (NotFound)");
	Check(host::VirtualObject_SetStrategyBytes(nullptr, subject, planted.data(), planted.size()) == host::VirtualObjectResult::BadCall, "refused: no level (BadCall)");

	std::vector<uint8_t> still;

	Check(MissionBytes(level, subject, &still) && still == before, "after the refusals the Mission Objective is as it was");
	Check(host::VirtualObject_SetStrategyBytes(&level, subject, planted.data(), planted.size()) == host::VirtualObjectResult::Done, "four alterations are put in: unknown, virtual, unknown, virtual");

	size_t const changed = host::StrategyRepair_Run(&level);
	std::vector<uint8_t> after;

	Check(changed >= 1 && MissionBytes(level, subject, &after), "the repair pass changes the Mission Objective");
	Check(IntAt(after, 40) == 2, "two alterations are left: " + std::to_string(IntAt(after, 40)));

	// inteng's walk leaves V2 (moved into U1's place) then V1
	std::vector<uint8_t> expected(v2);

	expected.insert(expected.end(), v1.begin(), v1.end());

	Check(after.size() >= 44 + expected.size() && memcmp(after.data() + 44, expected.data(), expected.size()) == 0, "the list is V2 (moved into the first place), then V1, with their words");
	Check(after.size() == before.size() + 24 && memcmp(after.data() + 44 + 24, before.data() + 44, before.size() - 44) == 0, "the bytes after the list (the request targets) are as they were");
	Check(memcmp(after.data(), before.data(), 40) == 0, "the words before the list are as they were");
	Check(host::StrategyRepair_Run(&level) == 0, "a second pass changes nothing");

	std::string const folder = argv[2];
	std::string const copyPath = folder + "\\repair-copy.rif";

	CreateDirectoryA(folder.c_str(), nullptr);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;
	std::vector<uint8_t> reread;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");
	Check(reopened.Level() && MissionBytes(reopened, subject, &reread) && reread == after, "the copy holds the repaired Mission Objective");

	TrackCase(argv[1], 72);

	if (argc >= 4)
	{
		TrackCase(argv[3], 80);
	}
	else
	{
		Check(false, "a level with a Track Object (Destructable) is named (the third argument, {LEVEL:Area52})");
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
