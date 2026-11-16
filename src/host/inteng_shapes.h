// inteng_shapes.h: inteng's OWN BUILT-IN SHAPES (the editor objects' icons), read at run time from the user's installed inteng.exe (inteng_exe.h). Written 2026-11-16 by Melanikus (Vasquez)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (reassess/object-placement.md section 8b V-25): every editor object (generator, bad guy, sound, particle generator, player start, placed hierarchy, virtual object) has a
// display block made by FUN_004e4ae0 / FUN_004b2590 with one of inteng's own compiled-in shapes, which FUN_0042c700 puts first in the shape list at every load (19 of them, DAT_00931140 +
// 4 x slot): slots 0..9 through the pointers at 0x5cd958, 0x5cd95c, 0x5cd960, 0x5cd964, 0x5cd97c, 0x5cd980, 0x5cd96c, 0x5cd970, 0x5cd974, 0x5cd984 (0x42c711-0x42c75f), slots 10..18 the
// headers at 0x5df230, 0x5de270, 0x5d9b90, 0x5e0730, 0x5d84e0, 0x5dfcb0, 0x5dcce0, 0x5d31a0, 0x5e48c8 (0x42c770-0x42c7c0). Each is a 3dc SHAPEHEADER (avpx 3dc shape.h): numpoints (+0),
// numitems (+4), shapeflags, points (+12: a pointer to the pointer to numpoints x 3 ints), items (+16: a pointer to numitems pointers to polygons). A polygon: its type, normal, flags, its flat
// COLOUR (+12, 0xRRGGBB), then its corners (+16..) as offsets into the point list (3 ints a point) ending with -1. inteng's numbers are 100 to a world millimetre.
//
// THE RULING (Melanikus via Bishop 11-16): inteng's binary data is never copied into our source: these shapes are read from his own inteng.exe when the editor runs (inteng_exe.h), and
// replace the copy that was in editor_icon_shapes.inc. Missing or refused: the editor draws its own plain boxes (editor_icons.h) and says why.
//
// THE BYTES ARE HOSTILE INPUT: every count, pointer and corner is checked before it is used (IntengShapes_Read); a read outside the exe's file bytes is refused (IntengExe_ReadBytes).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "inteng_exe.h"

namespace host
{
	int const kIntengShapeSlots = 19;           // FUN_0042c700 returns 0x13
	int32_t const kIntengShapeMostPoints = 1024;
	int32_t const kIntengShapeMostItems = 1024;
	int const kIntengShapeMostCorners = 16;     // a polygon's corners before its -1

	// One flat triangle of a shape, in inteng's units (100 to a millimetre), and its colour 0xRRGGBB
	struct IntengShapeTriangle
	{
		int32_t p[9];
		uint32_t colour;
	};

	struct IntengShapes
	{
		bool have[kIntengShapeSlots] = {};
		std::vector<IntengShapeTriangle> triangles[kIntengShapeSlots];   // every polygon fanned from its first corner
	};

	enum class IntengShapesRefusal
	{
		None,
		BadSlot,           // a wanted slot outside 0..18
		BadAddress,        // a header, pointer, point list or polygon that is not inside the exe's file bytes
		BadCounts,         // a shape with 0 or too many points or polygons
		BadCorner,         // a polygon with fewer than 3 corners, more than kIntengShapeMostCorners, a corner that is not a point of the shape, or no -1 at its end
	};

	// In plain words, for the status line and the log: static text, never null
	char const *IntengShapes_Why(IntengShapesRefusal why);

	// The VA of slot's SHAPEHEADER as FUN_0042c700 finds it (slots 0..9 read through their pointer word); false for a bad slot or an unreadable pointer
	bool IntengShapes_HeaderOf(IntengExeImage const &image, int slot, uint32_t *header);

	// Reads the shapes of the slots in `wanted` from the exe. All or nothing: false, *why set and *out emptied, when any wanted shape is refused.
	bool IntengShapes_Read(IntengExeImage const &image, int const *wanted, size_t wantedCount, IntengShapes *out, IntengShapesRefusal *why);

	// INTENG'S SHAPE-NAME TABLE (0x5e7b58; Bishop's ruling V-31 11-16: a table in the exe's data is read at run time, never copied): rows of 12 bytes, a pointer to a lower-case shape name, the
	// object type, its extra data; the row whose name pointer is 0 ends it (FUN_004f6a20 0x4f6a20-0x4f6a81 walks them with a strcmp). FUN_004e44c0 0x4e4738-0x4e4755 looks the lower-cased name of a
	// placed shape's outside file up in it (simple_object_edit.h). HOSTILE as the rest: at most kIntengShapeNamesMost rows, each name 1..kIntengShapeNameMost printable letters ending with its 0
	// inside the exe's bytes, the type 0..31 and the extra data 0..65535.
	uint32_t const kIntengShapeNamesAt = 0x5e7b58;
	size_t const kIntengShapeNamesMost = 64;
	size_t const kIntengShapeNameMost = 31;

	struct IntengShapeName
	{
		std::string name;
		int32_t type = 0;
		int32_t extra = 0;
	};

	enum class IntengNamesRefusal
	{
		None,
		BadAddress,   // a row or a name outside the exe's file bytes
		NoEnd,        // more than kIntengShapeNamesMost rows before the 0 that ends the table
		BadName,      // a name that is empty, longer than kIntengShapeNameMost, or holds a letter outside ! to ~
		BadNumber,    // a type outside 0..31 or an extra data outside 0..65535
	};

	char const *IntengNames_Why(IntengNamesRefusal why);

	// All the rows, or none (false, *why set, *out emptied)
	bool IntengShapeNames_Read(IntengExeImage const &image, std::vector<IntengShapeName> *out, IntengNamesRefusal *why);
}
