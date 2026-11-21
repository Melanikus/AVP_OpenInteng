// host_autoplot_floorside_test.cpp: THE FLOOR-ONLY CORRIDORS (autoplot_volumes.h, the climb side). Gap cause 2 (Bishop 11-21: 25% of the no-box stalls): the volume
// planner climbs a floor's wall only on a WHOLE side with no doorway on it, no other floor starting at it, and floor going on past it for under a quarter of its length.
// A corridor that loses every side that way gets no climb-in and no entry: the levels' rule's floor walk only ("corridor, floor only"), and the ceiling lanes of its
// neighbours end under its ceiling. Hudson's whole-corridor runs (Vasquez's gaps-2026-11-21.txt): SHED04 269 stalled frames (aliens come in on the ceiling over its exit
// box), new02 469.
// (a) THE WALL FOOT (Bishop's spec 11-21): floor past a side counts as "floor beyond" only where it goes on for at least 2 rows (600 mm); a one-row strip at the wall's
// foot is the foot, and the climb box reaches over it to the wall. SHED04 (Genshd1 AI module 225): a tube corridor 18.3 m long, its north side a ragged one-row strip
// at the floor's height under the wall, its south side the side room's seam, its ends the doorways.
// The cells are the planner's own (its cell map, AUTOPLOT_VOLUME_MAP, on a hidden whole Genshd1 plot of bishop-int 0907c886: dietrich-proof/whole-map-0907c886): '#' a
// cell at the floor's height (y 1948), 'h' a higher one (y 1819, lanes.exe cells on a copy of the stock Genshd1.RIF), '.' none; the ceiling over every cell the tube's top,
// y -1948 (lanes.exe). The exits, the doorways and the AI module's box are lanes.exe exits on the same copy.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <string.h>

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

static int32_t FlagsOf(host::WaypointRec const &b, int32_t to)
{
	for (host::WaypointLinkRec const &l : b.links)
	{
		if (l.index == to)
		{
			return l.flags;
		}
	}

	return -1;
}

// every box reaches every other along the links, both ways (the checker's "box 0 cannot reach box 1" when not)
static bool AllReach(std::vector<host::WaypointRec> const &list)
{
	for (bool const forward : { true, false })
	{
		std::vector<bool> seen(list.size(), false);
		std::vector<size_t> todo(1, 0);

		seen[0] = !list.empty();

		while (!todo.empty() && !list.empty())
		{
			size_t const at = todo.back();

			todo.pop_back();

			for (size_t b = 0; b < list.size(); b++)
			{
				int32_t const f = forward ? FlagsOf(list[at], (int32_t)b) : FlagsOf(list[b], (int32_t)at);

				if (!seen[b] && f >= 0 && (f & host::kLinkOneWayBack) == 0)
				{
					seen[b] = true;
					todo.push_back(b);
				}
			}
		}

		for (bool const s : seen)
		{
			if (!s)
			{
				return false;
			}
		}
	}

	return true;
}

// whether box `to` is reached from box `from` along links that stay on climbing boxes (the ceiling's way)
static bool ReachesOnCeiling(host::AutoPlotPlan const &plan, int32_t from, int32_t to)
{
	if (from < 0 || to < 0 || (size_t)from >= plan.list.size() || (size_t)to >= plan.list.size())
	{
		return false;
	}

	std::vector<bool> seen(plan.list.size(), false);
	std::vector<int32_t> todo(1, from);

	seen[(size_t)from] = true;

	while (!todo.empty())
	{
		int32_t const at = todo.back();

		todo.pop_back();

		for (host::WaypointLinkRec const &l : plan.list[(size_t)at].links)
		{
			size_t const b = (size_t)l.index;

			if (b < plan.list.size() && !seen[b] && (l.flags & host::kLinkOneWayBack) == 0 && plan.climbing[b])
			{
				seen[b] = true;
				todo.push_back((int32_t)b);
			}
		}
	}

	return seen[(size_t)to];
}

static std::string Role(host::AutoPlotPlan const &plan, size_t b)
{
	return b < plan.volumeRoles.size() ? plan.volumeRoles[b] : std::string("?");
}

