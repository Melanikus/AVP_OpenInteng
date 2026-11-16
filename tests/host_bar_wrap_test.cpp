// The test that the bars' titles WRAP onto more rows when they are wider than the window, and that every title can be clicked at every window width the editor allows. Written 2026-11-15 by Melanikus
// (Apone) for AVP_OpenInteng, after Bishop's order (a hidden Waypoint run showed "Menus past the window's edge: Edit Extents" on page 3 at 1008 pixels: an item that cannot be clicked breaks Melanikus's
// click rule). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// For EVERY screen and EVERY page of it (the code-built screens have several: inteng's menu method +0x18) and a range of window widths, from the least width the editor allows (EditorBar_LeastWidth) up:
//  - the rows are made (EditorBar_MakeRows), at most kEditorBarMostRows of them;
//  - the physical rows hold every menu of the bar's logical row (the second table is the right-click pop-up, not a bar), once each, in order (a menu is never split, never lost, never doubled), and a logical row starts a new physical row;
//  - no physical row is wider than the window (its titles' widths and the edge kept past them), unless it holds one title alone;
//  - the bars are laid out as the editor lays them out (EditorBar_SizesRows, EditorBar_PlaceRows, MenuBar::LayOut): every title is in the window with room, and a press on its middle opens THAT menu of THAT row;
//  - the rows are one above the other, never overlapping, and the toolbar is as tall as the rows.
// And hostile input: a width below any title, a width of 0 or less, no output are refused or kept inside the limits, never a crash.
//
// It runs none of the game. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "editor_bar.h"
#include "host_frame.h"
#include "menu_bar.h"
#include "menu_paint.h"
#include "panel_layout.h"

using namespace ui;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A font with wide letters (the widest the real one is likely to be), so the test is hard on the wrap
static int LetterWidth(int letter)
{
	return 6 + (letter & 255) % 5;
}

static int32_t const kLineHeight = 15;

static EditorBarRows rows;

struct Counts
{
	int pagesTried;
	int widthsTried;
	int titlesClicked;
	int failed;
};

