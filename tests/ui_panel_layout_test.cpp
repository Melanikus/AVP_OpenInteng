// The test for src/ui's panel layout. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>

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

static bool Same(const Box &box, int32_t x, int32_t y, int32_t width, int32_t height)
{
	return box.x == x && box.y == y && box.width == width && box.height == height;
}

// The test's own copy of the rule for a size from outside: below 0 is 0, above kPanelMostSide is kPanelMostSide
static int64_t Side(int64_t size)
{
	if (size < 0)
	{
		return 0;
	}

	return size > kPanelMostSide ? kPanelMostSide : size;
}

static bool OnTheSamePixel(const Box &a, const Box &b)
{
	bool const empty = a.width == 0 || a.height == 0 || b.width == 0 || b.height == 0;

	return !empty && (int64_t)a.x < (int64_t)b.x + b.width && (int64_t)b.x < (int64_t)a.x + a.width && (int64_t)a.y < (int64_t)b.y + b.height &&
		(int64_t)b.y < (int64_t)a.y + a.height;
}

// Everything a layout must always be, at any window size and any sizes asked for:
// - no panel is less than nothing wide or high, and none goes past an edge of the window;
// - no two panels cover the same pixel, and between them they cover every pixel laid out, so nothing is left unpainted;
// - no panel is bigger than it asked to be;
// - the panels gave way in the order panel_layout.h gives, and the window is "roomy" exactly when nothing had to give way.
static bool Sound(const PanelLayout &layout, int32_t windowWidth, int32_t windowHeight, const PanelSizes &sizes)
{
	Box const boxes[5] = { layout.toolbar, layout.list, layout.view, layout.fields, layout.status };
	int64_t const width = Side(windowWidth);
	int64_t const height = Side(windowHeight);
	int64_t area = 0;

	for (int i = 0; i < 5; i++)
	{
		Box const &box = boxes[i];

		if (box.x < 0 || box.y < 0 || box.width < 0 || box.height < 0 || (int64_t)box.x + box.width > width || (int64_t)box.y + box.height > height)
		{
			return false;
		}

		for (int j = i + 1; j < 5; j++)
		{
			if (OnTheSamePixel(box, boxes[j]))
			{
				return false;
			}
		}

		area += (int64_t)box.width * box.height;
	}

	if (area != width * height)
	{
		return false;
	}

	int64_t const askedToolbar = Side(sizes.toolbarHeight);
	int64_t const askedStatus = Side(sizes.statusHeight);
	int64_t const askedList = Side(sizes.listWidth);
	int64_t const askedFields = Side(sizes.fieldsWidth);
	int64_t const leastList = Side(sizes.leastListWidth) < askedList ? Side(sizes.leastListWidth) : askedList;
	int64_t const leastFields = Side(sizes.leastFieldsWidth) < askedFields ? Side(sizes.leastFieldsWidth) : askedFields;
	int64_t const toolbar = layout.toolbar.height;
	int64_t const status = layout.status.height;
	int64_t const middle = layout.view.height;
	int64_t const list = layout.list.width;
	int64_t const fields = layout.fields.width;
	int64_t const view = layout.view.width;

	if (toolbar > askedToolbar || status > askedStatus || list > askedList || fields > askedFields)
	{
		return false;
	}

	// The heights give way from the middle row up: the middle row, then the status line, then the toolbar
	if ((status < askedStatus && middle != 0) || (toolbar < askedToolbar && (status != 0 || middle != 0)))
	{
		return false;
	}

	// The widths give way as panel_layout.h says: the view, the list to its least, the field panel to its least, the list, the field panel
	if ((list < askedList && view != 0) || (fields < askedFields && (view != 0 || list > leastList)) || (list < leastList && fields > leastFields) ||
		(fields < leastFields && list != 0))
	{
		return false;
	}

	bool const roomy = toolbar == askedToolbar && status == askedStatus && list == askedList && fields == askedFields && view > 0 && middle > 0;

	return layout.roomy == roomy;
}

static void TestAsBuilt()
{
	puts("The sizes the editor starts with:");

	PanelLayout const layout = LayOutPanels(1280, 720, kPanelSizesAsBuilt);

	Check(Same(layout.toolbar, 0, 0, 1280, 32) && Same(layout.status, 0, 700, 1280, 20), "at 1280 by 720, the toolbar is the top 32 rows and the status line the bottom 20");
	Check(Same(layout.list, 0, 32, 260, 668) && Same(layout.view, 260, 32, 680, 668) && Same(layout.fields, 940, 32, 340, 668),
		"and between them, the list on the left, the view in the middle and the field panel on the right, each as wide as it asked");
	Check(layout.roomy && Sound(layout, 1280, 720, kPanelSizesAsBuilt), "the window is roomy, and the layout sound");
}

