// The test for src/host's panel paint: each panel's text made safe, cut to fit its box, and placed, with a pretend font. Written 2026-11-14
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It draws nothing and needs no window: it checks the text the panel pass would draw, and where. The layout is the real one
// (src/ui/panel_layout.cpp), and the safe-text rules are the game's own (avpx's online_screens.cpp). Each check's words say what it
// proves. The program gives back a non-zero result if any check failed. tests/takeouts/panel_paint_test.json proves each check can fail.

#include <limits.h>
#include <stdio.h>

#include <string>

#include "panel_paint.h"

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

// A pretend font: every letter 8 pixels wide but a full stop, 2. Lines are 16 tall.
static int PretendWidth(int letter)
{
	return letter == '.' ? 2 : 8;
}

int32_t const kPretendLine = 16;

// Fonts that say what no font should
static int NegativeWidth(int)
{
	return -100;
}

static int HugeWidth(int)
{
	return INT_MAX;
}

static int64_t WidthOf(const std::string &text)
{
	int64_t width = 0;

	for (char letter : text)
	{
		width += PretendWidth((unsigned char)letter);
	}

	return width;
}

static bool EndsWithDots(const std::string &text)
{
	return text.size() >= 2 && text.compare(text.size() - 2, 2, "..") == 0;
}

static void TestSafeText()
{
	puts("Letters that are safe to show:");

	std::string withCodes = "a\tb\nc";
	withCodes += '\x7F';
	withCodes += '\x01';
	Check(PanelPaint_SafeText(withCodes) == "a?b?c??", "a control code, or 127, is shown as '?'");

	std::string withZero("ab", 2);
	withZero += '\0';
	withZero += "cd";
	Check(PanelPaint_SafeText(withZero) == "ab", "a 0 ends the text there, as the game's text calls read it");

	Check(PanelPaint_SafeText("%n%s%x %d") == "%n%s%x %d", "\"%n\" and \"%s\" are only the letters they are: nothing reads them as a format");

	Check(PanelPaint_SafeText("caf\xE9 \xFF") == "caf? ?", "a letter above 126, which the font has no picture of, is shown as '?'");

	std::string allBytes;

	for (int b = 1; b < 256; b++)
	{
		allBytes += (char)b;
	}

	std::string const shownAll = PanelPaint_SafeText(allBytes);
	bool onlyPrintable = shownAll.size() == 255;

	for (char letter : shownAll)
	{
		onlyPrintable = onlyPrintable && (unsigned char)letter >= 32 && (unsigned char)letter <= 126;
	}

	Check(onlyPrintable, "every byte from 1 to 255 comes out as a letter from the space to the tilde, one for one");

	std::string const huge(100000, 'x');
	Check(PanelPaint_SafeText(huge).size() == kPaintMostLetters, "a text of 100,000 letters keeps its first 255");
	Check(PanelPaint_SafeText("").empty(), "and no text stays no text");
}

static void TestFit()
{
	puts("Text cut to fit its box:");

	PaintText line;
	ui::Box const box = { 100, 50, 200, 40 };   // 192 pixels of room inside the margins: 24 letters

	Check(PanelPaint_FitText("Objects", box, PretendWidth, kPretendLine, &line) && line.text == "Objects" && line.x == 104 && line.y == 52,
		"text that fits is shown whole, at the box's top left inside the margin");

	std::string const exactly(24, 'a');
	Check(PanelPaint_FitText(exactly, box, PretendWidth, kPretendLine, &line) && line.text == exactly, "text exactly as wide as the room is shown whole");

	std::string const longer(40, 'b');
	Check(PanelPaint_FitText(longer, box, PretendWidth, kPretendLine, &line) && line.text == std::string(23, 'b') + ".."
		&& WidthOf(line.text) <= 192, "longer text is cut, ends with two dots, and still fits (" + line.text + ")");

	ui::Box const noLetter = { 0, 0, 2 * kPaintMarginAcross + 9, 40 };   // 9 pixels: no letter and the dots, but the dots
	Check(PanelPaint_FitText("hello", noLetter, PretendWidth, kPretendLine, &line) && line.text == "..",
		"in room for the dots and no letter beside them, only the dots are shown");

	ui::Box const noDots = { 0, 0, 2 * kPaintMarginAcross + 3, 40 };
	Check(!PanelPaint_FitText("hello", noDots, PretendWidth, kPretendLine, &line), "in room for not even the dots, nothing is");

	ui::Box const oneShort = { 0, 0, 2 * kPaintMarginAcross + 7, 40 };
	Check(!PanelPaint_FitText("x", oneShort, PretendWidth, kPretendLine, &line), "nor is one letter that doesn't fit");

	ui::Box const noRoom = { 0, 0, 2 * kPaintMarginAcross, 40 };
	ui::Box const notTall = { 0, 0, 200, 2 * kPaintMarginDown + 15 };
	ui::Box const negative = { 0, 0, -50, -50 };
	Check(!PanelPaint_FitText("x", noRoom, PretendWidth, kPretendLine, &line) && !PanelPaint_FitText("x", notTall, PretendWidth, kPretendLine, &line)
		&& !PanelPaint_FitText("x", negative, PretendWidth, kPretendLine, &line),
		"a box with no room across, or not one line tall, or of a size below 0, shows nothing");

	Check(!PanelPaint_FitText("", box, PretendWidth, kPretendLine, &line), "nor does no text");
	Check(!PanelPaint_FitText("x", box, PretendWidth, kPretendLine, nullptr) && !PanelPaint_FitText("x", box, nullptr, kPretendLine, &line),
		"and nowhere to put it, or no font measure, is nothing");

	ui::Box const atTheEnd = { INT32_MAX - 2, 0, 200, 40 };
	Check(!PanelPaint_FitText("x", atTheEnd, PretendWidth, kPretendLine, &line),
		"a box so near the end of the numbers that its text's place wouldn't fit shows nothing");

	ui::Box const widest = { 0, 0, INT32_MAX, INT32_MAX };
	std::string const hostile(100000, 'w');
	Check(PanelPaint_FitText(hostile, widest, PretendWidth, kPretendLine, &line) && line.text.size() == kPaintMostLetters,
		"the widest box there is takes the 255 letters kept, with no sum going wrong");
}

