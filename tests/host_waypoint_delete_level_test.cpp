// The test for DELETE WAYPOINTS on real levels (host::WaypointMode::DeleteAlienLists and PaintFromFile, src/host/waypoint_mode.cpp). The command on the bar
// (waypoint_delete_editor.cpp: the count asked first) is the acceptance's (tests/scenarios/acceptance-waypoint-delete-hidden-save.txt). Written 2026-11-21 by Melanikus (Dietrich)
// for AVP_OpenInteng (the owner's ask, 11-21, through Bishop: "be able to select and delete Waypoint mapping by module, group of modules or envronment"). AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_delete_level_test.exe <stock level .rif> [<stock level .rif> ...] (tests/host_waypoint_delete_level_test.runargs). The levels are only read: every write stays in
// the memory image the editor would save.
// CHECKS, on each level given:
//   REBELLION'S FORM (the owner's "a module left with no network is written exactly as Rebellion writes one that has none"): no stock module holds an empty WAYPOINT chunk (a module
//   with no network has no chunk at all), and every stock WAYPOINT chunk comes out of our writer (Waypoints_Serialize of what Waypoints_Parse reads) byte for byte. MEASURED 11-21 on
//   the 63 stock levels: 368 WAYPOINT chunks, every one ALIEN ONLY (no ground list in any of them), none empty, every one byte for byte. So on a stock level a delete always
//   takes the chunk out, which is the form of every stock module with no network.
//   THE DELETE: the pick's AI module, a Ctrl-set one, one with alien waypoints only and one with a ground list too, deleted in one go (no stock level has a ground list, so the test
//   gives one alien-only module a ground list first). The alien-only one loses its WAYPOINT chunk; the other keeps its chunk with no alien waypoint and its ground list byte for
//   byte; no other AI module changes; the count is theirs. THE COLOURS: the written AI modules are painted from the file (white with no chunk, green with one), the pick and the set
//   keep their red until let go, and then take the file's colour. Show Alien Waypoints on a deleted one shows no box. A second delete of it is refused and changes nothing. The
//   current AI module's lists are read again.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>
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
	// an AI module's head (a master, or a module of none) with its WAYPOINT as the level holds it
	struct Head
	{
		int32_t index = -1;
		std::string name;
		bool hasWaypoint = false;
		std::vector<uint8_t> chunk;
		host::WaypointSet set;
		bool parsed = false;
	};

	std::map<int32_t, Head> Heads(OpenLevelFile *level)
	{
		std::vector<host::ModuleSnapshot> snapshots;
		std::map<int32_t, std::string> names;
		std::map<int32_t, Head> heads;
		List<Object_Chunk *> all;

		host::ReadModuleSnapshots(level, &snapshots);
		level->Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			size_t length = 0;

			while (length < sizeof(each()->object_data.o_name) && each()->object_data.o_name[length] != 0)
			{
				length++;
			}

			names[(int32_t)each()->object_data.index_num] = std::string(each()->object_data.o_name, length);
		}

		for (host::ModuleSnapshot const &s : snapshots)
		{
			if (!s.module || s.masterIndex >= 0 || heads.count(s.index))
			{
				continue;
			}

			Head h;
			std::string why;

			h.index = s.index;
			h.name = names[s.index];
			h.hasWaypoint = s.hasWaypoint;
			h.chunk = s.waypoint;
			h.parsed = s.hasWaypoint && host::Waypoints_Parse(s.waypoint.data(), s.waypoint.size(), &h.set, &why);
			heads[s.index] = h;
		}

		return heads;
	}

	uint32_t PaintOf(host::WaypointMode &mode, OpenLevelFile *level, int32_t module)
	{
		std::map<int32_t, uint32_t> const &paint = mode.ModulePaint(level);
		const std::map<int32_t, uint32_t>::const_iterator at = paint.find(module);

		return at == paint.end() ? 0 : at->second;
	}

	std::string Num(size_t n)
	{
		return std::to_string((unsigned long long)n);
	}

	bool aloneDeleted = false, withGroundDeleted = false;

	void TestLevel(char const *path)
	{
		printf("-- %s\n", path);

		OpenLevelFile level;

		if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
		{
			Check(false, "the game's loader takes the level");
			return;
		}

		std::map<int32_t, Head> const before = Heads(&level);

		// ---- REBELLION'S FORM: the survey of the stock chunks
		size_t withChunk = 0, empty = 0, alienOnly = 0, groundOnly = 0, both = 0, unreadable = 0, sameBytes = 0;
		std::string firstNotSame;

		for (auto const &each : before)
		{
			Head const &h = each.second;

			if (!h.hasWaypoint)
			{
				continue;
			}

			withChunk++;

			if (!h.parsed)
			{
				unreadable++;
				continue;
			}

			empty += h.set.alien.empty() && h.set.ground.empty() ? 1 : 0;
			alienOnly += !h.set.alien.empty() && h.set.ground.empty() ? 1 : 0;
			groundOnly += h.set.alien.empty() && !h.set.ground.empty() ? 1 : 0;
			both += !h.set.alien.empty() && !h.set.ground.empty() ? 1 : 0;

			if (host::Waypoints_Serialize(h.set) == h.chunk)
			{
				sameBytes++;
			}
			else if (firstNotSame.empty())
			{
				firstNotSame = h.name + " (AI module " + std::to_string((long long)h.index) + ")";
			}
		}

		printf("  --  %zu AI modules, %zu with a WAYPOINT chunk: %zu alien only, %zu ground only, %zu both, %zu empty, %zu unreadable\n", before.size(), withChunk, alienOnly, groundOnly,
			both, empty, unreadable);
		Check(unreadable == 0, "every stock WAYPOINT chunk is read (" + Num(unreadable) + " not)");
		Check(empty == 0, "no stock module holds an empty WAYPOINT chunk: Rebellion writes a module with no network with no chunk (" + Num(empty) + " empty)");
		Check(sameBytes == withChunk, "every stock WAYPOINT chunk comes out of our writer byte for byte: " + Num(sameBytes) + " of " + Num(withChunk) +
			(firstNotSame.empty() ? std::string() : ", the first not: " + firstNotSame));

		// ---- THE DELETE: the heads with alien waypoints, by kind
		std::vector<int32_t> alienAlone, alienGround;

		auto const kinds = [&](std::map<int32_t, Head> const &heads)
		{
			alienAlone.clear();
			alienGround.clear();

			for (auto const &each : heads)
			{
				Head const &h = each.second;

				if (h.parsed && !h.set.alien.empty())
				{
					(h.set.ground.empty() ? alienAlone : alienGround).push_back(h.index);
				}
			}
		};

		kinds(before);

		// No stock level holds a ground list (the survey of the 63 stock levels, 11-21: 368 WAYPOINT chunks, every one alien only), so the ground case is made here: one alien-only AI
		// module given a ground list (a copy of its alien one), written as the editor writes a WAYPOINT, before the delete
		if (alienGround.empty() && alienAlone.size() >= 5)
		{
			Head const &g = before.at(alienAlone.front());
			host::WaypointSet withGround = g.set;
			host::ModuleChildEdit edit;
			std::vector<host::ModuleChildEdit> edits;
			std::vector<bool> wrote;

			withGround.ground = g.set.alien;
			edit.objectIndex = g.index;
			memset(edit.name, 0, sizeof(edit.name));
			memcpy(edit.name, "WAYPOINT", 8);
			edit.remove = false;
			edit.chunk = host::Waypoints_Serialize(withGround);
			edits.push_back(edit);
			Check(host::SetModuleChildrenMany(&level, edits, &wrote) == host::ObjectEditResult::Done && wrote.size() == 1 && wrote[0],
				"(no stock ground list: " + g.name + " is given one, a copy of its " + Num(g.set.alien.size()) + " alien waypoints, to test the ground case)");
		}

		std::map<int32_t, Head> const ready = Heads(&level);

		kinds(ready);

		std::vector<int32_t> all = alienAlone;

		all.insert(all.end(), alienGround.begin(), alienGround.end());
		printf("  --  %zu AI modules with alien waypoints: %zu with no ground list, %zu with one\n", all.size(), alienAlone.size(), alienGround.size());

		if (all.size() < 4)
		{
			printf("  --  fewer than 4: the delete is tested on the other levels\n");
			return;
		}

		// the pick (P) and a Ctrl-set one (S) of them, then one alien-only (X) and one with ground (Y) when the level has them, each one other
		int32_t const x = alienAlone.empty() ? -1 : alienAlone.back();
		int32_t const y = alienGround.empty() ? -1 : alienGround.back();
		std::vector<int32_t> rest;

		for (int32_t const m : all)
		{
			if (m != x && m != y)
			{
				rest.push_back(m);
			}
		}

		int32_t const p = rest[0], s = rest[1];
		std::vector<int32_t> masters = { p, s };

		if (x >= 0)
		{
			masters.push_back(x);
		}

		if (y >= 0)
		{
			masters.push_back(y);
		}

		size_t alienCount = 0;

		for (int32_t const m : masters)
		{
			alienCount += ready.at(m).set.alien.size();
		}

		host::WaypointMode mode;
		host::WaypointContext context;

		context.open = &level;
		context.pickedModule = -1;

		mode.ModulePaint(&level);
		mode.Click(&level, p);
		mode.SelectToggle(&level, s);
		Check(mode.PickedModule() == p && mode.Selection() == std::vector<int32_t>({ p, s }), "P " + ready.at(p).name + " picked, S " + ready.at(s).name + " Ctrl-clicked: the set is P and S");

		size_t deleted = 0;
		host::WaypointOutcome out = mode.DeleteAlienLists(&level, masters, &deleted);

		Check(out.levelChanged && deleted == alienCount, "DeleteAlienLists of " + Num(masters.size()) + " AI modules takes out their " + Num(alienCount) + " alien waypoints: " + Num(deleted) +
			" (" + out.say + ")");

		std::map<int32_t, Head> const after = Heads(&level);
		bool othersSame = after.size() == ready.size();

		for (auto const &each : ready)
		{
			if (std::find(masters.begin(), masters.end(), each.first) != masters.end())
			{
				continue;
			}

			auto const now = after.find(each.first);

			othersSame = othersSame && now != after.end() && now->second.hasWaypoint == each.second.hasWaypoint && now->second.chunk == each.second.chunk;
		}

		Check(othersSame, "no other AI module's WAYPOINT changed (" + Num(ready.size() - masters.size()) + " of them)");

		for (int32_t const m : masters)
		{
			Head const &was = ready.at(m), &now = after.at(m);
			std::string const who = was.name + " (AI module " + std::to_string((long long)m) + ")";

			if (was.set.ground.empty())
			{
				Check(!now.hasWaypoint, who + " had no ground list: its WAYPOINT chunk is taken out, as a stock module with no network has none");
			}
			else
			{
				host::WaypointSet kept;

				kept.ground = was.set.ground;
				Check(now.hasWaypoint && now.parsed && now.set.alien.empty() && host::Waypoints_Serialize(now.set) == now.chunk && now.chunk == host::Waypoints_Serialize(kept),
					who + " keeps its WAYPOINT chunk with no alien waypoint and its " + Num(was.set.ground.size()) + " ground waypoints, byte for byte in Rebellion's form");
			}
		}

		if (x >= 0)
		{
			aloneDeleted = true;
			Check(PaintOf(mode, &level, x) == host::kWaypointModuleWhite, "X " + ready.at(x).name + ", deleted, no chunk left: painted WHITE from the file at once");
		}

		if (y >= 0)
		{
			withGroundDeleted = true;
			Check(PaintOf(mode, &level, y) == host::kWaypointModuleGreen, "Y " + ready.at(y).name + ", deleted, its ground list kept: GREEN from the file (its MODULEDT still holds a WAYPOINT)");
		}

		Check(PaintOf(mode, &level, p) == host::kWaypointModuleRed && PaintOf(mode, &level, s) == host::kWaypointModuleRed, "the pick and the Ctrl set keep their red");

		// let go (a plain click on nothing): the pick and the set take the file's colour
		mode.Click(&level, -1);

		uint32_t const pColour = ready.at(p).set.ground.empty() ? host::kWaypointModuleWhite : host::kWaypointModuleGreen;
		uint32_t const sColour = ready.at(s).set.ground.empty() ? host::kWaypointModuleWhite : host::kWaypointModuleGreen;

		Check(mode.Selection().empty() && PaintOf(mode, &level, p) == pColour && PaintOf(mode, &level, s) == sColour, "let go, the pick and the set take the file's colour");

		// Show Alien Waypoints on a deleted one: no box
		mode.Click(&level, p);
		context.pickedModule = p;
		out = mode.Command(3, context);
		Check(mode.Mode() == 1 && mode.CurrentModule() == p && mode.List().empty() && mode.NetsWords() == ready.at(p).name + " boxes 0 links 0",
			"Show Alien Waypoints on P shows no box: " + mode.NetsWords());
		out = mode.Command(2, context);
		Check(mode.Mode() == 0, "Exit goes back to the AI modules: " + out.say);

		// a second delete of it: refused, nothing changed
		std::map<int32_t, Head> const beforeAgain = Heads(&level);

		deleted = 7;
		out = mode.DeleteAlienLists(&level, std::vector<int32_t>({ p }), &deleted);

		bool sameAgain = true;

		for (auto const &each : Heads(&level))
		{
			sameAgain = sameAgain && beforeAgain.count(each.first) && beforeAgain.at(each.first).chunk == each.second.chunk &&
				beforeAgain.at(each.first).hasWaypoint == each.second.hasWaypoint;
		}

		Check(!out.levelChanged && deleted == 0 && sameAgain, "a second delete of P is refused and changes nothing: " + out.say);

		// the current AI module (open in Show Alien Waypoints) deleted: the mode's lists read again from the file
		int32_t const c = rest.size() > 2 ? rest[2] : -1;

		if (c >= 0)
		{
			mode.Click(&level, c);
			context.pickedModule = c;
			mode.Command(3, context);

			bool const shown = mode.Mode() == 1 && mode.CurrentModule() == c && mode.List().size() == ready.at(c).set.alien.size();

			out = mode.DeleteAlienLists(&level, std::vector<int32_t>({ c }), &deleted);
			Check(shown && out.levelChanged && mode.CurrentModule() == c && mode.List().empty(), "C " + ready.at(c).name + " open in Show Alien Waypoints and deleted: the mode reads its lists " +
				"again (" + Num(mode.List().size()) + " boxes now)");
			mode.Command(2, context);
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("host_waypoint_delete_level_test: give one stock level .rif or more\n");
		printf("0 checks, 1 failed\n");
		return 1;
	}

	for (int a = 1; a < argc; a++)
	{
		TestLevel(argv[a]);
	}

	Check(aloneDeleted && withGroundDeleted, "across the levels, an AI module with alien waypoints only and one with a ground list too were both deleted");
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
