// autoplot_arrival.cpp: ONE-WAY DOORWAYS for THE LANES, B's arrival where A's aliens come in (autoplot_arrival.h). Written 2026-11-20 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_arrival.h"

#include <math.h>

#include <map>

namespace host
{
	namespace
	{
		// a way through as the doorways have it: flat, finite, one long
		bool IsWayThrough(double const through[2])
		{
			if (!isfinite(through[0]) || !isfinite(through[1]))
			{
				return false;
			}

			double const length = sqrt(through[0] * through[0] + through[1] * through[1]);

			return fabs(length - 1.0) < 0.01;
		}
	}

	std::vector<AutoPlotArrival> AutoPlot_Arrivals(std::vector<AutoPlotExits> const &modules)
	{
		std::vector<AutoPlotArrival> arrivals;
		std::map<int32_t, size_t> placeOf;

		for (size_t m = 0; m < modules.size(); m++)
		{
			if (modules[m].head >= 0 && !placeOf.count(modules[m].head))
			{
				placeOf[modules[m].head] = m;   // the first with that head (a head twice in the list: the second is never anyone's B)
			}
		}

		for (AutoPlotExits const &a : modules)
		{
			size_t const exits = a.points.size();

			if (a.head < 0 || a.to.size() < exits || a.doors.size() < exits || a.backs.size() < exits)
			{
				continue;
			}

			for (size_t e = 0; e < exits && arrivals.size() < kAutoPlotMostArrivals; e++)
			{
				int32_t const toward = a.to[e];
				auto const found = placeOf.find(toward);

				if (toward < 0 || toward == a.head || found == placeOf.end())
				{
					continue;   // no neighbour known, itself, or a neighbour not plotted in this run
				}

				AutoPlotExits const &b = modules[found->second];
				bool backThere = false, backElsewhere = false;

				// an exit of B back to A AT THIS DOORWAY: its point within kAutoPlotArrivalSameDoorway of A's exit point, flat (the command's own test of one doorway); or only at
				// ANOTHER doorway (Genshd1's office lists doormp06 only at a doorway 7.8 m from doormp06 exit 0's, so its plan had no door box there)
				for (size_t g = 0; g < b.to.size() && g < b.points.size(); g++)
				{
					if (b.to[g] != a.head || b.points[g].size() != 3 || a.points[e].size() != 3)
					{
						continue;
					}

					double const dx = (double)b.points[g][0] - a.points[e][0], dz = (double)b.points[g][2] - a.points[e][2];
					bool const here = sqrt(dx * dx + dz * dz) <= kAutoPlotArrivalSameDoorway;

					backThere = backThere || here;
					backElsewhere = backElsewhere || !here;
				}

				AutoPlotDoor const &door = a.doors[e];

				if (backThere || !backElsewhere || !door.throughFound || !IsWayThrough(door.through) || a.points[e].size() != 3 || a.backs[e].size() != 3)
				{
					continue;   // a two-way doorway (B plots its own exit to A); B with no exit to A at all (no arrival: measured worse twice, see the header); or no doorway known on A's side
				}

				AutoPlotArrival arrival;

				arrival.into = found->second;
				arrival.from = a.head;
				arrival.fromExit = e;
				arrival.point = a.backs[e];
				arrival.back = a.points[e];
				arrival.door.throughFound = true;

				for (int i = 0; i < 3; i++)
				{
					arrival.door.middle[i] = door.middle[i];
				}

				arrival.door.through[0] = -door.through[0];
				arrival.door.through[1] = -door.through[1];
				arrivals.push_back(arrival);
			}
		}

		return arrivals;
	}

	bool AutoPlot_AddArrival(AutoPlotArrival const &arrival, std::vector<std::vector<int32_t> > *points, std::vector<int32_t> *to, std::vector<AutoPlotDoor> *doors,
		std::vector<std::vector<int32_t> > *backs, std::vector<std::vector<int32_t> > *goals)
	{
		if (!points || !to || !doors || !backs || !goals)
		{
			return false;
		}

		size_t const exits = points->size();

		if (to->size() != exits || doors->size() != exits || backs->size() != exits || goals->size() < exits || arrival.point.size() != 3)
		{
			return false;
		}

		points->push_back(arrival.point);
		to->push_back(arrival.from);
		doors->push_back(arrival.door);
		backs->push_back(arrival.back);
		goals->insert(goals->begin() + (ptrdiff_t)exits, arrival.point);
		return true;
	}
}
