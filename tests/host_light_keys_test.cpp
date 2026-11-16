// The test for Light Sourcing's keys on the level's data (src/host/light_keys.*): the selected light's attributes, invisible, changed, the clipboard over a light, the ambience, and the write-back of Exit and Return.
// Run on a REAL level (the first argument), saved as a COPY (the second is the folder) and loaded again with the game's own loader. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_light_keys_test.exe <a level .rif> <a folder for the copy>. With no arguments it says so and passes. The level is only read.

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
#include "light_edit.h"
#include "light_keys.h"

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

static int32_t At(host::LightData const &data, size_t at)
{
	int32_t value = 0;

	memcpy(&value, data.data() + at, 4);
	return value;
}

static host::LightData Light(OpenLevelFile *open, size_t index)
{
	host::LightData data;

	if (host::CopyLight(open, index, &data) != host::LightEditResult::Done)
	{
		data.clear();
	}

	return data;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_light_keys_test <a level .rif> <a folder for the copy>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	std::vector<char> original;

	Check(ReadWholeFile(argv[1], &original), "the level is read (it is only read, never written)");

	OpenLevelFile open;

	if (original.empty() || open.Open(argv[1]) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	size_t count = 0;

	Check(host::CountLights(&open, &count) == host::LightEditResult::Done && count >= 3, "the level has lights: " + std::to_string(count));

	if (count < 3)
	{
		return 1;
	}

	size_t withBits = 0;

	for (size_t i = 0; i < count; i++)
	{
		withBits += (At(Light(&open, i), 68) & 0x300) != 0 ? 1 : 0;
	}

	printf("  info  %zu of the stock level's %zu lights have engine flag bits 0x300 set\n", withBits, count);

	host::LightData const before = Light(&open, 1);

	// attributes
	printf("  info  light 1 starts with brightness %d\n", (int)At(before, 52));
	Check(host::LightAttribute_Change(&open, 1, 0, 100000) == host::LightEditResult::Done && At(Light(&open, 1), 52) == 0x20000, "Brightness is held at 0x20000");
	Check(host::LightAttribute_Change(&open, 1, 0, -100000) == host::LightEditResult::Done && At(Light(&open, 1), 52) == 0, "and at 0");
	Check(host::LightAttribute_Change(&open, 1, 0, 5) == host::LightEditResult::Done && At(Light(&open, 1), 52) == 50, "Brightness: +5 steps is +50");
	host::LightAttribute_Change(&open, 1, 0, 6554);
	Check(host::LightAttribute_Change(&open, 1, 1, 3) == host::LightEditResult::Done && At(Light(&open, 1), 56) == At(before, 56) + 6 && At(Light(&open, 1), 60) == At(before, 60), "Spread: +3 steps is +6, the range is as it was");
	Check(host::LightAttribute_Change(&open, 1, 1, -100000) == host::LightEditResult::Done && At(Light(&open, 1), 56) == 1, "Spread is held at 1 at least (and 0x800 at most)");
	Check(host::LightAttribute_Change(&open, 1, 2, 2) == host::LightEditResult::Done && At(Light(&open, 1), 60) == At(before, 60) + 20000, "Range: +2 steps is +20000");
	Check(host::LightAttribute_Change(&open, 1, 2, -100000) == host::LightEditResult::Done && At(Light(&open, 1), 60) == 1, "Range is held at 1 (the loader refuses 0)");
	Check(host::LightAttribute_Change(&open, 1, 8, 255) == host::LightEditResult::Done && ((At(Light(&open, 1), 64) >> 16) & 0xff) == 255, "Red is held at 255");
	Check(host::LightAttribute_Change(&open, 1, 9, -300) == host::LightEditResult::Done && ((At(Light(&open, 1), 64) >> 8) & 0xff) == 0, "Green is held at 0");
	Check(host::LightAttribute_Change(&open, 1, 10, 1) == host::LightEditResult::Done, "Blue +1");
	Check(host::LightAttribute_Change(&open, 1, 6, 1) == host::LightEditResult::Done && (At(Light(&open, 1), 68) & 4) != 0 && host::LightAttribute_Change(&open, 1, 6, -1) == host::LightEditResult::Done && (At(Light(&open, 1), 68) & 4) == 0, "Omni: up puts engine flag 4 in, down takes it out");
	Check(host::LightAttribute_Change(&open, 1, 3, 1) == host::LightEditResult::Done && (At(Light(&open, 1), 72) & 2) != 0, "Shadows: up puts local flag 2 in");
	Check(host::LightAttribute_Change(&open, 1, 11, 1) == host::LightEditResult::Done && (At(Light(&open, 1), 72) & 0x40) != 0 && (At(Light(&open, 1), 68) & 0x304) == 0x304, "No PreLight up: local flag 0x40 and engine 0x304");
	Check(host::LightAttribute_Change(&open, 1, 12, -1) == host::LightEditResult::Done && (At(Light(&open, 1), 68) & 0x800) != 0 && host::LightAttribute_Change(&open, 1, 12, 1) == host::LightEditResult::Done && (At(Light(&open, 1), 68) & 0x800) == 0, "Specular: down puts engine flag 0x800 in, up takes it out");
	Check((At(Light(&open, 1), 72) & 4) != 0, "every change put the local flag 4 (changed) in");
	Check(At(Light(&open, 1), 4) == At(before, 4) && At(Light(&open, 1), 0) == At(before, 0), "the place and the number are as they were");
	Check(host::LightAttribute_Change(&open, 99999, 0, 1) == host::LightEditResult::NoSuchLight && host::LightAttribute_Change(&open, 1, 99, 1) == host::LightEditResult::NoSuchLight && host::LightAttribute_Change(nullptr, 1, 0, 1) == host::LightEditResult::NothingOpen, "no such light, no such attribute, no level: refused");
	Check(host::LightAttribute_Words(&open, 1, 8).find("Red: 255") == 0 && std::string(host::LightAttribute_Name(4)) == "Atten" && host::LightAttribute_Words(&open, 1, 99).empty(), "the words of an attribute");

	// invisible and changed
	bool invisible = false;

	host::LightChanged_Clear(&open, 1);
	Check((At(Light(&open, 1), 72) & 4) == 0, "U takes the changed flag out");
	Check(host::LightInvisible_Toggle(&open, 1, &invisible) == host::LightEditResult::Done && invisible && (At(Light(&open, 1), 72) & 0x20) != 0, "I makes the light invisible");
	Check(host::LightInvisible_Toggle(&open, 1, &invisible) == host::LightEditResult::Done && !invisible && (At(Light(&open, 1), 72) & 0x20) == 0, "I again makes it visible");

	// the clipboard over a light
	host::LightData const clip = Light(&open, 0);
	host::LightData const target = Light(&open, 2);

	Check(host::LightPasteOver(&open, 2, clip) == host::LightEditResult::Done, "O puts the clipboard over a light");

	host::LightData const pasted = Light(&open, 2);

	Check(At(pasted, 52) == At(clip, 52) && At(pasted, 56) == At(clip, 56) && At(pasted, 60) == At(clip, 60) && At(pasted, 64) == At(clip, 64) && At(pasted, 68) == At(clip, 68) && At(pasted, 72) == (At(clip, 72) | 4),
		"its brightness, spread, range, colour, engine flags are the clipboard's and its local flags too, with changed put in");
	Check(At(pasted, 0) == At(target, 0) && At(pasted, 4) == At(target, 4) && At(pasted, 8) == At(target, 8) && At(pasted, 12) == At(target, 12), "its number and place are its own");
	Check(host::LightPasteOver(&open, 2, host::LightData(10)) == host::LightEditResult::BadLight, "a clipboard that is not 84 bytes is refused");

	// fly light: the place
	{
		int32_t const there[3] = { 1234, -567, 8910 };

		Check(host::LightPlace_Set(&open, 2, there) == host::LightEditResult::Done && At(Light(&open, 2), 4) == 1234 && At(Light(&open, 2), 8) == -567 && At(Light(&open, 2), 12) == 8910 && At(Light(&open, 2), 0) == At(target, 0), "L: the light is put at the place, its number is its own");

		int32_t const away[3] = { 1 << 25, 0, 0 };

		Check(host::LightPlace_Set(&open, 2, away) == host::LightEditResult::PlaceOutOfRange && At(Light(&open, 2), 4) == 1234, "a place past 2^24 is refused and the light stays");
	}

	// a PLACED light: the first object that has one
	{
		uint8_t id[8];
		bool found = false;
		::List<Object_Chunk *> objects;

		open.Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done() && !found; each.next())
		{
			if (each() && host::PlacedLight_Exists(&open, (uint8_t const *)&each()->object_data.ID))
			{
				memcpy(id, &each()->object_data.ID, 8);
				printf("  info  the first object with a placed light is object number %d\n", (int)each()->object_data.index_num);
				found = true;
			}
		}

		Check(found, "the level has an object with a placed light");

		if (found)
		{
			Check(host::PlacedLightAttribute_Change(&open, id, 0, 5, false) == host::LightEditResult::Done && host::PlacedLightAttribute_Words(&open, id, 0, false).find("Brightness: ") == 0, "a placed light's brightness changes");
			Check(host::PlacedLightAttribute_Change(&open, id, 8, 1, false) == host::LightEditResult::Done && host::PlacedLightAttribute_Words(&open, id, 8, false).find("Up colour: Red: ") == 0, "its Red is the UP colour's");
			Check(host::PlacedLightAttribute_Change(&open, id, 8, 1, true) == host::LightEditResult::Done && host::PlacedLightAttribute_Words(&open, id, 8, true).find("Down colour: Red: ") == 0, "and with T it is the DOWN colour's");
			Check(host::PlacedLightAttribute_Change(&open, id, 6, 1, false) == host::LightEditResult::Done && host::PlacedLightAttribute_Words(&open, id, 6, false) == "Omni: on", "Omni up");
			Check(host::PlacedLightAttribute_Change(&open, id, 99, 1, false) == host::LightEditResult::NoSuchLight && !host::PlacedLight_Exists(&open, nullptr), "no such attribute, no id: refused");
		}

		uint8_t none[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

		Check(!host::PlacedLight_Exists(&open, none) && host::PlacedLightAttribute_Change(&open, none, 0, 1, false) == host::LightEditResult::NoSuchLight, "an object that has none: refused");
	}

	// D: the headlight's numbers
	{
		int32_t const matrix[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		host::LightData const drop = host::LightDefaultDrop(matrix);

		Check(drop.size() == host::kLightBytes && At(drop, 52) == 0x10000 && At(drop, 56) == 1000 && At(drop, 60) == 4000000 && At(drop, 64) == 0xffffff && At(drop, 68) == 3 && At(drop, 72) == 5 && At(drop, 16) == 1 && At(drop, 48) == 9,
			"a light dropped from nothing has the headlight's numbers: 100%, spread 1000, range 4000000, white, engine flags 3, local flags 5, the camera's turn");

		size_t before = 0, after = 0;

		host::CountLights(&open, &before);

		size_t made = 0;
		int32_t const place[3] = { 100, 200, 300 };

		Check(host::DropLight(&open, place, &drop, &made) == host::LightEditResult::Done && made == before, "DropLight takes them");

		host::CountLights(&open, &after);
		Check(after == before + 1 && At(Light(&open, made), 52) == 0x10000 && At(Light(&open, made), 60) == 4000000 && At(Light(&open, made), 4) == 100 && At(Light(&open, made), 16) == 1, "the new light has them and the place given: the level has one light more");
		host::RemoveLight(&open, made);
		count = before;
	}

	// the ambience
	int percent = -1;

	Check(host::Ambience_Change(&open, 1, &percent) == host::LightEditResult::Done && percent >= 0 && percent <= 100, "the ambience goes up a hundredth: " + std::to_string(percent) + "%");

	int bigger = -1;

	Check(host::Ambience_Change(&open, 1000, &bigger) == host::LightEditResult::Done && bigger == 100, "and is held at 100%");
	Check(host::Ambience_Change(&open, -1000, &bigger) == host::LightEditResult::Done && bigger == 0, "and at 0%");
	host::Ambience_Change(&open, 40, &bigger);

	// the write-back: bits 0x300 taken out of every light's engine flags, nothing else changed
	std::vector<host::LightData> beforeWrite;

	for (size_t i = 0; i < count; i++)
	{
		beforeWrite.push_back(Light(&open, i));
	}

	size_t changed = 0;

	Check(host::LightsWriteBack(&open, true, &changed) == host::LightEditResult::Done && changed >= 1, "the write-back clears bits 0x300 of the engine flags: " + std::to_string(changed) + " lights");

	bool exact = true;
	bool cleared = true;

	for (size_t i = 0; i < count; i++)
	{
		host::LightData const now = Light(&open, i);
		host::LightData wanted = beforeWrite[i];
		int32_t flags = At(wanted, 68) & (int32_t)0xfffffcff;
		int32_t local = At(wanted, 72) & ~0x20;   // Exit un-hides every light

		memcpy(wanted.data() + 68, &flags, 4);
		memcpy(wanted.data() + 72, &local, 4);
		exact = exact && now == wanted;
		cleared = cleared && (At(now, 68) & 0x300) == 0;
	}

	Check(cleared, "no light has bits 0x300 any more");
	Check(exact, "and nothing else changed (the light's other numbers and flags are as they were, the invisible flag taken out by Exit)");
	Check(host::LightsWriteBack(&open, false, &changed) == host::LightEditResult::Done && changed == 0, "written again there is nothing to change");

	std::string const copyPath = std::string(argv[2]) + "\\light-keys-copy.rif";

	CreateDirectoryA(argv[2], NULL);
	DeleteFileA(copyPath.c_str());
	Check(open.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile copy;

	Check(copy.Open(copyPath.c_str()) == LevelOpenResult::Opened && copy.Level(), "the game's loader takes the copy");

	bool same = true;

	for (size_t i = 0; i < count; i++)
	{
		same = same && Light(&copy, i) == Light(&open, i);
	}

	Check(same, "every light in the copy is what the editor holds");

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");
	DeleteFileA(copyPath.c_str());

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
