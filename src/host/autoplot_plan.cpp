// See autoplot_plan.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_plan.h"

#include <math.h>

#include <algorithm>
#include <map>
#include <utility>

namespace host
{
	namespace
	{
		struct Box
		{
			double lo[3], hi[3];
			AutoPlotSurface surface;
			std::vector<std::vector<double> > hints;   // places off the surface along the way (centre candidates)
		};

		struct Stretch
		{
			std::vector<std::vector<double> > points;
			AutoPlotSurface surface;
			int32_t facing[3];
		};

		double Held(double v)
		{
			return v > kAutoPlotMostPlace ? (double)kAutoPlotMostPlace : (v < -kAutoPlotMostPlace ? -(double)kAutoPlotMostPlace : v);
		}

		bool Holds(Box const &b, double const p[3], double margin)
		{
			for (int a = 0; a < 3; a++)
			{
				if (p[a] < b.lo[a] + margin || p[a] > b.hi[a] - margin)
				{
					return false;
				}
			}

			return true;
		}

		double Volume(Box const &b)
		{
			return (b.hi[0] - b.lo[0]) * (b.hi[1] - b.lo[1]) * (b.hi[2] - b.lo[2]);
		}

		double Overlap(Box const &a, Box const &b)
		{
			double v = 1.0;

			for (int i = 0; i < 3; i++)
			{
				double const d = std::min(a.hi[i], b.hi[i]) - std::max(a.lo[i], b.lo[i]);

				if (d <= 0.0)
				{
					return 0.0;
				}

				v *= d;
			}

			return v;
		}

		bool Touch(Box const &a, Box const &b)
		{
			for (int i = 0; i < 3; i++)
			{
				if (std::min(a.hi[i], b.hi[i]) < std::max(a.lo[i], b.lo[i]))
				{
					return false;
				}
			}

			return true;
		}

		Box Union(Box const &a, Box const &b)
		{
			Box u;

			for (int i = 0; i < 3; i++)
			{
				u.lo[i] = std::min(a.lo[i], b.lo[i]);
				u.hi[i] = std::max(a.hi[i], b.hi[i]);
			}

			u.surface = a.surface == b.surface ? a.surface : AutoPlotSurface::Wall;   // a box of two surfaces is no floor box
			u.hints = a.hints;
			u.hints.insert(u.hints.end(), b.hints.begin(), b.hints.end());
			return u;
		}

		double Gap(Box const &a, Box const &b)
		{
			double sum = 0.0;

			for (int i = 0; i < 3; i++)
			{
				double const d = std::max(a.lo[i], b.lo[i]) - std::min(a.hi[i], b.hi[i]);

				sum += d > 0.0 ? d * d : 0.0;
			}

			return sqrt(sum);
		}

		// the way off the surface: an axis and its sign (+1 or -1); axis -1 when not known (a wall with no facing)
		void OffSurface(AutoPlotSurface surface, int32_t const facing[3], int *axis, int *sign)
		{
			int best = -1;
			int64_t most = 0;

			for (int i = 0; i < 3; i++)
			{
				int64_t const m = facing[i] < 0 ? -(int64_t)facing[i] : facing[i];

				if (m > most)
				{
					most = m;
					best = i;
				}
			}

			if (best >= 0)
			{
				*axis = best;
				*sign = facing[best] < 0 ? -1 : 1;
				return;
			}

			*axis = surface == AutoPlotSurface::Wall ? -1 : 1;
			*sign = surface == AutoPlotSurface::Floor ? -1 : 1;   // y is down: off a floor is -y, off a ceiling +y
		}

