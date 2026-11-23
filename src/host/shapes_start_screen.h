// shapes_start_screen.h: SHAPES PART 3 in the editor: the FOUND form at the start, the player's maps searched one a frame, the list written, the level
// opened again. Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng, on Bishop 2's order (alpha.6), to Drake's spec
// (AvP-docs/director/plan-first-launch-shapes.md, and his answers of 11-23). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// AT THE START, once the level is open (editor_main.cpp): the list of maps already searched is read from AVP_OpenInteng.ini (shapes_scanned.h), and the maps
// found (shapes_start.h ShapesStart_MapsFound).
//   - NO LIST YET (the player never said Yes): when there is something to search (shapes_start.h ShapesStart_ShouldAsk), the FOUND form opens over the level
//     at the first frame it can (Drake's Dialogs_Form, first_launch_ui.h: where Open Inteng is, where it belongs, what it found, "Make them now?"). Yes searches
//     every map; Not now searches nothing and the form asks again at the next start (the owner: "Ask again next start").
//   - A LIST (Yes was said once): every map not on it is searched, without asking (the owner: "whenever any new map not in its ini is found it'll scan it too").
// EACH FRAME, ONE MAP (Drake's trigger, shapes_trigger.h), so the editor keeps drawing: read once and hashed, those very bytes searched (shapes_start.h), new
// shapes written CREATE_NEW into <game>\shape_rifs. A level the editor opens later joins the search. AT THE END the list is written into the ini (every other
// byte kept, first_launch_screen.h FirstLaunch_WriteIni), the status line says it ("Shapes: 2 new maps scanned, 5 new shapes in shape_rifs"), and after the
// first search that wrote any shape the open level is opened again, so its new shapes are drawn (Drake's condition; the editor's reopen checks that nothing
// runs on it and nothing is unsaved). AT THE CLOSE a list that changed since is written too, so an editor closed mid-search keeps what it searched.
// A HIDDEN TEST RUN does none of this unless its ini says ShapesInTests=1 under [Editor] (read only in a hidden run: a player's ini can't switch it).
#pragma once

#include <functional>
#include <string>

namespace host
{
	struct ShapesStartSetting
	{
		std::string programFolder;   // the editor's own folder
		std::string dataFolder;      // the checked data folder (the one holding avp_rifs)
		std::string settingsFile;    // AVP_OpenInteng.ini beside the program
		bool fromIni = false;        // the data folder is the one DataFolder= names (said so on the form)
		bool on = true;              // false: nothing at all (a hidden run without ShapesInTests=1, or a start with no level)
		std::function<void(std::string const &)> say;   // the status line
		std::function<void()> reopen;                   // the open level opened again (the editor's: it checks nothing runs and nothing is unsaved)
	};

	// At the start, once its level is open
	void ShapesStartScreen_Begin(ShapesStartSetting const &setting);

	// Every frame (editor_panels.cpp): the form opened once, then one map searched; at the end, the list written and said
	void ShapesStartScreen_Frame();

	// A level the editor opened (its file as the game read it, level_view.h LevelView_FilePath): searched too, once a list is kept or Yes said
	void ShapesStartScreen_LevelOpened(std::string const &path);

	// At the editor's close: the list written when it changed since it was last written
	void ShapesStartScreen_AtClose();

	// The state line's "shapes" field: "off", "nothing to search", "asking", "asking (the form is open)", "not now", "searching <taken> of <found>",
	// "done; <n> new maps scanned, <m> new shapes in shape_rifs" (this run's counts)
	std::string ShapesStartScreen_StateWords();
}
