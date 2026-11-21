// autoplot_shortcut.cpp: THE FLOOR SHORTCUTS of THE LANES, a pass over the finished plan (autoplot_shortcut.h). Written 2026-11-20 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_shortcut.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <utility>

#include "autoplot_lane.h"
#include "waypoint_net_check.h"

namespace host
{
	namespace
	{
		// a box's link to another: its place in the box's own list, -1 for none
		int LinkTo(WaypointRec const &w, int32_t to)
		{
			for (size_t k = 0; k < w.links.size(); k++)
			{
				if (w.links[k].index == to)
				{
					return (int)k;
				}
			}

			return -1;
		}

		bool InList(std::vector<WaypointRec> const &list, int32_t at)
		{
			return at >= 0 && (size_t)at < list.size();
		}

		bool Climbing(AutoPlotPlan const &plan, int32_t at)
		{
			return at >= 0 && (size_t)at < plan.climbing.size() && plan.climbing[(size_t)at];
		}

		// the box the stock game takes as holding a place: the LAST that holds it (SweepWaypoints), each of the file's numbers times the scale cut to a whole one, lowest <=
		// place <= highest - 1 (the loader; autoplot_lane.cpp's BoxHolds). -1 for none
		int32_t TargetBox(std::vector<WaypointRec> const &list, std::vector<int32_t> const &place, double scale)
		{
			int32_t found = -1;

			for (size_t i = 0; place.size() == 3 && i < list.size(); i++)
			{
				bool holds = true;

				for (int a = 0; a < 3; a++)
				{
					holds = holds && (double)place[(size_t)a] >= trunc((double)list[i].min[a] * scale) && (double)place[(size_t)a] <= trunc((double)list[i].max[a] * scale) - 1.0;
				}

				found = holds ? (int32_t)i : found;
			}

			return found;
		}

		// V may no longer step to W: W lists V with One way (as inteng's Add Link makes "only W to V"), V's link to W, when it has one, One way the other direction. When V
		// is already stopped the other way (W may not step to V: V lists W with One way), the two are no longer joined at all: the link taken out, both ways (inteng's Delete
		// Link). False, nothing changed, when W does not list V, or a link is one-way some other way (the planner's own)
		bool StopStep(std::vector<WaypointRec> *list, int32_t v, int32_t w)
		{
			if (!InList(*list, v) || !InList(*list, w) || v == w)
			{
				return false;
			}

			int const atW = LinkTo((*list)[(size_t)w], v), atV = LinkTo((*list)[(size_t)v], w);

			if (atW < 0)
			{
				return false;
			}

			int32_t const kOneWays = kLinkOneWay | kLinkOneWayBack;
			int32_t const flagsW = (*list)[(size_t)w].links[(size_t)atW].flags, flagsV = atV >= 0 ? (*list)[(size_t)v].links[(size_t)atV].flags : 0;

			if ((flagsW & kOneWays) == kLinkOneWayBack && atV >= 0 && (flagsV & kOneWays) == kLinkOneWay)
			{
				return Waypoints_DeleteLink(list, v, w);   // stopped both ways: no link
			}

			if ((flagsW & kOneWays) != 0 || (flagsV & kOneWays) != 0)
			{
				return false;
			}

			(*list)[(size_t)w].links[(size_t)atW].flags |= kLinkOneWay;

			if (atV >= 0)
			{
				(*list)[(size_t)v].links[(size_t)atV].flags |= kLinkOneWayBack;
			}

			return true;
		}

		// What the plan breaks: each lane along it that is not continuous ("L i j": from exit i's door box toward exit j, as the planner's BrokenOf measures it; "A i j": the
		// alien arriving through exit i's doorway toward exit j, on every link shift, as Drake's gate measures it), each climbing box's walk toward each exit that does not hold
		// ("W c j"); and how many problems Frost's checker finds. Not the doorway floor boxes' climbs: the planner's BrokenOf counts those only when asked (Bishop 11-20: a floor
		// alien that walks is no stall), and here they refused cuts that made every lane of tolift hold
		struct Measure
		{
			std::set<std::string> broken;
			std::set<std::pair<size_t, size_t> > lanesBroken;   // (i, j): exit i's door box toward exit j; (kArrivalLane + i, j): the alien arriving through exit i's doorway
			size_t lanes = 0;
			size_t problems = 0;
		};

