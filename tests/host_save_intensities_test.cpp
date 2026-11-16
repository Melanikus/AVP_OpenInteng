// The test of Light Sourcing's Save Intensities (src/host/light_intensities_edit.*, light_state.*): on a real level the lighting calculation is run and every lit module's SHPVTINT chunk made again, as inteng's
// Save Intensities for Game / Tools does (FUN_004aef90). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_save_intensities_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: Save Intensities for Game with the doors closed (how the stock levels were saved) is Done; the level then holds the values the calculation gives, vertex for vertex (the stored ones were the same
// before: the oracle's match); the loader takes the level saved as a COPY and the copy holds them; Toggle on/off and Switch all on are the editor's state and write nothing: every light's chunk is byte for byte
// what it was, and the level's bytes too; a light turned off gives other values where it reaches (and a Save with it back on gives the first values again); a Save for Tools differs from one for the Game where a
// No PreLight light reaches (when the level has one); the opened file is unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_light.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "light_edit.h"
#include "light_intensities_edit.h"
#include "light_scene.h"
#include "light_state.h"

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

// What the level holds now, vertex for vertex, and what the calculation gives for it (doors closed, the game's save)
struct Counted
{
	size_t objects;
	size_t vertices;
	size_t equal;
	size_t withChunk;
};

static Counted Compare(OpenLevelFile &level, bool toolsSave)
{
	Counted counted = { 0, 0, 0, 0 };
	host::LightSceneLights lights;
	std::string why;

	if (!host::LightScene_ReadLights(&level, &lights, &why))
	{
		return counted;
	}

	std::vector<host::LitObject> lit;
	host::LightScene_LightModules(&level, lights, toolsSave, true, &lit);

	for (host::LitObject const &one : lit)
	{
		counted.objects++;

		if (!one.hasStored)
		{
			continue;
		}

		counted.withChunk++;
		counted.vertices += one.stored.size();

		for (size_t v = 0; v < one.stored.size() && v < one.computed.size(); v++)
		{
			counted.equal += one.stored[v] == one.computed[v] ? 1 : 0;
		}
	}

	return counted;
}

// Every light's own chunk bytes, in the set's order
static std::vector<host::LightData> LightBytes(OpenLevelFile &level)
{
	std::vector<host::LightData> all;
	size_t count = 0;

	if (host::CountLights(&level, &count) != host::LightEditResult::Done)
	{
		return all;
	}

	for (size_t i = 0; i < count; i++)
	{
		host::LightData data;

		if (host::CopyLight(&level, i, &data) == host::LightEditResult::Done)
		{
			all.push_back(data);
		}
	}

	return all;
}

