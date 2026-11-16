// The test for src/host's menu drawing plan: what the menu bar shows, and where. Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor, and draws nothing: it checks the boxes and lines the panel pass would draw. The font is made up: every
// letter 6 pixels wide, so every place can be worked out by hand. Each check's words say what it proves. The program gives back a
// non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>

#include "menu_paint.h"

using namespace ui;
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

static int SixWide(int)
{
	return 6;
}

// A font that says a letter is less than nothing wide: it counts as 0
static int BelowZero(int)
{
	return -9;
}

static const MenuItem kFile[] = { { "Open", "Ctrl+O", 1 }, { "Quit", 0, 2 } };
static const MenuItem kHelp[] = { { "About", 0, 3 } };
static const Menu kBar[] = { { "File", kFile, 2 }, { "Help", kHelp, 1 } };

static bool LineIs(const PaintText &line, const char *text, int32_t x, int32_t y)
{
	return line.text == text && line.x == x && line.y == y;
}

static bool BoxIs(const PaintBox &box, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour)
{
	return box.x == x && box.y == y && box.width == width && box.height == height && box.colour == colour;
}

static void TestMeasure()
{
	puts("Measuring:");

	int32_t titles[2] = { -1, -1 };
	int32_t drops[2] = { -1, -1 };

	MenuPaint_Measure(kBar, 2, SixWide, titles, drops);
	Check(titles[0] == 4 * 6 + 8 && titles[1] == 4 * 6 + 8, "a title is its words, 6 a letter, and the margin of 4 each side");
	Check(drops[0] == 4 * 6 + 8 + kMenuKeysGap + 6 * 6 + 8, "File's drop-down is its widest item and its widest keys, each with the margin each side, and the gap");
	Check(drops[1] == 5 * 6 + 8, "Help's has no keys, so no gap for them");
	Check(MenuPaint_ItemHeight(15) == 19 && MenuPaint_ItemHeight(0) == 5 && MenuPaint_ItemHeight(100000) == kPaintTallestLine + 4,
		"an item is a line of the font, 1 to 256, and the margin of 2 above and below");

	MenuPaint_Measure(kBar, 2, BelowZero, titles, drops);
	Check(titles[0] == 8 && drops[1] == 8, "a letter less than nothing wide counts as 0");

	std::string const huge(300, 'x');
	MenuItem const wide[] = { { huge.c_str(), 0, 9 } };
	Menu const wideMenu = { huge.c_str(), wide, 1 };
	MenuPaint_Measure(&wideMenu, 1, SixWide, titles, drops);
	Check(titles[0] == 255 * 6 + 8 && drops[0] == 255 * 6 + 8, "a text is measured as it's shown: at most 255 letters");

	MenuPaint_Measure(0, 2, SixWide, titles, drops);
	MenuPaint_Measure(kBar, 2, SixWide, 0, drops);
	Check(true, "no menus or no room measures nothing, and doesn't fall over");
}

static void LayOut(MenuBar &bar)
{
	Box const toolbar = { 0, 0, 1024, 32 };
	Box const room = { 0, 32, 260, 700 };
	int32_t titles[2];
	int32_t drops[2];

	MenuPaint_Measure(kBar, 2, SixWide, titles, drops);
	bar.LayOut(toolbar, room, titles, drops, MenuPaint_ItemHeight(15));
}

static PaintText lines[64];
static PaintBox boxes[64];

