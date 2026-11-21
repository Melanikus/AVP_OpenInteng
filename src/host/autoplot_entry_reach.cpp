// autoplot_entry_reach.cpp: AN ENTRY'S REACH TOWARD ITS DOORWAY'S TOP NEVER HOLDS ANOTHER BOX'S AIM (autoplot_entry_reach.h). Written 2026-11-21 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_entry_reach.h"

#include <algorithm>
#include <cmath>

namespace host
{
	std::vector<AutoPlotEntryReachCut> AutoPlot_EntryReachClear(double lo[3], double hi[3], double bandLo, double bandHi, std::vector<std::vector<double> > const &aims)
	{
		std::vector<AutoPlotEntryReachCut> cuts;

		if (!lo || !hi || !(bandLo <= bandHi) || bandLo < lo[1] || bandHi > hi[1])
		{
			return cuts;   // (no band inside the box: nothing is known to be its stretch)
		}

		// which aims the stretch holds, as the box was given (so the cut does not hang on the aims' order): x and z inside the box, y past the band and in the box
		double const margin = (double)kAutoPlotEntryReachClear;
		double newLo = lo[1], newHi = hi[1];

		for (size_t k = 0; k < aims.size(); k++)
		{
			std::vector<double> const &a = aims[k];

			if (a.size() != 3 || a[0] < lo[0] || a[0] > hi[0] || a[2] < lo[2] || a[2] > hi[2] || a[1] < lo[1] || a[1] > hi[1] || (a[1] >= bandLo && a[1] <= bandHi))
			{
				continue;   // not held, or held by the band (not the stretch's)
			}

			AutoPlotEntryReachCut cut;

			cut.aim = k;
			cut.below = a[1] > bandHi;

			if (cut.below)
			{
				// under the band (y is down): the bottom comes up over the body's top (900 over the aim's place) and the margin; never into the band
				double const end = std::max(bandHi, a[1] - 900.0 - margin - 1.0);

				cut.from = newHi;
				cut.to = std::min(newHi, end);
				newHi = cut.to;
			}
			else
			{
				// over the band: the top comes down under the body's foot (its place) and the margin; never into the band
				double const end = std::min(bandLo, a[1] + margin + 1.0);

				cut.from = newLo;
				cut.to = std::max(newLo, end);
				newLo = cut.to;
			}

			cuts.push_back(cut);
		}

		lo[1] = newLo;
		hi[1] = newHi;
		return cuts;
	}

	namespace
	{
		// the game's test of a place in a box on the loaded numbers (the file's times the scale; the loader takes 1 off each max): Waypoint_Holds, the repair's own
		bool Holds(WaypointRec const &box, int32_t const place[3], double scale)
		{
			for (int a = 0; a < 3; a++)
			{
				double const p = trunc((double)place[a] * scale);

				if (p < trunc((double)box.min[a] * scale) || p > trunc((double)box.max[a] * scale) - 1.0)
				{
					return false;
				}
			}

			return true;
		}

		// the places V2 tries on one axis (autoplot_volume_repair.cpp Tries): b's own two ends, and 1 and 2 past each end of every other box, those inside b
		std::vector<int32_t> Tries(std::vector<WaypointRec> const &list, size_t b, int axis)
		{
			WaypointRec const &box = list[b];
			std::vector<int32_t> tries = { box.min[axis], box.max[axis] - 1 };
			std::vector<int32_t> inside;

			for (size_t o = 0; o < list.size(); o++)
			{
				if (o != b)
				{
					tries.push_back(list[o].min[axis] - 1);
					tries.push_back(list[o].min[axis] - 2);
					tries.push_back(list[o].max[axis]);
					tries.push_back(list[o].max[axis] + 1);
				}
			}

			for (int32_t const t : tries)
			{
				if (t >= box.min[axis] && t <= box.max[axis] - 1)
				{
					inside.push_back(t);
				}
			}

			return inside;
		}
	}

	bool AutoPlot_PlaceOfItsOwn(std::vector<WaypointRec> const &list, size_t b, double scale)
	{
		if (b >= list.size())
		{
			return false;
		}

		WaypointRec const &box = list[b];
		auto const placeIsFree = [&](int32_t const place[3])
		{
			if (!Holds(box, place, scale))
			{
				return false;
			}

			for (size_t o = 0; o < list.size(); o++)
			{
				if (o != b && Holds(list[o], place, scale))
				{
					return false;
				}
			}

			return true;
		};

		// one axis at a time from the centre (the other two kept), then both: the centre's height always kept, as V2 keeps it
		std::vector<int32_t> const triesX = Tries(list, b, 0), triesZ = Tries(list, b, 2);

		for (int32_t const t : triesX)
		{
			int32_t const place[3] = { t, box.centre[1], box.centre[2] };

			if (placeIsFree(place))
			{
				return true;
			}
		}

		for (int32_t const t : triesZ)
		{
			int32_t const place[3] = { box.centre[0], box.centre[1], t };

			if (placeIsFree(place))
			{
				return true;
			}
		}

		for (int32_t const tx : triesX)
		{
			for (int32_t const tz : triesZ)
			{
				int32_t const place[3] = { tx, box.centre[1], tz };

				if (placeIsFree(place))
				{
					return true;
				}
			}
		}

		return false;
	}
}
