// The test that a click lands on what is drawn under it with TWO menu bars: the code-built screens of inteng (Texture Map, Strategy Data, and every other screen with a second row) show two rows of
// menus, the first on top. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It is host_click_test.cpp's test for both bars. For each such screen it makes the two bars from inteng's own tables (EditorBar_Make, EditorBar_MakeSecond), lays them out as the editor does
// (EditorBar_Sizes, EditorBar_Place: the same functions the editor calls), and takes the places the bars give each title, works out which of the window's pixels show it once the panels'
// picture is stretched over the window, and clicks the first, the middle and the last of those pixels, in both directions. Each click is moved into the picture by HostFrame_WindowToPanels as the editor
// moves every click, and must open THAT title's menu of THAT bar and no menu of the other. At the editor's own size, at 1.25 and 1.5 (Windows' 125% and 150% scaling) and at a second window size.
// And it checks it can fail: without the move into the picture a 1.5 stretch must send some click to the wrong place.
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

// A font whose letters are not all one width, so the titles' places come from real sums
static int LetterWidth(int letter)
{
	return 5 + (letter & 255) % 5;
}

static int32_t const kLineHeight = 15;

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

static EditorBarRoom room1;
static EditorBarRoom room2;

// The two bars of a screen, laid out in a picture of pictureWidth by pictureHeight as the editor lays them out each frame
static bool MakeTheBars(size_t screen, int32_t pictureWidth, int32_t pictureHeight, MenuBar *bar1, MenuBar *bar2)
{
	if (!EditorBar_Make(screen, KeyLayout::Classic, &room1) || !EditorBar_MakeSecond(screen, &room2) || room2.menuCount == 0 || !bar1->Load(room1.menus, room1.menuCount) ||
		!bar2->Load(room2.menus, room2.menuCount))
	{
		return false;
	}

	PanelLayout const layout = LayOutPanels(pictureWidth, pictureHeight, EditorBar_Sizes(HostFrame_PanelSizes(pictureWidth), true));
	EditorBarPlace const place = EditorBar_Place(layout, true);
	int32_t titles1[kMenuMostMenus], drops1[kMenuMostMenus], titles2[kMenuMostMenus], drops2[kMenuMostMenus];

	host::MenuPaint_Measure(room1.menus, room1.menuCount, LetterWidth, titles1, drops1);
	host::MenuPaint_Measure(room2.menus, room2.menuCount, LetterWidth, titles2, drops2);
	bar1->LayOut(place.bar1, place.dropRoom1, titles1, drops1, host::MenuPaint_ItemHeight(kLineHeight));
	bar2->LayOut(place.bar2, place.dropRoom2, titles2, drops2, host::MenuPaint_ItemHeight(kLineHeight));
	return true;
}

// Clicks the first, middle and last pixels showing each title of both bars of a screen; counts the clicks that opened the right menu of the right bar (and no menu of the other)
static void ClickBothBars(size_t screen, const Stretch &stretch, bool moved, int *clicks, int *landed, int *hidden)
{
	*clicks = 0;
	*landed = 0;
	*hidden = 0;

	MenuBar layoutBar1, layoutBar2;

	if (!MakeTheBars(screen, stretch.pictureWidth, stretch.pictureHeight, &layoutBar1, &layoutBar2))
	{
		return;
	}

	for (int row = 0; row < 2; row++)
	{
		size_t const count = row == 0 ? room1.menuCount : room2.menuCount;

		for (size_t m = 0; m < count; m++)
		{
			Box const title = (row == 0 ? layoutBar1 : layoutBar2).TitleBox(m);

			if (title.width <= 0)
			{
				(*hidden)++;   // a title past the window's edge: the editor's window is never that narrow (its least width counts both rows)
				continue;
			}

			std::vector<int32_t> const across = ThreeOf(PixelsShowing(title.x, title.x + title.width - 1, stretch.windowWidth, stretch.pictureWidth));
			std::vector<int32_t> const down = ThreeOf(PixelsShowing(title.y, title.y + title.height - 1, stretch.windowHeight, stretch.pictureHeight));

			for (int32_t x : across)
			{
				for (int32_t y : down)
				{
					MenuBar bar1, bar2;

					MakeTheBars(screen, stretch.pictureWidth, stretch.pictureHeight, &bar1, &bar2);

					int32_t px = x;
					int32_t py = y;

					if (moved)
					{
						HostFrame_WindowToPanels(x, y, stretch.windowWidth, stretch.windowHeight, stretch.pictureWidth, stretch.pictureHeight, &px, &py);
					}

					// The editor gives a press to the bar whose strip it is in (hitBar / hitBar2); here the strip is the bar's own box
					MenuBar &mine = row == 0 ? bar1 : bar2;
					MenuBar &other = row == 0 ? bar2 : bar1;
					bool const inMine = BoxHolds(mine.TitleBox(m), px, py);
					bool const inOther = BoxHolds(other.TitleBox(0), px, py) || BoxHolds(Box{ other.TitleBox(0).x, other.TitleBox(0).y, 100000, other.TitleBox(0).height }, px, py);

					(*clicks)++;
					*landed += inMine && !inOther && mine.Press(px, py) == MenuAnswer::Opened && mine.OpenMenu() == (int)m && !other.IsOpen();
				}
			}
		}
	}
}

