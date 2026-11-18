// See waypoint_mode.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_mode.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "object_edit.h"
#include "ai_modules_edit.h"
#include "adjacency_generate.h"

namespace host
{
	namespace
	{
		// The module snapshots by object index (the first module of a number)
		bool Snapshots(OpenLevelFile *open, std::map<int32_t, ModuleSnapshot> *out)
		{
			std::vector<ModuleSnapshot> all;

			if (ReadModuleSnapshots(open, &all) != ObjectEditResult::Done)
			{
				return false;
			}

			for (ModuleSnapshot const &snapshot : all)
			{
				if (snapshot.module && !out->count(snapshot.index))
				{
					(*out)[snapshot.index] = snapshot;
				}
			}

			return true;
		}

		// The object's name and place (the file's numbers) by its index
		bool NameAndPlace(OpenLevelFile *open, int32_t index, std::string *name, int32_t place[3])
		{
			List<Object_Chunk *> all;

			open->Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && (int32_t)object->object_data.index_num == index)
				{
					size_t length = 0;

					while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
					{
						length++;
					}

					if (name)
					{
						name->assign(object->object_data.o_name, length);
					}

					place[0] = (int32_t)object->object_data.location.x;
					place[1] = (int32_t)object->object_data.location.y;
					place[2] = (int32_t)object->object_data.location.z;
					return true;
				}
			}

