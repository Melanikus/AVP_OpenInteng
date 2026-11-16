// The tables for a level's acoustics: how the game echoes sound in each room. (The light set's own settings join them in the second
// commit.) Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every layout was read from the game's own reader at the pinned AvPx commit (491deb22); paths are under its 3dc folder. The words (the
// kinds of room, the labels) are the old editor's, from a study of its program. A new chunk's starting values are the ones the game's
// chunk library gives, which the old editor shares with the game.
//
// WHAT THE GAME DOES WITH THEM. A level can say how its rooms echo sound: a kind of room, one of 26 (from a padded cell to a hangar to
// underwater), and an amount of echo from 0 to 1. The level says it once for every room (ENVACOUS), and each module may say its own
// (MODACOUS): in a module's chunk a negative kind or a negative amount means "use the level's", and an amount above 1 means "the
// default", which the game keeps as -1, left to distance. The functions at the end of this file are those rules, word for word.

#include <string.h>

#include "acoustics_fields.h"
#include "field_bytes.h"
#include "real_numbers.h"

namespace model
{
	namespace
	{
		char const ModuleKindWhy[] =
			"the game has 26 room echoes, numbered 0 to 25; a negative number means the level's (the chunk library writes -1), and a number "
			"past 25 names no echo, so the game ignores it";
		char const LevelKindWhy[] =
			"the game has 26 room echoes, numbered 0 to 25; a level's number outside that names no echo, so the game ignores it, and a room "
			"that uses it keeps the echo it had";
		char const ModuleReverbWhy[] =
			"a module's reverb of 0 to 1 is its amount of echo, below 0 is the level's and above 1 is the default; the editor keeps to -1 to 2, "
			"which holds all 10,470 modules of the game's own levels";
		char const LevelReverbWhy[] =
			"the level's reverb of 0 to 1 is its amount of echo and above 1 is the default; the editor keeps to 0 to 2, as the old editor's "
			"prompts say, which holds all 6 of the game's own levels that have one";

		char const ModuleKindSource[] =
			"win95/chunk_object.cpp:911, :903; avp/win95/projload.cpp:2387-2390; avp/win95/openal_audio.cpp:1668-1679";
		char const ModuleReverbSource[] =
			"win95/chunk_object.cpp:913, :904, :919 (the loader refuses one that isn't a real number); avp/win95/projload.cpp:2391-2397; "
			"reverb_choice.h:27-30; the old editor's prompts";
		char const ModuleSpareSource[] = "win95/chunk_object.cpp:915, :905";
		char const LevelKindSource[] =
			"win95/environment_chunk.cpp:200, :192; avp/win95/projload.cpp:1968; avp/win95/openal_audio.cpp:1668-1679";
		char const LevelReverbSource[] =
			"win95/environment_chunk.cpp:202, :193, :208 (the loader refuses one that isn't a real number); avp/win95/projload.cpp:1967; "
			"reverb_choice.h:27-30; the old editor's prompts";
		char const LevelSpareSource[] = "win95/environment_chunk.cpp:204, :194";

		// The light set's chunks (the second commit)
		double const LightScaleMost = 16777216.0;   // 2^24

		char const AmbienceWhy[] =
			"full light is 65536 (100%); the game raises a point's light to the ambience if it is below it, so over 65536 pushes light past "
			"full and under 0 does nothing; the editor keeps to 0 to 100%";
		char const LightScaleWhy[] =
			"the game's loader refuses a level whose light scale has a number that isn't real or is beyond 2^24 (16,777,216) either way, and "
			"turns four of the six into whole numbers";

		char const AmbienceSource[] =
			"win95/chunk_light.hpp:225-228; avp/win95/object_setup.cpp:3682; kshape.cpp:5490-5493; avp/win95/system.h:38; the old editor's Ambience line";
		char const LightSetHeaderSource[] = "win95/chunk_light.cpp:64-66; avp/win95/object_setup.cpp:3658-3661";
		char const LightScaleSource[] =
			"win95/chunk_light.cpp:297-331, :309-316; a new one :287-291; labels: cencon's dialog 146 \"Scaling For PSX Lights\" (ccamain.exe); "
			"which box is which float is by their order, INFERRED, not read from code";

		Anchor const S = Anchor::Start;
	}

	// MODACOUS: a module's own acoustics. The game uses it only inside the module's data (MODULEDT, avp/win95/projload.cpp:2384), but it reads and
	// checks one inside any chunk (it's registered with no parent: win95/chunk.hpp:689), so its parent here is "". 12 bytes.
	static Field const ModuleAcousticsFields[] =
	{
		{ "env_index", "Acoustic type", S, 0, Storage::Int32, Unit::Choice, 0, Limit::GameMeaning, -1, 25, ModuleKindWhy, true, -1, ModuleKindSource },
		{ "reverb", "Reverb", S, 4, Storage::Float32, Unit::Plain, 0, Limit::Editor, -1, 2, ModuleReverbWhy, true, -1, ModuleReverbSource },
		{ "spare", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ModuleSpareSource },
	};

