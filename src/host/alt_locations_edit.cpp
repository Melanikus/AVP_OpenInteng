// See alt_locations_edit.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (P9). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "alt_locations_edit.h"

#include <string.h>

#include <exception>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk (object_data)
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "level_file.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Bytes;

		size_t const HeadingBytes = 12;   // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const NameAt = 60;         // in an object header's payload: the name starts here, after 60 bytes of numbers
		size_t const MostNameBytes = 63;

		int32_t IntAt(Bytes const &bytes, size_t at)
		{
			int32_t value;
			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Bytes &bytes, size_t at, int32_t value)
		{
			memcpy(bytes.data() + at, &value, sizeof(value));
		}

		// The size of the chunk at `at`, inside [at, end), or false when its heading isn't there or its size isn't a size that fits
		bool ChunkSizeAt(Bytes const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < HeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)HeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		bool IdIsAllZero(uint8_t const *id)
		{
			for (size_t i = 0; i < 8; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		// The name of the object whose ID this is, from the game's library of the open level: how the object is found among the bytes
		bool NameOfObject(OpenLevelFile *file, uint8_t const *id, std::string *name)
		{
			File_Chunk *const level = file->Level();

			if (!level)
			{
				return false;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					size_t length = 0;

					while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
					{
						length++;
					}

					name->assign(object->object_data.o_name, length);
					return true;
				}
			}

			return false;
		}

		struct Place
		{
			size_t objectAt;      // the object's RBOBJECT chunk
			size_t objectSize;
			bool hasAlt;
			size_t altAt;         // its first ALTLOCAT chunk, when it has one
			size_t altSize;
		};

		// The object with this name among the level's own (top-level) RBOBJECT chunks, and its first ALTLOCAT. False when the bytes can't be walked.
		AltLocationsResult FindObject(Bytes const &bytes, std::string const &name, Place *place)
		{
			if (bytes.size() < HeadingBytes || !NameIs(bytes, 0, "REBINFF2"))
			{
				return AltLocationsResult::CantBuild;
			}

			for (size_t at = HeadingBytes; at < bytes.size(); )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, bytes.size(), &size))
				{
					return AltLocationsResult::CantBuild;
				}

				if (NameIs(bytes, at, "RBOBJECT"))
				{
					bool named = false;
					Place found = { at, size, false, 0, 0 };

					for (size_t inner = at + HeadingBytes; inner < at + size; )
					{
						size_t innerSize = 0;

						if (!ChunkSizeAt(bytes, inner, at + size, &innerSize))
						{
							return AltLocationsResult::CantBuild;
						}

						if (NameIs(bytes, inner, "OBJHEAD1") && innerSize > HeadingBytes + NameAt)
						{
							char const *const first = bytes.data() + inner + HeadingBytes + NameAt;
							size_t const room = innerSize - HeadingBytes - NameAt;
							char const *const zero = (char const *)memchr(first, 0, room);

							if (zero && (size_t)(zero - first) <= MostNameBytes && std::string(first, (size_t)(zero - first)) == name)
							{
								named = true;
							}
						}
						else if (NameIs(bytes, inner, "ALTLOCAT") && !found.hasAlt)
						{
							found.hasAlt = true;
							found.altAt = inner;
							found.altSize = innerSize;
						}

						inner += innerSize;
					}

					if (named)
					{
						*place = found;
						return AltLocationsResult::Done;
					}
				}

				at += size;
			}

			return AltLocationsResult::NoSuchObject;
		}

		AltLocationsResult Prepare(OpenLevelFile *file, uint8_t const *id, Bytes *image, Place *place)
		{
			if (!file || !file->Level())
			{
				return AltLocationsResult::NothingOpen;
			}

			if (!id || IdIsAllZero(id))
			{
				return AltLocationsResult::NoName;
			}

			std::string name;

			if (!NameOfObject(file, id, &name))
			{
				return AltLocationsResult::NoSuchObject;
			}

			if (!file->BuildLevel(*image))
			{
				return AltLocationsResult::CantBuild;
			}

			return FindObject(*image, name, place);
		}

		AltLocationsResult FromLoader(LevelOpenResult result)
		{
			switch (result)
			{
			case LevelOpenResult::Opened: return AltLocationsResult::Done;
			case LevelOpenResult::NothingOpen: return AltLocationsResult::NothingOpen;
			case LevelOpenResult::CantRead: return AltLocationsResult::CantBuild;
			default: return AltLocationsResult::LoaderRefused;
			}
		}

		AltLocationsResult LoadIn(OpenLevelFile *file, uint8_t const *id, model::AltLocations *state)
		{
			Bytes image;
			Place place = {};
			AltLocationsResult const prepared = Prepare(file, id, &image, &place);

			*state = model::AltLocations();

			if (prepared != AltLocationsResult::Done)
			{
				return prepared;
			}

			if (!place.hasAlt)
			{
				return AltLocationsResult::Done;
			}

			if (!model::AltLocations_Parse((uint8_t const *)image.data() + place.altAt + HeadingBytes, place.altSize - HeadingBytes, state))
			{
				*state = model::AltLocations();
				return AltLocationsResult::BadChunk;
			}

			return AltLocationsResult::Done;
		}

		AltLocationsResult SaveIn(OpenLevelFile *file, uint8_t const *id, model::AltLocations const &state)
		{
			Bytes image;
			Place place = {};
			AltLocationsResult const prepared = Prepare(file, id, &image, &place);

			if (prepared != AltLocationsResult::Done)
			{
				return prepared;
			}

			if (state.list.size() > model::kMostAltLocations)
			{
				return AltLocationsResult::TooMany;
			}

			std::vector<uint8_t> payload;
			bool const makeOne = model::AltLocations_Build(state, &payload);   // an empty list makes none (FUN_004dfc70)
			size_t objectSize = place.objectSize;

			// FIRST the old chunk is destroyed
			if (place.hasAlt)
			{
				image.erase(image.begin() + (std::ptrdiff_t)place.altAt, image.begin() + (std::ptrdiff_t)(place.altAt + place.altSize));
				objectSize -= place.altSize;
			}

			// then a new one, after the object's other chunks
			if (makeOne)
			{
				Bytes chunk(HeadingBytes, 0);

				memcpy(chunk.data(), "ALTLOCAT", 8);
				chunk.insert(chunk.end(), payload.begin(), payload.end());
				PutIntAt(chunk, 8, (int32_t)chunk.size());

				image.insert(image.begin() + (std::ptrdiff_t)(place.objectAt + objectSize), chunk.begin(), chunk.end());
				objectSize += chunk.size();
			}

			if (objectSize > 0x7fffffff || image.size() > 0x7fffffff)
			{
				return AltLocationsResult::TooMany;
			}

			PutIntAt(image, place.objectAt + 8, (int32_t)objectSize);
			PutIntAt(image, 8, (int32_t)image.size());

			return FromLoader(file->ReplaceLevel(image));
		}
	}

	char const *AltLocationsWords(AltLocationsResult result)
	{
		switch (result)
		{
		case AltLocationsResult::Done: return "done";
		case AltLocationsResult::NothingOpen: return "no level is open";
		case AltLocationsResult::NoName: return "the object has no name to look for";
		case AltLocationsResult::NoSuchObject: return "the level has no object with that name";
		case AltLocationsResult::BadChunk: return "the object's alternate locations aren't ones the game's reader would take";
		case AltLocationsResult::TooMany: return "there are more locations than a chunk's size can say";
		case AltLocationsResult::CantBuild: return "the level couldn't be put together from its bytes";
		case AltLocationsResult::LoaderRefused: return "the game's loader wouldn't take the level with that change, so nothing was changed";
		}

		return "the alternate locations couldn't be changed";
	}

	AltLocationsResult AlternateLocations_Load(OpenLevelFile *file, uint8_t const *id, model::AltLocations *state)
	{
		try
		{
			return LoadIn(file, id, state);
		}
		catch (std::exception const &)
		{
			return AltLocationsResult::CantBuild;
		}
	}

	AltLocationsResult AlternateLocations_Save(OpenLevelFile *file, uint8_t const *id, model::AltLocations const &state)
	{
		try
		{
			return SaveIn(file, id, state);
		}
		catch (std::exception const &)
		{
			return AltLocationsResult::CantBuild;
		}
	}
	namespace
	{
		size_t const HeaderPlaceAt = 20;   // OBJHEAD1's payload: three ints
		size_t const HeaderTurnAt = 32;    // then four floats
		size_t const HeaderNumbers = 48;   // the numbers before the object's number

		Object_Chunk *ObjectById(OpenLevelFile *file, uint8_t const *id)
		{
			File_Chunk *const level = file ? file->Level() : 0;

			if (!level || !id)
			{
				return 0;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					return each();
				}
			}

			return 0;
		}

		bool HeaderOf(OpenLevelFile *file, uint8_t const *id, Chunk **header, std::vector<unsigned char> *payload)
		{
			Object_Chunk *const object = ObjectById(file, id);

			*header = object ? object->get_header() : 0;
			return *header && file->PayloadOf(*header, payload) && payload->size() >= HeaderNumbers;
		}
	}

	AltLocationsResult AlternateLocations_ObjectNow(OpenLevelFile *file, uint8_t const *id, model::AltLocation *now)
	{
		if (!file || !file->Level())
		{
			return AltLocationsResult::NothingOpen;
		}

		if (!id || IdIsAllZero(id))
		{
			return AltLocationsResult::NoName;
		}

		Chunk *header = 0;
		std::vector<unsigned char> payload;

		if (!HeaderOf(file, id, &header, &payload))
		{
			return AltLocationsResult::NoSuchObject;
		}

		model::AltLocation made;

		memcpy(made.place, payload.data() + HeaderPlaceAt, sizeof(made.place));
		memcpy(made.turn, payload.data() + HeaderTurnAt, sizeof(made.turn));
		*now = made;
		return AltLocationsResult::Done;
	}

	AltLocationsResult AlternateLocations_PutOnObject(OpenLevelFile *file, uint8_t const *id, model::AltLocation const &location)
	{
		if (!file || !file->Level())
		{
			return AltLocationsResult::NothingOpen;
		}

		if (!id || IdIsAllZero(id))
		{
			return AltLocationsResult::NoName;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (location.place[axis] > (1 << 24) || location.place[axis] < -(1 << 24))
			{
				return AltLocationsResult::BadChunk;
			}
		}

		for (int part = 0; part < 4; part++)
		{
			// a part that isn't a real number from -2 to 2 (the loader's CheckTurn): the comparison is false for a NaN
			if (!(location.turn[part] >= -2.0f && location.turn[part] <= 2.0f))
			{
				return AltLocationsResult::BadChunk;
			}
		}

		Chunk *header = 0;
		std::vector<unsigned char> payload;

		if (!HeaderOf(file, id, &header, &payload))
		{
			return AltLocationsResult::NoSuchObject;
		}

		memcpy(payload.data() + HeaderPlaceAt, location.place, sizeof(location.place));
		memcpy(payload.data() + HeaderTurnAt, location.turn, sizeof(location.turn));
		return file->SetPayload(header, payload) ? AltLocationsResult::Done : AltLocationsResult::CantBuild;
	}

	AltLocationsResult AlternateLocations_ObjectsWithLocations(OpenLevelFile *file, std::vector<uint8_t> *ids)
	{
		ids->clear();

		File_Chunk *const level = file ? file->Level() : 0;

		if (!level)
		{
			return AltLocationsResult::NothingOpen;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && object->lookup_single_child("ALTLOCAT"))
			{
				uint8_t const *const raw = (uint8_t const *)&object->object_data.ID;

				ids->insert(ids->end(), raw, raw + 8);
			}
		}

		return AltLocationsResult::Done;
	}
}
