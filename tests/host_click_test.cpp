// The test that a click lands on what's drawn under it: the editor's menu bar, with the editor's own menus, at several window sizes and
// several stretches between the window and the panels' picture. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: in Melanikus's first build with the menu bar (2026-11-14) a click on the right of the first menu's title opened the second. The
// panels were drawn stretched while clicks were tested unstretched. This test draws nothing: it takes where the menu bar puts each title
// and each item in the panels' picture, works out which of the window's pixels show it once the picture is stretched over the window
// (the stretch Windows does when the two sizes differ), and clicks the first, the middle and the last of those pixels. Each click is moved
// into the picture by host_frame.h's HostFrame_WindowToPanels, as the editor moves every click, and must land on that title or item.
// It runs at the editor's own size (no stretch), at 1.25 and 1.5 (Windows' 125% and 150% display scaling, if the picture were drawn at
// the unscaled size), and with an 800 by 600 picture in a 1024 by 768 window (the game's own first size), at two window sizes.
// And it checks it can fail: without the move into the picture, a 1.5 stretch must send some click to the wrong place.
//
// It runs none of the game. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "editor_actions.h"
#include "host_frame.h"
#include "menu_bar.h"
#include "menu_paint.h"

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

// A font whose letters aren't all one width, so the titles' places come from real sums
static int LetterWidth(int letter)
{
	return 5 + (letter & 255) % 5;
}

static int32_t const kLineHeight = 15;

static Menu menus[(size_t)EditorMenu::Count];
static MenuItem items[64];
static EditorKeyWords words[64];

// The editor's menu bar, laid out in a picture of pictureWidth by pictureHeight as the editor lays it out each frame
static void LayOutTheBar(MenuBar *bar, int32_t pictureWidth, int32_t pictureHeight)
{
	PanelLayout const layout = LayOutPanels(pictureWidth, pictureHeight, HostFrame_PanelSizes(pictureWidth));
	int32_t titleWidths[(size_t)EditorMenu::Count];
	int32_t dropWidths[(size_t)EditorMenu::Count];

	host::MenuPaint_Measure(menus, (size_t)EditorMenu::Count, LetterWidth, titleWidths, dropWidths);
	bar->LayOut(layout.toolbar, layout.list, titleWidths, dropWidths, host::MenuPaint_ItemHeight(kLineHeight));
}

// The window's pixels, across one side, that show a run of the picture's pixels from first to last: those whose middle, stretched into
// the picture, lands in that run. Stretched the way Windows stretches a picture over a window: the window's pixel x shows the picture's
// pixel (x + 1/2) times picture over window.
static std::vector<int32_t> PixelsShowing(int32_t first, int32_t last, int32_t windowSide, int32_t pictureSide)
{
	std::vector<int32_t> shown;

	for (int32_t x = 0; x < windowSide; x++)
	{
		int64_t const inPicture = ((2 * (int64_t)x + 1) * pictureSide) / (2 * (int64_t)windowSide);

		if (inPicture >= first && inPicture <= last)
		{
			shown.push_back(x);
		}
	}

	return shown;
}

// The first, the middle and the last of some pixels
static std::vector<int32_t> ThreeOf(const std::vector<int32_t> &pixels)
{
	std::vector<int32_t> three;

	if (!pixels.empty())
	{
		three.push_back(pixels.front());
		three.push_back(pixels[pixels.size() / 2]);
		three.push_back(pixels.back());
	}

	return three;
}

struct Stretch
{
	int32_t windowWidth;
	int32_t windowHeight;
	int32_t pictureWidth;
	int32_t pictureHeight;
	const char *words;
};