	// ENVACOUS: the level's acoustics. The game uses it only inside the level's environment data (REBENVDT, avp/win95/projload.cpp:1964), but it
	// reads and checks one inside any chunk (no parent: win95/chunk.hpp:689), so its parent here is "". 12 bytes. Every module with none of its
	// own uses these.
	static Field const LevelAcousticsFields[] =
	{
		{ "env_index", "Default Type", S, 0, Storage::Int32, Unit::Choice, 0, Limit::GameMeaning, 0, 25, LevelKindWhy, true, 0, LevelKindSource },
		{ "reverb", "Default Reverb", S, 4, Storage::Float32, Unit::Plain, 0, Limit::Editor, 0, 2, LevelReverbWhy, true, 2, LevelReverbSource },
		{ "spare", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, LevelSpareSource },
	};

	// AMBIENCE: the level's ambient light. The game uses it inside the light set (LIGHTSET) but reads one inside any chunk (no parent:
	// win95/chunk_light.cpp:262, win95/chunk.hpp:689). 4 bytes. The game uses it as the least light anywhere: 65536 is full.
	static Field const AmbienceFields[] =
	{
		{ "ambience", "Ambience", S, 0, Storage::Int32, Unit::Fixed16, 0, Limit::Editor, 0, 65536, AmbienceWhy, false, 0, AmbienceSource },
	};

	// LTSETHDR: the light set's header. The game uses it inside the light set (LIGHTSET) but reads one inside any chunk (no parent:
	// win95/chunk_light.cpp:40). 12 bytes: the set's name in the first 8 (a name of the name tables,
	// name_kinds.cpp: "light set name"), then one number the game reads and doesn't use. The game uses the set named NORMALLT.
	static Field const LightSetHeaderFields[] =
	{
		{ "pad", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightSetHeaderSource },
	};

	// LITSCALE: how the light set's light is scaled for another platform. It sits in the light set (LIGHTSET), but the game reads one inside any
	// chunk (no parent: win95/chunk_light.cpp:282). 40 bytes. The game reads it and
	// never uses it. inteng never showed it, but cencon (the old toolset's level-settings tool) did, in dialog 146, "Scaling For PSX Lights",
	// so the six fractions have its words (without the dialog's trailing " :"). Which box is which fraction is by their order, INFERRED:
	// the game never uses this chunk, so a mislabel can't mislead play, only editing. The mode and the spares have no box there, so they're
	// labelled "": the panel skips them and their bytes are kept as they are. Four of the six fractions (each "above" and each "add") are made
	// whole numbers when the game reads them. (Modified 2026-11-15 by Melanikus (Frost): the six labels, Apone's decision.)
	static Field const LightScaleFields[] =
	{
		{ "light_mode", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightScaleSource },
		{ "prelight_multiply", "Prelight: Scale", S, 4, Storage::Float32, Unit::Plain, 0, Limit::Loader, -LightScaleMost, LightScaleMost, LightScaleWhy, true, 1, LightScaleSource },
		{ "prelight_multiply_above", "Prelight: Scale if above", S, 8, Storage::Float32, Unit::Plain, 0, Limit::Loader, -LightScaleMost, LightScaleMost, LightScaleWhy, true, 0, LightScaleSource },
		{ "prelight_add", "Prelight: Add", S, 12, Storage::Float32, Unit::Plain, 0, Limit::Loader, -LightScaleMost, LightScaleMost, LightScaleWhy, true, 0, LightScaleSource },
		{ "runtime_multiply", "Runtime: Scale", S, 16, Storage::Float32, Unit::Plain, 0, Limit::Loader, -LightScaleMost, LightScaleMost, LightScaleWhy, true, 1, LightScaleSource },
		{ "runtime_multiply_above", "Runtime: Scale if above", S, 20, Storage::Float32, Unit::Plain, 0, Limit::Loader, -LightScaleMost, LightScaleMost, LightScaleWhy, true, 0, LightScaleSource },
		{ "runtime_add", "Runtime: Add", S, 24, Storage::Float32, Unit::Plain, 0, Limit::Loader, -LightScaleMost, LightScaleMost, LightScaleWhy, true, 0, LightScaleSource },
		{ "spare1", "", S, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, LightScaleSource },
		{ "spare2", "", S, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, LightScaleSource },
		{ "spare3", "", S, 36, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, LightScaleSource },
	};

