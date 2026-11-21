// waypoint_gizmo.h: THE WAYPOINT-BOX GIZMO on the Waypoint screen (Bishop's P4 item 4, 11-18, for the release after alpha.2; the sums in waypoint_box_gizmo.h). Written 2026-11-18 by
// Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// In mode 1 on the alien list, with a current waypoint, its box gets Drake's gizmo (the arrows and the centre ball, gizmo.h) and a handle on each face. The press on a handle starts a
// drag: the box follows live in the 3D view (WaypointMode::MouseShowBox: nothing written yet); the release WRITES it (WaypointMode::MouseSetBox, the game's loader checking it) and the
// status line says what was done and what the alien checker now finds in that AI module (waypoint_net_level.h WaypointNets_ReportModule: a REPORT, never a refusal). A click on the
// centre ball switches the arrows between the box and its aim point. Esc while dragging puts the box back; Ctrl+Z takes the last written edit back (one step). Nothing shows while a
// dialog, prompt, menu or pop-up is open, or one of the modal waits holds the frame (as Drake's gizmo, gizmo_screen.h).
// Each function is one line in editor_main.cpp, BEFORE Drake's GizmoScreen_ one of the same kind (this gizmo takes the press first on its own screen). The pointer is in the editor's own
// pixels from the view's top left (text_scale.h ToOwn), as the pick takes it.

#pragma once

#include <stdint.h>

bool WaypointGizmo_Press(int32_t x, int32_t y);                 // WM_LBUTTONDOWN on the view: true when a handle took the press (nothing else picks)
bool WaypointGizmo_Pointer(int32_t x, int32_t y, bool shift);   // WM_MOUSEMOVE on the view: true while a drag has the pointer
bool WaypointGizmo_Release();                                   // WM_LBUTTONUP: true when a drag ended (and was written), or the centre ball was clicked
bool WaypointGizmo_Key(unsigned key, bool shift, bool ctrl);    // a key's first press: true when the gizmo took it (Esc while dragging, Ctrl+Z)
void WaypointGizmo_Frame(int32_t viewWidth, int32_t viewHeight);   // each frame, AFTER GizmoScreen_Frame: its lines (only while it shows, so Drake's are never wiped)
void WaypointGizmo_StateLine();                                 // the harness's fields: boxgizmo, boxgizmopoints, boxgizmolast, waypointbox
