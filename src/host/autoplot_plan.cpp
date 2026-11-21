// See autoplot_plan.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_plan.h"
#include "autoplot_merge.h"

#include <math.h>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace host
{
	namespace
	{
		struct Box
		{
			double lo[3], hi[3];
			AutoPlotSurface surface;
			std::vector<std::vector<double> > hints;   // places along the way (centre candidates)
			bool inside = false;   // a wall's or a ceiling's: its centre only inside the surface (the hints)
			int door = -1;         // THE LANES: the exit whose doorway's top this box is (never made one with another, never taken out)
			int exitOwn = -1;      // the exit whose point this box was made to hold
			bool aimFirst = false; // its hints before its middle (a lane's exit box: its aim past the doorway)
			bool hasCeiling = false;
			double ceilingLow = 0.0;   // a ceiling's box: its surface's lowest place (y is down: the most); the floor boxes under it are kept kAutoPlotHangClear under this
			int cornerFor = -1;        // THE DOORWAY START: the exit whose climb station's corner this ceiling's box has its first centre in
			double cornerAt[3] = { 0.0, 0.0, 0.0 };   // that corner
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
			u.inside = a.inside && b.inside;
			u.exitOwn = a.exitOwn >= 0 ? a.exitOwn : b.exitOwn;
			u.aimFirst = a.aimFirst || b.aimFirst;
			u.hasCeiling = a.hasCeiling || b.hasCeiling;
			u.ceilingLow = a.hasCeiling && b.hasCeiling ? std::max(a.ceilingLow, b.ceilingLow) : (a.hasCeiling ? a.ceilingLow : b.ceilingLow);
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

			// off a floor the body's reach; off a wall or a ceiling only the corner that touches it (THE LANES: a thin climbing box holds no exit point, no floor box, no marine)
			double const reach = s.surface == AutoPlotSurface::Floor ? kAutoPlotReach : kAutoPlotClimbReach;

			if (s.surface == AutoPlotSurface::Ceiling)
			{
				b.hasCeiling = true;
				b.ceilingLow = b.hi[1];   // the points' lowest, before the box is grown
			}

			for (int i = 0; i < 3; i++)
			{
				if (i == axis)
				{
					b.lo[i] -= sign > 0 ? kAutoPlotInto : reach;
					b.hi[i] += sign > 0 ? reach : kAutoPlotInto;
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

			// a wall's or a ceiling's centre INSIDE the surface, half way into the part the box reaches into (Rebellion's rule, the inteng manual's glossary: "if you wish an alien to
			// crawl up a wall, the center of the waypoint should be placed inside the wall"; Bishop 11-18); a floor's in the air over each point, half the reach up first (the alien's
			// middle), then a quarter and three quarters (where the others leave room)
			if (s.surface != AutoPlotSurface::Floor && axis >= 0)
			{
				b.inside = true;

				for (std::vector<double> const &p : s.points)
				{
					std::vector<double> hint = p;

					hint[(size_t)axis] -= sign * kAutoPlotInto / 2.0;
					b.hints.push_back(hint);
				}
			}
			else
			{
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

			void Remove(size_t a, size_t b)
			{
				flags.erase(std::pair<size_t, size_t>(std::min(a, b), std::max(a, b)));
			}

			bool Has(size_t a, size_t b) const
			{
				return flags.count(std::pair<size_t, size_t>(std::min(a, b), std::max(a, b))) != 0;
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
		size_t Parts(size_t count, Links const &links, std::vector<size_t> *part, bool plainOnly = false)
		{
			std::vector<size_t> root(count);

			for (size_t i = 0; i < count; i++)
			{
				root[i] = i;
			}

			auto const find = [&root](size_t i) { while (root[i] != i) { root[i] = root[root[i]]; i = root[i]; } return i; };

			for (auto const &one : links.flags)
			{
				if (!plainOnly || one.second == 0)
				{
					root[find(one.first.first)] = find(one.first.second);
				}
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

			// AN EXIT BOX IS AN END, NEVER A WAY THROUGH (Drake's gate 11-20, BLOCK-10: its roof and its ground joined only by exit 3's box, so an alien going to exit 0 headed
			// for that box's centre past the doorway, into EXT-05): the parts are joined where two boxes that hold no exit touch first, and by an exit's box only where
			// nothing else touches
			for (int round = 0; round < 2; round++)
			{
				if (round == 1 && Parts(boxes.size(), *links, &part) <= 1)
				{
					return;
				}

				for (size_t a = 0; a < boxes.size(); a++)
				{
					for (size_t b = a + 1; b < boxes.size(); b++)
					{
						bool const exitBox = boxes[a].exitOwn >= 0 || boxes[b].exitOwn >= 0;

						if (part[a] != part[b] && (round == 1 || !exitBox) && Touch(boxes[a], boxes[b]))
						{
							links->Add(a, b, LinkFlags(boxes[a].surface, boxes[b].surface));
						}
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

		// a wall's or a ceiling's box (the highways); the floor's is never made one with them (the stock price)
		bool Climbing(Box const &b)
		{
			return b.surface != AutoPlotSurface::Floor;
		}

		// a wall's box thin as one (its centre in a wall face): a floor alien heading for it walks into the wall and climbs; a big one (several made one) has its centre in the
		// open (Drake, Object11 11-20: a doorway strip's "wall within reach", 5.9 by 12.6 m, its centre 7.5 m up: 419 stall places)
		bool WallLike(Box const &b)
		{
			return b.surface == AutoPlotSurface::Wall && std::min(b.hi[0] - b.lo[0], b.hi[2] - b.lo[2]) <= kAutoPlotClimbReach + kAutoPlotInto + 200.0;
		}

		// Each climbing box's bottom cut off at the top of the lowest floor box it reaches into (y is down: the tiles, the floor stretches', the exits' own and `more`), so it stands
		// on the floor and never covers a floor box's centre; one left too thin to climb under taken out (the floor is the way there)
		void StandOnTheFloor(std::vector<Box> *boxes, Links *links, std::vector<Box> const &more)
		{
			std::vector<Box> floors = more;

			for (Box const &b : *boxes)
			{
				if (!Climbing(b))
				{
					floors.push_back(b);
				}
			}

			for (Box &c : *boxes)
			{
				double cut = -1e300;

				for (Box const &under : floors)
				{
					if (Climbing(c) && c.door < 0 && Overlap(c, under) > 0.0 && under.lo[1] > c.lo[1])   // a door box keeps its place: the floor under it is cut (KeepTheLanes)
					{
						cut = std::max(cut, under.lo[1]);   // the lowest floor's top
					}
				}

				if (cut > -1e300)
				{
					c.hi[1] = std::min(c.hi[1], cut);
				}
			}

			for (size_t c = boxes->size(); c-- > 0;)
			{
				if (Climbing((*boxes)[c]) && (*boxes)[c].door < 0 && (*boxes)[c].hi[1] - (*boxes)[c].lo[1] < kAutoPlotHalfWidth / 2.0)
				{
					TakeOut(boxes, links, c);
				}
			}
		}

		// THE FLOOR TILES: the cells cut by squares `edge` across (x and z), each square's cells cut again where their heights are more than an alien's reach apart (a floor over a
		// floor); each part a floor box round its cells (half a cell past them and a seam more, so the next square's box touches), from kAutoPlotInto below its lowest to kAutoPlotReach
		// over its highest. The squares grow (x 1.25) until there are no more than `most`
		std::vector<Box> Tiles(std::vector<std::vector<double> > const &cells, double cellHalf, size_t most, double *edgeUsed, double mostSide,
			std::vector<std::vector<double> > const &strips)
		{
			double const seam = 10.0;
			double edge = std::max((double)kAutoPlotFloorTile, 2.0 * cellHalf);
			std::vector<Box> made;

			for (int round = 0; round < 60; round++)
			{
				std::map<std::pair<int64_t, int64_t>, std::vector<size_t> > squares;

				for (size_t i = 0; i < cells.size(); i++)
				{
					squares[std::make_pair((int64_t)floor(cells[i][0] / edge), (int64_t)floor(cells[i][2] / edge))].push_back(i);
				}

				made.clear();

				std::vector<std::pair<int64_t, int64_t> > keys;   // each tile's square

				for (auto &square : squares)
				{
					std::vector<size_t> &in = square.second;

					std::sort(in.begin(), in.end(), [&cells](size_t a, size_t b) { return cells[a][1] < cells[b][1] || (cells[a][1] == cells[b][1] && a < b); });

					for (size_t from = 0; from < in.size();)
					{
						size_t to = from + 1;

						while (to < in.size() && cells[in[to]][1] - cells[in[to - 1]][1] <= kAutoPlotReach + kAutoPlotInto)
						{
							to++;
						}

						Box b;
						double sum[3] = { 0.0, 0.0, 0.0 };

						for (int a = 0; a < 3; a++)
						{
							b.lo[a] = b.hi[a] = cells[in[from]][(size_t)a];
						}

						for (size_t k = from; k < to; k++)
						{
							for (int a = 0; a < 3; a++)
							{
								b.lo[a] = std::min(b.lo[a], cells[in[k]][(size_t)a]);
								b.hi[a] = std::max(b.hi[a], cells[in[k]][(size_t)a]);
								sum[a] += cells[in[k]][(size_t)a];
							}
						}

						for (int a = 0; a < 3; a += 2)
						{
							b.lo[a] = Held(b.lo[a] - cellHalf - seam);
							b.hi[a] = Held(b.hi[a] + cellHalf + seam);
						}

						b.lo[1] = Held(b.lo[1] - kAutoPlotReach);   // y is down: the reach is up
						b.hi[1] = Held(b.hi[1] + kAutoPlotInto);
						b.surface = AutoPlotSurface::Floor;

						// over the cell nearest the cells' middle: half the reach up first, then a quarter and three quarters
						double const n = (double)(to - from);
						size_t mid = in[from];
						double best = 1e300;

						for (size_t k = from; k < to; k++)
						{
							double const dx = cells[in[k]][0] - sum[0] / n, dz = cells[in[k]][2] - sum[2] / n;

							if (dx * dx + dz * dz < best)
							{
								best = dx * dx + dz * dz;
								mid = in[k];
							}
						}

						for (double const part : { 0.5, 0.25, 0.75 })
						{
							b.hints.push_back(std::vector<double>{ cells[mid][0], cells[mid][1] - kAutoPlotReach * part, cells[mid][2] });
						}

						made.push_back(b);
						keys.push_back(square.first);
						from = to;
					}
				}

				// CONSOLIDATED (autoplot_plan.h kAutoPlotFloorMost; Drake's AutoPlot_MergeTiles): the tiles merged into rectangles of whole squares, at most kAutoPlotFloorMost a side,
				// their floors within kAutoPlotFloorRise; each rectangle one box round its tiles, its centre over the tile nearest its middle
				std::vector<AutoPlotTileRect> rects(made.size());

				for (size_t k = 0; k < made.size(); k++)
				{
					for (int a = 0; a < 3; a++)
					{
						rects[k].lo[a] = made[k].lo[a];
						rects[k].hi[a] = made[k].hi[a];
					}

					rects[k].square[0] = keys[k].first;
					rects[k].square[1] = keys[k].second;
				}

				std::vector<std::vector<size_t> > groups;

				if (mostSide >= edge)
				{
					// THE DOORWAY STRIPS (the long paths, Bishop 11-20): the tiles within kAutoPlotDoorStrip of a doorway's line merged only among themselves, the rest in whole
					// rectangles: the strip climbs, the big floor behind it never does (a floor box beside an exit box: THE DOORWAY START), so ceiling -> strip -> floor -> exit
					// stays one box longer than ceiling -> door -> exit
					std::vector<size_t> partOf[2];

					for (size_t k = 0; k < made.size(); k++)
					{
						bool inStrip = false;

						for (std::vector<double> const &r : strips)
						{
							inStrip = inStrip || (made[k].lo[0] < r[1] && made[k].hi[0] > r[0] && made[k].lo[2] < r[3] && made[k].hi[2] > r[2]);
						}

						partOf[inStrip ? 1 : 0].push_back(k);
					}

					for (int part = 0; part < 2; part++)
					{
						std::vector<AutoPlotTileRect> some;

						for (size_t const k : partOf[part])
						{
							some.push_back(rects[k]);
						}

						// the strips as big as the rest: a strip holds only the tiles within kAutoPlotDoorStrip of the line, so its depth is held by that, and it runs the
						// doorway's whole width in one box (11-20: held to 1.2 m a side it was several boxes a doorway)
						for (std::vector<size_t> const &g : AutoPlot_MergeTiles(some, edge, mostSide,
							(double)kAutoPlotFloorRise))
						{
							std::vector<size_t> back;

							for (size_t const i : g)
							{
								back.push_back(partOf[part][i]);
							}

							groups.push_back(back);
						}
					}
				}
				else
				{
					for (size_t k = 0; k < made.size(); k++)
					{
						groups.push_back(std::vector<size_t>(1, k));   // not merged
					}
				}
				std::vector<Box> merged;

				for (std::vector<size_t> const &group : groups)
				{
					if (group.empty())
					{
						continue;
					}

					Box b = made[group[0]];
					double middle[2] = { 0.0, 0.0 };

					for (size_t const k : group)
					{
						for (int a = 0; a < 3; a++)
						{
							b.lo[a] = std::min(b.lo[a], made[k].lo[a]);
							b.hi[a] = std::max(b.hi[a], made[k].hi[a]);
						}
					}

					middle[0] = (b.lo[0] + b.hi[0]) / 2.0;
					middle[1] = (b.lo[2] + b.hi[2]) / 2.0;

					size_t nearest = group[0];
					double best = 1e300;

					for (size_t const k : group)
					{
						double const dx = (made[k].lo[0] + made[k].hi[0]) / 2.0 - middle[0], dz = (made[k].lo[2] + made[k].hi[2]) / 2.0 - middle[1];

						if (dx * dx + dz * dz < best)
						{
							best = dx * dx + dz * dz;
							nearest = k;
						}
					}

					b.hints = made[nearest].hints;
					merged.push_back(b);
				}

				made.swap(merged);

				if (made.size() <= most)
				{
					break;
				}

				edge *= 1.25;
			}

			*edgeUsed = edge;
			return made;
		}

		// THE CEILING TILES (autoplot_plan.h AutoPlotFloor): the ceiling's places cut by squares `edge` across (x and z), each square's places cut again where their heights are
		// more than a climbing alien's reach apart (a ceiling over a ceiling, a soffit under a higher room's ceiling); each part a thin ceiling box round its places, as a ceiling
		// stretch's (kAutoPlotInto into it, kAutoPlotClimbReach under its lowest), half a cell past them and a seam more so the next square's box touches; its centre inside the
		// ceiling over its places (the manual's rule). The squares grow (x 1.25) until there are no more than `most`
		std::vector<Box> CeilingTiles(std::vector<std::vector<double> > const &places, double cellHalf, size_t most, double *edgeUsed,
			std::vector<std::vector<double> > const &platformHeads = std::vector<std::vector<double> >())
		{
			double const seam = 10.0;
			double edge = std::max((double)kAutoPlotCeilingTile, 2.0 * cellHalf);
			std::vector<Box> made;
			double low[2] = { 0.0, 0.0 }, span[2] = { 0.0, 0.0 };

			for (int a = 0; a < 2 && !places.empty(); a++)
			{
				double high = places[0][(size_t)(a * 2)];

				low[a] = high;

				for (std::vector<double> const &p : places)
				{
					low[a] = std::min(low[a], p[(size_t)(a * 2)]);
					high = std::max(high, p[(size_t)(a * 2)]);
				}

				span[a] = high - low[a];
			}

			for (int round = 0; round < 60; round++)
			{
				// each axis in equal parts no wider than the edge, from the places' own lowest (24 m in four 6 m parts, not five with a sliver at the end: a box more on the lane)
				int64_t parts[2];
				double width[2];

				for (int a = 0; a < 2; a++)
				{
					parts[a] = std::max((int64_t)1, (int64_t)ceil(span[a] / edge));
					width[a] = span[a] > 0.0 ? span[a] / (double)parts[a] : 1.0;
				}

				std::map<std::pair<int64_t, int64_t>, std::vector<size_t> > squares;

				for (size_t i = 0; i < places.size(); i++)
				{
					int64_t at[2];

					for (int a = 0; a < 2; a++)
					{
						at[a] = std::min(parts[a] - 1, std::max((int64_t)0, (int64_t)floor((places[i][(size_t)(a * 2)] - low[a]) / width[a])));
					}

					squares[std::make_pair(at[0], at[1])].push_back(i);
				}

				made.clear();

				for (auto &square : squares)
				{
					std::vector<size_t> &in = square.second;

					std::sort(in.begin(), in.end(), [&places](size_t a, size_t b) { return places[a][1] < places[b][1] || (places[a][1] == places[b][1] && a < b); });

					// the platforms' head levels under this square's places (A PLATFORM'S HEADS, at the caller), sorted: a band stops where its next place would be as low
					std::vector<double> levels;
					{
						double xz[4] = { 1e300, -1e300, 1e300, -1e300 };

						for (size_t const i : in)
						{
							xz[0] = std::min(xz[0], places[i][0]), xz[1] = std::max(xz[1], places[i][0]);
							xz[2] = std::min(xz[2], places[i][2]), xz[3] = std::max(xz[3], places[i][2]);
						}

						for (std::vector<double> const &h : platformHeads)
						{
							if (h.size() == 3 && h[0] >= xz[0] - cellHalf && h[0] <= xz[1] + cellHalf && h[1] >= xz[2] - cellHalf && h[1] <= xz[3] + cellHalf)
							{
								levels.push_back(h[2]);
							}
						}

						std::sort(levels.begin(), levels.end());
					}

					auto const headBetween = [&levels](double higher, double lower)   // a head level L with higher < L <= lower (y is down)
					{
						auto const it = std::upper_bound(levels.begin(), levels.end(), higher);
						return it != levels.end() && *it <= lower;
					};

					for (size_t from = 0; from < in.size();)
					{
						size_t to = from + 1;

						// one lane: ceiling places of one level (no gap of more than a climbing alien's reach between one height and the next), and a slope rising no more than
						// kAutoPlotLaneRise in all (opscen02's ramp in four 450 bands made its ceiling route long, so every merge of its floor broke a lane: 90 boxes, 11-20;
						// the stall the bands were for, a strip climbing to a mid-room centre, is gone: a strip climbs only into a wall)
						while (to < in.size() && places[in[to]][1] - places[in[to - 1]][1] <= kAutoPlotClimbReach && places[in[to]][1] - places[in[from]][1] <= kAutoPlotLaneRise &&
							!headBetween(places[in[to - 1]][1], places[in[to]][1]))
						{
							to++;
						}

						Box b;
						double sum[3] = { 0.0, 0.0, 0.0 };

						for (int a = 0; a < 3; a++)
						{
							b.lo[a] = b.hi[a] = places[in[from]][(size_t)a];
						}

						for (size_t k = from; k < to; k++)
						{
							for (int a = 0; a < 3; a++)
							{
								b.lo[a] = std::min(b.lo[a], places[in[k]][(size_t)a]);
								b.hi[a] = std::max(b.hi[a], places[in[k]][(size_t)a]);
								sum[a] += places[in[k]][(size_t)a];
							}
						}

						b.hasCeiling = true;
						b.ceilingLow = b.hi[1];   // the places' lowest (y is down), before the box is grown

						for (int a = 0; a < 3; a += 2)
						{
							b.lo[a] = Held(b.lo[a] - cellHalf - seam);
							b.hi[a] = Held(b.hi[a] + cellHalf + seam);
						}

						b.lo[1] = Held(b.lo[1] - kAutoPlotInto);         // into the ceiling (up)
						b.hi[1] = Held(b.hi[1] + kAutoPlotClimbReach);   // and a hanging alien's top corners under it
						b.surface = AutoPlotSurface::Ceiling;
						b.inside = true;

						// inside the ceiling over the place nearest the places' middle first, then over places spread through the part
						double const n = (double)(to - from);
						size_t mid = in[from];
						double best = 1e300;

						for (size_t k = from; k < to; k++)
						{
							double const dx = places[in[k]][0] - sum[0] / n, dz = places[in[k]][2] - sum[2] / n;

							if (dx * dx + dz * dz < best)
							{
								best = dx * dx + dz * dz;
								mid = in[k];
							}
						}

						b.hints.push_back(std::vector<double>{ places[mid][0], places[mid][1] - kAutoPlotInto / 2.0, places[mid][2] });

						size_t const spread = std::max((size_t)1, (to - from) / 8);

						for (size_t k = from; k < to; k += spread)
						{
							b.hints.push_back(std::vector<double>{ places[in[k]][0], places[in[k]][1] - kAutoPlotInto / 2.0, places[in[k]][2] });
						}

						made.push_back(b);
						from = to;
					}
				}

				if (made.size() <= most)
				{
					break;
				}

				edge *= 1.25;
			}

			*edgeUsed = edge;
			return made;
		}

		// box j cut back clear of box d's face, on the axis and side that keep the most of j (THE LANES: the door box keeps its shape and its centre is its own); false when j
		// lies inside d's span on every axis (nothing to keep: give it up for d), or the cut would change nothing. Clear by one of the file's units: a box's max in the file
		// is one past its own and the game's inside is min <= p <= max - 1, so a box cut to the face itself still shares that unit, a centre there is in both, and the cut
		// made again and again changed nothing (Genshd1 v2c, 11-19: 4000 rounds, 120 AI modules refused)
		bool TrimOff(Box *j, Box const &d, double scale)
		{
			int bestAxis = -1;
			double bestKept = 0.0, bestTo = 0.0;
			bool cutHigh = false;

			for (int a = 0; a < 3; a++)
			{
				double const span = j->hi[a] - j->lo[a];
				double const highTo = (floor(d.lo[a] / scale) - 0.5) * scale;   // in the file: one unit under d's min
				double const lowTo = (floor(d.hi[a] / scale) + 1.5) * scale;    // and d's max, the unit past its own
				double const under = highTo - j->lo[a], over = j->hi[a] - lowTo;

				if (span > 0.0 && under > 0.0 && highTo < j->hi[a] && under / span > bestKept)
				{
					bestKept = under / span;
					bestAxis = a;
					bestTo = highTo;
					cutHigh = true;
				}

				if (span > 0.0 && over > 0.0 && lowTo > j->lo[a] && over / span > bestKept)
				{
					bestKept = over / span;
					bestAxis = a;
					bestTo = lowTo;
					cutHigh = false;
				}
			}

			if (bestAxis < 0)
			{
				return false;
			}

			(cutHigh ? j->hi[bestAxis] : j->lo[bestAxis]) = bestTo;
			return true;
		}

		// box `gone` given up for box `keep`, which keeps its shape (THE LANES: a climbing box a door box covers): gone's links become keep's, the last box takes gone's place
		void Absorb(std::vector<Box> *boxes, Links *links, size_t keep, size_t gone)
		{
			std::vector<size_t> from(boxes->size());

			for (size_t i = 0; i < boxes->size(); i++)
			{
				from[i] = i == gone ? keep : i;
			}

			size_t const last = boxes->size() - 1;

			if (gone != last)
			{
				(*boxes)[gone] = (*boxes)[last];

				for (size_t i = 0; i < from.size(); i++)
				{
					from[i] = from[i] == last ? gone : from[i];
				}
			}

			boxes->pop_back();
			links->Remap(from);
		}

		// THE LANES' rules, kept after anything that grows a box (autoplot_plan.h): every floor box under a ceiling's box (one over it in x and z) cut to kAutoPlotHangClear
		// under that ceiling's lowest place, where that leaves it kAutoPlotLeastRoom (a hanging alien's low test corners are 900 under the ceiling: in a floor box, its best
		// route could start there, plain, and gravity comes on); and no climbing box holding an exit point (in its target's box an alien's route has no link: it drops), cut
		// off 50 short of it on the side that keeps the most of the box. *tooLow: floor boxes too low to cut; *exitsIn: exit points a climbing box still holds
		void KeepTheLanes(std::vector<Box> *boxes, std::vector<std::vector<double> > const &exits, size_t *tooLow, size_t *exitsIn)
		{
			*tooLow = 0;
			*exitsIn = 0;

			for (Box &f : *boxes)
			{
				if (Climbing(f))
				{
					continue;
				}

				for (Box const &c : *boxes)
				{
					bool const over = c.hasCeiling && c.lo[0] < f.hi[0] && c.hi[0] > f.lo[0] && c.lo[2] < f.hi[2] && c.hi[2] > f.lo[2] && c.ceilingLow < f.hi[1];
					double top = c.ceilingLow + kAutoPlotHangClear;
					size_t const own = (size_t)f.exitOwn;

					// A PURE TARGET (autoplot_plan.h kAutoPlotHangLeast): an exit's own box keeps its top at its exit point, where that is far enough under the ceiling
					if (f.exitOwn >= 0 && own < exits.size() && exits[own].size() == 3 && exits[own][1] < top && exits[own][1] >= c.ceilingLow + kAutoPlotHangLeast)
					{
						top = exits[own][1];
					}
					else if (f.exitOwn >= 0 && own < exits.size() && exits[own].size() == 3 && exits[own][1] >= c.ceilingLow + kAutoPlotBandEdge &&
						exits[own][1] <= c.ceilingLow + kAutoPlotCornerHeight - kAutoPlotBandEdge)
					{
						continue;   // A PURE TARGET IN THE HANGING BAND: an exit's own box between a hanging alien's corner heights, not cut
					}

					if (!over || f.lo[1] >= top)
					{
						continue;
					}

					if (f.hi[1] - top >= kAutoPlotLeastRoom)
					{
						f.lo[1] = top;
					}
					else
					{
						(*tooLow)++;
					}
				}
			}

			for (Box &c : *boxes)
			{
				for (size_t e = 0; e < exits.size() && Climbing(c); e++)
				{
					if (exits[e].size() != 3)
					{
						continue;   // an exit of the wrong count of numbers: kept in its place (empty) so each exit's own box finds its own
					}

					double const p[3] = { exits[e][0], exits[e][1], exits[e][2] };

					if (!Holds(c, p, 0.0))
					{
						continue;
					}

					int bestAxis = -1;
					bool cutHigh = false;
					double bestKept = -1.0;

					for (int a = 0; a < 3; a++)
					{
						double const span = c.hi[a] - c.lo[a];
						double const under = p[a] - 50.0 - c.lo[a], over = c.hi[a] - (p[a] + 50.0);

						if (span > 0.0 && under >= 300.0 && under / span > bestKept)
						{
							bestKept = under / span;
							bestAxis = a;
							cutHigh = true;
						}

						if (span > 0.0 && over >= 300.0 && over / span > bestKept)
						{
							bestKept = over / span;
							bestAxis = a;
							cutHigh = false;
						}
					}

					if (bestAxis < 0)
					{
						(*exitsIn)++;
						continue;
					}

					(cutHigh ? c.hi[bestAxis] : c.lo[bestAxis]) = cutHigh ? p[bestAxis] - 50.0 : p[bestAxis] + 50.0;
				}
			}
		}
	}

	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, double levelScale)
	{
		return AutoPlot_Plan(ways, exits, AutoPlotFloor(), levelScale);
	}

	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floorGiven, double levelScale)
	{
		return AutoPlot_Plan(ways, exits, floorGiven, std::vector<AutoPlotDoor>(), levelScale);
	}

	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floorGiven,
		std::vector<AutoPlotDoor> const &doors, double levelScale)
	{
		AutoPlotPlan plan;
		double const scale = levelScale > 0.0 ? levelScale : 1.0;
		std::vector<Box> boxes;
		Links links;
		std::vector<std::vector<double> > cells;

		for (std::vector<int32_t> const &c : floorGiven.cells)
		{
			if (c.size() == 3 && cells.size() < kAutoPlotMostCells)
			{
				cells.push_back(std::vector<double>{ Held(c[0]), Held(c[1]), Held(c[2]) });
			}
		}

		bool const tiled = !cells.empty();
		double const cellHalf = floorGiven.cellEdge > 0 && floorGiven.cellEdge < 100000 ? floorGiven.cellEdge / 2.0 : 0.0;
		std::vector<std::vector<double> > ceilingPlaces;

		for (std::vector<int32_t> const &c : floorGiven.ceilings)
		{
			if (c.size() == 3 && ceilingPlaces.size() < kAutoPlotMostCells)
			{
				ceilingPlaces.push_back(std::vector<double>{ Held(c[0]), Held(c[1]), Held(c[2]) });
			}
		}

		bool const ceilingTiled = !ceilingPlaces.empty();

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

					if ((turns || length >= (now.surface == AutoPlotSurface::Floor ? kAutoPlotFloorStretch : kAutoPlotStretch)) && k < last)
					{
						stretches.push_back(now);
						begin(k);   // the next stretch starts where this one ends: their boxes meet
						length = 0.0;
					}
				}

				stretches.push_back(now);

				bool kept = false;   // the stretch before this one became the last box

				for (size_t s = 0; s < stretches.size(); s++)
				{
					if ((tiled && stretches[s].surface == AutoPlotSurface::Floor) || (ceilingTiled && stretches[s].surface == AutoPlotSurface::Ceiling))
					{
						kept = false;   // the tiles are the floor, and the ceiling's when it was given
						continue;
					}

					boxes.push_back(BoxOf(stretches[s]));

					if (kept)
					{
						links.Add(boxes.size() - 2, boxes.size() - 1, LinkFlags(stretches[s - 1].surface, stretches[s].surface));
					}

					kept = true;
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

		// 2b. THE FLOOR TILES (with floor cells): no more than the boxes the highways leave under 99 (half of them at least: the cap then merges highways first). Each highway's
		// bottom cut off at the top of the lowest floor box it reaches into (a tile, else a floor stretch's), so it stands on the floor and never covers a floor box's centre, and
		// one left too thin taken out. Tiles that touch
		// linked plain (the floor route: marines, predators and praetorians go), a highway and a tile that touch linked aliens only (0x4: AlienIsEncouragedToCrawl)
		std::vector<Box> tiles;
		std::vector<std::vector<double> > exitPlaces;   // the exits, for KeepTheLanes, one for each in its place (an exit's own box finds its own; one of the wrong count, empty)
		size_t tooLow = 0, exitsInClimbing = 0;

		for (std::vector<int32_t> const &e : exits)
		{
			exitPlaces.push_back(e.size() == 3 ? std::vector<double>{ Held(e[0]), Held(e[1]), Held(e[2]) } : std::vector<double>());
		}

		// THE CEILING TILES first (with the ceiling given): at most a quarter of the stock's 99, so the floor under them is always finer; each linked aliens only to every
		// climbing box it touches (the next tile, a wall the ways climbed)
		if (ceilingTiled)
		{
			// A PLATFORM'S HEADS (Drake's gate on e0a30bd6: Object11's 74 stall places): a standing floor under the ceiling that is not this AI module's own floor (another
			// module's or a still object's, Drake's AutoPlotGrid_FloorsIn), its standing alien's head level (kAutoPlotCornerHeight over it, 50 more): the ceiling tiles are
			// cut there, so the cut over the heads below can lift a tile clear of them (its own floor, a ramp under a ramp's ceiling, stays one tile: chain A's opscen02)
			std::vector<std::vector<double> > platformHeads;
			{
				std::set<std::tuple<int64_t, int64_t, int64_t> > own;

				for (std::vector<double> const &c : cells)
				{
					own.insert(std::make_tuple((int64_t)floor(c[0] / 10.0 + 0.5), (int64_t)floor(c[1] / 10.0 + 0.5), (int64_t)floor(c[2] / 10.0 + 0.5)));
				}

				for (std::vector<int32_t> const &c : floorGiven.standFloors)
				{
					if (c.size() == 3 && platformHeads.size() < kAutoPlotMostCells &&
						!own.count(std::make_tuple((int64_t)floor(Held(c[0]) / 10.0 + 0.5), (int64_t)floor(Held(c[1]) / 10.0 + 0.5), (int64_t)floor(Held(c[2]) / 10.0 + 0.5))))
					{
						// x, z, and the level the tiles are cut at: the head (kAutoPlotCornerHeight over the floor, 50 more) less 150, so the cut lifts the tile's bottom to the
						// head and every place left in it still has a hanging alien's top corners (100 under the ceiling, the plan test's measure) inside it
						platformHeads.push_back(std::vector<double>{ Held(c[0]), Held(c[2]), Held(c[1]) - kAutoPlotCornerHeight - 200.0 });
					}
				}
			}

			std::vector<Box> const ceiling = CeilingTiles(ceilingPlaces, cellHalf, (size_t)kAutoPlotMostBoxes / 4, &plan.ceilingTile, platformHeads);
			size_t const first = boxes.size();

			boxes.insert(boxes.end(), ceiling.begin(), ceiling.end());
			plan.ceilingBoxes = ceiling.size();

			// each lane box's bottom over a standing alien's head on every floor under it (Drake's gate on 3ff518cb: Object11's box 3 reached a platform's standing aliens'
			// head corners, held them with their floor's box, and they headed for its centre 1.4 m up: 60 stall places); not over its own lowest ceiling place (a crawl
			// space too low for that is left as it is)
			std::vector<std::vector<double> > under = cells;   // the AI module's own floor, and any module's or still object's under its ceiling (Drake's AutoPlotGrid_FloorsIn)

			for (std::vector<int32_t> const &c : floorGiven.standFloors)
			{
				if (c.size() == 3 && under.size() < 2 * kAutoPlotMostCells)
				{
					under.push_back(std::vector<double>{ Held(c[0]), Held(c[1]), Held(c[2]) });
				}
			}

			for (size_t k = first; k < boxes.size(); k++)
			{
				Box &lane = boxes[k];

				for (std::vector<double> const &cell : under)
				{
					double const head = cell[1] - kAutoPlotCornerHeight - 50.0;   // y is down

					if (cell[0] >= lane.lo[0] && cell[0] <= lane.hi[0] && cell[2] >= lane.lo[2] && cell[2] <= lane.hi[2] && head < lane.hi[1] && head > lane.ceilingLow + 50.0)
					{
						lane.hi[1] = head;
					}
				}
			}

			for (size_t a = first; a < boxes.size(); a++)
			{
				for (size_t b = 0; b < boxes.size(); b++)
				{
					if (b != a && (b < first || b > a) && Climbing(boxes[b]) && Touch(boxes[a], boxes[b]))
					{
						links.Add(a, b, LinkFlags(boxes[a].surface, boxes[b].surface));
					}
				}
			}
		}

		if (tiled)
		{
			size_t const climbing = (size_t)std::count_if(boxes.begin(), boxes.end(), Climbing);
			size_t const room = (size_t)kAutoPlotMostBoxes > climbing + exits.size() + 2 ? (size_t)kAutoPlotMostBoxes - climbing - exits.size() - 2 : 0;

			double const mostSide = floorGiven.mostSide < 0.0 ? (double)kAutoPlotFloorMost : floorGiven.mostSide;

			// the doorway strips (x lo, x hi, z lo, z hi): this side of each doorway's line, kAutoPlotDoorStrip deep, across its door box and a half width more
			std::vector<std::vector<double> > strips;

			for (AutoPlotDoor const &d : doors)
			{
				if (!d.throughFound || !d.soffitFound)
				{
					continue;
				}

				int const t = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2, c = t == 0 ? 2 : 0;
				double const s = d.through[t == 0 ? 0 : 1] < 0.0 ? -1.0 : 1.0;
				double const m = Held(d.middle[t]), across = Held(d.soffit[c]);
				double lo[3], hi[3];

				lo[t] = std::min(m, m - s * kAutoPlotDoorStrip);
				hi[t] = std::max(m, m - s * kAutoPlotDoorStrip);
				lo[c] = across - 2.0 * kAutoPlotHalfWidth;
				hi[c] = across + 2.0 * kAutoPlotHalfWidth;
				strips.push_back(std::vector<double>{ lo[0], hi[0], lo[2], hi[2] });
			}

			tiles = Tiles(cells, cellHalf, std::max(room, (size_t)kAutoPlotMostBoxes / 2), &plan.floorTile, mostSide, strips);
			plan.floorMost = mostSide >= plan.floorTile ? mostSide : 0.0;
			boxes.insert(boxes.end(), tiles.begin(), tiles.end());
		}

		// the floor kept under the ceilings first (THE LANES), then each highway stood on what floor is left
		KeepTheLanes(&boxes, exitPlaces, &tooLow, &exitsInClimbing);
		StandOnTheFloor(&boxes, &links, std::vector<Box>());

		if (tiled)
		{
			for (size_t a = 0; a < boxes.size(); a++)
			{
				for (size_t b = 0; b < a; b++)
				{
					if ((!Climbing(boxes[a]) || !Climbing(boxes[b])) && Touch(boxes[a], boxes[b]))
					{
						links.Add(a, b, LinkFlags(boxes[a].surface, boxes[b].surface));
					}
				}
			}
		}

		// 2c. THE LANES' DOOR BOXES (autoplot_plan.h): at each exit with a ceiling over it and a known way through, a box on the doorway's top, never made one with another
		// nor taken out; the climbing boxes it mostly covers given up for it (their links its own: the way from the soffit starts in it), the climbing boxes it touches linked
		// to it aliens only
		plan.doorWords.assign(exits.size(), std::string());

		for (size_t e = 0; e < exits.size(); e++)
		{
			AutoPlotDoor const *const d = e < doors.size() ? &doors[e] : nullptr;

			if (exits[e].size() != 3)
			{
				plan.doorWords[e] = "no exit point";
				continue;
			}

			if (!d || !d->soffitFound)
			{
				plan.doorWords[e] = "no lane: no ceiling over the exit point nor past the doorway";
				continue;
			}

			if (!d->throughFound)
			{
				plan.doorWords[e] = "no lane: the way through the doorway is not known (no exit point back)";
				continue;
			}

			double const ax = fabs(d->through[0]), az = fabs(d->through[1]);
			int const t = ax >= az ? 0 : 2, c = t == 0 ? 2 : 0;   // the axis through the doorway, and the one across it
			double const s = d->through[t == 0 ? 0 : 1] < 0.0 ? -1.0 : 1.0;
			double const m = Held(d->middle[t]);
			double const soffit[3] = { Held(d->soffit[0]), Held(d->soffit[1]), Held(d->soffit[2]) };
			Box door;

			door.lo[t] = Held(std::min(m - s * kAutoPlotLaneBack, m + s * kAutoPlotLaneOver));
			door.hi[t] = Held(std::max(m - s * kAutoPlotLaneBack, m + s * kAutoPlotLaneOver));
			door.lo[c] = Held(soffit[c] - kAutoPlotHalfWidth);
			door.hi[c] = Held(soffit[c] + kAutoPlotHalfWidth);
			door.lo[1] = Held(soffit[1] - kAutoPlotInto);
			door.hi[1] = Held(soffit[1] + kAutoPlotLaneDrop);
			door.surface = AutoPlotSurface::Ceiling;
			door.inside = true;
			door.door = (int)e;
			door.hasCeiling = true;
			door.ceilingLow = soffit[1];

			// its centre PAST the doorway's middle first, inside the soffit (Drake's hand2, Bishop 11-19): a hanging alien whose route starts here heads on through the
			// doorway, never back into the room
			door.hints.push_back(std::vector<double>{ t == 0 ? m + s * kAutoPlotDoorAimPast : soffit[0], soffit[1] - kAutoPlotInto / 2.0,
				t == 2 ? m + s * kAutoPlotDoorAimPast : soffit[2] });

			// THE CLIMB STATION: reaching across to the station's top (a side wall's within 8 m), its first centre over it, 300 off the wall, inside the ceiling
			if (d->stationFound && fabs((double)d->stationTop[c] - soffit[c]) <= 8000.0)
			{
				double const across = Held(d->stationTop[c]);
				double const inward = across < soffit[c] ? 300.0 : -300.0;
				std::vector<double> hint(3);

				door.lo[c] = std::min(door.lo[c], Held(across - 50.0));
				door.hi[c] = std::max(door.hi[c], Held(across + 50.0));
				hint[(size_t)t] = std::min(std::max((double)Held(d->stationTop[t]), door.lo[t] + 100.0), door.hi[t] - 100.0);
				hint[(size_t)c] = across + inward;
				hint[1] = soffit[1] - kAutoPlotInto / 2.0;
				door.hints.push_back(hint);
			}

			// its centre inside the soffit (the manual's climb rule), on this AI module's side of the doorway
			for (double const part : { 0.5, 0.25, 0.75 })
			{
				std::vector<double> hint(3);

				hint[(size_t)t] = m - s * kAutoPlotLaneBack * part;
				hint[(size_t)c] = soffit[c];
				hint[1] = soffit[1] - kAutoPlotInto / 2.0;
				door.hints.push_back(hint);
			}

			plan.doorWords[e] = std::string("lane") + (std::min(ax, az) > 0.3 * std::max(ax, az) ? " (the doorway is not square to x or z; its box is)" : "");
			boxes.push_back(door);
		}

		for (bool absorbed = true; absorbed;)
		{
			absorbed = false;

			for (size_t a = 0; a < boxes.size() && !absorbed; a++)
			{
				for (size_t b = 0; b < boxes.size() && !absorbed; b++)
				{
					if (boxes[a].door >= 0 && b != a && boxes[b].door < 0 && Climbing(boxes[b]) && Overlap(boxes[a], boxes[b]) >= 0.5 * Volume(boxes[b]))
					{
						Absorb(&boxes, &links, a, b);
						absorbed = true;
					}
				}
			}
		}

		for (size_t a = 0; a < boxes.size(); a++)
		{
			for (size_t b = 0; b < boxes.size(); b++)
			{
				if (boxes[a].door >= 0 && b != a && Climbing(boxes[b]) && Touch(boxes[a], boxes[b]))
				{
					links.Add(a, b, kLinkAlien);
				}
			}
		}

		// THE DOORWAY START (autoplot_plan.h): the ceiling's box over the doorway's floor at the climb station's side wall, its first centre IN that wall at the ceiling's
		// corner (half a cell and kAutoPlotCornerInWall past the station's top, kAutoPlotCornerIn inside the doorway), the box grown into the wall to hold it: an alien heading
		// for it walks into the wall and climbs (Drake's review: a centre over the floor, and it stands under it, its speed 0). No station, no corner: the floor box at the
		// doorway is linked to a wall's box instead (below)
		for (size_t a = 0; a < boxes.size(); a++)
		{
			int const e = boxes[a].door;

			if (e < 0 || (size_t)e >= doors.size() || !doors[(size_t)e].throughFound || !doors[(size_t)e].stationFound)
			{
				continue;
			}

			AutoPlotDoor const &d = doors[(size_t)e];
			int const t = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2, c = t == 0 ? 2 : 0;
			double const s = d.through[t == 0 ? 0 : 1] < 0.0 ? -1.0 : 1.0;
			double const m = Held(d.middle[t]);
			double const across = Held(d.stationTop[c]);
			double const wall = across < Held(d.soffit[c]) ? -1.0 : 1.0;   // the wall is past the station's top, away from the doorway
			double const inWall = Held(across + wall * (cellHalf + kAutoPlotCornerInWall));

			// the ceiling's box nearest the doorway on this side at the station's wall (the grid's ceiling may stop short of the doorway: the door box reaches over the gap)
			size_t nearest = boxes.size();
			double nearestGap = 1e300;

			for (size_t k = 0; k < boxes.size(); k++)
			{
				Box const &tile = boxes[k];
				double const nearFace = s > 0.0 ? tile.hi[t] : tile.lo[t], gap = s * (m - nearFace);

				if (tile.door < 0 && Climbing(tile) && tile.hasCeiling && across >= tile.lo[c] && across <= tile.hi[c] && gap >= -kAutoPlotHalfWidth &&
					gap <= kAutoPlotLaneBack && gap < nearestGap)
				{
					nearestGap = gap;
					nearest = k;
				}
			}

			// the one holding the station's top first (where it climbed)
			for (size_t k = 0; k < boxes.size(); k++)
			{
				Box const &tile = boxes[k];
				double const atT = Held(d.stationTop[t]);

				if (tile.door < 0 && Climbing(tile) && tile.hasCeiling && across >= tile.lo[c] && across <= tile.hi[c] && atT >= tile.lo[t] && atT <= tile.hi[t])
				{
					nearest = k;
					break;
				}
			}

			if (nearest == boxes.size())
			{
				continue;
			}

			Box &tile = boxes[nearest];
			std::vector<double> corner(3);
			double const inAt = m - s * kAutoPlotCornerIn;

			// along the doorway's way through, where the STATION climbed (its top's own place): there the grid's way went up a wall from the floor. At the doorway's line
			// it may be a lintel over an opening (Drake, the polygons of Object11 11-20: the corner 410 in from store01's south face, a wall only above 3.9 m: the alien
			// walked under it and stood)
			double const climbedAt = d.stationTop[t] != 0 || d.stationTop[c] != 0 ? Held(d.stationTop[t]) : inAt;

			corner[(size_t)t] = std::min(std::max(climbedAt, tile.lo[t] + 50.0), tile.hi[t] - 50.0);
			corner[(size_t)c] = inWall;
			corner[1] = tile.lo[1] + kAutoPlotInto / 2.0;   // inside the ceiling
			(wall < 0.0 ? tile.lo[c] : tile.hi[c]) = wall < 0.0 ? std::min(tile.lo[c], Held(inWall - 50.0)) : std::max(tile.hi[c], Held(inWall + 50.0));
			tile.hints.insert(tile.hints.begin(), corner);
			tile.cornerFor = e;
			tile.cornerAt[0] = corner[0];
			tile.cornerAt[1] = corner[1];
			tile.cornerAt[2] = corner[2];
		}

		// 3. an exit no box holds: a box of its own (as a floor's), linked to the boxes it touches, else its nearest. An exit with a door box always has its own, its top
		// kAutoPlotHangClear under the soffit, linked aliens only to the door box (THE LANES)
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
				held = held || (Holds(b, p, 0.0) && (!tiled || !Climbing(b)));   // with tiles, a floor box: the floor route starts there
			}

			int doorAt = -1;

			for (size_t b = 0; b < boxes.size(); b++)
			{
				doorAt = boxes[b].door == (int)e ? (int)b : doorAt;
			}

			if (held && doorAt < 0)
			{
				continue;
			}

			Box own;
			double hangBand = 1e300;   // in the hanging band: the lowest its box may reach (y is down: its greatest y)

			for (int i = 0; i < 3; i++)
			{
				own.lo[i] = Held(p[i] - (i == 1 ? kAutoPlotReach : kAutoPlotHalfWidth));
				own.hi[i] = Held(p[i] + (i == 1 ? kAutoPlotInto : kAutoPlotHalfWidth));
			}

			// reaching down onto the floor box under it (a doorway's exit sits about 2 m up: Drake's Colony measures), so it touches it and the floor route goes through
			double under = -1e300;

			for (Box const &b : boxes)
			{
				bool const beneath = !Climbing(b) && b.lo[0] <= own.hi[0] && b.hi[0] >= own.lo[0] && b.lo[2] <= own.hi[2] && b.hi[2] >= own.lo[2] && b.lo[1] >= p[1] &&
					b.lo[1] - p[1] <= kAutoPlotExitReach;

				if (beneath && (under == -1e300 || b.lo[1] < under))
				{
					under = b.lo[1];   // the nearest floor under it
				}
			}

			if (under > -1e300)
			{
				own.hi[1] = std::max(own.hi[1], Held(under));
			}

			// THE LANES: its top kAutoPlotHangClear under the soffit, else no lane at this doorway (the exit point is too near its top)
			if (doorAt >= 0)
			{
				double const top = boxes[(size_t)doorAt].ceilingLow + kAutoPlotHangClear;

				if (p[1] >= top)
				{
					own.lo[1] = std::max(own.lo[1], top);
				}
				else if (p[1] >= boxes[(size_t)doorAt].ceilingLow + kAutoPlotHangLeast)
				{
					own.lo[1] = std::max(own.lo[1], p[1]);   // A PURE TARGET: its top at the exit point itself, still under a hanging alien's low corners (autoplot_plan.h)
				}
				else if (p[1] >= boxes[(size_t)doorAt].ceilingLow + kAutoPlotBandEdge && p[1] <= boxes[(size_t)doorAt].ceilingLow + kAutoPlotCornerHeight - kAutoPlotBandEdge)
				{
					// A PURE TARGET IN THE HANGING BAND (autoplot_plan.h): between a hanging alien's two corner heights, the doorway's top and 900 under it
					hangBand = boxes[(size_t)doorAt].ceilingLow + kAutoPlotCornerHeight - kAutoPlotBandEdge;
					own.lo[1] = std::max(own.lo[1], boxes[(size_t)doorAt].ceilingLow + kAutoPlotBandEdge);
				}
				else
				{
					plan.doorWords[e] = "no lane: the exit point is " + std::to_string((long long)(p[1] - boxes[(size_t)doorAt].ceilingLow)) +
						" mm under the doorway's top (a hanging alien's low corners are 900 under it: it would be in its target's box)";
				}
			}

			// THE LANES (Drake's review): its centre, the aim a door box's alien heads for, kAutoPlotAimPast past the doorway's middle, the box reaching on past it: an aim
			// the exit point's 100 past would stop the alien where the aim is straight under it, at the old module's edge (chain A's boxes overlap there by 41 mm)
			if (doorAt >= 0 && e < doors.size() && doors[e].throughFound)
			{
				AutoPlotDoor const &d = doors[e];
				int const t = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2;
				double const s = d.through[t == 0 ? 0 : 1] < 0.0 ? -1.0 : 1.0;
				double const aim = Held(d.middle[t] + s * kAutoPlotAimPast), reach = Held(d.middle[t] + s * (kAutoPlotAimPast + 200.0));

				own.lo[t] = std::min(own.lo[t], std::min(aim, reach));
				own.hi[t] = std::max(own.hi[t], std::max(aim, reach));

				std::vector<double> hint(p, p + 3);

				hint[(size_t)t] = aim;
				hint[1] = std::min(std::max(p[1] - 300.0, own.lo[1] + 50.0), own.hi[1] - 50.0);
				own.hints.push_back(hint);
				own.aimFirst = true;

				// THE PURE TARGET (Bishop's gate on the chain 11-20, Drake's idea of 11-19): between a standing alien's two corner heights (its feet, kAutoPlotCornerHeight over
				// them) round the exit point, and from the doorway's line on. It holds the exit point and no alien, standing or hanging, so it is never a route's start: an alien
				// held by it headed for its centre, back past the doorway (Genshd1's chain: 16 such places in Ops01)
				double const m = Held(d.middle[t]);
				double floorY = 1e300;

				for (std::vector<double> const &c : cells)
				{
					if (fabs(c[0] - p[0]) <= kAutoPlotFloorLook && fabs(c[2] - p[2]) <= kAutoPlotFloorLook && c[1] >= p[1] && c[1] < floorY)
					{
						floorY = c[1];   // the floor under the exit point (y is down)
					}
				}

				if (floorY < 1e300)
				{
					double const head = floorY - kAutoPlotCornerHeight;
					double lo = p[1] - 300.0, hi = p[1] + 300.0;

					if (p[1] > head && p[1] < floorY)
					{
						lo = std::max(lo, head + 50.0);
						hi = std::min(hi, floorY - 50.0);
					}
					else if (p[1] <= head)
					{
						hi = std::min(hi, head - 50.0);
					}

					lo = std::max(lo, own.lo[1]);   // and still kAutoPlotHangClear under the soffit
					hi = hangBand < 1e300 ? std::min(hi, hangBand) : hi;   // and over a hanging alien's low corners, in the hanging band

					if (lo < p[1] && p[1] < hi)
					{
						own.lo[1] = lo;
						own.hi[1] = hi;
						own.hints.back()[1] = (lo + hi) / 2.0;
					}
				}

				(s > 0.0 ? own.lo[t] : own.hi[t]) = s > 0.0 ? std::max(own.lo[t], std::min(m, p[t] - 50.0)) : std::min(own.hi[t], std::max(m, p[t] + 50.0));
			}

			// the hanging band kept whatever the pure target above made of it; a band that can't hold the exit point is no lane
			if (hangBand < 1e300)
			{
				own.hi[1] = std::min(own.hi[1], hangBand);

				if (!(own.lo[1] <= p[1] && p[1] <= own.hi[1]))
				{
					plan.doorWords[e] = "no lane: the exit point is " + std::to_string((long long)(p[1] - boxes[(size_t)doorAt].ceilingLow)) +
						" mm under the doorway's top, and no band between a hanging alien's corners holds it";
				}
			}

			own.surface = AutoPlotSurface::Floor;
			own.exitOwn = (int)e;
			own.hints.push_back(std::vector<double>{ p[0], std::min(std::max(p[1] - kAutoPlotReach / 2.0, own.lo[1] + 10.0), own.hi[1] - 10.0), p[2] });
			own.hints.push_back(std::vector<double>{ p[0], p[1], p[2] });
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

			// a floor box a step away (the doorway's exit sits just past the floor's last cell, Colony's 0.4 m): its own box grown to that box's face, so they touch and the floor
			// route goes through (a marine walks that step)
			if (!touched && nearest != at && !Climbing(boxes[nearest]) && nearestGap <= kAutoPlotHalfWidth)
			{
				for (int a = 0; a < 3; a++)
				{
					boxes[at].hi[a] = boxes[at].hi[a] < boxes[nearest].lo[a] ? boxes[nearest].lo[a] : boxes[at].hi[a];
					boxes[at].lo[a] = boxes[at].lo[a] > boxes[nearest].hi[a] ? boxes[nearest].hi[a] : boxes[at].lo[a];
				}

				for (size_t b = 0; b < at; b++)
				{
					if (Touch(boxes[at], boxes[b]))
					{
						links.Add(at, b, LinkFlags(boxes[at].surface, boxes[b].surface));
						touched = true;
					}
				}
			}

			if (!touched && nearest != at)
			{
				links.Add(at, nearest, tiled ? kLinkAlien : LinkFlags(boxes[at].surface, boxes[nearest].surface));   // off the floor's reach: the aliens'
				plan.notes.push_back("exit " + std::to_string((long long)e) + " (" + Place(exits[e]) + ") was on no way: a box of its own, linked to its nearest, " +
					std::to_string((long long)nearestGap) + " mm away");
			}

			if (doorAt >= 0)
			{
				links.Add(at, (size_t)doorAt, kLinkAlien);   // the lane's way down to its target (and up from it)
			}
		}

		KeepTheLanes(&boxes, exitPlaces, &tooLow, &exitsInClimbing);   // the exits' own boxes are floor too: kept under the ceilings, then the highways stood on them
		StandOnTheFloor(&boxes, &links, std::vector<Box>());

		// 4a. THE WAYS JOIN WHAT THEY PASS THROUGH: along each way (inside the AI module, and one point past each end), two points after each other in two boxes link those
		// boxes, even where no box holds the ground between them (a doorway's threshold with no floor cell, a drop off a ledge): the search went that way, so the NPC can. A
		// marine's way gives a plain link (he walks it); an alien's a plain one only between two floor boxes that touch, else aliens only (a jump a marine can't make)
		// Each step is walked in kAutoPlotJoinStep pieces: a straight stretch is one step (the grid's ways keep their turning points only), and joining only its two ends linked
		// the floor boxes at the two ends of Genshd1's 39 m Ops01, a one-box floor route that beat the ceiling lane
		for (AutoPlotWay const &way : ways)
		{
			size_t const steps = std::min(way.steps.size(), kAutoPlotMostSteps);
			std::vector<std::vector<double> > samples;
			std::vector<bool> nearAt;

			for (size_t k = 0; k < steps && samples.size() < kAutoPlotMostSamples; k++)
			{
				bool const near = way.steps[k].inside || (k + 1 < steps && way.steps[k + 1].inside) || (k > 0 && way.steps[k - 1].inside);
				std::vector<double> const here = { Held(way.steps[k].place[0]), Held(way.steps[k].place[1]), Held(way.steps[k].place[2]) };

				if (k > 0 && near)
				{
					std::vector<double> const before = { Held(way.steps[k - 1].place[0]), Held(way.steps[k - 1].place[1]), Held(way.steps[k - 1].place[2]) };
					double const length = sqrt((here[0] - before[0]) * (here[0] - before[0]) + (here[1] - before[1]) * (here[1] - before[1]) +
						(here[2] - before[2]) * (here[2] - before[2]));
					size_t const pieces = (size_t)std::min(length / kAutoPlotJoinStep, 400.0);

					for (size_t c = 1; c < pieces && samples.size() < kAutoPlotMostSamples; c++)
					{
						double const f = (double)c / (double)pieces;

						samples.push_back(std::vector<double>{ before[0] + (here[0] - before[0]) * f, before[1] + (here[1] - before[1]) * f, before[2] + (here[2] - before[2]) * f });
						nearAt.push_back(true);
					}
				}

				samples.push_back(here);
				nearAt.push_back(near);
			}

			int last = -1;

			for (size_t k = 0; k < samples.size(); k++)
			{
				if (!nearAt[k])
				{
					last = -1;   // outside the AI module: nothing joined across the trip out
					continue;
				}

				double const p[3] = { samples[k][0], samples[k][1], samples[k][2] };
				int now = -1;

				// the box it was in if it still is, else a floor box for a marine's way, else any
				if (last >= 0 && Holds(boxes[(size_t)last], p, 0.0))
				{
					now = last;
				}

				for (size_t b = 0; b < boxes.size() && now < 0; b++)
				{
					if (Holds(boxes[b], p, 0.0) && (way.alien || !Climbing(boxes[b])))
					{
						now = (int)b;
					}
				}

				for (size_t b = 0; b < boxes.size() && now < 0; b++)
				{
					now = Holds(boxes[b], p, 0.0) ? (int)b : -1;
				}

				if (now < 0)
				{
					continue;   // in no box: the gap is bridged from the last box to the next
				}

				if (last >= 0 && now != last)
				{
					Box const &a = boxes[(size_t)last], &b = boxes[(size_t)now];
					bool const plain = !Climbing(a) && !Climbing(b) && (!way.alien || Touch(a, b));

					links.Add((size_t)last, (size_t)now, plain ? 0 : kLinkAlien);
				}

				last = now;
			}
		}

		// 4. parts that don't reach each other linked where their boxes touch
		std::vector<size_t> part;

		JoinParts(boxes, &links);

		// 4c. ISLANDS: a part still apart (a crate's top, a ledge a marine stands on that no way passes over) linked aliens only to the nearest box of another part, the nearest pair
		// first, while one is within kAutoPlotIslandReach (an alien climbs or jumps there; a marine never goes, so the floor route is not changed)
		for (size_t islands = 0;; islands++)
		{
			size_t const count = Parts(boxes.size(), links, &part);

			if (count <= 1)
			{
				break;
			}

			size_t bestA = 0, bestB = 0;
			double bestGap = 1e300;

			// the nearest pair of boxes that hold no exit first (an exit box is an end, never a way through: JoinParts), an exit's box only when no other pair is in reach.
			// And first of all no floor box joined to a climbing box that is no wall (A FLOOR BOX CLIMBS ONLY INTO A WALL, below; Drake's gate on b99de13d, med02: a raised
			// floor no way joins, boxes 29 and 30, linked to the 15.6 m ceiling band 0.55 m over it, its centre 10 m off, where floor boxes were 1.2 m beside it: 36 S)
			for (int pass = 0; pass < 3 && bestGap > kAutoPlotIslandReach; pass++)
			{
				bestGap = 1e300;

				for (size_t a = 0; a < boxes.size(); a++)
				{
					for (size_t b = a + 1; b < boxes.size(); b++)
					{
						bool const exitBox = boxes[a].exitOwn >= 0 || boxes[b].exitOwn >= 0;
						bool const upNoWall = Climbing(boxes[a]) != Climbing(boxes[b]) && !WallLike(Climbing(boxes[a]) ? boxes[a] : boxes[b]);
						bool const allowed = pass == 2 || (!exitBox && (pass == 1 || !upNoWall));
						double const g = part[a] != part[b] && allowed ? Gap(boxes[a], boxes[b]) : 1e300;

						if (g < bestGap)
						{
							bestGap = g;
							bestA = a;
							bestB = b;
						}
					}
				}
			}

			if (bestGap > kAutoPlotIslandReach)
			{
				break;
			}

			links.Add(bestA, bestB, kLinkAlien);

			if (islands == 0)
			{
				plan.notes.push_back("parts no way joins linked aliens only to their nearest box (the first " + std::to_string((long long)bestGap) + " mm away)");
			}
		}

		// 5. down to the stock game's most: a box made one only with one of its own kind (floor with floor, wall or ceiling with wall or ceiling: the stock price), the highways
		// first (fewer of them only makes the climb cheaper), those that link or touch first, the merge that grows the boxes least
		while ((int32_t)boxes.size() > kAutoPlotMostBoxes)
		{
			size_t bestA = 0, bestB = 0;
			double bestGrowth = 1e300;
			int bestRank = 4;

			for (size_t a = 0; a < boxes.size(); a++)
			{
				for (size_t b = a + 1; b < boxes.size(); b++)
				{
					// one of its own SURFACE (11-20: Object11's wall and ceiling made one, 5 by 24 by 6.4 m, its centre in the open: a floor alien heading for it stands);
					// a door box keeps its shape (THE LANES)
					if (boxes[a].surface != boxes[b].surface || boxes[a].door >= 0 || boxes[b].door >= 0)
					{
						continue;
					}

					bool const near = links.flags.count(std::make_pair(a, b)) || Touch(boxes[a], boxes[b]);
					int const rank = (Climbing(boxes[a]) ? 0 : 2) + (near ? 0 : 1);
					double const growth = Volume(Union(boxes[a], boxes[b])) - Volume(boxes[a]) - Volume(boxes[b]);

					if (rank < bestRank || (rank == bestRank && growth < bestGrowth))
					{
						bestA = a;
						bestB = b;
						bestGrowth = growth;
						bestRank = rank;
					}
				}
			}

			if (bestRank == 4)
			{
				break;   // no two of one kind (it can't be, over 99): the checker refuses it
			}

			Merge(&boxes, &links, bestA, bestB);
		}

		KeepTheLanes(&boxes, exitPlaces, &tooLow, &exitsInClimbing);   // a merge grows a box

		// 6. each centre, in the file's numbers and by the game's own test: inside its own box only, clear of the others by half an alien's width; else inside its own and
		// outside the others. A box with no such place (the others cover it: a 3 m wall between a floor's box and a ceiling's, each reaching 1.5 m) is taken out, or if that
		// would lose an exit or split the network, made one with the box that holds its best place; then every centre is picked again, until each is its own box's only:
		// Frost's checker refuses a network with a centre in another box ("box 1: its centre is in box 3 too", Genshd1's genent01). Each round takes one box away, so it ends
		int32_t const clear = (int32_t)std::min(ceil(kAutoPlotHalfWidth / 2.0 / scale), 1000000.0);
		std::vector<FileBox> files;
		std::vector<std::vector<int32_t> > centres;
		size_t joined = 0, takenOut = 0, trimmedForDoors = 0;

		size_t rounds = 0;

		for (bool again = true; again; rounds++)
		{
			again = false;
			KeepTheLanes(&boxes, exitPlaces, &tooLow, &exitsInClimbing);   // a join grows a box

			bool const mayChange = rounds < 4000;   // a trim takes no box away: past this many rounds the boxes stay as they are (the checker says what is shared)
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

				if (b.inside && !b.hints.empty())
				{
					candidates = b.hints;   // inside the wall or the ceiling only (the manual's rule)
				}
				else if (b.aimFirst)
				{
					candidates = b.hints;   // the aim past the doorway first, then the box's middle
					candidates.push_back(std::vector<double>{ (b.lo[0] + b.hi[0]) / 2.0, (b.lo[1] + b.hi[1]) / 2.0, (b.lo[2] + b.hi[2]) / 2.0 });
				}
				else
				{
					candidates.push_back(std::vector<double>{ (b.lo[0] + b.hi[0]) / 2.0, (b.lo[1] + b.hi[1]) / 2.0, (b.lo[2] + b.hi[2]) / 2.0 });
					candidates.insert(candidates.end(), b.hints.begin(), b.hints.end());
				}

				for (double fx : { 0.25, 0.5, 0.75 })
				{
					if (b.inside && !b.hints.empty())
					{
						break;
					}

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

					// a climb station's corner (THE DOORWAY START, the first hint) wins whenever no other box holds it: beside the door box it is always near one
					bool const corner = b.cornerFor >= 0 && &c == &candidates[0];
					int const tier = nearOthers == 0 || (corner && inOthers == 0) ? 0 : (inOthers == 0 ? 1 : 2);

					if (tier < bestTier || (tier == bestTier && tier == 2 && inOthers < bestOthers))
					{
						bestTier = tier;
						bestOthers = inOthers;
						best = at;
					}
				}

				if (bestTier != 2 || !mayChange)
				{
					plan.centresShared += bestTier == 2 ? 1 : 0;
					centres[i] = best;
					continue;
				}

				// THE LANES: a door box keeps its shape; the climbing box over its centre is cut back to its face (or given up for it), the floor boxes were cut under it
				if (b.door >= 0)
				{
					size_t over = i;

					for (size_t j = 0; j < boxes.size() && over == i; j++)
					{
						over = j != i && boxes[j].door < 0 && Climbing(boxes[j]) && GameHolds(files[j], best.data(), scale, 0) ? j : over;
					}

					if (over == i)
					{
						plan.centresShared++;
						centres[i] = best;
						continue;
					}

					if (!TrimOff(&boxes[over], boxes[i], scale))
					{
						Absorb(&boxes, &links, i, over);
					}

					trimmedForDoors++;
					again = true;
					break;
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
						other = other || (j != i && (Climbing(boxes[i]) || !Climbing(boxes[j])) && Holds(boxes[j], p, 0.0));   // a floor box's exits held by another floor box
					}

					exitsElsewhere = exitsElsewhere && (other || !Holds(boxes[i], p, 0.0));
				}

				// and a floor box only when every floor cell in it is in another box too (the floor under the player stays covered: the stock dropout)
				bool cellsElsewhere = true;

				for (size_t c = 0; c < cells.size() && cellsElsewhere && !Climbing(boxes[i]); c++)
				{
					double const q[3] = { cells[c][0], cells[c][1], cells[c][2] };

					if (!Holds(boxes[i], q, 0.0))
					{
						continue;
					}

					bool other = false;

					for (size_t j = 0; j < boxes.size() && !other; j++)
					{
						other = j != i && Holds(boxes[j], q, 0.0);
					}

					cellsElsewhere = other;
				}

				// a climbing box another climbing box covers is never taken out (THE LANES: on Genshd1's Ops01 the take-outs left 3 of a 39 m ceiling's boxes, the floor
				// route keeping the network whole): it is made one with that box, below, so the ceiling stays covered; one only floor boxes cover goes as before
				bool climbCover = false;

				for (size_t j = 0; j < boxes.size() && Climbing(boxes[i]) && !climbCover; j++)
				{
					climbCover = j != i && Climbing(boxes[j]) && boxes[j].door < 0 && GameHolds(files[j], best.data(), scale, 0);
				}

				if (exitsElsewhere && cellsElsewhere && !climbCover)
				{
					std::vector<size_t> partsNow;
					size_t const before = Parts(boxes.size(), links, &partsNow);
					size_t const plainBefore = Parts(boxes.size(), links, &partsNow, true);   // the floor route: a marine's, plain links only
					std::vector<Box> const keptBoxes = boxes;
					Links const keptLinks = links;

					TakeOut(&boxes, &links, i);
					JoinParts(boxes, &links);

					if (Parts(boxes.size(), links, &partsNow) <= before && Parts(boxes.size(), links, &partsNow, true) <= plainBefore)
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
					// a climbing box only with one of its own surface: a wall's and a ceiling's made one reached from above Ops01's ceiling down to the floor's top, across the
					// whole corridor, its centre low in the wall (Genshd1, 11-19)
					bool const sameKind = Climbing(boxes[j]) == Climbing(boxes[i]) && (!Climbing(boxes[i]) || boxes[j].surface == boxes[i].surface);

					if (j != i && boxes[j].door < 0 && sameKind && GameHolds(files[j], best.data(), scale, 0) && Overlap(boxes[i], boxes[j]) > most)
					{
						most = Overlap(boxes[i], boxes[j]);
						holder = j;
					}
				}

				// only a door box holds it, or a climbing box of the other surface: this climbing box cut back to that box's face (or given up for it)
				size_t doorOver = i;

				for (size_t j = 0; j < boxes.size() && holder == i && doorOver == i && Climbing(boxes[i]); j++)
				{
					doorOver = (boxes[j].door >= 0 || Climbing(boxes[j])) && j != i && GameHolds(files[j], best.data(), scale, 0) ? j : doorOver;
				}

				if (doorOver != i)
				{
					if (!TrimOff(&boxes[i], boxes[doorOver], scale))
					{
						Absorb(&boxes, &links, doorOver, i);
					}

					trimmedForDoors++;
					again = true;
					break;
				}

				if (holder == i)
				{
					plan.centresShared++;   // only a box of the other kind holds it (never made one: the stock price); kept, and the checker says so
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

		if (trimmedForDoors)
		{
			plan.notes.push_back(std::to_string((long long)trimmedForDoors) + " climbing boxes cut back to a door box's face, or given up for it (the door box's centre its own)");
		}

		if (tooLow)
		{
			plan.notes.push_back(std::to_string((long long)tooLow) + " floor boxes under a ceiling too low to keep " + std::to_string((long long)kAutoPlotHangClear) +
				" mm under it (a hanging alien's low corners may be in them)");
		}

		// the exits' own boxes LAST in the list: the game takes the last box that holds the target (SweepWaypoints, bh_waypoint.cpp:436-440), so an exit point a tile holds
		// too is still looked for in its own box, whose centre is past the doorway (Genshd1's Ops01: exit 2's point in tile 15 too, that tile's centre short of the doorway)
		{
			std::vector<size_t> from(boxes.size());
			std::vector<Box> movedBoxes;
			std::vector<FileBox> movedFiles;
			std::vector<std::vector<int32_t> > movedCentres;

			for (int pass = 0; pass < 2; pass++)
			{
				for (size_t i = 0; i < boxes.size(); i++)
				{
					if ((boxes[i].exitOwn >= 0) == (pass == 1))
					{
						from[i] = movedBoxes.size();
						movedBoxes.push_back(boxes[i]);
						movedFiles.push_back(files[i]);
						movedCentres.push_back(centres[i]);
					}
				}
			}

			boxes.swap(movedBoxes);
			files.swap(movedFiles);
			centres.swap(movedCentres);
			links.Remap(from);
		}

		// THE DOORWAY START (autoplot_plan.h), every box in its place and with its centre: each floor box at a lane's doorway linked aliens only to the ceiling's box over it
		// (else the door box) and not to the exit box; the exit box linked instead to the floor boxes those link to past the doorway (plain: the marine's way), and to no
		// climbing box but the door box
		size_t doorStarts = 0;

		plan.doorFloors.assign(exits.size(), std::vector<int32_t>());

		std::set<std::pair<size_t, size_t> > doorClimbs;   // each doorway floor box's chosen climb (floor, its climb): the climb-into-walls pass keeps those, and only those

		for (size_t e = 0; e < exits.size() && e < doors.size(); e++)
		{
			if (plan.doorWords[e].compare(0, 4, "lane") != 0 || !doors[e].throughFound)
			{
				continue;
			}

			int doorAt = -1, exitAt = -1;

			for (size_t b = 0; b < boxes.size(); b++)
			{
				doorAt = boxes[b].door == (int)e ? (int)b : doorAt;
				exitAt = boxes[b].exitOwn == (int)e ? (int)b : exitAt;
			}

			if (doorAt < 0 || exitAt < 0)
			{
				continue;
			}

			AutoPlotDoor const &d = doors[e];
			int const t = fabs(d.through[0]) >= fabs(d.through[1]) ? 0 : 2, c = t == 0 ? 2 : 0;
			double const s = d.through[t == 0 ? 0 : 1] < 0.0 ? -1.0 : 1.0;
			double const m = Held(d.middle[t]);
			Box const door = boxes[(size_t)doorAt], own = boxes[(size_t)exitAt];
			std::vector<size_t> first;

			for (size_t b = 0; b < boxes.size(); b++)
			{
				Box const &f = boxes[b];
				double const middleT = (f.lo[t] + f.hi[t]) / 2.0, nearFace = s > 0.0 ? f.hi[t] : f.lo[t];

				// at the doorway: this side, its near face within reach of the line and across the door box; or linked to the exit box (a merged rectangle's face falls on a
				// square's edge, further in: Genshd1's chain, Ops01's 22 at 979)
				bool const near = s * (m - nearFace) <= kAutoPlotDoorFloorReach && f.hi[c] >= door.lo[c] && f.lo[c] <= door.hi[c] && f.hi[1] >= own.lo[1] && f.lo[1] <= own.hi[1];

				if ((int)b != exitAt && !Climbing(f) && f.exitOwn < 0 && s * (middleT - m) < 0.0 && (near || links.Has(b, (size_t)exitAt)))
				{
					first.push_back(b);
				}
			}

			// A WALL BOX MADE for this doorway (Drake's rule, the chain gate 11-20): in the wall the doorway is in, beside its opening, from the strip's top up into the lane's box
			// the door box links, its centre IN that wall; linked aliens only to the lane's box (the strip is linked to it below). Strip -> wall -> lane is then as short as exit ->
			// door -> lane, and the strip, earlier in the list, is the start. Not made where its centre would be in another box, or it would hold an exit point or another centre
			int madeWall = -1;
			auto const MakeDoorWall = [&](size_t strip) -> size_t
			{
				size_t lane = boxes.size();
				double most = -1.0;

				for (auto const &one : links.flags)
				{
					size_t const other = (int)one.first.first == doorAt ? one.first.second : ((int)one.first.second == doorAt ? one.first.first : (size_t)-1);

					if (other != (size_t)-1 && boxes[other].door < 0 && boxes[other].surface == AutoPlotSurface::Ceiling && boxes[other].hasCeiling &&
						Overlap(door, boxes[other]) > most)
					{
						most = Overlap(door, boxes[other]);
						lane = other;
					}
				}

				if (lane == boxes.size() || boxes[lane].hi[1] - 100.0 >= boxes[strip].lo[1] + 100.0)
				{
					return boxes.size();
				}

				for (int side = 0; side < 2; side++)
				{
					Box wall;
					double const from = side == 0 ? door.hi[c] + 50.0 : door.lo[c] - 50.0 - 2.0 * kAutoPlotHalfWidth;

					wall.lo[t] = std::min(m - s * kAutoPlotClimbReach, m + s * kAutoPlotInto);
					wall.hi[t] = std::max(m - s * kAutoPlotClimbReach, m + s * kAutoPlotInto);
					wall.lo[c] = Held(from);
					wall.hi[c] = Held(from + 2.0 * kAutoPlotHalfWidth);
					wall.lo[1] = boxes[lane].hi[1] - 100.0;    // up into the lane's box
					wall.hi[1] = boxes[strip].lo[1] + 100.0;   // down onto the strip's top
					wall.surface = AutoPlotSurface::Wall;
					wall.inside = true;

					if (wall.hi[c] <= boxes[strip].lo[c] || wall.lo[c] >= boxes[strip].hi[c])
					{
						continue;   // not over the strip
					}

					double place[3];

					place[t] = m + s * kAutoPlotInto / 2.0;   // in the wall
					place[c] = (wall.lo[c] + wall.hi[c]) / 2.0;
					place[1] = (wall.lo[1] + wall.hi[1]) / 2.0;
					wall.hints.push_back(std::vector<double>{ place[0], place[1], place[2] });

					FileBox const file = FileOf(wall, scale);
					std::vector<int32_t> const centre = { File(place[0], scale), File(place[1], scale), File(place[2], scale) };
					bool good = GameHolds(file, centre.data(), scale, 0);

					for (size_t j = 0; j < boxes.size() && good; j++)
					{
						good = !GameHolds(files[j], centre.data(), scale, 0) && (j >= centres.size() || centres[j].size() != 3 || !GameHolds(file, centres[j].data(), scale, 0));
					}

					for (std::vector<double> const &x : exitPlaces)
					{
						int32_t const at[3] = { File(x[0], scale), File(x[1], scale), File(x[2], scale) };

						good = good && !GameHolds(file, at, scale, 0);
					}

					if (!good)
					{
						continue;
					}

					boxes.push_back(wall);
					files.push_back(file);
					centres.push_back(centre);
					links.Add(boxes.size() - 1, lane, kLinkAlien);
					madeWall = (int)(boxes.size() - 1);
					plan.wallsMade++;
					return boxes.size() - 1;
				}

				return boxes.size();
			};

			for (size_t const f : first)
			{
				// AT the doorway's line (Drake's review: a corner reaches 450, so a near face further in leaves a band only the exit box holds): its near face grown to the
				// line, unless another box's centre would then be inside it
				{
					Box grown = boxes[f];

					(s > 0.0 ? grown.hi[t] : grown.lo[t]) = s > 0.0 ? std::max(grown.hi[t], m) : std::min(grown.lo[t], m);

					FileBox const file = FileOf(grown, scale);
					bool clear = true;

					for (size_t j = 0; j < boxes.size() && clear; j++)
					{
						clear = j == f || j >= centres.size() || centres[j].size() != 3 || !GameHolds(file, centres[j].data(), scale, 0);
					}

					if (clear && f < files.size())
					{
						boxes[f] = grown;
						files[f] = file;   // step 8 writes the file's numbers made in step 6: the grown box's too
					}
				}

				// what it climbs to: the ceiling's box with its centre in the station's corner (Drake's review); without a station, a wall's box within reach (a climb by a
				// wall). Nothing else (Bishop's gate on the chain 11-20: the ceiling's box over it, its centre over the open floor, and the alien stands under it, its speed 0;
				// 24 such places in Ops01): with neither it does not climb, and walks
				// (the corner's tile only when its centre came out in the corner: Drake, the chain gate 11-20: a long lane's one centre mid-room, 22 stalls in opscen02); with neither,
				// A WALL BOX MADE beside the doorway's opening (Drake's rule: Ops01's side doorways had no wall within 3 m, their strips walked, the exit box was the start)
				size_t corner = boxes.size(), wallBox = boxes.size();
				double nearestWall = kAutoPlotDoorWallReach;

				for (size_t k = 0; k < boxes.size(); k++)
				{
					Box const &o = boxes[k];
					bool inCorner = o.cornerFor == (int)e && k < centres.size() && centres[k].size() == 3;

					for (int a = 0; a < 3 && inCorner; a++)
					{
						inCorner = abs(centres[k][(size_t)a] - File(o.cornerAt[a], scale)) <= 1;
					}

					corner = inCorner ? k : corner;

					if (WallLike(o) && o.door < 0 && Gap(o, boxes[f]) <= nearestWall)
					{
						nearestWall = Gap(o, boxes[f]);
						wallBox = k;
					}
				}

				if (corner == boxes.size() && wallBox == boxes.size())
				{
					wallBox = madeWall >= 0 ? (size_t)madeWall : MakeDoorWall(f);
				}

				// in a TALL room a wall within reach before a corner far up (a ways' wall box stands on the floor; the corner's wall may begin over an opening: Object11 11-20)
				bool const cornerHigh = corner < boxes.size() && corner < centres.size() && centres[corner].size() == 3 &&
					(boxes[f].hi[1] - kAutoPlotInto) - centres[corner][1] * scale > kAutoPlotTallClimb;
				size_t const over = corner < boxes.size() && !(cornerHigh && wallBox < boxes.size()) ? corner : wallBox;

				if (over == boxes.size())
				{
					continue;
				}

				links.Remove(f, over);
				links.Add(f, over, kLinkAlien);
				plan.doorFloors[e].push_back((int32_t)f);
				doorClimbs.insert(std::make_pair(f, over));

				if (!links.Has(f, (size_t)exitAt))
				{
					continue;
				}

				// past it: its plain floor neighbours not at the doorway
				std::vector<size_t> past;

				for (auto const &one : links.flags)
				{
					size_t const other = one.first.first == f ? one.first.second : (one.first.second == f ? one.first.first : (size_t)-1);

					if (other != (size_t)-1 && (int)other != exitAt && one.second == 0 && !Climbing(boxes[other]) &&
						std::find(first.begin(), first.end(), other) == first.end())
					{
						past.push_back(other);
					}
				}

				for (size_t const p : past)
				{
					links.Add((size_t)exitAt, p, 0);
				}

				if (!past.empty())
				{
					links.Remove(f, (size_t)exitAt);
				}
			}

			// the exit box to the ceiling only through its door box
			std::vector<size_t> climbs;

			for (auto const &one : links.flags)
			{
				size_t const other = (int)one.first.first == exitAt ? one.first.second : ((int)one.first.second == exitAt ? one.first.first : (size_t)-1);

				if (other != (size_t)-1 && (int)other != doorAt && Climbing(boxes[other]))
				{
					climbs.push_back(other);
				}
			}

			for (size_t const k : climbs)
			{
				links.Remove((size_t)exitAt, k);
			}

			links.Add((size_t)exitAt, (size_t)doorAt, kLinkAlien);

			// the door box linked to ONE ceiling's box, the one its doorway opens under (the most of it in the door box; Drake's rule from the chain gate 11-20: Ops01's side door
			// boxes 72 and 75 each linked two tiles, so their exit boxes were two boxes from either, fewer than the doorway floor box: the exit box the route's start, 23 back places)
			{
				size_t keep = boxes.size();
				double most = 0.0;
				std::vector<size_t> tilesOfDoor;

				for (auto const &one : links.flags)
				{
					size_t const other = (int)one.first.first == doorAt ? one.first.second : ((int)one.first.second == doorAt ? one.first.first : (size_t)-1);

					if (other != (size_t)-1 && boxes[other].door < 0 && boxes[other].surface == AutoPlotSurface::Ceiling && boxes[other].hasCeiling)
					{
						tilesOfDoor.push_back(other);

						double const o = Overlap(boxes[(size_t)doorAt], boxes[other]);

						if (keep == boxes.size() || o > most)
						{
							most = o;
							keep = other;
						}
					}
				}

				for (size_t const k : tilesOfDoor)
				{
					if (k != keep)
					{
						links.Remove((size_t)doorAt, k);
					}
				}

				// THE LINTEL (Drake's whole-level gate 11-20: 21 lanes stopped at the doorway: a doorway's top lower than the room's ceiling by more than a climbing alien's reach,
				// its door box linked to no ceiling's box): a wall box made on the wall over the opening, this side, from the door box's top up into the nearest ceiling's box over
				// the doorway's inner side, its centre in that wall; door box -> lintel -> ceiling linked aliens only. Not made where its centre would be in another box or it would
				// hold an exit point or another centre
				if (keep == boxes.size())
				{
					size_t room = boxes.size();
					double nearestUp = 1e300;
					double const inner = m - s * kAutoPlotLaneBack;   // the door box's end inside this AI module

					for (size_t k = 0; k < boxes.size(); k++)
					{
						Box const &o = boxes[k];

						if (o.door < 0 && o.surface == AutoPlotSurface::Ceiling && o.hasCeiling && o.hi[1] < door.lo[1] && o.lo[t] <= inner && o.hi[t] >= inner &&
							o.lo[c] < door.hi[c] && o.hi[c] > door.lo[c] && door.lo[1] - o.hi[1] < nearestUp)
						{
							nearestUp = door.lo[1] - o.hi[1];
							room = k;
						}
					}

					if (room < boxes.size())
					{
						Box lintel;

						lintel.lo[t] = std::min(m - s * kAutoPlotClimbReach, m + s * kAutoPlotInto);
						lintel.hi[t] = std::max(m - s * kAutoPlotClimbReach, m + s * kAutoPlotInto);
						lintel.lo[c] = door.lo[c];
						lintel.hi[c] = door.hi[c];
						lintel.lo[1] = boxes[room].hi[1] - 100.0;   // up into the room's ceiling box
						lintel.hi[1] = door.lo[1] + 100.0;          // down into the door box
						lintel.surface = AutoPlotSurface::Wall;
						lintel.inside = true;

						double place[3];

						place[t] = m + s * kAutoPlotInto / 2.0;
						place[c] = (door.lo[c] + door.hi[c]) / 2.0;
						place[1] = (lintel.lo[1] + lintel.hi[1]) / 2.0;
						lintel.hints.push_back(std::vector<double>{ place[0], place[1], place[2] });

						FileBox const file = FileOf(lintel, scale);
						std::vector<int32_t> const centre = { File(place[0], scale), File(place[1], scale), File(place[2], scale) };
						bool good = lintel.lo[1] < lintel.hi[1] && GameHolds(file, centre.data(), scale, 0);

						for (size_t j = 0; j < boxes.size() && good; j++)
						{
							good = !GameHolds(files[j], centre.data(), scale, 0) && (j >= centres.size() || centres[j].size() != 3 || !GameHolds(file, centres[j].data(), scale, 0));
						}

						for (std::vector<double> const &x : exitPlaces)
						{
							int32_t const at[3] = { File(x[0], scale), File(x[1], scale), File(x[2], scale) };

							good = good && !GameHolds(file, at, scale, 0);
						}

						if (good)
						{
							boxes.push_back(lintel);
							files.push_back(file);
							centres.push_back(centre);
							links.Add((size_t)doorAt, boxes.size() - 1, kLinkAlien);
							links.Add(boxes.size() - 1, room, kLinkAlien);
							plan.wallsMade++;
						}
					}
				}
			}

			// and a floor box linked to the exit box never climbs (Bishop's gate on the chain 11-20: Ops01's ceiling 5 -> wall 1 -> floor 22 -> exit 78 tied the lane's 5 -> 6 ->
			// door 73 -> 78, and the tie went to the floor): its aliens-only links to climbing boxes taken out
			std::vector<size_t> beside;

			for (auto const &one : links.flags)
			{
				size_t const other = (int)one.first.first == exitAt ? one.first.second : ((int)one.first.second == exitAt ? one.first.first : (size_t)-1);

				if (other != (size_t)-1 && !Climbing(boxes[other]))
				{
					beside.push_back(other);
				}
			}

			for (size_t const p : beside)
			{
				std::vector<size_t> up;

				for (auto const &one : links.flags)
				{
					size_t const other = one.first.first == p ? one.first.second : (one.first.second == p ? one.first.first : (size_t)-1);

					// but another exit's own box keeps the link to its own door box: its lane's end (Drake's gate on 164672c2: in a door module 0.4 m deep the two exit boxes
					// are linked, so the later exit took the earlier one's door link away; medoor02's door box 5 linked no box holding exit 0, and its lanes went over the floor)
					bool const ownDoor = other != (size_t)-1 && boxes[p].exitOwn >= 0 && boxes[other].door == boxes[p].exitOwn;

					if (other != (size_t)-1 && (int)other != exitAt && Climbing(boxes[other]) && !ownDoor)
					{
						up.push_back(other);
					}
				}

				for (size_t const k : up)
				{
					links.Remove(p, k);
				}
			}

			// THE PAIR ALWAYS JOINS (Drake's gate on 7b4be05e: 5 AI modules refused, an exit box and its door box an island holding the exit): the exit box keeps a plain link
			// to the floor (its nearest floor box when every one was taken), and the door box a link to the climbing network (its nearest climbing box, a ceiling's first)
			{
				bool floorLink = false, doorOut = false;

				for (auto const &one : links.flags)
				{
					size_t const x = (int)one.first.first == exitAt ? one.first.second : ((int)one.first.second == exitAt ? one.first.first : (size_t)-1);
					size_t const y = (int)one.first.first == doorAt ? one.first.second : ((int)one.first.second == doorAt ? one.first.first : (size_t)-1);

					floorLink = floorLink || (x != (size_t)-1 && one.second == 0 && !Climbing(boxes[x]));
					doorOut = doorOut || (y != (size_t)-1 && (int)y != exitAt);
				}

				size_t nearestFloor = boxes.size(), nearestClimb = boxes.size();
				double floorGap = 1e300, climbGap = 1e300;

				for (size_t k = 0; k < boxes.size(); k++)
				{
					if ((int)k == exitAt || (int)k == doorAt)
					{
						continue;
					}

					double const toExit = Gap(boxes[(size_t)exitAt], boxes[k]), toDoor = Gap(boxes[(size_t)doorAt], boxes[k]) - (boxes[k].surface == AutoPlotSurface::Ceiling ? 1.0 : 0.0);

					if (!Climbing(boxes[k]) && toExit < floorGap)
					{
						floorGap = toExit;
						nearestFloor = k;
					}

					if (Climbing(boxes[k]) && boxes[k].door < 0 && toDoor < climbGap)
					{
						climbGap = toDoor;
						nearestClimb = k;
					}
				}

				if (!floorLink && nearestFloor < boxes.size())
				{
					links.Add((size_t)exitAt, nearestFloor, 0);
				}

				if (!doorOut && nearestClimb < boxes.size())
				{
					links.Add((size_t)doorAt, nearestClimb, kLinkAlien);
				}
			}

			doorStarts += first.empty() ? 0 : 1;
		}

		// A FLOOR BOX CLIMBS ONLY INTO A WALL (Drake's whole-level gate on 535ad485: 4,066 stall places, the most where a floor box was linked aliens only straight to a ceiling's
		// box or to a big climbing box the cap made of a wall and a ceiling: Object11's floor 21 -> ceiling 3, 24 m long and 2 m up; its floor 13 -> box 1, 5 by 24 m by 6.4 m;
		// heading for that centre the alien walks under it and stands). Such a link is taken out unless the climbing box is thin as a wall's (its centre in a wall face), the
		// floor box is a doorway's (its climb is the station's corner), or taking it out would part the network
		{
			auto const wallLike = [](Box const &b) { return WallLike(b); };

			std::vector<std::pair<size_t, size_t> > upLinks;

			for (auto const &one : links.flags)
			{
				size_t const a = one.first.first, b = one.first.second;
				size_t const floorOne = !Climbing(boxes[a]) ? a : b, climbOne = floorOne == a ? b : a;

				// a doorway floor box keeps its CHOSEN climb only (Drake's gate on a1787093: Object11's box 40, a doorway floor box of another exit, kept an old 0x4 link to
				// box 20, 5.9 by 12.6 m: every link of a doorway floor box was spared)
				if (one.second == kLinkAlien && Climbing(boxes[a]) != Climbing(boxes[b]) && boxes[floorOne].exitOwn < 0 && boxes[climbOne].door < 0 &&
					!wallLike(boxes[climbOne]) && !doorClimbs.count(std::make_pair(floorOne, climbOne)))
				{
					upLinks.push_back(std::make_pair(floorOne, climbOne));
				}
			}

			std::vector<size_t> partNow;
			size_t const partsBefore = Parts(boxes.size(), links, &partNow);
			size_t takenUp = 0;

			for (std::pair<size_t, size_t> const &l : upLinks)
			{
				links.Remove(l.first, l.second);

				if (Parts(boxes.size(), links, &partNow) > partsBefore)
				{
					// the only join: through a wall-like box of the climbing box's part within reach of the floor box instead, when there is one (Drake's gate on 3ff518cb:
					// Object11's floor 40 -> box 20, 5.9 by 12.6 m, its centre 7.5 m up with no wall on the walk, kept as the only join: 419 stall places)
					std::vector<size_t> partOf;

					Parts(boxes.size(), links, &partOf);

					size_t viaWall = boxes.size();
					double nearestGap = kAutoPlotDoorWallReach;

					for (size_t k = 0; k < boxes.size(); k++)
					{
						if (k != l.second && partOf[k] == partOf[l.second] && wallLike(boxes[k]) && boxes[k].door < 0 && Gap(boxes[k], boxes[l.first]) <= nearestGap)
						{
							nearestGap = Gap(boxes[k], boxes[l.first]);
							viaWall = k;
						}
					}

					if (viaWall < boxes.size())
					{
						links.Add(l.first, viaWall, kLinkAlien);
						takenUp++;
					}
					else
					{
						links.Add(l.first, l.second, kLinkAlien);   // the only join, and no wall to go by: kept
					}
				}
				else
				{
					takenUp++;
				}
			}

			if (takenUp)
			{
				plan.notes.push_back(std::to_string((long long)takenUp) + " links from the floor straight up to a ceiling's box taken out (a floor box climbs only into a wall)");
			}
		}

		// CLIMBING BOXES THAT TOUCH ARE LINKED (Vasquez 11-20, module 05: its ceiling in two parts joined only through the floor, the wall strip 9 between exits 4 and 6
		// overlapping the main ceiling's box 1 with no link, because the parts were joined where boxes touch only while the network was apart): every two climbing
		// boxes that touch, aliens only, a door box left out (it keeps its ONE ceiling's box: Drake's rule, the doorway pass)
		{
			size_t joinedUp = 0;

			for (size_t a = 0; a < boxes.size(); a++)
			{
				for (size_t b = a + 1; b < boxes.size(); b++)
				{
					if (Climbing(boxes[a]) && Climbing(boxes[b]) && boxes[a].door < 0 && boxes[b].door < 0 && !links.Has(a, b) && Touch(boxes[a], boxes[b]))
					{
						links.Add(a, b, kLinkAlien);
						joinedUp++;
					}
				}
			}

			if (joinedUp)
			{
				plan.notes.push_back(std::to_string((long long)joinedUp) + " links between climbing boxes that touch");
			}
		}

		// AN EXIT TO OPEN SKY STILL ENDS THE LANES THAT COME TO IT (Drake's gate on 0f3b6bcf: ENT-01's exit 0 and RAMP-01's exit 2 have no ceiling over their point nor past
		// their doorway, so no door box; their exit boxes linked only to the floor, and every lane toward them along the module's ceiling went over the floor: 3 of the 11
		// ends): its exit box linked aliens only to the nearest ceiling's box within kAutoPlotDoorWallReach, so the lane goes along the ceiling to that doorway and ends there
		{
			size_t skyEnds = 0;

			for (size_t e = 0; e < exits.size() && e < plan.doorWords.size(); e++)
			{
				if (plan.doorWords[e].rfind("no lane: no ceiling over the exit point", 0) != 0)
				{
					continue;
				}

				size_t exitAt = boxes.size(), tile = boxes.size();
				double nearest = kAutoPlotDoorWallReach;

				for (size_t b = 0; b < boxes.size(); b++)
				{
					exitAt = boxes[b].exitOwn == (int)e ? b : exitAt;
				}

				for (size_t b = 0; b < boxes.size() && exitAt < boxes.size(); b++)
				{
					if (boxes[b].surface == AutoPlotSurface::Ceiling && boxes[b].hasCeiling && boxes[b].door < 0 && Gap(boxes[b], boxes[exitAt]) <= nearest)
					{
						nearest = Gap(boxes[b], boxes[exitAt]);
						tile = b;
					}
				}

				if (tile < boxes.size() && !links.Has(exitAt, tile))
				{
					links.Add(exitAt, tile, kLinkAlien);
					skyEnds++;
				}
			}

			if (skyEnds)
			{
				plan.notes.push_back(std::to_string((long long)skyEnds) + " exits to open sky joined to the ceiling's box over their doorway (the lanes end there)");
			}
		}

		if (doorStarts)
		{
			plan.notes.push_back(std::to_string((long long)doorStarts) + " doorways start their lane at the floor box inside them (linked aliens only to the ceiling over it)");
		}

		// THE LANES, each exit's at the end: its door box, its own box, and a climbing box still holding its exit point is no lane (an alien there is in its target's box)
		plan.doorBox.assign(exits.size(), -1);
		plan.exitBox.assign(exits.size(), -1);

		for (size_t i = 0; i < boxes.size(); i++)
		{
			if (boxes[i].door >= 0 && (size_t)boxes[i].door < exits.size())
			{
				plan.doorBox[(size_t)boxes[i].door] = (int32_t)i;
			}

			if (boxes[i].exitOwn >= 0 && (size_t)boxes[i].exitOwn < exits.size())
			{
				plan.exitBox[(size_t)boxes[i].exitOwn] = (int32_t)i;
			}
		}

		for (size_t e = 0; e < exits.size(); e++)
		{
			if (exits[e].size() != 3 || plan.doorWords[e].compare(0, 4, "lane") != 0)
			{
				continue;
			}

			double const p[3] = { Held(exits[e][0]), Held(exits[e][1]), Held(exits[e][2]) };

			for (Box const &b : boxes)
			{
				if (Climbing(b) && Holds(b, p, 0.0))
				{
					plan.doorWords[e] = "no lane: a climbing box holds the exit point (an alien there would be in its target's box)";
				}
			}
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

		for (Box const &b : boxes)
		{
			plan.floorBoxes += Climbing(b) ? 0 : 1;
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
			plan.climbing.push_back(Climbing(boxes[i]));
		}

		// the floor cells inside a box of the list, by the game's own test on the file's numbers (an NPC or its target in no box drops the waypoints: bh_waypoint.cpp:110-131)
		plan.cells = cells.size();

		for (std::vector<double> const &c : cells)
		{
			int32_t const at[3] = { File(c[0], scale), File(c[1], scale), File(c[2], scale) };
			bool in = false;

			for (size_t i = 0; i < files.size() && !in; i++)
			{
				in = GameHolds(files[i], at, scale, 0);
			}

			plan.cellsCovered += in ? 1 : 0;
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
