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
// THE STEPS: each way is cut to the AI module (its points inside it, and one point past each end: the doorway's poke-through, where the exit points lie); cut into STRETCHES of one surface,
// at most kAutoPlotStretch long (a stretch's last point is the next one's first, so their boxes meet); each stretch a box round its points, reaching kAutoPlotReach off the surface (the
// alien's body), kAutoPlotInto into it (a floor box a little below the floor: y is down) and kAutoPlotHalfWidth to each side; boxes of one surface that mostly overlap merged; an exit no box
// holds gets a box of its own, linked to those it touches or else its nearest; links between boxes that follow each other on a way; parts that don't reach each other linked where their
// boxes touch; then merged, the least growth first, down to kAutoPlotMostBoxes; each centre picked; all in the file's units (the world's over the level's scale).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "waypoint_model.h"

namespace host
{
	int32_t const kAutoPlotMostBoxes = 99;
	int32_t const kAutoPlotStretch = 3000;     // the world's mm one box covers along a way at most
	int32_t const kAutoPlotReach = 1500;       // off the surface: the alien's body
	int32_t const kAutoPlotInto = 200;         // into the surface
	int32_t const kAutoPlotHalfWidth = 600;    // to each side of the way
	size_t const kAutoPlotMostSteps = 100000;  // a way longer than this is cut there (a grid's way is far shorter)
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
		size_t centresShared = 0;        // centres the picking could not keep out of every other box
		std::vector<std::string> notes;  // what didn't go to plan, in words
	};

	// The plan for one AI module from its ways and its exit points (the world's mm), the level's scale (the file's units times it are the world's)
	AutoPlotPlan AutoPlot_Plan(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, double levelScale);

	// One line for the status line: "N boxes, M links; exits covered k of n; ..." (the plan's own numbers)
	std::string AutoPlot_Words(AutoPlotPlan const &plan);
}
