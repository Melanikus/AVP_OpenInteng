// verify_objects: the check of a scenario's saved copy for the objects it must have taken out or kept, read back through the game's own loader (as verify_strategy). Written 2026-11-17
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_objects.exe <the copy it saved> gone|kept|track|notrack <an object's name> [gone|kept|track|notrack <name>] ...
//      ... tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>   ... notracksound <name>|<index>   ... trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>   ... at <name>|<x>|<y>|<z>   ... near <name>|<x>|<y>|<z>|<within>
// CHECKS: the copy is read by the game's loader; for "gone <name>" no object of the copy (module or placed object) has that exact name; for "kept <name>" one has; for
// "track <name>" the object of that name holds a track of its own (OBJTRAK2), for "notrack <name>" it holds none (added 2026-11-17 by Frost, OP-20 / OP-21).
// For "alt <name>=<count>,<group>" the object's alternate locations (its ALTLOCAT, read by alt_locations_edit.cpp's loader from the copy the game's loader took) are that many, in that group
// (added 2026-11-17 by Drake, OP-34..38).
// For "tracksound" the object of that name holds the track sound with that index (TRAKSOUN: 0 the sound, 1 the start sound, 2 the end sound) and it has exactly that wav name
// and those numbers, as the file holds them (the ranges in the file's units). Added 2026-11-17 by Vasquez (Track Editing T-1: the Sound Edit window's OK reaches the save).
// For "notracksound <name>|<index>" the object of that name exists and holds no track sound with that index (TB-T1: Cancel on a sound Edit made adds none).
// For "trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>" the object's track has that section, its pivot from (sx sy sz) to (ex ey ez) (T-2: Shift+T moves a track onto its object).
// For "at <name>|<x>|<y>|<z>" the object's place is exactly (x y z).
// For "near <name>|<x>|<y>|<z>|<within>" each of the object's three numbers is at most <within> from (x y z), for a place the engine's dynamics chose: they step by the frame's own
// time (NormalFrameTime), so where a dropped object lands varies a little from run to run (added 2026-11-17 by Drake, OP-47: Gravity's landing place reaches the save).
// For "nostrategy <name>" the copy has a holder of that exact name (a module or placed object, a virtual object, an enemy or generator, or a placed hierarchy: the first in that order, as the
// strategy lookup finds them) and it holds no strategy: Delete Strategy took its AVPSTRAT and kept the holder (added 2026-11-17 by Bishop, S-39 on virtual objects and placed hierarchies).
// For "starts <count>" the copy holds that many player starts; for "startnear <n>|<x>|<y>|<z>|<within>" the n-th (from 1, the file's order) is within <within> of (x y z) (added 2026-11-18 by
// Drake, OP-29).
// Gives back 0 only if every check passed.

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "alt_locations_edit.h"
#include "special_alt_edit.h"
#include "strategy_lookup.h"

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

// The level's first object with exactly this name, or null
static Object_Chunk *ObjectNamed(OpenLevelFile &level, char const *name)
{
	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && object->object_data.o_name && strcmp(object->object_data.o_name, name) == 0)
		{
			return object;
		}
	}

	return nullptr;
}

// Whether an object of the level has exactly this name
static bool HasObjectNamed(OpenLevelFile &level, char const *name)
{
	return ObjectNamed(level, name) != nullptr;
}

// A whole number of the check's words, and only that: false for an empty word, one with anything after the number, or one outside [least, most]
static bool WholeNumber(std::string const &word, long long least, long long most, long long *out)
{
	if (word.empty() || word.size() > 20)
	{
		return false;
	}

	char *end = nullptr;
	long long const value = strtoll(word.c_str(), &end, 10);

	if (!end || *end != '\0' || value < least || value > most)
	{
		return false;
	}

	*out = value;
	return true;
}

