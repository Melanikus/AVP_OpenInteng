// track_pages.cpp: see track_pages.h. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "track_pages.h"

namespace host
{
	bool TrackPages_IsTrackPage(int page)
	{
		return page >= kTrackPageLeast && page <= kTrackPageMost;
	}

	int TrackPages_After(int page, int32_t command)
	{
		if (!TrackPages_IsTrackPage(page))
		{
			return -1;
		}

		switch (command)
		{
			case 0x36:    // Edit Track: state 2 (0x4dd069-0x4dd074)
				return 2;
			case 0x516:   // Edit Section End Position: state 3 (0x4dcc00-0x4dcc06)
			case 0x518:   // Add Section: state 3, the same store
				return 3;
			case 0x514:   // Play Track: state 4 (0x4dcb2b)
				return 4;
			case 0x51d:   // Edit Pivot Point: state 5 (FUN_004f3770, 0x4f37bb)
				return 5;
			case 0x51e:   // Stop Pivot Edit: back to page 3 (FUN_004f3cf0 + FUN_004f0c70, 0x4f4078)
				return page == 5 ? 3 : -1;
			case 0x517:   // Finish End Pos Edit (page 3): FUN_004f0cd0 at 0x4dccfe, then the shared tail LAB_004dcd03: PUSH 2 (0x4dcd09), CALL [EDX+0x18] (0x4dcd0d), state 2
				return page == 3 ? 2 : -1;
			case 0x515:   // Stop track play (page 4): the sound stopped (0x4dcb4d), FUN_004f0da0 (0x4dcb5e), then the same tail: page 2
				return page == 4 ? 2 : -1;
			default:
				return -1;
		}
	}
}
