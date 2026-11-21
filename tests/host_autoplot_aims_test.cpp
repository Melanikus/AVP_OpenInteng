// host_autoplot_aims_test.cpp: THE ENTRIES' AIM POINTS (autoplot_volumes.h). Bishop's spec 11-21 (c): on a wall-part climb (THE WALL PART, the floor-only corridors'
// rule b), the FAR entry's aim stays inside its own half (its own entry box), never landing where no candidate fits. A corridor's entries are its floor's two halves, one a
// doorway, and each aims at the climb-in's end nearer its doorway: a whole side's climb (2.4 m at the side's middle) reaches into both halves, but a wall part's climb can
// lie wholly in one half, and the other half's entry then aimed at a point in the first one's box (none fits: the volume repair moves it). new01 (Genshd1 AI module 396):
// Hudson's floor-only round 1 (Frost, run 123630): 449 stalled frames on its wall part, one run.
// The cells are the planner's own (its cell map on a hidden whole Genshd1 plot of 0907c886, dietrich-proof/whole-map-0907c886): '#' a cell at the floor (y 1948), 'h' a
// raised object's top (any height over 100 above the floor reads the same: a fitting), '.' none; the ceiling y -1363 over z 89850 .. 93450 (a lower band) and -1948 north
// of it (lanes.exe cells on a stock copy). The exits, the doorways' middles and the AI module's box are lanes.exe exits on the same copy.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_plan.h"
#include "autoplot_volumes.h"

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

static bool Holds(host::WaypointRec const &b, int32_t const p[3])
{
	for (int a = 0; a < 3; a++)
	{
		if (p[a] < b.min[a] || p[a] > b.max[a] - 1)
		{
			return false;
		}
	}

	return true;
}

static std::string Role(host::AutoPlotPlan const &plan, size_t b)
{
	return b < plan.volumeRoles.size() ? plan.volumeRoles[b] : std::string("?");
}

static std::string Notes(host::AutoPlotPlan const &plan)
{
	std::string notes;

	for (std::string const &n : plan.notes)
	{
		notes += (notes.empty() ? "" : "; ") + n;
	}

	return notes;
}

static std::string Point(int32_t const p[3])
{
	return std::to_string((long long)p[0]) + " " + std::to_string((long long)p[1]) + " " + std::to_string((long long)p[2]);
}

