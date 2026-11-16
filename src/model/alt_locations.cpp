// See alt_locations.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (P9). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "alt_locations.h"

#include <string.h>

namespace model
{
	namespace
	{
		size_t const HeaderBytes = 12;   // group, spare, count
		size_t const EntryBytes = 36;    // AltObjectLocation: three ints, four floats, two ints

		int32_t IntAt(uint8_t const *bytes, size_t at)
		{
			int32_t value;
			memcpy(&value, bytes + at, sizeof(value));
			return value;
		}

		void PutInt(std::vector<uint8_t> *bytes, int32_t value)
		{
			uint8_t raw[sizeof(value)];
			memcpy(raw, &value, sizeof(value));
			bytes->insert(bytes->end(), raw, raw + sizeof(value));
		}

		void PutFloat(std::vector<uint8_t> *bytes, float value)
		{
			uint8_t raw[sizeof(value)];
			memcpy(raw, &value, sizeof(value));
			bytes->insert(bytes->end(), raw, raw + sizeof(value));
		}
	}

	bool AltLocations_Parse(uint8_t const *payload, size_t length, AltLocations *out)
	{
		if (!payload || !out || length < HeaderBytes)
		{
			return false;
		}

		int32_t const count = IntAt(payload, 8);

		// A count from the file is never trusted: it has to be 0 or more and what the bytes after it really hold
		if (count < 0 || (size_t)count > (length - HeaderBytes) / EntryBytes)
		{
			return false;
		}

		AltLocations state;

		state.group = IntAt(payload, 0);
		state.spare = IntAt(payload, 4);
		state.list.resize((size_t)count);

		for (size_t i = 0; i < (size_t)count; i++)
		{
			uint8_t const *const entry = payload + HeaderBytes + i * EntryBytes;
			AltLocation &location = state.list[i];

			for (size_t axis = 0; axis < 3; axis++)
			{
				location.place[axis] = IntAt(entry, 4 * axis);
			}

			for (size_t part = 0; part < 4; part++)
			{
				memcpy(&location.turn[part], entry + 12 + 4 * part, sizeof(float));
			}

			location.spare[0] = IntAt(entry, 28);
			location.spare[1] = IntAt(entry, 32);
		}

		*out = state;
		return true;
	}

	bool AltLocations_Build(AltLocations const &state, std::vector<uint8_t> *payload)
	{
		payload->clear();

		// FUN_004dfc70: nothing is made for an empty list, so the object has no ALTLOCAT
		if (state.list.empty() || state.list.size() > kMostAltLocations)
		{
			return false;
		}

		PutInt(payload, state.group);
		PutInt(payload, state.spare);
		PutInt(payload, (int32_t)state.list.size());

		for (size_t i = 0; i < state.list.size(); i++)
		{
			AltLocation const &location = state.list[i];

			for (size_t axis = 0; axis < 3; axis++)
			{
				PutInt(payload, location.place[axis]);
			}

			for (size_t part = 0; part < 4; part++)
			{
				PutFloat(payload, location.turn[part]);
			}

			// ints 7 and 8 of a SAVED location are 0 (FUN_004dfc70), whatever a loaded one held
			PutInt(payload, 0);
			PutInt(payload, 0);
		}

		return true;
	}

	void AltLocations_DeleteCurrent(AltLocations *state)
	{
		if (state->list.empty())
		{
			return;
		}

		if (state->current >= state->list.size())
		{
			state->current = 0;   // the editor's number can't point outside: a state from a load starts at 0
		}

		state->list.erase(state->list.begin() + (std::ptrdiff_t)state->current);

		// FUN_004df6b0 :8097-8116: with ONE left, that one is taken out too
		if (state->list.size() == 1)
		{
			state->list.clear();
		}

		// :8117-8119: a count that is not above the current number puts the current number back to 0
		if (state->list.size() <= state->current)
		{
			state->current = 0;
		}
	}

	bool AltLocations_Add(AltLocations *state, AltLocation const &now)
	{
		if (state->list.size() + 2 > kMostAltLocations)
		{
			return false;
		}

		if (state->list.empty())
		{
			// FUN_004df820 :8154-8156 and :8160: the object's own place first, then the new one
			state->list.push_back(now);
		}
		else
		{
			if (state->current >= state->list.size())
			{
				state->current = 0;
			}

			state->list[state->current] = now;   // FUN_004f73a0
		}

		state->list.push_back(now);               // FUN_004f75e0, at the end
		state->current = state->list.size() - 1;  // :8161
		return true;
	}

	void AltLocations_Next(AltLocations *state, AltLocation const &now)
	{
		if (state->list.empty())
		{
			return;
		}

		if (state->current >= state->list.size())
		{
			state->current = 0;
		}

		state->list[state->current] = now;                          // FUN_004f73a0
		state->current = (state->current + 1) % state->list.size(); // :8213-8216
	}

	void AltLocations_Prev(AltLocations *state, AltLocation const &now)
	{
		if (state->list.empty())
		{
			return;
		}

		if (state->current >= state->list.size())
		{
			state->current = 0;
		}

		state->list[state->current] = now;                                                     // FUN_004f73a0
		state->current = state->current == 0 ? state->list.size() - 1 : state->current - 1;   // :8266-8270
	}

	void AltLocations_JumpTo(AltLocations *state, AltLocation const &now, int64_t typed)
	{
		if (state->list.empty() || typed < 0)
		{
			return;
		}

		if (state->current >= state->list.size())
		{
			state->current = 0;
		}

		state->list[state->current] = now;                       // FUN_004f73a0
		state->current = (size_t)(typed % (int64_t)state->list.size());   // :8335
	}

	bool AltLocations_SetGroup(AltLocations *state, bool confirmed, int64_t typed)
	{
		// FUN_004dfa60 :8292-8297: confirmed (the answer 3) and -1 < the number
		if (!confirmed || typed < 0 || typed > 0x7fffffff)
		{
			return false;
		}

		state->group = (int32_t)typed;
		return true;
	}
}