static void TestBothBars()
{
	puts("A click lands on the title drawn under it, in both bars:");

	// The screens with a second row, by their names in inteng's tables
	std::vector<size_t> screens;

	for (size_t s = 0; s < host::kIntengScreenCount; s++)
	{
		if (host::kIntengScreens[s].secondRow && host::kIntengScreens[s].secondRowCount > 0)
		{
			screens.push_back(s);
		}
	}

	Check(screens.size() >= 5, "(inteng's tables give " + std::to_string(screens.size()) + " screens a second row: Texture Map, Strategy Data, Path, Waypoint, Hierarchy)");

	bool haveTextureMap = false, haveStrategy = false;

	for (size_t s : screens)
	{
		haveTextureMap = haveTextureMap || std::string(host::kIntengScreens[s].name) == "Texture Map";
		haveStrategy = haveStrategy || std::string(host::kIntengScreens[s].name) == "Strategy Data";
	}

	Check(haveTextureMap && haveStrategy, "(Texture Map and Strategy Data are among them)");

	Stretch const stretches[] =
	{
		{ 1600, 900, 1600, 900, "a 1600 by 900 window, its picture the same size" },
		{ 1920, 1080, 1920, 1080, "a 1920 by 1080 window, its picture the same size" },
		{ 2000, 1125, 1600, 900, "a 2000 by 1125 window over a 1600 by 900 picture (a 1.25 stretch, Windows' 125%)" },
		{ 2400, 1350, 1600, 900, "a 2400 by 1350 window over a 1600 by 900 picture (1.5, Windows' 150%)" },
		{ 1800, 1012, 1200, 675, "a 1800 by 1012 window over a 1200 by 675 picture (1.5, a narrower window)" },
		{ 2880, 1620, 1920, 1080, "a 2880 by 1620 window over a 1920 by 1080 picture (1.5)" },
	};

	for (size_t s : screens)
	{
		for (const Stretch &stretch : stretches)
		{
			int clicks = 0, landed = 0, hidden = 0;

			ClickBothBars(s, stretch, true, &clicks, &landed, &hidden);
			Check(clicks > 0 && landed == clicks && hidden == 0, std::string(host::kIntengScreens[s].name) + ", " + stretch.words + ": " + std::to_string(landed) + " of " + std::to_string(clicks) +
				" clicks at the left, middle and right of every title of both bars open that title's menu" + (hidden ? " (" + std::to_string(hidden) + " titles past the edge)" : ""));
		}
	}

	// The test can fail: a 1.5 stretch with the click tested where it came in
	int clicks = 0, landed = 0, hidden = 0;

	if (!screens.empty())
	{
		ClickBothBars(screens[0], stretches[3], false, &clicks, &landed, &hidden);
		Check(clicks > 0 && landed < clicks, "without moving the click into the picture, a 1.5 stretch sends " + std::to_string(clicks - landed) + " of " + std::to_string(clicks) +
			" clicks somewhere else, so this test would have caught the bug");
	}
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestBothBars();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
