// The test for the WAYPOINT chunk model (src/host/waypoint_model.cpp) and the module child edit (host::SetModuleChildrenMany, src/host/object_edit.cpp). Written 2026-11-15 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_model_test.exe [<a level .rif> <a folder it may write its copy into>]. With no arguments only the hand-made cases run. The level is only read; the copy is the only file written.
// HAND-MADE: Add Link (two-way, one-way: the flags on both sides, a second one-way the other way clears both, alien, deviation), Delete Link (both directions), Delete Waypoint (links to it go,
// the ones past it count one less), Create Waypoint, Serialize / Parse round trip, and every way a chunk can be bad is refused (cut, a count that does not fit, a link past its list, a name
// with no end, a place beyond 2^24) and random / mutated bytes never crash.
// A REAL LEVEL: every module's WAYPOINT chunk parses and writes back to the SAME BYTES (the game's own writer's layout); the first module with waypoints gets a new waypoint and a link, written
// through SetModuleChildrenMany, the game's loader takes it, the copy read again holds them; a module with no WAYPOINT gets one; the opened file is byte for byte what it was.

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

static host::WaypointRec Make(int32_t x)
{
	host::WaypointRec wp;
	int32_t const place[3] = { x, 0, 0 };
	std::vector<host::WaypointRec> one;

	host::Waypoints_Create(&one, place);
	wp = one[0];
	return wp;
}

static host::WaypointSet SetOf(size_t alien, size_t ground)
{
	host::WaypointSet set;

	for (size_t i = 0; i < alien; i++)
	{
		set.alien.push_back(Make((int32_t)(1000 * i)));
	}

	for (size_t i = 0; i < ground; i++)
	{
		set.ground.push_back(Make((int32_t)(5000 + 1000 * i)));
	}

	return set;
}

static bool Refused(std::vector<uint8_t> const &bytes, std::string *why = nullptr)
{
	host::WaypointSet set;
	std::string reason;
	bool const ok = host::Waypoints_Parse(bytes.data(), bytes.size(), &set, &reason);

	if (why)
	{
		*why = reason;
	}

	return !ok && set.alien.empty() && set.ground.empty() && !reason.empty();
}

