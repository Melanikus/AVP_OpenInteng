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

#include <stdlib.h>
#include <string.h>

#include "texture_window_paint.h"
#include "panel_pass.h"        // PanelPass_DrawText: the words in inteng's font

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

		DrawQuad((uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height, NO_TEXTURE, colour, TRANSLUCENCY_OFF);
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

void TextureWindowPaint_Draw(int32_t windowWidth, int32_t windowHeight, const TextureWindowPicture &picture, const char *const *lines, int32_t lineCount, int32_t lineX, int32_t lineY)
{
	if (windowWidth < 1 || windowHeight < 1)
	{
		return;
	}

	CheckWireFrameMode(0);

	// the dark box over the level
	Box(0, picture.areaTop, windowWidth, picture.areaHeight, 0xFF14181Cu, windowWidth, windowHeight);

	// the picture
	// the picture, repeated; the game's quad takes the whole picture, so a repeat that is not wholly inside the box is left out
	int32_t const tilesX = picture.tilesX < 1 ? 1 : (picture.tilesX > 10 ? 10 : picture.tilesX);
	int32_t const tilesY = picture.tilesY < 1 ? 1 : (picture.tilesY > 10 ? 10 : picture.tilesY);

	if (picture.texture >= 0 && picture.width > 0 && picture.height > 0)
	{
		for (int32_t ty = 0; ty < tilesY; ty++)
		{
			for (int32_t tx = 0; tx < tilesX; tx++)
			{
				int32_t const x = picture.x + tx * picture.width, y = picture.y + ty * picture.height;

				if (x >= 0 && y >= 0 && x + picture.width <= windowWidth && y + picture.height <= picture.areaTop + picture.areaHeight && y + picture.height <= windowHeight)
				{
					DrawQuad((uint32_t)x, (uint32_t)y, (uint32_t)picture.width, (uint32_t)picture.height, (texID_t)picture.texture, 0xFFFFFFFFu, TRANSLUCENCY_OFF);
				}
			}
		}
	}

	// the outline, then the nodes over it
	for (int32_t i = 0; i < picture.nodeCount && picture.nodeCount >= 3 && picture.nodeCount <= 4; i++)
	{
		int32_t const next = (i + 1) % picture.nodeCount;

		Segment(picture.nodeX[i], picture.nodeY[i], picture.nodeX[next], picture.nodeY[next], 0xFFFFFF00u, windowWidth, windowHeight);
	}

	for (int32_t i = 0; i < picture.nodeCount && i < 4; i++)
	{
		bool const active = i == picture.activeNode;

		Box(picture.nodeX[i] - 4, picture.nodeY[i] - 4, 9, 9, active ? 0xFFFF4040u : 0xFFFFFFFFu, windowWidth, windowHeight);
	}

	// the words
	for (int32_t i = 0; i < lineCount && lineY + i * 15 + 15 < windowHeight; i++)
	{
		char letters[200];
		size_t const length = strlen(lines[i]) < sizeof(letters) - 1 ? strlen(lines[i]) : sizeof(letters) - 1;

		memcpy(letters, lines[i], length);
		letters[length] = '\0';
		PanelPass_DrawText(letters, lineX, lineY + i * 15, 0xFFFFFFFFu);   // inteng's font when it was read (view-state-table.md H-1i)
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
			DrawQuad((uint32_t)cell.x, (uint32_t)cell.y, (uint32_t)cell.width, (uint32_t)cell.height, (texID_t)cell.texture, 0xFFFFFFFFu, TRANSLUCENCY_OFF);
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
