// See light_pick.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (M-3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "light_pick.h"

namespace host
{
	namespace
	{
		int64_t const NearMillimetres = 300;   // how near the line a light has to be, however close it is
		int64_t const FarthestShare = 40;      // plus 1/40 (2.5%) of its distance along the line
	}

	bool LightPick_Nearest(PickRay const &ray, int32_t const *places, size_t count, size_t *light)
	{
		if (!places || !light)
		{
			return false;
		}

		bool found = false;
		double bestAway = 0;
		size_t best = 0;

		for (size_t i = 0; i < count; i++)
		{
			int64_t v[3];
			bool inRange = true;

			for (size_t axis = 0; axis < 3; axis++)
			{
				int32_t const place = places[3 * i + axis];

				inRange = inRange && place >= -kPickMostPlace && place <= kPickMostPlace;
				v[axis] = (int64_t)place - (int64_t)ray.from[axis];
			}

			if (!inRange)
			{
				continue;
			}

			// How far along the line the light's nearest point is, in millimetres (the direction is 65536 long), and how far off the line
			int64_t const along65536 = v[0] * ray.direction[0] + v[1] * ray.direction[1] + v[2] * ray.direction[2];

			if (along65536 <= 0)
			{
				continue;
			}

			double const along = (double)along65536 / (double)kPickOne;
			double const squareLength = (double)(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
			double const squareAway = squareLength - along * along;
			double const away = squareAway > 0 ? squareAway : 0;
			double const reach = (double)NearMillimetres + along / (double)FarthestShare;

			if (away <= reach * reach && (!found || away < bestAway))
			{
				found = true;
				bestAway = away;
				best = i;
			}
		}

		if (found)
		{
			*light = best;
		}

		return found;
	}
}
