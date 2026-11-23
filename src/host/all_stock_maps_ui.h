// all_stock_maps_ui.h: "ALL STOCK MAPS", ITS WORDS, ITS FORM, ITS PROGRESS LINE AND ITS REPORT (the owner's wishlist item 5, 2026-11-22: "on alien waypoint
// mode autolink we'll have a last option "All stock maps""; "Priority is my waypoint all maps, I want to see it working and I want to use it"; on the form,
// "Give them the full options bellow our disclaimer, make our disclaimer shorter"). Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files, no editor state.
//
// Waypoint mode's "Alien auto waypoint" menu gets a last item, "All stock maps" (kWaypointAlienAutoAllStockMaps), after Environment and Modules. It opens
// ONE FORM in inteng's own look: the SHORT disclaimer on top, then Environment's FULL options below it, exactly its own fields with their defaults
// (alien_auto_waypoint.h AlienAutoWaypoint_PlotForm, moved down under the words), then Ok and Cancel. Ok starts Dietrich's run (all_stock_maps.h: the 22
// Marine and Predator campaign levels, one at a time, each backed up first as <level>.rif.bak, an existing backup never replaced); Cancel changes nothing.
// While it runs, its progress line ("All stock maps: level 7 of 22, genshd1.RIF: <the plot's own words>"); after its last level, its report as inteng's
// text pages: CHANGED (each with its backup, and "not Steam's own" when it wasn't), SKIPPED, REFUSED, FAILED, NOT STARTED, and how to undo a level.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "all_stock_maps.h"
#include "dialog_box.h"

namespace host
{
	int32_t const kWaypointAlienAutoAllStockMaps = 0x7338;   // ours: "Alien auto waypoint" > "All stock maps" (0x7334 to 0x7337 are Modules, Environment, Delete
	                                                         // Waypoints and Delete All Waypoints)

	// The form's title and its short disclaimer (Bishop 2's short draft for the owner, with the run's own count and time)
	extern char const kAllStockMapsTitle[];
	extern char const kAllStockMapsWords[];

	// THE ONE FORM: `options` is Environment's own form (AlienAutoWaypoint_PlotForm(false, "")); its parts are moved down under the disclaimer's words,
	// which are wrapped to its width by widthOf (a letter's width as drawn), a line fontHeight high, and put first. Its title becomes kAllStockMapsTitle.
	// A KNOWN LIMIT (Bishop 2, 2026-11-22): the form needs a window at least 600 pixels tall. Environment's own parts take 454 of them and the words about
	// 140 more (594 at 16-pixel lines, host_all_stock_maps_ui_test), so on the smallest window, 640 x 480, its top 57 pixels are cut (the title and the
	// first lines of the words; Ok and Cancel stay on screen). Environment's parts are kept exactly inteng's rather than squeezed; the shipped ini starts
	// the editor at 1280 x 720, where it fits with room
	DialogFormSpec AllStockMaps_Form(DialogFormSpec const &options, std::function<int32_t(unsigned char)> const &widthOf, int32_t fontHeight);

	// "SAVE YOUR LEVEL FIRST?" (Bishop 2, 2026-11-23, after the owner's first real test: "Unsaved changes: ask once, plain words ... never a dead
	// end"): asked after Ok when the open level has changes not saved and nothing else stops the run. Inteng's own Yes / No box (400 x 200, its
	// words at 0x12, 0x12, its buttons 0x32 x 0x10 at 0x96 down) with a third button: Yes (kDialogOk, and Return) saves the level, then runs;
	// No (kDialogNo) runs without saving it; Cancel (kDialogEscape, and Esc) runs nothing. The words are wrapped by widthOf to the box, a line
	// fontHeight high; a taller box is made when they need it, so the buttons are always under them
	extern char const kAllStockMapsSaveFirstWords[];
	DialogFormSpec AllStockMaps_SaveFirstForm(std::function<int32_t(unsigned char)> const &widthOf, int32_t fontHeight);

