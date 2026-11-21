// autoplot_plan.h: AUTO-PLOT ALIEN WAYS, the generator (Bishop's design 11-18, AvP-docs/engine/inteng/reassess/alien-autoplot-design.md; the owner's addition, to honour the artists who
// set the paths by hand): the ways AvPx's own pathfinder found through one AI module (autoplot_grid.h, Drake's) made into STOCK alien waypoints, boxes and links, that the Steam game routes
// on as they are. Pure: ways in, a waypoint list out, a test can drive it with made-up ways. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE RULES (the design's, from the stock game's own code):
//   - at most 99 boxes (the stock search's ring has 100 places and fails silently when it wraps: bh_waypoint.cpp:679-686, 825-830);
//   - every exit point (the AI module's entry points, ADJMDLEP, one per neighbour) inside a box;
//   - every box reaches every other (one network);
//   - each centre (the place an NPC steers to) inside ITS OWN box only, and clear of the faces it shares with others (the straddle stall);
//   - links written both ways, their numbers inside the list; 0x4 (aliens only) on every link that touches a wall or a ceiling stretch, including the step back down to the floor; the floor
//     route's links plain, so marines, predators and praetorians (who skip 0x4) still go; no 0x8 (a large-target link into a box with no room crashes the stock game).
// THE STOCK PRICE (Bishop 11-18, from the game's code): every box weighs 5 on load whatever its size and the search adds each next box's weight (projload.cpp:2395, bh_waypoint.cpp:780),
// so a route costs its BOX COUNT. "Floor is lava" in stock: the floor is many small boxes and the climbing route a few LONG ones (the owner's "highways"), floor and climbing never made
// one box. And an NPC or its target in no box drops the waypoints and runs straight (bh_waypoint.cpp:110-131), so the floor a marine walks on is COVERED: given the grid's floor cells
// (AutoPlotFloor), the floor is tiles of them (kAutoPlotFloorTile across at least, larger only to stay under 99), the ways' floor stretches are not used, each climbing stretch is a
// highway box up to kAutoPlotStretch long standing on the tiles it reaches down to (cut off there, never over them), linked aliens only (0x4) to every tile it touches.
// THE STEPS: each way is cut to the AI module (its points inside it, and one point past each end: the doorway's poke-through, where the exit points lie); cut into STRETCHES of one surface,
// at most kAutoPlotStretch long (a stretch's last point is the next one's first, so their boxes meet); each stretch a box round its points, reaching kAutoPlotReach off the surface (the
// alien's body), kAutoPlotInto into it (a floor box a little below the floor: y is down) and kAutoPlotHalfWidth to each side; boxes of one surface that mostly overlap merged; an exit no box
// holds gets a box of its own, linked to those it touches or else its nearest; links between boxes that follow each other on a way; parts that don't reach each other linked where their
// boxes touch; then merged, the least growth first, down to kAutoPlotMostBoxes; each centre picked by the game's own test of a place in a box (the scaled numbers), a box the others
// cover (no place in it outside them) taken out, or made one with the one that holds its best place when taking it out would lose an exit or split the network, until every centre is
// its own box's only; all in the file's units (the world's over the level's scale).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "waypoint_model.h"

