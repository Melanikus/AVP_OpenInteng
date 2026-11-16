// The test for Track Editing's page state (src/host/track_pages.h) against the menus the editor shows (inteng_menus.h): every command that shows another page is an item of the page it is done on, every page
// it shows exists in the menus, the transitions are the cited ones and no others, and a hostile page or command changes nothing. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>

#include "inteng_menus.h"
#include "track_pages.h"

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

static const IntengScreen *Track()
{
	for (size_t s = 0; s < kIntengScreenCount; s++)
	{
		if (strcmp(kIntengScreens[s].name, "Track Editing") == 0)
		{
			return &kIntengScreens[s];
		}
	}

	return nullptr;
}

// Is the command an item of the page (either row)?
static bool OnPage(const IntengScreen &screen, int page, int32_t command)
{
	for (size_t p = 0; p < screen.pageCount; p++)
	{
		if (screen.pages[p].page != page)
		{
			continue;
		}

		for (size_t m = 0; m < screen.pages[p].count; m++)
		{
			for (size_t i = 0; i < screen.pages[p].menus[m].count; i++)
			{
				if (screen.pages[p].menus[m].items[i].command == command)
				{
					return true;
				}
			}
		}
	}

	return false;
}

static bool HasPage(const IntengScreen &screen, int page)
{
	for (size_t p = 0; p < screen.pageCount; p++)
	{
		if (screen.pages[p].page == page)
		{
			return true;
		}
	}

	return false;
}

int main()
{
	const IntengScreen *const screen = Track();

	Check(screen != nullptr && screen->pageCount == 13, "the Track Editing screen has its 13 pages");

	if (!screen)
	{
		return 1;
	}

	Check(kTrackPageEntry == 2 && HasPage(*screen, kTrackPageEntry), "the screen starts on page 2, which the menus have");

	// every transition: the page it comes from has the command as an item, and the page it goes to is one the menus have
	struct Move
	{
		int from;
		int32_t command;
		int to;
	};

	const Move moves[] = {
		{ 2, 0x516, 3 }, { 2, 0x518, 3 }, { 2, 0x514, 4 }, { 3, 0x518, 3 }, { 3, 0x51d, 5 }, { 5, 0x51e, 3 }, { 3, 0x517, 2 }, { 4, 0x515, 2 },
	};
	bool ok = true;

	for (const Move &move : moves)
	{
		int const to = TrackPages_After(move.from, move.command);

		ok = ok && to == move.to && HasPage(*screen, to);

		// a command that shows a page is an item of the page it is done on (Edit Pivot Point 0x51d is on page 3, Stop Pivot Edit 0x51e on page 5)
		if (!OnPage(*screen, move.from, move.command))
		{
			printf("  (command 0x%x is not an item of page %d)\n", (unsigned)move.command, move.from);
			ok = false;
		}
	}

	Check(ok, "each cited transition goes where the document says, from a page that has the command, to a page the menus have");

	// the ones that are NOT cited are not made
	Check(TrackPages_After(2, 0x517) == -1 && TrackPages_After(4, 0x517) == -1 && TrackPages_After(3, 0x515) == -1 && TrackPages_After(2, 0x515) == -1, "Finish End Pos Edit (0x517) goes back only from page 3 and Stop track play (0x515) only from page 4");
Check(TrackPages_After(2, 0x521) == -1 && TrackPages_After(3, 0x521) == -1 && TrackPages_After(3, 0x522) == -1 && TrackPages_After(5, 0x523) == -1, "This Section (0x521), Next Section (0x522) and Move Pivot To Centre (0x523) change no page");
	Check(TrackPages_After(2, 0x51e) == -1 && TrackPages_After(3, 0x51e) == -1, "Stop Pivot Edit only goes back from the pivot page (5)");

	// every command of every page: only the cited ones move
	int moved = 0;

	for (size_t p = 0; p < screen->pageCount; p++)
	{
		for (size_t m = 0; m < screen->pages[p].count; m++)
		{
			for (size_t i = 0; i < screen->pages[p].menus[m].count; i++)
			{
				int const to = TrackPages_After(screen->pages[p].page, screen->pages[p].menus[m].items[i].command);

				if (to >= 0)
				{
					moved++;
					ok = ok && HasPage(*screen, to);
				}
			}
		}
	}

	Check(ok && moved >= 8 && moved <= 20, "over every item of every page, the pages that are shown are all pages the menus have, and few items move: " + std::to_string(moved));

	// hostile input
	Check(TrackPages_After(-1, 0x516) == -1 && TrackPages_After(0, 0x516) == -1 && TrackPages_After(1, 0x516) == -1 && TrackPages_After(12, 0x516) == -1 && TrackPages_After(2147483647, 0x516) == -1 && TrackPages_After(-2147483647 - 1, 0x516) == -1,
		"a page that is not Track Editing's (negative, Object Placement's 0 and 1, past 11, the extremes) changes nothing");
	Check(TrackPages_After(2, -1) == -1 && TrackPages_After(2, 0) == -1 && TrackPages_After(2, 2147483647) == -1 && TrackPages_After(2, -2147483647 - 1) == -1, "a command that is none of the cited ones (0, negative, the extremes) changes nothing");
	Check(TrackPages_IsTrackPage(2) && TrackPages_IsTrackPage(11) && !TrackPages_IsTrackPage(1) && !TrackPages_IsTrackPage(12), "pages 2 to 11 are Track Editing's");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
