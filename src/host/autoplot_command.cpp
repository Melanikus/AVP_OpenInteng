// See autoplot_command.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The includes are construction_modes_screen.cpp's (editor_shared.h needs them).

#include "3dc.h"
#include "inline.h"
#include "gameplat.h"
#include "platform.h"
#include "renderer.h"
#include "d3_func.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>   // SHGetFolderPathA, CSIDL_PERSONAL: the user's Documents folder, the folder for copies (save_copy.h)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <map>
#include <array>
#include <functional>
#include <math.h>
#include <string>

#include "logString.h"
#include <ctype.h>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "inteng_menus.h"
#include "level_file.h"
#include "field_descriptor_from_model.h"
#include "light_edit.h"
#include "light_paste.h"
#include "light_intensities_edit.h"
#include "light_state.h"
#include "light_pick.h"
#include "level_objects.h"
#include "level_view.h"
#include "uv_project.h"
#include "hierarchy_player.h"
#include "hierarchy_view.h"
#include "object_edit.h"
#include "level_chunks.h"
#include "asc_text.h"
#include "module_links.h"
#include "object_place_edit.h"
#include "object_edit.h"
#include "object_make.h"
#include "object_base.h"
#include "object_type_edit.h"
#include "strategy_mode.h"
#include "strategy_views.h"
#include "small_modules.h"
#include "alt_locations_edit.h"
#include "quick_create.h"
#include "level_store.h"
#include "entry_recalc.h"
#include "record_edit.h"
#include "save_copy.h"
#include "level_save.h"
#include "nobody_watching.h"
#include "key_reader.h"
#include "one_editor.h"
#include "utilities.h"     // SaveFolder_UseThisFolder (SAVE-FOLDER-1)
#include "save_folder.h"   // SaveFolder_WhyNotUsable, for DataFolder too
#include "editor_actions.h"
#include "hidden_objects.h"
#include "poly_counts.h"
#include "path_mode.h"
#include "virtual_object.h"
#include "strategy_edit.h"
#include "strategy_lookup.h"
#include "pick_centre.h"
#include "vertex_pick.h"
#include "editor_bar.h"
#include "inteng_help.h"
#include "click_mode.h"
#include "screenshot_name.h"
#include "shot_context.h"       // #78: what a screenshot says about itself
#include "png_meta.h"           // #78: ... inside its PNG (Dietrich)
#include "log_files.h"          // #78: the log's own way of leaving the player out
#include "place_at_view.h"
#include "tool_buttons.h"
#include "file_pick.h"
#include "move_rel_poly.h"
#include "move_rel_poly_edit.h"
#include "shape_edit.h"
#include "uv_edit.h"
#include "anim_edit.h"
#include "texture_keys.h"
#include "topology_match.h"
#include "texture_anim.h"
#include "polygon_items.h"
#include "module_poly_count.h"
#include "hierarchy_mode.h"
#include "list_picker.h"
#include "dialog_box.h"
#include "dialogs.h"
#include "camera_file.h"
#include "strategy_repair.h"
#include "special_mode.h"
#include "generator_settings.h"
#include "bad_guy_edit.h"
#include "particle_edit.h"
#include "simple_object_edit.h"
#include "editor_icons.h"
#include "inteng_exe.h"
#include "inteng_shapes.h"
#include "virtual_object.h"
#include "generator_strategy.h"
#include "placed_hierarchy_strategy.h"
#include "fix_tears_all.h"
#include "shape_recalc_edit.h"
#include "shape_replace_edit.h"
#include "adjacent_list.h"
#include "adjacent_write.h"
#include "screen_keys.h"
#include "entry_markers.h"
#include "light_keys.h"
#include "reverse_links.h"
#include "autolink_edit.h"
#include "autolink_runner.h"
#include "autolink_scene.h"
#include "module_links_clean.h"
#include "autolink_mlf.h"
#include "adjacency_generate.h"
#include "waypoint_mode.h"
#include "waypoint_view.h"
#include "track_pages.h"
#include "editor_options.h"
#include "shape_chooser.h"
#include "shape_rescale_edit.h"
#include "module_site.h"
#include "shape_soften_edit.h"
#include "module_data_edit.h"
#include "acoustic_mode.h"
#include "shape_merge_edit.h"
#include "poly_edit.h"
#include "module_overlaps.h"
#include "track_edit.h"
#include "sound_edit.h"
#include "placed_sounds_game.h"
#include "track_editor.h"
#include "track_play_game.h"
#include "three_view.h"
#include "field_adapters.h"
#include "field_panel_host.h"
#include "harness_select.h"
#include "key_layout.h"
#include "host_frame.h"
#include "menu_bar.h"
#include "menu_paint.h"
#include "menu_font.h"
#include "object_list.h"
#include "panel_layout.h"
#include "panel_pass.h"
#include "autolink_show.h"
#include "autolink_show_draw.h"   // AutoLink's display: the counter, "Right click to cancel", the owner's grid (reassess/autolink-display-frost.md)
#include "inteng_text.h"          // FUN_0057be40's new lines, tabs and wrapping (view-state-table.md H-1f)
#include "inteng_options.h"       // the start size: -r, IntengOptions.ini, the monitor (startup-screen-frost.md SS-1..SS-13)
#include "texture_window_paint.h"
#include "screen_picture.h"
#include "pick_game.h"
#include "selection.h"
#include "view_surface.h"
#include "view_toggles.h"
#include "view_toggles_game.h"
#include "window_events.h"
#include "open_with.h"

#include "editor_shared.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#include "autoplot_command.h"

// the last run's end, for AutoPlotCommand_LastResult (autoplot_command.h)
static AutoPlotResult gLast;
#include "autoplot_preview.h"
#include "autoplot_waits.h"   // AN EDIT WAITS FOR THE PLOT: which bar items wait while a run goes
#include "autoplot_switch.h"   // the owner's "hide auto pilot" (11-19): nothing done unless AutoPlot=1
#include "autoplot_arrival.h"
#include "autoplot_closers.h"   // the gap closers stop a body under each final aim point (Vasquez 11-21)
#include "autoplot_handoff.h"
#include "autoplot_progress.h"
#include "alien_auto_waypoint.h"   // the Overwrite box's keep (AlienAutoWaypoint_SplitLinked) and its end-line words (Vasquez 11-21)
#include "autoplot_grid.h"
#include "autoplot_lane.h"
#include "autoplot_plan.h"
#include "autoplot_repair.h"
#include "autoplot_shortcut.h"
#include "autoplot_volume_repair.h"
#include "autoplot_volumes.h"
#include "waypoint_net_check.h"

namespace
{
	struct Job
	{
		int32_t head = -1;                          // the AI module: its master's (or a lone module's) object index
		std::vector<int32_t> gameModules;           // the game's numbers of its modules (MODULE::m_index)
		std::vector<std::vector<int32_t> > exitsWorld;   // its exit points, the world's mm (for the generator)
		std::vector<host::WaypointPoint> exitsFile;      // the same in the file's units (for the checker)
		std::vector<std::vector<int32_t> > goals;   // exits and stand spots, the world's mm
		host::AutoPlotPlan plan;
		bool planned = false;
		bool notShaped = false;                     // the volume planner has no rule for its kind yet: NO network written (Bishop 11-20: never the old small boxes), named
		std::string why;                            // the worker's own failure, if any
		std::map<std::pair<size_t, size_t>, std::pair<bool, bool> > exitWays;   // each pair of exits (a < b): the alien search found a way, the ground search did
		std::vector<std::string> pairLines;         // one line a pair: what the searches found and what the plan joins (the report's; Frost's floor-route gate)
		std::vector<int32_t> exitTo;                // THE LANES: each exit's neighbouring AI module (its head), -1 when not known
		std::vector<host::AutoPlotDoor> doors;      // each exit's doorway: its middle and way through (from the neighbour's exit back), its soffit (the grid's)
		std::vector<std::vector<int32_t> > alienGoals;   // the goals the alien search goes between: an exit's soffit where it has one (the lanes run soffit to soffit)
		std::vector<std::vector<int32_t> > backs;   // each exit: the neighbour's exit point back (inside this AI module: where an alien from there lands), empty when not known
		bool written = false;
		std::string name;                           // its head's name, for the progress words (taken as the run begins, so the frame never looks it up)
		size_t stockAliens = 0;                     // Rebellion's own alien waypoints it held as the run began (not written: it keeps them; the end line's "kept Rebellion's")
		double searchMs = 0.0, planMs = 0.0, writeMs = 0.0;   // THE RUN'S TIME (Bishop 11-21): the worker's searches, its plan and repair, the check and write
		size_t searches = 0, looks = 0, mostLooks = 0;        // THE RUN'S WORK (11-23): its way searches, the cells they looked at, the most in one (AutoPlotGrid_Way)
		double checkMs = 0.0, levelWriteMs = 0.0;             // the check and write's two parts: the checker (Waypoints_Check), its share of the level's one write
		// THE ONE WRITE (Drake 11-21): the checker's findings, made before the write (ToWrite), and what the one write of every passed network did for this one
		std::vector<host::WaypointProblem> problems, excepted;
		bool inWrite = false, levelWritten = false;
		std::string writeWords;
		std::vector<int32_t> members;               // its modules' object indexes (the Airduct flag goes on each)
		host::AutoPlotVolumeCounts counts;          // the run's tunables (autoplot_volumes.h), taken as the run begins
		std::vector<int> ceilingPast;               // OPEN SKY PER DOORWAY (the owner 11-20): each exit, 1 a ceiling over the floor just past the doorway (the lane must go on), 0 none (it may end), -1 not known
		std::vector<int> ceilingHere;               // the same on this side of the doorway (no ceiling here: no lane can start, so no lane end)
		size_t floorCellsSeen = 0, floorCellsCeiled = 0;   // OPEN SKY (the owner 11-20: "Stop only at exits modules with open sky"): its floor cells, and those under a ceiling of its own
	};

	// ONE-WAY DOORWAYS (autoplot_arrival.h; Vasquez 11-20): the first exitsFile.size() exits are the level's own; any after them in exitsWorld (and exitTo, doors, backs) are
	// ARRIVALS, where another AI module's aliens come in through a doorway this one has no exit back through. No alien here heads for one: never a lane's A side nor its target
	bool IsArrival(Job const &job, size_t e)
	{
		return e >= job.exitsFile.size();
	}

	enum class Stage
	{
		Idle,
		Grid,        // the pathfinder's grid building (autoplot_grid.h, its own thread)
		Searching,   // our worker searching and planning
		Writing,     // the networks checked and written, a slice a frame (FinishOne), then FinishEnd
		Starting,    // each AI module's job made, a slice a frame (BeginOne), then BeginEnd starts the grid (last here, so the stages before keep their numbers)
	};

