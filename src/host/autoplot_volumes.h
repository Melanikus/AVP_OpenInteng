// autoplot_volumes.h: THE VOLUME SHAPE (the owner 11-20: "one entry module on top on each side, one in the middle to climb the wall/get in and another to get out/attack",
// "So like 5 volumes, maybe 7/8 for corners", room-sized like stock's: Genshd1's 9 stock networks have 13 boxes each, floor boxes 7.9 x 4.8 m, floor to ceiling; our many small
// boxes made his aliens flicker, "not committing because there are a hundred small volumes"). The spec: AvP-docs/engine/inteng/reassess/alien-volumes-spec-dietrich.md. Pure: no
// level, no game. Written 2026-11-20 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE VOLUMES, in the list's order (the game's target box is the LAST that holds the point: SweepWaypoints, bh_waypoint.cpp:415-448):
//   FLOOR      one a floor rectangle (the floor cells in the fewest big rectangles), 200 under the floor to 1500 over it; its centre the middle, 750 over the floor
//   CLIMB-IN   one a floor rectangle that has a ceiling and a wall (the climb ways' wall steps): 2.4 m along that wall at the rectangle's middle, 200 into the wall to 450 off it,
//              the floor to the ceiling; its centre high on the wall, 100 OFF it (600 under the ceiling, or 100 under the entries it leads onto): on a wall an alien moves
//              only in the wall's plane (NPCSetVelocity, bh_ais.c:704-724), so the heading there is straight up it; and a centre the alien's own box can HOLD (Vasquez's
//              climbwalk 11-20: held by 2 boxes the game heads for the start box's centre until it holds it, bh_waypt.c:169-176; one inside the wall it never does)
//   GET-OUT    the attack drop, at the biggest ceiled rectangle's middle: 700 under the ceiling (a hanging alien's lower corners) down to 1500 over the floor; its centre its bottom
//              middle: from the ceiling straight down, so in the player's module the game throws it off onto him (Bh_near.c:984-1000). None when the room is under 2.6 m
//   ENTRY      one a doorway with a ceiling: the ceiling nearest that doorway (each ceiling place to its nearest doorway) and the doorway's top to 600 past its middle; 200 into the
//              ceiling down to 450 under the lower of ceiling and soffit; its centre 200 UNDER the ceiling (a hanging alien's top is the ceiling: one inside it no box ever
//              holds), 700 off the climb wall at the climb-in's end nearest the doorway (from the wall's top that heading is far off the wall's normal: never "straight off
//              it", Bh_near.c:984-1000)
//   EXIT       one an exit: the doorway passage, 600 inside to 1200 past the doorway's middle, 600 each side; 200 under the floor to 1000 under the soffit (a hanging alien
//              never touches its own target: start = end gives no first link, gravity on); its centre 1000 past the doorway's middle at the exit point's height
// THE LINKS, all aliens only; "->" one way (the stopped way: the box stepped from lists the other One way, kLinkOneWay; the other kLinkOneWayBack, as inteng's Add Link):
//   floor <-> floor (touching), floor -> climb-in, climb-in -> entry (touching), entry <-> entry (touching), entry -> its exit, exit -> floor, entry -> the floor under it
//   (THE ATTACK, 2 boxes: by the get-out it would be 3 and tie with entry, exit, floor, and a tie sends some aliens out through a doorway to reach him), entry -> get-out
//   -> floor (the landing column: a dropping alien stays in the network). An exit with no entry (no ceiling over it: open sky) is floor <-> exit. So the floor goes up only
//   by the climb-in, and a lane is entry, entry, exit: the fewest boxes, no ties.
#pragma once

#include <stdint.h>

#include <vector>

#include "autoplot_plan.h"

