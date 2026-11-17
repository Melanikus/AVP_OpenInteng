// verify_objects: the check of a scenario's saved copy for the objects it must have taken out or kept, read back through the game's own loader (as verify_strategy). Written 2026-11-17
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_objects.exe <the copy it saved> gone|kept|track|notrack <an object's name> [gone|kept|track|notrack <name>] ...
//      ... tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>   ... notracksound <name>|<index>
// CHECKS: the copy is read by the game's loader; for "gone <name>" no object of the copy (module or placed object) has that exact name; for "kept <name>" one has; for
// "track <name>" the object of that name holds a track of its own (OBJTRAK2), for "notrack <name>" it holds none (added 2026-11-17 by Frost, OP-20 / OP-21).
// For "alt <name>=<count>,<group>" the object's alternate locations (its ALTLOCAT, read by alt_locations_edit.cpp's loader from the copy the game's loader took) are that many, in that group
// (added 2026-11-17 by Drake, OP-34..38).
// For "tracksound" the object of that name holds the track sound with that index (TRAKSOUN: 0 the sound, 1 the start sound, 2 the end sound) and it has exactly that wav name
// and those numbers, as the file holds them (the ranges in the file's units). Added 2026-11-17 by Vasquez (Track Editing T-1: the Sound Edit window's OK reaches the save).
// For "notracksound <name>|<index>" the object of that name exists and holds no track sound with that index (TB-T1: Cancel on a sound Edit made adds none).
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

int main(int argc, char **argv)
{
	if (argc < 4 || (argc - 2) % 2 != 0)
	{
		printf("verify_objects: <copy> gone|kept|track|notrack <name> [gone|kept|track|notrack <name>] ... [tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>] [notracksound <name>|<index>] [alt <name>=<count>,<group>]\n");
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
		else if (what == "tracksound")
		{
			CheckTrackSound(copy, argv[a + 1]);
		}
		else if (what == "notracksound")
		{
			CheckNoTrackSound(copy, argv[a + 1]);
		}
		else
		{
			Check(false, "a word the check knows (gone, kept, track, notrack, tracksound, notracksound or alt), not \"" + what + "\"");
		}
	}

	printf("%s verify_objects\n", checksFailed ? "FAILED" : "PASSED");
	return checksFailed ? 1 : 0;
}
