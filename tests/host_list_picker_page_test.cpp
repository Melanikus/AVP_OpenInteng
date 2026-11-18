// host_list_picker_page_test: the list chooser's PAGE is inteng's 640 x 480 page on any window (Bishop's ruling 11-18): inteng sizes it to its screen (FUN_00475bf0: rows
// (height - 0x78) / 0x14, columns width / (column + 0x14)), and its screen was 640 x 480; a bigger window keeps that page, so a page always fits in one frame. Written 2026-11-18 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>

#include <string>
#include <vector>

#include "list_picker.h"

using namespace host;

static int checks = 0;
static int failures = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;
	failures += good ? 0 : 1;
}

// a letter 8 pixels wide, as inteng's font16 is near enough for this
static int32_t Width(std::string const &text)
{
	return (int32_t)text.size() * 8;
}

static std::vector<std::string> Names(int count)
{
	std::vector<std::string> names;

	for (int i = 0; i < count; i++)
	{
		names.push_back("Control01" + std::to_string(i));   // 10 to 12 letters: a column 96 + 0x14 = 116 wide
	}

	return names;
}

int main()
{
	std::vector<std::string> const many = Names(301);   // Invasion's Select Object list holds 301 objects
	int32_t const column = 12 * 8 + 0x14;               // the widest name + 0x14
	int32_t const inteng640Columns = 640 / (column + 0x14);
	int32_t const inteng480Rows = (480 - 0x78) / 0x14;

	puts("1. on inteng's own 640 x 480 screen: inteng's numbers");
	{
		ListPicker picker;
		picker.Open("Select Object", many, 640, 480, Width, 0, 0);
		Check(picker.Rows() == inteng480Rows, "18 rows, (480 - 0x78) / 0x14 (FUN_00475bf0 0x475d2f)");
		Check(picker.Columns() == inteng640Columns, "640 / (column + 0x14) columns (0x475d60): " + std::to_string(inteng640Columns));
	}

	struct { int32_t width, height; char const *what; } const windows[] = {
		{ 1024, 768, "a hidden run's 1024 x 768" },
		{ 2560, 1440, "the owner's 2560 x 1440" },
		{ 1280, 720, "the owner's at text scale 2 (1280 x 720 of the editor's own pixels)" },
		{ 3840, 2160, "a 3840 x 2160 window" },
	};

	puts("2. a bigger window keeps inteng's 640 x 480 page");

	for (auto const &one : windows)
	{
		ListPicker picker;
		picker.Open("Select Object", many, one.width, one.height, Width, 0, 0);
		Check(picker.Rows() == inteng480Rows && picker.Columns() == inteng640Columns,
			std::string(one.what) + ": " + std::to_string(picker.Rows()) + " rows x " + std::to_string(picker.Columns()) + " columns, inteng's page");
		Check(picker.Pages() == (301 + inteng480Rows * inteng640Columns - 1) / (inteng480Rows * inteng640Columns), std::string(one.what) + ": Prev / Next page through the rest");

		ListPickerBox const window = picker.Window();
		Check(window.x == (one.width - window.width) / 2 && window.y == (one.height - window.height) / 2, std::string(one.what) + ": the window is in the middle of the real screen");
		Check(window.width <= 640 - 0x14, std::string(one.what) + ": the window is no wider than inteng's screen allows (640 - 0x14)");
	}

	puts("3. a smaller window works out its page from its own size, as inteng does");
	{
		ListPicker picker;
		picker.Open("Select Object", many, 512, 384, Width, 0, 0);
		Check(picker.Rows() == (384 - 0x78) / 0x14 && picker.Columns() == 512 / (column + 0x14), "512 x 384: " + std::to_string(picker.Rows()) + " rows x " + std::to_string(picker.Columns()) + " columns");
	}

	puts("4. a short list still takes only what it needs");
	{
		ListPicker picker;
		picker.Open("Select Hierarchy", Names(8), 2560, 1440, Width, 0, 0);
		Check(picker.Rows() == 8 && picker.Columns() == 1 && picker.Pages() == 1, "8 choices: 8 rows, 1 column, 1 page");
	}

	printf("%d checks, %d failed\n", checks, failures);
	printf(failures ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failures ? 1 : 0;
}
