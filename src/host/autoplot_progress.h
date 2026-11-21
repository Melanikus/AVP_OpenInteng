// autoplot_progress.h: THE AUTO-PLOT'S PROGRESS WORDS on the status line (the owner 11-21: "When I try to run alien auto link I don't see anything", "It takes forever?",
// "Finished it": a whole-level plot ran about 112 s with nothing shown). While the worker plots, the module it is on, of how many, by name, and the run's ways; at the end, the
// run's summary (autoplot_command.cpp's, unchanged at its start, which the scenarios read) with the AI modules that kept Rebellion's own waypoints and the run's time.
// Pure: words only. Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "autoplot_volumes.h"   // AutoPlotVolumeCounts, AutoPlot_UsedCounts (the values a run plotted with)

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

	// THE VALUES A RUN PLOTTED WITH (the owner's dialog, autoplot_volumes.h THE OWNER'S COUNTS / SIZES; Tao 2: the words say how the networks were made), for the end line
	// before its time and the run's log: "; with climb-ins 1, get-outs 1, corridor aim along 2500 mm, upper-face aim up 1000 mm and under 300 mm, Airduct off". The numbers are
	// the ones the planner used (a number not asked is its default; one out of range, held to the range)
	inline std::string AutoPlot_ValuesWords(AutoPlotVolumeCounts const &asked)
	{
		AutoPlotVolumeCounts const used = AutoPlot_UsedCounts(asked);
		auto const n = [](int32_t v) { return std::to_string((long long)v); };

		return "; with climb-ins " + n(used.climbIns) + ", get-outs " + n(used.getOuts) + ", corridor aim along " + n(used.highAlong) + " mm, upper-face aim up " + n(used.highUp) +
			" mm and under " + n(used.highUnder) + " mm, Airduct " + (used.airduct ? "on" : "off");
	}

	// WHAT THE PICTURE SHOWS, after the progress words (the owner 11-21, through Bishop: the status line's module and the picture's module the same, or the words say
	// which): the preview shows module `shown` (counted from 0) while module `planning` is planned, and what that module came to. The same one: ": 14 volumes placed";
	// another: "; showing module 3: 14 volumes placed". With no words, only which module ("" when it is the same one)
	inline std::string AutoPlot_PictureWords(size_t planning, size_t shown, std::string const &words)
	{
		std::string const after = words.empty() ? std::string() : ": " + words;

		return planning == shown ? after : "; showing module " + std::to_string((unsigned long long)(shown + 1)) + after;
	}

	// THE PLOT'S FRAMES (the owner 11-21, through Bishop: "it's slow and sluggish"; the measure his live plot is judged by): one stage's frames, each the time from one of
	// the plot's frames to the next (the whole frame: the window drawn and the plot's own share), as "search 812 frames, median 16 ms, 95% within 18 ms, longest 140 ms,
	// 3 over 50 ms". No frames: "search no frames". The times are whole milliseconds, rounded
	inline std::string AutoPlot_FrameWords(char const *stage, std::vector<double> ms)
	{
		std::string const named(stage ? stage : "");

		if (ms.empty())
		{
			return named + " no frames";
		}

		size_t over = 0;

		for (double &one : ms)
		{
			one = one > 0.0 ? one : 0.0;   // (a clock that went back counts as 0)
			over += one > 50.0 ? 1 : 0;
		}

		std::sort(ms.begin(), ms.end());

		// the median: the middle one (of an even count, the upper of the two); 95%: the frame that 95 in 100 are no longer than (the 19th of 20)
		size_t const n = ms.size();
		size_t const p95 = (n * 95 + 99) / 100 - 1;
		auto const whole = [](double v) { return std::to_string((long long)(v + 0.5)); };

		return named + " " + std::to_string((unsigned long long)n) + " frames, median " + whole(ms[n / 2]) + " ms, 95% within " + whole(ms[p95]) + " ms, longest " +
			whole(ms[n - 1]) + " ms, " + std::to_string((unsigned long long)over) + " over 50 ms";
	}
}
