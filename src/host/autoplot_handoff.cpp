// autoplot_handoff.cpp: THE PLOT AND THE WINDOW IN STEP (autoplot_handoff.h: who owns what, and who waits for whom).
// Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_handoff.h"

#include <chrono>

namespace host
{
	void AutoPlotHandOff::Start()
	{
		std::lock_guard<std::mutex> held(lock);

		newest.reset();
		handed = 0;
		drawn = 0;
		runs++;   // a new run: its copies carry this number (Run)
		stopped = false;
		waitedMs = 0.0;
		waits = 0;
	}

	uint64_t AutoPlotHandOff::Progress(std::shared_ptr<AutoPlotShown> copy)
	{
		if (!copy)
		{
			return 0;
		}

		std::lock_guard<std::mutex> held(lock);

		// numbered here, under the lock, while the copy is still the worker's alone: once it is in `newest` nobody changes it again
		copy->number = ++handed;
		copy->run = runs;
		newest = copy;
		return copy->number;
	}

	bool AutoPlotHandOff::Finished(std::shared_ptr<AutoPlotShown> copy, std::atomic<bool> const &stop)
	{
		uint64_t const number = Progress(copy);
		std::unique_lock<std::mutex> held(lock);

		if (stopped)
		{
			return false;
		}

		if (!watching || number == 0)
		{
			return true;   // nobody to wait for: the plot goes on at full speed
		}

		waits++;

		auto const from = std::chrono::steady_clock::now();

		// the wait: until the window has drawn this copy, stops watching, or the plot stops. The lock is let go while waiting (the window's calls take it), and the
		// plot's own stop flag is looked at every kAutoPlotHandOffLookMs, so a flag set without a Stop call still ends the wait soon
		while (drawn < number && watching && !stopped && !stop)
		{
			woken.wait_for(held, std::chrono::milliseconds(kAutoPlotHandOffLookMs));
		}

		waitedMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
		return !stopped && !stop;
	}

	std::shared_ptr<AutoPlotShown const> AutoPlotHandOff::Newest()
	{
		std::lock_guard<std::mutex> held(lock);

		return newest;   // (a copy of the pointer: the window holds the copy from here, whatever the worker hands over next)
	}

	uint64_t AutoPlotHandOff::Run()
	{
		std::lock_guard<std::mutex> held(lock);

		return runs;
	}

	void AutoPlotHandOff::Drawn(uint64_t number)
	{
		{
			std::lock_guard<std::mutex> held(lock);

			drawn = number > drawn ? number : drawn;
		}

		woken.notify_all();
	}

	void AutoPlotHandOff::Watching(bool on)
	{
		{
			std::lock_guard<std::mutex> held(lock);

			watching = on;
		}

		woken.notify_all();
	}

	void AutoPlotHandOff::Stop()
	{
		{
			std::lock_guard<std::mutex> held(lock);

			stopped = true;
		}

		woken.notify_all();
	}

	double AutoPlotHandOff::WaitedMs()
	{
		std::lock_guard<std::mutex> held(lock);

		return waitedMs;
	}

	size_t AutoPlotHandOff::Waits()
	{
		std::lock_guard<std::mutex> held(lock);

		return waits;
	}
}
