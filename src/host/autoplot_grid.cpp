// autoplot_grid.cpp: see autoplot_grid.h. The build and the searches, from a list of polygons (autoplot_grid_feed.h); the listing of the open level, which reads the
// game's scene, is autoplot_grid_game.cpp, so the tests build this with no game. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "alien_ways_build.h"
#include "alien_ways_map.h"
#include "npc_grid.h"

#include "autoplot_grid.h"
#include "autoplot_grid_feed.h"

#include <math.h>

#include <mutex>

namespace host
{
	int32_t AutoPlot_ModuleOfListed(std::vector<AutoPlotRange> const &modules, size_t listed)
	{
		for (size_t m = 0; m < modules.size(); m++)
		{
			if (listed >= modules[m].first && listed < modules[m].end)
			{
				return (int32_t)m;
			}
		}

		return -1;
	}

	std::vector<size_t> AutoPlot_Spread(std::vector<AutoPlotPoint> const &places, size_t most)
	{
		std::vector<size_t> picked;
		size_t const want = most < kAutoPlotMostSpots ? most : kAutoPlotMostSpots;

		if (places.empty() || want == 0)
		{
			return picked;
		}

		std::vector<double> nearest(places.size(), 1e300);   // each place's distance (squared) to the nearest picked one
		size_t next = 0;

		while (picked.size() < want && picked.size() < places.size())
		{
			picked.push_back(next);

			AutoPlotPoint const &p = places[next];
			size_t farthest = places.size();
			double farthestDistance = -1.0;

			for (size_t i = 0; i < places.size(); i++)
			{
				double const dx = (double)places[i].place[0] - p.place[0], dy = (double)places[i].place[1] - p.place[1], dz = (double)places[i].place[2] - p.place[2];
				double const d = dx * dx + dy * dy + dz * dz;

				nearest[i] = d < nearest[i] ? d : nearest[i];

				if (nearest[i] > farthestDistance)   // strictly farther: on a tie the earlier keeps it
				{
					farthestDistance = nearest[i];
					farthest = i;
				}
			}

			if (farthest >= places.size() || !(farthestDistance > 0.0))
			{
				break;   // every place left is one already picked (or at the same spot as one)
			}

			next = farthest;
		}

		return picked;
	}

	void AutoPlot_Facing65536(int16_t const in[3], int32_t out[3])
	{
		double const x = in[0], y = in[1], z = in[2];
		double const length = sqrt(x * x + y * y + z * z);

		for (int i = 0; i < 3; i++)
		{
			out[i] = length > 0.5 ? (int32_t)floor((double)in[i] / length * 65536.0 + 0.5) : 0;
		}
	}

	namespace
	{
		std::mutex gridLock;                      // Start, Way and StandSpots: one at a time (the grid keeps one search's state)
		AlienWaysBuild theBuild;                  // the map and the grid, built on its own thread (alien_ways_build.h)
		std::vector<int32_t> listedModule;        // the game module of each polygon of the listing, -1 for none
		std::vector<int32_t> mapToListed;         // the listing's place of each map polygon, made once the build is ready
		bool startedOnce = false;
		std::string startWhy;                     // why the last Start couldn't build, if it couldn't
		bool buildOnCallersThread = false;        // a test's (AutoPlotGrid_BuildOnTheCallersThreadForATest)

		AutoPlotPoint::Surface SurfaceOf(AlienWaysKind kind)
		{
			return kind == kAlienWaysWall ? AutoPlotPoint::Surface::Wall : (kind == kAlienWaysCeiling ? AutoPlotPoint::Surface::Ceiling : AutoPlotPoint::Surface::Floor);
		}

