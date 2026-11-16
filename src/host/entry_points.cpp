// See entry_points.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "entry_points.h"

namespace host
{
	namespace
	{
		typedef std::vector<EntryModule const *> ModuleList;

		int64_t const kFar = 1000000000;

		// A module's box: its shape's extents moved by its place
		void BoxOf(EntryModule const &module, int64_t lowest[3], int64_t highest[3])
		{
			for (int axis = 0; axis < 3; axis++)
			{
				lowest[axis] = (int64_t)module.place[axis] + module.minExtent[axis];
				highest[axis] = (int64_t)module.place[axis] + module.maxExtent[axis];
			}
		}

		// A LIST's box: the smallest and largest of its members' boxes (inteng's loops at 0x402efd and 0x402fb7, starting from -+10^9)
		void ListBox(ModuleList const &list, int64_t lowest[3], int64_t highest[3])
		{
			for (int axis = 0; axis < 3; axis++)
			{
				lowest[axis] = kFar;
				highest[axis] = -kFar;
			}

			for (size_t m = 0; m < list.size(); m++)
			{
				int64_t low[3];
				int64_t high[3];

				BoxOf(*list[m], low, high);

				for (int axis = 0; axis < 3; axis++)
				{
					lowest[axis] = low[axis] <= lowest[axis] ? low[axis] : lowest[axis];
					highest[axis] = highest[axis] <= high[axis] ? high[axis] : highest[axis];
				}
			}
		}

		// FUN_004026e0 (a, b): whether b's place is inside a's box, the edges included
		bool PlaceIsInside(EntryModule const &a, EntryModule const &b)
		{
			int64_t lowest[3];
			int64_t highest[3];

			BoxOf(a, lowest, highest);

			for (int axis = 0; axis < 3; axis++)
			{
				if (b.place[axis] < lowest[axis] || highest[axis] < b.place[axis])
				{
					return false;
				}
			}

			return true;
		}

		// FUN_00402770 for one member: the points of the module's shape (point + place) strictly inside (lo, hi); how many, and their smallest and
		// largest, the smallest starting from the list's box maximum and the largest from its box minimum
		int CountInsideOne(EntryModule const &module, int64_t const lo[3], int64_t const hi[3], int64_t smallest[3], int64_t largest[3])
		{
			int count = 0;

			for (size_t i = 0; i + 2 < module.points.size(); i += 3)
			{
				int64_t p[3];

				for (int axis = 0; axis < 3; axis++)
				{
					p[axis] = (int64_t)module.points[i + (size_t)axis] + module.place[axis];
				}

				if (p[0] < hi[0] && p[1] < hi[1] && p[2] < hi[2] && lo[0] < p[0] && lo[1] < p[1] && lo[2] < p[2])
				{
					count++;

					for (int axis = 0; axis < 3; axis++)
					{
						smallest[axis] = p[axis] <= smallest[axis] ? p[axis] : smallest[axis];
						largest[axis] = largest[axis] < p[axis] ? p[axis] : largest[axis];
					}
				}
			}

			return count;
		}

		// FUN_00402770: ... over every member of the list
		int CountInside(ModuleList const &list, int64_t const lo[3], int64_t const hi[3], int64_t smallest[3], int64_t largest[3])
		{
			int count = 0;

			for (size_t m = 0; m < list.size(); m++)
			{
				count += CountInsideOne(*list[m], lo, hi, smallest, largest);
			}

			return count;
		}

