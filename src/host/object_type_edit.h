// object_type_edit.h: inteng's Construction > Object > Change Object Type, Module (0x12d) and Placed (0x12e), on the open level.
// Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES. Both items run their handler ONCE FOR EVERY SELECTED OBJECT (the selection list DAT_00631230, out\menu-reads-1-full.txt:6236 and
// :6249; nothing happens with an empty selection). The handlers read the object's chunk (the one at object + 0x154) and its children by NAME, in the
// game's own list: the lookup puts the children it finds into a list, and the next lookup starts from the FIRST entry of that list.
//
// Module, 0x12d = FUN_00448a10 (out\menu-reads-5.txt:~1100-1330 after "== @00448a10"; raw-listing-10.txt:12-40):
//   - the object has a MODULEDT: nothing is done at all (flags too);
//   - else, when it has an OBJPRJDT: every AVPSTRAT child of the FIRST OBJPRJDT is destroyed (the OBJPRJDT itself stays, even when it is left empty);
//   - then the header's flags (the first number of OBJHEAD1: FUN_004d0fc0 looks "OBJHEAD1" up): bit 0x400 (placed) cleared, bit 0x200 (module) set.
//   It does NOT make a MODULEDT.
//
// Placed, 0x12e = FUN_00448530 (out\menu-reads-5.txt:484-670; apone-reads-2.md "For Frost"):
//   - the object has a MODULEDT: a question "%s has module linking data. Do you want to delete it?" (the object's name); the answer 3 (yes) destroys the
//     FIRST MODULEDT and goes on; any other answer leaves THIS object as it was (the loop goes on to the next selected object);
//   - the OBJPRJDT: the first the object has, or (the compare at 0x448734 is the count of the list the lookup just filled: raw-listing-apone-2.txt:2-3)
//     a NEW one, put last in the object, when it has none;
//   - every AVPSTRAT child of that OBJPRJDT is destroyed, and a NEW AVPSTRAT is put last in it, holding the placed object's strategy
//     FUN_00448990(0, 0, 0, 0): kind 0x3e (62), Type 0, ExtraData 0, mass 0, integrity 0 | 0x80000000, flags 1, nothing targeted;
//   - the header's flags: |= 0x3800 (the three game modes), |= 0x400 (placed), &= ~0x200 (module).
//
// WHAT THIS DOES. The same data effect, on the bytes of the whole level (as object_edit.h does it): the level is written out as the next save would
// write it, the objects' chunks are made again, the sizes of the chunks around are made right, and OpenLevelFile::ReplaceLevel reads the result with
// the game's own loader. If the loader won't take it, nothing has changed. After a success every Chunk * the editor held is gone, and an object is
// named again by its 8-byte ID. One new image for all the selected objects, so the change is all or nothing.
//
// NAMED POINTS (none changes the data effect):
//   - inteng asks its question once per object with a MODULEDT, in the selection's order. Here the asker (the editor) says in advance, per object,
//     whether the MODULEDT may go; an object not allowed to lose it is left as it was, as inteng's "no" does.
//   - inteng's handlers work on the objects the editor holds in memory; here a selected object that the level's bytes no longer hold is NoObject.
//   - Hostile bytes: every size is checked as the chunks are walked, a level that can't be walked is refused, and so is a header without room for its
//     flags.
#pragma once

#include <stddef.h>
#include <stdint.h>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ObjectTypeWhat
	{
		Module,   // Change Object Type: Module (0x12d)
		Placed,   // Change Object Type: Placed (0x12e)
	};

	// One selected object: its 8-byte ID, and (for Placed) whether the answer to the module-data question is yes. For Module it is not used.
	struct ObjectTypeCall
	{
		uint8_t id[8];
		bool deleteModuleData;
	};

	enum class ObjectTypeOutcome
	{
		Changed,        // the object's chunks and flags are as inteng leaves them
		HasModuleData,  // Module: the object has a MODULEDT, nothing is done. Placed: it has one and the answer was no: nothing is done.
		NoObject,       // no object of the level has that ID
	};

	enum class ObjectTypeResult
	{
		Done,           // the calls are answered one by one in outcomes[]; the level is the new one when any object changed
		BadCall,        // no level, no calls, too many calls, or a null ID
		CantBuild,      // the level couldn't be put together from its bytes (not walkable, a header with no room for its flags, too big)
		LoaderRefused,  // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *ObjectTypeWords(ObjectTypeResult result);

	// The most objects one call takes (a selection is never near this; a hostile count is refused before anything is walked)
	size_t const kObjectTypeMostCalls = 100000;

	// Which of the objects (ids: count 8-byte IDs, one after the other) hold a MODULEDT: has[i] is true when the i-th does. Placed asks about these.
	// Gives back false when the level can't be walked (has[] is then all false).
	bool ObjectType_HaveModuleData(OpenLevelFile *file, uint8_t const *ids, size_t count, bool *has);

	// Changes the type of every object in calls (count of them). outcomes (count entries) says what became of each. An ID that comes twice is changed once
	// (its second outcome says the same as the first).
	ObjectTypeResult ObjectType_Change(OpenLevelFile *file, ObjectTypeWhat what, ObjectTypeCall const *calls, size_t count, ObjectTypeOutcome *outcomes);
}
