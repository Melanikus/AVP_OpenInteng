// light_pick.h: which light a click on the level's view means, as plain sums a test can drive. Written 2026-11-15 by Melanikus (Frost) for
// AVP_OpenInteng (M-3, Light Sourcing: inteng's "use the mouse to select a light"). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// A light is a point, so a click can't hit it; the click picks the light nearest to the line the click makes (pick_ray.h), in front of the
// camera, if it's near enough: within 300 millimetres of the line, plus 2.5% of how far along the line it is (so a far light is as easy to
// hit as a near one on the screen). Where inteng's own rule is isn't read (inteng-menus.md section 6.7 says only "use the mouse to select a
// light"), so this rule is the editor's. Nothing here reads the level: the places come in, checked, and one index goes out.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pick_ray.h"

namespace host
{
	// The nearest light to the line, of count lights whose places are in places (three whole numbers each, in the game's millimetres, each
	// within kPickMostPlace of the middle: a light farther is passed over). *light is its place in the list. Gives back false, leaving *light
	// alone, when there are no lights, no places, or none is near enough to the line and in front of its start.
	bool LightPick_Nearest(PickRay const &ray, int32_t const *places, size_t count, size_t *light);
}
