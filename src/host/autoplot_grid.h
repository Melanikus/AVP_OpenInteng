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
	int const kAutoPlotSnapReach = 4000;         // a way's start or goal with no cell within the grid's own reach (1.5 m) is moved to the nearest cell within this many mm: an
	                                             // exit point lies mid-doorway, ~2 m off the floor (Colony 11-18); the way's first and last points are the cells it was moved to

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
	// looks, if given, is set to the cells this search looked at (the grid's own count, LastSearchLooks; 0 for one refused before it began): THE PLOT'S WORK, counted
	// per AI module for the report and its budget (Dietrich 2026-11-23, Bishop 2's order: the caps multiply, so the work is measured, then bounded)
	bool AutoPlotGrid_Way(int32_t const from[3], int32_t const to[3], bool alien, std::vector<AutoPlotPoint> *way, std::string *why, size_t *looks = nullptr);

	// Places in a game module where a marine can stand (cells of the grid a marine stands on), spread out, at most `most`: the interior goals
	std::vector<AutoPlotPoint> AutoPlotGrid_StandSpots(int32_t module, size_t most);

	// EVERY cell a marine stands on in a game module (what StandSpots spreads from), for covering a module's walkable floor with boxes (Bishop's addendum, 11-18: stock
	// drops the waypoints when the NPC or its target is in no box). Each: the floor point under the cell's middle (the world's mm, y down), surface Floor, the module.
	// *cellEdge: a cell's side in mm (one cell covers cellEdge x cellEdge of floor). False (with why) when the grid isn't ready, the module has no such cells, or it has
	// more than kAutoPlotMostFloorCells (a hostile level can't make the caller tile for ever). One caller at a time, as Way.
	size_t const kAutoPlotMostFloorCells = 200000;
	bool AutoPlotGrid_FloorCells(int32_t module, std::vector<AutoPlotPoint> *cells, int32_t *cellEdge, std::string *why);

	// THE CRAWL CELLS (Drake 11-20, Frost's ask: a duct too low for a marine, Genshd1's shft10 / shft12 / shft18 at 1.9 m): every floor cell of the game module a CROUCHED
	// ALIEN stands on, as FloorCells gives them (the same places, surface, module, cell edge, cap and refusals): the grid's alien cells (kNpcGridClings: 900 of room out from
	// the surface, its crouching top, over the 3 x 3 round the cell) on a FLOOR sheet (the grid's own kind of the sheet). A marine's floor is a crawl floor too, so where a
	// marine stands these hold FloorCells' cells and more
	bool AutoPlotGrid_CrawlCells(int32_t module, std::vector<AutoPlotPoint> *cells, int32_t *cellEdge, std::string *why);

	// THE FLOORS IN A BOX (Dietrich's ask 11-20, Object11's platform: a lane box's bottom is cut over the floors a standing alien is on, and FloorCells gives only one
	// game module's): EVERY cell a marine stands on whose floor point (the cell's middle on its sheet, the world's mm, y down) is inside the box lo..hi (each end in), of ANY
	// module or still object; each with surface Floor and its game module (MODULE::m_index, -1 for none), in no set order. The look goes by the grid's columns of such cells
	// (made once a grid), so a box costs its own area, never the level's. True with none (cells empty); false (with why) when the grid isn't ready, or more than
	// kAutoPlotMostFloorCells are in the box (cells empty). A box with an end past the other on any axis holds none. One caller at a time, as Way. Added 2026-11-20 by Melanikus
	// (Drake).
	bool AutoPlotGrid_FloorsIn(int32_t const lo[3], int32_t const hi[3], std::vector<AutoPlotPoint> *cells, std::string *why);

	// THE CEILING OVER A PLACE (the world's mm), for the auto-plot's ceiling LANES (Bishop 11-19, the owner's "continuous lanes"): the nearest cell an alien fits on that is a
	// ceiling's, straight over `at` (its middle within a cell across), at most `most` mm (held to kAutoPlotMostCeilingLook) above it: a doorway's top over its exit point (Genshd1's
	// exit points are at floor level, under 3.9 m and 5.8 m ceilings). found: that cell's middle (on the ceiling), surface Ceiling, facing down (out of it, y is down), its module.
	// False (with why) when the grid isn't ready or no ceiling is that near. One caller at a time, as Way. Added 2026-11-19 by Melanikus (Dietrich).
	// 64 m (Drake 11-20, Dietrich's ask for the owner's "Stop only at exits modules with open sky": a tall ceiling is still a ceiling; Genshd1's go to 15.6 m). The ask costs
	// the same at any height: one look in the grid's COLUMNS of ceiling cells (made once a grid), no walk up in half-cell steps (each a pass over every sheet)
	int const kAutoPlotMostCeilingLook = 64000;
	bool AutoPlotGrid_CeilingOver(int32_t const at[3], int32_t most, AutoPlotPoint *found, std::string *why);

	// THE ROOF OVER A PLACE (Dietrich's ask 11-20: his open-sky verdicts must be the gate's; CeilingOver counts only the cells an alien clings to, and a door's polygons get
	// none but their floor, nor does a roof with no room to cling): the nearest POLYGON of any module, doors too, that the line straight up from `at` (the world's mm, y
	// down) crosses at least 300 mm above it and at most `most` above it (held to kAutoPlotMostCeilingLook), FACING DOWN (Bishop's ruling 11-20, on the game's collision:
	// a polygon is touched only from its front, dynamics.cpp:4486-4487, so an up-facing floor's top seen from below holds no hanging alien); a still object's polygons are not
	// roofs. The very test of the lane gate's ceilingOverAny (AvP-docs/tools/drake/lanes/lanes.cpp: a module polygon over the place, "s < y - 300"), from the polygons the
	// grid was built from (a copy of the level's floors, roofs and slopes, kept with the grid: walls, which no line straight up crosses, are not kept). found: the
	// crossing (at's x and z, the polygon's height there), surface Ceiling, facing down, the polygon's game module. False (with why) when the grid isn't ready or no
	// such polygon is that near. One caller at a time, as Way. Added 2026-11-20 by Melanikus (Drake).
	// onlyModules (when given; Dietrich 11-20, a door module's OWN roof: the cling cells found a neighbour's ceiling over its floor first): the nearest polygon of one of
	// those game modules only, the rest passed over
	bool AutoPlotGrid_RoofOver(int32_t const at[3], int32_t most, AutoPlotPoint *found, std::string *why, std::vector<int32_t> const *onlyModules = nullptr);

	// THE ROOF A HANGING ALIEN OF THESE MODULES USES (Bishop's own-space ruling 11-20; Dietrich's office under new03's ceiling): the nearest down-facing roof of ANY
	// module over `at` (as RoofOver), and true only when the hanging position, 900 under it at at's x and z, lies inside the box of one of `modules` (game modules): an
	// alien runs on the network of the module its position is in (pvisible.cpp:431-445 finds it from the position each frame; bh_ais.cpp:1226 takes that module's AI
	// module's waypoints). A roof whose hanging place is in another module's space (water under top's ceiling) is no roof of these: false, never a higher one. found: as
	// RoofOver's (module: the roof polygon's own module). Added 2026-11-20 by Melanikus (Drake).
	bool AutoPlotGrid_RoofHungIn(int32_t const at[3], int32_t most, std::vector<int32_t> const &modules, AutoPlotPoint *found, std::string *why);

	// A game module's box, as kept at Start: the extent of its polygons in the listing (the world's mm, y down). False when the module has none, or no grid was started
	bool AutoPlotGrid_ModuleBox(int32_t gameModule, int32_t lo[3], int32_t hi[3]);

	// THE CLIMB STATION OF AN EXIT (the owner's lanes, 11-19; Vasquez's drop audit S3: a far alien lands UPRIGHT ON THE FLOOR at the exit point it heads for, and a floor
	// alien only climbs where its route pushes it into a wall: one heading for a centre straight above stands still under it, NPCSetVelocity drops the up-axis part). From
	// the cell an alien stands on at `exitPoint` (the next module's entry point, where it lands), over the floor on the LANDING side of the doorway (`through`: the way
	// through, from the module it leaves into the one it lands in, any length, y not used; floor more than 100 behind the exit point along it is the other module's), to
	// the nearest wall it reaches within `most` mm (as the crow flies from the exit point), SIDE walls first (facing within 60 degrees of square to the way through), then
	// any other; then straight up that wall's own cells, every one an alien clings to, to the ceiling it joins (a cell it can't cling to, or no ceiling at the top, and that
	// wall is passed over for the next). way: the station's alien way: the landing cell, the floor cell at the wall, the wall's foot, the wall's top, the ceiling cell it
	// reaches (each point's surface the one it goes on from there, its facing out of that surface, 65536 long, its game module). top: that ceiling cell's middle. False
	// (with why) when the grid isn't ready, no cell is near the exit point, no wall is within most, or no wall reaches a ceiling. One caller at a time, as Way.
	// Added 2026-11-19 by Melanikus (Drake).
	bool AutoPlotGrid_ClimbStation(int32_t const exitPoint[3], int32_t const through[3], int32_t most, std::vector<AutoPlotPoint> *way, int32_t top[3], std::string *why);

	// The grid's size once it is ready: its cells and the bytes it keeps (false, both 0, before)
	bool AutoPlotGrid_Size(size_t *cells, size_t *bytesKept);

	// THE HARNESS'S PROBE (autoplot_grid_game.cpp; a stock level's proof, no menu): with AutoPlotProbe=1 in the editor's settings, once the level is open the grid is
	// started, and when it is ready an alien and a marine search run between stand spots of the first two game modules that have them; the state line says how it went
	// (autoplotgrid: "done 0|1 ready 0|1 ms <build> cells <n> kb <kept>", autoplotways: "alien found 0|1 points <n> ms <t> from <module> to <module>; floor ...").
	// AutoPlotProbe=2 instead searches between every AI module's EXITS (its ADJMDLEP entry points, entry_markers.h), pair by pair, as the auto-plot will: autoplotexits
	// "modules <n> pairs <p> found <f> climbing <c> toofar <t> noway <w> floorfound <ff> ms <total> max <slowest>". One line each in editor_main.cpp.
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
