// The test for TYPED PLACEMENT (Dietrich 2026-11-23, Bishop's card for beta.0; the owner: "Yes, set the known type"): a new object of a shape the identity table
// knows is made as the game object its kind says, only ever one of the closed list's (src/host/shape_identity.h ShapeIdentity_PlacedAs), and a tile of the
// level's own shape is known by the file that shape makes (src/host/shape_extract.h ShapeExtract_FileSums). Written 2026-11-23 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// USE: host_shape_typed_test.exe <Invasion.RIF> (only read; nothing is saved). Stock Invasion holds smrtam01 (ammo: smartgun) and pistol (weapon: marine pistol)
// at their own size, and timdesk (furniture), measured in the identity table (shape_identity_data.cpp, 2026-11-23).
//   (1) THE CLOSED LIST over the whole built-in table: a row is typed exactly when its kind is one of the 18 pickups, each with its own Type and ExtraData
//   (2) HOSTILE WORDS: anything else, near misses and very long words too, gives false and Type 0, ExtraData 0
//   (3) THE LEVEL'S OWN SHAPES: Invasion as the next save would write it (BuildLevel), each outside shape's file worked out: smrtam01 and pistol known by it
//   (4) THE MAKER: an object of smrtam01's shape made as its kind says holds Type 3, ExtraData 2 and the maker's other fields; made with no type, Type 0
// RED FIRST: the acceptance acceptance-picker-typed (the editor before this card made every object Type 0, and had no green border); these checks are the new
// calls' own.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_make.h"
#include "shape_extract.h"
#include "shape_identity.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
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

// The AVPSTRAT payload of the placed object of that name (its OBJPRJDT's), or empty
static std::vector<unsigned char> StrategyOf(OpenLevelFile &open, std::string const &name)
{
	List<Object_Chunk *> all;
	std::vector<unsigned char> payload;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || name != object->object_data.o_name)
		{
			continue;
		}

		Chunk *const projectChunk = object->lookup_single_child("OBJPRJDT");
		Chunk_With_Children *const project = projectChunk && !projectChunk->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(projectChunk) : nullptr;
		Chunk *const strategy = project ? project->lookup_single_child("AVPSTRAT") : nullptr;

		if (strategy)
		{
			open.PayloadOf(strategy, &payload);
		}

		break;
	}

	return payload;
}

