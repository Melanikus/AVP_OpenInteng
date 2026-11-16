// alt_locations_edit.h: an object's alternate locations in the OPEN level: read them into the editor's state (model::AltLocations) and write them back,
// as inteng's load and save do. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (P9, the file side). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE FILE RULES (alt_locations.h has the sources: FUN_004d3950 loads, FUN_004dfc70 saves, out\menu-reads-16.txt):
// - LOAD: the object's ALTLOCAT chunk (a child of its RBOBJECT) becomes the state: the group, the spare, the entries as they are. No chunk: an empty list.
// - SAVE: the object's old ALTLOCAT is destroyed FIRST (the first one found, as inteng's FUN_00431a00 finds one); an EMPTY list makes NO chunk; a list
//   with entries makes a new ALTLOCAT, put after the object's other chunks, with the group, the spare, the count and the entries
//   (model::AltLocations_Build: ints 7 and 8 of every entry are 0).
//
// HOW (as object_edit.h and light_edit.h do it): the level is written out as the next save would write it (BuildLevel), the chunk is cut out and a new one put
// in, the sizes of the object and the level are made right, and OpenLevelFile::ReplaceLevel reads the result with the game's own loader. If the loader
// wouldn't take it, nothing has changed. After a success every Chunk * the editor held is gone. An object is named by its 8-byte ID.
//
// What the level's bytes hold is hostile: every size is checked as the bytes are walked, and a level that can't be walked is refused.
#pragma once

#include <stdint.h>

#include <vector>

#include "alt_locations.h"

class OpenLevelFile;

namespace host
{
	enum class AltLocationsResult
	{
		Done,
		NothingOpen,       // no level is open (or none that's paired with its bytes)
		NoName,            // the ID is null or all 0
		NoSuchObject,      // no object in the level has that ID
		BadChunk,          // the object's ALTLOCAT isn't one the game's reader would take
		TooMany,           // the list is longer than a chunk's size can say
		CantBuild,         // the level couldn't be put together from the bytes (not walkable, or too big)
		LoaderRefused,     // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *AltLocationsWords(AltLocationsResult result);

	// LOAD: the object's alternate locations. *state is an empty list (current 0) when the object has none.
	AltLocationsResult AlternateLocations_Load(OpenLevelFile *file, uint8_t const *id, model::AltLocations *state);

	// SAVE: puts the state in the object, as above: the old chunk out, and a new one when the list isn't empty.
	AltLocationsResult AlternateLocations_Save(OpenLevelFile *file, uint8_t const *id, model::AltLocations const &state);

	// The object as it is now, in file form, for the list rules (Add new, Next, Prev store it): its place (three whole numbers) and its turn (four
	// floats) from its OBJHEAD1. inteng keeps the object's own matrix; the file's four floats are what the editor holds.
	AltLocationsResult AlternateLocations_ObjectNow(OpenLevelFile *file, uint8_t const *id, model::AltLocation *now);

	// FUN_004dfae0: puts a location on the object: its place and its turn are the location's (written into the OBJHEAD1's numbers, nothing else
	// changes). A place beyond 2^24 millimetres or a turn part that isn't a real number from -2 to 2 is refused (the game's loader would refuse it).
	AltLocationsResult AlternateLocations_PutOnObject(OpenLevelFile *file, uint8_t const *id, model::AltLocation const &location);

	// The IDs (8 bytes each, one after the other) of the objects that have an ALTLOCAT chunk, for Change shown location (all objects)
	AltLocationsResult AlternateLocations_ObjectsWithLocations(OpenLevelFile *file, std::vector<uint8_t> *ids);
}
