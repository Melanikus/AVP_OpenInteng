// verify_list_objects: the shared LISTER of a level's objects, read through the game's own loader (packed REBCRIF1 or not), for finding what a scenario should pick and for reading a
// saved copy back by eye. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng (Bishop's ask: one lister for everyone, from the kens-co-op bunker switch survey).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_list_objects.exe <level .rif>
// PRINTS one line per holder, in the level's order, then a count line:
//   object | <name> | id <8 bytes hex> | flags <OBJHEAD1 flags hex> | track yes|no | strategy <kind>|none | at <x y z> | box <w h d>|none   (modules and placed objects:
//            File_Chunk::list_objects; at = the OBJHEAD1 place, box = its shape's extents, max - min on each axis, so objects on top of each other show at once; added 11-18 for Bishop)
//            | imported no|yes|yes, own pictures|no shape   (imported_shape.h: its shape's outside file, and its own picture list there, which S + click refuses; added 11-19)
//   virtual | <name> | id ... | strategy <kind>|none                                                            (virtual objects)
//   generator | <name> | id ... | type <n> | strategy <kind>|none                                               (enemies and generators)
//   placedhier | <name> | id ... | strategy <kind>|none                                                         (placed hierarchies)
//   sound | <name> | id ... | strategy <kind>|none   and   particle | <name> | id ... | strategy <kind>|none     (SOUNDOB2s and particle generators, as a switch may name them)
//   count | objects <n> | with a track <n> | with a strategy <n> | virtual <n> | generators <n> | placedhier <n> | sounds <n> | particles <n> | imported <n>
// <kind> is the AVPSTRAT's kind word (strategy_chunk.hpp: 52 Binary Switch, 61 Link Switch, 62 New Simple Object, 66 Multiple Target Switch, 68 Area Switch, 72 Track, 79 Generator ...).
// The file is HOSTILE INPUT: a name is printed as at most 63 bytes up to its first zero, any byte outside 32..126 (and '|') as '?'; at most 100000 lines of each sort are printed.
// GIVES BACK 0 when the level was read, 2 when the game's loader refused it or the call is wrong.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "special_objects_edit.h"
#include "strategy_lookup.h"
#include "imported_shape.h"

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
	printf("REFUSED the game's code stopped the program (code %d)\n", code);
	exit(2);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static size_t const MostLines = 100000;

// a name from the file, safe to print on one line
static std::string Plain(char const *name, size_t room)
{
	std::string out;

	for (size_t i = 0; name && i < room && i < 63 && name[i] != 0; i++)
	{
		unsigned char const c = (unsigned char)name[i];

		out += c >= 32 && c <= 126 && c != '|' ? (char)c : '?';
	}

	return out;
}

static std::string Plain(std::string const &name)
{
	return Plain(name.c_str(), name.size());
}

