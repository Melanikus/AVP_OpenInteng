// The test for AutoLink's display arithmetic (autolink_show.h; reassess/autolink-display-frost.md AD-3, AD-5, AD-6). Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdio.h>

#include <string>

#include "autolink_show.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Place(int32_t w, int32_t h, int64_t n, int32_t wantSlot, int32_t wantX, int32_t wantY, bool wantInside)
{
	int32_t slot = -1, x = -1, y = -1;
	bool const inside = AutoLinkShow_TilePlace(w, h, n, &slot, &x, &y);

	return inside == wantInside && slot == wantSlot && x == wantX && y == wantY;
}

int main()
{
	puts("AutoLink's grid (the owner's ruling: columns = clamp(W / 640, 2, 4), rows = clamp(H / 480, 2, 3)):");

	AutoLinkGrid g = AutoLinkShow_Grid(1008, 729);
	Check(g.columns == 2 && g.rows == 2, "the harness' 1008 x 729 view: 2 x 2");
	g = AutoLinkShow_Grid(1920, 1080);
	Check(g.columns == 3 && g.rows == 2, "1920 x 1080: 3 x 2");
	g = AutoLinkShow_Grid(2560, 1600);
	Check(g.columns == 4 && g.rows == 3, "2560 x 1600: 4 x 3 (his: 1280 x 720 of tiles)");
	g = AutoLinkShow_Grid(5120, 2880);
	Check(g.columns == 4 && g.rows == 3, "5120 x 2880: still 4 x 3, the most");
	g = AutoLinkShow_Grid(640, 480);
	Check(g.columns == 2 && g.rows == 2, "640 x 480: 2 x 2, the least");
	g = AutoLinkShow_Grid(0, -5);
	Check(g.columns == 2 && g.rows == 2, "a size of 0 or below: 2 x 2, nothing divided by it");

	puts("Where each picture goes (the first bottom-right, then leftwards, then the row above, round-robin):");
	Check(Place(1008, 729, 0, 0, 688, 489, true), "1008 x 729, picture 0: slot 0 at (688, 489), the bottom-right corner");
	Check(Place(1008, 729, 1, 1, 368, 489, true), "picture 1: slot 1, to its left (368, 489)");
	Check(Place(1008, 729, 2, 2, 688, 249, true), "picture 2: the row above, at the right again (688, 249)");
	Check(Place(1008, 729, 3, 3, 368, 249, true), "picture 3: (368, 249)");
	Check(Place(1008, 729, 4, 0, 688, 489, true), "picture 4: every slot used, back to slot 0");
	Check(Place(1008, 729, 4000000001LL, 1, 368, 489, true), "a picture number past 32 bits wraps the same way (4000000001 % 4 = 1)");
	Check(Place(1008, 729, -3, 0, 688, 489, true), "a number below 0 counts as 0");
	Check(Place(2560, 1600, 11, 11, 1280, 880, true), "2560 x 1600, picture 11: the last slot, top-left of the 4 x 3 (1280, 880)");
	Check(Place(2560, 1600, 12, 0, 2240, 1360, true), "picture 12: slot 0 again (2240, 1360)");
	Check(Place(500, 400, 1, 1, -140, 160, false), "a view under 640 wide: the second tile would start left of it, so it is not drawn (its slot is still used)");
	Check(Place(500, 400, 3, 3, -140, -80, false), "and the row above lies above the view's top");
	Check(Place(500, 400, 0, 0, 180, 160, true), "its first tile still fits");

	puts("The ID colours (for the eye only):");
	Check(AutoLinkShow_IdColour(0) == 0xFF000000u, "ID 0 (nothing drawn there) is black");
	bool allOpaque = true, neighboursDiffer = true, noneBlack = true;

	for (uint32_t id = 1; id < 2000; id++)
	{
		uint32_t const c = AutoLinkShow_IdColour(id);

		allOpaque = allOpaque && (c >> 24) == 0xFF;
		noneBlack = noneBlack && (c & 0xFFFFFF) != 0;
		neighboursDiffer = neighboursDiffer && c != AutoLinkShow_IdColour(id + 1);
	}

	Check(allOpaque && noneBlack, "every ID from 1 is an opaque colour, never black");
	Check(neighboursDiffer, "the next ID is always another colour");
	Check(AutoLinkShow_IdColour(1) != AutoLinkShow_IdColour(13), "ID 1 and ID 13 (the twelve colours round once) differ");
	Check(AutoLinkShow_IdColour(0xFFFFFFFFu) >> 24 == 0xFF, "the largest ID gives a colour, with no overflow");

	puts("The counter (\"%03d\", 0x5e249c):");
	Check(AutoLinkShow_CounterWords(0) == "000", "0 -> 000");
	Check(AutoLinkShow_CounterWords(7) == "007", "7 -> 007");
	Check(AutoLinkShow_CounterWords(999) == "999", "999 -> 999");
	Check(AutoLinkShow_CounterWords(1000) == "1000", "1000 -> 1000 (never cut)");
	Check(AutoLinkShow_CounterWords(-4) == "000", "a number below 0 -> 000");
	Check(AutoLinkShow_CounterWords(2147483647) == "2147483647", "the largest number fits its words");

	printf("%s\n", checksFailed == 0 ? "All checks passed" : "SOME CHECKS FAILED");
	return checksFailed == 0 ? 0 : 1;
}
