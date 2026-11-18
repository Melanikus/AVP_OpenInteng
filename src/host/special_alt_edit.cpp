// See special_alt_edit.h. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "special_alt_edit.h"

#include <vector>

namespace host
{
	namespace
	{
		bool CanHaveLocations(SpecialKind kind)
		{
			return kind != SpecialKind::PlayerStart;   // inteng's kinds 1, 2, 3 and 7 (FUN_004df820)
		}

		// The object of that kind and number, read whole
		SpecialResult Find(OpenLevelFile *file, SpecialKind kind, size_t number, SpecialObject *found)
		{
			std::vector<SpecialObject> all;
			SpecialResult const read = SpecialObjects_Read(file, &all);

			if (read != SpecialResult::Done)
			{
				return read;
			}

			for (SpecialObject const &one : all)
			{
				if (one.kind == kind && one.number == number)
				{
					*found = one;
					return SpecialResult::Done;
				}
			}

			return SpecialResult::NotFound;
		}
	}

	SpecialResult SpecialAlt_Load(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocations *state)
	{
		if (!file || !state || !CanHaveLocations(kind))
		{
			return SpecialResult::BadCall;
		}

		std::vector<uint8_t> payload;
		bool has = false;
		SpecialResult const read = SpecialObject_ReadAlt(file, kind, number, &payload, &has);

		if (read != SpecialResult::Done)
		{
			return read;
		}

		model::AltLocations loaded;

		if (has && !model::AltLocations_Parse(payload.data(), payload.size(), &loaded))
		{
			return SpecialResult::BadValue;
		}

		*state = loaded;
		return SpecialResult::Done;
	}

	SpecialResult SpecialAlt_Save(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocations const &state)
	{
		if (!file || !CanHaveLocations(kind))
		{
			return SpecialResult::BadCall;
		}

		if (state.list.empty())
		{
			return SpecialObject_WriteAlt(file, kind, number, nullptr);   // no locations: no ALTLOCAT (V-28)
		}

		std::vector<uint8_t> payload;

		if (!model::AltLocations_Build(state, &payload))
		{
			return SpecialResult::BadValue;
		}

		return SpecialObject_WriteAlt(file, kind, number, &payload);
	}

	SpecialResult SpecialAlt_Now(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocation *now)
	{
		if (!file || !now || !CanHaveLocations(kind))
		{
			return SpecialResult::BadCall;
		}

		SpecialObject object;
		SpecialResult const found = Find(file, kind, number, &object);

		if (found != SpecialResult::Done)
		{
			return found;
		}

		model::AltLocation made;

		for (int axis = 0; axis < 3; axis++)
		{
			made.place[axis] = object.place[axis];
		}

		if (kind == SpecialKind::ParticleGenerator)
		{
			for (int part = 0; part < 4; part++)
			{
				made.turn[part] = object.turn[part];
			}
		}

		*now = made;
		return SpecialResult::Done;
	}

	SpecialResult SpecialAlt_Put(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocation const &location)
	{
		if (!file || !CanHaveLocations(kind))
		{
			return SpecialResult::BadCall;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			if (location.place[axis] > (1 << 24) || location.place[axis] < -(1 << 24))
			{
				return SpecialResult::BadValue;
			}
		}

		for (int part = 0; part < 4; part++)
		{
			if (!(location.turn[part] >= -2.0f && location.turn[part] <= 2.0f))   // a NaN fails this too
			{
				return SpecialResult::BadValue;
			}
		}

		SpecialObject object;
		SpecialResult const found = Find(file, kind, number, &object);

		if (found != SpecialResult::Done)
		{
			return found;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			object.place[axis] = location.place[axis];
		}

		if (kind == SpecialKind::ParticleGenerator)
		{
			for (int part = 0; part < 4; part++)
			{
				object.turn[part] = location.turn[part];
			}
		}

		return SpecialObject_Write(file, object);
	}
}
