// Tests THE WAYPOINT DISPLAY'S CLICK, WaypointMode::OpenAt (Vasquez 2026-11-19, waypoint_mode.h; the owner's "see and be able to edit all alien paths and volumes across all modules"): from
// mode 0, where every AI module's alien network is drawn, a click on one of its boxes opens that AI module as inteng's pick + Show Alien Waypoints would, with that box the current
// waypoint. Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng (Bishop: the display's unit tests, step 3 of the owner's order of work). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHAT, on a stock level with alien waypoints (Invasion): a box of one AI module opened from mode 0 (mode 1, the alien list, that AI module current, that box current, the outcome's page 1,
// the list the module's own); refused outside mode 0 (nothing changes); Exit back to mode 0; another AI module's box the same way; a module that is not an AI module's master, and no
// module at all, refused (still mode 0); a box past the list's end opens the AI module with no such current waypoint; and the level never written by any of it. The click itself (the
// ray from the view to a box, waypoint_mouse.cpp WaypointAllClick) is the screen's: acceptance-waypoint-all-pick.

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
#include "level_chunks.h"
#include "object_edit.h"
#include "waypoint_model.h"
#include "waypoint_mode.h"

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

namespace
{
	struct Master
	{
		int32_t index = -1;
		std::vector<host::WaypointRec> alien;
		int32_t slave = -1;   // a module of the AI module that is not its master, if it has one
	};

	bool SameBox(host::WaypointRec const &a, host::WaypointRec const &b)
	{
		return memcmp(a.min, b.min, sizeof(a.min)) == 0 && memcmp(a.max, b.max, sizeof(a.max)) == 0 && memcmp(a.centre, b.centre, sizeof(a.centre)) == 0 && a.links.size() == b.links.size();
	}

	bool SameList(std::vector<host::WaypointRec> const &a, std::vector<host::WaypointRec> const &b)
	{
		if (a.size() != b.size())
		{
			return false;
		}

		for (size_t i = 0; i < a.size(); i++)
		{
			if (!SameBox(a[i], b[i]))
			{
				return false;
			}
		}

		return true;
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 2)
	{
		printf("host_waypoint_open_at_test: give a level .rif with two AI modules that hold alien waypoints\n");
		printf("0 checks, 1 failed\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		printf("1 checks, 1 failed\n");
		return 1;
	}

	// two masters with alien waypoints (the first two in the file), each with a slave if it has one; and an object that is no module at all
	std::vector<host::ModuleSnapshot> snapshots;
	std::vector<Master> masters;
	int32_t notAModule = -1;

	host::ReadModuleSnapshots(&level, &snapshots);

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		host::WaypointSet set;
		std::string why;

		if (!snapshot.module && notAModule < 0)
		{
			notAModule = snapshot.index;
		}

		if (masters.size() < 2 && snapshot.module && snapshot.masterIndex < 0 && snapshot.hasWaypoint &&
			host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why) && set.alien.size() >= 2)
		{
			Master m;

			m.index = snapshot.index;
			m.alien = set.alien;

			for (host::ModuleSnapshot const &other : snapshots)
			{
				if (other.module && other.masterIndex == snapshot.index)
				{
					m.slave = other.index;
					break;
				}
			}

			masters.push_back(m);
		}
	}

	Check(masters.size() == 2, "the level has two AI modules that hold alien waypoints (" + std::to_string(masters.size()) + ")");

	if (masters.size() < 2)
	{
		printf("%d checks, %d failed\n", checks, checksFailed);
		return 1;
	}

	Master const &a = masters[0], &b = masters[1];
	int32_t const aBox = (int32_t)a.alien.size() - 1;   // the last box: not the one Show Alien Waypoints would make current by itself

	printf("  --  A #%d (%zu alien boxes, slave #%d), B #%d (%zu alien boxes), an object that is no module #%d\n", a.index, a.alien.size(), a.slave, b.index, b.alien.size(), notAModule);

	std::vector<char> imageBefore, imageNow;

	Check(level.BuildLevel(imageBefore) && !imageBefore.empty(), "the level's bytes before any click");

	host::WaypointMode mode;
	host::WaypointContext context;

	context.open = &level;
	context.pickedModule = -1;
	mode.Enter();

	// ---- a box of A, from mode 0
	host::WaypointOutcome out = mode.OpenAt(&level, a.index, aBox);

	Check(mode.Mode() == 1 && mode.AlienList(), "a box of A opens mode 1 on the alien list (" + out.say + ")");
	Check(mode.CurrentModule() == a.index, "with A the current AI module");
	Check(mode.CurrentWaypoint() == aBox, "and the box clicked the current waypoint (" + std::to_string(mode.CurrentWaypoint()) + ", clicked " + std::to_string(aBox) + ")");
	Check(out.page == 1, "the outcome shows the waypoints' page (" + std::to_string(out.page) + ")");
	Check(out.say.find("Waypoint " + std::to_string(aBox) + " is the current one") != std::string::npos, "and its words say which box is current");
	Check(SameList(mode.List(), a.alien), "the list is A's own alien list, as the level holds it");

	// ---- refused outside mode 0: nothing changes
	out = mode.OpenAt(&level, b.index, 0);
	Check(out.say.find("from the AI modules page only") != std::string::npos, "a second OpenAt outside mode 0 is refused (" + out.say + ")");
	Check(mode.Mode() == 1 && mode.CurrentModule() == a.index && mode.CurrentWaypoint() == aBox, "and leaves A and its current box as they were");

	// ---- Exit: back to mode 0; then a box of B
	context.pickedModule = a.index;
	mode.Command(2, context);
	Check(mode.Mode() == 0, "Exit goes back to mode 0");

	out = mode.OpenAt(&level, b.index, 0);
	Check(mode.Mode() == 1 && mode.CurrentModule() == b.index && mode.CurrentWaypoint() == 0 && SameList(mode.List(), b.alien),
		"from mode 0 again, a box of another AI module (B) opens B with that box current");

	context.pickedModule = b.index;
	mode.Command(2, context);
	Check(mode.Mode() == 0, "and Exit goes back to mode 0 again");

	// ---- what is not an AI module's master: refused, still mode 0
	if (a.slave >= 0)
	{
		out = mode.OpenAt(&level, a.slave, 0);
		Check(mode.Mode() == 0 && out.say.find("could not be picked") != std::string::npos, "a module that is not its AI module's master is refused (" + out.say + ")");
	}

	if (notAModule >= 0)
	{
		out = mode.OpenAt(&level, notAModule, 0);
		Check(mode.Mode() == 0 && out.say.find("could not be picked") != std::string::npos, "an object that is no module is refused (" + out.say + ")");
	}

	out = mode.OpenAt(&level, -1, 0);
	Check(mode.Mode() == 0 && out.say.find("could not be picked") != std::string::npos, "no module at all (-1) is refused (" + out.say + ")");

	// ---- a box past the list's end: the AI module opens, with no such current waypoint
	out = mode.OpenAt(&level, a.index, (int32_t)a.alien.size() + 100);
	Check(mode.Mode() == 1 && mode.CurrentModule() == a.index && mode.CurrentWaypoint() != (int32_t)a.alien.size() + 100 && mode.CurrentWaypoint() < (int32_t)a.alien.size(),
		"a box past the list's end opens A, with no such current waypoint (" + std::to_string(mode.CurrentWaypoint()) + ")");

	context.pickedModule = a.index;
	mode.Command(2, context);

	// ---- nothing written
	Check(level.BuildLevel(imageNow) && imageNow == imageBefore, "none of it wrote the level: its bytes are as they were");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