		// With the lock held: the grid, once the build is ready (and the map's numbers turned round the first time)
		NpcGrid *ReadyGrid(std::string *why)
		{
			AlienWaysBuildState const state = theBuild.State();

			if (state == kAlienWaysBuilding)
			{
				*why = "building";
				return nullptr;
			}

			if (state == kAlienWaysRefused || (state == kAlienWaysReady && !theBuild.Grid()))
			{
				char const *const reason = state == kAlienWaysRefused ? theBuild.Why() : theBuild.GridWhy();

				*why = std::string("no grid: ") + (reason ? reason : "refused");
				return nullptr;
			}

			if (state != kAlienWaysReady)
			{
				*why = startWhy.empty() ? "no grid: not started" : startWhy;
				return nullptr;
			}

			if (mapToListed.empty())
			{
				AlienWaysMap &map = theBuild.Map();

				mapToListed.assign(map.Polygons(), -1);

				for (size_t given = 0; given < map.PolygonsGiven(); given++)
				{
					int const number = map.MapNumberOf(given);

					if (number >= 0 && (size_t)number < mapToListed.size())
					{
						mapToListed[(size_t)number] = (int32_t)given;
					}
				}
			}

			return theBuild.Grid();
		}

		// A start or goal with no cell the body fits on within the search's own reach (kNpcGridReach, 1.5 m) is moved to the nearest such cell within kAutoPlotSnapReach:
		// a level's exit points (ADJMDLEP) lie in the middle of a doorway, about 2 m off the floor in a high corridor (Colony, 11-18: 49 of 187 searches between exits
		// found no cell near their ends). The way's first and last points are then that cell's, which shows how far it was moved.
		void SnapToTheGrid(NpcGrid const &grid, NpcGridBody body, AlienWaysXYZ *at)
		{
			if (grid.NearestCell(at->x, at->y, at->z, body, kNpcGridReach) != kNpcGridNoCell)
			{
				return;
			}

			uint32_t const cell = grid.NearestCell(at->x, at->y, at->z, body, kAutoPlotSnapReach);
			double middle[3];

			if (cell != kNpcGridNoCell && grid.Middle(cell, middle))
			{
				at->x = (int)floor(middle[0] + 0.5);
				at->y = (int)floor(middle[1] + 0.5);
				at->z = (int)floor(middle[2] + 0.5);
			}
		}

		int32_t ModuleOfMapPolygon(int polygon)
		{
			if (polygon < 0 || (size_t)polygon >= mapToListed.size())
			{
				return -1;
			}

			int32_t const listed = mapToListed[(size_t)polygon];

			return listed >= 0 && (size_t)listed < listedModule.size() ? listedModule[(size_t)listed] : -1;
		}
	}

	bool AutoPlotGrid_StartFrom(std::vector<AlienWaysPolygon> &&polygons, std::vector<int32_t> &&modules, std::string *why)
	{
		std::lock_guard<std::mutex> hold(gridLock);

		theBuild.Stop();
		mapToListed.clear();
		startedOnce = true;
		startWhy.clear();

		if (modules.size() != polygons.size())
		{
			listedModule.clear();
			startWhy = "no grid: the polygons and their modules don't match";
			*why = startWhy;
			return false;
		}

		listedModule = std::move(modules);
		theBuild.NoThreadForATest(buildOnCallersThread);
		theBuild.Start(std::move(polygons), true);
		return true;
	}

	void AutoPlotGrid_StartRefused(std::string const &why)
	{
		std::lock_guard<std::mutex> hold(gridLock);

		theBuild.Stop();
		listedModule.clear();
		mapToListed.clear();
		startedOnce = true;
		startWhy = why;
	}

	void AutoPlotGrid_BuildOnTheCallersThreadForATest(bool callers)
	{
		std::lock_guard<std::mutex> hold(gridLock);

		buildOnCallersThread = callers;
	}

	bool AutoPlotGrid_Ready(std::string *why)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;

		if (ReadyGrid(&reason))
		{
			return true;
		}

		if (why)
		{
			*why = startedOnce ? reason : "no grid: not started";
		}