	// id, label, parent, leastBytes, fixedBytes, listCountAt, listItemBytes, fields, fieldCount, reader, namesRequired, namesOptional,
	// sizeFieldAt
	ChunkKind const AcousticsKinds[] =
	{
		{ "MODACOUS", "module acoustics", "", 12, 12, -1, 0, ModuleAcousticsFields, sizeof(ModuleAcousticsFields) / sizeof(ModuleAcousticsFields[0]),
		  "win95/chunk_object.cpp:898, :908-923", 0, 0, -1 },
		{ "ENVACOUS", "level acoustics", "", 12, 12, -1, 0, LevelAcousticsFields, sizeof(LevelAcousticsFields) / sizeof(LevelAcousticsFields[0]),
		  "win95/environment_chunk.cpp:187, :197-212", 0, 0, -1 },
		{ "AMBIENCE", "ambience", "", 4, 4, -1, 0, AmbienceFields, sizeof(AmbienceFields) / sizeof(AmbienceFields[0]),
		  "win95/chunk_light.cpp:262, win95/chunk_light.hpp:225-227", 0, 0, -1 },
		{ "LTSETHDR", "light set header", "", 12, 12, -1, 0, LightSetHeaderFields, sizeof(LightSetHeaderFields) / sizeof(LightSetHeaderFields[0]),
		  "win95/chunk_light.cpp:40, :59-67", 0, 0, -1 },
		{ "LITSCALE", "light scale", "", 40, 40, -1, 0, LightScaleFields, sizeof(LightScaleFields) / sizeof(LightScaleFields[0]),
		  "win95/chunk_light.cpp:282, :294-333", 0, 0, -1 },
	};

	size_t const AcousticsKindCount = sizeof(AcousticsKinds) / sizeof(AcousticsKinds[0]);

	// The 26 kinds of room are the same for a module and for the level, in the game's order (inteng-strings.txt lines 2081-2106).
	// A module's chunk also takes -1, "Default": the level's kind.
	ChunkChoiceName const AcousticsChoices[] =
	{
		{ "MODACOUS", "env_index", -1, "Default" },
		{ "MODACOUS", "env_index", 0, "Generic" },
		{ "MODACOUS", "env_index", 1, "Padded Cell" },
		{ "MODACOUS", "env_index", 2, "Room" },
		{ "MODACOUS", "env_index", 3, "Bathroom" },
		{ "MODACOUS", "env_index", 4, "Living Room" },
		{ "MODACOUS", "env_index", 5, "Stone Room" },
		{ "MODACOUS", "env_index", 6, "Auditorium" },
		{ "MODACOUS", "env_index", 7, "Concert Hall" },
		{ "MODACOUS", "env_index", 8, "Cave" },
		{ "MODACOUS", "env_index", 9, "Arena" },
		{ "MODACOUS", "env_index", 10, "Hangar" },
		{ "MODACOUS", "env_index", 11, "Carpeted Hallway" },
		{ "MODACOUS", "env_index", 12, "Hallway" },
		{ "MODACOUS", "env_index", 13, "Stone Corridor" },
		{ "MODACOUS", "env_index", 14, "Alley" },
		{ "MODACOUS", "env_index", 15, "Forest" },
		{ "MODACOUS", "env_index", 16, "City" },
		{ "MODACOUS", "env_index", 17, "Mountains" },
		{ "MODACOUS", "env_index", 18, "Quarry" },
		{ "MODACOUS", "env_index", 19, "Plain" },
		{ "MODACOUS", "env_index", 20, "Parking Lot" },
		{ "MODACOUS", "env_index", 21, "Sewer Pipe" },
		{ "MODACOUS", "env_index", 22, "Underwater" },
		{ "MODACOUS", "env_index", 23, "Drugged" },
		{ "MODACOUS", "env_index", 24, "Dizzy" },
		{ "MODACOUS", "env_index", 25, "Psychotic" },
		{ "ENVACOUS", "env_index", 0, "Generic" },
		{ "ENVACOUS", "env_index", 1, "Padded Cell" },
		{ "ENVACOUS", "env_index", 2, "Room" },
		{ "ENVACOUS", "env_index", 3, "Bathroom" },
		{ "ENVACOUS", "env_index", 4, "Living Room" },
		{ "ENVACOUS", "env_index", 5, "Stone Room" },
		{ "ENVACOUS", "env_index", 6, "Auditorium" },
		{ "ENVACOUS", "env_index", 7, "Concert Hall" },
		{ "ENVACOUS", "env_index", 8, "Cave" },
		{ "ENVACOUS", "env_index", 9, "Arena" },
		{ "ENVACOUS", "env_index", 10, "Hangar" },
		{ "ENVACOUS", "env_index", 11, "Carpeted Hallway" },
		{ "ENVACOUS", "env_index", 12, "Hallway" },
		{ "ENVACOUS", "env_index", 13, "Stone Corridor" },
		{ "ENVACOUS", "env_index", 14, "Alley" },
		{ "ENVACOUS", "env_index", 15, "Forest" },
		{ "ENVACOUS", "env_index", 16, "City" },
		{ "ENVACOUS", "env_index", 17, "Mountains" },
		{ "ENVACOUS", "env_index", 18, "Quarry" },
		{ "ENVACOUS", "env_index", 19, "Plain" },
		{ "ENVACOUS", "env_index", 20, "Parking Lot" },
		{ "ENVACOUS", "env_index", 21, "Sewer Pipe" },
		{ "ENVACOUS", "env_index", 22, "Underwater" },
		{ "ENVACOUS", "env_index", 23, "Drugged" },
		{ "ENVACOUS", "env_index", 24, "Dizzy" },
		{ "ENVACOUS", "env_index", 25, "Psychotic" },
	};

