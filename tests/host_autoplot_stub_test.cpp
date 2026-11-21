// host_autoplot_stub_test.cpp: THE STUB'S CEILING (autoplot_volumes.h, a corridor's entries). Bishop's spec 11-21 (A): where a corridor's floor runs on past its last doorway
// (a dead-end stub), the ceiling entry of the doorway nearest that stub is extended over the stub to the floor's end, so the ceiling lane ends at the dead end; the entry's
// aim point stays at its doorway (no route changes, only the box's reach); with the gap closers on, the end wall's band (the floor box's top up to that entry) is then
// closed by the closers. opscor02 (Genshd1 AI module 151): a corridor 15 m long, one doorway at its north end and one half way along its west side, so both doorways lie
// in its north half and its entries (the corridor's halves) left the south 7.5 m with no ceiling box; the closers left its end wall ("no entry over it"). Hudson, whole-2b:
// 658 stalled frames in no box at (-13192, -17159, 58122) on that end wall, 73 mm over the floor box (aliens climbing the wall out of the floor box); rounds 1-6: 1,901
// (wholecorridor), 1,232 still there with the closers (Vasquez's closers-rounds1-6-2026-11-21.md).
// The cells are the planner's own (its cell map on a hidden whole Genshd1 plot of 0907c886, dietrich-proof/whole-map-0907c886): '#' a cell at the floor (y -15586), 'h' a
// higher one (a fitting on the west wall; any height over 100 above reads the same), '.' none; the ceiling y -19483 over every cell (lanes.exe cells on a stock copy).
// The exits, the doorways' middles and the AI module's box are lanes.exe exits on the same copy.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_closers.h"
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

