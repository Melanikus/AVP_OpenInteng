// host_autoplot_grid_test.cpp: the auto-plot's pathfinder service (src/host/autoplot_grid.h) on a level made of polygons, no game: two rooms side by side, each a
// game module of its own, joined by an open doorway. The grid is AvPx's own (npc_grid), built on the test's thread; the searches are StartSearch + Work. An alien's
// way keeps off the floor (floor is lava), a marine's stays on it; every point says its module, through map polygon -> the listing's place -> module; the stand spots
// are a module's own, spread out; the refusals. The polygon helpers are avpx/tests/npc_grid_test.cpp's. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>
#include <string.h>

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

// Two long rooms, x 0..3000 (module 0) and 3000..6000 (module 1), z 0..20000, floor at y 0, ceiling 3 m up (y -3000), the wall between them open only near the far
// end, from z 16000 to 17500, up to 2.4 m: a way from one near end to the other runs 16 m along each room, so floor is lava shows (a floor metre costs 10). Every polygon
// of the left room, its side of the middle wall included, is module 0's, every one of the right room module 1's
static void TwoRooms(std::vector<AlienWaysPolygon> *polygons, std::vector<int32_t> *modules)
{
	for (int side = 0; side < 2; side++)
	{
		int const x0 = side == 0 ? 0 : 3000, x1 = side == 0 ? 3000 : 6000, outer = side == 0 ? 0 : 6000, facesOuter = side == 0 ? 1 : -1, facesMiddle = side == 0 ? -1 : 1;
		size_t const before = polygons->size();

		polygons->push_back(Floor(x0, x1, 0, 20000, 0));
		polygons->push_back(Ceiling(x0, x1, 0, 20000, -3000));
		polygons->push_back(WallAtX(outer, 0, 20000, -3000, 0, facesOuter));
		polygons->push_back(WallAtZ(0, x0, x1, -3000, 0, 1));
		polygons->push_back(WallAtZ(20000, x0, x1, -3000, 0, -1));
		polygons->push_back(WallAtX(3000, 0, 16000, -3000, 0, facesMiddle));
		polygons->push_back(WallAtX(3000, 17500, 20000, -3000, 0, facesMiddle));
		polygons->push_back(WallAtX(3000, 16000, 17500, -3000, -2400, facesMiddle));
		modules->resize(polygons->size(), -1);

		for (size_t i = before; i < polygons->size(); i++)
		{
			(*modules)[i] = side;
		}
	}
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

		int32_t const left[3] = { 1500, -500, 1500 }, right[3] = { 4500, -500, 1500 };   // the near ends: the way goes 16 m along each room to the doorway
		std::vector<AutoPlotPoint> alien, marine;

		bool const alienFound = host::AutoPlotGrid_Way(left, right, true, &alien, &why);   // (found first: an argument's words are made in no set order)

		Check(alienFound && alien.size() >= 2, "an alien's way from the left room to the right (" + std::to_string(alien.size()) +
			" points" + (why.empty() ? "" : ", " + why) + ")");

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

		std::vector<AutoPlotPoint> const spots = host::AutoPlotGrid_StandSpots(0, 4);
		bool ownModule = !spots.empty(), apart = true;

		for (size_t i = 0; i < spots.size(); i++)
		{
			ownModule = ownModule && spots[i].module == 0 && spots[i].place[0] < 3000 && spots[i].surface == AutoPlotPoint::Surface::Floor;

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

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
