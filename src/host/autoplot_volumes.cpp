// See autoplot_volumes.h. Written 2026-11-20 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_volumes.h"
#include "autoplot_closers.h"  // the gap closers on the walls' band (Vasquez 11-21)
#include "autoplot_levels.h"   // the levels' rule: stairs or levels, and ducts (Frost 11-20)
#include "autoplot_rooms.h"    // the rooms' wall test (Drake 11-20)
#include "autoplot_stairs.h"   // the stair between two floors (Dietrich 11-21)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "waypoint_model.h"

namespace host
{
	namespace
	{
		enum class Kind
		{
			Floor,
			Climb,
			GetOut,
			Entry,
			Low,       // a sloped side's climb over its lower face (it stands for the climb-in)
			High,      // a sloped side's upper face, one piece an entry over it
			Exit,
			Closer,    // a gap closer on a wall's band, between the floor box and the entries (autoplot_closers.h; Vasquez 11-21)
			StairCeiling,   // the ceiling over a stair's treads, between the two floors' entries (autoplot_stairs.h; Dietrich 11-21). Never an Entry: it has no exit of its own
		};

		struct Vol
		{
			Kind kind;
			double lo[3], hi[3];                       // the world's mm, y down
			std::vector<std::vector<double> > centres; // the centre's candidates, the first preferred
			int exit = -1;                             // an entry's or an exit box's exit
			int floor = -1;                            // a climb-in's floor rectangle
		};

		// a floor rectangle of cells: its cell range, its floor's highest and lowest (y down), and its ceiling's (when any place over it has one)
		struct Rect
		{
			int ix0, ix1, iz0, iz1;
			double floorTop, floorLow;
			bool ceiled = false;
			double ceilTop = 0.0, ceilLow = 0.0;
		};

		struct Wall
		{
			int axis;      // 0 x, 2 z: the axis the surface faces along
			int sign;      // +1: the room on its + side
			double plane;  // the surface on that axis
			double along;  // where along the other axis the wall step lay
		};

		double Held(double v)
		{
			return v > kAutoPlotMostPlace ? (double)kAutoPlotMostPlace : (v < -kAutoPlotMostPlace ? -(double)kAutoPlotMostPlace : v);
		}

		int32_t File(double world, double scale)
		{
			double const v = floor(world / scale);

			return v > 2147483000.0 ? 2147483000 : (v < -2147483000.0 ? -2147483000 : (int32_t)v);
		}

		// the game's own test on the file's numbers (the loader takes 1 off each max: inside is min <= p <= max - 1)
		bool GameHolds(WaypointRec const &f, int32_t const at[3], double scale)
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

		bool Touch(Vol const &a, Vol const &b, double slack)
		{
			for (int i = 0; i < 3; i++)
			{
				if (a.lo[i] > b.hi[i] + slack || b.lo[i] > a.hi[i] + slack)
				{
					return false;
				}
			}

			return true;
		}

		double Gap(Vol const &a, Vol const &b)
		{
			double s = 0.0;

			for (int i = 0; i < 3; i++)
			{
				double const d = std::max(0.0, std::max(a.lo[i] - b.hi[i], b.lo[i] - a.hi[i]));

				s += d * d;
			}

			return sqrt(s);
		}

		// THE FLOOR IN THE FEWEST BIG RECTANGLES: the cells on their grid, the biggest all-cell rectangle (a row's histogram, each row's heights), taken again and again over
		// what is left, while one is at least 4 cells; a rectangle whose floor spreads more than kAutoPlotFloorRise (a ramp, stairs) is cut in two along its long side
		std::vector<Rect> Rectangles(std::map<std::pair<int, int>, double> const &cells, int ix0, int ix1, int iz0, int iz1, double edge)
		{
			std::vector<Rect> out;
			int const w = ix1 - ix0 + 1, h = iz1 - iz0 + 1;

			if ((long long)w * h > 4000000LL)
			{
				return out;   // cells too far apart for one grid: the old planner
			}

			// THE FLOOR'S LEVELS: FLAT PATCHES, cells joined where two side by side differ by at most kAutoPlotVolumeFlatStep (Frost's levels rule, 11-20: at a step of 450 a
			// tube's 45-degree face, its cells 300 apart and 300 higher each, joined the floor: the owner's tubes' bands took in 628 mm of slope and read as stairs, Drake's gold
			// compare); each level's rectangles apart, the biggest level first (a ledge's cells never in a rectangle with the floor under it). A gentle ramp still joins up
			std::map<std::pair<int, int>, int> levelOf;
			std::vector<size_t> levelSize;

			for (auto const &c : cells)
			{
				if (levelOf.count(c.first))
				{
					continue;
				}

				int const id = (int)levelSize.size();
				std::vector<std::pair<int, int> > todo(1, c.first);

				levelOf[c.first] = id;
				levelSize.push_back(1);

				while (!todo.empty())
				{
					std::pair<int, int> const at = todo.back();

					todo.pop_back();

					for (std::pair<int, int> const step : { std::make_pair(1, 0), std::make_pair(-1, 0), std::make_pair(0, 1), std::make_pair(0, -1) })
					{
						std::pair<int, int> const next(at.first + step.first, at.second + step.second);
						auto const there = cells.find(next);

						if (there != cells.end() && !levelOf.count(next) && fabs(there->second - cells.at(at)) <= kAutoPlotVolumeFlatStep)
						{
							levelOf[next] = id;
							levelSize[(size_t)id]++;
							todo.push_back(next);
						}
					}
				}
			}

			std::vector<int> levels;

			for (size_t i = 0; i < levelSize.size(); i++)
			{
				levels.push_back((int)i);
			}

			std::stable_sort(levels.begin(), levels.end(), [&levelSize](int a, int b) { return levelSize[(size_t)a] > levelSize[(size_t)b]; });

			std::vector<char> left((size_t)w * (size_t)h, 0);
			size_t leftCount = 0;

			auto const floorSpread = [&cells](Rect const &r, double *top, double *low)
			{
				*top = 1e300;
				*low = -1e300;

				for (int ix = r.ix0; ix <= r.ix1; ix++)
				{
					for (int iz = r.iz0; iz <= r.iz1; iz++)
					{
						double const y = cells.at(std::make_pair(ix, iz));

						*top = std::min(*top, y);
						*low = std::max(*low, y);
					}
				}

				return *low - *top;
			};

			for (int const level : levels)
			{
			// a level of fewer than 4 cells; after the main floor, one smaller than 8 cells and a tenth of it: a FITTING (Frost's levels tool on stock 11-20: SHED01's
			// and gencor05's pipe-run tops 1.1 m over the floor, 1.8 and 6.1 m2, made the owner's tubes "stairs or levels"); the main floor's box (1,500 up) holds an alien
			// standing on one. It and every smaller level after it: the stock dropout for them
			if (levelSize[(size_t)level] < 4 || (level != levels[0] && levelSize[(size_t)level] < std::max((size_t)8, levelSize[(size_t)levels[0]] / 10)))
			{
				break;
			}

			for (auto const &c : cells)
			{
				if (levelOf.at(c.first) == level)
				{
					left[(size_t)(c.first.second - iz0) * (size_t)w + (size_t)(c.first.first - ix0)] = 1;
					leftCount++;
				}
			}

			size_t const levelFirst = out.size();   // this level's first rectangle in out

			while (out.size() < kAutoPlotVolumeFloorsMost && leftCount > 0)
			{
				std::vector<int> height((size_t)w, 0);
				long long bestArea = 0;
				Rect best = { 0, 0, 0, 0, 0.0, 0.0 };

				for (int iz = iz0; iz <= iz1; iz++)
				{
					for (int i = 0; i < w; i++)
					{
						height[(size_t)i] = left[(size_t)(iz - iz0) * (size_t)w + (size_t)i] ? height[(size_t)i] + 1 : 0;
					}

					// the biggest rectangle under this row's histogram
					std::vector<int> stack;

					for (int i = 0; i <= w; i++)
					{
						int const h = i < w ? height[(size_t)i] : 0;

						while (!stack.empty() && height[(size_t)stack.back()] >= h)
						{
							int const top = height[(size_t)stack.back()];

							stack.pop_back();

							int const from = stack.empty() ? 0 : stack.back() + 1;
							long long const area = (long long)top * (i - from);

							// the biggest; of two as big, the wider in x (a corner's two arms: the one along x first, as Drake's hand5b SHED01)
							if (top > 0 && (area > bestArea || (area == bestArea && i - from > best.ix1 - best.ix0 + 1)))
							{
								bestArea = area;
								best.ix0 = ix0 + from;
								best.ix1 = ix0 + i - 1;
								best.iz0 = iz - top + 1;
								best.iz1 = iz;
							}
						}

						stack.push_back(i);
					}
				}

				if (bestArea < 4)
				{
					break;   // slivers only: the stock dropout for them
				}

				// the floor's spread over it; too much (a ramp, stairs): its long side halved until it isn't or it is one cell wide (the other half stays for a later round)
				double top = 0.0, low = 0.0;

				// (halved as lo + (hi - lo) / 2: indices west or north of the first cell are negative, and (lo + hi) / 2 rounds toward zero: -1 and 0 gave 0, the same
				// rectangle for ever: the first build never came back from Genshd1's MOD-01)
				while (floorSpread(best, &top, &low) > kAutoPlotFloorRise && (best.ix1 > best.ix0 || best.iz1 > best.iz0))
				{
					if (best.ix1 - best.ix0 >= best.iz1 - best.iz0)
					{
						best.ix1 = best.ix0 + (best.ix1 - best.ix0) / 2;
					}
					else
					{
						best.iz1 = best.iz0 + (best.iz1 - best.iz0) / 2;
					}
				}

				// a sliver (under 4 cells), or after the first floor a strip under kAutoPlotVolumeNarrow across: a tube's 45-degree face (the grid gives it floor cells, a row
				// a cell, each higher, so each row was a level of its own: the owner's gencor01-05, SHED01/05/10 came out "stairs or levels", 11-20; the side's LOW covers that
				// face), a doorway's sill (gencor05's 12x2 strips took its doorways off its band). A narrow strip that is the FIRST rectangle of a level of its own and at least
				// kAutoPlotLevelMinSide across is a TREAD, kept (Frost's split level, 1.2 x 2.4 m treads, read as "a room with 2 doorways" when they went: the merge of
				// frost-stairs a4130e75 on 9a4a5b7c's 2 m rule); a strip left over at a level already taken (SHED10's diagonal squares, sills) and a face row stay slivers
				double const across = std::min((double)(best.ix1 - best.ix0 + 1), (double)(best.iz1 - best.iz0 + 1)) * edge;
				bool const tread = out.size() == levelFirst && across >= (double)kAutoPlotLevelMinSide;
				bool const sliver = (long long)(best.ix1 - best.ix0 + 1) * (best.iz1 - best.iz0 + 1) < 4 || (!out.empty() && across < kAutoPlotVolumeNarrow && !tread);

				best.floorTop = top;
				best.floorLow = low;

				for (int ix = best.ix0; ix <= best.ix1; ix++)
				{
					for (int iz = best.iz0; iz <= best.iz1; iz++)
					{
						char &cell = left[(size_t)(iz - iz0) * (size_t)w + (size_t)(ix - ix0)];

						leftCount -= cell ? 1 : 0;
						cell = 0;
					}
				}

				if (!sliver)
				{
					out.push_back(best);   // a sliver halved off a ramp is dropped (the stock dropout), and the rest taken on
				}
			}

			// what this level leaves is dropped before the next level
			std::fill(left.begin(), left.end(), (char)0);
			leftCount = 0;
			}

			return out;
		}

		// A CORNER'S TWO ARMS (Drake's hand6g SHED01, the owner's gold standard 11-20): a tube corner's inner and outer corners are diagonal in plan, the cells a staircase,
		// and the fewest rectangles were 4 (the cell map on 9a4a5b7c); hand6g's are the two arms, a box can't follow a diagonal. Each doorway's arm: the band of flat cells
		// across at its doorway, from there inward to the farthest flat cell in that band. The arm whose rectangle is the bigger (of two as big, the wider in x: hand6g's
		// FLOOR 1, arm 1 along x) takes the CORNER SQUARE, the other stops at its band. False (the cells' own rectangles kept) unless the doorways face along the two axes,
		// each arm's band lies along the other's run and the shorter arm is left a line at least
		bool CornerArms(std::map<std::pair<int, int>, double> const &cells, AutoPlotDoor const &d0, AutoPlotDoor const &d1, double ox, double oz, double e,
			std::vector<Rect> *out)
		{
			struct Arm
			{
				int a = 0, sign = 0;          // the axis its doorway faces along, and the doorway's outward sign on it
				int line = 0, b0 = 0, b1 = 0; // the doorway's cell line along a; the band across it (cells along the other axis)
				int far = 0;                  // the farthest line inward with a flat cell in the band
				double y = 0.0;               // the floor at the doorway
			};

			Arm arm[2];
			AutoPlotDoor const *door[2] = { &d0, &d1 };

			for (int k = 0; k < 2; k++)
			{
				AutoPlotDoor const &d = *door[k];
				Arm &m = arm[k];
				double best = 1e300;
				std::pair<int, int> at(0, 0);

				m.a = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2;
				m.sign = (m.a == 0 ? d.through[0] : d.through[1]) > 0.0 ? 1 : -1;

				for (auto const &c : cells)
				{
					double const dx = ox + c.first.first * e - d.middle[0], dz = oz + c.first.second * e - d.middle[2];

					if (dx * dx + dz * dz < best)
					{
						best = dx * dx + dz * dz;
						at = c.first;
					}
				}

				if (best > 1e299)
				{
					return false;
				}

				m.y = cells.at(at);

				// a flat cell at (ia along a, ib across)
				auto const flat = [&cells, &m](int ia, int ib)
				{
					auto const f = cells.find(m.a == 0 ? std::make_pair(ia, ib) : std::make_pair(ib, ia));

					return f != cells.end() && fabs(f->second - m.y) <= kAutoPlotVolumeFlatStep;
				};

				m.line = m.a == 0 ? at.first : at.second;
				m.b0 = m.b1 = m.a == 0 ? at.second : at.first;

				while (flat(m.line, m.b0 - 1))
				{
					m.b0--;
				}

				while (flat(m.line, m.b1 + 1))
				{
					m.b1++;
				}

				m.far = m.line;

				for (auto const &c : cells)
				{
					int const ca = m.a == 0 ? c.first.first : c.first.second, cb = m.a == 0 ? c.first.second : c.first.first;

					if (cb >= m.b0 && cb <= m.b1 && fabs(c.second - m.y) <= kAutoPlotVolumeFlatStep && (ca - m.far) * -m.sign > 0)
					{
						m.far = ca;
					}
				}
			}

			if (arm[0].a == arm[1].a || fabs(arm[0].y - arm[1].y) > kAutoPlotVolumeFlatStep)
			{
				return false;
			}

			// arm k's rectangle: its band across, its doorway's line to `to` along
			auto const rectOf = [&arm](int k, int to)
			{
				Arm const &m = arm[k];
				int const l0 = std::min(m.line, to), l1 = std::max(m.line, to);
				Rect r = { 0, 0, 0, 0, 0.0, 0.0 };

				if (m.a == 0)
				{
					r.ix0 = l0;
					r.ix1 = l1;
					r.iz0 = m.b0;
					r.iz1 = m.b1;
				}
				else
				{
					r.iz0 = l0;
					r.iz1 = l1;
					r.ix0 = m.b0;
					r.ix1 = m.b1;
				}

				return r;
			};

			std::vector<Rect> best;
			long long bestArea = -1;
			int bestWide = -1;

			for (int k = 0; k < 2; k++)
			{
				Arm const &own = arm[k], &other = arm[1 - k];
				int const ownLo = std::min(own.line, own.far), ownHi = std::max(own.line, own.far);
				int const otherLo = std::min(other.line, other.far), otherHi = std::max(other.line, other.far);

				if (other.b0 < ownLo || other.b1 > ownHi || own.b0 < otherLo || own.b1 > otherHi)
				{
					continue;   // the arms don't cross: no corner square
				}

				// the other arm runs inward (against its doorway's outward sign) and stops short of this one's band
				int const stop = other.sign < 0 ? own.b0 - 1 : own.b1 + 1;

				if (other.sign < 0 ? other.line > stop : other.line < stop)
				{
					continue;
				}

				Rect const big = rectOf(k, own.far), small = rectOf(1 - k, stop);
				long long const area = (long long)(big.ix1 - big.ix0 + 1) * (big.iz1 - big.iz0 + 1);
				int const wide = big.ix1 - big.ix0 + 1;

				if (area > bestArea || (area == bestArea && wide > bestWide))
				{
					bestArea = area;
					bestWide = wide;
					best = { big, small };
				}
			}

			if (best.empty())
			{
				return false;
			}

			// each one's floor: its flat cells' highest and lowest (a diagonal side's corner has none: the floor box goes over it, as hand6g's)
			for (Rect &r : best)
			{
				r.floorTop = 1e300;
				r.floorLow = -1e300;

				for (int ix = r.ix0; ix <= r.ix1; ix++)
				{
					for (int iz = r.iz0; iz <= r.iz1; iz++)
					{
						auto const f = cells.find(std::make_pair(ix, iz));

						if (f != cells.end() && fabs(f->second - arm[0].y) <= kAutoPlotVolumeFlatStep)
						{
							r.floorTop = std::min(r.floorTop, f->second);
							r.floorLow = std::max(r.floorLow, f->second);
						}
					}
				}

				if (r.floorTop > 1e299)
				{
					return false;
				}
			}

			*out = best;
			return true;
		}

