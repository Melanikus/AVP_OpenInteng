// object_edit.h: take an object out of the open level, or add a copy of one: the data side of inteng's &Delete and &Create (D3, D9-D14).
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (S5). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT WORKS. The editor keeps a level as the game's chunk library holds it, and as bytes (OpenLevelFile). A chunk's numbers are edited
// through the bytes (the edit store); adding or taking away a whole chunk is done the same way, on the bytes of the whole level: the level is
// written out as the next save would write it (BuildLevel), the object's chunk is cut out of those bytes or a copy is put in beside the
// original, and OpenLevelFile::ReplaceLevel then reads the result with the game's own loader, pairs it, and swaps it in. If the game's loader
// wouldn't take it, nothing has changed. After a success every Chunk * the editor held is gone (the level is the new one), and an object is
// named again by its 8-byte ID, which is made from its name, so it stays the same.
//
// THE RULES (an object is an RBOBJECT chunk of the level):
// - A module (an object with a MODULEDT) is never taken out and never copied: other chunks name modules (adjacency, AI, paths), and a copy
//   would be a second room with the first's neighbours.
// - An object is taken out whether or not other objects name it, as inteng's Delete does (Melanikus's ruling 2026-11-15): a name another object's chunk holds
//   (a switch's target, a door, a parent) stays in the level and points at nothing.
// - A copy is of a PLACED object (a switch, a door, a light, a simple object), with a name nobody else has (names make the IDs, so a name
//   taken would be an ID taken), the next free object number, a place the game accepts (CheckPlace: within 2^24 millimetres of the middle),
//   and nobody's lock. Its strategy and every other chunk in it are the original's, byte for byte.
// - What the level's bytes hold is hostile: every size is checked as the bytes are walked, and a level that can't be walked is refused.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class OpenLevelFile;

namespace host
{
	enum class ObjectEditResult
	{
		Done,
		NothingOpen,       // no level is open (or none that's paired with its bytes)
		NoName,            // the ID is null or all 0
		NoSuchObject,      // no object in the level has that ID
		IsAModule,         // the object is a module: neither taken out nor copied
		NotPlaced,         // the object to copy isn't a placed object
		BadName,           // the new name is empty, longer than the game keeps (63 bytes), or has a letter that isn't plain text
		NameTaken,         // another object has that name (letters of either case are one), or no name could be made
		PlaceOutOfRange,   // a coordinate of the place is farther than 2^24 millimetres from the middle
		TooManyObjects,    // no object number is left
		CantBuild,         // the level couldn't be put together from the bytes (not walkable, or too big)
		LoaderRefused,     // the game's own loader wouldn't take the changed level, so nothing was changed
		NoStrategy,        // the object has no strategy (to take out or to copy)
		BadStrategy,       // the editor can't make a strategy of that kind
		SameObject,        // the strategy would be copied onto its own object
		NotAModule,        // the object has no MODULEDT (links are a module's)
		BadLinks,          // the links given are more than a VMDARRAY holds, or none
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *ObjectEditWords(ObjectEditResult result);

	// Takes the object whose 8-byte ID is id out of the open level.
	ObjectEditResult DeleteObject(OpenLevelFile *file, uint8_t const *id);

	// Adds a copy of the object whose ID is `like`, beside it. name is its new name (null or empty: the original's with a number, "_2", "_3"...);
	// place is its place, three whole numbers in millimetres (null: the original's). Gives its ID in newId (8 bytes), if newId isn't null.
	ObjectEditResult CreateObjectLike(OpenLevelFile *file, uint8_t const *like, char const *name, int32_t const *place, uint8_t *newId);

	// Gives the object whose ID is id a new name, the level made again with its header (OBJHEAD1) written at the size the game's library writes it for that name (72 bytes and
	// the name with its ending zero, to a whole 4), as a structural edit through ReplaceLevel, as create and delete are: every Chunk * handed out before is gone, and
	// Generation() is one more. The name is not judged here (ObjectEdit_Rename does). Gives its new ID in newId (8 bytes), if newId isn't null. (Added 2026-11-17 by Drake:
	// the rename wrote the name into the old header's room, so the header's bytes and the library's chunk were of different sizes and the level stopped pairing after a
	// save, strategy-data.md S-35a.)
	ObjectEditResult RenameObject(OpenLevelFile *file, uint8_t const *id, char const *name, uint8_t *newId);

