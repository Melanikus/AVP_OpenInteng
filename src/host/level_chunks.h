// level_chunks.h: finding a level's own settings chunks (the module flags, the environment's chunks) in the level that's open. Written
// 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (G, module flags, and L, level settings). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". The twin of strategy_lookup.h, for the chunks that aren't inside an object's strategy.
//
// WHAT IT DOES. A level is a tree of chunks. The settings the editor shows for G and L live in a few places in it:
//   - a module's flags: the object's MODULEDT chunk holds a MODFLAGS chunk;
//   - the level's settings: the level's one REBENVDT chunk holds AVPENVIR, GLOGENDC, ENVSDSCL, SOUNDDIR (one each at most), and any
//     number of RANTEXID, FRAGTYPE and GAMEMODE; a FRAGTYPE holds FRGTYPDC, FRGTYPSC and FRGSOUND; a GAMEMODE holds GMODHEAD.
// These functions walk that tree the way the game's own loader does (projload.cpp, avp_chunk.cpp, chunk_load.cpp) and give back the CHUNK,
// never a cast to the game's class: the edit store (OpenLevelFile::PayloadOf, SetPayload) takes the chunk and holds its bytes.
//
// THE RULES.
// - The level's bytes are hostile. Nothing here changes the level, and a chunk that isn't there is an answer, never a guess.
// - When the game takes "the first" (one AVPENVIR, one module's flags), so do these: two chunks of one name in a hostile file give the
//   FIRST in the level's own order, the one the game's lookup_single_child gives. When the game takes ALL of them (RANTEXID, FRAGTYPE),
//   these give all of them, in the level's own order.
// - A chunk's name is its 8 letters, exactly. Anything but 8 is refused before it's compared, so no short text is read past its end.
// - An object's ID is the 8 bytes the level gives it, as selection.h says; an ID that's all 0 is "no name" and is refused without a search.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "module_links.h"

class File_Chunk;   // the game's chunk library (mishchnk.hpp)
class Chunk;        // the game's chunk library (chunk.hpp)

namespace host
{
	enum class LevelChunkFound
	{
		Found,          // the chunk (or chunks) are in *out
		BadCall,        // no place to put the answer, no name or ID to look for, or a name that isn't 8 letters
		NoLevel,        // no level is open
		NoEnvironment,  // the level has no REBENVDT (the game's loader refuses such a file, so this is only a level made in memory)
		NoneThere,      // the level has no chunk of that name (the game then uses its own defaults, or makes one)
		NotAHolder,     // a chunk that holds no other chunks was asked for its children
		NoName,         // the object ID is all 0
		NoSuchObject,   // no object in the level has that ID
		NotAModule,     // the object has no MODULEDT: it's scenery or a placed object, not a module
	};

	// What an answer means, in plain words for the status line: static text, never null.
	char const *LevelChunkFoundWords(LevelChunkFound found);

	// The first child of the level's REBENVDT whose name is the 8 letters at name (AVPENVIR, GLOGENDC, ENVSDSCL, SOUNDDIR), into *out.
	// *out is null unless Found.
	LevelChunkFound FindEnvironmentChild(File_Chunk *level, char const *name, Chunk **out);

	// The level's own name: its REBENVDT's RIFFNAME, the RIF_Name_Chunk's rif_name (what inteng's Write Asc File puts in its name edit, FUN_004452b0 0x44545f / 0x4457c2), at most
	// `most` letters, kept to printable ASCII (the name is the file's own, hostile input: any other byte becomes '_'). False, with *name empty, when the level has none.
	bool LevelRiffName(File_Chunk *level, size_t most, std::string *name);

	// Every child of the level's REBENVDT with that name (RANTEXID, FRAGTYPE, GAMEMODE), in the level's order, into *out (emptied first).
	// NoneThere, with *out empty, when the level has none.
	LevelChunkFound FindEnvironmentChildren(File_Chunk *level, char const *name, std::vector<Chunk *> *out);

