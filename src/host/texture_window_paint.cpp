// texture_window_paint.cpp: see texture_window_paint.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "3dc.h"
#include "module.h"         // MODULE, which d3d_render.h needs
#include "inline.h"
#include "renderer.h"
#include "renderStates.h"   // TRANSLUCENCY_OFF
#include "d3_func.h"        // NO_TEXTURE
#include "d3d_render.h"     // CheckWireFrameMode
#include "menus.h"
#include "menu_gfx.hpp"

#include "TextureManager.h"   // Tex_GetTextureDetails: the picture's stored size (its texture coordinates)

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "texture_window_paint.h"
#include "panel_pass.h"        // PanelPass_DrawText: the words in inteng's font
#include "shape_picker.h"      // ShapePicker_TileLines: a tile's words broken at spaces
#include "text_scale.h"        // the editor's own pixels to the window's (the owner's text scale; nothing changes at 1.0)
#include "texture_window.h"    // model::WindowDots: inteng's dotted line

namespace
{
	void Box(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour, int32_t windowWidth, int32_t windowHeight)
	{
		// cut to the window: the game's quad takes unsigned places
		if (x < 0)
		{
			width += x;
			x = 0;
		}

		if (y < 0)
		{
			height += y;
			y = 0;
		}

		if (width <= 0 || height <= 0 || x >= windowWidth || y >= windowHeight)
		{
			return;
		}

		if (x + width > windowWidth)
		{
			width = windowWidth - x;
		}

		if (y + height > windowHeight)
		{
			height = windowHeight - y;
		}

		int32_t boxX = 0, boxY = 0, boxWidth = 0, boxHeight = 0;
		host::TextScale_BoxToWindow(x, y, width, height, &boxX, &boxY, &boxWidth, &boxHeight);   // cut in the editor's own pixels, drawn in the window's
		DrawQuad((uint32_t)boxX, (uint32_t)boxY, (uint32_t)boxWidth, (uint32_t)boxHeight, NO_TEXTURE, colour, TRANSLUCENCY_OFF);
	}

	// A line as a row of 2 by 2 boxes, one every 2 pixels along its longer side (at most 1500)
	void Segment(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour, int32_t windowWidth, int32_t windowHeight)
	{
		int32_t const dx = x1 - x0, dy = y1 - y0;
		int32_t const longest = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
		int32_t const steps = longest / 2 > 1500 ? 1500 : longest / 2;

		for (int32_t i = 0; i <= steps; i++)
		{
			int32_t const x = steps ? x0 + (int32_t)((int64_t)dx * i / steps) : x0;
			int32_t const y = steps ? y0 + (int32_t)((int64_t)dy * i / steps) : y0;

			Box(x - 1, y - 1, 2, 2, colour, windowWidth, windowHeight);
		}
	}

}

namespace
{
	// A node's marker is drawn only when the node is in the window (FUN_00556250: the markers of the nodes inside it)
	bool InWindow(const int32_t window[4], int32_t x, int32_t y)
	{
		return x >= window[0] && x < window[2] && y >= window[1] && y < window[3];
	}

	// A node's marker as inteng draws it (FUN_005535a0 fills the 9 x 9 sprites, FUN_00553ab0 blits them with the source colour key): a node's is a plus three pixels thick, its middle row
	// and column white, the rows and columns either side blue; the current node's a one-pixel blue plus. NAMED: these are inteng's 16-bit colours (0xffff white, 0x001f blue); its
	// 24 / 32-bit screens draw no marker at all (FUN_005535a0 returns at 0x5537f5), which is not copied
	void Marker(int32_t x, int32_t y, bool current, int32_t windowWidth, int32_t windowHeight)
	{
		uint32_t const white = 0xFFFFFFFFu, blue = 0xFF0000FFu;

		if (current)
		{
			Box(x - 4, y, 9, 1, blue, windowWidth, windowHeight);
			Box(x, y - 4, 1, 9, blue, windowWidth, windowHeight);
			return;
		}

		Box(x - 4, y - 1, 9, 3, blue, windowWidth, windowHeight);
		Box(x - 1, y - 4, 3, 9, blue, windowWidth, windowHeight);
		Box(x - 4, y, 9, 1, white, windowWidth, windowHeight);
		Box(x, y - 4, 1, 9, white, windowWidth, windowHeight);
	}

