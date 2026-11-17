// light_paste.cpp: see light_paste.h. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "light_paste.h"

#include <string.h>

#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_store.h"
#include "light_edit.h"
#include "object_make.h"
#include "strategy_edit.h"

namespace host
{
	namespace
	{
		int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
		{
			int32_t value;
			memcpy(&value, bytes.data() + at, 4);
			return value;
		}

		void AddInt(std::vector<char> &bytes, int32_t value)
		{
			char part[4];
			memcpy(part, &value, 4);
			bytes.insert(bytes.end(), part, part + 4);
		}

		// The level's first object with that ID (the list and a pick find it too)
		Object_Chunk *ObjectOf(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> all;
			level->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					return each();
				}
			}

			return nullptr;
		}

		// The size of a Placed Object (kind 62) strategy's bytes after the AVPSTRAT heading, and of a PLOBJLIT's light (Placed_Object_Light_Data: 15 whole numbers)
		size_t const kPlacedObjectBytes = 52;
		size_t const kPlacedLightWords = 15;
	}

	LightPasteResult PastePlacedLight(OpenLevelFile *file, uint8_t const *sourceId, std::string const &name, int32_t const *place, int32_t const *matrix, uint8_t *newId, std::string *why)
	{
		if (why)
		{
			why->clear();
		}

		if (!file || !file->Level() || !sourceId)
		{
			return LightPasteResult::NothingOpen;
		}

		File_Chunk *const level = file->Level();
		Object_Chunk *const source = ObjectOf(level, sourceId);

		if (!source || !source->get_header())
		{
			return LightPasteResult::NoSuchObject;
		}

		// the copied object's strategy as the editor holds it now (an edit is in it): a Placed Object, kind 62 (0x4abb95), measured before it is read
		LevelStore const store(file);
		ObjectStrategy strategy;

		if (LookUpStrategyBytes(level, store, sourceId, &strategy) != StrategyFound::Found || strategy.payload.size() < kPlacedObjectBytes || IntAt(strategy.payload, 8) != 62)
		{
			return LightPasteResult::NotPlacedObject;
		}

		// its light: the PLOBJLIT's 15 numbers, as the editor holds them now
		Chunk *const lightChunk = FindPlacedLightChunk(level, sourceId);
		std::vector<unsigned char> light;

		if (!lightChunk || !store.PayloadOf(lightChunk, &light) || light.size() < kPlacedLightWords * 4)
		{
			return LightPasteResult::NoPlacedLight;
		}

		// the shape the copied object uses, by its number (FUN_004d0fd0 / FUN_004d0ff0: the new object is associated with it): its OBJHEAD1's own word at +56 (the flags, the 16 letters of the lock
		// user, the place, the turn, the number and the version_no come first: object_make.h)
		std::vector<unsigned char> head;

		if (!store.PayloadOf(source->get_header(), &head) || head.size() < 60)
		{
			return LightPasteResult::NoSuchObject;
		}

		ObjectParts parts;

		parts.shapeNumber = IntAt(head, 56);

		// the header: the placed bit (0x4abca1), and the copied object's load bits (INTENG BUG, FIXED: light_paste.h)
		parts.headerFlags = 0x400u | ((uint32_t)source->get_header()->flags & kPastedLightLoadBits);

		// the new kind 62 strategy (FUN_0052b120(0x3e)): its heading as a new strategy's (object_make.h's), Type, ExtraData and mass copied, the integrity copied with bit 31 put in,
		// flags 1, the rest 0 (0x4abd3d-0x4abd85: the class's words 5..12 are the bytes +20..+48)
		AddInt(parts.strategy, 0);                                         // index
		AddInt(parts.strategy, 0);                                         // spare
		AddInt(parts.strategy, 62);                                        // the kind
		AddInt(parts.strategy, 0);
		AddInt(parts.strategy, 0);
		AddInt(parts.strategy, IntAt(strategy.payload, 20));               // Type
		AddInt(parts.strategy, IntAt(strategy.payload, 24));               // ExtraData
		AddInt(parts.strategy, IntAt(strategy.payload, 28));               // mass
		AddInt(parts.strategy, (int32_t)((uint32_t)IntAt(strategy.payload, 32) | 0x80000000u));   // integrity, bit 31 in
		AddInt(parts.strategy, 1);                                         // flags
		AddInt(parts.strategy, 0);
		AddInt(parts.strategy, 0);
		AddInt(parts.strategy, 0);

		// the PLOBJLIT (Placed_Object_Light_Chunk::fill_data_block): its heading, the 15 numbers, and the count of extra words, 0 (the class's +0x64 / +0x68 left 0)
		std::vector<char> &plobjlit = parts.after;
		char const heading[] = "PLOBJLIT";

		plobjlit.insert(plobjlit.end(), heading, heading + 8);
		AddInt(plobjlit, (int32_t)(12 + kPlacedLightWords * 4 + 4));
		plobjlit.insert(plobjlit.end(), light.begin(), light.begin() + (std::ptrdiff_t)(kPlacedLightWords * 4));
		AddInt(plobjlit, 0);

		// its local flags (the 7th number) with 4 (changed) in: the new light's record takes the copied one's flags | 4 (0x4abf6b), and the write-back writes them into the PLOBJLIT (FUN_004ad630
		// 0x4adcfe); the PLOBJLIT is made here at once, so they go in now
		size_t const localFlagsAt = 12 + 6 * 4;
		int32_t localFlags;

		memcpy(&localFlags, plobjlit.data() + localFlagsAt, 4);
		localFlags |= 4;
		memcpy(plobjlit.data() + localFlagsAt, &localFlags, 4);

		std::string made;
		ObjectMakeResult const result = CreateObjectFromParts(file, name.c_str(), place, matrix, parts, newId, &made);

		if (result != ObjectMakeResult::Done)
		{
			if (why)
			{
				*why = ObjectMakeWords(result);
			}

			return LightPasteResult::MakeFailed;
		}

		return LightPasteResult::Done;
	}

	char const *LightPasteWords(LightPasteResult result)
	{
		switch (result)
		{
		case LightPasteResult::Done: return "done";
		case LightPasteResult::NothingOpen: return "no level is open";
		case LightPasteResult::NoSuchObject: return "the copied light's object is no longer in the level";
		case LightPasteResult::NotPlacedObject: return "the copied light's object holds no Placed Object strategy";
		case LightPasteResult::NoPlacedLight: return "the copied object holds no light";
		case LightPasteResult::MakeFailed: return "the new object couldn't be made";
		}

		return "unknown answer";
	}

	std::string LightPasteFirstName(OpenLevelFile *file)
	{
		if (!file || !file->Level())
		{
			return std::string();
		}

		List<Object_Chunk *> all;
		file->Level()->list_objects(&all);

		for (int n = 0; n < 1000000; n++)
		{
			std::string const candidate = "Light" + std::to_string(n);
			bool taken = false;

			for (LIF<Object_Chunk *> each(&all); !each.done() && !taken; each.next())
			{
				taken = each() && each()->object_data.o_name && _stricmp(each()->object_data.o_name, candidate.c_str()) == 0;
			}

			if (!taken)
			{
				return candidate;
			}
		}

		return std::string();
	}
}
