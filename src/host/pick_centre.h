// pick_centre.h: Module Linking's "Select With Centres" (Base > Select With Centres, 0xcd): which module a click takes when the flag is on, as plain sums a test can drive.
// Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (Vasquez's read, AvP-docs\engine\inteng\module-linking-reads.md section 3; FUN_004c3760 0x4c3773-0x4c37eb, FUN_0046ddb0): with the flag off a click is the normal
// hit test; with it on, the click takes the module whose CENTRE is nearest the pointer ON THE SCREEN. For every loaded object it must pass all of:
//   - its first word (record+0) is not below DAT_008f588c (set at 0x46e62b / 0x46ed88, meaning unread; FUN_00405690 has the same test): not read here, every object passes;
//   - bit 4 of +0x5c clear (it is not hidden): the caller leaves hidden objects out of the candidates (hidden_objects.h);
//   - FUN_0046ddb0 gives a distance d with 0.0 < d < best so far (best starts at 100,000,000.0). The smallest wins, the first on a tie, none = nothing is taken.
// FUN_0046ddb0: the centre in view space (x, y, depth); a depth not beyond the viewport's near value gives -1.0 (never taken); else the centre is projected
// (x' = x * xScale / depth + the view's middle, the same down) and d is the straight-line pixel distance from the pointer to (x', y'). Exactly 0.0 is not taken.
//
// OURS: the centre is the module's place (the camera's world: file units times the level's scale, as the editor's camera is set); the projection is the game's own, as the
// click's line is made (pick_ray.h run forwards: the camera's matrix turned over, the projection across by xScale, down by yScale, the card's up turned into the game's down). The near
// value is kPickCentreNear (the game's own near plane is not read here: named).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pick_ray.h"

namespace host
{
	// The nearest depth a centre may have, in the game's units (millimetres)
	int32_t const kPickCentreNear = 1;

	// Inteng's start for the best distance (pixels)
	double const kPickCentreFarthest = 100000000.0;

	// One candidate: its centre in the game's world (the editor's camera units)
	struct PickCentre
	{
		int32_t place[3];
	};

	// The pixel distance from the click to the candidate's projected centre, as FUN_0046ddb0 works it; -1.0 when the centre is not ahead of the camera (depth below
	// kPickCentreNear) or the camera or view is unusable (the same checks as PickRay_FromClick).
	double PickCentre_Distance(PickCamera const &camera, int32_t clickX, int32_t clickY, PickCentre const &centre);

	// Where the centre is on the screen, in the view's own pixels (the projection the distance above is measured to): false when it is not ahead of the camera or the camera or view is unusable
	bool PickCentre_Project(PickCamera const &camera, PickCentre const &centre, double *x, double *y);

	// The index of the candidate the click takes, or -1 for none: the smallest distance above 0.0 (zeroTaken: 0.0 or above) and below kPickCentreFarthest, the first on a tie.
	// zeroTaken: Object Placement's click (FUN_004d6520 0x4d66c1: DAT_005ab360 (0.0) <= d < best); Module Linking's Select With Centres takes only above 0.0.
	// farthest: where the best starts (Light Sourcing's light and object clicks, FUN_004ab2c0 / FUN_004ab200: 1,000,000.0, kPickCentreLightFarthest).
	int PickCentre_Nearest(PickCamera const &camera, int32_t clickX, int32_t clickY, PickCentre const *candidates, size_t count, bool zeroTaken = false, double farthest = kPickCentreFarthest);

	// Light Sourcing's start for the best distance (FUN_004ab2c0 0x4ab2d3, FUN_004ab200 0x4ab21a: 1,000,000.0 pixels)
	double const kPickCentreLightFarthest = 1000000.0;
}
