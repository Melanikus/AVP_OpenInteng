// See object_place_edit.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (M-1). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "object_place_edit.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#include <vector>

#include "chunk_object.hpp"   // Object_Chunk, Object_Header_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk, ChunkShape
#include "environment_chunk.hpp"   // Environment_Scale_Chunk (ENVSDSCL)
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf and SetPayload
#include "level_file_name.h"  // LevelFileName_WhyRefused: the game's rule for a name

namespace host
{
	namespace
	{
		// Where things are in an OBJHEAD1 chunk's bytes after its heading (object_place_edit.h)
		size_t const kPlaceAt = 20;
		size_t const kNameAt = 60;
		int64_t const kFarthestPlace = 1 << 24;   // chunk.hpp:492

		// A name's bytes as text: to its first 0, never past the game's 64 bytes
		std::string NameOf(const Object_Chunk &object)
		{
			const char *const name = object.object_data.o_name;
			const void *const zero = memchr(name, 0, sizeof(object.object_data.o_name));
			size_t const length = zero ? (size_t)((const char *)zero - name) : sizeof(object.object_data.o_name);

			return std::string(name, length);
		}

		// The object with this ID, the first in the level's own order, or null
		Object_Chunk *ObjectWithId(File_Chunk *level, const uint8_t *id)
		{
			if (!level || !id)
			{
				return nullptr;
			}

			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					return object;
				}
			}

