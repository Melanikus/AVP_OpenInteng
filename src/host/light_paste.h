// light_paste.h: Light Sourcing's V with a PLACED light copied (C on a placed object's light): a new placed object is made at the camera, as inteng's FUN_004ac080 ->
// FUN_004abb00 makes it. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The map: AvP-docs\engine\inteng\reassess\light-create-frost.md.
//
// WHAT inteng MAKES (FUN_004abb00, dump-all-2.txt:42274; listing-all-2b.txt 0x4abb95-0x4abc1b):
//  - only when the copied light's object holds OBJPRJDT > AVPSTRAT of kind 62 (a Placed Object, 0x4abb95); else nothing;
//  - a new object (FUN_004d0e80: its header, FUN_004d21b0, and its notes, FUN_004d2450, as every new object's) of the NAME given (the caller asks it: Light<n>, the Name Object box),
//    associated with the copied object's SHAPE (FUN_004d0ff0), at the camera's place (each coordinate divided by the level's scale, cut toward 0) turned as the camera is (FUN_004de070);
//  - its header's flags: what FUN_004d21b0 gives (0) with the placed bit 0x400 put in (0x4abca1);
//  - its OBJPRJDT > AVPSTRAT: a new kind 62 strategy (FUN_0052b120(0x3e)) with the copied one's Type, ExtraData and mass, its integrity with bit 31 put in, flags 1, the rest 0 (0x4abd3d-0x4abd85);
//  - its PLOBJLIT: the copied light's 15 words (Placed_Object_Light_Data) and no extra words (0x4abda6-0x4abdfd), its local flags with 4 (changed) in: the new light's record takes the copied
//    one's flags | 4 (0x4abf6b) and the Exit write-back writes them into the PLOBJLIT (FUN_004ad630 0x4adcfe).
//
// INTENG BUG, FIXED: the game loads a placed object (kind 62) only when its header has PC load (0x8000) and the player's game-mode bit (object_setup.cpp:2467-2486): inteng's pasted
// object, with 0x400 alone, was in the editor and never in the game. Here the new header takes the COPIED object's load bits too (the game modes 0x800 / 0x1000 / 0x2000, platform load set
// 0x4000, PC / PSX / Saturn load 0x8000 / 0x10000 / 0x20000, not-difficulty 0x100000 / 0x200000 / 0x400000): the copy loads wherever the lamp it copies does.
//
// What the level's bytes hold is hostile: the copied object's strategy and light are measured before they are used; the object is made by object_make.h (the loader takes the result or
// nothing changes).
#pragma once

#include <stdint.h>

#include <string>

class OpenLevelFile;

namespace host
{
	enum class LightPasteResult
	{
		Done,
		NothingOpen,       // no level is open
		NoSuchObject,      // no object of the level has the copied light's ID (it was taken out since C)
		NotPlacedObject,   // the copied light's object holds no Placed Object strategy (kind 62): inteng makes nothing
		NoPlacedLight,     // the copied object holds no PLOBJLIT (its light was taken out since C)
		MakeFailed,        // the object maker refused (*why says how)
	};

	// The header bits a pasted placed light takes from the light it copies (the INTENG BUG, FIXED above)
	uint32_t const kPastedLightLoadBits = 0x00000800 | 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000 | 0x00010000 | 0x00020000 | 0x00100000 | 0x00200000 | 0x00400000;

	// Makes the new placed object of a pasted placed light: `sourceId` the copied light's object, `name` exactly as typed (the caller has checked no object has it), place in the file's
	// units (the camera's), matrix the camera's nine 16.16 numbers. newId (8 bytes) the new object's. why: the maker's words when the answer is MakeFailed.
	LightPasteResult PastePlacedLight(OpenLevelFile *file, uint8_t const *sourceId, std::string const &name, int32_t const *place, int32_t const *matrix, uint8_t *newId, std::string *why);

	// The words of an answer, for the status line: static text, never null
	char const *LightPasteWords(LightPasteResult result);

	// inteng's FUN_004ab980: "Light%d" with the first number from 0 that no object's name equals without regard to capitals (FUN_005909e0, _stricmp); "" when none is free
	std::string LightPasteFirstName(OpenLevelFile *file);
}
