// host_autoplot_merge_test.cpp: THE FLOOR MERGE (src/host/autoplot_merge.h AutoPlot_MergeTiles) on made grids: Dietrich's cases (an 8 x 39 m corridor of 600 mm squares,
// an L-shaped room, a step of 400 not merged across, two floors in one square never merged, a single tile), and every result checked whole: each tile in exactly one
// group, each group one full rectangle of distinct squares no longer a side than mostSide, its members within `rise` of its seed. Then the hostile inputs (too many tiles,
// edges and sides that are not numbers, a height that is not a number) and the work on the planner's most tiles. Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "autoplot_merge.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static double const kEdge = 600.0;

// a floor tile on square (x, z), its top `top` and its bottom `bottom` (y down: top < bottom)
static host::AutoPlotTileRect Tile(int64_t x, int64_t z, double top = -1000.0, double bottom = 0.0)
{
	host::AutoPlotTileRect t;

	t.lo[0] = (double)x * kEdge, t.lo[1] = top, t.lo[2] = (double)z * kEdge;
	t.hi[0] = t.lo[0] + kEdge, t.hi[1] = bottom, t.hi[2] = t.lo[2] + kEdge;
	t.square[0] = x, t.square[1] = z;
	return t;
}

// the whole result is right: "" or the first thing wrong
static std::string Wrong(std::vector<host::AutoPlotTileRect> const &tiles, std::vector<std::vector<size_t> > const &groups, double edge, double mostSide, double rise)
{
	std::vector<int> seen(tiles.size(), 0);

	for (size_t g = 0; g < groups.size(); g++)
	{
		std::vector<size_t> const &group = groups[g];

		if (group.empty())
		{
			return "group " + std::to_string(g) + " is empty";
		}

		std::set<std::pair<int64_t, int64_t> > squares;
		int64_t lo[2] = { INT64_MAX, INT64_MAX }, hi[2] = { INT64_MIN, INT64_MIN };
		host::AutoPlotTileRect const &seed = tiles[group[0] < tiles.size() ? group[0] : 0];

		for (size_t const i : group)
		{
			if (i >= tiles.size())
			{
				return "group " + std::to_string(g) + " names tile " + std::to_string(i) + ", past the end";
			}

			seen[i]++;

			host::AutoPlotTileRect const &t = tiles[i];

			if (!squares.insert(std::make_pair(t.square[0], t.square[1])).second)
			{
				return "group " + std::to_string(g) + " has two tiles of one square";
			}

			for (int a = 0; a < 2; a++)
			{
				lo[a] = std::min(lo[a], t.square[a]), hi[a] = std::max(hi[a], t.square[a]);
			}

			if (group.size() > 1 && !(fabs(t.lo[1] - seed.lo[1]) <= rise && fabs(t.hi[1] - seed.hi[1]) <= rise))
			{
				return "group " + std::to_string(g) + "'s tile " + std::to_string(i) + " is not within the rise of its seed";
			}
		}

		int64_t const w = hi[0] - lo[0] + 1, h = hi[1] - lo[1] + 1;

		if ((int64_t)group.size() != w * h)
		{
			return "group " + std::to_string(g) + " is not a full rectangle (" + std::to_string(group.size()) + " tiles in " + std::to_string(w) + " x " + std::to_string(h) + ")";
		}

		if (group.size() > 1 && ((double)w * edge > mostSide || (double)h * edge > mostSide))
		{
			return "group " + std::to_string(g) + " is " + std::to_string(w) + " x " + std::to_string(h) + " squares, more than mostSide a side";
		}
	}

	for (size_t i = 0; i < tiles.size(); i++)
	{
		if (seen[i] != 1)
		{
			return "tile " + std::to_string(i) + " is in " + std::to_string(seen[i]) + " groups";
		}
	}

	return "";
}

