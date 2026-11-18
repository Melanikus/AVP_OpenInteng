// view_toggles_game.h: the game's own switches set from the editor's view toggles (view_toggles.h). Written 2026-11-14 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". (Modified 2026-11-14 by Melanikus
// (Frost): the headlight; R-DRAWMODES-1's outlines.)
//
// The loop calls ViewToggles_Apply once a frame, before the level is drawn (LevelView_Draw). The panels' pass puts the game's fill mode
// back to solid itself (panel_pass.cpp), so the panels are never drawn as lines.

#pragma once

#include "view_toggles.h"

// Once a frame, before the level is drawn: the game's switches set from the editor's toggles (ViewToggles_Now). Fully lit, wireframe and
// the outlines are set every frame, as Hudson sets its bright world every frame, in case anything in the game has its own idea
// (hudson.cpp:6537); the gamma only when it changes, as setting it works out a whole table (InitialiseGammaSettings). With the headlight
// on, its light is put on the player for this frame only, and the game takes it away when the frame is finished.
void ViewToggles_Apply();

// The level was opened again (LevelView_ShowFile, the view gap): what the game was given is forgotten, so the next ViewToggles_Apply gives it all again (the gamma included)
void ViewToggles_LevelOpenedAgain();

// Strategy Data started with flag 0x80 (Construction's Mode > Strategy Data: No Textures, 0x3a; FUN_00522ab0 -> FUN_0046e700(0x93)): no texture is loaded, so every textured polygon becomes a white unlit LINE (FUN_004ff010
// 0x4ffc53) and every level shape is painted white (FUN_00522ab0 0x522bca): the same white wireframe as Module Linking's, but the untextured polygons (kinds 2 / 3) stay FILLED, in the level's
// light (R_SetEditorFillUntextured), until it is turned off again.
void ViewToggles_SetPaintWhite(bool on);

// THE WHITE WIREFRAME of the screens whose start makes it (editor_main.cpp ScreenLookPass: the Construction hub FUN_00447550, Module Linking FUN_004c3a00 0x4c3a94-0x4c3aab, Adjacent
// Modules, Path, Waypoint: no textures, every level shape painted white, FUN_0046da70 0xffffff, and every polygon set to type 0xe with flag 2, FUN_0046daa0): white lines in the true
// colours, no light and no texture; the tinted modules' and objects' lines come out in their own colour. Set each frame.
void ViewToggles_SetModuleWire(bool on);

// A screen whose start loaded no textures (load flag 0x80: the hub, Module Linking, Adjacent Modules, Path, Waypoint; editor_main.cpp ScreenLookPass): its draw modes are drawn untextured,
// Polygon white and lit (FUN_0046db20), Filled polyline the hidden-line look (FUN_0046dae0). Set each frame.
void ViewToggles_SetScreenNoTextures(bool on);

// Module Linking's Mixed (inteng 0x4a, FUN_004c17a0 with DAT_00704380 set): every module drawn as lines, in inteng's Mixed colour 0xFF7E05 + 0xFB = orange (0xFF7F00), and the modules the editor tints
// (the current one and the linked ones) drawn SOLID over them. Only meaningful in the Polyline draw mode; turning it off puts the lines' own colour back.
void ViewToggles_SetMixed(bool on);