// The check's words split at each '|', letter by letter; it stops once there are more fields than `most` (so a check with too many is refused by its count)
static std::vector<std::string> BarFields(std::string const &asked, size_t most)
{
	std::vector<std::string> fields(1);

	for (char const letter : asked)
	{
		if (letter != '|')
		{
			fields.back() += letter;
		}
		else if (fields.size() > most)
		{
			break;
		}
		else
		{
			fields.emplace_back();
		}
	}

	return fields;
}

// The object's track sound (TRAKSOUN) with that index, or null (no object, or no such sound)
static Object_Track_Sound_Chunk *TrackSoundOf(Object_Chunk *object, int index)
{
	if (!object)
	{
		return nullptr;
	}

	List<Chunk *> sounds = object->lookup_child("TRAKSOUN");

	for (LIF<Chunk *> each(&sounds); !each.done(); each.next())
	{
		Object_Track_Sound_Chunk *const sound = dynamic_cast<Object_Track_Sound_Chunk *>(each());

		if (sound && sound->index == index)
		{
			return sound;
		}
	}

	return nullptr;
}

// "at <name>|<x>|<y>|<z>": the object's place (OBJHEAD1, as the game's Object_Chunk holds it) is exactly that
static void CheckAt(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 4);
	long long at[3] = {};
	bool good = fields.size() == 4 && !fields[0].empty();

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "an at check as <name>|<x>|<y>|<z>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	std::string const held = object ? std::to_string(object->object_data.location.x) + " " + std::to_string(object->object_data.location.y) + " " +
		std::to_string(object->object_data.location.z) : std::string("no object of that name");

	Check(object && object->object_data.location.x == at[0] && object->object_data.location.y == at[1] && object->object_data.location.z == at[2],
		"the copy's object \"" + fields[0] + "\" is at " + fields[1] + " " + fields[2] + " " + fields[3] + " (it is at: " + held + ")");
}

// "near <name>|<x>|<y>|<z>|<within>": the object's place (OBJHEAD1) is within <within> of (x y z) on each of the three
static void CheckNear(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 5);
	long long at[3] = {};
	long long within = 0;
	bool good = fields.size() == 5 && !fields[0].empty() && WholeNumber(fields[4], 0, INT_MAX, &within);

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "a near check as <name>|<x>|<y>|<z>|<within>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	bool close = object != nullptr;

	if (object)
	{
		long long const held[3] = { object->object_data.location.x, object->object_data.location.y, object->object_data.location.z };

		for (int i = 0; i < 3; i++)
		{
			close = close && held[i] >= at[i] - within && held[i] <= at[i] + within;   // long long: no sum here can overflow (each part is an int)
		}
	}

	std::string const held = object ? std::to_string(object->object_data.location.x) + " " + std::to_string(object->object_data.location.y) + " " +
		std::to_string(object->object_data.location.z) : std::string("no object of that name");

	Check(close, "the copy's object \"" + fields[0] + "\" is within " + fields[4] + " of " + fields[1] + " " + fields[2] + " " + fields[3] + " (it is at: " + held + ")");
}

