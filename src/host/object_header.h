// object_header.h: an object's header, as the game loaded it from the level (its OBJHEAD1 chunk), and the field panel's record of it: the
// header half of the old editor's "Edit object properties" (dialog 129, coverage D1). Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT SHOWS. Of everything in the header, dialog 129 shows only the bits of its flags that it names (vasquez-ui.md 2.1), so the
// record has six boxes, read-only, in that dialog's order:
// - "Game mode": Marine, Alien, Predator. The bits are OBJECT_FLAG_AVPGAMEMODEMARINE, ALIEN and PREDATOR (avpx/3dc/win95/chunk_object.hpp:25-27).
//   The game reads them, with OBJECT_FLAG_PCLOAD, for the simple objects and for an object with no strategy
//   (avpx/3dc/avp/win95/object_setup.cpp:2459, :2471, :2507).
// - "Difficulty": Easy, Medium, Hard. The file keeps the OPPOSITE: OBJECT_FLAG_NOTDIFFICULTY1, 2 and 3 (chunk_object.hpp:34-36) mean "not
//   at this difficulty" (object_setup.cpp:659-664). So a difficulty's box is ticked when its bit is CLEAR: ticked means the object is in
//   the game at that difficulty. The edit path will have to flip it back.
// Nothing else in the header (its place, its turn, its numbers, the lock user, the other flags) has a label in dialog 129, so none of it
// is shown. Inspect changes nothing.
//
// HOSTILE BYTES. The name is the level's own. It's copied from the game's 64-byte o_name (chunk_type.hpp:217) to its first 0, never past
// those 64 bytes, and the field panel host reads the title only through the safe text (field_panel_host.h). The flags are a plain
// number: each box shows one bit of it.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "field_record.h"
#include "selection.h"   // kSelectionIdBytes

namespace host
{
	// The header's flag bits dialog 129 shows (avpx/3dc/win95/chunk_object.hpp:25-27 and :34-36)
	uint32_t const kHeaderMarine = 0x00000800;
	uint32_t const kHeaderAlien = 0x00001000;
	uint32_t const kHeaderPredator = 0x00002000;
	uint32_t const kHeaderNotEasy = 0x00100000;
	uint32_t const kHeaderNotMedium = 0x00200000;
	uint32_t const kHeaderNotHard = 0x00400000;

	// The bytes the game keeps for an object's name, its 0 included (OBJECT_NAME_BYTES, avpx/3dc/win95/chunk_object_name.hpp:13)
	size_t const kHeaderNameBytes = 64;

	// One object's header, as the game loaded it. It's a copy, so it stays good after the level closes.
	struct ObjectHeader
	{
		std::string name;                 // the name's bytes up to its first 0, at most kHeaderNameBytes of them (the level's own)
		uint8_t id[kSelectionIdBytes];    // the object's ID, the game's ObjectID
		uint32_t flags;                   // the header's flags, as the file holds them
	};

	// Builds the record of one object's header in *record (what it held before is let go of): the name as the title (empty when the
	// object has none: the adapter then gives it the ID) and the six boxes, read-only, in dialog 129's order. Gives back false for no
	// record, and false with an empty record if memory runs out.
	bool BuildObjectHeaderRecord(const ObjectHeader &header, FieldRecord *record);

	// Adds the six boxes to a record that already holds something (a strategy's record, D2), after what's there, without emptying it or
	// changing its title. Gives back false for no record, and false with an EMPTY record if memory runs out.
	bool AddObjectHeaderBoxes(const ObjectHeader &header, FieldRecord *record);
}