	// Every child of holder with that name, in order, into *out (emptied first): for the chunks under a FRAGTYPE (FRGTYPDC, FRGTYPSC,
	// FRGSOUND) or a GAMEMODE (GMODHEAD, RIFCHILD). A chunk that holds none, or that the library kept as plain bytes, gives NotAHolder.
	LevelChunkFound FindChildren(Chunk *holder, char const *name, std::vector<Chunk *> *out);

	// The MODFLAGS chunk of the module whose object has the ID at id (8 bytes), into *out. An object with no MODULEDT is NotAModule; a
	// module with no MODFLAGS is NoneThere.
	LevelChunkFound FindModuleFlags(File_Chunk *level, uint8_t const *id, Chunk **out);

	// A module's name and ID, as the list of modules without adjacent modules gives them
	struct ModuleWithoutAdjacency
	{
		std::string name;    // the object's name, to its first 0 and within the game's 64 bytes
		uint8_t id[8];       // the object's ID
	};

	// The modules of the level that have no adjacent modules, in the level's own order, into *out (emptied first): an object that is not a
	// placed object (inteng's modules are the objects it keeps in its module list, and a placed object is kept in another) and has no MODULEDT, or
	// a MODULEDT with no ADJMDLEP chunk in it. This is what inteng's Adjacent Modules, Find Not Done lists (FUN_00404a20, command 0x68,
	// menu-reads-1-full.txt:7332-7420, as Vasquez read it in wip-items.md). Gives back how many modules there are in all, 0 for no level.
	// Changes nothing.
	size_t FindModulesWithoutAdjacency(File_Chunk *level, std::vector<ModuleWithoutAdjacency> *out);

	// THE MODULES AS MODULE LINKING SEES THEM (FUN_004c4070, inteng's module list: the objects that are not placed objects), by object index
	// (ChunkObject index_num, the number VMDARRAY and ADJMDLEP name them by): the indexes of all of them into *out (emptied first), a module's index
	// from its 8-byte ID, and its ID from its index. False when no level is open or there is no such module. Change nothing.
	size_t ModuleIndexes(File_Chunk *level, std::set<int32_t> *out);
	bool ModuleIndexOfId(File_Chunk *level, uint8_t const *id, int32_t *index);
	bool ModuleIdOfIndex(File_Chunk *level, int32_t index, uint8_t *id);

	// Whether the level has any hierarchy: a top-level OBJCHIER chunk (inteng's Light Sourcing start looks for them with FUN_004316e0 "OBJCHIER" to choose its bar, FUN_004aa440 0x4aa4d7)
	bool LevelHasHierarchies(File_Chunk *level);

	// The 8-byte ID of the object (any object, not only a module) whose number in the level file is index
	bool ObjectIdOfIndex(File_Chunk *level, int32_t index, uint8_t *id);

	// The items of a module's VMDARRAY (the first one, as the game finds it), as the level holds them, into *out (emptied first): in the game's own
	// struct order. False when the module has no VMDARRAY or the level has no such module.
	bool ReadModuleLinks(File_Chunk *level, int32_t index, std::vector<LinkItem> *out);

	// THE GAME'S NUMBER OF EACH MODULE (MODULE.m_index), by object index, into *out (emptied first): the game numbers modules as it loads the level
	// (projload.cpp:2038-2193, the loop over File_Chunk::list_shapes): for each shape, in the level's order, the objects tied to it (Shape_Chunk::
	// list_assoc_objs), and each one that is not a placed object is the next module (mod_pos, from 0). Its m_index, the index of the game's module array,
	// is mod_pos + 1: the array's first entry is the "infinite module" the loader puts in front (projload.cpp:2012-2019, 2694-2726; module.cpp:744). So the
	// numbers here start at 1. A shape that is an external file holds none (the game asserts so). Gives how many there are, 0 for no level.
	size_t GameModuleNumbers(File_Chunk *level, std::map<int32_t, int32_t> *out);
}