		// FUN_004028f0 for one member: a floor polygon (normal y below the limit) whose box (in x and z) holds the point and whose lowest corner (10 above
		// its y, as inteng does) is below the point and lower than the best so far
		void FloorCandidates(EntryModule const &module, int64_t const point[3], float floorBelow, int64_t *best)
		{
			int64_t const relX = point[0] - module.place[0];
			int64_t const relZ = point[2] - module.place[2];
			size_t const polygons = module.polygonNormalY.size();

			for (size_t p = 0; p < polygons && 5 * p + 4 < module.polygonCorners.size(); p++)
			{
				if (!(module.polygonNormalY[p] < floorBelow))
				{
					continue;
				}

				int64_t lowX = kFar, lowY = kFar, lowZ = kFar, highX = -kFar, highZ = -kFar;
				int32_t const corners = module.polygonCorners[5 * p];

				for (int c = 0; c < corners && c < 4; c++)
				{
					int32_t const index = module.polygonCorners[5 * p + 1 + (size_t)c];

					if (index < 0 || (size_t)index * 3 + 2 >= module.points.size())
					{
						continue;
					}

					int64_t const x = module.points[(size_t)index * 3];
					int64_t const y = module.points[(size_t)index * 3 + 1];
					int64_t const z = module.points[(size_t)index * 3 + 2];

					lowX = x < lowX ? x : lowX;
					lowY = y - 10 < lowY ? y - 10 : lowY;
					lowZ = z < lowZ ? z : lowZ;
					highX = highX < x ? x : highX;
					highZ = highZ < z ? z : highZ;
				}

				if (lowX <= relX && relX <= highX && lowZ <= relZ && relZ <= highZ)
				{
					int64_t const floor = (int64_t)module.place[1] + lowY;

					if (point[1] < floor && floor < *best)
					{
						*best = floor;
					}
				}
			}
		}

		// FUN_004028f0: the height of the point is moved to the lowest such floor of the list's members (one best for all), when there is one
		void MoveToFloor(ModuleList const &list, int64_t point[3], float floorBelow)
		{
			int64_t best = kFar;

			for (size_t m = 0; m < list.size(); m++)
			{
				FloorCandidates(*list[m], point, floorBelow, &best);
			}

			if (best != kFar)
			{
				point[1] = best;
			}
		}

		// C's integer division, towards 0, as the 32-bit code does it
		int64_t Half(int64_t sum)
		{
			return sum / 2;
		}

		ModuleList Alone(EntryModule const &module)
		{
			return ModuleList(1, &module);
		}
	}

	EntryResult ComputeEntryPoint(EntryModule const &main, EntryModule const &adjacent, uint32_t flagsIn, int32_t const pointIn[3], float floorBelow,
	                              bool stopWhenNotTouching)
	{
		return ComputeEntryPointOfLists(main, adjacent, Alone(main), Alone(adjacent), flagsIn, pointIn, floorBelow, stopWhenNotTouching);
	}

	EntryResult ComputeEntryPointOfLists(EntryModule const &main, EntryModule const &adjacent, std::vector<EntryModule const *> const &mainList,
	                                     std::vector<EntryModule const *> const &adjacentList, uint32_t flagsIn, int32_t const pointIn[3],
	                                     float floorBelow, bool stopWhenNotTouching)
	{
		EntryResult result;

		result.done = true;
		result.warnedNoTouch = false;
		result.failedToPlace = false;
		result.flags = flagsIn & ~6u;
		result.axis = -1;
		result.middle = false;
		result.floorMoved = false;

		for (int axis = 0; axis < 3; axis++)
		{
			result.point[axis] = pointIn ? pointIn[axis] : 0;
			result.beforeFloor[axis] = result.point[axis];
		}

		// 1. one module's place inside the other's box (the two modules of the pair, not their lists)
		if (PlaceIsInside(main, adjacent))
		{
			result.flags |= 4;
			return result;
		}

		if (PlaceIsInside(adjacent, main))
		{
			result.flags |= 2;
			return result;
		}

		// A list with nobody in it has no box: inteng's assertion fires (Adjmmode.cpp:0x1b0, 0x1b1), nothing here to work out
		if (mainList.empty() || adjacentList.empty())
		{
			result.done = false;
			return result;
		}

		// 3. the two lists' boxes and the overlap box
		int64_t min1[3], max1[3], min2[3], max2[3], lo[3], hi[3];

		ListBox(mainList, min1, max1);
		ListBox(adjacentList, min2, max2);

		for (int axis = 0; axis < 3; axis++)
		{
			lo[axis] = (min1[axis] > min2[axis] ? min1[axis] : min2[axis]) - 100;
			hi[axis] = (max1[axis] < max2[axis] ? max1[axis] : max2[axis]) + 100;
		}

		// 4. the points inside the overlap box
		int64_t outMin1[3], outMax1[3], outMin2[3], outMax2[3];

		for (int axis = 0; axis < 3; axis++)
		{
			outMin1[axis] = max1[axis];
			outMax1[axis] = min1[axis];
			outMin2[axis] = max2[axis];
			outMax2[axis] = min2[axis];
		}

		int const inside1 = CountInside(mainList, lo, hi, outMin1, outMax1);
		int const inside2 = CountInside(adjacentList, lo, hi, outMin2, outMax2);
		int64_t entry[3];

		if (inside1 < 2 || inside2 < 2)
		{
			result.middle = true;
			// 5. too few points: the middle of the overlap box (FUN_0042f860, FUN_0042f960: a sum, halved and truncated)
			for (int axis = 0; axis < 3; axis++)
			{
				entry[axis] = Half(lo[axis] + hi[axis]);
			}
		}
		else
		{
			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const high = outMax1[axis] < outMax2[axis] ? outMax1[axis] : outMax2[axis];
				int64_t const low = outMin1[axis] > outMin2[axis] ? outMin1[axis] : outMin2[axis];

				entry[axis] = Half(low + high);
			}
		}