		void SetLink(std::vector<WaypointRec> *list, size_t a, size_t b, int32_t flagsAtA, int32_t flagsAtB)
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

	AutoPlotPlan AutoPlot_PlanVolumes(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floorIn,
		std::vector<AutoPlotDoor> const &doors, double levelScale, AutoPlotVolumeCounts const &counts)
	{
		AutoPlotPlan plan;
		double const scale = levelScale > 0.0 ? levelScale : 1.0;

		plan.exits = exits.size();
		plan.doorBox.assign(exits.size(), -1);
		plan.exitBox.assign(exits.size(), -1);
		plan.doorWords.assign(exits.size(), std::string());

		if (floorIn.cells.empty() || floorIn.cellEdge <= 0)
		{
			plan.volumeKind = "no floor";
			plan.notes.push_back("NOT SHAPED: no floor cells (no network written)");
			return plan;
		}

		// a CRAWL floor (Drake's CrawlCells: a duct): the levels' rule (Frost 11-20, autoplot_levels.h)
		if (floorIn.crawl)
		{
			return AutoPlot_LevelsVolumes(floorIn, exits, doors, levelScale, "duct");
		}

		// the cells on their grid (the first cell the origin), and the ceiling over each (the grid's places: their own list)
		double const e = (double)floorIn.cellEdge;
		double const ox = (double)floorIn.cells[0][0], oz = (double)floorIn.cells[0][2];
		auto const key = [&](double x, double z) { return std::make_pair((int)lround((x - ox) / e), (int)lround((z - oz) / e)); };
		std::map<std::pair<int, int>, double> cells, ceilings;
		int ix0 = 0, ix1 = 0, iz0 = 0, iz1 = 0;

		for (std::vector<int32_t> const &c : floorIn.cells)
		{
			if (c.size() != 3)
			{
				continue;
			}

			std::pair<int, int> const k = key((double)c[0], (double)c[2]);
			auto const at = cells.find(k);

			cells[k] = at == cells.end() ? (double)c[1] : std::min(at->second, (double)c[1]);   // two floors over one cell: the higher
			ix0 = std::min(ix0, k.first);
			ix1 = std::max(ix1, k.first);
			iz0 = std::min(iz0, k.second);
			iz1 = std::max(iz1, k.second);
		}

		for (std::vector<int32_t> const &c : floorIn.ceilings)
		{
			if (c.size() == 3)
			{
				std::pair<int, int> const k = key((double)c[0], (double)c[2]);
				auto const at = ceilings.find(k);

				ceilings[k] = at == ceilings.end() ? (double)c[1] : std::max(at->second, (double)c[1]);   // the lower
			}
		}

		// SMALL HOLES ARE FLOOR (gencor05 on 51fad1b1: a pipe lying across its floor split its band in two, and it read as a "room with 2 doorways"): a cell missing, or
		// standing up to kAutoPlotReach over the floor (a pipe's or a support's top: the floor's box holds an alien on it), between two cells of one flat height at most
		// kAutoPlotVolumeHoleCells along x or z, is floor at that height. One pass: a wider gap is a real one; a slope's edge has the flat floor on one side only
		{
			std::vector<std::pair<std::pair<int, int>, double> > fill;

			for (int ix = ix0; ix <= ix1; ix++)
			{
				for (int iz = iz0; iz <= iz1; iz++)
				{
					auto const here = cells.find(std::make_pair(ix, iz));
					double const hereY = here != cells.end() ? here->second : 1e300;

					bool done = false;

					for (int const ax : { 0, 1 })
					{
						// each side's floor: the LOWEST cell (y is down: the most) within kAutoPlotVolumeHoleCells (past a second pipe cell to the floor beyond it)
						double lowSide = -1e300, highSide = -1e300;

						for (int d = 1; d <= kAutoPlotVolumeHoleCells; d++)
						{
							auto const at = cells.find(ax == 0 ? std::make_pair(ix - d, iz) : std::make_pair(ix, iz - d));
							auto const bt = cells.find(ax == 0 ? std::make_pair(ix + d, iz) : std::make_pair(ix, iz + d));

							lowSide = at != cells.end() ? std::max(lowSide, at->second) : lowSide;
							highSide = bt != cells.end() ? std::max(highSide, bt->second) : highSide;
						}

						lowSide = lowSide < -1e299 ? 1e300 : lowSide;
						highSide = highSide < -1e299 ? 1e300 : highSide;

						double const flat = std::max(lowSide, highSide) > 1e299 ? 1e300 : std::min(lowSide, highSide);
						// missing, or standing on the floor at any height (y is down): a pipe's top, a column (gencor05's and SHED10's, a 3 x 2 block taller than 1.5 m mid-floor,
						// split each in two on a8ed1781: the cell map); a floor's box over a column is harmless, the alien walks round it
						bool const hole = hereY > 1e299 || hereY < flat - kAutoPlotVolumeFlatStep;

						if (!done && hole && lowSide < 1e299 && highSide < 1e299 && fabs(lowSide - highSide) <= kAutoPlotVolumeFlatStep)
						{
							fill.push_back(std::make_pair(std::make_pair(ix, iz), flat));
							done = true;
						}
					}
				}
			}

			for (std::pair<std::pair<int, int>, double> const &one : fill)
			{
				cells[one.first] = one.second;
			}
		}

		plan.cells = cells.size();

		std::vector<Rect> rects = Rectangles(cells, ix0, ix1, iz0, iz1, e);

		// A NOTCH ALONG A FLOOR'S EDGE (the owner's SHED10 on 9a4a5b7c: its bar's north row has no floor cells 2.4 m along under the north wall's middle, so the bar stopped
		// a row short, 273 mm off hand6g's FLOOR M): a rectangle takes the line of cells just past a side when both of its end cells and three quarters of it are flat floor
		// at its height that no other rectangle holds; one line a side (a diagonal side's line, a staircase, has an end missing: never)
		for (size_t i = 0; i < rects.size(); i++)
		{
			for (int const a : { 0, 2 })
			{
				for (int const s : { -1, 1 })
				{
					Rect &r = rects[i];
					int const line = a == 0 ? (s > 0 ? r.ix1 + 1 : r.ix0 - 1) : (s > 0 ? r.iz1 + 1 : r.iz0 - 1);
					int const from = a == 0 ? r.iz0 : r.ix0, to = a == 0 ? r.iz1 : r.ix1;
					auto const freeFlat = [&](int k)
					{
						std::pair<int, int> const at = a == 0 ? std::make_pair(line, k) : std::make_pair(k, line);
						auto const c = cells.find(at);

						if (c == cells.end() || c->second < r.floorTop - kAutoPlotVolumeFlatStep || c->second > r.floorLow + kAutoPlotVolumeFlatStep)
						{
							return false;
						}

						for (size_t j = 0; j < rects.size(); j++)
						{
							if (j != i && at.first >= rects[j].ix0 && at.first <= rects[j].ix1 && at.second >= rects[j].iz0 && at.second <= rects[j].iz1)
							{
								return false;
							}
						}

						return true;
					};
					int n = 0;

					for (int k = from; k <= to; k++)
					{
						n += freeFlat(k) ? 1 : 0;
					}

					if (to <= from || !freeFlat(from) || !freeFlat(to) || n * 4 < (to - from + 1) * 3)
					{
						continue;
					}

					for (int k = from; k <= to; k++)
					{
						auto const c = cells.find(a == 0 ? std::make_pair(line, k) : std::make_pair(k, line));

						if (c != cells.end() && freeFlat(k))
						{
							r.floorTop = std::min(r.floorTop, c->second);
							r.floorLow = std::max(r.floorLow, c->second);
						}
					}

					(a == 0 ? (s > 0 ? r.ix1 : r.ix0) : (s > 0 ? r.iz1 : r.iz0)) = line;
				}
			}
		}

		// THE CELL MAP, for reading a module's floor from the report (AUTOPLOT_VOLUME_MAP set in the editor's environment; off by default): a row of cells each line, '#' at
		// the first floor's height (within 100), 'h' higher, 'l' lower, '.' none; then each floor rectangle (cells x0..x1, z0..z1)
		if (getenv("AUTOPLOT_VOLUME_MAP") && !rects.empty())
		{
			double const main = rects[0].floorTop;

			printf("AVP_OpenInteng: volume-map: cells %zu, edge %.0f, origin %.0f %.0f, the first floor at y %.0f, %zu floors:", cells.size(), e, ox, oz, main, rects.size());

			for (Rect const &r : rects)
			{
				printf(" [%d..%d x %d..%d]", r.ix0, r.ix1, r.iz0, r.iz1);
			}

			printf("\n");

			for (int iz = iz1; iz >= iz0; iz--)
			{
				std::string row;

				for (int ix = ix0; ix <= ix1; ix++)
				{
					auto const at = cells.find(std::make_pair(ix, iz));

					row += at == cells.end() ? '.' : (fabs(at->second - main) <= kAutoPlotVolumeFlatStep ? '#' : (at->second < main ? 'h' : 'l'));
				}

				printf("AVP_OpenInteng: volume-map: %4d %s\n", iz, row.c_str());
			}

			fflush(stdout);
		}

		if (rects.empty())
		{
			plan.volumeKind = "no floor";
			plan.notes.push_back("NOT SHAPED: no floor rectangle of 4 cells or more (no network written)");
			return plan;
		}

		auto const cellLo = [&](int i, double o) { return o + (i - 0.5) * e; };
		auto const cellHi = [&](int i, double o) { return o + (i + 0.5) * e; };

		// each rectangle's ceiling: the places over its cells
		auto const ceilOver = [&ceilings](Rect &r)
		{
			for (int ix = r.ix0; ix <= r.ix1; ix++)
			{
				for (int iz = r.iz0; iz <= r.iz1; iz++)
				{
					auto const at = ceilings.find(std::make_pair(ix, iz));

					if (at != ceilings.end())
					{
						r.ceilTop = r.ceiled ? std::min(r.ceilTop, at->second) : at->second;
						r.ceilLow = r.ceiled ? std::max(r.ceilLow, at->second) : at->second;
						r.ceiled = true;
					}
				}
			}

			// a sloped edge lower than the flat ceiling by more than kAutoPlotVolumeFlat is left out of the ceiling's low (the entry covers the flat part)
			if (r.ceiled && r.ceilLow - r.ceilTop > kAutoPlotVolumeFlat)
			{
				r.ceilLow = r.ceilTop + kAutoPlotVolumeFlat;
			}
		};

		std::vector<Vol> vols;

		// FLOOR: one a rectangle
		auto const floorOf = [&](Rect const &r)
		{
			Vol v;

			v.kind = Kind::Floor;
			v.lo[0] = cellLo(r.ix0, ox);
			v.hi[0] = cellHi(r.ix1, ox);
			v.lo[2] = cellLo(r.iz0, oz);
			v.hi[2] = cellHi(r.iz1, oz);
			v.lo[1] = r.floorTop - kAutoPlotReach;
			v.hi[1] = r.floorLow + kAutoPlotInto;

			for (double const fx : { 0.5, 0.25, 0.75 })
			{
				for (double const fz : { 0.5, 0.25, 0.75 })
				{
					v.centres.push_back(std::vector<double>{ v.lo[0] + fx * (v.hi[0] - v.lo[0]), r.floorTop - kAutoPlotReach / 2.0, v.lo[2] + fz * (v.hi[2] - v.lo[2]) });
				}
			}

			return v;
		};

		for (Rect &r : rects)
		{
			ceilOver(r);
			vols.push_back(floorOf(r));
		}

		size_t floors = vols.size();

		// THE DOORWAYS: the exits with a doorway (its way through found), and the floor rectangle each lies at (the nearest)
		std::vector<int> lanes, laneRect, lanesAt, twinOf;   // twinOf: an exit through the same doorway as an earlier one (-1: none)
		auto const assign = [&]()
		{
			lanes.clear();
			laneRect.assign(exits.size(), -1);
			lanesAt.assign(rects.size(), 0);

			for (size_t x = 0; x < exits.size(); x++)
			{
				if (x < doors.size() && doors[x].throughFound && exits[x].size() == 3)
				{
					double best = 1e300;

					for (size_t i = 0; i < floors; i++)
					{
						double const gx = std::max(0.0, std::max(vols[i].lo[0] - doors[x].middle[0], doors[x].middle[0] - vols[i].hi[0]));
						double const gz = std::max(0.0, std::max(vols[i].lo[2] - doors[x].middle[2], doors[x].middle[2] - vols[i].hi[2]));

						if (gx * gx + gz * gz < best)
						{
							best = gx * gx + gz * gz;
							laneRect[x] = (int)i;
						}
					}

					lanes.push_back((int)x);
					lanesAt[(size_t)laneRect[x]]++;
				}
			}

			// TWO EXITS THROUGH ONE DOORWAY (tolift on 9f781c00: two neighbouring AI modules through one doorway; its T's bar had two "doorways" at one place, both entries
			// the same half, and the funnel's entry touched neither): the later is a TWIN of the first, shaped as one doorway (one entry), its point in the first one's exit box
			twinOf.assign(exits.size(), -1);

			std::vector<int> kept;

			for (int const x : lanes)
			{
				AutoPlotDoor const &dx = doors[(size_t)x];

				for (int const k : kept)
				{
					AutoPlotDoor const &dk = doors[(size_t)k];

					if (twinOf[(size_t)x] < 0 && fabs(dk.middle[0] - dx.middle[0]) <= kAutoPlotVolumeSide && fabs(dk.middle[2] - dx.middle[2]) <= kAutoPlotVolumeSide &&
						fabs(dk.middle[1] - dx.middle[1]) <= kAutoPlotVolumeTwinRise && dk.through[0] * dx.through[0] + dk.through[1] * dx.through[1] > 0.7)
					{
						twinOf[(size_t)x] = k;
						lanesAt[(size_t)laneRect[(size_t)x]]--;
					}
				}

				if (twinOf[(size_t)x] < 0)
				{
					kept.push_back(x);
				}
			}

			lanes = kept;
		};

		assign();

		// A FLOOR NO DOORWAY'S FLOOR REACHES (an isolated step or platform: med11's 4-cell level beside its floor, 11-20: "box 0 cannot reach box 1") is left out: the
		// stock dropout for it, never a box no route reaches
		{
			std::vector<int> part(floors, -1);
			int parts = 0;

			for (size_t s = 0; s < floors; s++)
			{
				if (part[s] >= 0)
				{
					continue;
				}

				std::vector<size_t> todo(1, s);

				part[s] = parts;

				while (!todo.empty())
				{
					size_t const n = todo.back();

					todo.pop_back();

					for (size_t j = 0; j < floors; j++)
					{
						if (part[j] < 0 && Touch(vols[n], vols[j], e / 2.0))
						{
							part[j] = parts;
							todo.push_back(j);
						}
					}
				}

				parts++;
			}

			std::vector<bool> reached((size_t)parts, false);

			for (size_t i = 0; i < floors; i++)
			{
				reached[(size_t)part[i]] = reached[(size_t)part[i]] || lanesAt[i] > 0;
			}

			std::vector<Rect> keptRects;
			std::vector<Vol> keptVols;

			for (size_t i = 0; i < floors; i++)
			{
				if (reached[(size_t)part[i]] || lanes.empty())
				{
					keptRects.push_back(rects[i]);
					keptVols.push_back(vols[i]);
				}
			}

			if (keptRects.size() != rects.size())
			{
				plan.notes.push_back(std::to_string((long long)(rects.size() - keptRects.size())) + " floor part(s) no doorway's floor reaches left out (the stock dropout)");
				rects = keptRects;
				vols = keptVols;
				floors = vols.size();
				assign();
			}
		}

		// THE KIND (the owner's coverage, Bishop 11-20): shaped only where an oracle proves the rules (Drake's hand5b: the corridor and the corner, the owner's in-game PASS;
		// a dead end is a corridor's half; his hand6e: the T); every other kind gets NO network yet, the way Rebellion left most modules, named (Bishop's ruling: never
		// the old small boxes)
		int funnelRect = -1, barRect = -1;   // a T's: the funnel's rectangle and the bar's

		{
			bool ceiled = false;
			size_t apart = 0;

			for (Rect const &r : rects)
			{
				ceiled = ceiled || r.ceiled;
				apart += fabs(r.floorTop - rects[0].floorTop) > 450.0 ? 1 : 0;
			}

			bool const oneRect = lanes.size() == 2 && laneRect[(size_t)lanes[0]] == laneRect[(size_t)lanes[1]];
			bool const twoRects = lanes.size() == 2 && !oneRect && Touch(vols[(size_t)laneRect[(size_t)lanes[0]]], vols[(size_t)laneRect[(size_t)lanes[1]]], e / 2.0);

			// A T (Drake's hand6e SHED10): 3 doorways, two at one floor rectangle (the bar), the third at a rectangle touching it (the funnel)
			if (lanes.size() == 3)
			{
				for (int const l : lanes)
				{
					int const R = laneRect[(size_t)l];

					if (lanesAt[(size_t)R] == 1)
					{
						for (int const o : lanes)
						{
							int const B = laneRect[(size_t)o];

							if (B != R && lanesAt[(size_t)B] == 2 && Touch(vols[(size_t)R], vols[(size_t)B], e / 2.0))
							{
								funnelRect = R;
								barRect = B;
							}
						}
					}
				}
			}

			plan.volumeKind = !ceiled ? "open sky" : (apart >= 1 && rects.size() >= 3) ? "stairs or levels" : lanes.empty() ? "no doorway" : lanes.size() == 1 ? "dead end" :
				oneRect ? "corridor" : twoRects ? "corner" : lanes.size() == 2 ? "room with 2 doorways" : funnelRect >= 0 ? "junction (T)" : "junction or room";

			// STAIRS OR LEVELS, and OPEN SKY (no ceiling: nothing to climb to, no lane; Bishop 11-20 for Hudson's full Genshd1 run, 60 of its modules): the levels' rule
			// (Frost 11-20, autoplot_levels.h: its floor walk); when it can't shape them either, named below
			if (plan.volumeKind == "stairs or levels" || plan.volumeKind == "open sky")
			{
				AutoPlotPlan const levels = AutoPlot_LevelsVolumes(floorIn, exits, doors, levelScale, plan.volumeKind);

				if (levels.volumes)
				{
					return levels;
				}
			}

			if (plan.volumeKind != "corridor" && plan.volumeKind != "corner" && plan.volumeKind != "dead end" && plan.volumeKind != "junction (T)" && plan.volumeKind != "junction or room" && plan.volumeKind != "room with 2 doorways")   // a room: the same rules, with the wall test (autoplot_rooms.h, Drake)
			{
				std::string floorsSaid;

				for (Rect const &r : rects)
				{
					floorsSaid += " " + std::to_string((long long)(r.ix1 - r.ix0 + 1)) + "x" + std::to_string((long long)(r.iz1 - r.iz0 + 1)) + "@" + std::to_string((long long)r.floorTop) +
						(r.floorLow > r.floorTop ? ".." + std::to_string((long long)r.floorLow) : std::string());
				}

				plan.notes.insert(plan.notes.begin(), "NOT SHAPED: a " + plan.volumeKind + " (no rule for it yet: no network written; its floors in cells @ height:" + floorsSaid +
					"; doorways " + std::to_string((long long)lanes.size()) + ")");
				return plan;
			}
		}

		// A CORNER: its two arms for its floors (CornerArms: hand6g's FLOOR 1 and FLOOR 2); the cells' own rectangles kept when the arms don't fit, or don't each take one
		// doorway and a ceiling
		if (plan.volumeKind == "corner")
		{
			std::vector<Rect> arms;

			if (CornerArms(cells, doors[(size_t)lanes[0]], doors[(size_t)lanes[1]], ox, oz, e, &arms))
			{
				std::vector<Rect> const keptRects = rects;
				std::vector<Vol> const keptVols = vols;

				rects = arms;
				vols.clear();

				for (Rect &r : rects)
				{
					ceilOver(r);
					vols.push_back(floorOf(r));
				}

				floors = vols.size();
				assign();

				if (lanes.size() != 2 || laneRect[(size_t)lanes[0]] == laneRect[(size_t)lanes[1]] || !rects[0].ceiled || !rects[1].ceiled)
				{
					rects = keptRects;
					vols = keptVols;
					floors = vols.size();
					assign();
				}
				else if (getenv("AUTOPLOT_VOLUME_MAP"))
				{
					printf("AVP_OpenInteng: volume-map: a corner's arms: [%d..%d x %d..%d] [%d..%d x %d..%d]\n", rects[0].ix0, rects[0].ix1, rects[0].iz0, rects[0].iz1,
						rects[1].ix0, rects[1].ix1, rects[1].iz0, rects[1].iz1);
					fflush(stdout);
				}
			}
		}

		// A DOOR SLAB, THE DOORWAY KIND (Drake's count on d523d3b3: medoor01, medoor02, shdr01, door01, MNDR01, opsdr01, opsdr02 "box 1 cannot reach box 0"; medoor01's
		// floor is one 300 mm cell deep): a corridor whose two doorways face opposite ways along one axis and whose floor is under kAutoPlotVolumeClimbLength deep along it.
		// As a corridor its climb went on a jamb and led up into nothing. Its shape: the floor, ONE entry over it all past both doorways, the two exits; no climb, no get-out
		bool slab = false;

		if (plan.volumeKind == "corridor" && lanes.size() == 2 && laneRect[(size_t)lanes[0]] >= 0)
		{
			AutoPlotDoor const &d0 = doors[(size_t)lanes[0]], &d1 = doors[(size_t)lanes[1]];
			int const t = fabs(d0.through[0]) >= fabs(d0.through[1]) ? 0 : 2, t1 = fabs(d1.through[0]) >= fabs(d1.through[1]) ? 0 : 2;
			double const s0 = t == 0 ? d0.through[0] : d0.through[1], s1 = t == 0 ? d1.through[0] : d1.through[1];
			Vol const &f = vols[(size_t)laneRect[(size_t)lanes[0]]];

			// (flat: its doorways at one height, within a tread's step; STEPS04, a staircase's 900 mm landing with doorways 3.7 m apart in height, read as one on 9f781c00)
			slab = t1 == t && s0 * s1 < 0.0 && f.hi[t] - f.lo[t] < kAutoPlotVolumeClimbLength && rects[(size_t)laneRect[(size_t)lanes[0]]].ceiled &&
				fabs(d0.middle[1] - d1.middle[1]) <= kAutoPlotLevelStep;
			plan.volumeKind = slab ? "doorway" : plan.volumeKind;
		}

		// THE CLIMB SIDE of each ceiled rectangle (Drake's hand5b, the owner's in-game PASS 11-20): one of its four sides with no doorway on it and no other floor rectangle
		// starting along it (none at all since 4cd2bc76+: a seam side gets no catchers); the longest (within a cell); of two as long the one farther from the other rectangles (a corner's outer wall); then the + side. Its
		// reach: how far the AI module's own box goes past it (a tube's two 45-degree faces meet 1,948 out on Genshd1; a wall stands at the floor's edge or near it)
		//
		// NO WALL ON A SIDE (no climb on it, no catchers over it) where a doorway is: one within 900 of it (any floor's), or ITS OWN FLOOR'S doorway anywhere out past it
		// (gencor02's west doorway 1.5 m past its band, past a sill the rectangles left out: d060b295 climbed there into nothing; Drake's count on d523d3b3, entries 2 to
		// 20 m past a floor's edge out to its doorway, IIroom, EXT-01: that side is the way on); or where the floor goes on past it at its height, more than a quarter of
		// it (a diagonal side in plan, a corner's inner staircase)
		auto const doorOn = [&](size_t i, int a, int s)
		{
			Vol const &f = vols[i];
			int const b = a == 0 ? 2 : 0;
			double const edge = s > 0 ? f.hi[a] : f.lo[a];
			bool door = false;

			for (int const x : lanes)
			{
				AutoPlotDoor const &d = doors[(size_t)x];
				bool const along = d.middle[b] >= f.lo[b] - e && d.middle[b] <= f.hi[b] + e;
				double const out = (d.middle[a] - edge) * s;   // how far out past this side
				double const outAcross = std::max(f.lo[b] - d.middle[b], d.middle[b] - f.hi[b]);   // how far out past the side's ends (> 0: off its span)

				// (its own floor's doorway off the side's span, out past the floor's CORNER: on the side it lies farthest out past; Drake on 544831f0, EXT-01's doorway 5.7 m
				// past its west side and 1.6 m past its north side, where the LOW climbed west toward it)
				door = door || (along && fabs(d.middle[a] - edge) <= kAutoPlotVolumeSide + e) || (laneRect[(size_t)x] == (int)i && out > 0.0 && (along || out >= outAcross));
			}

			return door;
		};
		// THE WALL FOOT (Bishop's spec 11-21, the floor-only corridors: SHED04's north side, a ragged one-row strip of floor under its wall, 46 of 61 cells, took the
		// corridor's only climb side, and it got no climb-in and no entry): read with rows = kAutoPlotVolumeFloorOnRows, the floor counts as going on past a side only where
		// it goes on that many lines (a cell at its height in the line just past the side AND in the line past that one). A one-row strip is the wall's foot: the side stays a
		// wall, and the climb box reaches over the strip to it (its reach is the AI module's own box past the side). rows = 1: one line past the side is enough (as before).
		// Used for a floor that has NO climb side read with one line (below, THE CLIMB SIDE): the whole-map plot of 505f1c0f, which read every side with two, moved
		// genent01's working climb (the owner's tunnel path) to its other side and gave seven shaped modules more catchers; the floor-only corridors were the spec
		// whether the floor goes on past side (a, s) of rectangle i at cell k along it, read with `rows` lines (THE WALL FOOT, above)
		auto const goesOnAt = [&](size_t i, int a, int s, int rows, int k)
		{
			Rect const &r = rects[i];
			int const line = a == 0 ? (s > 0 ? r.ix1 + 1 : r.ix0 - 1) : (s > 0 ? r.iz1 + 1 : r.iz0 - 1);
			bool goesOn = true;

			for (int out = 0; out < rows && goesOn; out++)
			{
				int const l = line + s * out;   // `out` lines past the first line past the side
				auto const c = cells.find(a == 0 ? std::make_pair(l, k) : std::make_pair(k, l));

				goesOn = c != cells.end() && c->second >= r.floorTop - kAutoPlotVolumeFlatStep && c->second <= r.floorLow + kAutoPlotVolumeFlatStep;
			}

			return goesOn;
		};
		auto const floorBeyond = [&](size_t i, int a, int s, int rows)
		{
			Rect const &r = rects[i];
			int const from = a == 0 ? r.iz0 : r.ix0, to = a == 0 ? r.iz1 : r.ix1;
			double beyond = 0.0;

			for (int k = from; k <= to; k++)
			{
				beyond += goesOnAt(i, a, s, rows, k) ? e : 0.0;
			}

			return beyond;   // mm along the side
		};

		// THE WALL PART (Bishop's spec (b) 11-21: new02's north side is a wall for 3.3 m and an arm of floor goes on past the rest, so no WHOLE side was left to climb):
		// the longest run of cells along side (a, s) of rectangle i past which the floor does not go on (read with `rows` lines), its span along the side in the
		// world's mm (lo .. hi); its length, 0 when there is none
		auto const wallPart = [&](size_t i, int a, int s, int rows, double *lo, double *hi)
		{
			Rect const &r = rects[i];
			int const from = a == 0 ? r.iz0 : r.ix0, to = a == 0 ? r.iz1 : r.ix1;
			int best = 0, bestFrom = from, run = 0;

			for (int k = from; k <= to; k++)
			{
				run = goesOnAt(i, a, s, rows, k) ? 0 : run + 1;

				if (run > best)
				{
					best = run;
					bestFrom = k - run + 1;
				}
			}

			if (best == 0)
			{
				return 0.0;
			}

			double const o = a == 0 ? oz : ox;   // the side runs along the other axis

			*lo = cellLo(bestFrom, o);
			*hi = cellHi(bestFrom + best - 1, o);
			return *hi - *lo;
		};

		// A ROOM'S WALL TEST (autoplot_rooms.h, Drake 11-20): in a room the module's flat ceiling can go on past a floor's side (a desk, a pit, a raised band left out of the
		// floor): the mm of the side it goes on past; 0 in every other kind (a corridor's floor fills it wall to wall; the gold modules stay as they are)
		bool const room = plan.volumeKind == "junction or room" || plan.volumeKind == "room with 2 doorways";
		auto const ceilingBeyond = [&](size_t i, int a, int s)
		{
			Rect const &r = rects[i];

			return room ? AutoPlot_CeilingBeyond(ceilings, r.ix0, r.ix1, r.iz0, r.iz1, a, s, r.ceilTop, (double)kAutoPlotVolumeFlat, e) : 0.0;
		};

		struct Side
		{
			int axis = -1;     // 0 x, 2 z: the axis the side faces along
			int sign = 0;      // +1: the side at the rectangle's high edge
			double edge = 0.0, reach = 0.0;
			int rows = 1;      // the lines of floor past it that count as the floor going on, as it was chosen with (THE WALL FOOT: 2 when 1 left the floor no side)
			bool part = false; // THE WALL PART: the climb on the side's wall part only (from .. to), its floor having no whole side
			double from = 0.0, to = 0.0;   // the climb's span along the side (the world's mm): the whole floor's, or the wall part's
		};

		std::vector<Side> sides(rects.size());

		for (size_t i = 0; i < rects.size(); i++)
		{
			if (!rects[i].ceiled || lanesAt[i] == 0 || (int)i == funnelRect || slab)
			{
				continue;   // a climb-in only on a floor that holds a doorway (an alcove's aliens walk to it); a T's funnel none (they walk into the bar and climb there)
			}

			Vol const &f = vols[i];
			double bestScore = -1e300;

			// one line of floor past a side first (as before); THE WALL FOOT's two only when that leaves the floor no side; THE WALL PART only when neither leaves it one
			for (int const pass : { 0, 1, 2 })
			{
				int const rows = pass == 0 ? 1 : kAutoPlotVolumeFloorOnRows;
				bool const parts = pass == 2;

				if (sides[i].axis >= 0)
				{
					break;
				}

				for (int const a : { 0, 2 })
				{
					int const b = a == 0 ? 2 : 0;

					for (int const s : { 1, -1 })
					{
						double const edge = s > 0 ? f.hi[a] : f.lo[a], length = f.hi[b] - f.lo[b];
						bool const door = doorOn(i, a, s);
						double seam = 0.0, outer = 0.0;

						for (size_t j = 0; j < floors; j++)
						{
							if (j == i)
							{
								continue;
							}

							Vol const &o = vols[j];

							if (fabs((s > 0 ? o.lo[a] : o.hi[a]) - edge) <= e)   // it starts at this side's line
							{
								seam += std::max(0.0, std::min(o.hi[b], f.hi[b]) - std::max(o.lo[b], f.lo[b]));
							}

							outer += fabs(edge - (o.lo[a] + o.hi[a]) / 2.0);
						}

						if (door || seam > e / 2.0 || ceilingBeyond(i, a, s) > length / 4.0 || (!parts && floorBeyond(i, a, s, rows) > length / 4.0))   // (any seam: a side another floor starts at gets no catchers, so a LOW there led up into nothing: doormp09, newcol03 on 9f781c00)
						{
							continue;
						}

						// THE WALL PART (pass 2): the side's longest run of wall, kAutoPlotVolumeWallPart or more, the climb's span; scored by that length
						double partLo = f.lo[b], partHi = f.hi[b], climbable = length;

						if (parts && (climbable = wallPart(i, a, s, rows, &partLo, &partHi)) < (double)kAutoPlotVolumeWallPart)
						{
							continue;
						}

						double const score = floor(climbable / e + 0.5) * 1e9 + outer * 10.0 + (s > 0 ? 1.0 : 0.0);

						if (score > bestScore)
						{
							bestScore = score;
							sides[i].axis = a;
							sides[i].sign = s;
							sides[i].edge = edge;
							sides[i].rows = rows;
							sides[i].part = parts;
							sides[i].from = partLo;
							sides[i].to = partHi;
						}
					}
				}
			}

			if (sides[i].axis >= 0)
			{
				Side &sd = sides[i];
				double const boxEdge = !floorIn.boxFound ? sd.edge : (sd.sign > 0 ? (double)floorIn.boxHi[sd.axis] : (double)floorIn.boxLo[sd.axis]);

				sd.reach = std::min(std::max((boxEdge - sd.edge) * sd.sign, 0.0), 3000.0);

				// a WALL PART: the AI module's own box past that side is the arm of floor beside the part (new02's north arm), not this wall; the wall is taken as a flat one at
				// most kAutoPlotVolumeSloped past the floor's edge (the grid's cells stop short of a wall: new02's floor goes on to 450 past its last cell, none at 750), and
				// the climb's centre goes past that (below), so a floor alien heading for it walks into the wall and climbs (lanes up on 730ab69a: with the wall at the edge,
				// the centre stood in open air over the aliens' heads, new02 74 stall places, new01 322, corr10 590)
				if (sd.part)
				{
					sd.reach = std::min(sd.reach, (double)kAutoPlotVolumeSloped);
				}

				// a SLOPED side's two 45-degree faces meet half the room's height out (Genshd1's tubes: 1,948 of 3,896); the module's box goes further where clutter stands
				// (gencor03's south ledge took its catchers 1 m too deep: Drake's gold compare on 64764f17)
				sd.reach = sd.reach > kAutoPlotVolumeSloped ? std::min(sd.reach, (rects[i].floorTop - rects[i].ceilTop) / 2.0) : sd.reach;
			}
		}

		// THE OWNER'S COUNTS (autoplot_volumes.h; the owner 11-20 on hand6g: "too many climb points"): at most climbIns climbs, the biggest floors' first (hand6g's
		// candidates: each doorway floor's climb side); a floor whose climb goes keeps its side for the entries' and the get-out's places, and its HIGH pieces as jump
		// catchers. At most getOuts get-outs (hand6g's one)
		// (AutoPlot_UsedCounts: -1 the default, any other number held to its range: Tao 3, the dialog is not the only caller; THE OWNER'S SIZES, the HIGH aim's, below)
		AutoPlotVolumeCounts const used = AutoPlot_UsedCounts(counts);
		int32_t const climbsWanted = used.climbIns;
		int32_t const getOutsWanted = used.getOuts;
		std::vector<bool> climbKept(rects.size(), false);
		size_t climbsKept = 0, climbCandidates = 0;
		{
			std::vector<size_t> order;

			for (size_t i = 0; i < rects.size(); i++)
			{
				if (sides[i].axis >= 0)
				{
					order.push_back(i);
				}
			}

			auto const area = [&vols](size_t i) { return (vols[i].hi[0] - vols[i].lo[0]) * (vols[i].hi[2] - vols[i].lo[2]); };

			// WHOLE SIDES FIRST (med02 on fe3ea91a: its biggest floor, with no side before, got a wall part, took the one climb from floor 5's whole side, its centre
			// inside the entry over it, and the module was refused): a side from THE WALL FOOT or THE WALL PART is for a floor that has no other; every floor with a
			// side read as before comes first, the biggest first, then the others
			auto const wholeSide = [&sides](size_t i) { return sides[i].axis >= 0 && sides[i].rows == 1 && !sides[i].part; };

			std::stable_sort(order.begin(), order.end(), [&area, &wholeSide](size_t p, size_t q) { return wholeSide(p) != wholeSide(q) ? wholeSide(p) : area(p) > area(q); });
			climbCandidates = order.size();

			for (size_t k = 0; k < order.size() && (int32_t)k < climbsWanted; k++)
			{
				climbKept[order[k]] = true;
				climbsKept++;
			}
		}

		// a side from THE WALL FOOT or THE WALL PART that got no climb is taken back: it was only ever for that climb, and the floor plans as it did before (its entries,
		// the get-out, the catchers: med02's floor 0 on fe3ea91a). A whole side keeps its place without a climb (THE OWNER'S COUNTS, above)
		for (size_t i = 0; i < rects.size(); i++)
		{
			if (sides[i].axis >= 0 && !climbKept[i] && (sides[i].rows != 1 || sides[i].part))
			{
				sides[i] = Side();
			}
		}

		std::vector<int> climbOf(rects.size(), -1);

		// CLIMB-IN: on each climb side, 2.4 m at the side's middle, from 100 into the room over the side's whole reach and 200 past it, the floor to the ceiling. Its centre
		// 300 under the ceiling and 100 beyond the side's face at that height (a tube's upper face is 300 out there, so 400 out: Drake's; a wall at its reach). Climbing,
		// the heading flattened onto each face (NPCSetVelocity, bh_ais.c:704-724) runs up the side to the ceiling's edge
		for (size_t i = 0; i < rects.size(); i++)
		{
			Side const &sd = sides[i];

			if (sd.axis < 0 || !climbKept[i])
			{
				continue;   // (a climb the owner's count leaves out)
			}

			if (sd.part)
			{
				plan.notes.push_back("the climb on a wall part: floor " + std::to_string((unsigned long long)i) + "'s " +
					(sd.axis == 0 ? (sd.sign > 0 ? "+x" : "-x") : (sd.sign > 0 ? "+z" : "-z")) + " side, " + std::to_string((long long)sd.from) + " .. " +
					std::to_string((long long)sd.to) + " along it (the floor goes on past the rest of it)");
			}

			Rect const &r = rects[i];
			Vol const &f = vols[i];
			int const a = sd.axis, b = a == 0 ? 2 : 0;
			double const mid = (sd.from + sd.to) / 2.0, half = std::min((double)kAutoPlotVolumeClimbLength, sd.to - sd.from) / 2.0;   // the whole side, or THE WALL PART
			Vol v;

			v.kind = Kind::Climb;
			v.floor = (int)i;
			v.lo[b] = mid - half;
			v.hi[b] = mid + half;
			v.lo[a] = sd.sign > 0 ? sd.edge - kAutoPlotVolumeClimbRoom : sd.edge - sd.reach - kAutoPlotInto;
			v.hi[a] = sd.sign > 0 ? sd.edge + sd.reach + kAutoPlotInto : sd.edge + kAutoPlotVolumeClimbRoom;
			v.lo[1] = r.ceilTop - kAutoPlotInto;
			v.hi[1] = r.floorLow + kAutoPlotInto;

			std::vector<double> c(3);

			c[(size_t)a] = sd.edge + sd.sign * ((sd.part ? sd.reach : std::min(sd.reach, (double)kAutoPlotVolumeUnder)) + kAutoPlotInto / 2.0);   // (a wall part's: past its wall)
			c[(size_t)b] = mid;
			c[1] = r.ceilTop + kAutoPlotVolumeUnder;

			// A SLOPED SIDE (a tube's two 45-degree faces: reach over kAutoPlotVolumeSloped): the climb in TWO, LOW over the lower face here and HIGH over the upper face
			// (below, after the entries). Frost's probe on hand5b (REPORT-hand5b-2026-11-20.md): held by the floor and one climb box, an alien heads for that box's centre,
			// and 300 under the ceiling it was straight over the foot (the up part dropped: no move); held by the climb box alone, it heads for the next box, and an
			// entry's centre (up and into the room) lies along the lower face's normal (no move either). LOW's centre 100 past the vertex and 100 over it: from the foot
			// out and up, into the lower face; LOW alone heads for HIGH, along the lower face; HIGH alone for its entry, along the upper face (Drake's test: a point
			// pulls an alien up the lower face when its across - y passes the vertex's, up the upper face when its across + y is short of the ceiling edge's)
			if (sd.reach > kAutoPlotVolumeSloped)
			{
				double const vy = (r.ceilTop + r.floorTop) / 2.0;   // the vertex's height (Genshd1's tubes: y 0)

				v.kind = Kind::Low;
				v.lo[1] = vy - kAutoPlotVolumeLowOver;
				c[(size_t)a] = sd.edge + sd.sign * (sd.reach + kAutoPlotInto / 2.0);
				c[1] = vy;   // at the vertex's height (Drake's hand6g, as built)
			}

			v.centres.push_back(c);
			c[(size_t)b] = mid - half / 2.0;
			v.centres.push_back(c);
			c[(size_t)b] = mid + half / 2.0;
			v.centres.push_back(c);
			climbOf[i] = (int)vols.size();
			vols.push_back(v);
		}

		if (climbCandidates == 0 && !slab)
		{
			// NO SIDE TO CLIMB (a flight of stairs: STEPS04): the levels' rule's floor-walk network, when it can shape it (Frost 11-20, Dietrich's yes; autoplot_levels.h)
			AutoPlotPlan const levels = AutoPlot_LevelsVolumes(floorIn, exits, doors, levelScale, "corridor, floor only");

			if (levels.volumes)
			{
				return levels;
			}

			plan.notes.insert(plan.notes.begin(), "NOT SHAPED: a " + plan.volumeKind + " with no side to climb (every side of its doorways' floors is a doorway or another floor; no network written)");
			return plan;
		}

		// GET-OUT (Drake's hand5b): at a corner (another ceiled floor rectangle starting at one of the biggest one's sides) over the CORNER SQUARE (the biggest one's part in
		// line with the other), else the biggest ceiled rectangle's middle 2.4 m along its climb side; less 450 on each climb side it reaches; 1001 under the ceiling (no
		// lane alien touches it) to 1500 over the floor. Its centre its bottom middle: from the ceiling straight down, so in the player's module the game drops it onto him
		int getOut = -1;
		{
			int best = -1;
			double bestArea = 0.0;

			// the biggest ceiled floor with a side; a side read as before first (WHOLE SIDES FIRST, above: med02's get-out followed its climb to the wall part's floor)
			for (bool const wholeOnly : { true, false })
			{
				if (best >= 0)
				{
					break;
				}

				for (size_t i = 0; i < rects.size(); i++)
				{
					double const area = (vols[i].hi[0] - vols[i].lo[0]) * (vols[i].hi[2] - vols[i].lo[2]);
					bool const whole = sides[i].axis >= 0 && sides[i].rows == 1 && !sides[i].part;

					if (rects[i].ceiled && sides[i].axis >= 0 && (whole || !wholeOnly) && area > bestArea)
					{
						bestArea = area;
						best = (int)i;
					}
				}
			}

			if (best >= 0 && getOutsWanted > 0)   // (the owner's count 0: none, the entries drop to the floor under them)
			{
				Rect const &r = rects[(size_t)best];
				Vol const &f = vols[(size_t)best];
				Vol v;

				v.kind = Kind::GetOut;
				v.floor = best;

				for (int a = 0; a < 3; a++)
				{
					v.lo[a] = f.lo[a];
					v.hi[a] = f.hi[a];
				}

				bool corner = false;

				for (size_t j = 0; j < floors && !corner; j++)
				{
					if (j == (size_t)best || !rects[j].ceiled || lanesAt[j] == 0)
					{
						continue;   // (a corner's or a T's other floor holds a doorway: med11's small level beside its floor made a strip of it, 11-20)
					}

					Vol const &o = vols[j];

					for (int const a : { 0, 2 })
					{
						int const b = a == 0 ? 2 : 0;

						if ((fabs(o.hi[a] - f.lo[a]) <= e || fabs(o.lo[a] - f.hi[a]) <= e) && std::min(o.hi[b], f.hi[b]) - std::max(o.lo[b], f.lo[b]) > e)
						{
							v.lo[b] = std::max(f.lo[b], o.lo[b]);
							v.hi[b] = std::min(f.hi[b], o.hi[b]);
							corner = true;
							break;
						}
					}
				}

				if (!corner)
				{
					int const along = sides[(size_t)best].axis == 0 ? 2 : 0;
					double const m = (f.lo[along] + f.hi[along]) / 2.0, half = std::min((double)kAutoPlotVolumeClimbLength, f.hi[along] - f.lo[along]) / 2.0;

					v.lo[along] = m - half;
					v.hi[along] = m + half;
				}

				for (size_t i = 0; i < rects.size(); i++)
				{
					Side const &sd = sides[i];

					if (sd.axis < 0)
					{
						continue;
					}

					if (sd.sign > 0 && fabs(v.hi[sd.axis] - sd.edge) <= e)
					{
						v.hi[sd.axis] = sd.edge - kAutoPlotClimbReach;
					}

					if (sd.sign < 0 && fabs(v.lo[sd.axis] - sd.edge) <= e)
					{
						v.lo[sd.axis] = sd.edge + kAutoPlotClimbReach;
					}
				}

				v.lo[1] = r.ceilTop + kAutoPlotVolumeHang;   // from the flat ceiling (as the entries)
				v.hi[1] = r.floorTop - kAutoPlotReach;
				v.centres.push_back(std::vector<double>{ (v.lo[0] + v.hi[0]) / 2.0, v.hi[1] - 1.0, (v.lo[2] + v.hi[2]) / 2.0 });

				if (v.hi[1] - v.lo[1] >= 300.0 && v.hi[0] - v.lo[0] > 2.0 && v.hi[2] - v.lo[2] > 2.0)
				{
					getOut = (int)vols.size();
					vols.push_back(v);
				}
			}
		}

		// ENTRY: one a doorway with a ceiling (Drake's hand5b). A CORRIDOR (two doorways at one floor rectangle): each its half, split at the middle along the climb side.
		// A CORNER (each doorway at its own rectangle): its rectangle out to the far side of the rectangles it touches, away from its doorway (over the corner square).
		// Else the ceiling places nearest that doorway. Each reaches 600 past its doorway's middle; 200 into the ceiling down to 450 under the lower of ceiling and soffit.
		// Its centre 100 UNDER the ceiling face (kAutoPlotVolumeAimUnder: a hanging alien's box holds it; Drake's spec-arrive had it 100 into the ceiling, held by no box,
		// Bishop's rule 11-20), 700 off its floor's climb side: in a corridor at the climb-in's
		// end on its half, at a corner 600 outside the other entry (toward its own doorway)
		double ceilTop = 1e300;

		for (Rect const &r : rects)
		{
			ceilTop = r.ceiled ? std::min(ceilTop, r.ceilTop) : ceilTop;
		}

		std::map<int, std::vector<std::pair<std::pair<int, int>, double> > > mine;

		for (auto const &c : ceilings)
		{
			if (c.second > ceilTop + kAutoPlotVolumeFlat || lanes.empty())
			{
				continue;   // a slope's low edge, or no doorway
			}

			double const x = ox + c.first.first * e, z = oz + c.first.second * e;
			int best = -1;
			double bestD = 1e300;

			for (int const l : lanes)
			{
				double const dx = x - doors[(size_t)l].middle[0], dz = z - doors[(size_t)l].middle[2];
				double const d = dx * dx + dz * dz;

				if (d < bestD)
				{
					bestD = d;
					best = l;
				}
			}

			mine[best].push_back(c);
		}

		std::vector<int> entryOf(exits.size(), -1), entryShape(exits.size(), 0);   // 1 corridor half, 2 corner arm, 0 places

		for (int const l : lanes)
		{
			AutoPlotDoor const &d = doors[(size_t)l];
			int const R = laneRect[(size_t)l];

			// a door slab's second doorway: its one entry reaches past this doorway too
			if (slab && l != lanes[0] && entryOf[(size_t)lanes[0]] >= 0)
			{
				Vol &v = vols[(size_t)entryOf[(size_t)lanes[0]]];

				for (double const along : { 0.0, (double)kAutoPlotVolumePast })
				{
					for (double const side : { -(double)kAutoPlotVolumeSide, (double)kAutoPlotVolumeSide })
					{
						double const px = d.middle[0] + d.through[0] * along - d.through[1] * side, pz = d.middle[2] + d.through[1] * along + d.through[0] * side;

						v.lo[0] = std::min(v.lo[0], px);
						v.hi[0] = std::max(v.hi[0], px);
						v.lo[2] = std::min(v.lo[2], pz);
						v.hi[2] = std::max(v.hi[2], pz);
					}
				}

				if (d.soffitFound)
				{
					v.lo[1] = std::min(v.lo[1], (double)d.soffit[1] - kAutoPlotInto);
					v.hi[1] = std::max(v.hi[1], (double)d.soffit[1] + kAutoPlotClimbReach);
				}

				entryOf[(size_t)l] = entryOf[(size_t)lanes[0]];
				entryShape[(size_t)l] = 4;
				continue;
			}

			bool const deadEnd = R >= 0 && lanes.size() == 1 && rects[(size_t)R].ceiled && sides[(size_t)R].axis >= 0;   // the whole rectangle, as a corridor's half
			bool const corridor = deadEnd || slab || (R >= 0 && lanesAt[(size_t)R] == 2 && rects[(size_t)R].ceiled && sides[(size_t)R].axis >= 0);
			bool const corner = R >= 0 && lanesAt[(size_t)R] == 1 && rects[(size_t)R].ceiled && lanes.size() == 2 && laneRect[(size_t)lanes[0]] != laneRect[(size_t)lanes[1]];
			bool const funnel = R >= 0 && R == funnelRect && rects[(size_t)R].ceiled;   // a T's funnel: its own rectangle only (Drake's hand6e ENTRY S)
			auto const at = mine.find(l);

			if (!corridor && !corner && !funnel && (at == mine.end() || at->second.empty()))
			{
				plan.doorWords[(size_t)l] = "no lane: no ceiling nearest this doorway";
				continue;
			}

			Vol v;
			double top = 1e300, low = -1e300;

			v.kind = Kind::Entry;
			v.exit = l;

			if (corridor || corner || funnel)
			{
				Vol const &f = vols[(size_t)R];
				Rect const &r = rects[(size_t)R];

				for (int a = 0; a < 3; a++)
				{
					v.lo[a] = f.lo[a];
					v.hi[a] = f.hi[a];
				}

				top = r.ceilTop;
				low = r.ceilTop;   // the FLAT ceiling (a light box 390 deep on gencor03's ceiling took the entries 390 lower: Drake's gold compare on 64764f17); the soffit below

				if (corridor && !deadEnd && !slab)
				{
					int const b = sides[(size_t)R].axis == 0 ? 2 : 0;
					double const mid = (f.lo[b] + f.hi[b]) / 2.0;

					(d.middle[b] < mid ? v.hi[b] : v.lo[b]) = mid;

					// THE STUB'S CEILING (Bishop's spec (A) 11-21: opscor02's two doorways both lie in its north half, so its south 7.5 m had no ceiling box, the closers left
					// its end wall, and aliens climbing it out of the floor box were in no box: Hudson 658 stalled frames in whole-2b): when the corridor's other doorway is in
					// the same half, the floor runs on past the last doorway into a stub, and the entry of the doorway nearer it (the first lane of two as near) reaches on
					// over the stub to the floor's end, so the ceiling lane ends at the dead end. Only the box's reach changes: its aim point is set below, as before
					for (int const o : lanes)
					{
						if (o == l || laneRect[(size_t)o] != R || (d.middle[b] < mid) != (doors[(size_t)o].middle[b] < mid))
						{
							continue;   // not the other doorway of this corridor's floor, or in the other half (no stub)
						}

						double const here = d.middle[b], there = doors[(size_t)o].middle[b];

						if (here >= mid && (here < there || (here == there && l < o)))
						{
							v.lo[b] = f.lo[b];   // the stub is the low half, and this doorway the nearer it
						}
						else if (here < mid && (here > there || (here == there && l < o)))
						{
							v.hi[b] = f.hi[b];   // the stub is the high half
						}
					}
				}
				else if (corridor || funnel)
				{
					// a dead end, a T's funnel: the whole rectangle
				}
				else
				{
					int const t = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2;
					double const tt = t == 0 ? d.through[0] : d.through[1];

					for (size_t j = 0; j < floors; j++)
					{
						if (j != (size_t)R && Touch(vols[j], f, e / 2.0))
						{
							if (tt < 0)
							{
								v.hi[t] = std::max(v.hi[t], vols[j].hi[t]);
							}
							else
							{
								v.lo[t] = std::min(v.lo[t], vols[j].lo[t]);
							}
						}
					}
				}

				entryShape[(size_t)l] = slab ? 4 : (corridor ? 1 : (funnel ? 3 : 2));   // 4: a door slab's one entry
			}
			else
			{
				v.lo[0] = v.lo[2] = 1e300;
				v.hi[0] = v.hi[2] = -1e300;

				for (auto const &c : at->second)
				{
					v.lo[0] = std::min(v.lo[0], cellLo(c.first.first, ox));
					v.hi[0] = std::max(v.hi[0], cellHi(c.first.first, ox));
					v.lo[2] = std::min(v.lo[2], cellLo(c.first.second, oz));
					v.hi[2] = std::max(v.hi[2], cellHi(c.first.second, oz));
					top = std::min(top, c.second);
					low = std::max(low, c.second);
				}
			}

			// the doorway's top to kAutoPlotVolumePast past its middle
			double const tx = d.through[0], tz = d.through[1];

			for (double const along : { 0.0, (double)kAutoPlotVolumePast })
			{
				for (double const side : { -(double)kAutoPlotVolumeSide, (double)kAutoPlotVolumeSide })
				{
					double const px = d.middle[0] + tx * along - tz * side, pz = d.middle[2] + tz * along + tx * side;

					v.lo[0] = std::min(v.lo[0], px);
					v.hi[0] = std::max(v.hi[0], px);
					v.lo[2] = std::min(v.lo[2], pz);
					v.hi[2] = std::max(v.hi[2], pz);
				}
			}

			if (d.soffitFound)
			{
				top = std::min(top, (double)d.soffit[1]);
				low = std::max(low, (double)d.soffit[1]);
			}

			v.lo[1] = top - kAutoPlotInto;
			v.hi[1] = low + kAutoPlotClimbReach;

			// the fallback's centres: inside the ceiling over its places nearest the doorway (the shaped ones are set below, once every entry is made)
			if (!corridor && !corner)
			{
				std::vector<std::pair<double, std::vector<double> > > byDoor;

				for (auto const &c : at->second)
				{
					double const x = ox + c.first.first * e, z = oz + c.first.second * e;
					double const dx = x - d.middle[0], dz = z - d.middle[2];

					byDoor.push_back(std::make_pair(dx * dx + dz * dz, std::vector<double>{ x, c.second + kAutoPlotVolumeAimUnder, z }));
				}

				std::sort(byDoor.begin(), byDoor.end(), [](std::pair<double, std::vector<double> > const &p, std::pair<double, std::vector<double> > const &q) { return p.first < q.first; });

				for (size_t k = 0; k < byDoor.size() && k < 12; k++)
				{
					v.centres.push_back(byDoor[k].second);
				}
			}

			entryOf[(size_t)l] = (int)vols.size();
			vols.push_back(v);
		}

		// the shaped entries' centres
		for (int const l : lanes)
		{
			if (entryOf[(size_t)l] < 0 || entryShape[(size_t)l] == 0 || entryShape[(size_t)l] == 3)
			{
				continue;   // (a T's funnel entry: below, from the bar's)
			}

			Vol &v = vols[(size_t)entryOf[(size_t)l]];
			int const R = laneRect[(size_t)l];
			Side const &sd = sides[(size_t)R];
			std::vector<double> c(3);

			c[1] = v.lo[1] + kAutoPlotInto + kAutoPlotVolumeAimUnder;   // 100 under the ceiling face (v.lo[1] is 200 into it)

			if (entryShape[(size_t)l] == 4)
			{
				// a door slab's one entry: its centre over the slab's middle (once, for its first doorway)
				if (l == lanes[0])
				{
					c[0] = (vols[(size_t)R].lo[0] + vols[(size_t)R].hi[0]) / 2.0;
					c[2] = (vols[(size_t)R].lo[2] + vols[(size_t)R].hi[2]) / 2.0;
					v.centres.insert(v.centres.begin(), c);
				}

				continue;
			}

			if (sd.axis >= 0)
			{
				c[(size_t)sd.axis] = sd.edge - sd.sign * kAutoPlotVolumeOffWall;
			}

			int const b = sd.axis == 0 ? 2 : 0;

			if (entryShape[(size_t)l] == 1 && climbOf[(size_t)R] >= 0)
			{
				Vol const &climb = vols[(size_t)climbOf[(size_t)R]];

				AutoPlotDoor const &d = doors[(size_t)l];

				c[(size_t)b] = fabs(climb.lo[b] - d.middle[b]) <= fabs(climb.hi[b] - d.middle[b]) ? climb.lo[b] : climb.hi[b];   // the climb-in's end nearer its doorway

				// THE ENTRIES' AIMS ON A WALL PART (Bishop's spec (c) 11-21): a whole side's climb (2.4 m at the side's middle) reaches into both halves, its ends 1.2 m
				// either side of the middle line where the two entries meet; a wall part's climb can lie anywhere along the side, wholly in one half (new01: the west entry
				// aimed 150 mm into the east one's box, none fit and the volume repair moved it) or with its end near the line (new01's east entry, 150 mm from it), where a
				// hanging body (450 each way) is in both entries and the game, taking the last box holding it as its own, never goes on (Hudson floor-only round 1: 291 of
				// new01's 449 stalled frames there, held by both). So on a wall part each entry's aim stays at least kAutoPlotVolumeEntryClear inside its own half, at
				// both its ends (the middle line and the doorway's), or at its middle when the half is narrower than that twice
				if (sd.part)
				{
					double const clear = (double)kAutoPlotVolumeEntryClear;

					c[(size_t)b] = v.hi[b] - v.lo[b] < 2.0 * clear ? (v.lo[b] + v.hi[b]) / 2.0 : std::min(std::max(c[(size_t)b], v.lo[b] + clear), v.hi[b] - clear);
				}
			}
			else
			{
				// 600 outside the other entry, toward this one's doorway (along the doorway's way through)
				AutoPlotDoor const &d = doors[(size_t)l];
				int const t = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2;
				double const tt = t == 0 ? d.through[0] : d.through[1];

				c[(size_t)t] = (v.lo[t] + v.hi[t]) / 2.0;

				for (int const o : lanes)
				{
					if (o != l && entryOf[(size_t)o] >= 0)
					{
						Vol const &other = vols[(size_t)entryOf[(size_t)o]];

						c[(size_t)t] = tt < 0 ? other.lo[t] - kAutoPlotVolumeEntryClear : other.hi[t] + kAutoPlotVolumeEntryClear;
					}
				}

				if (sd.axis < 0 || sd.axis == t)
				{
					c[(size_t)(t == 0 ? 2 : 0)] = (v.lo[t == 0 ? 2 : 0] + v.hi[t == 0 ? 2 : 0]) / 2.0;
				}
			}

			v.centres.insert(v.centres.begin(), c);
		}

		// A T'S FUNNEL ENTRY (Drake's hand6e ENTRY S): its centre straight across from ONE bar entry's (the lower one along the bar: not on the line where the bar's two
		// entries meet, where an alien came within 450 of the other, was held by both and shuttled), 2.4 m into the funnel from the bar; linked to that bar entry only
		// (to the other too, the routes from the climb's top tied at 3 and the start flipped)
		int funnelEntry = -1, funnelPartner = -1;

		for (int const l : lanes)
		{
			if (entryOf[(size_t)l] < 0 || entryShape[(size_t)l] != 3 || barRect < 0 || sides[(size_t)barRect].axis < 0)
			{
				continue;
			}

			Vol &v = vols[(size_t)entryOf[(size_t)l]];
			Vol const &F = vols[(size_t)funnelRect], &B = vols[(size_t)barRect];
			int const b = sides[(size_t)barRect].axis == 0 ? 2 : 0, s = sides[(size_t)barRect].axis;

			for (int const o : lanes)
			{
				if (o != l && entryOf[(size_t)o] >= 0 && laneRect[(size_t)o] == barRect && !vols[(size_t)entryOf[(size_t)o]].centres.empty() &&
					(funnelPartner < 0 || vols[(size_t)entryOf[(size_t)o]].centres[0][(size_t)b] < vols[(size_t)funnelPartner].centres[0][(size_t)b]))
				{
					funnelPartner = entryOf[(size_t)o];
				}
			}

			if (funnelPartner < 0)
			{
				continue;
			}

			std::vector<double> c(3);
			bool const below = fabs(F.hi[s] - B.lo[s]) <= fabs(F.lo[s] - B.hi[s]);   // the funnel on the bar's low side

			c[1] = v.lo[1] + kAutoPlotInto + kAutoPlotVolumeAimUnder;   // 100 under the ceiling face (v.lo[1] is 200 into it)
			c[(size_t)b] = vols[(size_t)funnelPartner].centres[0][(size_t)b];
			c[(size_t)s] = below ? std::max(B.lo[s] - kAutoPlotVolumeClimbLength, F.lo[s] + 300.0) : std::min(B.hi[s] + kAutoPlotVolumeClimbLength, F.hi[s] - 300.0);
			v.centres.insert(v.centres.begin(), c);
			funnelEntry = entryOf[(size_t)l];
		}

		// HIGH: every SLOPED side of a doorway's floor (its climb side, and any other side with no doorway on it) over its upper face, its whole length, one piece for each
		// entry over it (a corridor's halves: split at its middle), so an alien that climbs or jumps onto an upper face is always in a box (Frost's probe on hand5b: gencor03
		// f=118 landed on the +z chamfer 2.1 m from the climb, in no box, and fell). Each piece up into ITS entry only: at the climb's top an alien is held by a piece and
		// that entry, and the piece's route is one box longer, so the start is the entry (one piece under two entries tied at 3 boxes). A climb side's pieces: LOW -> piece
		// -> entry, one way; another side's (jump catchers): piece <-> entry. NONE on a side another floor starts at (a corner's inner side, a T's bar where its funnel
		// opens: the 9a4a5b7c run's catchers there held SHED10's funnel entry's centre, refused) or past which the floor goes on at its height (a diagonal side in plan:
		// a corner's inner staircase): no upper face there (Drake's hand6g has none)
		std::vector<std::pair<size_t, size_t> > highUp, highBoth, lowHigh;

		struct Piece
		{
			Vol h;                       // its box (the centres set once the pieces are joined)
			size_t owner = 0;            // the entry it goes up into
			size_t lowAt = (size_t)-1;   // the climb side's LOW under it, if any
			double lowLo = 0.0, lowHi = 0.0;
			int a = 0, b = 0, s = 0;
			double edge = 0.0, ceilTop = 0.0;
		};

		std::vector<Piece> pieces;

		for (size_t i = 0; i < rects.size(); i++)
		{
			if (!rects[i].ceiled || lanesAt[i] == 0 || (int)i == funnelRect || slab)
			{
				continue;
			}

			Vol const f = vols[i];
			Rect const r = rects[i];
			double const vy = (r.ceilTop + r.floorTop) / 2.0;

			for (int const a : { 0, 2 })
			{
				int const b = a == 0 ? 2 : 0;

				for (int const s : { 1, -1 })
				{
					double const edge = s > 0 ? f.hi[a] : f.lo[a], length = f.hi[b] - f.lo[b];
					bool const door = doorOn(i, a, s);
					double const beyond = floorBeyond(i, a, s, sides[i].axis == a && sides[i].sign == s ? sides[i].rows : 1);   // (the climb side as it was chosen; THE WALL FOOT)
					double seam = 0.0;

					for (size_t j = 0; j < floors; j++)
					{
						if (j != i && fabs((s > 0 ? vols[j].lo[a] : vols[j].hi[a]) - edge) <= e)
						{
							seam += std::max(0.0, std::min(vols[j].hi[b], f.hi[b]) - std::max(vols[j].lo[b], f.lo[b]));
						}
					}

					double const boxEdge = !floorIn.boxFound ? edge : (s > 0 ? (double)floorIn.boxHi[a] : (double)floorIn.boxLo[a]);
					double const boxReach = std::min(std::max((boxEdge - edge) * s, 0.0), 3000.0);
					double const reach = boxReach > kAutoPlotVolumeSloped ? std::min(boxReach, (r.floorTop - r.ceilTop) / 2.0) : boxReach;   // (the vertex: as the climb side's)

					if (door || seam > e / 2.0 || beyond > length / 4.0 || ceilingBeyond(i, a, s) > length / 4.0 || reach <= kAutoPlotVolumeSloped)
					{
						continue;
					}

					bool const climbSide = sides[i].axis == a && sides[i].sign == s;

					// the side's pieces: split at every entry's end along it. Each piece to the entry over it whose doorway is at ANOTHER floor (a corner square is under
					// both arms' entries: hand6g's north piece there and its east side go to ENTRY B, arm 2's, and arm 1's own part to ENTRY A), then the one that starts
					// latest; pieces of one entry joined
					std::vector<size_t> over;
					std::vector<double> cuts(1, f.lo[b]);

					for (size_t j = 0; j < vols.size(); j++)
					{
						if (vols[j].kind == Kind::Entry && vols[j].lo[b] < f.hi[b] && vols[j].hi[b] > f.lo[b] && (s > 0 ? vols[j].hi[a] >= edge - e : vols[j].lo[a] <= edge + e))
						{
							over.push_back(j);

							for (double const at : { vols[j].lo[b], vols[j].hi[b] })
							{
								if (at > f.lo[b] && at < f.hi[b])
								{
									cuts.push_back(at);
								}
							}
						}
					}

					cuts.push_back(f.hi[b]);
					std::sort(cuts.begin(), cuts.end());

					auto const ownFloor = [&](size_t j) { return vols[j].exit >= 0 && laneRect[(size_t)vols[j].exit] == (int)i; };
					std::vector<std::pair<std::pair<double, double>, size_t> > spans;   // (from, to), entry

					for (size_t k = 0; k + 1 < cuts.size(); k++)
					{
						double const from = cuts[k], to = cuts[k + 1], at = (from + to) / 2.0;
						size_t owner = (size_t)-1;

						for (size_t const j : over)
						{
							if (vols[j].lo[b] <= at && vols[j].hi[b] >= at && (owner == (size_t)-1 || (ownFloor(owner) && !ownFloor(j)) ||
								(ownFloor(owner) == ownFloor(j) && vols[j].lo[b] > vols[owner].lo[b])))
							{
								owner = j;
							}
						}

						if (owner == (size_t)-1 || to - from < 1.0)
						{
							continue;
						}

						if (!spans.empty() && spans.back().second == owner && fabs(spans.back().first.second - from) < 1.0)
						{
							spans.back().first.second = to;
						}
						else
						{
							spans.push_back(std::make_pair(std::make_pair(from, to), owner));
						}
					}

					Vol const *low = climbSide && climbOf[i] >= 0 && vols[(size_t)climbOf[i]].kind == Kind::Low ? &vols[(size_t)climbOf[i]] : nullptr;
					double const lowLo = low ? low->lo[b] : 0.0, lowHi = low ? low->hi[b] : 0.0;

					for (std::pair<std::pair<double, double>, size_t> const &sp : spans)
					{
						double const from = sp.first.first, to = sp.first.second;

						if (to - from < kAutoPlotVolumeSide)
						{
							continue;
						}

						Piece p;

						p.h.kind = Kind::High;
						p.h.floor = (int)i;
						p.h.exit = vols[sp.second].exit;
						p.h.lo[b] = from;
						p.h.hi[b] = to;
						p.h.lo[a] = s > 0 ? edge - kAutoPlotVolumeClimbRoom : edge - reach - kAutoPlotInto;
						p.h.hi[a] = s > 0 ? edge + reach + kAutoPlotInto : edge + kAutoPlotVolumeClimbRoom;
						p.h.lo[1] = r.ceilTop - kAutoPlotInto;
						p.h.hi[1] = vy - 1.0;   // down to the vertex's height (less 1: LOW's centre is there), 500 under LOW's top: the hand-over (Drake's hand6g)
						p.owner = sp.second;
						p.a = a;
						p.b = b;
						p.s = s;
						p.edge = edge;
						p.ceilTop = r.ceilTop;

						if (low && lowLo < to && lowHi > from)
						{
							p.lowAt = (size_t)climbOf[i];
							p.lowLo = lowLo;
							p.lowHi = lowHi;
						}

						pieces.push_back(p);
					}
				}
			}
		}

		// ONE PIECE ALONG A SIDE: two floors' pieces on one line, end to end, into one entry are one (hand6g's SHED01 east piece over both arms, up into ENTRY B)
		for (bool joined = true; joined;)
		{
			joined = false;

			for (size_t p = 0; p < pieces.size() && !joined; p++)
			{
				for (size_t q = p + 1; q < pieces.size() && !joined; q++)
				{
					Piece &P = pieces[p];
					Piece const &Q = pieces[q];

					if (P.owner != Q.owner || P.a != Q.a || P.s != Q.s || fabs(P.edge - Q.edge) > 1.0 ||
						(P.lowAt != (size_t)-1 && Q.lowAt != (size_t)-1 && P.lowAt != Q.lowAt) ||
						(fabs(P.h.hi[P.b] - Q.h.lo[P.b]) >= 1.0 && fabs(Q.h.hi[P.b] - P.h.lo[P.b]) >= 1.0))
					{
						continue;
					}

					for (int k = 0; k < 3; k++)
					{
						P.h.lo[k] = std::min(P.h.lo[k], Q.h.lo[k]);
						P.h.hi[k] = std::max(P.h.hi[k], Q.h.hi[k]);
					}

					if (P.lowAt == (size_t)-1)
					{
						P.lowAt = Q.lowAt;
						P.lowLo = Q.lowLo;
						P.lowHi = Q.lowHi;
					}

					P.ceilTop = std::min(P.ceilTop, Q.ceilTop);
					pieces.erase(pieces.begin() + (std::ptrdiff_t)q);
					joined = true;
				}
			}
		}

		for (Piece &p : pieces)
		{
			int const a = p.a, b = p.b;
			double const from = p.h.lo[b], to = p.h.hi[b];
			bool const touchesLow = p.lowAt != (size_t)-1;
			double along = (from + to) / 2.0;

			// its centre 100 under the ceiling's edge, just past the floor's edge (outside its entry): on the upper face's line, so a climber on the face heads along it,
			// never into the chamfer (Bishop's rule 11-20; Drake's hand6g had it 200 over the edge, inside the rock). A climb-side piece's straight over the
			// climb when it runs on past both ends of the climb's middle (hand6g's corner pieces), else the run's along (THE OWNER'S SIZES: kAutoPlotVolumeHighAlong unless the
			// dialog gave another) off that middle on its side (a corridor's halves), kept kAutoPlotVolumeHighEndClear in from the piece's ends; another's at its middle
			if (touchesLow)
			{
				double const mid = (p.lowLo + p.lowHi) / 2.0;
				bool const over = mid > from + kAutoPlotVolumeSide && mid < to - kAutoPlotVolumeSide;

				along = over ? mid : ((from + to) / 2.0 < mid ? mid - used.highAlong : mid + used.highAlong);

				if (!over)
				{
					along = to - from > 2.0 * kAutoPlotVolumeHighEndClear ? std::min(std::max(along, from + kAutoPlotVolumeHighEndClear), to - kAutoPlotVolumeHighEndClear) : (from + to) / 2.0;
				}

				along = std::min(std::max(along, from + 1.0), to - 1.0);
			}

			// ACROSS AND IN HEIGHT: a CLIMB-SIDE piece's aim on the upper face near the crease, the run's up (kAutoPlotVolumeHighUp unless the dialog gave another) up it and its
			// under (kAutoPlotVolumeHighUnder) under it (Vasquez's rule 2: from where the climb lands it heads along the face it is on, never into it), when the face is
			// long enough and the aim clears LOW; else (a jump
			// catcher, a short face) 100 under the ceiling's edge, just past the floor's edge. Out from the side: the crease reach out at the vertex's height, the
			// ceiling's edge at the side over the floor's edge
			double acrossAim = p.edge + p.s * 1.0, heightAim = p.ceilTop + kAutoPlotVolumeAimUnder;

			if (touchesLow)
			{
				double const reach = (p.s > 0 ? p.h.hi[a] - p.edge : p.edge - p.h.lo[a]) - kAutoPlotInto, vy = p.h.hi[1] + 1.0, rise = vy - p.ceilTop;
				double const face = sqrt(reach * reach + rise * rise);

				if (reach > 0.0 && rise > 0.0 && face >= used.highUp + used.highUnder)
				{
					// (out from the side: up the face goes back toward the side and up; under it, back out from the side and down)
					double const out = reach - used.highUp * reach / face - used.highUnder * rise / face;
					double const y = vy - used.highUp * rise / face + used.highUnder * reach / face;

					if (out > 1.0 && y < vy - kAutoPlotVolumeLowOver - 50.0 && y > p.ceilTop)
					{
						acrossAim = p.edge + p.s * out;
						heightAim = y;
					}
				}
			}

			for (double const shift : { 0.0, -0.25, 0.25 })
			{
				std::vector<double> c(3);

				c[(size_t)a] = acrossAim;
				c[(size_t)b] = std::min(std::max(along + shift * (to - from), from + 1.0), to - 1.0);
				c[1] = heightAim;
				p.h.centres.push_back(c);
			}

			size_t const piece = vols.size();

			vols.push_back(p.h);

			if (touchesLow)
			{
				highUp.push_back(std::make_pair(piece, p.owner));   // LOW -> piece -> its entry
				lowHigh.push_back(std::make_pair(p.lowAt, piece));
			}
			else
			{
				highBoth.push_back(std::make_pair(piece, p.owner));   // a jump catcher: one link to its entry, both ways
			}
		}

		// EXIT: one an exit, the doorway passage (or, with no doorway known, round the point)
		std::vector<int> exitOf(exits.size(), -1);

		for (size_t x = 0; x < exits.size(); x++)
		{
			if (exits[x].size() != 3 || twinOf[x] >= 0)
			{
				continue;   // (a twin: its point goes in the first one's box, below)
			}

			double const p[3] = { (double)exits[x][0], (double)exits[x][1], (double)exits[x][2] };
			bool const door = x < doors.size() && doors[x].throughFound;
			// the floor under the point: the nearest cell's
			double floorY = p[1] + kAutoPlotHangClear, bestD = 1e300;

			for (auto const &c : cells)
			{
				double const dx = ox + c.first.first * e - p[0], dz = oz + c.first.second * e - p[2];

				if (dx * dx + dz * dz < bestD)
				{
					bestD = dx * dx + dz * dz;
					floorY = c.second;
				}
			}

			Vol v;

			v.kind = Kind::Exit;
			v.exit = (int)x;
			v.hi[1] = std::max(floorY, p[1]) + kAutoPlotInto;

			if (door)
			{
				AutoPlotDoor const &d = doors[x];
				double const tx = d.through[0], tz = d.through[1];

				v.lo[0] = v.lo[2] = 1e300;
				v.hi[0] = v.hi[2] = -1e300;

				for (double const along : { -(double)kAutoPlotVolumePast, (double)kAutoPlotVolumeExitPast })
				{
					for (double const side : { -(double)kAutoPlotVolumeSide, (double)kAutoPlotVolumeSide })
					{
						double const px = d.middle[0] + tx * along - tz * side, pz = d.middle[2] + tz * along + tx * side;

						v.lo[0] = std::min(v.lo[0], px);
						v.hi[0] = std::max(v.hi[0], px);
						v.lo[2] = std::min(v.lo[2], pz);
						v.hi[2] = std::max(v.hi[2], pz);
					}
				}

				double const soffit = d.soffitFound ? (double)d.soffit[1] : p[1] - kAutoPlotVolumeTopClear - 1.0;

				v.lo[1] = soffit + kAutoPlotVolumeTopClear;

				if (v.lo[1] > p[1] - 1.0)
				{
					v.lo[1] = p[1] - 1.0;   // the doorway is too low for a pure target: the point kept in its box
					plan.doorWords[x] = plan.doorWords[x].empty() ? "no lane: the exit point is less than 1000 mm under the doorway's top" : plan.doorWords[x];
				}

				// at MID-DOOR height (Genshd1's exit points lie on the floor: Drake's hand5b), held inside the box
				double const midDoor = std::min(std::max((soffit + floorY) / 2.0, v.lo[1] + 1.0), v.hi[1] - 2.0);

				v.centres.push_back(std::vector<double>{ d.middle[0] + tx * kAutoPlotAimPast, midDoor, d.middle[2] + tz * kAutoPlotAimPast });
				v.centres.push_back(std::vector<double>{ p[0], p[1], p[2] });
			}
			else
			{
				v.lo[0] = p[0] - kAutoPlotVolumeSide;
				v.hi[0] = p[0] + kAutoPlotVolumeSide;
				v.lo[2] = p[2] - kAutoPlotVolumeSide;
				v.hi[2] = p[2] + kAutoPlotVolumeSide;
				v.lo[1] = p[1] - kAutoPlotHangClear;
				v.centres.push_back(std::vector<double>{ p[0], p[1], p[2] });
			}

			exitOf[x] = (int)vols.size();
			vols.push_back(v);
		}

		// a twin exit: the first one's exit box, grown to hold its point too, and the first one's entry
		for (size_t x = 0; x < exits.size(); x++)
		{
			int const first = twinOf[x];

			if (first < 0 || exitOf[(size_t)first] < 0 || exits[x].size() != 3)
			{
				continue;
			}

			Vol &v = vols[(size_t)exitOf[(size_t)first]];

			for (int k = 0; k < 3; k++)
			{
				v.lo[k] = std::min(v.lo[k], (double)exits[x][(size_t)k] - 1.0);
				v.hi[k] = std::max(v.hi[k], (double)exits[x][(size_t)k] + 2.0);
			}

			exitOf[x] = exitOf[(size_t)first];
			entryOf[x] = entryOf[(size_t)first];
			plan.doorWords[x] = plan.doorWords[x].empty() ? "lane (through exit " + std::to_string((long long)first) + "'s doorway)" : plan.doorWords[x];
		}

		// THE STAIR BETWEEN TWO FLOORS (autoplot_stairs.h; Dietrich 11-21: the owner's POC rooms opscen03 / opscen02 were refused, "box 0 cannot reach box 1"). The treads
		// no floor rectangle holds, between two floors more than a step apart, get ONE FLOOR BOX over them, its top kAutoPlotStairUnderCeiling under the lowest ceiling over
		// them (every floor box keeps clear of a hanging alien), linked to both floors; and ONE BOX IN THE CEILING over them, made as an entry is (into the ceiling, and a
		// climbing alien's reach under its lowest part), linked to every entry it touches. So the floor walks from one floor to the other, and the ceiling goes from one
		// doorway's entry to the other's, as in Drake's hand-built hand4 (the owner's in-game PASS). Made before the gap closers, so they see these boxes in the walls' band.
		// The links are made with the others, below (THE STAIRS' LINKS)
		struct StairVols
		{
			size_t floor = 0;
			int ceiling = -1;          // -1: no ceiling over the treads, so no box there
			std::vector<int> joins;    // the floor rectangles it meets (vols[i] is rects[i]'s floor box for i < floors)
		};

		std::vector<StairVols> stairs;

		{
			std::vector<AutoPlotStairFloor> flats;

			for (Rect const &r : rects)
			{
				AutoPlotStairFloor f;

				f.ix0 = r.ix0;
				f.ix1 = r.ix1;
				f.iz0 = r.iz0;
				f.iz1 = r.iz1;
				f.floorTop = r.floorTop;
				flats.push_back(f);
			}

			for (AutoPlotStair const &s : AutoPlot_Stairs(cells, ceilings, flats))
			{
				std::string between;

				for (int const f : s.joins)
				{
					between += (between.empty() ? "floors " : " and ") + std::to_string((long long)f);
				}

				// the floor box: the treads' cells across, 200 under the lowest tread, up to kAutoPlotReach over the highest or kAutoPlotStairUnderCeiling under the
				// lowest ceiling over them, whichever is lower. Too low over the highest tread to hold an alien standing there: no stair box, said
				Vol v;

				v.kind = Kind::Floor;
				v.lo[0] = cellLo(s.ix0, ox);
				v.hi[0] = cellHi(s.ix1, ox);
				v.lo[2] = cellLo(s.iz0, oz);
				v.hi[2] = cellHi(s.iz1, oz);
				v.lo[1] = s.ceiled ? std::max(s.floorTop - kAutoPlotReach, s.ceilLow + kAutoPlotStairUnderCeiling) : s.floorTop - kAutoPlotReach;
				v.hi[1] = s.floorLow + kAutoPlotInto;

				if (v.lo[1] > s.floorTop - kAutoPlotStairBody)
				{
					plan.notes.push_back("a stair between " + between + " left out: the ceiling over it is too low over its highest tread for a floor box");
					continue;
				}

				for (double const fx : { 0.5, 0.25, 0.75 })
				{
					for (double const fz : { 0.5, 0.25, 0.75 })
					{
						v.centres.push_back(std::vector<double>{ v.lo[0] + fx * (v.hi[0] - v.lo[0]), s.floorTop - kAutoPlotStairBody / 2.0, v.lo[2] + fz * (v.hi[2] - v.lo[2]) });
					}
				}

				StairVols sv;

				sv.floor = vols.size();
				sv.joins = s.joins;
				vols.push_back(v);

				// the ceiling box over the treads, as an entry is made: kAutoPlotInto into the ceiling at its highest, kAutoPlotClimbReach under it at its lowest; its centre
				// kAutoPlotVolumeAimUnder under the ceiling over the cells nearest its middle (the ceiling rises with the treads)
				if (s.ceiled)
				{
					Vol c;
					double const mx = (v.lo[0] + v.hi[0]) / 2.0, mz = (v.lo[2] + v.hi[2]) / 2.0;
					std::vector<std::pair<double, std::vector<double> > > byMiddle;

					c.kind = Kind::StairCeiling;
					c.lo[0] = v.lo[0];
					c.hi[0] = v.hi[0];
					c.lo[2] = v.lo[2];
					c.hi[2] = v.hi[2];
					c.lo[1] = s.ceilTop - kAutoPlotInto;
					c.hi[1] = s.ceilLow + kAutoPlotClimbReach;

					for (int ix = s.ix0; ix <= s.ix1; ix++)
					{
						for (int iz = s.iz0; iz <= s.iz1; iz++)
						{
							auto const floorAt = cells.find(std::make_pair(ix, iz));
							auto const roof = ceilings.find(std::make_pair(ix, iz));

							if (floorAt != cells.end() && roof != ceilings.end() && floorAt->second - roof->second >= kAutoPlotStairRoofLeast)
							{
								double const x = ox + ix * e, z = oz + iz * e;

								byMiddle.push_back(std::make_pair((x - mx) * (x - mx) + (z - mz) * (z - mz), std::vector<double>{ x, roof->second + kAutoPlotVolumeAimUnder, z }));
							}
						}
					}

					std::sort(byMiddle.begin(), byMiddle.end(), [](std::pair<double, std::vector<double> > const &p, std::pair<double, std::vector<double> > const &q) { return p.first < q.first; });

					for (size_t k = 0; k < byMiddle.size() && k < 12; k++)
					{
						c.centres.push_back(byMiddle[k].second);
					}

					if (c.centres.empty())
					{
						c.centres.push_back(std::vector<double>{ mx, s.ceilTop + kAutoPlotVolumeAimUnder, mz });   // (never so: ceiled means one such cell at least)
					}

					sv.ceiling = (int)vols.size();
					vols.push_back(c);
				}

				stairs.push_back(sv);
				plan.notes.push_back("a stair between " + between + ": a floor box over its treads" + (s.ceiled ? std::string(" and a box in the ceiling over them") : std::string()));
			}
		}

		// THE GAP CLOSERS (autoplot_closers.h; Vasquez 11-21, the owner: "Gaps first", "fitter boxes as large as possible within the space we have and 'tetris together'").
		// With the switch on: along each upright wall side of a ceiled floor, the band from the floor box's top up to the entries over it, less whatever box or aim point
		// already stands in it, is closed by the fewest, largest boxes that fit (kAutoPlotCloserLeast..kAutoPlotCloserMost along and in height, kAutoPlotCloserDepth out
		// from the face), touching what is there. Each side the rule leaves is named in the notes, with why
		if (AutoPlotClosers_On() && !slab)
		{
			size_t const before = vols.size();
			std::vector<std::string> made, left;
			auto const mm = [](double v) { return std::to_string((long long)floor(v + 0.5)); };

			for (size_t i = 0; i < floors && i < rects.size(); i++)
			{
				if (!rects[i].ceiled)
				{
					continue;   // no ceiling over the floor: no entry, no band (open sky is its own rule)
				}

				for (int const a : { 0, 2 })
				{
					int const b = a == 0 ? 2 : 0;

					for (int const s : { 1, -1 })
					{
						Vol const f = vols[i];   // (a copy: vols grows below)
						double const edge = s > 0 ? f.hi[a] : f.lo[a], length = f.hi[b] - f.lo[b];
						std::string const sideName = "floor " + std::to_string((unsigned long long)i) + "'s " + (s > 0 ? "+" : "-") + (a == 0 ? "x" : "z") + " side";
						double seam = 0.0;

						// THE WALL TESTS, the climb side's own (above): no doorway on it, no other floor along it, the floor and the ceiling not going on past it
						for (size_t j = 0; j < floors; j++)
						{
							if (j != i && fabs((s > 0 ? vols[j].lo[a] : vols[j].hi[a]) - edge) <= e)
							{
								seam += std::max(0.0, std::min(vols[j].hi[b], f.hi[b]) - std::max(vols[j].lo[b], f.lo[b]));
							}
						}

						if (doorOn(i, a, s))
						{
							left.push_back(sideName + ": a doorway on it (the doorway sides are the next rule)");
							continue;
						}

						if (seam > e / 2.0 || floorBeyond(i, a, s, sides[i].axis == a && sides[i].sign == s ? sides[i].rows : 1) > length / 4.0 || ceilingBeyond(i, a, s) > length / 4.0)
						{
							left.push_back(sideName + ": open (another floor along it, or the floor or the ceiling going on past it)");
							continue;
						}

						// THE FACE: as the climb side's, where the AI module's own box ends past the floor's edge (a wall at the edge or near it); a tube's sloped side is LOW and HIGH's
						double const boxEdge = !floorIn.boxFound ? edge : (s > 0 ? (double)floorIn.boxHi[a] : (double)floorIn.boxLo[a]);
						double const reach = std::min(std::max((boxEdge - edge) * s, 0.0), 3000.0);

						if (reach > kAutoPlotVolumeSloped)
						{
							left.push_back(sideName + ": sloped (its face " + mm(reach) + " mm past the floor: a tube's, LOW and HIGH climb there)");
							continue;
						}

						double const face = edge + s * reach;
						double lo[3], hi[3];

						lo[a] = s > 0 ? face - kAutoPlotCloserDepth : face - kAutoPlotInto;
						hi[a] = s > 0 ? face + kAutoPlotInto : face + kAutoPlotCloserDepth;
						lo[b] = f.lo[b];
						hi[b] = f.hi[b];
						hi[1] = f.lo[1];   // the band's foot: the floor box's top (y is down)

						// THE BAND'S TOP: the lowest bottom of the entries over this side (so no entry reaches into it)
						bool entryOver = false;

						lo[1] = -1e300;

						for (size_t j = floors; j < vols.size(); j++)
						{
							Vol const &en = vols[j];

							if (en.kind == Kind::Entry && std::min(en.hi[0], hi[0]) > std::max(en.lo[0], lo[0]) && std::min(en.hi[2], hi[2]) > std::max(en.lo[2], lo[2]))
							{
								lo[1] = std::max(lo[1], en.hi[1]);
								entryOver = true;
							}
						}

						if (!entryOver)
						{
							left.push_back(sideName + ": no entry over it");
							continue;
						}

						double const band = hi[1] - lo[1];

						if (band < kAutoPlotCloserLeast)
						{
							left.push_back(sideName + ": its band is " + mm(band) + " mm, under a body's height (a body there is always in the floor box or an entry)");
							continue;
						}

						// WHAT ALREADY STANDS IN THE BAND along this side: every box reaching into it, and every aim point in it (no closer may hold another box's)
						std::vector<std::pair<double, double> > blocked;
						int climbHere = -1;

						for (size_t j = 0; j < vols.size(); j++)
						{
							Vol const &o = vols[j];
							bool in = j != i;

							for (int k = 0; k < 3 && in; k++)
							{
								in = o.lo[k] < hi[k] && lo[k] < o.hi[k];
							}

							if (in)
							{
								blocked.push_back(std::make_pair(o.lo[b], o.hi[b]));
								climbHere = o.kind == Kind::Climb ? (int)j : climbHere;
							}

							for (std::vector<double> const &c : o.centres)
							{
								if (j != i && c.size() == 3 && c[(size_t)a] >= lo[a] && c[(size_t)a] <= hi[a] && c[1] >= lo[1] && c[1] <= hi[1])
								{
									blocked.push_back(std::make_pair(c[(size_t)b] - 1.0, c[(size_t)b] + 1.0));
								}
							}
						}

						int placed = 0;

						for (std::pair<double, double> const &stretch : AutoPlot_CloserFree(lo[b], hi[b], blocked))
						{
							double const along = stretch.second - stretch.first;

							if (along < kAutoPlotCloserLeast)
							{
								// A SLIVER: the climb-in takes it when it stands right beside it and the part it grows meets nothing it didn't meet already; else named
								bool taken = false;

								if (climbHere >= 0)
								{
									Vol const &c = vols[(size_t)climbHere];
									bool const below = fabs(c.lo[b] - stretch.second) <= 1.0, above = fabs(c.hi[b] - stretch.first) <= 1.0;

									if (below || above)
									{
										Vol grown = c, part = c;

										(below ? grown.lo[b] : grown.hi[b]) = below ? stretch.first : stretch.second;
										part.lo[b] = below ? stretch.first : c.hi[b];
										part.hi[b] = below ? c.lo[b] : stretch.second;

										bool clear = true;

										for (size_t j = 0; j < vols.size() && clear; j++)
										{
											if ((int)j == climbHere)
											{
												continue;
											}

											bool const meetsPart = std::min(part.hi[0], vols[j].hi[0]) > std::max(part.lo[0], vols[j].lo[0]) &&
												std::min(part.hi[1], vols[j].hi[1]) > std::max(part.lo[1], vols[j].lo[1]) && std::min(part.hi[2], vols[j].hi[2]) > std::max(part.lo[2], vols[j].lo[2]);
											bool const metBefore = std::min(c.hi[0], vols[j].hi[0]) > std::max(c.lo[0], vols[j].lo[0]) &&
												std::min(c.hi[1], vols[j].hi[1]) > std::max(c.lo[1], vols[j].lo[1]) && std::min(c.hi[2], vols[j].hi[2]) > std::max(c.lo[2], vols[j].lo[2]);

											clear = !meetsPart || metBefore;

											for (std::vector<double> const &p : vols[j].centres)
											{
												clear = clear && !(p.size() == 3 && p[0] >= part.lo[0] && p[0] <= part.hi[0] && p[1] >= part.lo[1] && p[1] <= part.hi[1] && p[2] >= part.lo[2] && p[2] <= part.hi[2]);
											}
										}

										if (clear)
										{
											vols[(size_t)climbHere] = grown;
											made.push_back(sideName + ": the climb-in stretched " + mm(along) + " mm along it over a sliver");
											taken = true;
										}
									}
								}

								if (!taken)
								{
									left.push_back(sideName + ": a " + mm(along) + " mm stretch at " + mm(stretch.first) + ".." + mm(stretch.second) + ", under the least, nothing beside it to take it");
								}

								continue;
							}

							// THE PIECES along and THE ROWS up: the fewest, evenly (one row when the band is at most the most)
							int const rows = band <= kAutoPlotCloserMost ? 1 : (int)ceil(band / (double)kAutoPlotCloserMost - 1e-9);

							for (std::pair<double, double> const &piece : AutoPlot_CloserPieces(stretch.first, stretch.second))
							{
								for (int row = 0; row < rows; row++)
								{
									Vol v;

									v.kind = Kind::Closer;
									v.floor = (int)i;
									v.lo[a] = lo[a];
									v.hi[a] = hi[a];
									v.lo[b] = piece.first;
									v.hi[b] = piece.second;
									v.lo[1] = lo[1] + band * row / rows;
									v.hi[1] = row + 1 == rows ? hi[1] : lo[1] + band * (row + 1) / rows;

									// its aim: half a body out from the face (an alien on the wall holds it), in the middle along and up
									std::vector<double> c(3);

									c[(size_t)a] = face - s * (kAutoPlotCloserDepth / 2.0);
									c[(size_t)b] = (piece.first + piece.second) / 2.0;
									c[1] = (v.lo[1] + v.hi[1]) / 2.0;
									v.centres.push_back(c);
									vols.push_back(v);
									placed++;
								}
							}
						}

						if (placed > 0)
						{
							made.push_back(sideName + ": " + std::to_string((long long)placed) + " closer(s), band " + mm(band) + " mm");
						}
					}
				}
			}

			std::string note = "closers: " + std::to_string((unsigned long long)(vols.size() - before)) + " made";

			for (std::string const &w : made)
			{
				note += "; " + w;
			}

			for (std::string const &w : left)
			{
				note += "; left " + w;
			}

			plan.notes.push_back(note);
		}

		// in the file's numbers; each centre the first candidate held by its own box and no other (else the first, counted)
		for (Vol const &v : vols)
		{
			WaypointRec wp;

			for (int a = 0; a < 3; a++)
			{
				wp.min[a] = File(Held(v.lo[a]), scale);
				wp.max[a] = std::max(std::min(File(Held(v.hi[a]), scale) + 1, kAutoPlotMostPlace), wp.min[a] + 2);
				wp.centre[a] = 0;
			}

			wp.flags = 0;
			wp.spare2 = 0;
			plan.list.push_back(wp);
			plan.climbing.push_back(v.kind == Kind::Climb || v.kind == Kind::Entry || v.kind == Kind::GetOut || v.kind == Kind::Low || v.kind == Kind::High || v.kind == Kind::Closer ||
				v.kind == Kind::StairCeiling);
			plan.volumeRoles.push_back(v.kind == Kind::Low ? "LOW" : v.kind == Kind::High ? "HIGH" : v.kind == Kind::Entry ? "entry" : v.kind == Kind::Exit ? "exit" : v.kind == Kind::GetOut ? "get-out" : v.kind == Kind::Climb ? "climb" :
				v.kind == Kind::Closer ? "closer" : v.kind == Kind::StairCeiling ? "stair ceiling" : "floor");   // Drake's volume repair: LOW and HIGH centres stay
		}

		for (size_t i = 0; i < vols.size(); i++)
		{
			bool chosen = false;
			int32_t first[3] = { 0, 0, 0 };

			for (size_t k = 0; k < vols[i].centres.size() && !chosen; k++)
			{
				int32_t at[3];

				for (int a = 0; a < 3; a++)
				{
					at[a] = std::min(std::max(File(Held(vols[i].centres[k][(size_t)a]), scale), plan.list[i].min[a]), plan.list[i].max[a] - 1);
				}

				if (k == 0)
				{
					for (int a = 0; a < 3; a++)
					{
						first[a] = at[a];
					}
				}

				bool other = false;

				for (size_t j = 0; j < vols.size() && !other; j++)
				{
					other = j != i && GameHolds(plan.list[j], at, scale);

					// a corner's entries OVERLAP (a corridor's only touch at the middle line): there an entry's centre kAutoPlotVolumeEntryClear outside the other entry too, while
					// a candidate is left that is (Drake's hand5b corner)
					if (!other && j != i && vols[i].kind == Kind::Entry && vols[j].kind == Kind::Entry && k + 1 < vols[i].centres.size() &&
						std::min(vols[i].hi[0], vols[j].hi[0]) > std::max(vols[i].lo[0], vols[j].lo[0]) && std::min(vols[i].hi[2], vols[j].hi[2]) > std::max(vols[i].lo[2], vols[j].lo[2]))
					{
						double const wx = (double)at[0] * scale, wz = (double)at[2] * scale, c = kAutoPlotVolumeEntryClear;

						other = wx > vols[j].lo[0] - c && wx < vols[j].hi[0] + c && wz > vols[j].lo[2] - c && wz < vols[j].hi[2] + c;
					}
				}

				if (GameHolds(plan.list[i], at, scale) && !other)
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

		// THE LINKS (autoplot_volumes.h): both ways kLinkAlien; a -> b only: a lists b kLinkAlien | kLinkOneWay, b lists a kLinkAlien | kLinkOneWayBack
		int32_t const both = kLinkAlien, stopped = kLinkAlien | kLinkOneWay, back = kLinkAlien | kLinkOneWayBack;
		auto const twoWay = [&](size_t a, size_t b) { SetLink(&plan.list, a, b, both, both); };
		auto const oneWay = [&](size_t from, size_t to) { SetLink(&plan.list, from, to, stopped, back); };

		for (size_t a = 0; a < floors; a++)
		{
			for (size_t b = a + 1; b < floors; b++)
			{
				if (Touch(vols[a], vols[b], e / 2.0))
				{
					twoWay(a, b);
				}
			}
		}

		// NO CLIMB AT ALL (the owner's count 0): each floor goes up straight into the entry over it most, so every box still reaches every other (Frost's checker)
		if (climbsKept == 0 && !slab)
		{
			for (size_t f = 0; f < floors; f++)
			{
				size_t over = vols.size();
				double most = 0.0;

				for (size_t j = floors; j < vols.size(); j++)
				{
					double const ox2 = std::min(vols[j].hi[0], vols[f].hi[0]) - std::max(vols[j].lo[0], vols[f].lo[0]), oz2 = std::min(vols[j].hi[2], vols[f].hi[2]) - std::max(vols[j].lo[2], vols[f].lo[2]);

					if (vols[j].kind == Kind::Entry && ox2 > 0.0 && oz2 > 0.0 && ox2 * oz2 > most)
					{
						most = ox2 * oz2;
						over = j;
					}
				}

				if (over < vols.size())
				{
					oneWay(f, over);
				}
			}
		}

		for (size_t k = floors; k < vols.size(); k++)
		{
			Vol const &v = vols[k];

			if (v.kind == Kind::Low)
			{
				oneWay((size_t)v.floor, k);   // the floor -> LOW only; LOW -> its HIGH pieces below (never LOW -> an entry: from the lower face an entry is straight off it)
			}
			else if (v.kind == Kind::Climb)
			{
				oneWay((size_t)v.floor, k);

				// up into the entry it lies under most, and any other under it at least half as much (a corridor's climb-in at the middle line: both; a corner's: its own
				// arm's only, Drake's hand5b: to the other entry it tied with that entry's route at its top)
				size_t near = (size_t)-1;
				double nearGap = 1e300, most = 0.0;
				bool any = false;
				auto const over = [&v](Vol const &en)
				{
					double const ox2 = std::min(v.hi[0], en.hi[0]) - std::max(v.lo[0], en.lo[0]), oz2 = std::min(v.hi[2], en.hi[2]) - std::max(v.lo[2], en.lo[2]);

					return ox2 > 0 && oz2 > 0 ? ox2 * oz2 : 0.0;
				};

				for (size_t j = floors; j < vols.size(); j++)
				{
					if (vols[j].kind == Kind::Entry && Touch(v, vols[j], 100.0))
					{
						most = std::max(most, over(vols[j]));
					}
				}

				for (size_t j = floors; j < vols.size(); j++)
				{
					if (vols[j].kind != Kind::Entry)
					{
						continue;
					}

					if (Touch(v, vols[j], 100.0) && over(vols[j]) >= most / 2.0)
					{
						oneWay(k, j);
						any = true;
					}
					else if (!Touch(v, vols[j], 100.0) && Gap(v, vols[j]) < nearGap)
					{
						nearGap = Gap(v, vols[j]);
						near = j;
					}
				}

				if (!any && near != (size_t)-1)
				{
					oneWay(k, near);
					plan.notes.push_back("a climb-in touches no entry: linked to the nearest, " + std::to_string((long long)nearGap) + " mm off");
				}
			}
			else if (v.kind == Kind::Entry)
			{
				for (size_t j = k + 1; j < vols.size(); j++)
				{
					// a T's funnel entry with its partner only (Drake's hand6e: no ENTRY E <-> ENTRY S)
					bool const funnelPair = (int)k == funnelEntry || (int)j == funnelEntry;
					bool const partner = funnelPair && ((int)k == funnelPartner || (int)j == funnelPartner);

					if (vols[j].kind == Kind::Entry && Touch(v, vols[j], 100.0) && (!funnelPair || partner))
					{
						twoWay(k, j);
					}
				}

				if (exitOf[(size_t)v.exit] >= 0)
				{
					oneWay(k, (size_t)exitOf[(size_t)v.exit]);
				}

				for (int const l : lanes)
				{
					if (slab && exitOf[(size_t)l] >= 0)
					{
						oneWay(k, (size_t)exitOf[(size_t)l]);   // a door slab's one entry -> both its exits
					}
				}

				// down into the get-out when the entry lies OVER it (more than a cell of it both ways in plan; a T's funnel entry only meets it along a line: hand6g has no
				// cS -> g1, d060b295's gold compare)
				Vol const *g = getOut >= 0 ? &vols[(size_t)getOut] : nullptr;

				if (g && Touch(v, *g, kAutoPlotVolumeHang - kAutoPlotClimbReach + 100.0) &&   // the get-out's top is 700 under, the entry's bottom 450
					std::min(v.hi[0], g->hi[0]) - std::max(v.lo[0], g->lo[0]) > e && std::min(v.hi[2], g->hi[2]) - std::max(v.lo[2], g->lo[2]) > e)
				{
					oneWay(k, (size_t)getOut);
				}

				// with NO GET-OUT (a room too low for one): down to the floor under it most, the only way down. (With one, Drake's hand5b has none: entry, get-out, floor
				// then ties with entry, exit, floor for an alien going down to him, as in his proven file: named, not changed)
				if (getOut < 0)
				{
					size_t under = 0;
					double most = -1.0;

					for (size_t f = 0; f < floors; f++)
					{
						double const ox2 = std::min(v.hi[0], vols[f].hi[0]) - std::max(v.lo[0], vols[f].lo[0]), oz2 = std::min(v.hi[2], vols[f].hi[2]) - std::max(v.lo[2], vols[f].lo[2]);
						double const area = ox2 > 0 && oz2 > 0 ? ox2 * oz2 : 0.0;

						if (area > most)
						{
							most = area;
							under = f;
						}
					}

					if (slab)
					{
						twoWay(k, under);   // a door slab: floor <-> entry, its only way up (every box reaches every other)
					}
					else
					{
						oneWay(k, under);
					}
				}
			}
			else if (v.kind == Kind::GetOut)
			{
				oneWay(k, (size_t)v.floor);
			}
			else if (v.kind == Kind::Closer)
			{
				// AS THE CLIMB-IN (autoplot_closers.h): the floor -> the closer; the closer -> each entry over it (touching, or under a body's height off: a body there is in
				// both); closer <-> each closer it touches (a row along the wall, and a corner's). No closer -> floor: down, it goes the lane's way (entry -> get-out -> floor)
				oneWay((size_t)v.floor, k);

				for (size_t j = floors; j < vols.size(); j++)
				{
					Vol const &o = vols[j];
					bool const over = std::min(v.hi[0], o.hi[0]) > std::max(v.lo[0], o.lo[0]) && std::min(v.hi[2], o.hi[2]) > std::max(v.lo[2], o.lo[2]);

					if (o.kind == Kind::Entry && over && v.lo[1] - o.hi[1] < (double)kAutoPlotCloserLeast && v.lo[1] >= o.hi[1] - 1.0)
					{
						oneWay(k, j);
					}
					else if (o.kind == Kind::Closer && j > k && Touch(v, o, 1.0))
					{
						twoWay(k, j);
					}
				}
			}
			else if (v.kind == Kind::Exit)
			{
				// into the floor nearest it; with no entry (no ceiling: open sky) both ways
				size_t near = 0;
				double nearGap = 1e300;

				for (size_t f = 0; f < floors; f++)
				{
					if (Gap(v, vols[f]) < nearGap)
					{
						nearGap = Gap(v, vols[f]);
						near = f;
					}
				}

				if (entryOf[(size_t)v.exit] >= 0 && !slab)   // (a door slab's exits <-> its floor: walked through)
				{
					oneWay(k, near);
				}
				else
				{
					twoWay(k, near);
				}
			}
		}

		// THE SLOPED SIDES' climbs: LOW -> each HIGH piece over it, a climb side's piece -> its entry, another side's piece <-> its entry
		for (std::pair<size_t, size_t> const &one : lowHigh)
		{
			oneWay(one.first, one.second);
		}

		for (std::pair<size_t, size_t> const &one : highUp)
		{
			oneWay(one.first, one.second);
		}

		for (std::pair<size_t, size_t> const &one : highBoth)
		{
			twoWay(one.first, one.second);
		}

		// a get-out that no entry touches: the nearest entry steps down into it
		if (getOut >= 0)
		{
			bool reached = false;

			for (WaypointLinkRec const &l : plan.list[(size_t)getOut].links)
			{
				reached = reached || (vols[(size_t)l.index].kind == Kind::Entry);
			}

			if (!reached)
			{
				size_t near = (size_t)-1;
				double nearGap = 1e300;

				for (size_t j = floors; j < vols.size(); j++)
				{
					if (vols[j].kind == Kind::Entry && Gap(vols[(size_t)getOut], vols[j]) < nearGap)
					{
						nearGap = Gap(vols[(size_t)getOut], vols[j]);
						near = j;
					}
				}

				if (near != (size_t)-1)
				{
					oneWay(near, (size_t)getOut);
				}
			}
		}

		// THE STAIRS' LINKS (THE STAIR BETWEEN TWO FLOORS, above), both ways and aliens only, as every link here: the stair's floor box with each floor it meets; its
		// ceiling box with each entry it touches (the upper floor's and the lower floor's)
		for (StairVols const &sv : stairs)
		{
			for (int const f : sv.joins)
			{
				if (f >= 0 && (size_t)f < floors)
				{
					twoWay(sv.floor, (size_t)f);
				}
			}

			if (sv.ceiling >= 0 && (size_t)sv.ceiling < vols.size())
			{
				for (size_t j = floors; j < vols.size(); j++)
				{
					if (vols[j].kind == Kind::Entry && Touch(vols[(size_t)sv.ceiling], vols[j], 100.0))
					{
						twoWay((size_t)sv.ceiling, j);
					}
				}
			}
		}

		// the words: each exit's lane, its door box (the entry) and its own box; the parts; the cells inside a box
		for (size_t x = 0; x < exits.size(); x++)
		{
			plan.doorBox[x] = entryOf[x];
			plan.exitBox[x] = exitOf[x];

			if (plan.doorWords[x].empty())
			{
				plan.doorWords[x] = entryOf[x] >= 0 ? "lane" : (x < doors.size() && doors[x].throughFound ? "no lane: no ceiling nearest this doorway" : "no lane: no doorway found");
			}

			if (exits[x].size() == 3)
			{
				int32_t const at[3] = { File(exits[x][0], scale), File(exits[x][1], scale), File(exits[x][2], scale) };
				bool held = false;

				for (WaypointRec const &wp : plan.list)
				{
					held = held || GameHolds(wp, at, scale);
				}

				plan.exitsCovered += held ? 1 : 0;
			}
		}

		{
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
		}

		for (auto const &c : cells)
		{
			int32_t const at[3] = { File(ox + c.first.first * e, scale), File(c.second - 10.0, scale), File(oz + c.first.second * e, scale) };
			bool in = false;

			for (size_t i = 0; i < floors && !in; i++)
			{
				in = GameHolds(plan.list[i], at, scale);
			}

			for (size_t k = 0; k < stairs.size() && !in; k++)
			{
				in = stairs[k].floor < plan.list.size() && GameHolds(plan.list[stairs[k].floor], at, scale);   // a tread, in its stair's floor box
			}

			plan.cellsCovered += in ? 1 : 0;
		}

		plan.floorBoxes = floors;
		plan.floorTile = e;
		plan.volumes = true;
		plan.notes.push_back("THE VOLUME SHAPE: " + std::to_string((long long)floors) + " floor, " + std::to_string((long long)std::count_if(vols.begin(), vols.end(), [](Vol const &v) { return v.kind == Kind::Climb; })) +
			" climb-in, " + std::to_string((long long)std::count_if(vols.begin(), vols.end(), [](Vol const &v) { return v.kind == Kind::Low; })) + " low climb (a sloped side), " +
			std::to_string((long long)std::count_if(vols.begin(), vols.end(), [](Vol const &v) { return v.kind == Kind::High; })) + " high (upper faces), " +
			(getOut >= 0 ? std::string("1 get-out, ") : std::string("no get-out (the room is under 2.6 m), ")) +
			std::to_string((long long)std::count_if(vols.begin(), vols.end(), [](Vol const &v) { return v.kind == Kind::Entry; })) + " entry, " +
			std::to_string((long long)std::count_if(vols.begin(), vols.end(), [](Vol const &v) { return v.kind == Kind::Exit; })) + " exit volumes");
		return plan;
	}
}
