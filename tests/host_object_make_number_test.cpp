// The test for a new object made of the level's shape with a given NUMBER (src/host/object_make.h CreateObjectFromShapeNumber), as inteng's Create Duplicate and Shape Chooser
// make it. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// USE: host_object_make_number_test.exe <office.rif> (only read; nothing is saved). Office's offbits.rif gives 7 shapes, its parts told apart only by their EXTOBJNM: Aerosol 150,
// CDBox 151, Game 152, LooRoll 153, Mousemat 154, Mug 155, Speaker 156 (measured 11-23).
//   (1) by number 152 (Game): the new object is shape 152, named "offbits<n>" from the name base given
//   (2) by number 155 (Mug): shape 155
//   (3) a number no shape has: NoSuchShape, and the level is as it was (no object more, nothing imported)
//   (4) by the NAME "offbits" (CreateObjectFromShape): the FIRST outside shape whose file is offbits, 150 (Aerosol), kept exactly as inteng's own lookup by name (FUN_004de260)
// RED FIRST: on a stand-in that makes by the file's name (the bug), (1) to (3) fail: 150 both times, and Done for the missing number.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_make.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own
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

static size_t CountObjects(OpenLevelFile const &open)
{
	List<Object_Chunk *> all;
	size_t count = 0;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		count += each() ? 1 : 0;
	}

	return count;
}

// The SHPHEAD1 number of the shape the game's library gives the object of that name, or -1 (no such object, or no shape)
static int32_t ShapeNumberOf(OpenLevelFile &open, std::string const &name)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || name != object->object_data.o_name)
		{
			continue;
		}

		Shape_Chunk *const shape = object->get_assoc_shape();
		Chunk *const header = shape ? shape->lookup_single_child("SHPHEAD1") : nullptr;
		std::vector<unsigned char> bytes;
		int32_t number = -1;

		if (header && open.PayloadOf(header, &bytes) && bytes.size() >= 24)
		{
			memcpy(&number, bytes.data() + 20, sizeof(number));
		}

		return number;
	}

	return -1;
}

static bool NamedFrom(std::string const &name, std::string const &base)
{
	if (name.size() <= base.size() || name.compare(0, base.size(), base) != 0)
	{
		return false;
	}

	for (size_t i = base.size(); i < name.size(); i++)
	{
		if (name[i] < '0' || name[i] > '9')
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("  FAILED  give office.rif: nothing was tested\n");
		return 1;
	}

	OpenLevelFile open;

	Check(open.Open(argv[1]) == LevelOpenResult::Opened && open.Level(), std::string("the level opens: ") + argv[1]);

	if (!open.Level())
	{
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	int32_t place[3] = { 0, 0, 0 };
	int32_t const matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };

	// ---- (1) by number 152, the part Game
	uint8_t id[8] = {};
	std::string name;
	host::ObjectMakeResult const game = host::CreateObjectFromShapeNumber(&open, 152, "offbits", place, matrix, id, &name);
	int32_t const gameShape = ShapeNumberOf(open, name);

	Check(game == host::ObjectMakeResult::Done && gameShape == 152 && NamedFrom(name, "offbits"),
		std::string("(1) by number 152 (Game): ") + host::ObjectMakeWords(game) + ", " + name + " is shape " + std::to_string(gameShape));

	// ---- (2) by number 155, the part Mug
	place[0] = 1000;

	std::string mugName;
	host::ObjectMakeResult const mug = host::CreateObjectFromShapeNumber(&open, 155, "offbits", place, matrix, id, &mugName);
	int32_t const mugShape = ShapeNumberOf(open, mugName);

	Check(mug == host::ObjectMakeResult::Done && mugShape == 155, std::string("(2) by number 155 (Mug): ") + host::ObjectMakeWords(mug) + ", " + mugName + " is shape " +
		std::to_string(mugShape));

	// ---- (3) a number no shape has
	size_t const before = CountObjects(open);
	std::string noName;
	host::ObjectMakeResult const missing = host::CreateObjectFromShapeNumber(&open, 99999, "offbits", place, matrix, id, &noName);
	size_t const after = CountObjects(open);

	Check(missing == host::ObjectMakeResult::NoSuchShape && after == before, std::string("(3) a number no shape has: ") + host::ObjectMakeWords(missing) + ", objects " +
		std::to_string(before) + " then " + std::to_string(after));

	// ---- (4) by the NAME: inteng's own lookup, the first outside shape of the file
	place[0] = 2000;

	std::string byName;
	host::ObjectMakeResult const named = host::CreateObjectFromShape(&open, "offbits", place, matrix, id, &byName);
	int32_t const namedShape = ShapeNumberOf(open, byName);

	Check(named == host::ObjectMakeResult::Done && namedShape == 150, std::string("(4) by the name \"offbits\": the file's first part, 150 (Aerosol), as inteng: ") +
		host::ObjectMakeWords(named) + ", " + byName + " is shape " + std::to_string(namedShape));

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