	Stage stage = Stage::Idle;
	bool everyOne = false;
	double plotScale = 1.0;
	host::AutoPlotVolumeCounts runCounts;   // the run's tunables, set by AutoPlotCommand as the run starts (each job takes a copy)
	std::vector<Job> jobs;
	std::thread worker;
	std::atomic<bool> workerDone(false), stopAsked(false), preparing(false);   // preparing: the worker's first step (Prepare), for the status line
	std::atomic<size_t> jobAt(0), wayAt(0), wayCount(0);
	std::atomic<size_t> prepareAt(0), prepareOf(0);   // Prepare's steps done and to do (each job twice: its goals, then its lanes), for the start's message
	// THE START, A SLICE A FRAME (Drake 11-22, alpha.5 item 1; the owner: "screen should disappear instantly and if we have some background running task while the
	// little right bottom screen loads then we put a big message on the screen"): the click only sets the run up (Begin); each AI module's job, its exits read from
	// the file, is made on the frames after it (BeginOne, kAutoPlotStartSliceMs a frame: about 19 ms a module, 5.3 s inside the click for Colony's 273 before), on
	// this thread, since it reads the open level, then BeginEnd starts the grid. Measured 11-22 on Colony (measure-plotstart-environment): Yes answered 5312 ms late
	struct StartingRun
	{
		OpenLevelFile *open = nullptr;                      // the level the run was asked on: a frame on another stops the start, named
		std::vector<int32_t> heads;
		std::map<int32_t, std::vector<int32_t> > groups;
		std::map<int32_t, int32_t> gameNumbers;             // each module's game number (GameModuleNumbers), once for the run
		std::map<int32_t, int32_t> headOf;                  // each module's AI module (its head), for the lanes' neighbours
		size_t at = 0;                                      // the next head to make a job of
	};
	StartingRun starting;
	double const kAutoPlotStartSliceMs = 30.0;   // a frame's job making at most (as the writing's slice), then the frame is drawn: the message and its %
	bool previewSeen = false;                    // the preview has drawn this run's first frame (AutoPlotCommand_PreviewDrew): the start's message goes then
	std::string messageShown;                    // the start's message as the last frame drew it (the state line's plotmessage)
	std::string messageBox;                      // and its black box, as the panel pass said it (plotmessagebox)
	// THE PLOT'S PREVIEW, IN STEP WITH THE PLOT (autoplot_handoff.h; the owner 11-21, "Show every module in order"): the worker hands each AI module over as a constant
	// copy and waits until the window has drawn its last one. runPreview: the preview's switch as the run began (set on this thread before the worker starts, read by
	// the worker only: no copies are made when it is off). Declared before workerEnd, so it is still there while workerEnd stops the worker at the program's end
	host::AutoPlotHandOff handOff;
	bool runPreview = false;
	// THE BUDGET (autoplot_command.h kAutoPlotMostModuleLooks): the setting (lowered or raised, 1 to kAutoPlotMostModuleLooksSet), and the run's copy of it, taken as runPreview is (set on this
	// thread before the worker starts, read by the worker only)
	size_t moduleBudget = host::kAutoPlotMostModuleLooks;
	size_t runBudget = host::kAutoPlotMostModuleLooks;
	double const kAutoPlotShownEveryMs = 50.0;   // the search's ways handed over at most this often (the owner's choice (a): "drawn as they're found")
	// the run's frames and the preview's drawing, for the whole plot's time line
	size_t plotFrames = 0;
	double previewDrawMs = 0.0;
	// THE PLOT'S FRAMES (the owner 11-21, through Bishop: "it's slow and sluggish"): each plot frame's time, from its start to the next one's (the whole frame: the window
	// drawn and the plot's share), counted to the stage it began in: the grid's, the search's, the writing's, the start's (index (int)stage - 1). Said on the frame after the end
	// (so the end's own frame counts)
	size_t const kAutoPlotStages = 4;
	std::vector<double> stageFrames[kAutoPlotStages];
	std::vector<double> stageOwn[kAutoPlotStages];   // the plot's OWN share of each of those frames: its time in AutoPlotCommand_Frame (the hidden runs pace their frames, so this shows what the plot adds)
	std::chrono::steady_clock::time_point lastFrameAt;
	Stage lastFrameStage = Stage::Idle;
	std::string commandWords;
	std::chrono::steady_clock::time_point runStart;   // the run's start (the click's: Begin), for the end line's time
	std::map<int32_t, size_t> runStock;               // each AI module's own alien waypoints as the run was asked (AiModules' count), for "kept Rebellion's"
	size_t runLinkedKept = 0;                         // the run's AI modules kept because they were linked already (the Overwrite box off), for the end line