		Box BoxOf(Stretch const &s)
		{
			Box b;

			for (int i = 0; i < 3; i++)
			{
				b.lo[i] = b.hi[i] = s.points[0][(size_t)i];
			}

			for (std::vector<double> const &p : s.points)
			{
				for (int i = 0; i < 3; i++)
				{
					b.lo[i] = std::min(b.lo[i], p[(size_t)i]);
					b.hi[i] = std::max(b.hi[i], p[(size_t)i]);
				}
			}

			int axis = -1, sign = 1;

			OffSurface(s.surface, s.facing, &axis, &sign);

			for (int i = 0; i < 3; i++)
			{
				if (i == axis)
				{
					b.lo[i] -= sign > 0 ? kAutoPlotInto : kAutoPlotReach;
					b.hi[i] += sign > 0 ? kAutoPlotReach : kAutoPlotInto;
				}
				else
				{
					b.lo[i] -= kAutoPlotHalfWidth;
					b.hi[i] += kAutoPlotHalfWidth;
				}

				b.lo[i] = Held(b.lo[i]);
				b.hi[i] = Held(b.hi[i]);
			}

			b.surface = s.surface;

			// over each point, half the reach off the surface first (the alien's middle), then a quarter and three quarters (where the others leave room)
			for (double const part : { 0.5, 0.25, 0.75 })
			{
				for (std::vector<double> const &p : s.points)
				{
					std::vector<double> hint = p;

					if (axis >= 0)
					{
						hint[(size_t)axis] += sign * kAutoPlotReach * part;
					}

					b.hints.push_back(hint);
				}
			}

			return b;
		}

		struct Links
		{
			std::map<std::pair<size_t, size_t>, int32_t> flags;   // (low, high) -> flags; a plain one wins over an aliens-only one (the floor route goes)

			void Add(size_t a, size_t b, int32_t f)
			{
				if (a == b)
				{
					return;
				}

				std::pair<size_t, size_t> const key(std::min(a, b), std::max(a, b));
				auto const at = flags.find(key);

				flags[key] = at == flags.end() ? f : (at->second & f);
			}

			// boxes renumbered: from[old] is the new one
			void Remap(std::vector<size_t> const &from)
			{
				Links again;

				for (auto const &one : flags)
				{
					again.Add(from[one.first.first], from[one.first.second], one.second);
				}

				*this = again;
			}
		};

		int32_t LinkFlags(AutoPlotSurface a, AutoPlotSurface b)
		{
			return a != AutoPlotSurface::Floor || b != AutoPlotSurface::Floor ? kLinkAlien : 0;
		}

		// the networks the links make: each box's part number; the count
		size_t Parts(size_t count, Links const &links, std::vector<size_t> *part)
		{
			std::vector<size_t> root(count);

			for (size_t i = 0; i < count; i++)
			{
				root[i] = i;
			}

			auto const find = [&root](size_t i) { while (root[i] != i) { root[i] = root[root[i]]; i = root[i]; } return i; };

			for (auto const &one : links.flags)
			{
				root[find(one.first.first)] = find(one.first.second);
			}

			std::map<size_t, size_t> numbers;

			part->assign(count, 0);

			for (size_t i = 0; i < count; i++)
			{
				size_t const r = find(i);

				if (!numbers.count(r))
				{
					numbers[r] = numbers.size();
				}

				(*part)[i] = numbers[r];
			}

			return numbers.size();
		}

		// boxes a and b made one (b into a; the last box takes b's place)
		void Merge(std::vector<Box> *boxes, Links *links, size_t a, size_t b)
		{
			(*boxes)[a] = Union((*boxes)[a], (*boxes)[b]);

			std::vector<size_t> from(boxes->size());

			for (size_t i = 0; i < boxes->size(); i++)
			{
				from[i] = i == b ? a : i;
			}

			size_t const last = boxes->size() - 1;

			if (b != last)
			{
				(*boxes)[b] = (*boxes)[last];

				for (size_t i = 0; i < from.size(); i++)
				{
					from[i] = from[i] == last ? b : from[i];
				}
			}

			boxes->pop_back();
			links->Remap(from);
		}

		// parts that don't reach each other linked where their boxes touch
		void JoinParts(std::vector<Box> const &boxes, Links *links)
		{
			std::vector<size_t> part;

			if (Parts(boxes.size(), *links, &part) <= 1)
			{
				return;
			}

			for (size_t a = 0; a < boxes.size(); a++)
			{
				for (size_t b = a + 1; b < boxes.size(); b++)
				{
					if (part[a] != part[b] && Touch(boxes[a], boxes[b]))
					{
						links->Add(a, b, LinkFlags(boxes[a].surface, boxes[b].surface));
					}
				}
			}
		}