static int32_t IntIn(std::vector<unsigned char> const &bytes, size_t at)
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
	// ---- (1) THE CLOSED LIST over the whole built-in table
	struct Wanted
	{
		char const *kind;
		int32_t type, extra;
	};

	Wanted const wanted[] = {
		{ "health", 4, 0 }, { "armour", 5, 0 }, { "field charge", 15, 0 },
		{ "weapon: pulse rifle", 2, 0 }, { "weapon: smartgun", 2, 2 }, { "weapon: flamethrower", 2, 3 }, { "weapon: SADAR", 2, 5 },
		{ "weapon: grenade launcher", 2, 6 }, { "weapon: minigun", 2, 7 }, { "weapon: marine pistol", 2, 22 }, { "weapon: frisbee launcher", 2, 23 },
		{ "ammo: pulse rifle", 3, 0 }, { "ammo: smartgun", 3, 2 }, { "ammo: flamethrower", 3, 3 }, { "ammo: SADAR", 3, 5 }, { "ammo: grenade launcher", 3, 6 },
		{ "ammo: minigun", 3, 7 }, { "ammo: pulse grenade", 3, 8 },
	};

	for (Wanted const &w : wanted)
	{
		host::ShapePlacedAs as;
		bool const typed = host::ShapeIdentity_PlacedAs(w.kind, &as);

		Check(typed && as.type == w.type && as.extra == w.extra, std::string("(1) \"") + w.kind + "\" is placed as Type " + std::to_string(w.type) + ", ExtraData " +
			std::to_string(w.extra) + " (it is " + (typed ? std::to_string(as.type) + ", " + std::to_string(as.extra) : std::string("not typed")) + ")");
	}

	size_t typedRows = 0, rowsWrong = 0;
	std::string firstWrong;

	for (size_t i = 0; i < host::kShapeIdentityRowCount; i++)
	{
		host::ShapeIdentityRow const &row = host::kShapeIdentityRows[i];
		host::ShapePlacedAs as;
		bool const typed = host::ShapeIdentity_PlacedAs(row.kind, &as);
		bool listed = false;

		for (Wanted const &w : wanted)
		{
			listed = listed || strcmp(row.kind, w.kind) == 0;
		}

		typedRows += typed ? 1 : 0;

		if (typed != listed)
		{
			rowsWrong++;
			firstWrong = firstWrong.empty() ? std::string(row.fileName) + " (" + row.kind + ")" : firstWrong;
		}
	}

	Check(rowsWrong == 0 && typedRows > 0, "(1) every row of the built-in table is typed exactly when its kind is on the list: " + std::to_string((unsigned long long)typedRows) +
		" of " + std::to_string((unsigned long long)host::kShapeIdentityRowCount) + " typed" + (firstWrong.empty() ? std::string() : "; first wrong " + firstWrong));

	// ---- (2) HOSTILE WORDS
	std::string const longWords(100000, 'h');
	char const *const refused[] = { "", "static", "furniture", "unknown", "special pickup", "placed light", "video screen", "track", "type 16", "Health", "health ",
		" health", "health; armour", "weapon: plasma gun", "weapon: number 2", "ammo: file number 2", "ammo: shotgun (map tool; no gun)", "weapon:pulse rifle" };

	for (char const *words : refused)
	{
		host::ShapePlacedAs as;

		as.type = 99;
		as.extra = 99;

		bool const typed = host::ShapeIdentity_PlacedAs(words, &as);

		Check(!typed && as.type == 0 && as.extra == 0, std::string("(2) \"") + words + "\" is not typed, Type 0, ExtraData 0");
	}

	{
		host::ShapePlacedAs as;
		bool const typed = host::ShapeIdentity_PlacedAs(longWords, &as);
		std::string withZero("health");

		withZero.push_back('\0');

		host::ShapePlacedAs zeroAs;
		bool const zeroTyped = host::ShapeIdentity_PlacedAs(withZero, &zeroAs);

		Check(!typed && as.type == 0, "(2) 100,000 letters are not typed");
		Check(!zeroTyped && zeroAs.type == 0, "(2) \"health\" with a zero after it is not typed (the words are compared whole)");
	}

	// the words the status line and the picker say
	host::ShapeIdentityRow const *medpack = nullptr;

	for (size_t i = 0; i < host::kShapeIdentityRowCount && !medpack; i++)
	{
		medpack = strcmp(host::kShapeIdentityRows[i].fileName, "medbox01.rif") == 0 && strcmp(host::kShapeIdentityRows[i].kind, "health") == 0 ? &host::kShapeIdentityRows[i] : nullptr;
	}

	std::string const medpackWords = host::ShapeIdentity_PlacedAsWords(medpack);
	std::string const unknownWords = host::ShapeIdentity_PlacedAsWords(nullptr);

	Check(medpack && medpackWords == "placed as health (the game's Type 4, ExtraData 0)", "(2) a medpack's words: " + medpackWords);
	Check(unknownWords == "placed as static (Type 0): its shape is not one the editor knows", "(2) an unknown shape's words: " + unknownWords);

	if (argc < 2)
	{
		printf("  FAILED  give Invasion.RIF: (3) and (4) were not tested\n");
		return 1;
	}

	// ---- (3) THE LEVEL'S OWN SHAPES
	OpenLevelFile open;

	Check(open.Open(argv[1]) == LevelOpenResult::Opened && open.Level(), std::string("(3) the level opens: ") + argv[1]);

	if (!open.Level())
	{
		printf("%d checks, %d failed\n", checks, checksFailed);
		return 1;
	}

	std::vector<char> image;
	std::map<int32_t, std::string> sums;
	bool const walked = open.BuildLevel(image) && host::ShapeExtract_FileSums(image, &sums);
	int32_t ammoNumber = -1;
	size_t known = 0;
	bool pistol = false, desk = false;

	for (auto const &[number, sum] : sums)
	{
		host::ShapeIdentityRow const *const row = host::ShapeIdentity_Find(sum);

		known += row ? 1 : 0;
		ammoNumber = row && strcmp(row->fileName, "smrtam01.rif") == 0 && strcmp(row->kind, "ammo: smartgun") == 0 ? number : ammoNumber;
		pistol = pistol || (row && strcmp(row->fileName, "pistol.rif") == 0 && strcmp(row->kind, "weapon: marine pistol") == 0);
		desk = desk || (row && strcmp(row->fileName, "timdesk.rif") == 0);
	}

	Check(walked && !sums.empty(), "(3) the level as the next save writes it: " + std::to_string((unsigned long long)sums.size()) + " outside shapes made into their files, " +
		std::to_string((unsigned long long)known) + " of them rows of the table");
	Check(ammoNumber >= 0, "(3) the level's own smrtam01 is known by its file: ammo: smartgun (shape " + std::to_string(ammoNumber) + ")");
	Check(pistol, "(3) the level's own pistol is known by its file: weapon: marine pistol");
	Check(desk, "(3) the level's own timdesk is known by its file (furniture: made static in this card)");

	{
		std::vector<char> cut(image.begin(), image.begin() + (std::ptrdiff_t)(image.size() / 2));
		std::map<int32_t, std::string> cutSums;
		bool const cutWalked = host::ShapeExtract_FileSums(cut, &cutSums);

		Check(!cutWalked && cutSums.empty(), "(3) HOSTILE: the level's first half only (its chunks run past the end) is refused, and nothing is given");
	}

	// ---- (4) THE MAKER
	int32_t const place[3] = { 0, 0, 0 };
	int32_t const matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
	host::ShapePlacedAs as;
	bool const typed = host::ShapeIdentity_PlacedAs("ammo: smartgun", &as);
	host::ObjectStrategyType makeAs;

	makeAs.type = as.type;
	makeAs.extra = as.extra;

	uint8_t id[8] = {};
	std::string typedName, plainName;
	host::ObjectMakeResult const madeTyped = ammoNumber >= 0 && typed ? host::CreateObjectFromShapeNumber(&open, ammoNumber, "smrtam01", place, matrix, id, &typedName, &makeAs) :
		host::ObjectMakeResult::NoSuchShape;
	std::vector<unsigned char> const typedStrategy = StrategyOf(open, typedName);

	Check(madeTyped == host::ObjectMakeResult::Done && typedStrategy.size() == 52 && IntIn(typedStrategy, 8) == 62 && IntIn(typedStrategy, 20) == 3 && IntIn(typedStrategy, 24) == 2,
		"(4) made as its kind says: " + std::string(host::ObjectMakeWords(madeTyped)) + ", " + typedName + " kind " + std::to_string(IntIn(typedStrategy, 8)) + " Type " +
		std::to_string(IntIn(typedStrategy, 20)) + " ExtraData " + std::to_string(IntIn(typedStrategy, 24)) + " (" + std::to_string((unsigned long long)typedStrategy.size()) + " bytes)");
	Check(IntIn(typedStrategy, 28) == 50 && (uint32_t)IntIn(typedStrategy, 32) == 0x80000002u && IntIn(typedStrategy, 36) == 1 && IntIn(typedStrategy, 40) == 0 &&
		IntIn(typedStrategy, 44) == 0 && IntIn(typedStrategy, 48) == 0, "(4) and the maker's other fields, as every stock pickup has them: mass " +
		std::to_string(IntIn(typedStrategy, 28)) + ", integrity word " + std::to_string((uint32_t)IntIn(typedStrategy, 32)) + ", flags " + std::to_string(IntIn(typedStrategy, 36)) +
		", no target");

	host::ObjectMakeResult const madePlain = ammoNumber >= 0 ? host::CreateObjectFromShapeNumber(&open, ammoNumber, "smrtam01", place, matrix, id, &plainName) :
		host::ObjectMakeResult::NoSuchShape;
	std::vector<unsigned char> const plainStrategy = StrategyOf(open, plainName);

	Check(madePlain == host::ObjectMakeResult::Done && plainStrategy.size() == 52 && IntIn(plainStrategy, 20) == 0 && IntIn(plainStrategy, 24) == 0,
		"(4) made with no type: static, Type " + std::to_string(IntIn(plainStrategy, 20)) + " ExtraData " + std::to_string(IntIn(plainStrategy, 24)) + " (" + plainName + ")");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