			return nullptr;
		}

		int32_t Read32(const unsigned char *at)
		{
			int32_t value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		void Write32(unsigned char *at, int32_t value)
		{
			memcpy(at, &value, sizeof(value));
		}

		bool IsHex16(const char *text)
		{
			if (strlen(text) != 16)
			{
				return false;
			}

			for (int i = 0; i < 16; i++)
			{
				if (!isxdigit((unsigned char)text[i]))
				{
					return false;
				}
			}

			return true;
		}

		int HexValue(char letter)
		{
			return letter <= '9' ? letter - '0' : (tolower((unsigned char)letter) - 'a' + 10);
		}

		bool Say(std::string *why, const char *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}
	}

	void ObjectEdit_Ids(File_Chunk *level, std::vector<std::array<uint8_t, 8>> *ids)
	{
		ids->clear();

		if (!level)
		{
			return;
		}

		List<Object_Chunk *> all;

		level->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			if (each())
			{
				std::array<uint8_t, 8> one;

				memcpy(one.data(), &each()->object_data.ID, 8);
				ids->push_back(one);
			}
		}
	}

	bool ObjectEdit_ShapeRadius(File_Chunk *level, const uint8_t *id, double *radius)
	{
		if (!level || !id || !radius)
		{
			return false;
		}

		List<Object_Chunk *> all;
		Object_Chunk *object = nullptr;

		level->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done() && !object; each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				object = each();
			}
		}

		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!shape)
		{
			return false;
		}

		// the first REBENVDT's first ENVSDSCL (FUN_004316e0 finds the lists; +0x28 is its double)
		double scale = 1.0;
		Chunk_With_Children *const envd = dynamic_cast<Chunk_With_Children *>(level->lookup_single_child("REBENVDT"));
		Environment_Scale_Chunk const *const scaleChunk = envd ? dynamic_cast<Environment_Scale_Chunk const *>(envd->lookup_single_child("ENVSDSCL")) : nullptr;

		if (scaleChunk)
		{
			scale = scaleChunk->scale;
		}

		// 0x529a3e-0x529a78: the two differences as whole numbers (the 32-bit wrap of SUB kept), then squared and added as doubles
		ChunkShape const &data = shape->shape_data;
		double const dx = (double)(int32_t)((uint32_t)data.max.x - (uint32_t)data.min.x);
		double const dz = (double)(int32_t)((uint32_t)data.max.z - (uint32_t)data.min.z);

		*radius = sqrt(dx * dx + dz * dz) * scale;
		return true;
	}

	bool ObjectEdit_Find(File_Chunk *level, const OpenLevelFile *store, const char *typed, FoundObject *found)
	{
		if (!level || !typed || !found || !typed[0] || strlen(typed) > kObjectEditTextMost)
		{
			return false;
		}

		uint8_t wanted[8] = {};
		bool const byId = IsHex16(typed);
		std::string lower;

		if (byId)
		{
			for (int i = 0; i < 8; i++)
			{
				wanted[i] = (uint8_t)(HexValue(typed[2 * i]) * 16 + HexValue(typed[2 * i + 1]));
			}
		}
		else
		{
			for (const char *c = typed; *c; c++)
			{
				lower += (char)tolower((unsigned char)*c);
			}
		}

		List<Object_Chunk *> all;
		Object_Chunk *first = nullptr;
		size_t matches = 0;

		level->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object)
			{
				continue;
			}

			bool matched = false;

			if (byId)
			{
				matched = memcmp(&object->object_data.ID, wanted, 8) == 0;
			}
			else
			{
				std::string name = NameOf(*object);

				for (char &c : name)
				{
					c = (char)tolower((unsigned char)c);
				}

				matched = name.find(lower) != std::string::npos;
			}

			if (matched)
			{
				matches++;
				first = first ? first : object;
			}
		}

		if (!first)
		{
			return false;
		}

		found->name = NameOf(*first);
		memcpy(found->id, &first->object_data.ID, 8);
		found->x = first->object_data.location.x;
		found->y = first->object_data.location.y;
		found->z = first->object_data.location.z;
		found->matches = matches;

		// The editor's own bytes say where the object is NOW, if a move has changed them
		std::vector<unsigned char> bytes;
		Object_Header_Chunk *const header = first->get_header();

		if (store && header && store->PayloadOf(header, &bytes) && bytes.size() >= kPlaceAt + 12)
		{
			found->x = Read32(&bytes[kPlaceAt]);
			found->y = Read32(&bytes[kPlaceAt + 4]);
			found->z = Read32(&bytes[kPlaceAt + 8]);
		}

		return true;
	}

	ObjectEditAnswer ObjectEdit_Move(OpenLevelFile *store, File_Chunk *level, const uint8_t *id, long long dx, long long dy, long long dz,
		std::string *why)
	{
		if (!store || !level || !id)
		{
			return ObjectEditAnswer::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return ObjectEditAnswer::NoObject;
		}

		Object_Header_Chunk *const header = object->get_header();
		std::vector<unsigned char> bytes;

		if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < kPlaceAt + 12)
		{
			return ObjectEditAnswer::NoHeader;
		}

		int64_t const now[3] = { Read32(&bytes[kPlaceAt]), Read32(&bytes[kPlaceAt + 4]), Read32(&bytes[kPlaceAt + 8]) };
		// A typed number can be as large as 64 bits: it is checked before it is added, so the sum can't wrap
		long long const by[3] = { dx, dy, dz };
		int64_t moved[3];

		for (int i = 0; i < 3; i++)
		{
			if (by[i] < -2 * kFarthestPlace || by[i] > 2 * kFarthestPlace)
			{
				Say(why, "that is too far: a place is held to 16777216 either way, as the game's loader holds it");
				return ObjectEditAnswer::Refused;
			}

			moved[i] = now[i] + by[i];

			if (moved[i] < -kFarthestPlace || moved[i] > kFarthestPlace)
			{
				Say(why, "that would put the object farther than 16777216 from the middle, which the game's loader refuses");
				return ObjectEditAnswer::Refused;
			}
		}

		for (int i = 0; i < 3; i++)
		{
			Write32(&bytes[kPlaceAt + 4 * (size_t)i], (int32_t)moved[i]);
		}

		if (!store->SetPayload(header, bytes))
		{
			Say(why, "the editor's bytes for the object's header wouldn't take the change");
			return ObjectEditAnswer::Refused;
		}

		return ObjectEditAnswer::Done;
	}

	ObjectEditAnswer ObjectEdit_Rename(OpenLevelFile *store, File_Chunk *level, const uint8_t *id, const char *name, uint8_t *newId,
		std::string *why)
	{
		if (!store || !level || !id || !name || !newId)
		{
			return ObjectEditAnswer::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return ObjectEditAnswer::NoObject;
		}

		Object_Header_Chunk *const header = object->get_header();
		std::vector<unsigned char> bytes;

		if (!header || !store->PayloadOf(header, &bytes) || bytes.size() <= kNameAt)
		{
			return ObjectEditAnswer::NoHeader;
		}

		size_t const length = strlen(name);
		size_t const room = bytes.size() - kNameAt;

		if (length == 0 || length > kObjectEditTextMost)
		{
			Say(why, "a name is 1 to 63 letters");
			return ObjectEditAnswer::Refused;
		}

		if (const char *const refused = LevelFileName_WhyRefused(name))
		{
			Say(why, refused);
			return ObjectEditAnswer::Refused;
		}

		if (length + 1 > room)
		{
			Say(why, "that name is longer than the room the old one leaves in the object's header, and a header never changes its size");
			return ObjectEditAnswer::Refused;
		}

		// No other object has the name already, in capitals or not
		List<Object_Chunk *> all;

		level->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const other = each();

			if (other && other != object && _stricmp(NameOf(*other).c_str(), name) == 0)
			{
				Say(why, "The name of the object has already been used.");
				return ObjectEditAnswer::Refused;
			}
		}

		memset(&bytes[kNameAt], 0, room);
		memcpy(&bytes[kNameAt], name, length);

		if (!store->SetPayload(header, bytes))
		{
			Say(why, "the editor's bytes for the object's header wouldn't take the change");
			return ObjectEditAnswer::Refused;
		}

		// The library's own copy of the name and the ID that comes from it, so the list and the lookups follow (the header chunk's bytes
		// are the editor's and are what a save writes)
		ChunkObject renamed = object->object_data;

		memset(renamed.o_name, 0, sizeof(renamed.o_name));
		memcpy(renamed.o_name, name, length);
		object->update_my_chunkobject(renamed);   // keeps its index, and makes its ID again from the new name (chunk_object.cpp:351)
		memcpy(newId, &object->object_data.ID, 8);
		return ObjectEditAnswer::Done;
	}
}
