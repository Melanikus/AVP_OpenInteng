// autoplot_grid_feed.h: what autoplot_grid.cpp builds from, for its two callers only: the editor's listing of the open level (autoplot_grid_game.cpp, which reads
// the game's scene) and the tests (a level made of polygons, no game). Not for anything else: the API is autoplot_grid.h. Written 2026-11-18 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "alien_ways_map.h"

namespace host
{
	// Builds the grid from these polygons (the game's numbers), each with its game module (listedModule: one for each polygon, -1 for none), on the build's own
	// thread. Whatever grid there was is stopped and let go first. False (with why) when the two lists don't match.
	bool AutoPlotGrid_StartFrom(std::vector<AlienWaysPolygon> &&polygons, std::vector<int32_t> &&listedModule, std::string *why);

	// Says why the last start couldn't build (the listing refused the level, say), for AutoPlotGrid_Ready; empty clears it
	void AutoPlotGrid_StartRefused(std::string const &why);

	// So a test builds on its own thread, with no second one. Nothing else calls this.
	void AutoPlotGrid_BuildOnTheCallersThreadForATest(bool callers);
}