static void TestTinyWindows()
{
	puts("Tiny windows:");

	PanelLayout layout = LayOutPanels(0, 0, kPanelSizesAsBuilt);

	Check(Same(layout.toolbar, 0, 0, 0, 0) && Same(layout.list, 0, 0, 0, 0) && Same(layout.view, 0, 0, 0, 0) && Same(layout.fields, 0, 0, 0, 0) &&
		Same(layout.status, 0, 0, 0, 0), "a 0 by 0 window (a minimised one) gives every panel nothing");
	Check(!layout.roomy && Sound(layout, 0, 0, kPanelSizesAsBuilt), "it isn't roomy, and the layout is sound");

	layout = LayOutPanels(1, 1, kPanelSizesAsBuilt);
	Check(Same(layout.toolbar, 0, 0, 1, 1) && layout.status.height == 0 && layout.view.height == 0, "in a 1 by 1 window the toolbar has the one row");
	Check(layout.list.width == 0 && layout.view.width == 0 && layout.fields.width == 1, "and of the middle row's widths, the field panel keeps the one column");
	Check(!layout.roomy && Sound(layout, 1, 1, kPanelSizesAsBuilt), "it isn't roomy, and the layout is sound");

	layout = LayOutPanels(-5, -2147483647 - 1, kPanelSizesAsBuilt);
	Check(Same(layout.toolbar, 0, 0, 0, 0) && Same(layout.fields, 0, 0, 0, 0) && Same(layout.status, 0, 0, 0, 0) && Sound(layout, -5, -2147483647 - 1, kPanelSizesAsBuilt),
		"a window with sides below 0, down to the least a number can be, is laid out as 0 by 0");
}

static void TestHugeWindows()
{
	puts("Huge and very wide windows:");

	PanelLayout layout = LayOutPanels(2147483647, 2147483647, kPanelSizesAsBuilt);

	Check(Same(layout.toolbar, 0, 0, kPanelMostSide, 32) && Same(layout.status, 0, kPanelMostSide - 20, kPanelMostSide, 20),
		"the largest window a number can give is laid out as the largest side, from its top left");
	Check(Same(layout.fields, kPanelMostSide - 340, 32, 340, kPanelMostSide - 52) && layout.roomy && Sound(layout, 2147483647, 2147483647, kPanelSizesAsBuilt),
		"its field panel sits at that side's right edge, and the layout is sound");

	layout = LayOutPanels(2147483647, 720, kPanelSizesAsBuilt);
	Check(Same(layout.view, 260, 32, kPanelMostSide - 600, 668) && Sound(layout, 2147483647, 720, kPanelSizesAsBuilt),
		"a window as wide as a number can be and 720 high gives the view all the width the list and field panel don't ask for");

	layout = LayOutPanels(kPanelMostSide + 1, 720, kPanelSizesAsBuilt);
	Check(layout.toolbar.width == kPanelMostSide && Sound(layout, kPanelMostSide + 1, 720, kPanelSizesAsBuilt), "one past the largest side is laid out as the largest side");

	layout = LayOutPanels(kPanelMostSide, 1, kPanelSizesAsBuilt);
	Check(Same(layout.toolbar, 0, 0, kPanelMostSide, 1) && layout.view.height == 0 && !layout.roomy && Sound(layout, kPanelMostSide, 1, kPanelSizesAsBuilt),
		"a window the largest side wide and one row high gives the row to the toolbar");

	layout = LayOutPanels(1, kPanelMostSide, kPanelSizesAsBuilt);
	Check(Same(layout.fields, 0, 32, 1, kPanelMostSide - 52) && layout.list.width == 0 && !layout.roomy && Sound(layout, 1, kPanelMostSide, kPanelSizesAsBuilt),
		"a window one column wide and the largest side high gives the column to the field panel");
}

static void TestGivingWay()
{
	puts("Giving way, in order:");

	struct WidthCase
	{
		int32_t width;
		int32_t list;
		int32_t view;
		int32_t fields;
		const char *what;
	};

	WidthCase const widths[] = {
		{ 601, 260, 1, 340, "at 601 wide, the view has one column left" },
		{ 600, 260, 0, 340, "at 600, the view has given way altogether, and the list and field panel still have all they asked" },
		{ 599, 259, 0, 340, "at 599, the list gives way first" },
		{ 480, 140, 0, 340, "at 480, the list is down to its least, and the field panel still has all it asked" },
		{ 479, 140, 0, 339, "at 479, the field panel gives way next" },
		{ 360, 140, 0, 220, "at 360, both are down to their least" },
		{ 359, 139, 0, 220, "at 359, the list gives way below its least" },
		{ 220, 0, 0, 220, "at 220, the list has gone, and the field panel keeps its least" },
		{ 100, 0, 0, 100, "at 100, the field panel gives way last" },
	};

	for (WidthCase const &each : widths)
	{
		PanelLayout const layout = LayOutPanels(each.width, 720, kPanelSizesAsBuilt);

		Check(layout.list.width == each.list && layout.view.width == each.view && layout.fields.width == each.fields && Sound(layout, each.width, 720, kPanelSizesAsBuilt),
			each.what);
	}

	Check(!LayOutPanels(600, 720, kPanelSizesAsBuilt).roomy && LayOutPanels(601, 720, kPanelSizesAsBuilt).roomy, "a window with no view left isn't roomy, and one with a column is");

	struct HeightCase
	{
		int32_t height;
		int32_t toolbar;
		int32_t middle;
		int32_t status;
		const char *what;
	};

	HeightCase const heights[] = {
		{ 53, 32, 1, 20, "at 53 high, the middle row has one row left" },
		{ 52, 32, 0, 20, "at 52, the middle row has given way altogether" },
		{ 40, 32, 0, 8, "at 40, the status line gives way next" },
		{ 32, 32, 0, 0, "at 32, the status line has gone and the toolbar keeps its height" },
		{ 10, 10, 0, 0, "at 10, the toolbar gives way last" },
	};

	for (HeightCase const &each : heights)
	{
		PanelLayout const layout = LayOutPanels(1280, each.height, kPanelSizesAsBuilt);

		Check(layout.toolbar.height == each.toolbar && layout.view.height == each.middle && layout.list.height == each.middle && layout.fields.height == each.middle &&
			layout.status.height == each.status && layout.status.y == each.toolbar + each.middle && Sound(layout, 1280, each.height, kPanelSizesAsBuilt), each.what);
	}
}

