// poly_counts.h: Module Linking's polygon counts (Base > Show Poly Counts 0xcc, FUN_004c0950; Show All 0xca's colours), as plain functions over the open level. Written
// 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (Vasquez's read, AvP-docs\engine\inteng\module-linking-reads.md section 1; FUN_004d5e60, FUN_004c93f0, FUN_004c0950): the polygon count of a module is the
// polygons drawn when the player stands in it: its own shape's plus the shape of every distinct other module its VMDARRAY lists.
//   count(shape) with no SHPMRGDT = the shape's polygon count; with SHPMRGDT = for each polygon i with merge_data[i] < i (-1, or the second half of a merged pair) 1, or 2 when
//   the polygon's flag word has bit 31 (0x80000000).
//   Its seen modules: the module's FIRST MODULEDT's FIRST VMDARRAY; each item's object_index is matched with the FIRST object, among those not yet used, whose index_num is
//   that number; the module itself is used first (so a link to itself adds nothing); an item with no match adds nothing; an object found twice counts once.
//   "Modules seen" (FUN_004c93f0) = 1 (the module) + the distinct modules found that way.
//   No MODULEDT, or no VMDARRAY: the module's own count, seen 1.
// The list inteng shows is in the order of the loaded objects (here: the level's object list order), one row per object that has a shape, "%d\t%d m\t%s": count, seen, name.
//
// NAMED: inteng skips a loaded-object record whose +0x60 bit 0 is set (unread: not known to be set in a level's shapes); here every object with a shape chunk is listed. An object
// with no shape chunk has no row (inteng's loaded-object table has none for it).
// SHOW ALL's colours (0xca): below 1200 green 0x00FF00, 1200..1499 blue 0x0000FF, 1500..1999 orange 0xFF7F00, 2000 and up yellow 0xFFFF00, white 0xFFFFFF with no MODULEDT or no
// VMDARRAY data (PolyCountColour).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class File_Chunk;

namespace host
{
	struct PolyCountRow
	{
		uint8_t id[8];       // the object's ID (selection.h)
		std::string name;
		int32_t number;      // its object number (index_num)
		int32_t count;       // the polygons (own + seen)
		int32_t seen;        // the modules seen (1 + the distinct linked ones found)
		bool placed;         // a placed object (header flag 0x400), not a module
		bool linked;         // it has a MODULEDT with VMDARRAY data (Show All: white when not)
		int32_t place[3];    // the object's place, in file units (the view goes there)
	};

	// The most rows (a hostile level cannot make the list unbounded)
	size_t const kPolyCountRowsMost = 100000;

	// Every object with a shape, in the level's order. Gives the number of rows (0 with no level).
	size_t PolyCounts_Read(File_Chunk *level, std::vector<PolyCountRow> *rows);

	// Show All's colour (0xRRGGBB) for a row
	uint32_t PolyCountColour(PolyCountRow const &row);

	// inteng's row text: "%d\t%d m\t%s"
	std::string PolyCountText(PolyCountRow const &row);

	// The hub's "Polygon count : %d / %d" (FUN_00472720, run at every start of the hub, 0x4475ed; TB-C7 of construction-closeout-frost.md): over every shape of the level (its REBSHAPE
	// list) that at least one object uses, *shapes adds the shape's count and *placed its count times the number of objects that use it (0x472811-0x472839). The count is count(shape)
	// above with every polygon ONCE (FUN_004725b0 called with no record: bit 31 doesn't count twice; inteng's other games' branch, FUN_0057aa20(2), is not AvP's). False (both 0) with no level.
	bool PolyCounts_Level(File_Chunk *level, int64_t *shapes, int64_t *placed);
}
