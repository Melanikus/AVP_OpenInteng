// The test for src/host's places in the editor's window: which panel a click lands in. Written
// 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It opens no window. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "host_frame.h"

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

static bool BoxIs(const Box &box, int32_t x, int32_t y, int32_t width, int32_t height)
{
	return box.x == x && box.y == y && box.width == width && box.height == height;
}

static void TestBoxes()
{
	puts("A box's points:");

	Box const box = { 10, 20, 30, 40 };

	Check(BoxHolds(box, 10, 20) && BoxHolds(box, 39, 59), "a box holds its first point and its last, x + width - 1 and y + height - 1");
	Check(!BoxHolds(box, 40, 20) && !BoxHolds(box, 10, 60) && !BoxHolds(box, 9, 20) && !BoxHolds(box, 10, 19),
		"and none just past any of its four edges");

	Box const empty = { 10, 20, 0, 40 };
	Box const flat = { 10, 20, 30, 0 };
	Check(!BoxHolds(empty, 10, 20) && !BoxHolds(flat, 10, 20), "a box of no width, or of no height, holds no point at all");

	Box const atTheEnd = { INT32_MAX - 1, INT32_MAX - 1, INT32_MAX, INT32_MAX };
	Check(BoxHolds(atTheEnd, INT32_MAX, INT32_MAX) && !BoxHolds(atTheEnd, INT32_MIN, INT32_MIN),
		"a box at the far end of the numbers holds its points, and its sums don't wrap round to take the lowest");

	Box const fromBelow = { INT32_MIN, INT32_MIN, 2, 2 };
	Check(BoxHolds(fromBelow, INT32_MIN + 1, INT32_MIN + 1) && !BoxHolds(fromBelow, INT32_MIN + 2, INT32_MIN), "and so does one at the lowest");
}

static void TestPanels()
{
	puts("Which panel a point is in:");

	PanelLayout const layout = LayOutPanels(1024, 768, kPanelSizesAsBuilt);

	Check(BoxIs(layout.toolbar, 0, 0, 1024, 32) && BoxIs(layout.status, 0, 748, 1024, 20) && BoxIs(layout.list, 0, 32, 260, 716)
		&& BoxIs(layout.fields, 684, 32, 340, 716) && BoxIs(layout.view, 260, 32, 424, 716), "(a 1024 by 768 window, laid out as built)");

	Check(HostFrame_PanelAt(layout, 0, 0) == HostPanel::Toolbar && HostFrame_PanelAt(layout, 1023, 31) == HostPanel::Toolbar, "the toolbar's corners are the toolbar");
	Check(HostFrame_PanelAt(layout, 0, 32) == HostPanel::List && HostFrame_PanelAt(layout, 259, 747) == HostPanel::List, "the list's corners are the list");
	Check(HostFrame_PanelAt(layout, 260, 32) == HostPanel::View && HostFrame_PanelAt(layout, 683, 747) == HostPanel::View, "the view's corners are the view");
	Check(HostFrame_PanelAt(layout, 684, 32) == HostPanel::Fields && HostFrame_PanelAt(layout, 1023, 747) == HostPanel::Fields, "the field panel's corners are the field panel");
	Check(HostFrame_PanelAt(layout, 0, 748) == HostPanel::Status && HostFrame_PanelAt(layout, 1023, 767) == HostPanel::Status, "the status line's corners are the status line");
	Check(HostFrame_PanelAt(layout, 1024, 0) == HostPanel::None && HostFrame_PanelAt(layout, 0, 768) == HostPanel::None && HostFrame_PanelAt(layout, -1, 0) == HostPanel::None
		&& HostFrame_PanelAt(layout, INT32_MIN, INT32_MAX) == HostPanel::None, "a point past the window's edge, or below 0, is in none");

	// Every point of several windows is in at most one panel, and in the one whose box holds it
	int32_t const sizes[][2] = { { 1024, 768 }, { 300, 200 }, { 600, 90 }, { 700, 50 }, { 0, 0 }, { 1, 1 }, { 160, 400 } };
	bool allSound = true;

	for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
	{
		PanelLayout const each = LayOutPanels(sizes[s][0], sizes[s][1], kPanelSizesAsBuilt);

		for (int32_t y = -1; y <= sizes[s][1]; y++)
		{
			for (int32_t x = -1; x <= sizes[s][0]; x++)
			{
				int const holding = BoxHolds(each.toolbar, x, y) + BoxHolds(each.list, x, y) + BoxHolds(each.view, x, y)
					+ BoxHolds(each.fields, x, y) + BoxHolds(each.status, x, y);
				HostPanel const panel = HostFrame_PanelAt(each, x, y);
				bool const outside = x < 0 || y < 0 || x >= sizes[s][0] || y >= sizes[s][1];

				allSound = allSound && holding <= 1 && (holding == 0) == (panel == HostPanel::None) && (!outside || panel == HostPanel::None);
			}
		}
	}

	Check(allSound, "in windows from none at all to 1024 by 768, every point is in one panel at most, the one that holds it, and none past the edge");
}

