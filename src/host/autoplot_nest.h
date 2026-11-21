// autoplot_nest.h: THE NESTED BOXES, the volume planner's last two steps (Bishop's specs (1) and (C') 11-21, Drake's read of MED05, the owner's tetris rule). Pure: no
// level, no game. Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The game takes the LAST box of the list holding an alien's body as its own (SweepWaypoints, bh_waypoint.cpp 425-446: a body is in a box when any of its 8 corners is),
// and an alien that has reached its route's start box's centre goes on to the next box only when that start is its own (177-182). So when a box A lies inside a box B
// and comes before it in the list, at A's centre the body is in B too, B is its own, and an alien whose route starts in A never goes on (MED05: its door0 entry inside
// its door2 entry; Hudson 082505:a16, ~4,000 stalled frames). Two steps put that right:
//   THE SPLIT (C'): where a ceiling entry lies inside another entry, the outer one is cut round the inner one into a few rectangles that cover the rest of its ceiling
//   ("tetris together": the owner), so the two no longer overlap at all and at the inner entry's centre the body is in the inner entry only;
//   THE ORDER (1): every box still lying inside another (a climb, a get-out) comes after it in the list.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "autoplot_plan.h"

namespace host
{
	int32_t const kAutoPlotNestSlack = 600;   // mm: a box lies inside another when on every axis it sticks out of it by at most this (and they overlap)
	int32_t const kAutoPlotNestLeast = 900;   // mm: no piece of a split entry narrower than this either way across (the owner's "no smaller than .9")
	int32_t const kAutoPlotNestAimClear = 100;   // mm: a piece's aim is where a body standing there, and this much round it, is in no other box (the closers' margin)

	// THE SPLIT. For each ceiling entry (role "entry") that has exactly one other entry lying inside it (and not the other way round too: two entries nearly one), the
	// outer entry is replaced by the fewest, largest rectangles (looking down) covering its area outside the inner one: strips across its whole length one way, then
	// strips between them the other way, whichever of the two ways gives fewer pieces, then the larger biggest piece. They touch the inner entry and each other, with no
	// overlap and no gap (the loader takes 1 off a box's max: a piece ending where the inner box starts meets it exactly). Each keeps the outer's height and its role.
	//   - the inner entry takes the outer's top when the outer's is higher (Bishop's (a)): over its area it stands alone now, and it was sized for its own ceiling cells
	//     only (doormp09: the roof over its door0 entry ramps from -1558 up to -1948, the entry's top -1794: there the outer alone held a hanging alien);
	//   - the piece nearest the outer's doorway (its exit point, exits[d] for the doorway d whose entry it is; else its exit box's middle) stays in the outer's place in
	//     the list: it keeps its doorway, its exit and every one of its links;
	//   - every other piece goes right after it in the list (the game takes the LAST box holding a body: so every box that came after the outer still comes after its
	//     pieces): aliens only both ways to that doorway piece and to the pieces it touches, and the outer's own link (its flags, at both ends) to each of the outer's
	//     linked boxes it touches looking down (the planner's choice of what an entry there links to, kept);
	//   - each piece's aim: the place nearest the outer's own aim (its height kept) where a body standing there, kAutoPlotNestAimClear round it, is in no other box;
	//     none such: the nearest where the body alone is in no other box; none: named in the notes, the aim at the nearest place inside the piece.
	// Not done, and named in the notes: a piece that would be under kAutoPlotNestLeast either way (the entry is left whole: THE ORDER still puts the inner after it), an
	// entry with two entries inside it, an entry that is no doorway's, and a split that would take the list past kAutoPlotMostBoxes (the stock search's ring).
	// exits: the AI module's exit points (the world's mm, as AutoPlot_PlanVolumes takes them); scale: the level's (the list is in the file's units). Afterwards
	// plan->centresShared counts the boxes whose aim another box holds. Returns how many entries were split.
	size_t AutoPlot_SplitNestedEntries(AutoPlotPlan *plan, std::vector<std::vector<int32_t> > const &exits, double scale);

	// THE ORDER. Reorders plan->list so that every box lying inside another (and not the other inside it too: two boxes nearly one) comes after it, the rest in their
	// order; the links, the climbing marks, the roles and every place in the list the plan names (each doorway's entry and exit box, the doorway floors) follow. No box's
	// extent, aim or links change, only their numbers. Returns how many boxes moved (0: the list as it was; also when the boxes' order cannot be met, a ring of boxes each
	// inside the next, said in the plan's notes)
	size_t AutoPlot_OrderNested(AutoPlotPlan *plan);
}