int main()
{
	// 1. Dietrich's corridor: 39 m by 8 m of 600 mm squares (65 x 13), mostSide 4000 (6 squares, 3600 mm), rise 300
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t z = 0; z < 13; z++)
		{
			for (int64_t x = 0; x < 65; x++)
			{
				tiles.push_back(Tile(x, z));
			}
		}

		std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0);
		std::string const wrong = Wrong(tiles, groups, kEdge, 4000.0, 300.0);

		Check(wrong.empty(), "the corridor: every tile once, full rectangles, at most 4000 a side" + (wrong.empty() ? std::string() : ": " + wrong));
		Check(groups.size() == 33, "the corridor (65 x 13 squares): 33 boxes = ceil(39 / 3.6) x ceil(8 / 3.6), was " + std::to_string(groups.size()));
		Check(!groups.empty() && groups[0].size() == 36, "its first box is 6 x 6 squares (3.6 m a side)");
	}

	// 2. an L-shaped room: 10 x 10 squares less its 6 x 6 corner (x 4..9, z 4..9)
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t z = 0; z < 10; z++)
		{
			for (int64_t x = 0; x < 10; x++)
			{
				if (!(x >= 4 && z >= 4))
				{
					tiles.push_back(Tile(x, z));
				}
			}
		}

		std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0);
		std::string const wrong = Wrong(tiles, groups, kEdge, 4000.0, 300.0);

		Check(wrong.empty(), "the L-shaped room: every tile once, full rectangles (none over the missing corner)" + (wrong.empty() ? std::string() : ": " + wrong));
		Check(groups.size() <= 6 && groups.size() >= 3, "the L-shaped room's 64 tiles: a few boxes (" + std::to_string(groups.size()) + ")");
	}

	// 3. a step of 400 halfway along a 20 x 6 corridor: never merged across (rise 300); a step of 200 is
	{
		for (double const step : { 400.0, 200.0 })
		{
			std::vector<host::AutoPlotTileRect> tiles;

			for (int64_t z = 0; z < 6; z++)
			{
				for (int64_t x = 0; x < 20; x++)
				{
					tiles.push_back(x < 10 ? Tile(x, z) : Tile(x, z, -1000.0 - step, -step));
				}
			}

			std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0);
			std::string const wrong = Wrong(tiles, groups, kEdge, 4000.0, 300.0);
			bool across = false;

			for (std::vector<size_t> const &group : groups)
			{
				bool low = false, high = false;

				for (size_t const i : group)
				{
					(tiles[i].square[0] < 10 ? low : high) = true;
				}

				across = across || (low && high);
			}

			Check(wrong.empty(), "a step of " + std::to_string((int)step) + ": every tile once, full rectangles, each within the rise of its seed" + (wrong.empty() ? std::string() : ": " + wrong));
			Check(step > 300.0 ? !across : across, step > 300.0 ? "a step of 400: no box across it" : "a step of 200 (under the rise): boxes across it");
		}
	}

	// 4. two floors in one square (a floor over a floor): never one group, also when the two are at one height
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t z = 0; z < 3; z++)
		{
			for (int64_t x = 0; x < 3; x++)
			{
				tiles.push_back(Tile(x, z));
			}
		}

		tiles.push_back(Tile(1, 1, -4000.0, -3000.0));   // an upper floor over the middle square
		tiles.push_back(Tile(2, 2));                      // and a second tile of the same height on the corner square

		std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0);
		std::string const wrong = Wrong(tiles, groups, kEdge, 4000.0, 300.0);

		Check(wrong.empty(), "two floors in one square: every tile once, never two of one square in a group" + (wrong.empty() ? std::string() : ": " + wrong));
		Check(groups.size() >= 3, "the 9 squares + the upper floor + the second corner tile: at least 3 boxes (" + std::to_string(groups.size()) + ")");
	}

	// 5. a single tile; no tiles
	{
		std::vector<host::AutoPlotTileRect> const one(1, Tile(7, -3));
		std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(one, kEdge, 4000.0, 300.0);

		Check(groups.size() == 1 && groups[0].size() == 1 && groups[0][0] == 0, "a single tile: one group of it");
		Check(host::AutoPlot_MergeTiles(std::vector<host::AutoPlotTileRect>(), kEdge, 4000.0, 300.0).empty(), "no tiles: no groups");
	}

	// 6. the sides: mostSide under an edge = nothing merged; an edge of 2300 with mostSide 4000 = one square a side
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t x = 0; x < 8; x++)
		{
			tiles.push_back(Tile(x, 0));
		}

		Check(host::AutoPlot_MergeTiles(tiles, kEdge, 500.0, 300.0).size() == 8, "mostSide 500 under the 600 edge: every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, 2300.0, 4000.0, 300.0).size() == 8, "edge 2300, mostSide 4000: one square a side, every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, kEdge, 1300.0, 300.0).size() == 4, "edge 600, mostSide 1300: two squares a side, 8 in a row = 4 boxes");
		Check(host::AutoPlot_MergeTiles(tiles, kEdge, 1.0e12, 300.0).size() == 1, "a huge mostSide: 8 in a row = 1 box");
		// THE SIDE IS mostSide's (Dietrich 11-20: opscen03's 17 m floor and Ops01's 39 m were 2 and 5 boxes along at 16 squares, 9.6 m): Ops01's 39 m of 600 mm squares
		// (65) in a row is one box when mostSide allows it; kAutoPlotMergeMostSquares only keeps the numbers whole
		std::vector<host::AutoPlotTileRect> long_(65, Tile(0, 0));
		for (int64_t x = 0; x < 65; x++)
		{
			long_[(size_t)x] = Tile(x, 0);
		}
		Check(host::AutoPlot_MergeTiles(long_, kEdge, 40000.0, 300.0).size() == 1, "mostSide 40 m: Ops01's 39 m (65 squares) in a row = 1 box");
		Check(host::AutoPlot_MergeTiles(long_, kEdge, 4000.0, 300.0).size() == 11, "mostSide 4000 (6 squares): 65 in a row = 10 of 6 + 5");
		Check(host::AutoPlot_MergeTiles(long_, kEdge, 1.0e12, 300.0).size() == 1, "a huge mostSide: 65 in a row = 1 box");
		Check(host::kAutoPlotMergeMostSquares >= 4096, "the guard is no limit a level meets (4096 squares of 600 mm: 2.4 km)");
	}

	// 7. HOSTILE: edges, sides and rises that are not numbers or not above 0; a tile whose height is not a number; too many tiles
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t x = 0; x < 6; x++)
		{
			tiles.push_back(Tile(x, 0));
		}

		double const nan = NAN;
		Check(host::AutoPlot_MergeTiles(tiles, 0.0, 4000.0, 300.0).size() == 6, "edge 0: every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, -600.0, 4000.0, 300.0).size() == 6, "a negative edge: every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, nan, 4000.0, 300.0).size() == 6, "edge not a number: every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, kEdge, nan, 300.0).size() == 6, "mostSide not a number: every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, nan).size() == 6, "rise not a number: every tile alone");
		Check(host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, -1.0).size() == 6, "a negative rise: every tile alone");

		tiles[3].lo[1] = nan;
		std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0);
		bool alone = false;
		for (std::vector<size_t> const &group : groups)
		{
			alone = alone || (group.size() == 1 && group[0] == 3);
		}
		Check(alone && groups.size() == 3 && Wrong(tiles, groups, kEdge, 4000.0, 1.0e30).empty(), "a tile whose top is not a number: alone, and splits the row (0-2, 3, 4-5)");

		std::vector<host::AutoPlotTileRect> extreme = { Tile(INT64_MAX, 0), Tile(INT64_MAX - 1, 0), Tile(INT64_MIN, INT64_MAX) };
		std::vector<std::vector<size_t> > const edgeOfRange = host::AutoPlot_MergeTiles(extreme, kEdge, 4000.0, 300.0);
		Check(Wrong(extreme, edgeOfRange, kEdge, 4000.0, 300.0).empty() && edgeOfRange.size() == 2, "squares at the ends of the number range: no wrap-around (INT64_MAX - 1 and INT64_MAX merged, the third alone)");

		std::vector<host::AutoPlotTileRect> many;
		for (int64_t i = 0; i < (int64_t)host::kAutoPlotMergeMostTiles + 1; i++)
		{
			many.push_back(Tile(i % 500, i / 500));
		}
		Check(host::AutoPlot_MergeTiles(many, kEdge, 4000.0, 300.0).size() == many.size(), "more than the planner's 200,000 tiles: every tile alone");

		// floors stacked over one square past any level's (each look at a square walks its tiles: 100,000 over each of two squares were 10^10 looks, the worker hung)
		std::vector<host::AutoPlotTileRect> stacked;
		for (int64_t k = 0; k < (int64_t)host::kAutoPlotMergeMostPerSquare + 1; k++)
		{
			stacked.push_back(Tile(0, 0, -1000.0 - 3000.0 * (double)k, -3000.0 * (double)k));
			stacked.push_back(Tile(1, 0, -1000.0 - 3000.0 * (double)k, -3000.0 * (double)k));
		}
		Check(host::AutoPlot_MergeTiles(stacked, kEdge, 4000.0, 300.0).size() == stacked.size(), "more than " + std::to_string(host::kAutoPlotMergeMostPerSquare) +
			" floors over one square: every tile alone");
		stacked.resize(2 * (size_t)host::kAutoPlotMergeMostPerSquare);
		Check(host::AutoPlot_MergeTiles(stacked, kEdge, 4000.0, 300.0).size() == (size_t)host::kAutoPlotMergeMostPerSquare, "exactly " +
			std::to_string(host::kAutoPlotMergeMostPerSquare) + " floors over each of two squares: merged, each floor one box of two squares");
	}

	// 8. THE WORK: the planner's most tiles (500 x 400 squares) merged at once, checked whole
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t z = 0; z < 400; z++)
		{
			for (int64_t x = 0; x < 500; x++)
			{
				tiles.push_back(Tile(x, z, -1000.0 + (double)((x / 50) % 2) * 400.0, (double)((x / 50) % 2) * 400.0));   // a step of 400 every 50 squares
			}
		}

		auto const start = std::chrono::steady_clock::now();
		std::vector<std::vector<size_t> > const groups = host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0);
		double const ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		std::string const wrong = Wrong(tiles, groups, kEdge, 4000.0, 300.0);

		Check(wrong.empty(), "200,000 tiles with a step every 50 squares: every tile once, full rectangles, within the rise" + (wrong.empty() ? std::string() : ": " + wrong));
		Check(groups.size() < 7000, "200,000 tiles: under 7,000 boxes (" + std::to_string(groups.size()) + ")");
		Check(ms < 2000.0, "200,000 tiles merged in under 2 s (" + std::to_string((int)ms) + " ms)");
	}

	// 9. the same tiles twice: the same groups
	{
		std::vector<host::AutoPlotTileRect> tiles;

		for (int64_t z = 0; z < 7; z++)
		{
			for (int64_t x = 0; x < 11; x++)
			{
				tiles.push_back(Tile(x, z));
			}
		}

		Check(host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0) == host::AutoPlot_MergeTiles(tiles, kEdge, 4000.0, 300.0), "the same tiles twice: the same groups");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
