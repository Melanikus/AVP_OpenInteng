// camera_file.h: inteng's CAMERA FILE, the eleven saved camera places of a level (F1 to F10 and the slot every mode starts from). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng (traced, FUN_00470950 reads and FUN_00470610 writes, both from the level's RIFFNAME chunk under REBENVDT): the file is named <RIFFNAME>.cam (the name with ".cam" appended, DAT_005d3d6c = ".cam"),
// and holds, in this order, in plain 32-bit little-endian numbers: the places of slots 0 to 9 (10 x 3 numbers: 0x78 bytes), their turns (10 x a 3 x 3 matrix of 16.16 numbers, the game's MATRIXCH in the order
// mat11, mat12, mat13, mat21 ...: 0x168 bytes), then slot 10's place (3 numbers) and its turn (a matrix). 528 bytes. inteng reads it when the level opens (the camera of the first frame is NOT from it: the hub
// restores slot 10 first, FUN_00447550, and only then is the file read, so a saved place is used from the next F key or mode start) and writes it when the program ends.
//
// THE OFFSET (FUN_00470950 0x470b97 to 0x470de1, read from the listing): the first CAMORIGN child of the level's first SPECLOBJ (under REBENVDT) gives three doubles (x, y, z: the first 24 bytes of its data, at +0x28, +0x30,
// +0x38 of inteng's own chunk object); with them the level's scale: ENVSDSCL's, or 1 when the level has none (inteng's default is the double 100.0, 0x40590000 at 0x470cd6, and every scale is times 100.0, the double at
// 0x5ac0f8). Each place the file gives is moved by (scale x 100 x x, scale x 100 x y, scale x 100 x z) cut to whole numbers (__ftol), and the same is taken off again when it is written (FUN_004fd9d0). With no CAMORIGN the offset is 0. The
// offset is applied only when a camera file was read, as in inteng (it comes after the file is opened). Slot 10's place at the first frame is not moved by it.
//
// NAMED (security fixes, which are the only differences): the file is read from and written to a folder of the editor's own (<the program's folder>\camera\), never beside the mapper's files; the name is the RIFFNAME's
// last part (after any "/" or "\"), kept to letters, digits and . - _ (anything else becomes _), at most 100 letters; a file that isn't exactly 528 bytes is not read at all.
#pragma once

#include <stdint.h>

#include <string>


class File_Chunk;   // the game's chunk library (mishchnk.hpp): the open level as it holds it

namespace host
{
	// A slot in the editor's terms: the place in the game's millimetres and the turn as the game's view matrix (VDB_Mat: 16.16 numbers, mat11, mat12, mat13, mat21 ...). The FILE holds inteng's own: the view's
	// +0x10 and +0x38 as FUN_004704d0 keeps them, the place in inteng's 1/100 mm (the offset taken off) and the matrix TURNED OVER (inteng's view keeps VDB_Mat transposed: FUN_004c19a0, FUN_004b3710). CameraFile_Read
	// and CameraFile_Write convert both ways (a place to the nearest mm, halves away from 0; a place too far for the file is held at the largest number, never wrapped). Until 11-18 (Vasquez) the file held
	// millimetres and the game's form, so an inteng-written file read 100 times too far and turned the other way; a file saved by those builds now reads 100 times too near. The editor turns a slot into its
	// camera and back (editor_main.cpp).
	struct CameraFileSlot
	{
		int32_t place[3];
		int32_t matrix[9];
	};

	size_t const kCameraFileSlots = 11;
	size_t const kCameraFileBytes = 0x78 + 0x168 + 0xc + 0x24;

	// The file name (without folder) the open level's camera is kept under: its RIFFNAME and ".cam", cleaned as above. False when the level has no RIFFNAME.
	bool CameraFile_Name(File_Chunk *level, std::string *name);

	// The offset the level gives every place of its camera file (see above); 0 0 0 when it has none
	void CameraFile_Offset(File_Chunk *level, int32_t offset[3]);

	// Read the eleven slots from folder + name; slots stay as they were when it can't be read (no file, or not exactly kCameraFileBytes). True when read.
	// form: the file's matrices are inteng's view block +0x38 (VDB_Mat turned over); the slots get the game's (CreateEulerMatrix's own, VDB_Mat for no look)
	bool CameraFile_Read(std::string const &path, int32_t const offset[3], CameraFileSlot slots[kCameraFileSlots]);

	// Write the eleven slots (the folder is made if it isn't there). True when written.
	// form: the slots' matrices are the game's (as Read gives them); the file gets inteng's +0x38, turned over
	bool CameraFile_Write(std::string const &folder, std::string const &name, int32_t const offset[3], CameraFileSlot const slots[kCameraFileSlots]);
}
