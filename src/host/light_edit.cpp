// See light_edit.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (M-3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "light_edit.h"

#include <limits.h>
#include <string.h>

#include <exception>
#include <vector>

#include "chunk.hpp"
#include "chunk_light.hpp"    // Light_Set_Header_Chunk
#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "level_file.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Bytes;

		size_t const HeadingBytes = 12;          // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const LightChunkBytes = HeadingBytes + kLightBytes;
		char const LightSetName[] = "NORMALLT";  // the set the game uses (chunk_light.hpp: Light_Set_Header_Chunk)
		int32_t const MostPlace = 1 << 24;       // CheckPlace (chunk.hpp): a place farther than 2^24 millimetres from the middle is refused

		// Where a STDLIGHT's numbers are, counted from the start of its payload (chunk_fields.cpp's LightFields, win95/chunk_light.cpp:131-164)
		size_t const NumberAt = 0;
		size_t const PlaceAt = 4;
		size_t const OrientationAt = 16;         // nine ints
		size_t const BrightnessAt = 52;
		size_t const SpreadAt = 56;
		size_t const RangeAt = 60;
		size_t const ColourAt = 64;
		size_t const EngineFlagsAt = 68;
		size_t const LocalFlagsAt = 72;

		int32_t const FullBrightness = 65536;    // 100%
		int32_t const StockSpread = 1000;        // vasquez-defaults.md: 6,510 of the 7,328 stock lights
		int32_t const StockRange = 4000;         // the commonest single range
		int32_t const StockEngineFlags = 0x7;    // LFlag_CosAtten, _CosSpreadAtten, _Omni
		int32_t const StockLocalFlags = 0x11;    // LOFlag_On, _Runtime
		int32_t const White = 0x00FFFFFF;        // blue, green and red 255 (the colour is read as bytes, blue first)

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

		struct Piece
		{
			size_t at;
			size_t size;
		};

		// Where the light set's chunks are in a level's bytes
		struct LightSetPlace
		{
			Piece environment;           // the REBENVDT that holds the set
			Piece set;                   // the LIGHTSET named NORMALLT
			std::vector<Piece> lights;   // its STDLIGHTs, in the file's order
			size_t after;                // where a new light goes: straight after the last light, or at the set's end
		};

		// Reads the chunks inside a light set. False for bytes that can't be walked.
		bool ReadSet(Bytes const &bytes, Piece set, LightSetPlace *place, bool *named)
		{
			*named = false;
			place->lights.clear();
			place->after = set.at + set.size;

			for (size_t at = set.at + HeadingBytes; at < set.at + set.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, set.at + set.size, &size))
				{
					return false;
				}

				if (NameIs(bytes, at, "LTSETHDR") && size >= HeadingBytes + 8 && memcmp(bytes.data() + at + HeadingBytes, LightSetName, 8) == 0)
				{
					*named = true;
				}
				else if (NameIs(bytes, at, "STDLIGHT") && size >= LightChunkBytes)
				{
					Piece const light = { at, size };
					place->lights.push_back(light);
					place->after = at + size;
				}

				at += size;
			}

			return true;
		}

		// Finds the light set NORMALLT in a level's bytes: false when they can't be walked, and *found says whether the set is there
		bool FindSet(Bytes const &bytes, LightSetPlace *place, bool *found)
		{
			*found = false;

			if (bytes.size() < HeadingBytes || !NameIs(bytes, 0, "REBINFF2"))
			{
				return false;
			}

			for (size_t at = HeadingBytes; at < bytes.size(); )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, bytes.size(), &size))
				{
					return false;
				}

				if (NameIs(bytes, at, "REBENVDT"))
				{
					for (size_t inner = at + HeadingBytes; inner < at + size; )
					{
						size_t innerSize = 0;

						if (!ChunkSizeAt(bytes, inner, at + size, &innerSize))
						{
							return false;
						}

						if (!*found && NameIs(bytes, inner, "LIGHTSET"))
						{
							Piece const set = { inner, innerSize };
							LightSetPlace here;
							bool named = false;

							if (!ReadSet(bytes, set, &here, &named))
							{
								return false;
							}

							if (named)
							{
								here.environment.at = at;
								here.environment.size = size;
								here.set = set;
								*place = here;
								*found = true;
							}
						}

						inner += innerSize;
					}
				}

				at += size;
			}

			return true;
		}

		LightEditResult FromLoader(LevelOpenResult result)
		{
			switch (result)
			{
			case LevelOpenResult::Opened: return LightEditResult::Done;
			case LevelOpenResult::NothingOpen: return LightEditResult::NothingOpen;
			case LevelOpenResult::CantRead: return LightEditResult::CantBuild;
			default: return LightEditResult::LoaderRefused;
			}
		}

		// The open level as the next save would write it, and where its light set is
		LightEditResult Prepare(OpenLevelFile *file, Bytes *image, LightSetPlace *place)
		{
			if (!file || !file->Level())
			{
				return LightEditResult::NothingOpen;
			}

			bool found = false;

			if (!file->BuildLevel(*image) || !FindSet(*image, place, &found))
			{
				return LightEditResult::CantBuild;
			}

			return found ? LightEditResult::Done : LightEditResult::NoLightSet;
		}

		// Makes the size of the light set, of the environment data and of the whole level right, after `change` bytes were put in (or, below 0,
		// taken out) inside the set: place has the sizes as they were. False when a size would no longer fit in the 31 bits the game keeps it in.
		bool FixSizes(Bytes *image, LightSetPlace const &place, int64_t change)
		{
			int64_t const set = (int64_t)place.set.size + change;
			int64_t const environment = (int64_t)place.environment.size + change;

			if (image->size() > (size_t)INT_MAX || set > INT_MAX || environment > INT_MAX)
			{
				return false;
			}

			PutIntAt(*image, place.set.at + 8, (int32_t)set);
			PutIntAt(*image, place.environment.at + 8, (int32_t)environment);
			PutIntAt(*image, 8, (int32_t)image->size());
			return true;
		}

		// The numbers of a new light: the editor's own (see the header), at a place, with a number
		LightData DefaultLight(int32_t const *place, int32_t number)
		{
			Bytes bytes(kLightBytes, 0);

			PutIntAt(bytes, NumberAt, number);
			PutIntAt(bytes, OrientationAt, FullBrightness);        // a turn of nothing: the matrix's diagonal is 1.0 (65536)
			PutIntAt(bytes, OrientationAt + 16, FullBrightness);
			PutIntAt(bytes, OrientationAt + 32, FullBrightness);
			PutIntAt(bytes, BrightnessAt, FullBrightness);
			PutIntAt(bytes, SpreadAt, StockSpread);
			PutIntAt(bytes, RangeAt, StockRange);
			PutIntAt(bytes, ColourAt, White);
			PutIntAt(bytes, EngineFlagsAt, StockEngineFlags);
			PutIntAt(bytes, LocalFlagsAt, StockLocalFlags);

			for (int axis = 0; axis < 3; axis++)
			{
				PutIntAt(bytes, PlaceAt + 4 * (size_t)axis, place[axis]);
			}

			return LightData(bytes.begin(), bytes.end());
		}

		bool PlaceIsInRange(int32_t const *place)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if (place[axis] > MostPlace || place[axis] < -MostPlace)
				{
					return false;
				}
			}

			return true;
		}

		// Puts a new light's chunk in the set, with these numbers
		LightEditResult Insert(Bytes *image, LightSetPlace const &place, Bytes const &payload)
		{
			Bytes chunk(LightChunkBytes, 0);

			memcpy(chunk.data(), "STDLIGHT", 8);
			PutIntAt(chunk, 8, (int32_t)LightChunkBytes);
			memcpy(chunk.data() + HeadingBytes, payload.data(), kLightBytes);

			image->insert(image->begin() + (std::ptrdiff_t)place.after, chunk.begin(), chunk.end());

			return FixSizes(image, place, (int64_t)LightChunkBytes) ? LightEditResult::Done : LightEditResult::TooMany;
		}

		// A light's numbers as a payload of a level's bytes
		Bytes PayloadAt(Bytes const &image, Piece const &light)
		{
			return Bytes(image.begin() + (std::ptrdiff_t)(light.at + HeadingBytes), image.begin() + (std::ptrdiff_t)(light.at + HeadingBytes + kLightBytes));
		}

		LightEditResult CountIn(OpenLevelFile *file, size_t *count)
		{
			Bytes image;
			LightSetPlace place;
			LightEditResult const prepared = Prepare(file, &image, &place);

			if (prepared == LightEditResult::Done)
			{
				*count = place.lights.size();
			}

			return prepared;
		}

		LightEditResult PlacesIn(OpenLevelFile *file, std::vector<int32_t> *places)
		{
			Bytes image;
			LightSetPlace set;
			LightEditResult const prepared = Prepare(file, &image, &set);

			if (prepared != LightEditResult::Done)
			{
				return prepared;
			}

			places->clear();

			for (Piece const &each : set.lights)
			{
				for (size_t axis = 0; axis < 3; axis++)
				{
					places->push_back(IntAt(image, each.at + HeadingBytes + PlaceAt + 4 * axis));
				}
			}

			return LightEditResult::Done;
		}

		LightEditResult CopyIn(OpenLevelFile *file, size_t light, LightData *out)
		{
			Bytes image;
			LightSetPlace place;
			LightEditResult const prepared = Prepare(file, &image, &place);

			if (prepared != LightEditResult::Done)
			{
				return prepared;
			}

			if (light >= place.lights.size())
			{
				return LightEditResult::NoSuchLight;
			}

			Bytes const payload = PayloadAt(image, place.lights[light]);

			out->assign(payload.begin(), payload.end());
			return LightEditResult::Done;
		}

		LightEditResult DropIn(OpenLevelFile *file, int32_t const *place, LightData const *data, size_t *newLight)
		{
			if (data && data->size() != kLightBytes)
			{
				return LightEditResult::BadLight;
			}

			if (!place || !PlaceIsInRange(place))
			{
				return LightEditResult::PlaceOutOfRange;
			}

			Bytes image;
			LightSetPlace set;
			LightEditResult const prepared = Prepare(file, &image, &set);

			if (prepared != LightEditResult::Done)
			{
				return prepared;
			}

			// The new light's number is one more than the greatest the set has
			int32_t number = 0;

			for (Piece const &each : set.lights)
			{
				int32_t const had = IntAt(image, each.at + HeadingBytes + NumberAt);
				number = had >= number && had < INT_MAX ? had + 1 : number;
			}

			Bytes payload;

			if (data)
			{
				payload.assign(data->begin(), data->end());
				PutIntAt(payload, NumberAt, number);

				for (int axis = 0; axis < 3; axis++)
				{
					PutIntAt(payload, PlaceAt + 4 * (size_t)axis, place[axis]);
				}
			}
			else
			{
				LightData const made = DefaultLight(place, number);
				payload.assign(made.begin(), made.end());
			}

			LightEditResult const inserted = Insert(&image, set, payload);

			if (inserted != LightEditResult::Done)
			{
				return inserted;
			}

			LightEditResult const done = FromLoader(file->ReplaceLevel(image));

			if (done == LightEditResult::Done && newLight)
			{
				*newLight = set.lights.size();
			}

			return done;
		}

		LightEditResult RemoveIn(OpenLevelFile *file, size_t light)
		{
			Bytes image;
			LightSetPlace set;
			LightEditResult const prepared = Prepare(file, &image, &set);

			if (prepared != LightEditResult::Done)
			{
				return prepared;
			}

			if (light >= set.lights.size())
			{
				return LightEditResult::NoSuchLight;
			}

			Piece const gone = set.lights[light];

			image.erase(image.begin() + (std::ptrdiff_t)gone.at, image.begin() + (std::ptrdiff_t)(gone.at + gone.size));

			if (!FixSizes(&image, set, -(int64_t)gone.size))
			{
				return LightEditResult::CantBuild;
			}

			return FromLoader(file->ReplaceLevel(image));
		}

		LightEditResult OverwriteIn(OpenLevelFile *file, size_t light, LightData const &data)
		{
			if (data.size() != kLightBytes)
			{
				return LightEditResult::BadLight;
			}

			Bytes image;
			LightSetPlace set;
			LightEditResult const prepared = Prepare(file, &image, &set);

			if (prepared != LightEditResult::Done)
			{
				return prepared;
			}

			if (light >= set.lights.size())
			{
				return LightEditResult::NoSuchLight;
			}

			size_t const at = set.lights[light].at + HeadingBytes;
			Bytes kept(image.begin() + (std::ptrdiff_t)at, image.begin() + (std::ptrdiff_t)(at + kLightBytes));

			// The copy's numbers, but for the light's own number and place
			memcpy(image.data() + at, data.data(), kLightBytes);
			memcpy(image.data() + at + NumberAt, kept.data() + NumberAt, 4);
			memcpy(image.data() + at + PlaceAt, kept.data() + PlaceAt, 12);

			return FromLoader(file->ReplaceLevel(image));
		}

		// The light set NORMALLT as the game's chunk library holds it, or null
		Chunk_With_Children *LibrarySet(File_Chunk *level)
		{
			if (!level)
			{
				return 0;
			}

			Chunk *const environment = level->lookup_single_child("REBENVDT");

			if (!environment || environment->r_u_miscellaneous())
			{
				return 0;
			}

			List<Chunk *> sets;

			((Chunk_With_Children *)environment)->lookup_child("LIGHTSET", sets);

			for (int i = 0; i < sets.size(); i++)
			{
				Chunk *const set = sets[i];

				if (!set || set->r_u_miscellaneous())
				{
					continue;
				}

				Chunk *const header = ((Chunk_With_Children *)set)->lookup_single_child("LTSETHDR");

				if (header && !header->r_u_miscellaneous() && memcmp(((Light_Set_Header_Chunk *)header)->light_set_name, LightSetName, 8) == 0)
				{
					return (Chunk_With_Children *)set;
				}
			}

			return 0;
		}
	}

	char const *LightEditWords(LightEditResult result)
	{
		switch (result)
		{
		case LightEditResult::Done: return "done";
		case LightEditResult::NothingOpen: return "no level is open";
		case LightEditResult::NoLightSet: return "the level has no light set named NORMALLT, the one the game uses";
		case LightEditResult::NoSuchLight: return "the light set has no light at that place";
		case LightEditResult::BadLight: return "that isn't a light's numbers";
		case LightEditResult::PlaceOutOfRange: return "a light can't be farther than 2^24 millimetres from the middle of the level";
		case LightEditResult::TooMany: return "the level has no room for another light";
		case LightEditResult::CantBuild: return "the level couldn't be put together from its bytes";
		case LightEditResult::LoaderRefused: return "the game's loader wouldn't take the level with that change, so nothing was changed";
		}

		return "the light couldn't be changed";
	}

	// Each public function runs its work and refuses, with nothing changed, when there isn't memory enough
	LightEditResult CountLights(OpenLevelFile *file, size_t *count)
	{
		if (!count)
		{
			return LightEditResult::CantBuild;
		}

		try
		{
			return CountIn(file, count);
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}
	}

	LightEditResult LightPlaces(OpenLevelFile *file, std::vector<int32_t> *places)
	{
		if (!places)
		{
			return LightEditResult::CantBuild;
		}

		try
		{
			return PlacesIn(file, places);
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}
	}

	LightEditResult CopyLight(OpenLevelFile *file, size_t light, LightData *out)
	{
		if (!out)
		{
			return LightEditResult::CantBuild;
		}

		try
		{
			return CopyIn(file, light, out);
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}
	}

	LightEditResult DropLight(OpenLevelFile *file, int32_t const *place, LightData const *data, size_t *newLight)
	{
		try
		{
			return DropIn(file, place, data, newLight);
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}
	}

	LightEditResult RemoveLight(OpenLevelFile *file, size_t light)
	{
		try
		{
			return RemoveIn(file, light);
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}
	}

	LightEditResult OverwriteLight(OpenLevelFile *file, size_t light, LightData const &data)
	{
		try
		{
			return OverwriteIn(file, light, data);
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}
	}

	// matches FUN_004ab5a0 and FUN_004ab4d0 (out/menu-reads-2.txt:3692-3725 and 3655-3690): every light of inteng's list gets bit 0x20 of its
	// flags word set (hide) or cleared (unhide). That word is the chunk's local light flags (loaded by FUN_004ae5d0, out/lights-sounds.txt:
	// 329-332 and 203, with bit 0x4 taken out, saved back as it is by FUN_004ad630, out/lights-sounds.txt:880-885 and 827-831)
	LightEditResult HideAllLights(OpenLevelFile *file, bool hide, size_t *count)
	{
		size_t const kStdLocalFlagsAt = 72;   // STDLIGHT (LightFields, chunk_fields.cpp)
		size_t const kPlacedLocalFlagsAt = 24;   // PLOBJLIT (PlacedLightFields, chunk_fields.cpp)
		uint32_t const invisible = 0x20;   // LOFlag_Invisible (chunk_light.hpp:20)
		size_t changed = 0;

		if (count)
		{
			*count = 0;
		}

		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		try
		{
			File_Chunk *const level = file->Level();

			if (!LibrarySet(level))
			{
				return LightEditResult::NoLightSet;
			}

			std::vector<Chunk *> lights;
			std::vector<size_t> flagsAt;

			for (size_t i = 0; Chunk *const light = FindLightChunk(level, i); i++)
			{
				lights.push_back(light);
				flagsAt.push_back(kStdLocalFlagsAt);
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Chunk *const placed = each() ? each()->lookup_single_child("PLOBJLIT") : 0;

				if (placed)
				{
					lights.push_back(placed);
					flagsAt.push_back(kPlacedLocalFlagsAt);
				}
			}

			for (size_t i = 0; i < lights.size(); i++)
			{
				std::vector<unsigned char> payload;

				// A light whose bytes aren't there, or are too few to hold the word, is left as it is (the loader would have refused the level)
				if (!file->PayloadOf(lights[i], &payload) || payload.size() < flagsAt[i] + 4)
				{
					continue;
				}

				uint32_t flags = 0;

				memcpy(&flags, payload.data() + flagsAt[i], 4);

				uint32_t const now = hide ? (flags | invisible) : (flags & ~invisible);

				if (now != flags)
				{
					memcpy(payload.data() + flagsAt[i], &now, 4);

					if (!file->SetPayload(lights[i], payload))
					{
						continue;
					}

					changed++;
				}
			}
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}

		if (count)
		{
			*count = changed;
		}

		return LightEditResult::Done;
	}

	Chunk *FindLightChunk(File_Chunk *level, size_t light)
	{
		Chunk_With_Children *const set = LibrarySet(level);

		if (!set)
		{
			return 0;
		}

		List<Chunk *> lights;

		set->lookup_child("STDLIGHT", lights);
		return light < (size_t)lights.size() ? lights[(int)light] : 0;
	}

	Chunk *FindLightSetChunk(File_Chunk *level, char const *id)
	{
		Chunk_With_Children *const set = LibrarySet(level);

		if (!set || !id || (strncmp(id, "AMBIENCE", 8) != 0 && strncmp(id, "LITSCALE", 8) != 0))
		{
			return 0;
		}

		return set->lookup_single_child(id);
	}

	Chunk *FindPlacedLightChunk(File_Chunk *level, uint8_t const *objectId)
	{
		if (!level || !objectId)
		{
			return 0;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && memcmp(&object->object_data.ID, objectId, 8) == 0)
			{
				return object->lookup_single_child("PLOBJLIT");
			}
		}

		return 0;
	}
}