// Makes the bars of a screen on the page now shown at a width, lays them out and clicks each title. Counts what failed; the first failures are printed.
static void TryOne(size_t screen, int32_t page, int32_t width, Counts *counts)
{
	std::string const where = std::string(host::kIntengScreens[screen].name) + " page " + std::to_string(page) + " at " + std::to_string(width);
	int const failedBefore = counts->failed;

	auto fail = [&](std::string const &why)
	{
		if (counts->failed < 20)
		{
			printf("  FAILED  %s: %s\n", where.c_str(), why.c_str());
		}

		counts->failed++;
	};

	if (!EditorBar_MakeRows(screen, KeyLayout::Classic, width, LetterWidth, &rows))
	{
		fail("the rows were not made");
		return;
	}

	counts->widthsTried++;

	if (rows.rowCount < 1 || rows.rowCount > kEditorBarMostRows)
	{
		fail("rows: " + std::to_string(rows.rowCount));
		return;
	}

	// the physical rows hold the logical rows' menus, in order, once each
	std::vector<ui::Menu const *> logicalMenus;

	for (size_t l = 0; l < 1; l++)
	{
		for (size_t m = 0; m < rows.logical[l].menuCount; m++)
		{
			logicalMenus.push_back(&rows.logical[l].menus[m]);
		}
	}

	size_t at = 0;

	for (size_t r = 0; r < rows.rowCount; r++)
	{
		// a page of inteng's with no menu of its own on the bar (some of Track Editing's pages hold only the pop-up table) is one empty row: the editor's own menu that was on every page is gone (Bishop 11-16)
		if (rows.menuCount[r] == 0 && !logicalMenus.empty())
		{
			fail("row " + std::to_string(r) + " is empty");
		}

		for (size_t m = 0; m < rows.menuCount[r]; m++)
		{
			if (at >= logicalMenus.size() || rows.menus[r][m].title != logicalMenus[at]->title || rows.menus[r][m].items != logicalMenus[at]->items || rows.menus[r][m].count != logicalMenus[at]->count)
			{
				fail("menu " + std::to_string(at) + " is not the one the logical rows have there");
			}

			at++;
		}
	}

	if (at != logicalMenus.size())
	{
		fail("the rows hold " + std::to_string(at) + " menus, the bars have " + std::to_string(logicalMenus.size()));
	}

	// no physical row wider than the window, but for a title alone
	for (size_t r = 0; r < rows.rowCount; r++)
	{
		int32_t titles[kMenuMostMenus];
		int32_t drops[kMenuMostMenus];
		int64_t sum = 0;

		host::MenuPaint_Measure(rows.menus[r], rows.menuCount[r], LetterWidth, titles, drops);

		for (size_t m = 0; m < rows.menuCount[r]; m++)
		{
			sum += titles[m];
		}

		if (rows.menuCount[r] > 1 && sum > width - kEditorBarEdge)
		{
			fail("row " + std::to_string(r) + " holds " + std::to_string(rows.menuCount[r]) + " titles " + std::to_string(sum) + " wide in " + std::to_string(width));
		}
	}

	// laid out as the editor lays them out
	int32_t const windowHeight = 720;
	PanelLayout const layout = LayOutPanels(width, windowHeight, EditorBar_SizesRows(HostFrame_PanelSizes(width), rows.rowCount));
	EditorBarPlaceRows const place = EditorBar_PlaceRows(layout, rows.rowCount);
	MenuBar bars[kEditorBarMostRows];

	for (size_t r = 0; r < rows.rowCount; r++)
	{
		int32_t titles[kMenuMostMenus];
		int32_t drops[kMenuMostMenus];

		if (rows.menuCount[r] == 0)
		{
			continue;
		}

		if (!bars[r].Load(rows.menus[r], rows.menuCount[r]))
		{
			fail("row " + std::to_string(r) + " was not taken by the menu bar");
			continue;
		}

		host::MenuPaint_Measure(rows.menus[r], rows.menuCount[r], LetterWidth, titles, drops);
		bars[r].LayOut(place.bar[r], place.dropRoom[r], titles, drops, host::MenuPaint_ItemHeight(kLineHeight));

		if (place.bar[r].height <= 0 || place.bar[r].y < layout.toolbar.y || place.bar[r].y + place.bar[r].height > layout.toolbar.y + layout.toolbar.height)
		{
			fail("row " + std::to_string(r) + "'s bar is outside the toolbar");
		}

		if (r > 0 && place.bar[r].y < place.bar[r - 1].y + place.bar[r - 1].height)
		{
			fail("row " + std::to_string(r) + " overlaps the row above");
		}

		for (size_t m = 0; m < rows.menuCount[r]; m++)
		{
			Box const title = bars[r].TitleBox(m);

			if (title.width <= 0 || title.height <= 0 || title.x < 0 || title.x + title.width > width)
			{
				fail("row " + std::to_string(r) + " title \"" + std::string(rows.menus[r][m].title) + "\" is past the window: x " + std::to_string(title.x) + " width " + std::to_string(title.width));
				continue;
			}

			// a press on the middle of the title opens this menu, on a fresh bar made the same
			MenuBar fresh;

			fresh.Load(rows.menus[r], rows.menuCount[r]);
			fresh.LayOut(place.bar[r], place.dropRoom[r], titles, drops, host::MenuPaint_ItemHeight(kLineHeight));

			MenuAnswer const answer = fresh.Press(title.x + title.width / 2, title.y + title.height / 2);

			counts->titlesClicked++;

			if (answer != MenuAnswer::Opened || fresh.OpenMenu() != (int)m)
			{
				fail("a press on the middle of row " + std::to_string(r) + " title \"" + std::string(rows.menus[r][m].title) + "\" did not open it");
			}
		}
	}

	if (layout.toolbar.height < (int32_t)rows.rowCount * (layout.toolbar.height / (int32_t)rows.rowCount))
	{
		fail("the toolbar is not as tall as its rows");
	}

	(void)failedBefore;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	Counts counts = {};
	int32_t const least = EditorBar_LeastWidth(KeyLayout::Classic, LetterWidth);

	printf("  (the least window width the bars allow, with this font: %d)\n", least);
	Check(least >= 200 && least <= 1300, "the least window width is something a screen can have: " + std::to_string(least));

	int32_t const widths[] = { least, least + 1, least + 50, 800, 1008, 1280, 1366, 1600, 1920, 2560 };

	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		host::IntengScreen const &screen = host::kIntengScreens[s];

		// page 0 and every page the screen registers
		std::vector<int32_t> pages = { 0 };

		for (size_t p = 0; p < screen.pageCount; p++)
		{
			bool known = false;

			for (int32_t const seen : pages)
			{
				known = known || seen == screen.pages[p].page;
			}

			if (!known)
			{
				pages.push_back(screen.pages[p].page);
			}
		}

		for (int32_t const page : pages)
		{
			EditorBar_SetPage(page);
			counts.pagesTried++;

			for (int32_t const width : widths)
			{
				if (width >= least)
				{
					TryOne(s, page, width, &counts);
				}
			}
		}
	}

	EditorBar_SetPage(0);
	printf("  (%d pages of %zu screens at up to %zu widths: %d layouts made, %d titles clicked)\n", counts.pagesTried, host::kIntengScreenCount, sizeof(widths) / sizeof(widths[0]), counts.widthsTried, counts.titlesClicked);
	Check(counts.failed == 0, "every page of every screen wraps into rows that fit, hold every menu once, and have every title clickable: " + std::to_string(counts.failed) + " failures");
	Check(counts.titlesClicked > 500, "a real number of titles was clicked: " + std::to_string(counts.titlesClicked));

	// the wrap is real: a narrow window gives more rows than a wide one on a screen whose titles are many (the hub's bar)
	{
		EditorBar_SetPage(0);

		EditorBarRows narrow, wide;

		Check(EditorBar_MakeRows(host::kIntengHub, KeyLayout::Classic, least, LetterWidth, &narrow) && EditorBar_MakeRows(host::kIntengHub, KeyLayout::Classic, 2560, LetterWidth, &wide), "the hub's bars are made at the least width and at 2560");
		Check(narrow.rowCount >= wide.rowCount, "the narrow window has at least as many rows as the wide one: " + std::to_string(narrow.rowCount) + " and " + std::to_string(wide.rowCount));
	}

	// hostile input
	{
		EditorBarRows hostile;

		Check(!EditorBar_MakeRows(0, KeyLayout::Classic, 1000, LetterWidth, nullptr), "no output: refused");
		Check(!EditorBar_MakeRows(0, KeyLayout::Classic, 1000, nullptr, &hostile), "no font: refused");
		Check(!EditorBar_MakeRows(host::kIntengScreenCount + 5, KeyLayout::Classic, 1000, LetterWidth, &hostile), "a screen there is not: refused");

		// widths below any title, zero, negative: the call is kept inside its limits (rows or a refusal, never more than kEditorBarMostRows)
		for (int32_t const width : { 1, 0, -1, -2147483647 - 1, 2147483647 })
		{
			bool const made = EditorBar_MakeRows(host::kIntengHub, KeyLayout::Classic, width, LetterWidth, &hostile);

			Check(!made || (hostile.rowCount >= 1 && hostile.rowCount <= kEditorBarMostRows), "width " + std::to_string(width) + ": " + (made ? "rows within the limit" : "refused"));
		}

		// the sizes and places for no rows, too many rows
		PanelLayout const layout = LayOutPanels(1000, 700, EditorBar_SizesRows(HostFrame_PanelSizes(1000), 0));
		EditorBarPlaceRows const none = EditorBar_PlaceRows(layout, 0);
		EditorBarPlaceRows const many = EditorBar_PlaceRows(layout, 99);

		Check(none.bar[0].height > 0 && many.bar[kEditorBarMostRows - 1].height > 0, "0 rows is taken as 1 and 99 as the most: the first and the last row have a height");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
