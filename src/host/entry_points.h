// entry_points.h: where the entry point between two adjacent modules goes, EXACTLY as inteng's FUN_00402ae0 (Adjmmode.cpp) works it out. Written
// 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (Adjacent Modules: Add, Set Current, Recalculate Entry Points). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// SOURCES (re-tools\projects\vasquez-inteng\out): FUN_00402ae0 menu-reads-4.txt:3522-4106; FUN_004026e0, FUN_00402770, FUN_004038a0, FUN_004028f0 in
// menu-reads-21.txt; the vector helpers FUN_0042f860 (a + b), FUN_0042f8a0 (a - b), FUN_0042f960 (three __ftol) in menu-reads-7.txt and -8.txt.
// What it does, step by step, as read:
//   1. flags lose bits 2 and 4. If the adjacent module's PLACE is inside the main module's box (its shape's extents moved by its place), flags get
//      bit 4 and that is all; else if the main module's place is inside the adjacent's box, bit 2 and that is all (FUN_004026e0).
//   2. Each module stands for a LIST of module objects (FUN_004038a0: itself; an AI slave stands for its master; a master for itself and its slaves),
//      the two lists are merged through the modules' ADJMDLEP (FUN_004039d0). The lists are made by entry_recalc.cpp (the helpers were read in
//      Vasquez's runs 31, 33 and 36); ComputeEntryPoint here takes each module alone, which is what the group step gives for a module that is
//      nobody's slave or master.
//   3. Each list's box: the smallest and largest of its members' (extent + place). The overlap box: lo = the larger of the two minimums - 100, hi = the
//      smaller of the two maximums + 100, on each axis.
//   4. For each list, the points of its members' shapes (shape point + place) STRICTLY inside (lo, hi): how many (FUN_00402770), and their smallest
//      and largest coordinates (starting from the list's own box maximum and minimum).
//   5. With fewer than 2 such points in a list, the entry point is the middle of the overlap box. Otherwise on each axis: the middle of the larger of
//      the two lists' smallest inside coordinates and the smaller of their largest.
//   6. The overlap box's size, hi - lo: when any side is below -100 the modules don't touch (inteng warns and lets the mapper stop: `warn` here).
//   7. The thinnest side of the overlap box (x strictly smallest, else y when not larger than z, else z) is the way through: the entry point's
//      coordinate on that axis is put 100 beyond the main list's inside points, on the side the adjacent module lies (maximum + 100 when the main
//      module's place is below the adjacent's on that axis, else minimum - 100). For x and z flag bit 8 is cleared and the point's height is moved to
//      the lowest floor polygon above it (FUN_004028f0); for y bit 8 is set.
//   8. Unless flag bit 1 is set: the point is held inside the adjacent module's box shrunk by 5 (clamped each side), and where it falls outside the
//      box altogether the adjacent module's own place is used for that coordinate.
//   9. Whether the point lies in some member of the adjacent list is checked; if not, inteng shows "Failed to placed entrypoint in a..." and goes on.
//
// CHECKED AGAINST THE STOCK LEVELS (2026-11-15, 63 levels, the oracle spike14 in AvP-docs\tools\drake): 31,083 stored entry points; 7,136 belong to AI
// master/slave groups (the merge, a named gap, so not counted); of the other 23,947, the flags agree on 23,735 (99.1%) and flags and point both on
// 21,822 (91.1%). Of the 2,125 that differ, 1,580 are floor moves (FUN_004028f0) and in 1,179 of those the stored height is the height BEFORE the floor
// step, i.e. inteng's stored data found no floor where this port does. Not explained: the stock files may carry hand-edited or stale points; a lead,
// not a result. _DAT_005ab2ac (the normal limit) is the float 0.0 (Vasquez, run 19, bytes at 0x5ab2ac): callers pass 0.0f; with it 21,864 of the 23,947 agree.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	struct EntryModule
	{
		int32_t place[3];     // the object's place
		int32_t maxExtent[3]; // its shape's extents: the largest x, y, z ...
		int32_t minExtent[3]; // ... and the smallest
		std::vector<int32_t> points;      // the shape's points, three whole numbers each
		std::vector<int32_t> polygonCorners;   // each polygon's corners: the count, then up to 4 point numbers, so five numbers each (unused -1)
		std::vector<float> polygonNormalY;     // each polygon's normal's y
	};

	struct EntryResult
	{
		bool done;            // false: inteng's early return 0 (the mapper stopped it after the warning), nothing is changed
		bool warnedNoTouch;   // the modules didn't touch (a side below -100)
		bool failedToPlace;   // inteng's "Failed to placed entrypoint" message
		uint32_t flags;       // the flags after
		int32_t point[3];     // the entry point (kept as it was when flag bit 1 was set, or when a box overlap decided)
		int axis;             // 0, 1, 2: the way through chosen in step 7, -1 when step 1 decided
		bool middle;          // step 5 took the middle of the overlap box (fewer than 2 points inside)
		bool floorMoved;      // FUN_004028f0 changed the height
		int32_t beforeFloor[3]; // the point just before FUN_004028f0 (for the check against the stock levels)
	};

	// The float inteng compares a polygon's normal y with in FUN_004028f0 (_DAT_005ab2ac, 0.0f): a polygon counts as a floor only when its normal's
	// y is below it.
	EntryResult ComputeEntryPoint(EntryModule const &main, EntryModule const &adjacent, uint32_t flagsIn, int32_t const pointIn[3], float floorBelow,
	                              bool stopWhenNotTouching);

	// The same for two LISTS of modules (an AI group stands for each of its members; entry_recalc.h makes the lists the way inteng does, FUN_004038a0
	// and FUN_004039d0). main and adjacent are the pair's own modules: step 1's place tests, the side the adjacent lies on (step 7) and the place used
	// where the point falls outside the box (step 8) read those; the boxes, the points inside, the floor and the check of step 9 read the lists.
	// A list with nobody in it: done is false, nothing is worked out (inteng's assertion at Adjmmode.cpp:0x1b0 and 0x1b1).
	EntryResult ComputeEntryPointOfLists(EntryModule const &main, EntryModule const &adjacent, std::vector<EntryModule const *> const &mainList,
	                                     std::vector<EntryModule const *> const &adjacentList, uint32_t flagsIn, int32_t const pointIn[3],
	                                     float floorBelow, bool stopWhenNotTouching);
}