		// box d taken out, with its links (the last box takes its place)
		void TakeOut(std::vector<Box> *boxes, Links *links, size_t d)
		{
			size_t const last = boxes->size() - 1;
			Links again;

			for (auto const &one : links->flags)
			{
				if (one.first.first != d && one.first.second != d)
				{
					again.Add(one.first.first == last ? d : one.first.first, one.first.second == last ? d : one.first.second, one.second);
				}
			}

			(*boxes)[d] = (*boxes)[last];
			boxes->pop_back();
			*links = again;
		}

		std::string Place(std::vector<int32_t> const &p)
		{
			return std::to_string((long long)p[0]) + " " + std::to_string((long long)p[1]) + " " + std::to_string((long long)p[2]);
		}

		int32_t File(double world, double scale)
		{
			double const v = floor(world / scale);

			return v > 2147483000.0 ? 2147483000 : (v < -2147483000.0 ? -2147483000 : (int32_t)v);
		}

		// a box in the file's numbers, as step 8 writes it: the stock loader takes 1 off each max (inside is min <= p <= max - 1: Frost's measures, waypoint_net_check.h), so
		// the max is one past the box, held to the cap too
		struct FileBox
		{
			int32_t min[3], max[3];
		};

		FileBox FileOf(Box const &b, double scale)
		{
			FileBox f;

			for (int a = 0; a < 3; a++)
			{
				f.min[a] = File(b.lo[a], scale);
				f.max[a] = std::max(std::min(File(b.hi[a], scale) + 1, kAutoPlotMostPlace), f.min[a] + 2);
			}

			return f;
		}

