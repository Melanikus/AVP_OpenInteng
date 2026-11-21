// host_autoplot_progress_test.cpp: the auto-plot's progress words (autoplot_progress.h): the module it is on, of how many, by name, and the run's ways; the end line's AI
// modules that kept Rebellion's waypoints and the run's time (the owner 11-21: "When I try to run alien auto link I don't see anything"). Written 2026-11-21 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_progress.h"

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

int main()
{
	printf("host_autoplot_progress_test\n");

	std::string const p = host::AutoPlot_ProgressWords(2, 251, "corr04A", 120, 9840);

	Check(p == "Auto-plot: module 3 of 251 (corr04A), way 120 of 9840", "the module it is on, counted from 1, of how many, by name, and the run's ways (" + p + ")");
	Check(host::AutoPlot_ProgressWords(0, 1, "", 0, 0) == "Auto-plot: module 1 of 1, way 0 of 0", "a module with no name: no brackets (" +
		host::AutoPlot_ProgressWords(0, 1, "", 0, 0) + ")");
	Check(host::AutoPlot_ProgressWords(5, 3, "x", 0, 0) == "Auto-plot: module 3 of 3 (x), way 0 of 0", "past the last (the worker just done): the last (" +
		host::AutoPlot_ProgressWords(5, 3, "x", 0, 0) + ")");

	std::string const w = host::AutoPlot_WritingWords(2, 273, "corr04A");

	Check(w == "Auto-plot: writing module 3 of 273 (corr04A)", "writing, a slice a frame: the module it is on, of how many, by name (" + w + ")");
	Check(w.find("written") == std::string::npos, "never \"written\" (the scenarios wait for the end line's \"AI modules written\")");

	Check(host::AutoPlot_KeptWords(0).empty(), "none kept Rebellion's: nothing said (the end line as the scenarios read it)");
	Check(host::AutoPlot_KeptWords(12) == ", 12 kept Rebellion's", "12 kept Rebellion's (" + host::AutoPlot_KeptWords(12) + ")");

	Check(host::AutoPlot_TimeWords(112.4) == "; in 112 s", "112.4 s: \"; in 112 s\" (" + host::AutoPlot_TimeWords(112.4) + ")");
	Check(host::AutoPlot_TimeWords(3.44) == "; in 3.4 s", "3.44 s: a tenth (" + host::AutoPlot_TimeWords(3.44) + ")");
	Check(host::AutoPlot_TimeWords(-1.0) == "; in 0.0 s", "a clock that went back: 0 (" + host::AutoPlot_TimeWords(-1.0) + ")");

	// THE VALUES A RUN PLOTTED WITH (Tao 2, Bishop's order 11-21: "the values used go into the plot notes and the status line's end line, so a saved file says how it was
	// plotted"): the numbers the planner used, each default filled in and each out-of-range number held to its range, and the Airduct tick
	host::AutoPlotVolumeCounts asked;
	std::string const v = host::AutoPlot_ValuesWords(asked);

	Check(v == "; with climb-ins 1, get-outs 1, corridor aim along 2500 mm, upper-face aim up 1000 mm and under 300 mm, Airduct off", "nothing asked: the defaults (" + v + ")");
	asked.climbIns = 2;
	asked.getOuts = 0;
	asked.airduct = true;
	asked.highAlong = 4000;
	asked.highUp = 1200;
	asked.highUnder = 550;
	Check(host::AutoPlot_ValuesWords(asked) == "; with climb-ins 2, get-outs 0, corridor aim along 4000 mm, upper-face aim up 1200 mm and under 550 mm, Airduct on", "the owner's numbers (" +
		host::AutoPlot_ValuesWords(asked) + ")");
	asked.climbIns = 99;
	asked.highAlong = 600;
	asked.highUnder = 9000;
	Check(host::AutoPlot_ValuesWords(asked) == "; with climb-ins 8, get-outs 0, corridor aim along 2500 mm, upper-face aim up 1200 mm and under 550 mm, Airduct on", "out of range: "
		"the numbers the planner used, held to the ranges (" + host::AutoPlot_ValuesWords(asked) + ")");

	// WHAT THE PICTURE SHOWS (the owner 11-21, through Bishop: the status line's module and the picture's the same, or the words say which)
	Check(host::AutoPlot_PictureWords(2, 2, "14 volumes placed") == ": 14 volumes placed", "the same module: what it came to (" +
		host::AutoPlot_PictureWords(2, 2, "14 volumes placed") + ")");
	Check(host::AutoPlot_PictureWords(3, 2, "not shaped (no floor), nothing drawn") == "; showing module 3: not shaped (no floor), nothing drawn",
		"another module: which one, counted from 1, and what it came to (" + host::AutoPlot_PictureWords(3, 2, "not shaped (no floor), nothing drawn") + ")");
	Check(host::AutoPlot_PictureWords(2, 2, "").empty() && host::AutoPlot_PictureWords(4, 0, "") == "; showing module 1", "no words: only which module (" +
		host::AutoPlot_PictureWords(4, 0, "") + ")");

	// THE PLOT'S FRAMES (the owner 11-21: "it's slow and sluggish"): one stage's frame times, the median, the time 95 in 100 are within, the longest, and how many took
	// over 50 ms (a frame a person sees as a stall)
	std::vector<double> frames;

	for (int i = 0; i < 19; i++)
	{
		frames.push_back(16.0);
	}

	frames.push_back(140.4);   // one long frame of the 20: the 95th in 100 is the 19th, 16 ms
	std::string const f = host::AutoPlot_FrameWords("search", frames);

	Check(f == "search 20 frames, median 16 ms, 95% within 16 ms, longest 140 ms, 1 over 50 ms", "20 frames, one long: median, 95%, longest, over 50 ms (" + f + ")");

	std::vector<double> const unsorted = { 70.0, 10.0, 30.0, 50.0, 90.0 };
	std::string const u = host::AutoPlot_FrameWords("writing", unsorted);

	Check(u == "writing 5 frames, median 50 ms, 95% within 90 ms, longest 90 ms, 2 over 50 ms", "out of order: sorted first; exactly 50 is not over 50 (" + u + ")");
	Check(host::AutoPlot_FrameWords("grid", std::vector<double>()) == "grid no frames", "no frames (" + host::AutoPlot_FrameWords("grid", std::vector<double>()) + ")");
	Check(host::AutoPlot_FrameWords("grid", std::vector<double>(1, -3.0)) == "grid 1 frames, median 0 ms, 95% within 0 ms, longest 0 ms, 0 over 50 ms",
		"a clock that went back: 0 (" + host::AutoPlot_FrameWords("grid", std::vector<double>(1, -3.0)) + ")");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
