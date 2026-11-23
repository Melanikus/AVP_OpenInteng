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

// The most extra boxes, and the most extra lines, one frame draws. 256 since 2026-11-16 (Vasquez): at 64 a window with many parts (edobtype's "Generator Settings": about 100 boxes and 80 lines) did not fit,
// and a dialog that does not fit is not drawn at all (editor_main.cpp), so Melanikus saw no window while its keys still went to it. The letters stay well under the game's ortho buffer (4096 vertices, d3_func.cpp).
size_t const kPanelPassMostExtras = 256;

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
// underTop, when given, is called between the overlay (the bar, its drop-downs, the pop-up, a dialog) and the top (a dialog's open drop-down list), with the same width and
// height: for what plays INSIDE a window and must stay under that window's open list, as everything in an inteng dialog does: the FMV preview (fmv_preview.h; the owner
// 11-21, Message window, Play on, the number's list opened: "the menu is drawing over the video though"). Whatever it adds is drawn in that place in the frame's order.
// It is called even for a window of no size (when nothing of the panels is), as the preview was before (it keeps its clock and its state field).
// underPanels, when given, is called first, before every panel: for what plays ON THE SCREEN ITSELF and must stay under all of the panels, the status line, the bar and
// its drop-downs, the pop-up and every dialog: the Video mode's preview (fmv_preview.h FmvPreviewLayer::UnderThePanels; the owner 11-22: Save pressed during a preview
// opened its form UNDER the film, which hid it). Also called for a window of no size.
// overPanels, when given, is called after the panels (the object list, the field panel, the status line: layer 0) and before the overlay (the bar, its drop-downs, the
// pop-up, a dialog): for what is drawn OVER the view's panels and must stay under the bar and every dialog: the placement picker (shape_picker_screen.h; the owner 11-23,
// "Gray thingie over my scroll bar!" and "It should be drawing using inteng look and overlayed by our shapes": the object's field panel was drawn over the picker's scroll
// bar). Not called for a window of no size (nothing of the panels is drawn then).
void PanelPass_AddToFrame(int32_t width, int32_t height, const PanelExtras &extras, void (*underTop)(int32_t width, int32_t height) = nullptr,
	void (*underPanels)(int32_t width, int32_t height) = nullptr, void (*overPanels)(int32_t width, int32_t height) = nullptr);

// How wide the small menu font draws a letter: the same rule as the game's own OnlinePage_SmallWidth (online_screens_page.cpp:176), which the
// editor doesn't compile. The letter's width from the font's table, looked up as 0 to 255, never below 0 and never above 17, its widest.
int PanelPass_SmallWidth(int letter);

// INTENG'S MENU FONT (menu_font.h, read from its Tools folder at the start): each letter made a texture (white where a bit is set, see-through elsewhere), drawn unfiltered
// (avpx DrawQuadSharp) in the line's colour, so a menu's words are inteng's pixels in its exact colours (view-state-table.md H-5b, H-5c, H-5e). Call once at the start, after
// the game's menu textures and outside a frame; false when a texture could not be made (the game's small font is used then).
namespace host { struct MenuFont; }
bool PanelPass_SetMenuFont(const host::MenuFont &font);
bool PanelPass_HasMenuFont();

// A letter's width in the menu font when one is set, else the small font's (PanelPass_SmallWidth): EVERY text is measured with it, as every text is drawn in inteng's font
// once it was read (view-state-table.md H-1a..H-1i: inteng has one text font, its folder's font16.fnt, for the menus, the dialogs and every screen's words alike)
int PanelPass_MenuWidth(int letter);

// One line of words at (x, y) in pixels of the frame being drawn, made safe first: in inteng's font when it was read (each set bit one pixel, unfiltered, solid in the colour), else in the
// game's small font. For the parts that draw inside the game's own frame (texture_window_paint.cpp); nothing at a place left of or above the window.
void PanelPass_DrawText(const std::string &text, int32_t x, int32_t y, uint32_t colour);

// The menu font's letters' height, 0 when none is set
int32_t PanelPass_MenuFontHeight();
