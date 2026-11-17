// The test for inteng's text layout (inteng_text.h; view-state-table.md H-1f: FUN_0057be40's new lines, tabs and wrapping). Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdio.h>

#include <string>

#include "inteng_text.h"

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

// every letter 10 pixels wide, a space 5
static int Ten(int letter)
{
	return letter == ' ' ? 5 : 10;
}

static std::string Show(std::vector<IntengTextPiece> const &pieces)
{
	std::string all;

	for (IntengTextPiece const &p : pieces)
	{
		all += "[" + p.text + "@" + std::to_string(p.x) + "," + std::to_string(p.y) + "]";
	}

	return all;
}

static bool Is(std::vector<IntengTextPiece> const &pieces, std::string const &want)
{
	std::string const got = Show(pieces);

	if (got != want)
	{
		printf("      got  %s\n      want %s\n", got.c_str(), want.c_str());
	}

	return got == want;
}

int main()
{
	puts("inteng's tabs (FUN_0057be40 0x57bf7d: x to ((x / (4 x height)) + 1) x 4 x height, from the screen's left):");
	Check(Is(IntengText_Layout("H\t\tHide object", 0, 0, 1008, 729, Ten, 16, 64), "[H@0,0][Hide object@128,0]"),
		"\"H\\t\\tHide object\" (Module Linking's Help): H at 0, the first tab to 64, the second to 128");
	Check(Is(IntengText_Layout("Shift+S\t\tSet", 0, 0, 1008, 729, Ten, 16, 64), "[Shift+S@0,0][Set@192,0]"),
		"\"Shift+S\" is 70 wide (past 64): its first tab goes to 128, its second to 192, so its words start a column later than H's");
	Check(Is(IntengText_Layout("\tA", 64, 0, 1008, 729, Ten, 16, 64), "[A@128,0]"), "a tab from exactly 64 goes on to 128 (always the NEXT multiple)");
	Check(Is(IntengText_Layout("A\tB", 0, 0, 1008, 729, Ten, 8, 64), "[A@0,0][B@32,0]"), "with font8 (height 8) a tab is 32 pixels");

	puts("New lines (the first x, down by the height):");
	Check(Is(IntengText_Layout("A\nB\n\nC", 10, 20, 1008, 729, Ten, 16, 64), "[A@10,20][B@10,36][C@10,68]"), "each line 16 below the last, at the first x; an empty line is 16 too");

	puts("Wrapping at the screen's right edge (0x57bef0 a word, 0x57bfc0 a letter):");
	Check(Is(IntengText_Layout("aaaaaaa bbb", 0, 0, 100, 729, Ten, 16, 64), "[aaaaaaa @0,0][bbb@0,16]"),
		"a word that would reach the edge, narrower than half the screen, starts the next line");
	Check(Is(IntengText_Layout("aaaa bbbbbbb", 0, 0, 100, 729, Ten, 16, 64), "[aaaa bbbbb@0,0][bb@0,16]"),
		"a word half the screen wide or more is cut where its letter reaches the edge");
	Check(Is(IntengText_Layout("wwwwwwwwwwww", 0, 0, 5, 729, Ten, 16, 64), ""), "a letter wider than the whole screen ends the text");

	puts("The screen's bottom (0x57bf2d, 0x57c18a: a line whose bottom would pass it ends the text):");
	Check(Is(IntengText_Layout("a\nb\nc", 0, 0, 1008, 40, Ten, 16, 64), "[a@0,0][b@0,16]"), "on a 40-high screen the lines at 0 and 16 fit, the one at 32 would end at 48");
	Check(Is(IntengText_Layout("a", 0, 30, 1008, 40, Ten, 16, 64), ""), "a text that starts too low draws nothing");

	puts("Hostile input:");
	Check(Is(IntengText_Layout(std::string("a\xc3\xa9z"), 0, 0, 1008, 729, Ten, 16, 64), "[az@0,0]"), "letters 128 and above (not in inteng's font) are drawn as nothing and take no room");
	Check(IntengText_Layout("a\tb\tc\td", 0, 0, 1008, 729, Ten, 16, 2).size() == 2, "at most the pieces asked for");
	Check(IntengText_Layout("abc", 0, 0, 1008, 729, nullptr, 16, 64).empty(), "no width function: nothing");
	Check(IntengText_Layout("abc", 0, 0, 1008, 729, Ten, 0, 64).empty(), "a height of 0: nothing");
	Check(IntengText_Layout("abc", 0, 0, 0, 729, Ten, 16, 64).empty(), "a screen 0 wide: nothing");
	Check(IntengText_Layout(std::string(100000, '\t') + "x", 0, 0, 2147483647, 729, Ten, 16, 64).size() == 1, "100,000 tabs on the widest screen: the place never overflows");
	Check(IntengText_Layout("", 0, 0, 1008, 729, Ten, 16, 64).empty(), "an empty text: nothing");

	printf("%s\n", checksFailed == 0 ? "All checks passed" : "SOME CHECKS FAILED");
	return checksFailed == 0 ? 0 : 1;
}
