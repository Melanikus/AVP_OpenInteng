// alien_auto_waypoint.h: THE OWNER'S "ALIEN AUTO WAYPOINT" MENU (Melanikus 2026-11-20, through Bishop: "I want the auto waypoint to say "Alien auto waypoint" and be a drop down with
// environment or modules and accept a string of multiple modules"). Waypoint mode's first bar has ONE menu of ours, "Alien auto waypoint", in place of the three Auto-plot items:
//   - "Environment" (0x7335): every AI module of the level;
//   - "Modules" (0x7334): the names of SEVERAL modules, spaces (or commas) between them, started with the Ctrl+click set's names (else the picked module's).
//   Both ask, in a dialog of inteng's own parts, how many CLIMB-INS and GET-OUTS each AI module gets (the owner's tunable counts 11-20, autoplot_volumes.h: started with the
//   planner's defaults) and whether the plotted modules get AIRDUCT (a tick box, unticked as it opens: the owner 11-20 19:06, "Let's not auto set it anymore"), and Ok runs Dietrich's planner (AutoPlotCommand_Run). A name the level has no module for, one that is not a module, or a count out of range, is NAMED
//   on the status line and NOTHING is plotted.
// Both stay hidden unless AutoPlot=1 (autoplot_switch.h, Drake's). This file is the pure part: the typed words to module object indices, and the start words. Written 2026-11-20 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOSTILE INPUT: the typed line and the level's names (a name may be empty, long, of any bytes, or two objects may share one). The line is read up to kAlienAutoWaypointMostLetters
// letters; a word over kAlienAutoWaypointMostWordLetters letters is refused by name (cut, with "..."), and at most kAlienAutoWaypointMostRefusalsNamed refusals are named (the
// rest counted). A name holding a space or a comma is typed between double quotes ("Object 16"); the start words quote such a name; an open quote runs to the end of the line.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "autoplot_volumes.h"
#include "dialog_box.h"

namespace host
{
	int32_t const kWaypointAlienAutoModules = 0x7334;          // ours: "Alien auto waypoint" > "Modules"
	int32_t const kWaypointAlienAutoEnvironment = 0x7335;      // ours: "Alien auto waypoint" > "Environment" (the counts asked, then every AI module; 0x7331 stays the bare all-run)
	size_t const kAlienAutoWaypointMostLetters = 2000;         // the line dialog's edit takes at most this many letters
	size_t const kAlienAutoWaypointMostWordLetters = 64;       // a typed word longer than this is refused (no module name is so long)
	size_t const kAlienAutoWaypointMostRefusalsNamed = 8;      // the status line names at most this many refused words, and counts the rest

	// One object of the level as the parse needs it: its name, its object index, and whether it is a module
	struct AlienAutoWaypointObject
	{
		std::string name;
		int32_t index = -1;
		bool module = false;
	};

	// The typed line's words to module object indices, in the order typed, each once. A word matches the object with exactly that name; else the one object whose name is the same
	// with the case ignored. True when every word names a module (*modules holds them); false with *refusal naming each word that does not: "no module named X", "X is not a module",
	// "X names N objects" (two or more objects share it with the case ignored and none is it exactly), "X is too long", or "no module names typed" for a line with none. On false,
	// *modules is empty: nothing is plotted.
	bool AlienAutoWaypoint_Parse(std::string const &typed, std::vector<AlienAutoWaypointObject> const &objects, std::vector<int32_t> *modules, std::string *refusal);

	// The words the Modules dialog starts with: each module's name (from `names`, by object index) in the order given, a space between, quoted when it holds a space, a comma or a
	// double quote (a double quote inside is left out), at most kAlienAutoWaypointMostLetters letters (whole names only). A module with no name, or not in `names`, is left out
	std::string AlienAutoWaypoint_StartWords(std::vector<int32_t> const &modules, std::map<int32_t, std::string> const &names);

	// A COUNT typed in the dialog (the owner 11-20, through Bishop: "climb-ins and get-outs", the plot dialog's tunable counts): the field's words to a whole number from `least`
	// to `most`. Spaces round it are fine; nothing typed (or only spaces) is `fallback` (the planner's default). False, with *why naming the field by `label` and saying what
	// was typed (its printable letters, cut), for anything else: letters, a sign, a fraction, a number outside the range, more than 9 digits. *value is set only on true
	bool AlienAutoWaypoint_ParseCount(std::string const &typed, int32_t least, int32_t most, int32_t fallback, char const *label, int32_t *value, std::string *why);

	// THE PLOT DIALOG, from inteng's own parts: the 400 x 250 window of its TEXT dialog (FUN_00474350, dialog_box.h) with its words at (0x12, 0x12), Ok at (100, 0xbe) and
	// Cancel at (200, 0xbe). Modules' names go in Write asc file's long edit (300 wide, at (0x1e, 100)). Each count is a TEXT field (0x96 x 0x12) with its label to the
	// right at x 0xbe, started with the planner's default. Under the two counts is the AIRDUCT TICK BOX (the owner 11-20 night; unticked by default since his 19:06):
	// inteng's own check box (FUN_0057d670, 12 x 12) at the fields' x, with its label to its right, as Path's Loop box has (path_screen.cpp). It is UNTICKED as the dialog opens (AutoPlotVolumeCounts().airduct).
	// withNames: Modules' dialog, started with `names`; false: Environment's (the counts and the box only)
	DialogFormSpec AlienAutoWaypoint_PlotForm(bool withNames, std::string const &names);

	// What the plot dialog holds when Ok is pressed: the typed words as they are, and the tick
	struct AlienAutoWaypointPlotAnswers
	{
		std::string names;      // Modules' names (empty for Environment)
		std::string climbIns;   // the climb-ins field's words
		std::string getOuts;    // the get-outs field's words
		bool airduct = false;   // the Airduct box ticked
	};

	// The plot dialog's parts (as Dialogs_Form gives them back on Ok) read into *answers. False when the parts are not the dialog's own: fewer or more fields than it has, or
	// no check box, or more than one (nothing is plotted then)
	bool AlienAutoWaypoint_ReadPlotForm(bool withNames, std::vector<DialogWidget> const &widgets, AlienAutoWaypointPlotAnswers *answers);

	// The answers' two counts (AlienAutoWaypoint_ParseCount, in the planner's ranges, empty is the default) and the tick, into the planner's counts. False, with *why naming the
	// field, when a count is refused (*counts is then not changed)
	bool AlienAutoWaypoint_PlotCounts(AlienAutoWaypointPlotAnswers const &answers, AutoPlotVolumeCounts *counts, std::string *why);

	// THE EDITOR'S SIDE (alien_auto_waypoint_editor.cpp, not in the pure tests): Waypoint mode's command for Modules (true when it was it): the line dialog, then the run on Ok.
	// pickedModule: the picked module's object index as the Entry points markers take it, -1 for none (the start words when there is no Ctrl+click set)
	bool AlienAutoWaypointCommand(int32_t command, int32_t pickedModule);
}
