// all_stock_maps_screen.h: WAYPOINT MODE'S "ALIEN AUTO WAYPOINT" > "ALL STOCK MAPS" in the editor (the owner's wishlist item 5, 2026-11-22: "I want to see it
// working and I want to use it"; he records it for instructions). Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// THE FORM (all_stock_maps_ui.h): the short disclaimer and Environment's full options in one form of inteng's own look. Ok reads the options as Environment
// reads them (AlienAutoWaypoint_ReadPlotForm, AlienAutoWaypoint_PlotCounts) and starts Dietrich's run (all_stock_maps.h AllStockMaps_Start); its refusal
// (unsaved changes, a plot running, AutoPlot off, no data folder) goes on the status line and nothing runs. Cancel changes nothing, and says so.
// WHILE IT RUNS (RECORDABLE, Bishop 2 for the owner): no pop-up or form of ours; each frame the engine's step, then the progress line, drawn on the Waypoint
// screen as the plot's own line is (status_words.h StatusSayer::WaypointPlot): "All stock maps: level 7 of 22, genshd1.RIF: <the plot's own words>". A menu
// item, or a click in the view, that would change the level waits as it waits for a plot (autoplot_waits.h); Esc stops the run after the level it is on.
// AFTER ITS LAST LEVEL: the report, as inteng's text pages (one after another, a press for the next), and its count on the status line.
#pragma once

#include <stdint.h>

namespace host
{
	// Alien auto waypoint > All stock maps: the one form opened (or, while a run goes, the status line says so)
	void AllStockMaps_Open();

	// Every frame, on any screen (editor_panels.cpp, before the text screen is planned): the engine's step, the progress line, and at the run's end the
	// report pages, laid out for inteng's text screen of screenWidth x screenHeight
	void AllStockMapsScreen_Frame(int32_t screenWidth, int32_t screenHeight);

	// A menu item chosen while the run goes (its action number): true when it waits (said on the status line, not done), as autoplot_waits.h rules
	bool AllStockMapsScreen_ItemWaits(int32_t action);

	// Anything else that would change the level while the run goes ("A click in the view"): true when it waits (said on the status line)
	bool AllStockMapsScreen_Waits(char const *what);

	// Esc while the run goes: it stops after the level it is on (all_stock_maps.h AllStockMaps_Stop); true when Esc was taken
	bool AllStockMapsScreen_Esc();
}
