// host_autoplot_handoff_test.cpp: THE PLOT AND THE WINDOW IN STEP (autoplot_handoff.h). Two threads, as in the editor: a worker that hands each AI module over and
// waits until it is drawn, and a "window" that takes the newest copy and says it drew it. Run thousands of times, so a copy read while it is still being written, a
// module skipped or shown out of order, or a wait that never ends would show here; it is also built with the address sanitizer (tests/host_autoplot_handoff_test.args,
// the same flags as avpx/tests/takeouts/sanitize.ps1), which stops at the first read or write outside what a thread was given.
// Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "autoplot_handoff.h"

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

static double MsSince(std::chrono::steady_clock::time_point from)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
}

// A copy for module `job` with `ways` ways: every number in it is the job's, so a copy read while it was half written (or freed) would show another number
static std::shared_ptr<host::AutoPlotShown> Copy(size_t job, size_t ways, bool finished)
{
	std::shared_ptr<host::AutoPlotShown> copy = std::make_shared<host::AutoPlotShown>();

	copy->job = job;
	copy->of = 0;
	copy->name = "module " + std::to_string((unsigned long long)job);
	copy->gameModules.assign(1 + job % 3, (int32_t)job);

	for (size_t w = 0; w < ways; w++)
	{
		host::AutoPlotShownWay way;

		way.alien = (w % 2) == 0;
		way.points.assign(3 * (2 + w % 5), (int32_t)job);
		copy->ways.push_back(way);
	}

	if (finished)
	{
		copy->volumes.resize(job % 7);

		for (host::WaypointRec &box : copy->volumes)
		{
			box.centre[0] = box.centre[1] = box.centre[2] = (int32_t)job;
		}
	}

	copy->finished = finished;
	copy->words = finished ? "done" : "searching";
	return copy;
}

// Every number in a copy is its job's
static bool Whole(host::AutoPlotShown const &copy)
{
	bool whole = copy.name == "module " + std::to_string((unsigned long long)copy.job) && copy.gameModules.size() == 1 + copy.job % 3;

	for (int32_t const m : copy.gameModules)
	{
		whole = whole && m == (int32_t)copy.job;
	}

	for (host::AutoPlotShownWay const &way : copy.ways)
	{
		for (int32_t const p : way.points)
		{
			whole = whole && p == (int32_t)copy.job;
		}
	}

	whole = whole && (!copy.finished || copy.volumes.size() == copy.job % 7);

	for (host::WaypointRec const &box : copy.volumes)
	{
		whole = whole && box.centre[0] == (int32_t)copy.job && box.centre[2] == (int32_t)copy.job;
	}

	return whole;
}

