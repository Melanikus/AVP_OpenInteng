// See waypoint_mode.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_mode.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <exception>
#include <map>
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
		zoomSteps = 0;
		lastOpen = nullptr;
		dragFaces = 0;
		group.clear();
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

			// the module the editor picked, and the master of its AI module (itself when it has none)
			int32_t picked = context.pickedModule;
			int32_t pickedMaster = -1;

			if (Snapshots(open, &modules) && picked >= 0 && modules.count(picked))
			{
				pickedMaster = modules[picked].masterIndex >= 0 ? modules[picked].masterIndex : picked;
			}
			else
			{
				picked = -1;
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

				// FUN_00583c10: back to the AI modules (the waypoints are in the level already: named)
				mode = 0;
				wpCurrent = -1;
				wpBase = -1;
				set = WaypointSet();
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
					if (picked < 0)
					{
						return Said("Edit Module: pick a module in the view first");
					}

					current = pickedMaster;
					RefreshGroup(open);
					out.say = "Edit Module: " + Words();
					return out;

				case 0x1a:   // Add Module: another AI module's master brings its whole AI module
				{
					if (current < 0)
					{
						return Said("Add Module: Edit Module first (the AI module to add to)");
					}

					if (picked < 0)
					{
						return Said("Add Module: pick the module to add in the view");
					}

					int32_t const target = pickedMaster != current ? pickedMaster : picked;
					AiModulesCounts counts;
					AiModulesResult const result = AiModules_Add(open, current, target, &counts, &why);

					if (result != AiModulesResult::Done)
					{
						return Said((std::string("Add Module: ") + AiModulesWords(result) + (why.empty() ? std::string() : " (" + why + ")")).c_str());
					}

					RefreshGroup(open);
					out.levelChanged = true;
					out.say = "Add Module: " + std::to_string(1 + counts.slavesMoved) + " modules joined, " + std::to_string(counts.waypointsMoved) + " waypoints moved; " + Words();
					return out;
				}

				case 0x1b:   // Remove Module: a module of the current AI module
				{
					if (current < 0 || picked < 0 || pickedMaster != current || picked == current)
					{
						return Said("Remove Module: pick a module of the current AI module (not its master)");
					}

					AiModulesResult const result = AiModules_Remove(open, picked, nullptr, &why);

					if (result != AiModulesResult::Done)
					{
						return Said((std::string("Remove Module: ") + AiModulesWords(result) + (why.empty() ? std::string() : " (" + why + ")")).c_str());
					}

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
				case 4:      // Show Alien / Ground Waypoints (FUN_00583b40)
				{
					int32_t const master = current >= 0 ? current : pickedMaster;

					if (master < 0)
					{
						return Said("Show Waypoints: pick a module in the view (or Edit Module) first");
					}

					if (!Load(open, master, &why))
					{
						return Said(("The waypoints could not be read: " + why).c_str());
					}

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

					if (wpBase < 0 || (size_t)wpBase >= List().size())
					{
						return Said("Linking: there is no base waypoint");
					}

					if (picked < 0 || !NameAndPlace(open, picked, &name, place) || name.empty() || name.size() > 62)
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
				if (command == 0x13)   // Finish Extent Edit (FUN_00584af0)
				{
					mode = 1;
					dragFaces = 0;
					centreEdit = false;
					zoomSteps = 0;
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
					out.say = "Zoom: " + std::to_string(zoomSteps);
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

					mode = 2;
					dragFaces = 0;
					centreEdit = false;
					zoomSteps = 0;
					wpBase = -1;
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
					int32_t place[3];
					std::string name;

					if (!NameAndPlace(open, current, &name, place))
					{
						return Said("Create Waypoint: the AI module's master is not in the level");
					}

					wpCurrent = Waypoints_Create(&list, place);
					out.say = "Waypoint " + std::to_string(wpCurrent) + " made at " + name + "'s place";
					changed = true;
					break;
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

		if (mode != 2 || wpCurrent < 0 || (size_t)wpCurrent >= List().size() || pixelFile <= 0)
		{
			return false;
		}

		WaypointRec const &wp = List()[(size_t)wpCurrent];

		// Centre Edit (the flag the "R" item flips): the press takes the aim point of the axes the pane shows (FUN_00585750 0x585ac2-0x585bb8: bits 0x40 / 0x80 / 0x100)
		if (centreEdit)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if (axisMask & (1 << axis))
				{
					dragFaces |= 0x40 << axis;
				}
			}

			dragBackup = set;
			return dragFaces != 0;
		}

		// FUN_00585750 0x585e74-0x585fd0: on each axis the pane shows, the lowest face when it is within 3 pixels, else the highest
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
				dragFaces |= 1 << (2 * axis);
			}
			else if ((nearHigh < 0 ? -nearHigh : nearHigh) / pixelFile < 3)
			{
				dragFaces |= 2 << (2 * axis);
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
		if (dragFaces == 0 || mode != 2 || wpCurrent < 0 || (size_t)wpCurrent >= List().size())
		{
			return;
		}

		WaypointRec &wp = Active()[(size_t)wpCurrent];

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
		int32_t snapped[3] = { SnapTo(0, place[0]), SnapTo(1, place[1]), SnapTo(2, place[2]) };

		DragTo(snapped);
	}

	void WaypointMode::Zoom(int delta)
	{
		if (mode == 2)
		{
			zoomSteps = zoomSteps + delta > 8 ? 8 : (zoomSteps + delta < -8 ? -8 : zoomSteps + delta);
		}
	}

	WaypointOutcome WaypointMode::EndDrag(WaypointContext const &context)
	{
		if (dragFaces == 0 || mode != 2 || wpCurrent < 0 || (size_t)wpCurrent >= List().size())
		{
			dragFaces = 0;
			return Said("");
		}

		dragFaces = 0;

		OpenLevelFile *const open = context.open;
		WaypointRec &wp = Active()[(size_t)wpCurrent];

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
