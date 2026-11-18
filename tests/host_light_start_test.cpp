// The test of Light Sourcing's start (light_keys.h LightsAtStart; inteng's FUN_004ae5d0, AvP-docs\engine\inteng\reassess\light-closeout-frost.md TB-L7 / TB-L8): every light's changed flag (4) taken out,
// a placed light's NoSpecular moved from its flags word into its engine flags; nothing changed on a stock level. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// USE: host_light_start_test.exe <Invasion> <a folder for the copy> <a placed lamp> <another placed lamp>. With no arguments it says so and passes. The level is only read.

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

static void Put(std::vector<unsigned char> *b, size_t at, int32_t v)
{
	memcpy(b->data() + at, &v, 4);
}

static Chunk *LampChunk(OpenLevelFile &f, char const *name)
{
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			return host::FindPlacedLightChunk(f.Level(), (uint8_t const *)&each()->object_data.ID);
		}
	}

	return nullptr;
}

static std::vector<unsigned char> Bytes(OpenLevelFile &f, Chunk *c)
{
	std::vector<unsigned char> b;

	if (c)
	{
		f.PayloadOf(c, &b);
	}

	return b;
}

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		printf("host_light_start_test: no level given (<Invasion> <folder> <lamp> <another lamp>), nothing to test: passed\n");
		return 0;
	}

	size_t changed = 99;

	Check(host::LightsAtStart(nullptr, &changed) == host::LightEditResult::NothingOpen && changed == 0, "no level: nothing open, nothing counted");

	OpenLevelFile f;

	Check(f.Open(argv[1]) == LevelOpenResult::Opened && f.Level(), "the level opens");

	// a stock level: no light has the changed flag or the old NoSpecular bit, so nothing changes
	Check(host::LightsAtStart(&f, &changed) == host::LightEditResult::Done && changed == 0 && !f.HasChanges(), "the start changes nothing on a stock level: " + std::to_string(changed));

	// the flags put in as a visit before would have left them: light 1's changed flag, lamp A's changed flag, lamp B's NoSpecular in its flags word
	Chunk *const set = host::FindLightChunk(f.Level(), 0);
	Chunk *const lampA = LampChunk(f, argv[3]);
	Chunk *const lampB = LampChunk(f, argv[4]);
	std::vector<unsigned char> s = Bytes(f, set), a = Bytes(f, lampA), b = Bytes(f, lampB);

	Check(s.size() >= 84 && a.size() >= 64 && b.size() >= 64, std::string("light 1 and the lamps ") + argv[3] + " and " + argv[4] + " are there");

	if (checksFailed)
	{
		printf("FAILED host_light_start_test\n");
		return 1;
	}

	int32_t const sLocal = W(s, 72), aLocal = W(a, 24), bFlags = W(b, 56), bEngine = W(b, 20), bLocal = W(b, 24);

	Put(&s, 72, sLocal | 4);
	Put(&a, 24, aLocal | 4);
	Put(&b, 56, bFlags | 4);
	Check(f.SetPayload(set, s) && f.SetPayload(lampA, a) && f.SetPayload(lampB, b), "the flags are put in");

	Check(host::LightsAtStart(&f, &changed) == host::LightEditResult::Done && changed == 3, "the start changes three lights: " + std::to_string(changed));

	std::vector<unsigned char> const s1 = Bytes(f, set), a1 = Bytes(f, lampA), b1 = Bytes(f, lampB);

	Check(W(s1, 72) == (sLocal & ~4), "light 1's changed flag is out (0x4aedb8), its other local flags kept");
	Check(W(a1, 24) == (aLocal & ~4), std::string(argv[3]) + "'s changed flag is out (0x4aeaa8)");
	Check(W(b1, 56) == (bFlags & ~4) && W(b1, 20) == (bEngine | 0x800), std::string(argv[4]) + "'s NoSpecular is out of its flags word and in its engine flags as 0x800 (0x4aea2e)");
	Check(W(b1, 24) == (bLocal & ~4), "its local flags as they were");

	std::vector<unsigned char> sWanted(s);   // the bytes put in, with only the changed flag taken out again

	Put(&sWanted, 72, sLocal & ~4);
	Check(s1 == sWanted, "nothing else of light 1 is changed");

	Check(host::LightsAtStart(&f, &changed) == host::LightEditResult::Done && changed == 0, "a second start finds nothing more");

	std::string const copy = std::string(argv[2]) + "\\lightstart-copy.rif";

	Check(f.SaveAs(copy.c_str()) == LevelSaveResult::Saved, "a copy is saved");

	OpenLevelFile again;

	Check(again.Open(copy.c_str()) == LevelOpenResult::Opened && again.Level(), "and read by the game's loader");

	Placed_Object_Light_Chunk *const loaded = again.Level() ? dynamic_cast<Placed_Object_Light_Chunk *>(LampChunk(again, argv[4])) : nullptr;

	Check(loaded && (loaded->light.flags & 4) == 0 && (loaded->light.engine_light_flags & 0x800) != 0, std::string("where ") + argv[4] + " is no-specular by its engine flags");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED host_light_start_test\n");
		return 1;
	}

	printf("passed host_light_start_test\n");
	return 0;
}
