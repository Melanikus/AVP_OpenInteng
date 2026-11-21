// autoplot_progress.h: THE AUTO-PLOT'S PROGRESS WORDS on the status line (the owner 11-21: "When I try to run alien auto link I don't see anything", "It takes forever?",
// "Finished it": a whole-level plot ran about 112 s with nothing shown). While the worker plots, the module it is on, of how many, by name, and the run's ways; at the end, the
// run's summary (autoplot_command.cpp's, unchanged at its start, which the scenarios read) with the AI modules that kept Rebellion's own waypoints and the run's time.
// Pure: words only. Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdio.h>

#include <string>

namespace host
{
	// While AI module `at` (counted from 0) of `of` is plotted: "Auto-plot: module 3 of 251 (corr04A), way 120 of 9840" (the ways counted over the whole run). An `at` past the
	// last (the worker just done) says the last; a module with no name has no brackets
	inline std::string AutoPlot_ProgressWords(size_t at, size_t of, std::string const &name, size_t wayAt, size_t wayCount)
	{
		size_t const shown = at < of ? at + 1 : of;

		return "Auto-plot: module " + std::to_string((unsigned long long)shown) + " of " + std::to_string((unsigned long long)of) + (name.empty() ? std::string() : " (" + name + ")") +
			", way " + std::to_string((unsigned long long)wayAt) + " of " + std::to_string((unsigned long long)wayCount);
	}

	// While the networks are checked and written, a slice a frame (the main thread's part, about 14 s of a whole Genshd1 plot that froze the window): "Auto-plot: writing
	// module 3 of 273 (corr04A)". Never the word "written" (the scenarios wait for the end line's "AI modules written")
	inline std::string AutoPlot_WritingWords(size_t at, size_t of, std::string const &name)
	{
		size_t const shown = at < of ? at + 1 : of;

		return "Auto-plot: writing module " + std::to_string((unsigned long long)shown) + " of " + std::to_string((unsigned long long)of) +
			(name.empty() ? std::string() : " (" + name + ")");
	}

	// The end line's AI modules not written (refused, not shaped) that still hold Rebellion's own alien waypoints: ", 12 kept Rebellion's"; none: nothing (so the end line reads
	// as the scenarios have always read it)
	inline std::string AutoPlot_KeptWords(size_t kept)
	{
		return kept == 0 ? std::string() : ", " + std::to_string((unsigned long long)kept) + " kept Rebellion's";
	}

	// The end line's last words, the run's time from its start to its last network written: "; in 112 s"; under 10 s, a tenth ("; in 3.4 s"); a clock gone back, 0
	inline std::string AutoPlot_TimeWords(double seconds)
	{
		double const s = seconds > 0.0 ? seconds : 0.0;
		char text[48];

		if (s < 10.0)
		{
			snprintf(text, sizeof(text), "; in %.1f s", s);
		}
		else
		{
			snprintf(text, sizeof(text), "; in %.0f s", s);
		}

		return text;
	}
}
