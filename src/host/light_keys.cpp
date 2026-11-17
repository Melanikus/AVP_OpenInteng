// See light_keys.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "light_keys.h"

#include <stdio.h>
#include <string.h>

#include <exception>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"

namespace host
{
	namespace
	{
		// The STDLIGHT's numbers (payload offsets after the heading: chunk_light.cpp:131-164)
		size_t const kBrightness = 52;
		size_t const kSpread = 56;
		size_t const kRange = 60;
		size_t const kColour = 64;
		size_t const kEngineFlags = 68;
		size_t const kLocalFlags = 72;

		// The placed light's (PLOBJLIT)
		size_t const kPlacedEngineFlags = 20;

		uint32_t const kLocalChanged = 0x4;
		uint32_t const kLocalInvisible = 0x20;

		int32_t Get(std::vector<unsigned char> const &payload, size_t at)
		{
			int32_t value = 0;

			memcpy(&value, payload.data() + at, 4);
			return value;
		}

		void Put(std::vector<unsigned char> *payload, size_t at, int32_t value)
		{
			memcpy(payload->data() + at, &value, 4);
		}

		bool Read(OpenLevelFile *file, size_t light, Chunk **chunk, std::vector<unsigned char> *payload)
		{
			if (!file || !file->Level())
			{
				return false;
			}

			*chunk = FindLightChunk(file->Level(), light);

			return *chunk && file->PayloadOf(*chunk, payload) && payload->size() >= 84;
		}

		int32_t Clamp(int64_t value, int32_t low, int32_t high)
		{
			return value < low ? low : (value > high ? high : (int32_t)value);
		}

		// Where a light's numbers are in the chunk after its heading: a STDLIGHT and a PLOBJLIT (chunk_fields.cpp) hold the same numbers in other places
		struct Layout
		{
			size_t brightness, spread, range, colour, engine, local, least;
		};

		Layout const kStd = { 52, 56, 60, 64, 68, 72, 84 };
		Layout const kPlacedUp = { 0, 4, 8, 12, 20, 24, 60 };
		Layout const kPlacedDown = { 0, 4, 8, 16, 20, 24, 60 };

		bool ReadPlaced(OpenLevelFile *file, uint8_t const *objectId, Chunk **chunk, std::vector<unsigned char> *payload)
		{
			if (!file || !file->Level() || !objectId)
			{
				return false;
			}

			*chunk = FindPlacedLightChunk(file->Level(), objectId);

			return *chunk && file->PayloadOf(*chunk, payload) && payload->size() >= kPlacedUp.least;
		}
	}

	char const *LightAttribute_Name(int attribute)
	{
		static char const *const names[kLightAttributeCount] = { "Brightness", "Spread", "Range", "Shadows", "Atten", "Spread Atten", "Omni", "Runtime", "Red", "Green", "Blue", "No PreLight", "Specular" };

		return attribute >= 0 && attribute < kLightAttributeCount ? names[attribute] : "";
	}