		// 6. the modules don't touch
		int64_t size[3] = { hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] };

		if (size[0] < -100 || size[1] < -100 || size[2] < -100)
		{
			result.warnedNoTouch = true;

			if (stopWhenNotTouching)
			{
				result.done = false;
				return result;
			}
		}

		// 7. the way through is the thinnest side. The side the adjacent module lies on is read from the two modules' places.
		if (size[0] < size[1] && size[0] < size[2])
		{
			result.axis = 0;
			for (int a = 0; a < 3; a++) { result.beforeFloor[a] = (int32_t)entry[a]; }
			entry[0] = main.place[0] < adjacent.place[0] ? outMax1[0] + 100 : outMin1[0] - 100;
			{ int64_t const before = entry[1]; MoveToFloor(adjacentList, entry, floorBelow); result.floorMoved = entry[1] != before; }
			result.flags &= ~8u;
		}
		else if (size[1] <= size[2])
		{
			result.axis = 1;
			entry[1] = main.place[1] < adjacent.place[1] ? outMax1[1] + 100 : outMin1[1] - 100;
			result.flags |= 8;
		}
		else
		{
			result.axis = 2;
			for (int a = 0; a < 3; a++) { result.beforeFloor[a] = (int32_t)entry[a]; }
			entry[2] = main.place[2] < adjacent.place[2] ? outMax1[2] + 100 : outMin1[2] - 100;
			{ int64_t const before = entry[1]; MoveToFloor(adjacentList, entry, floorBelow); result.floorMoved = entry[1] != before; }
			result.flags &= ~8u;
		}

		// 8. bit 1: the point is left as it was
		if (result.flags & 1)
		{
			return result;
		}

		// the point is held inside the adjacent list's box shrunk by 5, and where it falls outside the box altogether the adjacent module's own place
		// is used for that coordinate
		for (int axis = 0; axis < 3; axis++)
		{
			int64_t value = entry[axis];

			value = value > max2[axis] - 5 ? max2[axis] - 5 : value;
			value = value < min2[axis] + 5 ? min2[axis] + 5 : value;

			if (max2[axis] < value || value < min2[axis])
			{
				value = adjacent.place[axis];
			}

			entry[axis] = value;
		}

		// 9. the point lies in some member of the adjacent list (its box): else inteng's message
		bool inAdjacent = false;

		for (size_t m = 0; m < adjacentList.size() && !inAdjacent; m++)
		{
			int64_t low[3];
			int64_t high[3];
			bool inThis = true;

			BoxOf(*adjacentList[m], low, high);

			for (int axis = 0; axis < 3; axis++)
			{
				inThis = inThis && low[axis] <= entry[axis] && entry[axis] <= high[axis];
			}

			inAdjacent = inThis;
		}

		result.failedToPlace = !inAdjacent;

		for (int axis = 0; axis < 3; axis++)
		{
			result.point[axis] = (int32_t)entry[axis];
		}

		return result;
	}
}
