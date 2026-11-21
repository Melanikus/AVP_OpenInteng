// autoplot_nest.cpp: THE NESTED BOXES, the split and the order (autoplot_nest.h). Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_nest.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace host
{
	namespace
	{
		// whether box a lies inside box b: on every axis they overlap and a sticks out of b by at most kAutoPlotNestSlack (the file's numbers)
		bool Inside(WaypointRec const &a, WaypointRec const &b)
		{
			for (int k = 0; k < 3; k++)
			{
				if (a.max[k] <= b.min[k] || a.min[k] >= b.max[k])
				{
					return false;   // no overlap on this axis
				}

				if (b.min[k] - a.min[k] > kAutoPlotNestSlack || a.max[k] - b.max[k] > kAutoPlotNestSlack)
				{
					return false;   // a sticks out of b too far
				}
			}

			return true;
		}

		std::string Number(long long n)
		{
			return std::to_string(n);
		}

		// the game's own test of a place (the world's mm) in a box: on the loaded numbers, the file's times the level's scale, the loader taking 1 off each max (the
		// checker's Waypoint_Holds, the planner's GameHolds)
		bool Holds(WaypointRec const &b, double const p[3], double scale)
		{
			for (int a = 0; a < 3; a++)
			{
				if (p[a] < trunc((double)b.min[a] * scale) || p[a] > trunc((double)b.max[a] * scale) - 1.0)
				{
					return false;
				}
			}

			return true;
		}

		// whether a body standing at a place (the world's mm) is in a box: any of its 8 corners, 450 each way across, at the place and 900 over it (WaypointContainsNPC,
		// CE_ALIEN); `round` mm further out every way (0: the game's own test)
		bool BodyIn(WaypointRec const &b, double const p[3], double round, double scale)
		{
			for (int corner = 0; corner < 8; corner++)
			{
				double const c[3] = { p[0] + ((corner & 1) ? 450.0 + round : -450.0 - round), p[1] + ((corner & 2) ? -900.0 - round : round),
					p[2] + ((corner & 4) ? 450.0 + round : -450.0 - round) };

				if (Holds(b, c, scale))
				{
					return true;
				}
			}

			return false;
		}

		// a piece of a split entry looking down: x and z, the file's numbers (hi as a box's max: the box holds lo to hi - 1)
		struct Rect
		{
			int32_t lo[2], hi[2];
		};

		// whether a rectangle and a box meet looking down (overlap, or touch along an edge)
		bool Meets(Rect const &r, WaypointRec const &b)
		{
			return r.lo[0] <= b.max[0] && b.min[0] <= r.hi[0] && r.lo[1] <= b.max[2] && b.min[2] <= r.hi[1];
		}

		// the link from box a to box b with these flags, and back from b to a with those, unless a already has one (one link a pair: the planner's SetLink)
		void Link(std::vector<WaypointRec> *list, size_t a, size_t b, int32_t flagsAtA, int32_t flagsAtB)
		{
			for (WaypointLinkRec const &l : (*list)[a].links)
			{
				if (l.index == (int32_t)b)
				{
					return;
				}
			}

			WaypointLinkRec there, back;

			there.index = (int32_t)b;
			there.flags = flagsAtA;
			back.index = (int32_t)a;
			back.flags = flagsAtB;
			(*list)[a].links.push_back(there);
			(*list)[b].links.push_back(back);
		}

		// the flags at a box's end of its link to another; with none (a link the list holds one way only), the other end's flags turned round
		int32_t FlagsBack(WaypointRec const &from, int32_t to, int32_t there)
		{
			for (WaypointLinkRec const &l : from.links)
			{
				if (l.index == to)
				{
					return l.flags;
				}
			}

			return (there & ~(kLinkOneWay | kLinkOneWayBack)) | ((there & kLinkOneWay) ? kLinkOneWayBack : 0) | ((there & kLinkOneWayBack) ? kLinkOneWay : 0);
		}

		// THE AIM of box p (Bishop's (C'): "each piece's aim sits where a body is in no other box"): of the places on a grid over the box (about 100 mm apart, at most
		// 200 a side: a hostile box can't make it run on), at `want`'s height, the one nearest `want` that the box holds and where a body standing there, `round` round
		// it, is in no other box of the list. False: no such place (the aim is then left as it was)
		bool AimAt(std::vector<WaypointRec> *list, size_t p, int32_t const want[3], double round, double scale)
		{
			WaypointRec &b = (*list)[p];
			int32_t const y = std::min(std::max(want[1], b.min[1]), b.max[1] - 1);
			int32_t n[2];

			for (int k = 0; k < 2; k++)
			{
				int const a = k == 0 ? 0 : 2;
				double const across = ((double)b.max[a] - (double)b.min[a]) * scale;

				n[k] = (int32_t)std::min(200.0, std::max(1.0, floor(across / 100.0)));
			}

			std::vector<std::pair<double, std::pair<int32_t, int32_t> > > places;

			places.reserve((size_t)n[0] * (size_t)n[1]);

			for (int32_t i = 0; i < n[0]; i++)
			{
				for (int32_t j = 0; j < n[1]; j++)
				{
					int32_t const x = (int32_t)floor((double)b.min[0] + ((double)i + 0.5) * ((double)b.max[0] - (double)b.min[0]) / (double)n[0]);
					int32_t const z = (int32_t)floor((double)b.min[2] + ((double)j + 0.5) * ((double)b.max[2] - (double)b.min[2]) / (double)n[1]);
					double const dx = ((double)x - (double)want[0]) * scale, dz = ((double)z - (double)want[2]) * scale;

					places.push_back(std::make_pair(dx * dx + dz * dz, std::make_pair(x, z)));
				}
			}

			std::stable_sort(places.begin(), places.end(), [](std::pair<double, std::pair<int32_t, int32_t> > const &u, std::pair<double, std::pair<int32_t, int32_t> > const &v)
				{ return u.first < v.first; });

			for (auto const &place : places)
			{
				int32_t const at[3] = { place.second.first, y, place.second.second };
				double const w[3] = { trunc((double)at[0] * scale), trunc((double)at[1] * scale), trunc((double)at[2] * scale) };
				bool free = Holds(b, w, scale);

				for (size_t j = 0; j < list->size() && free; j++)
				{
					free = j == p || !BodyIn((*list)[j], w, round, scale);
				}

				if (free)
				{
					for (int a = 0; a < 3; a++)
					{
						b.centre[a] = at[a];
					}

					return true;
				}
			}

			return false;
		}

		// the pieces of `outer` round `inner` (clipped to it), one way: strips across its whole length along x first (west, east, then south and north between them),
		// or along z first; a side the inner reaches is no piece
		std::vector<Rect> Pieces(WaypointRec const &outer, WaypointRec const &inner, bool xFirst)
		{
			int32_t const ox0 = outer.min[0], ox1 = outer.max[0], oz0 = outer.min[2], oz1 = outer.max[2];
			int32_t const ix0 = std::max(inner.min[0], ox0), ix1 = std::min(inner.max[0], ox1), iz0 = std::max(inner.min[2], oz0), iz1 = std::min(inner.max[2], oz1);
			std::vector<Rect> r;
			auto const add = [&r](int32_t x0, int32_t x1, int32_t z0, int32_t z1)
			{
				if (x1 > x0 && z1 > z0)
				{
					r.push_back(Rect{ { x0, z0 }, { x1, z1 } });
				}
			};

			if (xFirst)
			{
				add(ox0, ix0, oz0, oz1);
				add(ix1, ox1, oz0, oz1);
				add(ix0, ix1, oz0, iz0);
				add(ix0, ix1, iz1, oz1);
			}
			else
			{
				add(ox0, ox1, oz0, iz0);
				add(ox0, ox1, iz1, oz1);
				add(ox0, ix0, iz0, iz1);
				add(ix1, ox1, iz0, iz1);
			}

			return r;
		}

		std::string Words(Rect const &r)
		{
			return "x " + Number(r.lo[0]) + ".." + Number(r.hi[0]) + ", z " + Number(r.lo[1]) + ".." + Number(r.hi[1]);
		}

		// THE RENUMBERING, both steps': the list in the order given (order[k]: the old place of the box that goes to place k), its climbing marks and its roles with
		// it; every link and every place in the list the plan names (each doorway's entry and exit box, the doorway floors) follows its box. Returns how many boxes
		// moved (0, the list as it was, also for an order that does not name every place once)
		size_t Renumber(AutoPlotPlan *plan, std::vector<size_t> const &order)
		{
			std::vector<WaypointRec> &list = plan->list;
			size_t const n = list.size();
			std::vector<int32_t> to(n, -1);   // to[old place] = the new place
			size_t moved = 0;

			if (order.size() != n)
			{
				return 0;
			}

			for (size_t k = 0; k < n; k++)
			{
				if (order[k] >= n || to[order[k]] >= 0)
				{
					return 0;
				}

				to[order[k]] = (int32_t)k;
				moved += order[k] != k ? 1 : 0;
			}

			if (moved == 0)
			{
				return 0;
			}

			std::vector<WaypointRec> list2(n);
			std::vector<bool> climbing2(plan->climbing.size() == n ? n : 0);
			std::vector<std::string> roles2(plan->volumeRoles.size() == n ? n : 0);

			for (size_t old = 0; old < n; old++)
			{
				size_t const k = (size_t)to[old];

				list2[k] = list[old];

				for (WaypointLinkRec &l : list2[k].links)
				{
					l.index = l.index >= 0 && (size_t)l.index < n ? to[(size_t)l.index] : l.index;
				}

				if (!climbing2.empty())
				{
					climbing2[k] = plan->climbing[old];
				}

				if (!roles2.empty())
				{
					roles2[k] = plan->volumeRoles[old];
				}
			}

			auto const follow = [&](int32_t &i) { i = i >= 0 && (size_t)i < n ? to[(size_t)i] : i; };

			for (int32_t &i : plan->doorBox)
			{
				follow(i);
			}

			for (int32_t &i : plan->exitBox)
			{
				follow(i);
			}

			for (std::vector<int32_t> &floorsAt : plan->doorFloors)
			{
				for (int32_t &i : floorsAt)
				{
					follow(i);
				}
			}

			list.swap(list2);

			if (!climbing2.empty())
			{
				plan->climbing.swap(climbing2);
			}

			if (!roles2.empty())
			{
				plan->volumeRoles.swap(roles2);
			}

			return moved;
		}
	}

	size_t AutoPlot_SplitNestedEntries(AutoPlotPlan *plan, std::vector<std::vector<int32_t> > const &exits, double scale)
	{
		if (!plan || plan->volumeRoles.size() != plan->list.size() || plan->climbing.size() != plan->list.size())
		{
			return 0;   // (a list the volume planner did not make: no roles beside it)
		}

		double const s = scale > 0.0 ? scale : 1.0;
		std::vector<WaypointRec> &list = plan->list;
		size_t const before = list.size();
		size_t split = 0;
		std::vector<std::vector<size_t> > piecesOf(before);   // each split entry's pieces, their places while they are made (at the list's end)

		// each entry of the planner's list once (a split's own pieces, at the end, are not looked at again)
		for (size_t o = 0; o < before; o++)
		{
			if (plan->volumeRoles[o] != "entry")
			{
				continue;
			}

			std::vector<size_t> inner;

			for (size_t i = 0; i < before; i++)
			{
				if (i != o && plan->volumeRoles[i] == "entry" && Inside(list[i], list[o]) && !Inside(list[o], list[i]))
				{
					inner.push_back(i);
				}
			}

			if (inner.empty())
			{
				continue;
			}

			// the notes name each entry by its doorway (the boxes are renumbered after this step); a box no doorway names, by the planner's own number
			auto const doorOf = [plan](size_t b)
			{
				size_t d = 0;

				while (d < plan->doorBox.size() && plan->doorBox[d] != (int32_t)b)
				{
					d++;
				}

				return d;
			};
			auto const name = [plan](size_t b, size_t d) { return d < plan->doorBox.size() ? "doorway " + Number((long long)d) + "'s entry" : "entry box " + Number((long long)b) + " (the planner's number)"; };
			size_t const door = doorOf(o);
			std::string const innerName = name(inner[0], doorOf(inner[0]));
			std::string const what = "the nested entries' split: " + name(o, door) + ", with " + innerName + " inside it,";

			if (inner.size() > 1)
			{
				plan->notes.push_back(what + " left whole: " + Number((long long)inner.size()) + " entries lie inside it (it is cut round one only)");
				continue;
			}

			// the two ways of cutting it; one with a piece under kAutoPlotNestLeast either way is no choice
			std::vector<Rect> const ways[2] = { Pieces(list[o], list[inner[0]], true), Pieces(list[o], list[inner[0]], false) };
			int pick = -1;
			std::string narrow;

			for (int w = 0; w < 2; w++)
			{
				std::string tooNarrow;
				double biggest = 0.0;

				for (Rect const &r : ways[w])
				{
					double const across = (double)(r.hi[0] - r.lo[0]) * s, along = (double)(r.hi[1] - r.lo[1]) * s;

					biggest = std::max(biggest, across * along);

					if (tooNarrow.empty() && (across < (double)kAutoPlotNestLeast || along < (double)kAutoPlotNestLeast))
					{
						tooNarrow = Words(r) + " (" + Number((long long)std::min(across, along)) + " mm)";
					}
				}

				if (!tooNarrow.empty() || ways[w].empty())
				{
					narrow = narrow.empty() ? tooNarrow : narrow;
					continue;
				}

				if (pick < 0 || ways[w].size() < ways[pick].size())
				{
					pick = w;
				}
				else if (ways[w].size() == ways[pick].size())
				{
					double best = 0.0;

					for (Rect const &r : ways[pick])
					{
						best = std::max(best, (double)(r.hi[0] - r.lo[0]) * s * (double)(r.hi[1] - r.lo[1]) * s);
					}

					pick = biggest > best ? w : pick;
				}
			}

			if (pick < 0)
			{
				plan->notes.push_back(what + " left whole: " + (narrow.empty() ? std::string("the inner entry covers it looking down") :
					"its piece " + narrow + " would be under " + Number(kAutoPlotNestLeast) + " mm across (the owner's least, 0.9 m)"));
				continue;
			}

			std::vector<Rect> const &pieces = ways[pick];

			// THE DOORWAY: the doorway whose entry it is; its exit point (else its exit box's middle) looking down
			if (door == plan->doorBox.size())
			{
				plan->notes.push_back(what + " left whole: it is no doorway's entry");
				continue;
			}

			if (before + pieces.size() - 1 > (size_t)kAutoPlotMostBoxes || list.size() + pieces.size() - 1 > (size_t)kAutoPlotMostBoxes)
			{
				plan->notes.push_back(what + " left whole: its " + Number((long long)pieces.size()) + " pieces would take the list past " + Number(kAutoPlotMostBoxes) + " boxes");
				continue;
			}

			double point[2] = { ((double)list[o].min[0] + (double)list[o].max[0]) / 2.0, ((double)list[o].min[2] + (double)list[o].max[2]) / 2.0 };
			int32_t const exitAt = door < plan->exitBox.size() ? plan->exitBox[door] : -1;

			if (door < exits.size() && exits[door].size() == 3)
			{
				point[0] = (double)exits[door][0] / s;
				point[1] = (double)exits[door][2] / s;
			}
			else if (exitAt >= 0 && (size_t)exitAt < list.size())
			{
				point[0] = ((double)list[(size_t)exitAt].min[0] + (double)list[(size_t)exitAt].max[0]) / 2.0;
				point[1] = ((double)list[(size_t)exitAt].min[2] + (double)list[(size_t)exitAt].max[2]) / 2.0;
			}

			size_t doorPiece = 0;
			double nearest = -1.0;

			for (size_t k = 0; k < pieces.size(); k++)
			{
				double const dx = std::max(0.0, std::max((double)pieces[k].lo[0] - point[0], point[0] - ((double)pieces[k].hi[0] - 1.0)));
				double const dz = std::max(0.0, std::max((double)pieces[k].lo[1] - point[1], point[1] - ((double)pieces[k].hi[1] - 1.0)));

				if (nearest < 0.0 || dx * dx + dz * dz < nearest)
				{
					nearest = dx * dx + dz * dz;
					doorPiece = k;
				}
			}

			// THE PIECES: the doorway's in the outer's place, with all its links; the others at the end
			WaypointRec const outer = list[o];
			std::vector<size_t> at(pieces.size(), o);

			list[o].min[0] = pieces[doorPiece].lo[0];
			list[o].max[0] = pieces[doorPiece].hi[0];
			list[o].min[2] = pieces[doorPiece].lo[1];
			list[o].max[2] = pieces[doorPiece].hi[1];

			// THE INNER TAKES THE OUTER'S TOP (Bishop's (a) 11-21): over its own area the inner entry now stands alone where the outer held the ceiling too, and an entry is
			// sized for its own ceiling cells only. doormp09: the roof over its door0 entry ramps from y -1558 up to -1948, the entry's top is -1794, so from z ~119400 on
			// an alien hanging there was held by the outer (door1) entry alone; cut round as it was, 101 of 504 places over the old entry would be in no box
			int32_t const innerTop = list[inner[0]].min[1];

			list[inner[0]].min[1] = std::min(innerTop, outer.min[1]);

			for (size_t k = 0; k < pieces.size(); k++)
			{
				if (k == doorPiece)
				{
					continue;
				}

				WaypointRec r = outer;

				r.min[0] = pieces[k].lo[0];
				r.max[0] = pieces[k].hi[0];
				r.min[2] = pieces[k].lo[1];
				r.max[2] = pieces[k].hi[1];
				r.links.clear();
				at[k] = list.size();
				list.push_back(r);
				plan->climbing.push_back(plan->climbing[o]);
				plan->volumeRoles.push_back("entry");
			}

			// THE LINKS of the other pieces: the outer's link to each of its boxes the piece meets looking down, its flags at both ends (what the planner linked the
			// entry there to: a climb up into it one way, the inner entry both ways, its exit down one way); aliens only both ways with the doorway piece and with the
			// pieces it meets
			for (size_t k = 0; k < pieces.size(); k++)
			{
				if (k == doorPiece)
				{
					continue;
				}

				for (WaypointLinkRec const &l : outer.links)
				{
					if (l.index >= 0 && (size_t)l.index < list.size() && (size_t)l.index != o && Meets(pieces[k], list[(size_t)l.index]))
					{
						Link(&list, at[k], (size_t)l.index, l.flags, FlagsBack(list[(size_t)l.index], (int32_t)o, l.flags));
					}
				}

				Link(&list, at[k], o, kLinkAlien, kLinkAlien);

				for (size_t m = 0; m < pieces.size(); m++)
				{
					if (m != k && m != doorPiece && Meets(pieces[k], list[at[m]]))
					{
						Link(&list, at[k], at[m], kLinkAlien, kLinkAlien);
					}
				}
			}

			// THE AIMS: each piece's nearest the outer's own (a climb's top, its doorway's middle: the planner's choice) where a body, and kAutoPlotNestAimClear round
			// it, is in no other box; else where the body alone is
			std::string aims;

			for (size_t k = 0; k < pieces.size(); k++)
			{
				if (!AimAt(&list, at[k], outer.centre, (double)kAutoPlotNestAimClear, s) && !AimAt(&list, at[k], outer.centre, 0.0, s))
				{
					int32_t const inside[3] = { std::min(std::max(outer.centre[0], list[at[k]].min[0]), list[at[k]].max[0] - 1), outer.centre[1],
						std::min(std::max(outer.centre[2], list[at[k]].min[2]), list[at[k]].max[2] - 1) };

					for (int a = 0; a < 3; a++)
					{
						list[at[k]].centre[a] = inside[a];
					}

					aims += "; the piece " + Words(pieces[k]) + " has no place where a body is in no other box: its aim is the nearest place inside it";
				}
			}

			std::string boxes;

			for (size_t k = 0; k < pieces.size(); k++)
			{
				boxes += (k == 0 ? "" : ", ") + Words(pieces[k]) + (k == doorPiece ? " (over the doorway: the doorway's entry still)" : "");
			}

			std::string const top = list[inner[0]].min[1] != innerTop ? "; " + innerName + "'s top raised to the outer's, y " + Number(innerTop) + " -> " +
				Number(list[inner[0]].min[1]) : std::string();

			plan->notes.push_back(what + " cut round it into " + Number((long long)pieces.size()) + " pieces: " + boxes + top + aims);
			split++;

			for (size_t k = 0; k < pieces.size(); k++)
			{
				piecesOf[o].push_back(at[k]);
			}
		}

		if (split > 0)
		{
			// THE PIECES IN THE OUTER'S PLACE: each split entry's other pieces right after it, so every box that came after the outer entry still comes after them.
			// The game takes the LAST box holding a body as its own: at the end of the list a piece would win over every box it touches (doormp09: its west piece over
			// the closer under it, the closer's own aim then the piece's)
			std::vector<size_t> order;

			order.reserve(list.size());

			for (size_t i = 0; i < before; i++)
			{
				order.push_back(i);

				for (size_t const p : piecesOf[i])
				{
					if (p != i)
					{
						order.push_back(p);
					}
				}
			}

			Renumber(plan, order);

			// the aims another box holds, again (the planner's count was made before the split)
			plan->centresShared = 0;

			for (size_t i = 0; i < list.size(); i++)
			{
				double const w[3] = { trunc((double)list[i].centre[0] * s), trunc((double)list[i].centre[1] * s), trunc((double)list[i].centre[2] * s) };
				bool shared = !Holds(list[i], w, s);

				for (size_t j = 0; j < list.size() && !shared; j++)
				{
					shared = j != i && Holds(list[j], w, s);
				}

				plan->centresShared += shared ? 1 : 0;
			}
		}

		return split;
	}

	size_t AutoPlot_OrderNested(AutoPlotPlan *plan)
	{
		if (!plan)
		{
			return 0;
		}

		std::vector<WaypointRec> &list = plan->list;
		size_t const n = list.size();

		// for each box, the boxes that must come after it (the ones lying inside it; two boxes each inside the other are nearly one box, and their order stays), and how
		// many boxes must still come before each
		std::vector<std::vector<size_t> > after(n);
		std::vector<size_t> waiting(n, 0);

		for (size_t a = 0; a < n; a++)
		{
			for (size_t b = 0; b < n; b++)
			{
				if (a != b && Inside(list[a], list[b]) && !Inside(list[b], list[a]))
				{
					after[b].push_back(a);   // a inside b: b first
					waiting[a]++;
				}
			}
		}

		// the order: each time the lowest-numbered box no other box must still come before, so the rest keep their order
		std::vector<size_t> order;
		std::vector<bool> placed(n, false);

		order.reserve(n);

		while (order.size() < n)
		{
			size_t next = n;

			for (size_t i = 0; i < n && next == n; i++)
			{
				next = !placed[i] && waiting[i] == 0 ? i : n;
			}

			if (next == n)
			{
				plan->notes.push_back("the nested boxes' order: a ring of boxes each inside the next, the list left as it was");
				return 0;   // (a ring: no order meets it)
			}

			placed[next] = true;
			order.push_back(next);

			for (size_t const a : after[next])
			{
				waiting[a]--;
			}
		}

		// the list in the new order; every link and every place in the list the plan names follows its box
		size_t const moved = Renumber(plan, order);

		if (moved == 0)
		{
			return 0;
		}

		plan->notes.push_back("the nested boxes' order: " + std::to_string((unsigned long long)moved) + " box(es) renumbered so that each box lying inside another comes after it (the "
			"game takes the last box holding a body as its own)");
		return moved;
	}
}
