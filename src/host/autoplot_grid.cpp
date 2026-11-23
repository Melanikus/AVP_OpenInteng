// autoplot_grid.cpp: see autoplot_grid.h. The build and the searches, from a list of polygons (autoplot_grid_feed.h); the listing of the open level, which reads the
// game's scene, is autoplot_grid_game.cpp, so the tests build this with no game. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "alien_ways_build.h"
#include "alien_ways_map.h"
#include "npc_grid.h"

#include "autoplot_grid.h"
#include "autoplot_grid_feed.h"

#include <math.h>

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <unordered_map>

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

		// THE CEILING COLUMNS (Drake 11-20, CeilingOver at any height): every ceiling cell an alien clings to, by the column (x and z in whole cells) its middle is in. Made at
		// the first CeilingOver of a grid and forgotten at every Start; with the lock held
		std::unordered_map<uint64_t, std::vector<uint32_t> > ceilingColumns;
		NpcGrid const *ceilingColumnsOf = nullptr;
		size_t ceilingColumnsCells = 0;

		// THE STANDING COLUMNS (Drake 11-20, FloorsIn): every cell a marine stands on, by its column, as the ceiling columns; made at the first FloorsIn of a grid and
		// forgotten at every Start; with the lock held
		std::unordered_map<uint64_t, std::vector<uint32_t> > standColumns;
		NpcGrid const *standColumnsOf = nullptr;
		size_t standColumnsCells = 0;

		// THE ROOFS (Drake 11-20, RoofOver): the level's polygons a line straight up can cross (each with some area seen from above: walls are left out), the modules'
		// only (a still object's is no roof), copied at every Start, by the columns (kRoofColumn a side) their extent from above covers; one over more than
		// kRoofMostColumns columns (a sky over the whole level) is looked at for every place. With the lock held
		int32_t const kRoofColumn = 4000;
		int64_t const kRoofMostColumns = 1024;

		struct Roof
		{
			int corners = 0;
			int32_t x[kAlienWaysMostCorners], y[kAlienWaysMostCorners], z[kAlienWaysMostCorners];
			int32_t module = -1;
		};

		std::vector<Roof> roofs;
		std::unordered_map<uint64_t, std::vector<uint32_t> > roofColumns;
		std::vector<uint32_t> bigRoofs;

		// EACH GAME MODULE'S BOX (Dietrich 11-20, for the own-space roof): the extent of its polygons in the listing, still objects' left out, kept at every Start
		struct ModuleExtent
		{
			int32_t lo[3], hi[3];
		};

		std::unordered_map<int32_t, ModuleExtent> moduleBoxes;

		// a hanging alien's position under the ceiling it hangs from (its CrouchingTop, 900: autoplot_lane.h kStockAlienTop)
		int32_t const kHangUnder = 900;

		void ForgetTheRoofs()
		{
			roofs.clear();
			roofColumns.clear();
			bigRoofs.clear();
			moduleBoxes.clear();
		}

		uint64_t ColumnKey(int64_t i, int64_t k)
		{
			return ((uint64_t)i * 0x9E3779B97F4A7C15ull) ^ (uint64_t)k;   // two columns may share a key: a look checks every cell's own place
		}

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

	bool AutoPlotGrid_StartFrom(std::vector<AlienWaysPolygon> &&polygons, std::vector<int32_t> &&modules, std::string *why, std::vector<bool> const &stillObject)
	{
		std::lock_guard<std::mutex> hold(gridLock);

		theBuild.Stop();
		mapToListed.clear();
		ceilingColumns.clear();
		ceilingColumnsOf = nullptr;
		standColumns.clear();
		standColumnsOf = nullptr;
		startedOnce = true;
		startWhy.clear();

		if (modules.size() != polygons.size())
		{
			listedModule.clear();
			startWhy = "no grid: the polygons and their modules don't match";
			*why = startWhy;
			return false;
		}

		// the roofs, copied before the build takes the polygons
		ForgetTheRoofs();

		for (size_t i = 0; i < polygons.size(); i++)
		{
			AlienWaysPolygon const &p = polygons[i];

			if ((i < stillObject.size() && stillObject[i]) || p.corners < 3 || p.corners > kAlienWaysMostCorners)
			{
				continue;
			}

			// its module's box grows to hold it (every facing: walls and floors too)
			if (i < modules.size() && modules[i] >= 0)
			{
				auto const known = moduleBoxes.find(modules[i]);
				ModuleExtent &box = known != moduleBoxes.end() ? known->second : moduleBoxes[modules[i]];

				for (int c = 0; c < p.corners; c++)
				{
					int32_t const v[3] = { p.x[c], p.y[c], p.z[c] };

					for (int axis = 0; axis < 3; axis++)
					{
						bool const first = known == moduleBoxes.end() && c == 0;

						box.lo[axis] = first ? v[axis] : std::min(box.lo[axis], v[axis]);
						box.hi[axis] = first ? v[axis] : std::max(box.hi[axis], v[axis]);
					}
				}
			}

			// only a polygon FACING DOWN is a roof (Bishop's ruling 11-20, on the game's own code: its collision takes a polygon only from its front, dynamics.cpp:4486-4487
			// NRBB, 4291 sphere, 1775 InterferenceAt, and a crawling alien's gravity is set from the normals it touches, 1040-1069 / 1859, so an up-facing floor's top seen
			// from below holds no hanging alien). Its facing: the listing's (the game's stored normal), the same kind the grid's own sheets take (AlienWays_KindOf)
			if (AlienWays_KindOf(p.up) != kAlienWaysCeiling)
			{
				continue;
			}

			double area = 0.0;
			int64_t lo[2] = { INT64_MAX, INT64_MAX }, hi[2] = { INT64_MIN, INT64_MIN };

			for (int c = 0; c < p.corners; c++)
			{
				int const n = (c + 1) % p.corners;

				area += (double)p.x[c] * p.z[n] - (double)p.x[n] * p.z[c];
				lo[0] = std::min<int64_t>(lo[0], p.x[c]), hi[0] = std::max<int64_t>(hi[0], p.x[c]);
				lo[1] = std::min<int64_t>(lo[1], p.z[c]), hi[1] = std::max<int64_t>(hi[1], p.z[c]);
			}

			if (fabs(area) < 2.0)
			{
				continue;   // a wall: no line straight up crosses it
			}

			Roof r;

			r.corners = p.corners;

			for (int c = 0; c < p.corners; c++)
			{
				r.x[c] = p.x[c], r.y[c] = p.y[c], r.z[c] = p.z[c];
			}

			r.module = i < modules.size() ? modules[i] : -1;

			uint32_t const index = (uint32_t)roofs.size();
			int64_t const i0 = (int64_t)floor((double)lo[0] / kRoofColumn), i1 = (int64_t)floor((double)hi[0] / kRoofColumn);
			int64_t const k0 = (int64_t)floor((double)lo[1] / kRoofColumn), k1 = (int64_t)floor((double)hi[1] / kRoofColumn);

			roofs.push_back(r);

			if ((i1 - i0 + 1) * (k1 - k0 + 1) > kRoofMostColumns)
			{
				bigRoofs.push_back(index);
				continue;
			}

			for (int64_t ci = i0; ci <= i1; ci++)
			{
				for (int64_t ck = k0; ck <= k1; ck++)
				{
					roofColumns[ColumnKey(ci, ck)].push_back(index);
				}
			}
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
		ForgetTheRoofs();
		ceilingColumns.clear();
		ceilingColumnsOf = nullptr;
		standColumns.clear();
		standColumnsOf = nullptr;
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

	bool AutoPlotGrid_Way(int32_t const from[3], int32_t const to[3], bool alien, std::vector<AutoPlotPoint> *way, std::string *why, size_t *looks)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;
		NpcGrid *const grid = ReadyGrid(&reason);

		way->clear();

		if (looks)
		{
			*looks = 0;
		}

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

		if (looks)
		{
			*looks = grid->LastSearchLooks();
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
		// With the lock held: every cell a marine stands on in the game module (its floor point: the cell's middle on its sheet), at most `most`; false (with why) past it.
		// crawl: instead every cell a CROUCHED ALIEN stands on: the grid's alien cells (kNpcGridClings: 900 of room out from the surface over the 3 x 3 round it) on a
		// FLOOR sheet (the grid's own kind of the sheet, as the ceiling cells are read at CeilingOver)
		bool StandingCells(NpcGrid const &grid, int32_t module, size_t most, std::vector<AutoPlotPoint> *out, std::string *why, bool crawl = false)
		{
			out->clear();

			for (uint32_t cell = 0; cell < (uint32_t)grid.Cells(); cell++)
			{
				double middle[3];
				bool const takes = crawl ? (grid.Flags(cell) & kNpcGridClings) != 0 && grid.KindOf(cell) == kAlienWaysFloor : (grid.Flags(cell) & kNpcGridStands) != 0;

				if (!takes || ModuleOfMapPolygon(grid.PolygonOf(cell)) != module || !grid.Middle(cell, middle))
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

	bool AutoPlotGrid_CrawlCells(int32_t module, std::vector<AutoPlotPoint> *cells, int32_t *cellEdge, std::string *why)
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

		if (!StandingCells(*grid, module, kAutoPlotMostFloorCells, cells, why, true))
		{
			return false;
		}

		if (cells->empty())
		{
			*why = "no floor a crouched alien stands on in game module " + std::to_string(module);
			return false;
		}

		return true;
	}

	bool AutoPlotGrid_FloorsIn(int32_t const lo[3], int32_t const hi[3], std::vector<AutoPlotPoint> *cells, std::string *why)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;
		NpcGrid *const grid = ReadyGrid(&reason);

		cells->clear();

		if (!grid)
		{
			*why = reason;
			return false;
		}

		// THE COLUMNS, once a grid: every cell a marine stands on, by the column its middle is in
		if (standColumnsOf != grid || standColumnsCells != grid->Cells())
		{
			standColumns.clear();

			for (size_t c = 0; c < grid->Cells(); c++)
			{
				uint32_t const cell = (uint32_t)c;
				double m[3];

				if ((grid->Flags(cell) & kNpcGridStands) == 0 || !grid->Middle(cell, m))
				{
					continue;
				}

				standColumns[ColumnKey((int64_t)floor(m[0] / kNpcGridCell), (int64_t)floor(m[2] / kNpcGridCell))].push_back(cell);
			}

			standColumnsOf = grid;
			standColumnsCells = grid->Cells();
		}

		if (lo[0] > hi[0] || lo[1] > hi[1] || lo[2] > hi[2])
		{
			return true;   // no box: none
		}

		int64_t const i0 = (int64_t)floor((double)lo[0] / kNpcGridCell), i1 = (int64_t)floor((double)hi[0] / kNpcGridCell);
		int64_t const k0 = (int64_t)floor((double)lo[2] / kNpcGridCell), k1 = (int64_t)floor((double)hi[2] / kNpcGridCell);

		// the cells of one column inside the box; `own`: only those whose own column is (i, k) (two columns may share a key), when the box's columns are walked
		auto const take = [&](std::vector<uint32_t> const &column, bool own, int64_t i, int64_t k) -> bool {
			for (uint32_t const cell : column)
			{
				double m[3];

				if (!grid->Middle(cell, m) || (own && ((int64_t)floor(m[0] / kNpcGridCell) != i || (int64_t)floor(m[2] / kNpcGridCell) != k)))
				{
					continue;
				}

				double const p[3] = { floor(m[0] + 0.5), floor(m[1] + 0.5), floor(m[2] + 0.5) };

				if (p[0] < lo[0] || p[0] > hi[0] || p[1] < lo[1] || p[1] > hi[1] || p[2] < lo[2] || p[2] > hi[2])
				{
					continue;
				}

				if (cells->size() >= kAutoPlotMostFloorCells)
				{
					cells->clear();
					*why = "more than " + std::to_string((unsigned long long)kAutoPlotMostFloorCells) + " floor cells in the box";
					return false;
				}

				AutoPlotPoint f;

				f.place[0] = (int32_t)p[0], f.place[1] = (int32_t)p[1], f.place[2] = (int32_t)p[2];
				f.surface = AutoPlotPoint::Surface::Floor;
				f.module = ModuleOfMapPolygon(grid->PolygonOf(cell));
				cells->push_back(f);
			}

			return true;
		};

		// a box over more columns than there are columns of standing cells: each of those looked at once (a hostile box costs the level's cells, never its own area)
		if ((double)(i1 - i0 + 1) * (double)(k1 - k0 + 1) > (double)standColumns.size())
		{
			for (auto const &column : standColumns)
			{
				if (!take(column.second, false, 0, 0))
				{
					return false;
				}
			}

			return true;
		}

		for (int64_t i = i0; i <= i1; i++)
		{
			for (int64_t k = k0; k <= k1; k++)
			{
				auto const column = standColumns.find(ColumnKey(i, k));

				if (column != standColumns.end() && !take(column->second, true, i, k))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool AutoPlotGrid_RoofOver(int32_t const at[3], int32_t most, AutoPlotPoint *found, std::string *why, std::vector<int32_t> const *onlyModules)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;

		if (!ReadyGrid(&reason))
		{
			*why = reason;
			return false;
		}

		int32_t const reach = most < 0 ? 0 : (most > kAutoPlotMostCeilingLook ? kAutoPlotMostCeilingLook : most);
		double const x = at[0], y = at[1], z = at[2];
		double best = -1e300;
		int32_t bestModule = -1;

		// the gate's test (lanes.cpp SurfacesAt, ceilingOverAny): the first triangle of the polygon's fan that holds x, z seen from above, its height there
		auto const look = [&](uint32_t index) {
			Roof const &r = roofs[index];

			if (onlyModules && std::find(onlyModules->begin(), onlyModules->end(), r.module) == onlyModules->end())
			{
				return;   // not one of the modules asked for
			}

			for (int k = 1; k + 1 < r.corners; k++)
			{
				double const ax = r.x[0], ay = r.y[0], az = r.z[0], bx = r.x[k], by = r.y[k], bz = r.z[k], cx = r.x[k + 1], cy = r.y[k + 1], cz = r.z[k + 1];
				double const area = (bx - ax) * (cz - az) - (cx - ax) * (bz - az);

				if (fabs(area) < 1.0)
				{
					continue;
				}

				double const s = ((x - ax) * (cz - az) - (cx - ax) * (z - az)) / area;
				double const t = ((bx - ax) * (z - az) - (x - ax) * (bz - az)) / area;

				if (s >= 0.0 && t >= 0.0 && s + t <= 1.0)
				{
					double const h = ay + s * (by - ay) + t * (cy - ay);

					if (h < y - 300.0 && h >= y - (double)reach && h > best)
					{
						best = h, bestModule = r.module;
					}

					return;
				}
			}
		};

		if (fabs(x) < 1e12 && fabs(z) < 1e12)
		{
			auto const column = roofColumns.find(ColumnKey((int64_t)floor(x / kRoofColumn), (int64_t)floor(z / kRoofColumn)));

			if (column != roofColumns.end())
			{
				for (uint32_t const index : column->second)
				{
					look(index);
				}
			}

			for (uint32_t const index : bigRoofs)
			{
				look(index);
			}
		}

		if (best == -1e300)
		{
			*why = "no roof within " + std::to_string((long long)reach) + " mm over it";
			return false;
		}

		found->place[0] = at[0];
		found->place[1] = (int32_t)floor(best + 0.5);
		found->place[2] = at[2];
		found->surface = AutoPlotPoint::Surface::Ceiling;
		found->facing[0] = 0;
		found->facing[1] = 65536;
		found->facing[2] = 0;
		found->module = bestModule;
		return true;
	}

	bool AutoPlotGrid_RoofHungIn(int32_t const at[3], int32_t most, std::vector<int32_t> const &modules, AutoPlotPoint *found, std::string *why)
	{
		AutoPlotPoint roof;

		if (!AutoPlotGrid_RoofOver(at, most, &roof, why))
		{
			return false;
		}

		std::lock_guard<std::mutex> hold(gridLock);
		int32_t const hang = roof.place[1] + kHangUnder;

		for (int32_t const m : modules)
		{
			auto const box = moduleBoxes.find(m);

			if (box != moduleBoxes.end() && at[0] >= box->second.lo[0] && at[0] <= box->second.hi[0] && at[2] >= box->second.lo[2] && at[2] <= box->second.hi[2] &&
				hang >= box->second.lo[1] && hang <= box->second.hi[1])
			{
				*found = roof;
				return true;
			}
		}

		*why = "the nearest roof (module " + std::to_string((long long)roof.module) + "'s, y " + std::to_string((long long)roof.place[1]) + ") hangs its alien at y " +
			std::to_string((long long)hang) + ", in none of these modules' space";
		return false;
	}

	bool AutoPlotGrid_ModuleBox(int32_t gameModule, int32_t lo[3], int32_t hi[3])
	{
		std::lock_guard<std::mutex> hold(gridLock);
		auto const box = moduleBoxes.find(gameModule);

		if (box == moduleBoxes.end())
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			lo[axis] = box->second.lo[axis], hi[axis] = box->second.hi[axis];
		}

		return true;
	}

	bool AutoPlotGrid_CeilingOver(int32_t const at[3], int32_t most, AutoPlotPoint *found, std::string *why)
	{
		std::lock_guard<std::mutex> hold(gridLock);
		std::string reason;
		NpcGrid *const grid = ReadyGrid(&reason);

		if (!grid)
		{
			*why = reason;
			return false;
		}

		int32_t const reach = most < 0 ? 0 : (most > kAutoPlotMostCeilingLook ? kAutoPlotMostCeilingLook : most);

		// THE COLUMNS, once a grid: every ceiling cell an alien clings to (the alien body's own test, as NearestCell's), by the column its middle is in
		if (ceilingColumnsOf != grid || ceilingColumnsCells != grid->Cells())
		{
			ceilingColumns.clear();

			for (size_t c = 0; c < grid->Cells(); c++)
			{
				uint32_t const cell = (uint32_t)c;
				double m[3];

				if (grid->KindOf(cell) != kAlienWaysCeiling || (grid->Flags(cell) & kNpcGridClings) == 0 || !grid->Middle(cell, m))
				{
					continue;
				}

				ceilingColumns[ColumnKey((int64_t)floor(m[0] / kNpcGridCell), (int64_t)floor(m[2] / kNpcGridCell))].push_back(cell);
			}

			ceilingColumnsOf = grid;
			ceilingColumnsCells = grid->Cells();
		}

		// the nearest ceiling over the place: of the cells in its column and the 8 round it, the ones whose middle is within a cell across and not below the place, at most
		// `reach` above it, the lowest (y is down: the greatest y); on a tie the nearest across. A place that is not a number is over nothing
		if (!(fabs((double)at[0]) < 1e12 && fabs((double)at[2]) < 1e12))
		{
			*why = "no ceiling within " + std::to_string((long long)reach) + " mm over it";
			return false;
		}

		int64_t const ci = (int64_t)floor((double)at[0] / kNpcGridCell), ck = (int64_t)floor((double)at[2] / kNpcGridCell);
		uint32_t best = kNpcGridNoCell;
		double bestY = -1e300, bestAcross = 1e300, bestMiddle[3] = { 0.0, 0.0, 0.0 };

		for (int64_t di = -1; di <= 1; di++)
		{
			for (int64_t dk = -1; dk <= 1; dk++)
			{
				auto const column = ceilingColumns.find(ColumnKey(ci + di, ck + dk));

				if (column == ceilingColumns.end())
				{
					continue;
				}

				for (uint32_t const cell : column->second)
				{
					double m[3];

					if (!grid->Middle(cell, m))
					{
						continue;
					}

					double const across = sqrt((m[0] - at[0]) * (m[0] - at[0]) + (m[2] - at[2]) * (m[2] - at[2])), up = (double)at[1] - m[1];

					if (across > kNpcGridCell || up < 0.0 || up > (double)reach)
					{
						continue;
					}

					if (m[1] > bestY || (m[1] == bestY && across < bestAcross))
					{
						best = cell, bestY = m[1], bestAcross = across;
						bestMiddle[0] = m[0], bestMiddle[1] = m[1], bestMiddle[2] = m[2];
					}
				}
			}
		}

		if (best == kNpcGridNoCell)
		{
			*why = "no ceiling within " + std::to_string((long long)reach) + " mm over it";
			return false;
		}

		found->place[0] = (int32_t)floor(bestMiddle[0] + 0.5);
		found->place[1] = (int32_t)floor(bestMiddle[1] + 0.5);
		found->place[2] = (int32_t)floor(bestMiddle[2] + 0.5);
		found->surface = AutoPlotPoint::Surface::Ceiling;
		found->facing[0] = 0;
		found->facing[1] = 65536;
		found->facing[2] = 0;
		found->module = ModuleOfMapPolygon(grid->PolygonOf(best));
		return true;
	}

	namespace
	{
		// A cell's neighbours: the 8 round it on its own sheet, then every cell it is linked to (another sheet's, across a join)
		void NeighboursOf(NpcGrid const &grid, uint32_t cell, std::vector<uint32_t> *out)
		{
			out->clear();

			int sheet = -1, i = 0, j = 0;

			if (grid.CellPlace(cell, &sheet, &i, &j))
			{
				for (int di = -1; di <= 1; di++)
				{
					for (int dj = -1; dj <= 1; dj++)
					{
						uint32_t const next = (di || dj) ? grid.CellAt(sheet, i + di, j + dj) : kNpcGridNoCell;

						if (next != kNpcGridNoCell)
						{
							out->push_back(next);
						}
					}
				}
			}

			size_t count = 0;
			uint32_t const *links = grid.LinksOf(cell, &count);

			for (size_t k = 0; links && k < count; k++)
			{
				out->push_back(links[k]);
			}
		}

		bool Clings(NpcGrid const &grid, uint32_t cell)
		{
			return (grid.Flags(cell) & kNpcGridClings) != 0;
		}

		double Apart(double const a[3], double const b[3])
		{
			return sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
		}

		AutoPlotPoint PointOf(NpcGrid const &grid, uint32_t cell, AutoPlotPoint::Surface surface, double const facing[3])
		{
			AutoPlotPoint p;
			double middle[3] = { 0, 0, 0 };

			grid.Middle(cell, middle);

			for (int k = 0; k < 3; k++)
			{
				p.place[k] = (int32_t)floor(middle[k] + 0.5);
				p.facing[k] = (int32_t)floor(facing[k] * 65536.0 + 0.5);
			}

			p.surface = surface;
			p.module = ModuleOfMapPolygon(grid.PolygonOf(cell));
			return p;
		}

		// A wall the floor reaches: its first cell, the floor cell beside it, what the floor costs to get there, and its facing (out of it, level)
		struct StationWall
		{
			uint32_t wall, floorCell;
			double cost;
			double facing[3];
			bool side;
		};
	}

	bool AutoPlotGrid_ClimbStation(int32_t const exitPoint[3], int32_t const through[3], int32_t most, std::vector<AutoPlotPoint> *way, int32_t top[3], std::string *why)
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

		double const tl = sqrt((double)through[0] * through[0] + (double)through[2] * through[2]);

		if (tl < 1.0 || most <= 0)
		{
			*why = "no station: no way through the doorway was given, or no reach";
			return false;
		}

		double const t[3] = { through[0] / tl, 0.0, through[2] / tl };
		double const exit[3] = { (double)exitPoint[0], (double)exitPoint[1], (double)exitPoint[2] };
		double const reach = (double)most;

		// the landing side: no further behind the exit point, along the way through, than the 100 it lies past the doorway
		auto const landingSide = [&](double const m[3]) { return (m[0] - exit[0]) * t[0] + (m[2] - exit[2]) * t[2] >= -100.0; };
		auto const crowFly = [&](double const m[3]) { return sqrt((m[0] - exit[0]) * (m[0] - exit[0]) + (m[2] - exit[2]) * (m[2] - exit[2])); };

		uint32_t const start = grid->NearestCell(exit[0], exit[1], exit[2], kNpcGridAlien, kNpcGridReach);

		if (start == kNpcGridNoCell)
		{
			*why = "no station: no cell an alien stands on within 1.5 m of the exit point";
			return false;
		}

		// over the floor, cheapest first (the distance between cells' middles), to every wall the landing side's floor reaches within `most`
		std::map<uint32_t, double> cost;
		std::map<uint32_t, uint32_t> cameFrom;
		std::map<int, StationWall> walls;   // the cheapest first cell of each wall sheet
		std::priority_queue<std::pair<double, uint32_t>, std::vector<std::pair<double, uint32_t> >, std::greater<std::pair<double, uint32_t> > > open;
		std::vector<uint32_t> next;
		size_t looked = 0;

		cost[start] = 0.0;
		open.push(std::make_pair(0.0, start));

		while (!open.empty() && looked < kAutoPlotMostLooks)
		{
			std::pair<double, uint32_t> const at = open.top();

			open.pop();

			if (at.first > cost[at.second])
			{
				continue;
			}

			looked++;

			double here[3];

			grid->Middle(at.second, here);
			NeighboursOf(*grid, at.second, &next);

			for (uint32_t const n : next)
			{
				double m[3];

				if (!Clings(*grid, n) || !grid->Middle(n, m) || !landingSide(m) || crowFly(m) > reach)
				{
					continue;
				}

				double const c = at.first + Apart(here, m);
				AlienWaysKind const kind = grid->KindOf(n);

				if (kind == kAlienWaysWall)
				{
					int const sheet = grid->SheetOf(n);
					auto const known = walls.find(sheet);

					if (known == walls.end() || c < known->second.cost)
					{
						StationWall w;
						double fx = here[0] - m[0], fz = here[2] - m[2];

						// the wall's facing: square to its own level line (a neighbour on its sheet at the same height), turned to the floor's side; the cells' offset
						// alone only when the sheet shows no level line
						int sheetOf = -1, ci = 0, cj = 0;

						if (grid->CellPlace(n, &sheetOf, &ci, &cj))
						{
							int const steps[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

							for (auto const &s : steps)
							{
								uint32_t const along = grid->CellAt(sheetOf, ci + s[0], cj + s[1]);
								double a[3];

								if (along != kNpcGridNoCell && grid->Middle(along, a) && fabs(a[1] - m[1]) < 50.0)
								{
									double const ax = a[0] - m[0], az = a[2] - m[2];
									double const side = (-az) * fx + ax * fz;   // which side of the line the floor is on

									fx = side >= 0.0 ? -az : az;
									fz = side >= 0.0 ? ax : -ax;
									break;
								}
							}
						}

						double const fl = sqrt(fx * fx + fz * fz);

						w.wall = n;
						w.floorCell = at.second;
						w.cost = c;
						w.facing[0] = fl > 0.0 ? fx / fl : 0.0;
						w.facing[1] = 0.0;
						w.facing[2] = fl > 0.0 ? fz / fl : 0.0;
						w.side = fabs(w.facing[0] * t[0] + w.facing[2] * t[2]) <= 0.5;   // within 60 degrees of square to the way through
						walls[sheet] = w;
					}
				}
				else if (kind == kAlienWaysFloor)
				{
					auto const known = cost.find(n);

					if (known == cost.end() || c < known->second)
					{
						cost[n] = c;
						cameFrom[n] = at.second;
						open.push(std::make_pair(c, n));
					}
				}
			}
		}

		if (walls.empty())
		{
			*why = "no station: no wall within " + std::to_string((long long)most) + " mm of the exit point, on the landing side";
			return false;
		}

		std::vector<StationWall> order;

		for (auto const &w : walls)
		{
			order.push_back(w.second);
		}

		std::sort(order.begin(), order.end(), [](StationWall const &a, StationWall const &b) { return a.side != b.side ? a.side : a.cost < b.cost; });

		std::string firstRefusal;
		double const upFacing[3] = { 0.0, -1.0, 0.0 }, downFacing[3] = { 0.0, 1.0, 0.0 };

		for (StationWall const &w : order)
		{
			// straight up the wall's own cells, every one an alien clings to, until a cell beside it is a ceiling's
			uint32_t cur = w.wall, ceiling = kNpcGridNoCell;
			std::string refused;

			for (int steps = 0; steps < kAutoPlotMostCeilingLook / (kNpcGridCell / 2) && ceiling == kNpcGridNoCell && refused.empty(); steps++)
			{
				double here[3];

				grid->Middle(cur, here);
				NeighboursOf(*grid, cur, &next);

				uint32_t up = kNpcGridNoCell;
				double upScore = 1e30, bestCeiling = 1e30;

				for (uint32_t const n : next)
				{
					double m[3];

					if (!grid->Middle(n, m))
					{
						continue;
					}

					AlienWaysKind const kind = grid->KindOf(n);

					if (kind == kAlienWaysCeiling && Clings(*grid, n) && landingSide(m) && m[1] < here[1] && Apart(here, m) < bestCeiling)
					{
						ceiling = n;
						bestCeiling = Apart(here, m);
					}
					else if (kind == kAlienWaysWall && grid->SheetOf(n) == grid->SheetOf(cur) && m[1] < here[1] - 100.0)
					{
						// straight up: the highest, but a step sideways costs twice what it rises (a diagonal step would let the climb drift along the wall)
						double const sideways = sqrt((m[0] - here[0]) * (m[0] - here[0]) + (m[2] - here[2]) * (m[2] - here[2]));
						double const score = m[1] + 2.0 * sideways;

						if (score < upScore)
						{
							up = n;
							upScore = score;
						}
					}
				}

				if (ceiling != kNpcGridNoCell)
				{
					break;
				}

				if (up == kNpcGridNoCell)
				{
					refused = "its wall stops with no ceiling at its top";
				}
				else if (!Clings(*grid, up))
				{
					refused = "a cell on the way up it can't cling to";
				}
				else
				{
					cur = up;
				}
			}

			if (ceiling == kNpcGridNoCell)
			{
				firstRefusal = firstRefusal.empty() ? (refused.empty() ? "no ceiling within reach above it" : refused) : firstRefusal;
				continue;
			}

			// the floor's way to the wall, landing cell first, kept where it turns
			std::vector<uint32_t> floorWay;

			for (uint32_t c = w.floorCell;; c = cameFrom[c])
			{
				floorWay.push_back(c);

				if (c == start || !cameFrom.count(c))
				{
					break;
				}
			}

			std::reverse(floorWay.begin(), floorWay.end());

			for (size_t i = 0; i < floorWay.size(); i++)
			{
				bool keep = i == 0 || i + 1 == floorWay.size();

				if (!keep)
				{
					double a[3], b[3], c[3];

					grid->Middle(floorWay[i - 1], a);
					grid->Middle(floorWay[i], b);
					grid->Middle(floorWay[i + 1], c);
					keep = fabs((b[0] - a[0]) * (c[2] - b[2]) - (b[2] - a[2]) * (c[0] - b[0])) > 1.0;   // the way turns here
				}

				if (keep)
				{
					way->push_back(PointOf(*grid, floorWay[i], AutoPlotPoint::Surface::Floor, upFacing));
				}
			}

			way->push_back(PointOf(*grid, w.wall, AutoPlotPoint::Surface::Wall, w.facing));

			if (cur != w.wall)
			{
				way->push_back(PointOf(*grid, cur, AutoPlotPoint::Surface::Wall, w.facing));
			}

			way->push_back(PointOf(*grid, ceiling, AutoPlotPoint::Surface::Ceiling, downFacing));

			for (int k = 0; k < 3; k++)
			{
				top[k] = way->back().place[k];
			}

			return true;
		}

		*why = "no station: no wall within " + std::to_string((long long)most) + " mm reaches a ceiling (" + std::to_string((unsigned long long)order.size()) + " tried; the first: " +
			firstRefusal + ")";
		return false;
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
