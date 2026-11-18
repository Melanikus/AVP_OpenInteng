// orbit_command.h: the harness's camera-orbit and camera-corner in the editor (the owner, 11-18: a test's camera "up/to side like 45 in all axis from subject", "like orbiting the subject",
// "that's how the click algorithms for humans works"):
//   "orbit|<name>[|<around>[|<up>[|<distance>]]]"   the camera round that object's box (camera_orbit.h; 45 round, 45 up and a distance from the box's size when not given), looking at its centre
//   "corner|<name>"                                 the four upper diagonal spots, 45, 135, 225 and 315 round and 45 up, tried in turn: the FIRST from which a click in the middle of the view
//                                                   picks the subject (the editor's own pick, PickGame_AtClick, on a frame drawn from that spot: a wall or another object in the way, or a
//                                                   spot inside another object, means the next spot); none: the first spot, and the words say so
// The text comes from the harness as WM_COPYDATA in a -picture run (editor_main.cpp's hook); corner's trial goes on in the frames (OrbitCommand_Frame). Written 2026-11-18 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE TEXT IS HOSTILE: at most 256 bytes of printable ASCII, the name 1 to 64 bytes, each number a plain decimal within its bound: around -3600 to 3600 degrees, up -89 to 89, distance 0 (the
// default) to a kilometre. The subject by its name: a placed object in play (its box: LevelView_ObjectBox), else a module (LevelView_ModuleBox), else "light <n>": the level's light n,
// counted from 1, its marker's box (LightMarkerBox); camera-corner keeps the spot from which the middle of the view meets that marker with nothing nearer (Light Sourcing's pick).

#pragma once

#include <stdint.h>

#include <string>

// Puts the editor's camera as the text says; *words say what was done or why not (for the status line and the log). For orbit, a line "AVP_OpenInteng: camera: x y z turn look L roll 0" is
// printed at once (the harness's camera verbs are answered so); for corner, when the trial ends (OrbitCommand_Frame). False when nothing was put.
bool OrbitCommand_Run(std::string const &text, std::string *words);

// Every frame, after it is drawn (the view's size in its own pixels): camera-corner's trial, one spot a frame or two; *words are set when the trial ends (the camera line is then printed)
void OrbitCommand_Frame(int32_t viewWidth, int32_t viewHeight, std::string *words);

// The WM_COPYDATA tag the harness's camera-orbit and camera-corner send their text with ('ORBT')
unsigned long const kOrbitCommandTag = 0x4f524254ul;