int main()
{
	puts("opscor02 (Genshd1 AI module 151), a corridor whose floor runs on 7.5 m past its last doorway, through the volume planner with the closers on:");

	// the planner's map, rows iz 0 (z 73051, north) down to -49 (z 58351); the string's first character is cell -3 (the origin cell, ix 0, at x -17550)
	static char const *const rows[] = {
		"...####################",   // 0
		"...####################",   // -1
		"...####################",   // -2
		"...####################",   // -3
		"...####################",   // -4
		"...####################",   // -5
		"...####################",   // -6
		"...####################",   // -7
		"...####################",   // -8
		"...####################",   // -9
		"...####################",   // -10
		"...####################",   // -11
		"hh.####################",   // -12
		"...####################",   // -13
		".######################",   // -14
		".######################",   // -15
		".######################",   // -16
		".######################",   // -17
		".######################",   // -18
		".######################",   // -19
		".######################",   // -20
		".######################",   // -21
		".######################",   // -22
		".######################",   // -23
		".######################",   // -24
		".######################",   // -25
		".######################",   // -26
		".######################",   // -27
		".######################",   // -28
		".######################",   // -29
		".######################",   // -30
		".######################",   // -31
		".######################",   // -32
		".######################",   // -33
		".######################",   // -34
		".######################",   // -35
		".######################",   // -36
		"...####################",   // -37
		"hh.####################",   // -38
		"hh.####################",   // -39
		"...####################",   // -40
		"...####################",   // -41
		"...####################",   // -42
		"...####################",   // -43
		"...####################",   // -44
		"...####################",   // -45
		"...####################",   // -46
		"...####################",   // -47
		"...####################",   // -48
		"...####################",   // -49
	};

	host::AutoPlotFloor floor;

	floor.cellEdge = 300;

	for (size_t k = 0; k < sizeof(rows) / sizeof(rows[0]); k++)
	{
		for (size_t c = 0; rows[k][c] != '\0'; c++)
		{
			if (rows[k][c] == '#' || rows[k][c] == 'h')
			{
				int32_t const x = -17550 + 300 * ((int32_t)c - 3), z = 73051 - 300 * (int32_t)k;

				floor.cells.push_back(std::vector<int32_t>{ x, rows[k][c] == '#' ? -15586 : -15786, z });
				floor.ceilings.push_back(std::vector<int32_t>{ x, -19483, z });
			}
		}
	}

	// (the origin is the first cell given: the planner's own map starts at ix 0 of row 0, so the first '#' of row 0 is pushed first)
	floor.boxFound = true;   // lanes.exe exits: the AI module's box -18407 -19483 57670 .. -11196 -15586 73258
	floor.boxLo[0] = -18407, floor.boxLo[1] = -19483, floor.boxLo[2] = 57670;
	floor.boxHi[0] = -11196, floor.boxHi[1] = -15586, floor.boxHi[2] = 73258;

	// its exits (to opscen01 at the north end, to opsdr02 on the west side) and each doorway's middle half way to the neighbour's exit back (opscen01's at z 73157,
	// opsdr02's at x -18305); the soffits the ceiling, y -19483
	std::vector<std::vector<int32_t> > const exits = { { -14703, -15596, 73358 }, { -18507, -15596, 65950 } };
	std::vector<host::AutoPlotDoor> doors(2);

	doors[0].soffitFound = true, doors[0].soffit[0] = -14703, doors[0].soffit[1] = -19483, doors[0].soffit[2] = 73358;
	doors[0].throughFound = true, doors[0].middle[0] = -14703, doors[0].middle[1] = -15596, doors[0].middle[2] = 73258, doors[0].through[1] = 1.0;
	doors[1].soffitFound = true, doors[1].soffit[0] = -18507, doors[1].soffit[1] = -19483, doors[1].soffit[2] = 65950;
	doors[1].throughFound = true, doors[1].middle[0] = -18406, doors[1].middle[1] = -15596, doors[1].middle[2] = 65950, doors[1].through[0] = -1.0;

	host::AutoPlotClosers_SetOn(true);

	host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

	host::AutoPlotClosers_SetOn(false);

	std::string notes;

	for (std::string const &n : plan.notes)
	{
		notes += (notes.empty() ? "" : "; ") + n;
	}

	printf("  (%s, %zu boxes: %s)\n", plan.volumeKind.c_str(), plan.list.size(), notes.c_str());

	Check(plan.volumes && plan.volumeKind == "corridor", "shaped as a corridor (" + plan.volumeKind + ")");

	int32_t const entry0 = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, entry1 = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

	Check(entry0 >= 0 && entry1 >= 0 && Role(plan, (size_t)entry0) == "entry" && Role(plan, (size_t)entry1) == "entry", "an entry at each doorway (" +
		std::to_string((long long)entry0) + ", " + std::to_string((long long)entry1) + ")");

	if (entry0 >= 0 && entry1 >= 0)
	{
		host::WaypointRec const &e1 = plan.list[(size_t)entry1];
		int32_t const stubCeiling[3] = { -14700, -19383, 58500 };   // under the ceiling over the stub, by the end wall

		// THE STUB'S CEILING: the west doorway's entry (the doorway nearer the stub) reaches over the stub to the floor's end (z 58201)
		Check(Holds(e1, stubCeiling) && e1.min[2] <= 58201, "the west doorway's entry reaches over the stub to the floor's end (z " + std::to_string((long long)e1.min[2]) + " .. " +
			std::to_string((long long)e1.max[2]) + ")");

		// THE AIM POINTS STAY (no route changes, only the box's reach): both entries' centres as the plan had them before, at the climb-in's north end (x -12400, y -19383,
		// z 66901 on 0dc32759: the plan before the volume repair, which then slides the two shared centres apart)
		for (int32_t const e : { entry0, entry1 })
		{
			host::WaypointRec const &v = plan.list[(size_t)e];

			Check(v.centre[0] == -12400 && v.centre[1] == -19383 && v.centre[2] == 66901, "entry " + std::to_string((long long)e) + "'s aim as before (" + std::to_string((long long)v.centre[0]) +
				" " + std::to_string((long long)v.centre[1]) + " " + std::to_string((long long)v.centre[2]) + "; before -12400 -19383 66901)");
		}
	}

	// THE END WALL'S BAND CLOSED: a closer on the south end wall (the face at the AI module's box, z 57670) holds the stall spot of whole-2b
	int32_t const stall[3] = { -13192, -17159, 58122 };
	int32_t holder = -1;

	for (size_t b = 0; b < plan.list.size() && holder < 0; b++)
	{
		holder = Holds(plan.list[b], stall) ? (int32_t)b : -1;
	}

	Check(holder >= 0 && Role(plan, (size_t)holder) == "closer", "the stall spot on the end wall (-13192 -17159 58122) in a closer (box " + std::to_string((long long)holder) + ", " +
		(holder >= 0 ? Role(plan, (size_t)holder) : std::string("none")) + ")");

	Check(plan.exitsCovered == 2, "both exit points held (" + std::to_string((long long)plan.exitsCovered) + ")");
	Check(plan.parts == 1, "one network (" + std::to_string((long long)plan.parts) + " parts)");
	Check(plan.centresShared <= 2, "no more centres shared than before (" + std::to_string((long long)plan.centresShared) + "; 2 on 0dc32759, the two entries', which the volume repair slides apart)");

	printf("host_autoplot_stub_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
