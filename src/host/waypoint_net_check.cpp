// See waypoint_net_check.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_net_check.h"

#include <string.h>

#include "waypoint_room.h"   // avpx/3dc: the stock room sum, and the loader's scaling of one corner

namespace host
{
	namespace
	{
		struct Problems
		{
			std::vector<WaypointProblem> list;
			size_t most;
			size_t more = 0;

			void Add(WaypointProblemKind kind, int32_t box, int32_t other, int axis, std::string const &words)
			{
				if (list.size() >= most)
				{
					more++;
					return;
				}

				WaypointProblem problem;

				problem.kind = kind;
				problem.box = box;
				problem.other = other;
				problem.axis = axis;
				problem.words = words;
				list.push_back(problem);
			}
		};

		char const kAxis[3] = { 'x', 'y', 'z' };

		std::string Box(size_t box)
		{
			return "box " + std::to_string(box);
		}

		// A box as the game holds it after loading, on one axis: its lowest and highest place, scaled (the highest 1 less, as the loader takes 1 off it). False when a number cannot be.
		bool Span(WaypointRec const &box, int axis, double scale, int32_t *low, int32_t *high)
		{
			int32_t top = 0;

			if (!WaypointScaledCorner(box.min[axis], scale, low) || !WaypointScaledCorner(box.max[axis], scale, &top) || top == INT32_MIN)
			{
				return false;
			}

			*high = top - 1;
			return true;
		}

		// The game's WaypointContainsPoint, on the scaled numbers
		bool Holds(WaypointRec const &box, int32_t const place[3], double scale)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				int32_t low = 0, high = 0, at = 0;

				if (!Span(box, axis, scale, &low, &high) || !WaypointScaledCorner(place[axis], scale, &at) || at < low || at > high)
				{
					return false;
				}
			}

