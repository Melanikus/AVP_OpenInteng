// texture_colour_pick.cpp: the Texture Map's colours (TM-B): Select Colour (Shift+K, 0x40, inteng FUN_00560620) and Apply Colour (Ctrl+K, 0x41, FUN_00570490). Written 2026-11-18 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// AS INTENG:
//  - Select Colour covers the screen with a 16 x 16 grid of inteng's 256 colours (its table at 0x5d77c8, in the editor itself: texture_colour_table.h) and waits for a mouse press;
//    a left press in the grid takes that cell's colour word (model/texture_colour.h) as the current colour (state +0xd0) and applies it; any other press does nothing more.
//  - Apply Colour (also the end of Select Colour) takes the selection's undo step (FUN_00573570), then makes every selected polygon a plain one (type 2) of the current colour (0 at the
//    start, 0x5560ca), taking it out of its texture animation first (FUN_005413d0).
// NAMED:
//  - the grid leaves our status line (36 pixels) out, which says what the press does (inteng's covers the whole screen and says nothing);
//  - a plain polygon keeps its list of texture coordinates in the file (inteng keeps them in its own record, +0..+0x17, for an undo; whether its save writes them is UNREAD);
//  - when a polygon taken out of an animation was the current animation's last, the current animation is not let go (0x541467).
// INTENG BUG, FIXED: the 24-bit grid's colours (model::ColourShown).
// Modified 2026-11-24 by Melanikus (Frost), the owner's ruling made plain ("Dude, I was specific about RIFs": only Rebellion's RIF data stays out of our source): inteng's
// table is in the editor itself (texture_colour_table.h, its 768 bytes copied one for one from inteng.exe), so Select Colour works for a player with no inteng.exe. It was read
// from the user's own inteng.exe at run time, and without one Select Colour said so and did nothing.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "editor_shared.h"     // texturePolygons, textureWords, statusWords, TexturePolygon
#include "texture_shared.h"    // TextureUndo_RecordSelection
#include "level_objects.h"     // LevelObjects_OpenForEdit
#include "level_file.h"
#include "shape_edit.h"
#include "anim_edit.h"
#include "texture_anim.h"
#include "texture_colour_table.h"   // inteng's 256 colours, host::kIntengColourTable
#include "texture_colour.h"
#include "texture_window_paint.h"   // TextureColourPaint_Grid
#include "state_line.h"

void ShowTheWorkingBytes();   // editor_screen_keys.cpp: the view loads the editor's working bytes

bool textureColourPicking = false;
static uint32_t textureColourCurrent = 0;   // +0xd0 (0x5560ca: 0 at the start)
static uint8_t const *const textureColourTable = host::kIntengColourTable;   // inteng's own colours, r g b each 0..63 (texture_colour_table.h)
static_assert(sizeof(host::kIntengColourTable) == model::kColourCells * 3, "inteng's table holds 256 colours of 3 bytes");
static int32_t textureColourFrameW = 0, textureColourFrameH = 0;   // the frame the grid was last drawn on (the press is in its pixels)

namespace
{
	int32_t const kStatusLineHeight = 36;
}

