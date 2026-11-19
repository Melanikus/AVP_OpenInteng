// autoplot_grid.h: the editor's OWN alien pathfinder of the open level, for Waypoint mode's Auto-plot alien ways (Bishop's design,
// AvP-docs/engine/inteng/reassess/alien-autoplot-design.md: AvPx's "floor is lava" pathfinder, written as STOCK waypoints; the owner's addition, to honour the
// artists who set the paths by hand). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE GRID: AvPx's own (avpx/3dc/avp npc_grid.h on alien_ways_map.h), built from the level the editor shows. The game builds its own copy at every level's start only
// while FloorIsLavaAsked is set; the editor sets it to 0 once at start-up (32-bit memory: a grid is 28 MB on Derelict, 47 MB on Genshd1), and builds THIS one only when
// asked (AutoPlotGrid_Start), on a thread of its own (about 2 s on Derelict).
//
// THE WAY: a search from one place to another, spread over the grid's own Work steps (StartSearch + Work, never the one-shot Search, whose way's ends go stale:
// npc_grid.cpp:1739-1754), as an ALIEN (floor is lava: the floor costs kAutoPlotAlienFloorPrice times as much, so it keeps to walls and ceilings) or as a MARINE (the
// ground route the other kinds walk). Each point says which surface the way is on from it and the game module it lies in.
//
// THE MODULES: the game's module number (MODULE::m_index), -1 for none: map polygon -> the listing's place (AlienWaysMap::MapNumberOf, turned round once) -> the
// listing's ranges (AlienWaysLevel_List: one range of the listing for each module); a still object's polygons, in no module's range, get theirs from the game's
// ModuleFromPosition at the polygon's middle, worked out ONCE when the grid starts (on the main thread), so a search only reads a table.
//
// THREADS: AutoPlotGrid_Start lists on the CALLING thread, which must be the editor's main one (the listing reads the game's scene), then builds on a worker.
// AutoPlotGrid_Way and AutoPlotGrid_StandSpots take a lock inside: one caller at a time, from any one thread (the grid keeps one search's state); they answer
// false / empty until the grid is Ready.
//
// HOSTILE LEVELS: the listing, the map and the grid each refuse what they can't trust and keep their own caps (alien_ways_level.h, npc_grid.h NpcGrid_UsualLimits);
// a search looks at no more than kAutoPlotMostLooks cells; StandSpots gives at most `most` (held to kAutoPlotMostSpots).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;

namespace host
{
	int const kAutoPlotAlienFloorPrice = 10;     // the design's alien search: a metre of floor costs as much as 10 of wall or ceiling
	size_t const kAutoPlotMostLooks = 200000;    // the cells one search may look at (the design's)
	size_t const kAutoPlotMostSpots = 64;        // the most stand spots one call gives

	struct AutoPlotPoint
	{
		enum class Surface : uint8_t
		{
			Floor,
			Wall,
			Ceiling,
		};

		int32_t place[3] = { 0, 0, 0 };      // the world's millimetres, y down
		Surface surface = Surface::Floor;    // the surface the way is on from this point to the next
		int32_t facing[3] = { 0, 0, 0 };     // that surface's facing, 65536 long; all 0 when it isn't known
		int32_t module = -1;                 // the GAME's module number (MODULE::m_index), -1 for none
	};

	// The grid of the level open now, built on a thread. Call on the editor's main thread, with the level open in the view. A second call drops the grid it had
	// and builds again. False (with why) when it can't start (no level, the listing refused it).
	bool AutoPlotGrid_Start(OpenLevelFile *open, std::string *why);

	// Whether the grid is ready to search: false while it builds (why: "building"), or when it failed (why: the grid's or the map's reason)
	bool AutoPlotGrid_Ready(std::string *why);

	// One way from `from` to `to` (the world's millimetres): alien, the alien search (floor is lava); else the marine's ground route. Once the grid is ready.
	// The points in order, start to goal, each with its surface and module. False (with why) when there is no way, it looked too far, or the grid isn't ready.
	bool AutoPlotGrid_Way(int32_t const from[3], int32_t const to[3], bool alien, std::vector<AutoPlotPoint> *way, std::string *why);

	// Places in a game module where a marine can stand (cells of the grid a marine stands on), spread out, at most `most`: the interior goals
	std::vector<AutoPlotPoint> AutoPlotGrid_StandSpots(int32_t module, size_t most);

	// THE HARNESS'S PROBE (autoplot_grid_game.cpp; a stock level's proof, no menu): with AutoPlotProbe=1 in the editor's settings, once the level is open the grid is
	// started, and when it is ready an alien and a marine search run between stand spots of the first two game modules that have them; the state line says how it went
	// (autoplotgrid: "done 0|1 ready 0|1 ms <build>", autoplotways: "alien found 0|1 points <n> ms <t> from <module> to <module>; floor ..."). One line each in editor_main.cpp.
	void AutoPlotGrid_ProbeLoad(char const *settingsFile);
	void AutoPlotGrid_ProbeFrame(OpenLevelFile *open);
	void AutoPlotGrid_ProbeStateLine();

	// THE PURE PARTS, for the tests (autoplot_grid.cpp uses them):
	// The game module of the listing's place `listed`: the module whose range holds it (its number is its place in `modules`), -1 when none does (a still object's
	// polygon: the caller asks the game's ModuleFromPosition)
	struct AutoPlotRange
	{
		size_t first, end;   // the listing's places [first, end) of one module
	};

	int32_t AutoPlot_ModuleOfListed(std::vector<AutoPlotRange> const &modules, size_t listed);

	// At most `most` of the places, spread out: the first, then each time the one farthest from those already picked (farthest-point picking). Ties: the earlier.
	std::vector<size_t> AutoPlot_Spread(std::vector<AutoPlotPoint> const &places, size_t most);

	// A facing in 32767ths (as the level and the grid keep it) made 65536 long; all 0 stays all 0
	void AutoPlot_Facing65536(int16_t const in[3], int32_t out[3]);
}
