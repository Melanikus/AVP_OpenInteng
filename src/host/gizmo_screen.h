// gizmo_screen.h: the mouse gizmo on Object Placement (P1 of Hicks's plan .claude/plans/streamed-twirling-spindle.md; the owner's addition, inteng had none). Written 2026-11-18
// by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// With a placed object selected on Object Placement, its gizmo shows (gizmo.h: three arrows, three rings). The press on a handle starts a drag (the object follows live, as F's
// flying copy); the release writes the pose through movable.h; Esc during the drag gives it up; Ctrl+Z takes the last drag back (one step); G switches the axes between the
// world's and the object's own; Shift snaps a turn to 15 degrees. inteng's Object Placement keys are left as they are (G and Ctrl+Z are no key of its). Nothing shows while F
// flies the object, the snap is live, a dialog, prompt, menu or pop-up is open, the three views are up, or one of the modal waits holds the frame (Insert's zoom, Scroll Lock's
// pick, Move rel. to poly's).
// Each function is one line in editor_main.cpp. The pointer is in the editor's own pixels from the view's top left (text_scale.h ToOwn), as the pick takes it.

#pragma once

#include <stdint.h>

#include "movable.h"

// THE OTHER SCREENS' THINGS (P2: Frost's kinds): a screen gives the gizmo its own current thing through a source, the ref of what that screen is working on (Object Placement's
// current special object or placed hierarchy, Light Sourcing's light, Waypoint's entry marker, Track Editing's section end, Strategy Data's virtual object), kind None when
// nothing is (or one of the screen's own waits holds it). The gizmo shows on Object Placement and on every screen with a source; it moves the selection's placed object when
// that one reads, else the source's (movable.h Movable_GetFirst). Give it ONE line in the screen's own file, at start-up or when the screen begins: a later call for the same
// screen replaces the source. The screen is its name as host::kIntengScreens has it. sourceFirst: the screen's thing BEFORE the selection's (Track Editing: the tracked
// object stays the selected placed object, so the selection's would always read; Frost 11-18). A kind whose pose the editor holds in memory (movable.h Movable_PoseInLevel
// false: a track's section end, a waypoint box) is read again every frame, not kept until the level changes.
typedef host::MovableRef (*GizmoRefSource)();
void GizmoScreen_UseRefSource(char const *screen, GizmoRefSource source, bool sourceFirst = false);

bool GizmoScreen_Press(int32_t x, int32_t y);                  // WM_LBUTTONDOWN on the view: true when a handle took the press (nothing else picks); the centre ball's is left alone
bool GizmoScreen_CentreAt(int32_t x, int32_t y);               // a shown gizmo's centre ball is under the pointer, and no drag is held (Vasquez's surface placement takes that press)
bool GizmoScreen_Pointer(int32_t x, int32_t y, bool shift);    // WM_MOUSEMOVE on the view: true while a drag has the pointer
bool GizmoScreen_Release();                                    // WM_LBUTTONUP: true when a drag ended (and was written)
bool GizmoScreen_Key(unsigned key, bool shift, bool ctrl);     // a key's first press: true when the gizmo took it (G, Esc while dragging, Ctrl+Z)
void GizmoScreen_Frame(int32_t viewWidth, int32_t viewHeight, char const *screen);   // each frame, before the level is drawn: the gizmo's lines
void GizmoScreen_StateLine();                                  // the harness's fields: gizmo, gizmospace, gizmopoints, gizmolast, gizmodrawn, gizmolive, gizmolag
// THE GIZMO DRAWN WITH ITS OWN FRAME'S CAMERA (Drake 2026-11-24, the owner: "when camera moves the gizmo flickers alot/doesn't follow rest of map"): Frame runs before the
// level is drawn, when the game's camera (PickGame_Camera, Global_VDB_Ptr) is still the LAST frame's, so while the camera moved the gizmo was sized and turned for where
// the camera had been. AfterEyes makes it again with Frame's last arguments once the frame's camera is in (LevelView_SetAfterEyes); AfterDraw measures, with the camera the
// frame was drawn from, how many pixels long an arrow came out (kGizmoArrowPixels when the camera is the one it was made from): gizmolag, the most it was off since the
// state line was last written.
void GizmoScreen_AfterEyes();
void GizmoScreen_AfterDraw();
bool GizmoScreen_HoldsTheCopy();                               // a drag holds the live copy that shows the object moving: no one else takes it away meanwhile
void GizmoScreen_ViewAt(int32_t x, int32_t y);                 // the 3D view's top left in the editor's own pixels (the split screen's bottom right quarter), before each Frame
char const *GizmoScreen_LiveCopyWords();                       // the live copy that stands for a moving object: "<x> <y> <z> drawn <0|1>" (the world's units), "" when none
