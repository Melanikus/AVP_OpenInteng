// pick_game.h: what a click on the level's view hits, found by the game's own line of sight. Written 2026-11-14 by Melanikus (Frost)
// for AVP_OpenInteng (click-picking). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// After the game has drawn a frame (LevelView_Draw), its camera and its list of what was on screen are still the frame's. A click then
// makes a line from that camera through the clicked pixel (pick_ray.h), and the game's own line of sight, the one its guns use
// (FindPolygonInLineOfSight, los.cpp:272), finds the first thing on screen the line meets. So a click picks exactly what the game
// itself sees there. The player's own body, whose eyes the camera is, is never hit.
//
// What's hit is turned into a PickReport (selection.h), checked before anything in it is used:
// - an object whose strategy has an 8-byte ID (SBname) that isn't all 0: an Object, by that ID. This comes first, because a door or a
//   lift is a module with a strategy, and it's the strategy that has the fields to edit;
// - otherwise a module, whose number must be one the game's module list has, 0 to ModuleArraySize - 1: a Module, or NumberRefused;
// - anything else is NotInTheFile; nothing hit is Nothing; and no frame, or a click no line can be made from, is NoLine.

#pragma once

#include <stdint.h>

#include "pick_ray.h"
#include "selection.h"

// What a click at (clickX, clickY), counted from the view's top left in the view's own pixels, hits in the frame the game drew last.
// viewWidth and viewHeight are the view's size in those same pixels.
// The line from the camera through that click, as the game drew the frame (pick_ray.h): false when there's no frame or no line can be made
// The camera the frame was drawn from, for the sums that work from it (pick_centre.h): false when there is none
bool PickGame_Camera(int32_t viewWidth, int32_t viewHeight, host::PickCamera *camera);

bool PickGame_RayAtClick(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight, host::PickRay *ray);

host::PickReport PickGame_AtClick(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight);
