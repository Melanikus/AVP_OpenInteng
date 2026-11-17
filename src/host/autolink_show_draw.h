// autolink_show_draw.h: AutoLink's display while a run is on (reassess/autolink-display-frost.md AD-2..AD-8; the arithmetic is autolink_show.h). Written 2026-11-16 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE DRIVER (Dietrich's, the frame's one writer of the link trees) calls these from the frame, OUTSIDE the draw (a picture is a texture made or filled here); the frame's draw calls
// AutoLinkShow_Draw inside the game's frame, over the level and under the menu bar. Interfaces: AvP-docs/director/autolink-interfaces-2026-11-16.md section 3.
#pragma once

#include <stdint.h>

#include <vector>

// A run begins: "Right click to cancel" at (200, wordsY) (kAutoLinkWordsY, or kAutoLinkWordsYFromFile for the run from a module file), no counter yet, an empty grid, no right press
void AutoLinkShow_Begin(int32_t wordsY);

// The record made current (inteng writes it only when FUN_004c10c0 said yes, 0x4c89fe-0x4c8a20): the counter shows it in "%03d"
void AutoLinkShow_Counter(int32_t moduleNumber);

// The next picture (320 x 240 IDs, row by row, the ID in the low bits, 0 = nothing): into the next tile of the grid. False, and nothing shown, when it is not 320 x 240 or no run is on, or its
// tile's texture could not be made.
bool AutoLinkShow_Picture(std::vector<uint32_t> const &pixels);

// A right press in the view since Begin (inteng: (DAT_00d8806a & 2) at the top of each record, 0x4c899c): the driver asks before each step and ends the run when true
bool AutoLinkShow_Cancelled();

// The window's right press while a run is on (the editor's message handler calls it, and opens no pop-up then)
void AutoLinkShow_RightPress();

// The run is over: the grid, the counter and the words go
void AutoLinkShow_End();

bool AutoLinkShow_On();

// Inside the game's frame, the view's size in pixels: the tiles, then over them each line's black cells and white letters in inteng's font (H-1e: opaque)
void AutoLinkShow_Draw(int32_t viewWidth, int32_t viewHeight);
