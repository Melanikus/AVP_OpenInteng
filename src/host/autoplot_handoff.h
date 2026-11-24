// autoplot_handoff.h: THE PLOT AND THE WINDOW IN STEP. Alien auto waypoint plots on a worker thread; the window (the main thread) shows each AI module in the plot's
// preview as it goes. The owner 11-21, through Bishop: "Show every module in order", and "I mean between UI and action (Auto Waypoint)": the worker plans module i,
// hands its finished copy over, and WAITS until the window has drawn it, then starts module i+1. Nothing is skipped, and the picture never falls behind the plot.
// Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHO OWNS WHAT (the reason this is safe between two threads):
// - The worker MAKES a copy (AutoPlotShown) and fills it in. While it fills it in, nobody else can see it.
// - It then HANDS IT OVER (Progress or Finished). From then on the copy is never changed by anyone: it is handed over as a pointer to a constant copy
//   (std::shared_ptr<AutoPlotShown const>), and the worker makes a NEW copy for anything more it has to show.
// - The window TAKES the newest copy (Newest) and keeps its own pointer to it for as long as it draws from it. Whichever side lets go of a copy last frees it
//   (the shared pointer counts its holders safely between threads).
// - The only thing the two threads share is the hand-over itself: which copy is newest, which one the window has drawn, and whether the window is watching. A lock
//   guards those few fields, and it is held only while they are read or set (a pointer copied, a number set), never while anything is made or drawn.
//
// WHO WAITS FOR WHOM:
// - The WINDOW NEVER WAITS for the worker. Its calls take the lock for a moment and return.
// - The WORKER WAITS in one place only, Finished: until the window says it has drawn that copy (Drawn). It also stops waiting at once when the window is not watching
//   (no preview: the plot runs at full speed, as with the preview off), when Stop is called (the plot cancelled, the level closed, the editor ending), or when the
//   plot's own stop flag is set. So a wait can never outlast the plot.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "waypoint_model.h"   // WaypointRec: the planned boxes

namespace host
{
	// One way the search found, as a line of points (the world's mm, y down): x, y, z, then the next point's x, y, z, and so on
	struct AutoPlotShownWay
	{
		bool alien = true;             // the alien's search (else the marine's floor route)
		std::vector<int32_t> points;   // 3 numbers a point
	};

	// One AI module as the preview shows it at one moment of the plot. Made by the worker, then never changed (see WHO OWNS WHAT)
	struct AutoPlotShown
	{
		uint64_t number = 0;                   // set by the hand-over: 1, 2, 3, ... for the run, so the window knows a copy from the one before
		uint64_t run = 0;                      // set by the hand-over too: the run it belongs to (Run), so the window knows a NEW run's copies from the last run's
		size_t job = 0;                        // which AI module of the run (counted from 0) ...
		size_t of = 0;                         // ... of how many
		int32_t head = -1;                     // its head module (the master, or a module of none)
		std::string name;                      // its head's name
		std::vector<int32_t> gameModules;      // the game's numbers of its modules (the preview frames these)
		std::vector<AutoPlotShownWay> ways;    // the search's ways found so far (the owner's choice (a): "the search's ways drawn as they're found")
		std::vector<WaypointRec> volumes;      // its planned boxes and links (the file's units), once placed; empty before that, or when none were
		bool finished = false;                 // the module's last copy: its plan is done, and the worker waits until the window has drawn it
		std::string words;                     // what the module came to, for the status line: "searching", "14 volumes placed", "not shaped, nothing drawn", ...
	};

	class AutoPlotHandOff
	{
	public:
		// THE WORKER'S SIDE

		// A new run: nothing handed over yet, nothing drawn, not stopped. (The window's watching is the window's own: it is left as it is.)
		void Start();

		// A copy of the module so far (the search's ways as they come): it becomes the newest, replacing any copy the window has not taken yet. Never waits.
		// Returns the number it was given.
		uint64_t Progress(std::shared_ptr<AutoPlotShown> copy);

		// The module's last copy: it becomes the newest, then the worker WAITS until the window has drawn it. Returns true when it was drawn, or when nobody was
		// watching (the plot goes on); false when the wait was ended by Stop or by `stop` (the plot is stopping: the worker should end). The wait looks at `stop`
		// every kAutoPlotHandOffLookMs, so a stop flag set without a Stop call still ends it soon.
		bool Finished(std::shared_ptr<AutoPlotShown> copy, std::atomic<bool> const &stop);

		// THE WINDOW'S SIDE (the main thread)

		// The newest copy handed over (null before the first). The window keeps the pointer as long as it draws from it.
		std::shared_ptr<AutoPlotShown const> Newest();

		// THE RUN, counted by Start (1 for the first): the window starts afresh when it changes (Drake 2026-11-23, the owner's "are we clearing the previous level"):
		// All stock maps plots one level after another with no frame between that the window could see as "no plot", and the copy numbers count from 1 again
		// each run, so a window that went by the numbers alone kept the last level's copy and never put the new level's modules on the map
		uint64_t Run();

		// The window has drawn copy `number` (the status line and the inset showed it on a frame). A worker waiting for that copy, or an older one, goes on.
		void Drawn(uint64_t number);

		// Whether the window is drawing the preview each frame. While it is not, the worker never waits (and one waiting now goes on).
		void Watching(bool on);

		// Ends any wait now, and every wait until the next Start (the plot cancelled, the level closed, the editor ending).
		void Stop();

		// For the run's end line: how long the worker waited for the window in all (ms), and how many times
		double WaitedMs();
		size_t Waits();

	private:
		std::mutex lock;                              // guards everything below, held only to read or set it
		std::condition_variable woken;                // the worker's wait in Finished
		std::shared_ptr<AutoPlotShown const> newest;  // the copy the window should show
		uint64_t handed = 0;                          // the last number given
		uint64_t runs = 0;                            // the runs Start has begun
		uint64_t drawn = 0;                           // the last number the window drew
		bool watching = false;
		bool stopped = false;
		double waitedMs = 0.0;
		size_t waits = 0;
	};

	// How often a waiting worker looks at the plot's own stop flag (ms)
	int const kAutoPlotHandOffLookMs = 50;
}