	size_t const AcousticsChoiceCount = sizeof(AcousticsChoices) / sizeof(AcousticsChoices[0]);

	ChunkKind const *FindAcousticsKind(char const *id)
	{
		if (!id)
		{
			return 0;
		}

		for (size_t i = 0; i < AcousticsKindCount; i++)
		{
			if (strncmp(AcousticsKinds[i].id, id, 8) == 0)
			{
				return &AcousticsKinds[i];
			}
		}

		return 0;
	}

	bool AcousticsChoiceIsNamed(char const *chunk, char const *field, int32_t value)
	{
		if (!chunk || !field)
		{
			return false;
		}

		for (size_t i = 0; i < AcousticsChoiceCount; i++)
		{
			if (strncmp(AcousticsChoices[i].chunk, chunk, 8) == 0 && strcmp(AcousticsChoices[i].field, field) == 0 &&
			    AcousticsChoices[i].value == value)
			{
				return true;
			}
		}

		return false;
	}

	bool MakeNewAcousticsChunk(char const *id, uint8_t *buffer, size_t capacity, size_t *length)
	{
		ChunkKind const *const kind = FindAcousticsKind(id);

		if (!kind || !buffer || capacity < kind->fixedBytes)
		{
			return false;
		}

		// Every field needs the library's starting value, and has to lie inside the fixed part, before anything is written
		for (size_t i = 0; i < kind->fieldCount; i++)
		{
			Field const &field = kind->fields[i];

			if (!field.hasCreation || field.offset + StorageBytes(field.storage) > kind->fixedBytes)
			{
				return false;
			}
		}

		memset(buffer, 0, kind->fixedBytes);

		for (size_t i = 0; i < kind->fieldCount; i++)
		{
			detail::WriteCreationAt(kind->fields[i], buffer, kind->fixedBytes, kind->fields[i].offset);
		}

		if (length)
		{
			*length = kind->fixedBytes;
		}

		return true;
	}

	RoomEcho LevelEcho(bool hasChunk, int32_t kind, float reverb)
	{
		// With no chunk the game uses kind 0 and an amount of -1 (avp/win95/projload.cpp:1962-1963)
		RoomEcho echo = { 0, -1.0f };

		if (hasChunk)
		{
			echo.kind = kind;

			// The amount is used only if it is 1 or less (projload.cpp:1967). A NaN is not, by the IEEE rules; it is told by its bits, since
			// /fp:fast doesn't promise the comparison.
			if (!IsNaN(reverb) && reverb <= 1.0f)
			{
				echo.amount = reverb;
			}
		}

		return echo;
	}

	RoomEcho ModuleEcho(RoomEcho level, bool hasChunk, int32_t kind, float reverb)
	{
		// Every module starts with the level's (avp/win95/projload.cpp:2027-2028)
		RoomEcho echo = level;

		if (hasChunk)
		{
			// Its own kind replaces the level's if it is 0 or more (projload.cpp:2387-2390)
			if (kind >= 0)
			{
				echo.kind = kind;
			}

			// Its own amount replaces the level's if it is 0 or more, and above 1 means the default, -1 (projload.cpp:2391-2397).
			// A NaN is not 0 or more, by the IEEE rules; it is told by its bits, since /fp:fast doesn't promise the comparison.
			if (!IsNaN(reverb) && reverb >= 0.0f)
			{
				echo.amount = reverb > 1.0f ? -1.0f : reverb;
			}
		}

		return echo;
	}

	bool RoomKindIsKnown(int32_t kind)
	{
		// The sound side takes the kind as an unsigned number (avp/win95/openal_audio.cpp:1670), so a negative one is past the end too
		return (uint32_t)kind < (uint32_t)RoomKindCount;
	}

	float RoomAmountKept(float amount)
	{
		// reverb_choice.h:27-30. A NaN is told by its bits first: under /fp:fast a comparison isn't promised to fail for it.
		return (!IsNaN(amount) && amount >= 0.0f && amount <= 1.0f) ? amount : -1.0f;
	}
}