	// THE STRATEGY OF AN OBJECT (strategy mode: Assign, Copy and Delete Strategy; F). The same way as above, on the bytes of the whole level:
	// the object's AVPSTRAT chunk, inside its OBJPRJDT, is cut out, put in, or replaced, the sizes of the chunks around it are made again,
	// and the game's loader reads the result before anything is swapped in. After a success every Chunk * the editor held is gone (as above),
	// and the object's ID is the same.
	// - Only a PLACED object has a strategy to give, take or copy: a module is refused (IsAModule), any other object (NotPlaced).
	// - A strategy is replaced or taken out whether or not other objects name the object (a door a switch opens), as inteng's Copy / Assign / Delete do: the names stay
	//   and the game follows them by name, null-safe and checking the kind (strategy-data-kinds.md S-KC-15).
	// - A copy is of the strategy's bytes as the level holds them now (edits made in the panel included), the target's own is replaced, and it
	//   names what the original names: its targets stay the original's.

	// Takes the strategy out of the object with that ID. An OBJPRJDT left with nothing in it goes too.
	ObjectEditResult DeleteStrategy(OpenLevelFile *file, uint8_t const *id);

	// Gives the object a new strategy of that kind (a kind the old editor offered: model::MakeNewStrategy, with its creation values), in
	// place of the one it has, if any.
	ObjectEditResult AssignStrategy(OpenLevelFile *file, uint8_t const *id, int32_t kind);

	// Gives the object `to` a copy of the strategy of the object `from`, in place of its own.
	ObjectEditResult CopyStrategy(OpenLevelFile *file, uint8_t const *from, uint8_t const *to);

	// Puts these payload bytes (the AVPSTRAT chunk's, after its 12-byte heading) in place of the object's strategy, or in as its strategy: an edit whose size changes (a Lift's lists). The object keeps a
	// strategy; the game's loader reads the result before it is swapped in, as for the others. Refused above 64 KB.
	ObjectEditResult SetStrategyBytes(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length);

	// THE LINKS OF A MODULE (Module Linking; P5): the module's VMDARRAY, inside its MODULEDT, is replaced by one holding these items (the file's order:
	// object index, branch number, flags, spare), or put in when the module has none. As inteng writes it (FUN_004c10c0, menu-reads-4.txt): only
	// when there is at least one item, so no items is refused (BadLinks). The sizes of the chunks around it are made again and the game's loader reads
	// the result before anything is swapped in, as for the strategy.
	struct ModuleLinkItem
	{
		int32_t objectIndex;
		int32_t branchNo;
		int32_t flags;
		int32_t spare;
	};

	ObjectEditResult SetModuleLinks(OpenLevelFile *file, uint8_t const *id, ModuleLinkItem const *items, size_t count);

	// The same for several modules in ONE new image and ONE loader read (Add Reverse Links touches hundreds): each edit names its module by its 8-byte ID. A
	// module with no MODULEDT or with no items is skipped, and so is a second edit of the same module; *written (when not null) says per edit whether it went
	// in. Nothing is changed unless the game's loader takes the whole image.
	struct ModuleLinksFor
	{
		uint8_t id[8];
		std::vector<ModuleLinkItem> items;
	};

	ObjectEditResult SetModuleLinksMany(OpenLevelFile *file, std::vector<ModuleLinksFor> const &edits, std::vector<bool> *written);

	// A MODULE'S ADJACENCY (the ADJMDLEP chunk of its MODULEDT: a count, then per entry the neighbour's object index, flags and entry point, 20 bytes each; chunk_object.cpp:753),
	// by object index, for the adjacency generator (adjacency_generate.h). Added 2026-11-15 by Melanikus (Drake).
	struct ModuleAdjacencyEntry
	{
		int32_t objectIndex;
		int32_t flags;
		int32_t point[3];
	};

