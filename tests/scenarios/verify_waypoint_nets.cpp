// verify_waypoint_nets.cpp: the check program of the alien auto-plot scenarios (tests/scenarios/acceptance-autoplot-*.txt): a level saved by Save a copy, read back through the game's own
// loader, and its AI modules' alien networks checked as the stock game would use them (src/host/waypoint_net_level.h, waypoint_net_check.h). Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_waypoint_nets <the saved copy> <master> <check> [<check> ...]
//   master: the AI module's master's object index (as Waypoint mode counts modules), or "all" for every AI module that has a network
//   check: network     it has an alien network (one box at least)
//          exits       every exit point lies in a box (EXIT OUTSIDE: none)
//          connected   every box reaches every other (CANNOT REACH: none)
//          most        99 boxes at most (TOO MANY: none)
//          links       every link in the list (LINK OUTSIDE: none), and no large target into a box with no room
//          clean       no problem of any kind
// It passes (exit 0) when every check does; the lines say what was found.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "level_file.h"
#include "waypoint_net_level.h"

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

static int failed = 0;

static void Check(bool ok, std::string const &what)
{
	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
	failed += ok ? 0 : 1;
}

static size_t CountOf(WaypointNetModule const &net, WaypointProblemKind kind)
{
	size_t n = 0;

	for (WaypointProblem const &p : net.problems)
	{
		n += p.kind == kind ? 1 : 0;
	}

	return n;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 4)
	{
		printf("  FAILED  usage: verify_waypoint_nets <the saved copy> <master|all> <check> [<check> ...]\n");
		return 1;
	}

	OpenLevelFile open;

	Check(open.Open(argv[1]) == LevelOpenResult::Opened, std::string("the saved copy is taken by the game's loader: ") + argv[1]);

	if (failed)
	{
		return 1;
	}

	std::vector<WaypointNetModule> nets;
	std::string why;

	Check(WaypointNets_CheckLevel(&open, &nets, &why), "its AI modules' networks are read" + (why.empty() ? std::string() : " (" + why + ")"));

	bool const all = strcmp(argv[2], "all") == 0;
	char *end = nullptr;
	long const master = all ? -1 : strtol(argv[2], &end, 10);

	if (!all && (!end || *end || master < 0))
	{
		Check(false, std::string("\"") + argv[2] + "\" is neither a module's number nor all");
		return 1;
	}

	std::vector<WaypointNetModule const *> chosen;

	for (WaypointNetModule const &net : nets)
	{
		if (all || net.master == (int32_t)master)
		{
			chosen.push_back(&net);
		}
	}

	for (int a = 3; a < argc; a++)
	{
		std::string const check = argv[a];

		if (check == "network")
		{
			Check(!chosen.empty() && chosen[0]->boxes > 0, all ? std::to_string(chosen.size()) + " AI modules have a network" :
				"AI module " + std::string(argv[2]) + " has a network of " + (chosen.empty() ? std::string("no") : std::to_string(chosen[0]->boxes)) + " boxes");
			continue;
		}

		struct Rule
		{
			char const *word;
			std::vector<WaypointProblemKind> kinds;
		};

		static Rule const rules[] = {
			{ "exits", { WaypointProblemKind::ExitOutside } },
			{ "connected", { WaypointProblemKind::CannotReach } },
			{ "most", { WaypointProblemKind::TooMany } },
			{ "links", { WaypointProblemKind::LinkOutside, WaypointProblemKind::LargeTargetSmallBox } },
		};

		bool known = check == "clean";
		std::vector<WaypointProblemKind> kinds;

		for (Rule const &r : rules)
		{
			if (check == r.word)
			{
				known = true;
				kinds = r.kinds;
			}
		}

		if (!known)
		{
			Check(false, "a check this program does not know: " + check);
			continue;
		}

		if (chosen.empty())
		{
			Check(false, check + ": no AI module " + argv[2] + " with a network");
			continue;
		}

		for (WaypointNetModule const *net : chosen)
		{
			size_t found = 0;
			std::string first;

			for (WaypointProblem const &p : net->problems)
			{
				bool counts = check == "clean";

				for (WaypointProblemKind const k : kinds)
				{
					counts = counts || p.kind == k;
				}

				if (counts)
				{
					found++;
					first = first.empty() ? p.words : first;
				}
			}

			found += check == "clean" ? net->more : 0;
			Check(found == 0, "AI module " + std::to_string(net->master) + " (" + std::to_string(net->boxes) + " boxes, " + std::to_string(net->exits.size()) + " exits): " + check + ": " +
				(found ? std::to_string(found) + " problems, e.g. " + first : std::string("none")));
		}
	}

	return failed ? 1 : 0;
}
