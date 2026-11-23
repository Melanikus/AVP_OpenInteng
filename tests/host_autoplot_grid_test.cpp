// host_autoplot_grid_test.cpp: the auto-plot's pathfinder service (src/host/autoplot_grid.h) on a level made of polygons, no game: two rooms side by side, each a
// game module of its own, joined by an open doorway. The grid is AvPx's own (npc_grid), built on the test's thread; the searches are StartSearch + Work. An alien's
// way keeps off the floor (floor is lava), a marine's stays on it; every point says its module, through map polygon -> the listing's place -> module; the stand spots
// are a module's own, spread out; the refusals. The polygon helpers are avpx/tests/npc_grid_test.cpp's. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <chrono>

#include <string>
#include <vector>

#include "autoplot_grid.h"
#include "autoplot_grid_feed.h"

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

struct Corner
{
	int x, y, z;
};

// a polygon from its corners, turned round if need be to face the way given (avpx/tests/npc_grid_test.cpp's Facing)
static AlienWaysPolygon Facing(std::vector<Corner> corners, double nx, double ny, double nz)
{
	double s[3] = { 0.0, 0.0, 0.0 };
	int const n = static_cast<int>(corners.size());

	for (int i = 0; i < n; i++)
	{
		Corner const &a = corners[i], &b = corners[(i + 1) % n];
		s[0] += (static_cast<double>(a.y) - b.y) * (static_cast<double>(a.z) + b.z);
		s[1] += (static_cast<double>(a.z) - b.z) * (static_cast<double>(a.x) + b.x);
		s[2] += (static_cast<double>(a.x) - b.x) * (static_cast<double>(a.y) + b.y);
	}

	if (s[0] * nx + s[1] * ny + s[2] * nz < 0.0)
	{
		std::vector<Corner> turned(corners.rbegin(), corners.rend());
		corners = turned;
		s[0] = -s[0];
		s[1] = -s[1];
		s[2] = -s[2];
	}

	double const size = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	AlienWaysPolygon p;

	memset(&p, 0, sizeof(p));
	p.corners = n;

	for (int i = 0; i < n && i < kAlienWaysMostCorners; i++)
	{
		p.x[i] = corners[i].x;
		p.y[i] = corners[i].y;
		p.z[i] = corners[i].z;
	}

	p.up = size > 0.0 ? static_cast<float>(-s[1] / size) : 0.0f;

	for (int k = 0; k < 3; k++)
	{
		p.facing[k] = size > 0.0 ? static_cast<float>(s[k] / size) : 0.0f;
	}

	return p;
}

