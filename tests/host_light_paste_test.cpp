// The test for Light Sourcing's paste of a PLACED light (src/host/light_paste.*: V with a placed object's light copied, inteng's FUN_004abb00): on a REAL level (the first argument, only
// read), the new placed object is made and then read back from the level the editor holds after the game's loader took it. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The rules: AvP-docs\engine\inteng\reassess\light-create-frost.md.
//
// USE: host_light_paste_test.exe <a level .rif> <a placed light's object name in it> [<an object of it with no Placed Object strategy>]. With no arguments it says so and passes.
// CHECKS: the first free name is Light<n> (capitals not mattering); a paste makes an object of the name given with the copied object's shape, the placed bit and the copied object's load bits
// (INTENG BUG, FIXED), the place given, a kind 62 strategy (Type / ExtraData / mass copied, integrity with bit 31, flags 1, the rest 0) and a PLOBJLIT with the copied 15 numbers and no extras;
// the copied object is as it was; the next free name is the next number; a name an object has is refused; an ID no object has, and an object with no Placed Object strategy, make nothing.

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
#include "light_paste.h"

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

static Object_Chunk *ObjectNamed(OpenLevelFile &level, char const *name)
{
	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			return each();
		}
	}

	return nullptr;
}

static std::vector<unsigned char> PayloadOf(OpenLevelFile &level, Chunk *chunk)
{
	std::vector<unsigned char> bytes;

	if (chunk)
	{
		level.PayloadOf(chunk, &bytes);
	}

	return bytes;
}

static std::vector<unsigned char> StrategyOf(OpenLevelFile &level, Object_Chunk *object)
{
	Chunk_With_Children *const project = object ? (Chunk_With_Children *)object->lookup_single_child("OBJPRJDT") : nullptr;

	return PayloadOf(level, project ? project->lookup_single_child("AVPSTRAT") : nullptr);
}

static int32_t Word(std::vector<unsigned char> const &bytes, size_t at)
{
	int32_t value = 0;

	if (at + 4 <= bytes.size())
	{
		memcpy(&value, bytes.data() + at, 4);
	}

	return value;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_light_paste_test <a level .rif> <a placed light's object> [<an object with no Placed Object strategy>]: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	OpenLevelFile open;

	if (open.Open(argv[1]) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	Object_Chunk *source = ObjectNamed(open, argv[2]);

	if (!source || !source->get_header())
	{
		Check(false, std::string("the level has the object \"") + argv[2] + "\"");
		return 1;
	}

	uint8_t sourceId[8];
	memcpy(sourceId, &source->object_data.ID, 8);

	std::vector<unsigned char> const strategyWas = StrategyOf(open, source);
	std::vector<unsigned char> const lightWas = PayloadOf(open, source->lookup_single_child("PLOBJLIT"));
	std::vector<unsigned char> const headWas = PayloadOf(open, source->get_header());
	uint32_t const flagsWas = (uint32_t)source->get_header()->flags;

	Check(strategyWas.size() >= 52 && Word(strategyWas, 8) == 62 && lightWas.size() >= 60 && headWas.size() >= 60, "the copied object is a placed light: a Placed Object strategy and a PLOBJLIT");

	std::string const first = host::LightPasteFirstName(&open);

	Check(!first.empty() && first.compare(0, 5, "Light") == 0, "the first free name is Light<n>: " + first);

	int32_t const place[3] = { 5000, -300, 9000 };
	int32_t const identity[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
	uint8_t newId[8] = {};
	std::string why;

	Check(host::PastePlacedLight(&open, sourceId, first, place, identity, newId, &why) == host::LightPasteResult::Done, "the paste makes the new object" + (why.empty() ? std::string() : ": " + why));

	// the level was read again by the game's loader: the old pointers are gone
	source = ObjectNamed(open, argv[2]);
	Object_Chunk *const made = ObjectNamed(open, first.c_str());

	Check(source && made && made->get_header(), "the level holds the new object " + first + " and the copied one");

	if (source && made && made->get_header())
	{
		std::vector<unsigned char> const madeHead = PayloadOf(open, made->get_header());
		uint32_t const wanted = 0x400u | (flagsWas & host::kPastedLightLoadBits);

		Check(memcmp(&made->object_data.ID, newId, 8) == 0, "the ID given back is the new object's");
		Check(Word(madeHead, 56) == Word(headWas, 56), "it uses the copied object's shape (its number)");
		Check((uint32_t)made->get_header()->flags == wanted && (wanted & 0x8000) != 0, "its header: the placed bit and the copied object's load bits, PC load among them (INTENG BUG, FIXED)");
		Check(made->object_data.location.x == 5000 && made->object_data.location.y == -300 && made->object_data.location.z == 9000, "it is at the place given");

		std::vector<unsigned char> const strategyNow = StrategyOf(open, made);
		bool restZero = true;

		for (size_t at : { 0, 4, 12, 16, 40, 44, 48 })
		{
			restZero = restZero && Word(strategyNow, at) == 0;
		}

		Check(strategyNow.size() == 52 && Word(strategyNow, 8) == 62 && Word(strategyNow, 20) == Word(strategyWas, 20) && Word(strategyNow, 24) == Word(strategyWas, 24) &&
			Word(strategyNow, 28) == Word(strategyWas, 28) && (uint32_t)Word(strategyNow, 32) == ((uint32_t)Word(strategyWas, 32) | 0x80000000u) && Word(strategyNow, 36) == 1 && restZero,
			"its strategy: kind 62, Type / ExtraData / mass copied, integrity with bit 31, flags 1, the rest 0");

		std::vector<unsigned char> const lightNow = PayloadOf(open, made->lookup_single_child("PLOBJLIT"));

		Check(lightNow.size() == 64 && memcmp(lightNow.data(), lightWas.data(), 24) == 0 && memcmp(lightNow.data() + 28, lightWas.data() + 28, 32) == 0 && Word(lightNow, 24) == (Word(lightWas, 24) | 4) &&
			Word(lightNow, 60) == 0, "its PLOBJLIT: the copied 15 numbers, the local flags with 4 (changed) in, no extra words");
		Check(made->lookup_single_child("OBINTDT") != nullptr, "it has the notes every new object has (FUN_004d0e80 -> FUN_004d2450)");
		Check(StrategyOf(open, source) == strategyWas && PayloadOf(open, source->lookup_single_child("PLOBJLIT")) == lightWas, "the copied object is as it was");
	}

	std::string const next = host::LightPasteFirstName(&open);

	Check(next != first && next.compare(0, 5, "Light") == 0, "the next free name is another number: " + next);
	host::LightPasteResult const taken = host::PastePlacedLight(&open, sourceId, first, place, identity, newId, &why);

	Check(taken == host::LightPasteResult::MakeFailed, "a name an object has makes nothing: " + why);

	uint8_t const nobody[8] = { 1, 2, 3, 4, 5, 6, 7, 0 };

	Check(host::PastePlacedLight(&open, nobody, next, place, identity, newId, &why) == host::LightPasteResult::NoSuchObject, "an ID no object has makes nothing");

	if (argc >= 4)
	{
		Object_Chunk *const other = ObjectNamed(open, argv[3]);
		uint8_t otherId[8] = {};

		if (other)
		{
			memcpy(otherId, &other->object_data.ID, 8);
		}

		host::LightPasteResult const refused = host::PastePlacedLight(&open, otherId, next, place, identity, newId, &why);

		Check(other && (refused == host::LightPasteResult::NotPlacedObject || refused == host::LightPasteResult::NoPlacedLight), std::string("an object with no Placed Object strategy (") + argv[3] + ") makes nothing");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
