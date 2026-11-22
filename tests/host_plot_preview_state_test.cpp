// host_plot_preview_state_test.cpp: THE PLOT'S PREVIEW's state line (src/host/plot_preview_state.h; the owner 11-21: "draw the module being linked at the bottom
// right/bottom 40% of the screen space with aspet ratio"): the "plotpreview" field's words for each thing the preview can show (off before a plot, on with its inset,
// off after a plot with its count kept), the field on the state line, and every number found by the harness's own rule (expect-number: the first place the label's
// words appear in the field, then the whole number after them), so no label is found inside another.
// What the preview showed comes from a stand-in here (the real one draws, plot_preview_view.cpp), so the test needs no view or device.
// Written 2026-11-21 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "plot_preview_state.h"
#include "plot_preview_view.h"
#include "state_line.h"

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

// THE STAND-IN: what the preview showed this frame, set by each case below
static PlotPreviewShown standIn;

PlotPreviewShown PlotPreviewView_Shown()
{
	return standIn;
}

// the harness's rule (tests/drive_scenario expect-number): the first place the label's words appear, then the whole number right after them; -1 when there is none
static long long NumberAfter(std::string const &field, std::string const &label)
{
	size_t const at = field.find(label);

	if (at == std::string::npos)
	{
		return -1;
	}

	size_t i = at + label.size();
	long long n = 0;
	bool any = false;

	while (i < field.size() && field[i] >= '0' && field[i] <= '9')
	{
		n = n * 10 + (field[i] - '0');
		i++;
		any = true;
	}

	return any ? n : -1;
}

static PlotPreviewShown Showing(int32_t x, int32_t y, int32_t w, int32_t h, int32_t head, size_t job, size_t redraws, double ms)
{
	PlotPreviewShown s;

	s.on = true;
	s.inset.x = x, s.inset.y = y, s.inset.w = w, s.inset.h = h;
	s.head = head;
	s.job = job;
	s.redraws = redraws;
	s.drawMs = ms;
	return s;
}

