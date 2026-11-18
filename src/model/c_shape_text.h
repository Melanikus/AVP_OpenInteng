// c_shape_text.h: the text of inteng's Construction > Write C File (0xc): FUN_00503da0 writes one shape as a C source file, FUN_00503730 makes its flag words.
// Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every line, its order and its numbers are AvP-docs\engine\inteng\write-c-file-facts.md's COMPLETED MAP (the format strings read in the listing, in code
// order, listing-all-3.txt 56508-60215). This is text only: the caller (host/write_c_file.h) reads the shape from the editor's working bytes and writes the file.
//
// THE NUMBERS: inteng reads the RIF library's ChunkShape of the object's Shape_Chunk; here the same values come from model::ShapeModel and the level's chunks.
// A float or a scaled number becomes an int as __ftol does (toward zero; out of range 0). The level's ENVSDSCL scale multiplies the object places, the header's
// seven numbers and the points, never the normals or the texture coordinates; "_pscale" is always 1.
//
// INTENG BUGS, FIXED (each named, each with a check in tests/c_shape_text_test.cpp):
//   B1 an animation frame's coordinates are read at n x f (0x505eee: the corner count, where a frame holds 2n numbers: animation_chunk.cpp:157), so every frame
//      after the first takes part of its neighbour's: here at 2n x f;
//   B2 with no BMPNAMES a textured polygon is written through a null map (0x50473e: inteng stops) and the header points at a %s_localtextures that is never
//      written; a picture BMPNAMES does not name is listed but never defined: here every picture used gets its definition, its name "" when unknown;
//   B3 a picture name is written between quotes as it is: here a ", a \ and any byte outside 32-126 are escaped (\", \\, \ooo), so the C reads back the name;
//   B4 an item type outside inteng's table of 26 names is read past it (0x5059fe): here its number is written;
//   B5 an animated polygon whose own flags hold 0x40000000 gets "|iflag_txanim" twice: here once;
//   B6 a list of texture coordinates numbered past the shape's lists is written past the table (0x5046b1): here it is left out of the table.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "shape_model.h"
#include "texture_anim.h"
#include "uv_model.h"

namespace model
{
	// An object that uses the shape (SHPHEAD1's list): its name and its place (Object_Chunk +0x44 / +0x48 / +0x4c, before the scale)
	struct CShapeUser
	{
		std::string name;
		int32_t x, y, z;
	};

	// A picture of the level's BMPNAMES: its number and its file name
	struct CShapePicture
	{
		int32_t number;
		std::string name;
	};

	struct CShapeSource
	{
		std::string name;                     // what was typed: every name in the file starts with it
		double scale = 1.0;                   // the level's ENVSDSCL scale, 1.0 without one
		ShapeModel shape;                     // points, polygons, normals, radius and extents
		std::vector<UvList> uvLists;          // SHPUVCRD, in order
		TextureAnim anim;                     // TEXTANIM (no polygons: none)
		std::vector<CShapePicture> pictures;  // BMPNAMES (empty: the level has none)
		std::vector<CShapeUser> users;        // in the list's order
	};

	// FUN_00503730: the flag words of `flags`. forDefine (inteng's mode 1, the "#define <name>_flags" line): 0 gives "0", and the first "|" becomes a space;
	// else (mode 0, an item's own flags): 0 gives "", and each word keeps its "|".
	std::string CShapeFlagText(uint32_t flags, bool forDefine);

	// Inteng's item type names (0x5eb968, 26 of them): the name of `type`, or its number for one outside the table (B4)
	std::string CShapeItemTypeName(int32_t type);

	// FUN_00503da0: the whole file
	std::string CShapeText(CShapeSource const &source);
}
