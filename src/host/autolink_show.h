// autolink_show.h: what the screen shows while AutoLink runs (reassess/autolink-display-frost.md AD-1..AD-8). Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG SHOWS (dump-full-2:64559 FUN_004c8930, 64643 FUN_004c8af0, 64693 FUN_004c8be0): "Right click to cancel" once at (200, 220) ((200, 200) in the run from a module file),
// and the counter "%03d" of the record number at (230, 180) each time a record is made current; both straight to the front buffer, opaque: each letter white in inteng's font
// (font16.fnt) on its black cell (view-state-table.md H-1d, H-1e). A right press stops the run at the top of the next record (0x4c899c). The pictures are never shown (Melanikus:
// "Just a counter").
//
// THE OWNER'S RULING, named "inteng showed only the counter": the pictures as a GRID of native 320 x 240 tiles in the bottom-right corner of the view, the first bottom-right, the next to its
// left, a full row going on in the row above, round-robin when every tile holds one; columns = clamp(floor(W / 640), 2, 4), rows = clamp(floor(H / 480), 2, 3). The words and the counter on top.
//
// This half is plain arithmetic, with no window and none of the game (tests/host_autolink_show_test.cpp); autolink_show_draw.cpp keeps the run's state and draws it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	int32_t const kAutoLinkTileWidth = 320;    // FUN_004c6380's screen: 320 x 240 (FUN_0057f520: the centre (160, 120), the lens 160)
	int32_t const kAutoLinkTileHeight = 240;
	int32_t const kAutoLinkMostColumns = 4;
	int32_t const kAutoLinkMostRows = 3;
	int32_t const kAutoLinkMostTiles = kAutoLinkMostColumns * kAutoLinkMostRows;
	size_t const kAutoLinkPicturePixels = (size_t)kAutoLinkTileWidth * (size_t)kAutoLinkTileHeight;

	int32_t const kAutoLinkCounterX = 0xe6;      // FUN_0057bd40(0xe6, 0xb4, 0, "%03d", i): 0x4c8a20 / 0x4c8b9e
	int32_t const kAutoLinkCounterY = 0xb4;
	int32_t const kAutoLinkWordsX = 200;         // FUN_0057bd40(200, 0xdc, 0, "Right click to cancel"): 0x4c8977 / 0x4c8b05
	int32_t const kAutoLinkWordsY = 0xdc;
	int32_t const kAutoLinkWordsYFromFile = 200; // FUN_004c8be0 0x4c8d9c: the run from a module file writes them at (200, 200)
	char const *const kAutoLinkWords = "Right click to cancel";   // 0x5e3c88

	// How many tiles across and up a view of that size holds (the owner's clamps); a size below 1 counts as 1
	struct AutoLinkGrid
	{
		int32_t columns;
		int32_t rows;
	};

	AutoLinkGrid AutoLinkShow_Grid(int32_t viewWidth, int32_t viewHeight);

	// Where picture n (counted from 0 since the run began) goes: its slot (0 = bottom-right, then leftwards, then the row above; n wraps round when every slot is used) and its tile's top-left
	// in the view. False when that tile does not lie wholly inside the view (a view smaller than the 2 x 2 the clamp asks for): it is not drawn, its slot is still used.
	bool AutoLinkShow_TilePlace(int32_t viewWidth, int32_t viewHeight, int64_t picture, int32_t *slot, int32_t *x, int32_t *y);

	// The colour (ARGB, opaque) a module's ID is shown in: 0 (nothing drawn there) black, every other ID a bright colour that differs from its neighbours'. For the eye only.
	uint32_t AutoLinkShow_IdColour(uint32_t id);

	// The counter's words: the number in at least three digits ("%03d"); a number below 0 gives "000" (inteng's numbers start at 0)
	std::string AutoLinkShow_CounterWords(int32_t number);
}
