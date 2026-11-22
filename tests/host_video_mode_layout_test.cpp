// The test for where the Video screen puts things (src/host/video_mode_layout.h): the controls in rows inside the view, none over another, the frame round them at
// the view's foot; the source's picture with its shape kept and THE RECTANGLE (the part that becomes the game's 128 x 96) over it, Fit's reaching past it, both
// whole inside the room; the lines of words at the top; and hostile numbers (no size, a rectangle that is not a number, a view too small, a label wider than the
// view) giving no box outside the view. Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <math.h>
#include <stdio.h>

#include <limits>
#include <string>
#include <vector>

#include "video_mode_layout.h"

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

static bool Inside(VideoLayoutBox const &box, VideoLayoutBox const &room)
{
	return box.width > 0 && box.height > 0 && box.x >= room.x && box.y >= room.y && box.x + box.width <= room.x + room.width && box.y + box.height <= room.y + room.height;
}

static bool Overlap(VideoLayoutBox const &a, VideoLayoutBox const &b)
{
	return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

static std::string Words(VideoLayoutBox const &box)
{
	return std::to_string(box.x) + " " + std::to_string(box.y) + " " + std::to_string(box.width) + " " + std::to_string(box.height);
}

// Every control placed inside the view, inside the frame, and none over another
static bool ControlsGood(VideoModeLayout const &plan, VideoLayoutBox const &view)
{
	for (size_t i = 0; i < plan.controls.size(); i++)
	{
		if (!Inside(plan.controls[i], view) || !Inside(plan.controls[i], plan.frame))
		{
			return false;
		}

		for (size_t j = i + 1; j < plan.controls.size(); j++)
		{
			if (Overlap(plan.controls[i], plan.controls[j]))
			{
				return false;
			}
		}
	}

	return Inside(plan.frame, view);
}

int main()
{
	VideoLayoutBox const view = { 210, 40, 1024, 700 };
	std::vector<int32_t> const eleven(11, 40);
	int32_t const font = 15;

	// THE OWNER'S TEST CLIP (acceptance-video-mode: 320 x 180, not 4:3), on Fit: all of it into 128 x 72 of 128 x 96, so the rectangle reaches 30 source pixels past
	// it above and below (video_import.h's numbers: left 0, top -30, 320 x 240)
	double const fit[4] = { 0.0, -30.0, 320.0, 240.0 };
	VideoModeLayout const plan = VideoModeLayout_Plan(view, eleven, font, 4, 320, 180, fit);

	Check(plan.controls.size() == 11 && ControlsGood(plan, view), "the 11 controls are each inside the view and the frame, none over another, the frame inside the view (" + Words(plan.frame) + ")");
	Check(plan.frame.y + plan.frame.height == view.y + view.height - kVideoLayoutMargin, "the frame sits at the view's foot, the margin under it");
	Check(plan.controls[0].y == plan.controls[10].y, "11 controls of 40 pixels make one row in a view 1024 wide");
	Check(plan.lineTops.size() == 4 && plan.lineTops[0] == view.y + kVideoLayoutMargin && plan.lineTops[3] == view.y + kVideoLayoutMargin + 3 * (font + kVideoLayoutLineGap),
		"the 4 lines of words at the top, one under the other");
	Check(Inside(plan.picture, view) && Inside(plan.rectangle, view), "the picture (" + Words(plan.picture) + ") and its rectangle (" + Words(plan.rectangle) + ") are inside the view");
	Check(fabs((double)plan.picture.width / plan.picture.height - 320.0 / 180.0) < 0.02, "the picture keeps its shape, 320 x 180");
	Check(fabs((double)plan.rectangle.width / plan.rectangle.height - 4.0 / 3.0) < 0.02, "the rectangle is 4:3, as the game's 128 x 96");
	Check(plan.rectangle.x == plan.picture.x && plan.rectangle.width == plan.picture.width && plan.rectangle.y < plan.picture.y &&
		plan.rectangle.y + plan.rectangle.height > plan.picture.y + plan.picture.height, "on Fit it is as wide as the picture and reaches past it above and below, where the black bars go");
	Check(plan.picture.y > plan.lineTops[3] + font && plan.rectangle.y >= plan.lineTops[3] + font && plan.rectangle.y + plan.rectangle.height <= plan.frame.y,
		"both stay between the words and the controls' frame");

	// Crop: the middle 240 x 180, inside the picture
	double const crop[4] = { 40.0, 0.0, 240.0, 180.0 };
	VideoModeLayout const cropped = VideoModeLayout_Plan(view, eleven, font, 4, 320, 180, crop);

	Check(Inside(cropped.rectangle, cropped.picture), "on Crop the rectangle (" + Words(cropped.rectangle) + ") is inside the picture (" + Words(cropped.picture) + ")");

	// A view only 300 wide: the controls take several rows, each inside it
	VideoLayoutBox const narrow = { 0, 0, 300, 500 };
	VideoModeLayout const rows = VideoModeLayout_Plan(narrow, eleven, font, 4, 320, 180, fit);

	Check(ControlsGood(rows, narrow) && rows.controls[0].y < rows.controls[10].y, "a view 300 wide: the controls go in rows, each inside it (the frame " + Words(rows.frame) + ")");

	// Loading: Cancel alone, no picture yet, no words
	VideoModeLayout const loading = VideoModeLayout_Plan(view, std::vector<int32_t>(1, 48), font, 0, 0, 0, nullptr);

	Check(loading.controls.size() == 1 && ControlsGood(loading, view) && loading.picture.width == 0 && loading.rectangle.width == 0 && loading.lineTops.empty(),
		"one control alone (Cancel while loading), no picture, no rectangle, no words");

	// HOSTILE NUMBERS
	double const nan = std::numeric_limits<double>::quiet_NaN();
	double const notNumbers[4] = { nan, 0.0, 320.0, 240.0 };
	double const huge[4] = { 0.0, 0.0, 1.0e12, 1.0e12 };
	double const noSize[4] = { 0.0, 0.0, 0.0, 240.0 };
	VideoModeLayout const nanPlan = VideoModeLayout_Plan(view, eleven, font, 4, 320, 180, notNumbers);
	VideoModeLayout const hugePlan = VideoModeLayout_Plan(view, eleven, font, 4, 320, 180, huge);
	VideoModeLayout const noSizePlan = VideoModeLayout_Plan(view, eleven, font, 4, 320, 180, noSize);

	Check(Inside(nanPlan.picture, view) && nanPlan.rectangle.width == 0, "a rectangle that is not a number: the picture alone, no rectangle");
	Check(Inside(hugePlan.picture, view) && hugePlan.rectangle.width == 0, "a rectangle of 10^12 pixels: the picture alone, no rectangle");
	Check(Inside(noSizePlan.picture, view) && noSizePlan.rectangle.width == 0, "a rectangle of no width: the picture alone, no rectangle");
	Check(VideoModeLayout_Plan(view, eleven, font, 4, 0, 180, fit).picture.width == 0 && VideoModeLayout_Plan(view, eleven, font, 4, 320, 0, fit).picture.width == 0,
		"a picture of no width or no height: none drawn");
	Check(VideoModeLayout_Plan(view, eleven, font, 4, 4000000000u, 180, fit).picture.width == 0, "a picture 4,000,000,000 pixels wide: none drawn");

	VideoModeLayout const tiny = VideoModeLayout_Plan(VideoLayoutBox{ 0, 0, 20, 20 }, eleven, font, 4, 320, 180, fit);
	bool noneTiny = tiny.picture.width == 0 && tiny.rectangle.width == 0 && tiny.frame.width == 0 && tiny.lineTops.empty();

	for (VideoLayoutBox const &control : tiny.controls)
	{
		noneTiny = noneTiny && control.width == 0;
	}

	Check(noneTiny && tiny.controls.size() == 11, "a view 20 x 20, too small: nothing is placed (the 11 controls' boxes empty)");
	Check(VideoModeLayout_Plan(view, eleven, 0, 4, 320, 180, fit).frame.width == 0 && VideoModeLayout_Plan(view, eleven, -5, 4, 320, 180, fit).picture.width == 0,
		"a font of no height: nothing is placed");

	std::vector<int32_t> odd = { 40, 0, -7, 2000000000, 40 };
	VideoModeLayout const oddPlan = VideoModeLayout_Plan(view, odd, font, 0, 320, 180, fit);

	Check(oddPlan.controls[1].width == 0 && oddPlan.controls[2].width == 0, "a label of no width or a width below 0: that control left out");
	Check(ControlsGood(VideoModeLayout{ { oddPlan.controls[0], oddPlan.controls[3], oddPlan.controls[4] }, oddPlan.frame, {}, {}, {} }, view) &&
		oddPlan.controls[3].width <= view.width - 2 * kVideoLayoutMargin - 2 * kVideoLayoutFramePad,
		"a label 2,000,000,000 pixels wide: its button is cut to a row's room, on a row of its own, inside the view");

	// What a press finds
	Check(VideoModeLayout_At(plan.controls, plan.controls[4].x + 1, plan.controls[4].y + 1) == 4, "a press inside the fifth control finds it");
	Check(VideoModeLayout_At(plan.controls, plan.controls[4].x + plan.controls[4].width, plan.controls[4].y) != 4, "a press on its right edge (one past it) does not");
	Check(VideoModeLayout_At(plan.controls, view.x + 5, view.y + 5) == -1, "a press away from them finds none");
	Check(VideoModeLayout_At(oddPlan.controls, oddPlan.controls[1].x, oddPlan.controls[1].y) != 1, "a control left out (no box) is never found");

	printf("%s: %d failed\n", checksFailed ? "FAILED" : "ok", checksFailed);
	return checksFailed ? 1 : 0;
}