			return false;
		}

		WaypointOutcome Said(char const *words)
		{
			WaypointOutcome out;

			out.say = words;
			out.page = -1;
			out.leaveScreen = false;
			out.levelChanged = false;
			out.askExtents = false;
			return out;
		}
	}

	WaypointMode::WaypointMode()
	{
		Enter();
	}

	void WaypointMode::Enter()
	{
		mode = 0;
		current = -1;
		alien = false;
		set = WaypointSet();
		wpCurrent = -1;
		wpBase = -1;
		highlight = 0;
		centreEdit = false;
		minimised = false;
		viewScale = 5000;   // FUN_00581650 0x582d0f
		viewsMade = 0;
		viewCentre[0] = viewCentre[1] = viewCentre[2] = 0;
		axisFixed[0] = axisFixed[1] = axisFixed[2] = true;
		sizingMask = 0;
		lastOpen = nullptr;
		dragFaces = 0;
		group.clear();
		picked = -1;
		lastSeenPick = -1;
		paint.clear();
		paintMade = false;
		droppedModuleLinks = 0;
	}

	// FUN_005846b0: the three views of Edit Extents, centred on the picked module's place (after Show Waypoints the AI module's master: DAT_0091bed4 +0x10..+0x18)
	void WaypointMode::MakeViews(OpenLevelFile *open)
	{
		int32_t place[3] = { 0, 0, 0 };

		if (open && open->Level() && NameAndPlace(open, current, nullptr, place))
		{
			viewCentre[0] = place[0];
			viewCentre[1] = place[1];
			viewCentre[2] = place[2];
		}

		viewsMade++;
	}

	namespace
	{
		// FUN_00588b50: the master of a module's AI module; a module with no AIMODSLA is its own
		int32_t MasterOf(std::map<int32_t, ModuleSnapshot> const &modules, int32_t module)
		{
			const std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.find(module);

			return it != modules.end() && it->second.masterIndex >= 0 ? it->second.masterIndex : module;
		}

		// FUN_00581430: the module's MODULEDT holds a WAYPOINT
		bool HoldsWaypoints(std::map<int32_t, ModuleSnapshot> const &modules, int32_t module)
		{
			const std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.find(module);

			return it != modules.end() && it->second.hasWaypoint;
		}
	}

	// FUN_00588e60(module, colour): the master of the module's AI module and every module whose AIMODSLA names it, one colour
	void WaypointMode::PaintAiModule(OpenLevelFile *open, int32_t module, uint32_t colour)
	{
		std::map<int32_t, ModuleSnapshot> modules;

		if (module < 0 || !Snapshots(open, &modules))
		{
			return;
		}

		int32_t const master = MasterOf(modules, module);

		paint[master] = colour;

		for (std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.begin(); it != modules.end(); ++it)
		{
			if (it->second.masterIndex == master)
			{
				paint[it->first] = colour;
			}
		}
	}

	// FUN_00581650 0x58169b-0x581714: every AI module green when its master's MODULEDT holds a WAYPOINT, else white
	void WaypointMode::MakeEntryPaint(OpenLevelFile *open)
	{
		std::map<int32_t, ModuleSnapshot> modules;

		paint.clear();
		paintMade = open && open->Level() && Snapshots(open, &modules);

		if (!paintMade)
		{
			return;
		}

		for (std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.begin(); it != modules.end(); ++it)
		{
			int32_t const master = MasterOf(modules, it->first);

			paint[it->first] = HoldsWaypoints(modules, master) ? kWaypointModuleGreen : kWaypointModuleWhite;
		}
	}

	std::map<int32_t, uint32_t> const &WaypointMode::ModulePaint(OpenLevelFile *open)
	{
		if (!paintMade)
		{
			MakeEntryPaint(open);
		}

		return paint;
	}

	// The click of the AI modules mode (FUN_00583d00 0x58402a-0x5841e4)
	void WaypointMode::Click(OpenLevelFile *open, int32_t module)
	{
		lastSeenPick = module;

		if (mode != 0 || !open || !open->Level())
		{
			return;
		}

		ModulePaint(open);

		std::map<int32_t, ModuleSnapshot> modules;

		if (!Snapshots(open, &modules))
		{
			return;
		}

		// the pick let go of: one module of the current AI module back to the current's colour; another AI module (its master) back to green or white
		if (picked >= 0)
		{
			if (MasterOf(modules, picked) == current)
			{
				paint[picked] = kWaypointModuleCurrent;
			}
			else
			{
				PaintAiModule(open, picked, HoldsWaypoints(modules, picked) ? kWaypointModuleGreen : kWaypointModuleWhite);
			}
		}

		picked = -1;

		if (module < 0 || !modules.count(module))
		{
			return;
		}

		int32_t const master = MasterOf(modules, module);

		if (master == current)
		{
			picked = module;
			paint[module] = kWaypointModuleRed;
		}
		else
		{
			picked = master;
			PaintAiModule(open, master, kWaypointModuleRed);
		}
	}

	// Edit Module (0x19, FUN_00583d00 0x583df3-0x583e80): the current AI module back to green or white, the picked one's AI module current (0xffffffff), the pick let go
	WaypointOutcome WaypointMode::EditModule(OpenLevelFile *open)
	{
		std::map<int32_t, ModuleSnapshot> modules;

		Snapshots(open, &modules);

		if (current >= 0)
		{
			PaintAiModule(open, current, HoldsWaypoints(modules, current) ? kWaypointModuleGreen : kWaypointModuleWhite);
		}

		current = -1;
		group.clear();

		if (picked < 0)
		{
			return Said("Edit Module: nothing picked, so no AI module is current");
		}

		current = MasterOf(modules, picked);
		picked = -1;
		PaintAiModule(open, current, kWaypointModuleCurrent);
		RefreshGroup(open);

		WaypointOutcome out = Said("");

		out.say = "Edit Module: " + Words();
		return out;
	}

	std::vector<WaypointRec> &WaypointMode::Active()
	{
		return alien ? set.alien : set.ground;
	}

	std::vector<WaypointRec> const &WaypointMode::List() const
	{
		return alien ? set.alien : set.ground;
	}

	void WaypointMode::RefreshGroup(OpenLevelFile *open)
	{
		group.clear();

		std::map<int32_t, ModuleSnapshot> modules;

		if (current < 0 || !Snapshots(open, &modules))
		{
			return;
		}

		group.push_back(current);

		for (std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.begin(); it != modules.end(); ++it)
		{
			if (it->second.masterIndex == current)
			{
				group.push_back(it->first);
			}
		}
	}

	bool WaypointMode::Load(OpenLevelFile *open, int32_t master, std::string *why)
	{
		std::map<int32_t, ModuleSnapshot> modules;

		if (!Snapshots(open, &modules) || !modules.count(master))
		{
			*why = "that is not a module of the level";
			return false;
		}

		set = WaypointSet();

		ModuleSnapshot const &snapshot = modules[master];

		if (snapshot.hasWaypoint && !Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, why))
		{
			set = WaypointSet();
			return false;
		}

		current = master;
		wpCurrent = -1;
		wpBase = -1;
		RefreshGroup(open);

		// W-6 (Bishop 11-17: copy inteng's drop 1:1): FUN_005867c0 0x58694b-0x586a5e keeps only the module links whose name is a module's of the level (the bytes compared exactly); the others
		// are dropped when the list is read, and the write-back of the mode's Exit (FUN_00583c10 -> FUN_00587210) leaves them out of the file. The 63 stock levels hold no module link.
		std::set<std::string> names;
		::List<Object_Chunk *> all;

		open->Level()->list_objects(&all);
		modulePlaces.clear();

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && modules.count((int32_t)object->object_data.index_num))
			{
				size_t length = 0;

				while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
				{
					length++;
				}

				names.insert(std::string(object->object_data.o_name, length));

				// W-4: the module's place, where its module-link lines end (FUN_00586700: the module record's +0x10..+0x18; the first of a name, as the name compare finds it)
				std::vector<int32_t> &place = modulePlaces[std::string(object->object_data.o_name, length)];

				if (place.empty())
				{
					place = { object->object_data.location.x, object->object_data.location.y, object->object_data.location.z };
				}
			}
		}

		droppedModuleLinks = 0;

		for (std::vector<WaypointRec> *each : { &set.alien, &set.ground })
		{
			for (WaypointRec &wp : *each)
			{
				size_t const had = wp.moduleLinks.size();

				wp.moduleLinks.erase(std::remove_if(wp.moduleLinks.begin(), wp.moduleLinks.end(), [&](WaypointModuleLink const &named) { return !names.count(named.name); }), wp.moduleLinks.end());
				droppedModuleLinks += had - wp.moduleLinks.size();
			}
		}

		return true;
	}

	// FUN_00587210's write-back: the module's WAYPOINT chunk is taken out and, when the lists are not both empty, a new one is put at the end of its MODULEDT
	bool WaypointMode::Write(OpenLevelFile *open, std::string *why)
	{
		ModuleChildEdit edit;

		edit.objectIndex = current;
		memset(edit.name, 0, sizeof(edit.name));
		memcpy(edit.name, "WAYPOINT", 8);
		edit.remove = set.alien.empty() && set.ground.empty();

		if (!edit.remove)
		{
			edit.chunk = Waypoints_Serialize(set);
		}

		std::vector<ModuleChildEdit> edits;
		std::vector<bool> wrote;

		edits.push_back(edit);

		ObjectEditResult const result = SetModuleChildrenMany(open, edits, &wrote);

		if (result != ObjectEditResult::Done || wrote.size() != 1 || !wrote[0])
		{
			*why = result != ObjectEditResult::Done ? ObjectEditWords(result) : "the module's waypoints could not be written";
			return false;
		}

		return true;
	}

	bool WaypointMode::PickBox(int32_t index)
	{
		// mode 1's click, and Edit Extents' press in the 3D quarter (FUN_00585750 0x585cff-0x585e3f; nothing while a new box is being sized, 0x585cff of the sizing branch)
		if ((mode != 1 && !(mode == 2 && !Sizing())) || index < 0 || (size_t)index >= List().size() || index == wpCurrent || index == wpBase)
		{
			return false;
		}

		wpCurrent = index;
		return true;
	}

	bool WaypointMode::PickNear(int32_t const place[3], int axisMask)
	{
		std::vector<WaypointRec> const &list = List();

		if ((mode != 1 && mode != 2) || list.empty())
		{
			return false;
		}

		int best = -1;
		bool bestInside = false;
		double bestDistance = 0.0;

		for (size_t i = 0; i < list.size(); i++)
		{
			WaypointRec const &wp = list[i];
			bool inside = true;
			double distance = 0.0;

			for (int axis = 0; axis < 3; axis++)
			{
				if (!(axisMask & (1 << axis)))
				{
					continue;
				}

				int64_t const low = wp.min[axis];
				int64_t const high = wp.max[axis];

				inside = inside && place[axis] >= low && place[axis] <= high;

				double const d = (double)place[axis] - (double)wp.centre[axis];

				distance += d * d;
			}

			if (best < 0 || (inside && !bestInside) || (inside == bestInside && distance < bestDistance))
			{
				best = (int)i;
				bestInside = inside;
				bestDistance = distance;
			}
		}

		wpCurrent = best;
		return best >= 0;
	}

	void WaypointMode::Step(int delta)
	{
		std::vector<WaypointRec> const &list = List();

		if ((mode != 1 && mode != 2) || list.empty())
		{
			return;
		}

		int64_t const count = (int64_t)list.size();
		int64_t const from = wpCurrent < 0 ? (delta > 0 ? -1 : 0) : wpCurrent;

		wpCurrent = (int32_t)((from + delta % count + count) % count);
	}

	bool WaypointMode::ModulePlace(std::string const &name, int32_t place[3]) const
	{
		auto const found = modulePlaces.find(name);

		if (found == modulePlaces.end() || found->second.size() != 3)
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			place[axis] = found->second[(size_t)axis];
		}

		return true;
	}

	std::string WaypointMode::Words() const
	{
		if (mode == 0)
		{
			return current >= 0 ? "Waypoint mode: AI module " + std::to_string(current) + " (" + std::to_string(group.size()) + " modules)" : std::string("Waypoint mode: pick a module, then Edit Module");
		}

		return std::string("Waypoints of AI module ") + std::to_string(current) + ": " + (alien ? "alien" : "ground") + " list of " + std::to_string(List().size()) + (wpCurrent >= 0 ? ", current " + std::to_string(wpCurrent) : std::string()) +
			(wpBase >= 0 ? ", base " + std::to_string(wpBase) : std::string()) + (highlight == kLinkAlien ? ", Highlight Alien Link" : (highlight == kLinkDeviation ? ", Highlight Deviation Link" : ""));
	}

	WaypointOutcome WaypointMode::Command(int32_t command, WaypointContext const &context)
	{
		OpenLevelFile *const open = context.open;

		if (!open || !open->Level())
		{
			return Said("No level is held to work on");
		}

		lastOpen = open;

		try
		{
			std::map<int32_t, ModuleSnapshot> modules;

			// a pick the editor made since the last one seen is a click (mode 0)
			ModulePaint(open);

			if (mode == 0 && context.pickedModule != lastSeenPick)
			{
				Click(open, context.pickedModule);
			}

			// the picked module (inteng's DAT_0091bed4) and the master of its AI module (itself when it has none)
			int32_t pickedMaster = -1;

			if (Snapshots(open, &modules) && picked >= 0 && modules.count(picked))
			{
				pickedMaster = MasterOf(modules, picked);
			}

			WaypointOutcome out = Said("");
			std::string why;

			if (command == 2)
			{
				if (mode == 0)
				{
					out.leaveScreen = true;
					out.say = "Waypoint mode left";
					return out;
				}

				// FUN_00583c10 -> FUN_00587210: the write-back; ours has written each change already (named), so only a list that lost module links when it was read (W-6) is written here
				if (droppedModuleLinks > 0)
				{
					out.levelChanged = Write(open, &why);
					droppedModuleLinks = 0;
				}

				// FUN_00583c10: back to the AI modules (the waypoints are in the level already: named), with no AI module current and the one whose waypoints were shown picked, RED (0x583cbc)
				mode = 0;
				wpCurrent = -1;
				wpBase = -1;
				set = WaypointSet();
				picked = current;
				current = -1;
				group.clear();
				lastSeenPick = context.pickedModule;
				PaintAiModule(open, picked, kWaypointModuleRed);
				out.page = 0;
				out.say = "AI modules: " + Words();
				return out;
			}

			if (command == 0x15)
			{
				if (mode == 2)
				{
					out.say = "Left click and drag on side to alter extents. Hold down shift while dragging to snap extents to the nearest module or waypoint extent. < and > zoom the views";
					return out;
				}

				out.say = mode == 0 ? "Pick a module in the view, then Edit Module (the AI module of it), Add Module / Remove Module, Show Alien / Ground Waypoints"
					: "Pick a waypoint in the views (or the keys for next and previous), then Set Base, the Add Link items, Delete Link, Create / Delete Waypoint, Can Crawl";
				return out;
			}

			if (mode == 0)
			{
				switch (command)
				{
				case 0x25:   // Entry points > Fly (FUN_0058b550): the movement keys move the picked entry-point marker instead of the camera. NOT BUILT: the markers (FUN_00588260) and the write-back of a moved one (FUN_0058a6b0) are not
					return Said("Fly (entry points): not built yet: the entry-point markers and the write-back of a moved one are not read");

				case 0x19:   // Edit Module
					return EditModule(open);

				case 0x1a:   // Add Module (0x583e9b): the picked module (another AI module's master brings its whole AI module) joins the current one, which is painted 0xffffffff; the pick let go
				{
					if (current < 0)
					{
						return Said("Add Module: Edit Module first (the AI module to add to)");
					}

					if (picked < 0 || picked == current)
					{
						return Said("Add Module: pick the module to add in the view");
					}

					int32_t const target = picked;
					AiModulesCounts counts;
					AiModulesResult const result = AiModules_Add(open, current, target, &counts, &why);

					picked = -1;
					PaintAiModule(open, current, kWaypointModuleCurrent);

					if (result != AiModulesResult::Done)
					{
						return Said((std::string("Add Module: ") + AiModulesWords(result) + (why.empty() ? std::string() : " (" + why + ")")).c_str());
					}

					PaintAiModule(open, current, kWaypointModuleCurrent);   // the joined modules too, read from the new level
					RefreshGroup(open);
					out.levelChanged = true;
					out.say = "Add Module: " + std::to_string(1 + counts.slavesMoved) + " modules joined, " + std::to_string(counts.waypointsMoved) + " waypoints moved; " + Words();
					return out;
				}

				case 0x1b:   // Remove Module (0x583ef9): a module of the current AI module goes; it is painted green or white by its own WAYPOINT and the pick is let go
				{
					// inteng's gate (0x583f05-0x583f23): a pick and a current AI module, the pick's master the current one; a master is its own master, so the MASTER itself passes (W-9: its
					// second branch, the first slave becomes the master); so does a plain module made current by Edit Module (nothing written, no AI module current after)
					if (current < 0 || picked < 0 || (pickedMaster != current && picked != current))
					{
						return Said("Remove Module: pick a module of the current AI module");
					}

					int32_t const removed = picked;
					int32_t newMaster = -1;
					AiModulesResult const result = AiModules_Remove(open, removed, nullptr, &why, &newMaster);

					if (result != AiModulesResult::Done && !(result == AiModulesResult::NotASlave && removed == current))
					{
						return Said((std::string("Remove Module: ") + AiModulesWords(result) + (why.empty() ? std::string() : " (" + why + ")")).c_str());
					}

					std::map<int32_t, ModuleSnapshot> after;

					Snapshots(open, &after);
					picked = -1;

					// the current AI module (0x583f3f-0x583f73): none, or the master the function gave back (the first slave, when the master itself was taken out)
					if (removed == current)
					{
						current = newMaster;
					}
					paint[removed] = HoldsWaypoints(after, removed) ? kWaypointModuleGreen : kWaypointModuleWhite;
					RefreshGroup(open);
					out.levelChanged = true;
					out.say = "Remove Module done; " + Words();
					return out;
				}

				case 0x24:   // Entry points > Delete (FUN_0058a0e0): the adjacency between the AI modules of the first two picked modules
				{
					if (context.pickedModules.size() < 2)
					{
						return Said("Entry points > Delete: pick two modules (Shift+click): the AI modules of them lose the entry points between them");
					}

					size_t removed = 0;
					AiModulesResult const result = AiModules_DeleteEntryPoints(open, context.pickedModules[0], context.pickedModules[1], &removed, &why);

					if (result != AiModulesResult::Done)
					{
						return Said((std::string("Entry points > Delete: ") + AiModulesWords(result) + (why.empty() ? std::string() : " (" + why + ")")).c_str());
					}

					out.levelChanged = removed > 0;
					out.say = "Entry points > Delete: " + std::to_string(removed) + " adjacency entries between the two AI modules taken out";
					return out;
				}

				case 0x26:   // Recalculate all (FUN_00588770 + FUN_00403d50 + FUN_00588260): every entry point worked out again
				{
					int64_t const worked = Adjacency_RecalculateEntryPoints(open);

					if (worked < 0)
					{
						return Said("Recalculate all: the level could not be read");
					}

					out.levelChanged = true;
					out.say = "Recalculate all: " + std::to_string(worked) + " entry points worked out";
					return out;
				}

				case 3:
				case 4:      // Show Alien / Ground Waypoints (FUN_00583b40): the PICKED module's AI module; with nothing picked, Edit Module's work (0x583daf / 0x583dd4 -> 0x583df3)
				{
					if (picked < 0)
					{
						return EditModule(open);
					}

					int32_t const master = pickedMaster;

					if (!Load(open, master, &why))
					{
						return Said(("The waypoints could not be read: " + why).c_str());
					}

					// 0x583b95-0x583bba: the pick becomes the master, painted white (the other modules are hidden in mode 1: W-2)
					picked = master;
					PaintAiModule(open, master, kWaypointModuleWhite);
					mode = 1;
					alien = command == 3;
					out.page = 1;
					out.say = Words();
					return out;
				}

				default:
					return Said("That item is not built yet");
				}
			}

			// the minimise toggle (0x1c, FUN_00585750 0x5859d4 / FUN_00584ee0 0x5853b4: every waypoint but the current one gets its minimised bit flipped and its box drawn again)
			if (command == 0x1c && (mode == 1 || mode == 2))
			{
				minimised = !minimised;
				out.say = minimised ? "Minimise: on (only the current waypoint's box is drawn)" : "Minimise: off (every waypoint's box is drawn)";
				return out;
			}

			// mode 3: LINKING (FUN_00584d80): the module picked is linked to the BASE waypoint, or unlinked
			if (mode == 3)
			{
				if (command == 0x12)   // Finish Linking (FUN_00584cf0)
				{
					mode = 1;
					out.page = 1;
					out.say = "Linking finished: " + Words();
					return out;
				}

				if (command == 8 || command == 0x10)
				{
					std::string name;
					int32_t place[3];
					int32_t const linkPick = context.pickedModule >= 0 && modules.count(context.pickedModule) ? context.pickedModule : -1;   // mode 3's own click (FUN_00584d80: FUN_00559720)

					if (wpBase < 0 || (size_t)wpBase >= List().size())
					{
						return Said("Linking: there is no base waypoint");
					}

					if (linkPick < 0 || !NameAndPlace(open, linkPick, &name, place) || name.empty() || name.size() > 62)
					{
						return Said("Linking: pick a module in the view first");
					}

					WaypointSet const backupSet = set;
					std::vector<WaypointModuleLink> &links = Active()[(size_t)wpBase].moduleLinks;
					size_t at = 0;

					while (at < links.size() && links[at].name != name)
					{
						at++;
					}

					if (command == 8)
					{
						if (at < links.size())
						{
							return Said("Linking: that module is linked to the base waypoint already");
						}

						WaypointModuleLink made;

						made.name = name;
						made.flags = 0;
						links.push_back(made);
						out.say = "Waypoint " + std::to_string(wpBase) + " is linked to " + name;
					}
					else
					{
						if (at >= links.size())
						{
							return Said("Linking: that module is not linked to the base waypoint");
						}

						links.erase(links.begin() + (std::ptrdiff_t)at);
						out.say = "Waypoint " + std::to_string(wpBase) + " is not linked to " + name + " any more";
					}

					std::string why;

					if (!Write(open, &why))
					{
						set = backupSet;
						return Said(("The waypoints could not be written: " + why).c_str());
					}

					out.levelChanged = true;
					return out;
				}

				return Said("Linking: Add Link, Delete Link or Finish Linking");
			}

			// mode 2: EXTENTS
			if (mode == 2)
			{
				// while the new box is being sized inteng reads no menu (FUN_00585750 0x585778: the sizing jumps past the switch)
				if (Sizing())
				{
					return Said("");
				}

				if (command == 0x13)   // Finish Extent Edit (FUN_00584af0)
				{
					mode = 1;
					dragFaces = 0;
					centreEdit = false;
					out.page = 1;
					out.say = "Edit Extents finished: " + Words();
					return out;
				}

				if (command == 0x1f)   // Centre Edit (FUN_005888c0 / FUN_00588920 by the flag at 0x91bf4c)
				{
					centreEdit = !centreEdit;
					out.say = centreEdit ? "Centre Edit: on (a drag moves the place aliens aim for)" : "Centre Edit: off (a drag moves a side of the box)";
					return out;
				}

				if (command == 0x20 || command == 0x21)
				{
					Zoom(command == 0x20 ? 1 : -1);   // 0x20 takes 1000 units off a pixel (zoom in), 0x21 adds them (zoom out)
					out.say = "Zoom: " + std::to_string(viewScale) + " a pixel";
					return out;
				}

				if (command != 5 && command != 6 && command != 0x18 && command != 0x15 && command != 2)
				{
					return Said("Not while editing extents: Finish Extent Edit first");
				}
			}

			// mode 1
			if (mode == 1 || mode == 2)
			{
				WaypointSet const backupSet = set;
				int32_t const backupCurrent = wpCurrent;
				int32_t const backupBase = wpBase;
				std::vector<WaypointRec> &list = Active();
				int32_t const cur = wpCurrent;
				int32_t const base = wpBase;
				bool changed = false;

				switch (command)
				{
				case 0x14:   // Edit Extents (FUN_00584ee0 0x585101): mode 2, whose page is 4; the sides of the current box are dragged in the three views
					if (cur < 0)
					{
						return Said("Edit Extents: pick a waypoint first");
					}

					// 0x58510f-0x585173: the three axes fixed (no sizing), the base let go, the views made
					mode = 2;
					dragFaces = 0;
					sizingMask = 0;
					axisFixed[0] = axisFixed[1] = axisFixed[2] = true;
					centreEdit = false;
					wpBase = -1;
					MakeViews(open);
					out.page = 4;
					out.say = "Edit Extents of waypoint " + std::to_string(cur) + ": left click and drag on a side; Shift snaps; < > zoom";
					return out;

				case 0x11:   // Linking (FUN_00584cc0): only with a base waypoint (no menu item or key of inteng sends it)
					if (wpBase < 0)
					{
						return Said("Linking: set a base waypoint first");
					}

					mode = 3;
					out.page = 2;
					out.say = "Linking: pick a module, then Add Link or Delete Link";
					return out;

				case 0x18:   // Reset Waypoint Centre
					if (cur < 0)
					{
						return Said("Reset Waypoint Centre: pick a waypoint first");
					}

					return ApplyExtentsText("reset", context);

				case 5:   // Create Waypoint
				{
					// FUN_00584ee0 0x584f8f-0x5850f2 (mode 1) and FUN_00585750 0x585800-0x585929 (mode 2): an EMPTY box at the picked module's place (the AI module's master), its three axes free for
					// the sizing; from mode 1 the base is let go and Edit Extents' views are made (FUN_005846b0). It is written when its sizing is done (EndDrag).
					int32_t place[3];
					std::string name;

					if (!NameAndPlace(open, current, &name, place))
					{
						return Said("Create Waypoint: the AI module's master is not in the level");
					}

					WaypointRec made;

					for (int c = 0; c < 3; c++)
					{
						made.min[c] = made.max[c] = made.centre[c] = place[c];
					}

					made.flags = 0;
					made.spare2 = 0;
					list.push_back(made);
					wpCurrent = (int32_t)list.size() - 1;
					axisFixed[0] = axisFixed[1] = axisFixed[2] = false;
					sizingMask = 0;
					dragFaces = 0;

					if (mode == 1)
					{
						mode = 2;
						centreEdit = false;
						wpBase = -1;
						MakeViews(open);
						out.page = 4;
					}

					out.say = "Waypoint " + std::to_string(wpCurrent) + " made at " + name + "'s place: drag in two of the views to size it";
					return out;
				}

				case 6:   // Delete Waypoint
					if (cur < 0 || !Waypoints_Delete(&list, cur))
					{
						return Said("Delete Waypoint: pick a waypoint first");
					}

					if (wpBase == cur)
					{
						wpBase = -1;
					}
					else if (wpBase > cur)
					{
						wpBase--;
					}

					wpCurrent = -1;
					out.say = "Waypoint " + std::to_string(cur) + " deleted";
					changed = true;
					break;

				case 7:   // Set Base: the current waypoint becomes the base, and the links page shows
					if (cur < 0)
					{
						return Said("Set Base: pick a waypoint first");
					}

					wpBase = cur;
					wpCurrent = -1;
					out.page = 3;
					out.say = "Base is waypoint " + std::to_string(wpBase) + ": pick another, then an Add Link item";
					return out;

				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:  // Add Link, Add Link - 1 way, Add Alien Link, Add Alien Link - 1 way, Add Deviation Link, Add Deviation Link - 1 way (FUN_00586b40: base to current)
				{
					static int32_t const flags[6] = { 0, kLinkOneWay, kLinkAlien, kLinkAlien | kLinkOneWay, kLinkDeviation, kLinkDeviation | kLinkOneWay };

					if (cur < 0 || base < 0 || cur == base)
					{
						return Said("Add Link: set a base, then pick another waypoint");
					}

					Waypoints_AddLink(&list, base, cur, flags[command - 8]);
					out.say = "Link between " + std::to_string(base) + " and " + std::to_string(cur);
					changed = true;
					break;
				}

				case 0x10:   // Delete Link
					if (cur < 0 || base < 0 || !Waypoints_DeleteLink(&list, base, cur))
					{
						return Said("Delete Link: there is no link between the base and the current waypoint");
					}

					out.say = "Link between " + std::to_string(base) + " and " + std::to_string(cur) + " deleted";
					changed = true;
					break;

				case 0xe:    // Highlight link flag > Alien (FUN_00588980(4); the words "Highlight Alien Link" on the screen, FUN_00584ee0 0x585619)
				case 0xf:    // Highlight link flag > Deviation allowed (FUN_00588980(8))
				{
					int32_t const flag = command == 0xe ? kLinkAlien : kLinkDeviation;

					highlight = highlight == flag ? 0 : flag;
					out.say = Words();
					return out;
				}

				case 0x27:   // Toggle Can Crawl (the W key: flags bit 0 flipped)
					if (cur < 0)
					{
						return Said("Can Crawl: pick a waypoint first");
					}

					list[(size_t)cur].flags ^= kWaypointCanCrawl;
					out.say = std::string((list[(size_t)cur].flags & kWaypointCanCrawl) ? "Can Crawl on" : "Can Crawl off") + " for waypoint " + std::to_string(cur);
					changed = true;
					break;

				case 0x28:   // Cycle the link flag highlight (the Y key, FUN_00584ee0 0x585413: from Aliens only it goes to Deviation allowed and the other way; with none chosen nothing happens)
					if (highlight == kLinkAlien)
					{
						return Command(0xf, context);
					}

					if (highlight == kLinkDeviation)
					{
						return Command(0xe, context);
					}

					return Said("Cycle link highlight: choose Highlight link flag, Alien or Deviation allowed, first");

				case 0x16:   // Add Can Crawl
				case 0x17:   // Remove Can Crawl
					if (cur < 0)
					{
						return Said("Can Crawl: pick a waypoint first");
					}

					if (command == 0x16)
					{
						list[(size_t)cur].flags |= kWaypointCanCrawl;
					}
					else
					{
						list[(size_t)cur].flags &= ~kWaypointCanCrawl;
					}

					out.say = std::string(command == 0x16 ? "Can Crawl on" : "Can Crawl off") + " for waypoint " + std::to_string(cur);
					changed = true;
					break;

				default:
					return Said("That item is not built yet");
				}

				if (changed)
				{
					if (!Write(open, &why))
					{
						// the game's loader refused the level: nothing was changed there, and here the lists are as they were
						set = backupSet;
						wpCurrent = backupCurrent;
						wpBase = backupBase;
						return Said(("The waypoints could not be written: " + why).c_str());
					}

					out.levelChanged = true;
				}

				return out;
			}

			return Said("That item is not built yet");
		}
		catch (std::exception const &)
		{
			return Said("Out of memory");
		}
	}

	WaypointOutcome WaypointMode::ApplyExtentsText(std::string const &typed, WaypointContext const &context)
	{
		OpenLevelFile *const open = context.open;

		if (!open || !open->Level() || (mode != 1 && mode != 2) || wpCurrent < 0 || (size_t)wpCurrent >= List().size())
		{
			return Said("Edit Extents: pick a waypoint first");
		}

		try
		{
			// the words: reset, or aim x y z, or six whole numbers
			std::vector<std::string> words;
			size_t at = 0;

			while (at < typed.size())
			{
				while (at < typed.size() && (typed[at] == ' ' || typed[at] == '\t' || typed[at] == ','))
				{
					at++;
				}

				size_t const start = at;

				while (at < typed.size() && typed[at] != ' ' && typed[at] != '\t' && typed[at] != ',')
				{
					at++;
				}

				if (at > start)
				{
					words.push_back(typed.substr(start, at - start));
				}
			}

			auto number = [](std::string const &word, int32_t *value) -> bool
			{
				char *end = nullptr;
				long long const parsed = strtoll(word.c_str(), &end, 10);

				if (word.empty() || *end != '\0' || parsed > (1 << 24) || parsed < -(1 << 24))
				{
					return false;
				}

				*value = (int32_t)parsed;
				return true;
			};

			WaypointSet const backupSet = set;
			WaypointRec &wp = Active()[(size_t)wpCurrent];
			std::string say;

			if (words.size() == 1 && _stricmp(words[0].c_str(), "reset") == 0)
			{
				// Reset Waypoint Centre (0x18): the flag goes and the aim point is the middle of the box (FUN_00586e90)
				wp.flags &= ~kWaypointFlagCentreByUser;
				say = "Waypoint " + std::to_string(wpCurrent) + ": the aim point is back in the middle of the box";
			}
			else if (words.size() == 4 && _stricmp(words[0].c_str(), "aim") == 0)
			{
				int32_t v[3];

				if (!number(words[1], &v[0]) || !number(words[2], &v[1]) || !number(words[3], &v[2]))
				{
					return Said("Edit Extents: the aim point is three whole numbers, up to 16777216 either way");
				}

				for (int i = 0; i < 3; i++)
				{
					wp.centre[i] = v[i];
				}

				wp.flags |= kWaypointFlagCentreByUser;
				say = "Waypoint " + std::to_string(wpCurrent) + ": the aim point is yours now";
			}
			else if (words.size() == 6)
			{
				int32_t v[6];

				for (int i = 0; i < 6; i++)
				{
					if (!number(words[(size_t)i], &v[i]))
					{
						return Said("Edit Extents: the box is six whole numbers (lowest x y z, highest x y z), up to 16777216 either way");
					}
				}

				// each axis in order, as inteng puts them when the drag ends
				for (int axis = 0; axis < 3; axis++)
				{
					wp.min[axis] = v[axis] < v[axis + 3] ? v[axis] : v[axis + 3];
					wp.max[axis] = v[axis] < v[axis + 3] ? v[axis + 3] : v[axis];
				}

				say = "Waypoint " + std::to_string(wpCurrent) + ": the box is changed";
			}
			else
			{
				return Said("Edit Extents: type six numbers (lowest x y z, highest x y z), or aim x y z, or reset");
			}

			// FUN_00586e90: the aim point is the middle of the box unless it is the user's
			if (!(wp.flags & kWaypointFlagCentreByUser))
			{
				for (int axis = 0; axis < 3; axis++)
				{
					wp.centre[axis] = (int32_t)(((int64_t)wp.min[axis] + wp.max[axis]) / 2);
				}
			}

			std::string why;

			if (!Write(open, &why))
			{
				set = backupSet;
				return Said(("The waypoints could not be written: " + why).c_str());
			}

			WaypointOutcome out = Said(say.c_str());

			out.levelChanged = true;
			return out;
		}
		catch (std::exception const &)
		{
			return Said("Out of memory");
		}
	}

	bool WaypointMode::BeginDrag(int32_t const place[3], int axisMask, int32_t pixelFile)
	{
		dragFaces = 0;
		sizingMask = 0;

		if (mode != 2 || wpCurrent < 0 || (size_t)wpCurrent >= List().size() || pixelFile <= 0)
		{
			return false;
		}

		WaypointRec &wp = Active()[(size_t)wpCurrent];

		// SIZING the new box (FUN_00585750 0x585795-0x58582d): the free axes the pane shows get min, max and the aim point on the place; the hold then moves max (DragTo)
		if (Sizing())
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if ((axisMask & (1 << axis)) && !axisFixed[axis])
				{
					wp.min[axis] = wp.max[axis] = wp.centre[axis] = place[axis];
				}
			}

			sizingMask = axisMask;
			return true;
		}

		// Centre Edit (the flag the "R" key flips): the press puts the aim point's shown axes on the place, the user's aim point (0x585e7e-0x585ee6); no drag. EndDrag writes it.
		if (centreEdit)
		{
			dragBackup = set;

			for (int axis = 0; axis < 3; axis++)
			{
				if (axisMask & (1 << axis))
				{
					wp.centre[axis] = place[axis];
				}
			}

			wp.flags |= kWaypointFlagCentreByUser;
			dragFaces = 0x200;   // ours: a press that waits for its release to be written
			return false;
		}

		// FUN_00585750 0x585e74-0x585fd0: for x, then y, then z of the axes the pane shows, the low face when it is within 3 pixels, else the high face; inteng ASSIGNS _DAT_0091bf48, so the
		// last axis that matched is the one face taken
		for (int axis = 0; axis < 3; axis++)
		{
			if (!(axisMask & (1 << axis)))
			{
				continue;
			}

			int64_t const nearLow = (int64_t)wp.min[axis] - place[axis];
			int64_t const nearHigh = (int64_t)wp.max[axis] - place[axis];

			if ((nearLow < 0 ? -nearLow : nearLow) / pixelFile < 3)
			{
				dragFaces = 1 << (2 * axis);
			}
			else if ((nearHigh < 0 ? -nearHigh : nearHigh) / pixelFile < 3)
			{
				dragFaces = 2 << (2 * axis);
			}
		}

		if (dragFaces != 0)
		{
			dragBackup = set;
		}

		return dragFaces != 0;
	}

	void WaypointMode::DragTo(int32_t const place[3])
	{
		if (mode != 2 || wpCurrent < 0 || (size_t)wpCurrent >= List().size())
		{
			return;
		}

		WaypointRec &wp = Active()[(size_t)wpCurrent];

		// the sizing hold (0x58586a-0x5858c3 of the decompile's order: 0x585ff5's else): max of the free axes the pane shows follows; the aim point with it (FUN_00586e90)
		if (sizingMask != 0)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if ((sizingMask & (1 << axis)) && !axisFixed[axis])
				{
					wp.max[axis] = place[axis];

					if (!(wp.flags & kWaypointFlagCentreByUser))
					{
						wp.centre[axis] = (int32_t)(((int64_t)wp.min[axis] + wp.max[axis]) / 2);
					}
				}
			}

			return;
		}

		if (dragFaces == 0 || dragFaces == 0x200)
		{
			return;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (dragFaces & (1 << (2 * axis)))
			{
				wp.min[axis] = place[axis];
			}

			if (dragFaces & (2 << (2 * axis)))
			{
				wp.max[axis] = place[axis];
			}

			if (dragFaces & (0x40 << axis))
			{
				wp.centre[axis] = place[axis];
				wp.flags |= kWaypointFlagCentreByUser;
			}
		}
	}

	// FUN_00588ac0: the nearest of the sorted snap places (a tie goes to the lower one); a list with nothing in it keeps the number
	int32_t WaypointMode::SnapTo(int axis, int32_t value) const
	{
		std::vector<int32_t> places;

		std::vector<WaypointRec> const &list = List();

		for (size_t i = 0; i < list.size(); i++)
		{
			if ((int32_t)i != wpCurrent)
			{
				places.push_back(list[i].min[axis]);
				places.push_back(list[i].max[axis]);
			}
		}

		if (lastOpen && lastOpen->Level() && !group.empty())
		{
			::List<Object_Chunk *> all;

			lastOpen->Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object || std::find(group.begin(), group.end(), (int32_t)object->object_data.index_num) == group.end() || !object->get_assoc_shape())
				{
					continue;
				}

				ChunkShape const &data = object->get_assoc_shape()->shape_data;
				int32_t const at = axis == 0 ? (int32_t)object->object_data.location.x : (axis == 1 ? (int32_t)object->object_data.location.y : (int32_t)object->object_data.location.z);
				int32_t const lo = axis == 0 ? data.min.x : (axis == 1 ? data.min.y : data.min.z);
				int32_t const hi = axis == 0 ? data.max.x : (axis == 1 ? data.max.y : data.max.z);

				places.push_back((int32_t)((int64_t)at + lo));
				places.push_back((int32_t)((int64_t)at + hi));
			}
		}

		if (places.empty())
		{
			return value;
		}

		int32_t lower = -10000000;
		int32_t upper = 10000000;

		for (int32_t const place : places)
		{
			if (place == value)
			{
				return value;
			}

			if (place < value && place > lower)
			{
				lower = place;
			}

			if (place > value && place < upper)
			{
				upper = place;
			}
		}

		return (int64_t)value - lower <= (int64_t)upper - value ? lower : upper;
	}

	void WaypointMode::DragToSnapped(int32_t const place[3])
	{
		// the sizing does not snap (FUN_00588ac0 is only in the face drag, 0x585afe)
		if (sizingMask != 0)
		{
			DragTo(place);
			return;
		}

		int32_t snapped[3] = { SnapTo(0, place[0]), SnapTo(1, place[1]), SnapTo(2, place[2]) };

		DragTo(snapped);
	}

	// 0x20 (FUN_00585750 0x585a5f): 1000 off while above 1000; 0x21 (0x585a7f): 1000 more; FUN_005880e0 makes the views again with it
	void WaypointMode::Zoom(int delta)
	{
		if (mode != 2 || Sizing())   // the keys are menu commands too: none is read while a new box is sized
		{
			return;
		}

		if (delta > 0)
		{
			if (viewScale > 1000)
			{
				viewScale -= 1000;
			}
		}
		else if (delta < 0 && viewScale <= INT32_MAX - 1000)
		{
			viewScale += 1000;
		}
	}

	WaypointOutcome WaypointMode::EndDrag(WaypointContext const &context)
	{
		if ((dragFaces == 0 && sizingMask == 0) || mode != 2 || wpCurrent < 0 || (size_t)wpCurrent >= List().size())
		{
			dragFaces = 0;
			sizingMask = 0;
			return Said("");
		}

		OpenLevelFile *const open = context.open;
		WaypointRec &wp = Active()[(size_t)wpCurrent];

		// the sizing's release (FUN_00585750 0x585839-0x5858c3 of the decompile's order): the pane's axes are fixed and put in order; when all three are fixed the new box is written. NAMED: a box
		// the game's loader refuses then (an axis with no room) gets its three axes free again, to be sized anew (inteng writes the box when the mode is left, whatever it is)
		if (sizingMask != 0)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if (sizingMask & (1 << axis))
				{
					axisFixed[axis] = true;

					if (wp.max[axis] < wp.min[axis])
					{
						std::swap(wp.min[axis], wp.max[axis]);
					}
				}
			}

			sizingMask = 0;

			if (Sizing())
			{
				return Said(("Waypoint " + std::to_string(wpCurrent) + ": size it in another view").c_str());
			}

			std::string why;

			if (!open || !open->Level() || !Write(open, &why))
			{
				axisFixed[0] = axisFixed[1] = axisFixed[2] = false;
				return Said(("The new box could not be written (" + (open && open->Level() ? why : std::string("no level")) + "): size it again").c_str());
			}

			WaypointOutcome made = Said(("Waypoint " + std::to_string(wpCurrent) + " made").c_str());

			made.levelChanged = true;
			return made;
		}

		bool const centreOnly = dragFaces == 0x200;

		dragFaces = 0;

		if (centreOnly)
		{
			std::string why;

			if (!open || !open->Level() || !Write(open, &why))
			{
				set = dragBackup;
				return Said(("The aim point could not be written: " + (open && open->Level() ? why : std::string("no level"))).c_str());
			}

			WaypointOutcome aimed = Said(("Waypoint " + std::to_string(wpCurrent) + ": the aim point is set").c_str());

			aimed.levelChanged = true;
			return aimed;
		}

		// when the drag ends each axis is put in order (0x58607a-0x586100), then the aim point follows the box unless it is the user's (FUN_00586e90)
		for (int axis = 0; axis < 3; axis++)
		{
			if (wp.max[axis] < wp.min[axis])
			{
				int32_t const swap = wp.min[axis];

				wp.min[axis] = wp.max[axis];
				wp.max[axis] = swap;
			}

			if (!(wp.flags & kWaypointFlagCentreByUser))
			{
				wp.centre[axis] = (int32_t)(((int64_t)wp.min[axis] + wp.max[axis]) / 2);
			}
		}

		std::string why;

		if (!open || !open->Level() || !Write(open, &why))
		{
			set = dragBackup;
			return Said(("The box could not be written: " + (open && open->Level() ? why : std::string("no level"))).c_str());
		}

		WaypointOutcome out = Said(("Waypoint " + std::to_string(wpCurrent) + ": the box is changed").c_str());

		out.levelChanged = true;
		return out;
	}
}