// Clicks the first, middle and last pixels showing each title, and each shown item of each menu, in the window, and counts the clicks
// that landed where they should. With moved false, the click isn't moved into the picture first (the editor before this fix).
static void ClickEverything(const Stretch &stretch, bool moved, int *clicks, int *landed)
{
	*clicks = 0;
	*landed = 0;

	for (size_t m = 0; m < (size_t)EditorMenu::Count; m++)
	{
		MenuBar bar;

		bar.Load(menus, (size_t)EditorMenu::Count);
		LayOutTheBar(&bar, stretch.pictureWidth, stretch.pictureHeight);

		Box const title = bar.TitleBox(m);
		std::vector<int32_t> const across = ThreeOf(PixelsShowing(title.x, title.x + title.width - 1, stretch.windowWidth, stretch.pictureWidth));
		std::vector<int32_t> const down = ThreeOf(PixelsShowing(title.y, title.y + title.height - 1, stretch.windowHeight, stretch.pictureHeight));

		for (int32_t x : across)
		{
			for (int32_t y : down)
			{
				MenuBar each;

				each.Load(menus, (size_t)EditorMenu::Count);
				LayOutTheBar(&each, stretch.pictureWidth, stretch.pictureHeight);

				int32_t px = x;
				int32_t py = y;

				if (moved)
				{
					HostFrame_WindowToPanels(x, y, stretch.windowWidth, stretch.windowHeight, stretch.pictureWidth, stretch.pictureHeight, &px, &py);
				}

				(*clicks)++;
				*landed += each.Press(px, py) == MenuAnswer::Opened && each.OpenMenu() == (int)m;
			}
		}

		// The menu open, each item it shows, clicked the same way: an item that's on is picked, with its own action
		bar.Press(title.x, title.y);

		if (bar.OpenMenu() != (int)m)
		{
			(*clicks)++;
			continue;
		}

		Box const drop = bar.DropBox();
		int32_t const itemHeight = host::MenuPaint_ItemHeight(kLineHeight);

		for (size_t s = 0; s < bar.ShownItems(); s++)
		{
			size_t const item = bar.FirstShownItem() + s;

			if (!bar.IsEnabled(m, item))
			{
				continue;
			}

			int32_t const top = (int32_t)(drop.y + (int64_t)s * itemHeight);
			std::vector<int32_t> const itemAcross = ThreeOf(PixelsShowing(drop.x, drop.x + drop.width - 1, stretch.windowWidth, stretch.pictureWidth));
			std::vector<int32_t> const itemDown = ThreeOf(PixelsShowing(top, top + itemHeight - 1, stretch.windowHeight, stretch.pictureHeight));

			for (int32_t x : itemAcross)
			{
				for (int32_t y : itemDown)
				{
					MenuBar each;

					each.Load(menus, (size_t)EditorMenu::Count);
					LayOutTheBar(&each, stretch.pictureWidth, stretch.pictureHeight);
					each.Press(title.x, title.y);

					int32_t px = x;
					int32_t py = y;

					if (moved)
					{
						HostFrame_WindowToPanels(x, y, stretch.windowWidth, stretch.windowHeight, stretch.pictureWidth, stretch.pictureHeight, &px, &py);
					}

					(*clicks)++;
					*landed += each.Press(px, py) == MenuAnswer::Picked && each.Picked() == menus[m].items[item].action;
				}
			}
		}
	}
}

static void TestTheMenusUnderTheClick()
{
	puts("A click lands on the title or item drawn under it:");

	Check(EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count, items,
		sizeof(items) / sizeof(items[0]), words, sizeof(words) / sizeof(words[0])), "(the editor's own menus are made, with the Classic keys)");

	Stretch const stretches[] =
	{
		{ 1024, 768, 1024, 768, "a 1024 by 768 window, its picture the same size, as the editor's keeper makes it" },
		{ 1600, 900, 1600, 900, "a 1600 by 900 window, its picture the same size" },
		{ 1280, 960, 1024, 768, "a 1280 by 960 window over a 1024 by 768 picture (a 1.25 stretch, Windows' 125%)" },
		{ 2000, 1125, 1600, 900, "a 2000 by 1125 window over a 1600 by 900 picture (1.25)" },
		{ 1536, 1152, 1024, 768, "a 1536 by 1152 window over a 1024 by 768 picture (a 1.5 stretch, Windows' 150%)" },
		{ 2400, 1350, 1600, 900, "a 2400 by 1350 window over a 1600 by 900 picture (1.5)" },
		{ 1024, 768, 800, 600, "a 1024 by 768 window over an 800 by 600 picture (the game's own first size)" },
	};

	for (const Stretch &stretch : stretches)
	{
		int clicks = 0;
		int landed = 0;

		ClickEverything(stretch, true, &clicks, &landed);
		Check(clicks > 0 && landed == clicks, std::string(stretch.words) + ": " + std::to_string(landed) + " of " + std::to_string(clicks)
			+ " clicks at the left, middle and right of every title and item land on it");
	}

	// The test can fail: the editor before the fix, a 1.5 stretch with the click tested where it came in
	int clicks = 0;
	int landed = 0;

	ClickEverything(stretches[4], false, &clicks, &landed);
	Check(clicks > 0 && landed < clicks, "without moving the click into the picture, a 1.5 stretch sends " + std::to_string(clicks - landed) + " of "
		+ std::to_string(clicks) + " clicks somewhere else, so this test would have caught the bug");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheMenusUnderTheClick();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
