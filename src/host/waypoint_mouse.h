// waypoint_mouse.h: WAYPOINT MODE BY MOUSE in the editor (the owner's addition 11-18; Hicks's plan .claude/plans/streamed-twirling-spindle.md section 6, Bishop's order P4): mode 1's
// clicks in the 3D view on the ALIEN list (the one the game loads, projload.cpp:2614-2651), the sums in waypoint_pick.h. Mode 2 (Edit Extents) and the ground list keep inteng's own
// pick (WaypointBoxClick: the box whose centre is nearest the pointer on the screen). The words go to the status line (statusWords; the Waypoint screen draws none, as inteng's:
// the owner's question, 11-18, so for now they are the state line's). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <vector>

#include "selection.h"

// Waypoint mode's left click in the 3D view (modes 1 and 2), with what the game's line of sight met there (picked), the click in the view's own pixels, and the keys held at the press
// (Shift, Ctrl: a drag from a box links with them)
void WaypointMouseClick(host::PickReport const &picked, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight, bool shift, bool ctrl);

// The left button let go at (x, y), the editor's own pixels: after a press that took a box, a release at least a few pixels away is a DRAG, whose end the next frame's pick takes
// (it asks that pick: true); else false and nothing happens
bool WaypointMouse_Release(int32_t x, int32_t y);

// THE WAYPOINT DISPLAY'S CLICK (Vasquez 2026-11-19, the owner's "see and be able to edit all alien paths and volumes across all modules"; named): on the AI modules page (mode 0), where
// every AI module's alien network is drawn, a plain click on a box of the AI module ALREADY PICKED (the first click is inteng's pick, unchanged) takes the box the line from the camera meets first among EVERY network (in front of the wall the game's line of sight met;
// not one the camera is inside: WaypointPick_BoxOnRay) and opens its AI module in mode 1 with that box current (WaypointMode::OpenAt), so inteng's editing works on it. False when it
// took no box (then the click is inteng's module pick), outside mode 0, or with no level.
bool WaypointAllClick(host::PickReport const &picked, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);

// THE MODULES' NAMES (Vasquez 2026-11-20, the owner: "I don't know module names"; named): on the Waypoint screen the mode's own top-left text (inteng's, rows from 2) is followed by
// the names he needs to find a place: the AI module whose box is under the mouse ("Under the mouse: <name> (box <n>)", among every network drawn), and in mode 0 the pick and the
// Ctrl+click set, in modes 1 to 3 the AI module being edited. WaypointNames_Pointer keeps the pointer (the editor's pixels of the view), WaypointNames_Frame looks under it each frame,
// WaypointNames_Lines appends the lines.
struct HierarchyTextLine;
void WaypointNames_Pointer(int32_t x, int32_t y);
void WaypointNames_Frame(int32_t viewWidth, int32_t viewHeight);
void WaypointNames_Lines(std::vector<HierarchyTextLine> *out);