	// Inteng's dotted line (FUN_005564b0, model::WindowDots): one-pixel dots every 3 pixels, black and white by turns, crawling with the phase, only those inside the window
	void Dots(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int phase, const int32_t window[4], int32_t windowWidth, int32_t windowHeight)
	{
		static std::vector<model::WindowDot> dots;

		model::WindowDots(x0, y0, x1, y1, phase, &dots);

		for (model::WindowDot const &dot : dots)
		{
			if (InWindow(window, dot.x, dot.y))
			{
				Box(dot.x, dot.y, 1, 1, dot.white ? 0xFFFFFFFFu : 0xFF000000u, windowWidth, windowHeight);
			}
		}
	}

	int32_t FloorDiv(int32_t a, int32_t b)
	{
		int32_t const q = a / b;

		return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
	}

	// The view stretched onto the window, one quad for each repeat of the picture it takes in (at most 32 each way), with the game's point filtering
	void ViewPicture(const TextureWindowPicture &picture, int32_t windowWidth, int32_t windowHeight)
	{
		int32_t const pw = picture.pictureWidth, ph = picture.pictureHeight;
		int32_t const vl = picture.view[0], vt = picture.view[1], vr = picture.view[2], vb = picture.view[3];
		int32_t const wl = picture.window[0], wt = picture.window[1], wr = picture.window[2], wb = picture.window[3];

		if (picture.texture < 0 || pw < 1 || ph < 1 || vr <= vl || vb <= vt || wr <= wl || wb <= wt)
		{
			return;
		}

		Texture const &details = Tex_GetTextureDetails((texID_t)picture.texture);

		if (details.realWidth < 1 || details.realHeight < 1)
		{
			return;
		}

		double const sx = (double)(wr - wl) / (vr - vl), sy = (double)(wb - wt) / (vb - vt);
		int32_t const firstX = FloorDiv(vl, pw), lastX = FloorDiv(vr - 1, pw), firstY = FloorDiv(vt, ph), lastY = FloorDiv(vb - 1, ph);

		for (int32_t ty = firstY; ty <= lastY && ty - firstY < 32; ty++)
		{
			for (int32_t tx = firstX; tx <= lastX && tx - firstX < 32; tx++)
			{
				int32_t const a0 = vl > tx * pw ? vl : tx * pw, a1 = vr < (tx + 1) * pw ? vr : (tx + 1) * pw;
				int32_t const b0 = vt > ty * ph ? vt : ty * ph, b1 = vb < (ty + 1) * ph ? vb : (ty + 1) * ph;
				int32_t const x0 = wl + (int32_t)floor((a0 - vl) * sx), x1 = wl + (int32_t)floor((a1 - vl) * sx);
				int32_t const y0 = wt + (int32_t)floor((b0 - vt) * sy), y1 = wt + (int32_t)floor((b1 - vt) * sy);

				if (x1 <= x0 || y1 <= y0 || x0 < 0 || y0 < 0 || x1 > windowWidth || y1 > windowHeight)
				{
					continue;
				}

				float const u0 = (float)(a0 - tx * pw) / details.realWidth, u1 = (float)(a1 - tx * pw) / details.realWidth;
				float const v0 = (float)(b0 - ty * ph) / details.realHeight, v1 = (float)(b1 - ty * ph) / details.realHeight;
				float uv[8] = { u0, v1, u0, v0, u1, v1, u1, v0 };   // DrawFontQuad's order: bottom left, top left, bottom right, top right

				DrawFontQuad((uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0), (texID_t)picture.texture, uv, 0xFFFFFFFFu, TRANSLUCENCY_OFF);
			}
		}
	}
}

