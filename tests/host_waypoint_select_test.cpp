// The test for Waypoint mode's MULTI-MODULE SELECTION (host::WaypointMode SelectToggle / Selection / ShowSelection, src/host/waypoint_mode.cpp; the lines in src/host/waypoint_view.cpp) on a
// real level. Written 2026-11-19 by Melanikus (Vasquez) for AVP_OpenInteng (the owner's ask through Bishop: "select multiple modules and apply to them", "Inteng hides unselected modules").
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_select_test.exe <a level .rif with three AI modules that hold alien waypoints> (Invasion: tests/host_waypoint_select_test.runargs). The level is only read.
// CHECKS: inteng's plain click makes no set; Ctrl+click (SelectToggle) starts the set from the pick's AI module, adds an AI module by any of its modules (its master), takes one out again (its
// colour back), and does nothing outside mode 0; the set is painted red; a plain click clears it, colours back; Show Alien Waypoints opens the PICKED AI module as inteng's and shows the
// set's others (ShownNets: their lists as the level holds them; ShownGroup: their modules kept with the current one's); the 3D view draws their boxes and links on top of the current
// list's (WaypointView_WorldLines: twelve edges a box, two halves a pair); NetsWords counts them; Exit keeps the set, red again; without a set Show shows one AI module, as inteng's.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "object_edit.h"
#include "waypoint_model.h"
#include "waypoint_mode.h"
#include "waypoint_view.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

namespace
{
	struct Master
	{
		int32_t index = -1;
		std::string name;
		std::vector<host::WaypointRec> alien;
		std::vector<int32_t> modules;   // the master, then its slaves
	};

	// the pairs a list draws (a waypoint earlier in the list with a link back drew the pair already)
	size_t Pairs(std::vector<host::WaypointRec> const &list)
	{
		size_t pairs = 0;

		for (size_t w = 0; w < list.size(); w++)
		{
			for (host::WaypointLinkRec const &link : list[w].links)
			{
				if (link.index < 0 || (size_t)link.index >= list.size())
				{
					continue;
				}

				std::vector<host::WaypointLinkRec> const &back = list[(size_t)link.index].links;

				if ((size_t)link.index < w && std::any_of(back.begin(), back.end(), [&](host::WaypointLinkRec const &b) { return b.index == (int32_t)w; }))
				{
					continue;
				}

				pairs++;
			}
		}

		return pairs;
	}

	// the lines a list draws in the 3D view: twelve edges a box, a module link one line, a pair two halves
	size_t LinesOf(std::vector<host::WaypointRec> const &list, host::WaypointMode const &mode)
	{
		size_t lines = 12 * list.size() + 2 * Pairs(list);

		for (host::WaypointRec const &wp : list)
		{
			for (host::WaypointModuleLink const &link : wp.moduleLinks)
			{
				int32_t place[3];

				lines += mode.ModulePlace(link.name, place) ? 1 : 0;
			}
		}

		return lines;
	}

	uint32_t PaintOf(host::WaypointMode &mode, OpenLevelFile *level, int32_t module)
	{
		std::map<int32_t, uint32_t> const &paint = mode.ModulePaint(level);
		const std::map<int32_t, uint32_t>::const_iterator at = paint.find(module);

		return at == paint.end() ? 0 : at->second;
	}

	std::string Words(std::string const &name, std::vector<host::WaypointRec> const &list)
	{
		return name + " boxes " + std::to_string((unsigned long long)list.size()) + " links " + std::to_string((unsigned long long)Pairs(list));
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("host_waypoint_select_test: give a level .rif with three AI modules that hold alien waypoints\n");
		printf("0 checks, 1 failed\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		printf("1 checks, 1 failed\n");
		return 1;
	}

	// three masters with alien waypoints (the first three in the file), each with its modules and name; the second one with a slave, if the level has one
	std::vector<host::ModuleSnapshot> snapshots;
	std::map<int32_t, std::string> names;
	List<Object_Chunk *> all;

	host::ReadModuleSnapshots(&level, &snapshots);
	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		size_t length = 0;

		while (length < sizeof(each()->object_data.o_name) && each()->object_data.o_name[length] != 0)
		{
			length++;
		}

		names[(int32_t)each()->object_data.index_num] = std::string(each()->object_data.o_name, length);
	}

