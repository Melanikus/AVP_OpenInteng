// light_edit.h: inteng's Light Sourcing keys as functions on the open level, with no screen: drop a light (D), copy (C) and paste (V) one, remove
// one (R), switch one on (S) or off (X) or all on, and copy a light's numbers over another's (O); and the lookups that let a light's numbers, an
// object's placed light, the ambience and the light scale be edited through the edit path (field_edit.h, record_edit.h). Written 2026-11-15 by
// Melanikus (Frost) for AVP_OpenInteng (M-3, H1-H5). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH LIGHTS. A level holds its lights as STDLIGHT chunks inside a light set (LIGHTSET, inside the environment data REBENVDT), and the game
// uses the set named NORMALLT (the name is the first 8 bytes of the set's LTSETHDR). A light is named here by its place among that set's
// lights in the file's order, counting from 0. A remove or a drop changes the places after it.
//
// HOW IT WORKS (as object_edit.h's create and delete): the level is written out as the next save would write it (BuildLevel), the STDLIGHT is cut
// out or put in, the sizes of the chunks around it (the light set, the environment data, the whole level) are made right, and
// OpenLevelFile::ReplaceLevel reads the result with the game's own loader and swaps it in. If the loader wouldn't take it, nothing has changed.
// After a success every Chunk * the editor held is gone. A light's numbers (and a placed light's, the ambience's and the light scale's) are NOT
// changed here: they are edited through the edit path (ApplyChunkEdit), on the chunk the Find... functions below give.
//
// WHAT IS PROVEN AND WHAT IS INFERRED.
// - Proven (the game's reader and the 7,328 stock lights, vasquez-defaults.md): a STDLIGHT is 84 bytes; the loader refuses a place farther than
//   2^24 from the middle and a range of 0 or less or above 2^28.
// - INFERRED: "on" is the light's local flag LOFlag_On (0x1, chunk_light.hpp:15), as all 7,328 stock lights have it set. The game's own code never
//   reads it (only prelighting does), so a level that holds a light "off" plays the same. inteng's own code for S and X, and for D, was not
//   read (inteng-menus.md section 6.7), so the numbers of a dropped light are the editor's: the stock lights' most common (spread 1000, range
//   4000, engine flags 0x7, local flags 0x11, full brightness, white, no turn).
// - What the old editor did with a placed light's "T" and "P" keys isn't read; those lights are edited as any chunk.
//
// What the level's bytes hold is hostile: every size is checked as the bytes are walked, and a level that can't be walked is refused.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class Chunk;
class File_Chunk;
class OpenLevelFile;

namespace host
{
	// The bytes of a STDLIGHT chunk after its 12-byte heading: 84 of them
	size_t const kLightBytes = 84;
	typedef std::vector<uint8_t> LightData;

	enum class LightEditResult
	{
		Done,
		NothingOpen,       // no level is open (or none that's paired with its bytes)
		NoLightSet,        // the level has no light set named NORMALLT
		NoSuchLight,       // the light set has no light at that place
		BadLight,          // light data that isn't 84 bytes
		PlaceOutOfRange,   // a coordinate of the place is farther than 2^24 millimetres from the middle
		TooMany,           // the level has no room for another chunk (a size no longer fits)
		CantBuild,         // the level couldn't be put together from the bytes (not walkable, or too big)
		LoaderRefused,     // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *LightEditWords(LightEditResult result);

	// How many lights the light set NORMALLT holds
	LightEditResult CountLights(OpenLevelFile *file, size_t *count);

	// The places of all the lights of the set, three whole numbers (millimetres) each, in the set's order: for picking one with the mouse
	LightEditResult LightPlaces(OpenLevelFile *file, std::vector<int32_t> *places);

	// The numbers of the light at that place (C: copy), to be kept and given to DropLight (V: paste) or OverwriteLight (O)
	LightEditResult CopyLight(OpenLevelFile *file, size_t light, LightData *out);

	// D and V: adds a light at `place` (three whole numbers in millimetres), after the set's last. With data (a copied light) it has those
	// numbers, but for its place and its number; without, the editor's default numbers. *newLight (if not null) is its place in the set.
	LightEditResult DropLight(OpenLevelFile *file, int32_t const *place, LightData const *data, size_t *newLight);

	// R: takes the light out
	LightEditResult RemoveLight(OpenLevelFile *file, size_t light);

	// (S, X and Switch all on are NOT here: inteng's on / off is the editor's own state and writes nothing to a light's chunk (FUN_004ac240), so it is kept in light_state.h; Bishop's ruling 2026-11-15, replacing the
	// earlier SwitchLight / SwitchAllOn, which wrote LOFlag_On.)

	// O: puts the numbers of a copied light over the light at that place, keeping its place and its number
	LightEditResult OverwriteLight(OpenLevelFile *file, size_t light, LightData const &data);

	// Lights > Hide All / Unhide All: puts LOFlag_Invisible (0x20) in the local flags of EVERY light the level holds, or takes it out: the
	// light set NORMALLT's STDLIGHTs (at +72) and every object's PLOBJLIT (at +24), as inteng's FUN_004ab5a0 and FUN_004ab4d0 do to every
	// light of its list, and its save writes that word as it is (FUN_004ad630). The chunks keep their size: the bytes are put in with
	// OpenLevelFile::SetPayload, nothing is rebuilt. *count (if not null) says how many lights were changed.
	LightEditResult HideAllLights(OpenLevelFile *file, bool hide, size_t *count);

	// ---- The chunks a light's numbers are edited on (through ApplyChunkEdit) ----

	// The STDLIGHT at that place of the light set NORMALLT, the light set's AMBIENCE or LITSCALE (id: "AMBIENCE" or "LITSCALE"), or an
	// object's PLOBJLIT (by the object's 8-byte ID). Null when the level has none. Good while the level stays open and isn't replaced.
	Chunk *FindLightChunk(File_Chunk *level, size_t light);
	Chunk *FindLightSetChunk(File_Chunk *level, char const *id);
	Chunk *FindPlacedLightChunk(File_Chunk *level, uint8_t const *objectId);
}