static void TestHostileFonts()
{
	puts("A font that says what no font should:");

	PaintText line;
	ui::Box const box = { 0, 0, 200, 40 };

	Check(PanelPaint_FitText("abc", box, NegativeWidth, kPretendLine, &line) && line.text == "abc", "letters said to be less than 0 wide count as 0");

	// Letters of no width fit any room, even none: so a box with no room across has to show nothing by itself
	ui::Box const noRoom = { 0, 0, 2 * kPaintMarginAcross, 40 };
	Check(!PanelPaint_FitText("abc", noRoom, NegativeWidth, kPretendLine, &line), "and still, a box with no room across shows nothing");
	Check(!PanelPaint_FitText("abcdef", box, HugeWidth, kPretendLine, &line),
		"letters said to be huge count as 1000 at most: in 192 pixels not even the dots fit, so nothing is shown");

	Check(PanelPaint_FitText("abc", box, PretendWidth, 0, &line), "a line said to be 0 tall counts as 1");

	ui::Box const tallBox = { 0, 0, 200, 300 };
	Check(!PanelPaint_FitText("abc", box, PretendWidth, INT32_MAX, &line) && PanelPaint_FitText("abc", tallBox, PretendWidth, INT32_MAX, &line),
		"a line said to be huge counts as 256 at most: too tall for 40, not for 300");
}

static void TestPlan()
{
	puts("Every panel's line:");

	PanelTexts texts;
	texts.toolbar = "Save";
	texts.list = "Objects";
	texts.fields = "Fields";
	texts.status = "Genshd1";

	ui::PanelLayout const roomy = ui::LayOutPanels(1600, 900, ui::kPanelSizesAsBuilt);
	PaintText items[8];
	size_t const count = PanelPaint_Plan(roomy, texts, PretendWidth, kPretendLine, items, 8);

	Check(roomy.roomy && count == 4, "a roomy window shows four lines: toolbar, list, field panel and status line");
	Check(count == 4 && items[0].text == "Save" && items[1].text == "Objects" && items[2].text == "Fields" && items[3].text == "Genshd1",
		"in that order, each its own panel's");
	Check(count == 4 && items[1].x == roomy.list.x + kPaintMarginAcross && items[1].y == roomy.list.y + kPaintMarginDown
		&& items[3].x == roomy.status.x + kPaintMarginAcross && items[3].y == roomy.status.y + kPaintMarginDown,
		"each at its own box's top left, inside the margin");

	bool noneInTheView = true;

	for (size_t i = 0; i < count; i++)
	{
		bool const inView = items[i].x >= roomy.view.x && items[i].x < roomy.view.x + roomy.view.width && items[i].y >= roomy.view.y
			&& items[i].y < roomy.view.y + roomy.view.height;
		noneInTheView = noneInTheView && !inView;
	}

	Check(noneInTheView, "and none in the level's view");

	ui::PanelLayout const small = ui::LayOutPanels(500, 300, ui::kPanelSizesAsBuilt);
	size_t const smallCount = PanelPaint_Plan(small, texts, PretendWidth, kPretendLine, items, 8);
	bool saysTooSmall = false;

	for (size_t i = 0; i < smallCount; i++)
	{
		saysTooSmall = saysTooSmall || (items[i].y == small.status.y + kPaintMarginDown && items[i].text.find("The window") == 0);
	}

	Check(!small.roomy && saysTooSmall, "a window too small for every panel says so in the status line");

	Check(PanelPaint_Plan(roomy, texts, PretendWidth, kPretendLine, items, 2) == 2, "no more lines than there's room for in the list given");
	Check(PanelPaint_Plan(roomy, texts, PretendWidth, kPretendLine, nullptr, 8) == 0, "and none with no list at all");

	ui::PanelLayout const none = ui::LayOutPanels(0, 0, ui::kPanelSizesAsBuilt);
	Check(PanelPaint_Plan(none, texts, PretendWidth, kPretendLine, items, 8) == 0, "a window of no size shows nothing");

	PanelTexts hostile;
	hostile.list = std::string(5000, '\n') + "%n";
	size_t const hostileCount = PanelPaint_Plan(roomy, hostile, PretendWidth, kPretendLine, items, 8);
	bool hostileSafe = hostileCount >= 1;

	for (size_t i = 0; i < hostileCount; i++)
	{
		hostileSafe = hostileSafe && items[i].text.find('\n') == std::string::npos && items[i].text.size() <= kPaintMostLetters
			&& WidthOf(items[i].text) <= 1600;
	}

	Check(hostileSafe && EndsWithDots(items[0].text), "a list line of 5,000 new lines and a \"%n\" is shown as safe letters, cut, with its dots");
}

