// autoplot_lane.h: THE LANES' PROOF (Bishop 11-19, the owner's "continuous lanes": an alien on a module's ceiling stays up through the doorway into the next one's ceiling).
// The stock game's own route choice for one alien, made as bh_waypoint.cpp makes it, on an alien list as it is written (the file's units and the level's scale), so the
// auto-plot can say which lanes hold before anyone plays them. Pure. Written 2026-11-19 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT COPIES (avpx/3dc/avp, Rebellion's code as it is):
//   - the boxes that hold the alien: any of its 8 test corners, x and z at its position +-450, y at its position and 900 above (WaypointContainsNPC, bh_waypoint.cpp:335-386;
//     CE_ALIEN, extents.cpp; bh_alien.cpp:1134-1139); a box holds a place when lowest <= place <= highest - 1, each of the file's numbers times the scale cut to a whole one
//     (the loader, projload.cpp:2596-2607);
//   - the target's box: the last in the list that holds it (SweepWaypoints, bh_waypoint.cpp:415-448);
//   - the route from each box that holds the alien to the target's, by NewFindThisRoute with ALLOW_USE_OF_LONGER_PATHS (every box weighing 5, the search from the END, the
//     ring of 100 places, the links taken from GlobalLinkShift % links: bh_waypoint.cpp:687-874), the fewest taken (FindBestRoute, 575-642; one start with no route fails it all);
//   - an alien's search skips no link, the others skip 0x4 (bh_waypoint.cpp:761); a one-way link is never taken (767);
//   - AlienIsEncouragedToCrawl (877-900): the best route's FIRST link (the second box's link back to the start box) aliens-only. No route, no box, or the alien in its target's
//     box (start == last): no first link, gravity on (more than 8 m from the player, bh_near.cpp:904-906): it drops.
// A HANGING ALIEN: its collision box never turns (dynamics.cpp:5568-5613), so on a ceiling its position is 900 under it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autoplot_plan.h"
#include "waypoint_model.h"

namespace host
{
	int32_t const kStockAlienRadius = 450;     // CE_ALIEN's CollisionRadius
	int32_t const kStockAlienTop = 900;        // CE_ALIEN's CrouchingTop, up (y is down)

	struct StockChoice
	{
		size_t holding = 0;        // boxes that hold the alien (NPC_Waypoints)
		bool targetHeld = false;   // a box holds the target
		bool routed = false;       // FindBestRoute found a route (false: "no continuous route", or no box)
		int32_t start = -1, second = -1, last = -1;
		int32_t firstLinkFlags = -1;   // the flags of the route's first link, -1 for none (start == last)
		bool crawls = false;       // that link is aliens-only: gravity stays off
	};

	// The stock choice for an alien (alien: true; else a marine, a predator, a praetorian: 0x4 skipped) at `position` (the world's mm, y down: its feet; a hanging one's is
	// 900 under the ceiling) heading for `target`, with GlobalLinkShift `linkShift` (((intptr_t)sbPtr & 0xffff) >> 4 in the game: a different alien may break ties another way)
	StockChoice Stock_Choose(std::vector<WaypointRec> const &list, double scale, int32_t const position[3], int32_t const target[3], bool alien, uint32_t linkShift);

	// THE DOORWAY: a hanging alien carried along the way through it, its position 900 under the soffit, from `back` mm before the doorway's middle (in AI module A, heading
	// for A's exit point toward B) to `over` mm past it (in B, heading for `targetB`), every `step` mm, each link shift 0..15. Continuous when it crawls at every place.
	struct LaneCrossing
	{
		bool continuous = false;
		std::string why;           // the first place it does not crawl, in words ("" when continuous)
	};

	struct LaneSide
	{
		std::vector<WaypointRec> const *list = nullptr;
		int32_t target[3] = { 0, 0, 0 };
	};

	LaneCrossing Lane_Cross(LaneSide const &a, LaneSide const &b, double scale, int32_t const soffit[3], int32_t const middle[3], double const through[2],
		int32_t back, int32_t over, int32_t step);

	// INSIDE A MODULE: the route a crawling alien takes from box `from` toward `target`, box by box (each time held by the next box only), crawling at each step and over
	// climbing boxes only until the target's box. Continuous when every step's first link is aliens-only; why: the first box where it is not
	LaneCrossing Lane_Through(std::vector<WaypointRec> const &list, double scale, int32_t from, int32_t const target[3], std::vector<bool> const &climbing);

	// THE SAME, ON EVERY LINK SHIFT'S ROUTE (Vasquez 11-20: the game gives each alien its own GlobalLinkShift, so where two routes have as many boxes some aliens take
	// one and some the other; Lane_Through walks shift 0's only, and read Object16's tie 1-4-11-13 against 1-6-5-13 as continuous). From each box on the way, the
	// second box of all 16 shifts' routes, each branch walked on (a box once). Continuous when every branch reaches the target's box over climbing boxes, each step's
	// first link aliens only. why, at the first fault found (fewest steps first): Lane_Through's own words, and for a floor box on the way the shift that took that branch,
	// "box N on the way is a floor box (the route a b ... N, the target's box T); link shift s"
	LaneCrossing Lane_ThroughAll(std::vector<WaypointRec> const &list, double scale, int32_t from, int32_t const target[3], std::vector<bool> const &climbing);

	// CONSOLIDATED (the owner 11-20: "the game is not dealing well with discreet squares"): the plan with the BIGGEST floor boxes whose lanes along the AI module all hold. The floor
	// tiles merged to at most kAutoPlotFloorMost a side first, then 2400, 1200 and not merged (Drake's floor price: the route with the fewest boxes wins, so a short room's floor in two
	// boxes beats its ceiling); a lane along it: from each exit's door box toward every other exit's point (Lane_Through). The first with no lane broken, else the one with the fewest
	// broken (the biggest of those). *broken: its broken lanes; *mostSide: the side used (0: not merged)
	AutoPlotPlan AutoPlot_PlanConsolidated(std::vector<AutoPlotWay> const &ways, std::vector<std::vector<int32_t> > const &exits, AutoPlotFloor const &floor,
		std::vector<AutoPlotDoor> const &doors, double levelScale, size_t *broken = nullptr, double *mostSide = nullptr);
}
