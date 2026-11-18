// hub_text.h: the Construction hub's text lines under "View Distance" (FUN_004452b0 0x446c46-0x4470df and 0x447104-0x447138; AvP-docs/engine/inteng/view-state-table.md H-2;
// construction-closeout-frost.md TB-C7). Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DRAWS, every frame of the hub, each line at x 10 (FUN_0057b1b0 into the hub's drawer DAT_00631240), y from the screen's top:
//   - "Polygon count : %d / %d" (0x5cf72c) at y 0x32: DAT_00636f7c / DAT_00636f80, made at every start of the hub by FUN_00472720 (poly_counts.h PolyCounts_Level);
//   - for each selected object, in the selection's order, h the text font's height (DAT_00915210: 16), the k-th (from 0) at y 0x32 + 2h + 2hk:
//       "%s radius %-6.4f" (0x5cf70c): the object's name, its shape's radius (the loaded shape header's +0x34, the file's radius times the level's scale, cut to a whole number) x 1e-05,
//       then " (placed)" (0x5cf700) when its header flags have 0x400, " (base)" (0x5cf6f8) with 0x100, and with 0x4000: " (not psx)" (0x5cf6ec) without 0x10000 and " (not saturn)"
//       (0x5cf6dc) without 0x20000 (0x446e06-0x446ed4), then " (subdivide)" (0x5cf6cc) when the engine block's +0x50 has bit 31, which only the commands 0x259 / 0x25a set and no AvP
//       menu has them (DEAD: never shown), then "\n" (0x5cf6c8);
//       and at y 0x32 + 3h + 2hk "Location %d %d %d\n" (0x5cf6b4): the loaded record's place (+0x10, +0x14, +0x18: the file's place times the level's scale, cut) each times the float
//       0.01 (0x5abf9c) and cut toward 0 (__ftol, 0x447053-0x44707a).
// NAMED: the "\n" at each line's end draws nothing more (the next line is placed by its own y), so it is not in the words here.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	struct HubTextLine
	{
		int32_t y;            // from the top of inteng's screen, in pixels
		std::string words;
	};

	// The most selected objects whose lines are made (a hostile selection cannot make the list unbounded)
	size_t const kHubTextMostObjects = 64;

	// The lines above, in the order inteng draws them: the polygon count first. shapes / placed: PolyCounts_Level's numbers (made at the hub's start). selected: the 8-byte IDs of the
	// selected objects, in order; an ID no object has is skipped. The values are the editor's own (the header's bytes in the store: an edit not yet saved shows). scale: the level's scale.
	void HubText_Lines(File_Chunk *level, OpenLevelFile const *store, int64_t shapes, int64_t placed, std::vector<std::array<uint8_t, 8>> const &selected, double scale,
		int32_t fontHeight, std::vector<HubTextLine> *lines);
}
