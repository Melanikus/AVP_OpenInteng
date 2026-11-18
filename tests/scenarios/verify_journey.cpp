// verify_journey: the check of THE MAPPER'S JOURNEY (tests/scenarios/acceptance-mapper-journey.txt): a level its one player start was taken out of, then given a start, an object, a texture
// and a light by his path, read back with the game's own loader. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_journey.exe <the level with no start> <the copy saved at the end> <start x|y|z> <object name> <light x|y|z> <within> [<picture file name>]
// CHECKS: both are read by the game's loader; the level with no start has none; the copy has exactly ONE player start (the game starts the player at the first, projload.cpp
// setup_start_position), within <within> of the start's place on each axis (the file's units); the copy has an object of that name and the level before has none; the copy has one light
// more, and a light within <within> of the light's place that the level before has not; with a picture's name, of the shapes both levels hold (by number) exactly ONE polygon's colour word
// changed, and the picture it now holds (its low 12 bits) is the BMPNAMES entry of that file (the name as the chooser said it, compared without regard to capitals or the slashes' kind).
// Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "bitmap_names.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "light_edit.h"
#include "special_objects_edit.h"

#include "arg_number.h"

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

// "x|y|z": three whole numbers within the game's level (the loader's own limit is far inside a long)
static bool Place(char const *what, char const *text, long out[3])
{
	std::string const words = text ? text : "";
	size_t const first = words.find('|');
	size_t const second = first == std::string::npos ? std::string::npos : words.find('|', first + 1);

	if (second == std::string::npos || words.find('|', second + 1) != std::string::npos)
	{
		printf("usage: %s is x|y|z, not \"%s\"\n", what, words.c_str());
		return false;
	}

	std::string const parts[3] = { words.substr(0, first), words.substr(first + 1, second - first - 1), words.substr(second + 1) };

	for (int i = 0; i < 3; i++)
	{
		if (!ArgNumber(what, parts[i].c_str(), -1000000000L, 1000000000L, &out[i]))
		{
			return false;
		}
	}

	return true;
}

static bool Near(int32_t const held[3], long const want[3], long within)
{
	for (int i = 0; i < 3; i++)
	{
		if ((long long)held[i] < (long long)want[i] - within || (long long)held[i] > (long long)want[i] + within)
		{
			return false;
		}
	}

	return true;
}

static bool HasObjectNamed(OpenLevelFile &level, char const *name)
{
	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (each() && strncmp(each()->object_data.o_name, name, sizeof(each()->object_data.o_name)) == 0)
		{
			return true;
		}
	}

	return false;
}

// every shape of the level by its number: its polygons' colour words
static std::map<int, std::vector<int>> Colours(OpenLevelFile &level)
{
	std::map<int, std::vector<int>> out;
	List<Chunk *> shapes;

	level.Level()->lookup_child("REBSHAPE", shapes);

	for (LIF<Chunk *> each(&shapes); !each.done(); each.next())
	{
		Shape_Chunk *const shape = static_cast<Shape_Chunk *>(each());
		Shape_Header_Chunk *const header = shape ? shape->get_header() : nullptr;
		std::vector<unsigned char> bytes;
		int number = -1;

		if (header && level.PayloadOf(header, &bytes) && bytes.size() >= 24)
		{
			memcpy(&number, bytes.data() + 20, 4);   // the shape's number (shape_model.h, poly_edit's kShapeNumberAt)
		}

		if (number < 0 || out.count(number))
		{
			continue;
		}

		std::vector<int> &colours = out[number];

		for (int p = 0; p < shape->shape_data.num_polys && shape->shape_data.poly_list; p++)
		{
			colours.push_back(shape->shape_data.poly_list[p].colour);
		}
	}

	return out;
}

static std::string Plain(std::string text)
{
	for (char &c : text)
	{
		c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
	}

	return text;
}

