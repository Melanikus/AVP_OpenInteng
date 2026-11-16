// See sequence_speed.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "sequence_speed.h"

#include <string.h>

namespace model
{
	char const *SequenceSpeedWords(SequenceSpeedResult result)
	{
		switch (result)
		{
		case SequenceSpeedResult::Ok: return "done";
		case SequenceSpeedResult::BadCall: return "nothing to read or change";
		case SequenceSpeedResult::TooShort: return "the sequence speed chunk is shorter than its three numbers";
		case SequenceSpeedResult::NotAccepted: return "that number is not one the platform speed or angle takes";
		case SequenceSpeedResult::WrongSize: return "the bytes are shorter than the sequence speed chunk's three numbers";
		}

		return "unknown answer";
	}

	SequenceSpeedResult ReadSequenceSpeed(uint8_t const *payload, size_t size, SequenceSpeed *out)
	{
		if (!out || (!payload && size))
		{
			return SequenceSpeedResult::BadCall;
		}

		if (size < kSequenceSpeedBytes)
		{
			return SequenceSpeedResult::TooShort;
		}

		SequenceSpeed read;

		memcpy(&read.speedMmPerSecond, payload, 4);
		memcpy(&read.angleDegrees, payload + 4, 4);
		memcpy(&read.spare, payload + 8, 4);
		*out = read;
		return SequenceSpeedResult::Ok;
	}

	SequenceSpeedResult PutSequenceSpeed(SequenceSpeed const &value, std::vector<uint8_t> *payload)
	{
		if (!payload)
		{
			return SequenceSpeedResult::BadCall;
		}

		if (payload->size() < kSequenceSpeedBytes)
		{
			return SequenceSpeedResult::WrongSize;
		}

		memcpy(payload->data(), &value.speedMmPerSecond, 4);
		memcpy(payload->data() + 4, &value.angleDegrees, 4);
		memcpy(payload->data() + 8, &value.spare, 4);
		return SequenceSpeedResult::Ok;
	}

	SequenceSpeedResult SetPlatformSpeed(PlatformState *state, int32_t typed)
	{
		if (!state)
		{
			return SequenceSpeedResult::BadCall;
		}

		if (typed < 0)
		{
			return SequenceSpeedResult::NotAccepted;
		}

		state->lastSpeed = typed;
		return SequenceSpeedResult::Ok;
	}

	SequenceSpeedResult SetPlatformAngle(PlatformState *state, int32_t typed, int32_t *displayTurn)
	{
		if (!state)
		{
			return SequenceSpeedResult::BadCall;
		}

		if (typed < 0 || typed > 360)
		{
			return SequenceSpeedResult::NotAccepted;
		}

		state->lastAngle = typed;

		if (displayTurn)
		{
			*displayTurn = (typed << 12) / 0x168;
		}

		return SequenceSpeedResult::Ok;
	}

	void ApplyPlatformState(PlatformState const &state, SequenceSpeed *chunk)
	{
		if (chunk)
		{
			chunk->speedMmPerSecond = state.lastSpeed;
			chunk->angleDegrees = state.lastAngle;
		}
	}
}