void TextureWindowPaint_Draw(int32_t windowWidth, int32_t windowHeight, const TextureWindowPicture &picture, const std::vector<TextureWindowWords> &words)
{
	if (windowWidth < 1 || windowHeight < 1)
	{
		return;
	}

	CheckWireFrameMode(0);

	// the picture: the view on the window, over the level (FUN_00556250 draws no background: the level and its selected polygons stay in sight, as I (0x105) needs)
	ViewPicture(picture, windowWidth, windowHeight);

	// the outline (inteng's dotted line, FUN_005564b0), then the nodes over it
	for (int32_t i = 0; i < picture.nodeCount && picture.nodeCount >= 3 && picture.nodeCount <= 4; i++)
	{
		int32_t const next = (i + 1) % picture.nodeCount;

		Dots(picture.nodeX[i], picture.nodeY[i], picture.nodeX[next], picture.nodeY[next], picture.dotPhase, picture.window, windowWidth, windowHeight);
	}

	for (int32_t i = 0; i < picture.nodeCount && i < 4; i++)
	{
		bool const active = i == picture.activeNode;

		if (InWindow(picture.window, picture.nodeX[i], picture.nodeY[i]))
		{
			Marker(picture.nodeX[i], picture.nodeY[i], active, windowWidth, windowHeight);   // the current node's sprite (0x5563d8: the node is +0x88)
		}
	}

	// the words, each at its place
	for (TextureWindowWords const &line : words)
	{
		if (line.y + 15 < windowHeight)
		{
			PanelPass_DrawText(line.text.substr(0, 199), line.x, line.y, 0xFFFFFFFFu);   // inteng's font when it was read (view-state-table.md H-1i)
		}
	}
}

void TextureColourPaint_Grid(int32_t windowWidth, int32_t windowHeight, int32_t gridWidth, int32_t gridHeight, const std::vector<uint32_t> &colours)
{
	int32_t const cellW = gridWidth / 16, cellH = gridHeight / 16;

	if (cellW < 1 || cellH < 1)
	{
		return;
	}

	CheckWireFrameMode(0);

	for (size_t i = 0; i < colours.size() && i < 256; i++)
	{
		Box((int32_t)(i % 16) * cellW, (int32_t)(i / 16) * cellH, cellW, cellH, colours[i], windowWidth, windowHeight);
	}
}

void TextureWindowPaint_Nodes(int32_t windowWidth, int32_t windowHeight, const std::vector<int32_t> &x, const std::vector<int32_t> &y, const std::vector<std::pair<int32_t, int32_t>> &links,
	const std::vector<bool> &lit, const int32_t window[4], int dotPhase)
{
	CheckWireFrameMode(0);

	// the nodes first: each line is a row of the game's quads, and a frame's quads run out before many long links do; only the nodes in the window (0x5562dd)
	for (size_t i = 0; i < x.size() && i < y.size(); i++)
	{
		if (InWindow(window, x[i], y[i]))
		{
			Marker(x[i], y[i], i < lit.size() && lit[i], windowWidth, windowHeight);
		}
	}

	for (std::pair<int32_t, int32_t> const &link : links)
	{
		if (link.first >= 0 && link.second >= 0 && (size_t)link.first < x.size() && (size_t)link.second < x.size() && x.size() == y.size())
		{
			Dots(x[(size_t)link.first], y[(size_t)link.first], x[(size_t)link.second], y[(size_t)link.second], dotPhase, window, windowWidth, windowHeight);
		}
	}
}

void TextureWindowPaint_Lines(int32_t windowWidth, int32_t windowHeight, const std::vector<std::string> &lines, int32_t x, int32_t y)
{
	CheckWireFrameMode(0);

	for (size_t i = 0; i < lines.size() && y + (int32_t)i * 15 + 15 < windowHeight; i++)
	{
		char letters[200];
		size_t const length = lines[i].size() < sizeof(letters) - 1 ? lines[i].size() : sizeof(letters) - 1;

		memcpy(letters, lines[i].data(), length);
		letters[length] = '\0';
		PanelPass_DrawText(letters, x, y + (int32_t)i * 15, 0xFFFFFFFFu);   // inteng's font when it was read (view-state-table.md H-1i)
	}

	(void)windowWidth;
}

