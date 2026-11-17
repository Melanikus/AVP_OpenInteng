// The test of Find Errors on a Mission Objective (kind 70; FUN_005335f0; row S-K70-R of AvP-docs\engine\inteng\reassess\strategy-data-kinds.md, SKD-70-7 of strategy-kinds-dietrich.md): every
// alteration whose target no object answers to is removed, the LAST entry moved into its place, the count shrunk. Written 2026-11-16 by Melanikus (Dietrich, then Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_strategy_repair_test.exe <a stock level .rif> <a folder it may write its copy into> <a stock level with a Track Object (Destructable), Area52>. The levels are only read; the copy is the only file written.
// THE CASE: no stock level has a Mission Objective with alterations (AvP-docs\tools\drake\kind-survey, 2026-11-16: every one has 0), so the test gives the level's first Mission Objective four by the
// editor's own SetStrategyBytes: an ID no object has, a virtual object, another unknown ID, another virtual object (U1, V1, U2, V2). inteng's walk (FUN_005335f0 0x533626-0x53363e): U1 is
// rejected, the last entry V2 takes its place and is looked at in turn (kept), V1 is kept, U2 is rejected and is the last, so nothing moves: the list is V2, V1. A second pass changes nothing; the
// request targets after the list are as they were; the copy is taken by the game's loader.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"   // Shape_Chunk: its SHPMORPH child (the morphing target case)
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_edit.h"
#include "strategy_lookup.h"
#include "level_store.h"
#include "strategy_edit.h"   // LookUpStrategyBytes
#include "strategy_repair.h"
#include "strategy_views.h"   // StrategyRecordRef, for ListStrategyRecords (the Multi / Area Switch case)
#include "virtual_object.h"
#include "placed_hierarchy_strategy.h"
#include "chunk_sound.hpp"   // Sound_Object_Chunk::CalculateID
#include "avp_chunk.hpp"     // AVP_Particle_Generator_Data_Chunk

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as host_strategy_named_test)

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

static int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
{
	int32_t value = 0;

	if (at + 4 <= bytes.size())
	{
		memcpy(&value, bytes.data() + at, 4);
	}

	return value;
}

// The kind-70 strategy bytes of a virtual object, or false
static bool MissionBytes(OpenLevelFile &level, uint8_t const *id, std::vector<uint8_t> *out)
{
	host::ObjectStrategy found;

	if (host::FindObjectStrategy(level.Level(), id, &found) != host::StrategyFound::Found || found.payload.size() < 44 || IntAt(found.payload, 8) != 70)
	{
		return false;
	}

	*out = found.payload;
	return true;
}

// One alteration entry: the target's ID and its word
static std::vector<uint8_t> Entry(uint8_t const *id, int32_t word)
{
	std::vector<uint8_t> one(12);

	memcpy(one.data(), id, 8);
	memcpy(one.data() + 8, &word, 4);
	return one;
}