static void TestTheViewsShare()
{
	puts("The panels' sizes: the view is the main part of the window:");

	PanelLayout const at1024 = LayOutPanels(1024, 768, HostFrame_PanelSizes(1024));
	Check(BoxIs(at1024.list, 0, 32, 153, 716) && BoxIs(at1024.view, 153, 32, 871, 716) && at1024.fields.width == 0 && at1024.roomy,
		"at 1024 by 768 the list is 15% of the width, 153, the field panel takes nothing, and the view the rest: 871 by 716");
	Check(HostFrame_PanelSizes(2560).listWidth == 200 && HostFrame_PanelSizes(1333).listWidth == 199 && HostFrame_PanelSizes(1334).listWidth == 200,
		"the list is 200 at the most");
	Check(HostFrame_PanelSizes(0).listWidth == 0 && HostFrame_PanelSizes(-5).listWidth == 0 && HostFrame_PanelSizes(-1000).listWidth == 0 && HostFrame_PanelSizes(INT32_MAX).listWidth == 200,
		"a width of none, below 0 or the largest gives a list from 0 to 200, never past it");

	// Every window from 640 by 480 to 3840 by 2160, in steps: the view's box is at least three quarters of the window's area
	bool allThreeQuarters = true;
	double least = 1.0;

	for (int32_t width = 640; width <= 3840; width += 64)
	{
		for (int32_t height = 480; height <= 2160; height += 48)
		{
			PanelLayout const each = LayOutPanels(width, height, HostFrame_PanelSizes(width));
			double const share = (double)each.view.width * each.view.height / ((double)width * height);

			allThreeQuarters = allThreeQuarters && share >= 0.75;
			least = share < least ? share : least;
		}
	}

	Check(allThreeQuarters, "at every window from 640 by 480 to 3840 by 2160 the view is at least 75% of it (the least: " + std::to_string(least * 100.0) + "%)");
}

static void TestTheColumnShares()
{
	puts("The list's column, shared with the field panel:");

	// The list's column in a 1024 by 768 window with a 32 pixel toolbar and a 20 pixel status line: 716 high
	Box const column = { 0, 32, 153, 716 };
	Box list = {};
	Box fields = {};

	HostFrame_ShareTheColumn(column, 0, &list, &fields);
	Check(BoxIs(list, 0, 32, 153, 716) && BoxIs(fields, 0, 748, 153, 0), "a field panel that wants nothing leaves the list the whole column, and has an empty box at its foot");
	HostFrame_ShareTheColumn(column, -5, &list, &fields);
	Check(BoxIs(list, 0, 32, 153, 716) && BoxIs(fields, 0, 748, 153, 0), "and so does one that wants less than nothing");

	HostFrame_ShareTheColumn(column, 79, &list, &fields);
	Check(BoxIs(list, 0, 32, 153, 637) && BoxIs(fields, 0, 669, 153, 79), "one that wants 79 pixels gets the foot of the column, 79 high, and the list keeps the rest above it");
	HostFrame_ShareTheColumn(column, 358, &list, &fields);
	Check(BoxIs(list, 0, 32, 153, 358) && BoxIs(fields, 0, 390, 153, 358), "half the column is the most it takes");
	HostFrame_ShareTheColumn(column, 359, &list, &fields);
	Check(BoxIs(list, 0, 32, 153, 358) && BoxIs(fields, 0, 390, 153, 358), "asking for more doesn't get more");
	HostFrame_ShareTheColumn(column, INT32_MAX, &list, &fields);
	Check(BoxIs(list, 0, 32, 153, 358) && BoxIs(fields, 0, 390, 153, 358), "not even the most an int holds");

	Box const odd = { 10, 20, 50, 101 };

	HostFrame_ShareTheColumn(odd, INT32_MAX, &list, &fields);
	Check(BoxIs(list, 10, 20, 50, 51) && BoxIs(fields, 10, 71, 50, 50), "a column of an odd number of pixels gives the list the odd one");

	// A column with no room
	Box const flat = { 5, 6, 100, 0 };

	HostFrame_ShareTheColumn(flat, 100, &list, &fields);
	Check(BoxIs(list, 5, 6, 100, 0) && BoxIs(fields, 5, 6, 100, 0), "a column of no height gives both no height");
	Box const thin = { 5, 6, 0, 100 };

	HostFrame_ShareTheColumn(thin, 20, &list, &fields);
	Check(list.width == 0 && fields.width == 0 && list.height == 80 && fields.height == 20, "a column of no width gives both no width, and still adds up to the column");
	Box const below = { 0, 0, -50, -50 };

	HostFrame_ShareTheColumn(below, 20, &list, &fields);
	Check(BoxIs(list, 0, 0, 0, 0) && BoxIs(fields, 0, 0, 0, 0), "a column with sides below 0 counts them as 0");

	// The ends of the numbers: nothing wraps round
	Box const farEnd = { INT32_MAX - 10, INT32_MAX - 10, 100, 100 };

	HostFrame_ShareTheColumn(farEnd, INT32_MAX, &list, &fields);
	Check(BoxIs(list, INT32_MAX - 10, INT32_MAX - 10, 100, 5) && BoxIs(fields, INT32_MAX - 10, INT32_MAX - 5, 100, 5), "a column at the far end of the numbers is cut there");
	Box const atTheEnd = { 0, INT32_MAX, 10, 10 };

	HostFrame_ShareTheColumn(atTheEnd, 5, &list, &fields);
	Check(BoxIs(list, 0, INT32_MAX, 10, 0) && BoxIs(fields, 0, INT32_MAX, 10, 0), "and one that starts at the very end has no room");
	Box const fromBelow = { 0, INT32_MIN, 10, 100 };

	HostFrame_ShareTheColumn(fromBelow, 30, &list, &fields);
	Check(BoxIs(list, 0, INT32_MIN, 10, 70) && BoxIs(fields, 0, INT32_MIN + 70, 10, 30), "one that starts at the lowest place works as any other");

	// A sweep: every column height from 0 to 300 with each want. The two parts are the column's width; the list starts where the column
	// does, the field panel starts where the list ends, and together they are the column; the field panel takes no more than half the
	// column or than it wanted, and takes one of the two.
	bool allSound = true;
	int32_t const wants[] = { -1, 0, 1, 15, 79, 150, 151, 300, 301, INT32_MAX };

	for (int32_t height = 0; height <= 300; height++)
	{
		Box const sweep = { 7, 11, 40, height };

		for (int32_t want : wants)
		{
			int32_t const wantedHeight = want > 0 ? want : 0;

			HostFrame_ShareTheColumn(sweep, want, &list, &fields);

			bool const sound = list.x == 7 && fields.x == 7 && list.width == 40 && fields.width == 40 && list.y == 11
				&& fields.y == list.y + list.height && list.height + fields.height == height && fields.height <= height / 2
				&& fields.height <= wantedHeight && (fields.height == wantedHeight || fields.height == height / 2);

			allSound = allSound && sound;
		}
	}

	Check(allSound, "at every column height from 0 to 300 and every want, the two parts add up to the column, the list above and the field panel below, and it takes what it wants up to half");

	Box const untouched = { 1, 2, 3, 4 };

	list = untouched;
	fields = untouched;
	HostFrame_ShareTheColumn(column, 79, nullptr, &fields);
	HostFrame_ShareTheColumn(column, 79, &list, nullptr);
	Check(BoxIs(list, 1, 2, 3, 4) && BoxIs(fields, 1, 2, 3, 4), "with no place to put an answer, nothing happens and nothing crashes");
}

