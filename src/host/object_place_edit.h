// object_place_edit.h: the object-placement items that read and change one object's header, through the edit store: finding an object by its name or
// ID, moving it, and naming it. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (M-1, object placement: inteng's Find, Move
// relative and Name). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE THE NUMBERS ARE. An object's header is its OBJHEAD1 chunk (avpx/3dc/win95/chunk_object.cpp:392-446). After the chunk's 12-byte heading
// it holds: the flags (4 bytes), the user who locked it (16), the PLACE (three whole numbers, bytes 20 to 31), the turn (four floats, 32 to
// 47), the index, the version and the shape's number (4 bytes each), and from byte 60 on the object's NAME, ended by a 0 and rounded up to a
// multiple of 4. The editor never changes the game's class for a chunk: it changes the chunk's BYTES in the edit store (OpenLevelFile's
// SetPayload, level_file.h), which has to keep the chunk's size.
//
// THE RULES (all of it is typed by a person, so it's checked as hostile; Tao 1 and 3):
// - A place is held to FarthestPlace either way (2^24, chunk.hpp:492), as the game's loader holds it (CheckPlace), and the sums are done in 64
//   bits, so no typed number can wrap round.
// - A name has to pass the game's own file-name rule (level_file_name.h), be 1 to 63 letters, and be none already used by another object in
//   the level (letters counted the same in capitals or not). Any length is taken, as inteng takes it: the rename makes the level again with the
//   object's header at the size the game's library writes it for the new name (object_edit.h RenameObject; 2026-11-17, Drake: it used to have
//   to fit the old name's room, which kept the header's size but left the library's chunk a different size, strategy-data.md S-35a).
// - An object's ID comes from its level's name and its own, so a new name is a new ID: the answer says it. The level is made again (every
//   Chunk * from before is gone), so the caller builds its lists again.
// - Nothing is changed when an answer isn't Done.
// - The level in the game's world isn't changed: what the view draws is what the file held when the level was opened. The edits are in the
//   editor's bytes, for the field panel and for the save.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class File_Chunk;      // the game's chunk library
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

#include <array>
#include <vector>

namespace host
{
	// The most a typed name or search is read to, in letters
	size_t const kObjectEditTextMost = 63;

	// The object a search found
	struct FoundObject
	{
		std::string name;      // up to its first 0, at most 63 letters (the level's own)
		uint8_t id[8];         // the object's ID
		int32_t x;             // its place, as the file holds it (the world's place is this times the level's scale)
		int32_t y;
		int32_t z;
		size_t matches;        // how many objects matched in all: the first in the file's order is the one given
	};

	// Finds an object by what was typed: exactly 16 hexadecimal digits are an ID, anything else is a part of a name, in capitals or not. Its
	// place is from the editor's bytes when the store has them. False, with *found left alone, for no level, nothing typed, or no match.
	bool ObjectEdit_Find(File_Chunk *level, const OpenLevelFile *store, const char *typed, FoundObject *found);

	enum class ObjectEditAnswer
	{
		Done,
		BadCall,      // no store or no level, no ID, or no room for an answer
		NoObject,     // no object has that ID
		NoHeader,     // the object has no header, or the store has no bytes for it
		Refused       // *why says why, in plain words
	};

	// Moves the object by (dx, dy, dz) in the file's units. The new place has to be inside FarthestPlace either way.
	ObjectEditAnswer ObjectEdit_Move(OpenLevelFile *store, File_Chunk *level, const uint8_t *id, long long dx, long long dy, long long dz,
		std::string *why);

	// Gives the object a new name (the rules above). *newId gets its new ID when the answer is Done.
	// The size inteng gives an object for a lift (FUN_00529850: the Lift view's "Radius %f" and each position's "R:%f"): the shape's stored box (SHPHEAD1's max and min, the chunk library's
	// ChunkShape) across x and z, sqrt((max.x - min.x)^2 + (max.z - min.z)^2) with the differences taken as whole numbers, times the level's ENVSDSCL scale (1.0 when it has none). False when no object
	// has the ID (inteng then gives 1.0) or the object has no shape (inteng reads through the null shape there; ours gives 1.0 too: memory safety, reassess/strategy-data-kinds.md S-K51-2).
	bool ObjectEdit_ShapeRadius(File_Chunk *level, const uint8_t *id, double *radius);

	// The IDs of every object of the level (modules and placed objects), in the file's order
	void ObjectEdit_Ids(File_Chunk *level, std::vector<std::array<uint8_t, 8>> *ids);

	ObjectEditAnswer ObjectEdit_Rename(OpenLevelFile *store, File_Chunk *level, const uint8_t *id, const char *name, uint8_t *newId,
		std::string *why);
}