	namespace
	{
		LightEditResult ChangeIn(OpenLevelFile *file, Chunk *chunk, std::vector<unsigned char> payload, Layout const &at, int attribute, int steps)
		{
		if (attribute < 0 || attribute >= kLightAttributeCount)
		{
			return LightEditResult::NoSuchLight;
		}

		size_t const kBrightness = at.brightness, kSpread = at.spread, kRange = at.range, kColour = at.colour, kEngineFlags = at.engine, kLocalFlags = at.local;
		uint32_t engine = (uint32_t)Get(payload, kEngineFlags);
		uint32_t local = (uint32_t)Get(payload, kLocalFlags);
		int32_t colour = Get(payload, kColour);
		int64_t const step = steps;

		switch (attribute)
		{
			case 0: Put(&payload, kBrightness, Clamp((int64_t)Get(payload, kBrightness) + step * 10, 0, 0x20000)); break;
			case 1: Put(&payload, kSpread, Clamp((int64_t)Get(payload, kSpread) + step * 2, 1, 0x800)); break;
			case 2: Put(&payload, kRange, Clamp((int64_t)Get(payload, kRange) + step * 10000, 1, 1 << 28)); break;   // the loader refuses a range of 0 or more than 2^28 (light_edit.h)
			case 3: local = steps > 0 ? (local | 2) : (steps < 0 ? (local & ~2u) : local); break;
			case 4: engine = steps > 0 ? (engine | 1) : (steps < 0 ? (engine & ~1u) : engine); break;
			case 5: engine = steps > 0 ? (engine | 2) : (steps < 0 ? (engine & ~2u) : engine); break;
			case 6: engine = steps > 0 ? (engine | 4) : (steps < 0 ? (engine & ~4u) : engine); break;
			case 7: local = steps > 0 ? (local | 0x10) : (steps < 0 ? (local & ~0x10u) : local); break;
			case 8:
			{
				int32_t const red = Clamp((int64_t)((colour >> 16) & 0xff) + step, 0, 255);
				colour = (colour & 0xff00ffff) | (red << 16);
				break;
			}
			case 9:
			{
				int32_t const green = Clamp((int64_t)((colour >> 8) & 0xff) + step, 0, 255);
				colour = (colour & 0xffff00ff) | (green << 8);
				break;
			}
			case 10:
			{
				int32_t const blue = Clamp((int64_t)(colour & 0xff) + step, 0, 255);
				colour = (colour & 0xffffff00) | blue;
				break;
			}
			case 11:
				if (steps > 0)
				{
					local |= 0x40;
					engine |= 0x304;
				}
				else if (steps < 0)
				{
					local &= ~0x40u;
					engine &= ~0x300u;
				}

				break;
			case 12:
				if (steps > 0)
				{
					engine &= ~0x800u;
				}
				else if (steps < 0)
				{
					engine |= 0x800u;
				}

				break;
		}

		if (steps != 0)
		{
			local |= kLocalChanged;
		}

		Put(&payload, kColour, colour);
		Put(&payload, kEngineFlags, (int32_t)engine);
		Put(&payload, kLocalFlags, (int32_t)local);

		if (!file->SetPayload(chunk, payload))
		{
			return LightEditResult::LoaderRefused;
		}

		return LightEditResult::Done;
		}
	}

	LightEditResult LightAttribute_Change(OpenLevelFile *file, size_t light, int attribute, int steps)
	{
		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (!Read(file, light, &chunk, &payload))
		{
			return LightEditResult::NoSuchLight;
		}

		return ChangeIn(file, chunk, payload, kStd, attribute, steps);
	}

