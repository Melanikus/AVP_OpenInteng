// autoplot_volume_repair.cpp: see autoplot_volume_repair.h. Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "autoplot_volume_repair.h"

#include <math.h>

#include <algorithm>
#include <limits>
#include <string>

namespace host
{
	namespace
	{
		// The box's word in a note: the volume planner's kind when it gave one, else climbing or floor
		std::string RoleOf(AutoPlotPlan const &plan, size_t b)
		{
			if (b < plan.volumeRoles.size() && !plan.volumeRoles[b].empty())
			{
				return plan.volumeRoles[b];
			}

			return b < plan.climbing.size() && plan.climbing[b] ? "climb" : "floor";
		}

		bool HoldsAnExit(WaypointRec const &box, std::vector<WaypointPoint> const &exits, double scale)
		{
			for (WaypointPoint const &e : exits)
			{
				if (Waypoint_Holds(box, e.place, scale))
				{
					return true;
				}
			}

			return false;
		}

		// Does a box of the list other than b hold the place? (the checker's test)
		bool InAnother(std::vector<WaypointRec> const &list, size_t b, int32_t const place[3], double scale)
		{
			for (size_t o = 0; o < list.size(); o++)
			{
				if (o != b && Waypoint_Holds(list[o], place, scale))
				{
					return true;
				}
			}

			return false;
		}

		// A place on one axis for b's centre to try: the box's own two ends, and just outside each end of every other box (one and two past it, so that the loader's
		// rounding of a scaled corner cannot leave the place on the edge), those inside b only, the nearest to the centre first
		std::vector<int32_t> Tries(std::vector<WaypointRec> const &list, size_t b, int axis)
		{
			WaypointRec const &box = list[b];
			std::vector<int32_t> tries = { box.min[axis], box.max[axis] - 1 };

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

			int32_t const from = box.centre[axis];
			std::vector<int32_t> inside;

			for (int32_t const t : tries)
			{
				if (t >= box.min[axis] && t <= box.max[axis] - 1)
				{
					inside.push_back(t);
				}
			}

			std::sort(inside.begin(), inside.end(), [from](int32_t p, int32_t q)
			{
				long long const dp = llabs((long long)p - from), dq = llabs((long long)q - from);
				return dp != dq ? dp < dq : p < q;
			});
			inside.erase(std::unique(inside.begin(), inside.end()), inside.end());
			return inside;
		}

		// a's record of b and b's of a: aliens, neither way stopped (made when it isn't there)
		void LinkBothWays(std::vector<WaypointRec> &list, size_t a, size_t b)
		{
			for (int k = 0; k < 2; k++)
			{
				WaypointRec &from = list[k == 0 ? a : b];
				int32_t const to = (int32_t)(k == 0 ? b : a);
				bool found = false;

				for (WaypointLinkRec &l : from.links)
				{
					if (l.index == to)
					{
						l.flags = (l.flags & ~(kLinkOneWay | kLinkOneWayBack)) | kLinkAlien;
						found = true;
					}
				}

				if (!found)
				{
					WaypointLinkRec l;

					l.index = to;
					l.flags = kLinkAlien;
					from.links.push_back(l);
				}
			}
		}

		// Does w list v with no One way (the checker's step: v steps to w then)?
		bool Steps(std::vector<WaypointRec> const &list, size_t v, size_t w)
		{
			for (WaypointLinkRec const &l : list[w].links)
			{
				if (l.index == (int32_t)v && (l.flags & kLinkOneWay) == 0)
				{
					return true;
				}
			}

			return false;
		}

		// The boxes box 0 reaches (forward) or that reach box 0 (backward), by the checker's steps
		std::vector<bool> ReachOfFirst(std::vector<WaypointRec> const &list, bool forward)
		{
			std::vector<bool> seen(list.size(), false);
			std::vector<size_t> todo;

			if (!list.empty())
			{
				seen[0] = true;
				todo.push_back(0);
			}

			while (!todo.empty())
			{
				size_t const at = todo.back();

				todo.pop_back();

				for (size_t o = 0; o < list.size(); o++)
				{
					if (!seen[o] && (forward ? Steps(list, at, o) : Steps(list, o, at)))
					{
						seen[o] = true;
						todo.push_back(o);
					}
				}
			}

			return seen;
		}

