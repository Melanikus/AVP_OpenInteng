// texture_window_paint.h: what the Texture Map's TEXTURE WINDOW screen draws on the game's frame: the picture, the outline of the polygon's coordinates over it, the four corner nodes and a few lines of words. Written 2026-11-15 by
// Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng's texture screen (FUN_00551f80, sub-mode 1) shows the current texture's bitmap with the selected polygon's texture coordinates as nodes joined by lines, "Texture Scale x%d", and the nodes' "%d %d" numbers. This
// draws the same things with the game's own quads (DrawQuad, the way PanelPass does boxes) over the level (no background, as inteng), the picture as textured quads, each line as a row of small boxes, each node as a box. It must be
// called inside the game's frame (LevelView_Draw's addToFrame), like PanelPass_AddToFrame.

#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

// The texture window as inteng lays it out (model/texture_window.h): the VIEW, the part of the texture area shown (texels; the area can be the picture repeated, so the view can take in
// several repeats), stretched onto the WINDOW, in the screen's bottom-right corner (FUN_00556250: one textured quad from the window's top left to its far edges, its texture
// coordinates the view's; inteng's texture wraps, the game's quads clamp, so each repeat is its own quad. NAMED: inteng's half-texel shift of the coordinates (+0.5) is left out, as
// a clamped repeat would take a strip of its neighbour's edge).
struct TextureWindowPicture
{
	int32_t texture;           // the game's texture number of the picture (texID_t); -1: none
	int32_t pictureWidth, pictureHeight;   // the picture's size, texels
	int32_t view[4];           // left, top, right, bottom: texels
	int32_t window[4];         // left, top, right, bottom on the frame (right and bottom not in it)
	int32_t nodeCount;         // 3 or 4
	int32_t nodeX[4], nodeY[4];   // each node's place on the window, in pixels
	int32_t activeNode;        // 0 to 3
	int dotPhase;              // the outline's dotted line's phase, 0..5, one step a frame (DAT_00914d14)
};

// A line of the window's text at its own place on the frame (FUN_00551f80's text: each at inteng's place)
struct TextureWindowWords
{
	int32_t x, y;
	std::string text;
};

// Lines of text, one under the other from (x, y), 15 pixels apart, on the game's frame (the Texture Map's flag list)
// THE MULTIPLE NODE MODE (Texture Map sub-mode 3, FUN_00556250 0x5562dd): along each link (pairs of node numbers) inteng's dotted line (FUN_005564b0, model::WindowDots: one-pixel
// dots every 3 pixels, black and white by turns, crawling with dotPhase, only inside the window) and a box on each node in the window (red when lit)
void TextureWindowPaint_Nodes(int32_t windowWidth, int32_t windowHeight, const std::vector<int32_t> &x, const std::vector<int32_t> &y, const std::vector<std::pair<int32_t, int32_t>> &links,
	const std::vector<bool> &lit, const int32_t window[4], int dotPhase);   // window: left, top, right, bottom
void TextureWindowPaint_Lines(int32_t windowWidth, int32_t windowHeight, const std::vector<std::string> &lines, int32_t x, int32_t y);

// THE TEXTURE CHOOSER (inteng's FUN_005575a0: a grid of every texture): each cell holds one picture, drawn as the game's quad; the hovered cell has a frame, and lines of words (the picture's size and name) go under the grid.
struct TextureChooserCell
{
	int32_t texture;           // the game's texture number
	int32_t x, y, width, height;   // the cell, in pixels of the window
};

void TextureChooserPaint_Draw(int32_t windowWidth, int32_t windowHeight, int32_t areaTop, int32_t areaHeight, const std::vector<TextureChooserCell> &cells, int32_t hovered, const std::vector<std::string> &lines, int32_t lineX, int32_t lineY);

// THE PLACEMENT PICKER'S GRID (shape_picker.h, shape_picker_screen.cpp; Vasquez 2026-11-23): each tile its picture (the game's texture of its drawing, or its words while it
// has none: "drawing...", "can't be drawn"), its name under it in inteng's font, the selected one framed in the chooser's yellow; its SCROLL BAR down the right side in the
// dialog lists' look (the column black framed white, the thumb blue framed white, two arrow buttons; the owner 11-23: "a dragable scroll box on the top right"); then lines
// of words at the bottom. NO GROUND behind the tiles (the owner 11-23, "The selection box for the items is grey": the dark box was the texture chooser's, never checked
// against inteng; inteng's texture windows draw none, TM-B, and the Shape Chooser it stands in for shows the level), so the level stays in sight round them. Places in
// the editor's own pixels (as the texture chooser's), drawn in the window's.
namespace host
{
	struct ShapePickerBar;
}

struct ShapePickerPaintTile
{
	int32_t texture;           // the game's texture number of its drawing; -1 while it has none
	int32_t x, y, side;        // its picture's square
	int32_t nameHeight;        // the line under it
	bool selected;
	bool live = false;         // its shape is drawn LIVE in the level's frame (shape_pane_gpu.h): no still picture and no words in its square
	std::string name;          // under it, cut to fit
	std::string words;         // in the square while it has no picture
};

void ShapePickerPaint_Draw(int32_t windowWidth, int32_t windowHeight, const std::vector<ShapePickerPaintTile> &tiles, const host::ShapePickerBar &bar, const std::vector<std::string> &lines, int32_t lineX, int32_t lineY);

void TextureWindowPaint_Draw(int32_t windowWidth, int32_t windowHeight, const TextureWindowPicture &picture, const std::vector<TextureWindowWords> &words);

// SELECT COLOUR (Shift+K, FUN_00560620): 256 cells of gridWidth / 16 x gridHeight / 16 from the top left, cell i at column i % 16, row i / 16, each filled with colours[i] (0xAARRGGBB)
void TextureColourPaint_Grid(int32_t windowWidth, int32_t windowHeight, int32_t gridWidth, int32_t gridHeight, const std::vector<uint32_t> &colours);