	double MsSince(std::chrono::steady_clock::time_point from)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
	}

	// a run still going when the editor ends is stopped and waited for (a joinable thread at exit ends the program)
	struct WorkerEnd
	{
		~WorkerEnd()
		{
			stopAsked = true;
			handOff.Stop();   // (a worker waiting for the window goes on at once)

			if (worker.joinable())
			{
				worker.join();
			}
		}
	} workerEnd;

	// the auto-plot's own words: the Waypoint screen draws them (StatusSayer::WaypointPlot; the owner 11-21, "When I try to run alien auto link I don't see anything": as plain
	// settings, since alpha.3's status-line call, every one was kept and none drawn)
	void Say(std::string const &words)
	{
		commandWords = words;
		statusWords.Say(commandWords.c_str(), StatusSayer::WaypointPlot);
	}

	std::string Number(long long n)
	{
		return std::to_string(n);
	}

	// every AI module of the level: its head (a master, or a module of none) and its modules, by object index
	bool AiModules(OpenLevelFile *open, std::map<int32_t, std::vector<int32_t> > *groups, std::map<int32_t, size_t> *alienCounts)
	{
		std::vector<host::ModuleSnapshot> all;

		if (host::ReadModuleSnapshots(open, &all) != host::ObjectEditResult::Done)
		{
			return false;
		}

		for (host::ModuleSnapshot const &one : all)
		{
			if (!one.module)
			{
				continue;
			}

			int32_t const head = one.masterIndex >= 0 ? one.masterIndex : one.index;

			(*groups)[head].push_back(one.index);

			if (one.index == head && one.hasWaypoint)
			{
				host::WaypointSet set;
				std::string why;

				(*alienCounts)[head] = host::Waypoints_Parse(one.waypoint.data(), one.waypoint.size(), &set, &why) ? set.alien.size() : 0;
			}
		}

		return true;
	}

	// The box an exit is in, by the game's own test (each number times the scale, cut to a whole one; inside = lowest <= place <= highest - 1), -1 for none
	int ExitBox(std::vector<host::WaypointRec> const &list, host::WaypointPoint const &exit)
	{
		for (size_t i = 0; i < list.size(); i++)
		{
			bool in = true;

			for (int a = 0; a < 3 && in; a++)
			{
				double const at = trunc((double)exit.place[a] * plotScale);

				in = at >= trunc((double)list[i].min[a] * plotScale) && at <= trunc((double)list[i].max[a] * plotScale) - 1.0;
			}

			if (in)
			{
				return (int)i;
			}
		}

		return -1;
	}

	// Whether the plan joins box `from` to box `to`: by any link (the alien route), or by plain ones only (the floor route: a marine, a predator and a praetorian skip 0x4,
	// and a one-way link goes only its way)
	bool Joins(std::vector<host::WaypointRec> const &list, int from, int to, bool plainOnly)
	{
		if (from < 0 || to < 0)
		{
			return false;
		}

		std::vector<bool> seen(list.size(), false);
		std::vector<int> next(1, from);

		seen[(size_t)from] = true;

		while (!next.empty())
		{
			int const at = next.back();

			next.pop_back();

			if (at == to)
			{
				return true;
			}

			for (host::WaypointLinkRec const &l : list[(size_t)at].links)
			{
				if (l.index >= 0 && (size_t)l.index < list.size() && !seen[(size_t)l.index] && (!plainOnly || (l.flags & (host::kLinkAlien | 0x1)) == 0))
				{
					seen[(size_t)l.index] = true;
					next.push_back(l.index);
				}
			}
		}

		return false;
	}

	// One line for each pair of exits: what the two searches found, what the plan joins, and for a floor route missing, which it is: "no ground way" (the ground search
	// found none: right, a vent only an alien climbs) or "DROPPED BY THE PLANNER" (a bug)
	std::vector<std::string> PairLines(Job const &job)
	{
		std::vector<std::string> lines;

		for (auto const &one : job.exitWays)
		{
			size_t const a = one.first.first, b = one.first.second;

			if (IsArrival(job, a) || IsArrival(job, b))
			{
				continue;   // an arrival is no exit of the level's: no pair line (the lane table tells its lanes)
			}

			int const boxA = ExitBox(job.plan.list, job.exitsFile[a]), boxB = ExitBox(job.plan.list, job.exitsFile[b]);
			bool const floorRoute = Joins(job.plan.list, boxA, boxB, true) && Joins(job.plan.list, boxB, boxA, true);
			bool const alienRoute = Joins(job.plan.list, boxA, boxB, false) && Joins(job.plan.list, boxB, boxA, false);
			std::string line = "exits " + Number((long long)a) + "-" + Number((long long)b) + ": ground way " + (one.second.second ? "yes" : "no") + ", alien way " +
				(one.second.first ? "yes" : "no") + "; in the plan floor route " + (floorRoute ? "yes" : "NO") + ", alien route " + (alienRoute ? "yes" : "NO");

			if (!floorRoute)
			{
				line += one.second.second ? " (DROPPED BY THE PLANNER)" : " (no ground way)";
			}

			lines.push_back(line);
		}

		return lines;
	}

	// the worker: every job's ways (alien and floor, each pair of goals), then its plan; it reads only its jobs and calls only the grid (one caller) and pure sums
	// THE PREVIEW'S COPY of job j as the window should show it now (autoplot_handoff.h): its search's ways so far and, once planned, its volumes. Made here on the
	// worker, from the worker's own data; handed over, it is never changed again
	std::shared_ptr<host::AutoPlotShown> ShownCopy(Job const &job, size_t j, std::vector<host::AutoPlotWay> const &ways, bool finished, std::string const &words)
	{
		std::shared_ptr<host::AutoPlotShown> copy = std::make_shared<host::AutoPlotShown>();

		copy->job = j;
		copy->of = jobs.size();
		copy->head = job.head;
		copy->name = job.name;
		copy->gameModules = job.gameModules;

		for (host::AutoPlotWay const &way : ways)
		{
			host::AutoPlotShownWay shownWay;

			shownWay.alien = way.alien;

			for (host::AutoPlotStep const &step : way.steps)
			{
				shownWay.points.insert(shownWay.points.end(), step.place, step.place + 3);
			}

			copy->ways.push_back(shownWay);
		}

		if (finished)
		{
			copy->volumes = job.plan.list;
		}

		copy->finished = finished;
		copy->words = words;
		return copy;
	}

	// What a planned job came to, for the status line beside its picture: its volumes, or why nothing is drawn (the owner 11-21, through Bishop: a module with nothing
	// to draw still says so, so words never come without a picture unexplained)
	std::string ShownWords(Job const &job)
	{
		if (!job.planned)
		{
			return "stopped, nothing drawn";
		}

		if (job.notShaped)
		{
			return "not shaped (" + (job.plan.volumeKind.empty() ? std::string("no rule for its kind") : job.plan.volumeKind) + "), nothing drawn";
		}

		if (job.plan.list.empty())
		{
			return job.exitsFile.empty() ? "no exits, nothing drawn" : "no way found through it, nothing drawn";
		}

		return Number((long long)job.plan.list.size()) + (job.plan.list.size() == 1 ? " volume placed" : " volumes placed");
	}

	// THE PREPARATION, ON THE WORKER (Drake 11-21: on the window's thread it was one frame of 1.4 to 2.1 s at the grid's end, the plot's longest, in the timed whole
	// Genshd1 plots of 6fd81100 and 7e85ca17): each AI module's goals, its doorways' ways through, the arrivals, the lanes' soffits and open sky per doorway, all from the
	// grid, as the worker's first step (one thread asks the grid at a time, as before). Returns how many ways the search will look for in all
	size_t Prepare()
	{
		// the goals: each AI module's exits and a few places a marine stands in its modules (StandSpots: on the worker, before its search)
		size_t total = 0;

		prepareAt = 0;
		prepareOf = 2 * jobs.size();   // (each job counted once in each of the two loops below, for the start's message)

		for (Job &job : jobs)
		{
			job.goals = job.exitsWorld;

			for (int32_t const module : job.gameModules)
			{
				if (job.goals.size() >= job.exitsWorld.size() + host::kAutoPlotMostSpotsPerAiModule)
				{
					break;
				}

				for (host::AutoPlotPoint const &spot : host::AutoPlotGrid_StandSpots(module, 2))
				{
					job.goals.push_back(std::vector<int32_t>{ spot.place[0], spot.place[1], spot.place[2] });
				}
			}

			// A DOORWAY WITH NO EXIT POINT BACK (Drake's whole-level gate 11-20: 26 lane ends, one-way links in Genshd1's data): the way through from this AI module's stand spot
			// nearest the exit point to it, flat and square to x or z, the doorway's middle kAutoPlotExitPast short of the exit point (exits lie about 100 inside the next module:
			// Frost's measures), the point back mirrored across it
			for (size_t e = 0; e < job.exitsWorld.size() && e < job.doors.size(); e++)
			{
				if (job.doors[e].throughFound || job.exitsWorld[e].size() != 3)
				{
					continue;
				}

				double best = 1e300, dx = 0.0, dz = 0.0;

				for (size_t g = job.exitsWorld.size(); g < job.goals.size(); g++)
				{
					double const ox = job.exitsWorld[e][0] - job.goals[g][0], oz = job.exitsWorld[e][2] - job.goals[g][2];

					if (ox * ox + oz * oz < best)
					{
						best = ox * ox + oz * oz;
						dx = ox;
						dz = oz;
					}
				}

				if (best >= 1e300 || (fabs(dx) < 10.0 && fabs(dz) < 10.0))
				{
					continue;
				}

				bool const alongX = fabs(dx) >= fabs(dz);
				host::AutoPlotDoor &d = job.doors[e];

				d.through[0] = alongX ? (dx < 0.0 ? -1.0 : 1.0) : 0.0;
				d.through[1] = alongX ? 0.0 : (dz < 0.0 ? -1.0 : 1.0);
				d.middle[0] = (int32_t)floor(job.exitsWorld[e][0] - d.through[0] * host::kAutoPlotExitPast + 0.5);
				d.middle[1] = job.exitsWorld[e][1];
				d.middle[2] = (int32_t)floor(job.exitsWorld[e][2] - d.through[1] * host::kAutoPlotExitPast + 0.5);
				d.throughFound = true;
				job.backs[e] = std::vector<int32_t>{ (int32_t)floor(job.exitsWorld[e][0] - d.through[0] * 2.0 * host::kAutoPlotExitPast + 0.5), job.exitsWorld[e][1],
					(int32_t)floor(job.exitsWorld[e][2] - d.through[1] * 2.0 * host::kAutoPlotExitPast + 0.5) };
			}

			prepareAt++;
		}

		// ONE-WAY DOORWAYS (autoplot_arrival.h; Vasquez 11-20, Drake's whole-level gate: 31 lane ends "no exit back"): once every AI module's doorways are known (the loop above),
		// each B with no exit back to an A that has one into it gets an ARRIVAL there, after its own exits (its goal after theirs), so its plan makes a door box where A's
		// aliens come in and its lanes go on from it. Only the level's exits are read (none is added before all are found)
		{
			std::vector<host::AutoPlotExits> sides(jobs.size());

			for (size_t j = 0; j < jobs.size(); j++)
			{
				sides[j].head = jobs[j].head;
				sides[j].points = jobs[j].exitsWorld;
				sides[j].to = jobs[j].exitTo;
				sides[j].doors = jobs[j].doors;
				sides[j].backs = jobs[j].backs;
			}

			for (host::AutoPlotArrival const &arrival : host::AutoPlot_Arrivals(sides))
			{
				Job &b = jobs[arrival.into];

				host::AutoPlot_AddArrival(arrival, &b.exitsWorld, &b.exitTo, &b.doors, &b.backs, &b.goals);
			}
		}

		for (Job &job : jobs)
		{
			// THE LANES: the ceiling over each exit point (the doorway's top), and the alien's goal there instead of the exit point, so its ways run soffit to soffit. At any height
			// (a tall ceiling is still a ceiling), and where there is none over the exit point itself, over the floor kAutoPlotSkyPast past the doorway's middle (Drake's gate: 16
			// lane ends where another module's roof or an opening was over the point but a ceiling went on past it)
			job.alienGoals = job.goals;

			for (size_t e = 0; e < job.exitsWorld.size() && e < job.doors.size(); e++)
			{
				host::AutoPlotPoint soffit;
				std::string why;
				bool found = host::AutoPlotGrid_CeilingOver(job.exitsWorld[e].data(), host::kAutoPlotCeilingLook, &soffit, &why);

				if (!found && job.doors[e].throughFound)
				{
					int32_t const past[3] = { (int32_t)floor(job.doors[e].middle[0] + job.doors[e].through[0] * host::kAutoPlotSkyPast + 0.5), job.exitsWorld[e][1],
						(int32_t)floor(job.doors[e].middle[2] + job.doors[e].through[1] * host::kAutoPlotSkyPast + 0.5) };

					found = host::AutoPlotGrid_CeilingOver(past, host::kAutoPlotCeilingLook, &soffit, &why);

					// no cell an alien clings to over either: the roof's own polygon there (Drake's RoofOver: a door's roof, one with no room to cling), the exit point's first
					found = found || host::AutoPlotGrid_RoofOver(job.exitsWorld[e].data(), host::kAutoPlotCeilingLook, &soffit, &why) ||
						host::AutoPlotGrid_RoofOver(past, host::kAutoPlotCeilingLook, &soffit, &why);
				}
				else if (!found)
				{
					found = host::AutoPlotGrid_RoofOver(job.exitsWorld[e].data(), host::kAutoPlotCeilingLook, &soffit, &why);
				}

				// THE DOORWAY'S TOP IS THE LOWER CEILING (Drake's gate 11-20, his soffit since lanes10): the roof's polygon straight over the exit point, and over the point back
				// inside this AI module, where either is more than kAutoPlotSoffitLower under the one found (y is down: greater); an alien crosses at the doorway's top. medoor02:
				// its door boxes went to a 5.8 m ceiling cell further back, past the 3.9 m lintel that no cell an alien clings to is on
				for (int side = 0; found && side < 2; side++)
				{
					std::vector<int32_t> const &at = side == 0 ? job.exitsWorld[e] : (e < job.backs.size() ? job.backs[e] : job.exitsWorld[e]);
					host::AutoPlotPoint roof;
					std::string none;

					if (at.size() == 3 && host::AutoPlotGrid_RoofOver(at.data(), host::kAutoPlotCeilingLook, &roof, &none) &&
						roof.place[1] > soffit.place[1] + host::kAutoPlotSoffitLower)
					{
						soffit = roof;
					}
				}

				if (found)
				{
					job.doors[e].soffitFound = true;

					for (int i = 0; i < 3; i++)
					{
						job.doors[e].soffit[i] = soffit.place[i];
					}

					job.alienGoals[e] = std::vector<int32_t>{ soffit.place[0], soffit.place[1], soffit.place[2] };
				}
			}

			// OPEN SKY PER DOORWAY (the owner's choice 11-20, through Bishop: "Stop only at exits modules with open sky", read at each exit): the floor just past the doorway, where
			// the lane would go on (kAutoPlotSkyPast past its middle, at the exit point's height), under a ceiling of ANY module at any height: then the lane must go on there
			job.ceilingPast.assign(job.exitsWorld.size(), -1);
			job.ceilingHere.assign(job.exitsWorld.size(), -1);

			for (size_t e = 0; e < job.exitsWorld.size() && e < job.doors.size(); e++)
			{
				if (job.exitsWorld[e].size() != 3)
				{
					continue;
				}

				int32_t past[3] = { job.exitsWorld[e][0], job.exitsWorld[e][1], job.exitsWorld[e][2] };

				if (job.doors[e].throughFound)
				{
					past[0] = (int32_t)floor(job.doors[e].middle[0] + job.doors[e].through[0] * host::kAutoPlotSkyPast + 0.5);
					past[2] = (int32_t)floor(job.doors[e].middle[2] + job.doors[e].through[1] * host::kAutoPlotSkyPast + 0.5);
				}

				host::AutoPlotPoint over;
				std::string why;

				// by the roof's polygons (Drake's AutoPlotGrid_RoofOver: the very test of his lane gate, any module, doors too, at least 300 over), not the cells an alien clings
				// to (no cells under a door's roof, nor a roof with no room to cling: 28 Genshd1 AI modules were open sky by the cells, roofed by the polygons, 11-20)
				job.ceilingPast[e] = host::AutoPlotGrid_RoofOver(past, host::kAutoPlotCeilingLook, &over, &why) ? 1 : 0;

				// and this side's (Drake's gate: a lane can't start under open sky, so a doorway with none on this side is no lane end)
				int32_t here[3] = { job.exitsWorld[e][0], job.exitsWorld[e][1], job.exitsWorld[e][2] };

				if (job.doors[e].throughFound)
				{
					here[0] = (int32_t)floor(job.doors[e].middle[0] - job.doors[e].through[0] * host::kAutoPlotSkyPast + 0.5);
					here[2] = (int32_t)floor(job.doors[e].middle[2] - job.doors[e].through[1] * host::kAutoPlotSkyPast + 0.5);
				}

				job.ceilingHere[e] = host::AutoPlotGrid_RoofOver(here, host::kAutoPlotCeilingLook, &over, &why) ? 1 : 0;
			}

			size_t const n = job.goals.size();

			total += 2 * (std::min)(n * (n > 0 ? n - 1 : 0) / 2, host::kAutoPlotMostPairs);
			prepareAt++;
		}

		return total;
	}

	void Work()
	{
		// the preparation first (Prepare: never on the window's thread), then each AI module in turn. Out of memory there (nothing else in it throws): no module is
		// searched, each one says why, and the worker ends as it does after the last (an exception left to leave a thread would end the editor)
		preparing = true;

		try
		{
			wayCount = Prepare();
		}
		catch (std::exception const &)
		{
			for (Job &job : jobs)
			{
				job.why = "out of memory";
			}

			preparing = false;
			workerDone = true;
			return;
		}

		preparing = false;

		for (size_t j = 0; j < jobs.size() && !stopAsked; j++)
		{
			Job &job = jobs[j];

			jobAt = j;

			try
			{
				std::vector<host::AutoPlotWay> ways;
				size_t pairs = 0;
				auto const searchFrom = std::chrono::steady_clock::now();
				auto lastShown = searchFrom;

				// THE PREVIEW: the module as its search begins (no ways yet), then its ways as they are found, at most every kAutoPlotShownEveryMs (none of it waits)
				if (runPreview)
				{
					handOff.Progress(ShownCopy(job, j, ways, false, "searching"));
				}

				auto const showWays = [&]()
				{
					if (runPreview && MsSince(lastShown) >= kAutoPlotShownEveryMs)
					{
						handOff.Progress(ShownCopy(job, j, ways, false, "searching"));
						lastShown = std::chrono::steady_clock::now();
					}
				};

				// THE BUDGET (kAutoPlotMostModuleLooks): once this module's searches have looked at more cells than the run's budget, it searches no more
				bool overBudget = false;

				for (size_t a = 0; a < job.goals.size() && pairs < host::kAutoPlotMostPairs && !stopAsked && !overBudget; a++)
				{
					for (size_t b = a + 1; b < job.goals.size() && pairs < host::kAutoPlotMostPairs && !stopAsked && !overBudget; b++, pairs++)
					{
						for (bool const alien : { true, false })
						{
							if (overBudget)
							{
								break;
							}

							std::vector<host::AutoPlotPoint> points;
							std::string why;

							std::vector<std::vector<int32_t> > const &goals = alien && job.alienGoals.size() == job.goals.size() ? job.alienGoals : job.goals;
							size_t looked = 0;
							bool const found = host::AutoPlotGrid_Way(goals[a].data(), goals[b].data(), alien, &points, &why, &looked);

							job.searches++;
							job.looks += looked;
							job.mostLooks = looked > job.mostLooks ? looked : job.mostLooks;
							overBudget = job.looks > runBudget;

							if (a < job.exitsWorld.size() && b < job.exitsWorld.size())
							{
								std::pair<bool, bool> &got = job.exitWays[std::make_pair(a, b)];

								(alien ? got.first : got.second) = found;
							}

							if (found)
							{
								host::AutoPlotWay way;

								way.alien = alien;

								for (host::AutoPlotPoint const &p : points)
								{
									host::AutoPlotStep step;

									for (int i = 0; i < 3; i++)
									{
										step.place[i] = p.place[i];
										step.facing[i] = p.facing[i];
									}

									step.surface = p.surface == host::AutoPlotPoint::Surface::Floor ? host::AutoPlotSurface::Floor :
										(p.surface == host::AutoPlotPoint::Surface::Ceiling ? host::AutoPlotSurface::Ceiling : host::AutoPlotSurface::Wall);
									step.inside = std::find(job.gameModules.begin(), job.gameModules.end(), p.module) != job.gameModules.end();
									way.steps.push_back(step);
								}

								ways.push_back(way);
								showWays();
							}

							wayAt++;
						}
					}
				}

				// PAST THE BUDGET: the module is refused here, named with its numbers (the report's "<name> not written: stopped by the plot's budget: ..."), and
				// nothing of it is planned or written; the next module is planned as usual
				if (overBudget)
				{
					job.why = "stopped by the plot's budget: its way searches looked at " + std::to_string((unsigned long long)job.looks) + " cells in " +
						std::to_string((unsigned long long)job.searches) + " searches, more than the " + std::to_string((unsigned long long)runBudget) +
						" an AI module may" + (runBudget < host::kAutoPlotMostModuleLooks ? std::string(" (lowered by [Editor] AutoPlotMostModuleLooks)") :
						runBudget > host::kAutoPlotMostModuleLooks ? std::string(" (raised by [Editor] AutoPlotMostModuleLooks)") :
						std::string(" (the most of the 10,770 stock AI modules is 2,212,533)")) +
						// HOW TO PLOT IT IN FULL (the owner, 11-23: "Don't decrease map precision because it would take longer"): the user's own setting raises it
						// (the second words are a GUARD no run reaches: at the most, 16000000, a module's 40 pairs x 2 searches of at most 200000 cells each can't
						// pass it, the static_assert below at AutoPlotCommand_SetModuleBudget keeping the three equal; Drake's nit on 6774ce25. Kept so a change of one of
						// them still reads right)
						(runBudget < host::kAutoPlotMostModuleLooksSet ? "; raise AutoPlotMostModuleLooks= under [Editor] in AVP_OpenInteng.ini (up to " +
							std::to_string((unsigned long long)host::kAutoPlotMostModuleLooksSet) + ") to plot it in full" :
							"; that is already the most AutoPlotMostModuleLooks= takes");
					job.searchMs = MsSince(searchFrom);

					if (runPreview)
					{
						handOff.Finished(ShownCopy(job, j, ways, true, job.why), stopAsked);
					}

					continue;
				}

				// THE CLIMB STATIONS (Drake's AutoPlotGrid_ClimbStation): at each exit with a doorway's ceiling, where an alien coming from that neighbour lands (its exit point
				// back, inside this AI module: a far alien lands there upright, Vasquez's drop audit S3), its way across the floor on this side to the nearest side wall, up it,
				// onto the ceiling; its top is where the door box's centre goes, so an alien there heads for that wall and climbs
				for (size_t e = 0; e < job.exitsWorld.size() && e < job.doors.size() && e < job.backs.size() && !stopAsked; e++)
				{
					std::vector<host::AutoPlotPoint> points;
					std::string why;
					host::AutoPlotDoor const &d = job.doors[e];
					int32_t const into[3] = { (int32_t)(-d.through[0] * 65536.0), 0, (int32_t)(-d.through[1] * 65536.0) };   // from the neighbour into this AI module
					int32_t top[3] = { 0, 0, 0 };

					if (!d.soffitFound || !d.throughFound || job.backs[e].size() != 3 ||
						!host::AutoPlotGrid_ClimbStation(job.backs[e].data(), into, host::kAutoPlotStationReach, &points, top, &why))
					{
						continue;
					}

					host::AutoPlotWay way;
					bool walled = false;

					way.alien = true;

					for (host::AutoPlotPoint const &p : points)
					{
						host::AutoPlotStep step;

						for (int i = 0; i < 3; i++)
						{
							step.place[i] = p.place[i];
							step.facing[i] = p.facing[i];
						}

						step.surface = p.surface == host::AutoPlotPoint::Surface::Floor ? host::AutoPlotSurface::Floor :
							(p.surface == host::AutoPlotPoint::Surface::Ceiling ? host::AutoPlotSurface::Ceiling : host::AutoPlotSurface::Wall);
						step.inside = std::find(job.gameModules.begin(), job.gameModules.end(), p.module) != job.gameModules.end();
						way.steps.push_back(step);

						walled = walled || step.surface == host::AutoPlotSurface::Wall;
					}

					job.doors[e].stationFound = walled;

					for (int i = 0; i < 3; i++)
					{
						job.doors[e].stationTop[i] = top[i];
					}

					ways.push_back(way);
					showWays();
				}

				job.searchMs = MsSince(searchFrom);

				auto const planFrom = std::chrono::steady_clock::now();

				// the floor a marine stands on: every cell of the AI module's game modules (Drake's FloorCells), the plan tiles it
				host::AutoPlotFloor floorCells;

				for (int32_t const module : job.gameModules)
				{
					std::vector<host::AutoPlotPoint> cells;
					int32_t edge = 0;
					std::string why;

					if (!stopAsked && host::AutoPlotGrid_FloorCells(module, &cells, &edge, &why))
					{
						floorCells.cellEdge = edge;

						for (host::AutoPlotPoint const &cell : cells)
						{
							if (floorCells.cells.size() < host::kAutoPlotMostCells)
							{
								floorCells.cells.push_back(std::vector<int32_t>{ cell.place[0], cell.place[1], cell.place[2] });
							}

							// THE CEILING over it (the plan tiles it: Drake's rule, autoplot_plan.h AutoPlotFloor), when it is this AI module's own
							host::AutoPlotPoint over;
							std::string none;

							// at any height (Bishop 11-20: "a tall ceiling is still a ceiling"; the grid holds the look to its own most)
							job.floorCellsSeen++;

							// where no cell an alien clings to is over it, the roof's own polygon (Drake's AutoPlotGrid_RoofOver, the lane gate's open-sky test: a door's polygons get
							// no cells but their floor, and a roof with no room to cling none; 28 of Genshd1's AI modules were open sky by the cells and roofed by the polygons, 11-20)
							// (the roof of its OWN modules only, Drake's onlyModules: a cling cell of another module over a door's floor, MAIN03's over medoor01, kept the || from asking)
							// and last ANY module's down-facing roof whose hanging alien would be in this AI module's own space (Bishop's ruling 11-20, Drake's RoofHungIn: the
							// game runs an alien on the network of the module its position is in): office under new03's ceiling at doormp06's doorway, and not water under top's
							if (!stopAsked && floorCells.ceilings.size() < host::kAutoPlotMostCells &&
								((host::AutoPlotGrid_CeilingOver(cell.place, host::kAutoPlotCeilingLook, &over, &none) &&
									std::find(job.gameModules.begin(), job.gameModules.end(), over.module) != job.gameModules.end()) ||
									host::AutoPlotGrid_RoofOver(cell.place, host::kAutoPlotCeilingLook, &over, &none, &job.gameModules) ||
									host::AutoPlotGrid_RoofHungIn(cell.place, host::kAutoPlotCeilingLook, job.gameModules, &over, &none)))
							{
								floorCells.ceilings.push_back(std::vector<int32_t>{ over.place[0], over.place[1], over.place[2] });
								job.floorCellsCeiled++;
							}
						}
					}
				}

				// every floor a marine stands on in the AI module's floor-to-ceiling extent, of ANY module or still object (Drake's AutoPlotGrid_FloorsIn): a platform under its
				// lane boxes that isn't its own polygons (Object11's at y -581, 11-20) keeps them over its standing heads too
				if (!stopAsked && !floorCells.cells.empty())
				{
					int32_t lo[3], hi[3];

					for (int i = 0; i < 3; i++)
					{
						lo[i] = hi[i] = floorCells.cells[0][(size_t)i];
					}

					for (std::vector<std::vector<int32_t> > const *list : { &floorCells.cells, &floorCells.ceilings })
					{
						for (std::vector<int32_t> const &c : *list)
						{
							for (int i = 0; i < 3; i++)
							{
								lo[i] = (std::min)(lo[i], c[(size_t)i]);
								hi[i] = (std::max)(hi[i], c[(size_t)i]);
							}
						}
					}

					std::vector<host::AutoPlotPoint> standing;
					std::string none;

					if (host::AutoPlotGrid_FloorsIn(lo, hi, &standing, &none))
					{
						for (host::AutoPlotPoint const &p : standing)
						{
							if (floorCells.standFloors.size() < host::kAutoPlotMostCells)
							{
								floorCells.standFloors.push_back(std::vector<int32_t>{ p.place[0], p.place[1], p.place[2] });
							}
						}
					}
				}

				// NO CELL A MARINE STANDS ON in any of its game modules: the cells a crouched alien stands on (Drake's CrawlCells: a duct), a crawl floor for the levels' rule
				// (Frost 11-20, autoplot_levels.h)
				if (floorCells.cells.empty())
				{
					for (int32_t const module : job.gameModules)
					{
						std::vector<host::AutoPlotPoint> cells;
						int32_t edge = 0;
						std::string why;

						if (!stopAsked && host::AutoPlotGrid_CrawlCells(module, &cells, &edge, &why))
						{
							floorCells.cellEdge = edge;
							floorCells.crawl = true;

							for (host::AutoPlotPoint const &cell : cells)
							{
								if (floorCells.cells.size() < host::kAutoPlotMostCells)
								{
									floorCells.cells.push_back(std::vector<int32_t>{ cell.place[0], cell.place[1], cell.place[2] });
								}
							}
						}
					}
				}

				// THE VOLUME SHAPE first (the owner 11-20: a few room-sized volumes, autoplot_volumes.h); a module it can't shape keeps the old tiles, and the note says so.
				// It takes the AI module's own box too: how far its sides (a tube's 45-degree faces) reach past the floor
				for (int32_t const module : job.gameModules)
				{
					int32_t lo[3], hi[3];

					if (host::AutoPlotGrid_ModuleBox(module, lo, hi))
					{
						for (int i = 0; i < 3; i++)
						{
							floorCells.boxLo[i] = floorCells.boxFound ? (std::min)(floorCells.boxLo[i], lo[i]) : lo[i];
							floorCells.boxHi[i] = floorCells.boxFound ? (std::max)(floorCells.boxHi[i], hi[i]) : hi[i];
						}

						floorCells.boxFound = true;
					}
				}

				job.plan = host::AutoPlot_PlanVolumes(ways, job.exitsWorld, floorCells, job.doors, plotScale, job.counts);   // the owner's counts (the run's, taken at Begin)
				host::AutoPlot_RepairVolumes(&job.plan, job.exitsFile, plotScale);   // Drake's volume repair (autoplot_volume_repair.h): a scrap box out, a centre in another box moved flat
				host::AutoPlot_CloserClearAims(&job.plan, plotScale);   // then no closer reaches into the body at a final aim point (autoplot_closers.h)
				job.planMs = MsSince(planFrom);

				// a kind it has no rule for yet: NO network (Bishop's ruling 11-20: the way Rebellion left most modules, never the old small boxes), named in the summary
				job.notShaped = !job.plan.volumes;
				job.planned = !stopAsked;
				job.pairLines = PairLines(job);

				// THE PREVIEW, IN STEP: the module's last copy (its ways and its volumes, or why nothing is drawn), and the worker waits until the window has drawn it
				// (no wait when nobody watches: the preview off, another screen up). A wait ended by a stop: the loop's own test ends the run
				if (runPreview)
				{
					handOff.Finished(ShownCopy(job, j, ways, true, ShownWords(job)), stopAsked);
				}
			}
			catch (std::exception const &)
			{
				job.why = "out of memory";
			}
		}

		workerDone = true;
	}

	// THE START, ON THE CLICK (see StartingRun): the run's lists kept, the clocks started from here (so the whole plot's time counts the start), and nothing slow: the
	// click's dialog is gone on the next frame, and the jobs are made on the frames after it (BeginOne), then the grid (BeginEnd)
	void Begin(OpenLevelFile *open, std::vector<int32_t> const &heads, std::map<int32_t, std::vector<int32_t> > const &groups)
	{
		// A RUN GOING IS NEVER STARTED AGAIN (Bishop 2, 11-22, from Vasquez's review of item 1): everything below makes the run's state new, and the jobs it clears may
		// be in the worker thread's hands. The Run checks the stage before it asks anything, but the replace question's Yes calls here later, so the guard is Begin's own,
		// before anything is touched: refused, said, nothing changed (tests/host_autoplot_begin_guard_test.cpp)
		if (stage != Stage::Idle)
		{
			Say("Auto-plot: a run is going already, so this one did not start (nothing changed)");
			return;
		}

		starting = StartingRun();
		starting.open = open;
		starting.heads = heads;
		starting.groups = groups;
		host::GameModuleNumbers(open->Level(), &starting.gameNumbers);

		for (auto const &group : groups)
		{
			for (int32_t const member : group.second)
			{
				starting.headOf[member] = group.first;
			}
		}

		plotScale = local_scale > 0.0 ? local_scale : 1.0;
		jobs.clear();
		previewSeen = false;
		prepareAt = 0;
		prepareOf = 0;
		// THE HAND-OVER STARTS WITH THE RUN (Drake 2026-11-23, the owner's "are we clearing the previous level"): the run is busy from here, and the preview's window
		// takes the newest copy every frame, so a hand-over started only with the worker (below) still offered the LAST run's copy through the grid's whole stage
		// (All stock maps: derelict's head 384 drawn over genshd1's start). Start lets go of it and counts this run (autoplot_handoff.h Run)
		handOff.Start();
		stage = Stage::Starting;
		runStart = std::chrono::steady_clock::now();
		plotFrames = 0;
		previewDrawMs = 0.0;

		for (size_t s = 0; s < kAutoPlotStages; s++)
		{
			stageFrames[s].clear();
			stageOwn[s].clear();
		}

		lastFrameStage = Stage::Idle;   // (the first plot frame starts the count)
		Say("Auto-plot: preparing " + Number((long long)heads.size()) + " AI modules");
	}

	// ONE AI MODULE'S JOB (a frame's slice of them, from AutoPlotCommand_Frame): its exits read from the file (its head's entry points), its lanes' doorways
	void BeginOne(int32_t head)
	{
		OpenLevelFile *const open = starting.open;
		std::map<int32_t, int32_t> const &gameNumbers = starting.gameNumbers;
		std::map<int32_t, std::vector<int32_t> > const &groups = starting.groups;
		auto const headOfModule = [](int32_t module) { auto const found = starting.headOf.find(module); return found != starting.headOf.end() ? found->second : -1; };

		{
			Job job;
			std::vector<host::EntryMarker> markers;

			job.head = head;
			job.name = ModuleNameOfIndex(head);
			job.stockAliens = runStock.count(head) ? runStock.at(head) : 0;
			job.members = groups.at(head);
			job.counts = runCounts;

			for (int32_t const member : groups.at(head))
			{
				auto const found = gameNumbers.find(member);

				if (found != gameNumbers.end())
				{
					job.gameModules.push_back(found->second);
				}
			}

			// its exits: its head's entry points (ADJMDLEP: one to each neighbouring AI module, in the file's units)
			if (host::EntryMarkers_Of(open, head, &markers))
			{
				for (host::EntryMarker const &m : markers)
				{
					host::WaypointPoint file;
					std::vector<int32_t> world(3);

					for (int i = 0; i < 3; i++)
					{
						file.place[i] = m.point[i];
						world[(size_t)i] = (int32_t)((double)m.point[i] * plotScale);
					}

					job.exitsFile.push_back(file);
					job.exitsWorld.push_back(world);
					job.exitTo.push_back(headOfModule(m.adjacent));
				}
			}

			// THE LANES: each exit's doorway, its middle half way to the neighbour's exit point back (inside this AI module), the way through from that one to this one, flat
			job.doors.assign(job.exitsWorld.size(), host::AutoPlotDoor());
			job.backs.assign(job.exitsWorld.size(), std::vector<int32_t>());

			for (size_t e = 0; e < job.exitsWorld.size(); e++)
			{
				std::vector<host::EntryMarker> back;

				if (job.exitTo[e] < 0 || !host::EntryMarkers_Of(open, job.exitTo[e], &back))
				{
					continue;
				}

				for (host::EntryMarker const &m : back)
				{
					if (headOfModule(m.adjacent) != head)
					{
						continue;
					}

					double const bx = (double)m.point[0] * plotScale, by = (double)m.point[1] * plotScale, bz = (double)m.point[2] * plotScale;
					double const dx = job.exitsWorld[e][0] - bx, dz = job.exitsWorld[e][2] - bz, flat = sqrt(dx * dx + dz * dz);

					if (flat < 10.0 || flat > 4000.0)
					{
						break;   // the two points on one spot, or too far apart to be one doorway: no way through known
					}

					host::AutoPlotDoor &d = job.doors[e];

					d.throughFound = true;
					d.middle[0] = (int32_t)floor((job.exitsWorld[e][0] + bx) / 2.0 + 0.5);
					d.middle[1] = (int32_t)floor((job.exitsWorld[e][1] + by) / 2.0 + 0.5);
					d.middle[2] = (int32_t)floor((job.exitsWorld[e][2] + bz) / 2.0 + 0.5);
					d.through[0] = dx / flat;
					d.through[1] = dz / flat;
					job.backs[e] = std::vector<int32_t>{ (int32_t)floor(bx + 0.5), (int32_t)floor(by + 0.5), (int32_t)floor(bz + 0.5) };
					break;
				}
			}

			jobs.push_back(job);
		}
	}

	// THE START'S END (the frame after the last job): the grid started on its own thread, the run on to Stage::Grid
	void BeginEnd()
	{
		std::string why;

		if (!host::AutoPlotGrid_Start(starting.open, &why))
		{
			stage = Stage::Idle;
			Say("Auto-plot: the pathfinder would not start: " + why);
			gLast = AutoPlotResult{ true, false, 0, 0, 0, "the pathfinder would not start: " + why };
			return;
		}

		stage = Stage::Grid;
		Say("Auto-plot: building the grid for " + Number((long long)jobs.size()) + " AI modules");
	}

	// the networks the worker made: each checked as the stock game would take it, the good ones written over their AI module's alien list
	// THE WRITING (Drake 11-21): on the writing's first frame every network is checked (ToWrite) and every one that passed goes into the level in ONE write
	// (WaypointMode::AutoPlotWriteMany). Each AI module's own write had read the whole level three times and loaded it again once, about 70 ms whatever the module:
	// a whole Genshd1 plot spent 14 s of its 80 there, at 6 frames a second (the timed run of 6fd81100). Then each module's report, in order, kAutoPlotWriteSliceMs a
	// frame (FinishOne, its sums kept here across the frames), then FinishEnd once
	double const kAutoPlotWriteSliceMs = 30.0;   // a frame's reports at most (then the frame is drawn: the status line's "writing module i of N", the busy cursor)

	struct Finishing
	{
		OpenLevelFile *open = nullptr;   // the level the networks go into (the one open when the writing began; another open since: the writing stops)
		size_t at = 0;                   // the next AI module to report
		bool writtenAll = false;         // the one write is done
		double writeMs = 0.0;            // how long the one write took
		size_t inWrite = 0;              // how many networks went into it
		size_t written = 0, refused = 0, boxes = 0, noExits = 0, exceptions = 0, notShaped = 0;
		std::string last, lastRefused, noExitNames, notShapedNames;
	};

	Finishing finishing;

	// Whether a job's network is written: what FinishOne refuses before the write, in its own order (no exits, not shaped, nothing planned, the checker's problems
	// after rule 6 (a)'s exceptions), the checker's findings kept on the job for FinishOne's report
	bool ToWrite(Job &job)
	{
		if (job.planned && job.why.empty() && job.exitsFile.empty() && job.plan.list.empty())
		{
			return false;
		}

		if (job.planned && job.why.empty() && job.notShaped)
		{
			return false;
		}

		if (!job.planned || job.plan.list.empty())
		{
			return false;
		}

		auto const checkFrom = std::chrono::steady_clock::now();

		job.problems = host::Waypoints_Check(job.plan.list, job.exitsFile, plotScale, 50);
		job.excepted.clear();

		// the checker's exceptions by rule 6 (a) (Bishop 11-20; Frost's checker unchanged): the exits the repair left out, no alien way to them (printed when written)
		host::AutoPlot_RepairExceptions(&job.problems, job.plan.exitsLeftOut, &job.excepted);
		job.checkMs = MsSince(checkFrom);
		return job.problems.empty();
	}

	// The one write: every job's network that passed, into the level at once; each job told what it did for it
	void WriteAll()
	{
		std::vector<int32_t> masters;
		std::vector<std::vector<host::WaypointRec> const *> lists;
		std::vector<size_t> jobOf;

		for (size_t j = 0; j < jobs.size(); j++)
		{
			jobs[j].inWrite = ToWrite(jobs[j]);

			if (jobs[j].inWrite)
			{
				masters.push_back(jobs[j].head);
				lists.push_back(&jobs[j].plan.list);
				jobOf.push_back(j);
			}
		}

		std::vector<bool> wrote;
		std::vector<std::string> words;
		auto const writeFrom = std::chrono::steady_clock::now();

		waypointMode.AutoPlotWriteMany(finishing.open, masters, lists, &wrote, &words);
		finishing.writeMs = MsSince(writeFrom);
		finishing.inWrite = masters.size();

		for (size_t k = 0; k < jobOf.size(); k++)
		{
			Job &job = jobs[jobOf[k]];

			job.levelWritten = k < wrote.size() && wrote[k];
			job.writeWords = k < words.size() ? words[k] : std::string();
			job.levelWriteMs = finishing.writeMs / (double)jobOf.size();   // (its share of the one write)
		}

		printf("AVP_OpenInteng: auto-plot: the networks written in one go: %zu AI modules in %.0f ms\n", masters.size(), finishing.writeMs);
		fflush(stdout);
		finishing.writtenAll = true;
	}

	// one AI module's network: checked as the stock game would take it and, when good, written over its alien list (what the one-go Finish did for each)
	void FinishOne(Job &job)
	{
		OpenLevelFile *const open = finishing.open;
		size_t &written = finishing.written, &refused = finishing.refused, &boxes = finishing.boxes, &noExits = finishing.noExits, &exceptions = finishing.exceptions,
			&notShaped = finishing.notShaped;
		std::string &last = finishing.last, &lastRefused = finishing.lastRefused, &noExitNames = finishing.noExitNames, &notShapedNames = finishing.notShapedNames;

		{
			// (its check and write timed by every way out of this loop: a clock that writes the time down as it goes; the check and its share of the one write were
			// made before, in WriteAll, and are added in)
			struct WriteClock
			{
				Job &job;
				std::chrono::steady_clock::time_point from;

				~WriteClock()
				{
					job.writeMs = job.checkMs + job.levelWriteMs + MsSince(from);
				}
			} const writeClock{ job, std::chrono::steady_clock::now() };

			std::string const name = "Auto-plot: AI module " + Number(job.head);
			std::string const named = name + " (" + ModuleNameOfIndex(job.head) + ")";   // the report's: which room it is

			// the report's lines (the harness keeps the editor's output): one a pair of exits, one an exit's doorway (THE LANES)
			for (std::string const &line : job.pairLines)
			{
				printf("AVP_OpenInteng: auto-plot: AI module %d, %s\n", (int)job.head, line.c_str());
			}

			for (size_t e = 0; e < job.plan.doorWords.size(); e++)
			{
				// Vasquez's drop audit D4 (bh_waypoint.cpp:118-124): the target, this exit's point, is looked for only in THIS AI module's boxes; the game takes the last box
				// that holds it (SweepWaypoints): none, and every alien heading through this doorway drops out
				int target = -1;

				for (size_t i = 0; e < job.exitsFile.size() && i < job.plan.list.size(); i++)
				{
					std::vector<host::WaypointRec> const one(1, job.plan.list[i]);

					target = ExitBox(one, job.exitsFile[e]) == 0 ? (int)i : target;
				}

				std::string const d4 = IsArrival(job, e) ? "an ARRIVAL (a one-way doorway: this AI module has no exit back, so no alien here heads for it: no D4)" :
					target < 0 ? "D4 FAILS: no box of this AI module holds its point" : "D4: its point in box " + Number(target) +
					((size_t)target < job.plan.climbing.size() && job.plan.climbing[(size_t)target] ? " (a CLIMBING box: an alien there is in its target's box)" : " (a floor box)");

				printf("AVP_OpenInteng: auto-plot: AI module %d, exit %d (to AI module %d): %s; %s\n", (int)job.head, (int)e, e < job.exitTo.size() ? (int)job.exitTo[e] : -1,
					job.plan.doorWords[e].c_str(), d4.c_str());
			}

			// every box, for the report and for reading a break: its kind (door / exit / climb / floor), its numbers (the file's), its links (index, flags)
			for (size_t i = 0; i < job.plan.list.size(); i++)
			{
				host::WaypointRec const &w = job.plan.list[i];
				std::string kind = i < job.plan.climbing.size() && job.plan.climbing[i] ? "climb" : "floor";
				std::string links;

				for (size_t e = 0; e < job.plan.doorBox.size(); e++)
				{
					kind += job.plan.doorBox[e] == (int32_t)i ? " door" + Number((long long)e) : std::string();
					kind += e < job.plan.exitBox.size() && job.plan.exitBox[e] == (int32_t)i ? " exit" + Number((long long)e) : std::string();
				}

				// the volume planner's own word for the box last (LOW, HIGH, entry, get-out, ...; Dietrich 11-20: after the old words, so "floor exit0" / "climb doorN" still read)
				kind += i < job.plan.volumeRoles.size() && !job.plan.volumeRoles[i].empty() ? " " + job.plan.volumeRoles[i] : std::string();

				for (host::WaypointLinkRec const &l : w.links)
				{
					// "o" after it: one-way (inteng's Add Link "only W to V"; the stock search never takes it this way: Vasquez's floor-shortcut pass stops steps so)
					links += " " + Number(l.index) + ((l.flags & host::kLinkAlien) ? "a" : "p") + ((l.flags & host::kLinkOneWay) ? "o" : "");
				}

				printf("AVP_OpenInteng: auto-plot: AI module %d box %d %s: min %d %d %d max %d %d %d centre %d %d %d links%s\n", (int)job.head, (int)i, kind.c_str(), w.min[0], w.min[1],
					w.min[2], w.max[0], w.max[1], w.max[2], w.centre[0], w.centre[1], w.centre[2], links.c_str());
			}

			fflush(stdout);

			auto const tell = [](std::string const &words)
			{
				printf("AVP_OpenInteng: auto-plot: %s\n", words.c_str());
				fflush(stdout);
			};

			// NO EXITS (no entry points, ADJMDLEP): no route goes through it, so it needs no list. Bishop 11-20: "no exits, nothing to plot", counted and named, not refused
			if (job.planned && job.why.empty() && job.exitsFile.empty() && job.plan.list.empty())
			{
				noExits++;
				noExitNames += noExits > 10 ? std::string(noExits == 11 ? ", ..." : "") : std::string(noExits > 1 ? ", " : "") + ModuleNameOfIndex(job.head);
				last = name + ": no exits, nothing to plot (no entry points: no route goes through it)";
				tell(named + last.substr(name.size()));
				return;
			}

			// THE VOLUME SHAPE: its kind on the report, and a kind with no rule yet left as it is (NO network written), counted and named
			if (job.planned && job.why.empty())
			{
				tell(named + ": KIND " + (job.plan.volumeKind.empty() ? std::string("?") : job.plan.volumeKind) + (job.notShaped ? std::string(", NOT SHAPED: ") +
					(job.plan.notes.empty() ? std::string("no reason given") : job.plan.notes[0]) : std::string(", shaped as volumes")));
			}

			if (job.planned && job.why.empty() && job.notShaped)
			{
				notShaped++;
				notShapedNames += notShaped > 10 ? std::string(notShaped == 11 ? ", ..." : "") : std::string(notShaped > 1 ? ", " : "") + ModuleNameOfIndex(job.head) + " (" + job.plan.volumeKind + ")";
				last = name + ": not shaped, no network written (" + job.plan.volumeKind + ")";
				return;
			}

			if (!job.planned || job.plan.list.empty())
			{
				refused++;
				last = name + " not written: " + (!job.why.empty() ? job.why : (job.planned ? std::string("no way was found through it") : std::string("the run was stopped")));
				lastRefused = last;
				tell(named + last.substr(name.size()));
				return;
			}

			// the checker's findings, made before the one write (ToWrite)
			std::vector<host::WaypointProblem> const &problems = job.problems, &excepted = job.excepted;

			if (!problems.empty())
			{
				refused++;
				last = name + " not written: " + problems[0].words + (problems.size() > 1 ? " (and " + Number((long long)problems.size() - 1) + " more)" : std::string());
				lastRefused = last;
				tell(named + last.substr(name.size()) + (job.plan.notes.empty() || job.plan.notes.back().compare(0, 7, "repair:") != 0 ? std::string() : "; " + job.plan.notes.back()));   // what the repair did first
				return;
			}

			for (host::WaypointProblem const &p : excepted)   // Frost's words for the finding
			{
				exceptions++;
				tell(named + ": checker exception by rule 6(a): " + p.words + "; exit " + Number((long long)p.other) + ": no alien route to this exit: an alien heading there walks the floor way (D4)");
			}

			// what the one write did for it (WriteAll)
			host::WaypointOutcome done = {};

			done.levelChanged = job.inWrite && job.levelWritten;
			done.say = job.inWrite ? job.writeWords : "Auto-plot: AI module " + Number(job.head) + " not written: it was not in the one write";

			if (!done.levelChanged)
			{
				refused++;
				last = done.say;
				lastRefused = name + " not written: " + last;
				tell(named + " not written: " + last);
				return;
			}

			written++;
			job.written = true;

			// AIRDUCT on every module of an AI module the planner SHAPED (Bishop 11-20, Drake's read-back of 435ff6c7: hand6g, the gold standard, has it on all its shaped
			// modules, and it is the owner's force-crawl: "the flag worked and they are trying to climb"), through Strategy mode's own Add Flag > Airduct; only when asked (counts.airduct, the dialog's
			// box: off by default since the owner's 19:06)
			if (job.plan.volumes && job.counts.airduct)
			{
				std::vector<uint8_t> ids;

				for (int32_t const member : job.members)
				{
					uint8_t id[host::kSelectionIdBytes];

					if (host::ModuleIdOfIndex(LevelObjects_Level(), member, id))
					{
						ids.insert(ids.end(), id, id + host::kSelectionIdBytes);
					}
				}

				size_t flagged = 0;

				if (!ids.empty() && host::StrategyMode_SetModuleFlag(open, ids.data(), ids.size() / host::kSelectionIdBytes, host::kAutoPlotAirduct, true, &flagged) ==
					host::StrategyModeResult::Done && flagged > 0)
				{
					tell(named + ": Airduct set on " + Number((long long)flagged) + " of its " + Number((long long)(ids.size() / host::kSelectionIdBytes)) + " modules (the force-crawl)");
				}
			}
			boxes += job.plan.list.size();
			last = name + ": " + host::AutoPlot_Words(job.plan);

			std::string more = "; floor boxes " + Number((long long)job.plan.floorBoxes) + ", climbing " + Number((long long)(job.plan.list.size() - job.plan.floorBoxes)) +
				(job.plan.floorTile > 0.0 ? ", tiles " + Number((long long)job.plan.floorTile) + " mm, floor cells covered " + Number((long long)job.plan.cellsCovered) + " of " +
					Number((long long)job.plan.cells) : std::string(", no floor cells")) +
				(job.plan.ceilingTile > 0.0 ? ", ceiling tiles " + Number((long long)job.plan.ceilingBoxes) + " of " + Number((long long)job.plan.ceilingTile) + " mm" :
					std::string(", no ceiling over the floor")) +
				(job.plan.floorMost <= 0.0 ? std::string(", the floor not merged (a merge broke a lane)") : job.plan.floorMost >= 1.0e8 ? std::string(", the floor in whole rectangles") :
					", the floor merged up to " + Number((long long)job.plan.floorMost) + " mm") +
				(job.floorCellsSeen == 0 ? std::string() : 2 * job.floorCellsCeiled < job.floorCellsSeen ?
					", OPEN SKY (" + Number((long long)(100 - 100 * job.floorCellsCeiled / job.floorCellsSeen)) + "% of the floor under no ceiling)" :
					", ceiling over " + Number((long long)(100 * job.floorCellsCeiled / job.floorCellsSeen)) + "% of the floor");

			for (std::string const &note : job.plan.notes)
			{
				more += "; " + note;
			}

			tell(named + last.substr(name.size()) + more);
		}
	}

	// after the last AI module's network: the lane table, open sky per doorway, the run's time and the end line (the rest of what the one-go Finish did)
	void FinishEnd()
	{
		size_t &written = finishing.written, &refused = finishing.refused, &boxes = finishing.boxes, &noExits = finishing.noExits, &exceptions = finishing.exceptions,
			&notShaped = finishing.notShaped;
		std::string &last = finishing.last, &lastRefused = finishing.lastRefused, &noExitNames = finishing.noExitNames, &notShapedNames = finishing.notShapedNames;

		// THE LANE TABLE: each doorway with a lane on both sides, a hanging alien carried through it from AI module A (heading for its exit to B) into B (heading for each of B's
		// other exits), by the stock game's own choice on the lists as written (autoplot_lane.h); then along B's ceiling from its door box back to A toward that exit
		size_t lanes = 0, continuous = 0;
		std::map<int32_t, size_t> jobOf;
		std::set<std::pair<int32_t, size_t> > brokeAt, noLaneBack;   // the doorways (AI module, exit) a lane through breaks, and those with no lane on the other side

		for (size_t j = 0; j < jobs.size(); j++)
		{
			jobOf[jobs[j].head] = j;
		}

		for (Job const &a : jobs)
		{
			for (size_t e = 0; e < a.plan.doorWords.size() && e < a.exitTo.size() && e < a.doors.size(); e++)
			{
				auto const found = jobOf.find(a.exitTo[e]);

				if (a.plan.doorWords[e].compare(0, 4, "lane") != 0 || found == jobOf.end() || IsArrival(a, e))
				{
					continue;   // (an arrival: no alien of A goes out through it)
				}

				Job const &b = jobs[found->second];
				int back = -1;

				for (size_t g = 0; g < b.exitTo.size(); g++)
				{
					// (an arrival is this exit's own when its point back is this exit's point: A may have two exits into B)
					bool const itsOwn = !IsArrival(b, g) || (g < b.backs.size() && e < a.exitsWorld.size() && b.backs[g] == a.exitsWorld[e]);

					back = b.exitTo[g] == a.head && itsOwn ? (int)g : back;
				}

				// B's own exits a lane goes on toward: the level's, other than the one back (no alien of B heads for an arrival)
				size_t onward = 0;

				for (size_t f = 0; f < b.exitsWorld.size(); f++)
				{
					onward += (int)f != back && !IsArrival(b, f) ? 1 : 0;
				}

				if (back < 0 || (size_t)back >= b.plan.doorWords.size() || b.plan.doorWords[(size_t)back].compare(0, 4, "lane") != 0 || (size_t)back >= b.plan.doorBox.size() ||
					b.plan.doorBox[(size_t)back] < 0)
				{
					noLaneBack.insert(std::make_pair(a.head, e));
					continue;   // no lane on B's side of this doorway: its own line says why
				}

				// A ROOFED DEAD END (Bishop's ruling 11-20 under the owner's rule: the lane goes INTO it and ends inside, at its far side; only stopping at its doorway is a fault):
				// the crossing toward the centre of B's box farthest from the doorway
				if (onward == 0 && !b.plan.list.empty())
				{
					host::LaneSide sideA, sideB;
					double farthest = -1.0;

					sideA.list = &a.plan.list;
					sideB.list = &b.plan.list;

					for (int i = 0; i < 3; i++)
					{
						sideA.target[i] = a.exitsWorld[e][(size_t)i];
					}

					for (host::WaypointRec const &w : b.plan.list)
					{
						double const ox = w.centre[0] * plotScale - a.doors[e].middle[0], oz = w.centre[2] * plotScale - a.doors[e].middle[2];

						if (ox * ox + oz * oz > farthest)
						{
							farthest = ox * ox + oz * oz;

							for (int i = 0; i < 3; i++)
							{
								sideB.target[i] = (int32_t)floor(w.centre[i] * plotScale + 0.5);
							}
						}
					}

					host::LaneCrossing const into = host::Lane_Cross(sideA, sideB, plotScale, a.doors[e].soffit, a.doors[e].middle, a.doors[e].through, host::kAutoPlotLaneOver,
						host::kAutoPlotLaneOver, 50);

					lanes++;
					continuous += into.continuous && a.written && b.written ? 1 : 0;

					if (!into.continuous || !a.written || !b.written)
					{
						brokeAt.insert(std::make_pair(a.head, e));
					}

					printf("AVP_OpenInteng: auto-plot: lane AI module %d (%s) exit %d -> AI module %d (%s), a dead end, into its far side: %s\n", (int)a.head,
						ModuleNameOfIndex(a.head).c_str(), (int)e, (int)b.head, ModuleNameOfIndex(b.head).c_str(), into.continuous ? "CONTINUOUS" : ("BREAKS: " + into.why).c_str());
				}

				for (size_t f = 0; f < b.exitsWorld.size(); f++)
				{
					if ((int)f == back || IsArrival(b, f))
					{
						continue;
					}

					host::LaneSide sideA, sideB;

					sideA.list = &a.plan.list;
					sideB.list = &b.plan.list;

					for (int i = 0; i < 3; i++)
					{
						sideA.target[i] = a.exitsWorld[e][(size_t)i];
						sideB.target[i] = b.exitsWorld[f][(size_t)i];
					}

					host::LaneCrossing const cross = host::Lane_Cross(sideA, sideB, plotScale, a.doors[e].soffit, a.doors[e].middle, a.doors[e].through, host::kAutoPlotLaneOver,
						host::kAutoPlotLaneOver, 50);
					host::LaneCrossing const along = host::Lane_Through(b.plan.list, plotScale, b.plan.doorBox[(size_t)back], sideB.target, b.plan.climbing);
					bool const both = cross.continuous && along.continuous && a.written && b.written;

					lanes++;
					continuous += both ? 1 : 0;

					if (!both)
					{
						brokeAt.insert(std::make_pair(a.head, e));
					}
					printf("AVP_OpenInteng: auto-plot: lane AI module %d (%s) exit %d -> AI module %d (%s) toward its exit %d (to AI module %d): %s; doorway %s; along %s\n",
						(int)a.head, ModuleNameOfIndex(a.head).c_str(), (int)e, (int)b.head, ModuleNameOfIndex(b.head).c_str(), (int)f, f < b.exitTo.size() ? (int)b.exitTo[f] : -1,
						both ? "CONTINUOUS" : "BREAKS", cross.continuous ? "continuous" : ("breaks: " + cross.why).c_str(),
						along.continuous ? "continuous" : ("breaks: " + along.why).c_str());
					printf("%s", (!a.written || !b.written) ? "AVP_OpenInteng: auto-plot: (that lane: a side was not written)\n" : "");
				}
			}
		}

		// OPEN SKY PER DOORWAY (the owner's choice 11-20): a lane may end only at a doorway with no ceiling over the floor just past it. Every doorway into a plotted AI module
		// with a ceiling there where the lane does not go on is a fault: no lane on this side, none on the other, or a lane through it that breaks
		size_t endsUnderCeiling = 0, endsAtOpenSky = 0;

		for (Job const &a : jobs)
		{
			for (size_t e = 0; e < a.exitTo.size() && e < a.ceilingPast.size() && e < a.plan.doorWords.size(); e++)
			{
				if (jobOf.find(a.exitTo[e]) == jobOf.end() || a.ceilingPast[e] < 0 || (e < a.ceilingHere.size() && a.ceilingHere[e] == 0) || IsArrival(a, e))
				{
					continue;   // into an AI module not plotted here (a series' way out is fine), or not known; or an arrival (no lane goes out through it: A's exit is the lane)
				}

				std::pair<int32_t, size_t> const at(a.head, e);
				std::string const reason = a.plan.doorWords[e].compare(0, 4, "lane") != 0 ? "no lane on this side (" + a.plan.doorWords[e] + ")" :
					(noLaneBack.count(at) ? std::string("no lane on the other side") : (brokeAt.count(at) ? std::string("a lane through it breaks") : std::string()));

				if (reason.empty())
				{
					continue;   // the lane goes on
				}

				if (a.ceilingPast[e] == 0)
				{
					endsAtOpenSky++;
					continue;
				}

				endsUnderCeiling++;
				printf("AVP_OpenInteng: auto-plot: lane end UNDER A CEILING: AI module %d (%s) exit %d -> AI module %d (%s): %s\n", (int)a.head, ModuleNameOfIndex(a.head).c_str(),
					(int)e, (int)a.exitTo[e], ModuleNameOfIndex(a.exitTo[e]).c_str(), reason.c_str());
			}
		}

		// THE RUN'S TIME (Bishop 11-21, after the owner's 112 s whole-level plot): each AI module's searches, plan and check-and-write, for the report (the slowest found by
		// reading these lines); the AI modules not written that keep Rebellion's own waypoints, for the end line
		size_t kept = 0;

		for (Job const &job : jobs)
		{
			// ("check and write N ms" first, as Frost's timing-report.cjs reads it; then its two parts: the checker's, and the level's own write)
			// (and its WORK after: its way searches, the cells they looked at, the most one looked at: the numbers its budget is set from)
			printf("AVP_OpenInteng: auto-plot: AI module %d (%s) took: search %.0f ms, plan %.0f ms, check and write %.0f ms (the check %.0f ms, the level's write %.0f ms); "
				"work: %zu searches, %zu cells looked at, the most in one %zu\n",
				(int)job.head, job.name.c_str(), job.searchMs, job.planMs, job.writeMs, job.checkMs, job.levelWriteMs, job.searches, job.looks, job.mostLooks);
			kept += !job.written && job.stockAliens > 0 ? 1 : 0;
		}

		fflush(stdout);

		if (written)
		{
			LoadTheObjectListAgain();

			// THE MODULES' COLOURS AFTER A PLOT (the beta's last piece, from Dietrich's find 11-21): Waypoint mode 0 paints a module green when its AI module holds waypoints,
			// white when not, and nothing painted them again after the plot's write, so the plotted modules stayed white until each was picked. The AI modules written are
			// painted from the file now, by Dietrich's WaypointMode::PaintFromFile (the one his Delete Waypoints uses; the pick, the current module and the Ctrl set keep theirs)
			std::vector<int32_t> paintAgain;

			for (Job const &job : jobs)
			{
				if (job.written)
				{
					paintAgain.push_back(job.head);
				}
			}

			waypointMode.PaintFromFile(finishing.open, paintAgain);
		}

		// the end line's last words: the run's time, then the counts and sizes it plotted with (Tao 2: the words, and the log, say how the networks were made; the owner's
		// dialog sizes, Bishop 11-21: "the values used go into the plot notes and the status line's end line")
		std::string const values = host::AutoPlot_ValuesWords(runCounts);
		std::string const tail = host::AutoPlot_TimeWords(MsSince(runStart) / 1000.0) + values;

		printf("AVP_OpenInteng: auto-plot: plotted%s\n", values.substr(values.empty() ? 0 : 1).c_str());   // ("; with ..." without its ";": "plotted with climb-ins 1, ...")

		// THE WHOLE PLOT'S TIME (Frost's timing of the preview, 11-21), printed always: from Ok to the last module written, its frames, and the preview's own drawing
		printf("AVP_OpenInteng: auto-plot: the whole plot took %.0f ms (from Ok to the last module written), %zu frames, preview %s, preview drawing %.0f ms\n", MsSince(runStart),
			plotFrames, host::PlotPreview_On() ? "on" : "off", previewDrawMs);

		// IN STEP (autoplot_handoff.h): what waiting for the window cost the plot (the owner's lockstep, measured: Bishop 11-21)
		printf("AVP_OpenInteng: auto-plot: in step with the preview: the plot waited for the window %zu times, %.0f ms in all\n", handOff.Waits(), handOff.WaitedMs());

		Say(everyOne ? "Auto-plot: " + Number((long long)written) + " AI modules written, " + Number((long long)refused) + " refused" + host::AutoPlot_KeptWords(kept) + host::AlienAutoWaypoint_LinkedKeptWords(runLinkedKept) + (notShaped ? ", " + Number((long long)notShaped) +
			" not shaped (no network written: " + notShapedNames + ")" : std::string()) + (noExits ? ", " + Number((long long)noExits) +
			" with no exits (nothing to plot: " + noExitNames + ")" : std::string()) + (exceptions ? ", " + Number((long long)exceptions) + " checker exception(s) by rule 6(a)" :
			// (Bishop 11-20: the lane numbers are the editor's own rough count, not the lane gate's; the gate, Drake's lanes.exe, is the measure until the shared lane rules
			// replace this table: on the stock copy this said 34 ends under a ceiling where the gate says 0)
			std::string()) + "; " + Number((long long)boxes) + " boxes; lanes (the editor's rough count; the measure is the lane gate) " +
			Number((long long)continuous) + " of " + Number((long long)lanes) + " continuous, ends under a ceiling " + Number((long long)endsUnderCeiling) + ", at open sky " +
			Number((long long)endsAtOpenSky) + (refused ? " (the last refused: " + lastRefused + ")" : std::string()) + tail : last + tail);
		gLast = AutoPlotResult{ true, true, written, refused, kept + runLinkedKept, std::string() };
	}
}

