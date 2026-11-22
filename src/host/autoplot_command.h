// autoplot_command.h: WAYPOINT MODE'S AUTO-PLOT ALIEN WAYS (the owner's addition 11-18; Bishop's design AvP-docs/engine/inteng/reassess/alien-autoplot-design.md): mode 0's "Auto-plot AI
// module" (the picked module's AI module) and "Auto-plot all AI modules" (every one), our menu items (menu-status.json "extras" on Waypoint's first bar). Each AI module's goals are its exit
// points (its entry points, ADJMDLEP: where an NPC heads to leave it) and a few places a marine stands; AvPx's own pathfinder (autoplot_grid.h, Drake's) finds the ways between them, an
// alien's (floor is lava) and a marine's on the floor; the generator (autoplot_plan.h) makes them stock alien waypoints; the checker (waypoint_net_check.h, Frost's) refuses a network the
// stock game would choke on; what passes is WRITTEN at once over the AI module's alien list (WaypointMode::AutoPlotWrite: the game's loader checks it). Existing alien waypoints are only
// replaced after a Yes. The searches run on a worker (the grid takes one caller at a time); the frame shows the progress and writes on the editor's main thread.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autoplot_volumes.h"

namespace host
{
	class AutoPlotHandOff;   // autoplot_handoff.h


	int32_t const kWaypointAutoPlotModule = 0x7330;   // ours: the picked module's AI module
	int32_t const kWaypointAutoPlotAll = 0x7331;      // ours: every AI module
	int32_t const kWaypointAutoPlotSelected = 0x7332; // ours: the AI modules selected in Waypoint mode 0 (Vasquez's Ctrl+click set; the owner 11-19, through Bishop: "select multiple
	                                                  // modules and apply to them"), planned as one set so their lanes join across the doorways between them
	size_t const kAutoPlotMostSpotsPerAiModule = 6;   // marine stand spots added to the exits as goals
	size_t const kAutoPlotMostPairs = 40;             // goal pairs searched per AI module (each twice: alien and floor)
	int32_t const kAutoPlotStationReach = 6000;       // a climb station's wall at most this far from where the alien lands (Drake: chain A's side walls 3.5-3.9 m off the lane)
	uint32_t const kAutoPlotAirduct = 0x80000000u;    // MODULEFLAG_AIRDUCT, the bit Strategy mode's Add Flag > Airduct (0xe) sets (strategy_mode.h)
}

// Waypoint mode's command for one of the two (true when it was one of them); the rest of the run is AutoPlotCommand_Frame's. pickedModule: the picked module's object index as
// the Entry points markers take it (the view's pick, the selection, the mode's current module), -1 for none
bool AutoPlotCommand(int32_t command, int32_t pickedModule);

// Each frame on the Waypoint screen: the grid's and the worker's progress said, a finished run's networks checked and written
void AutoPlotCommand_Frame();

// A run is going (the grid building or the worker searching)
bool AutoPlotCommand_Busy();

// While a run goes: the busy cursor (the arrow with the hourglass: the editor still answers) set, true; else nothing done, false. The window's WM_SETCURSOR (editor_main.cpp)
// asks it (the owner 11-21, a whole-level plot of about 112 s with nothing shown)
bool AutoPlotCommand_BusyCursor();

// THE CAN CRAWL OFFER's Apply Module Flag (can_crawl_offer.h; the owner 11-21): the Airduct flag on every module of the AI module that holds `module` (any module's object
// index), by the plot's own way (Strategy mode's Add Flag > Airduct, as its Airduct box). flagged: the modules newly flagged; of: its modules; name: its head's name. False when
// the level or its modules can't be read, or the flag could not be set
bool AutoPlotCommand_AirductOn(int32_t module, size_t *flagged, size_t *of, std::string *name);

// Auto-plot these modules' AI modules (any module's object index, each taken to the head of the AI module that holds it, once, in the order given), or EVERY AI module when the
// list is empty: the same run and status line as Auto-plot selected / all (Vasquez's "Alien auto waypoint" menu, the owner 11-20: "a drop down with environment or modules and
// accept a string of multiple modules"). What AutoPlotCommand gives back
bool AutoPlotCommand_Run(std::vector<int32_t> const &modules);

// The same, with the owner's counts for every AI module of the run (autoplot_volumes.h AutoPlotVolumeCounts: climb-ins and get-outs; Vasquez's plot dialog). overwriteLinked:
// the dialog's "Overwrite already linked modules" box (alien_auto_waypoint.h, the owner 11-21; off as the dialog opens): false keeps every AI module of the run that already
// has alien waypoints as it is (not plotted, not asked about; the end line's ", N kept, already linked"); true, the default for every other caller, plots them all as before,
// after the replace question
bool AutoPlotCommand_Run(std::vector<int32_t> const &modules, host::AutoPlotVolumeCounts const &counts, bool overwriteLinked = true);

// The counts of the run going (or the last one): the defaults unless AutoPlotCommand_Run gave them
host::AutoPlotVolumeCounts AutoPlotCommand_RunCounts();

// THE PLOT'S PREVIEW, IN STEP WITH THE PLOT (autoplot_handoff.h; the owner 11-21, "Show every module in order"): the hand-over between the worker and the window.
// The worker hands each AI module over as a constant copy (its search's ways as they are found, then its volumes) and waits until the window has drawn its last
// copy; the window (plot_preview_view.cpp) takes the newest and says when it drew it. The copies are the window's own once taken: nothing of the run's jobs is read.
host::AutoPlotHandOff &AutoPlotCommand_HandOff();
// The preview's drawing time this frame (ms), added up for the whole plot's time line; during a run, it also marks the preview's first frame drawn
void AutoPlotCommand_PreviewDrew(double ms);

// THE START'S MESSAGE (Drake 11-22, alpha.5 item 1; the owner: "screen should disappear instantly ... a big message on the screen 'Loading X%' or 'Calculating
// some stuff' until the little screen shows"): from the click until the preview's first frame, the words of the stage the run is in ("Calculating the alien
// waypoints: the AI modules 37%", "... the grid", "... the doorways 54%", "... the first AI module"); empty otherwise. The panel pass draws them big on a black box
// (editor_panels.cpp; the owner's pick 11-22) and says what it drew (MessageDrawn: "" for none; the box as "left L top T width W height H scale S view ..."), for the
// state line's plotmessage and plotmessagebox. MessageForms: the longest form of each stage's words (100%), the box's width taken from the widest
std::string AutoPlotCommand_Message();
std::vector<std::string> AutoPlotCommand_MessageForms();
void AutoPlotCommand_MessageDrawn(std::string const &words, std::string const &box);
std::string const &AutoPlotCommand_MessageShown();
std::string const &AutoPlotCommand_MessageBox();

// AN EDIT WAITS FOR THE PLOT (autoplot_waits.h): true, and the status line says the item waits, when a run is going and the bar item numbered action would
// change the level or leave the Waypoint screen (an item no bar has waits too); false otherwise, and the item is done as always. editor_main's RunAction asks it first
bool AutoPlotCommand_ItemWaits(int32_t action);

// The same for what reaches the level without the bar (the Waypoint screen's keys, its clicks in the view): true, and the status line says that what waits, while
// a run is going; false when none is. A click waits too: while the plot's inset is drawn, the frame's last view is the inset's, which a pick would read
bool AutoPlotCommand_Waits(char const *what);

// The editor is closing: a run's worker still searching is asked to stop and waited for, before the level and the game go (it stops between two AI modules,
// so this can take as long as one module's search). Nothing happens when no worker runs
void AutoPlotCommand_Stop();
