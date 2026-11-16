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

// The game's small font string at a place (menu_gfx.cpp): the same drawing RenderSmallMenuText ends in, without its debug-only assert that x is above 0, so a line at inteng's x 0 lands on the window's left edge exactly
extern int RenderSmallFontString(char *textPtr, int sx, int sy, int alpha, int red, int green, int blue);

namespace
{
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
	void DrawLine(const std::string &text, int32_t x, int32_t y)
	{
		char letters[host::kPaintMostLetters + 1];
		size_t const length = text.size() < host::kPaintMostLetters ? text.size() : host::kPaintMostLetters;
		memcpy(letters, text.data(), length);
		letters[length] = '\0';
		RenderSmallFontString(letters, x, y, ONE_FIXED, ONE_FIXED, ONE_FIXED, ONE_FIXED);   // = RenderSmallMenuText(letters, x, y, ONE_FIXED, AVPMENUFORMAT_LEFTJUSTIFIED)
	}

	// The extra boxes, each cut to the window: plain colour, no picture, as the game's own scope draws its bars (weapon_scope.cpp)
	void AddBoxes(const PanelExtras &extras, int32_t width, int32_t height)
	{
		size_t const boxes = extras.boxes ? (extras.boxCount < kPanelPassMostExtras ? extras.boxCount : kPanelPassMostExtras) : 0;

		for (size_t i = 0; i < boxes; i++)
		{
			host::PaintBox cut;

			if (host::PanelPaint_CutBox(extras.boxes[i], width, height, &cut))
			{
				DrawQuad((uint32_t)cut.x, (uint32_t)cut.y, (uint32_t)cut.width, (uint32_t)cut.height, NO_TEXTURE, cut.colour, TRANSLUCENCY_OFF);
			}
		}
	}

	// The extra lines: made safe again, and only those whose place is inside the window (a line at x 0 is on the left edge)
	void AddLines(const PanelExtras &extras, int32_t width, int32_t height)
	{
		size_t const extraLines = extras.lines ? (extras.lineCount < kPanelPassMostExtras ? extras.lineCount : kPanelPassMostExtras) : 0;

		for (size_t i = 0; i < extraLines; i++)
		{
			host::PaintText const &line = extras.lines[i];

			if (line.x < 0 || line.y < 0 || line.x >= width || line.y >= height)
			{
				continue;
			}

			DrawLine(host::PanelPaint_SafeText(line.text), line.x, line.y);
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

		// 1. The extra boxes
		AddBoxes(*frame->extras, frame->width, frame->height);

		// 2. Each panel's own line
		host::PaintText lines[4];
		size_t const count = host::PanelPaint_Plan(*frame->layout, *frame->texts, PanelPass_SmallWidth, kPanelPassLineHeight, lines, 4);

		for (size_t i = 0; i < count; i++)
		{
			DrawLine(lines[i].text, lines[i].x, lines[i].y);
		}

		// 3. The extra lines, last
		AddLines(*frame->extras, frame->width, frame->height);
	}
}

void PanelPass_AddToFrame(int32_t width, int32_t height, const PanelExtras &extras)
{
	if (width < 1 || height < 1)
	{
		return;
	}

	// The level may have been drawn as lines (the View menu's Wireframe): the panels never are, as in PanelPass_Draw below. The boxes are
	// added before the lines, so a line is over every box.
	CheckWireFrameMode(0);
	AddBoxes(extras, width, height);
	AddLines(extras, width, height);
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
