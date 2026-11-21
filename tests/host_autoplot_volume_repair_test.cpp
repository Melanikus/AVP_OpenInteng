// host_autoplot_volume_repair_test.cpp: THE VOLUME REPAIR (autoplot_volume_repair.h, Drake 11-20). The lists are the volume planner's own, copied from the auto-plot's box
// lines of a whole Genshd1 run on Dietrich's 9a4a5b7c (shards/drake-whole-9a4a5b7c, the editor's output), where Frost's checker refused them:
//   med04 (AI module 184): "box 6: its centre is in box 4 too"  (a climb's top at the ceiling, inside the next doorway's entry)
//   obs01 (AI module 287): "box 2: its centre is in box 5 too (and 1 more)"
// V2 must move each such centre flat, inside its own box, to the nearest place no other box holds, and the checker must then find nothing; nothing else may change.
// THE ROLES: a box line says only "floor", "climb", "climb door<n>" (an entry) or "floor exit<n>", so the roles below are those words. The run WITH the repair
// (d523d3b3) names med04's box 6 a HIGH, so on the real plan V2 leaves it (the HIGH case below is that real case); as a plain climb it shows how V2 moves one.
// V1 must take out a box with no links and no exit point, renumbering the links and the plan's lists. A LOW or HIGH centre is never moved; a centre with no place is left.
// Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "autoplot_plan.h"
#include "autoplot_volume_repair.h"
#include "waypoint_net_check.h"

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

// One box line as the auto-plot prints it, after "box N <kind>: ": "min x y z max x y z centre x y z links 1ao 2a ..." (a: aliens only, 0x4; o: one way, 0x1)
static host::WaypointRec Box(char const *line)
{
	host::WaypointRec w = {};
	int n = 0;

	sscanf(line, "min %d %d %d max %d %d %d centre %d %d %d links%n", &w.min[0], &w.min[1], &w.min[2], &w.max[0], &w.max[1], &w.max[2], &w.centre[0], &w.centre[1],
		&w.centre[2], &n);

	for (char const *at = line + n; *at;)
	{
		while (*at == ' ')
		{
			at++;
		}

		if (!*at)
		{
			break;
		}

		host::WaypointLinkRec link;
		char *end = nullptr;

		link.index = (int32_t)strtol(at, &end, 10);
		link.flags = 0;
		at = end;

		for (; *at && *at != ' '; at++)
		{
			link.flags |= *at == 'a' ? host::kLinkAlien : *at == 'o' ? host::kLinkOneWay : 0;
		}

		w.links.push_back(link);
	}

	return w;
}

static host::AutoPlotPlan Plan(std::vector<char const *> const &lines, std::vector<char const *> const &roles)
{
	host::AutoPlotPlan plan;

	plan.volumes = true;

	for (size_t i = 0; i < lines.size(); i++)
	{
		plan.list.push_back(Box(lines[i]));
		plan.volumeRoles.push_back(roles[i]);
		plan.climbing.push_back(strcmp(roles[i], "floor") != 0 && strcmp(roles[i], "exit") != 0);
	}

	return plan;
}

static size_t Problems(host::AutoPlotPlan const &plan, std::vector<host::WaypointPoint> const &exits, host::WaypointProblemKind *first = nullptr)
{
	std::vector<host::WaypointProblem> const p = host::Waypoints_Check(plan.list, exits, 1.0, 50);

	if (first && !p.empty())
	{
		*first = p[0].kind;
	}

	return p.size();
}

static bool SameBoxes(host::AutoPlotPlan const &a, host::AutoPlotPlan const &b, size_t movedBox)
{
	if (a.list.size() != b.list.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.list.size(); i++)
	{
		host::WaypointRec const &x = a.list[i], &y = b.list[i];

		if (memcmp(x.min, y.min, sizeof(x.min)) || memcmp(x.max, y.max, sizeof(x.max)) || x.links.size() != y.links.size() || (i != movedBox && memcmp(x.centre, y.centre, sizeof(x.centre))))
		{
			return false;
		}

		for (size_t k = 0; k < x.links.size(); k++)
		{
			if (x.links[k].index != y.links[k].index || x.links[k].flags != y.links[k].flags)
			{
				return false;
			}
		}
	}

	return true;
}

