// waypoint_mouse.h: WAYPOINT MODE BY MOUSE in the editor (the owner's addition 11-18; Hicks's plan .claude/plans/streamed-twirling-spindle.md section 6, Bishop's order P4): mode 1's
// clicks in the 3D view on the ALIEN list (the one the game loads, projload.cpp:2614-2651), the sums in waypoint_pick.h. Mode 2 (Edit Extents) and the ground list keep inteng's own
// pick (WaypointBoxClick: the box whose centre is nearest the pointer on the screen). The words go to the status line (statusWords; the Waypoint screen draws none, as inteng's:
// the owner's question, 11-18, so for now they are the state line's). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include "selection.h"

// Waypoint mode's left click in the 3D view (modes 1 and 2), with what the game's line of sight met there (picked), the click in the view's own pixels, and the keys held at the press
// (Shift, Ctrl: a drag from a box links with them)
void WaypointMouseClick(host::PickReport const &picked, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight, bool shift, bool ctrl);

// The left button let go at (x, y), the editor's own pixels: after a press that took a box, a release at least a few pixels away is a DRAG, whose end the next frame's pick takes
// (it asks that pick: true); else false and nothing happens
bool WaypointMouse_Release(int32_t x, int32_t y);