// THE TRACK CASE (FUN_00534450, rows S-K72-R / S-K80-R): the level's first placed object holding that kind (72, or 80) gets four point effects: A = unknown, virtual; B = unknown; C = none;
// D = virtual (and, for 80, an unknown destruct target). inteng's walk leaves A = the virtual (the last target moved into the unknown's place), B and C deleted (C had no target at all),
// D kept, in their order; for 80 the destruct target is the null ID and its request word as it was. A second pass changes nothing.
static void TrackCase(char const *path, int32_t kind)
{
	std::string const label = "kind " + std::to_string((long long)kind) + ": ";
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, label + "the level opens: " + path);
		return;
	}

	std::vector<host::VirtualObjectInfo> virtuals;
	host::VirtualObject_List(&level, &virtuals);

	List<Object_Chunk *> objects;
	level.Level()->list_objects(&objects);

	uint8_t subject[8] = { 0 };
	std::vector<uint8_t> before;
	bool found = false;

	for (LIF<Object_Chunk *> each(&objects); !each.done() && !found; each.next())
	{
		host::ObjectStrategy strategy;
		uint8_t id[8];

		if (!each())
		{
			continue;
		}

		memcpy(id, &each()->object_data.ID, 8);

		if (host::FindObjectStrategy(level.Level(), id, &strategy) == host::StrategyFound::Found && strategy.payload.size() >= 24 && IntAt(strategy.payload, 8) == kind)
		{
			memcpy(subject, id, 8);
			before = strategy.payload;
			found = true;
		}
	}

	Check(found && virtuals.size() >= 2, label + "the level has a placed object holding it, and two virtual objects");

	if (!found || virtuals.size() < 2)
	{
		return;
	}

	// the bytes after the effects, as they are (the kind's own words)
	size_t at = 24;
	int32_t const count = IntAt(before, 20);
	bool walked = count >= 0;

	for (int32_t i = 0; walked && i < count; i++)
	{
		int32_t const n = IntAt(before, at + 8);

		walked = at + 12 <= before.size() && n >= 0 && (size_t)n <= (before.size() - at - 12) / 16;
		at += walked ? 12 + 16 * (size_t)n : 0;
	}

	Check(walked, label + "its effects are read (" + std::to_string((long long)count) + ")");

	if (!walked)
	{
		return;
	}

	std::vector<uint8_t> tail(before.begin() + (std::ptrdiff_t)at, before.end());
	uint8_t unknown1[8], unknown2[8], unknown3[8];

	memset(unknown1, 0xEE, 8);
	memset(unknown2, 0xDD, 8);
	memset(unknown3, 0xCC, 8);

	auto const target = [](uint8_t const *id, int32_t request, int32_t direction)
	{
		std::vector<uint8_t> one(16);

		memcpy(one.data(), id, 8);
		memcpy(one.data() + 8, &request, 4);
		memcpy(one.data() + 12, &direction, 4);
		return one;
	};

	auto const effect = [](int32_t point, std::vector<std::vector<uint8_t>> const &targets)
	{
		std::vector<uint8_t> one(12, 0);
		int32_t const n = (int32_t)targets.size();

		memcpy(one.data(), &point, 4);
		memcpy(one.data() + 8, &n, 4);

		for (std::vector<uint8_t> const &t : targets)
		{
			one.insert(one.end(), t.begin(), t.end());
		}

		return one;
	};

	std::vector<uint8_t> const tA1 = target(unknown1, 1, 0), tA2 = target(virtuals[0].id, 3, 1), tB = target(unknown2, 0, 0), tD = target(virtuals[1].id, 2, 0);
	std::vector<uint8_t> planted(before.begin(), before.begin() + 24);
	int32_t const four = 4;

	memcpy(planted.data() + 20, &four, 4);

	for (std::vector<uint8_t> const &one : { effect(0, { tA1, tA2 }), effect(1, { tB }), effect(2, {}), effect(3, { tD }) })
	{
		planted.insert(planted.end(), one.begin(), one.end());
	}

	std::vector<uint8_t> plantedTail = tail;

	if (kind == 80 && plantedTail.size() >= 12)
	{
		memcpy(plantedTail.data() + plantedTail.size() - 8, unknown3, 8);   // the destruct target: an ID no object has
	}

	planted.insert(planted.end(), plantedTail.begin(), plantedTail.end());
	Check(host::SetStrategyBytes(&level, subject, planted.data(), planted.size()) == host::ObjectEditResult::Done, label + "the four effects are put in (a placed object: SetStrategyBytes)");

	// then an edit the way the editor's field edits are made (SetPayload through the store: the parsed class still holds the bytes before it): D's pad word becomes 0x55. The repair must
	// rebuild from the bytes as they are NOW, or it writes the edit away (found 2026-11-16: the pass read the class, FindObjectStrategy)
	host::ObjectStrategy plantedNow;
	std::vector<uint8_t> withEdit = planted;
	size_t const dPad = 24 + (12 + 32) + (12 + 16) + 12 + 4;   // A (two targets), B (one), C (none), then D's point and its pad word
	int32_t const mark = 0x55;

	memcpy(withEdit.data() + dPad, &mark, 4);

	host::LevelStore store(&level);

	Check(host::FindObjectStrategy(level.Level(), subject, &plantedNow) == host::StrategyFound::Found && store.SetPayload(plantedNow.chunk, withEdit),
	      label + "a field edit through the store: D's pad word 0x55");

	host::StrategyRepair_Run(&level);

	host::ObjectStrategy now;
	std::vector<uint8_t> expected(before.begin(), before.begin() + 24);
	int32_t const two = 2;

	memcpy(expected.data() + 20, &two, 4);

	for (std::vector<uint8_t> const &one : { effect(0, { tA2 }), effect(3, { tD }) })
	{
		expected.insert(expected.end(), one.begin(), one.end());
	}

	std::vector<uint8_t> expectedTail = plantedTail;

	if (kind == 80 && expectedTail.size() >= 12)
	{
		memset(expectedTail.data() + expectedTail.size() - 8, 0, 8);   // the null ID; the request word before it as it was
	}

	expected.insert(expected.end(), expectedTail.begin(), expectedTail.end());
	memcpy(expected.data() + 24 + (12 + 16) + 4, &mark, 4);   // D's pad word, after A (one target left): the field edit kept

	bool const read = host::FindObjectStrategy(level.Level(), subject, &now) == host::StrategyFound::Found;

	Check(read && IntAt(now.payload, 20) == 2, label + "two effects are left: " + std::to_string((long long)(read ? IntAt(now.payload, 20) : -1)));
	Check(read && now.payload == expected, label + "A keeps its virtual target (moved into the unknown's place), B (its only target unknown) and C (no target) are gone, D kept, in order" +
	      (kind == 80 ? "; the destruct target is the null ID, its request word as it was" : ""));
	Check(host::StrategyRepair_Run(&level) == 0, label + "a second pass changes nothing");
}

