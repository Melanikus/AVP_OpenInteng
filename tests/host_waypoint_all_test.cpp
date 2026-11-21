// host_waypoint_all_test.cpp: waypoint_all.h (THE WAYPOINT DISPLAY's data: every AI module's alien list at once, cached) on PRIVATE copies of stock Genshd1 and of the auto-plot's v2d
// Genshd1 (AvP-builds-excluded/autoplot-genshd1-v2, sha 21f1578e; never the Steam file). Frost's waypoint_net_level.h reads the same lists the stock loader's way and is the oracle.
// Written 2026-11-19 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Arguments: the stock Genshd1, the v2d Genshd1 (its checks are skipped, and say so, where it isn't there: CI), a scratch folder of its own (the copies go there).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#define NOMINMAX   // std::max, not windows.h's macro
#include <windows.h>

#include "level_file.h"
#include "object_edit.h"
#include "waypoint_all.h"
#include "waypoint_model.h"
#include "waypoint_net_level.h"

static int checks = 0, checksFailed = 0;

// the game's file and message calls the loader makes, as host_waypoint_net_level_test.cpp has them (Frost): files opened for reading only
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

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

// (Win32 reads and writes, as the other unit tests do: Snyk Code took a command-line path into std::ifstream / std::ofstream for path traversal; Vasquez 11-19)
static bool CopyTo(std::string const &from, std::string const &to)
{
	HANDLE const in = CreateFileA(from.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (in == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	HANDLE const out = CreateFileA(to.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (out == INVALID_HANDLE_VALUE)
	{
		CloseHandle(in);
		return false;
	}

	std::vector<char> buffer(1 << 20);
	bool good = true;

	for (;;)
	{
		DWORD got = 0, put = 0;

		if (!ReadFile(in, buffer.data(), (DWORD)buffer.size(), &got, 0))
		{
			good = false;
			break;
		}

		if (got == 0)
		{
			break;
		}

		if (!WriteFile(out, buffer.data(), got, &put, 0) || put != got)
		{
			good = false;
			break;
		}
	}

	CloseHandle(in);
	CloseHandle(out);
	return good;
}

static size_t Boxes(std::vector<host::AlienNet> const &nets)
{
	size_t boxes = 0;

	for (host::AlienNet const &net : nets)
	{
		boxes += net.alien.size();
	}

	return boxes;
}

static double MsSince(std::chrono::steady_clock::time_point from)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
}

// the same masters in the same order, the same boxes each, as Frost's reader (the stock loader's way: projload.cpp 2497)
static bool SameAsFrost(OpenLevelFile *open, std::vector<host::AlienNet> const &nets, std::string *words)
{
	std::vector<host::WaypointNetModule> frost;
	std::string why;

	if (!host::WaypointNets_CheckLevel(open, &frost, &why))
	{
		*words = "Frost's reader refused the level: " + why;
		return false;
	}

	*words = std::to_string(frost.size()) + " AI modules by Frost's reader";

	if (frost.size() != nets.size())
	{
		return false;
	}

	for (size_t i = 0; i < nets.size(); i++)
	{
		if (frost[i].master != nets[i].master || frost[i].boxes != nets[i].alien.size())
		{
			*words += "; the first that differs: " + std::to_string(i) + " (master " + std::to_string(nets[i].master) + ", " + std::to_string(nets[i].alien.size()) + " boxes; Frost's " +
				std::to_string(frost[i].master) + ", " + std::to_string(frost[i].boxes) + ")";
			return false;
		}
	}

	return true;
}

static size_t BoxesOf(std::vector<host::AlienNet> const &nets, char const *name)
{
	for (host::AlienNet const &net : nets)
	{
		if (net.name == name)
		{
			return net.alien.size();
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("host_waypoint_all_test\n");

	if (argc < 4)
	{
		printf("usage: host_waypoint_all_test <stock Genshd1> <v2d Genshd1> <scratch folder>\n");
		return 2;
	}

	std::string const stock = argv[1], v2d = argv[2], scratch = argv[3];

	printf("NO LEVEL\n");
	{
		OpenLevelFile none;
		size_t unreadable = 7;

		Check(host::WaypointAll_AlienNets(&none, &unreadable).empty() && unreadable == 0, "an open with no level: an empty list, nothing unreadable");
		Check(host::WaypointAll_AlienNets(nullptr).empty(), "no open at all: an empty list");
	}

	printf("STOCK GENSHD1 (A PRIVATE COPY)\n");

	std::string const stockCopy = scratch + "\\Genshd1-stock.RIF";
	OpenLevelFile level;

	Check(CopyTo(stock, stockCopy), "stock Genshd1 copied to the scratch folder");

	if (level.Open(stockCopy.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "stock Genshd1's copy opens");
		printf("%d checks, %d failed\nFAILED host_waypoint_all_test\n", checks, checksFailed);
		return 1;
	}

	size_t const readsAtStart = host::WaypointAll_Reads();
	size_t unreadable = 99;
	std::chrono::steady_clock::time_point const t0 = std::chrono::steady_clock::now();
	std::vector<host::AlienNet> const &nets = host::WaypointAll_AlienNets(&level, &unreadable);
	double const firstMs = MsSince(t0);
	std::string frostWords;
	bool named = !nets.empty();

	for (host::AlienNet const &net : nets)
	{
		named = named && !net.name.empty();
	}

	printf("  (the first read: %.1f ms)\n", firstMs);
	Check(host::WaypointAll_Reads() == readsAtStart + 1, "the first call reads the level");
	Check(nets.size() == 9 && Boxes(nets) == 136 && unreadable == 0, "9 AI modules with alien waypoints, 136 boxes (the editor's dialog: \"replace the 136 alien waypoints of 9 AI modules\"), "
		"none unreadable (" + std::to_string(nets.size()) + ", " + std::to_string(Boxes(nets)) + ", " + std::to_string(unreadable) + ")");
	bool const sameStock = SameAsFrost(&level, nets, &frostWords);   // before the words are made (the order of a call's arguments is not fixed)

	Check(sameStock, "the same masters, order and boxes as Frost's reader (" + frostWords + ")");
	Check(named, "every one named");

	printf("THE CACHE\n");

	size_t const readsBefore = host::WaypointAll_Reads();
	double mostMs = 0.0;
	bool sameList = true;

	for (int i = 0; i < 100; i++)
	{
		std::chrono::steady_clock::time_point const t = std::chrono::steady_clock::now();
		std::vector<host::AlienNet> const &again = host::WaypointAll_AlienNets(&level);

		mostMs = std::max(mostMs, MsSince(t));
		sameList = sameList && &again == &nets && again.size() == 9;
	}

	Check(host::WaypointAll_Reads() == readsBefore && sameList, "100 calls in the same state: the level is not read again, the same list");
	Check(mostMs < 5.0, "a call in the same state takes under 5 ms (the slowest " + std::to_string(mostMs) + " ms)");

	printf("A CHANGE: ONE ALIEN BOX TAKEN OUT OF A NETWORK (ITS WAYPOINT WRITTEN AGAIN THROUGH THE GAME'S LOADER)\n");

	size_t pick = nets.size();

	for (size_t i = 0; i < nets.size() && pick == nets.size(); i++)
	{
		pick = nets[i].alien.size() >= 2 ? i : pick;
	}

	std::vector<host::ModuleSnapshot> snapshots;
	host::WaypointSet set;
	std::string why;
	int32_t master = pick < nets.size() ? nets[pick].master : -1;
	size_t boxesBefore = pick < nets.size() ? nets[pick].alien.size() : 0;

	bool parsed = host::ReadModuleSnapshots(&level, &snapshots) == host::ObjectEditResult::Done;

	for (host::ModuleSnapshot const &one : snapshots)
	{
		if (one.index == master && one.module && one.hasWaypoint)
		{
			parsed = parsed && host::Waypoints_Parse(one.waypoint.data(), one.waypoint.size(), &set, &why);
		}
	}

	Check(pick < nets.size() && parsed && set.alien.size() == boxesBefore, "a network of 2 boxes or more, its WAYPOINT read (" + std::to_string(boxesBefore) + " boxes)");

	host::Waypoints_Delete(&set.alien, (int32_t)set.alien.size() - 1);

	host::ModuleChildEdit edit;

	edit.objectIndex = master;
	memcpy(edit.name, "WAYPOINT", 9);
	edit.remove = false;
	edit.chunk = host::Waypoints_Serialize(set);

	std::vector<bool> written;
	unsigned const generationBefore = level.Generation();

	Check(host::SetModuleChildrenMany(&level, { edit }, &written) == host::ObjectEditResult::Done && written.size() == 1 && written[0] && level.Generation() == generationBefore + 1,
		"the WAYPOINT written again (a level swapped in: Generation() one more)");

	size_t const readsBeforeChange = host::WaypointAll_Reads();
	std::vector<host::AlienNet> const &changed = host::WaypointAll_AlienNets(&level);
	bool found = false;

	for (host::AlienNet const &net : changed)
	{
		found = found || (net.master == master && net.alien.size() + 1 == boxesBefore);
	}

	Check(host::WaypointAll_Reads() == readsBeforeChange + 1, "after the change the level is read again");
	Check(found && Boxes(changed) == 135, "the changed network one box less, 135 in all (" + std::to_string(Boxes(changed)) + ")");

	printf("THE AUTO-PLOT'S v2d GENSHD1 (A PRIVATE COPY), OPENED IN THE SAME OPEN (A NEW OPEN: ITS COUNTS START AT 0 AGAIN)\n");

	std::string const v2dCopy = scratch + "\\Genshd1-v2d.RIF";

	if (GetFileAttributesA(v2d.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		printf("  (no v2d file at %s: its checks are not run here)\n", v2d.c_str());
	}
	else
	{
		Check(CopyTo(v2d, v2dCopy), "v2d copied to the scratch folder");
		// (Vasquez 11-20: an open's Generation() is a number the run never gave before, level_file.h, so a cache made for the level open before is not taken for this one)
		unsigned const generationOfTheOneBefore = level.Generation();
		Check(level.Open(v2dCopy.c_str()) == LevelOpenResult::Opened && level.Level() && level.Generation() != generationOfTheOneBefore && level.Changes() == 0,
			"v2d's copy opens in the same open (a new Generation(), Changes() 0 again)");

		size_t const readsBeforeOpen = host::WaypointAll_Reads();
		std::chrono::steady_clock::time_point const t1 = std::chrono::steady_clock::now();
		std::vector<host::AlienNet> const &all = host::WaypointAll_AlienNets(&level, &unreadable);
		double const v2dMs = MsSince(t1);

		printf("  (the read of v2d: %.1f ms)\n", v2dMs);
		Check(host::WaypointAll_Reads() == readsBeforeOpen + 1, "a new level in the same open is read again");
		Check(all.size() == 244 && Boxes(all) == 15954 && unreadable == 0, "244 AI modules, 15,954 boxes: the auto-plot's 242 and 15,933 (Bishop's), and the stock lists of the 2 it "
			"refused (MOD-03 13, shft21 8) (" + std::to_string(all.size()) + ", " + std::to_string(Boxes(all)) + ", " + std::to_string(unreadable) + " unreadable)");
		Check(BoxesOf(all, "opscen03") == 73 && BoxesOf(all, "Ops01") == 82 && BoxesOf(all, "opscen02") == 84, "chain A: opscen03 73, Ops01 82, opscen02 84 boxes (" +
			std::to_string(BoxesOf(all, "opscen03")) + ", " + std::to_string(BoxesOf(all, "Ops01")) + ", " + std::to_string(BoxesOf(all, "opscen02")) + ")");
		bool const sameV2d = SameAsFrost(&level, all, &frostWords);

		Check(sameV2d, "the same masters, order and boxes as Frost's reader (" + frostWords + ")");
		Check(v2dMs < 10000.0, "the read of the whole level under 10 s (" + std::to_string(v2dMs) + " ms)");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_waypoint_all_test\n");
		return 1;
	}

	printf("passed host_waypoint_all_test\n");
	return 0;
}
