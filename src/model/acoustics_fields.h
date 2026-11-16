// The field model's tables for a level's acoustics (how each room echoes sound) and, from the second commit on, for the light set's own
// settings: what each number in these chunks means, where it is, and what values it may take. Written 2026-11-14 by Melanikus for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT THIS IS. field_model.h describes the chunks a mapper edits, and chunk_fields.cpp holds the first tables (lights, sounds and
// markers). The chunks here are described the same way, in tables of their own, so the two files never need to change together. A table
// here has the same shapes as one there (ChunkKind, Field, ChunkChoiceName), so CheckChunkSize, SetChunkField and FindChunkField
// (field_model.h) work on these kinds as they do on chunk_fields.cpp's.
//
// WHERE THE FACTS COME FROM. Every layout and limit was read from the game's own loader at the pinned AvPx commit (491deb22); paths are
// under its 3dc folder. The words are the old editor's (Rebellion's inteng, 2000), from a study of its program. Each table entry says
// where its facts come from.
//
// THE GAME'S RULES FOR A ROOM'S ECHO are at the end of this file as small functions, so the editor can say what a room will sound like
// and a test can hold them to the game's code.
//
// Nothing here reads a file or uses the game's globals. ALL NUMBERS ARE LITTLE-ENDIAN, as the game wrote them on the PC.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "field_model.h"

namespace model
{
	// ---- The tables (chunk_fields_acoustics.cpp), in the shapes of chunk_fields.cpp's ----

	extern ChunkKind const AcousticsKinds[];
	extern size_t const AcousticsKindCount;

	extern ChunkChoiceName const AcousticsChoices[];
	extern size_t const AcousticsChoiceCount;

	// The kind with that id, or 0 if these tables don't hold it (or id is 0). id is the chunk's 8-letter name as a level file holds it:
	// exactly its 8 bytes are compared, and no zero is needed after them (a shorter string ending in a zero simply matches nothing).
	ChunkKind const *FindAcousticsKind(char const *id);

	// Whether value is one of the named values of that chunk's Choice field in these tables (what its dropdown offers). chunk is as for
	// FindAcousticsKind.
	bool AcousticsChoiceIsNamed(char const *chunk, char const *field, int32_t value);

	// Writes the payload of a new chunk of that kind as the game's own chunk library makes one: every field at its creation value. Gives
	// back false, writing nothing, for a kind these tables don't hold, for a kind with a field that has no creation value (the library
	// gives it none), or when capacity is smaller than the kind's fixedBytes; otherwise sets *length (if not 0) to fixedBytes.
	bool MakeNewAcousticsChunk(char const *id, uint8_t *buffer, size_t capacity, size_t *length);

	// ---- The game's rules for a room's echo (chunk_fields_acoustics.cpp) ----

	// The game has 26 kinds of room echo, numbered 0 to 25 (Sound_Enviroments, avp/win95/openal_audio.cpp:482-512).
	int32_t const RoomKindCount = 26;

	// What the game keeps for one room's echo: which of its kinds, and how much of it.
	struct RoomEcho
	{
		int32_t kind;    // the room kind, as the game keeps it (m_sound_env_index). Only 0 to 25 are kinds the game has.
		float amount;    // the amount of echo, as the game keeps it (m_sound_reverb): 0 to 1, or -1, which leaves it to distance; a level's
		                 // chunk can also leave any other number of 1 or less here (projload.cpp:1967), which the sound side reads as -1
	};

	// The level's echo, as the game works it out when it loads the level (avp/win95/projload.cpp:1962-1969). hasChunk says whether the
	// level has an ENVACOUS chunk, and kind and reverb are what it holds.
	//   - With none, the game uses kind 0 and an amount of -1.
	//   - With one, the kind is used as it is, and the amount only if it is 1 or less. Above 1 means "the default", which is -1.
	// A reverb that is NaN is taken as the IEEE rules give it: not 1 or less, so -1, told apart by its bits first (real_numbers.h). The game
	// doesn't promise that: its Release build compares with the fast rules (/fp:fast, win32/VS2026/AvP.vcxproj:199), under which a
	// comparison with a NaN can come out either way.
	// At the pin the loader refuses a level whose reverb isn't a real number (win95/chunk_object.cpp:919, win95/environment_chunk.cpp:208),
	// so a loaded level never brings one here. The NaN rules are kept so each function gives an answer for every number it can be given.
	RoomEcho LevelEcho(bool hasChunk, int32_t kind, float reverb);

	// One module's echo (avp/win95/projload.cpp:2027-2028 gives it the level's, then :2384-2397 lets its own MODACOUS chunk replace each
	// half). hasChunk says whether the module has a MODACOUS chunk, and kind and reverb are what it holds.
	//   - Its kind replaces the level's if it is 0 or more. A negative one leaves the level's.
	//   - Its amount replaces the level's if it is 0 or more, and is -1 (the default) if it is above 1. A negative one leaves the level's.
	// A reverb that is NaN is taken as the IEEE rules give it: not 0 or more, so the level's amount stays. As for LevelEcho, the game's
	// /fp:fast build doesn't promise that comparison, and the loader at the pin refuses such a level.
	RoomEcho ModuleEcho(RoomEcho level, bool hasChunk, int32_t kind, float reverb);

	// Whether the game's sound side has a kind of room with that number. It takes the number as an unsigned one, so a negative number is
	// past the end like any other (avp/win95/openal_audio.cpp:1670). A number it doesn't have is ignored, and the room keeps the echo it has.
	bool RoomKindIsKnown(int32_t kind);

	// The amount of echo the sound side keeps for a room: 0 to 1 as it is, and anything else as -1, which leaves it to distance
	// (reverb_choice.h:27-30, used at avp/win95/openal_audio.cpp:1693). A NaN is taken as -1, as the IEEE rules give it; the game's
	// /fp:fast build doesn't promise that.
	float RoomAmountKept(float amount);
}
