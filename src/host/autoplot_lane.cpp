// See autoplot_lane.h. Written 2026-11-19 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_lane.h"

#include <math.h>

#include <algorithm>
#include <set>

namespace host
{
	namespace
	{
		int const kStartingDepth = 5000;   // ALLOW_USE_OF_LONGER_PATHS (bh_waypoint.cpp:722)
		int const kWeighting = 5;          // every box on load (projload.cpp:2621)
		size_t const kQueueLength = 100;   // WAYVOLUME_QUEUE_LENGTH (bh_waypoint.cpp:676)

		// the loader's numbers in the world's (projload.cpp:2596-2607): lowest = min x scale, highest = max x scale - 1, each cut to a whole one; in doubles, no sum in 32 bits
		bool BoxHolds(WaypointRec const &w, double const p[3], double scale)
		{
			for (int a = 0; a < 3; a++)
			{
				if (p[a] < trunc((double)w.min[a] * scale) || p[a] > trunc((double)w.max[a] * scale) - 1.0)
				{
					return false;
				}
			}

			return true;
		}

		bool HoldsAlien(WaypointRec const &w, int32_t const position[3], double scale)
		{
			for (int corner = 0; corner < 8; corner++)
			{
				double const p[3] = { (double)position[0] + ((corner & 1) ? kStockAlienRadius : -kStockAlienRadius),
					(double)position[1] - ((corner & 2) ? kStockAlienTop : 0), (double)position[2] + ((corner & 4) ? kStockAlienRadius : -kStockAlienRadius) };

				if (BoxHolds(w, p, scale))
				{
					return true;
				}
			}

			return false;
		}

		struct Route
		{
			bool found = false;
			int num = 65536;   // ONE_FIXED
			int32_t start = -1, second = -1, last = -1;
			int32_t firstLinkFlags = -1;
		};

		struct QueueEntry
		{
			int depth = 0;
			int32_t box = -1;   // -1: NULL
		};

		// NewFindThisRoute (bh_waypoint.cpp:687-874), as it is; startHoldsTarget is the trivial case's test (the start box holds the target too)
		Route FindThisRoute(std::vector<WaypointRec> const &list, int32_t start, int32_t end, bool startHoldsTarget, bool alien, uint32_t shift)
		{
			Route r;

			if (startHoldsTarget)
			{
				r.found = true;
				r.num = 0;
				r.start = r.second = r.last = start;
				return r;
			}

			std::vector<int> workspace(list.size(), 0);
			std::vector<QueueEntry> queue(kQueueLength);
			size_t queueEnd = 1, queueExec = 0;
			bool terminate = false;

			queue[0].depth = kStartingDepth;
			queue[0].box = end;
			workspace[(size_t)end] = kStartingDepth;
			queue[1].box = -1;

			while (queue[queueExec].box >= 0 && !terminate)
			{
				int32_t const here = queue[queueExec].box;
				WaypointRec const &w = list[(size_t)here];
				size_t const links = w.links.size();
				size_t current = links ? shift % links : 0;

				for (size_t a = 0; a < links; a++)
				{
					WaypointLinkRec const &link = w.links[current];

					current++;
					current = current == links ? 0 : current;

					bool const take = alien || (link.flags & kLinkAlien) == 0;

					if ((link.flags & kLinkOneWay) != 0 || !take || link.index < 0 || (size_t)link.index >= list.size())
					{
						continue;
					}

					int32_t const next = link.index;

					if (next == start)
					{
						r.found = true;
						r.start = next;
						r.second = here;
						r.last = end;
						r.num = 1 + kStartingDepth - workspace[(size_t)here];
						r.firstLinkFlags = link.flags;
						terminate = true;
					}
					else if (workspace[(size_t)next] + kWeighting < workspace[(size_t)here])
					{
						queue[queueEnd].box = next;
						queue[queueEnd].depth = queue[queueExec].depth - kWeighting;
						workspace[(size_t)next] = queue[queueEnd].depth;

						// moved down the ring until it is sorted by depth (bh_waypoint.cpp:793-819)
						for (size_t at = queueEnd;;)
						{
							size_t const prev = at == 0 ? kQueueLength - 1 : at - 1;

							if (queue[at].depth > queue[prev].depth)
							{
								QueueEntry const keep = queue[at];

								queue[at] = queue[prev];
								queue[prev] = keep;
							}
							else
							{
								break;
							}

							at = prev;
						}

						queueEnd++;
						queueEnd = queueEnd >= kQueueLength ? 0 : queueEnd;
						queue[queueEnd].box = -1;

						if (queueEnd == queueExec)
						{
							r.found = false;   // the game's LOCALASSERT: the ring wrapped onto itself (more than 99 boxes' worth); taken as no route
							return r;
						}
					}
				}

				queueExec++;
				queueExec = queueExec >= kQueueLength ? 0 : queueExec;
			}

			if (!terminate)
			{
				r.found = false;
			}

			return r;
		}