// "trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>": the object's track (OBJTRAK2) has that section, and the section's pivot starts and ends at those places (the file's units)
static void CheckTrackPivot(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 8);
	long long section = 0, at[6] = {};
	bool good = fields.size() == 8 && !fields[0].empty() && WholeNumber(fields[1], 0, INT_MAX, &section);

	for (int i = 0; good && i < 6; i++)
	{
		good = WholeNumber(fields[(size_t)i + 2], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "a trackpivot check as <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Object_Track_Chunk2 *const track = object ? dynamic_cast<Object_Track_Chunk2 *>(object->lookup_single_child("OBJTRAK2")) : nullptr;
	std::string held = object ? (track ? "no such section" : "no track") : "no object of that name";
	bool same = false;

	if (track && track->sections && section < (long long)track->num_sections)
	{
		ChunkTrackSection const &s = track->sections[section];

		held = std::to_string(s.pivot_start.x) + "|" + std::to_string(s.pivot_start.y) + "|" + std::to_string(s.pivot_start.z) + "|" + std::to_string(s.pivot_end.x) + "|" +
			std::to_string(s.pivot_end.y) + "|" + std::to_string(s.pivot_end.z);
		same = s.pivot_start.x == at[0] && s.pivot_start.y == at[1] && s.pivot_start.z == at[2] && s.pivot_end.x == at[3] && s.pivot_end.y == at[4] && s.pivot_end.z == at[5];
	}

	Check(same, "the copy's object \"" + fields[0] + "\" has track section " + fields[1] + " from " + fields[2] + " " + fields[3] + " " + fields[4] + " to " + fields[5] + " " + fields[6] + " " +
		fields[7] + " (it holds: " + held + ")");
}

// "trackendmoved <name>|<section>|<x>|<y>|<z>": the object's track has that section, and the section's pivot no longer ENDS at (x y z) (T-3b: a snap moved the end marker, wherever it landed)
static void CheckTrackEndMoved(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 5);
	long long section = 0, at[3] = {};
	bool good = fields.size() == 5 && !fields[0].empty() && WholeNumber(fields[1], 0, INT_MAX, &section);

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 2], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "a trackendmoved check as <name>|<section>|<x>|<y>|<z>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Object_Track_Chunk2 *const track = object ? dynamic_cast<Object_Track_Chunk2 *>(object->lookup_single_child("OBJTRAK2")) : nullptr;
	std::string held = object ? (track ? "no such section" : "no track") : "no object of that name";
	bool moved = false;

	if (track && track->sections && section < (long long)track->num_sections)
	{
		ChunkTrackSection const &t = track->sections[section];

		held = std::to_string(t.pivot_end.x) + " " + std::to_string(t.pivot_end.y) + " " + std::to_string(t.pivot_end.z);
		moved = t.pivot_end.x != at[0] || t.pivot_end.y != at[1] || t.pivot_end.z != at[2];
	}

	Check(moved, "the copy's object \"" + fields[0] + "\" has track section " + fields[1] + " ending somewhere else than " + fields[2] + " " + fields[3] + " " + fields[4] + " (it ends at: " + held + ")");
}

// "notracksound <name>|<index>": the object is there and holds no track sound with that index
static void CheckNoTrackSound(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 2);
	long long index = 0;

	if (fields.size() != 2 || fields[0].empty() || !WholeNumber(fields[1], 0, 2, &index))
	{
		Check(false, "a notracksound check as <name>|<index 0-2>, not \"" + asked + "\"");
		return;
	}

	std::string const &name = fields[0];
	Object_Chunk *const object = ObjectNamed(level, name.c_str());
	Object_Track_Sound_Chunk *const found = TrackSoundOf(object, (int)index);

	Check(object && !found, "the copy's object \"" + name + "\" holds no track sound " + std::to_string(index) + (object ? (found ? std::string(" (it holds one: ") +
		(found->wav_name ? found->wav_name : "(no name)") + ")" : std::string()) : std::string(" (no object of that name)")));
}