// AutoPlotCommand_Run's list (Vasquez's "Alien auto waypoint" menu), taken in place of the Ctrl+click set while that call runs
static std::vector<int32_t> const *gRunModules = nullptr;

// the owner's counts: the next run's (AutoPlotCommand_Run's), taken as the run's when it starts; any other start takes the defaults
static host::AutoPlotVolumeCounts gNextCounts, gRunCounts;

// the dialog's Overwrite box for the next run (AutoPlotCommand_Run's); any other start overwrites, as before
static bool gNextOverwrite = true;
static bool gNextAnswered = false;   // AutoPlotCommand_RunAnswered: the replace question answered Yes by the caller's own form

bool AutoPlotCommand(int32_t command, int32_t pickedModule)
{
	if (command != host::kWaypointAutoPlotModule && command != host::kWaypointAutoPlotAll && command != host::kWaypointAutoPlotSelected)
	{
		return false;
	}

	// the counts this call brings (AutoPlotCommand_Run's), else the defaults; kept while a run goes (a call that starts none leaves the run's as they were)
	host::AutoPlotVolumeCounts const counts = gNextCounts;

	gNextCounts = host::AutoPlotVolumeCounts();

	bool const overwriteLinked = gNextOverwrite;

	gNextOverwrite = true;

	bool const answered = gNextAnswered;

	gNextAnswered = false;

	if (stage == Stage::Idle)
	{
		gRunCounts = counts;
		runCounts = counts;
	}

	// hidden by the owner's ruling (autoplot_switch.h): the items are not shown, and their command, however it comes (a key, the harness's item), does nothing
	if (host::AutoPlot_CommandHidden(command, host::AutoPlot_Shown()))
	{
		return true;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (stage != Stage::Idle)
	{
		Say("Auto-plot: a run is going: wait for its end");
		return true;
	}

	if (!open || !open->Level())
	{
		Say("Auto-plot: no level is held to work on");
		return true;
	}

	if (waypointMode.Mode() != 0)
	{
		Say("Auto-plot: from the AI modules (Exit to them first)");
		return true;
	}

	std::map<int32_t, std::vector<int32_t> > groups;
	std::map<int32_t, size_t> alienCounts;

	if (!AiModules(open, &groups, &alienCounts) || groups.empty())
	{
		Say("Auto-plot: the level's modules can't be read");
		return true;
	}

	runStock = alienCounts;   // (Rebellion's own alien waypoints in each, for the end line's "kept Rebellion's")

	std::vector<int32_t> heads;
	bool const selected = command == host::kWaypointAutoPlotSelected;

	everyOne = command == host::kWaypointAutoPlotAll || selected;   // a run over many: its summary on the status line

	if (command == host::kWaypointAutoPlotAll)
	{
		for (auto const &one : groups)
		{
			heads.push_back(one.first);
		}
	}
	else if (selected)
	{
		// the selected set, as AI module heads: Waypoint mode 0's Ctrl+click set in the order added, else the pick alone (waypoint_mode.h WaypointMode_SelectedModules; Vasquez 11-19),
		// or AutoPlotCommand_Run's list; each module named is taken to the head of the AI module that holds it, once
		for (int32_t const module : gRunModules ? *gRunModules : host::WaypointMode_SelectedModules())
		{
			for (auto const &one : groups)
			{
				if (std::find(one.second.begin(), one.second.end(), module) != one.second.end() && std::find(heads.begin(), heads.end(), one.first) == heads.end())
				{
					heads.push_back(one.first);
				}
			}
		}

		if (heads.empty())
		{
			Say("Auto-plot selected AI modules: select the AI modules first");
			return true;
		}
	}
	else
	{
		int32_t const picked = pickedModule;
		int32_t head = -1;

		for (auto const &one : groups)
		{
			head = std::find(one.second.begin(), one.second.end(), picked) != one.second.end() ? one.first : head;
		}

		if (picked < 0 || head < 0)
		{
			Say("Auto-plot AI module: pick a module first");
			return true;
		}

		heads.push_back(head);
	}

	// THE OVERWRITE BOX (alien_auto_waypoint.h, the owner 11-21: off as the Alien auto waypoint dialog opens): an AI module that already has alien waypoints, most likely
	// Rebellion's own (or ours from an earlier plot: the file doesn't say whose), is kept as it is: not plotted, not asked about, counted on the end line
	std::vector<int32_t> keptLinked;

	host::AlienAutoWaypoint_SplitLinked(std::vector<int32_t>(heads), alienCounts, overwriteLinked, &heads, &keptLinked);

	if (heads.empty())
	{
		Say("Auto-plot: 0 AI modules written, 0 refused" + host::AlienAutoWaypoint_LinkedKeptWords(keptLinked.size()) +
			" (tick Overwrite already linked modules to plot them)");
		gLast = AutoPlotResult{ true, true, 0, 0, keptLinked.size(), std::string() };
		return true;
	}

	runLinkedKept = keptLinked.size();

	// alien waypoints there already (an artist's, maybe): replaced only after a Yes
	size_t replaced = 0, modulesWithSome = 0;

	for (int32_t const head : heads)
	{
		auto const found = alienCounts.find(head);

		if (found != alienCounts.end() && found->second > 0)
		{
			replaced += found->second;
			modulesWithSome++;
		}
	}

	if (replaced == 0)
	{
		Begin(open, heads, groups);
		return true;
	}

	// the caller's own form asked already (AutoPlotCommand_RunAnswered: All stock maps' Overwrite tick): no question, as on a Yes
	if (answered)
	{
		Begin(open, heads, groups);
		return true;
	}

	std::string const question = everyOne ? std::string(selected ? "Auto-plot selected AI modules" : "Auto-plot all AI modules") + ": replace the " + Number((long long)replaced) +
		" alien waypoints of " + Number((long long)modulesWithSome) +
		" AI modules with the auto-plot's?" : "Auto-plot AI module " + Number(heads[0]) + ": replace its " + Number((long long)replaced) + " alien waypoints with the auto-plot's?";

	host::Dialogs_Confirm(question, [heads, groups](bool yes)
	{
		OpenLevelFile *const again = LevelObjects_OpenForEdit();

		if (!yes || !again || !again->Level())
		{
			Say("Auto-plot: nothing changed");
			gLast = AutoPlotResult{ true, false, 0, 0, 0, "nothing changed" };
			return;
		}

		Begin(again, heads, groups);
	});
	return true;
}

void AutoPlotCommand_Frame()
{
	if (stage != Stage::Idle)
	{
		plotFrames++;   // (the whole plot's time line)
	}

	// the plot's own share of this frame: its time in here, by every way out, to the stage the frame began in
	struct OwnClock
	{
		Stage at;
		std::chrono::steady_clock::time_point from;

		~OwnClock()
		{
			if (at != Stage::Idle)
			{
				stageOwn[(int)at - 1].push_back(MsSince(from));
			}
		}
	} const ownClock{ stage, std::chrono::steady_clock::now() };

	// THE PLOT'S FRAMES: the last plot frame's time, to the stage it began in; the frame after the end says them all
	{
		auto const now = std::chrono::steady_clock::now();

		if (lastFrameStage != Stage::Idle)
		{
			stageFrames[(int)lastFrameStage - 1].push_back(std::chrono::duration<double, std::milli>(now - lastFrameAt).count());

			if (stage == Stage::Idle)
			{
				printf("AVP_OpenInteng: auto-plot: the plot's frames: %s; %s; %s; %s\n", host::AutoPlot_FrameWords("start", stageFrames[3]).c_str(),
					host::AutoPlot_FrameWords("grid", stageFrames[0]).c_str(), host::AutoPlot_FrameWords("search", stageFrames[1]).c_str(),
					host::AutoPlot_FrameWords("writing", stageFrames[2]).c_str());
				printf("AVP_OpenInteng: auto-plot: the plot's own share of them: %s; %s; %s; %s\n", host::AutoPlot_FrameWords("start", stageOwn[3]).c_str(),
					host::AutoPlot_FrameWords("grid", stageOwn[0]).c_str(), host::AutoPlot_FrameWords("search", stageOwn[1]).c_str(),
					host::AutoPlot_FrameWords("writing", stageOwn[2]).c_str());
				fflush(stdout);
			}
		}

		lastFrameAt = now;
		lastFrameStage = stage;
	}

	// THE START, A SLICE A FRAME (see StartingRun): the jobs made on the level the run was asked on, kAutoPlotStartSliceMs a frame, then the grid started
	if (stage == Stage::Starting)
	{
		if (!starting.open || LevelObjects_OpenForEdit() != starting.open)
		{
			stage = Stage::Idle;
			Say("Auto-plot: stopped: another level was opened while the AI modules were being prepared (" + Number((long long)starting.at) + " of " +
				Number((long long)starting.heads.size()) + " done, nothing written)");
			gLast = AutoPlotResult{ true, false, 0, 0, 0, "another level was opened while the AI modules were being prepared" };
			return;
		}

		auto const sliceFrom = std::chrono::steady_clock::now();

		while (starting.at < starting.heads.size() && MsSince(sliceFrom) < kAutoPlotStartSliceMs)
		{
			BeginOne(starting.heads[starting.at++]);
		}

		if (starting.at < starting.heads.size())
		{
			Say("Auto-plot: preparing AI module " + Number((long long)starting.at + 1) + " of " + Number((long long)starting.heads.size()));
			return;
		}

		BeginEnd();
		return;
	}

	if (stage == Stage::Grid)
	{
		std::string why;

		if (!host::AutoPlotGrid_Ready(&why))
		{
			if (why != "building")
			{
				stage = Stage::Idle;
				Say("Auto-plot: the pathfinder failed: " + why);
				gLast = AutoPlotResult{ true, false, 0, 0, 0, "the pathfinder failed: " + why };
			}

			return;
		}

		// THE PREPARATION is the worker's first step now (Prepare); the ways' count with it
		wayAt = 0;
		wayCount = 0;
		jobAt = 0;
		workerDone = false;
		stopAsked = false;
		runPreview = host::PlotPreview_On();   // (read here, before the worker starts: the worker reads only this copy of it)
		runBudget = moduleBudget;              // (the same)
		preparing = true;   // (set before the worker starts as well as in Work, so no frame between sees the search begun before its preparation)
		handOff.Start();
		worker = std::thread(Work);
		stage = Stage::Searching;
		return;
	}

	if (stage == Stage::Searching)
	{
		if (!workerDone)
		{
			if (preparing)
			{
				Say("Auto-plot: finding the doorways and ceilings of " + Number((long long)jobs.size()) + " AI modules");
				return;
			}

			size_t const at = jobAt;
			std::shared_ptr<host::AutoPlotShown const> const showing = runPreview ? handOff.Newest() : nullptr;

			// the module planned, and what the picture beside it shows (the same module, or the words say which: the owner 11-21, through Bishop)
			Say(host::AutoPlot_ProgressWords(at, jobs.size(), at < jobs.size() ? jobs[at].name : std::string(), wayAt, wayCount) +
				(showing ? host::AutoPlot_PictureWords(at, showing->job, showing->words) : std::string()));
			return;
		}

		worker.join();
		finishing = Finishing();
		finishing.open = LevelObjects_OpenForEdit();
		stage = Stage::Writing;

		// THE LAST MODULE SHOWN (Frost 11-21: a one-room plot never showed its module in the preview): the writing starts on the NEXT frame. Before, this frame
		// went on to write, and a plot whose last module was planned and written within one frame went Idle before the preview's turn in the frame
		// (plot_preview_view.h), so that module was never drawn. One frame more for every plot
		Say(host::AutoPlot_WritingWords(0, jobs.size(), jobs.empty() ? std::string() : jobs[0].name));
		return;
	}

	if (stage != Stage::Writing)
	{
		return;
	}

	// THE WRITING, A SLICE A FRAME: into the level open when it began, and only while that one is still open (the frames between let anything else happen)
	if (!finishing.open || LevelObjects_OpenForEdit() != finishing.open)
	{
		stage = Stage::Idle;
		Say("Auto-plot: stopped: another level was opened while the networks were being saved (" + Number((long long)finishing.written) + " of " +
			Number((long long)jobs.size()) + " AI modules done)");
		gLast = AutoPlotResult{ true, false, 0, 0, 0, "another level was opened while the networks were being saved" };
		return;
	}

	// the writing's first frame: every network checked, the passed ones written in one go; the reports from the next frame on
	if (!finishing.writtenAll)
	{
		WriteAll();
		Say(host::AutoPlot_WritingWords(0, jobs.size(), jobs.empty() ? std::string() : jobs[0].name));
		return;
	}

	auto const sliceFrom = std::chrono::steady_clock::now();

	while (finishing.at < jobs.size() && MsSince(sliceFrom) < kAutoPlotWriteSliceMs)
	{
		FinishOne(jobs[finishing.at++]);
	}

	if (finishing.at < jobs.size())
	{
		Say(host::AutoPlot_WritingWords(finishing.at, jobs.size(), jobs[finishing.at].name));
		return;
	}

	stage = Stage::Idle;
	FinishEnd();
}

bool AutoPlotCommand_Busy()
{
	return stage != Stage::Idle;
}

// (the most a setting takes is the caps' own most: kept equal to them here, so a change of either cap changes it too or doesn't build)
static_assert(host::kAutoPlotMostModuleLooksSet == host::kAutoPlotMostPairs * 2 * host::kAutoPlotMostLooks, "kAutoPlotMostModuleLooksSet is the caps' own most");

void AutoPlotCommand_SetModuleBudget(size_t looks)
{
	// lowered or raised by the user's own settings file (a map can't set it); 0 keeps the budget; past the caps' own most, held there
	moduleBudget = looks == 0 ? host::kAutoPlotMostModuleLooks : (looks > host::kAutoPlotMostModuleLooksSet ? host::kAutoPlotMostModuleLooksSet : looks);
}

size_t AutoPlotCommand_ModuleBudget()
{
	return moduleBudget;
}

bool AutoPlotCommand_BusyCursor()
{
	if (stage == Stage::Idle)
	{
		return false;
	}

	SetCursor(LoadCursor(NULL, IDC_APPSTARTING));
	return true;
}

bool AutoPlotCommand_AirductOn(int32_t module, size_t *flagged, size_t *of, std::string *name)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	std::map<int32_t, std::vector<int32_t> > groups;
	std::map<int32_t, size_t> alienCounts;

	if (!flagged || !of || !name || !open || !open->Level() || !AiModules(open, &groups, &alienCounts))
	{
		return false;
	}

	for (auto const &group : groups)
	{
		if (group.first != module && std::find(group.second.begin(), group.second.end(), module) == group.second.end())
		{
			continue;
		}

		// (the plot's own way: FinishOne's Airduct, each module of the AI module by its id)
		std::vector<uint8_t> ids;

		for (int32_t const member : group.second)
		{
			uint8_t id[host::kSelectionIdBytes];

			if (host::ModuleIdOfIndex(LevelObjects_Level(), member, id))
			{
				ids.insert(ids.end(), id, id + host::kSelectionIdBytes);
			}
		}

		size_t done = 0;
		bool const set = !ids.empty() && host::StrategyMode_SetModuleFlag(open, ids.data(), ids.size() / host::kSelectionIdBytes, host::kAutoPlotAirduct, true, &done) ==
			host::StrategyModeResult::Done;

		*flagged = done;
		*of = group.second.size();
		*name = ModuleNameOfIndex(group.first);

		if (set)
		{
			LoadTheObjectListAgain();
		}

		return set;
	}

	return false;
}

