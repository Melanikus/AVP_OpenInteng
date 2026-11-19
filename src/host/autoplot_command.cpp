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
#include <thread>

#include "autoplot_command.h"
#include "autoplot_switch.h"   // the owner's "hide auto pilot" (11-19): nothing done unless AutoPlot=1
#include "autoplot_grid.h"
#include "autoplot_plan.h"
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
		std::string why;                            // the worker's own failure, if any
	};

	enum class Stage
	{
		Idle,
		Grid,        // the pathfinder's grid building (autoplot_grid.h, its own thread)
		Searching,   // our worker searching and planning
	};

	Stage stage = Stage::Idle;
	bool everyOne = false;
	double plotScale = 1.0;
	std::vector<Job> jobs;
	std::thread worker;
	std::atomic<bool> workerDone(false), stopAsked(false);
	std::atomic<size_t> jobAt(0), wayAt(0), wayCount(0);
	std::string commandWords;

	// a run still going when the editor ends is stopped and waited for (a joinable thread at exit ends the program)
	struct WorkerEnd
	{
		~WorkerEnd()
		{
			stopAsked = true;

			if (worker.joinable())
			{
				worker.join();
			}
		}
	} workerEnd;

	void Say(std::string const &words)
	{
		commandWords = words;
		statusWords = commandWords.c_str();
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

	// the worker: every job's ways (alien and floor, each pair of goals), then its plan; it reads only its jobs and calls only the grid (one caller) and pure sums
	void Work()
	{
		for (size_t j = 0; j < jobs.size() && !stopAsked; j++)
		{
			Job &job = jobs[j];

			jobAt = j;

			try
			{
				std::vector<host::AutoPlotWay> ways;
				size_t pairs = 0;

				for (size_t a = 0; a < job.goals.size() && pairs < host::kAutoPlotMostPairs && !stopAsked; a++)
				{
					for (size_t b = a + 1; b < job.goals.size() && pairs < host::kAutoPlotMostPairs && !stopAsked; b++, pairs++)
					{
						for (bool const alien : { true, false })
						{
							std::vector<host::AutoPlotPoint> points;
							std::string why;

							if (host::AutoPlotGrid_Way(job.goals[a].data(), job.goals[b].data(), alien, &points, &why))
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
							}

							wayAt++;
						}
					}
				}

				job.plan = host::AutoPlot_Plan(ways, job.exitsWorld, plotScale);
				job.planned = !stopAsked;
			}
			catch (std::exception const &)
			{
				job.why = "out of memory";
			}
		}

		workerDone = true;
	}

	void Begin(OpenLevelFile *open, std::vector<int32_t> const &heads, std::map<int32_t, std::vector<int32_t> > const &groups)
	{
		std::map<int32_t, int32_t> gameNumbers;

		host::GameModuleNumbers(open->Level(), &gameNumbers);
		plotScale = local_scale > 0.0 ? local_scale : 1.0;
		jobs.clear();

		for (int32_t const head : heads)
		{
			Job job;
			std::vector<host::EntryMarker> markers;

			job.head = head;

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
				}
			}

			jobs.push_back(job);
		}

		std::string why;

		if (!host::AutoPlotGrid_Start(open, &why))
		{
			Say("Auto-plot: the pathfinder would not start: " + why);
			return;
		}

		stage = Stage::Grid;
		Say("Auto-plot: building the grid");
	}

	// the networks the worker made: each checked as the stock game would take it, the good ones written over their AI module's alien list
	void Finish()
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		size_t written = 0, refused = 0, boxes = 0;
		std::string last;

		for (Job const &job : jobs)
		{
			std::string const name = "Auto-plot: AI module " + Number(job.head);

			if (!job.planned || job.plan.list.empty())
			{
				refused++;
				last = name + " not written: " + (!job.why.empty() ? job.why : (job.planned ? std::string("no way was found through it") : std::string("the run was stopped")));
				continue;
			}

			std::vector<host::WaypointProblem> const problems = host::Waypoints_Check(job.plan.list, job.exitsFile, plotScale, 50);

			if (!problems.empty())
			{
				refused++;
				last = name + " not written: " + problems[0].words + (problems.size() > 1 ? " (and " + Number((long long)problems.size() - 1) + " more)" : std::string());
				continue;
			}

			host::WaypointOutcome const done = waypointMode.AutoPlotWrite(open, job.head, job.plan.list);

			if (!done.levelChanged)
			{
				refused++;
				last = done.say;
				continue;
			}

			written++;
			boxes += job.plan.list.size();
			last = name + ": " + host::AutoPlot_Words(job.plan);
		}

		if (written)
		{
			LoadTheObjectListAgain();
		}

		Say(everyOne ? "Auto-plot: " + Number((long long)written) + " AI modules written, " + Number((long long)refused) + " refused; " + Number((long long)boxes) + " boxes" +
			(refused ? " (the last refused: " + last + ")" : std::string()) : last);
	}
}

bool AutoPlotCommand(int32_t command, int32_t pickedModule)
{
	if (command != host::kWaypointAutoPlotModule && command != host::kWaypointAutoPlotAll)
	{
		return false;
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

	std::vector<int32_t> heads;

	everyOne = command == host::kWaypointAutoPlotAll;

	if (everyOne)
	{
		for (auto const &one : groups)
		{
			heads.push_back(one.first);
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

	std::string const question = everyOne ? "Auto-plot all AI modules: replace the " + Number((long long)replaced) + " alien waypoints of " + Number((long long)modulesWithSome) +
		" AI modules with the auto-plot's?" : "Auto-plot AI module " + Number(heads[0]) + ": replace its " + Number((long long)replaced) + " alien waypoints with the auto-plot's?";

	host::Dialogs_Confirm(question, [heads, groups](bool yes)
	{
		OpenLevelFile *const again = LevelObjects_OpenForEdit();

		if (!yes || !again || !again->Level())
		{
			Say("Auto-plot: nothing changed");
			return;
		}

		Begin(again, heads, groups);
	});
	return true;
}

void AutoPlotCommand_Frame()
{
	if (stage == Stage::Grid)
	{
		std::string why;

		if (!host::AutoPlotGrid_Ready(&why))
		{
			if (why != "building")
			{
				stage = Stage::Idle;
				Say("Auto-plot: the pathfinder failed: " + why);
			}

			return;
		}

		// the goals: each AI module's exits and a few places a marine stands in its modules (StandSpots: on this thread, before the worker takes the grid)
		size_t total = 0;

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

			size_t const n = job.goals.size();

			total += 2 * (std::min)(n * (n > 0 ? n - 1 : 0) / 2, host::kAutoPlotMostPairs);
		}

		wayAt = 0;
		wayCount = total;
		jobAt = 0;
		workerDone = false;
		stopAsked = false;
		worker = std::thread(Work);
		stage = Stage::Searching;
		return;
	}

	if (stage != Stage::Searching)
	{
		return;
	}

	if (!workerDone)
	{
		size_t const at = jobAt;

		Say("Auto-plot: AI module " + Number(at < jobs.size() ? jobs[at].head : -1) + ", way " + Number((long long)wayAt) + " of " + Number((long long)wayCount));
		return;
	}

	worker.join();
	stage = Stage::Idle;
	Finish();
}

bool AutoPlotCommand_Busy()
{
	return stage != Stage::Idle;
}