int main()
{
	printf("host_autoplot_handoff_test\n");

	// 1. IN STEP, THOUSANDS OF TIMES: the worker hands over 3000 modules (each three copies while it searches, then its last), the window draws whatever is newest.
	// Every module's last copy must be drawn, in order, once; every copy the window reads must be whole; the newest never goes back
	{
		size_t const modules = 3000;
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false), workerDone(false);
		std::atomic<size_t> notDrawn(0);

		handOff.Start();
		handOff.Watching(true);

		std::thread worker([&]()
		{
			for (size_t j = 0; j < modules; j++)
			{
				for (size_t w = 1; w <= 3; w++)
				{
					handOff.Progress(Copy(j, w, false));
				}

				if (!handOff.Finished(Copy(j, 3, true), stop))
				{
					notDrawn++;
				}
			}

			workerDone = true;
		});

		std::vector<size_t> finishedSeen;
		size_t broken = 0, backwards = 0, taken = 0;
		uint64_t lastNumber = 0;
		size_t lastJob = 0;
		auto const from = std::chrono::steady_clock::now();

		while (!workerDone && MsSince(from) < 60000.0)
		{
			std::shared_ptr<host::AutoPlotShown const> const copy = handOff.Newest();

			if (!copy)
			{
				continue;
			}

			taken++;
			broken += Whole(*copy) ? 0 : 1;
			backwards += copy->number < lastNumber || copy->job < lastJob ? 1 : 0;

			if (copy->finished && copy->number != lastNumber)
			{
				finishedSeen.push_back(copy->job);
			}

			lastNumber = copy->number;
			lastJob = copy->job;
			handOff.Drawn(copy->number);
		}

		worker.join();

		bool inOrder = finishedSeen.size() == modules;

		for (size_t j = 0; inOrder && j < modules; j++)
		{
			inOrder = finishedSeen[j] == j;
		}

		Check(workerDone && notDrawn == 0, "the worker finished all " + std::to_string((unsigned long long)modules) + " modules, each one's last copy drawn (" +
			std::to_string((unsigned long long)notDrawn.load()) + " not)");
		Check(inOrder, "every module's last copy was drawn, once, in order (" + std::to_string((unsigned long long)finishedSeen.size()) + " seen of " +
			std::to_string((unsigned long long)modules) + ")");
		Check(broken == 0, "every copy the window read was whole: no number of another module in it (" + std::to_string((unsigned long long)taken) + " read, " +
			std::to_string((unsigned long long)broken) + " broken)");
		Check(backwards == 0, "the newest copy never went back (" + std::to_string((unsigned long long)backwards) + " times)");
		Check(handOff.Waits() == modules, "the worker waited once a module (" + std::to_string((unsigned long long)handOff.Waits()) + ")");
	}

	// 2. NOBODY WATCHING (the preview off, or not on the screen): Finished never waits
	{
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false);

		handOff.Start();

		auto const from = std::chrono::steady_clock::now();
		bool const went = handOff.Finished(Copy(1, 1, true), stop);
		double const ms = MsSince(from);

		Check(went && ms < 20.0, "nobody watching: the last copy is handed over and the worker goes on at once (" + std::to_string(ms) + " ms)");
		Check(handOff.Newest() && handOff.Newest()->job == 1 && handOff.Newest()->number == 1, "it is still the newest, numbered 1");
	}

	// 3. STOP ENDS A WAIT (the plot cancelled, the level closed): the window watches but never draws; Stop after 100 ms
	{
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false);
		bool went = true;
		double ms = 0.0;

		handOff.Start();
		handOff.Watching(true);

		std::thread worker([&]()
		{
			auto const from = std::chrono::steady_clock::now();

			went = handOff.Finished(Copy(2, 1, true), stop);
			ms = MsSince(from);
		});

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		handOff.Stop();
		worker.join();
		Check(!went && ms >= 90.0 && ms < 1000.0, "Stop ends the wait, and says the plot is stopping (false after " + std::to_string(ms) + " ms)");

		auto const from = std::chrono::steady_clock::now();
		bool const after = handOff.Finished(Copy(3, 1, true), stop);

		Check(!after && MsSince(from) < 20.0, "after Stop, no wait at all until the next Start");
	}

	// 4. THE PLOT'S OWN STOP FLAG (set without a Stop call) ends a wait within a look or two
	{
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false);
		bool went = true;
		double ms = 0.0;

		handOff.Start();
		handOff.Watching(true);

		std::thread worker([&]()
		{
			auto const from = std::chrono::steady_clock::now();

			went = handOff.Finished(Copy(4, 1, true), stop);
			ms = MsSince(from);
		});

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		stop = true;
		worker.join();
		Check(!went && ms < 100.0 + 3 * host::kAutoPlotHandOffLookMs + 200.0, "the stop flag alone ends the wait (false after " + std::to_string(ms) + " ms)");
	}

	// 5. THE WINDOW STOPS WATCHING (the screen left): a waiting worker goes on, and the plot with it
	{
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false);
		bool went = false;

		handOff.Start();
		handOff.Watching(true);

		std::thread worker([&]() { went = handOff.Finished(Copy(5, 1, true), stop); });

		std::this_thread::sleep_for(std::chrono::milliseconds(60));
		handOff.Watching(false);
		worker.join();
		Check(went, "not watching any more: the waiting worker goes on (true: the plot goes on)");
	}

	// 6. A NEW RUN after a stopped one waits again
	{
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false);

		handOff.Start();
		handOff.Stop();
		handOff.Start();
		handOff.Watching(true);

		std::atomic<bool> went(false);
		std::thread worker([&]() { went = handOff.Finished(Copy(6, 1, true), stop); });

		std::this_thread::sleep_for(std::chrono::milliseconds(60));

		bool const stillWaiting = !went;
		std::shared_ptr<host::AutoPlotShown const> const copy = handOff.Newest();

		handOff.Drawn(copy ? copy->number : 0);
		worker.join();
		Check(stillWaiting && went, "after Start again, the worker waits for the window, and goes on once it has drawn the copy");
		Check(copy && copy->number == 1, "a new run counts from 1 again (" + std::to_string((unsigned long long)(copy ? copy->number : 0)) + ")");
	}

	// A NEW RUN IS TOLD APART (Drake 2026-11-23, the owner's "are we clearing the previous level when loading the new one?"): All stock maps plotted derelict and
	// then genshd1 with no frame between that the window saw as "no plot", the copy numbers counting from 1 again, so the window kept derelict's last copy (head 384,
	// job 161) into genshd1's plot and never landed genshd1's modules (measured on 8275930d: landed 1 for genshd1's first 34 modules, 14 when plotted alone).
	// Each Start counts a run, every copy carries the run it belongs to, and a Start lets go of the last run's newest copy.
	{
		host::AutoPlotHandOff handOff;
		std::atomic<bool> stop(false);

		handOff.Start();
		uint64_t const first = handOff.Run();
		handOff.Progress(Copy(161, 2, false));
		bool const firstCopyIsFirstRun = handOff.Newest() && handOff.Newest()->run == first;
		handOff.Finished(Copy(161, 2, true), stop);

		handOff.Start();
		uint64_t const second = handOff.Run();
		bool const lastRunsCopyGone = !handOff.Newest();

		handOff.Progress(Copy(0, 1, false));
		std::shared_ptr<host::AutoPlotShown const> const copy = handOff.Newest();

		Check(first == 1 && second == 2, "each Start counts a run (" + std::to_string((unsigned long long)first) + ", then " + std::to_string((unsigned long long)second) + ")");
		Check(firstCopyIsFirstRun, "a copy carries the run it was handed over in");
		Check(lastRunsCopyGone, "a new run's Start lets go of the last run's newest copy");
		Check(copy && copy->run == second && copy->number == 1, "the new run's first copy: its own run, numbered 1 (run " +
			std::to_string((unsigned long long)(copy ? copy->run : 0)) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
