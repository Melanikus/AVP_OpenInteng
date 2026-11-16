// hierarchy_collections.h: the SHAPE SET COLLECTIONS of the Hierarchy mode (the HSETCOLL chunks): reading and writing the chunk's bytes, and the rules inteng's
// commands Create / Rename / Delete / Voice Index follow. Hierarchy mode, first data slice. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. A collection is a numbered, named list of shape-set numbers ("alternate shapes") that the game applies together (avpx 3dc/win95/hierarchy_chunk.hpp
// Hierarchy_Shape_Set_Collection_Chunk). Its chunk holds, after the 12-byte heading: the collection's number (4), its name (zero-ended, padded so that name length + 1
// is rounded up to a multiple of 4), a count (4) and that many shape-set numbers (4 each), then the voice index "TypeIndex" (4) and the flags (4)
// (hierarchy_chunk.cpp:515-578). inteng's own commands: AvP-docs\engine\inteng\p11-shape-handlers.md section 59.
//
// THE RULES (a level's bytes are hostile; Tao 1 and 3): every count is checked against the bytes before anything is sized from it; the number must be 0 to 4095
// (the game's Max_Set_Collection_Number); a name has no end inside the chunk is refused; a chunk with bytes left over after the flags is kept as it is by the
// writer (a same-size write changes only the numbers it has).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	int32_t const kMostCollectionNumber = 4095;     // the game's Max_Set_Collection_Number
	size_t const kMostCollectionSets = 1u << 16;    // an editor bound for the list of shape-set numbers (the game has none)
	size_t const kMostCollectionNameBytes = 255;    // an editor bound: inteng's prompt length is not read (named deviation, unfinished-in-inteng.md)

	struct ShapeSetCollection
	{
		int32_t number;                  // the collection's place in the game's list (2 to 18 in the game's own levels)
		std::string name;
		std::vector<int32_t> shapeSets;  // the numbers of the alternate shape sets it applies
		int32_t voiceIndex;              // the chunk's "TypeIndex"
		int32_t flags;                   // bit 0 = AvP_HColl_Flag_NotRandom
	};

	enum class CollectionResult
	{
		Ok,
		BadCall,
		TooShort,          // the payload ends before a field is complete
		BadNumber,         // a number outside 0 to 4095
		BadName,           // a name with no end inside the payload, longer than kMostCollectionNameBytes, or (for a command) empty
		BadCount,          // the count of shape-set numbers does not fit the bytes, or is above kMostCollectionSets
		NameInUse,         // a command's name equals another collection's name (compared ignoring upper and lower case)
		NoSuchCollection,  // a command names a collection that is not in the list
		WrongSize,         // a writer was handed a collection whose bytes would not be the size of the payload it overwrites
		NoSuchShapeSet,    // Add: the number is not one of the level's alternate shape sets (inteng only offers those)
		AlreadyInCollection, // Add: the collection has that shape set (inteng does not offer it)
		EmptyCollection,   // Remove: "This shape collection contains no alternate shape sets"
		NotInCollection,   // Remove: the collection does not have that shape set
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *CollectionWords(CollectionResult result);

	// The payload of one HSETCOLL chunk (after the 12-byte heading) -> the collection. *out is set only when the answer is Ok.
	CollectionResult ReadCollection(uint8_t const *payload, size_t size, ShapeSetCollection *out);

	// The bytes of the collection, as the game's fill_data_block writes them (the payload only). BadName / BadNumber / BadCount when it is not one the reader takes.
	CollectionResult CollectionBytes(ShapeSetCollection const &collection, std::vector<uint8_t> *payload);

	// ----- inteng's commands on the list of the hierarchy's collections (FUN_0048af00, FUN_0047a880, FUN_0047a780, FUN_0047ac10) -----

	// Create (0x53, FUN_0048af00): the new collection's number is the smallest number from 2 up that no collection of the list has (0x48b051-0x48b0cf); its name must
	// not be empty (the prompt asks again, 0x48afc3) and must not equal another collection's name ignoring case (FUN_005909e0, 0x48afd1: "That name is already used");
	// the list of shape sets is empty, the voice index and the flags 0 (the constructor, hierarchy_chunk.cpp:505). *created is the new collection (the list is not
	// changed: the caller adds it).
	CollectionResult CreateCollection(std::vector<ShapeSetCollection> const &list, std::string const &name, ShapeSetCollection *created);

	// Rename (0x57, FUN_0047a880): the collection is taken out of the list first (0x47a98f), then the new name is checked against the others exactly as for Create
	// (non-empty, not equal to another's, ignoring case: 0x47aa18); the other fields stay. A name equal to the collection's own is accepted (it is not among the others).
	CollectionResult RenameCollection(std::vector<ShapeSetCollection> *list, int32_t number, std::string const &newName);

	// Delete (0x54, FUN_0047a780): the collection leaves the list (the confirmation "Delete the current shape set collection?" is the caller's).
	CollectionResult DeleteCollection(std::vector<ShapeSetCollection> *list, int32_t number);

	// Voice Index (0x59, FUN_0047ac10): the collection's voice index becomes the number typed (no range is checked by inteng: 0x47aca8).
	CollectionResult SetCollectionVoiceIndex(std::vector<ShapeSetCollection> *list, int32_t number, int32_t voiceIndex);

	// ----- Add to / Remove from collection (0x55 FUN_00479f20, 0x56 FUN_0047a380; p11-shape-handlers.md section 63) -----

	struct AlternateSetInfo
	{
		int32_t number;      // the OBHALTSH chunk's number (its first 4 bytes)
		std::string name;
	};

	// What Add's chooser ("Add which shape set?") lists: every alternate shape set of the level, in file order, that the collection does NOT already have. *offered empty
	// is inteng's "All shape sets have already been applied".
	CollectionResult OfferedShapeSets(std::vector<AlternateSetInfo> const &alternates, ShapeSetCollection const &collection, std::vector<AlternateSetInfo> *offered);

	// Add: the chosen shape set's number goes on the END of the collection's list (FUN_00405c60 adds at the end); only a set that is offered can be chosen.
	CollectionResult AddShapeSet(std::vector<ShapeSetCollection> *list, int32_t collectionNumber, int32_t setNumber, std::vector<AlternateSetInfo> const &alternates);

	// Remove: a collection with no shape sets answers EmptyCollection; else the FIRST entry equal to the number leaves the list (0x47a675-0x47a6e7), and NotInCollection if there is
	// none. (inteng's chooser lists the sets it has APPLIED to the view for the collection; a number whose alternate no longer exists is not listed there, so it cannot be
	// chosen: here it can be removed, which is the way to clean it up: a named point.)
	CollectionResult RemoveShapeSet(std::vector<ShapeSetCollection> *list, int32_t collectionNumber, int32_t setNumber);

	// Names compared the way FUN_005909e0 does: letter by letter, A to Z folded to a to z; true when they are equal
	bool CollectionNamesEqual(std::string const &a, std::string const &b);
}
