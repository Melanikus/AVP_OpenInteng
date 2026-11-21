// autoplot_levels.cpp: see autoplot_levels.h. Written 2026-11-20 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autoplot_levels.h"
#include "autoplot_volumes.h"
#include "waypoint_model.h"

#include <math.h>

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace host
{
	namespace
	{
		struct Cell
		{
			int ix = 0, iz = 0;
			double y = 0;      // its floor's height (y down)
			int patch = -1;    // its flat patch
			int unit = -1;     // the floor box it is in (a level's or a stair's), -1 for none (a fitting's, or left over)
		};

		int const kDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

		// the biggest rectangle of a patch's free cells (in the patch, in no level yet): w x h cells from ix0, iz0; false when none is least x least or more
		bool BiggestRectangle(std::set<std::pair<int, int> > const &free, int least, int *ix0, int *iz0, int *w, int *h)
		{
			if (free.empty())
			{
				return false;
			}

			int x0 = free.begin()->first, x1 = x0, z0 = free.begin()->second, z1 = z0;

			for (std::pair<int, int> const &c : free)
			{
				x0 = c.first < x0 ? c.first : x0;
				x1 = c.first > x1 ? c.first : x1;
				z0 = c.second < z0 ? c.second : z0;
				z1 = c.second > z1 ? c.second : z1;
			}

			int const width = x1 - x0 + 1;
			std::vector<int> run((size_t)width, 0);   // free cells straight above each column, ending at this row
			long long best = 0;

			for (int iz = z0; iz <= z1; iz++)
			{
				for (int i = 0; i < width; i++)
				{
					run[(size_t)i] = free.count(std::make_pair(x0 + i, iz)) ? run[(size_t)i] + 1 : 0;
				}

				// every span of columns: its lowest run is the rectangle's height (patches are small: the plain way)
				for (int a = 0; a < width; a++)
				{
					int lowest = run[(size_t)a];

					for (int b = a; b < width && lowest > 0; b++)
					{
						lowest = run[(size_t)b] < lowest ? run[(size_t)b] : lowest;

						long long const area = (long long)(b - a + 1) * lowest;

						if (lowest >= least && b - a + 1 >= least && area > best)
						{
							best = area;
							*ix0 = x0 + a;
							*iz0 = iz - lowest + 1;
							*w = b - a + 1;
							*h = lowest;
						}
					}
				}
			}

			return best > 0;
		}
	}

	AutoPlotLevelPlan AutoPlot_PlanLevels(AutoPlotFloor const &floor, std::vector<std::vector<int32_t> > const &exits, std::vector<AutoPlotDoor> const &doors)
	{
		(void)doors;   // the volume planner's EXIT rule takes the doorways; here only the exit points

		AutoPlotLevelPlan plan;

		if (floor.cells.empty() || floor.cellEdge <= 0)
		{
			plan.notes.push_back("NOT SHAPED: no floor cells (no network written)");
			return plan;
		}

		if (floor.cells.size() > kAutoPlotLevelMostCells)
		{
			plan.notes.push_back("NOT SHAPED: more than " + std::to_string((long long)kAutoPlotLevelMostCells) + " floor cells");
			return plan;
		}

		double const e = (double)floor.cellEdge;
		double const ox = (double)floor.cells[0][0], oz = (double)floor.cells[0][2];
		std::vector<Cell> cells;
		std::map<std::pair<int, int>, std::vector<int> > at;   // the cells over each place (storeys: several)

		for (std::vector<int32_t> const &c : floor.cells)
		{
			if (c.size() != 3)
			{
				continue;
			}

			Cell k;

			k.ix = (int)lround((c[0] - ox) / e);
			k.iz = (int)lround((c[2] - oz) / e);
			k.y = (double)c[1];

			bool twice = false;

			for (int const j : at[std::make_pair(k.ix, k.iz)])
			{
				twice = twice || fabs(cells[(size_t)j].y - k.y) < 1.0;   // one cell listed twice
			}

			if (!twice)
			{
				at[std::make_pair(k.ix, k.iz)].push_back((int)cells.size());
				cells.push_back(k);
			}
		}

		// each cell's neighbours (4 ways, any height): fn(j)
		auto const neighbours = [&](int i, auto fn) {
			for (int const *d : kDirs)
			{
				auto const found = at.find(std::make_pair(cells[(size_t)i].ix + d[0], cells[(size_t)i].iz + d[1]));

				if (found != at.end())
				{
					for (int const j : found->second)
					{
						fn(j);
					}
				}
			}
		};

		// 1. THE FLAT PATCHES
		int patches = 0;

		for (size_t s = 0; s < cells.size(); s++)
		{
			if (cells[s].patch >= 0)
			{
				continue;
			}

			std::vector<int> todo(1, (int)s);

			cells[s].patch = patches;

			while (!todo.empty())
			{
				int const n = todo.back();

				todo.pop_back();
				neighbours(n, [&](int j) {
					if (cells[(size_t)j].patch < 0 && fabs(cells[(size_t)j].y - cells[(size_t)n].y) <= kAutoPlotLevelFlat)
					{
						cells[(size_t)j].patch = patches;
						todo.push_back(j);
					}
				});
			}

			patches++;
		}

		// 2. THE LEVELS: each patch's biggest rectangles of 2 x 2 cells or more
		struct Unit
		{
			bool stair = false;
			int patch = -1;                      // a level's flat patch (its room)
			double lo[3], hi[3];
			double floorTop = 0, floorLow = 0;   // its floor's highest (smallest y) and lowest
			std::vector<int> cells;
		};

		std::vector<Unit> units;
		size_t leftOver = 0;
		int const least = std::max(1, (int)ceil(kAutoPlotLevelMinSide / e));
		int treads = 0;

		for (int p = 0; p < patches; p++)
		{
			std::set<std::pair<int, int> > free;
			std::map<std::pair<int, int>, int> cellOf;

			for (size_t i = 0; i < cells.size(); i++)
			{
				if (cells[i].patch == p)
				{
					free.insert(std::make_pair(cells[i].ix, cells[i].iz));
					cellOf[std::make_pair(cells[i].ix, cells[i].iz)] = (int)i;
				}
			}

			int ix0 = 0, iz0 = 0, w = 0, h = 0;

			while (BiggestRectangle(free, least, &ix0, &iz0, &w, &h))
			{
				if (units.size() >= kAutoPlotLevelsMost)
				{
					leftOver += free.size();
					break;
				}

				// a TREAD-sized rectangle (under kAutoPlotLevelRoomSide either way; never on a crawl floor): its cells are left for the stairs
				if (!floor.crawl && (w * e < kAutoPlotLevelRoomSide || h * e < kAutoPlotLevelRoomSide))
				{
					for (int ix = ix0; ix < ix0 + w; ix++)
					{
						for (int iz = iz0; iz < iz0 + h; iz++)
						{
							free.erase(std::make_pair(ix, iz));
						}
					}

					treads++;
					continue;
				}

				Unit u;

				u.floorTop = 1e300, u.floorLow = -1e300;
				u.patch = p;

				for (int ix = ix0; ix < ix0 + w; ix++)
				{
					for (int iz = iz0; iz < iz0 + h; iz++)
					{
						int const i = cellOf[std::make_pair(ix, iz)];

						free.erase(std::make_pair(ix, iz));
						cells[(size_t)i].unit = (int)units.size();
						u.cells.push_back(i);
						u.floorTop = cells[(size_t)i].y < u.floorTop ? cells[(size_t)i].y : u.floorTop;
						u.floorLow = cells[(size_t)i].y > u.floorLow ? cells[(size_t)i].y : u.floorLow;
					}
				}

				u.lo[0] = ox + (ix0 - 0.5) * e, u.hi[0] = ox + (ix0 + w - 0.5) * e;
				u.lo[2] = oz + (iz0 - 0.5) * e, u.hi[2] = oz + (iz0 + h - 0.5) * e;
				units.push_back(u);
			}
		}

		size_t const levels = units.size();

		// 3. THE STAIRS AND THE FITTINGS: the cells in no level, joined where they step at most kAutoPlotLevelStep
		std::vector<int> group(cells.size(), -1);
		int groups = 0, stairs = 0, fittings = 0;

		for (size_t s = 0; s < cells.size(); s++)
		{
			if (cells[s].unit >= 0 || group[s] >= 0)
			{
				continue;
			}

			std::vector<int> members, todo(1, (int)s);
			std::set<int> metLevels;

			group[s] = groups;

			while (!todo.empty())
			{
				int const n = todo.back();

				todo.pop_back();
				members.push_back(n);
				neighbours(n, [&](int j) {
					if (fabs(cells[(size_t)j].y - cells[(size_t)n].y) > kAutoPlotLevelStep)
					{
						return;
					}

					if (cells[(size_t)j].unit >= 0 && (size_t)cells[(size_t)j].unit < levels)
					{
						metLevels.insert(cells[(size_t)j].unit);
					}
					else if (cells[(size_t)j].unit < 0 && group[(size_t)j] < 0)
					{
						group[(size_t)j] = groups;
						todo.push_back(j);
					}
				});
			}

			groups++;

			double top = 1e300, low = -1e300;

			for (int const m : members)
			{
				top = cells[(size_t)m].y < top ? cells[(size_t)m].y : top;
				low = cells[(size_t)m].y > low ? cells[(size_t)m].y : low;
			}

			// the heights of the levels it meets: two apart (more than a tread) make it a stair between them
			bool twoHeights = false;

			for (int const a : metLevels)
			{
				for (int const b : metLevels)
				{
					twoHeights = twoHeights || fabs(units[(size_t)a].floorTop - units[(size_t)b].floorTop) > kAutoPlotLevelStep;
				}
			}

			if (!twoHeights && low - top <= kAutoPlotLevelRiseAlone)
			{
				fittings++;
				continue;   // a fitting (or a scrap of floor): no box
			}

			Unit u;

			u.stair = true;
			u.floorTop = top, u.floorLow = low;
			u.lo[0] = u.lo[2] = 1e300, u.hi[0] = u.hi[2] = -1e300;

			for (int const m : members)
			{
				Cell &c = cells[(size_t)m];

				c.unit = (int)units.size();
				u.cells.push_back(m);
				u.lo[0] = ox + (c.ix - 0.5) * e < u.lo[0] ? ox + (c.ix - 0.5) * e : u.lo[0];
				u.hi[0] = ox + (c.ix + 0.5) * e > u.hi[0] ? ox + (c.ix + 0.5) * e : u.hi[0];
				u.lo[2] = oz + (c.iz - 0.5) * e < u.lo[2] ? oz + (c.iz - 0.5) * e : u.lo[2];
				u.hi[2] = oz + (c.iz + 0.5) * e > u.hi[2] ? oz + (c.iz + 0.5) * e : u.hi[2];
			}

			units.push_back(u);
			stairs++;
		}

		for (Unit &u : units)
		{
			u.lo[1] = u.floorTop - kAutoPlotLevelReach;
			u.hi[1] = u.floorLow + kAutoPlotLevelInto;
		}

		// 4. THE LINKS between floor boxes whose cells are neighbours: a step, or a riser (the lower box reaching up to the upper)
		std::map<std::pair<int, int>, double> nearest;   // each pair of units next to each other: the smallest height step between neighbouring cells

		for (size_t i = 0; i < cells.size(); i++)
		{
			if (cells[i].unit < 0)
			{
				continue;
			}

			neighbours((int)i, [&](int j) {
				int const a = cells[i].unit, b = cells[(size_t)j].unit;

				if (b < 0 || a == b)
				{
					return;
				}

				std::pair<int, int> const key = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
				double const step = fabs(cells[i].y - cells[(size_t)j].y);
				auto const found = nearest.find(key);

				nearest[key] = found == nearest.end() || step < found->second ? step : found->second;
			});
		}

		std::vector<std::pair<int, int> > unitLinks;
		int risers = 0;

		for (auto const &n : nearest)
		{
			if (n.second > kAutoPlotLevelRiserMost)
			{
				continue;
			}

			if (n.second > kAutoPlotLevelStep)
			{
				// a riser: the lower box reaches up to the upper's floor box, so the two overlap
				Unit &a = units[(size_t)n.first.first], &b = units[(size_t)n.first.second];
				Unit &lower = a.floorTop > b.floorTop ? a : b, &upper = a.floorTop > b.floorTop ? b : a;

				lower.lo[1] = upper.hi[1] - 1.0 < lower.lo[1] ? upper.hi[1] - 1.0 : lower.lo[1];
				risers++;
			}

			unitLinks.push_back(n.first);
		}

		// 5. THE EXITS: each on the floor box under its point
		std::vector<int> exitUnit(exits.size(), -1);

		for (size_t x = 0; x < exits.size(); x++)
		{
			if (exits[x].size() != 3)
			{
				continue;
			}

			double best = 1e300;

			for (size_t i = 0; i < cells.size(); i++)
			{
				if (cells[i].unit < 0 || fabs(cells[i].y - exits[x][1]) > kAutoPlotLevelExitReach)
				{
					continue;
				}

				double const dx = ox + cells[i].ix * e - exits[x][0], dz = oz + cells[i].iz * e - exits[x][2];

				if (dx * dx + dz * dz < best)
				{
					best = dx * dx + dz * dz;
					exitUnit[x] = cells[i].unit;
				}
			}
		}

		// 6. THE STOCK DROPOUT: a floor box no exit reaches is left out
		std::vector<bool> reached(units.size(), false);
		std::vector<int> todo;

		for (int const u : exitUnit)
		{
			if (u >= 0 && !reached[(size_t)u])
			{
				reached[(size_t)u] = true;
				todo.push_back(u);
			}
		}

		while (!todo.empty())
		{
			int const n = todo.back();

			todo.pop_back();

			for (std::pair<int, int> const &l : unitLinks)
			{
				int const other = l.first == n ? l.second : l.second == n ? l.first : -1;

				if (other >= 0 && !reached[(size_t)other])
				{
					reached[(size_t)other] = true;
					todo.push_back(other);
				}
			}
		}

		std::vector<int> boxOf(units.size(), -1);
		size_t dropped = 0;

		for (size_t u = 0; u < units.size(); u++)
		{
			if (!reached[u] && !exits.empty())
			{
				dropped++;
				continue;
			}

			AutoPlotLevelBox b;

			b.kind = kAutoPlotLevelFloor;

			for (int a = 0; a < 3; a++)
			{
				b.lo[a] = units[u].lo[a];
				b.hi[a] = units[u].hi[a];
			}

			// its centre over its floor's middle (the nearest cell to the box's middle, 750 over it), then the box's quarters
			double const mx = (b.lo[0] + b.hi[0]) / 2.0, mz = (b.lo[2] + b.hi[2]) / 2.0;
			int mid = units[u].cells.front();
			double best = 1e300;

			for (int const c : units[u].cells)
			{
				double const dx = ox + cells[(size_t)c].ix * e - mx, dz = oz + cells[(size_t)c].iz * e - mz;

				if (dx * dx + dz * dz < best)
				{
					best = dx * dx + dz * dz;
					mid = c;
				}
			}

			b.centres.push_back(std::vector<double>{ ox + cells[(size_t)mid].ix * e, cells[(size_t)mid].y - kAutoPlotLevelReach / 2.0, oz + cells[(size_t)mid].iz * e });
			boxOf[u] = (int)plan.boxes.size();
			plan.boxes.push_back(b);
		}

		for (std::pair<int, int> const &l : unitLinks)
		{
			if (boxOf[(size_t)l.first] >= 0 && boxOf[(size_t)l.second] >= 0)
			{
				AutoPlotLevelLink k;

				k.from = boxOf[(size_t)l.first];
				k.to = boxOf[(size_t)l.second];
				k.flagsAtFrom = kLinkAlien;
				k.flagsAtTo = kLinkAlien;
				plan.links.push_back(k);
			}
		}

		size_t exitsPlaced = 0;

		for (size_t x = 0; x < exits.size(); x++)
		{
			if (exitUnit[x] < 0 || boxOf[(size_t)exitUnit[x]] < 0)
			{
				plan.notes.push_back("exit " + std::to_string((long long)x) + ": no floor cell under it within " + std::to_string((long long)kAutoPlotLevelExitReach) + " mm of its height");
				continue;
			}

			// a stand-in round the point (the volume planner's EXIT rule makes the real box)
			AutoPlotLevelBox b;

			b.kind = kAutoPlotLevelExit;
			b.exit = (int)x;
			b.floor = boxOf[(size_t)exitUnit[x]];
			b.lo[0] = exits[x][0] - 600.0, b.hi[0] = exits[x][0] + 600.0;
			b.lo[2] = exits[x][2] - 600.0, b.hi[2] = exits[x][2] + 600.0;
			b.lo[1] = exits[x][1] - 1000.0, b.hi[1] = exits[x][1] + kAutoPlotLevelInto;
			b.centres.push_back(std::vector<double>{ (double)exits[x][0], (double)exits[x][1], (double)exits[x][2] });

			AutoPlotLevelLink k;

			k.from = (int)plan.boxes.size();
			k.to = b.floor;
			k.flagsAtFrom = kLinkAlien;
			k.flagsAtTo = kLinkAlien;
			plan.links.push_back(k);
			plan.boxes.push_back(b);
			exitsPlaced++;
		}

		// THE LEVEL ROOMS: each level's patch one room, its floor boxes and the exits standing on them
		std::map<int, int> roomOfPatch, roomOfBox;

		for (size_t u = 0; u < units.size(); u++)
		{
			if (units[u].stair || boxOf[u] < 0)
			{
				continue;
			}

			auto const found = roomOfPatch.find(units[u].patch);
			int const r = found != roomOfPatch.end() ? found->second : (int)plan.rooms.size();

			if (found == roomOfPatch.end())
			{
				roomOfPatch[units[u].patch] = r;
				plan.rooms.push_back(AutoPlotLevelRoom());
				plan.rooms.back().floorY = units[u].floorTop;
			}

			plan.rooms[(size_t)r].floors.push_back(boxOf[u]);
			roomOfBox[boxOf[u]] = r;
		}

		for (AutoPlotLevelBox const &b : plan.boxes)
		{
			auto const r = b.kind == kAutoPlotLevelExit ? roomOfBox.find(b.floor) : roomOfBox.end();

			if (r != roomOfBox.end())
			{
				plan.rooms[(size_t)r->second].exits.push_back(b.exit);
			}
			else if (b.kind == kAutoPlotLevelExit)
			{
				plan.notes.push_back("exit " + std::to_string((long long)b.exit) + " stands on a stair: in no room");
			}
		}

		size_t const floors = plan.boxes.size() - exitsPlaced;

		plan.shaped = floors > 0;
		plan.notes.insert(plan.notes.begin(), std::string(plan.shaped ? "THE LEVELS" : "NOT SHAPED: THE LEVELS") + ": " + std::to_string((long long)levels) + " level(s), " +
			std::to_string((long long)stairs) + " stair(s), " + std::to_string((long long)fittings) + " fitting(s) left out, " + std::to_string((long long)risers) + " riser(s) climbed, " +
			std::to_string((long long)dropped) + " floor(s) no exit reaches left out, " + std::to_string((long long)leftOver) + " cell(s) past " +
			std::to_string((long long)kAutoPlotLevelsMost) + " levels, " + std::to_string((long long)exitsPlaced) + " exit(s); no climb-in or entry yet (the floor walks these)");
		return plan;
	}

	namespace
	{
		// the volume planner's own numbers (autoplot_volumes.cpp Held, File, GameHolds, SetLink: copied so the two give the same file)
		double HeldPlace(double v)
		{
			return v > kAutoPlotMostPlace ? (double)kAutoPlotMostPlace : (v < -kAutoPlotMostPlace ? -(double)kAutoPlotMostPlace : v);
		}

		int32_t FilePlace(double world, double scale)
		{
			double const v = floor(world / scale);

			return v > 2147483000.0 ? 2147483000 : (v < -2147483000.0 ? -2147483000 : (int32_t)v);
		}

		// the game's own test on the file's numbers (the loader takes 1 off each max: inside is min <= p <= max - 1)
		bool GameHoldsPlace(WaypointRec const &f, int32_t const at[3], double scale)
		{
			for (int a = 0; a < 3; a++)
			{
				double const p = trunc((double)at[a] * scale);

				if (p < trunc((double)f.min[a] * scale) || p > trunc((double)f.max[a] * scale) - 1.0)
				{
					return false;
				}
			}

			return true;
		}

		void LinkBoth(std::vector<WaypointRec> *list, size_t a, size_t b, int32_t flagsAtA, int32_t flagsAtB)
		{
			for (WaypointLinkRec const &l : (*list)[a].links)
			{
				if (l.index == (int32_t)b)
				{
					return;   // one link a pair
				}
			}

			WaypointLinkRec there, back;

			there.index = (int32_t)b;
			there.flags = flagsAtA;
			back.index = (int32_t)a;
			back.flags = flagsAtB;
			(*list)[a].links.push_back(there);
			(*list)[b].links.push_back(back);
		}
	}

	AutoPlotPlan AutoPlot_LevelsVolumes(AutoPlotFloor const &floorIn, std::vector<std::vector<int32_t> > const &exits, std::vector<AutoPlotDoor> const &doors, double levelScale,
		std::string const &kind)
	{
		AutoPlotPlan plan;
		double const scale = levelScale > 0.0 ? levelScale : 1.0;

		plan.exits = exits.size();
		plan.doorBox.assign(exits.size(), -1);
		plan.exitBox.assign(exits.size(), -1);
		plan.doorWords.assign(exits.size(), std::string());
		plan.volumeKind = kind;
		plan.cells = floorIn.cells.size();

		AutoPlotLevelPlan const lv = AutoPlot_PlanLevels(floorIn, exits, doors);

		plan.notes = lv.notes;

		if (!lv.shaped)
		{
			plan.notes.insert(plan.notes.begin(), "NOT SHAPED: a " + kind + " the levels' rule could not shape (no network written)");
			return plan;
		}

		std::vector<AutoPlotLevelBox> boxes = lv.boxes;

		// EACH EXIT BY THE VOLUME PLANNER'S EXIT RULE (autoplot_volumes.cpp, Dietrich's: the doorway passage, 600 inside to 1200 past its middle, 600 each side, 200 under
		// the floor to more than 1000 under the soffit; its centre 1000 past the middle at mid-door height, then the exit point): the levels' stand-in replaced
		for (AutoPlotLevelBox &b : boxes)
		{
			if (b.kind != kAutoPlotLevelExit || b.exit < 0 || (size_t)b.exit >= exits.size() || exits[(size_t)b.exit].size() != 3)
			{
				continue;
			}

			size_t const x = (size_t)b.exit;
			double const p[3] = { (double)exits[x][0], (double)exits[x][1], (double)exits[x][2] };
			bool const door = x < doors.size() && doors[x].throughFound;
			double floorY = p[1] + kAutoPlotHangClear, bestD = 1e300;

			// the floor under the point: the nearest cell's (within the levels' reach of its height: a storey's, not the one over it)
			for (std::vector<int32_t> const &c : floorIn.cells)
			{
				if (c.size() != 3)
				{
					continue;
				}

				double const dx = c[0] - p[0], dz = c[2] - p[2];

				if (fabs(c[1] - p[1]) <= kAutoPlotLevelExitReach && dx * dx + dz * dz < bestD)
				{
					bestD = dx * dx + dz * dz;
					floorY = c[1];
				}
			}

			b.centres.clear();
			b.hi[1] = std::max(floorY, p[1]) + kAutoPlotInto;

			if (door)
			{
				AutoPlotDoor const &d = doors[x];
				double const tx = d.through[0], tz = d.through[1];

				b.lo[0] = b.lo[2] = 1e300;
				b.hi[0] = b.hi[2] = -1e300;

				for (double const along : { -(double)kAutoPlotVolumePast, (double)kAutoPlotVolumeExitPast })
				{
					for (double const side : { -(double)kAutoPlotVolumeSide, (double)kAutoPlotVolumeSide })
					{
						double const px = d.middle[0] + tx * along - tz * side, pz = d.middle[2] + tz * along + tx * side;

						b.lo[0] = std::min(b.lo[0], px);
						b.hi[0] = std::max(b.hi[0], px);
						b.lo[2] = std::min(b.lo[2], pz);
						b.hi[2] = std::max(b.hi[2], pz);
					}
				}

				double const soffit = d.soffitFound ? (double)d.soffit[1] : p[1] - kAutoPlotVolumeTopClear - 1.0;

				b.lo[1] = soffit + kAutoPlotVolumeTopClear;

				if (b.lo[1] > p[1] - 1.0)
				{
					b.lo[1] = p[1] - 1.0;   // the doorway is too low for a pure target: the point kept in its box
				}

				double const midDoor = std::min(std::max((soffit + floorY) / 2.0, b.lo[1] + 1.0), b.hi[1] - 2.0);

				b.centres.push_back(std::vector<double>{ d.middle[0] + tx * kAutoPlotAimPast, midDoor, d.middle[2] + tz * kAutoPlotAimPast });
				b.centres.push_back(std::vector<double>{ p[0], p[1], p[2] });
			}
			else
			{
				b.lo[0] = p[0] - kAutoPlotVolumeSide;
				b.hi[0] = p[0] + kAutoPlotVolumeSide;
				b.lo[2] = p[2] - kAutoPlotVolumeSide;
				b.hi[2] = p[2] + kAutoPlotVolumeSide;
				b.lo[1] = p[1] - kAutoPlotHangClear;
				b.centres.push_back(std::vector<double>{ p[0], p[1], p[2] });
			}
		}

		// in the file's numbers, as the volume planner writes them; each centre the first candidate held by its own box and no other (else the first, counted)
		for (AutoPlotLevelBox const &b : boxes)
		{
			WaypointRec wp;

			for (int a = 0; a < 3; a++)
			{
				wp.min[a] = FilePlace(HeldPlace(b.lo[a]), scale);
				wp.max[a] = std::max(std::min(FilePlace(HeldPlace(b.hi[a]), scale) + 1, kAutoPlotMostPlace), wp.min[a] + 2);
				wp.centre[a] = 0;
			}

			wp.flags = 0;
			wp.spare2 = 0;
			plan.list.push_back(wp);
			plan.climbing.push_back(false);
		}

		for (size_t i = 0; i < boxes.size(); i++)
		{
			bool chosen = false;
			int32_t first[3] = { 0, 0, 0 };

			for (size_t k = 0; k < boxes[i].centres.size() && !chosen; k++)
			{
				int32_t at[3];

				for (int a = 0; a < 3; a++)
				{
					at[a] = std::min(std::max(FilePlace(HeldPlace(boxes[i].centres[k][(size_t)a]), scale), plan.list[i].min[a]), plan.list[i].max[a] - 1);
					first[a] = k == 0 ? at[a] : first[a];
				}

				bool other = false;

				for (size_t j = 0; j < boxes.size() && !other; j++)
				{
					other = j != i && GameHoldsPlace(plan.list[j], at, scale);
				}

				if (GameHoldsPlace(plan.list[i], at, scale) && !other)
				{
					for (int a = 0; a < 3; a++)
					{
						plan.list[i].centre[a] = at[a];
					}

					chosen = true;
				}
			}

			if (!chosen)
			{
				for (int a = 0; a < 3; a++)
				{
					plan.list[i].centre[a] = first[a];
				}

				plan.centresShared++;
			}
		}

		for (AutoPlotLevelLink const &l : lv.links)
		{
			if (l.from >= 0 && l.to >= 0 && (size_t)l.from < plan.list.size() && (size_t)l.to < plan.list.size())
			{
				LinkBoth(&plan.list, (size_t)l.from, (size_t)l.to, l.flagsAtFrom, l.flagsAtTo);
			}
		}

		// the words: each exit's box and why no lane; the parts; the cells inside a floor box
		size_t floors = 0;

		for (size_t i = 0; i < boxes.size(); i++)
		{
			floors += boxes[i].kind == kAutoPlotLevelFloor ? 1 : 0;

			if (boxes[i].kind == kAutoPlotLevelExit && boxes[i].exit >= 0 && (size_t)boxes[i].exit < exits.size())
			{
				plan.exitBox[(size_t)boxes[i].exit] = (int32_t)i;
			}
		}

		for (size_t x = 0; x < exits.size(); x++)
		{
			plan.doorWords[x] = "no lane: a " + kind + " (the levels' rule: the floor walks it, no ceiling lane yet)";

			if (exits[x].size() == 3)
			{
				int32_t const at[3] = { FilePlace(exits[x][0], scale), FilePlace(exits[x][1], scale), FilePlace(exits[x][2], scale) };
				bool held = false;

				for (WaypointRec const &wp : plan.list)
				{
					held = held || GameHoldsPlace(wp, at, scale);
				}

				plan.exitsCovered += held ? 1 : 0;
			}
		}

		std::vector<int> part(plan.list.size(), -1);

		for (size_t s = 0; s < plan.list.size(); s++)
		{
			if (part[s] >= 0)
			{
				continue;
			}

			std::vector<size_t> todo(1, s);

			part[s] = (int)plan.parts;

			while (!todo.empty())
			{
				size_t const n = todo.back();

				todo.pop_back();

				for (WaypointLinkRec const &l : plan.list[n].links)
				{
					if (part[(size_t)l.index] < 0)
					{
						part[(size_t)l.index] = (int)plan.parts;
						todo.push_back((size_t)l.index);
					}
				}
			}

			plan.parts++;
		}

		for (std::vector<int32_t> const &c : floorIn.cells)
		{
			if (c.size() != 3)
			{
				continue;
			}

			int32_t const at[3] = { FilePlace(c[0], scale), FilePlace(c[1] - 10.0, scale), FilePlace(c[2], scale) };
			bool in = false;

			for (size_t i = 0; i < boxes.size() && !in; i++)
			{
				in = boxes[i].kind == kAutoPlotLevelFloor && GameHoldsPlace(plan.list[i], at, scale);
			}

			plan.cellsCovered += in ? 1 : 0;
		}

		plan.floorBoxes = floors;
		plan.floorTile = (double)floorIn.cellEdge;

		// NOT ONE NETWORK (opscen03 with no cell on its treads, Frost 11-20): floors with no cells between them aren't joined, and a network of parts fails the checker
		// ("box 0 cannot reach box 1"), so none is written, and the note says why
		if (plan.parts > 1)
		{
			plan.notes.insert(plan.notes.begin(), "NOT SHAPED: a " + kind + " whose floors are not joined (" + std::to_string((long long)plan.parts) +
				" parts: no cells between them; no network written)");
			plan.list.clear();
			plan.climbing.clear();
			return plan;
		}

		plan.volumes = true;
		plan.notes.push_back("THE LEVELS AS VOLUMES (autoplot_levels.h): " + std::to_string((long long)floors) + " floor, " + std::to_string((long long)(boxes.size() - floors)) +
			" exit volumes, as a " + kind);
		return plan;
	}
}