void TextureChooserPaint_Draw(int32_t windowWidth, int32_t windowHeight, int32_t areaTop, int32_t areaHeight, const std::vector<TextureChooserCell> &cells, int32_t hovered, const std::vector<std::string> &lines, int32_t lineX, int32_t lineY)
{
	if (windowWidth < 1 || windowHeight < 1)
	{
		return;
	}

	CheckWireFrameMode(0);
	Box(0, areaTop, windowWidth, areaHeight, 0xFF14181Cu, windowWidth, windowHeight);

	for (size_t i = 0; i < cells.size(); i++)
	{
		TextureChooserCell const &cell = cells[i];

		if (cell.x >= 0 && cell.y >= 0 && cell.width > 0 && cell.height > 0 && cell.x + cell.width <= windowWidth && cell.y + cell.height <= windowHeight)
		{
			int32_t cellX = 0, cellY = 0, cellWidth = 0, cellHeight = 0;
			host::TextScale_BoxToWindow(cell.x, cell.y, cell.width, cell.height, &cellX, &cellY, &cellWidth, &cellHeight);   // in the window's pixels (text_scale.h)
			DrawQuad((uint32_t)cellX, (uint32_t)cellY, (uint32_t)cellWidth, (uint32_t)cellHeight, (texID_t)cell.texture, 0xFFFFFFFFu, TRANSLUCENCY_OFF);
		}

		if ((int32_t)i == hovered)
		{
			Segment(cell.x, cell.y, cell.x + cell.width, cell.y, 0xFFFFFF00u, windowWidth, windowHeight);
			Segment(cell.x + cell.width, cell.y, cell.x + cell.width, cell.y + cell.height, 0xFFFFFF00u, windowWidth, windowHeight);
			Segment(cell.x + cell.width, cell.y + cell.height, cell.x, cell.y + cell.height, 0xFFFFFF00u, windowWidth, windowHeight);
			Segment(cell.x, cell.y + cell.height, cell.x, cell.y, 0xFFFFFF00u, windowWidth, windowHeight);
		}
	}

	TextureWindowPaint_Lines(windowWidth, windowHeight, lines, lineX, lineY);
}

