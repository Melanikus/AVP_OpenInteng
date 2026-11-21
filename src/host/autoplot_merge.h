// autoplot_merge.h: THE FLOOR MERGE of the consolidated auto-plot (the owner 2026-11-19 after playing v2d: "the game is not dealing well with discreet squares"; Dietrich's
// planner calls it from Tiles(), Bishop's chain-A test): the planner's floor tiles made into fewer, bigger boxes, each one whole rectangle of the grid's squares. Pure: no
// level, no game. Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULE (Dietrich's, "keep it simple, no optimal cover"): the tiles are taken in the order of their squares (z, then x, then their top, then their number); each one not
// in a group yet is a SEED: its group grows along x while the next square holds a free tile that fits, up to the most squares a side, then along z one whole row at a time
// while every square of the row holds one; then it is emitted and its tiles marked. A tile FITS the seed when its lo[1] and its hi[1] are each within `rise` of the seed's
// (a step higher than that is not merged across). A square gives at most ONE member to a group (the free fitting tile nearest the seed's top; the lowest number on a tie),
// so two tiles of one square (a floor over a floor) are never in one group. A side is at most mostSide / squareEdge squares: the caller's (Dietrich 11-20: Ops01's 39 m
// floor one box along, not five); kAutoPlotMergeMostSquares only keeps that number whole (no level is 2.4 km across). The side never costs work: a group's tiles are
// each looked at once, and only its one row that is not whole is looked at in vain.
// FLOOR PRICE (the stock route search takes the fewest boxes): the caller keeps mostSide well under its ceiling lanes' length (Dietrich: 4000 against 12 m), so a floor
// alien's route up a station still has fewer boxes than the floor's.
// HOSTILE INPUT: more than kAutoPlotMergeMostTiles tiles, a square holding more than kAutoPlotMergeMostPerSquare (floors one over another), an edge or a side that is not a finite number above 0 (or a side shorter than an edge), or a rise that is not
// a finite number of 0 or more (0: the same heights only), and nothing is
// merged: every tile is a group of its own. A tile with a height that is not a number fits nothing. The work is the sort and one look per square of each group tried.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	// A floor tile: its box (the world's millimetres, y down) and its square's x and z index on the planner's grid
	struct AutoPlotTileRect
	{
		double lo[3], hi[3];
		int64_t square[2];
	};

	size_t const kAutoPlotMergeMostTiles = 200000;   // the most tiles the planner makes; more are not merged
	int const kAutoPlotMergeMostSquares = 4096;      // the most squares along a side, whatever mostSide says: a guard on the numbers, never a limit a level meets
	size_t const kAutoPlotMergeMostPerSquare = 64;   // the most tiles (floors one over another) a square may hold; a square with more and nothing is merged

	// The tiles in groups of tile indexes (into `tiles`): every tile in exactly one group, each group's first member its seed, its members filling one rectangle of squares
	// (one member each), groups in the order they were made
	std::vector<std::vector<size_t> > AutoPlot_MergeTiles(std::vector<AutoPlotTileRect> const &tiles, double squareEdge, double mostSide, double rise);
}