static void HandMade()
{
	printf("HAND-MADE\n");

	// Add Link: two-way
	std::vector<host::WaypointRec> list = SetOf(4, 0).alien;

	Check(host::Waypoints_AddLink(&list, 0, 1, 0) && list[0].links.size() == 1 && list[0].links[0].index == 1 && list[0].links[0].flags == 0 && list[1].links.size() == 1 && list[1].links[0].index == 0 &&
		list[1].links[0].flags == 0, "Add Link: a link both ways with flags 0");

	// one-way: a -> b is One way (1), b -> a is One way the other direction (2)
	Check(host::Waypoints_AddLink(&list, 1, 2, host::kLinkOneWay) && list[1].links.size() == 2 && list[1].links[1].index == 2 && list[1].links[1].flags == 1 && list[2].links.size() == 1 &&
		list[2].links[0].index == 1 && list[2].links[0].flags == 2, "Add Link - 1 way: One way on the first, One way the other direction on the second");

	// a one-way the other direction: both bits on the same link clear both
	Check(host::Waypoints_AddLink(&list, 2, 1, host::kLinkOneWay) && list[2].links[0].flags == 3 - 3 + 0 && list[1].links[1].flags == 0, "Add Link - 1 way over the opposite one-way: both bits cancel, a two-way link is left");

	// alien and deviation bits are ORed in
	Check(host::Waypoints_AddLink(&list, 0, 3, host::kLinkAlien) && list[0].links[1].flags == 4 && list[3].links[0].flags == 4, "Add Alien Link: the Aliens only bit on both");
	Check(host::Waypoints_AddLink(&list, 0, 3, host::kLinkDeviation) && list[0].links[1].flags == 12 && list[3].links[0].flags == 12, "Add Deviation Link on the same pair: the bits are ORed");
	Check(!host::Waypoints_AddLink(&list, 0, 9, 0) && !host::Waypoints_AddLink(&list, -1, 0, 0), "a waypoint not in the list gives false");

	// Delete Link: both directions
	Check(host::Waypoints_DeleteLink(&list, 3, 0) && list[0].links.size() == 1 && list[3].links.empty() && !host::Waypoints_DeleteLink(&list, 0, 3), "Delete Link takes out both directions; the second time there is nothing");

	// Delete Waypoint: links to it go, those past it count one less
	std::vector<host::WaypointRec> chain = SetOf(4, 0).alien;

	host::Waypoints_AddLink(&chain, 0, 1, 0);
	host::Waypoints_AddLink(&chain, 1, 2, 0);
	host::Waypoints_AddLink(&chain, 2, 3, 0);
	Check(host::Waypoints_Delete(&chain, 1) && chain.size() == 3 && chain[0].links.empty() && chain[1].links.size() == 1 && chain[1].links[0].index == 2 && chain[2].links.size() == 1 && chain[2].links[0].index == 1,
		"Delete Waypoint 1 of a chain 0-1-2-3: the links to it go, 2 and 3 become 1 and 2 and still lead to each other");
	Check(!host::Waypoints_Delete(&chain, 3) && !host::Waypoints_Delete(&chain, -1), "Delete Waypoint outside the list gives false");

	// Serialize / Parse
	host::WaypointSet set = SetOf(2, 3);

	host::Waypoints_AddLink(&set.alien, 0, 1, host::kLinkOneWay);
	host::Waypoints_AddLink(&set.ground, 0, 2, host::kLinkAlien);
	set.ground[1].moduleLinks.push_back({ "Room12", 3 });
	set.ground[1].moduleLinks.push_back({ "A", 0 });
	set.alien[0].flags |= host::kWaypointCanCrawl;

	std::vector<uint8_t> const bytes = host::Waypoints_Serialize(set);
	host::WaypointSet again;
	std::string why;

	Check(host::Waypoints_Parse(bytes.data(), bytes.size(), &again, &why), "a set made by hand is read back: " + why);
	Check(again.alien.size() == 2 && again.ground.size() == 3 && again.alien[0].links.size() == 1 && again.ground[1].moduleLinks.size() == 2 && again.ground[1].moduleLinks[0].name == "Room12" &&
		again.alien[0].flags == host::kWaypointCanCrawl && again.ground[0].flags == 0, "its alien and ground waypoints, links, module links and flags are as made (the first-ground mark is not kept)");
	Check(host::Waypoints_Serialize(again) == bytes, "written again it is the same bytes");

	// a set with only ground waypoints: the first one is the first ground waypoint
	host::WaypointSet groundOnly = SetOf(0, 2);
	std::vector<uint8_t> const groundBytes = host::Waypoints_Serialize(groundOnly);
	host::WaypointSet groundAgain;

	Check(host::Waypoints_Parse(groundBytes.data(), groundBytes.size(), &groundAgain, &why) && groundAgain.alien.empty() && groundAgain.ground.size() == 2, "only ground waypoints: all of them come back as ground");

	// hostile: every way a chunk can be bad
	std::vector<uint8_t> bad;

	Check(Refused(std::vector<uint8_t>()), "an empty chunk is refused");
	bad = bytes;
	bad.resize(bytes.size() - 1);
	Check(Refused(bad, &why), "a chunk cut one byte short is refused: " + why);
	bad = bytes;
	bad.push_back(0);
	Check(Refused(bad, &why), "a chunk with a size that is not its bytes' is refused: " + why);
	bad = bytes;
	memcpy(bad.data(), "WAYPOINX", 8);
	Check(Refused(bad, &why), "a chunk with another name is refused: " + why);

	int32_t big = 1000000;

	bad = bytes;
	memcpy(bad.data() + 12, &big, 4);
	Check(Refused(bad, &why), "a count of a million waypoints is refused: " + why);

	bad = bytes;
	big = 1 << 25;
	memcpy(bad.data() + 16, &big, 4);   // the first waypoint's min.x
	Check(Refused(bad, &why), "a corner beyond 2^24 is refused: " + why);

	host::WaypointSet outside = SetOf(2, 0);

	outside.alien[0].links.push_back({ 5, 0 });
	std::vector<uint8_t> const outsideBytes = host::Waypoints_Serialize(outside);

	Check(Refused(outsideBytes, &why), "a link to a waypoint that is not in its list is refused: " + why);

	host::WaypointSet named = SetOf(1, 0);

	named.alien[0].moduleLinks.push_back({ "Name", 0 });

	std::vector<uint8_t> noEnd = host::Waypoints_Serialize(named);

	// the name's four bytes (letters and the zero) are all letters now: no end inside the chunk
	for (size_t i = 0; i < noEnd.size(); i++)
	{
		if (noEnd[i] == 'N' && i + 3 < noEnd.size() && noEnd[i + 1] == 'a')
		{
			memset(noEnd.data() + i, 'x', 8 < noEnd.size() - i ? 8 : noEnd.size() - i);
			break;
		}
	}

	Check(Refused(noEnd, &why), "a module link's name with no end is refused: " + why);

	// mutated bytes: never a crash, whole or nothing
	unsigned int seed = 777u;
	size_t taken = 0, refused = 0;

	for (int round = 0; round < 4000; round++)
	{
		std::vector<uint8_t> mutated = bytes;

		for (int change = 0; change < 1 + round % 5; change++)
		{
			seed = seed * 1664525u + 1013904223u;
			mutated[(seed >> 8) % mutated.size()] = (uint8_t)(seed >> 24);
		}

		if (round % 4 == 0)
		{
			mutated.resize(1 + (seed >> 8) % mutated.size());
		}

		host::WaypointSet got;
		std::string reason;

		if (host::Waypoints_Parse(mutated.data(), mutated.size(), &got, &reason))
		{
			taken++;
		}
		else
		{
			refused++;

			if (!got.alien.empty() || !got.ground.empty() || reason.empty())
			{
				Check(false, "a refused chunk gave waypoints or no reason (round " + std::to_string(round) + ")");
				return;
			}
		}
	}

	Check(taken > 0 && refused > 0, "4000 mutated chunks: " + std::to_string(taken) + " taken whole, " + std::to_string(refused) + " refused with a reason and nothing");
}