// med04, 9a4a5b7c (its exit points: the planner's exit boxes are made round them; their centres stand for them here)
static std::vector<char const *> const kMed04 = {
	"min -53700 -7345 93600 max -46799 -5643 96901 centre -50250 -6595 95250 links 1ao 2a 7a 8a",
	"min -51450 -9941 96800 max -49049 -5643 97618 centre -49650 -9441 97300 links 0a 4ao",
	"min -51450 -8740 93600 max -49049 -7344 96451 centre -50250 -7346 95025 links 0ao 3a 4a",
	"min -50665 -9941 92920 max -46799 -9290 96901 centre -49050 -9841 96200 links 4a 7ao 2ao 5a",
	"min -54659 -9941 93600 max -50249 -9290 98018 centre -51450 -9841 96200 links 1a 3a 8ao 2ao 6a",
	"min -46900 -9941 93600 max -45870 -7793 96901 centre -46799 -9940 95250 links 3a",
	"min -55848 -9941 93600 max -53599 -7793 96901 centre -53701 -9940 95250 links 4a",
	"min -50665 -8740 92320 max -49464 -5643 94121 centre -50065 -7793 92520 links 3a 0ao",
	"min -54659 -8740 96817 max -53458 -5643 98618 centre -54059 -7793 98417 links 4a 0ao",
};
static std::vector<char const *> const kMed04Roles = { "floor", "climb", "climb", "entry", "entry", "climb", "climb", "exit", "exit" };
static std::vector<host::WaypointPoint> const kMed04Exits = { { { -50065, -7793, 92520 } }, { { -54059, -7793, 98417 } } };

// obs01, 9a4a5b7c
static std::vector<char const *> const kObs01 = {
	"min -30000 -7345 57000 max -29099 -5644 60001 centre -29550 -6595 57750 links 1ao 2a 5a 6a",
	"min -30000 -9941 59900 max -29099 -5644 60599 centre -29550 -9441 60400 links 0a 3ao 4ao",
	"min -30000 -8740 57000 max -29099 -7344 59551 centre -29550 -7346 58275 links 0ao 3a 4a",
	"min -29550 -9941 57000 max -28520 -9290 60001 centre -29100 -9841 59300 links 1a 4a 5ao 2ao",
	"min -30499 -9941 57000 max -29549 -9290 60001 centre -30000 -9841 59300 links 1a 3a 6ao 2ao",
	"min -29721 -8740 57849 max -27920 -5644 59050 centre -28121 -7793 58449 links 3a 0ao",
	"min -31099 -8740 57849 max -29298 -5644 59050 centre -30899 -7793 58449 links 4a 0ao",
};
static std::vector<char const *> const kObs01Roles = { "floor", "climb", "climb", "entry", "entry", "exit", "exit" };
static std::vector<host::WaypointPoint> const kObs01Exits = { { { -28121, -7793, 58449 } }, { { -30899, -7793, 58449 } } };

