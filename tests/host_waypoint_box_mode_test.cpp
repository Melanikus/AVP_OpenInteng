// host_waypoint_box_mode_test.cpp: WaypointMode's two calls for the waypoint-box gizmo (src/host/waypoint_box_mode.cpp: MouseShowBox, MouseSetBox) on a stock level, Invasion: module 54
// picked and its AI module's alien list opened (Show Alien Waypoints, as on the screen), box 14 shown moved (in memory only: the level as it was), then its aim point written inside it
// (the level's WAYPOINT holds it, the links as they were, the checker's report for the AI module is clean), a box the game's loader refuses (a room of 0 on x) not written (the list and
// the level as they were, the words say why), the calls refused outside mode 1 or past the list, and the copy saved and taken by the loader. Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: host_waypoint_box_mode_test <Invasion.RIF> <a folder for the copy> (tests/host_waypoint_box_mode_test.runargs).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "level_file.h"
#include "object_edit.h"
#include "waypoint_box_gizmo.h"
#include "waypoint_model.h"
#include "waypoint_mode.h"
#include "waypoint_net_check.h"
#include "waypoint_net_level.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

// the game's file and message calls the level code links to (as host_waypoint_mode_test has them)
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

// the module's WAYPOINT as the level holds it now
static bool SetOfModule(OpenLevelFile &level, int32_t module, host::WaypointSet *set)
{
	std::vector<host::ModuleSnapshot> snapshots;
	std::string why;

	*set = host::WaypointSet();
	host::ReadModuleSnapshots(&level, &snapshots);

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (snapshot.module && snapshot.index == module)
		{
			return snapshot.hasWaypoint && host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), set, &why);
		}
	}

	return false;
}

static bool SameShape(host::WaypointRec const &a, host::WaypointRec const &b)
{
	return memcmp(a.min, b.min, sizeof(a.min)) == 0 && memcmp(a.max, b.max, sizeof(a.max)) == 0 && memcmp(a.centre, b.centre, sizeof(a.centre)) == 0 && a.flags == b.flags;
}