static std::vector<host::SpecialObject> Starts(OpenLevelFile *level, bool *read)
{
	std::vector<host::SpecialObject> all, starts;

	*read = host::SpecialObjects_Read(level, &all) == host::SpecialResult::Done;

	for (host::SpecialObject const &one : all)
	{
		if (one.kind == host::SpecialKind::PlayerStart)
		{
			starts.push_back(one);
		}
	}

	return starts;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 7)
	{
		printf("usage: verify_journey <the level with no start> <the copy saved at the end> <start x|y|z> <object name> <light x|y|z> <within>\n");
		return 2;
	}

	long start[3], light[3], within = 0;

	if (!Place("the start", argv[3], start) || !Place("the light", argv[5], light) || !ArgNumber("within", argv[6], 0, 1000000L, &within) || !argv[4][0])
	{
		return 2;
	}

	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level with no start");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy saved at the end");

	if (!before.Level() || !after.Level())
	{
		return 1;
	}

	// the start
	bool readBefore = false, readAfter = false;
	std::vector<host::SpecialObject> const startsBefore = Starts(&before, &readBefore);
	std::vector<host::SpecialObject> const startsAfter = Starts(&after, &readAfter);

	Check(readBefore && startsBefore.empty(), "the level before has no player start (it has " + std::to_string((unsigned long long)startsBefore.size()) + ")");
	Check(readAfter && startsAfter.size() == 1 && Near(startsAfter[0].place, start, within), "the copy has one player start, within " + std::to_string(within) + " of " + argv[3] + " (" +
		std::to_string((unsigned long long)startsAfter.size()) + (startsAfter.empty() ? std::string(")") : ", the first at " + std::to_string(startsAfter[0].place[0]) + " " + std::to_string(startsAfter[0].place[1]) + " " +
		std::to_string(startsAfter[0].place[2]) + ")"));

	// the object
	Check(!HasObjectNamed(before, argv[4]) && HasObjectNamed(after, argv[4]), std::string("the copy has the object \"") + argv[4] + "\" and the level before has not");

	// the light
	size_t was = 0, now = 0;
	std::vector<int32_t> placesBefore, placesAfter;
	bool const counted = host::CountLights(&before, &was) == host::LightEditResult::Done && host::CountLights(&after, &now) == host::LightEditResult::Done &&
		host::LightPlaces(&before, &placesBefore) == host::LightEditResult::Done && host::LightPlaces(&after, &placesAfter) == host::LightEditResult::Done;

	Check(counted && now == was + 1, "the copy has one light more: " + std::to_string((unsigned long long)was) + " to " + std::to_string((unsigned long long)now));

	bool newLight = false;
	std::string nearest = "none";

	for (size_t k = 0; counted && k + 2 < placesAfter.size(); k += 3)
	{
		int32_t const here[3] = { placesAfter[k], placesAfter[k + 1], placesAfter[k + 2] };
		bool before = false;

		if (!Near(here, light, within))
		{
			continue;
		}

		for (size_t b = 0; b + 2 < placesBefore.size() && !before; b += 3)
		{
			before = placesBefore[b] == here[0] && placesBefore[b + 1] == here[1] && placesBefore[b + 2] == here[2];
		}

		if (!before)
		{
			newLight = true;
			nearest = std::to_string(here[0]) + " " + std::to_string(here[1]) + " " + std::to_string(here[2]);
		}
	}

	Check(newLight, std::string("the copy has a new light within ") + std::to_string(within) + " of " + argv[5] + " (" + nearest + ")");

	// the texture
	if (argc >= 8)
	{
		std::map<int, std::vector<int>> const was = Colours(before), now = Colours(after);
		int changed = 0, picture = -1;

		for (auto const &shape : was)
		{
			auto const there = now.find(shape.first);

			for (size_t p = 0; there != now.end() && p < shape.second.size() && p < there->second.size(); p++)
			{
				if (shape.second[p] != there->second[p])
				{
					changed++;
					picture = there->second[p] & 0xfff;
				}
			}
		}

		Chunk *names = nullptr;
		std::string held = "(none)";

		if (picture >= 0 && host::FindEnvironmentChild(after.Level(), "BMPNAMES", &names) == host::LevelChunkFound::Found && names)
		{
			Chunk_With_BMPs *const holder = static_cast<Chunk_With_BMPs *>(names);

			for (LIF<BMP_Name> each(&holder->bmps); !each.done(); each.next())
			{
				if (each().index == picture && each().filename)
				{
					held = each().filename;
				}
			}
		}

		Check(changed == 1, "of the shapes both levels hold, exactly one polygon's picture changed (" + std::to_string(changed) + ")");
		Check(changed == 1 && Plain(held) == Plain(argv[7]), std::string("its picture is ") + argv[7] + " (picture " + std::to_string(picture) + ": " + held + ")");
	}

	if (checksFailed)
	{
		printf("FAILED verify_journey (%d)\n", checksFailed);
		return 1;
	}

	printf("PASSED verify_journey\n");
	return 0;
}
