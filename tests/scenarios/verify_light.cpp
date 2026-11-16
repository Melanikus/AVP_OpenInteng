// verify_light: the check of the Light Sourcing scenario (tests/scenarios/light-main-job.txt): the level the hidden editor run saved as a copy, read with the game's own loader and the lighting calculation.
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_light.exe <the level the editor opened> <the copy it saved> <the number of the light the scenario switched off, counting from 1, or 0 for none>
// CHECKS: both levels are read by the game's loader; every light's chunk in the copy is byte for byte the one in the original (Toggle on/off is the editor's state and writes nothing); the copy holds a SHPVTINT for
// every module the original has one for; the values the copy holds are the ones the lighting calculation gives for it with the doors closed and that one light off (at least 99.5% of the vertices exact), which are not the
// values the original holds (the switched-off light changed some module) when a light was switched off. Gives back 0 only if every check passed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "light_edit.h"
#include "light_scene.h"

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

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("verify_light: give the level, its copy and the number of the light switched off (0 for none)\n");
		return 2;
	}

	long const off = atol(argv[3]);

	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level(), "the game's loader takes the original level");
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level(), "the game's loader takes the copy");

	if (!original.Level() || !copy.Level())
	{
		return 1;
	}

	std::vector<host::LightData> const before = LightBytes(original);
	std::vector<host::LightData> const after = LightBytes(copy);

	Check(!before.empty() && before == after, "every light's chunk in the copy is byte for byte the original's (" + std::to_string(before.size()) + " lights): Toggle on/off wrote nothing");

	host::LightSceneLights lights;
	std::string why;

	Check(host::LightScene_ReadLights(&copy, &lights, &why), "the copy has its light set: " + why);

	if (off > 0 && (size_t)off <= lights.lights.size())
	{
		lights.lights[(size_t)off - 1].on = false;
	}
	else if (off > 0)
	{
		Check(false, "the light the scenario switched off (" + std::to_string(off) + ") is one of the level's " + std::to_string(lights.lights.size()));
	}

	std::vector<host::LitObject> lit, litOriginal;
	host::LightScene_LightModules(&copy, lights, false, true, &lit);

	host::LightSceneLights allOn;
	host::LightScene_ReadLights(&original, &allOn, &why);
	host::LightScene_LightModules(&original, allOn, false, true, &litOriginal);

	size_t modulesWith = 0, modulesWithOriginal = 0, vertices = 0, exact = 0, changedModules = 0;

	for (host::LitObject const &one : litOriginal)
	{
		modulesWithOriginal += one.hasStored ? 1 : 0;
	}

	for (size_t m = 0; m < lit.size(); m++)
	{
		host::LitObject const &one = lit[m];

		if (!one.hasStored)
		{
			continue;
		}

		modulesWith++;

		bool differs = false;

		for (size_t v = 0; v < one.stored.size() && v < one.computed.size(); v++)
		{
			vertices++;
			exact += one.stored[v] == one.computed[v] ? 1 : 0;

			if (m < litOriginal.size() && v < litOriginal[m].stored.size() && litOriginal[m].stored[v] != one.stored[v])
			{
				differs = true;
			}
		}

		changedModules += differs ? 1 : 0;
	}

	printf("  info  copy: %zu modules with a SHPVTINT (original %zu), %zu vertices, %zu exact to the calculation, %zu modules differ from the original's values\n", modulesWith, modulesWithOriginal, vertices, exact, changedModules);

	Check(modulesWith >= modulesWithOriginal && modulesWith > 0, "the copy has a SHPVTINT for every module the original has one for");
	Check(vertices > 0 && exact * 1000 >= vertices * 995, "the values the copy holds are the calculation's, with the doors closed and the one light switched off (" + std::to_string(exact) + " of " + std::to_string(vertices) + ")");

	if (off > 0)
	{
		Check(changedModules > 0, "and they are not the original's: the switched-off light changed " + std::to_string(changedModules) + " modules");
	}

	if (checksFailed)
	{
		printf("FAILED verify_light (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed verify_light\n");
	return 0;
}