void ShapePickerPaint_Backdrops(int32_t windowWidth, int32_t windowHeight, const std::vector<ShapePickerPaintTile> &tiles)
{
	// black, alpha 128 of 255: half. The game backs its own words the same way (d3d_render.cpp's "dark backing": DrawQuad, NO_TEXTURE, which is a white 1x1
	// texture, so the colour is what is drawn, its alpha blended by TRANSLUCENCY_NORMAL). The game's 2D list draws things in the order they are added (it is
	// never sorted), so whatever is drawn after this goes over it.
	uint32_t const kHalfBlack = 0x80000000u;

	if (windowWidth < 1 || windowHeight < 1)
	{
		return;
	}

	for (ShapePickerPaintTile const &tile : tiles)
	{
		if (tile.x < 0 || tile.y < 0 || tile.side <= 0 || tile.x + tile.side > windowWidth || tile.y + tile.side + tile.nameHeight > windowHeight)
		{
			continue;   // (as ShapePickerPaint_Draw leaves it out)
		}

		int32_t x = 0, y = 0, w = 0, h = 0;
		host::TextScale_BoxToWindow(tile.x, tile.y, tile.side, tile.side, &x, &y, &w, &h);   // in the window's pixels, as a still picture is (text_scale.h)
		DrawQuad((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, NO_TEXTURE, kHalfBlack, TRANSLUCENCY_NORMAL);
	}
}

void ShapePickerPaint_Draw(int32_t windowWidth, int32_t windowHeight, const std::vector<ShapePickerPaintTile> &tiles, const host::ShapePickerBar &bar, const std::vector<std::string> &lines, int32_t lineX, int32_t lineY)
{
	if (windowWidth < 1 || windowHeight < 1)
	{
		return;
	}

	CheckWireFrameMode(0);   // (no ground: the level stays in sight round the tiles, as round inteng's texture windows)

	for (ShapePickerPaintTile const &tile : tiles)
	{
		if (tile.x < 0 || tile.y < 0 || tile.side <= 0 || tile.x + tile.side > windowWidth || tile.y + tile.side + tile.nameHeight > windowHeight)
		{
			continue;
		}

		if (tile.live)
		{
			// (its shape is drawn turning in the level's frame, under these panels: its name, the frame and the bar only)
		}
		else if (tile.texture >= 0)
		{
			int32_t x = 0, y = 0, w = 0, h = 0;
			host::TextScale_BoxToWindow(tile.x, tile.y, tile.side, tile.side, &x, &y, &w, &h);   // in the window's pixels (text_scale.h)
			DrawQuad((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, (texID_t)tile.texture, 0xFFFFFFFFu, TRANSLUCENCY_OFF);
		}
		else
		{
			// its words on the chooser's ground (no colour of our own: the owner's rule, the editor looks like inteng)
			size_t const most = (size_t)(tile.side / 8 > 1 ? tile.side / 8 : 1);   // inteng's font: 8 pixels a letter at most
			std::vector<std::string> const words = host::ShapePicker_TileLines(tile.words, most, 5);   // broken at spaces (shape_picker.h)

			TextureWindowPaint_Lines(windowWidth, windowHeight, words, tile.x + 2, tile.y + 4);
		}

		// its name under it, cut to the tile's width
		size_t const fits = (size_t)(tile.side / 8 > 1 ? tile.side / 8 : 1);
		std::vector<std::string> const name = { tile.name.size() > fits ? tile.name.substr(0, fits) : tile.name };

		TextureWindowPaint_Lines(windowWidth, windowHeight, name, tile.x, tile.y + tile.side + 1);

		// TYPED PLACEMENT (the owner, 2026-11-23: "Those must have a green border around their render pane"): two pixels of green round the picture itself, the
		// line just outside it and its own edge, so the selection's yellow frame (two pixels further out, round the name too) still shows apart from it
		if (tile.typed)
		{
			for (int32_t inset = 0; inset < 2; inset++)   // (not named "in": named so, Snyk left this whole file unparsed, measured 2026-11-24)
			{
				int32_t const x0 = tile.x - 1 + inset, y0 = tile.y - 1 + inset, x1 = tile.x + tile.side - inset, y1 = tile.y + tile.side - inset;

				Segment(x0, y0, x1, y0, 0xFF00FF00u, windowWidth, windowHeight);
				Segment(x1, y0, x1, y1, 0xFF00FF00u, windowWidth, windowHeight);
				Segment(x1, y1, x0, y1, 0xFF00FF00u, windowWidth, windowHeight);
				Segment(x0, y1, x0, y0, 0xFF00FF00u, windowWidth, windowHeight);
			}
		}

		if (tile.selected)
		{
			int32_t const x0 = tile.x - 2, y0 = tile.y - 2, x1 = tile.x + tile.side + 1, y1 = tile.y + tile.side + tile.nameHeight;

			Segment(x0, y0, x1, y0, 0xFFFFFF00u, windowWidth, windowHeight);   // the chooser's yellow frame
			Segment(x1, y0, x1, y1, 0xFFFFFF00u, windowWidth, windowHeight);
			Segment(x1, y1, x0, y1, 0xFFFFFF00u, windowWidth, windowHeight);
			Segment(x0, y1, x0, y0, 0xFFFFFF00u, windowWidth, windowHeight);
		}
	}

	// THE SCROLL BAR, in the dialog lists' own look (dialogs.cpp: a box framed white, the column black, the thumb and the arrow buttons blue, each arrow a small white
	// triangle 7, 5, 3 and 1 pixels wide, as Windows draws a scroll bar's)
	if (bar.column[2] > 0)
	{
		auto framed = [&](int32_t const box[4], uint32_t inside) {
			if (box[2] > 0 && box[3] > 0)
			{
				Box(box[0], box[1], box[2], box[3], 0xFFFFFFFFu, windowWidth, windowHeight);

				if (box[2] > 2 && box[3] > 2)
				{
					Box(box[0] + 1, box[1] + 1, box[2] - 2, box[3] - 2, inside, windowWidth, windowHeight);
				}
			}
		};

		framed(bar.column, 0xFF000000u);
		framed(bar.thumb, 0xFF0000FFu);

		for (int32_t end = 0; end < 2; end++)
		{
			int32_t const *const arrow = end == 0 ? bar.up : bar.down;

			framed(arrow, 0xFF0000FFu);

			for (int32_t step = 0; step < 4; step++)   // its point up for the top arrow, down for the bottom one
			{
				int32_t const across = 7 - 2 * step;
				int32_t const y = arrow[1] + arrow[3] / 2 + (end == 0 ? 1 - step : step - 2);

				Box(arrow[0] + (arrow[2] - across) / 2, y, across, 1, 0xFFFFFFFFu, windowWidth, windowHeight);
			}
		}
	}

	TextureWindowPaint_Lines(windowWidth, windowHeight, lines, lineX, lineY);
}
