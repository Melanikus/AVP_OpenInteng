// host_waypoint_net_level_test.cpp: the alien waypoint checker on whole stock levels (src/host/waypoint_net_level.h): every AI module's network is read as the loader reads it, its exits
// are one per neighbouring AI module, Invasion's known entry point is among them, and every level's networks are checked. What the checker finds in RAD's and Rebellion's own networks is
// printed, kind by kind, for the auto-plot's measurements (AvP-docs/engine/inteng/reassess/alien-autoplot-design.md, open points): the stock networks were never checked by anything.
// THE REPORT the waypoint-box gizmo gives after each edit (WaypointNets_ReportWords, WaypointNets_ReportModule): its words for none, one and many problems, and on Invasion the report
// of module 54's AI module names box 14's centre outside it (acceptance-waypoint-box-gizmo moves it inside).
// Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_waypoint_net_level_test.args builds it; host_waypoint_net_level_test <level file> [...]. A non-zero result means a check failed (not that a stock level has problems).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "level_file.h"
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

using namespace host;

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	std::map<std::string, size_t> allKinds;
	size_t levels = 0, modules = 0, boxes = 0, clean = 0, exits = 0;
	bool invasionSeen = false, invasionReported = false;

	// THE REPORT's words
	{
		std::vector<WaypointProblem> problems;

		Check(WaypointNets_ReportWords(problems, 0) == "checker: nothing found", "no problem: \"checker: nothing found\"");

		WaypointProblem one;

		one.kind = WaypointProblemKind::CentreOutside;
		one.box = 14;
		one.words = "box 14: its centre is outside it";
		problems.push_back(one);
		Check(WaypointNets_ReportWords(problems, 0) == "checker: box 14: its centre is outside it", "one: its own words");

		for (int k = 0; k < 4; k++)
		{
			WaypointProblem more;

			more.kind = WaypointProblemKind::CannotReach;
			more.words = "box " + std::to_string(k) + " cannot reach box 0";
			problems.push_back(more);
		}

		std::string const five = WaypointNets_ReportWords(problems, 0);
		std::string const past = WaypointNets_ReportWords(problems, 10);

		Check(five == "checker: 5 found: box 14: its centre is outside it; box 0 cannot reach box 0; box 1 cannot reach box 0; and 2 more", "five: the count, the first three, how many more: " + five);
		Check(past.compare(0, 19, "checker: 15 found: ") == 0 && past.find("; and 12 more") != std::string::npos, "and those past the ones kept count too: " + past);
		Check(WaypointNets_ReportModule(nullptr, 54).compare(0, 38, "checker: the level could not be read: ") == 0, "no level: the report says it could not be read");
	}

	for (int a = 1; a < argc; a++)
	{
		OpenLevelFile open;

		if (open.Open(argv[a]) != LevelOpenResult::Opened)
		{
			continue;
		}

		levels++;

		std::string const whole = argv[a];
		size_t const slash = whole.find_last_of("/\\");
		std::string const name = slash == std::string::npos ? whole : whole.substr(slash + 1);
		std::vector<WaypointNetModule> nets;
		std::string why;

		Check(WaypointNets_CheckLevel(&open, &nets, &why), name + ": its AI modules' networks are read" + (why.empty() ? "" : " (" + why + ")"));

		bool onePerNeighbour = true;
		std::map<std::string, size_t> kinds;

		for (WaypointNetModule const &net : nets)
		{
			modules++;
			boxes += net.boxes;
			exits += net.exits.size();
			clean += net.problems.empty() ? 1 : 0;

			for (size_t i = 0; i < net.exitTo.size(); i++)
			{
				for (size_t j = i + 1; j < net.exitTo.size(); j++)
				{
					onePerNeighbour = onePerNeighbour && net.exitTo[i] != net.exitTo[j];
				}
			}

			for (WaypointProblem const &p : net.problems)
			{
				kinds[WaypointProblemName(p.kind)]++;
				allKinds[WaypointProblemName(p.kind)]++;
			}

			if (net.more)
			{
				kinds["(more than kept)"] += net.more;
			}

			// Invasion's module 54: its entry point 3816 -38 23130 is an exit of its AI module (host_entry_markers_test and acceptance-gizmo-entry-point use that marker)
			if (name == "Invasion.RIF")
			{
				for (WaypointPoint const &e : net.exits)
				{
					bool const that = e.place[0] == 3816 && e.place[1] == -38 && e.place[2] == 23130;

					invasionSeen = invasionSeen || that;

					// that AI module's report, as the gizmo gives it after an edit: box 14's centre is outside it
					if (that)
					{
						std::string const report = WaypointNets_ReportModule(&open, net.master);

						invasionReported = report.find("box 14: its centre is outside it") != std::string::npos;
						printf("  Invasion, the AI module of master %d: %s\n", (int)net.master, report.c_str());
					}
				}
			}
		}

		if (name == "Invasion.RIF")
		{
			Check(WaypointNets_ReportModule(&open, -5) == "checker: no alien waypoints", "Invasion: a master of no alien network's report says so");
		}

		Check(onePerNeighbour, name + ": one exit per neighbouring AI module");

		std::string summary;

		for (std::pair<std::string const, size_t> const &k : kinds)
		{
			summary += (summary.empty() ? "" : ", ") + k.first + " " + std::to_string(k.second);
		}

		printf("  %s: %u networks, scale %.3f; problems: %s\n", name.c_str(), (unsigned)nets.size(), WaypointNets_Scale(&open), summary.empty() ? "none" : summary.c_str());
	}

	bool const invasionGiven = [&]() { for (int a = 1; a < argc; a++) if (strstr(argv[a], "Invasion.RIF")) return true; return false; }();

	Check(levels > 0, "at least one level opened (" + std::to_string(levels) + ")");
	Check(!invasionGiven || invasionSeen, "Invasion: the entry point 3816 -38 23130 is an exit of its AI module");
	Check(!invasionGiven || invasionReported, "Invasion: that AI module's report names box 14's centre outside it");

	std::string summary;

	for (std::pair<std::string const, size_t> const &k : allKinds)
	{
		summary += (summary.empty() ? "" : ", ") + k.first + " " + std::to_string(k.second);
	}

	printf("STOCK: %u levels, %u networks (%u with no problem), %u boxes, %u exits; problems: %s\n", (unsigned)levels, (unsigned)modules, (unsigned)clean, (unsigned)boxes, (unsigned)exits,
		summary.empty() ? "none" : summary.c_str());
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
