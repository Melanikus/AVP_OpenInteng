// autoplot_rooms.cpp: see autoplot_rooms.h. Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "autoplot_rooms.h"

#include <math.h>

namespace host
{
	double AutoPlot_CeilingBeyond(std::map<std::pair<int, int>, double> const &ceilings, int ix0, int ix1, int iz0, int iz1, int axis, int sign, double ceilTop, double flat,
		double cellEdge)
	{
		if (cellEdge <= 0.0 || (axis != 0 && axis != 2) || (sign != 1 && sign != -1) || ix1 < ix0 || iz1 < iz0)
		{
			return 0.0;
		}

		int const look = (int)ceil(kAutoPlotRoomWallLook / cellEdge);   // cells out past the side
		int const line = axis == 0 ? (sign > 0 ? ix1 : ix0) : (sign > 0 ? iz1 : iz0);   // the rectangle's last cell line on that side
		int const from = axis == 0 ? iz0 : ix0, to = axis == 0 ? iz1 : ix1;
		double beyond = 0.0;

		for (int k = from; k <= to; k++)
		{
			bool goesOn = false;

			for (int step = 1; step <= look && !goesOn; step++)
			{
				int const out = line + sign * step;
				auto const at = ceilings.find(axis == 0 ? std::make_pair(out, k) : std::make_pair(k, out));

				goesOn = at != ceilings.end() && at->second <= ceilTop + flat;   // y down: a ceiling no lower than the flat's tolerance under the room's
			}

			beyond += goesOn ? cellEdge : 0.0;
		}

		return beyond;
	}
}