int main()
{
	// ---- BEFORE ANY PLOT: nothing shown, nothing counted
	{
		std::string const words = PlotPreviewState_Words(PlotPreviewShown());

		Check(words == "off", "before any plot the field is exactly \"off\" (it is \"" + words + "\")");
	}

	// ---- WHILE IT SHOWS: the inset of a 1024 x 768 view (614 461 410 307: its bottom right, 40% of each side), the module, the counts; the time rounded
	{
		std::string const words = PlotPreviewState_Words(Showing(614, 461, 410, 307, 235, 2, 3, 40.6));

		Check(words == "on left 614 top 461 width 410 height 307 head 235 job 2 redraws 3 ms 41 main 0", "while it shows, every number after its name (\"" + words + "\")");

		// each label as a scenario writes it (expect-number's "_" is a space): found where it belongs, never inside another word
		Check(NumberAfter(words, "left ") == 614, "expect-number left_ reads 614");
		Check(NumberAfter(words, "top ") == 461, "expect-number top_ reads 461");
		Check(NumberAfter(words, "width ") == 410, "expect-number width_ reads 410");
		Check(NumberAfter(words, "height ") == 307, "expect-number height_ reads 307");
		Check(NumberAfter(words, "head ") == 235, "expect-number head_ reads 235");
		Check(NumberAfter(words, "job ") == 2, "expect-number job_ reads 2");
		Check(NumberAfter(words, "redraws ") == 3, "expect-number redraws_ reads 3");
		Check(NumberAfter(words, "ms ") == 41, "expect-number ms_ reads 41");
	}

	// ---- THE FULL MAP UNDER IT (the owner 11-21: "Full map + inset on top"; Frost): "main 1" when the last frame drew the inset OVER the whole main view
	// (PlotPreviewShown.over, level_view.h LevelView_InsetDrawn), "main 0" when not; it ends the "on" words, and never comes while off
	{
		PlotPreviewShown s = Showing(614, 461, 410, 307, 235, 2, 3, 40.6);

		s.over = true;

		std::string const words = PlotPreviewState_Words(s);

		Check(words == "on left 614 top 461 width 410 height 307 head 235 job 2 redraws 3 ms 41 main 1", "over the main view: main 1 (\"" + words + "\")");
		Check(NumberAfter(words, "main ") == 1, "expect-number main_ reads 1");
		s.on = false;
		Check(PlotPreviewState_Words(s).find("main") == std::string::npos, "off: no main word");
	}

	// ---- SHOWING BEFORE A JOB IS PLANNED: the job is "none", the counts 0
	{
		std::string const words = PlotPreviewState_Words(Showing(768, 432, 512, 288, -1, (size_t)-1, 0, 0.0));

		Check(words == "on left 768 top 432 width 512 height 288 head -1 job none redraws 0 ms 0 main 0", "no job yet: \"job none\" (\"" + words + "\")");
	}

	// ---- AFTER A PLOT: off, the last plot's count kept (so a scenario can check one redraw a module after the end)
	{
		PlotPreviewShown s = Showing(614, 461, 410, 307, 235, 2, 3, 120.4);

		s.on = false;

		std::string const words = PlotPreviewState_Words(s);

		Check(words == "off job 2 redraws 3 ms 120", "after a plot, off with its count (\"" + words + "\")");
		Check(words.compare(0, 4, "off ") == 0, "after a plot the field still starts with \"off\"");
		Check(NumberAfter(words, "redraws ") == 3, "after a plot expect-number redraws_ reads 3");
	}

	// ---- THE MAIN VIEW'S LIVE MODULES (the owner 11-21: "the modules updating on the map as they are build"): once a module of the plot is on the main view, how many,
	// and the main view's lines for them, after the drawing time; while it shows and after the plot. None yet: no such words (the words as before)
	{
		PlotPreviewShown s = Showing(614, 461, 410, 307, 235, 2, 3, 40.6);

		s.landed = 3;
		s.mapLines = 1200;

		std::string const words = PlotPreviewState_Words(s);

		Check(words == "on left 614 top 461 width 410 height 307 head 235 job 2 redraws 3 ms 41 landed 3 maplines 1200 main 0", "while it shows, the modules on the main view and "
			"their lines (\"" + words + "\")");
		Check(NumberAfter(words, "landed ") == 3 && NumberAfter(words, "maplines ") == 1200 && NumberAfter(words, "main ") == 0, "expect-number landed_, maplines_ and main_");
		s.on = false;
		Check(PlotPreviewState_Words(s) == "off job 2 redraws 3 ms 41 landed 3 maplines 1200", "after the plot, kept (\"" + PlotPreviewState_Words(s) + "\")");
	}

	// ---- A NEGATIVE TIME (never from the preview, but the words must not say it): 0
	{
		std::string const words = PlotPreviewState_Words(Showing(614, 461, 410, 307, 235, 0, 1, -5.0));

		Check(NumberAfter(words, "ms ") == 0, "a negative drawing time reads ms 0 (\"" + words + "\")");
	}

	// ---- ON THE STATE LINE: PlotPreviewState_Line adds the field from what the preview showed this frame
	{
		StateLine_NewFrame();
		standIn = Showing(614, 461, 410, 307, 235, 2, 3, 40.6);
		PlotPreviewState_Line();

		std::string const line = StateLine_Words();

		Check(line.find("; plotpreview \"on left 614 top 461 width 410 height 307 head 235 job 2 redraws 3 ms 41 main 0\"") != std::string::npos,
			"the state line carries plotpreview (\"" + line + "\")");

		StateLine_NewFrame();
		standIn = PlotPreviewShown();
		PlotPreviewState_Line();
		Check(StateLine_Words().find("; plotpreview \"off\"") != std::string::npos, "the next frame, nothing shown: plotpreview \"off\"");
	}

	printf("host_plot_preview_state_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