static void TestWindowToPanels()
{
	puts("A click moved into the panels' picture:");

	int32_t px = 0;
	int32_t py = 0;
	bool same = true;

	for (int32_t x = 0; x < 1024; x++)
	{
		HostFrame_WindowToPanels(x, x % 768, 1024, 768, 1024, 768, &px, &py);
		same = same && px == x && py == x % 768;
	}

	Check(same, "a picture the window's own size leaves every place where it is");
	HostFrame_WindowToPanels(0, 0, 1536, 1152, 1024, 768, &px, &py);
	bool corners = px == 0 && py == 0;
	HostFrame_WindowToPanels(1535, 1151, 1536, 1152, 1024, 768, &px, &py);
	corners = corners && px == 1023 && py == 767;
	HostFrame_WindowToPanels(767, 575, 1536, 1152, 1024, 768, &px, &py);
	Check(corners && px == 511 && py == 383, "with a 1.5 stretch the corners stay the picture's corners, and the middle its middle");
	HostFrame_WindowToPanels(1023, 767, 1024, 768, 800, 600, &px, &py);
	bool small = px == 799 && py == 599;
	HostFrame_WindowToPanels(512, 384, 1024, 768, 800, 600, &px, &py);
	Check(small && px == 400 && py == 300, "an 800 by 600 picture in a 1024 by 768 window: its last pixel and its middle");
	HostFrame_WindowToPanels(999, 499, 1000, 500, 500, 500, &px, &py);
	Check(px == 499 && py == 499, "a window twice as wide as its picture but as high: across is halved, down isn't");
	HostFrame_WindowToPanels(-1, -7, 1536, 1152, 1024, 768, &px, &py);
	bool outside = px == -1 && py == -1;
	HostFrame_WindowToPanels(1536, 1152, 1536, 1152, 1024, 768, &px, &py);
	Check(outside && px >= 1024 && py >= 768, "a place left of or above the window stays outside the picture, and one past its edge stays past");
	HostFrame_WindowToPanels(10, 20, 0, 768, 1024, 0, &px, &py);
	bool none = px == 10 && py == 20;
	HostFrame_WindowToPanels(INT32_MAX, INT32_MAX, 1, 1, 16384, 16384, &px, &py);
	Check(none && px == INT32_MAX && py == INT32_MAX, "with no size on a side the place is as it was, and the largest place stays an int");
}

int main()
{
	TestBoxes();
	TestPanels();
	TestTheViewsShare();
	TestTheColumnShares();
	TestWindowToPanels();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
