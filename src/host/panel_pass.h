// panel_pass.h: the panels drawn once a frame, after the level, into their own swap chain, with the game's own menu text. Written 2026-11-14
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The loop calls PanelPass_Draw once a frame, after the game's frame is presented (LevelView_Draw), with the panels' chain
// (ViewSurface_PanelsChain), the parent's client size, this frame's layout, each panel's text, and what else the panels show this frame
// (the menu bar's, from its own menu_paint). It has R_DrawScreenToChain (draw_chain.h) draw, over the background colour, in this order:
// 1. the extra boxes (an open menu's background, say), each cut to the window;
// 2. each panel's own line (panel_paint.h: made safe and cut to fit its box by the game's own rules);
// 3. the extra lines, last, so an open menu is on top. Each is made safe again here, as it comes from another part of the editor, and a
//    line whose place isn't inside the window is left out.
// At most kPanelPassMostExtras of each are drawn; the rest are left out.
//
// The menu font's pictures have to be loaded once, at the editor's start (the game's LoadAllMenuTextures, after InitialiseDirect3D): that's
// the loop's, never this, as nothing can be loaded while the chain is the target.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "panel_layout.h"
#include "panel_paint.h"

struct IDirect3DSwapChain9;

// The colour the panels' chain is cleared to before their text: a dark grey (ARGB)
uint32_t const kPanelPassBackground = 0xFF202428u;

// The small menu font's lines, in pixels (HUD_FONT_HEIGHT, hud_layout.h, for the full game)
int32_t const kPanelPassLineHeight = 15;

// The most extra boxes, and the most extra lines, one frame draws
size_t const kPanelPassMostExtras = 64;

// What else the panels show this frame, besides each panel's own line: none when both counts are 0
struct PanelExtras
{
	const host::PaintBox *boxes;
	size_t boxCount;
	const host::PaintText *lines;
	size_t lineCount;
};

// Draws the panels into chain, width by height pixels (the parent's client size). Gives back whether they were drawn and presented: false
// with no chain, and whenever R_DrawScreenToChain says it couldn't.
bool PanelPass_Draw(IDirect3DSwapChain9 *chain, int32_t width, int32_t height, const ui::PanelLayout &layout, const host::PanelTexts &texts,
	const PanelExtras &extras);

// Adds extras to the game's own frame, as it is being drawn: the boxes, then the lines over them, cut to a window of width by height pixels. It
// has to be called inside the frame (LevelView_Draw's addToFrame), where the places are in pixels; the game draws them at the end of the frame,
// over the level. Nothing is drawn at once, and nothing is presented here. (Added 2026-11-15 by Melanikus (Dietrich), L-1.)
void PanelPass_AddToFrame(int32_t width, int32_t height, const PanelExtras &extras);

// How wide the small menu font draws a letter: the same rule as the game's own OnlinePage_SmallWidth (online_screens_page.cpp:176), which the
// editor doesn't compile. The letter's width from the font's table, looked up as 0 to 255, never below 0 and never above 17, its widest.
int PanelPass_SmallWidth(int letter);
