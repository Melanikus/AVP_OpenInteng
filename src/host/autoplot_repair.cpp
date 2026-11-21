// autoplot_repair.cpp: see autoplot_repair.h. Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autoplot_repair.h"

#include <math.h>

#include <algorithm>

#include "waypoint_room.h"   // avpx/3dc: the loader's scaling of one corner, as the checker uses it

namespace host
{
	namespace
	{
		// the checker's own test (the game's WaypointContainsPoint on the loader's scaled numbers, the highest 1 less)
		bool Holds(WaypointRec const &box, int32_t const place[3], double scale)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				int32_t low = 0, top = 0, at = 0;

				if (!WaypointScaledCorner(box.min[axis], scale, &low) || !WaypointScaledCorner(box.max[axis], scale, &top) || top == INT32_MIN ||
					!WaypointScaledCorner(place[axis], scale, &at) || at < low || at > top - 1)
				{
					return false;
				}
			}

			return true;
		}

		// a climbing box: every link aliens only (a marine never steps into it)
		bool Climbing(WaypointRec const &box)
		{
			if (box.links.empty())
			{
				return false;
			}

			for (WaypointLinkRec const &l : box.links)
			{
				if ((l.flags & kLinkAlien) == 0)
				{
					return false;
				}
			}

			return true;
		}

		bool HoldsAnExit(WaypointRec const &box, std::vector<WaypointPoint> const &exits, double scale)
		{
			for (WaypointPoint const &e : exits)
			{
				if (Holds(box, e.place, scale))
				{
					return true;
				}
			}

			return false;
		}

		bool InAnyOther(std::vector<WaypointRec> const &list, size_t self, int32_t const place[3], double scale)
		{
			for (size_t i = 0; i < list.size(); i++)
			{
				if (i != self && Holds(list[i], place, scale))
				{
					return true;
				}
			}

			return false;
		}

		// the boxes kept, numbered again, every link following (a link to a box taken out goes with it); *where (when given): each box as it came, its place now
		void Keep(std::vector<WaypointRec> *list, std::vector<bool> const &keep, std::vector<int32_t> *where)
		{
			std::vector<int32_t> number(list->size(), -1);
			std::vector<WaypointRec> kept;

			for (size_t i = 0; i < list->size(); i++)
			{
				if (keep[i])
				{
					number[i] = (int32_t)kept.size();
					kept.push_back((*list)[i]);
				}
			}

			for (WaypointRec &box : kept)
			{
				std::vector<WaypointLinkRec> links;

				for (WaypointLinkRec const &l : box.links)
				{
					if (l.index >= 0 && (size_t)l.index < number.size() && number[(size_t)l.index] >= 0)
					{
						links.push_back({ number[(size_t)l.index], l.flags });
					}
				}

				box.links.swap(links);
			}

			list->swap(kept);

			for (int32_t &w : *where)
			{
				w = w >= 0 && (size_t)w < number.size() ? number[(size_t)w] : -1;
			}
		}

		void AddLink(WaypointRec *box, int32_t to, int32_t flags)
		{
			for (WaypointLinkRec const &l : box->links)
			{
				if (l.index == to)
				{
					return;
				}
			}

			box->links.push_back({ to, flags });
		}

		// box j made one with box i: their union, i's centre, the links of both (each other's and doubles left out, i's flags first); every link to j goes to i; j taken out
		void MakeOne(std::vector<WaypointRec> *list, size_t i, size_t j, std::vector<int32_t> *where)
		{
			for (int32_t &w : *where)
			{
				w = w == (int32_t)j ? (int32_t)i : w;   // the boxes made one with j are now i
			}

			WaypointRec &a = (*list)[i];
			WaypointRec const b = (*list)[j];

			for (int axis = 0; axis < 3; axis++)
			{
				a.min[axis] = std::min(a.min[axis], b.min[axis]);
				a.max[axis] = std::max(a.max[axis], b.max[axis]);
			}

			std::vector<WaypointLinkRec> const mine = a.links;

			a.links.clear();

			for (WaypointLinkRec const &l : mine)
			{
				if ((size_t)l.index != i && (size_t)l.index != j)
				{
					AddLink(&a, l.index, l.flags);
				}
			}

			for (WaypointLinkRec const &l : b.links)
			{
				if ((size_t)l.index != i && (size_t)l.index != j)
				{
					AddLink(&a, l.index, l.flags);
				}
			}

			for (size_t k = 0; k < list->size(); k++)
			{
				if (k == i || k == j)
				{
					continue;
				}

				std::vector<WaypointLinkRec> links;
				bool toI = false;

				for (WaypointLinkRec const &l : (*list)[k].links)
				{
					toI = toI || (size_t)l.index == i;
				}

				for (WaypointLinkRec const &l : (*list)[k].links)
				{
					if ((size_t)l.index == j)
					{
						if (!toI)
						{
							links.push_back({ (int32_t)i, l.flags });
							toI = true;
						}
						continue;
					}

					links.push_back(l);
				}

				(*list)[k].links.swap(links);
			}

			std::vector<bool> keep(list->size(), true);

			keep[j] = false;
			Keep(list, keep, where);
		}

		// the gap between two boxes' faces (0 when they meet or overlap)
		double Gap(WaypointRec const &a, WaypointRec const &b)
		{
			double s = 0.0;

			for (int axis = 0; axis < 3; axis++)
			{
				double const d = std::max({ 0.0, (double)a.min[axis] - b.max[axis], (double)b.min[axis] - a.max[axis] });

				s += d * d;
			}

			return sqrt(s);
		}

		// rule 4: box o trimmed on one face just past `place` so it no longer holds it; across first (x, z), up / down only if neither can; never taking o's own centre,
		// an exit point o holds, or its room (at least 8 on the axis). False when no face can be
		bool TrimFor(std::vector<WaypointRec> *list, size_t o, int32_t const place[3], std::vector<WaypointPoint> const &exits, double scale)
		{
			WaypointRec const before = (*list)[o];
			std::vector<WaypointPoint> held;

			for (WaypointPoint const &e : exits)
			{
				if (Holds(before, e.place, scale))
				{
					held.push_back(e);
				}
			}

			for (int pass = 0; pass < 2; pass++)
			{
				WaypointRec best = before;
				double bestCut = 1e300;

				for (int axis = 0; axis < 3; axis++)
				{
					if ((axis == 1) != (pass == 1))
					{
						continue;
					}

					for (int side = 0; side < 2; side++)
					{
						WaypointRec t = before;
						double cut = 0.0;

						if (side == 0)
						{
							t.min[axis] = place[axis] + 1;   // the face up to just past the place
							cut = (double)t.min[axis] - before.min[axis];
						}
						else
						{
							t.max[axis] = place[axis];       // the highest is 1 less: the place is out
							cut = (double)before.max[axis] - t.max[axis];
						}

						for (int more = 0; more < 4 && Holds(t, place, scale); more++)
						{
							side == 0 ? t.min[axis]++ : t.max[axis]--;   // a scale under 1 can need a step or two more
						}

						bool good = !Holds(t, place, scale) && t.max[axis] - t.min[axis] >= 8 && Holds(t, t.centre, scale);

						for (WaypointPoint const &e : held)
						{
							good = good && Holds(t, e.place, scale);
						}

						if (good && cut < bestCut)
						{
							best = t, bestCut = cut;
						}
					}
				}

				if (bestCut < 1e300)
				{
					(*list)[o] = best;
					return true;
				}
			}

			return false;
		}

		// rule 5: box b's centre moved, at its own height, to the nearest place in b that is in no other box: just past each face of the boxes that hold it now, else on a
		// grid over b. False when there is no such place
		bool MoveCentre(std::vector<WaypointRec> *list, size_t b, double scale)
		{
			WaypointRec &box = (*list)[b];
			int32_t const from[3] = { box.centre[0], box.centre[1], box.centre[2] };
			std::vector<std::vector<int32_t> > tries;

			for (size_t o = 0; o < list->size(); o++)
			{
				if (o == b || !Holds((*list)[o], from, scale))
				{
					continue;
				}

				for (int axis = 0; axis < 3; axis += 2)   // across only: the height stays
				{
					std::vector<int32_t> low(from, from + 3), high(from, from + 3);

					low[(size_t)axis] = (*list)[o].min[axis] - 1;
					high[(size_t)axis] = (*list)[o].max[axis];
					tries.push_back(low);
					tries.push_back(high);
				}
			}

			int const steps = 16;

			for (int i = 0; i <= steps; i++)
			{
				for (int k = 0; k <= steps; k++)
				{
					tries.push_back({ (int32_t)(box.min[0] + (double)(box.max[0] - 1 - box.min[0]) * i / steps), from[1],
						(int32_t)(box.min[2] + (double)(box.max[2] - 1 - box.min[2]) * k / steps) });
				}
			}

			double bestFar = 1e300;
			std::vector<int32_t> best;

			for (std::vector<int32_t> const &p : tries)
			{
				if (!Holds(box, p.data(), scale) || InAnyOther(*list, b, p.data(), scale))
				{
					continue;
				}

				double const dx = (double)p[0] - from[0], dz = (double)p[2] - from[2], distance = dx * dx + dz * dz;

				if (distance < bestFar)
				{
					bestFar = distance, best = p;
				}
			}

			if (best.empty())
			{
				return false;
			}

			box.centre[0] = best[0], box.centre[1] = best[1], box.centre[2] = best[2];
			return true;
		}

		// the parts: the boxes joined by links in the list (either way)
		std::vector<int> Parts(std::vector<WaypointRec> const &list, int *count)
		{
			std::vector<int> part(list.size(), -1);
			std::vector<std::vector<size_t> > joined(list.size());

			for (size_t i = 0; i < list.size(); i++)
			{
				for (WaypointLinkRec const &l : list[i].links)
				{
					if (l.index >= 0 && (size_t)l.index < list.size())
					{
						joined[i].push_back((size_t)l.index);
						joined[(size_t)l.index].push_back(i);
					}
				}
			}

			*count = 0;

			for (size_t i = 0; i < list.size(); i++)
			{
				if (part[i] >= 0)
				{
					continue;
				}

				std::vector<size_t> todo(1, i);

				part[i] = *count;

				while (!todo.empty())
				{
					size_t const at = todo.back();

					todo.pop_back();

					for (size_t const n : joined[at])
					{
						if (part[n] < 0)
						{
							part[n] = *count;
							todo.push_back(n);
						}
					}
				}

				(*count)++;
			}

			return part;
		}

		// a place of the file's units in the world's millimetres (the plan's own: the file's number times the scale), false when it would not fit
		bool ToWorld(int32_t const file[3], double scale, int32_t world[3])
		{
			for (int axis = 0; axis < 3; axis++)
			{
				double const v = (double)file[axis] * scale;

				if (!(v > -2.0e9 && v < 2.0e9))
				{
					return false;
				}

				world[axis] = (int32_t)v;
			}

			return true;
		}

		// rule 6 (b): the grid's alien way from box a's centre to box b's, every point of it outside the list's boxes on a wall or a ceiling of one of this AI module's game
		// modules (a floor stretch no box covers, or a way through another module's space, is not a surface that connects them)
		bool ClimbedBetween(std::vector<WaypointRec> const &list, size_t a, size_t b, double scale, AutoPlotRepairWay const &alienWay, std::vector<int32_t> const &modules)
		{
			int32_t from[3], to[3];

			if (!ToWorld(list[a].centre, scale, from) || !ToWorld(list[b].centre, scale, to))
			{
				return false;
			}

			std::vector<AutoPlotPoint> way;

			if (!alienWay(from, to, &way) || way.empty())
			{
				return false;
			}

			for (AutoPlotPoint const &point : way)
			{
				int32_t file[3];

				for (int axis = 0; axis < 3; axis++)
				{
					double const v = floor((double)point.place[axis] / std::max(scale, 1e-9) + 0.5);

					if (!(v > -2.0e9 && v < 2.0e9))
					{
						return false;
					}

					file[axis] = (int32_t)v;
				}

				bool inBox = false;

				for (size_t i = 0; i < list.size() && !inBox; i++)
				{
					inBox = Holds(list[i], file, scale);
				}

				if (inBox)
				{
					continue;
				}

				if (point.surface == AutoPlotPoint::Surface::Floor || std::find(modules.begin(), modules.end(), point.module) == modules.end())
				{
					return false;
				}
			}

			return true;
		}
	}

	AutoPlotRepairCounts AutoPlot_Repair(std::vector<WaypointRec> *list, std::vector<WaypointPoint> const &exits, double scale, std::string *words,
		std::vector<int32_t> *renumber, AutoPlotRepairWay const &alienWay, std::vector<int32_t> const &modules)
	{
		AutoPlotRepairCounts counts;
		std::vector<int32_t> where;

		for (size_t i = 0; list && i < list->size(); i++)
		{
			where.push_back((int32_t)i);
		}

		if (renumber)
		{
			*renumber = where;
		}

		if (!list || list->empty())
		{
			return counts;
		}

		std::string firstLeft;

		for (int round = 0; round < kAutoPlotRepairMostRounds; round++)
		{
			std::vector<WaypointProblem> const problems = Waypoints_Check(*list, exits, scale, 1000);
			bool other = false, unreachable = false;
			WaypointProblem const *centre = nullptr;

			for (WaypointProblem const &p : problems)
			{
				if (p.kind == WaypointProblemKind::CannotReach)
				{
					unreachable = true;
				}
				else if (p.kind == WaypointProblemKind::CentreInAnother)
				{
					centre = centre ? centre : &p;
				}
				else if (p.kind == WaypointProblemKind::ExitOutside && std::find(counts.leftOutExits.begin(), counts.leftOutExits.end(), p.other) != counts.leftOutExits.end())
				{
					// an exit rule 6 (a) left out itself: the checker's exception, never a reason to stop (apcend, d4798b45)
				}
				else
				{
					other = true;   // the planner's: never the repair's (and its sums would not be trusted)
				}
			}

			if (problems.empty() || other)
			{
				break;
			}

			bool changed = false;

			if (unreachable)
			{
				// rules 1 and 2. The main part: the one holding the most exit points, then the most boxes
				int count = 0;
				std::vector<int> const part = Parts(*list, &count);
				std::vector<size_t> exitsIn((size_t)count, 0), boxesIn((size_t)count, 0);

				// each exit point counted once for each part holding it; heldBy: the part holding it, -2 when more than one does, -1 none
				std::vector<int> heldBy(exits.size(), -1);

				for (size_t i = 0; i < list->size(); i++)
				{
					boxesIn[(size_t)part[i]]++;
				}

				for (size_t e = 0; e < exits.size(); e++)
				{
					std::vector<bool> counted((size_t)count, false);

					for (size_t i = 0; i < list->size(); i++)
					{
						if (!counted[(size_t)part[i]] && Holds((*list)[i], exits[e].place, scale))
						{
							counted[(size_t)part[i]] = true;
							exitsIn[(size_t)part[i]]++;
							heldBy[e] = heldBy[e] == -1 ? part[i] : -2;
						}
					}
				}

				int main = 0;

				for (int p = 1; p < count; p++)
				{
					if (exitsIn[(size_t)p] > exitsIn[(size_t)main] || (exitsIn[(size_t)p] == exitsIn[(size_t)main] && boxesIn[(size_t)p] > boxesIn[(size_t)main]))
					{
						main = p;
					}
				}

				std::vector<bool> keep(list->size(), true);

				for (int p = 0; p < count && count > 1; p++)
				{
					if (p == main)
					{
						continue;
					}

					// its nearest box to the main part's
					double nearest = 1e300;
					size_t from = 0, to = 0;

					for (size_t i = 0; i < list->size(); i++)
					{
						if (part[i] != p)
						{
							continue;
						}

						for (size_t k = 0; k < list->size(); k++)
						{
							if (part[k] == main)
							{
								double const g = Gap((*list)[i], (*list)[k]);

								if (g < nearest)
								{
									nearest = g, from = i, to = k;
								}
							}
						}
					}

					if (nearest <= (double)kAutoPlotRepairJoinGap / std::max(scale, 1e-9))
					{
						int32_t const flags = Climbing((*list)[from]) || Climbing((*list)[to]) ? kLinkAlien : 0;   // rule 2

						AddLink(&(*list)[from], (int32_t)to, flags);
						AddLink(&(*list)[to], (int32_t)from, flags);
						counts.partsLinked++;
						changed = true;
					}
					else if (exitsIn[(size_t)p] == 0)
					{
						for (size_t i = 0; i < list->size(); i++)   // rule 1
						{
							if (part[i] == p)
							{
								keep[i] = false;
								counts.boxesTakenOut++;
							}
						}

						counts.partsTakenOut++;
						changed = true;
					}
					else
					{
						// rule 6: an area with its own exit, too far to link as near. (b) joined where the grid's alien way climbs between them, nearest pairs first
						bool joined = false;

						if (alienWay)
						{
							std::vector<std::pair<double, std::pair<size_t, size_t> > > pairs;

							for (size_t i = 0; i < list->size(); i++)
							{
								for (size_t k = 0; part[i] == p && k < list->size(); k++)
								{
									if (part[k] == main)
									{
										pairs.push_back({ Gap((*list)[i], (*list)[k]), { i, k } });
									}
								}
							}

							std::sort(pairs.begin(), pairs.end());

							for (size_t t = 0; t < pairs.size() && t < kAutoPlotRepairMostWays && !joined; t++)
							{
								size_t const a = pairs[t].second.first, b = pairs[t].second.second;

								if (ClimbedBetween(*list, a, b, scale, alienWay, modules))
								{
									AddLink(&(*list)[a], (int32_t)b, kLinkAlien);
									AddLink(&(*list)[b], (int32_t)a, kLinkAlien);
									joined = true;
								}
							}
						}

						if (joined)
						{
							counts.areasJoined++;
						}
						else
						{
							for (size_t i = 0; i < list->size(); i++)   // (a): the area with fewer exits taken out
							{
								if (part[i] == p)
								{
									keep[i] = false;
									counts.boxesTakenOut++;
								}
							}

							for (size_t e = 0; e < heldBy.size(); e++)
							{
								if (heldBy[e] == p)
								{
									counts.exitsLeftOut++;
									counts.leftOutExits.push_back((int32_t)e);
								}
							}

							counts.areasTakenOut++;
						}

						changed = true;
					}
				}

				Keep(list, keep, &where);
			}
			else if (centre)
			{
				size_t const b = (size_t)centre->box, o = (size_t)centre->other;

				if (b >= list->size() || o >= list->size())
				{
					break;
				}

				if (Holds((*list)[b], (*list)[o].centre, scale))
				{
					MakeOne(list, std::min(b, o), std::max(b, o), &where);   // rule 3
					counts.madeOne++;
					changed = true;
				}
				else if (Climbing((*list)[b]))
				{
					int32_t const place[3] = { (*list)[b].centre[0], (*list)[b].centre[1], (*list)[b].centre[2] };

					if (TrimFor(list, o, place, exits, scale))   // rule 4
					{
						counts.trimmed++;
						changed = true;
					}
					else if (MoveCentre(list, b, scale))
					{
						counts.centresMoved++;
						changed = true;
					}
				}
				else if (MoveCentre(list, b, scale))   // rule 5
				{
					counts.centresMoved++;
					changed = true;
				}
				else
				{
					int32_t const place[3] = { (*list)[b].centre[0], (*list)[b].centre[1], (*list)[b].centre[2] };

					if (TrimFor(list, o, place, exits, scale))
					{
						counts.trimmed++;
						changed = true;
					}
				}

				if (!changed)
				{
					MakeOne(list, std::min(b, o), std::max(b, o), &where);   // the last way out: one box
					counts.madeOne++;
					changed = true;
				}
			}

			if (!changed)
			{
				break;
			}
		}

		std::vector<WaypointProblem> const after = Waypoints_Check(*list, exits, scale, 1000);

		counts.left = after.size();

		if (renumber)
		{
			*renumber = where;
		}

		bool const did = counts.partsTakenOut || counts.partsLinked || counts.madeOne || counts.trimmed || counts.centresMoved || counts.areasJoined || counts.areasTakenOut;

		if (words && (did || counts.left))
		{
			std::string w = "repair:";

			auto const add = [&](size_t n, std::string const &what) {
				if (n)
				{
					w += (w.size() > 7 ? ", " : " ") + std::to_string((unsigned long long)n) + " " + what;
				}
			};

			add(counts.partsTakenOut, "part(s) no exit reaches taken out (" + std::to_string((unsigned long long)counts.boxesTakenOut) + " boxes)");
			add(counts.partsLinked, "near part(s) linked");
			add(counts.madeOne, "pair(s) of boxes made one");
			add(counts.trimmed, "box(es) trimmed off a climbing centre");
			add(counts.centresMoved, "centre(s) moved out of another box");
			add(counts.areasJoined, "area(s) with an exit joined by an aliens-only link where the grid climbs between them");
			std::string leftOut;

			for (int32_t const e : counts.leftOutExits)
			{
				leftOut += ", exit " + std::to_string((long long)e) + " left out, rule 6(a): no alien way to the rest";
			}

			add(counts.areasTakenOut, "area(s) with an exit taken out (no climbing way to the area with more exits" + leftOut + ")");

			if (counts.left)
			{
				w += std::string(did ? "; " : " ") + std::to_string((unsigned long long)counts.left) + " problem(s) left (" + after[0].words + ")";
			}

			*words = w;
		}

		return counts;
	}

	AutoPlotRepairCounts AutoPlot_RepairPlan(AutoPlotPlan *plan, std::vector<WaypointPoint> const &exits, double scale, AutoPlotRepairWay const &alienWay,
		std::vector<int32_t> const &modules)
	{
		if (!plan)
		{
			return AutoPlotRepairCounts();
		}

		std::string words;
		std::vector<int32_t> renumber;
		AutoPlotRepairCounts const counts = AutoPlot_Repair(&plan->list, exits, scale, &words, &renumber, alienWay, modules);

		if (!words.empty())
		{
			plan->notes.push_back(words);
		}

		auto const to = [&](int32_t old) { return old >= 0 && (size_t)old < renumber.size() ? renumber[(size_t)old] : -1; };

		// each box's kind: the kept one's (of boxes made one, the first as they came: the one MakeOne keeps)
		std::vector<bool> climbing(plan->list.size(), false), set(plan->list.size(), false);

		for (size_t i = 0; i < renumber.size(); i++)
		{
			int32_t const n = renumber[i];

			if (n >= 0 && (size_t)n < climbing.size() && !set[(size_t)n])
			{
				climbing[(size_t)n] = i < plan->climbing.size() && plan->climbing[i];
				set[(size_t)n] = true;
			}
		}

		plan->climbing.swap(climbing);

		for (int32_t &d : plan->doorBox)
		{
			d = to(d);
		}

		for (int32_t &e : plan->exitBox)
		{
			e = to(e);
		}

		for (std::vector<int32_t> &floors : plan->doorFloors)
		{
			std::vector<int32_t> kept;

			for (int32_t const f : floors)
			{
				int32_t const n = to(f);

				if (n >= 0 && std::find(kept.begin(), kept.end(), n) == kept.end())
				{
					kept.push_back(n);
				}
			}

			floors.swap(kept);
		}

		plan->floorBoxes = (size_t)std::count(plan->climbing.begin(), plan->climbing.end(), false);
		plan->exitsLeftOut = counts.leftOutExits;   // the exits keep their numbers (only boxes are renumbered)

		int parts = 0;

		Parts(plan->list, &parts);
		plan->parts = (size_t)parts;
		return counts;
	}

	size_t AutoPlot_RepairExceptions(std::vector<WaypointProblem> *problems, std::vector<int32_t> const &leftOutExits, std::vector<WaypointProblem> *excepted)
	{
		if (!problems)
		{
			return 0;
		}

		std::vector<WaypointProblem> kept;
		size_t moved = 0;

		for (WaypointProblem const &p : *problems)
		{
			if (p.kind == WaypointProblemKind::ExitOutside && std::find(leftOutExits.begin(), leftOutExits.end(), p.other) != leftOutExits.end())
			{
				moved++;

				if (excepted)
				{
					excepted->push_back(p);
				}
			}
			else
			{
				kept.push_back(p);
			}
		}

		problems->swap(kept);
		return moved;
	}
}
