// strategy_lookup.h: from what's selected in the editor (an object, by its 8-byte ID) to that object's strategy, the AVPSTRAT chunk
// inside it, in the level that's open. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng (the two-dev sprint: the step between
// the selection and the strategy record). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT DOES. The selection (selection.h) names an object by the 8 bytes the level gives it. The level, as the game's chunk library
// holds it, is a tree: the level holds objects (RBOBJECT), an object that has a strategy holds a project-data chunk (OBJPRJDT), and that
// holds the strategy (AVPSTRAT). This walks that tree, the way the game's own loader does (object_setup.cpp, deal_with_module_object),
// and gives back the strategy's payload: the bytes the AVPSTRAT chunk holds after its 12-byte chunk heading, which is what
// BuildStrategyRecord (strategy_record.h) and the field model (field_model.h) take. The payload is a COPY: the level is never changed
// here, and the copy stays good however long the level stays open.
//
// THE RULES.
// - The level's bytes are hostile. Nothing is read that the game's own loader hasn't already accepted, and a strategy that isn't
//   there, isn't whole, or is far larger than any real one is refused, never guessed at.
// - An object is found by its ID alone, never by its place in a list. Two objects with one ID (a level can have them: the ID is made
//   from the object's name) give the FIRST one in the level's own order, the same one the object list's FindById gives.
// - An ID that's all 0 is "no name" (the game's own NAME_ISNULL, strategy_def.h:299): it's refused without a search.
// - An object with no strategy (scenery, most of them) is not a mistake: it gives NoStrategy, which the panel shows as "no strategy".
// - The chunk is given back too (ObjectStrategy::chunk), for the edit path to mark as changed (OpenLevelFile::MarkChanged). Nothing
//   here writes to it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "selection.h"

class File_Chunk;   // the game's chunk library (mishchnk.hpp)
class Chunk;        // one chunk of it (chunk.hpp)
class Chunk;        // the game's chunk library (chunk.hpp)

namespace host
{
	// The largest payload that's given back. The largest in the game's own levels is a few hundred bytes; the field model shows nothing
	// larger than a few hundred either. A kind the editor doesn't know can be any size, so a larger one is refused as it is.
	size_t const kStrategyLookupMostBytes = 1u << 20;

	enum class StrategyFound
	{
		Found,            // the object's strategy is in *out
		BadCall,          // no place to put the answer, or no ID to look for
		NoLevel,          // no level is open
		NotAnObject,      // what's selected isn't an object (nothing, or a module)
		NoName,           // the ID is all 0
		NoSuchObject,     // no object in the level has that ID
		NoStrategy,       // the object has no strategy: it has no OBJPRJDT, or none with an AVPSTRAT inside
		NoStrategyBytes,  // the strategy chunk has nothing in it to read (an editor-made one not filled in), or is shorter than any strategy
		TooBig,           // the payload is larger than kStrategyLookupMostBytes
		NoMemory,         // there wasn't memory to copy the payload
	};

	struct ObjectStrategy
	{
		ObjectStrategy() : chunk(0) {}

		Chunk *chunk;                  // the AVPSTRAT chunk (an AVP_Strategy_Chunk), good while the level stays open; null unless Found
		std::vector<uint8_t> payload;  // the chunk's bytes after its 12-byte heading: the index and spare number, then the strategy; empty unless Found
	};

	// What an answer means, in plain words for the panel's status line: static text, never null.
	char const *StrategyFoundWords(StrategyFound found);

	// THE LEVEL'S VIRTUAL OBJECTS (VIOBJECT in REBENVDT > SPECLOBJ, virtual_object.h), as the game's chunk classes hold them: each one's name and ID (VOBJPROP) and its VIOBJECT chunk. A virtual
	// object is found by its ID like any object, so it can be selected, listed and its strategy (AVPSTRAT in its VIOBJECT) shown and edited.
	struct VirtualObjectRef
	{
		std::string name;
		uint8_t id[kSelectionIdBytes];
		Chunk *viobject;
	};

	size_t const kMostVirtualObjects = 100000;

	void ListVirtualObjects(File_Chunk *level, std::vector<VirtualObjectRef> *out);

	// Looks for the object whose ID is the 8 bytes at id, in level, and gives its strategy in *out (emptied first, whatever happens).
	StrategyFound FindObjectStrategy(File_Chunk *level, uint8_t const *id, ObjectStrategy *out);

	// The same for what's selected: only an Object is looked for (selected.objectId); Nothing and a Module give NotAnObject.
	StrategyFound FindSelectedStrategy(File_Chunk *level, Selected const &selected, ObjectStrategy *out);
}