namespace host
{
	int32_t const kAutoPlotVolumeClimbLength = 2400;   // the climb-in's and the get-out's length along their wall / the room
	int32_t const kAutoPlotVolumeOffWall = 700;        // an entry's centre this far off the climb wall's face (R1: 76 degrees off the normal at the wall's top)
	int32_t const kAutoPlotVolumeUnder = 300;          // the climb-in's centre this far under the ceiling, 100 beyond the side's face there (Drake's hand5b)
	int32_t const kAutoPlotVolumeHang = 1001;          // the get-out's top this far under the ceiling: a hanging alien (its lower corners 900 under) is never in it on a lane
	                                                   // (Drake's hand5b 11-20: at 700 every entry centre was held by the get-out too, and held by 2 boxes it stalls)
	int32_t const kAutoPlotVolumeClimbRoom = 100;      // the climb-in reaches this far into the room (Drake's hand5b: at 450 a hanging alien 700 out was still in it)
	int32_t const kAutoPlotVolumeEntryClear = 600;     // an entry's centre at least this far (x and z) outside every other entry (Drake's hand5b corner)
	int32_t const kAutoPlotVolumeTopClear = 1051;      // an exit volume's top this far under the soffit: MORE than 1000 under the real ceiling (Drake's D3; the grid's
	                                                   // soffit reads a few mm high: at 1001 six exits on 1d4b78e7's path were 998-1000 under it)
	int32_t const kAutoPlotVolumeAimUnder = 100;       // an entry's and a ceiling piece's (HIGH's) centre this far UNDER the ceiling face: a hanging alien's box (the
	                                                   // ceiling to 900 under it) holds it, and a climber on an upper face heads along it (Bishop 11-20, the loop the
	                                                   // owner sat through at gencor01's climb, Frost's run 153834: aims 100 INTO the ceiling, held by no box; at the
	                                                   // climb's top the flattened move pushed into the upper chamfer, NPC_IsObstructed, then Avoidance down the tube.
	                                                   // hand6g has it too: the gold compare names this difference)
	int32_t const kAutoPlotVolumeSloped = 600;         // a side reaching more than this past the floor is SLOPED (a tube's 45-degree faces): its climb in LOW and HIGH
	int32_t const kAutoPlotVolumeLowOver = 400;        // LOW reaches this far over the vertex's height, HIGH down to it: they overlap by this (the hand-over; Drake's hand6g
	                                                   // had 500: 400 keeps the climb-side HIGH's aim, 495 over it, out of LOW)
	int32_t const kAutoPlotVolumeHighUp = 1000;        // VASQUEZ'S RULE 2 (11-20, the game's own frames at gencor01's climb with Rebellion's NPC_IsObstructed): a climb-side
	int32_t const kAutoPlotVolumeHighUnder = 300;      // HIGH's aim this far up the upper face from the crease and this far under the face, this far along the tube
	int32_t const kAutoPlotVolumeHighAlong = 2500;     // from the climb's middle (a corridor's halves; a piece over the whole climb is straight over it). The climber
	                                                   // lands with its up axis 60-80 degrees behind the face: an aim far up it is turned into it (11 frames, Avoidance).
	                                                   // At 400 the body (450 across) was in the other half, the game's own box, so it never moved on (5444c5b4); at 600
	                                                   // (1fc3dd51) and 1500 a climber that JUMPED into the climb wedges in the crease, its y row behind, and gets Avoidance
	                                                   // (Vasquez on Frost's run 174100, a10, both arrivals replayed); 2500 along, its move runs along the crease while the
	                                                   // y row turns, and both arrivals pass the entry
	int32_t const kAutoPlotVolumeHighEndClear = 1050;  // a split piece's aim at least this far in from its ends (600, an exit box's reach inside its doorway, and the body's
	                                                   // 450): on a short half it comes back toward the middle, never into the exit's box or the other half
	int32_t const kAutoPlotVolumePast = 600;           // an entry reaches this far past its doorway's middle; an exit box starts this far inside it
	int32_t const kAutoPlotVolumeExitPast = 1200;      // an exit box reaches this far past its doorway's middle
	int32_t const kAutoPlotVolumeSide = 600;           // an entry's and an exit box's half width at the doorway
	int32_t const kAutoPlotVolumeFlat = 1000;          // an entry takes the ceiling places this near the ceiling's top (a sloped edge lower than this is left out)
	size_t const kAutoPlotVolumeFloorsMost = 6;        // floor rectangles at most (the cells' leftovers are the stock dropout, counted)
	double const kAutoPlotVolumeFlatStep = 100.0;      // a floor level is a flat patch: neighbouring cells at most this apart in height (Frost's levels rule)
	double const kAutoPlotVolumeNarrow = 2000.0;       // after the first floor, a floor strip narrower than this is a sliver (a slope's row, a doorway's sill, the 1.8 m
	                                                   // squares along SHED10's diagonal funnel sides; a corner's arm or a T's funnel is 3.6 m or more)
	int const kAutoPlotVolumeHoleCells = 3;            // a hole in a flat floor up to this many cells across (along x or z) is floor (under a pipe run, a support)

	// THE OWNER'S COUNTS (11-20: "tunable, I want to be able to define how many entry points per module/modules"; his picks: climb-ins and get-outs, set in the plot dialog,
	// for every AI module of that run; "we have too many climb points": hand6g's own count, one a module and two at a corner, is the most, and the default fewer). Per AI
	// module. -1: the default. Vasquez's dialog refuses a number outside least..most, by name
	struct AutoPlotVolumeCounts
	{
		int32_t climbIns = -1;   // at most this many climb-ins an AI module; -1: kAutoPlotDefaultClimbIns
		int32_t getOuts = -1;    // at most this many get-outs an AI module; -1: kAutoPlotDefaultGetOuts
		bool airduct = false;    // AIRDUCT (MODULEFLAG_AIRDUCT) on every module of each AI module the planner shapes: the owner's force-crawl (Bishop 11-20, Drake's read-back
		                         // of 435ff6c7: hand6g, the gold standard, has it on all its shaped modules; "the flag worked and they are trying to climb"). OFF by default, set only
		                         // when the dialog's box is ticked (the owner 11-20 19:06 through Bishop, after playing his gold path with it off ("It was perfect"): "Let's not auto set it anymore")
	};

	int32_t const kAutoPlotDefaultClimbIns = 1;   // one a module (a corner one, not hand6g's two: its second climb goes, its HIGH catchers stay)
	int32_t const kAutoPlotDefaultGetOuts = 1;
	int32_t const kAutoPlotLeastClimbIns = 0, kAutoPlotMostClimbIns = 8;   // 0: no climb of its own (climbed from its neighbours' lanes)
	int32_t const kAutoPlotLeastGetOuts = 0, kAutoPlotMostGetOuts = 8;

	// The volume plan for one AI module (the same inputs as AutoPlot_Plan). plan.volumes is true when the module was shaped; false, with plan.notes saying why, when it can't be
	// (a kind it has no rule for yet): the caller then writes NO network for it, and names it. counts: the owner's (THE COUNTS above). Climb-ins: hand6g's candidates are one
	// a doorway floor's climb side (a corridor 1, a T 1, a corner 2); at most climbIns of them are kept, the biggest floors' first; a floor whose climb goes keeps its HIGH
	// pieces as jump catchers. 0: no climb at all, each floor goes up straight into the entry over it (so every box still reaches every other). Get-outs: hand6g's one; 0: none,
	// the entries drop to the floor under them. A number over the candidates gives the candidates
	AutoPlotPlan AutoPlot_PlanVolumes(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floor,
		std::vector<AutoPlotDoor> const &doors, double levelScale, AutoPlotVolumeCounts const &counts = AutoPlotVolumeCounts());
}
