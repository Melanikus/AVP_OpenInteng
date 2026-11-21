// autoplot_merge.cpp: see autoplot_merge.h. Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autoplot_merge.h"

#include <math.h>

#include <algorithm>
#include <unordered_map>

namespace host
{
	namespace
	{
		// a square's key in the look-up (two squares may share one: every look checks the tile's own square)
		uint64_t KeyOf(int64_t x, int64_t z)
		{
			return ((uint64_t)x * 0x9E3779B97F4A7C15ull) ^ (uint64_t)z;
		}

		// a top for the order: one that is not a number goes last (a sort needs every pair ordered)
		double TopForOrder(double top)
		{
			return top == top ? top : INFINITY;
		}

		// the tile fits the seed: its top and its bottom each within the rise of the seed's (a height that is not a number fits nothing)
		bool Fits(AutoPlotTileRect const &t, AutoPlotTileRect const &seed, double rise)
		{
			return fabs(t.lo[1] - seed.lo[1]) <= rise && fabs(t.hi[1] - seed.hi[1]) <= rise;
		}

		// one past a square along an axis, false at the end of the number range
		bool Next(int64_t from, int64_t steps, int64_t *out)
		{
			if (from > INT64_MAX - steps)
			{
				return false;
			}

			*out = from + steps;
			return true;
		}
	}

	std::vector<std::vector<size_t> > AutoPlot_MergeTiles(std::vector<AutoPlotTileRect> const &tiles, double squareEdge, double mostSide, double rise)
	{
		std::vector<std::vector<size_t> > groups;
		size_t const n = tiles.size();

		// the most squares along a side; 1 (nothing merged) for a hostile call
		int64_t side = 1;

		if (n <= kAutoPlotMergeMostTiles && isfinite(squareEdge) && squareEdge > 0.0 && isfinite(mostSide) && mostSide >= squareEdge && isfinite(rise) && rise >= 0.0)
		{
			double const squares = floor(mostSide / squareEdge);

			side = squares >= (double)kAutoPlotMergeMostSquares ? kAutoPlotMergeMostSquares : (int64_t)squares;
		}

		if (side <= 1)
		{
			groups.reserve(n);

			for (size_t i = 0; i < n; i++)
			{
				groups.push_back(std::vector<size_t>(1, i));
			}

			return groups;
		}

		// each square's tiles
		std::unordered_map<uint64_t, std::vector<size_t> > at;

		at.reserve(n);

		for (size_t i = 0; i < n; i++)
		{
			std::vector<size_t> &square = at[KeyOf(tiles[i].square[0], tiles[i].square[1])];

			square.push_back(i);

			if (square.size() > kAutoPlotMergeMostPerSquare)
			{
				// more floors over one square than any level has (hostile): nothing merged (each look at a square walks its tiles; the work stays bounded)
				groups.reserve(n);

				for (size_t k = 0; k < n; k++)
				{
					groups.push_back(std::vector<size_t>(1, k));
				}

				return groups;
			}
		}

		// the seeds' order: z, then x, then the top, then the number
		std::vector<size_t> order(n);

		for (size_t i = 0; i < n; i++)
		{
			order[i] = i;
		}

		std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
			AutoPlotTileRect const &ta = tiles[a], &tb = tiles[b];

			if (ta.square[1] != tb.square[1])
			{
				return ta.square[1] < tb.square[1];
			}

			if (ta.square[0] != tb.square[0])
			{
				return ta.square[0] < tb.square[0];
			}

			double const topA = TopForOrder(ta.lo[1]), topB = TopForOrder(tb.lo[1]);

			if (topA != topB)
			{
				return topA < topB;
			}

			return a < b;
		});

		std::vector<char> used(n, 0);

		// the free tile of square (x, z) that fits the seed, nearest its top (the lowest number on a tie); -1 when there is none
		auto const take = [&](int64_t x, int64_t z, AutoPlotTileRect const &seed) -> long long {
			auto const found = at.find(KeyOf(x, z));

			if (found == at.end())
			{
				return -1;
			}

			long long best = -1;
			double bestGap = 0.0;

			for (size_t const i : found->second)
			{
				AutoPlotTileRect const &t = tiles[i];

				if (used[i] || t.square[0] != x || t.square[1] != z || !Fits(t, seed, rise))
				{
					continue;
				}

				double const gap = fabs(t.lo[1] - seed.lo[1]);

				if (best < 0 || gap < bestGap)
				{
					best = (long long)i, bestGap = gap;
				}
			}

			return best;
		};

		for (size_t const s : order)
		{
			if (used[s])
			{
				continue;
			}

			AutoPlotTileRect const &seed = tiles[s];
			int64_t const x0 = seed.square[0], z0 = seed.square[1];
			std::vector<size_t> group(1, s);

			used[s] = 1;   // its own square gives the seed, never another tile of it

			// along x while the next square has a free tile that fits
			int64_t width = 1;

			for (; width < side; width++)
			{
				int64_t x;
				long long const t = Next(x0, width, &x) ? take(x, z0, seed) : -1;

				if (t < 0)
				{
					break;
				}

				group.push_back((size_t)t);
				used[(size_t)t] = 1;
			}

			// then along z, one whole row at a time
			for (int64_t height = 1; height < side; height++)
			{
				int64_t z;

				if (!Next(z0, height, &z))
				{
					break;
				}

				std::vector<size_t> row;

				for (int64_t c = 0; c < width; c++)
				{
					int64_t x;
					long long const t = Next(x0, c, &x) ? take(x, z, seed) : -1;

					if (t < 0)
					{
						break;
					}

					row.push_back((size_t)t);
				}

				if ((int64_t)row.size() != width)
				{
					break;
				}

				for (size_t const t : row)
				{
					group.push_back(t);
					used[t] = 1;
				}
			}

			groups.push_back(group);
		}

		return groups;
	}
}
