// texture_window_paint.h: what the Texture Map's TEXTURE WINDOW screen draws on the game's frame: the picture, the outline of the polygon's coordinates over it, the four corner nodes and a few lines of words. Written 2026-11-15 by
// Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng's texture screen (FUN_00551f80, sub-mode 1) shows the current texture's bitmap with the selected polygon's texture coordinates as nodes joined by lines, "Texture Scale x%d", and the nodes' "%d %d" numbers. This
// draws the same things with the game's own quads (DrawQuad, the way PanelPass does boxes): a plain dark box over the level, the picture as one textured quad, each line as a row of small boxes, each node as a box. It must be
// called inside the game's frame (LevelView_Draw's addToFrame), like PanelPass_AddToFrame.

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

struct TextureWindowPicture
{
	int32_t texture;           // the game's texture number of the picture (texID_t); -1: none
	int32_t x, y;              // the picture's top left on the window, in pixels
	int32_t width, height;     // its size on the window, in pixels
	int32_t tilesX, tilesY;    // how many times the picture is repeated across and down (the texture window of inteng is the picture repeated 1 to 10 times: Select Number of Repeats); 0 is 1
	int32_t nodeCount;         // 3 or 4
	int32_t nodeX[4], nodeY[4];   // each node's place on the window, in pixels
	int32_t activeNode;        // 0 to 3
	int32_t areaTop, areaHeight;   // the dark box's top and height (below the bars, above the status line)
};

// Lines of text, one under the other from (x, y), 15 pixels apart, on the game's frame (the Texture Map's flag list)
void TextureWindowPaint_Lines(int32_t windowWidth, int32_t windowHeight, const std::vector<std::string> &lines, int32_t x, int32_t y);

// THE TEXTURE CHOOSER (inteng's FUN_005575a0: a grid of every texture): each cell holds one picture, drawn as the game's quad; the hovered cell has a frame, and lines of words (the picture's size and name) go under the grid.
struct TextureChooserCell
{
	int32_t texture;           // the game's texture number
	int32_t x, y, width, height;   // the cell, in pixels of the window
};

void TextureChooserPaint_Draw(int32_t windowWidth, int32_t windowHeight, int32_t areaTop, int32_t areaHeight, const std::vector<TextureChooserCell> &cells, int32_t hovered, const std::vector<std::string> &lines, int32_t lineX, int32_t lineY);

void TextureWindowPaint_Draw(int32_t windowWidth, int32_t windowHeight, const TextureWindowPicture &picture, const char *const *lines, int32_t lineCount, int32_t lineX, int32_t lineY);