// centent (AI module 201) and EXT-01 (AI module 61), d523d3b3's run (the repair's V1 and V2 in; V2 named their HIGHs: centent's 7 and 10, EXT-01's 4). The roles: the
// box lines' words, and HIGH and LOW by their shape (a HIGH from the ceiling, 200 over it, down to the vertex; a LOW from 500 over the vertex down past the floor)
static std::vector<char const *> const kCentent = {
	"min -41100 330 132300 max -22199 2149 140401 centre -31650 1080 136350 links 1a 2ao 4a 15a",
	"min -41700 431 124800 max -33899 2149 132301 centre -37800 1181 128550 links 0a 3ao 14a",
	"min -32850 -559 140300 max -30449 2149 142490 centre -31650 -59 142389 links 0a 9ao",
	"min -43738 -509 127350 max -41599 2149 129751 centre -43638 -9 128550 links 1a 13ao",
	"min -41100 -947 132300 max -33899 331 139951 centre -37500 329 136125 links 0ao 5a 6a",
	"min -41700 -2148 124094 max -33899 -1497 140401 centre -41000 -2048 130210 links 6a 14ao 4ao 13a 8a 10a 12a",
	"min -41700 -2148 132300 max -21505 -1497 140401 centre -33300 -2048 139700 links 5a 15ao 4ao 9a 7a 11a",
	"min -43189 -2148 132300 max -40999 -59 140401 centre -41101 -2147 136350 links 6a",
	"min -41100 -2148 140300 max -33899 -59 142490 centre -37500 -2147 140401 links 5a",
	"min -33900 -2148 140300 max -22199 -59 142490 centre -31050 -2147 140401 links 2a 6ao",
	"min -41100 -2148 130211 max -33899 -59 132401 centre -37500 -2147 132299 links 5a",
	"min -33900 -2148 130211 max -22199 -59 132401 centre -28050 -2147 132299 links 6a",
	"min -34000 -2148 124800 max -31760 -9 132301 centre -33899 -2147 128550 links 5a",
	"min -43738 -2148 124800 max -41599 -9 132301 centre -41701 -2147 129150 links 3a 5ao",
	"min -38292 -947 123494 max -37091 2149 125295 centre -37692 0 123694 links 5a 1ao",
	"min -22706 -947 135782 max -20905 2149 136983 centre -21106 0 136382 links 6a 0ao",
};
static std::vector<char const *> const kCententRoles = { "floor", "floor", "LOW", "LOW", "climb", "entry", "entry", "HIGH", "HIGH", "HIGH", "HIGH", "HIGH", "HIGH", "HIGH",
	"exit", "exit" };
static std::vector<host::WaypointPoint> const kCententExits = { { { -37692, 0, 123694 } }, { { -21106, 0, 136382 } } };

static std::vector<char const *> const kExt01 = {
	"min -26100 447 41400 max -22499 2148 46201 centre -24300 1197 43800 links 1ao 2a 5a",
	"min -22600 -2538 42600 max -21906 2148 45001 centre -22100 -2038 43800 links 0a 3ao",
	"min -26100 -1337 42600 max -22949 448 45001 centre -24525 446 43800 links 0ao 3a",
	"min -32444 -11890 41400 max -22499 -1887 47360 centre -23200 -11790 45000 links 1a 5ao 2ao 4a",
	"min -28443 -2538 41400 max -25999 -196 46201 centre -26101 -2537 43800 links 3a",
	"min -32444 -10689 46159 max -31243 -1195 47960 centre -31844 -6543 47759 links 3a 0ao",
};
static std::vector<char const *> const kExt01Roles = { "floor", "climb", "LOW", "entry", "HIGH", "exit" };
static std::vector<host::WaypointPoint> const kExt01Exits = { { { -31844, -6543, 47759 } } };

// doormp06 (264), office01 (289), MED01 (274): Dietrich's plot of the owner's second stretch on the rooms exe 7cba3497 (dietrich-proof/owner-stretch2-rooms-7cba3497,
// e5f6f361), refused. Their exits: the level's own (lanes exits --all), each AI module's in its order
static std::vector<char const *> const kDoormp06 = {
	"min -88500 58 72300 max -84299 2149 78001 centre -86550 808 75150 links 1a 2a",
	"min -87001 -897 77335 max -85800 2149 79136 centre -86401 0 78935 links 0a",
	"min -87001 -574 71375 max -85800 1759 73176 centre -86401 -34 71575 links 0a",
};
static std::vector<char const *> const kDoormp06Roles = { "floor", "exit", "exit" };
static std::vector<host::WaypointPoint> const kDoormp06Exits = { { { -86402, 0, 85732 } }, { { -86401, 1938, 78035 } }, { { -86401, 1548, 72475 } } };