	bool PlacedLight_Exists(OpenLevelFile *file, uint8_t const *objectId)
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		return ReadPlaced(file, objectId, &chunk, &payload);
	}

	LightEditResult PlacedLightAttribute_Change(OpenLevelFile *file, uint8_t const *objectId, int attribute, int steps, bool downColour)
	{
		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (!ReadPlaced(file, objectId, &chunk, &payload))
		{
			return LightEditResult::NoSuchLight;
		}

		return ChangeIn(file, chunk, payload, downColour ? kPlacedDown : kPlacedUp, attribute, steps);
	}

	LightData LightDefaultDrop(int32_t const *orientation9)
	{
		LightData data(kLightBytes, 0);
		int32_t const identity[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
		int32_t const *const matrix = orientation9 ? orientation9 : identity;

		memcpy(data.data() + 16, matrix, 36);
		Put(&data, kBrightness, 0x10000);
		Put(&data, kSpread, 1000);
		Put(&data, kRange, 4000000);
		Put(&data, kColour, 0xffffff);
		Put(&data, kEngineFlags, 3);
		Put(&data, kLocalFlags, 5);
		return data;
	}

	namespace
	{
		std::string WordsIn(std::vector<unsigned char> const &payload, Layout const &at, int attribute)
		{
		size_t const kBrightness = at.brightness, kSpread = at.spread, kRange = at.range, kColour = at.colour, kEngineFlags = at.engine, kLocalFlags = at.local;
		uint32_t const engine = (uint32_t)Get(payload, kEngineFlags);
		uint32_t const local = (uint32_t)Get(payload, kLocalFlags);
		int32_t const colour = Get(payload, kColour);
		char text[96];
		bool on = false;

		switch (attribute)
		{
			case 0: snprintf(text, sizeof(text), "Brightness: %.2f%%", (double)Get(payload, kBrightness) * 100.0 / 65536.0); return text;
			case 1: snprintf(text, sizeof(text), "Spread: %.2f deg", (double)Get(payload, kSpread) * 360.0 / 4096.0); return text;
			case 2: snprintf(text, sizeof(text), "Range: %d units", (int)Get(payload, kRange)); return text;
			case 3: on = (local & 2) != 0; break;
			case 4: on = (engine & 1) != 0; break;
			case 5: on = (engine & 2) != 0; break;
			case 6: on = (engine & 4) != 0; break;
			case 7: on = (local & 0x10) != 0; break;
			case 8: snprintf(text, sizeof(text), "Red: %d", (colour >> 16) & 0xff); return text;
			case 9: snprintf(text, sizeof(text), "Green: %d", (colour >> 8) & 0xff); return text;
			case 10: snprintf(text, sizeof(text), "Blue: %d", colour & 0xff); return text;
			case 11: on = (local & 0x40) != 0; break;
			case 12: on = (engine & 0x800) == 0; break;
		}

		return std::string(LightAttribute_Name(attribute)) + ": " + (on ? "on" : "off");
		}
	}

	std::string LightAttribute_Words(OpenLevelFile *file, size_t light, int attribute)
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (attribute < 0 || attribute >= kLightAttributeCount || !Read(file, light, &chunk, &payload))
		{
			return "";
		}

		return WordsIn(payload, kStd, attribute);
	}

	std::string PlacedLightAttribute_Words(OpenLevelFile *file, uint8_t const *objectId, int attribute, bool downColour)
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (attribute < 0 || attribute >= kLightAttributeCount || !ReadPlaced(file, objectId, &chunk, &payload))
		{
			return "";
		}

		std::string const words = WordsIn(payload, downColour ? kPlacedDown : kPlacedUp, attribute);

		return attribute >= 8 && attribute <= 10 ? std::string(downColour ? "Down colour: " : "Up colour: ") + words : words;
	}

	LightEditResult LightInvisible_Toggle(OpenLevelFile *file, size_t light, bool *nowInvisible)
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (!Read(file, light, &chunk, &payload))
		{
			return file && file->Level() ? LightEditResult::NoSuchLight : LightEditResult::NothingOpen;
		}

		uint32_t local = (uint32_t)Get(payload, kLocalFlags);

		local ^= kLocalInvisible;
		Put(&payload, kLocalFlags, (int32_t)local);

		if (!file->SetPayload(chunk, payload))
		{
			return LightEditResult::LoaderRefused;
		}

		if (nowInvisible)
		{
			*nowInvisible = (local & kLocalInvisible) != 0;
		}

		return LightEditResult::Done;
	}

	LightEditResult LightChanged_Clear(OpenLevelFile *file, size_t light)
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (!Read(file, light, &chunk, &payload))
		{
			return file && file->Level() ? LightEditResult::NoSuchLight : LightEditResult::NothingOpen;
		}

		Put(&payload, kLocalFlags, (int32_t)((uint32_t)Get(payload, kLocalFlags) & ~kLocalChanged));
		return file->SetPayload(chunk, payload) ? LightEditResult::Done : LightEditResult::LoaderRefused;
	}

	LightEditResult LightPlace_Set(OpenLevelFile *file, size_t light, int32_t const place[3])
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (!Read(file, light, &chunk, &payload))
		{
			return file && file->Level() ? LightEditResult::NoSuchLight : LightEditResult::NothingOpen;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (place[axis] > (1 << 24) || place[axis] < -(1 << 24))
			{
				return LightEditResult::PlaceOutOfRange;
			}

			Put(&payload, 4 + 4 * (size_t)axis, place[axis]);
		}

		Put(&payload, kLocalFlags, (int32_t)((uint32_t)Get(payload, kLocalFlags) | kLocalChanged));
		return file->SetPayload(chunk, payload) ? LightEditResult::Done : LightEditResult::LoaderRefused;
	}

	LightEditResult LightPasteOver(OpenLevelFile *file, size_t light, LightData const &clipboard)
	{
		Chunk *chunk = nullptr;
		std::vector<unsigned char> payload;

		if (clipboard.size() != kLightBytes)
		{
			return LightEditResult::BadLight;
		}

		if (!Read(file, light, &chunk, &payload))
		{
			return file && file->Level() ? LightEditResult::NoSuchLight : LightEditResult::NothingOpen;
		}

		// the engine record (brightness, spread, range, colour, engine flags) and the local flags, with "changed" put in; the place, the orientation and the number stay (FUN_004acf30 0x4ad364-0x4ad3f5)
		for (size_t at = kBrightness; at <= kEngineFlags; at += 4)
		{
			memcpy(payload.data() + at, clipboard.data() + at, 4);
		}

		int32_t clipLocal = 0;

		memcpy(&clipLocal, clipboard.data() + kLocalFlags, 4);
		Put(&payload, kLocalFlags, (int32_t)((uint32_t)clipLocal | kLocalChanged));

		return file->SetPayload(chunk, payload) ? LightEditResult::Done : LightEditResult::LoaderRefused;
	}

	LightEditResult LightCopyOver(OpenLevelFile *file, LightKeyRef const &target, LightKeyRef const &source, bool sourceDown)
	{
		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		Chunk *fromChunk = nullptr, *toChunk = nullptr;
		std::vector<unsigned char> from, to;
		bool const haveFrom = source.placed ? ReadPlaced(file, source.objectId, &fromChunk, &from) : Read(file, source.light, &fromChunk, &from);
		bool const haveTo = target.placed ? ReadPlaced(file, target.objectId, &toChunk, &to) : Read(file, target.light, &toChunk, &to);

		if (!haveFrom || !haveTo)
		{
			return LightEditResult::NoSuchLight;
		}

		// the colour: the copied light's up or down one by its [5]; into the same one of a placed target, whose [5] becomes the copied one's (a light of the set's [5] is 0: the up colour)
		bool const down = source.placed && sourceDown;
		Layout const &fromAt = source.placed ? (down ? kPlacedDown : kPlacedUp) : kStd;
		Layout const &toAt = target.placed ? (down ? kPlacedDown : kPlacedUp) : kStd;

		// both placed: the copied PLOBJLIT's 15 numbers first (Placed_Object_Light_Data, 60 bytes), the target's extra words kept
		if (source.placed && target.placed)
		{
			memcpy(to.data(), from.data(), 60);
		}

		Put(&to, toAt.brightness, Get(from, fromAt.brightness));
		Put(&to, toAt.spread, Get(from, fromAt.spread));
		Put(&to, toAt.range, Get(from, fromAt.range));
		Put(&to, toAt.colour, Get(from, fromAt.colour));
		Put(&to, toAt.engine, Get(from, fromAt.engine));
		Put(&to, toAt.local, (int32_t)((uint32_t)Get(from, fromAt.local) | kLocalChanged));

		return file->SetPayload(toChunk, to) ? LightEditResult::Done : LightEditResult::LoaderRefused;
	}

	LightEditResult Ambience_Change(OpenLevelFile *file, int steps, int *percent)
	{
		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		Chunk *const chunk = FindLightSetChunk(file->Level(), "AMBIENCE");
		std::vector<unsigned char> payload;

		if (!chunk || !file->PayloadOf(chunk, &payload) || payload.size() < 4)
		{
			return LightEditResult::NoLightSet;
		}

		int32_t const value = Clamp((int64_t)Get(payload, 0) + (int64_t)steps * 0x28f, 0, 0x10000);

		Put(&payload, 0, value);

		if (!file->SetPayload(chunk, payload))
		{
			return LightEditResult::LoaderRefused;
		}

		if (percent)
		{
			*percent = (int)((int64_t)value * 100 / 0x10000);
		}

		return LightEditResult::Done;
	}

	LightEditResult Ambience_Percent(OpenLevelFile *file, int *percent)
	{
		if (!file || !file->Level() || !percent)
		{
			return LightEditResult::NothingOpen;
		}

		Chunk *const chunk = FindLightSetChunk(file->Level(), "AMBIENCE");
		std::vector<unsigned char> payload;

		if (!chunk || !file->PayloadOf(chunk, &payload) || payload.size() < 4)
		{
			return LightEditResult::NoLightSet;
		}

		*percent = (int)((int64_t)Clamp((int64_t)Get(payload, 0), 0, 0x10000) * 100 / 0x10000);
		return LightEditResult::Done;
	}

	LightEditResult LightsWriteBack(OpenLevelFile *file, bool unhide, size_t *changed)
	{
		size_t count = 0;

		if (changed)
		{
			*changed = 0;
		}

		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		try
		{
			if (unhide)
			{
				LightEditResult const shown = HideAllLights(file, false, nullptr);

				if (shown != LightEditResult::Done && shown != LightEditResult::NoLightSet)
				{
					return shown;
				}
			}

			File_Chunk *const level = file->Level();

			for (size_t i = 0; Chunk *const chunk = FindLightChunk(level, i); i++)
			{
				std::vector<unsigned char> payload;

				if (!file->PayloadOf(chunk, &payload) || payload.size() < 84)
				{
					continue;
				}

				uint32_t const flags = (uint32_t)Get(payload, kEngineFlags);

				if ((flags & 0x300) != 0)
				{
					Put(&payload, kEngineFlags, (int32_t)(flags & 0xfffffcff));

					if (!file->SetPayload(chunk, payload))
					{
						return LightEditResult::LoaderRefused;
					}

					count++;
				}
			}

			::List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object)
				{
					continue;
				}

				Chunk *const placed = FindPlacedLightChunk(level, (uint8_t const *)&object->object_data.ID);
				std::vector<unsigned char> payload;

				if (!placed || !file->PayloadOf(placed, &payload) || payload.size() < kPlacedEngineFlags + 4)
				{
					continue;
				}

				uint32_t const flags = (uint32_t)Get(payload, kPlacedEngineFlags);

				if ((flags & 0x300) != 0)
				{
					Put(&payload, kPlacedEngineFlags, (int32_t)(flags & 0xfffffcff));

					if (!file->SetPayload(placed, payload))
					{
						return LightEditResult::LoaderRefused;
					}

					count++;
				}
			}
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}

		if (changed)
		{
			*changed = count;
		}

		return LightEditResult::Done;
	}

	LightEditResult LightsAtStart(OpenLevelFile *file, size_t *changed)
	{
		size_t count = 0;

		if (changed)
		{
			*changed = 0;
		}

		if (!file || !file->Level())
		{
			return LightEditResult::NothingOpen;
		}

		size_t const kPlacedLocalFlags = 24, kPlacedFlags = 56;   // PLOBJLIT: the local flags (7th number), the flags word (15th)
		uint32_t const kNoSpecularPlaced = 0x4, kNoSpecularEngine = 0x800;   // PlacedLightFlag_NoSpecular, LFlag_NoSpecular

		try
		{
			File_Chunk *const level = file->Level();

			// the set's lights: the record's local flags are the chunk's & ~4 (0x4aedb8)
			for (size_t i = 0; Chunk *const chunk = FindLightChunk(level, i); i++)
			{
				std::vector<unsigned char> payload;

				if (!file->PayloadOf(chunk, &payload) || payload.size() < 84)
				{
					continue;
				}

				uint32_t const local = (uint32_t)Get(payload, kLocalFlags);

				if ((local & kLocalChanged) != 0)
				{
					Put(&payload, kLocalFlags, (int32_t)(local & ~kLocalChanged));

					if (!file->SetPayload(chunk, payload))
					{
						return LightEditResult::LoaderRefused;
					}

					count++;
				}
			}

			// the placed lights: NoSpecular moved (0x4aea2e), then the record's local flags & ~4 (0x4aeaa8)
			::List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Chunk *const placed = object ? FindPlacedLightChunk(level, (uint8_t const *)&object->object_data.ID) : nullptr;
				std::vector<unsigned char> payload;

				if (!placed || !file->PayloadOf(placed, &payload) || payload.size() < kPlacedFlags + 4)
				{
					continue;
				}

				uint32_t const flags = (uint32_t)Get(payload, kPlacedFlags);
				uint32_t const local = (uint32_t)Get(payload, kPlacedLocalFlags);
				bool const moveSpecular = (flags & kNoSpecularPlaced) != 0;

				if (!moveSpecular && (local & kLocalChanged) == 0)
				{
					continue;
				}

				if (moveSpecular)
				{
					Put(&payload, kPlacedFlags, (int32_t)(flags & ~kNoSpecularPlaced));
					Put(&payload, kPlacedEngineFlags, (int32_t)((uint32_t)Get(payload, kPlacedEngineFlags) | kNoSpecularEngine));
				}

				Put(&payload, kPlacedLocalFlags, (int32_t)(local & ~kLocalChanged));

				if (!file->SetPayload(placed, payload))
				{
					return LightEditResult::LoaderRefused;
				}

				count++;
			}
		}
		catch (std::exception const &)
		{
			return LightEditResult::CantBuild;
		}

		if (changed)
		{
			*changed = count;
		}

		return LightEditResult::Done;
	}
}