bool AutoPlotCommand_Run(std::vector<int32_t> const &modules)
{
	return AutoPlotCommand_Run(modules, host::AutoPlotVolumeCounts());
}

bool AutoPlotCommand_Run(std::vector<int32_t> const &modules, host::AutoPlotVolumeCounts const &counts, bool overwriteLinked)
{
	gLast = AutoPlotResult();
	gNextCounts = counts;
	gNextOverwrite = overwriteLinked;

	if (modules.empty())
	{
		return AutoPlotCommand(host::kWaypointAutoPlotAll, -1);
	}

	gRunModules = &modules;

	bool const done = AutoPlotCommand(host::kWaypointAutoPlotSelected, -1);

	gRunModules = nullptr;
	return done;
}

host::AutoPlotVolumeCounts AutoPlotCommand_RunCounts()
{
	return gRunCounts;
}

// THE PLOT'S PREVIEW, IN STEP WITH THE PLOT (autoplot_handoff.h, plot_preview_view.h): the hand-over the window takes each module's copy from
host::AutoPlotHandOff &AutoPlotCommand_HandOff()
{
	return handOff;
}

void AutoPlotCommand_PreviewDrew(double ms)
{
	previewDrawMs += ms > 0.0 ? ms : 0.0;

	if (stage != Stage::Idle)
	{
		previewSeen = true;   // the preview has drawn a frame of this run: the start's message goes
	}
}