static void TestOddSizes()
{
	puts("Odd sizes asked for:");

	PanelSizes const belowNothing = { -5, -20, -100, -2147483647 - 1, -1, -1 };
	PanelLayout layout = LayOutPanels(1280, 720, belowNothing);

	Check(Same(layout.toolbar, 0, 0, 1280, 0) && Same(layout.status, 0, 720, 1280, 0) && Same(layout.view, 0, 0, 1280, 720) && Sound(layout, 1280, 720, belowNothing),
		"sizes below 0 are taken as nothing, so the view has the whole window");

	PanelSizes const listLeastAboveAsked = { 32, 20, 260, 500, 340, 220 };

	layout = LayOutPanels(550, 720, listLeastAboveAsked);
	Check(layout.list.width == 260 && layout.fields.width == 290 && Sound(layout, 550, 720, listLeastAboveAsked),
		"a list's least above what it asks for is taken as what it asks for, so the list doesn't grow while giving way");

	PanelSizes const fieldsLeastAboveAsked = { 32, 20, 260, 140, 340, 900 };

	layout = LayOutPanels(550, 720, fieldsLeastAboveAsked);
	Check(layout.list.width == 210 && layout.fields.width == 340 && Sound(layout, 550, 720, fieldsLeastAboveAsked),
		"and so is the field panel's, so it doesn't grow either");

	PanelSizes const pastTheLargest = { 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647 };

	layout = LayOutPanels(1280, 720, pastTheLargest);
	Check(Same(layout.toolbar, 0, 0, 1280, 720) && layout.status.height == 0 && layout.list.width == 0 && layout.fields.width == 1280 && Sound(layout, 1280, 720, pastTheLargest),
		"sizes as large as a number can be can't wrap a sum round: the toolbar takes the height and the field panel the width");

	layout = LayOutPanels(2147483647, 2147483647, pastTheLargest);
	Check(layout.toolbar.height == kPanelMostSide && layout.fields.width == kPanelMostSide && Sound(layout, 2147483647, 2147483647, pastTheLargest),
		"and in the largest window too");
}

static void TestEverySize()
{
	puts("Every size keeps the layout sound:");

	bool sound = true;

	for (int32_t width = 0; width <= 1400; width++)
	{
		for (int32_t height = 0; height <= 80; height++)
		{
			sound = sound && Sound(LayOutPanels(width, height, kPanelSizesAsBuilt), width, height, kPanelSizesAsBuilt);
		}
	}

	Check(sound, "with the sizes the editor starts with, every window up to 1400 wide by 80 high is sound");

	// Window sides and sizes asked for, picked from the edges and from anywhere a 32-bit number can be
	int32_t const edges[] = { -2147483647 - 1, -1, 0, 1, 2, 31, 32, 33, 139, 140, 220, 260, 340, 600, kPanelMostSide - 1, kPanelMostSide, kPanelMostSide + 1, 2147483647 };
	unsigned int seed = 4343;

	sound = true;

	for (int i = 0; i < 200000; i++)
	{
		int32_t picked[8];

		for (int j = 0; j < 8; j++)
		{
			seed = seed * 1103515245u + 12345u;

			unsigned int const kind = (seed >> 16) % 4;
			unsigned int const random = seed * 2654435761u;

			picked[j] = kind == 0 ? edges[(seed >> 4) % (sizeof(edges) / sizeof(edges[0]))]
				: kind == 1 ? (int32_t)(random % 2000u)
				: kind == 2 ? (int32_t)(random % 40000u) - 10000
				: (int32_t)random;
		}

		PanelSizes const sizes = { picked[2], picked[3], picked[4], picked[5], picked[6], picked[7] };

		sound = sound && Sound(LayOutPanels(picked[0], picked[1], sizes), picked[0], picked[1], sizes);
	}

	Check(sound, "with 200,000 picked window sides and sizes asked for, from below 0 to the largest a number can be, every layout was sound");
}

int main()
{
	TestAsBuilt();
	TestTinyWindows();
	TestHugeWindows();
	TestGivingWay();
	TestOddSizes();
	TestEverySize();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
