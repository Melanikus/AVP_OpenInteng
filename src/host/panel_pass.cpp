// See panel_pass.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "3dc.h"
#include "module.h"         // MODULE, which d3d_render.h needs
#include "inline.h"
#include "renderer.h"
#include "renderStates.h"   // TRANSLUCENCY_OFF
#include "d3_func.h"        // NO_TEXTURE
#include "d3d_render.h"     // CheckWireFrameMode
#include "menus.h"
#include "menu_gfx.hpp"
#include "hud_layout.h"     // AAFontWidths, the small font's letter widths

#include <string.h>

#include "draw_chain.h"
#include "panel_pass.h"
#include "menu_font.h"
#include "text_scale.h"       // the editor's own pixels to the window's (the owner's text scale; nothing changes at 1.0)
#include "TextureManager.h"   // Tex_Create / Tex_Lock: one texture a letter of inteng's menu font

// The game's small font string at a place (menu_gfx.cpp): the same drawing RenderSmallMenuText ends in, without its debug-only assert that x is above 0, so a line at inteng's x 0 lands on the window's left edge exactly
extern int RenderSmallFontString(char *textPtr, int sx, int sy, int alpha, int red, int green, int blue);

namespace
{
	// inteng's menu font, once set: its widths and height, and a texture a letter (MISSING_TEXTURE for none)
	bool menuFontSet = false;
	int32_t menuFontHeight = 0;
	int32_t menuFontCell = 0;
	int32_t menuFontWidths[host::kMenuFontLetters] = {};
	texID_t menuFontLetters[host::kMenuFontLetters];

	// A line in inteng's menu font: each letter 0 to 127 its texture, unfiltered, solid in the colour (TRANSLUCENCY_NORMAL with the texture's 0 / 255 alpha), the next letter its width on
	void DrawMenuFontLine(const std::string &text, int32_t x, int32_t y, uint32_t colour)
	{
		int64_t at = x;

		for (size_t i = 0; i < text.size() && i < host::kPaintMostLetters; i++)
		{
			unsigned const letter = (unsigned char)text[i];

			if (letter >= host::kMenuFontLetters)
			{
				continue;   // the font has 128 letters: anything past them is not drawn
			}

			// the letter's cell in the window's pixels (text_scale.h): inteng's own at 1.0; at another scale each of its pixels a block of that size, the cell's edges rounded so the
			// letters meet as they did
			int32_t cellX = 0, cellY = 0, cellWidth = 0, cellHeight = 0;
			host::TextScale_BoxToWindow((int32_t)(at < 16384 ? at : 16384), y, menuFontCell, menuFontHeight, &cellX, &cellY, &cellWidth, &cellHeight);

			if (at >= 0 && at < 16384 && cellX < 16384)
			{
				DrawQuadSharp((uint32_t)cellX, (uint32_t)cellY, (uint32_t)cellWidth, (uint32_t)cellHeight, menuFontLetters[letter], colour, TRANSLUCENCY_NORMAL);
			}

			at += menuFontWidths[letter];
		}
	}

	// What the fill needs from the frame: where the panels are, what each one says, what else is shown, and the window's size
	struct PanelFrame
	{
		const ui::PanelLayout *layout;
		const host::PanelTexts *texts;
		const PanelExtras *extras;
		int32_t width;
		int32_t height;
	};

	// One line on the game's screen list: a copy of its letters, at most kPaintMostLetters and always ending in 0, as the game's call takes
	// text it may not change as a plain char *. Fully opaque, as the game's own pages draw it (ONE_FIXED), from the line's left.
	void DrawLine(const std::string &text, int32_t x, int32_t y, uint32_t colour = 0xFFFFFFFFu)
	{
		char letters[host::kPaintMostLetters + 1];
		size_t const length = text.size() < host::kPaintMostLetters ? text.size() : host::kPaintMostLetters;
		memcpy(letters, text.data(), length);
		letters[length] = '\0';
		// the font's white letters times the colour's parts (ONE_FIXED each for white: RenderSmallMenuText(letters, x, y, ONE_FIXED, AVPMENUFORMAT_LEFTJUSTIFIED))
		int const red = (int)(((colour >> 16) & 0xFFu) * (uint32_t)ONE_FIXED / 255u);
		int const green = (int)(((colour >> 8) & 0xFFu) * (uint32_t)ONE_FIXED / 255u);
		int const blue = (int)((colour & 0xFFu) * (uint32_t)ONE_FIXED / 255u);

		// (the game's small font has its own fixed size: only its place follows the text scale, text_scale.h; it is drawn only when inteng's font16.fnt isn't found)
		RenderSmallFontString(letters, host::TextScale_ToWindow(x), host::TextScale_ToWindow(y), ONE_FIXED, red, green, blue);
	}