static bool Is(const PaintBox &box, int32_t x, int32_t y, int32_t width, int32_t height)
{
	return box.x == x && box.y == y && box.width == width && box.height == height;
}

static void TestCutBox()
{
	puts("A box cut to the window:");

	PaintBox cut;
	PaintBox const inside = { 10, 20, 100, 50, 0xFF112233u };
	Check(PanelPaint_CutBox(inside, 800, 600, &cut) && Is(cut, 10, 20, 100, 50) && cut.colour == 0xFF112233u,
		"a box inside the window stays as it is, colour and all");

	PaintBox const leftAndTop = { -30, -40, 100, 100, 1u };
	Check(PanelPaint_CutBox(leftAndTop, 800, 600, &cut) && Is(cut, 0, 0, 70, 60), "past the left and the top, it's cut at both");

	PaintBox const rightAndBottom = { 750, 580, 100, 100, 1u };
	Check(PanelPaint_CutBox(rightAndBottom, 800, 600, &cut) && Is(cut, 750, 580, 50, 20), "past the right and the bottom, it's cut at both");

	PaintBox const all = { -1000, -1000, 5000, 5000, 1u };
	Check(PanelPaint_CutBox(all, 800, 600, &cut) && Is(cut, 0, 0, 800, 600), "bigger than the window all round, it's the window");

	PaintBox const outside[] = { { 800, 0, 10, 10, 1u }, { 0, 600, 10, 10, 1u }, { -10, 0, 10, 10, 1u }, { 0, -10, 10, 10, 1u } };
	bool noneInside = true;

	for (const PaintBox &box : outside)
	{
		noneInside = noneInside && !PanelPaint_CutBox(box, 800, 600, &cut);
	}

	Check(noneInside, "wholly outside on any side, even touching the edge, it's nothing");

	PaintBox const empty = { 10, 10, 0, 10, 1u };
	PaintBox const negative = { 10, 10, -5, -5, 1u };
	Check(!PanelPaint_CutBox(empty, 800, 600, &cut) && !PanelPaint_CutBox(negative, 800, 600, &cut), "a box of no size, or of a size below 0, is nothing");
	Check(!PanelPaint_CutBox(inside, 0, 600, &cut) && !PanelPaint_CutBox(inside, 800, -1, &cut), "and so is any box in a window of no size");
	Check(!PanelPaint_CutBox(inside, 800, 600, nullptr), "and nowhere to put it is nothing");

	PaintBox const huge = { INT32_MAX - 5, INT32_MAX - 5, INT32_MAX, INT32_MAX, 1u };
	PaintBox const wide = { INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, 1u };
	Check(!PanelPaint_CutBox(huge, 800, 600, &cut) && !PanelPaint_CutBox(wide, 800, 600, &cut),
		"boxes at the ends of the numbers, whose edges would wrap round in 32 bits, are worked out right: both outside");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestSafeText();
	TestFit();
	TestHostileFonts();
	TestPlan();
	TestCutBox();

	printf("%s: %d check(s) failed\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed", checksFailed);
	return checksFailed ? 1 : 0;
}
