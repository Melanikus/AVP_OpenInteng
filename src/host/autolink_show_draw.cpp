// See autolink_show_draw.h. Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "3dc.h"
#include "module.h"         // MODULE, which d3d_render.h needs
#include "inline.h"
#include "renderer.h"
#include "renderStates.h"   // TRANSLUCENCY_OFF
#include "d3_func.h"
#include "d3d_render.h"
#include "TextureManager.h" // Tex_Create / Tex_Lock: one texture a tile

#include <string>

#include "autolink_show.h"
#include "autolink_show_draw.h"
#include "panel_pass.h"     // the words in inteng's font, measured with its widths
#include "text_scale.h"     // the editor's own pixels to the window's (the owner's text scale; nothing changes at 1.0)

namespace
{
	bool on = false;
	bool rightPressed = false;
	int32_t wordsAt = host::kAutoLinkWordsY;
	bool counterShown = false;
	int32_t counter = 0;
	int64_t pictures = 0;                                // pictures shown since Begin: the next one's tile
	texID_t tiles[host::kAutoLinkMostTiles];             // a texture a slot, made the first time the slot is used, kept for the next run
	bool tileMade[host::kAutoLinkMostTiles] = {};
	bool tileShown[host::kAutoLinkMostTiles] = {};       // the slot holds a picture of this run
	int32_t lastWidth = 1008;                            // the view's size at the last draw: a picture's slot is worked out with it
	int32_t lastHeight = 729;

	// One line of inteng's opaque text (FUN_0057bd40 with 0, view-state-table.md H-1e): a black cell under each letter, its width x the font's height, and the white letters over them
	void AddOpaqueLine(std::string const &words, int32_t x, int32_t y, std::vector<host::PaintBox> *boxes, std::vector<host::PaintText> *lines)
	{
		int32_t width = 0;

		for (char const letter : words)
		{
			width += PanelPass_MenuWidth((unsigned char)letter);
		}

		int32_t const height = PanelPass_MenuFontHeight() > 0 ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
		host::PaintBox cell;

		cell.x = x;
		cell.y = y;
		cell.width = width;
		cell.height = height;
		cell.colour = 0xFF000000u;
		boxes->push_back(cell);

		host::PaintText line;

		line.text = words;
		line.x = x;
		line.y = y;
		line.colour = 0xFFFFFFFFu;
		lines->push_back(line);
	}
}

void AutoLinkShow_Begin(int32_t wordsY)
{
	on = true;
	rightPressed = false;
	wordsAt = wordsY;
	counterShown = false;
	counter = 0;
	pictures = 0;

	for (bool &shown : tileShown)
	{
		shown = false;
	}
}

void AutoLinkShow_Counter(int32_t moduleNumber)
{
	if (!on)
	{
		return;
	}

	counter = moduleNumber;
	counterShown = true;
}

bool AutoLinkShow_Picture(std::vector<uint32_t> const &pixels)
{
	if (!on || pixels.size() != host::kAutoLinkPicturePixels)
	{
		return false;
	}

	int32_t slot = 0;

	host::AutoLinkShow_TilePlace(lastWidth, lastHeight, pictures, &slot, nullptr, nullptr);
	pictures++;

	if (slot < 0 || slot >= host::kAutoLinkMostTiles)
	{
		return false;
	}

	if (!tileMade[slot])
	{
		tiles[slot] = Tex_Create("editor_autolink_tile_" + std::to_string((long long)slot), (uint32_t)host::kAutoLinkTileWidth, (uint32_t)host::kAutoLinkTileHeight, 32, TextureUsage_Normal);
		tileMade[slot] = tiles[slot] != MISSING_TEXTURE;

		if (!tileMade[slot])
		{
			return false;
		}
	}

	uint8_t *data = nullptr;
	uint32_t pitch = 0;

	if (!Tex_Lock(tiles[slot], &data, &pitch) || !data || pitch < (uint32_t)host::kAutoLinkTileWidth * 4)
	{
		tileShown[slot] = false;
		return false;
	}

	for (int32_t y = 0; y < host::kAutoLinkTileHeight; y++)
	{
		uint32_t *const row = (uint32_t *)(data + (size_t)y * pitch);
		uint32_t const *const from = pixels.data() + (size_t)y * host::kAutoLinkTileWidth;

		for (int32_t x = 0; x < host::kAutoLinkTileWidth; x++)
		{
			row[x] = host::AutoLinkShow_IdColour(from[x]);
		}
	}

	Tex_Unlock(tiles[slot]);
	tileShown[slot] = true;
	return true;
}

bool AutoLinkShow_Cancelled()
{
	return on && rightPressed;
}

void AutoLinkShow_RightPress()
{
	if (on)
	{
		rightPressed = true;
	}
}

void AutoLinkShow_End()
{
	on = false;
	counterShown = false;

	for (bool &shown : tileShown)
	{
		shown = false;
	}
}

bool AutoLinkShow_On()
{
	return on;
}

void AutoLinkShow_Draw(int32_t viewWidth, int32_t viewHeight)
{
	if (!on || viewWidth < 1 || viewHeight < 1)
	{
		return;
	}

	lastWidth = viewWidth;
	lastHeight = viewHeight;
	CheckWireFrameMode(0);

	// 1. the tiles, each in its slot's place (a slot whose tile would leave the view is not drawn)
	host::AutoLinkGrid const grid = host::AutoLinkShow_Grid(viewWidth, viewHeight);

	for (int32_t slot = 0; slot < grid.columns * grid.rows && slot < host::kAutoLinkMostTiles; slot++)
	{
		int32_t x = 0, y = 0;

		if (tileShown[slot] && tileMade[slot] && host::AutoLinkShow_TilePlace(viewWidth, viewHeight, slot, nullptr, &x, &y))
		{
			int32_t tileX = 0, tileY = 0, tileWidth = 0, tileHeight = 0;
			host::TextScale_BoxToWindow(x, y, host::kAutoLinkTileWidth, host::kAutoLinkTileHeight, &tileX, &tileY, &tileWidth, &tileHeight);   // in the window's pixels (text_scale.h)
			DrawQuadSharp((uint32_t)tileX, (uint32_t)tileY, (uint32_t)tileWidth, (uint32_t)tileHeight, tiles[slot], 0xFFFFFFFFu, TRANSLUCENCY_OFF);
		}
	}

	// 2. over them, inteng's two lines: the words, and the counter once a record was made current
	std::vector<host::PaintBox> boxes;
	std::vector<host::PaintText> lines;

	AddOpaqueLine(host::kAutoLinkWords, host::kAutoLinkWordsX, wordsAt, &boxes, &lines);

	if (counterShown)
	{
		AddOpaqueLine(host::AutoLinkShow_CounterWords(counter), host::kAutoLinkCounterX, host::kAutoLinkCounterY, &boxes, &lines);
	}

	PanelExtras extras = { boxes.data(), boxes.size(), lines.data(), lines.size() };

	PanelPass_AddToFrame(viewWidth, viewHeight, extras);
}