// THE OTHER RECORDS (FUN_00521cb0, FUN_005219f0; added 2026-11-17 by Drake): inteng keeps a record for each enemy and generator, sound, placed hierarchy and particle generator, and a target that
// names one EXISTS (its record type is not 0). A Placed Object (62) of the level is given each such target in turn, with its request word 1; the repair pass must keep both. RED before: the
// repair knew objects and virtual objects only, and reset each of these targets to the null ID (a switch's generator, sound, dropship or particle generator lost to Find Errors).
static void OtherRecordsCase(char const *path)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "other records: the level opens: " + std::string(path));
		return;
	}

	// a placed object holding a Placed Object (62): its Event Target is the ID at 44, its request word at 40
	List<Object_Chunk *> objects;
	level.Level()->list_objects(&objects);

	uint8_t subject[8] = { 0 };
	std::vector<uint8_t> before;

	for (LIF<Object_Chunk *> each(&objects); !each.done() && before.empty(); each.next())
	{
		host::ObjectStrategy strategy;
		uint8_t id[8];

		if (!each())
		{
			continue;
		}

		memcpy(id, &each()->object_data.ID, 8);

		if (host::FindObjectStrategy(level.Level(), id, &strategy) == host::StrategyFound::Found && strategy.payload.size() >= 52 && IntAt(strategy.payload, 8) == 62)
		{
			memcpy(subject, id, 8);
			before = strategy.payload;
		}
	}

	// the targets: the first of each kind of record the level has, with the ID the game's library gives it
	struct Named
	{
		std::string what;
		uint8_t id[8];
	};

	std::vector<Named> targets;
	std::vector<host::GeneratorRef> generators;
	std::vector<host::PlacedHierarchyRef> placed;

	host::ListGenerators(level.Level(), &generators);
	host::ListPlacedHierarchies(level.Level(), &placed);

	if (!generators.empty())
	{
		Named one = { "an enemy or generator (" + generators[0].name + ")" };
		memcpy(one.id, generators[0].id, 8);
		targets.push_back(one);
	}

	if (!placed.empty())
	{
		Named one = { "a placed hierarchy (" + placed[0].name + ")" };
		memcpy(one.id, placed[0].id, 8);
		targets.push_back(one);
	}

	Chunk *const env = level.Level()->lookup_single_child("REBENVDT");
	Chunk *const spec = env && !env->r_u_miscellaneous() ? ((Chunk_With_Children *)env)->lookup_single_child("SPECLOBJ") : nullptr;

	if (spec && !spec->r_u_miscellaneous())
	{
		List<Chunk *> sounds, particles;

		((Chunk_With_Children *)spec)->lookup_child("SOUNDOB2", sounds);
		((Chunk_With_Children *)spec)->lookup_child("PARGENER", particles);

		if (sounds.size() && !sounds.first_entry()->r_u_miscellaneous() && ((Sound_Object_Chunk *)sounds.first_entry())->snd_name)
		{
			Sound_Object_Chunk *const sound = (Sound_Object_Chunk *)sounds.first_entry();
			ObjectID const id = sound->CalculateID();
			Named one = { std::string("a sound (") + sound->snd_name + ")" };

			memcpy(one.id, &id, 8);
			targets.push_back(one);
		}

		if (particles.size() && !particles.first_entry()->r_u_miscellaneous())
		{
			Chunk *const data = ((Chunk_With_Children *)particles.first_entry())->lookup_single_child("PARGENDA");

			if (data && !data->r_u_miscellaneous())
			{
				Named one = { "a particle generator" };

				memcpy(one.id, &((AVP_Particle_Generator_Data_Chunk *)data)->id, 8);
				targets.push_back(one);
			}
		}
	}

	// FUN_005219f0's last case (Vasquez 11-17): a placed object with NO strategy whose own shape has SHPMORPH is a target that exists (kept); one with no strategy and a shape that does not morph is
	// not (the second control below). RED before: the morphing one was reset (strategy_repair.h read the case as the SELECTED object's)
	uint8_t plain[8] = { 0 };
	bool havePlain = false;

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		host::ObjectStrategy strategy;
		uint8_t id[8];

		if (!object)
		{
			continue;
		}

		memcpy(id, &object->object_data.ID, 8);

		if (host::FindObjectStrategy(level.Level(), id, &strategy) == host::StrategyFound::Found)
		{
			continue;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();
		bool const morph = shape && !shape->r_u_miscellaneous() && shape->lookup_single_child("SHPMORPH") != nullptr;

		if (morph && targets.size() < 5)
		{
			Named one = { std::string("a placed object with no strategy whose shape morphs (") + object->object_data.o_name + ")" };

			memcpy(one.id, id, 8);
			targets.push_back(one);
		}
		else if (!morph && !havePlain && shape)
		{
			memcpy(plain, id, 8);
			havePlain = true;
		}
	}

	Check(havePlain, "other records: the level has a placed object with no strategy and a shape that does not morph (the second control)");
	Check(!before.empty() && targets.size() >= 4, "other records: the level has a Placed Object (62) and at least four kinds of other records (" + std::to_string((unsigned long long)targets.size()) + ")");

	if (before.empty())
	{
		return;
	}

	// the control: a target no record has IS reset (the null ID, request 0), so the case can tell
	{
		std::vector<uint8_t> bytes = before;
		int32_t const request = 1;

		memset(bytes.data() + 44, 0xA7, 8);
		memcpy(bytes.data() + 40, &request, 4);

		bool const put = host::SetStrategyBytes(&level, subject, bytes.data(), bytes.size()) == host::ObjectEditResult::Done;

		host::StrategyRepair_Run(&level);

		host::ObjectStrategy now;
		uint8_t const none[8] = {};
		bool const read = put && host::LookUpStrategyBytes(level.Level(), host::LevelStore(&level), subject, &now) == host::StrategyFound::Found && now.payload.size() >= 52;   // as the store holds it: a field edit waits there for the save

		Check(read && memcmp(now.payload.data() + 44, none, 8) == 0 && IntAt(now.payload, 40) == 0, "other records: the control, a target no record has, is reset (the null ID, request 0)");
	}

	// the second control: a placed object with no strategy whose shape does not morph is no target that exists (FUN_005219f0 answers no): reset
	if (havePlain)
	{
		std::vector<uint8_t> bytes = before;
		int32_t const request = 1;

		memcpy(bytes.data() + 44, plain, 8);
		memcpy(bytes.data() + 40, &request, 4);

		bool const put = host::SetStrategyBytes(&level, subject, bytes.data(), bytes.size()) == host::ObjectEditResult::Done;

		host::StrategyRepair_Run(&level);

		host::ObjectStrategy now;
		uint8_t const none[8] = {};
		bool const read = put && host::LookUpStrategyBytes(level.Level(), host::LevelStore(&level), subject, &now) == host::StrategyFound::Found && now.payload.size() >= 52;

		Check(read && memcmp(now.payload.data() + 44, none, 8) == 0 && IntAt(now.payload, 40) == 0, "other records: the second control, a placed object with no strategy and no SHPMORPH, is reset");
	}

	for (Named const &target : targets)
	{
		std::vector<uint8_t> bytes = before;
		int32_t const request = 1;

		memcpy(bytes.data() + 44, target.id, 8);
		memcpy(bytes.data() + 40, &request, 4);

		bool const put = host::SetStrategyBytes(&level, subject, bytes.data(), bytes.size()) == host::ObjectEditResult::Done;

		host::StrategyRepair_Run(&level);

		host::ObjectStrategy now;
		bool const read = put && host::LookUpStrategyBytes(level.Level(), host::LevelStore(&level), subject, &now) == host::StrategyFound::Found && now.payload.size() >= 52;   // as the store holds it: a field edit waits there for the save

		Check(read && memcmp(now.payload.data() + 44, target.id, 8) == 0 && IntAt(now.payload, 40) == 1, "other records: a target naming " + target.what + " is kept, its request word too");
	}
}

