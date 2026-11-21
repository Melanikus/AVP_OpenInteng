// autoplot_arrival.h: A DOORWAY B'S PLAN DOES NOT KNOW, for THE LANES (the owner's continuous lanes: an alien on a module's ceiling stays up through the doorway into the
// next one's ceiling). AI module A has an exit to B through a doorway, and B's entry points (ADJMDLEP, as the game's loader takes them, projload.cpp:2255-2335) name A only at
// ANOTHER doorway: Genshd1's office lists doormp06 only at a doorway 7.8 m from doormp06 exit 0's, the whole-level gate's last lane end under a ceiling (Drake's lanes26 on
// Dietrich's b6eac9c3, 11-20). A's side has a doorway (autoplot_command.cpp's "A DOORWAY WITH NO EXIT POINT BACK", Dietrich's 7b4be05e: the way through from A's nearest
// stand spot), but B's plan had no door box where A's aliens come in, so the lane stopped there. Pure: no level, no game. Written 2026-11-20 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULE (agreed with Dietrich 11-20): B gets an ARRIVAL for each such exit of A, an exit of B's plan after B's own (the level's), so B's plan makes a door box where A's
// aliens arrive and its lanes run on from there. NOT where B names A at no doorway at all (a one-way doorway): arrivals there measured worse twice on the whole level
// (08552b37 on 3b38b0ca; on b6eac9c3 BLOCK-05 refused over the 99-box cap and 35 more stall or back places), the gate takes those doorways from A's side, and the planner's
// own rules since hold most of them. The arrival:
//   - its point (where an alien of B heading for A would aim, about 100 inside A, as a two-way doorway's exit point lies): A's point back, mirrored across the doorway;
//   - its point back (where an alien from A lands in B, the climb station's start): A's exit point, which lies about 100 inside B;
//   - its doorway: A's middle, the way through turned round (out of B, into A); the soffit is looked up later, as for every exit;
//   - its neighbour: A's head, so the lane table pairs A's exit with it.
// No alien of B ever heads for A through it (B has no exit to A there: the game's route search never sets that point as a target), so an arrival is never a lane's A side
// nor a lane's target in B, and it is never given to the checker (the level's exits only): the caller keeps those apart by the count of the level's exits.
// ONE DOORWAY: B's exit back to A is at this doorway when its point lies within kAutoPlotArrivalSameDoorway of A's exit point, flat (the command's own test).
// HOSTILE INPUT (the lists come from the level's entry points): any list shorter than the exits, a point that is not three numbers, a neighbour of -1 or A itself, a way
// through that is not a finite unit flat vector: that exit gives no arrival. At most kAutoPlotMostArrivals arrivals are given (no level has more doorways than that).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "autoplot_plan.h"

namespace host
{
	size_t const kAutoPlotMostArrivals = 65536;   // the most arrivals given; more are left out (a guard on the numbers, never a limit a level meets)
	double const kAutoPlotArrivalSameDoorway = 4000.0;   // two exit points this far apart at most, flat, are one doorway (autoplot_command.cpp's gathering: farther, "too far apart")

	// One AI module's exits as the auto-plot gathers them from the level, before any arrival is added: each exit's point (the world's mm, y down), its neighbour (an AI
	// module's head, -1 not known), its doorway, and the neighbour's point back (inside this AI module; empty when not known)
	struct AutoPlotExits
	{
		int32_t head = -1;
		std::vector<std::vector<int32_t> > points;
		std::vector<int32_t> to;
		std::vector<AutoPlotDoor> doors;
		std::vector<std::vector<int32_t> > backs;
	};

	struct AutoPlotArrival
	{
		size_t into = 0;                  // B: its place in the list given
		int32_t from = -1;                // A: its head
		size_t fromExit = 0;              // A's exit toward B
		std::vector<int32_t> point;       // B's exit point toward A: A's point back (inside A)
		std::vector<int32_t> back;        // where an alien from A lands in B: A's exit point (inside B)
		AutoPlotDoor door;                // the same doorway from B's side: A's middle, the way through turned round; no soffit, no climb station yet
	};

	// Every exit of every AI module in `modules` into another one in the list whose exits back to it are all at other doorways (at least one, none at this one), with a
	// doorway known on A's side (its way through and its point back): B's arrival, in the order of the modules and their exits. An exit into an AI module not in the list (not
	// plotted in this run) gives none
	std::vector<AutoPlotArrival> AutoPlot_Arrivals(std::vector<AutoPlotExits> const &modules);

	// Adds one arrival to B's lists: its point, neighbour, doorway and point back after B's exits; and its goal right after the exits' goals (the first points->size() of
	// `goals` are the exits', the stand spots after them), so a goal's number is still its exit's. Lists that are not as long as the exits are left as they are (false)
	bool AutoPlot_AddArrival(AutoPlotArrival const &arrival, std::vector<std::vector<int32_t> > *points, std::vector<int32_t> *to, std::vector<AutoPlotDoor> *doors,
		std::vector<std::vector<int32_t> > *backs, std::vector<std::vector<int32_t> > *goals);
}