		// a and b joined by a link either way
		bool Joined(std::vector<WaypointRec> const &list, size_t a, size_t b)
		{
			for (WaypointLinkRec const &l : list[a].links)
			{
				if (l.index == (int32_t)b)
				{
					return true;
				}
			}

			for (WaypointLinkRec const &l : list[b].links)
			{
				if (l.index == (int32_t)a)
				{
					return true;
				}
			}

			return false;
		}

		// V1's taking out: box b goes, every link to it goes, the links after it are renumbered, and the plan's lists beside the boxes or naming them follow
		void TakeOut(AutoPlotPlan *plan, size_t b)
		{
			bool const wasFloor = !(b < plan->climbing.size() && plan->climbing[b]);
			int32_t const gone = (int32_t)b;

			plan->list.erase(plan->list.begin() + (std::ptrdiff_t)b);

			for (WaypointRec &box : plan->list)
			{
				std::vector<WaypointLinkRec> kept;

				for (WaypointLinkRec link : box.links)
				{
					if (link.index != gone)
					{
						link.index -= link.index > gone ? 1 : 0;
						kept.push_back(link);
					}
				}

				box.links.swap(kept);
			}

			if (b < plan->climbing.size())
			{
				plan->climbing.erase(plan->climbing.begin() + (std::ptrdiff_t)b);
			}

			if (b < plan->volumeRoles.size())
			{
				plan->volumeRoles.erase(plan->volumeRoles.begin() + (std::ptrdiff_t)b);
			}

			if (wasFloor && plan->floorBoxes > 0)
			{
				plan->floorBoxes--;
			}

			auto const renumber = [gone](int32_t *at) { *at = *at == gone ? -1 : *at - (*at > gone ? 1 : 0); };

			for (int32_t &at : plan->doorBox)
			{
				renumber(&at);
			}

			for (int32_t &at : plan->exitBox)
			{
				renumber(&at);
			}

			for (std::vector<int32_t> &floors : plan->doorFloors)
			{
				std::vector<int32_t> kept;

				for (int32_t at : floors)
				{
					renumber(&at);

					if (at >= 0)
					{
						kept.push_back(at);
					}
				}

				floors.swap(kept);
			}
		}
	}

