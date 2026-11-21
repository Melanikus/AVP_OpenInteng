// host_autoplot_stairs_test.cpp: THE STAIR BETWEEN TWO FLOORS (autoplot_volumes.h). Genshd1's opscen03 (AI module 153) and opscen02 (154), the owner's own ceiling-lane
// POC (chain A, opscen03 - Ops01 - opscen02), were refused by the auto-plot: "box 0 cannot reach box 1". Each is a room on two floors 1.95 m apart, an upper floor at one
// doorway and a lower floor at the other, joined by four treads. The volume planner made a floor box for each floor and none for the treads (each tread is a flat patch too
// small or too narrow to be a floor of its own), so the plan was two networks that never meet, one at each doorway, and none of it was written.
// Drake's hand-built chain (hand4, make-spec4.js; the owner's in-game PASS, 63 boxes) joins the two halves on the floor with boxes over the treads, linked to both floors, and
// on the ceiling with one box over the treads' ceiling, linked to the upper and the lower ceiling, so an alien walks or hangs from one doorway to the other.
// The cells below are the grid's own for opscen03 (lanes.exe cells on a copy of the stock Genshd1.RIF, sha8 19f00eae, 11-21): 300 mm cells, 20 across (x -63450 .. -57750),
// one more at x -57450 on the lower floor near its doorway; the floor and the ceiling over each cell as the grid gives them (at a tread's edge the grid gives two cells,
// the higher tread's and the lower one's, the lower one's "ceiling" being the tread over it).
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "autoplot_plan.h"
#include "autoplot_stairs.h"
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

// whether box `to` is reached from box `from` along links that stay on climbing boxes (climbing true: the ceiling's way) or on the others (false: the floor's way)
static bool ReachesOn(host::AutoPlotPlan const &plan, int32_t from, int32_t to, bool climbing)
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

			if (b < plan.list.size() && !seen[b] && (l.flags & host::kLinkOneWayBack) == 0 && (bool)plan.climbing[b] == climbing)
			{
				seen[b] = true;
				todo.push_back((int32_t)b);
			}
		}
	}

	return seen[(size_t)to];
}

// the floor box (not a climbing one) holding a point, -1 when none does
static int32_t FloorBoxAt(host::AutoPlotPlan const &plan, int32_t const p[3])
{
	for (size_t i = 0; i < plan.list.size(); i++)
	{
		if (!plan.climbing[i] && Holds(plan.list[i], p))
		{
			return (int32_t)i;
		}
	}

	return -1;
}

static void Cell(host::AutoPlotFloor *floor, int32_t x, int32_t y, int32_t z, int32_t roof)
{
	floor->cells.push_back(std::vector<int32_t>{ x, y, z });
	floor->ceilings.push_back(std::vector<int32_t>{ x, roof, z });
}

// opscen03's floor as the grid gives it, every column alike: the lower floor first (the grid's first cell, its origin, is the lower floor's far corner, as on the plot's
// own cell map), then the treads, then the upper floor
static host::AutoPlotFloor Opscen03()
{
	struct Row { int32_t y, z, roof; };
	std::vector<Row> rows;

	for (int32_t z = 96750; z >= 89250; z -= 300)
	{
		rows.push_back({ -13638, z, -17535 });
	}

	Row const treads[] = {
		{ -13638, 88950, -17535 }, { -14028, 88950, -17535 }, { -14028, 88650, -17632 }, { -14028, 88350, -17782 }, { -14028, 88050, -14417 }, { -14417, 88050, -17932 },
		{ -14417, 87750, -18082 }, { -14417, 87450, -18232 }, { -14417, 87150, -14807 }, { -14807, 87150, -18382 }, { -14807, 86850, -18532 }, { -14807, 86550, -18682 },
		{ -15197, 86550, -18682 }, { -15197, 86250, -18832 }, { -15197, 85950, -18982 }, { -15197, 85650, -15586 }, { -15585, 85651, -19131 }, { -15585, 85351, -19281 },
		{ -15585, 85051, -19431 },
	};

	// (the grid's two cells at a tread's edge: the lower tread's "ceiling" there is the tread over it, as the grid gives it)
	for (Row const &t : treads)
	{
		rows.push_back(t);
	}

	for (int32_t z = 84751; z >= 81151; z -= 300)
	{
		rows.push_back({ -15585, z, -19484 });
	}

	host::AutoPlotFloor floor;

	floor.cellEdge = 300;

	for (Row const &r : rows)
	{
		for (int32_t x = -63450; x <= -57750; x += 300)
		{
			Cell(&floor, x, r.y, r.z, r.roof);
		}

		if (r.y == -13638 && r.z >= 90150)
		{
			Cell(&floor, -57450, r.y, r.z, r.roof);   // the lower floor's one column more, by its doorway
		}
	}

	floor.boxFound = true;   // the AI module's own box (hand4's measures: x -64231 .. -57136, z 81050 .. 97416), from the upper ceiling to the lower floor
	floor.boxLo[0] = -64231, floor.boxLo[1] = -19684, floor.boxLo[2] = 81050;
	floor.boxHi[0] = -57136, floor.boxHi[1] = -13638, floor.boxHi[2] = 97416;
	return floor;
}