// THE HIERARCHY OBJECT (76; vtable 0x5af6f4 +0x1c = FUN_00534450, 72's repair, read from inteng.exe 2026-11-17; row S-K76-R): a placed hierarchy's flag effects are 72's point effects. The
// level's first placed hierarchy holding a 76 gets one more target in its first effect, an ID no object has; the repair pass takes it out again: the bytes are as they were. RED before: the
// placed hierarchies were no records and 76 had no repair.
static void HierarchyObjectCase(char const *path)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "kind 76: the level opens: " + std::string(path));
		return;
	}

	std::vector<host::PlacedHierarchyRef> placed;
	host::ListPlacedHierarchies(level.Level(), &placed);

	uint8_t subject[8] = { 0 };
	std::vector<uint8_t> before;

	for (host::PlacedHierarchyRef const &one : placed)
	{
		host::ObjectStrategy strategy;

		if (before.empty() && host::FindObjectStrategy(level.Level(), one.id, &strategy) == host::StrategyFound::Found && strategy.payload.size() >= 36 &&
		    IntAt(strategy.payload, 8) == 76 && IntAt(strategy.payload, 20) >= 1)
		{
			memcpy(subject, one.id, 8);
			before = strategy.payload;
		}
	}

	Check(!before.empty(), "kind 76: the level has a placed hierarchy holding a Hierarchy Object with a flag effect");

	if (before.empty())
	{
		return;
	}

	// the first effect: its flag, a pad word, its target count at 24 + 8, then 16 bytes per target; one more target goes after its last
	int32_t const count = IntAt(before, 32);

	Check(count >= 0 && 36 + 16 * (size_t)count <= before.size(), "kind 76: its first effect's targets are read (" + std::to_string((long long)count) + ")");

	if (count < 0 || 36 + 16 * (size_t)count > before.size())
	{
		return;
	}

	std::vector<uint8_t> bytes = before;
	std::vector<uint8_t> dead(16, 0);
	int32_t const more = count + 1, request = 1, direction = 3;

	memset(dead.data(), 0xBB, 8);
	memcpy(dead.data() + 8, &request, 4);
	memcpy(dead.data() + 12, &direction, 4);
	bytes.insert(bytes.begin() + (std::ptrdiff_t)(36 + 16 * (size_t)count), dead.begin(), dead.end());
	memcpy(bytes.data() + 32, &more, 4);

	Check(host::PlacedHierarchyStrategy_Put(&level, subject, bytes.data(), bytes.size()) == host::PlacedHierarchyStrategyResult::Done, "kind 76: the dead target is put in (" +
	      std::to_string((unsigned long long)bytes.size()) + " bytes)");

	size_t const changed = host::StrategyRepair_Run(&level);
	host::ObjectStrategy now;
	bool const read = host::FindObjectStrategy(level.Level(), subject, &now) == host::StrategyFound::Found;

	Check(changed >= 1 && read && now.payload == before, "kind 76: the repair takes the dead target out: the bytes are the level's own again (" +
	      std::to_string((unsigned long long)(read ? now.payload.size() : 0)) + " bytes)");
}