	// One object as the level's image holds it: for the modules, the first ADJMDLEP and the first VMDARRAY
	struct ModuleSnapshot
	{
		int32_t index;
		bool module;
		bool hasAdjacency;
		std::vector<ModuleAdjacencyEntry> adjacency;
		bool hasLinks;
		std::vector<ModuleLinkItem> links;
		bool isMaster;                  // its MODULEDT has an AIMODMAS (an AI module's master)
		int32_t masterIndex;            // its AIMODSLA's master module, or -1 when it has none
		bool hasWaypoint;               // its MODULEDT has a WAYPOINT
		std::vector<uint8_t> waypoint;  // that chunk, whole, with its heading
	};

	// Every object of the level in the file's order, read from the image the next save would write. Changes nothing. A chunk that is not a whole ADJMDLEP / VMDARRAY
	// (a count that does not fit its size) is read as not there.
	ObjectEditResult ReadModuleSnapshots(OpenLevelFile *file, std::vector<ModuleSnapshot> *out);

	// Puts a module's adjacency in, as a NEW chunk at the end of its MODULEDT (inteng makes one this way: the old chunk is deleted first), or takes it out (remove).
	// Several modules in ONE new image and ONE loader read; a module is given by its object index; an object that is not a module, an unknown index and a second edit of
	// the same module are skipped (*written false for them). Nothing is changed unless the game's loader takes the whole image.
	struct ModuleAdjacencyFor
	{
		int32_t objectIndex;
		bool remove;
		std::vector<ModuleAdjacencyEntry> entries;
	};

	ObjectEditResult SetModuleAdjacencyMany(OpenLevelFile *file, std::vector<ModuleAdjacencyFor> const &edits, std::vector<bool> *written);

	// MODULE CHILD EDITS (the AI module chunks AIMODMAS / AIMODSLA and the WAYPOINT chunk, chunk_waypoint.cpp): in the module's MODULEDT the first chunk with that name is taken
	// out and, unless remove, the new one (given whole, with its 12-byte heading and a size that fits) is put at the END (as the game's tools make a chunk). An object may have
	// several edits: they are applied in the order given. Everything in ONE new image and ONE loader read; an edit for an unknown index or an object with no MODULEDT is
	// skipped (*written false); nothing is changed unless the game's loader takes the whole image.
	struct ModuleChildEdit
	{
		int32_t objectIndex;
		char name[9];                 // the chunk's 8 letters and a zero
		bool remove;
		std::vector<uint8_t> chunk;   // the whole new chunk (not used when remove)
	};

	ObjectEditResult SetModuleChildrenMany(OpenLevelFile *file, std::vector<ModuleChildEdit> const &edits, std::vector<bool> *written);

	// The same for the chunks directly inside an OBJECT (an RBOBJECT: a module or a placed object), as the light save makes a shape's SHPVTINT: every child of the object named `name` whose payload starts with
	// `payloadStarts` (up to 8 letters; empty: any) is taken out and, unless remove, the new one (given whole, with its 12-byte heading and a size that fits) is put at the END of the object's children, as the game's
	// tools make a chunk (a Chunk added to its parent goes last). Everything in ONE new image and ONE loader read; an edit for an unknown index is skipped (*written false); nothing is changed unless the game's loader
	// takes the whole image.
	struct ObjectChildEdit
	{
		int32_t objectIndex;
		char name[9];
		char payloadStarts[9];
		bool remove;
		std::vector<uint8_t> chunk;
	};

	ObjectEditResult SetObjectChildrenMany(OpenLevelFile *file, std::vector<ObjectChildEdit> const &edits, std::vector<bool> *written);

	// DESTROYS a module's VMDARRAY (its links), as Delete Links does (inteng makes the module's chunk go with the loaded tree there). Used when an EMPTY link tree is written: inteng takes the
	// module flag out and leaves the old VMDARRAY (a module with links and no flag, drake-deviations.md); here the chunk goes too. *dropped says whether the module had one. Nothing is
	// changed unless the game's loader takes the new image.
	ObjectEditResult DropModuleLinks(OpenLevelFile *file, int32_t objectIndex, bool *dropped);
}
