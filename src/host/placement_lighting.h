// placement_lighting.h: Object Placement's LIGHTING (reassess/object-placement.md OP-3, OP-44). Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// inteng's entry FUN_004d68e0 (not from the Shape Chooser, 0x4d843e) draws every module unlit (its shapes' items get flag 2, iflag_nolight, 0x4d84e6-0x4d8526) and calls FUN_004d67f0 with
// DAT_008f4f0c: the object shapes with baked colours lit (1) or unlit (0). Misc > Lighting (0x54, 0x4d9f31-0x4d9f5f) flips DAT_008f4f0c, calls it again and ticks the item by it, as the entry
// does (0x4d840f-0x4d8419). DAT_008f4f0c is .bss: 0 when inteng starts, then kept from one entry to the next. The mode's end loads the level again, so the hub draws it as the file has it.
// Ours: the look is on from the mode's start until it is left, and LevelView_PlacementLighting (level_view.h) sets the game's item bits each frame it changes. Nothing of it reaches the file.

#pragma once

// Object Placement started (inteng's entry, not the Shape Chooser's return): the look is wanted from the next frame
void PlacementLighting_Start();

// Misc > Lighting (0x54): in the mode, DAT_008f4f0c flips, *words gets the status line's words, and true; out of it, false and *words as it was (the item then keeps its old work)
bool PlacementLighting_Flip(char const **words);

// DAT_008f4f0c: the placed objects with baked colours lit, the item's tick (editor_panels.cpp)
bool PlacementLightingOn();

// Each frame before the level is drawn, with the screen's name: off the mode's own screens the look ends (the level drawn as the file has it again); only what changed since the last frame
// is done (the look's start or end, a flip, a level opened again)
void PlacementLighting_Frame(char const *screen);