// THE MULTI / AREA SWITCH CASE (FUN_00530820, rows S-K66-12 / S-K68-12; Vasquez 11-17): the level's first strategy of that kind (66 or 68) gets four targets, U1, V1, U2, V2 (unknown IDs and
// virtual objects), its Links and the words after them kept. The walk is 70's (FUN_00530820 0x530833-0x53087a): U1 rejected, V2 moved into its place and kept, V1 kept, U2 rejected and the last:
// the targets are V2, V1 with their requests; the Links, pad and flags after the list as they were; a second pass changes nothing.
static void MultiCase(char const *path, int32_t kind)
{
	std::string const label = "kind " + std::to_string((long long)kind) + ": ";
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, label + "the level opens: " + path);
		return;
	}

	std::vector<host::VirtualObjectInfo> virtuals;
	std::vector<host::StrategyRecordRef> records;

	host::VirtualObject_List(&level, &virtuals);
	host::ListStrategyRecords(level.Level(), &records);

	host::StrategyRecordRef const *subject = nullptr;

	for (host::StrategyRecordRef const &one : records)
	{
		if (one.kind == kind && one.payload.size() >= 40 && IntAt(one.payload, 36) >= 0 && (size_t)IntAt(one.payload, 36) <= (one.payload.size() - 40) / 12)
		{
			subject = &one;
			break;
		}
	}

	std::vector<uint8_t const *> others;

	for (host::VirtualObjectInfo const &one : virtuals)
	{
		if (others.size() < 2 && (!subject || memcmp(one.id, subject->id.data(), 8) != 0))
		{
			others.push_back(one.id);
		}
	}

	Check(subject != nullptr && others.size() == 2, label + "the level has one (and two virtual objects for targets): " + path);

	if (!subject || others.size() != 2)
	{
		return;
	}

	std::vector<uint8_t> const before = subject->payload;
	uint8_t id[8];
	uint8_t unknown1[8], unknown2[8];

	memcpy(id, subject->id.data(), 8);
	memset(unknown1, 0xEE, 8);
	memset(unknown2, 0xDD, 8);

	int32_t const count = IntAt(before, 36);
	size_t const rest = 40 + 12 * (size_t)count;   // the Links, pad and flags after the targets
	std::vector<uint8_t> const u1 = Entry(unknown1, 1), v1 = Entry(others[0], 2), u2 = Entry(unknown2, 3), v2 = Entry(others[1], 1);
	std::vector<uint8_t> planted(before.begin(), before.begin() + 40);
	int32_t const four = 4;

	memcpy(planted.data() + 36, &four, 4);

	for (std::vector<uint8_t> const *entry : { &u1, &v1, &u2, &v2 })
	{
		planted.insert(planted.end(), entry->begin(), entry->end());
	}

	planted.insert(planted.end(), before.begin() + (std::ptrdiff_t)rest, before.end());

	bool const put = host::SetStrategyBytes(&level, id, planted.data(), planted.size()) == host::ObjectEditResult::Done ||
	                 host::VirtualObject_SetStrategyBytes(&level, id, planted.data(), planted.size()) == host::VirtualObjectResult::Done;

	Check(put, label + "four targets are put in: unknown, virtual, unknown, virtual");

	size_t const changed = host::StrategyRepair_Run(&level);
	host::ObjectStrategy after;

	Check(changed >= 1 && host::FindObjectStrategy(level.Level(), id, &after) == host::StrategyFound::Found && after.payload != planted, label + "the repair pass changes it (its own bytes, not only another record's)");
	Check(IntAt(after.payload, 36) == 2, label + "two targets are left: " + std::to_string((long long)IntAt(after.payload, 36)));

	std::vector<uint8_t> expected(v2);

	expected.insert(expected.end(), v1.begin(), v1.end());
	Check(after.payload.size() >= 40 + expected.size() && memcmp(after.payload.data() + 40, expected.data(), expected.size()) == 0, label + "the targets are V2 (moved into the first place), then V1, with their requests");
	Check(after.payload.size() == 40 + 24 + (before.size() - rest) && memcmp(after.payload.data() + 64, before.data() + rest, before.size() - rest) == 0,
		label + "the Links, pad and flags after the targets are as they were");
	Check(memcmp(after.payload.data(), before.data(), 36) == 0, label + "the words before the list are as they were");
	Check(host::StrategyRepair_Run(&level) == 0, label + "a second pass changes nothing");
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("  FAILED  usage: host_strategy_repair_test <level .rif> <folder for the copy>\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the level opens: %s\n", argv[1]);
		return 1;
	}

	std::vector<host::VirtualObjectInfo> virtuals;
	uint8_t subject[8] = { 0 };
	std::vector<uint8_t> before;
	std::vector<uint8_t const *> others;
	bool found = false;

	host::VirtualObject_List(&level, &virtuals);

	for (host::VirtualObjectInfo const &one : virtuals)
	{
		std::vector<uint8_t> bytes;

		if (!found && MissionBytes(level, one.id, &bytes) && IntAt(bytes, 40) == 0)
		{
			memcpy(subject, one.id, 8);
			before = bytes;
			found = true;
		}
		else if (others.size() < 2)
		{
			others.push_back(one.id);
		}
	}

	Check(found && others.size() == 2, "the level has a Mission Objective with no alterations and two other virtual objects");

	if (!found || others.size() != 2)
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	// U1, V1, U2, V2 (IDs no object has: 0xEE.. and 0xDD..), the words 3, 1, 2, 3
	uint8_t unknown1[8], unknown2[8];

	memset(unknown1, 0xEE, 8);
	memset(unknown2, 0xDD, 8);

	std::vector<uint8_t> const u1 = Entry(unknown1, 3), v1 = Entry(others[0], 1), u2 = Entry(unknown2, 2), v2 = Entry(others[1], 3);
	std::vector<uint8_t> planted(before.begin(), before.begin() + 44);
	int32_t const four = 4;

	memcpy(planted.data() + 40, &four, 4);

	for (std::vector<uint8_t> const *entry : { &u1, &v1, &u2, &v2 })
	{
		planted.insert(planted.end(), entry->begin(), entry->end());
	}

	planted.insert(planted.end(), before.begin() + 44, before.end());   // the request targets and the rest, as they were

	// object_edit.h's SetStrategyBytes reaches placed objects and modules only: a virtual object's ID is not one of them (the write every stock Mission Objective needs)
	Check(host::SetStrategyBytes(&level, subject, planted.data(), planted.size()) == host::ObjectEditResult::NoSuchObject, "SetStrategyBytes does not reach a virtual object (NoSuchObject)");

	// VirtualObject_SetStrategyBytes' refusals, each for its reason, the level unchanged
	std::vector<uint8_t> const huge(65537, 0);

	Check(host::VirtualObject_SetStrategyBytes(&level, subject, planted.data(), 11) == host::VirtualObjectResult::BadBytes, "refused: 11 bytes are shorter than a strategy's header (BadBytes)");
	Check(host::VirtualObject_SetStrategyBytes(&level, subject, huge.data(), huge.size()) == host::VirtualObjectResult::BadBytes, "refused: 65537 bytes are more than a strategy chunk the editor makes (BadBytes)");
	Check(host::VirtualObject_SetStrategyBytes(&level, unknown1, planted.data(), planted.size()) == host::VirtualObjectResult::NotFound, "refused: no virtual object has the ID (NotFound)");
	Check(host::VirtualObject_SetStrategyBytes(nullptr, subject, planted.data(), planted.size()) == host::VirtualObjectResult::BadCall, "refused: no level (BadCall)");

	std::vector<uint8_t> still;

	Check(MissionBytes(level, subject, &still) && still == before, "after the refusals the Mission Objective is as it was");
	Check(host::VirtualObject_SetStrategyBytes(&level, subject, planted.data(), planted.size()) == host::VirtualObjectResult::Done, "four alterations are put in: unknown, virtual, unknown, virtual");

	size_t const changed = host::StrategyRepair_Run(&level);
	std::vector<uint8_t> after;

	Check(changed >= 1 && MissionBytes(level, subject, &after), "the repair pass changes the Mission Objective");
	Check(IntAt(after, 40) == 2, "two alterations are left: " + std::to_string(IntAt(after, 40)));

	// inteng's walk leaves V2 (moved into U1's place) then V1
	std::vector<uint8_t> expected(v2);

	expected.insert(expected.end(), v1.begin(), v1.end());

	Check(after.size() >= 44 + expected.size() && memcmp(after.data() + 44, expected.data(), expected.size()) == 0, "the list is V2 (moved into the first place), then V1, with their words");
	Check(after.size() == before.size() + 24 && memcmp(after.data() + 44 + 24, before.data() + 44, before.size() - 44) == 0, "the bytes after the list (the request targets) are as they were");
	Check(memcmp(after.data(), before.data(), 40) == 0, "the words before the list are as they were");
	Check(host::StrategyRepair_Run(&level) == 0, "a second pass changes nothing");

	std::string const folder = argv[2];
	std::string const copyPath = folder + "\\repair-copy.rif";

	CreateDirectoryA(folder.c_str(), nullptr);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;
	std::vector<uint8_t> reread;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");
	Check(reopened.Level() && MissionBytes(reopened, subject, &reread) && reread == after, "the copy holds the repaired Mission Objective");

	TrackCase(argv[1], 72);
	MultiCase(argv[1], 66);
	MultiCase(argv[1], 68);

	if (argc >= 4)
	{
		TrackCase(argv[3], 80);
	}
	else
	{
		Check(false, "a level with a Track Object (Destructable) is named (the third argument, {LEVEL:Area52})");
	}

	OtherRecordsCase(argv[1]);
	HierarchyObjectCase(argv[1]);

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