		size_t const kArrivalLane = (size_t)1 << 20;   // a lane's first number from here on: an arrival lane (no level has that many exits)

		// where a lane starts: exit i's door box, or for an ARRIVAL lane (Drake's gate: the alien carried through exit i's doorway, along the next module from the box that
		// holds it 600 past the doorway's middle) the box the stock choice starts from there, a hanging alien heading for exit j (Stock_Choose's start; its position as
		// Lane_Cross places it: 900 under the soffit, square to the doorway). -1 when there is none (no door box; no doorway, soffit or box holding it there)
		int32_t LaneStart(std::vector<WaypointRec> const &list, AutoPlotPlan const &plan, std::vector<AutoPlotDoor> const &doors, std::vector<std::vector<int32_t> > const &exits,
			size_t first, size_t j, double scale)
		{
			if (first < kArrivalLane)
			{
				return first < plan.doorBox.size() && InList(list, plan.doorBox[first]) ? plan.doorBox[first] : -1;
			}

			size_t const i = first - kArrivalLane;

			if (i >= doors.size() || !doors[i].throughFound || !doors[i].soffitFound || j >= exits.size() || exits[j].size() != 3)
			{
				return -1;
			}

			AutoPlotDoor const &d = doors[i];
			int32_t const into = 600;   // the doorway's way through points out of this AI module: the arriving alien is that far back along it
			bool const squareX = fabs(d.through[0]) >= fabs(d.through[1]);
			int32_t const at[3] = { squareX ? (int32_t)floor(d.middle[0] - d.through[0] * into + 0.5) : d.soffit[0], d.soffit[1] + kStockAlienTop,
				squareX ? d.soffit[2] : (int32_t)floor(d.middle[2] - d.through[1] * into + 0.5) };
			StockChoice const c = Stock_Choose(list, scale, at, exits[j].data(), true, 0);

			return c.holding && InList(list, c.start) ? c.start : -1;
		}

		Measure MeasureOf(std::vector<WaypointRec> const &list, AutoPlotPlan const &plan, std::vector<std::vector<int32_t> > const &exits,
			std::vector<WaypointPoint> const &exitsFile, std::vector<AutoPlotDoor> const &doors, double scale)
		{
			Measure m;

			// THE ARRIVAL LANES, walked on every link shift's route (Lane_ThroughAll: the game gives each alien its own link shift, so a tie with a floor way is a break)
			for (size_t i = 0; i < exits.size() && i < doors.size(); i++)
			{
				for (size_t j = 0; j < exits.size() && j < exitsFile.size(); j++)   // (toward the level's exits only: an ARRIVAL, autoplot_arrival.h, is no alien's target)
				{
					int32_t const start = j == i ? -1 : LaneStart(list, plan, doors, exits, kArrivalLane + i, j, scale);

					if (start < 0)
					{
						continue;
					}

					m.lanes++;

					if (!Lane_ThroughAll(list, scale, start, exits[j].data(), plan.climbing).continuous)
					{
						m.lanesBroken.insert(std::make_pair(kArrivalLane + i, j));
						m.broken.insert("A " + std::to_string((unsigned long long)i) + " " + std::to_string((unsigned long long)j));
					}
				}
			}

			// every climbing box's walk toward each exit ("W c j"): a lane may start at any of them (Drake's gate starts B's walk at the box holding the alien 600 past the
			// doorway, not always a door box), so none that held may break
			for (size_t c = 0; c < list.size() && c < plan.climbing.size(); c++)
			{
				for (size_t j = 0; plan.climbing[c] && j < exits.size() && j < exitsFile.size(); j++)
				{
					if (exits[j].size() == 3 && !Lane_Through(list, scale, (int32_t)c, exits[j].data(), plan.climbing).continuous)
					{
						m.broken.insert("W " + std::to_string((unsigned long long)c) + " " + std::to_string((unsigned long long)j));
					}
				}
			}

			for (size_t i = 0; i < exits.size() && i < plan.doorBox.size(); i++)
			{
				for (size_t j = 0; j < exits.size() && j < exitsFile.size(); j++)
				{
					if (j == i || exits[j].size() != 3)
					{
						continue;
					}

					if (InList(list, plan.doorBox[i]))
					{
						m.lanes++;

						// (link shift 0's walk, as the planner measures it: walked on every shift these chase ties the gate does not count as lane ends, and the cuts made
						// stall and back places, Dietrich 11-20)
						if (!Lane_Through(list, scale, plan.doorBox[i], exits[j].data(), plan.climbing).continuous)
						{
							m.lanesBroken.insert(std::make_pair(i, j));
							m.broken.insert("L " + std::to_string((unsigned long long)i) + " " + std::to_string((unsigned long long)j));
						}
					}
				}
			}

			size_t more = 0;

			m.problems = Waypoints_Check(list, exitsFile, scale, 1000, &more).size() + more;
			return m;
		}