	std::vector<Master> masters;

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		host::WaypointSet set;
		std::string why;

		if (masters.size() < 3 && snapshot.module && snapshot.masterIndex < 0 && snapshot.hasWaypoint &&
			host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why) && set.alien.size() >= 2)
		{
			Master m;

			m.index = snapshot.index;
			m.name = names[snapshot.index];
			m.alien = set.alien;
			m.modules.push_back(snapshot.index);

			for (host::ModuleSnapshot const &other : snapshots)
			{
				if (other.module && other.masterIndex == snapshot.index)
				{
					m.modules.push_back(other.index);
				}
			}

			masters.push_back(m);
		}
	}

	Check(masters.size() == 3, "the level has three AI modules that hold alien waypoints (" + std::to_string(masters.size()) + ")");

	if (masters.size() < 3)
	{
		printf("%d checks, %d failed\n", checks, checksFailed);
		return 1;
	}

	Master const &a = masters[0], &b = masters[1], &c = masters[2];
	int32_t const bByModule = b.modules.back();   // a slave of b when it has one: the set takes its master

	printf("  --  A %s #%d (%zu alien), B %s #%d (%zu alien, clicked by #%d), C %s #%d (%zu alien)\n", a.name.c_str(), a.index, a.alien.size(), b.name.c_str(), b.index, b.alien.size(), bByModule,
		c.name.c_str(), c.index, c.alien.size());

	host::WaypointMode mode;
	host::WaypointContext context;

	context.open = &level;
	context.pickedModule = -1;

	// inteng's plain click: a pick, no set
	mode.Click(&level, a.index);
	Check(mode.PickedModule() == a.index && mode.Selection().empty() && mode.SelectionWords().empty(), "a plain click is inteng's pick and makes no set");

	// Ctrl+click B (by one of its modules): the set starts from the pick, then B's master
	Check(mode.SelectToggle(&level, bByModule), "Ctrl+click on B changes the set");
	Check(mode.Selection() == std::vector<int32_t>({ a.index, b.index }), "the set starts from the pick's AI module, then B's MASTER (a module of it was clicked)");
	Check(mode.SelectionWords() == a.name + "|" + b.name, "SelectionWords names the set in order: " + mode.SelectionWords());
	Check(PaintOf(mode, &level, b.index) == host::kWaypointModuleRed && PaintOf(mode, &level, bByModule) == host::kWaypointModuleRed && PaintOf(mode, &level, a.index) == host::kWaypointModuleRed,
		"the set's AI modules are painted red, every module of them (as inteng paints its pick)");
	Check(mode.PickedModule() == a.index, "the pick is still inteng's (A)");

	// Ctrl+click C adds it; Ctrl+click B again takes B out, its colour back; again puts it at the end
	mode.SelectToggle(&level, c.index);
	Check(mode.Selection() == std::vector<int32_t>({ a.index, b.index, c.index }), "Ctrl+click on C adds it");
	mode.SelectToggle(&level, b.index);
	Check(mode.Selection() == std::vector<int32_t>({ a.index, c.index }), "Ctrl+click on B again takes it out");
	Check(PaintOf(mode, &level, b.index) == host::kWaypointModuleGreen, "B taken out is green again (its master holds waypoints)");
	mode.SelectToggle(&level, b.index);
	Check(mode.Selection() == std::vector<int32_t>({ a.index, c.index, b.index }), "and Ctrl+click puts it back, at the end");

	// a plain click on nothing clears the set, colours back (the pick let go too, as inteng's)
	mode.Click(&level, -1);
	Check(mode.Selection().empty() && PaintOf(mode, &level, b.index) == host::kWaypointModuleGreen && PaintOf(mode, &level, c.index) == host::kWaypointModuleGreen &&
		PaintOf(mode, &level, a.index) == host::kWaypointModuleGreen, "a plain click clears the set: every AI module of it green again");

	// a Ctrl+click on the pick's own AI module with no set: the set is that one alone
	mode.Click(&level, a.index);
	mode.SelectToggle(&level, a.index);
	Check(mode.Selection() == std::vector<int32_t>({ a.index }), "a Ctrl+click on the pick's own AI module makes a set of that one");
	mode.SelectToggle(&level, b.index);
	mode.SelectToggle(&level, c.index);
	Check(mode.Selection() == std::vector<int32_t>({ a.index, b.index, c.index }), "then B and C join it");

	// Show Alien Waypoints: A (the pick) current, as inteng's; B and C shown with it
	context.pickedModule = c.index;   // the editor's own selection follows the last Ctrl+click: not a plain click (SelectToggle saw it)
	host::WaypointOutcome out = mode.Command(3, context);

	Check(mode.Mode() == 1 && mode.CurrentModule() == a.index && mode.List().size() == a.alien.size(), "Show Alien Waypoints opens the PICKED AI module (A), as inteng's: " + out.say);
	Check(mode.ShownNets().size() == 2 && mode.ShownNets()[0].master == b.index && mode.ShownNets()[1].master == c.index, "and shows the set's others, B then C");
	Check(mode.ShownNets().size() == 2 && mode.ShownNets()[0].list.size() == b.alien.size() && mode.ShownNets()[1].list.size() == c.alien.size() && mode.ShownNets()[0].name == b.name,
		"their alien lists as the level holds them, and their names");

	std::vector<int32_t> const group = mode.ShownGroup();
	bool keepsAll = true;

	for (Master const *m : { &a, &b, &c })
	{
		for (int32_t const module : m->modules)
		{
			keepsAll = keepsAll && std::find(group.begin(), group.end(), module) != group.end();
		}
	}

	Check(keepsAll && group.size() == a.modules.size() + b.modules.size() + c.modules.size(), "ShownGroup keeps every module of A, B and C, and no other (" + std::to_string(group.size()) + ")");
	Check(mode.CurrentGroup().size() == a.modules.size(), "CurrentGroup is still the current AI module's alone (the clicks of mode 1 pick in it)");
	Check(PaintOf(mode, &level, b.index) == host::kWaypointModuleWhite && PaintOf(mode, &level, c.index) == host::kWaypointModuleWhite, "the shown ones are painted white, as the current one (0x583b95)");

	std::vector<host::TrackLine> lines;

	host::WaypointView_WorldLines(mode, 1.0, &lines);

	size_t const expected = LinesOf(a.alien, mode) + LinesOf(b.alien, mode) + LinesOf(c.alien, mode);

	Check(lines.size() == expected, "the 3D view draws A's lines and B's and C's: " + std::to_string(lines.size()) + " of " + std::to_string(expected));
	Check(mode.NetsWords() == Words(a.name, a.alien) + "|" + Words(b.name, b.alien) + "|" + Words(c.name, c.alien), "NetsWords counts each network drawn: " + mode.NetsWords());

	size_t plainBoxes = 0;

	for (size_t i = LinesOf(a.alien, mode); i < lines.size(); i++)
	{
		plainBoxes += lines[i].colour == host::kWaypointBoxPlain || lines[i].colour == host::kWaypointLinkPlain || lines[i].colour == host::kWaypointLinkClosed ||
			lines[i].colour == host::kWaypointModuleLink ? 1 : 0;
	}

	Check(plainBoxes == lines.size() - LinesOf(a.alien, mode), "the shown ones are in inteng's plain colours: no current, no base");
	Check(!mode.SelectToggle(&level, b.index) && mode.Selection().size() == 3, "SelectToggle does nothing outside mode 0");

	// Exit: back to mode 0, the set kept and red again, nothing shown
	out = mode.Command(2, context);
	Check(mode.Mode() == 0 && mode.Selection().size() == 3 && mode.ShownNets().empty(), "Exit goes back to the AI modules with the set kept: " + out.say);
	Check(PaintOf(mode, &level, b.index) == host::kWaypointModuleRed && PaintOf(mode, &level, c.index) == host::kWaypointModuleRed, "the set is red again");

	// without a set, Show shows one AI module, as inteng's
	mode.Click(&level, b.index);
	context.pickedModule = b.index;
	out = mode.Command(3, context);
	host::WaypointView_WorldLines(mode, 1.0, &lines);
	Check(mode.Mode() == 1 && mode.CurrentModule() == b.index && mode.ShownNets().empty() && lines.size() == LinesOf(b.alien, mode) && mode.NetsWords() == Words(b.name, b.alien),
		"with no set, Show Alien Waypoints shows the one AI module, as inteng's: " + mode.NetsWords());
	Check(mode.ShownGroup() == mode.CurrentGroup(), "and keeps its modules only");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
