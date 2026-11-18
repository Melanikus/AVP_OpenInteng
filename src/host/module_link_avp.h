// module_link_avp.h: LINK WITH AVP (Module Linking's Misc > Link with AVP, 0x190): inteng's live link with a running DEBUG build of the game, through two files in the game's avp_rifs
// folder. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// INTENG (re-tools\projects\vasquez-inteng\out; AvP-docs reassess/module-linking.md, the four Link with AVP rows):
// - THE SWITCH: FUN_004c1a90 0x4c36b6-0x4c36da flips DAT_0070436c and ticks the item (the menu object's +0x2c with 400 and the flag), and nothing else; Exit clears it (FUN_004c3b90
//   0x4c3bb3, so Exit + New / All Adjacencies too).
// - ITS READER is the window procedure, on WM_ACTIVATEAPP (listing-all-4.txt 0x58ba6f-0x58bc26): wParam goes to DAT_00602b20; after the surfaces and the cursor, with the switch on, the
//   program GETTING the focus calls FUN_004c19a0 and LOSING it calls FUN_004c0f00 (dump-all-2.txt 59060 / 58581). (Until 11-18 the docs said the switch was read by nothing.)
// - FUN_004c0f00, THE FOCUS LOST: the current module's tree is written into the level when it changed (FUN_004c10c0, FUN_004c17a0, its "changed" cleared), then "module.aaa" is made
//   (CreateFileA, GENERIC_WRITE, not shared, CREATE_ALWAYS): 60 bytes of the level's file name (DAT_0060801c +0x2c, the File_Chunk's), then for every object of the level's list
//   (DAT_0093d120, DAT_0093d108 of them, the file's order) that has a file object (+0x154) whose MODULEDT holds a VMDARRAY: the object's index (+0x98), the item count (+0x28), then per
//   item its object_index (+0xc) and its branch_no (+0x0). The object list is drawn again when the tree write asked for it (DAT_0070438c, FUN_004719e0(5)).
// - FUN_004c19a0, THE FOCUS BACK: "module.bbb" is opened (GENERIC_READ, OPEN_EXISTING; none: nothing), 12 bytes read (the game's VDB_World) and 36 (its VDB_Mat), the file closed and
//   DELETED, and the view takes it: the place x 100 (inteng's unit is 1/100 mm) at the view's +0x10, the matrix transposed (FUN_004b3710 swaps 1/3, 2/6, 5/7: inteng keeps the view's
//   matrix the other way round) at its +0x38.
// - THE GAME'S SIDE (avpx projload.cpp:3204-3353 LoadModuleData, screenshot.cpp:80-99): Shift with the save-camera key starts it; then EVERY frame the game writes avp_rifs/module.bbb
//   and reads avp_rifs/module.aaa: the name after its last '/' or ':' must be the open level's (system.cpp LoadTheLevel: "avp_rifs/<name>.rif"), else the file is deleted; then each
//   record's links become that module's VMODULE list (vmodi_bra_vc when the branch is not 0) and the file is deleted. ALL of it is inside `#if debug`: a release game has none of it.
//
// OURS: the same two files, in the game's avp_rifs folder (inteng names them relative, "module.aaa", in its working folder; the game's are "avp_rifs/module.aaa"; the editor's data
// folder is the game's folder, module_link_avp_editor.cpp). The name is LevelView_FilePath's "avp_rifs/<name>.rif", the game's own path of the level (the copy the editor opens in
// avp_rifs/OpenedByEditor has the same last part, which is all the game compares). The camera: our camera is in mm, so the place is the game's as it is (inteng's x 100 is its unit);
// our camera's angles are of the game's own matrix form (LevelView_AnglesOfMatrix: the view the game builds from the camera), so the game's VDB_Mat is taken as it is (inteng's
// transpose is its own view's form).
//
// INTENG BUGS, FIXED (MAPPING-ORDERS section 15), each a named difference:
// (1) the name is copied with no bound into 60 bytes on the stack (0x4c0f6f-0x4c0f96) and the bytes after its end are whatever the stack held: ours writes zeros after it, and a name
//     that does not fit is cut to its part after the last '/' or ':' (all the game compares); one whose last part does not fit either is not written (the status line says why).
// (2) module.aaa's CreateFileA is not checked (0x4c0f5b): a file that can't be made is written to nowhere, silently. Ours says so on the status line.
// (3) module.bbb is read with no check of its size or its numbers (0x4c19d0 / 0x4c19e1: a short file leaves the rest as the stack held it; x 100 overflows past 21,474,836 mm). Ours
//     reads it as HOSTILE input: exactly 48 bytes, the place within the camera's limits (2^24 mm each way, level_view.h kFlyCameraMostPlace), the matrix a turn (each row of length 1
//     and square to the others within 2%, not a mirror); anything else changes nothing and the status line says why. The file is deleted either way, as inteng deletes what it read.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	size_t const kLinkAvpNameBytes = 60;     // module.aaa's name field
	size_t const kLinkAvpCameraBytes = 48;   // module.bbb: VECTORCH (12) and MATRIXCH (36)
	int32_t const kLinkAvpMostPlace = 1 << 24;   // level_view.h kFlyCameraMostPlace (module_link_avp_editor.cpp checks they agree)

	// One VMDARRAY item as module.aaa carries it (VMod_Arr_Item, chunk_type.hpp: object_index and branch_no)
	struct LinkAvpItem
	{
		int32_t objectIndex;
		int32_t branchNo;
	};

	// One object whose MODULEDT holds a VMDARRAY: its index (OBJHEAD1's index_num) and the items, in their order
	struct LinkAvpModule
	{
		int32_t index;
		std::vector<LinkAvpItem> items;
	};

	// module.aaa's bytes (FUN_004c0f00), 32-bit little-endian numbers: the name field (levelName and zeros; see bug (1) for a long one), then per module its index, its item count and
	// per item the object_index and the branch_no. False, with why, when the name can't be put in the field or a module has more items than a count holds.
	bool LinkAvp_AaaBytes(std::string const &levelName, std::vector<LinkAvpModule> const &modules, std::vector<uint8_t> *out, std::string *why);

	// module.bbb as read (FUN_004c19a0): the game's camera place (mm) and its matrix (16.16, the game's MATRIXCH order mat11, mat12, mat13, mat21 ...)
	// form: the game's VDB_Mat as it is (local to world), the form LevelView_AnglesOfMatrix takes; inteng turned it over for its own view block
	struct LinkAvpCamera
	{
		int32_t place[3];
		int32_t matrix[9];
	};

	// Reads module.bbb's bytes as HOSTILE input (bug (3) above): true only for exactly 48 bytes whose place is within kLinkAvpMostPlace on each axis and whose matrix is a turn.
	// False, with why, otherwise (*out is then not changed).
	bool LinkAvp_ReadCamera(uint8_t const *bytes, size_t size, LinkAvpCamera *out, std::string *why);

	// ---- The editor's side (module_link_avp_editor.cpp): the switch, the focus, the state line ----

	// Whether Link with AVP is on (DAT_0070436c): the item's tick
	bool ModuleLinkAvp_On();

	// The item (0x190): the switch flipped (refused, with the words the other Module Linking items say, while AutoLink or Delete Links runs: the trees are the run's)
	void ModuleLinkAvp_Toggle();

	// Module Linking's Exit (FUN_004c3b90 0x4c3bb3): the switch off
	void ModuleLinkAvp_Off();

	// The window procedure's WM_ACTIVATEAPP (0x58bc04): with the switch on, active false writes the tree and module.aaa, active true takes module.bbb's camera
	void ModuleLinkAvp_AppActive(bool active);

	// The harness's fields, every frame: linkavp (on / off), linkavpaaa (the last module.aaa: what was written or why not), linkavpbbb (the last module.bbb: taken, or why not)
	void ModuleLinkAvp_StateLine();
}