		// THE EXIT'S END at exit k (rules 1 and 2). RULE 1 only where its doorway has a CLIMB STATION (autoplot_plan.h: the door box's centre over the top of a wall way, so an
		// alien in the exit box heading for it walks to that wall and climbs; with none, the centre straight overhead stalls it: Drake's gate counted 10 more stall and back
		// places in 3 AI modules when the join was made at every exit): its door box joined to its target's box (aliens only, both ways) when they are not. RULE 2 at every
		// exit whose box steps up to its door box (joined before, or by rule 1; Dietrich 11-20: it only stops steps, it adds no way up, so it makes no new stall): the target's
		// box, a floor box, stopped from stepping on to any other floor box (it may still step up to its door box, and floor boxes may still step into it). Gives the changes
		size_t EndAtExit(std::vector<WaypointRec> *list, AutoPlotPlan const &plan, size_t k, std::vector<std::vector<int32_t> > const &exits,
			std::vector<AutoPlotDoor> const &doors, double scale, size_t *joined, size_t *madeOneWay)
		{
			int32_t const door = k < plan.doorBox.size() ? plan.doorBox[k] : -1;
			int32_t const box = k < exits.size() ? TargetBox(*list, exits[k], scale) : -1;

			if (k >= doors.size() || !InList(*list, door) || !InList(*list, box) || door == box || Climbing(plan, box) || exits[k].size() != 3)
			{
				return 0;   // no lane at this exit, no box holds its point, or a climbing box does (not a floor end)
			}

			// A HANGING-BAND EXIT (Dietrich's: the exit point less than a hanging alien's height, kStockAlienTop, under its doorway's top; its own box a band between a hanging
			// alien's two corner heights): no end made there. At Genshd1's start exit 0 -> top (779 under) rules 1 and 2 made 4 back places in Drake's gate. The doorway's top is
			// its soffit, else the door box's top
			double const top = doors[k].soffitFound ? (double)doors[k].soffit[1] : trunc((double)(*list)[(size_t)door].min[1] * scale);

			if ((double)exits[k][1] - top < (double)kStockAlienTop)
			{
				return 0;
			}

			size_t changes = 0;

			if (doors[k].stationFound && LinkTo((*list)[(size_t)door], box) < 0 && LinkTo((*list)[(size_t)box], door) < 0)
			{
				if (!Waypoints_AddLink(list, door, box, kLinkAlien))
				{
					return 0;
				}

				changes++;
				(*joined)++;
			}

			// rule 2 only when the box steps up to its door box (the door box lists it, not One way): else an alien standing there would have no way on at all
			int const up = LinkTo((*list)[(size_t)door], box);

			if (up < 0 || ((*list)[(size_t)door].links[(size_t)up].flags & kLinkOneWay) != 0)
			{
				return changes;
			}

			std::vector<int32_t> floors;

			for (WaypointLinkRec const &link : (*list)[(size_t)box].links)
			{
				if (InList(*list, link.index) && link.index != door && !Climbing(plan, link.index))
				{
					floors.push_back(link.index);
				}
			}

			for (int32_t const g : floors)
			{
				if (StopStep(list, box, g))
				{
					changes++;
					(*madeOneWay)++;
				}
			}

			return changes;
		}