static void TestPlan()
{
	puts("The plan:");

	MenuBar bar;
	bar.Load(kBar, 2);
	LayOut(bar);

	size_t lineCount = 99;
	size_t boxCount = 99;

	MenuPaint_Plan(bar, kBar, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount == 2 && boxCount == 0 && LineIs(lines[0], "File", 4, 2) && LineIs(lines[1], "Help", 32 + 4, 2),
		"closed: each title at its box's top left inside the margin, and no boxes");

	bar.Handle(KeyEvent(Key::Menu));
	MenuPaint_Plan(bar, kBar, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);

	int32_t const dropWidth = 4 * 6 + 8 + kMenuKeysGap + 6 * 6 + 8;
	Check(boxCount == 3 && BoxIs(boxes[0], 0, 0, 32, 32, kMenuLitColour) && BoxIs(boxes[1], 0, 32, dropWidth, 2 * 19, kMenuDropColour)
		&& BoxIs(boxes[2], 0, 32, dropWidth, 19, kMenuLitColour), "open: the open title's band, the drop-down's background, then the lit item's band");
	Check(lineCount == 5 && LineIs(lines[2], "Open", 4, 32 + 2) && LineIs(lines[3], "Ctrl+O", dropWidth - (6 * 6 + 8) + 4, 32 + 2) && LineIs(lines[4], "Quit", 4, 32 + 19 + 2),
		"and each item's words at its row's left, its keys at the right, row by row");

	bar.Handle(KeyEvent(Key::Down));
	MenuPaint_Plan(bar, kBar, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(BoxIs(boxes[2], 0, 32 + 19, dropWidth, 19, kMenuLitColour), "the lit band follows the lit item");

	MenuPaint_Plan(bar, kBar, SixWide, 15, lines, 3, &lineCount, boxes, 2, &boxCount);
	Check(lineCount == 3 && boxCount == 2, "it never writes more lines or boxes than it has room for");

	MenuBar empty;
	MenuPaint_Plan(empty, kBar, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount == 0 && boxCount == 0, "a bar with no menus shows nothing");
	MenuPaint_Plan(bar, 0, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount == 0 && boxCount == 0, "and so does one asked with no menus");
}

static void TestHostileWords()
{
	puts("Words that aren't the font's:");

	// The bar itself refuses words that aren't font letters. Here the plan is asked with a bar loaded from good menus, but handed odd
	// words, as a mistake would hand them, to see it still makes every word safe and never reads one as a format
	MenuBar bar;
	bar.Load(kBar, 2);
	LayOut(bar);
	bar.Handle(KeyEvent(Key::Menu));

	size_t lineCount = 0;
	size_t boxCount = 0;
	MenuItem const oddFile[] = { { "Op%s\x01n", 0, 1 }, { "Quit", 0, 2 } };
	Menu const oddFirst[] = { { "F%n", oddFile, 2 }, { "Help", kHelp, 1 } };

	MenuPaint_Plan(bar, oddFirst, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount >= 3 && lines[0].text == "F%n" && lines[2].text == "Op%s?n", "a % is only its letter, never a format, and a control letter shows as ?");
}

static void TestTags()
{
	puts("A tag is never cut:");

	MenuItem const tagged[] = { { "Fix tears between Objects (Selected)", 0, 1, " [Blender]" }, { "Quit", 0, 2 } };
	Menu const tagBar[] = { { "Object", tagged, 2 } };
	MenuBar bar;
	int32_t titles[1];
	int32_t drops[1];

	Check(bar.Load(tagBar, 1), "(a menu with a tagged item loads)");
	MenuPaint_Measure(tagBar, 1, SixWide, titles, drops);
	Check(drops[0] == (36 + 10) * 6 + 8, "the drop-down is measured with the tag: the name's 36 letters and the tag's 10, 6 wide, and the margins");

	// With room, the name is whole and the tag follows it at once
	Box const toolbar = { 0, 0, 1024, 32 };
	size_t lineCount = 0;
	size_t boxCount = 0;

	bar.LayOut(toolbar, Box{ 0, 32, 400, 700 }, titles, drops, MenuPaint_ItemHeight(15));
	bar.Handle(KeyEvent(Key::Menu));
	MenuPaint_Plan(bar, tagBar, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount == 4 && LineIs(lines[1], "Fix tears between Objects (Selected)", 4, 32 + 2) && LineIs(lines[2], " [Blender]", 36 * 6 + 4, 32 + 2)
		&& LineIs(lines[3], "Quit", 4, 32 + 19 + 2), "with room, the name is whole and its tag follows it at once");

	// In a narrow room the name is shortened, never the tag
	bar.Handle(KeyEvent(Key::Escape));
	bar.LayOut(toolbar, Box{ 0, 32, 150, 700 }, titles, drops, MenuPaint_ItemHeight(15));
	bar.Handle(KeyEvent(Key::Menu));
	MenuPaint_Plan(bar, tagBar, SixWide, 15, lines, 64, &lineCount, boxes, 64, &boxCount);
	Check(lineCount == 4 && lines[1].text.size() < 36 && lines[1].text.find("..") != std::string::npos,
		"in a narrow room the name is shortened, and says so with the game's \"..\"");
	Check(lineCount == 4 && lines[2].text == " [Blender]" && lines[2].x == 4 + (int32_t)lines[1].text.size() * 6 && lines[2].x + 10 * 6 <= 150,
		"and the tag is whole, right after the shortened name, inside the drop-down");
}

int main()
{
	TestMeasure();
	TestPlan();
	TestTags();
	TestHostileWords();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