			return true;
		}

		// Which boxes `from` reaches by aliens' steps (forward), or which reach it (backward): V steps to W when W lists V without 0x1. Links outside the list are not followed.
		std::vector<bool> Reached(std::vector<WaypointRec> const &alien, size_t from, bool forward)
		{
			size_t const n = alien.size();
			std::vector<bool> seen(n, false);
			std::vector<size_t> todo;

			// steps[v]: the boxes v steps to (W lists v); backs[w]: the boxes that step to w (w's own links)
			std::vector<std::vector<size_t> > steps(n);

			for (size_t w = 0; w < n; w++)
			{
				for (WaypointLinkRec const &link : alien[w].links)
				{
					if (link.index >= 0 && (size_t)link.index < n && (link.flags & kWaypointLinkOneWay) == 0)
					{
						if (forward)
							steps[(size_t)link.index].push_back(w);
						else
							steps[w].push_back((size_t)link.index);
					}
				}
			}

			seen[from] = true;
			todo.push_back(from);

			while (!todo.empty())
			{
				size_t const at = todo.back();
				todo.pop_back();

				for (size_t const next : steps[at])
				{
					if (!seen[next])
					{
						seen[next] = true;
						todo.push_back(next);
					}
				}
			}

			return seen;
		}
	}

	char const *WaypointProblemName(WaypointProblemKind kind)
	{
		switch (kind)
		{
			case WaypointProblemKind::TooMany: return "too many boxes";
			case WaypointProblemKind::NoRoom: return "no room";
			case WaypointProblemKind::LargeTargetSmallBox: return "large target into a box with no room";
			case WaypointProblemKind::LinkOutside: return "link outside the list";
			case WaypointProblemKind::CannotReach: return "cannot reach";
			case WaypointProblemKind::ExitOutside: return "exit outside every box";
			case WaypointProblemKind::CentreOutside: return "centre outside its box";
			case WaypointProblemKind::CentreInAnother: return "centre in another box";
			case WaypointProblemKind::BadScale: return "bad numbers";
		}

		return "an unknown problem";
	}

	std::vector<WaypointProblem> Waypoints_Check(std::vector<WaypointRec> const &alien, std::vector<WaypointPoint> const &exits, double scale, size_t most, size_t *more)
	{
		Problems problems;

		problems.most = most;

		int32_t probe = 0;

		if (!WaypointScaledCorner(0, scale, &probe) || !(scale > 0.0))
		{
			problems.Add(WaypointProblemKind::BadScale, -1, -1, -1, "the level's scale is no number the sums can use");

			if (more)
				*more = problems.more;

			return problems.list;
		}

		size_t const n = alien.size();

		if (n > kWaypointsMostPerModule)
		{
			problems.Add(WaypointProblemKind::TooMany, -1, -1, -1, std::to_string(n) + " boxes: the stock route search takes 99 at most");
		}

		// every box: room on every axis, its links in the list, and a large target only into a box with room
		std::vector<bool> roomy(n, true);

		for (size_t b = 0; b < n; b++)
		{
			WaypointRec const &box = alien[b];

			for (int axis = 0; axis < 3; axis++)
			{
				int32_t room = 0;

				if (box.min[axis] > box.max[axis])
				{
					roomy[b] = false;
					problems.Add(WaypointProblemKind::NoRoom, (int32_t)b, -1, axis, Box(b) + ": its lowest corner is past its highest on " + kAxis[axis]);
				}
				else if (!WaypointRoom(box.min[axis], box.max[axis], box.centre[axis], scale, &room))
				{
					roomy[b] = false;
					problems.Add(WaypointProblemKind::BadScale, (int32_t)b, -1, axis, Box(b) + ": a corner on " + std::string(1, kAxis[axis]) + " is no number the game can hold");
				}
				else if (room < 1)
				{
					roomy[b] = false;
					problems.Add(WaypointProblemKind::NoRoom, (int32_t)b, -1, axis, Box(b) + ": room " + std::to_string(room) + " on " + kAxis[axis] + " (1 or more is needed)");
				}
			}

			bool largeTarget = false;

			for (WaypointLinkRec const &link : box.links)
			{
				if (link.index < 0 || (size_t)link.index >= n)
				{
					problems.Add(WaypointProblemKind::LinkOutside, (int32_t)b, link.index, -1, Box(b) + ": a link to " + std::to_string(link.index) + ", outside the list of " + std::to_string(n));
				}

				largeTarget = largeTarget || (link.flags & kWaypointLinkLargeTarget) != 0;
			}

			if (largeTarget && !roomy[b])
			{
				problems.Add(WaypointProblemKind::LargeTargetSmallBox, (int32_t)b, -1, -1, Box(b) + ": a large-target link (0x8) leads into it, and it has no room: the stock game divides by 0");
			}
		}

		// every box reaches every other: each reaches box 0, and box 0 reaches each
		if (n > 1)
		{
			std::vector<bool> const fromFirst = Reached(alien, 0, true);
			std::vector<bool> const toFirst = Reached(alien, 0, false);

			for (size_t b = 1; b < n; b++)
			{
				if (!fromFirst[b])
				{
					problems.Add(WaypointProblemKind::CannotReach, 0, (int32_t)b, -1, "box 0 cannot reach " + Box(b));
				}

				if (!toFirst[b])
				{
					problems.Add(WaypointProblemKind::CannotReach, (int32_t)b, 0, -1, Box(b) + " cannot reach box 0");
				}
			}
		}

		// every exit point in a box
		for (size_t e = 0; e < exits.size(); e++)
		{
			bool held = false;

			for (size_t b = 0; b < n && !held; b++)
			{
				held = Holds(alien[b], exits[e].place, scale);
			}

			if (!held)
			{
				problems.Add(WaypointProblemKind::ExitOutside, -1, (int32_t)e, -1, "exit " + std::to_string(e) + " (" + std::to_string(exits[e].place[0]) + " " +
					std::to_string(exits[e].place[1]) + " " + std::to_string(exits[e].place[2]) + ") lies in no box");
			}
		}

		// every centre in its own box and in no other
		for (size_t b = 0; b < n; b++)
		{
			if (!Holds(alien[b], alien[b].centre, scale))
			{
				problems.Add(WaypointProblemKind::CentreOutside, (int32_t)b, -1, -1, Box(b) + ": its centre is outside it");
			}

			for (size_t o = 0; o < n; o++)
			{
				if (o != b && Holds(alien[o], alien[b].centre, scale))
				{
					problems.Add(WaypointProblemKind::CentreInAnother, (int32_t)b, (int32_t)o, -1, Box(b) + ": its centre is in " + Box(o) + " too");
				}
			}
		}

		if (more)
			*more = problems.more;

		return problems.list;
	}
}
