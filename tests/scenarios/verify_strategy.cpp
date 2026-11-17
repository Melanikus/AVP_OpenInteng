// verify_strategy: the check of a Strategy Data scenario's saved copy, read back through the game's own loader (Bishop 11-16: "read the copy back through the loader"). Written 2026-11-16 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_strategy.exe <the level the editor opened> <the copy it saved> <an object's name> <field> <value> [<field> <value> | holds <another object's name> | also <another object's name>] ...
// A <field> is a field of its strategy as the model names it, <value> what the copy must hold there; "holds <name>" checks that its strategy holds that object's 8-byte ID (a target in a list).
// "also <name>": that object was changed by the scenario too (checked by its own verify line), so it is left out of the check that every other strategy is the original's.
// "size <n>": the strategy is n bytes long; "nullid <field>": that object field holds the null ID (both added 2026-11-17 by Drake for the Find Errors repairs' read-back).
// A Multiple Target / Area Switch's list (66 / 68 only, read as the view reads it: host::StrategyView_MultiTargets; added 2026-11-17 for Vasquez's read-backs): "targets <n>": it holds n targets;
// "target.<i> <name>": target i (from 0) is that object ("none": the null ID); "target_request.<i> <n>": target i's request word is n.
// The object is looked for among the level's objects (modules and placed objects), its virtual objects, its enemies / generators and its placed hierarchies (added 2026-11-17), by its exact name; a value may be decimal or 0x hex.
// CHECKS: both levels are read by the game's loader; the object holds a strategy in both; in the copy the field holds the value; every OTHER object's strategy is byte for byte the original's (the
// scenario changed that one only). Gives back 0 only if every check passed.

#include <errno.h>
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
#include "strategy_lookup.h"
#include "strategy_views.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// what the game's chunk code calls
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

struct Held
{
	std::string name;
	bool hasStrategy = false;
	int32_t kind = 0;
	std::vector<uint8_t> payload;
};

// every object with an ID: its name and its strategy, by the 8 bytes of its ID
static void Strategies(OpenLevelFile &level, std::map<std::string, Held> *out)
{
	out->clear();

	auto add = [&](uint8_t const *id, std::string const &name)
	{
		Held one;
		host::ObjectStrategy found;

		one.name = name;

		if (host::FindObjectStrategy(level.Level(), id, &found) == host::StrategyFound::Found && found.payload.size() >= 12)
		{
			one.hasStrategy = true;
			memcpy(&one.kind, found.payload.data() + 8, 4);
			one.payload = found.payload;
		}

		std::string const key((char const *)id, 8);

		if (out->find(key) == out->end())
		{
			(*out)[key] = one;
		}
	};

	List<Object_Chunk *> objects;
	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (each())
		{
			uint8_t id[8];

			memcpy(id, &each()->object_data.ID, 8);
			add(id, each()->object_data.o_name ? each()->object_data.o_name : "");
		}
	}

	std::vector<host::VirtualObjectRef> virtuals;
	host::ListVirtualObjects(level.Level(), &virtuals);

	for (host::VirtualObjectRef const &v : virtuals)
	{
		add(v.id, v.name);
	}

	std::vector<host::GeneratorRef> generators;
	host::ListGenerators(level.Level(), &generators);

	for (host::GeneratorRef const &g : generators)
	{
		add(g.id, g.name);
	}

	std::vector<host::PlacedHierarchyRef> placed;
	host::ListPlacedHierarchies(level.Level(), &placed);

	for (host::PlacedHierarchyRef const &p : placed)
	{
		add(p.id, p.name);
	}
}