		LaneCrossing Fails(std::string const &why)
		{
			LaneCrossing c;

			c.continuous = false;
			c.why = why;
			return c;
		}
	}

	StockChoice Stock_Choose(std::vector<WaypointRec> const &list, double scale, int32_t const position[3], int32_t const target[3], bool alien, uint32_t linkShift)
	{
		StockChoice choice;
		double const t[3] = { (double)target[0], (double)target[1], (double)target[2] };
		int32_t targetBox = -1;
		std::vector<bool> holdsAlien(list.size(), false), holdsTarget(list.size(), false);

		for (size_t i = 0; i < list.size(); i++)
		{
			holdsTarget[i] = BoxHolds(list[i], t, scale);
			targetBox = holdsTarget[i] ? (int32_t)i : targetBox;
			holdsAlien[i] = HoldsAlien(list[i], position, scale);
			choice.holding += holdsAlien[i] ? 1 : 0;
		}

		choice.targetHeld = targetBox >= 0;

		if (choice.holding == 0 || targetBox < 0)
		{
			return choice;   // "no containing waypoints": no route, no link
		}

		Route best;

		for (size_t i = 0; i < list.size(); i++)
		{
			if (!holdsAlien[i])
			{
				continue;
			}

			Route const r = FindThisRoute(list, (int32_t)i, targetBox, holdsTarget[i], alien, linkShift);

			if (!r.found)
			{
				return choice;   // "no continuous route!": FindBestRoute gives up on the first start with none
			}

			if (r.num < best.num)
			{
				best = r;
			}
		}

		choice.routed = best.start >= 0;
		choice.start = best.start;
		choice.second = best.second;
		choice.last = best.last;
		choice.firstLinkFlags = best.start != best.last ? best.firstLinkFlags : -1;
		choice.crawls = choice.firstLinkFlags >= 0 && (choice.firstLinkFlags & kLinkAlien) != 0;
		return choice;
	}

	LaneCrossing Lane_Cross(LaneSide const &a, LaneSide const &b, double scale, int32_t const soffit[3], int32_t const middle[3], double const through[2],
		int32_t back, int32_t over, int32_t step)
	{
		if (!a.list || !b.list || step <= 0 || back < 0 || over < 0)
		{
			return Fails("nothing to carry it through");
		}

		// the through axis's place of the middle, the across axis's of the soffit (the doorway's middle across), the hanging height
		for (int32_t d = -back; d <= over; d += step)
		{
			LaneSide const &side = d < 0 ? a : b;
			int32_t const position[3] = { (int32_t)floor(middle[0] + through[0] * d + 0.5), soffit[1] + kStockAlienTop, (int32_t)floor(middle[2] + through[1] * d + 0.5) };
			bool const squareX = fabs(through[0]) >= fabs(through[1]);
			int32_t const at[3] = { squareX ? position[0] : soffit[0], position[1], squareX ? soffit[2] : position[2] };

			for (uint32_t shift = 0; shift < 16; shift++)
			{
				StockChoice const c = Stock_Choose(*side.list, scale, at, side.target, true, shift);

				if (c.crawls)
				{
					continue;
				}

				std::string const where = std::to_string((long long)(d < 0 ? -d : d)) + " mm " + (d < 0 ? "before" : "past") + " the doorway's middle (" +
					(d < 0 ? "this" : "the next") + " AI module's boxes, link shift " + std::to_string((unsigned long long)shift) + "): ";

				if (c.holding == 0)
				{
					return Fails(where + "no box holds it");
				}

				if (!c.targetHeld)
				{
					return Fails(where + "no box holds its target");
				}

				if (!c.routed)
				{
					return Fails(where + "no route to its target");
				}

				if (c.start == c.last)
				{
					return Fails(where + "it is in its target's box (box " + std::to_string((long long)c.start) + ")");
				}

				return Fails(where + "the route's first link (box " + std::to_string((long long)c.second) + " to box " + std::to_string((long long)c.start) +
					") is plain");
			}
		}

		LaneCrossing c;

		c.continuous = true;
		return c;
	}

