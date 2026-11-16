// verify_placedlight: the check of the placed light scenario (tests/scenarios/light-placed.txt): the level the hidden editor run saved as a copy, read with the game's own loader. Written 2026-11-15 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_placedlight.exe <the level the editor opened> <the copy it saved> <the object (its number) that has the placed light>
// CHECKS: both levels are read by the game's loader; the object's PLOBJLIT in the copy has a larger brightness (the + key held), the same UP colour, a DOWN colour whose red is larger (T, then the colour attribute Red and the
// + key), its local flags with the changed bit (4) in, and every other byte of the chunk as it was; no other object's PLOBJLIT changed. Gives back 0 only if every check passed.

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

static bool Placed(OpenLevelFile *open, int32_t number, std::vector<unsigned char> *payload, std::vector<std::vector<unsigned char> > *others, std::vector<int32_t> *otherNumbers)
{
	::List<Object_Chunk *> objects;
	bool found = false;

	open->Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		Chunk *const placed = host::FindPlacedLightChunk(open->Level(), (uint8_t const *)&object->object_data.ID);
		std::vector<unsigned char> bytes;

		if (!placed || !open->PayloadOf(placed, &bytes))
		{
			continue;
		}

		if ((int32_t)object->object_data.index_num == number)
		{
			*payload = bytes;
			found = true;
		}
		else
		{
			others->push_back(bytes);
			otherNumbers->push_back((int32_t)object->object_data.index_num);
		}
	}

	return found;
}

static int32_t At(std::vector<unsigned char> const &payload, size_t at)
{
	int32_t value = 0;

	memcpy(&value, payload.data() + at, 4);
	return value;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 4)
	{
		printf("usage: verify_placedlight <the level the editor opened> <the copy it saved> <the object's number>\n");
		return 2;
	}

	OpenLevelFile before, after;
	int32_t const number = atoi(argv[3]);

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level the editor opened");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the editor's copy");

	if (!before.Level() || !after.Level())
	{
		return 1;
	}

	std::vector<unsigned char> was, now;
	std::vector<std::vector<unsigned char> > wasOthers, nowOthers;
	std::vector<int32_t> wasNumbers, nowNumbers;

	Check(Placed(&before, number, &was, &wasOthers, &wasNumbers) && Placed(&after, number, &now, &nowOthers, &nowNumbers) && was.size() >= 60 && now.size() == was.size(), "the object has a placed light in both");

	if (was.size() < 60 || now.size() != was.size())
	{
		return 1;
	}

	Check(At(now, 0) > At(was, 0), "its brightness is larger: " + std::to_string(At(was, 0)) + " to " + std::to_string(At(now, 0)));
	Check(At(now, 12) == At(was, 12), "its UP colour is as it was");
	Check(((At(now, 16) >> 16) & 0xff) > ((At(was, 16) >> 16) & 0xff) || ((At(was, 16) >> 16) & 0xff) == 255, "its DOWN colour's red is larger: " + std::to_string((At(was, 16) >> 16) & 0xff) + " to " + std::to_string((At(now, 16) >> 16) & 0xff));
	Check((At(now, 24) & 4) != 0, "its local flags have the changed bit");
	Check(At(now, 28) == 5000 && At(was, 28) != 5000, "its Fade up time, typed in the Light properties panel, is 5000: " + std::to_string(At(was, 28)) + " to " + std::to_string(At(now, 28)));

	bool rest = true;

	for (size_t i = 0; i < was.size(); i++)
	{
		bool const edited = i < 4 || (i >= 16 && i < 20) || (i >= 24 && i < 32);

		rest = rest && (edited || was[i] == now[i]);
	}

	Check(rest, "every other byte of the chunk is as it was");
	Check(wasOthers == nowOthers && wasNumbers == nowNumbers, "no other object's placed light changed");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