		return false;
	}

	bool AutoPlotGrid_Way(int32_t const from[3], int32_t const to[3], bool alien, std::vector<AutoPlotPoint> *way, std::string *why)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;
		NpcGrid *const grid = ReadyGrid(&reason);

		way->clear();

		if (!grid)
		{
			*why = reason;
			return false;
		}

		NpcGridBody const body = alien ? kNpcGridAlien : kNpcGridMarine;
		AlienWaysXYZ start = { from[0], from[1], from[2] };
		AlienWaysXYZ goal = { to[0], to[1], to[2] };

		SnapToTheGrid(*grid, body, &start);
		SnapToTheGrid(*grid, body, &goal);

		char const *const refused = grid->StartSearch(start, goal, body, alien ? kAutoPlotAlienFloorPrice : 1, kAutoPlotMostLooks);

		if (refused)
		{
			*why = std::string("no way: ") + refused;
			return false;
		}

		// Work in steps (never the one-shot Search: npc_grid.cpp:1739-1754); the looks are capped by StartSearch's own, so this ends
		NpcGridSearch answer = kNpcGridSearching;

		for (size_t round = 0; round < kAutoPlotMostLooks / 20000 + 4 && answer == kNpcGridSearching; round++)
		{
			answer = grid->Work(20000);
		}

		if (answer != kNpcGridFound)
		{
			*why = answer == kNpcGridNoWay ? "no way: the cells don't join up for this body" : (answer == kNpcGridTooFar ? "no way: it looked as far as it may" :
				"no way: the search didn't end");
			return false;
		}

		std::vector<AlienWaysPoint> const &points = grid->Way();
		std::vector<int> const &polygons = grid->WayPolygons();

		way->reserve(points.size());

		for (size_t i = 0; i < points.size(); i++)
		{
			AutoPlotPoint p;

			p.place[0] = points[i].x;
			p.place[1] = points[i].y;
			p.place[2] = points[i].z;
			p.surface = SurfaceOf(points[i].kind);
			AutoPlot_Facing65536(points[i].facing, p.facing);
			p.module = i < polygons.size() ? ModuleOfMapPolygon(polygons[i]) : -1;
			way->push_back(p);
		}

		return true;
	}

	bool AutoPlotGrid_Size(size_t *cells, size_t *bytesKept)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;
		NpcGrid *const grid = ReadyGrid(&reason);

		*cells = grid ? grid->Cells() : 0;
		*bytesKept = grid ? grid->BytesKept() : 0;
		return grid != nullptr;
	}

	namespace
	{
		// With the lock held: every cell a marine stands on in the game module (its floor point: the cell's middle on its sheet), at most `most`; false (with why) past it
		bool StandingCells(NpcGrid const &grid, int32_t module, size_t most, std::vector<AutoPlotPoint> *out, std::string *why)
		{
			out->clear();

			for (uint32_t cell = 0; cell < (uint32_t)grid.Cells(); cell++)
			{
				double middle[3];

				if ((grid.Flags(cell) & kNpcGridStands) == 0 || ModuleOfMapPolygon(grid.PolygonOf(cell)) != module || !grid.Middle(cell, middle))
				{
					continue;
				}

				if (out->size() >= most)
				{
					out->clear();
					*why = "more floor cells in game module " + std::to_string(module) + " than " + std::to_string((unsigned long long)most);
					return false;
				}

				AutoPlotPoint p;

				p.place[0] = (int32_t)floor(middle[0] + 0.5);
				p.place[1] = (int32_t)floor(middle[1] + 0.5);
				p.place[2] = (int32_t)floor(middle[2] + 0.5);
				p.surface = AutoPlotPoint::Surface::Floor;
				p.module = module;
				out->push_back(p);
			}

			return true;
		}
	}

	bool AutoPlotGrid_FloorCells(int32_t module, std::vector<AutoPlotPoint> *cells, int32_t *cellEdge, std::string *why)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		NpcGrid *const grid = ReadyGrid(why);

		cells->clear();
		*cellEdge = kNpcGridCell;

		if (!grid)
		{
			return false;
		}

		if (module < 0)
		{
			*why = "no game module";
			return false;
		}

		if (!StandingCells(*grid, module, kAutoPlotMostFloorCells, cells, why))
		{
			return false;
		}

		if (cells->empty())
		{
			*why = "no floor cells in game module " + std::to_string(module);
			return false;
		}

		return true;
	}

	std::vector<AutoPlotPoint> AutoPlotGrid_StandSpots(int32_t module, size_t most)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;
		NpcGrid *const grid = ReadyGrid(&reason);
		std::vector<AutoPlotPoint> spots, standing;

		if (!grid || module < 0 || !StandingCells(*grid, module, kAutoPlotMostFloorCells, &standing, &reason))
		{
			return spots;
		}

		for (size_t at : AutoPlot_Spread(standing, most))
		{
			spots.push_back(standing[at]);
		}

		return spots;
	}
}
