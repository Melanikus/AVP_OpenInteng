// The test of Light Sourcing's O between ANY two lights (light_keys.h LightCopyOver; inteng's FUN_004acf30 0x4ad34e-0x4ad3f5, AvP-docs\engine\inteng\reassess\light-closeout-frost.md TB-L1): a light of the
// set over a light of the set, a placed light over a light of the set, a light of the set over a placed light and a placed light over a placed light, each on a fresh open of a real level, the copy
// saved and read again by the game's loader. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_light_copy_over_test.exe <Invasion> <a folder for the copies> <a placed lamp> <another placed lamp>. With no arguments it says so and passes. The level is only read.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_light.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "light_edit.h"
#include "light_keys.h"

static int checksFailed = 0;
static int checks = 0;

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

static int32_t W(std::vector<unsigned char> const &b, size_t at)
{
	int32_t v = 0;

	if (at + 4 <= b.size())
	{
		memcpy(&v, b.data() + at, 4);
	}

	return v;
}

static bool IdOf(OpenLevelFile &f, char const *name, uint8_t *id)
{
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			memcpy(id, &each()->object_data.ID, 8);
			return true;
		}
	}

	return false;
}

static std::vector<unsigned char> SetLight(OpenLevelFile &f, size_t light)
{
	std::vector<unsigned char> b;
	Chunk *const c = host::FindLightChunk(f.Level(), light);

	if (c)
	{
		f.PayloadOf(c, &b);
	}

	return b;
}

static std::vector<unsigned char> PlacedLight(OpenLevelFile &f, uint8_t const *id)
{
	std::vector<unsigned char> b;
	Chunk *const c = host::FindPlacedLightChunk(f.Level(), id);

	if (c)
	{
		f.PayloadOf(c, &b);
	}

	return b;
}