namespace host
{
	int32_t const kAutoPlotMostBoxes = 99;
	int32_t const kAutoPlotStretch = 6000;     // the world's mm one climbing box (a highway's) covers along a way at most
	int32_t const kAutoPlotFloorStretch = 3000; // the same for a way's floor stretch, when no floor cells are given
	int32_t const kAutoPlotFloorPrice = 10;    // the alien search's (autoplot_grid.h kAutoPlotAlienFloorPrice): about this many floor boxes a metre for each climbing one
	int32_t const kAutoPlotFloorTile = kAutoPlotStretch / kAutoPlotFloorPrice;   // a floor tile's least width, x and z (600 mm)
	size_t const kAutoPlotMostCells = 200000;  // floor cells used at most (a hostile level's grid can't make the tiling run on)
	int32_t const kAutoPlotIslandReach = 6000; // a part no way joins (a crate's top, a ledge) linked aliens only to the nearest box of another part this near at most
	int32_t const kAutoPlotExitReach = 4000;   // an exit's own box reaches down this far at most to the floor box under it (Colony's exits sit mid-doorway, about 2 m up)
	int32_t const kAutoPlotReach = 1500;       // off the floor: the alien's body, a marine's (his test corners: the floor and 1200 up) and the player's chest (the target)
	int32_t const kAutoPlotClimbReach = 450;   // off a wall or a ceiling: a climbing alien's test corner that touches it is all a box needs to hold (THE LANES, below)
	int32_t const kAutoPlotInto = 200;         // into the surface
	// THE LANES (Bishop 11-19, the owner's "continuous lanes across modules": an alien on a module's ceiling stays up through the doorway into the next one's ceiling). The
	// stock code's rules (Drake's and Dietrich's readings, the same): more than 8 m from the player an approaching alien has gravity on every frame unless the route's FIRST
	// link is aliens-only (bh_near.cpp:904-906, 1036-1042; AlienIsEncouragedToCrawl, bh_waypoint.cpp:877-900); no link at all when its box also holds its target
	// (start == last, bh_waypoint.cpp:154) or when no box holds it; which boxes hold it: its 8 test corners, +-450 in x and z, y at its position and 900 above (WaypointContainsNPC;
	// CE_ALIEN), and its collision box never turns (dynamics.cpp:5568-5613), so on a ceiling it hangs with its position 900 under it: corners AT the ceiling and 900 below.
	// So at a doorway with a ceiling (the grid's ceiling over the exit point, at most kAutoPlotSoffitLook above): a DOOR BOX on the doorway's top (the soffit), from
	// kAutoPlotLaneBack inside this AI module to kAutoPlotLaneOver past the doorway's middle (more than the corners' 450: the next module's door box holds the alien the moment
	// its module changes; Drake), down kAutoPlotLaneDrop under the soffit, its centre inside it, linked aliens only; the exit point's own box under it with its top at least
	// kAutoPlotHangClear under the soffit, so a crawling alien is never in the box of its target; and every floor box under a ceiling box cut to kAutoPlotHangClear under it
	int32_t const kAutoPlotExitPast = 100;       // an exit point lies about this far past its doorway's middle (Frost's measures: about 100 inside the next module)
	int32_t const kAutoPlotSkyPast = 1500;       // OPEN SKY PER DOORWAY: the floor this far past a doorway's middle is where the lane would go on (past a soffit's depth)
	int32_t const kAutoPlotSoffitLower = 100;    // THE DOORWAY'S TOP IS THE LOWER CEILING (Drake's gate 11-20): a roof polygon over the exit point or the point back more than this
	                                             // under the ceiling cell found for the soffit is the doorway's top (medoor02: a 3.9 m lintel with no cell, a 5.8 m cell further back)
	int32_t const kAutoPlotCeilingLook = 64000;  // the ceiling over a floor cell looked for this far up (any height: the grid holds it to its own most; OPEN SKY is no ceiling at all)
	int32_t const kAutoPlotSoffitLook = 6500;   // Genshd1's exit points are at floor level, under ceilings of 3.9 m (chain A) and 5.8 m (chain B): Vasquez's corridor map
	int32_t const kAutoPlotLaneBack = 1200;
	int32_t const kAutoPlotLaneOver = 600;
	int32_t const kAutoPlotLaneDrop = 450;
	int32_t const kAutoPlotHangClear = 1000;
	int32_t const kAutoPlotHangLeast = 920;   // A PURE TARGET (Drake's gate 11-20, office01's exit 0, 981 under its doorway's top): an exit point at least this far under the
	                                          // doorway's top (a hanging alien's low corners are 900 under it, CE_ALIEN's CrouchingTop; 20 more for the file's rounding)
	                                          // keeps its lane: its own box's top at the exit point itself, where kAutoPlotHangClear would leave the point out
	int32_t const kAutoPlotBandEdge = 20;     // A PURE TARGET IN THE HANGING BAND (Drake's gate 11-20: Object11 exit 1 874 under its doorway's top, BLOCK-10 exit 3 841, start
	                                          // and tolift 779): an exit point this far inside the band between a hanging alien's two corner heights (the doorway's top and
	                                          // kAutoPlotCornerHeight under it) keeps its lane, its own box held inside that band, clear of both corners by this much
	int32_t const kAutoPlotAimPast = 1000;    // the exit point's own box's centre this far past the doorway's middle (Drake: an aim straight under the alien stops it)
	// THE DOORWAY START (Drake's hand2, Bishop 11-19 "fold it into v2"). The stock heading (bh_waypoint.cpp after FindBestRoute): held by one box an alien heads for the
	// route's second box's centre, by two or more for its start's own centre until it holds that place. At a doorway a floor alien is in the exit box too (Genshd1's exit
	// points about 310 over the floor), and v2d's exit box was the route's start, its centre past the doorway: the alien headed back out (Drake's lanes up on v2d: Ops01's
	// boxes 81 and 78, opscen03's 72). So: the door box's centre kAutoPlotDoorAimPast past the doorway's middle; the ceiling's box over the doorway's floor its centre in
	// the wall-ceiling corner just inside (kAutoPlotCornerIn in, beside the door box), so an alien heading for it walks into a wall and climbs; each floor box at the
	// doorway (its near face within kAutoPlotDoorFloorReach of the middle) linked aliens only to the ceiling's box over it, and not to the exit box, which links the floor
	// boxes past them instead and the ceiling only through the door box: the floor box at the doorway is the route's start, one box fewer than the exit box's
	int32_t const kAutoPlotDoorAimPast = 300;
	int32_t const kAutoPlotCornerIn = 300;
	int32_t const kAutoPlotCornerInWall = 100;   // the corner centre this far into the side wall past the station's cell (Drake's hand2: 100 into the -z wall)
	int32_t const kAutoPlotDoorFloorReach = 900;
	int32_t const kAutoPlotCornerHeight = 900;   // a standing alien's two test corner heights: its feet and this over them (WaypointContainsNPC; the pure target box is between them)
	double const kAutoPlotFloorLook = 1500.0;     // the floor under an exit point looked for this far round it (x and z)
	double const kAutoPlotDoorWallReach = 3000.0;   // with no climb station, the doorway's floor box is linked to a wall's box this near (a climb by a wall, never straight up)
	int32_t const kAutoPlotLeastRoom = 600;    // a floor box cut under a ceiling keeps at least this much height (a duct's floor box is not cut, the note says so)
	int32_t const kAutoPlotHalfWidth = 600;    // to each side of the way
	size_t const kAutoPlotMostSteps = 100000;  // a way longer than this is cut there (a grid's way is far shorter)
	double const kAutoPlotJoinStep = 250.0;    // the ways' joins walk each step in pieces this long (step 4a)
	size_t const kAutoPlotMostSamples = 200000;   // and at most this many places a way (a hostile way of far-apart steps can't make it run on)
	int32_t const kAutoPlotMostPlace = (1 << 24) - (1 << 20);   // places are held inside this, every axis (the loader refuses past 2^24)