static void RealLevel(char const *levelPath, std::string const &folder, std::vector<char> const &original)
{
	printf("A REAL LEVEL\n");

	OpenLevelFile level;

	if (level.Open(levelPath) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	std::vector<host::ModuleSnapshot> snapshots;

	Check(host::ReadModuleSnapshots(&level, &snapshots) == host::ObjectEditResult::Done, "the level's modules are read");

	size_t withWaypoints = 0, roundTrip = 0, parsed = 0, waypoints = 0, links = 0, masters = 0, slaves = 0;
	int32_t firstWith = -1, firstWithout = -1;

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (!snapshot.module)
		{
			continue;
		}

		masters += snapshot.isMaster ? 1 : 0;
		slaves += snapshot.masterIndex >= 0 ? 1 : 0;

		if (!snapshot.hasWaypoint)
		{
			firstWithout = firstWithout < 0 ? snapshot.index : firstWithout;
			continue;
		}

		withWaypoints++;
		firstWith = firstWith < 0 ? snapshot.index : firstWith;

		host::WaypointSet set;
		std::string why;

		if (host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why))
		{
			parsed++;
			waypoints += set.alien.size() + set.ground.size();

			for (host::WaypointRec const &wp : set.alien)
			{
				links += wp.links.size();
			}

			for (host::WaypointRec const &wp : set.ground)
			{
				links += wp.links.size();
			}

			roundTrip += host::Waypoints_Serialize(set) == snapshot.waypoint ? 1 : 0;
		}
	}

	printf("  --  %zu modules with a WAYPOINT chunk: %zu masters, %zu slaves in the level; %zu waypoints, %zu links\n", withWaypoints, masters, slaves, waypoints, links);
	Check(parsed == withWaypoints, "every WAYPOINT chunk of the level parses: " + std::to_string(parsed) + " of " + std::to_string(withWaypoints));
	Check(roundTrip == parsed, "every one is written back to the same bytes: " + std::to_string(roundTrip) + " of " + std::to_string(parsed));

	if (firstWith >= 0)
	{
		// a new waypoint and a link in the first module that has a WAYPOINT
		host::WaypointSet set;
		std::string why;

		for (host::ModuleSnapshot const &snapshot : snapshots)
		{
			if (snapshot.module && snapshot.index == firstWith && snapshot.hasWaypoint)
			{
				host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why);
				break;
			}
		}

		std::vector<host::WaypointRec> &list = set.ground.empty() ? set.alien : set.ground;

		if (!list.empty())
		{
			int32_t const place[3] = { list[0].centre[0], list[0].centre[1], list[0].centre[2] };
			int32_t const added = host::Waypoints_Create(&list, place);

			Check(host::Waypoints_AddLink(&list, 0, added, 0), "a waypoint is made at the first one's place and linked to it");

			host::ModuleChildEdit edit;

			edit.objectIndex = firstWith;
			memcpy(edit.name, "WAYPOINT", 9);
			edit.remove = false;
			edit.chunk = host::Waypoints_Serialize(set);

			std::vector<host::ModuleChildEdit> edits;
			std::vector<bool> wrote;

			edits.push_back(edit);

			host::ObjectEditResult const result = host::SetModuleChildrenMany(&level, edits, &wrote);

			Check(result == host::ObjectEditResult::Done && wrote.size() == 1 && wrote[0], std::string("the module's WAYPOINT chunk is replaced and the game's loader takes it: ") + host::ObjectEditWords(result));
		}
	}

	if (firstWithout >= 0)
	{
		host::WaypointSet set = SetOf(0, 1);
		host::ModuleChildEdit edit;

		set.ground[0].centre[0] = set.ground[0].centre[1] = set.ground[0].centre[2] = 0;
		edit.objectIndex = firstWithout;
		memcpy(edit.name, "WAYPOINT", 9);
		edit.remove = false;
		edit.chunk = host::Waypoints_Serialize(set);

		std::vector<host::ModuleChildEdit> edits;
		std::vector<bool> wrote;

		edits.push_back(edit);

		host::ObjectEditResult const result = host::SetModuleChildrenMany(&level, edits, &wrote);

		// the game's loader may refuse a waypoint that is not inside the level's scale room: only the answer is reported
		printf("  --  a WAYPOINT chunk put in a module that had none: %s\n", host::ObjectEditWords(result));
		Check(result == host::ObjectEditResult::Done || result == host::ObjectEditResult::LoaderRefused, "that either went in or the loader refused it (nothing changed then)");
	}

	// the copy
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "waypoint-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");
	DeleteFileA(copyPath.c_str());

	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");
}

int main(int argc, char **argv)
{
	HandMade();

	if (argc >= 3)
	{
		std::vector<char> original;

		if (!ReadWholeFile(argv[1], &original))
		{
			printf("  FAILED  the level can be read: %s\n", argv[1]);
			return 1;
		}

		RealLevel(argv[1], argv[2], original);
	}

	if (checksFailed)
	{
		printf("FAILED host_waypoint_model_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_waypoint_model_test\n");
	return 0;
}
