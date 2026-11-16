// See object_base.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "object_base.h"

#include <string.h>

#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "level_file.h"

namespace host
{
	namespace
	{
		uint32_t const BaseObjectFlag = 0x100;      // OBJECT_FLAG_BASE_OBJECT
		uint32_t const ModuleObjectFlag = 0x200;    // OBJECT_FLAG_MODULE_OBJECT
		uint32_t const PlacedObjectFlag = 0x400;    // OBJECT_FLAG_PLACED_OBJECT

		bool FlagsOf(OpenLevelFile *file, Object_Chunk *object, Chunk **header, std::vector<unsigned char> *payload, uint32_t *flags)
		{
			*header = object ? object->get_header() : 0;

			if (!*header || !file->PayloadOf(*header, payload) || payload->size() < 4)
			{
				return false;
			}

			memcpy(flags, payload->data(), 4);
			return true;
		}

		bool PutFlags(OpenLevelFile *file, Chunk *header, std::vector<unsigned char> *payload, uint32_t flags)
		{
			memcpy(payload->data(), &flags, 4);
			return file->SetPayload(header, *payload);
		}
	}

	char const *BaseObjectWords(BaseObjectResult result)
	{
		switch (result)
		{
		case BaseObjectResult::Done: return "done";
		case BaseObjectResult::NothingOpen: return "no level is open";
		case BaseObjectResult::NoName: return "the object has no name to look for";
		case BaseObjectResult::NoSuchObject: return "the level has no object with that ID";
		case BaseObjectResult::IsPlaced: return "a placed object can't be the base module";
		case BaseObjectResult::CantChange: return "an object's header couldn't be changed";
		}

		return "the base module couldn't be set";
	}

	BaseObjectResult SetBaseObject(OpenLevelFile *file, uint8_t const *id, int *changed)
	{
		if (changed)
		{
			*changed = 0;
		}

		File_Chunk *const level = file ? file->Level() : 0;

		if (!level)
		{
			return BaseObjectResult::NothingOpen;
		}

		static uint8_t const none[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		if (!id || memcmp(id, none, 8) == 0)
		{
			return BaseObjectResult::NoName;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		Object_Chunk *selected = 0;

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				selected = each();
				break;
			}
		}

		if (!selected)
		{
			return BaseObjectResult::NoSuchObject;
		}

		// inteng's first test: the selected object's word has 0x400 clear
		Chunk *header = 0;
		std::vector<unsigned char> payload;
		uint32_t flags = 0;

		if (!FlagsOf(file, selected, &header, &payload, &flags))
		{
			return BaseObjectResult::CantChange;
		}

		if (flags & PlacedObjectFlag)
		{
			return BaseObjectResult::IsPlaced;
		}

		int made = 0;

		// then 0x100 comes out of every object's word (the selected one included) ...
		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Chunk *otherHeader = 0;
			std::vector<unsigned char> otherPayload;
			uint32_t otherFlags = 0;

			if (!FlagsOf(file, each(), &otherHeader, &otherPayload, &otherFlags) || !(otherFlags & BaseObjectFlag))
			{
				continue;
			}

			if (!PutFlags(file, otherHeader, &otherPayload, otherFlags & ~BaseObjectFlag))
			{
				return BaseObjectResult::CantChange;
			}

			made++;
		}

		// ... and goes into the selected one's
		if (!FlagsOf(file, selected, &header, &payload, &flags) || !PutFlags(file, header, &payload, flags | BaseObjectFlag))
		{
			return BaseObjectResult::CantChange;
		}

		made++;

		if (changed)
		{
			*changed = made;
		}

		return BaseObjectResult::Done;
	}

	BaseObjectResult SetModuleObjectFlag(OpenLevelFile *file, uint8_t const *id, bool on)
	{
		File_Chunk *const level = file ? file->Level() : 0;

		if (!level)
		{
			return BaseObjectResult::NothingOpen;
		}

		static uint8_t const none[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		if (!id || memcmp(id, none, 8) == 0)
		{
			return BaseObjectResult::NoName;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		Object_Chunk *found = 0;

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				found = each();
				break;
			}
		}

		if (!found)
		{
			return BaseObjectResult::NoSuchObject;
		}

		Chunk *header = 0;
		std::vector<unsigned char> payload;
		uint32_t flags = 0;

		if (!FlagsOf(file, found, &header, &payload, &flags))
		{
			return BaseObjectResult::CantChange;
		}

		if (flags & PlacedObjectFlag)
		{
			return BaseObjectResult::IsPlaced;
		}

		uint32_t const wanted = on ? (flags | ModuleObjectFlag) : (flags & ~ModuleObjectFlag);

		if (wanted == flags)
		{
			return BaseObjectResult::Done;
		}

		return PutFlags(file, header, &payload, wanted) ? BaseObjectResult::Done : BaseObjectResult::CantChange;
	}
}
