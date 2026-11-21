// host_autoplot_preview_test.cpp: THE PLOT'S PREVIEW (src/host/autoplot_preview.h; the owner 11-21: "the module being linked at the bottom right/bottom 40% of the
// screen space with aspet ratio"): the inset's rectangle (the view's bottom right, 40% of each side, its aspect kept, none in a view too small), and which module it
// shows when (the newest planned, drawn anew once a module, never twice for one; nothing after the plot ends).
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_preview.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static std::string Said(host::PlotPreviewRect const &r)
{
	return std::to_string((long long)r.x) + "," + std::to_string((long long)r.y) + " " + std::to_string((long long)r.w) + "x" + std::to_string((long long)r.h);
}

int main()
{
	// ---- THE INSET: the view's bottom right, 40% of its width and of its height (the owner's "bottom 40% ... with aspet ratio")
	{
		host::PlotPreviewRect view;

		view.w = 1024, view.h = 768;

		host::PlotPreviewRect const r = host::PlotPreview_Inset(view);

		Check(r.w == 410 && r.h == 307 && r.x == 614 && r.y == 461, "a 1024 x 768 view: the inset 410 x 307 at 614,461, its right and bottom edges on the view's (" + Said(r) + ")");

		double const viewAspect = 1024.0 / 768.0, insetAspect = r.h > 0 ? (double)r.w / (double)r.h : 0.0;

		Check(r.h > 0 && fabs(insetAspect - viewAspect) <= 1.0 / r.h + 1e-9, "its aspect the view's, within a pixel's rounding (" + std::to_string(insetAspect) + " against " + std::to_string(viewAspect) + ")");
	}

	{
		host::PlotPreviewRect view;

		view.x = 100, view.y = 50, view.w = 1600, view.h = 900;   // (a view that does not start at the window's corner, and is wide)

		host::PlotPreviewRect const r = host::PlotPreview_Inset(view);

		Check(r.w == 640 && r.h == 360 && r.x == 100 + 1600 - 640 && r.y == 50 + 900 - 360, "a 1600 x 900 view at 100,50: the inset 640 x 360 at its own bottom right (" + Said(r) + ")");
	}

	{
		host::PlotPreviewRect view;

		view.w = 60, view.h = 40;   // (40% of it is 24 x 16: under the least)

		host::PlotPreviewRect const r = host::PlotPreview_Inset(view);

		Check(r.w == 0 && r.h == 0, "a view too small for an inset: none (" + Said(r) + ")");
	}

	// ---- ONCE A MODULE: frame by frame, the worker's count of planned jobs; the inset drawn anew only when a newer job is planned, with that newest job
	{
		host::PlotPreviewState state;
		std::vector<size_t> const frames = { 0, 0, 1, 1, 1, 2, 2, 5, 5, 5, 6 };   // (a frame can see several jobs finish at once: 2 -> 5)
		std::vector<size_t> drawn;

		for (size_t const planned : frames)
		{
			if (host::PlotPreview_Next(&state, planned))
			{
				drawn.push_back(state.shown);
			}
		}

		Check(drawn == std::vector<size_t>({ 0, 1, 4, 5 }) && state.redraws == 4, "drawn anew 4 times over 11 frames, each with the newest job planned (0, 1, 4, 5), never twice for one (" +
			std::to_string((long long)state.redraws) + " redraws)");

		host::PlotPreview_End(&state);

		Check(state.shown == (size_t)-1 && state.redraws == 4 && !host::PlotPreview_Next(&state, 0), "the plot ended: nothing shown, the count kept, nothing to draw for no job");
	}

	// ---- THE WHOLE MODULE IN VIEW: a module 12 m x 3 m x 8 m (gencor-sized) in a 1024 x 768 view with a 90-degree view across (focal 512 px): the camera stands far
	// enough that the box's round (its half diagonal, with the margin) lies inside the narrower angle (the vertical: atan(384 / 512)); a box with a side the wrong
	// way round, or no focal, gives 0
	{
		int32_t const mins[3] = { 0, -1000, 0 }, maxs[3] = { 12000, 2000, 8000 };
		double const d = host::PlotPreview_Distance(mins, maxs, 512.0, 1024, 768);
		double const half = 0.5 * sqrt(12000.0 * 12000.0 + 3000.0 * 3000.0 + 8000.0 * 8000.0), angle = atan(384.0 / 512.0);

		Check(d > 0.0 && (half * host::kPlotPreviewMargin) / d <= sin(angle) + 1e-9 && (half * host::kPlotPreviewMargin) / d >= sin(angle) - 1e-6, "the camera at the distance where the "
			"box's round just fills the narrower angle (" + std::to_string(d) + " mm)");

		int32_t const wrong[3] = { 12001, 0, 0 };

		Check(host::PlotPreview_Distance(wrong, maxs, 512.0, 1024, 768) == 0.0 && host::PlotPreview_Distance(mins, maxs, 0.0, 1024, 768) == 0.0, "a box the wrong way round, or no focal: 0");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