// STDLIGHT: brightness 52, spread 56, range 60, colour 64, engine 68, local 72. PLOBJLIT: brightness 0, spread 4, range 8, up 12, down 16, engine 20, local 24.
static bool SavedAndRead(OpenLevelFile &f, std::string const &path)
{
	if (f.SaveAs(path.c_str()) != LevelSaveResult::Saved)
	{
		return false;
	}

	OpenLevelFile again;

	return again.Open(path.c_str()) == LevelOpenResult::Opened && again.Level() != nullptr;
}

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		printf("host_light_copy_over_test: no level given (<Invasion> <folder> <lamp> <another lamp>), nothing to test: passed\n");
		return 0;
	}

	std::string const folder = argv[2];

	// 1. a light of the set over a light of the set (LightPasteOver's rule, kept)
	{
		OpenLevelFile f;

		Check(f.Open(argv[1]) == LevelOpenResult::Opened && f.Level(), "the level opens");

		std::vector<unsigned char> const from = SetLight(f, 0), before = SetLight(f, 1);
		host::LightKeyRef target, source;

		target.light = 1;
		source.light = 0;
		Check(host::LightCopyOver(&f, target, source, true) == host::LightEditResult::Done, "O: light 1 of the set over light 2");

		std::vector<unsigned char> const after = SetLight(f, 1);

		Check(after.size() == before.size() && memcmp(after.data() + 52, from.data() + 52, 20) == 0, "light 2 has light 1's brightness, spread, range, colour and engine flags");
		Check((uint32_t)W(after, 72) == ((uint32_t)W(from, 72) | 4), "and its local flags with 4 (changed) in");
		Check(memcmp(after.data(), before.data(), 52) == 0 && memcmp(after.data() + 76, before.data() + 76, after.size() - 76) == 0, "its number, place and turn and what follows are its own");
		Check(SavedAndRead(f, folder + "\\copyover-1.rif"), "the copy is read by the game's loader");
	}

	// 2. a PLACED light over a light of the set, its DOWN colour chosen (T): sourceDown
	{
		OpenLevelFile f;
		uint8_t lamp[8];

		Check(f.Open(argv[1]) == LevelOpenResult::Opened && f.Level() && IdOf(f, argv[3], lamp), std::string("the level opens and holds ") + argv[3]);

		std::vector<unsigned char> const from = PlacedLight(f, lamp), before = SetLight(f, 0);
		host::LightKeyRef target, source;

		source.placed = true;
		memcpy(source.objectId, lamp, 8);
		target.light = 0;
		Check(from.size() >= 64 && host::LightCopyOver(&f, target, source, true) == host::LightEditResult::Done, std::string("O: the placed light ") + argv[3] + " over light 1 of the set");

		std::vector<unsigned char> const after = SetLight(f, 0);

		Check(W(after, 52) == W(from, 0) && W(after, 56) == W(from, 4) && W(after, 60) == W(from, 8), "light 1 has the lamp's brightness, spread (its PLOBJLIT's: NAME-L1) and range");
		Check(W(after, 64) == W(from, 16), "the lamp's DOWN colour (its [5] chose it)");
		Check(W(after, 68) == W(from, 20) && (uint32_t)W(after, 72) == ((uint32_t)W(from, 24) | 4), "its engine flags, and its local flags with 4 in");
		Check(memcmp(after.data(), before.data(), 52) == 0 && memcmp(after.data() + 76, before.data() + 76, after.size() - 76) == 0, "light 1's number, place and turn are its own");
		Check(PlacedLight(f, lamp) == from, "the lamp is as it was");
		Check(SavedAndRead(f, folder + "\\copyover-2.rif"), "the copy is read by the game's loader");
	}

	// 3. a light of the set over a PLACED light: the colour goes to its UP colour (a light of the set's [5] is 0), whatever T chose
	{
		OpenLevelFile f;
		uint8_t lamp[8];

		Check(f.Open(argv[1]) == LevelOpenResult::Opened && f.Level() && IdOf(f, argv[3], lamp), "the level opens again");

		std::vector<unsigned char> const from = SetLight(f, 0), before = PlacedLight(f, lamp);
		host::LightKeyRef target, source;

		target.placed = true;
		memcpy(target.objectId, lamp, 8);
		source.light = 0;
		Check(host::LightCopyOver(&f, target, source, true) == host::LightEditResult::Done, std::string("O: light 1 of the set over the placed light ") + argv[3]);

		std::vector<unsigned char> const after = PlacedLight(f, lamp);

		Check(W(after, 0) == W(from, 52) && W(after, 4) == W(from, 56) && W(after, 8) == W(from, 60), "the lamp has light 1's brightness, spread and range");
		Check(W(after, 12) == W(from, 64) && W(after, 16) == W(before, 16), "light 1's colour as its UP colour, its down colour its own");
		Check(W(after, 20) == W(from, 68) && (uint32_t)W(after, 24) == ((uint32_t)W(from, 72) | 4), "light 1's engine flags, and its local flags with 4 in");
		Check(after.size() == before.size() && memcmp(after.data() + 28, before.data() + 28, after.size() - 28) == 0, "the lamp's times, types, flags and extra words are its own");
		Check(SavedAndRead(f, folder + "\\copyover-3.rif"), "the copy is read by the game's loader");
	}

	// 4. a PLACED light over a PLACED light: the 15 numbers (0x4ad3be), its local flags with 4
	{
		OpenLevelFile f;
		uint8_t lamp[8], other[8];

		Check(f.Open(argv[1]) == LevelOpenResult::Opened && f.Level() && IdOf(f, argv[3], lamp) && IdOf(f, argv[4], other), std::string("the level opens and holds ") + argv[4]);

		std::vector<unsigned char> const from = PlacedLight(f, lamp), before = PlacedLight(f, other);
		host::LightKeyRef target, source;

		source.placed = true;
		memcpy(source.objectId, lamp, 8);
		target.placed = true;
		memcpy(target.objectId, other, 8);
		Check(memcmp(from.data(), before.data(), 60) != 0, "the two lamps differ to start with");
		Check(host::LightCopyOver(&f, target, source, false) == host::LightEditResult::Done, std::string("O: the placed light ") + argv[3] + " over the placed light " + argv[4]);

		std::vector<unsigned char> after = PlacedLight(f, other);

		Check((uint32_t)W(after, 24) == ((uint32_t)W(from, 24) | 4), "the local flags with 4 in");

		std::vector<unsigned char> wanted(from.begin(), from.begin() + 60);
		int32_t local = W(from, 24) | 4;

		memcpy(wanted.data() + 24, &local, 4);
		Check(after.size() == before.size() && memcmp(after.data(), wanted.data(), 60) == 0, "the other 14 numbers are the first lamp's (up and down colour, times, types, flags)");
		Check(memcmp(after.data() + 60, before.data() + 60, after.size() - 60) == 0, "the extra words are the target's own");
		Check(SavedAndRead(f, folder + "\\copyover-4.rif"), "the copy is read by the game's loader");
	}

	// 5. NAME-L1 (INTENG BUG, FIXED): a placed light's Spread is its PLOBJLIT's own; inteng's showed the headlight's 1000 and never saved a change
	{
		OpenLevelFile f;
		uint8_t lamp[8];

		Check(f.Open(argv[1]) == LevelOpenResult::Opened && f.Level() && IdOf(f, argv[3], lamp), "the level opens for the spread");

		std::vector<unsigned char> const before = PlacedLight(f, lamp);

		Check(host::PlacedLightAttribute_Words(&f, lamp, 1, false) == "Spread: " + std::string(W(before, 4) == 0 ? "0.00" : "?") + " deg", "the lamp's Spread is its file's, 0 (not the headlight's 87.89 deg): " +
			host::PlacedLightAttribute_Words(&f, lamp, 1, false));
		Check(host::PlacedLightAttribute_Change(&f, lamp, 1, 3, false) == host::LightEditResult::Done, "the Spread key changes it");

		std::vector<unsigned char> const after = PlacedLight(f, lamp);

		Check(W(after, 4) > W(before, 4) && memcmp(after.data(), before.data(), 4) == 0 && memcmp(after.data() + 8, before.data() + 8, 16) == 0, "in the PLOBJLIT's spread (its 2nd number), the numbers around it kept");
		Check(SavedAndRead(f, folder + "\\copyover-5.rif"), "and the copy is read by the game's loader");
	}

	// 6. wrong calls
	{
		OpenLevelFile f;
		host::LightKeyRef target, source;
		uint8_t const nobody[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };

		Check(host::LightCopyOver(nullptr, target, source, false) == host::LightEditResult::NothingOpen, "no level: nothing open");
		Check(f.Open(argv[1]) == LevelOpenResult::Opened, "the level opens once more");
		target.light = 100000;
		Check(host::LightCopyOver(&f, target, source, false) == host::LightEditResult::NoSuchLight, "a light past the set's end: no such light");
		target.light = 0;
		source.placed = true;
		memcpy(source.objectId, nobody, 8);
		Check(host::LightCopyOver(&f, target, source, false) == host::LightEditResult::NoSuchLight, "an ID no object has: no such light");
		Check(!f.HasChanges(), "and nothing changed");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED host_light_copy_over_test\n");
		return 1;
	}

	printf("passed host_light_copy_over_test\n");
	return 0;
}
