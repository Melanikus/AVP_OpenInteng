// The test for Waypoint mode's AI modules: Add Module and Remove Module (host::AiModules_Add / AiModules_Remove, src/host/ai_modules_edit.cpp: the game's own AddModule and inteng's FUN_00589570)
// on REAL levels. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_ai_modules_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: every way an Add / Remove is refused (not a module, the master itself, a slave as master, already there, Remove of a module that is nobody's slave); an Add of a module with no AI chunk
// at all makes it a slave of the master and changes nothing else; an Add of ANOTHER MASTER moves its whole list to the new master, takes its AIMODMAS out, moves its waypoints into the master's
// (the count adds up, the links of the moved ones still lead to each other) and takes its WAYPOINT out; every write is taken by the game's loader; Remove takes the AIMODSLA out; the copy
// read again holds the result; the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "ai_modules_edit.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

typedef std::map<int32_t, host::ModuleSnapshot> Modules;

static Modules ModulesOf(OpenLevelFile &level)
{
	Modules modules;
	std::vector<host::ModuleSnapshot> snapshots;

	host::ReadModuleSnapshots(&level, &snapshots);

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (snapshot.module && !modules.count(snapshot.index))
		{
			modules[snapshot.index] = snapshot;
		}
	}

	return modules;
}

static size_t WaypointCount(host::ModuleSnapshot const &module)
{
	host::WaypointSet set;
	std::string why;

	if (!module.hasWaypoint || !host::Waypoints_Parse(module.waypoint.data(), module.waypoint.size(), &set, &why))
	{
		return 0;
	}

	return set.alien.size() + set.ground.size();
}