int main()
{
	puts("opscen03 (Genshd1 AI module 153), two floors and the stair between them, through the volume planner:");

	host::AutoPlotFloor const floor = Opscen03();

	// its exits as the auto-plot takes them (ADJMDLEP, levels.exe on the stock file): to Ops02 at the upper floor's end, to Ops01 at the lower floor's side
	std::vector<std::vector<int32_t> > const exits = { { -60684, -15596, 80950 }, { -57076, -13648, 93520 } };
	std::vector<host::AutoPlotDoor> doors(2);

	doors[0].soffitFound = true, doors[0].soffit[0] = -60684, doors[0].soffit[1] = -19483, doors[0].soffit[2] = 80950;
	doors[0].throughFound = true, doors[0].middle[0] = -60684, doors[0].middle[1] = -15596, doors[0].middle[2] = 81050, doors[0].through[1] = -1.0;
	doors[1].soffitFound = true, doors[1].soffit[0] = -57076, doors[1].soffit[1] = -17535, doors[1].soffit[2] = 93520;
	doors[1].throughFound = true, doors[1].middle[0] = -57176, doors[1].middle[1] = -13648, doors[1].middle[2] = 93520, doors[1].through[0] = 1.0;

	host::AutoPlotPlan const plan = host::AutoPlot_PlanVolumes({}, exits, floor, doors, 1.0);
	std::string notes;

	for (std::string const &n : plan.notes)
	{
		notes += (notes.empty() ? "" : "; ") + n;
	}

	printf("  (%s, %zu boxes: %s)\n", plan.volumeKind.c_str(), plan.list.size(), notes.c_str());

	Check(plan.volumes && plan.list.size() >= 2, "shaped as volumes (" + plan.volumeKind + ", " + std::to_string((long long)plan.list.size()) + " boxes)");
	Check(plan.exitsCovered == 2, "both exit points held (" + std::to_string((long long)plan.exitsCovered) + ")");

	// THE REFUSAL: two networks, one at each doorway
	Check(plan.parts == 1 && AllReach(plan.list), "ONE network: every box reaches every other (" + std::to_string((long long)plan.parts) + " parts)");

	// THE STAIR ON THE FLOOR: an alien standing on each tread is in a floor box, and the floor walks from the upper floor over the treads to the lower one
	int32_t const upper[3] = { -60750, -15585 - 10, 83251 }, lower[3] = { -60750, -13638 - 10, 93250 };
	int32_t const upperBox = FloorBoxAt(plan, upper), lowerBox = FloorBoxAt(plan, lower);
	int32_t const tread[4][3] = { { -60750, -15197 - 10, 86250 }, { -60750, -14807 - 10, 86850 }, { -60750, -14417 - 10, 87450 }, { -60750, -14028 - 10, 88350 } };
	bool treadsHeld = true;
	std::string treadWords;

	for (int t = 0; t < 4; t++)
	{
		int32_t const b = FloorBoxAt(plan, tread[t]);

		treadsHeld = treadsHeld && b >= 0;
		treadWords += (t ? " " : "") + std::to_string((long long)b);
	}

	Check(upperBox >= 0 && lowerBox >= 0, "each floor in a floor box (upper " + std::to_string((long long)upperBox) + ", lower " + std::to_string((long long)lowerBox) + ")");
	Check(treadsHeld, "every tread in a floor box (boxes " + treadWords + ")");
	Check(ReachesOn(plan, upperBox, lowerBox, false) && ReachesOn(plan, lowerBox, upperBox, false), "the floor walks from the upper floor to the lower one and back, on floor boxes only");

	// A FLOOR BOX OVER THE TREADS KEEPS ITS TOP 1000 UNDER THE CEILING over it (D3 / D7: a hanging alien never touches a floor box): the lowest ceiling over the treads is
	// the lower floor's, -17535
	for (int t = 0; t < 4; t++)
	{
		int32_t const b = FloorBoxAt(plan, tread[t]);

		if (b >= 0 && b != upperBox && b != lowerBox)
		{
			Check(plan.list[(size_t)b].min[1] >= -17535 + 1000, "tread " + std::to_string((long long)t) + "'s floor box top " + std::to_string((long long)plan.list[(size_t)b].min[1]) +
				" at least 1000 under the lowest ceiling over the treads (-17535)");
		}
	}

	// THE STAIR ON THE CEILING: a hanging alien goes from one doorway's entry to the other's on climbing boxes only (the owner's ceiling lane through the room)
	int32_t const entry0 = plan.doorBox.size() == 2 ? plan.doorBox[0] : -1, entry1 = plan.doorBox.size() == 2 ? plan.doorBox[1] : -1;

	Check(entry0 >= 0 && entry1 >= 0, "an entry at each doorway (" + std::to_string((long long)entry0) + ", " + std::to_string((long long)entry1) + ")");
	Check(ReachesOn(plan, entry0, entry1, true) && ReachesOn(plan, entry1, entry0, true), "the ceiling goes from the upper doorway's entry to the lower doorway's and back, on climbing boxes only");

	Check(plan.centresShared == 0, "every centre in its own box only (" + std::to_string((long long)plan.centresShared) + " shared)");

	// THE FINDER BY ITSELF (autoplot_stairs.h): what is NOT a stair stays as it was
	puts("the stair finder, what is not a stair:");
	{
		std::map<std::pair<int, int>, double> cells, roofs;
		std::vector<host::AutoPlotStairFloor> floors;

		Check(host::AutoPlot_Stairs(cells, roofs, floors).empty(), "no cells, no floors: no stair");

		// a floor (ix 0..9, iz 0..9) at y 0, and a ledge 390 up beside it (ix 10..11): it meets one floor only
		for (int ix = 0; ix < 12; ix++)
		{
			for (int iz = 0; iz < 10; iz++)
			{
				cells[std::make_pair(ix, iz)] = ix < 10 ? 0.0 : -390.0;
				roofs[std::make_pair(ix, iz)] = -3900.0;
			}
		}

		host::AutoPlotStairFloor f;

		f.ix0 = 0, f.ix1 = 9, f.iz0 = 0, f.iz1 = 9, f.floorTop = 0.0;
		floors.push_back(f);
		Check(host::AutoPlot_Stairs(cells, roofs, floors).empty(), "a ledge beside one floor: no stair");

		// a second floor beyond the ledge (ix 12..21) at the first floor's height: the ledge meets two floors of one height (a sill between them)
		for (int ix = 12; ix < 22; ix++)
		{
			for (int iz = 0; iz < 10; iz++)
			{
				cells[std::make_pair(ix, iz)] = 0.0;
				roofs[std::make_pair(ix, iz)] = -3900.0;
			}
		}

		f.ix0 = 12, f.ix1 = 21, f.floorTop = 0.0;
		floors.push_back(f);
		Check(host::AutoPlot_Stairs(cells, roofs, floors).empty(), "a sill between two floors of one height: no stair");

		// the second floor 780 up (two treads of 390: the ledge, then the floor): a stair between floors 0 and 1, its ceiling the rooms' (3900 over), and a "ceiling" one
		// tread over a cell (a tread's edge, as the grid gives it) not counted
		for (int ix = 12; ix < 22; ix++)
		{
			for (int iz = 0; iz < 10; iz++)
			{
				cells[std::make_pair(ix, iz)] = -780.0;
				roofs[std::make_pair(ix, iz)] = -4680.0;
			}
		}

		roofs[std::make_pair(11, 0)] = -780.0;   // a tread's edge over the ledge's cell (390 over it)
		floors[1].floorTop = -780.0;

		std::vector<host::AutoPlotStair> const s = host::AutoPlot_Stairs(cells, roofs, floors);

		Check(s.size() == 1 && s[0].joins.size() == 2 && s[0].joins[0] == 0 && s[0].joins[1] == 1, "two floors 780 apart joined by a ledge 390 between: one stair between floors 0 and 1");
		Check(s.size() == 1 && s[0].ix0 == 10 && s[0].ix1 == 11 && s[0].iz0 == 0 && s[0].iz1 == 9 && s[0].floorTop == -390.0 && s[0].floorLow == -390.0, "its cells: the ledge's, at 390 up");
		Check(s.size() == 1 && s[0].ceiled && s[0].ceilTop == -3900.0 && s[0].ceilLow == -3900.0, "its ceiling the room's, the tread's edge 390 over one cell not counted");

		// a step no alien walks (the second floor 900 over the ledge): no stair
		for (int ix = 12; ix < 22; ix++)
		{
			for (int iz = 0; iz < 10; iz++)
			{
				cells[std::make_pair(ix, iz)] = -1290.0;
			}
		}

		floors[1].floorTop = -1290.0;
		Check(host::AutoPlot_Stairs(cells, roofs, floors).empty(), "a riser of 900 between the ledge and the upper floor (no alien walks it): no stair");
	}

	printf("host_autoplot_stairs_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