static std::vector<char const *> const kOffice01 = {
	"min -79200 448 79500 max -70199 2149 85501 centre -74850 1198 82350 links 1a 3a",
	"min -75600 448 85500 max -73199 2149 89701 centre -74550 1198 87450 links 0a 2a",
	"min -75312 -312 89024 max -74111 2149 90825 centre -74712 292 90624 links 1a",
	"min -81367 -312 83179 max -79566 2149 84380 centre -81167 292 83779 links 0a",
};
static std::vector<char const *> const kOffice01Roles = { "floor", "floor", "exit", "exit" };
static std::vector<host::WaypointPoint> const kOffice01Exits = { { { -68863, 0, 83779 } }, { { -80562, 0, 87675 } }, { { -74712, 1938, 89724 } }, { { -80267, 1938, 83779 } } };

static std::vector<char const *> const kMed01 = {
	"min -65700 396 78300 max -56399 2149 82801 centre -61050 1146 80550 links 2ao 3a 6a",
	"min -64800 448 85500 max -56399 2149 89701 centre -60600 1198 87600 links 7a",
	"min -62250 -2148 77734 max -59849 2149 78401 centre -61050 -1648 77900 links 0a 4ao",
	"min -62250 -947 78750 max -59849 397 82801 centre -61050 395 80775 links 0ao 4a",
	"min -69857 -2148 78300 max -56399 -523 84380 centre -65400 -1848 79000 links 2a 6ao 3ao",
	"min -64800 -2148 85500 max -56399 -912 90225 centre -60600 -1848 85500 links 7ao",
	"min -70457 -1 83179 max -68656 201 84380 centre -70257 0 83779 links 4a 0ao",
	"min -61674 -312 89024 max -60473 2149 90825 centre -61074 292 90624 links 5a 1ao",
};
static std::vector<char const *> const kMed01Roles = { "floor", "floor", "climb", "get-out", "entry", "entry", "exit", "exit" };
static std::vector<host::WaypointPoint> const kMed01Exits = { { { -69651, 0, 83779 } }, { { -61074, 1938, 89724 } } };

// a and b each list the other, aliens, neither way stopped (no One way, no One way the other direction)
static bool LinkedBothWays(std::vector<host::WaypointRec> const &list, size_t a, size_t b)
{
	auto const plain = [&list](size_t from, size_t to)
	{
		for (host::WaypointLinkRec const &l : list[from].links)
		{
			if (l.index == (int32_t)to)
			{
				return (l.flags & (host::kLinkOneWay | host::kLinkOneWayBack)) == 0 && (l.flags & host::kLinkAlien) != 0;
			}
		}

		return false;
	};

	return a < list.size() && b < list.size() && plain(a, b) && plain(b, a);
}