	LaneCrossing Lane_Through(std::vector<WaypointRec> const &list, double scale, int32_t from, int32_t const target[3], std::vector<bool> const &climbing)
	{
		double const t[3] = { (double)target[0], (double)target[1], (double)target[2] };
		int32_t targetBox = -1;

		for (size_t i = 0; i < list.size(); i++)
		{
			targetBox = BoxHolds(list[i], t, scale) ? (int32_t)i : targetBox;
		}

		if (from < 0 || (size_t)from >= list.size() || targetBox < 0)
		{
			return Fails(targetBox < 0 ? "no box holds the target" : "no box to start from");
		}

		int32_t at = from;
		std::string route = std::to_string((long long)from);

		for (size_t steps = 0; steps <= list.size(); steps++)
		{
			if (at == targetBox)
			{
				LaneCrossing c;

				c.continuous = true;
				return c;
			}

			if ((size_t)at >= climbing.size() || !climbing[(size_t)at])
			{
				return Fails("box " + std::to_string((long long)at) + " on the way is a floor box (the route " + route + ", the target's box " +
					std::to_string((long long)targetBox) + ")");
			}

			for (uint32_t shift = 0; shift < 16; shift++)
			{
				Route const r = FindThisRoute(list, at, targetBox, false, true, shift);

				if (!r.found)
				{
					return Fails("no route from box " + std::to_string((long long)at));
				}

				if ((r.firstLinkFlags & kLinkAlien) == 0)
				{
					return Fails("the link from box " + std::to_string((long long)at) + " to box " + std::to_string((long long)r.second) + " is plain (link shift " +
						std::to_string((unsigned long long)shift) + ")");
				}
			}

			at = FindThisRoute(list, at, targetBox, false, true, 0).second;
			route += " " + std::to_string((long long)at);
		}

		return Fails("the route goes round");
	}

	LaneCrossing Lane_ThroughAll(std::vector<WaypointRec> const &list, double scale, int32_t from, int32_t const target[3], std::vector<bool> const &climbing)
	{
		double const t[3] = { (double)target[0], (double)target[1], (double)target[2] };
		int32_t targetBox = -1;

		for (size_t i = 0; i < list.size(); i++)
		{
			targetBox = BoxHolds(list[i], t, scale) ? (int32_t)i : targetBox;
		}

		if (from < 0 || (size_t)from >= list.size() || targetBox < 0)
		{
			return Fails(targetBox < 0 ? "no box holds the target" : "no box to start from");
		}

		// each box reached once, the box before it on the branch that reached it first and that branch's shift; fewest steps first
		std::vector<int32_t> before(list.size(), -2);
		std::vector<uint32_t> shiftOf(list.size(), 0);
		std::vector<int32_t> todo(1, from);

		before[(size_t)from] = -1;

		auto const routeTo = [&before](int32_t at)
		{
			std::vector<int32_t> back;

			for (int32_t b = at; b >= 0 && back.size() <= before.size(); b = before[(size_t)b])
			{
				back.push_back(b);
			}

			std::string words;

			for (size_t k = back.size(); k-- > 0;)
			{
				words += (words.empty() ? "" : " ") + std::to_string((long long)back[k]);
			}

			return words;
		};

		for (size_t next = 0; next < todo.size(); next++)
		{
			int32_t const at = todo[next];

			if (at == targetBox)
			{
				continue;
			}

			if ((size_t)at >= climbing.size() || !climbing[(size_t)at])
			{
				return Fails("box " + std::to_string((long long)at) + " on the way is a floor box (the route " + routeTo(at) + ", the target's box " +
					std::to_string((long long)targetBox) + "); link shift " + std::to_string((unsigned long long)shiftOf[(size_t)at]));
			}

			for (uint32_t shift = 0; shift < 16; shift++)
			{
				Route const r = FindThisRoute(list, at, targetBox, false, true, shift);

				if (!r.found)
				{
					return Fails("no route from box " + std::to_string((long long)at));
				}

				if ((r.firstLinkFlags & kLinkAlien) == 0)
				{
					return Fails("the link from box " + std::to_string((long long)at) + " to box " + std::to_string((long long)r.second) + " is plain (link shift " +
						std::to_string((unsigned long long)shift) + ")");
				}

				if (r.second >= 0 && (size_t)r.second < list.size() && before[(size_t)r.second] == -2)
				{
					before[(size_t)r.second] = at;
					shiftOf[(size_t)r.second] = shift;
					todo.push_back(r.second);
				}
			}
		}

		LaneCrossing c;

		c.continuous = true;
		return c;
	}