static AlienWaysPolygon Floor(int x0, int x1, int z0, int z1, int y) { return Facing({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, 0, -1, 0); }
static AlienWaysPolygon Ceiling(int x0, int x1, int z0, int z1, int y) { return Facing({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, 0, 1, 0); }
static AlienWaysPolygon WallAtX(int x, int z0, int z1, int top, int bottom, int facesX) { return Facing({ { x, bottom, z0 }, { x, bottom, z1 }, { x, top, z1 }, { x, top, z0 } }, facesX, 0, 0); }
static AlienWaysPolygon WallAtZ(int z, int x0, int x1, int top, int bottom, int facesZ) { return Facing({ { x0, bottom, z }, { x1, bottom, z }, { x1, top, z }, { x0, top, z } }, 0, 0, facesZ); }

// Two long halls, x 0..8000 (module 0) and 8000..16000 (module 1), z 0..20000, floor at y 0, ceiling 6 m up (y -6000), the wall between them open only near the far
// end, from z 16000 to 17500, up to 2.4 m: a way from one near end to the other runs 16 m along each room, so floor is lava shows (a floor metre costs 10). Every polygon
// of the left room, its side of the middle wall included, is module 0's, every one of the right room module 1's
static void TwoRooms(std::vector<AlienWaysPolygon> *polygons, std::vector<int32_t> *modules)
{
	for (int side = 0; side < 2; side++)
	{
		int const x0 = side == 0 ? 0 : 8000, x1 = side == 0 ? 8000 : 16000, outer = side == 0 ? 0 : 16000, facesOuter = side == 0 ? 1 : -1, facesMiddle = side == 0 ? -1 : 1;
		size_t const before = polygons->size();

		polygons->push_back(Floor(x0, x1, 0, 20000, 0));
		polygons->push_back(Ceiling(x0, x1, 0, 20000, -6000));
		polygons->push_back(WallAtX(outer, 0, 20000, -6000, 0, facesOuter));
		polygons->push_back(WallAtZ(0, x0, x1, -6000, 0, 1));
		polygons->push_back(WallAtZ(20000, x0, x1, -6000, 0, -1));
		polygons->push_back(WallAtX(8000, 0, 16000, -6000, 0, facesMiddle));
		polygons->push_back(WallAtX(8000, 17500, 20000, -6000, 0, facesMiddle));
		polygons->push_back(WallAtX(8000, 16000, 17500, -6000, -2400, facesMiddle));
		modules->resize(polygons->size(), -1);

		for (size_t i = before; i < polygons->size(); i++)
		{
			(*modules)[i] = side;
		}
	}
}

// THE CLIMB STATION'S LEVELS (Drake 11-19). A CORRIDOR (Genshd1's chain A in small): two modules along x, 0..12000 (module 0) and 12000..24000 (module 1), 4 m wide (z 0..4000),
// floor y 0, ceiling 4 m up (y -4000), the doorway between them open the whole width and height; its side walls face across the corridor (z), its end walls along it (x).
static void Corridor(std::vector<AlienWaysPolygon> *polygons, std::vector<int32_t> *modules)
{
	for (int side = 0; side < 2; side++)
	{
		int const x0 = side == 0 ? 0 : 12000, x1 = side == 0 ? 12000 : 24000;
		size_t const before = polygons->size();

		polygons->push_back(Floor(x0, x1, 0, 4000, 0));
		polygons->push_back(Ceiling(x0, x1, 0, 4000, -4000));
		polygons->push_back(WallAtZ(0, x0, x1, -4000, 0, 1));
		polygons->push_back(WallAtZ(4000, x0, x1, -4000, 0, -1));
		polygons->push_back(side == 0 ? WallAtX(0, 0, 4000, -4000, 0, 1) : WallAtX(24000, 0, 4000, -4000, 0, -1));
		modules->resize(polygons->size(), -1);

		for (size_t i = before; i < polygons->size(); i++)
		{
			(*modules)[i] = side;
		}
	}
}

// A CORRIDOR INTO A HALL through a door: module 0, x 0..12000, z 1000..3000 (2 m wide); module 1, x 12000..24000, z -4000..8000 (12 m wide); both 4 m tall. Module 1's
// wall at x 12000 (facing +x, into the hall) has the door, z 1000..3000, the whole height: the walls nearest the door's landing spot are that door wall's, beside it.
static void DoorIntoHall(std::vector<AlienWaysPolygon> *polygons, std::vector<int32_t> *modules)
{
	polygons->push_back(Floor(0, 12000, 1000, 3000, 0));
	polygons->push_back(Ceiling(0, 12000, 1000, 3000, -4000));
	polygons->push_back(WallAtZ(1000, 0, 12000, -4000, 0, 1));
	polygons->push_back(WallAtZ(3000, 0, 12000, -4000, 0, -1));
	polygons->push_back(WallAtX(0, 1000, 3000, -4000, 0, 1));
	modules->resize(polygons->size(), 0);
	polygons->push_back(Floor(12000, 24000, -4000, 8000, 0));
	polygons->push_back(Ceiling(12000, 24000, -4000, 8000, -4000));
	polygons->push_back(WallAtZ(-4000, 12000, 24000, -4000, 0, 1));
	polygons->push_back(WallAtZ(8000, 12000, 24000, -4000, 0, -1));
	polygons->push_back(WallAtX(24000, -4000, 8000, -4000, 0, -1));
	polygons->push_back(WallAtX(12000, -4000, 1000, -4000, 0, 1));
	polygons->push_back(WallAtX(12000, 3000, 8000, -4000, 0, 1));
	modules->resize(polygons->size(), 1);
}

// The checks of one station way: it starts on the floor near the exit point, goes floor -> one wall -> ceiling (no floor once it is on the wall), and its top is the
// last point, on the ceiling, in the landing module, on the landing side
static void CheckStation(bool found, std::vector<host::AutoPlotPoint> const &way, int32_t const top[3], int32_t const exit[3], std::string const &why, std::string const &what,
	int32_t landing, int sideAxis, int32_t const sideAt[2])
{
	using host::AutoPlotPoint;

	Check(found && way.size() >= 4, what + ": a station is found (" + std::to_string(way.size()) + " points" + (found ? "" : ", " + why) + ")");

	if (!found || way.size() < 4)
	{
		return;
	}

	double const startOff = sqrt(pow((double)way.front().place[0] - exit[0], 2.0) + pow((double)way.front().place[2] - exit[2], 2.0));
	size_t firstWall = way.size(), floorAfter = 0, walls = 0;

	for (size_t i = 0; i < way.size(); i++)
	{
		if (way[i].surface == AutoPlotPoint::Surface::Wall)
		{
			firstWall = i < firstWall ? i : firstWall;
			walls++;
		}
		else if (way[i].surface == AutoPlotPoint::Surface::Floor && i > firstWall)
		{
			floorAfter++;
		}
	}

	Check(way.front().surface == AutoPlotPoint::Surface::Floor && startOff <= 450.0, what + ": it starts on the floor at the exit point (" + std::to_string((int)startOff) + " mm off)");
	Check(walls >= 1 && floorAfter == 0 && way.back().surface == AutoPlotPoint::Surface::Ceiling, what + ": floor, then a wall, then the ceiling, with no floor after the wall");

	bool sameWall = true;

	for (AutoPlotPoint const &p : way)
	{
		if (p.surface == AutoPlotPoint::Surface::Wall)
		{
			int32_t const at = p.place[sideAxis];

			sameWall = sameWall && (abs(at - sideAt[0]) <= 300 || abs(at - sideAt[1]) <= 300);
		}
	}

	Check(sameWall, what + ": the wall it climbs is a side wall (" + std::string(sideAxis == 2 ? "z" : "x") + " " + std::to_string(sideAt[0]) + " or " + std::to_string(sideAt[1]) + ")");

	// it climbs STRAIGHT up: the top is over the wall's foot (no drift along the wall), and the foot is the wall's nearest to the exit point across the way through
	size_t const foot = firstWall < way.size() ? firstWall : 0;
	double const drift = sqrt(pow((double)way.back().place[0] - way[foot].place[0], 2.0) + pow((double)way.back().place[2] - way[foot].place[2], 2.0));
	int const along = sideAxis == 2 ? 0 : 2;

	Check(drift <= 600.0, what + ": straight up, the top over the wall's foot (" + std::to_string((int)drift) + " mm apart)");
	Check(abs(way[foot].place[along] - exit[along]) <= 600, what + ": the foot is the wall's nearest to the exit point (" + std::to_string(way[foot].place[along]) + " against " +
		std::to_string(exit[along]) + ")");
	Check(top[0] == way.back().place[0] && top[1] == way.back().place[1] && top[2] == way.back().place[2] && abs(top[1] + 4000) <= 300,
		what + ": top is the last point, on the ceiling (y " + std::to_string(top[1]) + ")");
	Check(top[0] >= 12000 && way.back().module == landing, what + ": on the landing side, in the landing module (x " + std::to_string(top[0]) + ", module " +
		std::to_string(way.back().module) + ")");

	bool facingsLong = true;

	for (AutoPlotPoint const &p : way)
	{
		double const l = sqrt((double)p.facing[0] * p.facing[0] + (double)p.facing[1] * p.facing[1] + (double)p.facing[2] * p.facing[2]);

		facingsLong = facingsLong && fabs(l - 65536.0) < 200.0;
	}

	Check(facingsLong, what + ": every point's facing is known, 65536 long");
}

int main()
{
	using host::AutoPlotPoint;

	printf("THE PURE PARTS\n");
	{
		std::vector<host::AutoPlotRange> const ranges = { { 0, 5 }, { 5, 5 }, { 5, 12 } };

		Check(host::AutoPlot_ModuleOfListed(ranges, 0) == 0 && host::AutoPlot_ModuleOfListed(ranges, 4) == 0 && host::AutoPlot_ModuleOfListed(ranges, 5) == 2 &&
			host::AutoPlot_ModuleOfListed(ranges, 11) == 2, "a listing's place is in its module's range (an empty range holds none)");
		Check(host::AutoPlot_ModuleOfListed(ranges, 12) == -1 && host::AutoPlot_ModuleOfListed(std::vector<host::AutoPlotRange>(), 0) == -1, "past every range: no module");

		std::vector<AutoPlotPoint> places(5);
		int32_t const xs[5] = { 0, 100, 10000, 5000, 9900 };

		for (int i = 0; i < 5; i++)
		{
			places[i].place[0] = xs[i];
		}

		std::vector<size_t> const three = host::AutoPlot_Spread(places, 3);

		Check(three.size() == 3 && three[0] == 0 && three[1] == 2 && three[2] == 3, "spread: the first, then the farthest (10000), then the farthest from both (5000)");
		Check(host::AutoPlot_Spread(places, 0).empty() && host::AutoPlot_Spread(std::vector<AutoPlotPoint>(), 4).empty(), "none asked, or none to pick from: none");
		Check(host::AutoPlot_Spread(places, 1000).size() == 5, "asked for more than there are: every place once");

		std::vector<AutoPlotPoint> same(4);

		Check(host::AutoPlot_Spread(same, 3).size() == 1, "places all at one spot: one");

		int16_t const along[3] = { 32767, 0, 0 }, diagonal[3] = { 23170, 23170, 0 }, none[3] = { 0, 0, 0 };
		int32_t out[3];

		host::AutoPlot_Facing65536(along, out);
		Check(out[0] == 65536 && out[1] == 0 && out[2] == 0, "a facing of 32767ths made 65536 long");
		host::AutoPlot_Facing65536(diagonal, out);
		Check(abs(out[0] - 46341) <= 1 && abs(out[1] - 46341) <= 1 && out[2] == 0, "a diagonal one too");
		host::AutoPlot_Facing65536(none, out);
		Check(out[0] == 0 && out[1] == 0 && out[2] == 0, "a facing not known stays all 0");
	}

	printf("BEFORE A GRID\n");
	{
		std::string why;
		std::vector<AutoPlotPoint> way;
		int32_t const a[3] = { 1500, -500, 3000 }, b[3] = { 4500, -500, 3000 };

		Check(!host::AutoPlotGrid_Ready(&why) && why == "no grid: not started", "not started: not ready (" + why + ")");
		Check(!host::AutoPlotGrid_Way(a, b, true, &way, &why) && way.empty(), "no way without a grid (" + why + ")");

		size_t noGridLooks = 99;

		host::AutoPlotGrid_Way(a, b, true, &way, &why, &noGridLooks);
		Check(noGridLooks == 0, "THE PLOT'S WORK (11-23): a search with no grid looked at no cell (" + std::to_string((unsigned long long)noGridLooks) + ")");
		Check(host::AutoPlotGrid_StandSpots(0, 4).empty(), "no stand spots without a grid");

		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;

		TwoRooms(&polygons, &modules);
		modules.pop_back();
		Check(!host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why), "a module for every polygon but one: refused (" + why + ")");
		host::AutoPlotGrid_StartRefused("no grid: the level's polygons: a test's");
		Check(!host::AutoPlotGrid_Ready(&why) && why == "no grid: the level's polygons: a test's", "the listing's refusal is what Ready says");
	}

	printf("TWO ROOMS\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::string why;

		TwoRooms(&polygons, &modules);
		host::AutoPlotGrid_BuildOnTheCallersThreadForATest(true);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why), "the two rooms start building");
		Check(host::AutoPlotGrid_Ready(&why), "and are ready (" + why + ")");

		int32_t const left[3] = { 1500, -500, 1500 }, right[3] = { 14500, -500, 1500 };   // the near ends: the way goes 16 m along each room to the doorway
		std::vector<AutoPlotPoint> alien, marine;

		bool const alienFound = host::AutoPlotGrid_Way(left, right, true, &alien, &why);   // (found first: an argument's words are made in no set order)

		Check(alienFound && alien.size() >= 2, "an alien's way from the left room to the right (" + std::to_string(alien.size()) +
			" points" + (why.empty() ? "" : ", " + why) + ")");

		{
			std::vector<AutoPlotPoint> again;
			std::string againWhy;
			size_t looks = 0;
			bool const againFound = host::AutoPlotGrid_Way(left, right, true, &again, &againWhy, &looks);

			Check(againFound && again.size() == alien.size() && looks > 0 && looks <= host::kAutoPlotMostLooks, "THE PLOT'S WORK: the same search gives the same way, and "
				"says how many cells it looked at, 1 to kAutoPlotMostLooks (" + std::to_string((unsigned long long)looks) + ")");
		}

		if (alien.size() >= 2)
		{
			bool offFloor = false, knownFacingsLong = true;

			for (AutoPlotPoint const &p : alien)
			{
				offFloor = offFloor || p.surface != AutoPlotPoint::Surface::Floor;

				double const l = sqrt((double)p.facing[0] * p.facing[0] + (double)p.facing[1] * p.facing[1] + (double)p.facing[2] * p.facing[2]);

				knownFacingsLong = knownFacingsLong && (l == 0.0 || fabs(l - 65536.0) < 200.0);
			}

			Check(alien.front().module == 0 && alien.back().module == 1, "it starts in module 0 and ends in module 1 (" + std::to_string(alien.front().module) + " -> " +
				std::to_string(alien.back().module) + ")");
			Check(offFloor, "the floor is lava: some of it is on a wall or the ceiling");
			Check(knownFacingsLong, "every known facing is 65536 long");
		}

		bool const marineFound = host::AutoPlotGrid_Way(left, right, false, &marine, &why);

		Check(marineFound && marine.size() >= 2, "a marine's way too (" + std::to_string(marine.size()) + " points)");

		if (marine.size() >= 2)
		{
			bool onFloor = true;

			for (AutoPlotPoint const &p : marine)
			{
				onFloor = onFloor && p.surface == AutoPlotPoint::Surface::Floor;
			}

			Check(onFloor && marine.front().module == 0 && marine.back().module == 1, "all of it on the floor, module 0 to module 1");
		}

		int32_t const outside[3] = { 100000, -500, 100000 };
		std::vector<AutoPlotPoint> none;

		bool const outsideFound = host::AutoPlotGrid_Way(left, outside, true, &none, &why);

		Check(!outsideFound && none.empty(), "to a place 100 m out of the rooms: no way (" + why + ")");

		// a goal with no cell within the grid's reach (1.5 m) but one within 4 m (an exit point mid-doorway, 2 m and more off the floor): moved to that cell, and the way
		// ends on it. The middle of the left hall, 3 m off its floor and its ceiling and 4 m from its walls: nothing is within reach
		int32_t const beyond[3] = { 4000, -3000, 10000 };
		std::vector<AutoPlotPoint> snapped;
		bool const snappedFound = host::AutoPlotGrid_Way(left, beyond, true, &snapped, &why);

		double const movedBy = snapped.empty() ? 0.0 : sqrt(pow((double)snapped.back().place[0] - beyond[0], 2.0) + pow((double)snapped.back().place[1] - beyond[1], 2.0) +
			pow((double)snapped.back().place[2] - beyond[2], 2.0));

		Check(snappedFound && !snapped.empty() && movedBy > 1500.0 && movedBy <= 4000.0 && snapped.back().module == 0,
			"a goal in the middle of a hall, out of the grid's reach, is moved to the nearest cell within 4 m and the way ends there (" + (snapped.empty() ? "none: " + why :
			std::to_string((int)movedBy) + " mm") + ")");

		// every floor cell of a module, for covering its walkable floor with boxes
		std::vector<AutoPlotPoint> floorCells;
		int32_t edge = 0;
		bool const haveCells = host::AutoPlotGrid_FloorCells(0, &floorCells, &edge, &why);
		bool cellsOk = haveCells && !floorCells.empty();

		for (AutoPlotPoint const &c : floorCells)
		{
			cellsOk = cellsOk && c.module == 0 && c.surface == AutoPlotPoint::Surface::Floor && c.place[1] == 0 && c.place[0] >= 0 && c.place[0] <= 8000;
		}

		Check(cellsOk && edge == 300, "module 0's floor cells: all on its floor (y 0), in the left hall, a cell 300 mm a side (" + std::to_string(floorCells.size()) + " cells)");
		Check(floorCells.size() * 300.0 * 300.0 > 8000.0 * 20000.0 * 0.5, "they cover more than half of its 8 x 20 m floor (a marine's square keeps off the walls)");

		std::vector<AutoPlotPoint> noCells;

		Check(!host::AutoPlotGrid_FloorCells(7, &noCells, &edge, &why) && noCells.empty(), "a module with no floor: false (" + why + ")");
		Check(!host::AutoPlotGrid_FloorCells(-1, &noCells, &edge, &why), "no module: false (" + why + ")");

		std::vector<AutoPlotPoint> const spots = host::AutoPlotGrid_StandSpots(0, 4);
		bool ownModule = !spots.empty(), apart = true;

		for (size_t i = 0; i < spots.size(); i++)
		{
			ownModule = ownModule && spots[i].module == 0 && spots[i].place[0] < 8000 && spots[i].surface == AutoPlotPoint::Surface::Floor;

			for (size_t j = 0; j < i; j++)
			{
				double const dx = (double)spots[i].place[0] - spots[j].place[0], dz = (double)spots[i].place[2] - spots[j].place[2];

				apart = apart && sqrt(dx * dx + dz * dz) >= 1000.0;
			}
		}

		Check(spots.size() == 4 && ownModule, "four stand spots in module 0, on its floor in the left room (" + std::to_string(spots.size()) + ")");
		Check(apart, "spread at least a metre apart");
		Check(host::AutoPlotGrid_StandSpots(1, 1000).size() <= host::kAutoPlotMostSpots && !host::AutoPlotGrid_StandSpots(1, 2).empty(), "module 1 has its own, never more than 64");
		Check(host::AutoPlotGrid_StandSpots(7, 4).empty() && host::AutoPlotGrid_StandSpots(-1, 4).empty(), "a module with no polygons, or none: no spots");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
		Check(!host::AutoPlotGrid_Ready(&why) && !host::AutoPlotGrid_Way(left, right, true, &alien, &why), "a refused start lets the old grid go");
	}

	printf("CLIMB STATION: A CORRIDOR\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::string why;
		std::vector<AutoPlotPoint> way;
		int32_t top[3] = { 0, 0, 0 };
		int32_t const exit[3] = { 12100, -10, 2000 }, through[3] = { 65536, 0, 0 }, sides[2] = { 0, 4000 };

		Check(!host::AutoPlotGrid_ClimbStation(exit, through, 6000, &way, top, &why) && way.empty(), "no grid: no station (" + why + ")");
		Corridor(&polygons, &modules);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why) && host::AutoPlotGrid_Ready(&why), "the corridor's grid is ready (" + why + ")");

		bool const found = host::AutoPlotGrid_ClimbStation(exit, through, 6000, &way, top, &why);

		CheckStation(found, way, top, exit, why, "the corridor's doorway, landing in module 1", 1, 2, sides);

		std::vector<AutoPlotPoint> none;
		int32_t noTop[3] = { 0, 0, 0 };
		bool const tooNear = host::AutoPlotGrid_ClimbStation(exit, through, 1200, &none, noTop, &why);

		Check(!tooNear && none.empty() && why.find("no wall") != std::string::npos, "the side walls are 2 m away: within 1.2 m there is no station (" + why + ")");

		int32_t const nowhere[3] = { 100000, -10, 100000 };
		bool const outside = host::AutoPlotGrid_ClimbStation(nowhere, through, 6000, &none, noTop, &why);   // (called first: an argument's words are made in no set order)

		Check(!outside && none.empty() && why.find("no cell") != std::string::npos, "an exit point far out of the level: none (" + why + ")");

		// the other way through the same doorway: an alien from module 1 lands in module 0, and climbs there
		int32_t const back[3] = { 11900, -10, 2000 }, backThrough[3] = { -65536, 0, 0 };
		std::vector<AutoPlotPoint> backWay;
		int32_t backTop[3] = { 0, 0, 0 };
		bool const backFound = host::AutoPlotGrid_ClimbStation(back, backThrough, 6000, &backWay, backTop, &why);

		Check(backFound && !backWay.empty() && backWay.back().module == 0 && backTop[0] <= 12000, "landing the other way, it climbs in module 0 (x " +
			std::to_string(backTop[0]) + (backFound ? "" : ", " + why) + ")");
	}

	printf("CLIMB STATION: A DOOR INTO A HALL\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::string why;
		std::vector<AutoPlotPoint> way;
		int32_t top[3] = { 0, 0, 0 };
		int32_t const exit[3] = { 12100, -10, 2000 }, through[3] = { 65536, 0, 0 }, sides[2] = { -4000, 8000 };

		DoorIntoHall(&polygons, &modules);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why) && host::AutoPlotGrid_Ready(&why), "the hall's grid is ready (" + why + ")");

		bool const found = host::AutoPlotGrid_ClimbStation(exit, through, 8000, &way, top, &why);

		CheckStation(found, way, top, exit, why, "the hall's door: a SIDE wall 6 m away before the door wall beside it", 1, 2, sides);

		std::vector<AutoPlotPoint> near;
		int32_t nearTop[3] = { 0, 0, 0 };
		bool const nearFound = host::AutoPlotGrid_ClimbStation(exit, through, 3000, &near, nearTop, &why);
		bool doorWall = nearFound;

		for (AutoPlotPoint const &p : near)
		{
			doorWall = doorWall && (p.surface != AutoPlotPoint::Surface::Wall || abs(p.place[0] - 12000) <= 300);
		}

		Check(doorWall && !near.empty() && near.back().module == 1, "with no side wall within 3 m, the door wall beside the door, on the hall's side (" +
			(near.empty() ? why : "x " + std::to_string(nearTop[0]) + " z " + std::to_string(nearTop[2])) + ")");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
	}

	// THE CEILING AT ANY HEIGHT (Drake 11-20, Dietrich's ask for the owner's "Stop only at exits modules with open sky": a tall ceiling is still a ceiling; Genshd1's go up to
	// 15.6 m, SHED06). A HALL, module 0, x 0..6000, z 0..6000, floor y 0, its ceiling 15.6 m up (y -15600), a mezzanine's ceiling 3 m up over x 0..2000 (y -3000); beside it
	// a YARD, module 1, x 6000..12000, floor y 0, walls 3 m high, no ceiling at all: open sky
	printf("CEILING OVER: ANY HEIGHT\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::string why;

		polygons.push_back(Floor(0, 6000, 0, 6000, 0));
		polygons.push_back(Ceiling(0, 6000, 0, 6000, -15600));
		polygons.push_back(Ceiling(0, 2000, 0, 6000, -3000));
		polygons.push_back(WallAtX(0, 0, 6000, -15600, 0, 1));
		polygons.push_back(WallAtZ(0, 0, 6000, -15600, 0, 1));
		polygons.push_back(WallAtZ(6000, 0, 6000, -15600, 0, -1));
		modules.resize(polygons.size(), 0);
		polygons.push_back(Floor(6000, 12000, 0, 6000, 0));
		polygons.push_back(WallAtX(12000, 0, 6000, -3000, 0, -1));
		polygons.push_back(WallAtZ(0, 6000, 12000, -3000, 0, 1));
		polygons.push_back(WallAtZ(6000, 6000, 12000, -3000, 0, -1));
		modules.resize(polygons.size(), 1);

		host::AutoPlotGrid_BuildOnTheCallersThreadForATest(true);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why) && host::AutoPlotGrid_Ready(&why), "the hall and the yard are ready (" + why + ")");
		Check(host::kAutoPlotMostCeilingLook >= 64000, "the longest look is 64 m or more (was 8 m: Genshd1's ceilings go to 15.6 m)");

		int32_t const tall[3] = { 4000, -500, 3000 }, under[3] = { 1000, -500, 3000 }, yard[3] = { 9000, -500, 3000 }, overTop[3] = { 4000, -16000, 3000 };
		AutoPlotPoint found;

		bool const tallFound = host::AutoPlotGrid_CeilingOver(tall, 64000, &found, &why);
		Check(tallFound && abs(found.place[1] + 15600) <= 300 && found.surface == AutoPlotPoint::Surface::Ceiling && found.module == 0,
			"a ceiling 15.6 m up: found with a 64 m look (" + (tallFound ? "y " + std::to_string(found.place[1]) : why) + ")");
		Check(!host::AutoPlotGrid_CeilingOver(tall, 8000, &found, &why) && why.find("8000") != std::string::npos, "asked for 8 m only: not found (" + why + ")");

		bool const lowFound = host::AutoPlotGrid_CeilingOver(under, 64000, &found, &why);
		Check(lowFound && abs(found.place[1] + 3000) <= 300, "under the mezzanine: its ceiling, the nearest over the place (" + (lowFound ? "y " + std::to_string(found.place[1]) : why) + ")");

		Check(!host::AutoPlotGrid_CeilingOver(yard, 64000, &found, &why), "the yard: open sky, no ceiling at any height (" + why + ")");
		Check(!host::AutoPlotGrid_CeilingOver(overTop, 64000, &found, &why), "a place over the hall's ceiling: none above it (" + why + ")");

		auto const start = std::chrono::steady_clock::now();
		for (int i = 0; i < 4000; i++)
		{
			int32_t const at[3] = { 6100 + (i % 60) * 95, -500, 100 + (i / 60) * 85 };
			host::AutoPlotGrid_CeilingOver(at, 64000, &found, &why);
		}
		double const ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		Check(ms < 2000.0, "4000 open-sky asks with the 64 m look: under 2 s (" + std::to_string((int)ms) + " ms)");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
	}

	printf("FLOORS IN A BOX: ANY MODULE (Dietrich 11-20, Object11's platform at y -581 under a lane box: another module's floor)\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::string why;

		// the hall (module 0): floor at 0, ceiling 12 m up, walls; the platform (module 1): a floor 2.5 m up over the hall's x 3000..6000, z 0..3000
		polygons.push_back(Floor(0, 6000, 0, 6000, 0));
		polygons.push_back(Ceiling(0, 6000, 0, 6000, -12000));
		polygons.push_back(WallAtX(0, 0, 6000, -12000, 0, 1));
		polygons.push_back(WallAtX(6000, 0, 6000, -12000, 0, -1));
		polygons.push_back(WallAtZ(0, 0, 6000, -12000, 0, 1));
		polygons.push_back(WallAtZ(6000, 0, 6000, -12000, 0, -1));
		modules.resize(polygons.size(), 0);
		polygons.push_back(Floor(3000, 6000, 0, 3000, -2500));
		modules.resize(polygons.size(), 1);

		std::vector<AutoPlotPoint> cells;
		int32_t const lo[3] = { 2000, -4000, 0 }, hi[3] = { 7000, 500, 4000 };

		Check(!host::AutoPlotGrid_FloorsIn(lo, hi, &cells, &why) && cells.empty(), "no grid yet: none (" + why + ")");

		host::AutoPlotGrid_BuildOnTheCallersThreadForATest(true);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why) && host::AutoPlotGrid_Ready(&why), "the hall and its platform are ready (" + why + ")");

		bool const found = host::AutoPlotGrid_FloorsIn(lo, hi, &cells, &why);
		size_t hall = 0, platform = 0, outside = 0;
		for (AutoPlotPoint const &c : cells)
		{
			bool const in = c.place[0] >= lo[0] && c.place[0] <= hi[0] && c.place[1] >= lo[1] && c.place[1] <= hi[1] && c.place[2] >= lo[2] && c.place[2] <= hi[2];
			outside += in && c.surface == AutoPlotPoint::Surface::Floor ? 0 : 1;
			hall += c.module == 0 && abs(c.place[1]) <= 300 ? 1 : 0;
			platform += c.module == 1 && abs(c.place[1] + 2500) <= 300 ? 1 : 0;
		}
		Check(found && platform > 0, "the platform's floor, another module's, is in the box (" + std::to_string(platform) + " cells; " + (found ? std::string("found") : why) + ")");
		Check(found && hall > 0, "and the hall's floor under it (" + std::to_string(hall) + " cells)");
		Check(found && outside == 0 && hall + platform == cells.size(), "every cell given is a floor inside the box, of one of the two (" + std::to_string(outside) + " not)");

		int32_t const lowOnly[3] = { 2000, -1000, 0 };
		std::vector<AutoPlotPoint> low;
		bool const lowFound = host::AutoPlotGrid_FloorsIn(lowOnly, hi, &low, &why);
		bool lowAll = lowFound && !low.empty();
		for (AutoPlotPoint const &c : low)
		{
			lowAll = lowAll && c.module == 0;
		}
		Check(lowAll, "a box under the platform's height: the hall's cells only (" + std::to_string(low.size()) + ")");

		int32_t const airLo[3] = { 1000, -11000, 1000 }, airHi[3] = { 2000, -9000, 2000 };
		std::vector<AutoPlotPoint> air(3);
		Check(host::AutoPlotGrid_FloorsIn(airLo, airHi, &air, &why) && air.empty(), "a box in the air: true, none (" + why + ")");

		int32_t const turnedLo[3] = { 7000, 500, 4000 }, turnedHi[3] = { 2000, -4000, 0 };
		std::vector<AutoPlotPoint> turned(2);
		Check(host::AutoPlotGrid_FloorsIn(turnedLo, turnedHi, &turned, &why) && turned.empty(), "hostile: ends the wrong way round: none");

		int32_t const hugeLo[3] = { INT32_MIN, INT32_MIN, INT32_MIN }, hugeHi[3] = { INT32_MAX, INT32_MAX, INT32_MAX };
		std::vector<AutoPlotPoint> every;
		auto const start = std::chrono::steady_clock::now();
		bool const everyFound = host::AutoPlotGrid_FloorsIn(hugeLo, hugeHi, &every, &why);
		double const ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		Check(everyFound && every.size() >= cells.size() && ms < 2000.0, "hostile: the whole number range: every floor cell, bounded by the level's own (" +
			std::to_string(every.size()) + " cells, " + std::to_string((int)ms) + " ms)");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
		Check(!host::AutoPlotGrid_FloorsIn(lo, hi, &cells, &why) && cells.empty(), "the grid gone: none (" + why + ")");
	}

	printf("ROOF OVER: THE GATE'S OPEN SKY, FROM THE POLYGONS (Dietrich 11-20: door modules read 100%% open by the cling cells, 0%% by the polygons)\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::vector<bool> objects;
		std::string why;
		auto const add = [&](AlienWaysPolygon p, int32_t module, bool object) { polygons.push_back(p); modules.push_back(module); objects.push_back(object); };

		// the hall (module 0): floor at 0, ceiling 15.6 m up, walls
		add(Floor(0, 6000, 0, 6000, 0), 0, false);
		add(Ceiling(0, 6000, 0, 6000, -15600), 0, false);
		add(WallAtX(0, 0, 6000, -15600, 0, 1), 0, false);
		add(WallAtZ(0, 0, 6000, -15600, 0, 1), 0, false);
		add(WallAtZ(6000, 0, 6000, -15600, 0, -1), 0, false);
		// a door (module 2): its floor and a roof 3 m up only 400 mm across (no room for an alien to cling)
		add(Floor(6000, 6400, 0, 6000, 0), 2, false);
		add(Ceiling(6000, 6400, 0, 6000, -3000), 2, false);
		// the yard (module 1): a floor, no roof; a still object's top 2 m up over part of it; a sky polygon 80 m up over everything (another module, 3)
		add(Floor(6400, 12000, 0, 6000, 0), 1, false);
		add(Floor(9000, 10000, 2000, 3000, -2000), 1, true);
		add(Ceiling(-1000000000, 1000000000, -1000000000, 1000000000, -80000), 3, false);
		// a floor's TOP 2 m over the yard (module 1's, an upper deck seen from below): it faces up, so it holds no hanging alien (Bishop's ruling 11-20, the game's collision
		// takes a polygon only from its front: dynamics.cpp:4486-4487)
		add(Floor(6500, 7500, 4000, 5000, -2500), 1, false);

		int32_t const hall[3] = { 3000, -500, 3000 }, door[3] = { 6200, -500, 3000 }, yard[3] = { 8000, -500, 1000 }, crate[3] = { 9500, -500, 2500 };
		AutoPlotPoint found;

		Check(!host::AutoPlotGrid_RoofOver(hall, 64000, &found, &why), "no grid yet: none (" + why + ")");

		host::AutoPlotGrid_BuildOnTheCallersThreadForATest(true);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why, objects) && host::AutoPlotGrid_Ready(&why), "the hall, the door and the yard are ready (" +
			why + ")");

		bool const hallFound = host::AutoPlotGrid_RoofOver(hall, 64000, &found, &why);
		Check(hallFound && found.place[1] == -15600 && found.place[0] == 3000 && found.place[2] == 3000 && found.module == 0 && found.surface == AutoPlotPoint::Surface::Ceiling,
			"the hall: its ceiling polygon 15.6 m up, straight over the place (" + (hallFound ? "y " + std::to_string(found.place[1]) : why) + ")");
		Check(!host::AutoPlotGrid_RoofOver(hall, 8000, &found, &why), "asked for 8 m only: none (" + why + ")");

		bool const doorFound = host::AutoPlotGrid_RoofOver(door, 64000, &found, &why);
		Check(doorFound && found.place[1] == -3000 && found.module == 2, "the door: its own roof 3 m up, a door module's polygon (" + (doorFound ? "y " + std::to_string(found.place[1]) +
			", module " + std::to_string(found.module) : why) + ")");

		Check(!host::AutoPlotGrid_RoofOver(yard, 64000, &found, &why), "the yard: open sky (the sky polygon is 80 m up, past the 64 m look) (" + why + ")");
		Check(!host::AutoPlotGrid_RoofOver(crate, 64000, &found, &why), "under a still object's top: open sky still (the gate reads the modules' shapes only) (" + why + ")");
		Check(!host::AutoPlotGrid_RoofOver(yard, 2000000000, &found, &why), "hostile: a look past 64 m is held to 64 m (" + why + ")");

		int32_t const justUnder[3] = { 6200, -2700, 3000 }, justOver[3] = { 6200, -2699, 3000 };
		Check(!host::AutoPlotGrid_RoofOver(justUnder, 64000, &found, &why) || found.place[1] != -3000, "a roof only 300 mm up: not a roof (the gate's s < y - 300)");
		bool const overFound = host::AutoPlotGrid_RoofOver(justOver, 64000, &found, &why);
		Check(overFound && found.place[1] == -3000, "301 mm up: a roof");

		// ONLY THESE MODULES (a door module's own roof): over the door, the door's roof for module 2, none for the hall's (0); over the hall, the hall's for 0, none for 2
		std::vector<int32_t> const doorOnly(1, 2), hallOnly(1, 0), none;
		bool const ownDoor = host::AutoPlotGrid_RoofOver(door, 64000, &found, &why, &doorOnly);
		Check(ownDoor && found.place[1] == -3000 && found.module == 2, "only the door's own module: its roof (" + (ownDoor ? std::to_string(found.place[1]) : why) + ")");
		Check(!host::AutoPlotGrid_RoofOver(door, 64000, &found, &why, &hallOnly), "only the hall's: none over the door (" + why + ")");
		bool const ownHall = host::AutoPlotGrid_RoofOver(hall, 64000, &found, &why, &hallOnly);
		Check(ownHall && found.place[1] == -15600 && found.module == 0, "only the hall's, over the hall: its ceiling");
		Check(!host::AutoPlotGrid_RoofOver(hall, 64000, &found, &why, &doorOnly), "only the door's, over the hall: none");
		Check(!host::AutoPlotGrid_RoofOver(hall, 64000, &found, &why, &none), "an empty list of modules: none");

		int32_t const underDeck[3] = { 7000, -500, 4500 };
		Check(!host::AutoPlotGrid_RoofOver(underDeck, 64000, &found, &why), "under a floor's top that faces up: no roof (only a polygon facing down is one) (" + why + ")");

		int32_t const off[3] = { 50000, -500, 50000 };
		Check(!host::AutoPlotGrid_RoofOver(off, 64000, &found, &why), "a place past every polygon but the far sky: none");

		auto const start = std::chrono::steady_clock::now();
		for (int i = 0; i < 4000; i++)
		{
			int32_t const at[3] = { 100 + (i % 120) * 99, -500, 100 + (i / 120) * 170 };
			host::AutoPlotGrid_RoofOver(at, 64000, &found, &why);
		}
		double const ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		Check(ms < 2000.0, "4000 asks over the hall, the door and the yard: under 2 s (" + std::to_string((int)ms) + " ms)");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
		Check(!host::AutoPlotGrid_RoofOver(hall, 64000, &found, &why), "the grid gone: none (" + why + ")");
	}

	printf("ROOF HUNG IN: THE HANGING ALIEN'S OWN SPACE (Bishop 11-20; water under top, office under new03)\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		std::string why;
		auto const add = [&](AlienWaysPolygon p, int32_t module) { polygons.push_back(p); modules.push_back(module); };

		// WATER (module 4): a floor at 0 and walls up to -2400, no roof of its own; TOP (module 5) stacked over it: walls from -2400 up to -6000 and its ceiling at -6000
		add(Floor(0, 6000, 0, 6000, 0), 4);
		add(WallAtX(0, 0, 6000, -2400, 0, 1), 4);
		add(WallAtX(6000, 0, 6000, -2400, 0, -1), 4);
		add(WallAtX(0, 0, 6000, -6000, -2400, 1), 5);
		add(WallAtX(6000, 0, 6000, -6000, -2400, -1), 5);
		add(Ceiling(0, 6000, 0, 6000, -6000), 5);
		// OFFICE (module 6): a floor at 0 and walls up to -3000, its roof NEW03's (module 7) polygon at -3000 over it
		add(Floor(20000, 26000, 0, 6000, 0), 6);
		add(WallAtX(20000, 0, 6000, -3000, 0, 1), 6);
		add(WallAtX(26000, 0, 6000, -3000, 0, -1), 6);
		add(Ceiling(20000, 26000, 0, 6000, -3000), 7);
		add(Floor(26000, 30000, 0, 6000, -3000), 7);

		int32_t const inWater[3] = { 3000, -500, 3000 }, inOffice[3] = { 23000, -500, 3000 };
		AutoPlotPoint found;
		std::vector<int32_t> const water(1, 4), top(1, 5), office(1, 6);

		host::AutoPlotGrid_BuildOnTheCallersThreadForATest(true);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why) && host::AutoPlotGrid_Ready(&why), "water under top, office under new03: ready (" + why + ")");

		Check(!host::AutoPlotGrid_RoofHungIn(inWater, 64000, water, &found, &why), "in water: top's ceiling hangs its alien in top's space, so no roof of water's (" + why + ")");
		bool const forTop = host::AutoPlotGrid_RoofHungIn(inWater, 64000, top, &found, &why);
		Check(forTop && found.place[1] == -6000 && found.module == 5, "the same place for top's modules: top's ceiling (" + (forTop ? std::to_string(found.place[1]) : why) + ")");
		bool const forOffice = host::AutoPlotGrid_RoofHungIn(inOffice, 64000, office, &found, &why);
		Check(forOffice && found.place[1] == -3000 && found.module == 7, "in office: new03's ceiling, its hanging place inside office's box, is office's roof (" +
			(forOffice ? std::to_string(found.place[1]) + ", module " + std::to_string(found.module) : why) + ")");
		Check(!host::AutoPlotGrid_RoofHungIn(inOffice, 2000, office, &found, &why), "held to its look: 2 m is short of new03's 2.5 m (" + why + ")");

		int32_t lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
		bool const box = host::AutoPlotGrid_ModuleBox(4, lo, hi);
		Check(box && lo[0] == 0 && hi[0] == 6000 && lo[1] == -2400 && hi[1] == 0 && lo[2] == 0 && hi[2] == 6000, "water's box: its polygons' extent (" +
			(box ? std::to_string(lo[1]) + ".." + std::to_string(hi[1]) : std::string("none")) + ")");
		Check(!host::AutoPlotGrid_ModuleBox(99, lo, hi), "a module with no polygons: no box");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
		Check(!host::AutoPlotGrid_RoofHungIn(inOffice, 64000, office, &found, &why) && !host::AutoPlotGrid_ModuleBox(4, lo, hi), "the grid gone: none");
	}

	// THE CRAWL CELLS (Drake 11-20, Frost's ask for Bishop's order: Genshd1's ducts shft10 / shft12 / shft18 have real floors but are 1.9 m tall, so a MARINE stands
	// nowhere there and FloorCells gives none, though aliens crawl them): AutoPlotGrid_CrawlCells gives the floor cells a crouched alien stands on (its crouching top is
	// 900: npc_grid's kNpcGridClings on a floor sheet). Module 0, a closed duct 6 x 3 m, 1.9 m tall; module 1, a closed slot 6 x 3 m, 0.8 m tall: under an alien's 900
	printf("THE CRAWL CELLS\n");
	{
		std::vector<AlienWaysPolygon> polygons;
		std::vector<int32_t> modules;
		int const tops[2] = { -1900, -800 }, xs[2] = { 0, 7000 };

		for (int m = 0; m < 2; m++)
		{
			int const x0 = xs[m], x1 = xs[m] + 6000, top = tops[m];
			size_t const before = polygons.size();

			polygons.push_back(Floor(x0, x1, 0, 3000, 0));
			polygons.push_back(Ceiling(x0, x1, 0, 3000, top));
			polygons.push_back(WallAtX(x0, 0, 3000, top, 0, 1));
			polygons.push_back(WallAtX(x1, 0, 3000, top, 0, -1));
			polygons.push_back(WallAtZ(0, x0, x1, top, 0, 1));
			polygons.push_back(WallAtZ(3000, x0, x1, top, 0, -1));
			modules.resize(polygons.size(), -1);

			for (size_t i = before; i < polygons.size(); i++)
			{
				modules[i] = m;
			}
		}

		std::string why;

		host::AutoPlotGrid_BuildOnTheCallersThreadForATest(true);
		Check(host::AutoPlotGrid_StartFrom(std::move(polygons), std::move(modules), &why) && host::AutoPlotGrid_Ready(&why), "the duct and the slot are ready (" + why + ")");

		std::vector<AutoPlotPoint> cells;
		int32_t edge = 0;

		Check(!host::AutoPlotGrid_FloorCells(0, &cells, &edge, &why) && cells.empty(), "the 1.9 m duct: no floor cell a marine stands on (" + why + ")");

		bool const crawl = host::AutoPlotGrid_CrawlCells(0, &cells, &edge, &why);
		bool onFloor = crawl && !cells.empty();

		for (AutoPlotPoint const &c : cells)
		{
			onFloor = onFloor && c.module == 0 && c.surface == AutoPlotPoint::Surface::Floor && c.place[1] == 0 && c.place[0] >= 0 && c.place[0] <= 6000 && c.place[2] >= 0 &&
				c.place[2] <= 3000;
		}

		Check(onFloor && edge == 300, "the duct's CRAWL cells: all on its floor (y 0), inside it, a cell 300 a side (" + (crawl ? std::to_string(cells.size()) + " cells" : why) + ")");
		Check(cells.size() * 300.0 * 300.0 > 6000.0 * 3000.0 * 0.5, "they cover more than half of its 6 x 3 m floor");

		std::vector<AutoPlotPoint> slot;

		Check(!host::AutoPlotGrid_CrawlCells(1, &slot, &edge, &why) && slot.empty(), "the 0.8 m slot: no crawl cell, under a crouching alien's 900 (" + why + ")");
		Check(!host::AutoPlotGrid_CrawlCells(-1, &slot, &edge, &why), "no module: false (" + why + ")");

		host::AutoPlotGrid_StartRefused("no grid: the next level's polygons");
		Check(!host::AutoPlotGrid_CrawlCells(0, &slot, &edge, &why), "the grid gone: none");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
