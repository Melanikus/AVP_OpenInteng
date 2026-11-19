// The test for tests/scenarios/imported_shape.h, the lister's (verify_list_objects) "imported" per object. Written 2026-11-19 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT TESTS, on the stock levels (read only, through the game's own loader):
// - Invasion's chair1, the subject that held a scenario up (Vasquez 11-18: S + click refused it), is imported with its own pictures; Invasion has 174 imported objects.
// - No module is imported (an imported shape is placed objects only: level_chunks.cpp).
// - On every stock level, for every object: "own pictures" is exactly what the editor's own S + click guard says (PolygonItems_ShapeHasOwnPictureList), and imported (with or
//   without its own pictures) is exactly what the shape's header says (SHAPE_FLAG_EXTERNALFILE, the flag the game's loader and the shape chooser read).
// - No object of a stock level has no shape; nothing (a null object) is "no shape"; each answer has its own word.
// HOW IT RUNS: built with tests/imported_shape_test.args; tests/imported_shape_test.runargs gives Invasion first, then every stock level. Non-zero when a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "polygon_items.h"
#include "scenarios/imported_shape.h"

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

static bool IsImported(ImportedShape imported)
{
	return imported == ImportedShape::Yes || imported == ImportedShape::YesOwnPictures;
}

// Invasion: the subject that held a scenario up, the count, and the modules
static void Invasion(char const *path)
{
	OpenLevelFile open;

	if (open.Open(path) != LevelOpenResult::Opened || !open.Level())
	{
		Check(false, std::string("the game's loader reads Invasion: ") + path);
		return;
	}

	List<Object_Chunk *> all;
	open.Level()->list_objects(&all);
	size_t imported = 0, modulesImported = 0, modules = 0;
	bool chairSeen = false;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		ImportedShape const answer = ImportedShapeOf(object);
		Object_Header_Chunk *const header = object->get_header();
		bool const placed = header && (header->flags & OBJECT_FLAG_PLACED_OBJECT);

		imported += IsImported(answer) ? 1 : 0;

		if (!placed)
		{
			modules++;
			modulesImported += IsImported(answer) ? 1 : 0;
		}

		if (strcmp(object->object_data.o_name, "chair1") == 0)
		{
			chairSeen = true;
			Check(answer == ImportedShape::YesOwnPictures, "Invasion's chair1 is imported with its own pictures (the S + click guard refuses it)");
			Check(strcmp(ImportedShapeWords(answer), "yes, own pictures") == 0, "and the lister says \"yes, own pictures\" for it");
		}
	}

	Check(chairSeen, "Invasion has a chair1");
	Check(imported == 174, "Invasion has 174 imported objects (found " + std::to_string(imported) + ")");
	Check(modules > 0 && modulesImported == 0, "none of Invasion's " + std::to_string(modules) + " modules is imported");
}

// Every stock level: the answer against the editor's own guard and the shape's header
static void EveryLevel(int count, char **paths)
{
	size_t levels = 0, objects = 0, imported = 0, againstGuard = 0, againstFlag = 0, noShape = 0;
	std::string firstBad;

	for (int i = 0; i < count; i++)
	{
		OpenLevelFile open;

		if (open.Open(paths[i]) != LevelOpenResult::Opened || !open.Level())
		{
			Check(false, std::string("the game's loader reads ") + paths[i]);
			continue;
		}

		levels++;
		List<Object_Chunk *> all;
		open.Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const object = each();
			ImportedShape const answer = ImportedShapeOf(object);
			Shape_Chunk *const shape = object->get_assoc_shape();
			Shape_Header_Chunk *const shapeHeader = shape ? shape->get_header() : nullptr;
			bool const flagged = shapeHeader && (shapeHeader->flags & SHAPE_FLAG_EXTERNALFILE);
			bool const guard = host::PolygonItems_ShapeHasOwnPictureList(open.Level(), (uint8_t const *)&object->object_data.ID);
			bool const ownPictures = answer == ImportedShape::YesOwnPictures;

			objects++;
			imported += IsImported(answer) ? 1 : 0;
			noShape += answer == ImportedShape::NoShape ? 1 : 0;

			if (ownPictures != guard || IsImported(answer) != flagged)
			{
				againstGuard += ownPictures != guard ? 1 : 0;
				againstFlag += IsImported(answer) != flagged ? 1 : 0;

				if (firstBad.empty())
				{
					firstBad = std::string(paths[i]) + " " + object->object_data.o_name;
				}
			}
		}
	}

	Check(levels == (size_t)count && levels >= 60, "every stock level is read (" + std::to_string(levels) + " of " + std::to_string(count) + ")");
	Check(againstGuard == 0, "own pictures is the S + click guard's answer for every one of " + std::to_string(objects) + " objects" +
		(againstGuard ? " (first " + firstBad + ")" : ""));
	Check(againstFlag == 0, "imported is the shape header's SHAPE_FLAG_EXTERNALFILE for every object" + (againstFlag ? " (first " + firstBad + ")" : std::string("")));
	Check(imported > 0 && imported < objects, std::to_string(imported) + " of them imported, the rest not");
	Check(noShape == 0, "every object of a stock level has a shape");
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("imported_shape_test: give Invasion's .rif, then every stock level\n");
		printf("0 checks, 1 failed\n");
		return 1;
	}

	Check(ImportedShapeOf(nullptr) == ImportedShape::NoShape, "no object is \"no shape\"");
	Check(strcmp(ImportedShapeWords(ImportedShape::No), "no") == 0 && strcmp(ImportedShapeWords(ImportedShape::Yes), "yes") == 0 &&
		strcmp(ImportedShapeWords(ImportedShape::YesOwnPictures), "yes, own pictures") == 0 && strcmp(ImportedShapeWords(ImportedShape::NoShape), "no shape") == 0,
		"each answer has its own word");

	Invasion(argv[1]);
	EveryLevel(argc - 2, argv + 2);

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
