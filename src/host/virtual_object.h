// virtual_object.h: Strategy mode's Create Virtual Object (commands 0x39 Here, 0x3a Above, 0x3b Below, 0x3c Left, 0x3d Right, 0x3e Front, 0x3f Behind), as plain functions. Written 2026-11-15
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_00524e90 cases 0x39-0x3f, dump-all-3.txt:58467-58581; the constructor FUN_005396d0, dump-all-3.txt:71775-72000, raw listing-all-3.txt:124919-125050):
//   A virtual object is a VIOBJECT chunk in REBENVDT > SPECLOBJ (path_mode.h's SPECLOBJ), holding a VOBJPROP chunk (avpx strategy_chunk.cpp: location x y z, size, the name padded to 4
//   with its end, the 8-byte ID, two spare numbers) and, once a strategy is assigned to it (Assign Strategy, not here), an AVPSTRAT chunk. The game reads a VIOBJECT that has both
//   (object_setup.cpp DealWithExternalObjectStategies) and skips one without a strategy.
//   Create: SPECLOBJ is made (last in REBENVDT) when missing (FUN_00404560); the VIOBJECT is made last in it; the prompt "Type name for new virtual object" (0x5f02b0) is asked; an answer
//   that is not OK gives no virtual object; the ID is the first 7 bytes of md5("VirtOb" + name) with the 8th byte 0 (Virtual_Object_Properties_Chunk::CalculateID); an ID that an object
//   already holds gives "Please try another name" (0x5f0298) and the question again (here: the answer is refused and the question stays); the location is each of x, y, z, size
//   (whole numbers in the game's units) DIVIDED BY the level's scale and cut to a whole number (0x539ad2-0x539b31: FILD, FDIV [0x607bd8], __ftol), the size being 100000.
//   WHERE (FUN_00524e90 0x524fc6-0x525034): Here (0x39) = the view's place (DAT_00962308 + 0x10..0x18); the other six start from the SELECTED object's place (+0x10..0x18) and move one axis:
//     Above (0x3a): y = y + ymin - 60000;  Below (0x3b): y = y + ymax + 60000;  Left (0x3c): x = x - 60000 + xmin;  Right (0x3d): x = x + 60000 + xmax;
//     Front (0x3e): z = z + 60000 + zmax;  Behind (0x3f): z = z - 60000 + zmin,   where min / max are the object's extents (record +0x104..+0x118), here the shape's own min / max points.
//   NAMED: the extents are read as the shape's own min and max (the record's extents are read from them by the game's display block: not read); the object picked for the six moves is the
//   editor's selected object; Destroy Virtual Object (0x40) is not here (a virtual object is not one of the editor's selectable objects yet).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;
class File_Chunk;

namespace host
{
	enum class VirtualObjectResult
	{
		Done,
		BadCall,        // no level
		BadName,        // empty, too long, or not plain text
		BadKind,        // no such kind of strategy
		NotFound,       // no virtual object has that ID
		NameUsed,       // an object or a virtual object already has that ID: "Please try another name"
		NoEnvironment,  // the level has no REBENVDT
		CantBuild,      // the level couldn't be put together from its bytes
		LoaderRefused,  // the game's own loader wouldn't take the changed level, so nothing was changed
		BadBytes,       // strategy bytes shorter than a strategy's header (12) or longer than 65536
	};

	char const *VirtualObjectWords(VirtualObjectResult result);

	size_t const kVirtualObjectMostName = 60;
	int32_t const kVirtualObjectSize = 100000;     // the size inteng passes (game units)
	int32_t const kVirtualObjectOffset = 60000;    // how far the six moves go

	// The first command of the seven, and the last
	int32_t const kVirtualHere = 0x39;
	int32_t const kVirtualBehind = 0x3f;

	// The 8-byte ID of a virtual object of that name (md5 of "VirtOb" + name, first 7 bytes, the 8th 0)
	void VirtualObject_Id(char const *name, uint8_t *id);

	// Where command 0x3a..0x3f puts the new object (game units): objectPlace = the selected object's place, min and max = its shape's extents. False for command 0x39 or a number
	// that is not one of the six (the place for Here is the view's).
	bool VirtualObject_Moved(int32_t command, int64_t const objectPlace[3], int64_t const min[3], int64_t const max[3], int64_t out[3]);

	// THE LEVEL'S VIRTUAL OBJECTS (every VIOBJECT of SPECLOBJ that holds a VOBJPROP, in file order) and DESTROY (0x40, FUN_00524e90 case 0x40, vtable +0x14 = FUN_00539e00: the VIOBJECT chunk with
	// its VOBJPROP and any AVPSTRAT goes; nothing that names its ID is checked). The question "Delete this object?" is the caller's.
	struct VirtualObjectInfo
	{
		uint8_t id[8];
		std::string name;
		int32_t place[3];     // file units
		int32_t size;
		bool hasStrategy;     // its VIOBJECT holds an AVPSTRAT
	};

	size_t const kVirtualObjectMost = 100000;

	bool VirtualObject_List(OpenLevelFile *file, std::vector<VirtualObjectInfo> *list);

	// Cuts the first VIOBJECT whose ID is that one. NotFound when there is none (the level is as it was).
	VirtualObjectResult VirtualObject_Destroy(OpenLevelFile *file, uint8_t const *id);

	// Gives the virtual object with that ID a new strategy of that kind (model::MakeNewStrategy: every field at its creation value), in place of the AVPSTRAT its VIOBJECT holds, if any (FUN_00528830
	// on a virtual object: the class takes Binary / Linked / Multi / Area Switch, Mission Objective, Message, Volume of Death, Self Destruct). The strategy goes last in the VIOBJECT, after its VOBJPROP.
	VirtualObjectResult VirtualObject_AssignStrategy(OpenLevelFile *file, uint8_t const *id, int32_t kind);

	// Puts these strategy bytes (the AVPSTRAT's payload) in place of the strategy of the virtual object with that ID, as AssignStrategy does: object_edit.h's SetStrategyBytes reaches placed objects and
	// modules only, so a Mission Objective's alterations (S-K70-7, -8, -R), all on virtual objects in the stock levels, are written through this. BadBytes for under 12 or over 65536 bytes.
	VirtualObjectResult VirtualObject_SetStrategyBytes(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length);

	// The place (file units) and the shape's min and max points of the object with that ID: false when there is no such object or it has no shape
	bool VirtualObject_Geometry(File_Chunk *level, uint8_t const *id, int64_t place[3], int64_t min[3], int64_t max[3]);

	// Makes the virtual object: place in game units, divided by scale (the level's, above 0) and cut to whole numbers, size too. name 1 to kVirtualObjectMostName plain letters.
	VirtualObjectResult VirtualObject_Create(OpenLevelFile *file, char const *name, int64_t const place[3], double scale);
}