	// The extra boxes of one layer (0 the screen's own, 1 the overlay, 2 the top: PanelPaint_LayerOf), each cut to the window: plain colour, no picture, as the game's own scope draws its bars (weapon_scope.cpp)
	void AddBoxes(const PanelExtras &extras, int32_t width, int32_t height, int layer)
	{
		size_t const boxes = extras.boxes ? (extras.boxCount < kPanelPassMostExtras ? extras.boxCount : kPanelPassMostExtras) : 0;

		for (size_t i = 0; i < boxes; i++)
		{
			host::PaintBox cut;

			if (host::PanelPaint_LayerOf(extras.boxes[i]) == layer && host::PanelPaint_CutBox(extras.boxes[i], width, height, &cut))
			{
				// cut in the editor's own pixels (the window's size divided by the text scale), drawn in the window's (text_scale.h)
				int32_t boxX = 0, boxY = 0, boxWidth = 0, boxHeight = 0;
				host::TextScale_BoxToWindow(cut.x, cut.y, cut.width, cut.height, &boxX, &boxY, &boxWidth, &boxHeight);
				DrawQuad((uint32_t)boxX, (uint32_t)boxY, (uint32_t)boxWidth, (uint32_t)boxHeight, NO_TEXTURE, cut.colour, TRANSLUCENCY_OFF);
			}
		}
	}

	// The extra lines of one layer: made safe again, and only those whose place is inside the window (a line at x 0 is on the left edge)
	void AddLines(const PanelExtras &extras, int32_t width, int32_t height, int layer)
	{
		size_t const extraLines = extras.lines ? (extras.lineCount < kPanelPassMostExtras ? extras.lineCount : kPanelPassMostExtras) : 0;

		for (size_t i = 0; i < extraLines; i++)
		{
			host::PaintText const &line = extras.lines[i];

			if (host::PanelPaint_LayerOf(line) != layer || line.x < 0 || line.y < 0 || line.x >= width || line.y >= height)
			{
				continue;
			}

			// inteng draws every text in its one font (view-state-table.md H-1a..H-1d): with it read, every line is drawn in it; a menu's line keeps its glyphs 1 and 2 (the tick, a
			// sub-menu's mark), any other line is made safe first
			if (menuFontSet)
			{
				DrawMenuFontLine(line.menuFont ? line.text : host::PanelPaint_SafeText(line.text), line.x, line.y, line.colour);
				continue;
			}

			DrawLine(host::PanelPaint_SafeText(line.text), line.x, line.y, line.colour);
		}
	}

	// Runs inside R_DrawScreenToChain, with the chain as the target: puts the boxes and lines on the game's screen list, which
	// R_DrawScreenToChain then draws. Nothing is loaded or reset here.
	void FillPanels(void *context)
	{
		PanelFrame const *const frame = (PanelFrame const *)context;

		if (!frame || !frame->layout || !frame->texts || !frame->extras)
		{
			return;
		}

		// 1. The screen's own extra boxes
		AddBoxes(*frame->extras, frame->width, frame->height, 0);

		// 2. Each panel's own line
		host::PaintText lines[4];
		size_t const count = host::PanelPaint_Plan(*frame->layout, *frame->texts, PanelPass_MenuWidth, kPanelPassLineHeight, lines, 4);

		for (size_t i = 0; i < count; i++)
		{
			if (menuFontSet)
			{
				DrawMenuFontLine(lines[i].text, lines[i].x, lines[i].y, 0xFFFFFFFFu);
				continue;
			}

			DrawLine(lines[i].text, lines[i].x, lines[i].y);
		}

		// 3. The screen's own extra lines
		AddLines(*frame->extras, frame->width, frame->height, 0);

		// 4. Over all of it, the overlay: the bar, its drop-downs, the pop-up, a dialog (BAR-6: inteng blits them opaque over the frame), their boxes and then their words
		AddBoxes(*frame->extras, frame->width, frame->height, 1);
		AddLines(*frame->extras, frame->width, frame->height, 1);

		// 5. Over that, the top: a dialog's open drop-down list, its boxes and then its words, so no word of its window shows through it (the owner's screenshot, 11-18)
		AddBoxes(*frame->extras, frame->width, frame->height, 2);
		AddLines(*frame->extras, frame->width, frame->height, 2);
	}
}