	// The form's parts as Dialogs_Form gives them back, without the disclaimer's words: Environment's own parts, for AlienAutoWaypoint_ReadPlotForm. Empty
	// when the first part isn't the disclaimer's (then nothing is read, and nothing runs)
	std::vector<DialogWidget> AllStockMaps_OptionsOf(std::vector<DialogWidget> const &widgets);

	// A PATH KEPT WHOLE BY THE WRAP (Drake 2026-11-23, the owner's "Full paths here" for the first-start form): words may mark a path by putting
	// kWrapPathStart before it and kWrapPathEnd after it. The wrap then breaks that path only right after one of its \ or /, never at a space inside it
	// ("John Smith") and never inside a name, and takes the marks out. Neither can come from a name: the safe text rule (PanelPaint_SafeText) makes
	// every byte outside the space to the tilde a '?'. Words with no marks wrap exactly as they did before.
	char const kWrapPathStart = '\x01';
	char const kWrapPathEnd = '\x02';

	// The words with the marks above taken out (what the log and the tests read)
	std::string AllStockMaps_Unmarked(std::string const &words);

	// The words wrapped to `width` pixels: lines separated by "\n", a paragraph's break kept as an empty line; a line ends at a space, or inside a marked
	// path right after a \ or /; only a word, or a path's part, wider than the whole line is cut inside
	std::string AllStockMaps_Wrapped(std::string const &words, std::function<int32_t(unsigned char)> const &widthOf, int32_t width);

	// The progress line while it runs: "All stock maps: level 7 of 22, genshd1.RIF: <stage>", and "; stopping after this level (Esc)" once Esc was pressed
	std::string AllStockMaps_ProgressWords(AllStockMapsProgress const &progress);

	// Its words before the stage, "All stock maps: level 7 of 22, genshd1.RIF: " ("All stock maps: level 16 of 22: " with no file): the status line's LEAD for
	// the plot's own words while the run goes (status_words.h; Dietrich 2026-11-23, the alpha.6 cut), so a line the plot says reads "All stock maps: level 17 of 22,
	// battle.RIF: Auto-plot: module 69 of 120 ...". AllStockMaps_ProgressWords always starts with it (but with no stage, its last ": " left off)
	std::string AllStockMaps_ProgressLead(AllStockMapsProgress const &progress);

	// THE END STAGE (endStage: AllStockMaps_Progress().stage once the run is over): plain "done", or empty, says nothing more; anything else (the engine's
	// "done, but the level open before, X, could not be opened again: <why>", Vasquez's review: the editor may end holding no level) is said in all three
	// below, as the safe text rule makes it, never left silent

	// The status line's words once it ended: "All stock maps: 21 of 22 levels changed, 1 failed (the report is open)"; with an end stage, it before the
	// report's note: "All stock maps: 22 of 22 levels changed; done, but the level open before, ... (the report is open)"
	std::string AllStockMaps_EndWords(std::vector<AllStockMapsLevel> const &report, std::string const &endStage = std::string());

	// The report for the editor's log (Vasquez's plan: "The same report also goes to the editor's log"): its count first ("All stock maps: the run
	// ended: 21 of 22 levels changed, 1 failed"), then one line a level, in the run's order, each "All stock maps: CHANGED derelict.RIF: <why>; backup
	// derelict.RIF.bak made", the heading CHANGED, SKIPPED, REFUSED, FAILED or NOT STARTED. Every name and reason as the safe text rule makes it
	// (with an end stage, a last line "All stock maps: <the end stage>")
	std::vector<std::string> AllStockMaps_LogLines(std::vector<AllStockMapsLevel> const &report, std::string const &endStage = std::string());

	// The report as inteng's text pages: wrapped by widthOf to screenWidth, cut into pages of linesPerPage lines, each page but the last ending
	// "(more on the next page)"
	// (with an end stage, right under the count: "AT THE END: <the end stage>.")
	std::vector<std::string> AllStockMaps_ReportPages(std::vector<AllStockMapsLevel> const &report, std::function<int32_t(unsigned char)> const &widthOf,
		int32_t screenWidth, size_t linesPerPage, std::string const &endStage = std::string());
}