// The modules whose AIMODSLA names `master`
static std::set<int32_t> SlavesOf(Modules const &modules, int32_t master)
{
	std::set<int32_t> out;

	for (Modules::const_iterator it = modules.begin(); it != modules.end(); ++it)
	{
		if (it->second.masterIndex == master)
		{
			out.insert(it->first);
		}
	}

	return out;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_ai_modules_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_ai_modules_test (0 ok)\n");
		return 0;
	}

	std::vector<char> original;

	if (!ReadWholeFile(argv[1], &original))
	{
		printf("  FAILED  the level can be read: %s\n", argv[1]);
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	Modules before = ModulesOf(level);
	int32_t master = -1, otherMaster = -1, free = -1, slave = -1;

	for (Modules::const_iterator it = before.begin(); it != before.end(); ++it)
	{
		host::ModuleSnapshot const &m = it->second;

		if (m.isMaster && m.masterIndex < 0)
		{
			if (master < 0)
			{
				master = it->first;
			}
			else if (otherMaster < 0 && m.hasWaypoint)
			{
				otherMaster = it->first;
			}
		}
		else if (!m.isMaster && m.masterIndex < 0 && free < 0)
		{
			free = it->first;
		}
		else if (m.masterIndex >= 0 && slave < 0)
		{
			slave = it->first;
		}
	}

	printf("  --  %zu modules: a master %d, another master with waypoints %d, a free module %d, a slave %d\n", before.size(), master, otherMaster, free, slave);
	Check(master >= 0 && slave >= 0, "the level has masters and slaves to work with");

	host::AiModulesCounts counts;
	std::string why;

	// refusals
	Check(host::AiModules_Add(&level, master, master, &counts, &why) == host::AiModulesResult::SameModule, "Add Module of the master to itself is refused");
	Check(host::AiModules_Add(&level, master, -777, &counts, &why) == host::AiModulesResult::NotAModule, "Add Module of an object that is not a module is refused");
	Check(host::AiModules_Add(&level, slave, master, &counts, &why) == host::AiModulesResult::MasterIsASlave, "Add Module with a slave as the master is refused (pick its master)");

	int32_t const slaveOf = before[slave].masterIndex;

	Check(host::AiModules_Add(&level, slaveOf, slave, &counts, &why) == host::AiModulesResult::AlreadyThere, "Add Module of a module that is in the master's list already is refused");

	if (free >= 0)
	{
		Check(host::AiModules_Remove(&level, free, &counts, &why) == host::AiModulesResult::NotASlave, "Remove Module of a module that is nobody's slave is refused");
	}

	// a free module joins the master
	if (free >= 0)
	{
		std::set<int32_t> const slavesBefore = SlavesOf(before, master);
		host::AiModulesResult const result = host::AiModules_Add(&level, master, free, &counts, &why);

		Check(result == host::AiModulesResult::Done, std::string("Add Module of a module with no AI chunk: ") + host::AiModulesWords(result) + (why.empty() ? "" : " (" + why + ")"));

		Modules const after = ModulesOf(level);
		std::set<int32_t> const slavesAfter = SlavesOf(after, master);
		std::set<int32_t> expected = slavesBefore;

		expected.insert(free);
		Check(slavesAfter == expected && after.at(free).masterIndex == master && !after.at(free).isMaster, "it is a slave of the master now and the master's list holds that one more module");

		// Remove takes it out again
		Check(host::AiModules_Remove(&level, free, &counts, &why) == host::AiModulesResult::Done, "Remove Module takes it out again");

		Modules const removed = ModulesOf(level);

		Check(SlavesOf(removed, master) == slavesBefore && removed.at(free).masterIndex < 0, "the master's list is what it was and the module is on its own");
	}

	// another master joins: its list, its waypoints
	if (otherMaster >= 0)
	{
		Modules const now = ModulesOf(level);
		std::set<int32_t> const moving = SlavesOf(now, otherMaster);
		std::set<int32_t> const masterSlaves = SlavesOf(now, master);
		size_t const masterWaypoints = WaypointCount(now.at(master));
		size_t const otherWaypoints = WaypointCount(now.at(otherMaster));
		host::AiModulesResult const result = host::AiModules_Add(&level, master, otherMaster, &counts, &why);

		Check(result == host::AiModulesResult::Done, std::string("Add Module of another master: ") + host::AiModulesWords(result) + (why.empty() ? "" : " (" + why + ")"));
		printf("  --  %zu modules of its list moved, %zu waypoints moved, %zu chunks changed\n", counts.slavesMoved, counts.waypointsMoved, counts.chunksChanged);

		Modules const after = ModulesOf(level);
		std::set<int32_t> expected = masterSlaves;

		expected.insert(moving.begin(), moving.end());
		expected.insert(otherMaster);

		Check(SlavesOf(after, master) == expected && SlavesOf(after, otherMaster).empty() && !after.at(otherMaster).isMaster && after.at(otherMaster).masterIndex == master,
			"the other master and its whole list are now the master's slaves, and its AIMODMAS is gone");
		Check(WaypointCount(after.at(master)) == masterWaypoints + otherWaypoints && !after.at(otherMaster).hasWaypoint, "the master holds both modules' waypoints (" + std::to_string(masterWaypoints) + " + " +
			std::to_string(otherWaypoints) + ") and the other module's WAYPOINT chunk is gone");

		// the moved waypoints still lead to each other: every link of the master's chunk is to a place in its list (the parse checks it)
		host::WaypointSet set;
		std::string reason;

		Check(host::Waypoints_Parse(after.at(master).waypoint.data(), after.at(master).waypoint.size(), &set, &reason), "the master's merged WAYPOINT chunk reads back: " + reason);
	}
	else
	{
		printf("  --  no second master with waypoints in this level: the merge is not run\n");
	}

	// the copy
	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "ai-modules-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		Modules const now = ModulesOf(level);
		Modules const again = ModulesOf(reopened);
		bool same = now.size() == again.size();

		for (Modules::const_iterator it = now.begin(); it != now.end() && same; ++it)
		{
			Modules::const_iterator two = again.find(it->first);

			same = two != again.end() && two->second.isMaster == it->second.isMaster && two->second.masterIndex == it->second.masterIndex && two->second.waypoint == it->second.waypoint;
		}

		Check(same, "the copy, read again, holds the same AI modules and waypoints");
	}

	DeleteFileA(copyPath.c_str());

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_ai_modules_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_ai_modules_test\n");
	return 0;
}
