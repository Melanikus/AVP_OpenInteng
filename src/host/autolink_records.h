// autolink_records.h: the RECORDS AutoLink works on, numbered exactly as inteng numbers them in Module Linking (the ID colour of record n is n + 1; the driver takes its numbering from here).
// Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (reassess/autolink-meet-vasquez.md AM-11..AM-24, checked by Dietrich on listing-all-2):
// - Module Linking's entry (FUN_004c3a00) takes every record out (FUN_004d41f0; the hub's exit FUN_004476d0 marked them all, FUN_004704a0) and makes them again from the level's map
//   (FUN_0046e700(0x481), then FUN_0046d9e0 -> FUN_004b2590): RECORDS 0 .. N-1 ARE THE LEVEL'S MODULES, in the file's RBOBJECT order, those with no placed bit (OBJHEAD1 +0x2c & 0x400,
//   0x46ee3b) and a shape; then one viewer record (an editor shape: no part). A PLACED OBJECT HAS NO RECORD there.
// - AutoLink's setup (FUN_004c6380) gives every record whose shape has a SHPMORPH (a door; FUN_004d0fd0's shape, 0x4c6556-0x4c65a3) a STAND-IN: from the last record down to the first
//   (0x4c6963-0x4c6a20), FUN_00447f10 makes a new record appended after all the others, so the stand-ins follow the modules in DESCENDING order of their doors; DAT_00704330[stand-in] = its
//   door's number, -1 for every other record (0x4c68e6-0x4c6923, 0x4c6a14). The kind-1000 path is not taken in AvP (AM-11).
// - A stand-in's shape (FUN_00447f10 0x447f4c-0x448333) is a closed BOX over the door's shape: 8 points, point i at (bit 0 ? max x : min x, bit 1 ? max y : min y, bit 2 ? max z : min z) of the
//   door shape's extents; 6 quads with OUTWARD normals: -x (0 4 6 2), +x (1 3 7 5), -y (4 0 1 5), +y (2 6 7 3), -z (0 2 3 1), +z (4 5 7 6); its own extents the door's widened by 1 on each
//   side (0x44831b-0x448333); the block at the door's place (0x4483a5-0x4483cb).
// NAMED: the editor's filter of the modules is AutoLinkScene_Build's (the level's modules with a shape the model reads); inteng also leaves out an object whose record +0x40 is set and one whose
// shape has flag 0x1000 (0x46eebe-0x46ef0a): no stock module has either (to check against the oracle).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "autolink_portal.h"
#include "autolink_scene.h"

class OpenLevelFile;

namespace host
{
	struct AutoLinkRecords
	{
		AutoLinkScene scene;                                  // the modules (records 0 .. scene.list.size() - 1) and their shapes' storage
		std::vector<std::unique_ptr<PortalShape> > boxes;     // the stand-ins' box shapes
		std::vector<PortalModule> records;                    // record n: its place, extents and shape (a stand-in: its box), in inteng's memory units
		std::vector<int32_t> objectIndexOf;                   // record n: its module's object number (a stand-in: its door's)
		std::vector<int32_t> standsFor;                       // record n: -1, or for a stand-in the record number of its door (DAT_00704330)
		std::vector<uint8_t> isDoor;                          // record n: 1 when its shape has a SHPMORPH (a door with a stand-in)
	};

	// The stand-in's box over a door's shape extents (maxX, minX, maxY, minY, maxZ, minZ): its 8 points and 6 faces with their normals, and its own extents (the door's widened by 1)
	void AutoLink_StandInBox(int32_t const doorExtents[6], PortalShape *box, int32_t boxExtents[6]);

	// The open level's records, as inteng's Module Linking holds them for AutoLink (emptied first). Changes nothing.
	void AutoLink_Records(OpenLevelFile *open, double levelScale, AutoLinkRecords *out);
}
