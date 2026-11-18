// The test of Delete Strategy on a VIRTUAL object (virtual_object.h VirtualObject_DeleteStrategy; inteng's FUN_00528790 on its virtual record, class 0x5af9e0: the holder at +0x14, the VIOBJECT's
// first AVPSTRAT, is destroyed and nothing else; reassess/strategy-data.md S-39, strategy-regen-suite-design.md F2 / V3). Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_virtual_object_test.exe <Vaults.RIF> <a folder it may write its copy into> (tests/host_virtual_object_test.runargs). The level is only read; the copy is the only file written.
// THE CASE: Vaults' virtual Message Vmessage077 (73). Its strategy is taken out: the virtual object stays, with its name, place and size; every other strategy holder is as it was, byte for byte;
// a second delete answers NoStrategy and an unknown ID NotFound, the level unchanged by either; the copy saved is taken by the game's loader and holds the virtual object without a strategy.

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
#include "strategy_lookup.h"
#include "strategy_views.h"   // StrategyRecordRef, for ListStrategyRecords
#include "virtual_object.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as host_strategy_repair_test); a read-only open is shared for reading, so another test may read the same stock level

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

// The virtual object of that name, or false
static bool VirtualNamed(OpenLevelFile &level, char const *name, host::VirtualObjectInfo *out)
{
	std::vector<host::VirtualObjectInfo> all;

	if (!host::VirtualObject_List(&level, &all))
	{
		return false;
	}

	for (host::VirtualObjectInfo const &one : all)
	{
		if (one.name == name)
		{
			*out = one;
			return true;
		}
	}

	return false;
}

static size_t VirtualCount(OpenLevelFile &level)
{
	std::vector<host::VirtualObjectInfo> all;

	host::VirtualObject_List(&level, &all);
	return all.size();
}

// Every holder's strategy but the one with this ID: the same in both lists, in the same order
static bool OthersSame(std::vector<host::StrategyRecordRef> const &a, std::vector<host::StrategyRecordRef> const &b, uint8_t const *except)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i].id != b[i].id)
		{
			return false;
		}

		if (memcmp(a[i].id.data(), except, 8) != 0 && (a[i].kind != b[i].kind || a[i].payload != b[i].payload))
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 3)
	{
		printf("  FAILED  usage: host_virtual_object_test <Vaults.RIF> <folder for the copy> (tests/host_virtual_object_test.runargs)\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level: %s\n", argv[1]);
		return 1;
	}

	puts("Delete Strategy on a virtual object (VirtualObject_DeleteStrategy, FUN_00528790):");

	host::VirtualObjectInfo before;

	Check(VirtualNamed(level, "Vmessage077", &before) && before.hasStrategy, "Vaults has the virtual object Vmessage077, with a strategy");

	if (!before.hasStrategy)
	{
		printf("host_virtual_object_test: %d checks, %d failed\n", checks, checksFailed);
		return 1;
	}

	std::vector<host::StrategyRecordRef> recordsBefore, recordsAfter;
	size_t const countBefore = VirtualCount(level);
	uint8_t unknown[8];

	memset(unknown, 0xEE, 8);
	host::ListStrategyRecords(level.Level(), &recordsBefore);

	// the refusals first, each for its reason, the level as it was after them
	unsigned const generation = level.Generation();

	Check(host::VirtualObject_DeleteStrategy(nullptr, before.id) == host::VirtualObjectResult::BadCall, "no level: BadCall");
	Check(host::VirtualObject_DeleteStrategy(&level, nullptr) == host::VirtualObjectResult::BadCall, "no ID: BadCall");
	Check(host::VirtualObject_DeleteStrategy(&level, unknown) == host::VirtualObjectResult::NotFound, "an ID no virtual object has: NotFound");
	Check(level.Generation() == generation, "the refusals changed nothing (the level was not made again)");

	// the delete
	Check(host::VirtualObject_DeleteStrategy(&level, before.id) == host::VirtualObjectResult::Done, "Vmessage077's strategy is deleted");

	host::VirtualObjectInfo after;
	host::ObjectStrategy held;

	Check(VirtualNamed(level, "Vmessage077", &after) && !after.hasStrategy, "the virtual object is still there, and holds no AVPSTRAT");
	Check(memcmp(after.id, before.id, 8) == 0 && after.place[0] == before.place[0] && after.place[1] == before.place[1] && after.place[2] == before.place[2] && after.size == before.size,
		"its VOBJPROP is as it was: the same ID, place and size");
	Check(VirtualCount(level) == countBefore, "the level has as many virtual objects as before (" + std::to_string(countBefore) + ")");
	Check(host::FindObjectStrategy(level.Level(), before.id, &held) == host::StrategyFound::NoStrategy, "the strategy lookup finds no strategy for it");

	host::ListStrategyRecords(level.Level(), &recordsAfter);
	Check(OthersSame(recordsBefore, recordsAfter, before.id), "every other holder's strategy is as it was, in the same order (" + std::to_string(recordsBefore.size()) + " holders)");

	// a second delete has no strategy to take: NoStrategy, the level not made again
	unsigned const deleted = level.Generation();

	Check(host::VirtualObject_DeleteStrategy(&level, before.id) == host::VirtualObjectResult::NoStrategy && level.Generation() == deleted, "a second delete: NoStrategy, nothing changed");

	// the copy, through the game's loader
	std::string const folder = argv[2];
	std::string const copyPath = folder + "\\virtual-delete-copy.rif";

	CreateDirectoryA(folder.c_str(), nullptr);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a copy: " + copyPath);

	OpenLevelFile copy;
	host::VirtualObjectInfo saved;

	Check(copy.Open(copyPath.c_str()) == LevelOpenResult::Opened && copy.Level(), "the game's loader takes the copy");
	Check(copy.Level() && VirtualNamed(copy, "Vmessage077", &saved) && !saved.hasStrategy && memcmp(saved.id, before.id, 8) == 0,
		"the copy holds Vmessage077 without a strategy");

	printf("host_virtual_object_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