	enum class AutoPlotSurface : uint8_t
	{
		Floor,
		Wall,
		Ceiling,
	};

	// One point of a way, as the pathfinder gives it (autoplot_grid.h AutoPlotPoint), and whether it lies in the AI module being plotted
	struct AutoPlotStep
	{
		int32_t place[3];        // the world's mm, y down
		AutoPlotSurface surface; // the surface the way is on from this point to the next
		int32_t facing[3];       // the surface's facing, out of it (65536 long; all 0 when not known)
		bool inside;             // in one of the AI module's modules
	};

	struct AutoPlotWay
	{
		std::vector<AutoPlotStep> steps;
		bool alien;              // the alien search (floor is lava); else the marine's floor route
	};

	struct AutoPlotPlan
	{
		std::vector<WaypointRec> list;   // the alien list to write (the file's units), links both ways
		size_t exits = 0, exitsCovered = 0;
		size_t parts = 0;                // how many networks the boxes make (1: all reach each other)
		size_t floorBoxes = 0;           // of the list, the floor's (tiles, or floor stretches when no cells were given)
		double floorTile = 0.0;          // the tiles' width (mm), 0 when the floor was not tiled
		double ceilingTile = 0.0;        // the ceiling tiles' width (mm), 0 when no ceiling was given
		size_t ceilingBoxes = 0;         // the ceiling tiles made (before the cap to 99 and the centres' take-outs)
		double floorMost = 0.0;          // the side the floor tiles were merged to at most (mm), 0 when they were not merged
		std::vector<std::vector<int32_t> > doorFloors;   // each exit: the floor boxes at its doorway that climb (THE DOORWAY START), their places in the list
		size_t wallsMade = 0;            // wall boxes made beside doorways with no wall to climb (THE DOORWAY START)
		size_t cells = 0, cellsCovered = 0;   // the floor cells used, and those inside a box of the list by the game's own test (the rest: the stock dropout)
		size_t centresShared = 0;        // centres the picking could not keep out of every other box (0: such a box is taken out or made one with a box that covers it)
		std::vector<std::string> notes;  // what didn't go to plan, in words
		std::vector<bool> climbing;      // each box of the list: a wall's or a ceiling's (else the floor's)
		std::vector<int32_t> doorBox;    // each exit: its door box's place in the list (THE LANES), -1 for none
		std::vector<int32_t> exitBox;    // each exit: the box made to hold its exit point, -1 when another box holds it
		std::vector<std::string> doorWords;   // each exit: "lane" or why not ("no ceiling over it", "the exit point is 700 mm under the doorway's top", ...)
		std::vector<int32_t> exitsLeftOut;    // the exits the repair's rule 6 (a) left in no box (no alien way to the rest; autoplot_repair.h, Drake)
		bool volumes = false;            // made by the volume planner (autoplot_volumes.h: the owner's few room-sized volumes); false: the old planner's tiles
		std::string volumeKind;          // the volume planner's kind of the AI module: corridor, corner, dead end, junction or room, open sky, stairs or levels, no doorway, no floor
		std::vector<std::string> volumeRoles;   // each box of the list: the volume planner's kind of it ("floor", "climb", "get-out", "entry", "LOW", "HIGH", "exit"); empty from the old planner
	};

