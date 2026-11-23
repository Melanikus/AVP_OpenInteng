// all_stock_maps.h: Waypoint > Alien auto waypoint > All stock maps, its engine. Every Marine and Predator campaign level is checked, backed up,
// opened, given the alien auto waypoints on every AI module, checked again, saved, and closed, ONE AT A TIME: nothing of the next level starts
// before this one is saved and closed. Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng (the owner: "Priority is my waypoint all
// maps, I want to see it working and I want to use it"). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE PLAN it follows is Vasquez's (AvP-docs director/plan-all-stock-maps.md, from her code map of bishop-int d2d2d909): the 22 levels from
// env_info.cpp's own tables; each file opened without following a link and judged on its handle; <name>.rif.bak written CREATE_NEW, never
// replacing one there (the oldest copy is always kept); the plot never asks its own Yes/No (it would act on whatever level is open at Yes); the
// file re-checked right before the save; the save through LevelSave_Over with no backup name of its own. The owner: "Include them, back up":
// a level that isn't byte for byte Steam's is still done, and only named so in the report.
//
// IT WRITES THE DATA FOLDER'S OWN LEVEL FILES (and their .rif.bak), where a hand Save of a level opened from a copy writes the copy: run it on a
// private data copy in a test (run-shards refuses a DataFolder outside its shard), never on the owner's game folder unless he starts it.
//
// THE LOADER'S LOG: a level the game's loader refuses, or loads with something left out, gets <name>_errorlog.txt beside it (avpx/3dc/
// level_log.h). One that this run's open made is moved into the editor's own folder, logs/all-stock-maps (never over a file there) and named in the
// level's why; one there before the run is left alone (Bishop 2: never left in avp_rifs).
//
// WHO DOES WHAT: this engine is Dietrich's (src/host/all_stock_maps_host.cpp, and the close and open of a level in the same sitting,
// src/host/level_reopen.cpp). The menu item, the owner's disclaimer, the progress line and the end report are Drake's; he calls these.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autoplot_volumes.h"

namespace host
{
	enum class StockMapOutcome
	{
		Changed,      // plotted and saved (with its new .rif.bak, or the one there before kept)
		Skipped,      // not touched: missing, a link or a folder, two names for one file, too large, or unreadable (why says which)
		Refused,      // not touched: the backup couldn't be written, the file changed on disk while open, or the save refused it (why)
		Failed,       // the plot said it couldn't, or the level couldn't be opened: not saved, untouched apart from its new .rif.bak (why)
		NotStarted    // the run was stopped before this level
	};

	struct AllStockMapsLevel
	{
		std::string file;          // the level's file as built: env_info's level name and ".RIF", "derelict.RIF"
		StockMapOutcome outcome = StockMapOutcome::NotStarted;
		bool backupMade = false;   // a new <file>.bak was written by this run, holding the bytes as they were before it
		bool backupKept = false;   // a <file>.bak was there already and is left exactly as it was (the oldest copy is always kept)
		int32_t steamsOwn = -1;    // the file before the run was byte for byte Steam's: 1 yes, 0 no ("not Steam's own"), -1 not known (Stage 2)
		std::string loaderLog;     // the game loader's <name>_errorlog.txt this run's open left, moved out of avp_rifs to here ("" none)
		std::string why;           // the plot's own result ("N AI modules written, M kept, already linked") or the reason it wasn't done
	};

	struct AllStockMapsProgress
	{
		size_t level = 0;          // 1 to levels: the one being worked; 0 before the first
		size_t levels = 0;
		std::string file;
		std::string stage;         // "checking", "backing up", "opening", the plot's own progress words, "saving", "closing"
		bool stopping = false;     // Esc was pressed: this level ends whole, then the run stops
	};

	// Why the run can't start now, in words, or empty when it can: a run is going, there is no data folder, a plot, AutoLink or Delete Links is
	// running, AutoPlot isn't on in the ini (the plot would do nothing), or it isn't the Waypoint screen in its first mode; and, unless
	// changesAllowed, the open level has changes not saved. HOW THE LEVEL WAS OPENED IS NO REASON (the owner's first real test, 2026-11-23,
	// Bishop 2's relay: his level, double-clicked, was refused as "opened with Open with", the most common way in): see AllStockMaps_Start.
	std::string AllStockMaps_WhyNot(bool changesAllowed);

	// The open level has changes not saved (the level file's own record of its edits, level_file.h HasChanges): the form then asks "Save your
	// level first?" before the run (all_stock_maps_ui.h AllStockMaps_SaveFirstForm), never a dead end
	bool AllStockMaps_ChangesNotSaved();

	// Starts the run, nothing loaded yet. Refused (false, *why) for AllStockMaps_WhyNot's reasons; changesAllowed is the question's No ("run
	// without saving it"): the level's changes not saved are let go when the first level opens. counts: the plot's counts and its
	// airduct box (autoplot_volumes.h), the same for every level. overwriteLinked: the form's "Overwrite already linked modules" tick (the owner:
	// "Give them the full options"): off keeps every AI module that already has alien waypoints (Rebellion's hand-made ones) as it is; on
	// replaces them, and the tick IS the answer, so the plot's own replace question never opens (its callback would act on whatever level is
	// open at Yes). Either way each level's .rif.bak holds what it had before.
	// THE LEVEL OPEN BEFORE, at the end: opened again by its name when it was opened by its name; one opened with "Open with" (a double-click
	// on a .rif anywhere: the editor works on its copy in avp_rifs\OpenedByEditor, and its Save writes the file where it really is) is NOT
	// opened again, as its name would open another file and its Save would lose where it came from: the report says so, naming the file.
	// THE CAMERA (the owner, 2026-11-23, who records the run: "It will preserve my camera angle and position from origin across level loads?"):
	// its place and its turn, look and roll as the run starts are put back on every level as it opens, before its plot draws, and once more at
	// the end. The place is in the game's units from the world's origin, which is every level's own (0, 0, 0), so each level is seen from the
	// same place and the same way.
	bool AllStockMaps_Start(host::AutoPlotVolumeCounts const &counts, bool overwriteLinked, bool changesAllowed, std::string *why);

	// A step each frame (the run moves only on the Waypoint screen in its mode 0, where the plot moves): the editor never freezes.
	void AllStockMaps_Frame();

	bool AllStockMaps_Busy();

	// Esc: between levels the run stops at once; during a plot that level ends whole (plotted, checked, saved), then it stops. Never half a level.
	void AllStockMaps_Stop();

	AllStockMapsProgress AllStockMaps_Progress();

	// Every level of the run, in its order, the ones not reached NotStarted. Whole once Busy() is false.
	std::vector<AllStockMapsLevel> AllStockMaps_Report();

	// The 22 level files, Marine's campaign then Predator's, from env_info.cpp's tables (Campaign_LevelCount, Campaign_Environment,
	// SetLevelToLoad), as the game names them. The run uses exactly these.
	std::vector<std::string> AllStockMaps_LevelFiles();

	// The harness's state field allstockmaps (Frost's acceptance): busy or idle, level N of M, the file, the stage, the last level's verdict, and
	// the worst frame time while the current level's plot ran (R3)
	std::string AllStockMaps_StateWords();
}