		// RULE 3, one lane's chain (`first` as Measure's lanes: a door box's, or an arrival's): up to kAutoPlotShortcutChain steps down from a climbing box into a floor box
		// (not the target's) stopped, each where the lane's walk on every link shift now first leaves the ceiling (a tie's floor branches one after another). Gives how many
		// were made (0: none could be)
		size_t Chain(std::vector<WaypointRec> *list, AutoPlotPlan const &plan, std::vector<AutoPlotDoor> const &doors, size_t first, size_t j,
			std::vector<std::vector<int32_t> > const &exits, double scale, size_t *madeOneWay)
		{
			size_t changes = 0;

			while (changes < kAutoPlotShortcutChain)
			{
				int32_t const start = LaneStart(*list, plan, doors, exits, first, j, scale);

				if (start < 0)
				{
					return changes;
				}

				LaneCrossing const walk = first >= kArrivalLane ? Lane_ThroughAll(*list, scale, start, exits[j].data(), plan.climbing) :
					Lane_Through(*list, scale, start, exits[j].data(), plan.climbing);   // (each lane walked as Measure walks it)
				std::vector<int32_t> route;
				int32_t target = -1;

				if (walk.continuous || !AutoPlot_ShortcutWords(walk.why, &route, &target) || route.size() < 2)
				{
					return changes;   // it holds, or it breaks some other way (not a floor box on the way): nothing more here
				}

				int32_t const floorBox = route.back(), from = route[route.size() - 2];

				if (floorBox == target || !Climbing(plan, from) || !StopStep(list, from, floorBox))
				{
					return changes;
				}

				changes++;
				(*madeOneWay)++;
			}

			return changes;
		}

		// a whole number read at *p (digits only, at most 9), *p moved past it; false when none
		bool ReadNumber(char const **p, int32_t *out)
		{
			char const *s = *p;
			long long v = 0;
			size_t digits = 0;

			while (*s >= '0' && *s <= '9' && digits < 9)
			{
				v = v * 10 + (*s - '0');
				s++;
				digits++;
			}

			if (digits == 0 || (*s >= '0' && *s <= '9'))
			{
				return false;   // no digits, or more than 9 (no list has that many boxes)
			}

			*out = (int32_t)v;
			*p = s;
			return true;
		}

		// the plan's state and the counts, with one more try judged: kept when nothing new breaks, fewer lanes break and Frost's checker finds no more problems
		struct Judge
		{
			AutoPlotPlan *plan;
			std::vector<std::vector<int32_t> > const &exits;
			std::vector<WaypointPoint> const &exitsFile;
			std::vector<AutoPlotDoor> const &doors;
			double scale;
			Measure now;
			AutoPlotShortcutCounts counts;

			bool Try(std::vector<WaypointRec> *trial, size_t joined, size_t madeOneWay)
			{
				counts.tried++;

				Measure after = MeasureOf(*trial, *plan, exits, exitsFile, doors, scale);
				bool nothingNew = true;

				for (std::string const &b : after.broken)
				{
					nothingNew = nothingNew && now.broken.count(b) != 0;
				}

				if (!nothingNew || after.lanesBroken.size() >= now.lanesBroken.size() || after.problems > now.problems)
				{
					return false;
				}

				plan->list.swap(*trial);
				now = after;
				counts.kept++;
				counts.joined += joined;
				counts.madeOneWay += madeOneWay;
				return true;
			}

			bool OutOfTries() const
			{
				return counts.tried >= kAutoPlotShortcutMostTries;
			}
		};
	}

	bool AutoPlot_ShortcutWords(std::string const &why, std::vector<int32_t> *route, int32_t *target)
	{
		// "box N on the way is a floor box (the route a b ... N, the target's box T)", Lane_Through's own words (autoplot_lane.cpp)
		static char const kHead[] = "box ", kMiddle[] = " on the way is a floor box (the route ", kTarget[] = ", the target's box ";

		if (!route || !target || why.compare(0, sizeof kHead - 1, kHead) != 0)
		{
			return false;
		}

		char const *p = why.c_str() + sizeof kHead - 1;
		int32_t floorBox = -1;

		if (!ReadNumber(&p, &floorBox) || strncmp(p, kMiddle, sizeof kMiddle - 1) != 0)
		{
			return false;
		}

		p += sizeof kMiddle - 1;

		std::vector<int32_t> boxes;

		for (;;)
		{
			int32_t n = -1;

			if (!ReadNumber(&p, &n) || boxes.size() >= 1000)
			{
				return false;
			}

			boxes.push_back(n);

			if (*p != ' ')
			{
				break;
			}

			p++;
		}

		int32_t t = -1;

		if (strncmp(p, kTarget, sizeof kTarget - 1) != 0)
		{
			return false;
		}

		p += sizeof kTarget - 1;

		if (!ReadNumber(&p, &t) || *p != ')' || boxes.back() != floorBox)
		{
			return false;
		}

		p++;

		// Lane_ThroughAll's words go on with the link shift that took that branch: "; link shift s"
		static char const kShift[] = "; link shift ";
		int32_t shift = -1;

		if (*p != '\0' && (strncmp(p, kShift, sizeof kShift - 1) != 0 || (p += sizeof kShift - 1, !ReadNumber(&p, &shift)) || *p != '\0'))
		{
			return false;
		}

		*route = boxes;
		*target = t;
		return true;
	}

