// host_waypoint_delete_test.cpp: DELETE WAYPOINTS, the pure part (waypoint_delete.h; the owner 11-21, the last patch for alpha.3). A made-up level of module snapshots, as
// ReadModuleSnapshots gives them, each AI module's WAYPOINT chunk made by the model's own writer (Waypoints_Serialize):
//   AI module 2: a master with its two slaves 3 and 4, 5 alien waypoints and 2 ground ones;  AI module 7: a module of none, 3 alien;  AI module 9: ground only (4);
//   module 11: no WAYPOINT chunk (as most of Rebellion's);  module 13: a WAYPOINT chunk that can't be read;  object 0: not a module.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "waypoint_delete.h"
#include "waypoint_model.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static std::vector<uint8_t> Chunk(size_t alien, size_t ground)
{
	host::WaypointSet set;

	for (size_t k = 0; k < alien + ground; k++)
	{
		host::WaypointRec r = {};

		for (int a = 0; a < 3; a++)
		{
			r.min[a] = (int32_t)(k * 1000);
			r.max[a] = (int32_t)(k * 1000 + 900);
			r.centre[a] = (int32_t)(k * 1000 + 450);
		}

		(k < alien ? set.alien : set.ground).push_back(r);
	}

	return host::Waypoints_Serialize(set);
}

static host::ModuleSnapshot Module(int32_t index, int32_t master, bool isMaster, std::vector<uint8_t> const &waypoint)
{
	host::ModuleSnapshot s;

	s.index = index;
	s.module = true;
	s.hasAdjacency = false;
	s.hasLinks = false;
	s.isMaster = isMaster;
	s.masterIndex = master;
	s.hasWaypoint = !waypoint.empty();
	s.waypoint = waypoint;
	return s;
}

static std::vector<int32_t> Heads(host::WaypointDeletePlan const &plan)
{
	std::vector<int32_t> h;

	for (host::WaypointDeleteHead const &one : plan.heads)
	{
		h.push_back(one.head);
	}

	return h;
}

int main()
{
	std::vector<host::ModuleSnapshot> level;
	host::ModuleSnapshot object = Module(0, -1, false, {});

	object.module = false;
	level.push_back(object);
	level.push_back(Module(2, -1, true, Chunk(5, 2)));
	level.push_back(Module(3, 2, false, {}));
	level.push_back(Module(4, 2, false, {}));
	level.push_back(Module(7, -1, false, Chunk(3, 0)));
	level.push_back(Module(9, -1, false, Chunk(0, 4)));
	level.push_back(Module(11, -1, false, {}));
	level.push_back(Module(13, -1, false, std::vector<uint8_t>(20, 0x5a)));

	host::WaypointDeletePlan const all = host::WaypointDelete_Plan(level, {}, true);

	Check(Heads(all) == std::vector<int32_t>{ 2, 7 } && all.alien == 8, "Delete All: the AI modules with alien waypoints (2 and 7), 8 of them; 9 (ground only) and 11 (none) left out");
	Check(all.heads.size() == 2 && all.heads[0].ground == 2 && all.heads[1].ground == 0, "and each AI module's ground waypoints counted, to be kept (2 has 2)");
	Check(all.unreadable == 1, "and the chunk that can't be read named, its module left as it is");

	Check(Heads(host::WaypointDelete_Plan(level, { 3 }, false)) == std::vector<int32_t>{ 2 }, "a slave picked: its AI module's head (2) is the one deleted from");
	Check(Heads(host::WaypointDelete_Plan(level, { 4, 3, 7 }, false)) == std::vector<int32_t>{ 2, 7 } && host::WaypointDelete_Plan(level, { 4, 3, 7 }, false).alien == 8,
		"a Ctrl group of 4, 3 and 7: AI modules 2 and 7, each once, in the selection's order, 8 waypoints");
	Check(host::WaypointDelete_Plan(level, { 9 }, false).heads.empty(), "an AI module with ground waypoints only: nothing to delete");
	Check(host::WaypointDelete_Plan(level, { 11 }, false).heads.empty(), "a module with no WAYPOINT chunk: nothing to delete");

	std::string const one = host::WaypointDelete_Question(host::WaypointDelete_Plan(level, { 3 }, false), false);
	std::string const every = host::WaypointDelete_Question(all, true);

	printf("    %s\n    %s\n", one.c_str(), every.c_str());
	Check(one.find("5 alien waypoints") != std::string::npos && one.find("AI module 2") != std::string::npos && one.find("made by hand") != std::string::npos,
		"the question, one AI module: its count, its number, and that some may be made by hand");
	Check(every.find("8 alien waypoints") != std::string::npos && every.find("2 AI modules") != std::string::npos && every.find("Rebellion") != std::string::npos &&
		every.find("ground") != std::string::npos, "the question, Delete All: the count, how many AI modules, Rebellion's may go, the ground waypoints kept");

	std::string const done = host::WaypointDelete_Done(8, 2);

	printf("    %s\n", done.c_str());
	Check(done.find("8 alien waypoints") != std::string::npos && done.find("2 AI modules") != std::string::npos && done.find("not saved") != std::string::npos,
		"the end line: what went, and that nothing is lost until the file is saved");

	printf("host_waypoint_delete_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
