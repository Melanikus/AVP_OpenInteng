// verify_placedcolour: the check of a scenario that pressed T on one placed light and then changed the red of it and of another placed light (Light Sourcing; inteng's T is the SELECTED light's own [5],
// FUN_004acf30 0x4ad4bf; light-closeout-frost.md TB-L5), read back through the game's own loader. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USE: verify_placedcolour.exe <opened level> <copy> <the lamp T was pressed on> <the other lamp>
// CHECKS: the first lamp's DOWN colour has its red larger, its up colour as it was; the other lamp's UP colour has its red larger, its down colour as it was (its own [5] was still 0).
// Gives back 0 only if every check passed.

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

static int32_t W(std::vector<unsigned char> const &b, size_t at)
{
	int32_t v = 0;

	if (at + 4 <= b.size())
	{
		memcpy(&v, b.data() + at, 4);
	}

	return v;
}

static std::vector<unsigned char> Lamp(OpenLevelFile &f, char const *name)
{
	std::vector<unsigned char> b;
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			Chunk *const c = host::FindPlacedLightChunk(f.Level(), (uint8_t const *)&each()->object_data.ID);

			if (c)
			{
				f.PayloadOf(c, &b);
			}
		}
	}

	return b;
}

// the red larger (a press is one step a frame while the key is down: one or a few), the green and blue the same
static bool RedUp(int32_t before, int32_t after)
{
	return ((after >> 16) & 0xff) > ((before >> 16) & 0xff) && (after & 0xffff) == (before & 0xffff);
}

static std::string Hex(int32_t v)
{
	char text[16];

	snprintf(text, sizeof(text), "0x%06x", (unsigned)v);
	return text;
}

int main(int argc, char **argv)
{
	if (argc != 5)
	{
		printf("verify_placedcolour: <opened level> <copy> <the lamp T was pressed on> <the other lamp>\n");
		return 2;
	}

	OpenLevelFile opened, copy;

	Check(opened.Open(argv[1]) == LevelOpenResult::Opened && opened.Level() != nullptr, std::string("the game's loader reads the opened level: ") + argv[1]);
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[2]);

	if (checksFailed)
	{
		printf("FAILED verify_placedcolour\n");
		return 1;
	}

	std::vector<unsigned char> const t0 = Lamp(opened, argv[3]), t1 = Lamp(copy, argv[3]), o0 = Lamp(opened, argv[4]), o1 = Lamp(copy, argv[4]);

	Check(t0.size() >= 64 && t1.size() == t0.size() && o0.size() >= 64 && o1.size() == o0.size(), "both lamps are in both levels");

	if (checksFailed)
	{
		printf("FAILED verify_placedcolour\n");
		return 1;
	}

	// PLOBJLIT: up colour at 12, down colour at 16
	Check(((W(t0, 16) >> 16) & 0xff) < 255 && ((W(o0, 12) >> 16) & 0xff) < 255, "the reds to change are below 255 (so a change shows)");
	Check(RedUp(W(t0, 16), W(t1, 16)), std::string(argv[3]) + " (T pressed): its DOWN colour's red larger: " + Hex(W(t0, 16)) + " -> " + Hex(W(t1, 16)));
	Check(W(t1, 12) == W(t0, 12), std::string("and its up colour as it was: ") + Hex(W(t1, 12)));
	Check(RedUp(W(o0, 12), W(o1, 12)), std::string(argv[4]) + " (its own T not pressed): its UP colour's red larger: " + Hex(W(o0, 12)) + " -> " + Hex(W(o1, 12)));
	Check(W(o1, 16) == W(o0, 16), std::string("and its down colour as it was: ") + Hex(W(o1, 16)));

	if (checksFailed)
	{
		printf("FAILED verify_placedcolour (%d)\n", checksFailed);
		return 1;
	}

	printf("PASSED verify_placedcolour\n");
	return 0;
}