// A floor from the planner's own cell map: row k of `rows` is cell line iz = top - k, its characters cells ix = 0, 1, ... from the origin; '#' a cell at y `floorY`, 'h' one
// at y `highY`, anything else none; the ceiling over each at y `ceilingY`
static host::AutoPlotFloor FromMap(char const *const *rows, size_t count, int top, int32_t ox, int32_t oz, int32_t floorY, int32_t highY, int32_t ceilingY)
{
	host::AutoPlotFloor floor;

	floor.cellEdge = 300;

	for (size_t k = 0; k < count; k++)
	{
		int const iz = top - (int)k;

		for (size_t ix = 0; rows[k][ix] != '\0'; ix++)
		{
			char const c = rows[k][ix];

			if (c != '#' && c != 'h')
			{
				continue;
			}

			int32_t const x = ox + 300 * (int32_t)ix, z = oz + 300 * iz;

			floor.cells.push_back(std::vector<int32_t>{ x, c == '#' ? floorY : highY, z });
			floor.ceilings.push_back(std::vector<int32_t>{ x, ceilingY, z });
		}
	}

	return floor;
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

int main()
{
	// ---- (a) THE WALL FOOT: SHED04 ----
	puts("SHED04 (Genshd1 AI module 225), a tube corridor with a one-row strip at its north wall's foot, through the volume planner:");
	{
		// the planner's map, rows iz 0 (z 3150, north) down to -22, the origin cell (ix 0) at x 16350 (the plot's own "volume-map" lines)
		static char const *const rows[] = {
			"############.........#######......###########################",   //   0: the strip at the wall's foot, one row
			"############........#########..#..###########################",   //  -1: the corridor's north row (its rectangle takes it: a notch)
			"#############################################################",   //  -2
			"#############################################################",
			"#############################################################",
			"#############################################################",
			"#############################################################",
			"#############################################################",
			"#############################################################",
			"#############################################################",   //  -9: the corridor's south row
			"....#############################............................",   // -10: the side room
			"....#############################............................",
			"....##.......####################............................",
			"....##..hhhh.####################............................",
			"....##..hhhh.####################............................",
			"....##..hhhh.####################............................",
			"....##..hhhh.###########################.....................",
			"........hhhh.###########################.....................",
			"........hhhh.###########################.....................",
			"....hhhhhhhh.###########################.....................",
			"....h...hhhh.###########################.....................",
			"...........h.###################.............................",
			"...........h.................................................",   // -22
		};

		host::AutoPlotFloor floor = FromMap(rows, sizeof(rows) / sizeof(rows[0]), 0, 16350, 3150, 1948, 1819, -1948);

		floor.boxFound = true;   // lanes.exe exits: the AI module's box 16081 -1982 -3896 .. 34394 1982 3946
		floor.boxLo[0] = 16081, floor.boxLo[1] = -1982, floor.boxLo[2] = -3896;
		floor.boxHi[0] = 34394, floor.boxHi[1] = 1982, floor.boxHi[2] = 3946;

		// its exits (to SHED02 at the east end, to SHDCL03 at the west end) and each doorway's middle half way to the neighbour's exit back (SHED02's 34294, SHDCL03's 16181)
		std::vector<std::vector<int32_t> > const exits = { { 34494, 1937, 1947 }, { 15981, 1938, 1948 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].soffitFound = true, doors[0].soffit[0] = 34494, doors[0].soffit[1] = -1948, doors[0].soffit[2] = 1947;
		doors[0].throughFound = true, doors[0].middle[0] = 34394, doors[0].middle[1] = 1937, doors[0].middle[2] = 1947, doors[0].through[0] = 1.0;
		doors[1].soffitFound = true, doors[1].soffit[0] = 15981, doors[1].soffit[1] = -1948, doors[1].soffit[2] = 1948;
		doors[1].throughFound = true, doors[1].middle[0] = 16081, doors[1].middle[1] = 1938, doors[1].middle[2] = 1948, doors[1].through[0] = -1.0;

		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		printf("  (%s, %zu boxes: %s)\n", plan.volumeKind.c_str(), plan.list.size(), Notes(plan).c_str());

		Check(plan.volumes && plan.volumeKind.find("floor only") == std::string::npos, "shaped as volumes, not floor only (" + plan.volumeKind + ")");

		// a climb up the north wall: a climb-in (or a sloped side's LOW) whose box reaches over the strip at the wall's foot (z 3000 .. 3300) to the wall
		bool climb = false, overFoot = false;

		for (size_t b = 0; b < plan.list.size(); b++)
		{
			std::string const r = Role(plan, b);

			if (r == "climb" || r == "LOW")
			{
				climb = true;
				overFoot = overFoot || plan.list[b].max[2] >= 3300;
			}
		}

		Check(climb, "a climb-in");
		Check(overFoot, "the climb-in reaches over the strip at the wall's foot (to z 3300 and past it) to the wall");

		int32_t const entry0 = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, entry1 = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

		Check(entry0 >= 0 && entry1 >= 0 && Role(plan, (size_t)entry0) == "entry" && Role(plan, (size_t)entry1) == "entry", "an entry at each doorway (" +
			std::to_string((long long)entry0) + ", " + std::to_string((long long)entry1) + "): SHED02's and SHDCL03's ceiling lanes go on into it");
		Check(ReachesOnCeiling(plan, entry0, entry1) && ReachesOnCeiling(plan, entry1, entry0), "the ceiling goes from one doorway's entry to the other's, on climbing boxes only");
		Check(plan.exitsCovered == 2, "both exit points held (" + std::to_string((long long)plan.exitsCovered) + ")");
		Check(plan.parts == 1 && AllReach(plan.list), "one network (" + std::to_string((long long)plan.parts) + " parts)");
		Check(plan.centresShared == 0, "every centre in its own box only (" + std::to_string((long long)plan.centresShared) + " shared)");
	}

	// ---- (b) THE WALL PART: new02 ----
	// new02 (Genshd1 AI module 398; Hudson: 469 stalled frames): its main floor (cells 12..29 x -22..-5, 5.4 m square) has a doorway on its south and its east side, a second
	// floor's seam on its west side, and on its north side a wall for 3.3 m (cells 12..22: nothing past it) and an arm of floor going on for the rest (cells 23..29, five
	// rows north): no whole side to climb, so a corridor, floor only. Bishop's spec (b): a side that is wall for at least 2.4 m gets its climb on that part only.
	// The planner's map (origin x -79050 z 96750), '#' y 1948, 'h' a raised object's top (the planner's grid has one there; any height over 100 above the floor reads
	// the same: a fitting); the ceiling y -1363 over z 93450 and south (a lower band, lanes.exe), y -1948 north of it
	puts("new02 (Genshd1 AI module 398), a room whose north side is wall for 3.3 m only, through the volume planner:");
	{
		static char const *const rows[] = {
			"#######................#######...",   //   0
			"#######................#######...",
			"#######................#######...",
			"#######................#######...",
			"#######................#######...",   //  -4: the north arms (cells 0..6 and 23..29); the wall between them over cells 7..22
			"##############################...",   //  -5: the main floor's north row
			"##############################...",
			"##############################...",
			"##############################...",
			"##############################...",
			"##############################...",
			"##############################...",
			"##############################...",   // -12
			"#####.......#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",
			"#####.hhhhh.#####################",   // -22: the main floor's south row
			"#####..hhhh.########.............",
			".......hhhh......................",   // -24
		};

		host::AutoPlotFloor floor = FromMap(rows, sizeof(rows) / sizeof(rows[0]), 0, -79050, 96750, 1948, 1148, -1948);

		for (std::vector<int32_t> &c : floor.ceilings)
		{
			c[1] = c[2] <= 93450 ? -1363 : -1948;   // the lower band over the south part (lanes.exe: z 89850 .. 93450 at every column read)
		}

		floor.boxFound = true;   // lanes.exe exits: the AI module's box -79778 -1948 89624 .. -69257 1948 97417
		floor.boxLo[0] = -79778, floor.boxLo[1] = -1948, floor.boxLo[2] = 89624;
		floor.boxHi[0] = -69257, floor.boxHi[1] = 1948, floor.boxHi[2] = 97417;

		// its exits (to office01 south, to new01 east) and each doorway's middle half way to the neighbour's exit back (office01's at z 89724, new01's at x -69358); the
		// soffits the ceilings over the exit points (office01's -1948, new01's -1363)
		std::vector<std::vector<int32_t> > const exits = { { -74712, 1938, 89524 }, { -69157, 1938, 91572 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].soffitFound = true, doors[0].soffit[0] = -74712, doors[0].soffit[1] = -1948, doors[0].soffit[2] = 89524;
		doors[0].throughFound = true, doors[0].middle[0] = -74712, doors[0].middle[1] = 1938, doors[0].middle[2] = 89624, doors[0].through[1] = -1.0;
		doors[1].soffitFound = true, doors[1].soffit[0] = -69157, doors[1].soffit[1] = -1363, doors[1].soffit[2] = 91572;
		doors[1].throughFound = true, doors[1].middle[0] = -69258, doors[1].middle[1] = 1938, doors[1].middle[2] = 91572, doors[1].through[0] = 1.0;

		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);

		printf("  (%s, %zu boxes: %s)\n", plan.volumeKind.c_str(), plan.list.size(), Notes(plan).c_str());

		Check(plan.volumes && plan.volumeKind.find("floor only") == std::string::npos, "shaped as volumes, not floor only (" + plan.volumeKind + ")");

		// the climb on the north wall's part: a climb-in over the wall face (z 95400) whose span along it lies on cells 12..22 (x -75600 .. -72300), not over the arm
		int32_t climbBox = -1;

		for (size_t b = 0; b < plan.list.size() && climbBox < 0; b++)
		{
			std::string const r = Role(plan, b);

			climbBox = (r == "climb" || r == "LOW") ? (int32_t)b : -1;
		}

		Check(climbBox >= 0, "a climb-in");

		if (climbBox >= 0)
		{
			host::WaypointRec const &c = plan.list[(size_t)climbBox];

			Check(c.min[2] <= 95400 && c.max[2] >= 95400, "the climb-in at the north wall's face (z " + std::to_string((long long)c.min[2]) + " .. " + std::to_string((long long)c.max[2]) + ")");
			Check(c.min[0] >= -75600 && c.max[0] <= -72300, "the climb-in on the wall's part (x " + std::to_string((long long)c.min[0]) + " .. " + std::to_string((long long)c.max[0]) +
				" within -75600 .. -72300), not over the arm going on past the rest");

			// ITS CENTRE BEHIND THE WALL (lanes up on fe3ea91a's whole plot: 74 stall places, the aliens under the climb's centre in open air): the grid's cells stop short of
			// the wall, the floor goes on to z 95850 and there is none at 96150 (lanes.exe probe at x -73950 on the stock copy). A floor alien heading for a point past the
			// wall's face walks into it and climbs; for one in front of it, straight over its head, it stops
			Check(c.centre[2] >= 96000, "the climb-in's centre past the wall's face (z " + std::to_string((long long)c.centre[2]) + ", the floor going on to 95850, none at 96150)");
		}

		int32_t const entry0 = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, entry1 = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

		Check(entry0 >= 0 && entry1 >= 0 && Role(plan, (size_t)entry0) == "entry" && Role(plan, (size_t)entry1) == "entry", "an entry at each doorway (" +
			std::to_string((long long)entry0) + ", " + std::to_string((long long)entry1) + ")");
		Check(ReachesOnCeiling(plan, entry0, entry1) && ReachesOnCeiling(plan, entry1, entry0), "the ceiling goes from one doorway's entry to the other's, on climbing boxes only");
		Check(plan.exitsCovered == 2, "both exit points held (" + std::to_string((long long)plan.exitsCovered) + ")");
		Check(plan.parts == 1 && AllReach(plan.list), "one network (" + std::to_string((long long)plan.parts) + " parts)");
		Check(plan.centresShared == 0, "every centre in its own box only (" + std::to_string((long long)plan.centresShared) + " shared)");
	}

	// ---- A WALL PART NEVER TAKES A WHOLE SIDE'S CLIMB: med02 ----
	// med02 (Genshd1 AI module 186): six floors, one climb-in (the owner's count). Its biggest floor (floor 0, cells 0..22 x -35..0) has no whole side, the others' climb
	// went to floor 5's whole north side, and it was written. fe3ea91a gave floor 0 a wall part on its south side, and the climb went to the biggest floor with a side:
	// floor 0's part, its centre inside the entry over that floor, so "box 6: its centre is in box 8 too" and med02 was refused (the whole-map plot of fe3ea91a). A side
	// from the wall foot or a wall part is for a floor that has none other: when the climbs are handed out, every floor with a whole side comes first.
	// The planner's map (origin x -61350 z 62250: the string's first character is cell -15), '#' y -5844 under a ceiling at -11690, 'h' a raised floor at -6624 (lanes.exe),
	// under a ceiling at -8962 on its west side (cells -14 .. -1, lanes.exe), -11690 elsewhere
	puts("med02 (Genshd1 AI module 186), six floors and one climb, through the volume planner:");
	{
		static char const *const rows[] = {
			"...............#######################................##########",   // 0
			"...............#######################................##########",   // -1
			".###############################################################",   // -2
			".###############################################################",   // -3
			".###############################################################",   // -4
			".###############################################################",   // -5
			".###############################################################",   // -6
			".###############################################################",   // -7
			".###############################################################",   // -8
			".###############################################################",   // -9
			".###############################################################",   // -10
			".###############################################################",   // -11
			"...............#######################................##########",   // -12
			"hhhhhhhhhhhhhh.#######################.hh...........h.##########",   // -13
			"hhhhhhhhhhhhhh.#######################.h............h.##########",   // -14
			"hhhhhhhhhhhhhh.#######################.h..............##########",   // -15
			"hhhhhhhhhhhhhh.#######################......hhhhh.....##########",   // -16
			"hhhhhhhhhhhhhh.#######################.....hhhhhh.....##########",   // -17
			"hhhhhhhhhhhhhh.#######################.....hhhhhhh....##########",   // -18
			"hhhhhhhhhhhhhh.#######################....hhhhhhhh....##########",   // -19
			"hhhhhhhhhhhhhh.#######################.....hhhhhhh....##########",   // -20
			"hhhhhhhhhhhhhh.#######################.....hhhhhh.....##########",   // -21
			"hhhhhhhhhhhhhh.#######################......hhhhh.....##########",   // -22
			"hhhhhhhhhhhhhh.#######################.h.hh.hhhh......##########",   // -23
			"hhhhhhhhhhhhhh.#######################.h............h.##########",   // -24
			"hhhhhhhhhhhhhh.#######################.hh...........h.##########",   // -25
			"hhhhhhhhhhhhhh.#######################.hh..........hh.##########",   // -26
			"...............#######################................##########",   // -27
			".###############################################################",   // -28
			".###############################################################",   // -29
			".###############################################################",   // -30
			".###############################################################",   // -31
			".###############################################################",   // -32
			".###############################################################",   // -33
			".###############################################################",   // -34
			".###############################################################",   // -35
			".#################..........####################################",   // -36
			".#################.hhhhhhhh.####################################",   // -37
		};

		host::AutoPlotFloor floor = FromMap(rows, sizeof(rows) / sizeof(rows[0]), 0, -61350 - 15 * 300, 62250, -5844, -6624, -11690);

		for (size_t k = 0; k < floor.cells.size(); k++)
		{
			if (floor.cells[k][1] == -6624 && floor.cells[k][0] < -61350)
			{
				floor.ceilings[k][1] = -8962;   // the raised floor's lower ceiling on the west side
			}
		}

		floor.boxFound = true;   // lanes.exe exits: the AI module's box -65749 -11690 50657 .. -46266 -5844 62347
		floor.boxLo[0] = -65749, floor.boxLo[1] = -11690, floor.boxLo[2] = 50657;
		floor.boxHi[0] = -46266, floor.boxHi[1] = -5844, floor.boxHi[2] = 62347;

		// its exits (to med09 north, to newhug03 west) and each doorway's middle half way to the neighbour's exit back (med09's at z 62247, newhug03's at x -65649)
		std::vector<std::vector<int32_t> > const exits = { { -56007, -5854, 62447 }, { -65849, -5854, 56502 } };
		std::vector<host::AutoPlotDoor> doors(2);

		doors[0].soffitFound = true, doors[0].soffit[0] = -56007, doors[0].soffit[1] = -11690, doors[0].soffit[2] = 62447;
		doors[0].throughFound = true, doors[0].middle[0] = -56007, doors[0].middle[1] = -5854, doors[0].middle[2] = 62347, doors[0].through[1] = 1.0;
		doors[1].soffitFound = true, doors[1].soffit[0] = -65849, doors[1].soffit[1] = -8962, doors[1].soffit[2] = 56502;
		doors[1].throughFound = true, doors[1].middle[0] = -65749, doors[1].middle[1] = -6244, doors[1].middle[2] = 56502, doors[1].through[0] = -1.0;

		host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
		std::string const notes = Notes(plan);

		printf("  (%s, %zu boxes: %s)\n", plan.volumeKind.c_str(), plan.list.size(), notes.c_str());

		int32_t climbBox = -1;

		for (size_t b = 0; b < plan.list.size() && climbBox < 0; b++)
		{
			std::string const r = Role(plan, b);

			climbBox = (r == "climb" || r == "LOW") ? (int32_t)b : -1;
		}

		Check(plan.volumes && climbBox >= 0, "shaped as volumes, one climb-in (" + plan.volumeKind + ")");

		if (climbBox >= 0)
		{
			host::WaypointRec const &c = plan.list[(size_t)climbBox];

			Check(c.min[0] >= -65800 && c.max[0] <= -61400, "the climb is floor 5's whole side's, as before (x " + std::to_string((long long)c.min[0]) + " .. " +
				std::to_string((long long)c.max[0]) + " within floor 5's -65700 .. -61499), not a wall part on the biggest floor");
		}

		Check(notes.find("wall part") == std::string::npos, "no wall part taken (a floor with a whole side has the climb)");
		Check(plan.centresShared == 0, "every centre in its own box only (" + std::to_string((long long)plan.centresShared) + " shared)");
		Check(plan.exitsCovered == 2 && plan.parts == 1 && AllReach(plan.list), "both exits held, one network (" + std::to_string((long long)plan.parts) + " parts)");
	}

	printf("host_autoplot_floorside_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