	AutoPlotVolumeRepair AutoPlot_RepairVolumes(AutoPlotPlan *plan, std::vector<WaypointPoint> const &exits, double scale)
	{
		AutoPlotVolumeRepair done;

		if (!plan || !plan->volumes)
		{
			return done;
		}

		std::vector<std::string> words;

		// V1, from the end so the places still to look at keep their numbers
		for (size_t b = plan->list.size(); b-- > 0;)
		{
			if (plan->list[b].links.empty() && !HoldsAnExit(plan->list[b], exits, scale))
			{
				words.push_back("V1 took out box " + std::to_string((unsigned long long)b) + " (" + RoleOf(*plan, b) + ": no links, no exit point)");
				TakeOut(plan, b);
				done.removed++;
			}
		}

		// V4: an exit point no box holds gets a box round it, as the planner's exit rule makes one (kAutoPlotVolumeRepairExitSide each side, from kAutoPlotVolumeRepairExitOver over
		// the point down to the foot of the floor box nearest it), its centre the point, linked both ways with that floor box (the levels' floor walk leaves out an exit
		// more than its reach over the floor: doormp06, office01 on the owner's 2nd stretch)
		for (size_t x = 0; x < exits.size(); x++)
		{
			bool held = false;

			for (WaypointRec const &box : plan->list)
			{
				held = held || Waypoint_Holds(box, exits[x].place, scale);
			}

			if (held)
			{
				continue;
			}

			int32_t const *const p = exits[x].place;
			size_t floorAt = std::numeric_limits<size_t>::max();
			double best = 0.0;

			for (size_t b = 0; b < plan->list.size(); b++)
			{
				if (RoleOf(*plan, b) != "floor")
				{
					continue;
				}

				WaypointRec const &f = plan->list[b];
				double const dx = std::max(0.0, std::max((double)f.min[0] - p[0], (double)p[0] - (f.max[0] - 1)));
				double const dz = std::max(0.0, std::max((double)f.min[2] - p[2], (double)p[2] - (f.max[2] - 1)));

				if (floorAt == std::numeric_limits<size_t>::max() || dx * dx + dz * dz < best)
				{
					best = dx * dx + dz * dz;
					floorAt = b;
				}
			}

			WaypointRec w = {};
			int32_t const foot = floorAt != std::numeric_limits<size_t>::max() ? plan->list[floorAt].max[1] : p[1] + kAutoPlotVolumeRepairExitOver;

			w.min[0] = p[0] - kAutoPlotVolumeRepairExitSide, w.max[0] = p[0] + kAutoPlotVolumeRepairExitSide + 1;
			w.min[2] = p[2] - kAutoPlotVolumeRepairExitSide, w.max[2] = p[2] + kAutoPlotVolumeRepairExitSide + 1;
			w.min[1] = p[1] - kAutoPlotVolumeRepairExitOver, w.max[1] = std::max(foot, p[1] + 2);
			std::copy(p, p + 3, w.centre);

			size_t const at = plan->list.size();

			plan->list.push_back(w);
			plan->climbing.resize(at, false);
			plan->climbing.push_back(false);

			if (!plan->volumeRoles.empty())
			{
				plan->volumeRoles.resize(at);
				plan->volumeRoles.push_back("exit");
			}

			if (x < plan->exitBox.size() && plan->exitBox[x] < 0)
			{
				plan->exitBox[x] = (int32_t)at;
			}

			std::string words4 = "V4 made box " + std::to_string((unsigned long long)at) + " (exit) round exit " + std::to_string((unsigned long long)x) + "'s point";

			if (floorAt != std::numeric_limits<size_t>::max())
			{
				LinkBothWays(plan->list, at, floorAt);
				words4 += ", linked both ways to box " + std::to_string((unsigned long long)floorAt) + " (floor)";
			}

			words.push_back(words4);
			done.made++;
		}

		// V5: every box reaches box 0 and box 0 reaches every box (the checker's rule). A part no link joins to box 0's is joined: the nearest two ceiling entries (one
		// each side, overlapping across and in height, at most kAutoPlotVolumeRepairJoinGap apart) by the second stretched to meet the first, else any two boxes at most
		// kAutoPlotVolumeRepairTouchGap apart; linked both ways. Then a box that can't reach box 0, or that box 0 can't reach, gets the links of its shortest way (either
		// direction) to one that can made both ways: a dead-end floor (MED01's north floor, its one link coming in from an arrival's exit box) gets its way back.
		for (int round = 0; round < 64 && plan->list.size() > 1; round++)
		{
			std::vector<WaypointRec> &boxes = plan->list;
			size_t const n = boxes.size();
			std::vector<bool> const fwd = ReachOfFirst(boxes, true), bwd = ReachOfFirst(boxes, false);

			if (std::find(fwd.begin(), fwd.end(), false) == fwd.end() && std::find(bwd.begin(), bwd.end(), false) == bwd.end())
			{
				break;
			}

			// box 0's part, by links either way
			std::vector<bool> part(n, false);
			std::vector<size_t> todo(1, 0);

			part[0] = true;

			while (!todo.empty())
			{
				size_t const at = todo.back();

				todo.pop_back();

				for (size_t o = 0; o < n; o++)
				{
					if (!part[o] && Joined(boxes, at, o))
					{
						part[o] = true;
						todo.push_back(o);
					}
				}
			}

			if (std::find(part.begin(), part.end(), false) != part.end())
			{
				size_t joinA = n, joinB = n;
				int joinAxis = -1;
				long long joinGap = 0;

				for (size_t a = 0; a < n; a++)
				{
					for (size_t b = 0; b < n; b++)
					{
						if (!part[a] || part[b] || RoleOf(*plan, a) != "entry" || RoleOf(*plan, b) != "entry")
						{
							continue;
						}

						WaypointRec const &p = boxes[a], &q = boxes[b];
						auto const overlap = [&](int k) { return std::min(p.max[k], q.max[k]) - std::max(p.min[k], q.min[k]) > 0; };

						for (int const k : { 0, 2 })
						{
							long long const gap = std::max((long long)q.min[k] - p.max[k], (long long)p.min[k] - q.max[k]) + 1;   // spans min .. max - 1

							if (gap > 0 && gap <= kAutoPlotVolumeRepairJoinGap && overlap(2 - k) && overlap(1) && (joinAxis < 0 || gap < joinGap))
							{
								joinA = a, joinB = b, joinAxis = k, joinGap = gap;
							}
						}
					}
				}

				if (joinAxis >= 0)
				{
					WaypointRec const &p = boxes[joinA];
					WaypointRec &q = boxes[joinB];

					if (q.min[joinAxis] >= p.max[joinAxis])
					{
						q.min[joinAxis] = p.max[joinAxis] - 1;   // both hold the line they meet on
					}
					else
					{
						q.max[joinAxis] = p.min[joinAxis] + 1;
					}

					LinkBothWays(boxes, joinA, joinB);
					words.push_back("V5 joined box " + std::to_string((unsigned long long)joinB) + " (entry) to box " + std::to_string((unsigned long long)joinA) + " (entry): stretched " +
						std::to_string(joinGap) + " mm along " + (joinAxis == 0 ? "x" : "z") + ", linked both ways");
					done.joined++;
					continue;
				}

				// else any two boxes this near: linked, no box changed
				for (size_t a = 0; a < n && joinAxis < 0; a++)
				{
					for (size_t b = 0; b < n && joinAxis < 0; b++)
					{
						if (!part[a] || part[b])
						{
							continue;
						}

						long long apart = 0;

						for (int k = 0; k < 3; k++)
						{
							apart = std::max(apart, std::max((long long)boxes[b].min[k] - boxes[a].max[k], (long long)boxes[a].min[k] - boxes[b].max[k]) + 1);
						}

						if (apart <= kAutoPlotVolumeRepairTouchGap)
						{
							LinkBothWays(boxes, a, b);
							words.push_back("V5 linked box " + std::to_string((unsigned long long)b) + " to box " + std::to_string((unsigned long long)a) + " both ways (" +
								std::to_string(std::max(apart, 0LL)) + " mm apart)");
							done.joined++;
							joinAxis = 3;
						}
					}
				}

				if (joinAxis < 0)
				{
					words.push_back("V5 left: a part no link joins to box 0's, and none of its boxes near enough to join");
					done.left++;
					break;
				}

				continue;
			}

			// one part: the first box that can't reach box 0 (or box 0 can't reach) gets its shortest way, by links either way, to one that can, made both ways
			size_t stuck = n;
			bool backward = true;

			for (size_t b = 0; b < n && stuck == n; b++)
			{
				if (!bwd[b])
				{
					stuck = b, backward = true;
				}
				else if (!fwd[b])
				{
					stuck = b, backward = false;
				}
			}

			std::vector<size_t> from(n, n);
			std::vector<size_t> queue(1, stuck);
			size_t found = n;

			from[stuck] = stuck;

			for (size_t head = 0; head < queue.size() && found == n; head++)
			{
				size_t const at = queue[head];

				for (size_t o = 0; o < n && found == n; o++)
				{
					if (from[o] == n && Joined(boxes, at, o))
					{
						from[o] = at;
						queue.push_back(o);
						found = (backward ? bwd[o] : fwd[o]) ? o : n;
					}
				}
			}

			if (found == n)
			{
				words.push_back("V5 left box " + std::to_string((unsigned long long)stuck) + ": no way to box 0 by links either way");
				done.left++;
				break;
			}

			std::string way;

			for (size_t at = found; at != stuck; at = from[at])
			{
				LinkBothWays(boxes, at, from[at]);
				way = std::to_string((unsigned long long)from[at]) + "-" + std::to_string((unsigned long long)at) + (way.empty() ? std::string() : ", " + way);
			}

			words.push_back("V5 gave box " + std::to_string((unsigned long long)stuck) + " (" + RoleOf(*plan, stuck) + ") its way " + (backward ? "back to box 0" : "from box 0") + ": " + way + " both ways");
			done.joined++;
		}

		// V2
		std::vector<WaypointRec> &list = plan->list;

		for (size_t b = 0; b < list.size(); b++)
		{
			WaypointRec &box = list[b];

			if (Waypoint_Holds(box, box.centre, scale) && !InAnother(list, b, box.centre, scale))
			{
				continue;
			}

			std::string const role = RoleOf(*plan, b);
			std::string const named = "box " + std::to_string((unsigned long long)b) + " (" + role + ")";

			if (role == "HIGH")
			{
				continue;   // V3's, below
			}

			if (role == "LOW")
			{
				words.push_back("V2 left " + named + ": its centre sits on its sloped face, never moved");
				done.left++;
				continue;
			}

			auto const placeIsFree = [&](int32_t const place[3]) { return Waypoint_Holds(box, place, scale) && !InAnother(list, b, place, scale); };   // (not "free": a reader would take free(place) for the C library's free)
			int const longSide = (long long)box.max[0] - box.min[0] >= (long long)box.max[2] - box.min[2] ? 0 : 2;
			int const axes[2] = { longSide, 2 - longSide };
			bool found = false;
			int32_t at[3] = { box.centre[0], box.centre[1], box.centre[2] };
			std::string how;

			// along the long side, then across: one axis at a time
			for (int k = 0; k < 2 && !found; k++)
			{
				for (int32_t const t : Tries(list, b, axes[k]))
				{
					int32_t place[3] = { box.centre[0], box.centre[1], box.centre[2] };

					place[axes[k]] = t;

					if (placeIsFree(place))
					{
						std::copy(place, place + 3, at);
						how = std::to_string((long long)llabs((long long)t - box.centre[axes[k]])) + " mm along " + (axes[k] == 0 ? "x" : "z");
						found = true;
						break;
					}
				}
			}

			// both: the nearest free pair
			if (!found)
			{
				double best = -1.0;
				std::vector<int32_t> const triesX = Tries(list, b, 0), triesZ = Tries(list, b, 2);

				for (int32_t const tx : triesX)
				{
					for (int32_t const tz : triesZ)
					{
						int32_t const place[3] = { tx, box.centre[1], tz };
						double const d = hypot((double)tx - box.centre[0], (double)tz - box.centre[2]);

						if ((best < 0.0 || d < best) && placeIsFree(place))
						{
							best = d;
							std::copy(place, place + 3, at);
						}
					}
				}

				found = best >= 0.0;
				how = found ? std::to_string((long long)lround(best)) + " mm on x and z" : std::string();
			}

			if (found)
			{
				words.push_back("V2 moved " + named + "'s centre " + how);
				std::copy(at, at + 3, box.centre);
				done.moved++;
			}
			else
			{
				words.push_back("V2 left " + named + ": no place in it outside the other boxes at its centre's height");
				done.left++;
			}
		}

		// V3: a HIGH's centre in another box (an entry, nearly always). The planner makes a HIGH on a floor rectangle's side, its centre 1 past the floor's edge and
		// kAutoPlotInto over the ceiling, the box reaching kAutoPlotVolumeClimbRoom back into the room and out to the face's foot (autoplot_volumes.cpp, Drake's hand6g).
		// From those numbers: the side (the axis on which the centre is kAutoPlotVolumeClimbRoom + 1 from the box's inner end), the face's slope (from the ceiling,
		// the box's top + kAutoPlotInto, down to the vertex, its bottom, over the face's reach), and so the place past the holders' outer end at the same height over
		// the face. (a) That place in the box and in no other: the centre slides there. (b) Else, a jump catcher (one link, to an entry holding its centre) is taken
		// out: that entry reaches past it and is the catcher there. (c) Else (a HIGH on a climb) it is left and named.
		std::vector<size_t> drops;

		for (size_t b = 0; b < list.size(); b++)
		{
			WaypointRec &box = list[b];

			if (RoleOf(*plan, b) != "HIGH" || (Waypoint_Holds(box, box.centre, scale) && !InAnother(list, b, box.centre, scale)))
			{
				continue;
			}

			std::string const named = "box " + std::to_string((unsigned long long)b) + " (HIGH)";
			std::vector<size_t> holders;

			for (size_t o = 0; o < list.size(); o++)
			{
				if (o != b && Waypoint_Holds(list[o], box.centre, scale))
				{
					holders.push_back(o);
				}
			}

			// the side: the axis whose inner end the centre is nearest (+1: the room on the box's low side, the face outward along +axis)
			int axis = 0, sign = 1;
			long long nearest = -1;

			for (int const a : { 0, 2 })
			{
				long long const fromLow = (long long)box.centre[a] - box.min[a], fromHigh = (long long)box.max[a] - 1 - box.centre[a];
				long long const d = (std::min)(fromLow, fromHigh);

				if (nearest < 0 || d < nearest)
				{
					nearest = d;
					axis = a;
					sign = fromLow <= fromHigh ? 1 : -1;
				}
			}

			// the holders' outer end on that side: the first place past them
			long long end = box.centre[axis];

			for (size_t const o : holders)
			{
				end = sign > 0 ? (std::max)(end, (long long)list[o].max[axis]) : (std::min)(end, (long long)list[o].min[axis] - 1);
			}

			long long const out = (end - box.centre[axis]) * sign;
			long long const faceReach = (sign > 0 ? (long long)box.max[axis] - box.centre[axis] : (long long)box.centre[axis] + 1 - box.min[axis]) - kAutoPlotInto;
			double const slope = faceReach > 0 ? (double)((long long)box.max[1] - ((long long)box.min[1] + kAutoPlotInto)) / (double)faceReach : 0.0;
			int32_t place[3] = { box.centre[0], box.centre[1], box.centre[2] };
			bool slid = false;

			if (out > 0 && !holders.empty())
			{
				place[axis] = (int32_t)end;
				place[1] = (int32_t)(box.centre[1] + lround((double)out * slope));
				slid = Waypoint_Holds(box, place, scale) && !InAnother(list, b, place, scale);
			}

			std::string const firstHolder = holders.empty() ? std::string("?") : "box " + std::to_string((unsigned long long)holders[0]) + " (" + RoleOf(*plan, holders[0]) + ")";

			if (slid)
			{
				words.push_back("V3 slid " + named + "'s centre " + std::to_string(out) + " mm out along its face (" + (axis == 0 ? "x" : "z") + "), " +
					std::to_string((long long)place[1] - box.centre[1]) + " mm down, past " + firstHolder);
				std::copy(place, place + 3, box.centre);
				done.moved++;
				continue;
			}

			bool const catcher = box.links.size() == 1 && std::find(holders.begin(), holders.end(), (size_t)box.links[0].index) != holders.end() &&
				RoleOf(*plan, (size_t)box.links[0].index) == "entry";

			if (catcher)
			{
				words.push_back("V3 took out " + named + ": a jump catcher inside its entry " + firstHolder + ", which reaches past its whole face");
				drops.push_back(b);
			}
			else
			{
				words.push_back("V3 left " + named + ": on a climb, and " + firstHolder + " reaches past its whole face (the planner's: no wall on that side)");
				done.left++;
			}
		}

		for (size_t k = drops.size(); k-- > 0;)   // from the last, so the earlier numbers hold
		{
			TakeOut(plan, drops[k]);
			done.removed++;
		}

		if (!words.empty())
		{
			std::string note = "repair:";

			for (size_t i = 0; i < words.size(); i++)
			{
				note += (i ? "; " : " ") + words[i];
			}

			plan->notes.push_back(note);
		}

		return done;
	}
}
