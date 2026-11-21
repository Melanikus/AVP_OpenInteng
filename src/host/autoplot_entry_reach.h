// autoplot_entry_reach.h: AN ENTRY'S REACH TOWARD ITS DOORWAY'S TOP NEVER HOLDS ANOTHER BOX'S AIM (Bishop's spec (a) 11-21, the tall-entry refusals). Pure: boxes in,
// boxes out, no level. Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A ceiling entry is its ceiling's band (from 200 into the highest ceiling over its floor cells to 450 under the lowest: a hanging alien's reach), stretched to
// its doorway's top so a lane runs on through the doorway (autoplot_volumes.cpp). Where the doorway's top lies far above or below that band (a vertical doorway
// between levels, flag 8, or one at the foot of a lift shaft), the stretch makes the entry a column metres tall, and it can swallow another box's aim: the checker
// then refuses the whole AI module ("box N: its centre is in box M too") and the repair finds no place to move it (Genshd1, lanes33 cells on the stock level:
// opsstr01 AI 150, a flat roof at -17,535, door0 a vertical doorway whose top is at -9,740, 7.6 m under; liftshft02 AI 385, a flat roof at -5,672, its doorway at a
// 15 m shaft's foot; strwll01 AI 149, a roof at -1,948, door1 a vertical doorway whose top is 7.8 m over it). Of the 34 entries over 2 m tall on the whole
// level only those three modules' hold an aim, so only a held aim cuts: the stretch toward the doorway stops short of the body standing at that aim (the game's
// body: 450 each way across, from 900 over its place to its place), kAutoPlotEntryReachClear round it. The band itself is never cut, and an aim the band holds is
// left as it is (the stretch is not what holds it).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "waypoint_model.h"

namespace host
{
	int32_t const kAutoPlotEntryReachClear = 100;   // mm round the body at a held aim (the closers' margin, autoplot_closers.h)

	struct AutoPlotEntryReachCut
	{
		size_t aim = 0;        // which of the aims it was cut clear of
		bool below = true;     // the stretch under the band (true) or over it
		double from = 0.0;     // the entry's end before the cut and after it (y, the world's mm: y is down)
		double to = 0.0;
	};

	// The entry's box (lo, hi: the world's mm, y down) with its ceiling band [bandLo, bandHi] inside it on y, and every other box's aim (the world's mm, 3 numbers
	// each). Where the box's stretch past the band holds an aim (x and z inside the box, y in the stretch), that stretch stops short of the body at the aim:
	// under the band, the box's bottom above the body's top; over it, the box's top under the body's foot; never into the band. lo[1] and hi[1] are changed in
	// place; the cuts made are returned (none: the box as it was)
	std::vector<AutoPlotEntryReachCut> AutoPlot_EntryReachClear(double lo[3], double hi[3], double bandLo, double bandHi, std::vector<std::vector<double> > const &aims);

	// ONLY WHERE THE REPAIR CAN'T (Bishop's (a') 11-21): whether box b of the list has a place for its centre that the volume repair's V2 would find, at its centre's
	// height, inside b and in no other box (autoplot_volume_repair.cpp, Drake's: the places it tries on each axis are b's own two ends and just outside every other box's
	// ends, 1 and 2 past them, inside b; one axis at a time from the centre, then both; the game's test on the loaded numbers, the file's times the scale, the loader
	// taking 1 off each max). An aim whose box has such a place is the repair's to move: the planner leaves its entry whole (STEPS04 and genstrs, a get-out 1 mm inside
	// their entry; yard19's HIGHs, past its end). Only an aim with none cuts a stretch
	bool AutoPlot_PlaceOfItsOwn(std::vector<WaypointRec> const &list, size_t b, double scale);
}
