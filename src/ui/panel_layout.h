// Where the editor's fixed panels go in its window: a toolbar along the top, the object list on the left, the field panel on the right,
// a status line along the bottom, and the level's view in the middle. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULES. The panels are fixed, with no docking (R-INT-6). Their places are plain sums from the window's size, so they're tested at
// every size, from none at all to the largest.
// - A window's size comes from the system, and can be anything: 0 by 0 while it's minimised, or larger than any screen. A side below 0
//   is taken as 0, and one above kPanelMostSide as kPanelMostSide. Every sum is done in 64 bits, so none can wrap round.
// - The toolbar and the status line take their height first, the toolbar before the status line. What's left between them is the
//   middle row.
// - Across the middle row, each panel starts at the width it asks for. When there isn't room, the level's view gives way first, then
//   the list down to its least width, then the field panel down to its least, then the list to nothing, and then the field panel. So
//   the field panel, where a mapper edits, is the last to go.
// - No panel is ever less than nothing wide or high, none goes past an edge of the window, and no two cover the same pixel.

#pragma once

#include <stdint.h>

namespace ui
{
	// The largest window side laid out. Larger windows are laid out as this, from their top left, so nothing goes past their edges.
	int32_t const kPanelMostSide = 16384;

	// A rectangle on the window, in pixels from its top left. Its width and height are never below 0.
	struct Box
	{
		int32_t x;
		int32_t y;
		int32_t width;
		int32_t height;
	};

	// How big each panel asks to be, and the least the list and the field panel can be before they give way altogether. A size below 0
	// is taken as 0, one above kPanelMostSide as kPanelMostSide, and a least above what's asked for as what's asked for.
	struct PanelSizes
	{
		int32_t toolbarHeight;
		int32_t statusHeight;
		int32_t listWidth;
		int32_t leastListWidth;
		int32_t fieldsWidth;
		int32_t leastFieldsWidth;
	};

	// The sizes the panels were designed with, which their tests lay out. The editor's own window now gives the level's view more room
	// (editor_main.cpp, kEditorPanelSizes).
	PanelSizes const kPanelSizesAsBuilt = { 32, 20, 260, 140, 340, 220 };

	struct PanelLayout
	{
		Box toolbar;
		Box list;
		Box view;      // the level's view, between the list and the field panel
		Box fields;
		Box status;
		bool roomy;    // true when every panel has what it asked for; otherwise the panel may say so ("the window is too small")
	};

	// Lays the panels out in a window of width by height pixels
	PanelLayout LayOutPanels(int32_t width, int32_t height, const PanelSizes &sizes);

	// Whether a box holds a point: the points from x to x + width - 1 across and y to y + height - 1 down, so an empty box holds none.
	// The sums are done in 64 bits, so no place or size can wrap round. Every click in the editor is placed with this.
	bool BoxHolds(const Box &box, int32_t x, int32_t y);
}