int main()
{
	printf("THE STARTING POINT: the checker refuses both lists as the planner made them\n");
	{
		host::AutoPlotPlan const med04 = Plan(kMed04, kMed04Roles), obs01 = Plan(kObs01, kObs01Roles);
		host::WaypointProblemKind k1 = host::WaypointProblemKind::NoRoom, k2 = host::WaypointProblemKind::NoRoom;

		Check(Problems(med04, kMed04Exits, &k1) == 1 && k1 == host::WaypointProblemKind::CentreInAnother, "med04: one problem, box 6's centre in box 4 (as the run said)");
		Check(Problems(obs01, kObs01Exits, &k2) == 2 && k2 == host::WaypointProblemKind::CentreInAnother, "obs01: two problems, box 2's centre in box 5 first (as the run said)");
	}

	printf("V2 ON med04: box 6's centre moved flat, 959 mm along x (its long side, z, has no free place: box 4 covers it), to just past box 4's end; the checker finds nothing\n");
	{
		host::AutoPlotPlan const before = Plan(kMed04, kMed04Roles);
		host::AutoPlotPlan plan = before;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kMed04Exits, 1.0);

		Check(done.moved == 1 && done.removed == 0 && done.left == 0, "one centre moved, none taken out or left");
		Check(plan.list[6].centre[0] == -54660 && plan.list[6].centre[1] == -9940 && plan.list[6].centre[2] == 95250, "box 6's centre is -54660 -9940 95250 (x only; its height kept)");
		Check(Problems(plan, kMed04Exits) == 0, "the checker finds nothing");
		Check(SameBoxes(before, plan, 6), "every box, link and every other centre as the planner made them");
		Check(!plan.notes.empty() && plan.notes.back().find("repair: V2 moved box 6 (climb)'s centre 959 mm along x") == 0, "the note names it: " + (plan.notes.empty() ? std::string("(none)") : plan.notes.back()));
	}

	printf("V2 ON obs01: box 2's centre moved 427 mm along z, its long side; the checker finds nothing\n");
	{
		host::AutoPlotPlan const before = Plan(kObs01, kObs01Roles);
		host::AutoPlotPlan plan = before;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kObs01Exits, 1.0);

		Check(done.moved == 1 && done.left == 0, "one centre moved");
		Check(plan.list[2].centre[0] == -29550 && plan.list[2].centre[1] == -7346 && plan.list[2].centre[2] == 57848, "box 2's centre is -29550 -7346 57848 (just before box 5's start on z)");
		Check(Problems(plan, kObs01Exits) == 0, "the checker finds nothing");
		Check(SameBoxes(before, plan, 2), "nothing else changed");
	}

	printf("V3 SLIDE: med04's box 6 is the HIGH it really is; the entry (box 4) reaches 959 mm past its centre, inside its face's reach, so the centre slides out ALONG ITS FACE:\n");
	printf("959 mm out on x and, the face being 45 degrees (from the ceiling, -9941 + 200, down 1948 to the vertex over a reach of 1948), 959 mm down; the checker finds nothing\n");
	{
		std::vector<char const *> roles = kMed04Roles;
		roles[6] = "HIGH";
		host::AutoPlotPlan const before = Plan(kMed04, roles);
		host::AutoPlotPlan plan = before;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kMed04Exits, 1.0);

		Check(done.moved == 1 && done.left == 0 && done.removed == 0, "one centre moved");
		Check(plan.list[6].centre[0] == -54660 && plan.list[6].centre[1] == -8981 && plan.list[6].centre[2] == 95250, "box 6's centre is -54660 -8981 95250 (200 over its face, as at the ceiling's edge)");
		Check(Problems(plan, kMed04Exits) == 0, "the checker finds nothing");
		Check(SameBoxes(before, plan, 6), "nothing else changed");
		Check(!plan.notes.empty() && plan.notes.back().find("V3 slid box 6 (HIGH)'s centre 959 mm out along its face (x), 959 mm down, past box 4 (entry)") != std::string::npos, "the note names it: " + (plan.notes.empty() ? std::string("(none)") : plan.notes.back()));
	}

	printf("A LOW CENTRE IS NEVER MOVED (it sits on its face): med04 with box 6 called LOW is left, and named\n");
	{
		std::vector<char const *> roles = kMed04Roles;
		roles[6] = "LOW";
		host::AutoPlotPlan const before = Plan(kMed04, roles);
		host::AutoPlotPlan plan = before;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kMed04Exits, 1.0);

		Check(done.moved == 0 && done.left == 1 && SameBoxes(before, plan, (size_t)-1), "none moved, one left, the list as it was");
		Check(!plan.notes.empty() && plan.notes.back().find("V2 left box 6 (LOW): its centre sits on its sloped face") != std::string::npos, "the note names it");
	}

	printf("V3 ON centent (AI module 201, d523d3b3's run: \"box 7: its centre is in box 5 too (and 2 more)\"): HIGH 7 slides 600 mm out along its 45-degree face past both\n");
	printf("entries; HIGH 10, a jump catcher (one link, to entry 5) that entry 5 reaches 8.2 m past, is taken out; the checker finds nothing\n");
	{
		host::AutoPlotPlan plan = Plan(kCentent, kCententRoles);
		host::AutoPlotPlan const before = plan;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kCententExits, 1.0);

		Check(done.removed == 1 && done.left == 0, "one taken out, none left");
		Check(plan.list.size() == 15 && plan.volumeRoles.size() == 15 && plan.climbing.size() == 15, "15 boxes left");
		Check(plan.list[7].centre[0] == -41701 && plan.list[7].centre[1] == -1547 && plan.list[7].centre[2] == 136350, "HIGH 7's centre is -41701 -1547 136350");
		Check(memcmp(plan.list[10].min, before.list[11].min, sizeof(plan.list[10].min)) == 0 && plan.volumeRoles[10] == "HIGH", "the old box 11 is box 10 now");

		std::vector<int32_t> links5;

		for (host::WaypointLinkRec const &l : plan.list[5].links)
		{
			links5.push_back(l.index);
		}

		Check(links5 == std::vector<int32_t>({ 6, 13, 4, 12, 8, 11 }), "entry 5's links 6 14 4 13 8 10 12 are 6 13 4 12 8 11: the old 10 gone, the ones past it one down");
		Check(Problems(plan, kCententExits) == 0, "the checker finds nothing");
		Check(!plan.notes.empty() && plan.notes.back().find("V3 took out box 10 (HIGH): a jump catcher inside its entry box 5 (entry)") != std::string::npos, "the note names both: " + (plan.notes.empty() ? std::string("(none)") : plan.notes.back()));
	}

	printf("V3 ON EXT-01 (AI module 61): its HIGH 4 is a jump catcher inside the dead end's entry, which reaches 6.3 m past it to the doorway: taken out; the checker finds nothing\n");
	{
		host::AutoPlotPlan plan = Plan(kExt01, kExt01Roles);
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kExt01Exits, 1.0);

		Check(done.removed == 1 && plan.list.size() == 5, "taken out: 5 boxes");
		Check(Problems(plan, kExt01Exits) == 0, "the checker finds nothing");
	}

	printf("V3 LEAVES A HIGH ON A CLIMB that the entry covers outward (EXT-01's HIGH 4 given a second link, as a climb's HIGH has): named, for the planner\n");
	{
		host::AutoPlotPlan plan = Plan(kExt01, kExt01Roles);

		plan.list[4].links.push_back({ 0, host::kLinkAlien });   // both ways, as the planner's links are
		plan.list[0].links.push_back({ 4, host::kLinkAlien });

		host::AutoPlotPlan const before = plan;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kExt01Exits, 1.0);

		Check(done.left == 1 && done.removed == 0 && SameBoxes(before, plan, (size_t)-1), "left, the list as it was");
		Check(!plan.notes.empty() && plan.notes.back().find("V3 left box 4 (HIGH): on a climb, and box 3 (entry) reaches past its whole face") != std::string::npos, "the note names it");
	}

	printf("NO PLACE: a box whose every flat place at its centre's height is inside another box is left, and named (the checker refuses it, as before)\n");
	{
		host::AutoPlotPlan plan = Plan({ "min 0 0 0 max 4000 1000 4000 centre 2000 500 2000 links 1a", "min 1000 0 1000 max 3000 1000 3000 centre 2000 500 2000 links 0a" },
			{ "floor", "climb" });
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, {}, 1.0);

		Check(done.left == 1 && done.moved == 1, "the outer box's centre moved out of the inner one, the inner box's left");
		Check(plan.list[1].centre[0] == 2000 && plan.list[1].centre[2] == 2000, "the inner box's centre where it was");
		Check(!plan.notes.empty() && plan.notes.back().find("V2 left box 1 (climb): no place in it outside the other boxes") != std::string::npos, "the note names it");
	}

	printf("V1: a box with no links and no exit point is taken out; the links after it, doorBox, exitBox, doorFloors, climbing and the roles follow\n");
	{
		host::AutoPlotPlan const repaired = [] { host::AutoPlotPlan p = Plan(kMed04, kMed04Roles); host::AutoPlot_RepairVolumes(&p, kMed04Exits, 1.0); return p; }();
		std::vector<char const *> lines = kMed04, roles = kMed04Roles;

		// a scrap put in at place 3: every link to 3 and past it moves up one
		std::vector<char const *> scrapLines, scrapRoles;

		for (size_t i = 0; i < lines.size(); i++)
		{
			if (i == 3)
			{
				scrapLines.push_back("min 0 0 0 max 600 600 600 centre 300 300 300 links");
				scrapRoles.push_back("climb");
			}

			scrapLines.push_back(lines[i]);
			scrapRoles.push_back(roles[i]);
		}

		host::AutoPlotPlan plan = Plan(scrapLines, scrapRoles);

		for (host::WaypointRec &w : plan.list)
		{
			for (host::WaypointLinkRec &l : w.links)
			{
				l.index += l.index >= 3 ? 1 : 0;
			}
		}

		plan.floorBoxes = 3;
		plan.doorBox = { 4, 5 };
		plan.exitBox = { 8, 9 };
		plan.doorFloors = { { 0, 3, 4 }, { 9 } };
		plan.climbing[3] = true;

		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kMed04Exits, 1.0);

		Check(done.removed == 1 && done.moved == 1, "one taken out, then med04's centre moved as before");
		Check(plan.list.size() == 9 && plan.climbing.size() == 9 && plan.volumeRoles.size() == 9, "9 boxes, 9 climbing, 9 roles");
		Check(SameBoxes(repaired, plan, (size_t)-1), "the list is med04's repaired list, box for box, link for link");
		Check(plan.doorBox == std::vector<int32_t>({ 3, 4 }) && plan.exitBox == std::vector<int32_t>({ 7, 8 }), "doorBox 3 4, exitBox 7 8");
		Check(plan.doorFloors.size() == 2 && plan.doorFloors[0] == std::vector<int32_t>({ 0, 3 }) && plan.doorFloors[1] == std::vector<int32_t>({ 8 }), "doorFloors: the scrap gone from the first, the rest renumbered");
		Check(plan.floorBoxes == 3, "floorBoxes kept (the scrap was a climbing box)");
		Check(Problems(plan, kMed04Exits) == 0, "the checker finds nothing");
		Check(!plan.notes.empty() && plan.notes.back().find("repair: V1 took out box 3 (climb: no links, no exit point)") == 0, "the note names it first: " + (plan.notes.empty() ? std::string("(none)") : plan.notes.back()));
	}

	printf("V4 ON doormp06 (AI module 264, Dietrich's rooms plot of the owner's 2nd stretch, e5f6f361: \"exit 0 (-86402 0 85732) lies in no box\"): its floor-only network\n");
	printf("(the levels' floor walk) left out the exit 1.9 m over the floor; V4 makes a box round it, as the planner's exit rule does, down to the floor, linked both ways\n");
	{
		host::AutoPlotPlan plan = Plan(kDoormp06, kDoormp06Roles);
		host::WaypointProblemKind first = host::WaypointProblemKind::NoRoom;

		Check(Problems(plan, kDoormp06Exits, &first) == 1 && first == host::WaypointProblemKind::ExitOutside, "as planned: one problem, exit 0 in no box");

		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kDoormp06Exits, 1.0);

		Check(plan.list.size() == 4 && plan.volumeRoles.size() == 4 && plan.volumeRoles[3] == "exit" && !plan.climbing[3], "one exit box added, box 3");
		bool const made = plan.list.size() == 4;

		Check(made && plan.list[3].min[0] == -87002 && plan.list[3].max[0] == -85801 && plan.list[3].min[2] == 85132 && plan.list[3].max[2] == 86333 && plan.list[3].min[1] == -1000 &&
			plan.list[3].max[1] == 2149, "600 each side of the point, from 1000 over it down to its floor box's foot (2149)");
		Check(made && plan.list[3].centre[0] == -86402 && plan.list[3].centre[1] == 0 && plan.list[3].centre[2] == 85732, "its centre the exit point");
		Check(LinkedBothWays(plan.list, 3, 0), "linked both ways with the floor box nearest it (box 0)");
		Check(Problems(plan, kDoormp06Exits) == 0, "the checker finds nothing");
		Check(done.removed == 0, "nothing taken out");
		Check(!plan.notes.empty() && plan.notes.back().find("V4 made box 3 (exit) round exit 0's point, linked both ways to box 0 (floor)") != std::string::npos, "named: " + (plan.notes.empty() ? std::string("(none)") : plan.notes.back()));
	}

	printf("V4 ON office01 (AI module 289, e5f6f361: \"exit 0 (-68863 0 83779) lies in no box (and 1 more)\"): both exits its floor walk left out get a box; the checker finds nothing\n");
	{
		host::AutoPlotPlan plan = Plan(kOffice01, kOffice01Roles);

		Check(Problems(plan, kOffice01Exits) == 2, "as planned: two exits in no box");
		host::AutoPlot_RepairVolumes(&plan, kOffice01Exits, 1.0);
		Check(plan.list.size() == 6 && plan.volumeRoles[4] == "exit" && plan.volumeRoles[5] == "exit", "two exit boxes added");
		Check(LinkedBothWays(plan.list, 4, 0) && LinkedBothWays(plan.list, 5, 0), "each linked both ways with its nearest floor box (box 0 for both)");
		Check(Problems(plan, kOffice01Exits) == 0, "the checker finds nothing");
	}

	printf("V5 ON MED01 (AI module 274, e5f6f361: \"box 0 cannot reach box 1 (and 5 more)\", the owner's start): two parts, the south floor's (0, 2, 3, 4, 6) and the north's\n");
	printf("(1, 5, 7), the room's two ceiling entries 1.1 m apart. V5 stretches entry 5 to meet entry 4 and links them both ways; then the north floor, a dead end (its one link\n");
	printf("comes in from exit 7, which comes in from entry 5), gets its way back: 1-7 and 7-5 both ways. The checker finds nothing\n");
	{
		host::AutoPlotPlan plan = Plan(kMed01, kMed01Roles);
		host::WaypointProblemKind first = host::WaypointProblemKind::NoRoom;

		Check(Problems(plan, kMed01Exits, &first) > 0 && first == host::WaypointProblemKind::CannotReach, "as planned: cannot reach");
		host::AutoPlot_RepairVolumes(&plan, kMed01Exits, 1.0);
		Check(plan.list.size() == 8, "no box added or taken out");
		Check(plan.list[5].min[2] == 84379 && plan.list[5].max[2] == 90225, "entry 5 reaches entry 4 now (its south side from 85500 to 84379)");
		Check(LinkedBothWays(plan.list, 4, 5), "entries 4 and 5 linked both ways");
		Check(LinkedBothWays(plan.list, 1, 7) && LinkedBothWays(plan.list, 7, 5), "the north floor's way back: 1-7 and 7-5 both ways");
		Check(Problems(plan, kMed01Exits) == 0, "the checker finds nothing");
		Check(!plan.notes.empty() && plan.notes.back().find("V5 joined box 5 (entry) to box 4 (entry)") != std::string::npos, "named: " + (plan.notes.empty() ? std::string("(none)") : plan.notes.back()));
	}

	printf("A BOX WITH NO LINKS THAT HOLDS AN EXIT POINT STAYS (the exit point needs a box)\n");
	{
		host::AutoPlotPlan plan = Plan({ "min 0 0 0 max 600 600 600 centre 300 300 300 links" }, { "exit" });
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, { { { 100, 100, 100 } } }, 1.0);

		Check(done.removed == 0 && plan.list.size() == 1 && plan.notes.empty(), "kept, and no note");
	}

	printf("THE OLD PLANNER'S LIST IS NOT TOUCHED (plan.volumes false)\n");
	{
		host::AutoPlotPlan plan = Plan(kMed04, kMed04Roles);
		plan.volumes = false;
		host::AutoPlotPlan const before = plan;
		host::AutoPlotVolumeRepair const done = host::AutoPlot_RepairVolumes(&plan, kMed04Exits, 1.0);

		Check(done.moved == 0 && done.removed == 0 && done.left == 0 && SameBoxes(before, plan, (size_t)-1) && plan.notes.empty(), "as it was, no note");
		Check(host::AutoPlot_RepairVolumes(nullptr, kMed04Exits, 1.0).moved == 0, "no plan: nothing, no crash");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
