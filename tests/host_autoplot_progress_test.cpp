// host_autoplot_progress_test.cpp: the auto-plot's progress words (autoplot_progress.h): the module it is on, of how many, by name, and the run's ways; the end line's AI
// modules that kept Rebellion's waypoints and the run's time (the owner 11-21: "When I try to run alien auto link I don't see anything"). Written 2026-11-21 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

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

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
