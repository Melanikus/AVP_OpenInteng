// See light_intensities_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "light_intensities_edit.h"

#include <string.h>

#include <vector>

#include "level_file.h"
#include "light_scene.h"
#include "object_edit.h"

namespace host
{
	namespace
	{
		void PutInt(std::vector<uint8_t> *bytes, int32_t value)
		{
			uint8_t raw[4];
			memcpy(raw, &value, 4);
			bytes->insert(bytes->end(), raw, raw + 4);
		}

		// The whole SHPVTINT chunk (Shape_Vertex_Intensities_Chunk, FUN_004b1ef0): its heading, the set's name (8 letters), a pad, the number of vertices, and a whole number for each
		std::vector<uint8_t> MakeChunk(std::vector<uint32_t> const &values)
		{
			std::vector<uint8_t> chunk;

			chunk.insert(chunk.end(), (uint8_t const *)"SHPVTINT", (uint8_t const *)"SHPVTINT" + 8);
			PutInt(&chunk, (int32_t)(12 + 8 + 4 + 4 + 4 * values.size()));
			chunk.insert(chunk.end(), (uint8_t const *)"NORMALLT", (uint8_t const *)"NORMALLT" + 8);
			PutInt(&chunk, 0);
			PutInt(&chunk, (int32_t)values.size());

			for (uint32_t const value : values)
			{
				PutInt(&chunk, (int32_t)value);
			}

			return chunk;
		}
	}

	char const *SaveIntensitiesWords(SaveIntensitiesResult result)
	{
		switch (result)
		{
		case SaveIntensitiesResult::Done: return "done";
		case SaveIntensitiesResult::NothingOpen: return "no level is open";
		case SaveIntensitiesResult::NoLightSet: return "the level has no light set named NORMALLT";
		case SaveIntensitiesResult::CantBuild: return "the level could not be put together";
		case SaveIntensitiesResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		}

		return "the intensities could not be saved";
	}

	SaveIntensitiesResult SaveIntensities(OpenLevelFile *open, LightState const &state, bool toolsSave, SaveIntensitiesCounts *counts, std::vector<LitObject> *written)
	{
		SaveIntensitiesCounts none;

		if (!counts)
		{
			counts = &none;
		}

		*counts = SaveIntensitiesCounts();

		if (!open || !open->Level())
		{
			return SaveIntensitiesResult::NothingOpen;
		}

		LightSceneLights lights;
		std::string why;

		if (!LightScene_ReadLights(open, &lights, &why))
		{
			return SaveIntensitiesResult::NoLightSet;
		}

		// the state: a light that is off adds nothing; for the game's save the lights that are No PreLight are off too (LightScene_LightVertex)
		for (size_t i = 0; i < lights.lights.size(); i++)
		{
			lights.lights[i].on = state.IsOn(i);
			counts->lightsOff += (!lights.lights[i].on || (lights.lights[i].noPreLight && !toolsSave)) ? 1 : 0;
		}

		std::vector<LitObject> lit;

		LightScene_LightModules(open, lights, toolsSave, state.DoorsClosed(), &lit, state.FlatNormals());

		std::vector<ObjectChildEdit> edits;

		for (LitObject const &one : lit)
		{
			counts->modules++;

			if (!one.hasStored && !one.reached)
			{
				continue;   // nothing to take out and nothing to make
			}

			ObjectChildEdit edit;

			edit.objectIndex = one.number;
			memcpy(edit.name, "SHPVTINT", 9);
			memcpy(edit.payloadStarts, "NORMALLT", 9);
			edit.remove = !one.reached || one.computed.empty();

			if (!edit.remove)
			{
				edit.chunk = MakeChunk(one.computed);
				counts->written++;
				counts->vertices += one.computed.size();
			}
			else
			{
				counts->removed++;
			}

			edits.push_back(edit);
		}

		if (edits.empty())
		{
			return SaveIntensitiesResult::Done;
		}

		std::vector<LitObject> keep(lit);

		ObjectEditResult const result = SetObjectChildrenMany(open, edits, nullptr);

		switch (result)
		{
		case ObjectEditResult::Done:
			if (written)
			{
				written->swap(keep);
			}

			return SaveIntensitiesResult::Done;
		case ObjectEditResult::NothingOpen: return SaveIntensitiesResult::NothingOpen;
		case ObjectEditResult::LoaderRefused: return SaveIntensitiesResult::LoaderRefused;
		default: return SaveIntensitiesResult::CantBuild;
		}
	}
}
