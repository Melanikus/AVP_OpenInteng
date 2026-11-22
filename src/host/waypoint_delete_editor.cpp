// waypoint_delete_editor.cpp: DELETE WAYPOINTS, the command on Waypoint mode's bar (waypoint_delete.h; the owner 11-21). Delete Waypoints: the Ctrl-selected AI modules,
// else the picked one; Delete All Waypoints: every AI module of the level. The count asked first (Dialogs_Confirm); on Yes the AI modules' alien lists are emptied in ONE
// write (WaypointMode::DeleteAlienLists: one new image and one loader read for all of them, so a level plotted whole is not read hundreds of times; each ground list kept;
// the WAYPOINT chunk taken out when there is no ground list, as Rebellion's files hold none; the colours painted again from the file). Nothing is lost until the file is saved.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "waypoint_delete.h"

#include <string>
#include <vector>

#include "autoplot_command.h"
#include "dialogs.h"
#include "editor_shared.h"
#include "level_file.h"
#include "level_objects.h"
#include "object_edit.h"
#include "waypoint_mode.h"

namespace
{
	std::string sayWords;   // the status line's words (statusWords copies them)

	void Say(std::string const &words)
	{
		sayWords = words;
		statusWords = sayWords.c_str();
	}
}

bool host::WaypointDeleteCommand(int32_t command, int32_t pickedModule)
{
	if (command != kWaypointDeleteWaypoints && command != kWaypointDeleteAllWaypoints)
	{
		return false;
	}

	bool const all = command == kWaypointDeleteAllWaypoints;
	char const *const name = all ? "Delete All Waypoints" : "Delete Waypoints";

	if (AutoPlotCommand_Busy())
	{
		Say(std::string(name) + ": an auto-plot run is going: wait for its end");
		return true;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		Say(std::string(name) + ": no level open");
		return true;
	}

	if (waypointMode.Mode() != 0)
	{
		Say(std::string(name) + ": from the AI modules (Exit to them first)");
		return true;
	}

	std::vector<ModuleSnapshot> snapshots;

	if (ReadModuleSnapshots(open, &snapshots) != ObjectEditResult::Done)
	{
		Say(std::string(name) + ": the level's modules can't be read");
		return true;
	}

	// the Ctrl+click set (Waypoint mode 0's, in the order added), else the pick alone
	std::vector<int32_t> modules = WaypointMode_SelectedModules();

	if (modules.empty() && pickedModule >= 0)
	{
		modules.push_back(pickedModule);
	}

	if (!all && modules.empty())
	{
		Say("Delete Waypoints: pick an AI module, or Ctrl+click several, first");
		return true;
	}

	WaypointDeletePlan const plan = WaypointDelete_Plan(snapshots, modules, all);

	if (plan.heads.empty())
	{
		Say(std::string(name) + (all ? ": no AI module has alien waypoints" : ": the picked AI module has no alien waypoints") +
			(plan.unreadable > 0 ? " (" + std::to_string((unsigned long long)plan.unreadable) + " could not be read)" : std::string()));
		return true;
	}

	Dialogs_Confirm(WaypointDelete_Question(plan, all), [plan, name](bool yes)
	{
		OpenLevelFile *const again = LevelObjects_OpenForEdit();

		if (!yes || !again || !again->Level())
		{
			Say(std::string(name) + ": nothing changed");
			return;
		}

		std::vector<int32_t> heads;

		for (WaypointDeleteHead const &one : plan.heads)
		{
			heads.push_back(one.head);
		}

		// one write for all of them (the loader takes it all or none), the colours after it from the file
		size_t deleted = 0;
		WaypointOutcome const done = waypointMode.DeleteAlienLists(again, heads, &deleted);

		Say(done.levelChanged ? WaypointDelete_Done(deleted, heads.size()) : std::string(name) + ": nothing changed (" + done.say + ")");
	});
	return true;
}