static bool SameLinks(host::WaypointRec const &a, host::WaypointRec const &b)
{
	if (a.links.size() != b.links.size() || a.moduleLinks.size() != b.moduleLinks.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.links.size(); i++)
	{
		if (a.links[i].index != b.links[i].index || a.links[i].flags != b.links[i].flags)
		{
			return false;
		}
	}

	for (size_t i = 0; i < a.moduleLinks.size(); i++)
	{
		if (a.moduleLinks[i].name != b.moduleLinks[i].name || a.moduleLinks[i].flags != b.moduleLinks[i].flags)
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
		printf("host_waypoint_box_mode_test: give Invasion.RIF and a folder for the copy; with none there is nothing to run.\n");
		printf("0 checks, 0 failed\n");
		return 0;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	double const scale = host::WaypointNets_Scale(&level);
	host::WaypointMode mode;
	host::WaypointContext context;

	context.open = &level;
	context.pickedModule = -1;

	// outside mode 1: refused
	host::WaypointRec any = {};

	Check(!mode.MouseShowBox(0, any) && mode.MouseSetBox(&level, 0, any, "x").say.find("alien list") != std::string::npos, "in mode 0 both calls are refused, with why");

	// module 54 picked, its AI module's alien list opened, as on the screen (Show Alien Waypoints, command 3)
	context.pickedModule = 54;
	mode.Command(3, context);

	host::WaypointSet before;
	bool const read = SetOfModule(level, mode.CurrentModule(), &before);

	Check(mode.Mode() == 1 && mode.AlienList() && mode.CurrentModule() == 54 && read && before.alien.size() == 28 && mode.List().size() == 28,
		"module 54's AI module (master " + std::to_string(mode.CurrentModule()) + "): mode 1, the alien list of 28 (the stock survey's)");

	if (mode.List().size() != 28 || before.alien.size() != 28)
	{
		printf("%d checks, %d failed\n", checks, checksFailed);
		return 1;
	}

	host::WaypointRec const box14 = before.alien[14];

	// its numbers, for acceptance-waypoint-box-gizmo's camera (the file's units; scale 1 on Invasion)
	printf("  --  box 14: lowest %d %d %d, highest %d %d %d, aim point %d %d %d, flags 0x%08x\n", (int)box14.min[0], (int)box14.min[1], (int)box14.min[2], (int)box14.max[0],
		(int)box14.max[1], (int)box14.max[2], (int)box14.centre[0], (int)box14.centre[1], (int)box14.centre[2], (unsigned)box14.flags);

	Check(!host::Waypoint_Holds(box14, box14.centre, scale) && box14.centre[0] == box14.min[0] - 46, "box 14's aim point is 46 below its lowest x, as stock has it");
	Check(host::WaypointNets_ReportModule(&level, 54) == "checker: box 14: its centre is outside it", "and the checker's report for the AI module names it");

	// SHOWN: in memory only
	host::WaypointRec moved;

	Check(host::WaypointBox_Moved(box14, 1, -500, scale, &moved) && mode.MouseShowBox(14, moved) && SameShape(mode.List()[14], moved) && SameLinks(mode.List()[14], box14),
		"MouseShowBox: the list holds box 14 moved 500 up, its links as they were");

	host::WaypointSet still;

	Check(SetOfModule(level, 54, &still) && SameShape(still.alien[14], box14), "and the level is as it was (nothing written)");
	Check(mode.MouseShowBox(14, box14) && SameShape(mode.List()[14], box14), "the drag's start put back the same way");
	Check(!mode.MouseShowBox(28, moved) && !mode.MouseShowBox(-1, moved), "a number past the list: refused");

	// WRITTEN: the aim point moved inside (the gizmo's own sum)
	host::WaypointRec aimed;

	Check(host::WaypointBox_AimMoved(box14, 0, 60, scale, &aimed) && host::Waypoint_Holds(aimed, aimed.centre, scale), "the aim point moved 60 along x: inside its box");

	host::WaypointOutcome const written = mode.MouseSetBox(&level, 14, aimed, "its aim point moved 60 along x");
	host::WaypointSet after;

	Check(written.levelChanged && written.say == "Waypoint 14: its aim point moved 60 along x", "MouseSetBox writes at once: " + written.say);
	Check(SetOfModule(level, 54, &after) && after.alien.size() == 28 && SameShape(after.alien[14], aimed) && SameLinks(after.alien[14], box14),
		"the level's WAYPOINT holds box 14's new aim point, the user's flag, its links as they were");

	bool othersSame = after.alien.size() == before.alien.size();

	for (size_t b = 0; b < after.alien.size() && othersSame; b++)
	{
		othersSame = b == 14 || (SameShape(after.alien[b], before.alien[b]) && SameLinks(after.alien[b], before.alien[b]));
	}

	Check(othersSame && after.ground.size() == before.ground.size(), "every other box, and the ground list, as they were");

	std::string const report = host::WaypointNets_ReportModule(&level, 54);

	Check(report == "checker: nothing found", "the checker's report for the AI module now: " + report);

	// REFUSED by the game's loader: a room of 0 on x (1 wide, its aim point at its lowest: waypoint_room.h). A box with NO size on an axis has a room of -1, which the loader takes
	// (three stock levels have one); the gizmo makes neither (WaypointBox_HasRoom: 1 or more)
	host::WaypointRec flat = aimed;

	flat.max[0] = flat.min[0] + 1;
	flat.centre[0] = flat.min[0];

	std::vector<char> bytesBefore, bytesAfter;
	bool const builtBefore = level.BuildLevel(bytesBefore);
	host::WaypointOutcome const refused = mode.MouseSetBox(&level, 14, flat, "flattened");
	bool const builtAfter = level.BuildLevel(bytesAfter);

	Check(!refused.levelChanged && refused.say.find("not written") != std::string::npos, "a box the loader refuses (a room of 0 on x) is not written: " + refused.say);
	Check(SameShape(mode.List()[14], aimed) && builtBefore && builtAfter && bytesAfter == bytesBefore, "the list and the level's bytes (as the next save would write them) as they were");
	Check(mode.MouseSetBox(&level, 28, aimed, "x").say.find("alien list") != std::string::npos && !mode.MouseSetBox(&level, 28, aimed, "x").levelChanged, "a number past the list: refused");
	Check(mode.MouseSetBox(nullptr, 14, aimed, "x").say.find("alien list") != std::string::npos, "no level: refused");

	// the copy
	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "waypoint-box-mode-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a copy");

	OpenLevelFile reopened;
	host::WaypointSet inCopy;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level() && SetOfModule(reopened, 54, &inCopy) && SameShape(inCopy.alien[14], aimed),
		"the game's loader takes the copy, and its box 14 is the one written");
	DeleteFileA(copyPath.c_str());

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
