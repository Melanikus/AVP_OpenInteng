// shape_chooser.h: the list the Shape Chooser screen (inteng's FUN_004dd910 enter, FUN_004ddb60 per frame; Apone's facts in AvP-docs\engine\inteng\apone-reads-3.md,
// "The Shape Chooser") offers: every shape the level holds from an outside file, with its name, number and counts. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES. At the start of Object Placement (FUN_004d4750, dump-all-2.txt L73870-73960) it makes two lists from the loaded shapes of the numbers
// DAT_00636f50 to DAT_00636f70: the shape's number, and its name = the first SHPEXTFN inside the shape's first SHPEXTFL (+0x28 of the chunk, the outside file's name),
// with the extension cut at the LAST '.' and then everything up to the last '\' (0x4d4823-0x4d4855). The chooser shows one shape at a time ("Shape '%s'", "Polys %d, Verts
// %d" from the shape record: polygons [1], points [0]), Previous and Next move through the list (wrapping), a list dialog ("Select shape") picks by position, Return
// leaves WITH the shape (DAT_008f4f34 = its number, then the placement screen runs FUN_004e2e10 on the selected object: host::ShapeReplace_ByNumber) and Escape
// leaves without (DAT_008f4f34 = 0).
//
// WHAT THIS GIVES. The same list from the level: the shapes whose header has SHAPE_FLAG_EXTERNALFILE (0x1000), in the library's shape order, with the SHPEXTFN's name
// cut as above (a name no longer than 63 bytes; a hostile name is cut, never trusted to be one). NAMED: inteng's range DAT_00636f50..70 is not read, so every outside
// shape is listed; the "Show" items (All Shapes / Sprites / Dynamic Objects, 0x32 / 0x34 / 0x36) do nothing in inteng: the screen's only handler (FUN_004ddb60, dump-all-3.txt:5391-5546) compares the menu's command with 10 and 12 and nothing else, and no other code reads the table 0x5e78c8 (UNFINISHED IN INTENG, tagged so).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class File_Chunk;      // the game's chunk library
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	struct ShapeChoice
	{
		int32_t number;     // the shape's own number (SHPHEAD1: the number ShapeReplace_ByNumber takes)
		std::string name;   // the outside file's name, without folder or extension
		int32_t polygons;   // SHPHEAD1's polygon count
		int32_t points;     // SHPHEAD1's point count
	};

	// Every outside shape of the level, in the library's order, into *out (emptied first). Gives how many. 0 for no level or none. Changes nothing. Hostile bytes: a header
	// shorter than its fixed part, or an SHPEXTFN with no name, leaves the shape out.
	size_t ShapeChooser_List(File_Chunk *level, OpenLevelFile const *store, std::vector<ShapeChoice> *out);

	// Whether the object whose 8-byte ID is id is a PLACED object (header flag 0x400): inteng shows the chooser only for such an object (FUN_004d9af0 at 0x4d94e0)
	bool ShapeChooser_ObjectIsPlaced(File_Chunk *level, OpenLevelFile const *store, uint8_t const *id);

	// The shape number of the object's own shape (0 and false when it has none the list knows), so the chooser can start on it (inteng starts on the list's first)
	bool ShapeChooser_ShapeOfObject(File_Chunk *level, OpenLevelFile const *store, uint8_t const *id, int32_t *number);

	// Whether any object of the level already has exactly this name (a byte-wise compare, as inteng's Create does before it makes an object: FUN_004d9af0 0x4d9450-0x4d97be)
	bool ObjectNameInUse(File_Chunk *level, char const *name);
}