	// A doorway's ceiling at one exit (THE LANES): the grid's ceiling over the exit point (the soffit, the world's mm), and the doorway's middle (half way between this
	// exit point and the neighbour's exit point back, which lies inside this AI module) with the way through it, flat (x and z, from this AI module into the neighbour's)
	struct AutoPlotDoor
	{
		bool soffitFound = false;
		int32_t soffit[3] = { 0, 0, 0 };
		bool throughFound = false;
		int32_t middle[3] = { 0, 0, 0 };
		double through[2] = { 0.0, 0.0 };   // x, z: 1 long
		// THE CLIMB STATION (a far alien lands upright on the floor at the exit point, Vasquez's drop audit S3): an alien way from the exit point to a side wall, up it, onto the
		// ceiling (given among the ways); its top, the first ceiling point. The door box reaches across to it and its centre is over it: an alien in the exit box heads for
		// the door box's centre, so it walks to that wall and climbs (a centre straight overhead would stall it: NPCSetVelocity drops the part along its up axis)
		bool stationFound = false;
		int32_t stationTop[3] = { 0, 0, 0 };
	};

	// The floor a marine stands on in the AI module, as the grid has it: every cell's floor point (the world's mm, y down) and the cells' edge (mm). And THE CEILING over it
	// (THE LANES, Drake's rule from his hand-built Ops01, 11-19: the route with the fewest boxes wins, so a lane holds where the ceiling is a few long boxes edge to edge and
	// the floor under it many): the grid's ceiling cell straight over each floor cell where there is one (AutoPlotGrid_CeilingOver, the world's mm, y down). With these the
	// ceiling is tiled too, in squares from kAutoPlotCeilingTile across, and the ways give only the walls
	struct AutoPlotFloor
	{
		std::vector<std::vector<int32_t> > cells;
		int32_t cellEdge = 0;
		std::vector<std::vector<int32_t> > ceilings;
		double mostSide = -1.0;   // CONSOLIDATED: the floor tiles merged into rectangles of at most this a side (mm); -1: kAutoPlotFloorMost, 0: not merged
		std::vector<std::vector<int32_t> > standFloors;   // every floor a marine stands on under the AI module's ceiling, of ANY module or still object (a platform): the lane boxes keep over their standing heads too
		bool boxFound = false;                // THE VOLUME SHAPE: the AI module's own box (its modules' boxes together, the world's mm): how far its sides reach past the floor
		int32_t boxLo[3] = { 0, 0, 0 }, boxHi[3] = { 0, 0, 0 };
		bool crawl = false;                   // CRAWL CELLS (Frost 11-20): the cells are Drake's AutoPlotGrid_CrawlCells (no cell a marine stands on: a duct): the levels' rule shapes it
	};

	// CONSOLIDATED (the owner 11-20: "the game is not dealing well with discreet squares"): the ceiling in lanes of up to two highway stretches, the floor's tiles merged into rectangles
	// of at most kAutoPlotFloorMost a side, a third of it: the route with the fewest boxes is still the ceiling's (Drake's floor price, kept), with far fewer boxes
	int32_t const kAutoPlotCeilingTile = 60000;   // THE LONG PATHS (the owner 11-20: "an auto planner to generate the long paths"): a flat ceiling one lane its whole length
	int32_t const kAutoPlotTallClimb = 6000;     // a corner centre higher than this over the doorway floor: a wall within reach is climbed first (a tall room)
	int32_t const kAutoPlotLaneRise = 3000;      // a lane box's ceiling may rise this much in all (a ramp), its heights never further apart than a climbing alien's reach
	int32_t const kAutoPlotDoorStrip = 1200;      // the doorway strip: the floor within this of a doorway's line, merged apart from the rest (it climbs, the rest never)
	int32_t const kAutoPlotFloorMost = 4000;
	int32_t const kAutoPlotFloorRise = 600;   // floor tiles merged only when their tops and bottoms are this near (a gentle slope one box: 300 cut opscen02's ramp up, 11-20)

	// The plan for one AI module from its ways and its exit points (the world's mm), the level's scale (the file's units times it are the world's); with floor cells, the floor is
	// tiled and the ways give only the climbing (highway) boxes; with none, the ways' floor stretches are boxes too
	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, double levelScale);
	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floor, double levelScale);
	// With each exit's doorway (THE LANES; one for each exit, or none)
	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floor,
		std::vector<AutoPlotDoor> const &doors, double levelScale);

	// One line for the status line: "N boxes, M links; exits covered k of n; ..." (the plan's own numbers)
	std::string AutoPlot_Words(AutoPlotPlan const &plan);
}
