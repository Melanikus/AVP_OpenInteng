// moving_wait.h: the scenario harness's WAIT THAT FOLLOWS THE RUN ("wait-moving"). Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng, on
// Bishop 2's order for the alpha.6 cut. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a fixed time limit can't tell a slow run from a stuck one. acceptance-all-stock-maps-all waited 300 s for each of its 22 levels, and went RED twice
// at the same step while Caverns was still plotting ("Auto-plot: module 69 of 120 (CAVERNH-02), way 860 of 1418: searching"): moving all the time, only
// slower than the limit (Dietrich's round on 120b270f, 11-23). A longer fixed limit would only move the edge.
// WHAT IT DOES: wait-moving <stall ms> <field> <words>: the state is asked over and over until the field holds the words. It waits as long as the run
// MOVES: every time the field's text changes, the stall clock starts again. It fails only when the field stays the same, word for word, for <stall ms>
// (the run is stuck, or has stopped without ever saying the words), or after kMostMs in all (the file's own time limit, run-shards', comes first anyway).
// A state the editor doesn't answer in time counts as no change: a frame that takes long is not a stall until the stall time says so.
// These are the pure parts (no Windows), so tests/harness_moving_wait_test.cpp can check them; drive_scenario.cpp asks the editor and the clock.
#pragma once

#include <stdint.h>

#include <string>

namespace moving_wait
{
	uint64_t const kMostStallMs = 600000;    // 10 minutes without one change: more than that is a typo, and is refused
	uint64_t const kMostMs = 3600000;        // an hour in all: what run-shards allows a file at most

	enum class Step
	{
		Keep,       // not there yet, and still moving: ask again
		Reached,    // the field holds the words
		Stalled,    // the field hasn't changed for the stall time
		TooLong     // kMostMs in all
	};

	struct Wait
	{
		uint64_t stallMs = 0, start = 0, lastChange = 0;
		std::string lastText;
		bool seen = false;       // any answer at all yet
	};

	inline Wait Begin(uint64_t stallMs, uint64_t now)
	{
		Wait w;
		w.stallMs = stallMs;
		w.start = now;
		w.lastChange = now;
		return w;
	}

	// One look: the field's text now (answered = false when the editor gave no state line in time), at the clock's `now` (ms)
	inline Step Look(Wait *w, bool answered, std::string const &fieldText, std::string const &words, uint64_t now)
	{
		if (answered)
		{
			if (fieldText.find(words) != std::string::npos)
			{
				return Step::Reached;
			}

			if (!w->seen || fieldText != w->lastText)
			{
				w->lastText = fieldText;
				w->lastChange = now;
				w->seen = true;
			}
		}

		if (now - w->start >= kMostMs)
		{
			return Step::TooLong;
		}

		return now - w->lastChange >= w->stallMs ? Step::Stalled : Step::Keep;
	}
}
