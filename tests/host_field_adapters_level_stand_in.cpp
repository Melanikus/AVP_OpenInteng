// A stand-in for the game side's LevelObjects_FindHeader and LevelObjects_Level (level_objects.cpp) and for Drake's FindSelectedStrategy
// (strategy_lookup.cpp), for host_field_adapters_test only: a level of four objects held here (a door, an object with no name, one with a
// strategy, and one whose strategy can't be read), so the adapters are tested without the game. Written 2026-11-14 by Melanikus (Dietrich)
// for AVP_OpenInteng (D1 and D2). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <string.h>

#include "level_objects.h"
#include "light_edit.h"
#include "field_model.h"
#include "field_edit.h"
#include "level_chunks.h"
#include "strategy_edit.h"
#include "strategy_lookup.h"

bool LevelObjects_FindHeader(const uint8_t *id, host::ObjectHeader *header)
{
	// "DOOR0001": a door in the Marine game, loaded on the PC, and not at Easy
	static uint8_t const door[host::kSelectionIdBytes] = { 0x44, 0x4F, 0x4F, 0x52, 0x30, 0x30, 0x30, 0x31 };
	// "NONAME01": an object with no name and no flags
	static uint8_t const nameless[host::kSelectionIdBytes] = { 0x4E, 0x4F, 0x4E, 0x41, 0x4D, 0x45, 0x30, 0x31 };
	// "STRAT001": an object with a strategy (a new Door, below), in the Marine game and loaded on the PC
	static uint8_t const withStrategy[host::kSelectionIdBytes] = { 0x53, 0x54, 0x52, 0x41, 0x54, 0x30, 0x30, 0x31 };
	// "BADSTRAT": an object whose strategy is of a kind the editor doesn't know
	static uint8_t const broken[host::kSelectionIdBytes] = { 0x42, 0x41, 0x44, 0x53, 0x54, 0x52, 0x41, 0x54 };

	if (!id || !header)
	{
		return false;
	}

	if (memcmp(id, door, sizeof(door)) == 0)
	{
		header->name = "door_main";
		memcpy(header->id, id, sizeof(door));
		header->flags = 0x00000800 | 0x00008000 | 0x00100000;
		return true;
	}

	if (memcmp(id, nameless, sizeof(nameless)) == 0)
	{
		header->name.clear();
		memcpy(header->id, id, sizeof(nameless));
		header->flags = 0;
		return true;
	}

	if (memcmp(id, withStrategy, sizeof(withStrategy)) == 0)
	{
		header->name = "lever_a";
		memcpy(header->id, id, sizeof(withStrategy));
		header->flags = 0x00000800 | 0x00008000;
		return true;
	}

	if (memcmp(id, broken, sizeof(broken)) == 0)
	{
		header->name = "broken_one";
		memcpy(header->id, id, sizeof(broken));
		header->flags = 0;
		return true;
	}

	return false;
}

// The light lookup the adapters ask for a selected light (light_edit.h): this test has no light set, so it finds none
Chunk *host::FindLightChunk(File_Chunk *, size_t)
{
	return 0;
}

File_Chunk *LevelObjects_Level()
{
	// The stand-in's level is the tables here, not a file
	return nullptr;
}

namespace host
{
	// A stand-in for LookUpStrategyBytes (strategy_edit.cpp), for the editable form of the adapters: the store holds nothing here, so it is the
	// same strategy FindSelectedStrategy gives
	StrategyFound FindSelectedStrategy(File_Chunk *, Selected const &selected, ObjectStrategy *out);

	// Stand-ins for the finders of the module's flags and the level's settings (level_chunks.cpp): this level has none
	LevelChunkFound FindModuleFlags(File_Chunk *, uint8_t const *, Chunk **out)
	{
		if (out)
		{
			*out = 0;
		}

		return LevelChunkFound::NoLevel;
	}

	// The level's settings: this stand-in has one, the scale (ENVSDSCL), as a chunk the test's store knows by its address (kScaleChunk); the others aren't there
	extern Chunk *const kScaleChunk;
	Chunk *const kScaleChunk = reinterpret_cast<Chunk *>(static_cast<uintptr_t>(0x5C41E));

	LevelChunkFound FindEnvironmentChild(File_Chunk *, char const *name, Chunk **out)
	{
		if (out)
		{
			*out = 0;
		}

		if (name && strcmp(name, "ENVSDSCL") == 0 && out)
		{
			*out = kScaleChunk;
			return LevelChunkFound::Found;
		}

		return LevelChunkFound::NoLevel;
	}

	StrategyFound LookUpStrategyBytes(File_Chunk *level, PayloadStore const &, uint8_t const *id, ObjectStrategy *out)
	{
		Selected selected = {};

		selected.kind = SelectedKind::Object;
		memcpy(selected.objectId, id, kSelectionIdBytes);
		return FindSelectedStrategy(level, selected, out);
	}

	// A stand-in for Drake's lookup (strategy_lookup.cpp): STRAT001 has a new Door (50) as its strategy, made as the old editor made one;
	// BADSTRAT has 24 bytes of a kind numbered 9999, which the editor doesn't know; every other object has none.
	StrategyFound FindSelectedStrategy(File_Chunk *, Selected const &selected, ObjectStrategy *out)
	{
		static uint8_t const withStrategy[kSelectionIdBytes] = { 0x53, 0x54, 0x52, 0x41, 0x54, 0x30, 0x30, 0x31 };
		static uint8_t const broken[kSelectionIdBytes] = { 0x42, 0x41, 0x44, 0x53, 0x54, 0x52, 0x41, 0x54 };

		if (!out)
		{
			return StrategyFound::BadCall;
		}

		out->chunk = 0;
		out->payload.clear();

		if (selected.kind != SelectedKind::Object)
		{
			return StrategyFound::NotAnObject;
		}

		if (memcmp(selected.objectId, withStrategy, kSelectionIdBytes) == 0)
		{
			uint8_t bytes[1024];
			size_t length = 0;

			if (!model::MakeNewStrategy(50, bytes, sizeof(bytes), &length))
			{
				return StrategyFound::NoStrategyBytes;
			}

			out->payload.assign(bytes, bytes + length);
			return StrategyFound::Found;
		}

		if (memcmp(selected.objectId, broken, kSelectionIdBytes) == 0)
		{
			out->payload.assign(24, 0);
			out->payload[8] = 0x0F;   // 9999, little-endian, at +8: the kind's number
			out->payload[9] = 0x27;
			return StrategyFound::Found;
		}

		return StrategyFound::NoStrategy;
	}
}