void PanelPass_AddToFrame(int32_t width, int32_t height, const PanelExtras &extras, void (*underTop)(int32_t width, int32_t height))
{
	if (width < 1 || height < 1)
	{
		if (underTop)
		{
			underTop(width, height);   // nothing of the panels to draw; what plays under the top is still called (panel_pass.h)
		}

		return;
	}

	// The level may have been drawn as lines (the View menu's Wireframe): the panels never are, as in PanelPass_Draw below. The boxes are
	// added before the lines, so a line is over every box; the overlay (the bar, its drop-downs, the pop-up, a dialog) after both, so nothing of the screen is over it.
	CheckWireFrameMode(0);
	AddBoxes(extras, width, height, 0);
	AddLines(extras, width, height, 0);
	AddBoxes(extras, width, height, 1);
	AddLines(extras, width, height, 1);

	if (underTop)
	{
		underTop(width, height);   // what plays inside a window (the FMV preview): over the window, under its open list
	}

	AddBoxes(extras, width, height, 2);   // a dialog's open drop-down list, over its window's words
	AddLines(extras, width, height, 2);
}

bool PanelPass_SetMenuFont(const host::MenuFont &font)
{
	if (font.height <= 0 || font.rowBytes <= 0 || font.bits.size() != host::kMenuFontLetters * (size_t)font.height * (size_t)font.rowBytes)
	{
		return false;
	}

	int32_t const cell = font.rowBytes * 8;
	texID_t made[host::kMenuFontLetters];

	for (size_t letter = 0; letter < host::kMenuFontLetters; letter++)
	{
		made[letter] = Tex_Create("editor_menu_font_" + std::to_string((unsigned long long)letter), (uint32_t)cell, (uint32_t)font.height, 32, TextureUsage_Normal);

		uint8_t *data = nullptr;
		uint32_t pitch = 0;

		if (made[letter] == MISSING_TEXTURE || !Tex_Lock(made[letter], &data, &pitch) || !data || pitch < (uint32_t)cell * 4)
		{
			for (size_t back = 0; back <= letter; back++)
			{
				if (made[back] != MISSING_TEXTURE)
				{
					Tex_Release(made[back]);
				}
			}

			return false;
		}

		for (int32_t y = 0; y < font.height; y++)
		{
			uint32_t *const row = (uint32_t *)(data + (size_t)y * pitch);

			for (int32_t x = 0; x < cell; x++)
			{
				row[x] = host::MenuFont_Pixel(font, (unsigned)letter, x, y) ? 0xFFFFFFFFu : 0x00FFFFFFu;
			}
		}

		Tex_Unlock(made[letter]);
	}

	memcpy(menuFontLetters, made, sizeof(menuFontLetters));
	memcpy(menuFontWidths, font.widths, sizeof(menuFontWidths));
	menuFontHeight = font.height;
	menuFontCell = cell;
	menuFontSet = true;
	return true;
}

bool PanelPass_HasMenuFont()
{
	return menuFontSet;
}

int PanelPass_MenuWidth(int letter)
{
	if (!menuFontSet)
	{
		return PanelPass_SmallWidth(letter);
	}

	return letter >= 0 && letter < (int)host::kMenuFontLetters ? menuFontWidths[letter] : 0;
}

int32_t PanelPass_MenuFontHeight()
{
	return menuFontSet ? menuFontHeight : 0;
}

void PanelPass_DrawText(const std::string &text, int32_t x, int32_t y, uint32_t colour)
{
	if (x < 0 || y < 0)
	{
		return;
	}

	std::string const safe = host::PanelPaint_SafeText(text);

	if (menuFontSet)
	{
		DrawMenuFontLine(safe, x, y, colour);
		return;
	}

	DrawLine(safe, x, y, colour);
}

int PanelPass_SmallWidth(int letter)
{
	int const width = AAFontWidths[letter & 255];
	return width < 0 ? 0 : (width > 17 ? 17 : width);
}

bool PanelPass_Draw(IDirect3DSwapChain9 *chain, int32_t width, int32_t height, const ui::PanelLayout &layout, const host::PanelTexts &texts,
	const PanelExtras &extras)
{
	// R_DrawScreenToChain refuses a side outside 1 to 16384 itself; below 1 there's nothing to draw anyway
	if (!chain || width < 1 || height < 1)
	{
		return false;
	}

	// The level may have been drawn as lines (the View menu's Wireframe): the panels never are. The game's own switch puts the fill mode back
	// to solid, so its record of the card's state stays right (CheckWireFrameMode, d3_func.cpp:988); the game sets it again for each of the
	// level's own draws next frame.
	CheckWireFrameMode(0);

	PanelFrame frame = { &layout, &texts, &extras, width, height };
	return R_DrawScreenToChain(chain, (uint32_t)width, (uint32_t)height, kPanelPassBackground, FillPanels, &frame);
}
