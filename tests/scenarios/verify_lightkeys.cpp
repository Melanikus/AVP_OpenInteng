// verify_lightkeys: the check of the Light Sourcing keys scenario (tests/scenarios/light-keys.txt): the level the hidden editor run saved as a copy, read with the game's own loader. Written 2026-11-15 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_lightkeys.exe <the level the editor opened> <the copy it saved> <the light (from 0) that was copied, changed and pasted> <how many lights were made>
// CHECKS: both levels are read by the game's loader; the copy has that many lights more; every light but the chosen one is as it was; the chosen light's spread is larger (the + key held on Spread), its range, brightness,
// colour and place are as they were and its changed flag (4) is in; the last light made is a PASTE of the chosen light as it was (brightness, spread, range, colour, engine flags) at the place of the one before it (both
// were made at the camera). Gives back 0 only if every check passed.

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

	if (argc < 5)
	{
		printf("usage: verify_lightkeys <the level the editor opened> <the copy it saved> <the light> <how many lights were made>\n");
		return 2;
	}

	OpenLevelFile before, after;
	// the light and the count are bounded before they are used (arg_number.h)
	long targetNumber = 0, madeNumber = 0;

	if (!ArgNumber("the light", argv[3], 0, 100000, &targetNumber) || !ArgNumber("how many lights were made", argv[4], 0, 100000, &madeNumber))
	{
		return 2;
	}

	size_t const target = (size_t)targetNumber;
	size_t const made = (size_t)madeNumber;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the editor's copy");

	if (!before.Level() || !after.Level())
	{
		return 1;
	}

	size_t was = 0, now = 0;

	Check(host::CountLights(&before, &was) == host::LightEditResult::Done && host::CountLights(&after, &now) == host::LightEditResult::Done, "the lights are counted");
	Check(now == was + made, "the copy has " + std::to_string(made) + " lights more: " + std::to_string(was) + " to " + std::to_string(now));

	bool others = true;

	for (size_t i = 0; i < was; i++)
	{
		if (i != target && Light(&before, i) != Light(&after, i))
		{
			others = false;
		}
	}

	Check(others, "every light but the chosen one is as it was");

	host::LightData const was1 = Light(&before, target);
	host::LightData const now1 = Light(&after, target);

	Check(!was1.empty() && !now1.empty(), "the chosen light is in both");
	Check(At(now1, 56) > At(was1, 56), "its spread is larger: " + std::to_string(At(was1, 56)) + " to " + std::to_string(At(now1, 56)));
	Check(At(now1, 60) == At(was1, 60) && At(now1, 52) == At(was1, 52) && At(now1, 64) == At(was1, 64) && At(now1, 0) == At(was1, 0) && At(now1, 4) == At(was1, 4) && At(now1, 8) == At(was1, 8) && At(now1, 12) == At(was1, 12),
		"its range, brightness, colour, number and place are as they were");
	Check((At(now1, 72) & 4) != 0, "its changed flag (4) is in");

	if (made >= 2 && now >= 2)
	{
		host::LightData const pasted = Light(&after, now - 1);
		host::LightData const dropped = Light(&after, now - 2);

		Check(At(pasted, 52) == At(was1, 52) && At(pasted, 56) == At(was1, 56) && At(pasted, 60) == At(was1, 60) && At(pasted, 64) == At(was1, 64) && At(pasted, 68) == At(was1, 68),
			"the last light made is a paste of the chosen light as it was copied");

		if (argc >= 11)
		{
			int32_t const drop[3] = { atoi(argv[5]), atoi(argv[6]), atoi(argv[7]) };
			int32_t const fly[3] = { atoi(argv[8]), atoi(argv[9]), atoi(argv[10]) };

			printf("  info  the dropped light is at %d %d %d (the camera was at %d %d %d), the pasted one at %d %d %d (flown to %d %d %d)\n", (int)At(dropped, 4), (int)At(dropped, 8), (int)At(dropped, 12), (int)drop[0], (int)drop[1], (int)drop[2],
				(int)At(pasted, 4), (int)At(pasted, 8), (int)At(pasted, 12), (int)fly[0], (int)fly[1], (int)fly[2]);
			Check(At(dropped, 52) == 0x10000 && At(dropped, 56) == 1000 && At(dropped, 60) == 4000000 && At(dropped, 64) == 0xffffff && At(dropped, 68) == 3 && At(dropped, 72) == 5, "the dropped light has the headlight's numbers (100%, spread 1000, range 4000000, white, engine flags 3, local flags 5)");
			Check(At(dropped, 4) == drop[0] && At(dropped, 8) == drop[1] && At(dropped, 12) == drop[2], "the dropped light is where the camera was (the place in the level's own numbers)");
			Check(At(pasted, 4) == fly[0] && At(pasted, 8) == fly[1] && At(pasted, 12) == fly[2], "the pasted light was flown to where the camera was put (L, the camera moved, L again)");
		}
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