	namespace
	{
		// What a plan breaks: each lane along it that is not continuous ("L i j": from exit i's door box toward exit j), and each doorway floor box from which a standing alien
		// does not climb toward another exit, at four link shifts ("U i f j s": its route's first link plain)
		std::set<std::string> BrokenOf(AutoPlotPlan const &plan, std::vector<std::vector<int32_t> > const &exits, double scale, bool climbs = false)
		{
			std::set<std::string> broken;

			for (size_t i = 0; i < exits.size() && i < plan.doorBox.size(); i++)
			{
				for (size_t j = 0; j < exits.size(); j++)
				{
					if (j == i || exits[j].size() != 3)
					{
						continue;
					}

					// the world's mm, as the command gives it
					if (plan.doorBox[i] >= 0 && !Lane_Through(plan.list, scale, plan.doorBox[i], exits[j].data(), plan.climbing).continuous)
					{
						broken.insert("L " + std::to_string((unsigned long long)i) + " " + std::to_string((unsigned long long)j));
					}

					// the climb from each doorway floor box: only when asked (a floor alien that walks is no stall and no back place, the gate's measure; Bishop 11-20)
					for (size_t n = 0; climbs && i < plan.doorFloors.size() && n < plan.doorFloors[i].size(); n++)
					{
						int32_t const f = plan.doorFloors[i][n];

						if (f < 0 || (size_t)f >= plan.list.size())
						{
							continue;
						}

						WaypointRec const &box = plan.list[(size_t)f];
						int32_t const standing[3] = { (int32_t)floor((box.min[0] + box.max[0]) / 2.0 * scale), (int32_t)floor((box.max[1] - 1 - kAutoPlotInto / scale) * scale),
							(int32_t)floor((box.min[2] + box.max[2]) / 2.0 * scale) };

						for (uint32_t shift = 0; shift < 4; shift++)
						{
							if (!Stock_Choose(plan.list, scale, standing, exits[j].data(), true, shift).crawls)
							{
								broken.insert("U " + std::to_string((unsigned long long)i) + " " + std::to_string((unsigned long long)n) + " " + std::to_string((unsigned long long)j) + " " +
									std::to_string((unsigned long long)shift));
							}
						}
					}
				}
			}

			return broken;
		}
	}

	AutoPlotPlan AutoPlot_PlanConsolidated(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floor,
		std::vector<AutoPlotDoor> const &doors, double levelScale, size_t *broken, double *mostSide)
	{
		double const scale = levelScale > 0.0 ? levelScale : 1.0;
		AutoPlotFloor unmerged = floor;

		unmerged.mostSide = 0.0;

		// the floor not merged first: what it breaks is what the level gives (a lane no plan can hold); a merge may break nothing more
		AutoPlotPlan const base = AutoPlot_Plan(ways, exits, unmerged, doors, levelScale);
		std::set<std::string> const baseBroken = BrokenOf(base, exits, scale);
		AutoPlotPlan chosen = base;
		size_t chosenBroken = baseBroken.size();

		// whole rectangles first (the long paths: AutoPlot_MergeTiles holds a side to 16 squares), then smaller
		for (double const side : { 1.0e9, 16000.0, 9600.0, 6000.0, (double)kAutoPlotFloorMost, 2400.0, 1200.0 })   // steps between whole and 4 m (11-20: Ops01 one 39 m floor tied its lane, then fell to 4 m: 53 boxes)
		{
			AutoPlotFloor sized = floor;

			sized.mostSide = side;

			AutoPlotPlan const plan = AutoPlot_Plan(ways, exits, sized, doors, levelScale);
			std::set<std::string> const planBroken = BrokenOf(plan, exits, scale);

			if (std::includes(baseBroken.begin(), baseBroken.end(), planBroken.begin(), planBroken.end()))
			{
				chosen = plan;
				chosenBroken = planBroken.size();
				break;
			}
		}

		if (broken)
		{
			*broken = chosenBroken;
		}

		if (mostSide)
		{
			*mostSide = chosen.floorMost;
		}

		return chosen;
	}
}