int main()
{
	puts("new01 (Genshd1 AI module 396), a corridor whose climb is a wall part in its east half, through the volume planner:");
	{
		// the planner's map, rows iz 0 (z 96750, north) down to -23 (z 89850); the string's first character is cell -3 (the origin cell, ix 0, at x -68250)
		static char const *const rows[] = {
			"...##########.............................##########",   // 0
			"...##########.............................##########",   // -1
			"...##########.............................##########",   // -2
			"...##########.............................##########",   // -3
			"...##########.............................##########",   // -4
			"...#################################################",   // -5
			"...#################################################",   // -6
			"...#################################################",   // -7
			"...#################################################",   // -8
			"...#################################################",   // -9
			"...########################################.........",   // -10
			"...########################################.hhhhhhhh",   // -11
			"...########################################.hhhhhhhh",   // -12
			"###########################################.hhhhhhhh",   // -13
			"###########################################.hhhhhhhh",   // -14
			"###########################################.........",   // -15
			"################################################....",   // -16
			"################################################.hh.",   // -17
			"################################################.hh.",   // -18
			"################################################....",   // -19
			"####################################################",   // -20
			"####################################################",   // -21
			"####################################################",   // -22
			"...########.##.############################.........",   // -23
		};

		host::AutoPlotFloor floor;

		floor.cellEdge = 300;

		for (size_t k = 0; k < sizeof(rows) / sizeof(rows[0]); k++)
		{
			for (size_t c = 0; rows[k][c] != '\0'; c++)
			{
				if (rows[k][c] == '#' || rows[k][c] == 'h')
				{
					int32_t const x = -68250 + 300 * ((int32_t)c - 3), z = 96750 - 300 * (int32_t)k;

					floor.cells.push_back(std::vector<int32_t>{ x, rows[k][c] == '#' ? 1948 : 1148, z });
					floor.ceilings.push_back(std::vector<int32_t>{ x, z <= 93450 ? -1363 : -1948, z });
				}
			}
		}

		floor.boxFound = true;   // lanes.exe exits: the AI module's box -69258 -1948 89624 .. -53280 1948 97417
		floor.boxLo[0] = -69258, floor.boxLo[1] = -1948, floor.boxLo[2] = 89624;
		floor.boxHi[0] = -53280, floor.boxHi[1] = 1948, floor.boxHi[2] = 97417;

		// its exits (to MED01 south, to new02 west) and each doorway's middle half way to the neighbour's exit back (MED01's at z 89724, new02's at x -69157); the soffits
		// the ceilings over the exit points (MED01's -1948, new02's lower band -1363)
		std::vector<std::vector<int32_t> > const exits = { { -61074, 1938, 89524 }, { -69358, 1938, 91572 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].soffitFound = true, doors[0].soffit[0] = -61074, doors[0].soffit[1] = -1948, doors[0].soffit[2] = 89524;
		doors[0].throughFound = true, doors[0].middle[0] = -61074, doors[0].middle[1] = 1938, doors[0].middle[2] = 89624, doors[0].through[1] = -1.0;
		doors[1].soffitFound = true, doors[1].soffit[0] = -69358, doors[1].soffit[1] = -1363, doors[1].soffit[2] = 91572;
		doors[1].throughFound = true, doors[1].middle[0] = -69258, doors[1].middle[1] = 1938, doors[1].middle[2] = 91572, doors[1].through[0] = -1.0;

		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		std::string const notes = Notes(plan);

		printf("  (%s, %zu boxes: %s)\n", plan.volumeKind.c_str(), plan.list.size(), notes.c_str());

		Check(plan.volumes && plan.volumeKind == "corridor" && notes.find("wall part") != std::string::npos, "shaped as a corridor, its climb on a wall part (" + plan.volumeKind + ")");

		int32_t const entry0 = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, entry1 = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

		Check(entry0 >= 0 && entry1 >= 0 && entry0 != entry1 && Role(plan, (size_t)entry0) == "entry" && Role(plan, (size_t)entry1) == "entry", "an entry at each doorway (" +
			std::to_string((long long)entry0) + ", " + std::to_string((long long)entry1) + ")");

		if (entry0 >= 0 && entry1 >= 0 && entry0 != entry1)
		{
			// EACH ENTRY'S AIM IN ITS OWN BOX AND NOT IN THE OTHER'S (the plan before the volume repair: an aim no candidate fits for is the first, shared)
			for (int const k : { 0, 1 })
			{
				int32_t const mine = k == 0 ? entry0 : entry1, other = k == 0 ? entry1 : entry0;
				int32_t const *const aim = plan.list[(size_t)mine].centre;

				Check(Holds(plan.list[(size_t)mine], aim) && !Holds(plan.list[(size_t)other], aim), "door" + std::to_string((long long)k) + "'s entry aims inside its own box, not the other entry's (" +
					Point(aim) + ")");

				// A BODY AT THE AIM IN ITS OWN ENTRY ONLY (Hudson floor-only round 1, run 123630: 291 of new01's 449 stalled frames at (-62, -1, 95) m held by entries 4 AND 5, at
				// the middle line where both aims were): the game takes the last box holding the body as its own (bh_waypoint.cpp 425-446) and goes on from the route's start
				// only when that is the start (177-182), so a hanging alien's body there (450 each way across) must not reach into the other entry
				host::WaypointRec const &o = plan.list[(size_t)other];
				bool const bodyInOther = aim[0] + 450 > o.min[0] && aim[0] - 450 < o.max[0] - 1 && aim[2] + 450 > o.min[2] && aim[2] - 450 < o.max[2] - 1 &&
					aim[1] > o.min[1] - 900 && aim[1] - 900 < o.max[1];

				Check(!bodyInOther, "door" + std::to_string((long long)k) + "'s entry: a body at its aim is not in the other entry too");
			}
		}

		Check(plan.centresShared == 0, "every centre in its own box only (" + std::to_string((long long)plan.centresShared) + " shared)");
		Check(plan.exitsCovered == 2 && plan.parts == 1, "both exits held, one network (" + std::to_string((long long)plan.parts) + " parts)");
	}

	printf("host_autoplot_aims_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