		// The game's own test of a place in a box (WaypointContainsPoint, as Frost's checker does it): each number times the level's scale with what's after the point cut
		// off, inside = lowest <= place <= highest - 1. widen grows the box that many of the file's units each way (the "clear of it" test). In doubles: no sum is put in 32 bits
		bool GameHolds(FileBox const &f, int32_t const at[3], double scale, int32_t widen)
		{
			for (int a = 0; a < 3; a++)
			{
				double const p = trunc((double)at[a] * scale);

				if (p < trunc(((double)f.min[a] - widen) * scale) || p > trunc(((double)f.max[a] + widen) * scale) - 1.0)
				{
					return false;
				}
			}

			return true;
		}
	}

	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, double levelScale)
	{
		AutoPlotPlan plan;
		double const scale = levelScale > 0.0 ? levelScale : 1.0;
		std::vector<Box> boxes;
		Links links;

		// 1. each way cut to the AI module (plus one point past each end), into stretches of one surface, each a box; boxes that follow each other on a way linked
		for (AutoPlotWay const &way : ways)
		{
			size_t const steps = std::min(way.steps.size(), kAutoPlotMostSteps);
			size_t i = 0;

			while (i < steps)
			{
				if (!way.steps[i].inside)
				{
					i++;
					continue;
				}

				size_t end = i;

				while (end + 1 < steps && way.steps[end + 1].inside)
				{
					end++;
				}

				size_t const first = i > 0 ? i - 1 : i;
				size_t const last = end + 1 < steps ? end + 1 : end;

				i = end + 1;

				std::vector<Stretch> stretches;
				Stretch now;
				double length = 0.0;

				auto const point = [&way](size_t k) { return std::vector<double>{ Held(way.steps[k].place[0]), Held(way.steps[k].place[1]), Held(way.steps[k].place[2]) }; };
				auto const begin = [&now, &way, &point](size_t k)
				{
					now.points.assign(1, point(k));
					now.surface = way.steps[k].surface;
					now.facing[0] = way.steps[k].facing[0];
					now.facing[1] = way.steps[k].facing[1];
					now.facing[2] = way.steps[k].facing[2];
				};

				begin(first);

				for (size_t k = first + 1; k <= last; k++)
				{
					std::vector<double> const p = point(k);
					std::vector<double> const q = now.points.back();   // a copy: the push below may move the points

					now.points.push_back(p);
					length += sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));

					bool const turns = k < last && way.steps[k].surface != now.surface;

					if ((turns || length >= kAutoPlotStretch) && k < last)
					{
						stretches.push_back(now);
						begin(k);   // the next stretch starts where this one ends: their boxes meet
						length = 0.0;
					}
				}

				stretches.push_back(now);

				for (size_t s = 0; s < stretches.size(); s++)
				{
					boxes.push_back(BoxOf(stretches[s]));

					if (s > 0)
					{
						links.Add(boxes.size() - 2, boxes.size() - 1, LinkFlags(stretches[s - 1].surface, stretches[s].surface));
					}
				}
			}
		}

		// 2. boxes of one surface that mostly overlap (80% of the smaller) made one
		for (bool merged = true; merged;)
		{
			merged = false;

			for (size_t a = 0; a < boxes.size() && !merged; a++)
			{
				for (size_t b = a + 1; b < boxes.size() && !merged; b++)
				{
					if (boxes[a].surface == boxes[b].surface && Overlap(boxes[a], boxes[b]) >= 0.8 * std::min(Volume(boxes[a]), Volume(boxes[b])))
					{
						Merge(&boxes, &links, a, b);
						merged = true;
					}
				}
			}
		}

		// 3. an exit no box holds: a box of its own (as a floor's), linked to the boxes it touches, else its nearest
		plan.exits = exits.size();

		for (size_t e = 0; e < exits.size(); e++)
		{
			if (exits[e].size() != 3)
			{
				continue;
			}

			double const p[3] = { Held(exits[e][0]), Held(exits[e][1]), Held(exits[e][2]) };
			bool held = false;

			for (Box const &b : boxes)
			{
				held = held || Holds(b, p, 0.0);
			}

			if (held)
			{
				continue;
			}

			Box own;

			for (int i = 0; i < 3; i++)
			{
				own.lo[i] = Held(p[i] - (i == 1 ? kAutoPlotReach : kAutoPlotHalfWidth));
				own.hi[i] = Held(p[i] + (i == 1 ? kAutoPlotInto : kAutoPlotHalfWidth));
			}

			own.surface = AutoPlotSurface::Floor;
			own.hints.push_back(std::vector<double>{ p[0], p[1] - kAutoPlotReach / 2.0, p[2] });
			boxes.push_back(own);

			size_t const at = boxes.size() - 1;
			bool touched = false;
			size_t nearest = at;
			double nearestGap = 1e300;

			for (size_t b = 0; b < at; b++)
			{
				if (Touch(boxes[at], boxes[b]))
				{
					links.Add(at, b, LinkFlags(boxes[at].surface, boxes[b].surface));
					touched = true;
				}

				double const g = Gap(boxes[at], boxes[b]);

				if (g < nearestGap)
				{
					nearestGap = g;
					nearest = b;
				}
			}

			if (!touched && nearest != at)
			{
				links.Add(at, nearest, LinkFlags(boxes[at].surface, boxes[nearest].surface));
				plan.notes.push_back("exit " + std::to_string((long long)e) + " (" + Place(exits[e]) + ") was on no way: a box of its own, linked to its nearest, " +
					std::to_string((long long)nearestGap) + " mm away");
			}
		}

		// 4. parts that don't reach each other linked where their boxes touch
		std::vector<size_t> part;

		JoinParts(boxes, &links);

		// 5. down to the stock game's most: the merge that grows the boxes least, those that link or touch first
		while ((int32_t)boxes.size() > kAutoPlotMostBoxes)
		{
			size_t bestA = 0, bestB = 1;
			double bestGrowth = 1e300;
			bool bestNear = false;

			for (size_t a = 0; a < boxes.size(); a++)
			{
				for (size_t b = a + 1; b < boxes.size(); b++)
				{
					bool const near = links.flags.count(std::make_pair(a, b)) || Touch(boxes[a], boxes[b]);
					double const growth = Volume(Union(boxes[a], boxes[b])) - Volume(boxes[a]) - Volume(boxes[b]);

					if ((near && !bestNear) || (near == bestNear && growth < bestGrowth))
					{
						bestA = a;
						bestB = b;
						bestGrowth = growth;
						bestNear = near;
					}
				}
			}

			Merge(&boxes, &links, bestA, bestB);
		}

		// 6. each centre, in the file's numbers and by the game's own test: inside its own box only, clear of the others by half an alien's width; else inside its own and
		// outside the others. A box with no such place (the others cover it: a 3 m wall between a floor's box and a ceiling's, each reaching 1.5 m) is taken out, or if that
		// would lose an exit or split the network, made one with the box that holds its best place; then every centre is picked again, until each is its own box's only:
		// Frost's checker refuses a network with a centre in another box ("box 1: its centre is in box 3 too", Genshd1's genent01). Each round takes one box away, so it ends
		int32_t const clear = (int32_t)std::min(ceil(kAutoPlotHalfWidth / 2.0 / scale), 1000000.0);
		std::vector<FileBox> files;
		std::vector<std::vector<int32_t> > centres;
		size_t joined = 0, takenOut = 0;

		for (bool again = true; again;)
		{
			again = false;
			files.clear();

			for (Box const &b : boxes)
			{
				files.push_back(FileOf(b, scale));
			}

			centres.assign(boxes.size(), std::vector<int32_t>(3, 0));
			plan.centresShared = 0;

			// the wall and ceiling boxes first: an alien route on a floor route's line is the one that goes, and the floor route keeps its plain links (a marine may go)
			std::vector<size_t> order;

			for (int floors = 0; floors < 2; floors++)
			{
				for (size_t i = 0; i < boxes.size(); i++)
				{
					if ((boxes[i].surface == AutoPlotSurface::Floor) == (floors == 1))
					{
						order.push_back(i);
					}
				}
			}

			for (size_t const i : order)
			{
				Box const &b = boxes[i];
				std::vector<std::vector<double> > candidates;

				candidates.push_back(std::vector<double>{ (b.lo[0] + b.hi[0]) / 2.0, (b.lo[1] + b.hi[1]) / 2.0, (b.lo[2] + b.hi[2]) / 2.0 });
				candidates.insert(candidates.end(), b.hints.begin(), b.hints.end());

				for (double fx : { 0.25, 0.5, 0.75 })
				{
					for (double fy : { 0.25, 0.5, 0.75 })
					{
						for (double fz : { 0.25, 0.5, 0.75 })
						{
							candidates.push_back(std::vector<double>{ b.lo[0] + fx * (b.hi[0] - b.lo[0]), b.lo[1] + fy * (b.hi[1] - b.lo[1]), b.lo[2] + fz * (b.hi[2] - b.lo[2]) });
						}
					}
				}

				// a place in the file's numbers, held inside the box's own
				auto const placeOf = [&files, i, scale](std::vector<double> const &c)
				{
					std::vector<int32_t> at(3);

					for (int a = 0; a < 3; a++)
					{
						at[(size_t)a] = std::min(std::max(File(c[(size_t)a], scale), files[i].min[a]), files[i].max[a] - 1);
					}

					return at;
				};

				int bestTier = 3;
				size_t bestOthers = (size_t)-1;
				std::vector<int32_t> best = placeOf(candidates[0]);

				for (std::vector<double> const &c : candidates)
				{
					std::vector<int32_t> const at = placeOf(c);

					if (!GameHolds(files[i], at.data(), scale, 0))
					{
						continue;
					}

					size_t inOthers = 0, nearOthers = 0;

					for (size_t j = 0; j < boxes.size(); j++)
					{
						if (j != i)
						{
							inOthers += GameHolds(files[j], at.data(), scale, 0) ? 1 : 0;
							nearOthers += GameHolds(files[j], at.data(), scale, clear) ? 1 : 0;
						}
					}

					int const tier = nearOthers == 0 ? 0 : (inOthers == 0 ? 1 : 2);

					if (tier < bestTier || (tier == bestTier && tier == 2 && inOthers < bestOthers))
					{
						bestTier = tier;
						bestOthers = inOthers;
						best = at;
					}
				}

				if (bestTier != 2)
				{
					centres[i] = best;
					continue;
				}

				// covered: taken out when every exit it holds is held by another too, and the network stays as whole as it was (parts that come apart linked where their boxes
				// touch); b is not used after this
				bool exitsElsewhere = true;

				for (std::vector<int32_t> const &e : exits)
				{
					if (e.size() != 3)
					{
						continue;
					}

					double const p[3] = { Held(e[0]), Held(e[1]), Held(e[2]) };
					bool other = false;

					for (size_t j = 0; j < boxes.size(); j++)
					{
						other = other || (j != i && Holds(boxes[j], p, 0.0));
					}

					exitsElsewhere = exitsElsewhere && (other || !Holds(boxes[i], p, 0.0));
				}

				if (exitsElsewhere)
				{
					std::vector<size_t> partsNow;
					size_t const before = Parts(boxes.size(), links, &partsNow);
					std::vector<Box> const keptBoxes = boxes;
					Links const keptLinks = links;

					TakeOut(&boxes, &links, i);
					JoinParts(boxes, &links);

					if (Parts(boxes.size(), links, &partsNow) <= before)
					{
						takenOut++;
						again = true;
						break;
					}

					boxes = keptBoxes;
					links = keptLinks;
				}

				// else made one with the box that holds its best place, the one it overlaps most
				size_t holder = i;
				double most = -1.0;

				for (size_t j = 0; j < boxes.size(); j++)
				{
					if (j != i && GameHolds(files[j], best.data(), scale, 0) && Overlap(boxes[i], boxes[j]) > most)
					{
						most = Overlap(boxes[i], boxes[j]);
						holder = j;
					}
				}

				if (holder == i)
				{
					plan.centresShared++;   // no box holds it after all (it can't be: tier 2 is "in another"); kept, and the checker says so
					centres[i] = best;
					continue;
				}

				Merge(&boxes, &links, std::min(i, holder), std::max(i, holder));
				joined++;
				again = true;
				break;
			}
		}

		if (takenOut)
		{
			plan.notes.push_back(takenOut == 1 ? std::string("1 box the others covered taken out (each centre must be in its own box only)") :
				std::to_string((long long)takenOut) + " boxes the others covered taken out (each centre must be in its own box only)");
		}

		if (joined)
		{
			plan.notes.push_back(joined == 1 ? std::string("1 box made one with a box that covered it (each centre must be in its own box only)") :
				std::to_string((long long)joined) + " boxes made one with boxes that covered them (each centre must be in its own box only)");
		}

		if (plan.centresShared)
		{
			plan.notes.push_back(std::to_string((long long)plan.centresShared) + " box centres could not be kept out of every other box");
		}

		// the networks the boxes make, after the joins (a join only joins parts)
		plan.parts = boxes.empty() ? 0 : Parts(boxes.size(), links, &part);

		if (plan.parts > 1)
		{
			plan.notes.push_back(std::to_string((long long)plan.parts) + " parts don't reach each other: no way and no box joins them");
		}

		// 7. the exits held at the end
		for (std::vector<int32_t> const &e : exits)
		{
			if (e.size() != 3)
			{
				continue;
			}

			double const p[3] = { Held(e[0]), Held(e[1]), Held(e[2]) };
			bool held = false;

			for (Box const &b : boxes)
			{
				held = held || Holds(b, p, 0.0);
			}

			plan.exitsCovered += held ? 1 : 0;
		}

		// 8. in the file's units; the links both ways (0 or 0x4: no one-way bit, so each end the same)
		for (size_t i = 0; i < boxes.size(); i++)
		{
			WaypointRec wp;

			for (int a = 0; a < 3; a++)
			{
				wp.min[a] = files[i].min[a];   // FileOf: the max one past the box
				wp.max[a] = files[i].max[a];
				wp.centre[a] = centres[i][(size_t)a];
			}

			wp.flags = 0;
			wp.spare2 = 0;
			plan.list.push_back(wp);
		}

		for (auto const &one : links.flags)
		{
			WaypointLinkRec there, back;

			there.index = (int32_t)one.first.second;
			there.flags = one.second;
			back.index = (int32_t)one.first.first;
			back.flags = one.second;
			plan.list[one.first.first].links.push_back(there);
			plan.list[one.first.second].links.push_back(back);
		}

		return plan;
	}

	std::string AutoPlot_Words(AutoPlotPlan const &plan)
	{
		size_t links = 0, alienOnly = 0;

		for (WaypointRec const &wp : plan.list)
		{
			for (WaypointLinkRec const &l : wp.links)
			{
				links++;
				alienOnly += (l.flags & kLinkAlien) ? 1 : 0;
			}
		}

		std::string words = std::to_string((long long)plan.list.size()) + " boxes, " + std::to_string((long long)(links / 2)) + " links (" + std::to_string((long long)(alienOnly / 2)) +
			" aliens only); exits covered " + std::to_string((long long)plan.exitsCovered) + " of " + std::to_string((long long)plan.exits);

		words += plan.parts > 1 ? "; " + std::to_string((long long)plan.parts) + " parts apart" : std::string();
		words += plan.centresShared ? "; " + std::to_string((long long)plan.centresShared) + " centres shared" : std::string();
		return words;
	}
}