static std::string IdWords(uint8_t const *id)
{
	char words[32];

	snprintf(words, sizeof(words), "%02x%02x%02x%02x %02x%02x%02x%02x", id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
	return words;
}

// the holder's strategy kind, or "none"
static std::string StrategyWords(OpenLevelFile &level, uint8_t const *id, bool *has)
{
	host::ObjectStrategy found;

	*has = false;

	if (host::FindObjectStrategy(level.Level(), id, &found) == host::StrategyFound::Found && found.payload.size() >= 12)
	{
		int32_t kind = 0;

		memcpy(&kind, found.payload.data() + 8, 4);
		*has = true;
		return std::to_string(kind);
	}

	return "none";
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc != 2)
	{
		printf("REFUSED use: verify_list_objects.exe <level .rif>\n");
		return 2;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("REFUSED %s: the game's loader did not take it\n", argv[1]);
		return 2;
	}

	size_t objects = 0, tracked = 0, withStrategy = 0, imported = 0;
	List<Object_Chunk *> all;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || objects >= MostLines)
		{
			continue;
		}

		uint8_t id[8];
		bool has = false;

		// the ID's 8 bytes a byte at a time (Snyk Code took a memcpy into an array that is later printed for a text that might not end, as 20558ba8): numbers, never text
		static_assert(sizeof(object->object_data.ID) == 8, "an object ID is 8 bytes");
		uint8_t const *const idBytes = (uint8_t const *)&object->object_data.ID;

		for (int k = 0; k < 8; k++)
		{
			id[k] = idBytes[k];
		}

		Object_Header_Chunk *const header = object->get_header();
		bool const track = object->lookup_single_child("OBJTRAK2") != nullptr;
		std::string const strategy = StrategyWords(level, id, &has);

		Shape_Chunk *const shape = object->get_assoc_shape();
		std::string box = "none";

		if (shape)
		{
			ChunkShape const &data = shape->shape_data;

			box = std::to_string((int64_t)data.max.x - (int64_t)data.min.x) + " " + std::to_string((int64_t)data.max.y - (int64_t)data.min.y) + " " +
				std::to_string((int64_t)data.max.z - (int64_t)data.min.z);
		}

		ImportedShape const outside = ImportedShapeOf(object);

		printf("object | %s | id %s | flags %x | track %s | strategy %s | at %d %d %d | box %s | imported %s\n", Plain(object->object_data.o_name, 64).c_str(), IdWords(id).c_str(),
			header ? (unsigned)header->flags : 0u, track ? "yes" : "no", strategy.c_str(), object->object_data.location.x, object->object_data.location.y, object->object_data.location.z,
			box.c_str(), ImportedShapeWords(outside));
		objects++;
		tracked += track ? 1 : 0;
		withStrategy += has ? 1 : 0;
		imported += outside == ImportedShape::Yes || outside == ImportedShape::YesOwnPictures ? 1 : 0;
	}

	std::vector<host::VirtualObjectRef> virtuals;
	host::ListVirtualObjects(level.Level(), &virtuals);

	for (size_t i = 0; i < virtuals.size() && i < MostLines; i++)
	{
		bool has = false;
		std::string const strategy = StrategyWords(level, virtuals[i].id, &has);

		printf("virtual | %s | id %s | strategy %s\n", Plain(virtuals[i].name).c_str(), IdWords(virtuals[i].id).c_str(), strategy.c_str());
	}

	std::vector<host::GeneratorRef> generators;
	host::ListGenerators(level.Level(), &generators);

	for (size_t i = 0; i < generators.size() && i < MostLines; i++)
	{
		bool has = false;
		std::string const strategy = StrategyWords(level, generators[i].id, &has);

		printf("generator | %s | id %s | type %d | strategy %s\n", Plain(generators[i].name).c_str(), IdWords(generators[i].id).c_str(), generators[i].type, strategy.c_str());
	}

	std::vector<host::PlacedHierarchyRef> placed;
	host::ListPlacedHierarchies(level.Level(), &placed);

	for (size_t i = 0; i < placed.size() && i < MostLines; i++)
	{
		bool has = false;
		std::string const strategy = StrategyWords(level, placed[i].id, &has);

		printf("placedhier | %s | id %s | strategy %s\n", Plain(placed[i].name).c_str(), IdWords(placed[i].id).c_str(), strategy.c_str());
	}

	size_t sounds = 0, particles = 0;
	std::vector<host::SpecialObject> specials;

	if (host::SpecialObjects_Read(&level, &specials) == host::SpecialResult::Done)
	{
		for (host::SpecialObject const &one : specials)
		{
			bool const sound = one.kind == host::SpecialKind::Sound;

			if ((!sound && one.kind != host::SpecialKind::ParticleGenerator) || (sound ? sounds : particles) >= MostLines)
			{
				continue;
			}

			bool has = false;
			std::string const strategy = StrategyWords(level, one.id, &has);

			printf("%s | %s | id %s | strategy %s\n", sound ? "sound" : "particle", Plain(one.name).c_str(), IdWords(one.id).c_str(), strategy.c_str());
			(sound ? sounds : particles)++;
		}
	}

	printf("count | objects %zu | with a track %zu | with a strategy %zu | virtual %zu | generators %zu | placedhier %zu | sounds %zu | particles %zu | imported %zu\n", objects, tracked,
		withStrategy, virtuals.size(), generators.size(), placed.size(), sounds, particles, imported);
	return 0;
}
