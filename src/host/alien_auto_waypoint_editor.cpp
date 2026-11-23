// alien_auto_waypoint_editor.cpp: the editor's side of the "Alien auto waypoint" menu (alien_auto_waypoint.h): Modules' line dialog, the typed names read on the level, and the run.
// Environment is kWaypointAutoPlotAll, which autoplot_command.cpp runs (the same call as AutoPlotCommand_Run with an empty list). Written 2026-11-20 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RUN (agreed with Dietrich 11-20): his planner's entry point, AutoPlotCommand_Run (autoplot_command.h), with the typed modules' object indices: each is taken to the head
// of the AI module that holds it, once, and the volume shape is tried first for each (autoplot_volumes.h).
#include "alien_auto_waypoint.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "all_stock_maps_screen.h"   // All stock maps' disclaimer (Drake)
#include "all_stock_maps_ui.h"       // kWaypointAlienAutoAllStockMaps
#include "chunk.hpp"
#include "chunk_object.hpp"
#include "dialogs.h"
#include "editor_shared.h"
#include "level_file.h"
#include "level_objects.h"
#include "object_edit.h"
#include "autoplot_command.h"
#include "autoplot_switch.h"
#include "waypoint_mode.h"

namespace
{
	std::string sayWords;   // the status line's words (statusWords copies them)

	void Say(std::string const &words)
	{
		sayWords = words;
		statusWords = sayWords.c_str();
	}

	// the level's objects (name, object index, module or not) and each module's AI module (its master, else itself)
	bool ReadTheLevel(OpenLevelFile *open, std::vector<host::AlienAutoWaypointObject> *objects, std::map<int32_t, std::string> *names, std::map<int32_t, int32_t> *headOf)
	{
		std::vector<host::ModuleSnapshot> snapshots;

		if (!open || !open->Level() || host::ReadModuleSnapshots(open, &snapshots) != host::ObjectEditResult::Done)
		{
			return false;
		}

		std::map<int32_t, host::ModuleSnapshot const *> byIndex;

		for (host::ModuleSnapshot const &s : snapshots)
		{
			byIndex[s.index] = &s;
		}

		List<Object_Chunk *> list;
		open->Level()->list_objects(&list);

		for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
		{
			Object_Chunk *const o = each();
			host::AlienAutoWaypointObject one;

			one.index = o->object_data.index_num;
			one.name = o->object_data.o_name ? o->object_data.o_name : "";

			auto const found = byIndex.find(one.index);

			one.module = found != byIndex.end() && found->second->module;

			if (one.module)
			{
				(*headOf)[one.index] = found->second->masterIndex >= 0 ? found->second->masterIndex : one.index;
			}

			(*names)[one.index] = one.name;
			objects->push_back(one);
		}

		return true;
	}

	// THE RUN: Dietrich's entry point (see the file's head) with the counts and the Overwrite box (off: the AI modules already linked are kept); false only when a run is
	// going already
	void RunTheModules(std::vector<int32_t> const &modules, host::AutoPlotVolumeCounts const &counts, bool overwriteLinked)
	{
		if (!AutoPlotCommand_Run(modules, counts, overwriteLinked))
		{
			Say("Alien auto waypoint: a run is going: wait for its end");
		}
	}

	// THE DIALOG (alien_auto_waypoint.h, AlienAutoWaypoint_PlotForm: inteng's own parts, the counts and the Airduct box). names: Modules' start words; withNames false:
	// Environment's. then(answers) on Ok
	void OpenThePlotDialog(bool withNames, std::string const &names, std::function<void(host::AlienAutoWaypointPlotAnswers const &)> const &then)
	{
		host::Dialogs_Form(host::AlienAutoWaypoint_PlotForm(withNames, names), [withNames, then](int32_t answer, std::vector<host::DialogWidget> const &widgets)
		{
			if (answer != host::kDialogOk)
			{
				return;
			}
			host::AlienAutoWaypointPlotAnswers answers;
			if (!host::AlienAutoWaypoint_ReadPlotForm(withNames, widgets, &answers))
			{
				Say("Alien auto waypoint: nothing plotted: the dialog lost its fields");
				return;
			}
			then(answers);
		});
	}

	// the two counts and the Airduct tick read; false (named on the status line, nothing plotted) when a count is not a whole number in the planner's range
	bool ReadTheCounts(host::AlienAutoWaypointPlotAnswers const &answers, host::AutoPlotVolumeCounts *counts)
	{
		std::string why;
		if (!host::AlienAutoWaypoint_PlotCounts(answers, counts, &why))
		{
			Say("Alien auto waypoint: nothing plotted: " + why);
			return false;
		}
		return true;
	}
}

bool host::AlienAutoWaypointCommand(int32_t command, int32_t pickedModule)
{
	if (command != kWaypointAlienAutoModules && command != kWaypointAlienAutoEnvironment && command != kWaypointAlienAutoAllStockMaps)
	{
		return false;
	}

	if (!AutoPlot_Shown())
	{
		return true;   // hidden (the owner's "hide auto pilot", autoplot_switch.h): it does nothing
	}

	if (AutoPlotCommand_Busy())
	{
		Say("Alien auto waypoint: a run is going: wait for its end");
		return true;
	}

	// ALL STOCK MAPS (the owner's wishlist item 5, Drake's all_stock_maps_screen.h): the disclaimer first; it needs no level of its own open
	if (command == kWaypointAlienAutoAllStockMaps)
	{
		AllStockMaps_Open();
		return true;
	}

	if (!LevelObjects_OpenForEdit())
	{
		Say("Alien auto waypoint: no level open");
		return true;
	}

	// ENVIRONMENT: every AI module of the level, with the counts
	if (command == kWaypointAlienAutoEnvironment)
	{
		OpenThePlotDialog(false, std::string(), [](AlienAutoWaypointPlotAnswers const &answers)
		{
			AutoPlotVolumeCounts counts;
			if (ReadTheCounts(answers, &counts))
			{
				RunTheModules({}, counts, answers.overwrite);
			}
		});
		return true;
	}

	// MODULES: the typed names, started with the Ctrl+click set (its AI modules' masters), else the picked module as Auto-plot AI module took it (the Entry points markers' pick)
	std::vector<AlienAutoWaypointObject> objects;
	std::map<int32_t, std::string> names;
	std::map<int32_t, int32_t> headOf;

	if (!ReadTheLevel(LevelObjects_OpenForEdit(), &objects, &names, &headOf))
	{
		Say("Alien auto waypoint: no level open");
		return true;
	}

	std::vector<int32_t> start = WaypointMode_SelectedModules();

	if (start.size() <= 1 && pickedModule >= 0)
	{
		start = { pickedModule };
	}

	OpenThePlotDialog(true, AlienAutoWaypoint_StartWords(start, names), [](AlienAutoWaypointPlotAnswers const &answers)
	{
		std::vector<AlienAutoWaypointObject> now;
		std::map<int32_t, std::string> nowNames;
		std::map<int32_t, int32_t> nowHeads;

		if (!ReadTheLevel(LevelObjects_OpenForEdit(), &now, &nowNames, &nowHeads))
		{
			Say("Alien auto waypoint: no level open");
			return;
		}

		std::vector<int32_t> modules;
		std::string why;

		if (!AlienAutoWaypoint_Parse(answers.names, now, &modules, &why))
		{
			Say("Alien auto waypoint: nothing plotted: " + why);
			return;
		}

		AutoPlotVolumeCounts counts;

		if (ReadTheCounts(answers, &counts))
		{
			RunTheModules(modules, counts, answers.overwrite);
		}
	});
	return true;
}
