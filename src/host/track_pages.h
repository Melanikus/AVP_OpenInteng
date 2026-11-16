// track_pages.h: which page of the code-built bars Track Editing shows, by the track's state. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". Facts: AvP-docs\engine\inteng\track-menu-pairs.md (Frost, every store cited from FUN_004d68e0 and its callers).
//
// inteng's placement frame (FUN_004d9af0) keeps a state in DAT_008f4f2c and shows the pages of its menu object by method +0x18: page 0 and 1 are Object Placement's, then, for Track Editing:
//   2  a track is loaded (Edit Track 0x36 sets state 2 and shows page 2: 0x4dd069-0x4dd074)         bar A: the table 0x5e6e98, bar B: the track's menus
//   3  a section's end position is being edited (0x516 and 0x518 set state 3, show page 3: 0x4dcc00)  bar A: the table 0x5e6e98
//   4  the track plays (0x514 shows page 4: 0x4dcb2b)                                                  bar A: the table 0x5e6e98
//   5  the PIVOT POINT edit (0x51d: FUN_004f3770 shows page 5, 0x4f37bb); 0x51e finishes it and goes back to page 3 (FUN_004f3cf0 + FUN_004f0c70)
//   7  the acoustic mode of a track's sound (0x4f56af shows page 7)
//   6, 8, 9, 10, 11  track sounds and the hierarchy flag edits: no call that shows them was found in FUN_004d9af0 (Frost): they are reached by the commands of the handlers read later
//   Finish End Pos Edit (0x517, from page 3) and Stop track play (0x515, from page 4) both go to page 2 through the shared tail LAB_004dcd03 (Frost: PUSH 2 at 0x4dcd09, CALL [EDX+0x18] at 0x4dcd0d, state 2)
//   This Section (0x521) / Next Section (0x522, Jump To Start) change NO page and no state; 0x523 stays on page 5. Page 0 is shown by Stop Track Edit (0x51c), by 0x51f and 0x520 (the track deleted) and by the end of the
//   pivot code: those LEAVE the screen (page 0 is Object Placement's), so they are not here. Pages 1, 6, 9 and 10 are shown by no call on this object; 8 and 0xb by the hierarchy editor's code.
// ONLY THE TRANSITIONS ABOVE ARE HERE, each with its address.
// The editor calls TrackPages_After for a command of the Track Editing screen that does something; a page the menus file does not have is never asked for.
#pragma once

#include <stdint.h>

namespace host
{
	int const kTrackPageEntry = 2;       // the page the screen starts on: a track is loaded (the screen is entered by Edit Track)
	int const kTrackPageLeast = 2;
	int const kTrackPageMost = 11;

	// The page to show after the command was done on the page now shown, or -1 when the command shows no other page (or its page is not read yet)
	int TrackPages_After(int page, int32_t command);

	// True when page is one of Track Editing's (2 to 11)
	bool TrackPages_IsTrackPage(int page);
}