// THE START'S MESSAGE (alpha.5 item 1, the owner 11-22: "if we have some background running task while the little right bottom screen loads then we put a big
// message on the screen 'Loading X%' or 'Calculating some stuff' until the little screen shows"): the words of the stage the run is in, from the click until the
// preview's first frame (or, with the preview off, until the search begins: the status line goes on from there), with a % where the stage has a count
std::string AutoPlotCommand_Message()
{
	auto const percent = [](size_t at, size_t of) { return Number((long long)(of == 0 ? 0 : (at >= of ? 100 : at * 100 / of))) + "%"; };
	std::string const words = "Calculating the alien waypoints: ";

	switch (stage)
	{
	case Stage::Starting:
		return words + "the AI modules " + percent(starting.at, starting.heads.size());   // each head's job made (BeginOne)
	case Stage::Grid:
		return words + "the grid";   // the pathfinder's own thread: no count of its own (autoplot_grid.h), so no %
	case Stage::Searching:
		if (workerDone)
		{
			return std::string();
		}

		if (preparing)
		{
			return words + "the doorways " + percent(prepareAt, prepareOf);   // Prepare's two loops, each job once in each
		}

		return runPreview && !previewSeen ? words + "the first AI module" : std::string();   // the search begun, the preview's first frame not drawn yet
	default:
		return std::string();
	}
}

