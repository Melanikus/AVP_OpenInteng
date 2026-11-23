// The test for tests/drive_scenario/moving_wait.h, the harness's wait that follows the run (wait-moving). Written 2026-11-23 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no editor, no clock; the times are given.

#include <stdio.h>

#include <string>

#include "drive_scenario/moving_wait.h"

using namespace moving_wait;

static int failed = 0;

static void Check(bool good, char const *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);
	failed += good ? 0 : 1;
}

int main()
{
	puts("the wait that follows the run");

	// THE CASE THAT WENT RED: a level plotting for 9 minutes, its progress changing every few seconds, then the next level: reached, never stalled
	{
		Wait w = Begin(300000, 1000);
		Step step = Step::Keep;
		uint64_t t = 1000;

		for (int module = 1; module <= 120 && step == Step::Keep; module++, t += 4500)
		{
			step = Look(&w, true, "busy; level 16 of 22; caverns.RIF; Auto-plot: module " + std::to_string(module) + " of 120", "; level 17 of 22;", t);
		}

		Check(step == Step::Keep && t - 1000 > 300000, "a level plotting for 9 minutes, moving all the time, is waited for past the old fixed 300 s");
		Check(Look(&w, true, "busy; level 17 of 22; battle.RIF; opening", "; level 17 of 22;", t) == Step::Reached, "and the next level is reached");
	}

	// A STUCK RUN: the field the same, word for word, for the stall time: stalled, not before
	{
		Wait w = Begin(300000, 0);
		Check(Look(&w, true, "busy; level 16 of 22; caverns.RIF; Auto-plot: module 69 of 120", "; level 17 of 22;", 0) == Step::Keep, "the first look keeps waiting");
		Check(Look(&w, true, "busy; level 16 of 22; caverns.RIF; Auto-plot: module 69 of 120", "; level 17 of 22;", 299999) == Step::Keep, "the same words for just under the stall time: still waiting");
		Check(Look(&w, true, "busy; level 16 of 22; caverns.RIF; Auto-plot: module 69 of 120", "; level 17 of 22;", 300000) == Step::Stalled, "the same words for the stall time: STALLED");
	}

	// NO ANSWER counts as no change: a long frame is not a stall until the stall time says so, and doesn't start the clock again
	{
		Wait w = Begin(60000, 0);
		Look(&w, true, "busy; level 3 of 22; sulaco.RIF; plotting", "; level 4 of 22;", 0);
		Check(Look(&w, false, "", "; level 4 of 22;", 59999) == Step::Keep, "no state line for just under the stall time: still waiting");
		Check(Look(&w, false, "", "; level 4 of 22;", 60000) == Step::Stalled, "no state line for the stall time: STALLED (an editor that never answers)");
	}

	// A CHANGE starts the stall clock again
	{
		Wait w = Begin(60000, 0);
		Look(&w, true, "a", "z", 0);
		Look(&w, true, "b", "z", 50000);
		Check(Look(&w, true, "b", "z", 100000) == Step::Keep, "a change at 50 s: at 100 s it has been still for only 50 s: waiting");
		Check(Look(&w, true, "b", "z", 110000) == Step::Stalled, "and at 110 s, 60 s still: STALLED");
	}

	// AN HOUR IN ALL, however it moves
	{
		Wait w = Begin(60000, 0);
		Step step = Step::Keep;
		uint64_t t = 0;

		for (int i = 0; step == Step::Keep; i++, t += 1000)
		{
			step = Look(&w, true, std::to_string(i), "never", t);
		}

		Check(step == Step::TooLong && t - 1000 == kMostMs, "a run that moves for ever is stopped at an hour in all");
	}

	// the words already there at the first look
	{
		Wait w = Begin(1000, 0);
		Check(Look(&w, true, "idle; level 22 of 22; (the report is open)", "(the report is open)", 0) == Step::Reached, "words already there: reached at once");
	}

	printf(failed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failed ? 1 : 0;
}