// "tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>": the object's TRAKSOUN with that index holds exactly these (a chunk under 24 bytes is not read as a track
// sound by the game's loader: RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST, so it is not found here either)
static void CheckTrackSound(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 8);
	long long index = 0, inner = 0, outer = 0, volume = 0, pitch = 0, flags = 0;

	if (fields.size() != 8 || fields[0].empty() || !WholeNumber(fields[1], 0, 2, &index) || !WholeNumber(fields[3], 0, 0xffffffffLL, &inner) ||
		!WholeNumber(fields[4], 0, 0xffffffffLL, &outer) || !WholeNumber(fields[5], INT_MIN, INT_MAX, &volume) || !WholeNumber(fields[6], INT_MIN, INT_MAX, &pitch) ||
		!WholeNumber(fields[7], INT_MIN, INT_MAX, &flags))
	{
		Check(false, "a tracksound check as <name>|<index 0-2>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Object_Track_Sound_Chunk *const found = TrackSoundOf(object, (int)index);
	std::string held = "none";

	if (found)
	{
		held = std::string(found->wav_name ? found->wav_name : "(no name)") + "|" + std::to_string(found->inner_range) + "|" + std::to_string(found->outer_range) + "|" +
			std::to_string(found->max_volume) + "|" + std::to_string(found->pitch) + "|" + std::to_string(found->flags);
	}

	bool const same = found && found->wav_name && fields[2] == found->wav_name && (long long)found->inner_range == inner && (long long)found->outer_range == outer &&
		found->max_volume == (int)volume && found->pitch == (int)pitch && found->flags == (int)flags;

	Check(same, "the copy's object \"" + fields[0] + "\" holds track sound " + fields[1] + " as " + fields[2] + "|" + fields[3] + "|" + fields[4] + "|" + fields[5] + "|" + fields[6] + "|" +
		fields[7] + (object ? " (it holds: " + held + ")" : " (no object of that name)"));
}

// "nostrategy <name>": the first holder of that exact name, in the strategy lookup's order (the level's objects, its virtual objects, its enemies and generators, its placed hierarchies), is in the
// copy and holds no strategy (FindObjectStrategy answers NoStrategy: no AVPSTRAT where its strategy would be)
static void CheckNoStrategy(OpenLevelFile &copy, char const *name)
{
	uint8_t id[8] = { 0 };
	bool found = false;
	std::string what;
	Object_Chunk *const object = ObjectNamed(copy, name);

	if (object)
	{
		memcpy(id, &object->object_data.ID, 8);
		found = true;
		what = "object";
	}

	std::vector<host::VirtualObjectRef> virtuals;
	std::vector<host::GeneratorRef> generators;
	std::vector<host::PlacedHierarchyRef> hierarchies;

	host::ListVirtualObjects(copy.Level(), &virtuals);
	host::ListGenerators(copy.Level(), &generators);
	host::ListPlacedHierarchies(copy.Level(), &hierarchies);

	for (size_t i = 0; !found && i < virtuals.size(); i++)
	{
		if (virtuals[i].name == name)
		{
			memcpy(id, virtuals[i].id, 8);
			found = true;
			what = "virtual object";
		}
	}

	for (size_t i = 0; !found && i < generators.size(); i++)
	{
		if (generators[i].name == name)
		{
			memcpy(id, generators[i].id, 8);
			found = true;
			what = "enemy or generator";
		}
	}

	for (size_t i = 0; !found && i < hierarchies.size(); i++)
	{
		if (hierarchies[i].name == name)
		{
			memcpy(id, hierarchies[i].id, 8);
			found = true;
			what = "placed hierarchy";
		}
	}

	host::ObjectStrategy held;
	host::StrategyFound const answer = found ? host::FindObjectStrategy(copy.Level(), id, &held) : host::StrategyFound::NoSuchObject;

	Check(found && answer == host::StrategyFound::NoStrategy, std::string("the copy has the ") + (found ? what : std::string("holder")) + " \"" + name + "\" and it holds no strategy (" +
		(found ? host::StrategyFoundWords(answer) : "no holder of that name") + ")");
}

int main(int argc, char **argv)
{
	if (argc < 4 || (argc - 2) % 2 != 0)
	{
		printf("verify_objects: <copy> gone|kept|track|notrack <name> [gone|kept|track|notrack <name>] ... [tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>] [notracksound <name>|<index>] [trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>] [at <name>|<x>|<y>|<z>] [trackendmoved <name>|<section>|<x>|<y>|<z>] [alt <name>=<count>,<group>] [virtualgone|virtualkept <name>] [nostrategy <name>]\n");
		return 2;
	}

	OpenLevelFile copy;

	Check(copy.Open(argv[1]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[1]);

	if (checksFailed)
	{
		printf("FAILED verify_objects\n");
		return 1;
	}

	for (int a = 2; a + 1 < argc; a += 2)
	{
		std::string const what = argv[a];
		bool const there = HasObjectNamed(copy, argv[a + 1]);

		if (what == "gone")
		{
			Check(!there, std::string("the copy has no object named \"") + argv[a + 1] + "\"");
		}
		else if (what == "kept")
		{
			Check(there, std::string("the copy has an object named \"") + argv[a + 1] + "\"");
		}
		else if (what == "track" || what == "notrack")
		{
			// the object holds a track of its own (an OBJTRAK2 chunk: Object Placement's Copy / Paste Track, OP-20 / OP-21), or holds none
			Object_Chunk *const object = ObjectNamed(copy, argv[a + 1]);
			bool const hasTrack = object && object->lookup_single_child("OBJTRAK2") != nullptr;

			Check(object && hasTrack == (what == "track"), std::string("the copy's object \"") + argv[a + 1] + "\" " + (what == "track" ? "holds a track (OBJTRAK2)" : "holds no track") +
				(object ? "" : " (no object of that name)"));
		}
		else if (what == "alt")
		{
			// <name>=<count>,<group>: the name up to the last =, then two whole numbers
			std::string const words = argv[a + 1];
			size_t const equals = words.rfind('=');
			int count = -1, group = -1;
			bool const read = equals != std::string::npos && sscanf_s(words.c_str() + equals + 1, "%d,%d", &count, &group) == 2;
			std::string const name = read ? words.substr(0, equals) : words;
			Object_Chunk *const object = read ? ObjectNamed(copy, name.c_str()) : nullptr;
			model::AltLocations locations;
			uint8_t id[8];

			if (object)
			{
				memcpy(id, &object->object_data.ID, sizeof(id));
			}

			bool const loaded = object && host::AlternateLocations_Load(&copy, id, &locations) == host::AltLocationsResult::Done;

			Check(loaded && (int)locations.list.size() == count && locations.group == group, "the copy's object \"" + name + "\" has " + std::to_string(count) + " alternate locations in group " +
				std::to_string(group) + (!read ? " (the words are not <name>=<count>,<group>)" : !object ? " (no object of that name)" : !loaded ? " (its ALTLOCAT does not load)" : " (it has " +
				std::to_string((unsigned long long)locations.list.size()) + " in group " + std::to_string(locations.group) + ")"));
		}
		else if (what == "specialalt")
		{
			// <name>=<count>,<group>: a SPECIAL object of that name (a generator, a bad guy, a sound, a particle generator), its ALTLOCAT where the game reads a special's (special_alt_edit.h;
			// added 2026-11-17 by Drake, OP-34..38 for specials)
			std::string const words = argv[a + 1];
			size_t const equals = words.rfind('=');
			int count = -1, group = -1;
			bool const read = equals != std::string::npos && sscanf_s(words.c_str() + equals + 1, "%d,%d", &count, &group) == 2;
			std::string const name = read ? words.substr(0, equals) : words;
			std::vector<host::SpecialObject> specials;
			host::SpecialObject const *found = nullptr;

			if (read && host::SpecialObjects_Read(&copy, &specials) == host::SpecialResult::Done)
			{
				for (host::SpecialObject const &one : specials)
				{
					found = !found && one.name == name && one.kind != host::SpecialKind::PlayerStart ? &one : found;
				}
			}

			model::AltLocations locations;
			bool const loaded = found && host::SpecialAlt_Load(&copy, found->kind, found->number, &locations) == host::SpecialResult::Done;

			Check(loaded && (int)locations.list.size() == count && locations.group == group, "the copy's special object \"" + name + "\" has " + std::to_string(count) + " alternate locations in group " +
				std::to_string(group) + (!read ? " (the words are not <name>=<count>,<group>)" : !found ? " (no special object of that name)" : !loaded ? " (its ALTLOCAT does not load)" : " (it has " +
				std::to_string((unsigned long long)locations.list.size()) + " in group " + std::to_string(locations.group) + ")"));
		}
		else if (what == "starts" || what == "startnear")
		{
			// "starts <count>": the copy holds that many player starts (AVPSTART); "startnear <n>|<x>|<y>|<z>|<within>": the n-th of them (from 1, in the file's order, the order inteng's
			// FUN_00422110 numbers them "Player Start %d" in) is within <within> of (x y z) on each axis, the file's units (added 2026-11-18 by Drake, OP-29)
			std::vector<host::SpecialObject> specials;
			std::vector<host::SpecialObject> starts;
			bool const read = host::SpecialObjects_Read(&copy, &specials) == host::SpecialResult::Done;

			for (host::SpecialObject const &one : specials)
			{
				if (one.kind == host::SpecialKind::PlayerStart)
				{
					starts.push_back(one);
				}
			}

			if (what == "starts")
			{
				long long want = -1;
				bool const asked = WholeNumber(argv[a + 1], 0, INT_MAX, &want);

				Check(read && asked && (long long)starts.size() == want, std::string("the copy holds ") + argv[a + 1] + " player start(s) (it holds " + std::to_string((unsigned long long)starts.size()) +
					(read ? "" : "; its special objects could not be read") + ")");
			}
			else
			{
				std::vector<std::string> const fields = BarFields(argv[a + 1], 5);
				long long n = 0, at[3] = {}, within = 0;
				bool good = fields.size() == 5 && WholeNumber(fields[0], 1, INT_MAX, &n) && WholeNumber(fields[4], 0, INT_MAX, &within);

				for (int i = 0; good && i < 3; i++)
				{
					good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &at[i]);
				}

				bool close = good && read && (size_t)n <= starts.size();
				std::string held = "no such start";

				if (close)
				{
					host::SpecialObject const &one = starts[(size_t)n - 1];

					for (int i = 0; i < 3; i++)
					{
						close = close && (long long)one.place[i] >= at[i] - within && (long long)one.place[i] <= at[i] + within;
					}

					held = std::to_string(one.place[0]) + " " + std::to_string(one.place[1]) + " " + std::to_string(one.place[2]);
				}

				Check(close, std::string("the copy's player start ") + (good ? std::to_string(n) : "(a startnear check as <n>|<x>|<y>|<z>|<within>)") + " is within " + (good ? fields[4] : "?") + " of " +
					(good ? fields[1] + " " + fields[2] + " " + fields[3] : std::string("?")) + " (it is at: " + held + "; " + std::to_string((unsigned long long)starts.size()) + " starts)");
			}
		}
		else if (what == "tracksound")
		{
			CheckTrackSound(copy, argv[a + 1]);
		}
		else if (what == "notracksound")
		{
			CheckNoTrackSound(copy, argv[a + 1]);
		}
		else if (what == "trackendmoved")
		{
			CheckTrackEndMoved(copy, argv[a + 1]);
		}
		else if (what == "trackpivot")
		{
			CheckTrackPivot(copy, argv[a + 1]);
		}
		else if (what == "virtualgone" || what == "virtualkept")
		{
			// a VIRTUAL object (VIOBJECT in SPECLOBJ, strategy_lookup.h ListVirtualObjects) of that exact name is not there / is (S-87: Delete Multiple's Obsolete Switches)
			std::vector<host::VirtualObjectRef> virtuals;
			bool there = false;

			host::ListVirtualObjects(copy.Level(), &virtuals);

			for (host::VirtualObjectRef const &v : virtuals)
			{
				there = there || v.name == argv[a + 1];
			}

			Check(there == (what == "virtualkept"), std::string("the copy ") + (what == "virtualkept" ? "has" : "has no") + " virtual object named \"" + argv[a + 1] + "\"");
		}
		else if (what == "nostrategy")
		{
			CheckNoStrategy(copy, argv[a + 1]);
		}
		else if (what == "at")
		{
			CheckAt(copy, argv[a + 1]);
		}
		else if (what == "near")
		{
			CheckNear(copy, argv[a + 1]);
		}
		else
		{
			Check(false, "a word the check knows (gone, kept, track, notrack, tracksound, notracksound, trackpivot, trackendmoved, at, near, alt, specialalt, virtualgone, virtualkept or nostrategy), not \"" + what + "\"");
		}
	}

	printf("%s verify_objects\n", checksFailed ? "FAILED" : "PASSED");
	return checksFailed ? 1 : 0;
}