	AutoPlotShortcutCounts AutoPlot_CutShortcuts(AutoPlotPlan *plan, std::vector<std::vector<int32_t> > const &exits, std::vector<WaypointPoint> const &exitsFile,
		std::vector<AutoPlotDoor> const &doors, double scale, bool exitEnds)
	{
		if (!plan || plan->list.empty() || !isfinite(scale) || scale <= 0.0)
		{
			return AutoPlotShortcutCounts();
		}

		Judge judge{ plan, exits, exitsFile, doors, scale, MeasureOf(plan->list, *plan, exits, exitsFile, doors, scale), AutoPlotShortcutCounts() };

		judge.counts.lanes = judge.now.lanes;
		judge.counts.continuousBefore = judge.now.lanes - judge.now.lanesBroken.size();

		if (judge.now.lanesBroken.empty())
		{
			judge.counts.continuousAfter = judge.counts.continuousBefore;
			return judge.counts;   // every lane along it holds: nothing to do
		}

		// RULES 1 AND 2 (when asked), every exit's end at once; else exit by exit
		if (exitEnds)
		{
			std::vector<WaypointRec> trial = plan->list;
			size_t joined = 0, madeOneWay = 0, changes = 0;

			for (size_t k = 0; k < exits.size() && k < exitsFile.size(); k++)   // (an arrival's box is no exit's end: no alien heads for it)
			{
				changes += EndAtExit(&trial, *plan, k, exits, doors, scale, &joined, &madeOneWay);
			}

			if (changes && !judge.Try(&trial, joined, madeOneWay))
			{
				for (size_t k = 0; k < exits.size() && k < exitsFile.size() && !judge.OutOfTries(); k++)
				{
					std::vector<WaypointRec> one = plan->list;
					size_t joinedOne = 0, madeOne = 0;

					if (EndAtExit(&one, *plan, k, exits, doors, scale, &joinedOne, &madeOne))
					{
						judge.Try(&one, joinedOne, madeOne);
					}
				}
			}
		}

		// RULE 3, lane by lane, rounds while one is kept
		for (size_t round = 0; round < kAutoPlotShortcutRounds && !judge.OutOfTries(); round++)
		{
			bool keptOne = false;
			std::set<std::pair<size_t, size_t> > const brokenNow = judge.now.lanesBroken;

			for (std::pair<size_t, size_t> const &lane : brokenNow)
			{
				if (judge.OutOfTries() || !judge.now.lanesBroken.count(lane))
				{
					continue;   // out of tries, or a chain kept before made this one hold too
				}

				std::vector<WaypointRec> trial = plan->list;
				size_t madeOneWay = 0;

				if (Chain(&trial, *plan, doors, lane.first, lane.second, exits, scale, &madeOneWay) != 0 && judge.Try(&trial, 0, madeOneWay))
				{
					keptOne = true;
				}
			}

			if (!keptOne)
			{
				break;
			}
		}

		AutoPlotShortcutCounts counts = judge.counts;

		counts.continuousAfter = judge.now.lanes - judge.now.lanesBroken.size();

		if (counts.kept)
		{
			// (before the repair's own note, which the refused line shows when it is the last)
			std::string const note = "shortcuts: lanes along " + std::to_string((unsigned long long)counts.continuousBefore) + " -> " +
				std::to_string((unsigned long long)counts.continuousAfter) + " of " + std::to_string((unsigned long long)counts.lanes) + " continuous (" +
				std::to_string((unsigned long long)counts.joined) + " door box(es) joined to their exit's box, " + std::to_string((unsigned long long)counts.madeOneWay) +
				" step(s) stopped)";
			bool const repairLast = !plan->notes.empty() && plan->notes.back().compare(0, 7, "repair:") == 0;

			plan->notes.insert(plan->notes.end() - (repairLast ? 1 : 0), note);
		}

		return counts;
	}
}
