// The test for screen_picture.h's measuring (screen_measure.cpp): the self-check that a screen picture really shows the editor's panels,
// and the view's brightness that says whether it's black. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: first-screen.png is the proof Apone checks that the level reached the screen. A self-check that passed on a blank or wrong picture
// would make it a proof that can pass on a broken picture. So every way the check could be fooled is tried here on pictures the test
// makes: all black, the panels' colour off in one channel, just inside and just outside the slack, a patch of no size or off the picture,
// and a picture whose rows are longer than its width. It takes no picture of any window.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "screen_picture.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static uint32_t const kGrey = 0xFF202428u;   // the panels' own colour, as panel_pass.h's kPanelPassBackground

// A picture of width by height, rowPixels to a row, all one colour
struct Picture
{
	int32_t width;
	int32_t height;
	int32_t rowPixels;
	std::vector<uint32_t> pixels;

	Picture(int32_t w, int32_t h, int32_t row, uint32_t colour) : width(w), height(h), rowPixels(row), pixels((size_t)row * (size_t)h, colour) {}

	void Fill(const ui::Box &box, uint32_t colour)
	{
		for (int32_t y = box.y; y < box.y + box.height; y++)
		{
			for (int32_t x = box.x; x < box.x + box.width; x++)
			{
				pixels[(size_t)y * (size_t)rowPixels + (size_t)x] = colour;
			}
		}
	}

	ScreenPictureReport Measure(const ui::Box &view, const ui::Box &patch) const
	{
		ScreenPictureReport report = { true, true, -1.0, -1.0 };

		ScreenPicture_Measure(pixels.data(), width, height, rowPixels, view, patch, kGrey, &report);
		return report;
	}
};

static ui::Box const kPatch = { 84, 4, 8, 8 };
static ui::Box const kView = { 20, 32, 60, 40 };

static void TestTheSelfCheck()
{
	puts("The self-check: the toolbar's patch is the panels' own colour:");

	Picture black(100, 80, 100, 0xFF000000u);
	Check(!black.Measure(kView, kPatch).panelsSeen, "an all-black picture, as a capture that came back empty, is never taken for the editor's window");

	Picture shown(100, 80, 100, kGrey);
	shown.Fill(kView, 0xFF000000u);
	Check(shown.Measure(kView, kPatch).panelsSeen, "the panels' grey in the patch is seen");

	Picture blueOff(100, 80, 100, kGrey);
	blueOff.Fill(ui::Box{ 90, 10, 1, 1 }, 0xFF202430u);
	Check(!blueOff.Measure(kView, kPatch).panelsSeen, "one pixel of the patch off in its blue alone isn't the panels' colour");

	Picture slack(100, 80, 100, kGrey);
	slack.Fill(kPatch, 0xFF23272Bu);
	Check(slack.Measure(kView, kPatch).panelsSeen, "3 off in every channel is still the panels' colour: the screen's own rounding");
	slack.Fill(ui::Box{ 84, 4, 1, 1 }, 0xFF24282Cu);
	Check(!slack.Measure(kView, kPatch).panelsSeen, "4 off in one pixel isn't");

	Check(!shown.Measure(kView, ui::Box{ 84, 4, 0, 8 }).panelsSeen && !shown.Measure(kView, ui::Box{ 84, 4, 8, -3 }).panelsSeen,
		"a patch of no width or no height is never seen");
	Check(!shown.Measure(kView, ui::Box{ 200, 4, 8, 8 }).panelsSeen && !shown.Measure(kView, ui::Box{ -50, -50, 8, 8 }).panelsSeen,
		"a patch wholly off the picture is never seen");
	Check(shown.Measure(kView, ui::Box{ 96, -4, 8, 12 }).panelsSeen, "a patch partly off the picture is checked where it's on it");

	ScreenPictureReport report = { true, true, -1.0, -1.0 };
	ScreenPicture_Measure(nullptr, 100, 80, 100, kView, kPatch, kGrey, &report);
	bool const noPixels = !report.panelsSeen && report.viewMean == 0.0 && report.viewLitShare == 0.0;
	ScreenPicture_Measure(shown.pixels.data(), 100, 80, 50, kView, kPatch, kGrey, &report);
	Check(noPixels && !report.panelsSeen, "no pixels, or rows shorter than the width, measure nothing and see nothing");
}

static void TestTheViewsBrightness()
{
	puts("The view's brightness:");

	Picture shown(100, 80, 100, kGrey);
	shown.Fill(kView, 0xFF000000u);
	ScreenPictureReport report = shown.Measure(kView, kPatch);
	Check(report.viewMean == 0.0 && report.viewLitShare == 0.0, "a black view measures a mean of 0, and none of its pixels lit");

	shown.Fill(kView, 0xFF404040u);
	report = shown.Measure(kView, kPatch);
	Check(report.viewMean == 64.0 && report.viewLitShare == 1.0, "a view all 64 grey measures a mean of 64, all of it lit");

	shown.Fill(kView, 0xFF000000u);
	shown.Fill(ui::Box{ 20, 32, 30, 40 }, 0xFF603000u);
	report = shown.Measure(kView, kPatch);
	Check(report.viewMean == 24.0 && report.viewLitShare == 0.5, "half lit at 96, 48, 0 (its red, green and blue averaged: 48) and half black: a mean of 24, half above 16");

	shown.Fill(kView, 0xFF101010u);
	report = shown.Measure(kView, kPatch);
	Check(report.viewMean == 16.0 && report.viewLitShare == 0.0, "a brightness of exactly 16 isn't counted as lit: only above it");

	shown.Fill(kView, 0xFF000000u);
	report = shown.Measure(ui::Box{ 90, 70, 30, 30 }, kPatch);
	Check(report.viewMean == (double)0x20 + 4.0 && report.viewLitShare == 1.0, "a view box partly off the picture is measured where it's on it");

	report = shown.Measure(ui::Box{ 20, 32, 0, 40 }, kPatch);
	Check(report.viewMean == 0.0 && report.viewLitShare == 0.0 && report.panelsSeen, "a view box of no size measures 0, and the patch still counts");
}

static void TestLongRows()
{
	puts("A picture whose rows are longer than its width:");

	Picture wide(100, 80, 128, 0xFFFFFFFFu);
	wide.Fill(ui::Box{ 0, 0, 100, 80 }, kGrey);
	wide.Fill(kView, 0xFF000000u);
	ScreenPictureReport const report = wide.Measure(kView, kPatch);
	Check(report.panelsSeen && report.viewMean == 0.0, "the white past each row's width is never read: the patch is grey and the view black");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheSelfCheck();
	TestTheViewsBrightness();
	TestLongRows();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