int main(int argc, char **argv)
{
	if (argc < 6 || (argc - 4) % 2 != 0)
	{
		printf("verify_strategy: <original> <copy> <object name> <field> <value> [<field> <value> | holds <name>] ...\n");
		return 2;
	}

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level() != nullptr, std::string("the game's loader reads the original: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	if (checksFailed)
	{
		printf("FAILED verify_strategy\n");
		return 1;
	}

	std::map<std::string, Held> before, after;

	Strategies(original, &before);
	Strategies(copy, &after);

	std::string const name = argv[3];
	std::string key;

	for (auto const &one : after)
	{
		if (one.second.name == name)
		{
			key = one.first;
			break;
		}
	}

	Check(!key.empty(), "the copy holds an object named \"" + name + "\"");

	for (int a = 4; !key.empty() && a + 1 < argc; a += 2)
	{
		Held const &now = after[key];
		std::string const field = argv[a];

		if (field == "also")
		{
			continue;   // another object the scenario changed too: left out of the byte-for-byte check below
		}

		if (field == "size")
		{
			// the strategy's whole size (the AVPSTRAT payload): a list repair that takes items out makes it shorter (added 2026-11-17, strategy-closeout-drake.md E-5)
			long long const wanted = strtoll(argv[a + 1], nullptr, 0);

			Check(now.hasStrategy && (long long)now.payload.size() == wanted, "in the copy its strategy is " + std::to_string(wanted) + " bytes (it is " +
			      (now.hasStrategy ? std::to_string((unsigned long long)now.payload.size()) : std::string("not there")) + ")");
			continue;
		}

		if (field == "nullid")
		{
			// an object field holds the null ID (a repair reset a target no object has; added 2026-11-17). The reader gives back false for the null ID too, after writing its 8 zero
			// bytes; a field it can't read leaves the bytes as they were, and they start as a non-null pattern here
			uint8_t id[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
			uint8_t const none[8] = {};

			if (now.hasStrategy)
			{
				host::StrategyView_FieldObject(now.kind, now.payload, argv[a + 1], id);
			}

			Check(now.hasStrategy && memcmp(id, none, 8) == 0, "in the copy its " + std::string(argv[a + 1]) + " is the null ID");
			continue;
		}

		if (field == "targets" || field.compare(0, 7, "target.") == 0 || field.compare(0, 15, "target_request.") == 0)
		{
			std::vector<host::MultiTarget> list;
			bool const multi = now.hasStrategy && (now.kind == 66 || now.kind == 68) && host::StrategyView_MultiTargets(now.payload, &list);

			if (field == "targets")
			{
				long long const wanted = strtoll(argv[a + 1], nullptr, 0);

				Check(multi && (long long)list.size() == wanted, "in the copy its list holds " + std::to_string(wanted) + " targets (" + (multi ? std::to_string((unsigned long long)list.size()) :
				      std::string("no 66 / 68 list read, kind ") + std::to_string((long long)now.kind)) + ")");
				continue;
			}

			size_t const dot = field.find('.');
			char *end = nullptr;
			unsigned long const index = strtoul(field.c_str() + dot + 1, &end, 10);
			bool const there = multi && end && *end == 0 && end != field.c_str() + dot + 1 && index < list.size();
			std::string const which = "target " + field.substr(dot + 1);

			if (field.compare(0, 7, "target.") == 0)
			{
				std::string target(8, 0);
				bool known = std::string(argv[a + 1]) == "none";

				for (auto const &one : after)
				{
					if (!known && one.second.name == argv[a + 1])
					{
						target = one.first;
						known = true;
					}
				}

				Check(there && known && memcmp(list[index].id.data(), target.data(), 8) == 0, "in the copy its " + which + " is \"" + std::string(argv[a + 1]) + "\"" +
				      (!known ? " (no object has that name)" : !there ? " (the list has no such target)" : ""));
				continue;
			}

			long long const wanted = strtoll(argv[a + 1], nullptr, 0);

			Check(there && (long long)list[index].request == wanted, "in the copy its " + which + "'s request is " + std::to_string(wanted) + " (" +
			      (there ? std::to_string((unsigned long long)list[index].request) : std::string("the list has no such target")) + ")");
			continue;
		}

		if (field == "holds")
		{
			// the strategy holds the named object's ID (after its 12-byte header)
			std::string target;

			for (auto const &one : after)
			{
				if (one.second.name == argv[a + 1])
				{
					target = one.first;
					break;
				}
			}

			bool held = false;

			for (size_t at = 12; !target.empty() && at + 8 <= now.payload.size() && !held; at++)
			{
				held = memcmp(now.payload.data() + at, target.data(), 8) == 0;
			}

			Check(!target.empty() && held, "in the copy its strategy holds the ID of \"" + std::string(argv[a + 1]) + "\"" + (target.empty() ? " (no object has that name)" : ""));
			continue;
		}

		// the wanted value: a number in any base strtoll takes, from -2147483648 to 4294967295, so a flags word with its top bit set can be given in hex ("0x80000002" is the
		// field's -2147483646; strtol held it at 2147483647: Vasquez, 2026-11-17). A word that isn't such a number is a failed check, never a 0 or a clamped value.
		char *wantedEnd = nullptr;

		errno = 0;

		long long const wantedNumber = strtoll(argv[a + 1], &wantedEnd, 0);

		if (wantedEnd == argv[a + 1] || *wantedEnd != 0 || errno == ERANGE || wantedNumber < -2147483648LL || wantedNumber > 4294967295LL)
		{
			Check(false, "the value \"" + std::string(argv[a + 1]) + "\" for " + field + " is a whole number from -2147483648 to 4294967295 (0x prefix for hex)");
			continue;
		}

		int32_t const wanted = (int32_t)(uint32_t)wantedNumber;
		int32_t value = 0, was = 0;

		// read first: the words below are made from the value (a call's arguments may be worked out in any order, so the words were made before the read and said 0)
		bool const read = now.hasStrategy && host::StrategyView_FieldNumber(now.kind, now.payload, field.c_str(), &value);

		Check(read && value == wanted, "in the copy its " + field + " is " + std::to_string((long long)wanted) + " (it is " + (read ? std::to_string((long long)value) : std::string("not readable")) +
		      ", kind " + std::to_string((long long)now.kind) + ")");

		if (before.count(key) && before[key].hasStrategy && host::StrategyView_FieldNumber(before[key].kind, before[key].payload, field.c_str(), &was))
		{
			printf("  --  in the original it was %d\n", was);
		}
	}

	size_t others = 0, changed = 0;
	std::vector<std::string> also;

	for (int a = 4; a + 1 < argc; a += 2)
	{
		if (std::string(argv[a]) == "also")
		{
			also.push_back(argv[a + 1]);
		}
	}

	for (auto const &one : before)
	{
		if (one.first == key || std::find(also.begin(), also.end(), one.second.name) != also.end())
		{
			continue;
		}

		others++;

		auto const found = after.find(one.first);

		if (found == after.end() || found->second.hasStrategy != one.second.hasStrategy || found->second.payload != one.second.payload)
		{
			changed++;
			printf("  --  changed: %s\n", one.second.name.c_str());
		}
	}

	Check(changed == 0, "every other object's strategy (" + std::to_string((long long)others) + ") is the original's, byte for byte");

	if (checksFailed)
	{
		printf("FAILED verify_strategy (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed verify_strategy\n");
	return 0;
}