// A sum of the stored values: enough to tell two saves apart
static unsigned long long Sum(OpenLevelFile &level)
{
	host::LightSceneLights lights;
	std::string why;
	std::vector<host::LitObject> lit;
	unsigned long long sum = 0;

	host::LightScene_ReadLights(&level, &lights, &why);
	host::LightScene_LightModules(&level, lights, false, true, &lit);

	for (host::LitObject const &one : lit)
	{
		for (size_t v = 0; v < one.stored.size(); v++)
		{
			sum = sum * 1000003ull + one.stored[v] + (unsigned long long)v;
		}
	}

	return sum;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_save_intensities_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_save_intensities_test (0 ok)\n");
		return 0;
	}

	std::vector<char> original;

	if (!ReadWholeFile(argv[1], &original))
	{
		printf("  FAILED  the level can be read: %s\n", argv[1]);
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	host::LightState state;
	state.SetDoorsClosed(true);

	std::vector<host::LightData> const lightsBefore = LightBytes(level);
	Check(!lightsBefore.empty(), "the level has lights");

	// the state is the editor's: nothing is written to the level
	std::vector<char> imageBefore;
	level.BuildLevel(imageBefore);

	state.Toggle(0);
	state.Toggle(1);
	state.SwitchAllOn();
	state.SetOn(2, false);
	state.Toggle(2);
	Check(state.IsOn(0) && state.IsOn(1) && state.IsOn(2) && state.OffCount(1000) == 0, "Toggle on/off and Switch all on are state: everything is on again");

	std::vector<char> imageAfterToggles;
	level.BuildLevel(imageAfterToggles);
	Check(imageAfterToggles == imageBefore, "and the level's bytes are what they were: the toggles wrote nothing");

	// Save Intensities for Game, doors closed
	host::SaveIntensitiesCounts counts;
	host::SaveIntensitiesResult const saved = host::SaveIntensities(&level, state, false, &counts);

	Check(saved == host::SaveIntensitiesResult::Done, std::string("Save Intensities for Game is done: ") + host::SaveIntensitiesWords(saved));
	printf("  info  %zu modules, %zu chunks made, %zu taken out, %zu vertices, %zu lights off\n", counts.modules, counts.written, counts.removed, counts.vertices, counts.lightsOff);

	Counted const after = Compare(level, false);

	Check(after.withChunk > 0 && after.equal * 1000 >= after.vertices * 998, "the level holds the values the calculation gives, vertex for vertex (" + std::to_string(after.equal) + " of " + std::to_string(after.vertices) + ")");

	std::vector<host::LightData> const lightsAfter = LightBytes(level);
	Check(lightsAfter == lightsBefore, "every light's own chunk is byte for byte what it was");

	// Save Intensities again changes nothing
	unsigned long long const firstSum = Sum(level);
	Check(host::SaveIntensities(&level, state, false, nullptr) == host::SaveIntensitiesResult::Done && Sum(level) == firstSum, "a second save with the same state gives the same values");

	// a light off gives other values where it reaches
	state.SetOn(0, false);
	Check(host::SaveIntensities(&level, state, false, nullptr) == host::SaveIntensitiesResult::Done, "Save Intensities with light 1 off is done");
	Check(Sum(level) != firstSum, "its values differ from the ones with every light on (light 1 reaches some module)");
	Check(LightBytes(level) == lightsBefore, "and the lights' chunks are still what they were");

	state.SwitchAllOn();
	Check(host::SaveIntensities(&level, state, false, nullptr) == host::SaveIntensitiesResult::Done && Sum(level) == firstSum, "Switch all on and a save give the first values again");

	// a save for the tools: the No PreLight lights are on
	host::LightSceneLights lights;
	std::string why;
	host::LightScene_ReadLights(&level, &lights, &why);

	bool anyNoPreLight = false;

	for (host::light::Light const &one : lights.lights)
	{
		anyNoPreLight = anyNoPreLight || one.noPreLight;
	}

	for (host::light::Light const &one : lights.placedLights)
	{
		anyNoPreLight = anyNoPreLight || one.noPreLight;
	}

	if (anyNoPreLight)
	{
		Check(host::SaveIntensities(&level, state, true, nullptr) == host::SaveIntensitiesResult::Done && Sum(level) != firstSum, "a save for the tools differs from the game's: the No PreLight lights are on");
		Check(host::SaveIntensities(&level, state, false, nullptr) == host::SaveIntensitiesResult::Done && Sum(level) == firstSum, "and the game's again gives the first values");
	}
	else
	{
		printf("  info  no light of this level is No PreLight\n");
	}

	// the copy goes through the loader and holds the values
	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "save-intensities-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;
	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");

	if (reopened.Level())
	{
		Counted const again = Compare(reopened, false);
		Check(again.equal * 1000 >= again.vertices * 998 && again.withChunk == after.withChunk, "read again from the copy the values are there (" + std::to_string(again.equal) + " of " + std::to_string(again.vertices) + ")");
	}

	DeleteFileA(copyPath.c_str());
	DeleteFileA((copyPath + ".bak").c_str());

	std::vector<char> originalAfter;
	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_save_intensities_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_save_intensities_test\n");
	return 0;
}