std::vector<std::string> AutoPlotCommand_MessageForms()
{
	std::string const words = "Calculating the alien waypoints: ";

	return { words + "the AI modules 100%", words + "the grid", words + "the doorways 100%", words + "the first AI module" };
}

void AutoPlotCommand_MessageDrawn(std::string const &words, std::string const &box)
{
	messageShown = words;
	messageBox = words.empty() ? std::string() : box;
}

std::string const &AutoPlotCommand_MessageShown()
{
	return messageShown;
}

std::string const &AutoPlotCommand_MessageBox()
{
	return messageBox;
}

bool AutoPlotCommand_ItemWaits(int32_t action)
{
	if (stage == Stage::Idle)
	{
		return false;
	}

	host::IntengItem const *const item = host::IntengMenus_Find(action);

	if (item && !host::AutoPlot_ItemWaits(item->work))
	{
		return false;   // (an item that only looks, or Quit)
	}

	Say(host::AutoPlot_WaitWords(item ? item->name : nullptr));
	return true;
}

bool AutoPlotCommand_Waits(char const *what)
{
	if (stage == Stage::Idle)
	{
		return false;
	}

	Say(host::AutoPlot_WaitWords(what));
	return true;
}

void AutoPlotCommand_Stop()
{
	// (the same as the WorkerEnd at the program's end, but now, while the level and the grid the worker reads are still there: the order the program's own
	// ends run in, file by file, is not fixed)
	stopAsked = true;
	handOff.Stop();   // (a worker waiting for the window to draw its module goes on at once, and ends)

	if (worker.joinable())
	{
		worker.join();
	}
}

AutoPlotResult AutoPlotCommand_LastResult()
{
	return gLast;
}

bool AutoPlotCommand_RunAnswered(std::vector<int32_t> const &modules, host::AutoPlotVolumeCounts const &counts)
{
	gNextAnswered = true;

	bool const done = AutoPlotCommand_Run(modules, counts, true);

	gNextAnswered = false;   // a call that started nothing leaves no answer behind for the next
	return done;
}