// Ctrl+K and the end of Select Colour (FUN_00570490)
void TextureColourApply(uint32_t word)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		textureWords = "Texture Map: no level is open";
		statusWords = textureWords.c_str();
		return;
	}

	TextureUndo_RecordSelection();   // FUN_00573570 (0x570499), first of all

	if (texturePolygons.empty())
	{
		textureWords = "Texture Map: nothing is selected to colour";
		statusWords = textureWords.c_str();
		return;
	}

	struct ShapeWrite
	{
		uint8_t id[8];
		model::ShapeModel shape;
	};

	std::vector<ShapeWrite> shapes;
	std::vector<host::AnimWrite> animWrites;
	std::vector<bool> done(texturePolygons.size(), false);
	size_t left = 0, unanimated = 0;
	std::string firstWhy;

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		std::vector<int32_t> selected;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (!done[i] && one.isModule == lead.isModule && one.moduleIndex == lead.moduleIndex && memcmp(one.id, lead.id, 8) == 0)
			{
				done[i] = true;
				selected.push_back(one.polygon);
			}
		}

		ShapeWrite write;
		std::string why;

		memcpy(write.id, lead.id, 8);

		if (host::ShapeEdit_Read(open->Level(), open, lead.id, &write.shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			left += selected.size();
			firstWhy = firstWhy.empty() ? "the shape could not be read" : firstWhy;
			continue;
		}

		// a selected shape shared by two objects is one shape: each polygon once
		bool const already = std::any_of(shapes.begin(), shapes.end(), [&](ShapeWrite const &other) { return memcmp(other.id, lead.id, 8) == 0; });

		if (already)
		{
			continue;
		}

		size_t coloured = 0;

		for (int32_t const p : selected)
		{
			if (p < 0 || (size_t)p >= write.shape.polygons.size())
			{
				left++;
				continue;
			}

			write.shape.polygons[(size_t)p].engineType = 2;   // +0x18 = 2
			write.shape.polygons[(size_t)p].colour = (int32_t)word;   // +0x28 = the colour
			coloured++;
		}

		if (coloured == 0)
		{
			continue;
		}

		// FUN_005413d0: a polygon in the shape's texture animation is taken out of it
		model::TextureAnim anim;
		int32_t shapeNumber = 0;

		if (host::AnimEdit_Read(open, lead.id, &shapeNumber, &anim) == host::AnimEditResult::Done && !anim.polygons.empty())
		{
			size_t const before = anim.polygons.size();

			anim.polygons.erase(std::remove_if(anim.polygons.begin(), anim.polygons.end(), [&](model::AnimPolygon const &record) {
				return std::find(selected.begin(), selected.end(), record.polygon) != selected.end(); }), anim.polygons.end());

			if (anim.polygons.size() != before)
			{
				host::AnimWrite animWrite;

				animWrite.shapeNumber = shapeNumber;
				animWrite.anim = anim;
				animWrites.push_back(animWrite);
				unanimated += before - anim.polygons.size();
			}
		}

		shapes.push_back(write);
	}

	// the animations first (a plain polygon left in one is not the game's), then the polygons
	std::string animWhy;

	if (!animWrites.empty() && host::AnimEdit_Write(open, animWrites, &animWhy) != host::AnimEditResult::Done)
	{
		textureWords = "Texture Map: nothing coloured: the polygons could not be taken out of their animation (" + animWhy + ")";
		statusWords = textureWords.c_str();
		return;
	}

	size_t coloured = 0;

	for (ShapeWrite const &write : shapes)
	{
		std::string why;

		if (host::ShapeEdit_WriteSameCounts(open, open->Level(), write.id, write.shape, &why) != host::ShapeEditResult::Done)
		{
			firstWhy = firstWhy.empty() ? why : firstWhy;
			continue;
		}

		for (TexturePolygon const &one : texturePolygons)
		{
			coloured += memcmp(one.id, write.id, 8) == 0 && one.polygon >= 0 && (size_t)one.polygon < write.shape.polygons.size() ? 1 : 0;
		}
	}

	if (coloured)
	{
		LoadTheObjectListAgain();
		ShowTheWorkingBytes();   // the view loads the level as it is now (FUN_0055e110 makes the polygon's item again): a plain polygon is the game's I_Polygon
	}

	char colour[16];

	snprintf(colour, sizeof(colour), "%08X", (unsigned)word);
	textureWords = "Texture Map: " + std::to_string(coloured) + " polygon" + (coloured == 1 ? "" : "s") + " given the colour " + colour;

	if (unanimated)
	{
		textureWords += ", " + std::to_string(unanimated) + " taken out of their animation";
	}

	if (left || coloured < texturePolygons.size())
	{
		textureWords += ", " + std::to_string(texturePolygons.size() - coloured) + " left as they were" + (firstWhy.empty() ? "" : " (" + firstWhy + ")");
	}

	statusWords = textureWords.c_str();
}

// Ctrl+K (0x41): the current colour
void TextureColourApplyCurrent()
{
	TextureColourApply(textureColourCurrent);
}

// Shift+K (0x40): the grid, until a press
void TextureColourPickStart()
{
	textureColourPicking = true;
	textureWords = "Texture Map: Select Colour: click a colour (any other press: none)";
	statusWords = textureWords.c_str();
}

// The press while the grid is up (FUN_00560620 0x560879): true when it was the grid's
bool TextureColourPickPress(int32_t x, int32_t y, bool left)
{
	if (!textureColourPicking)
	{
		return false;
	}

	textureColourPicking = false;

	int const cell = left ? model::ColourCellAt(x, y, textureColourFrameW, textureColourFrameH - kStatusLineHeight) : -1;

	if (cell < 0)
	{
		textureWords = "Texture Map: Select Colour: no colour taken";
		statusWords = textureWords.c_str();
		return true;
	}

	textureColourCurrent = model::ColourWord(cell, textureColourTable[cell * 3], textureColourTable[cell * 3 + 1], textureColourTable[cell * 3 + 2]);
	TextureColourApply(textureColourCurrent);
	return true;
}

// The grid, while it is up (over everything but the status line)
void TextureColourDraw(int32_t frameW, int32_t frameH)
{
	if (!textureColourPicking)
	{
		return;
	}

	textureColourFrameW = frameW;
	textureColourFrameH = frameH;

	std::vector<uint32_t> colours((size_t)model::kColourCells);

	for (int i = 0; i < model::kColourCells; i++)
	{
		colours[(size_t)i] = model::ColourShown(textureColourTable[i * 3], textureColourTable[i * 3 + 1], textureColourTable[i * 3 + 2]);
	}

	TextureColourPaint_Grid(frameW, frameH, frameW, frameH - kStatusLineHeight, colours);

	// the scenario line: the grid is up, its cell size
	StateLine_Add("colourgrid", std::to_string(frameW / 16) + " " + std::to_string((frameH - kStatusLineHeight) / 16));
}

// The current colour word, for the scenario line
uint32_t TextureColourCurrent()
{
	return textureColourCurrent;
}
