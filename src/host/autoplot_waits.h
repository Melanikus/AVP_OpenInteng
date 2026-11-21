// autoplot_waits.h: AN EDIT WAITS FOR THE PLOT. While Alien auto waypoint plots (the owner 11-21, choosing "Full map + inset on top": the main view keeps drawing
// and the camera stays usable; Bishop's spec: "Edits that would touch the level being written wait until the plot ends, and the status line says so"), a menu
// item that would change the level, or leave the Waypoint screen (the plot goes on only there, autoplot_command.h), is not done: the status line says it waits
// for the plot's end, and it can be chosen again then. The items that only look are done as always: the view's draw modes and toggles, the keys' help, a
// picture, the text size and the options kept in the settings file. Quit is done too: the plot is stopped and waited for first (AutoPlotCommand_Stop).
// Pure: the rule by the item's kind (inteng_menus.h IntengWork), and the status line's words.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <string>

#include "inteng_menus.h"

namespace host
{
	// True when an item of this kind waits while a plot runs: every kind but the ones that only look (and Quit). A kind this list does not know waits (a new
	// kind added later is taken as one that changes the level until someone says otherwise here). None ("the editor does it") waits too: it is many items.
	bool AutoPlot_ItemWaits(IntengWork work);

	// The status line's words for an item that waits: its name, and that it waits for the plot's end
	std::string AutoPlot_WaitWords(char const *itemName);
}
