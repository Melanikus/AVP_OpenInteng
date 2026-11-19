// autoplot_switch.h: THE OWNER'S RULING 11-19, "hide auto pilot" (waypoints are on hold): Waypoint mode's two Auto-plot items, "Auto-plot AI module" (0x7330) and "Auto-plot all AI
// modules" (0x7331, autoplot_command.h), are not on the bar or the pop-up, and their command does nothing, unless AVP_OpenInteng.ini says AutoPlot=1 under [Editor]. The shipped ini
// (dist\AVP_OpenInteng.ini) never says it; the autoplot acceptances set it with their "#ini AutoPlot=1" line, so the code they test stays as it is and keeps running.
// The menus are generated (make-inteng-menus.js from menu-status.json), so the items stay in the generated tables and the bar leaves them out as it is built (editor_bar.cpp).
// Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include "autoplot_command.h"

namespace host
{
	// The switch, read once at start-up from the ini (editor_main.cpp): off unless AutoPlot=1
	inline bool &AutoPlotShownFlag()
	{
		static bool shown = false;
		return shown;
	}

	inline void AutoPlot_SetShown(bool shown)
	{
		AutoPlotShownFlag() = shown;
	}

	inline bool AutoPlot_Shown()
	{
		return AutoPlotShownFlag();
	}

	// The rule, a plain sum: an item of inteng's tables (its command number, IntengItem::command) is left out while the switch is off when it is one of the two
	inline bool AutoPlot_CommandHidden(int32_t command, bool shown)
	{
		return !shown && (command == kWaypointAutoPlotModule || command == kWaypointAutoPlotAll);
	}
}
