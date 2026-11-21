// autoplot_stairs.cpp: THE STAIR BETWEEN TWO FLOORS (autoplot_stairs.h). Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
#include "autoplot_stairs.h"

#include <math.h>

#include <algorithm>

#include "autoplot_levels.h"   // kAutoPlotLevelStep: the step a tread is from the next, the grid's own

namespace host
{
	namespace
	{
		std::pair<int, int> const kSides[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

		// which floor rectangle holds the cell, -1 when none
		int FloorOf(std::vector<AutoPlotStairFloor> const &floors, std::pair<int, int> const &cell)
		{
			for (size_t f = 0; f < floors.size(); f++)
			{
				AutoPlotStairFloor const &r = floors[f];

				if (cell.first >= r.ix0 && cell.first <= r.ix1 && cell.second >= r.iz0 && cell.second <= r.iz1)
				{
					return (int)f;
				}
			}

			return -1;
		}
	}

	std::vector<AutoPlotStair> AutoPlot_Stairs(std::map<std::pair<int, int>, double> const &cells, std::map<std::pair<int, int>, double> const &ceilings,
		std::vector<AutoPlotStairFloor> const &floors)
	{
		std::vector<AutoPlotStair> out;
		std::map<std::pair<int, int>, bool> seen;   // the cells already in a group

		for (auto const &start : cells)
		{
			if (seen.count(start.first) || FloorOf(floors, start.first) >= 0)
			{
				continue;   // grouped already, or a floor rectangle's
			}

			// the group: every cell no rectangle holds, reached from this one by steps of at most kAutoPlotLevelStep
			std::vector<std::pair<int, int> > group, todo(1, start.first);
			std::vector<bool> meets(floors.size(), false);

			seen[start.first] = true;

			while (!todo.empty())
			{
				std::pair<int, int> const at = todo.back();
				double const y = cells.at(at);

				todo.pop_back();
				group.push_back(at);

				for (std::pair<int, int> const &side : kSides)
				{
					std::pair<int, int> const next(at.first + side.first, at.second + side.second);
					auto const there = cells.find(next);

					if (there == cells.end() || fabs(there->second - y) > (double)kAutoPlotLevelStep)
					{
						continue;   // no cell there, or a step no alien walks
					}

					int const f = FloorOf(floors, next);

					if (f >= 0)
					{
						meets[(size_t)f] = true;   // the group meets that floor here
					}
					else if (!seen.count(next))
					{
						seen[next] = true;
						todo.push_back(next);
					}
				}
			}

			// a stair only between floors more than a step apart: a group meeting floors of one height (or one floor) is a fitting, a sill, a ledge: left as it was
			AutoPlotStair s;
			double top = 1e300, low = -1e300;

			for (size_t f = 0; f < floors.size(); f++)
			{
				if (meets[f])
				{
					s.joins.push_back((int)f);
					top = std::min(top, floors[f].floorTop);
					low = std::max(low, floors[f].floorTop);
				}
			}

			if (s.joins.size() < 2 || low - top <= (double)kAutoPlotLevelStep)
			{
				continue;
			}

			s.ix0 = s.ix1 = group[0].first;
			s.iz0 = s.iz1 = group[0].second;
			s.floorTop = 1e300;
			s.floorLow = -1e300;

			for (std::pair<int, int> const &c : group)
			{
				double const y = cells.at(c);

				s.ix0 = std::min(s.ix0, c.first);
				s.ix1 = std::max(s.ix1, c.first);
				s.iz0 = std::min(s.iz0, c.second);
				s.iz1 = std::max(s.iz1, c.second);
				s.floorTop = std::min(s.floorTop, y);
				s.floorLow = std::max(s.floorLow, y);

				// the ceiling over the cell, when it is one (a tread's edge over the cell is not)
				auto const roof = ceilings.find(c);

				if (roof != ceilings.end() && y - roof->second >= kAutoPlotStairRoofLeast)
				{
					s.ceilTop = s.ceiled ? std::min(s.ceilTop, roof->second) : roof->second;
					s.ceilLow = s.ceiled ? std::max(s.ceilLow, roof->second) : roof->second;
					s.ceiled = true;
				}
			}

			out.push_back(s);
		}

		return out;
	}
}
