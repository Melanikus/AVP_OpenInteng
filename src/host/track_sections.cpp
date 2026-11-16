// See track_sections.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "track_sections.h"

#include <math.h>
#include <string.h>

#include "tracks_fields.h"

namespace host
{
	namespace
	{
		size_t const SectionBytes = model::TrackSectionBytes;   // 76
		uint32_t const QuatSortedBit = 0x80000000u;
		int64_t const kMarkerSlack = 3;                        // file units
		int32_t const PlaceMost = 16777216;                    // CheckPlace: within 2^24 of the level's middle on each axis (model/chunk_fields_tracks.cpp)

		int32_t IntAt(uint8_t const *at)
		{
			int32_t value;
			memcpy(&value, at, sizeof(value));
			return value;
		}

		float FloatAt(uint8_t const *at)
		{
			float value;
			memcpy(&value, at, sizeof(value));
			return value;
		}

		void PutInt(std::vector<uint8_t> *to, int32_t value)
		{
			uint8_t bytes[4];

			memcpy(bytes, &value, 4);
			to->insert(to->end(), bytes, bytes + 4);
		}

		void PutFloat(std::vector<uint8_t> *to, float value)
		{
			uint8_t bytes[4];

			memcpy(bytes, &value, 4);
			to->insert(to->end(), bytes, bytes + 4);
		}

		bool PlaceIsInside(int32_t const place[3])
		{
			for (int axis = 0; axis < 3; axis++)
			{
				if (place[axis] < -PlaceMost || place[axis] > PlaceMost)
				{
					return false;
				}
			}

			return true;
		}
	}

	bool TrackData_Parse(uint8_t const *payload, size_t length, TrackData *track)
	{
		size_t count = 0;

		if (!payload || !track || !model::TrackSectionCount(payload, length, &count))
		{
			return false;
		}

		// A track whose turns are still in the older form (bit 31 not set) is changed to the game's form first, as the game's reader does
		std::vector<uint8_t> copy(payload, payload + length);

		model::UpgradeOldTrack(copy.data(), copy.size());

		TrackData made;
		uint8_t const *at = copy.data() + 4;

		made.sections.resize(count);

		for (size_t i = 0; i < count; i++, at += SectionBytes)
		{
			TrackSection &s = made.sections[i];

			for (int k = 0; k < 4; k++)
			{
				s.quatStart[k] = FloatAt(at + 4 * k);
				s.quatEnd[k] = FloatAt(at + 16 + 4 * k);
			}

			for (int axis = 0; axis < 3; axis++)
			{
				s.pivotStart[axis] = IntAt(at + 32 + 4 * axis);
				s.pivotEnd[axis] = IntAt(at + 44 + 4 * axis);
				s.offset[axis] = IntAt(at + 56 + 4 * axis);
			}

			s.time = IntAt(at + 68);
			s.spare = IntAt(at + 72);
		}

		made.flags = (uint32_t)IntAt(at);
		made.timerStart = IntAt(at + 4);
		*track = made;
		return true;
	}

	void TrackData_Bytes(TrackData const &track, std::vector<uint8_t> *payload)
	{
		payload->clear();
		PutInt(payload, (int32_t)track.sections.size());

		for (TrackSection const &s : track.sections)
		{
			for (int k = 0; k < 4; k++)
			{
				PutFloat(payload, s.quatStart[k]);
			}

			for (int k = 0; k < 4; k++)
			{
				PutFloat(payload, s.quatEnd[k]);
			}

			for (int axis = 0; axis < 3; axis++)
			{
				PutInt(payload, s.pivotStart[axis]);
			}

			for (int axis = 0; axis < 3; axis++)
			{
				PutInt(payload, s.pivotEnd[axis]);
			}

			for (int axis = 0; axis < 3; axis++)
			{
				PutInt(payload, s.offset[axis]);
			}

			PutInt(payload, s.time);
			PutInt(payload, s.spare);
		}

		// inteng saves the flags with bit 31 set (FUN_004f1300 0x4f1393: flags | 0x80000000), and the game's reader leaves a track that has it as it is
		PutInt(payload, (int32_t)(track.flags | QuatSortedBit));
		PutInt(payload, track.timerStart);
	}

	TrackData TrackData_Default(int32_t const place[3], float const quat[4])
	{
		TrackData made;
		TrackSection s;

		memset(&s, 0, sizeof(s));

		for (int k = 0; k < 4; k++)
		{
			s.quatStart[k] = quat[k];
			s.quatEnd[k] = quat[k];
		}

		for (int axis = 0; axis < 3; axis++)
		{
			s.pivotStart[axis] = place[axis];
			s.pivotEnd[axis] = place[axis];
		}

		s.time = kTrackNewSectionTime;
		made.sections.push_back(s);
		made.flags = 0;
		made.timerStart = 0;
		return made;
	}

	void TrackData_MarkerAt(TrackData const &track, size_t section, bool atEnd, TrackObjectPlaceFn placeFn, int32_t place[3])
	{
		if (placeFn && placeFn(track, section, atEnd, place))
		{
			return;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			place[axis] = atEnd ? track.sections[section].pivotEnd[axis] : track.sections[section].pivotStart[axis];
		}
	}

	namespace
	{
		// The pivot at an end of a section that puts the object at marker: marker - (the object's place - the pivot). False for one outside the level.
		bool PivotForMarker(TrackData const &track, size_t section, bool atEnd, int32_t const marker[3], TrackObjectPlaceFn placeFn, int32_t pivot[3])
		{
			int32_t now[3];
			int32_t const *const have = atEnd ? track.sections[section].pivotEnd : track.sections[section].pivotStart;

			TrackData_MarkerAt(track, section, atEnd, placeFn, now);

			// The game's fixed-point maths puts the object a unit or two off the exact place, so a marker this close to where the object already is
			// means "leave it": a Finish that moved nothing changes nothing
			bool near = true;

			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const gap = (int64_t)marker[axis] - (int64_t)now[axis];

				near = near && gap >= -kMarkerSlack && gap <= kMarkerSlack;
			}

			if (near)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					pivot[axis] = have[axis];
				}

				return true;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const made = (int64_t)marker[axis] - ((int64_t)now[axis] - (int64_t)have[axis]);

				if (made < -PlaceMost || made > PlaceMost)
				{
					return false;
				}

				pivot[axis] = (int32_t)made;
			}

			return true;
		}
	}

	bool TrackData_AddSection(TrackData *track, size_t after, size_t *added)
	{
		if (!track || after >= track->sections.size() || track->sections.size() >= kTrackMostSections)
		{
			return false;
		}

		TrackSection const here = track->sections[after];
		TrackSection s;

		memset(&s, 0, sizeof(s));

		for (int k = 0; k < 4; k++)
		{
			s.quatStart[k] = here.quatEnd[k];
			s.quatEnd[k] = here.quatEnd[k];
		}

		for (int axis = 0; axis < 3; axis++)
		{
			s.pivotStart[axis] = here.pivotEnd[axis];
			s.pivotEnd[axis] = here.pivotEnd[axis];
			s.offset[axis] = here.offset[axis];
		}

		s.time = kTrackNewSectionTime;
		s.spare = here.spare;
		track->sections.insert(track->sections.begin() + (std::ptrdiff_t)(after + 1), s);

		if (added)
		{
			*added = after + 1;
		}

		return true;
	}

	bool TrackData_FinishEnd(TrackData *track, size_t current, int32_t const place[3], TrackObjectPlaceFn placeFn)
	{
		if (!track || current >= track->sections.size() || !PlaceIsInside(place))
		{
			return false;
		}

		TrackData made = *track;
		TrackSection &s = made.sections[current];
		int32_t pivot[3];

		if (!PivotForMarker(made, current, true, place, placeFn, pivot))
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			s.pivotEnd[axis] = pivot[axis];
		}

		// the next section (no wrapping round: FUN_004f0cd0 0x4f0d3a is "less than the count") starts where this one now ends, with its turn
		if (current + 1 < track->sections.size())
		{
			TrackSection &next = made.sections[current + 1];

			for (int k = 0; k < 4; k++)
			{
				next.quatStart[k] = s.quatEnd[k];
			}

			if (!PivotForMarker(made, current + 1, false, place, placeFn, pivot))
			{
				return false;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				next.pivotStart[axis] = pivot[axis];
			}
		}

		*track = made;
		return true;
	}

	bool TrackData_SetEndTurn(TrackData *track, size_t current, float const quat[4], TrackObjectPlaceFn placeFn, bool keepObject)
	{
		if (!track || current >= track->sections.size())
		{
			return false;
		}

		for (int k = 0; k < 4; k++)
		{
			if (!(quat[k] >= -2.0f && quat[k] <= 2.0f))   // the loader's limit for a turn's part (CheckTurn); also refuses NaN
			{
				return false;
			}
		}

		TrackData made = *track;
		int32_t endMarker[3], nextMarker[3];
		bool const hasNext = current + 1 < made.sections.size();

		TrackData_MarkerAt(made, current, true, placeFn, endMarker);

		if (hasNext)
		{
			TrackData_MarkerAt(made, current + 1, false, placeFn, nextMarker);
		}

		for (int k = 0; k < 4; k++)
		{
			made.sections[current].quatEnd[k] = quat[k];

			if (hasNext)
			{
				made.sections[current + 1].quatStart[k] = quat[k];
			}
		}

		int32_t pivot[3];

		if (!keepObject)
		{
			*track = made;
			return true;
		}

		if (!PivotForMarker(made, current, true, endMarker, placeFn, pivot))
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			made.sections[current].pivotEnd[axis] = pivot[axis];
		}

		if (hasNext)
		{
			if (!PivotForMarker(made, current + 1, false, nextMarker, placeFn, pivot))
			{
				return false;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				made.sections[current + 1].pivotStart[axis] = pivot[axis];
			}
		}

		*track = made;
		return true;
	}

	bool TrackData_DeleteSection(TrackData *track, size_t at, size_t *newCurrent, TrackObjectPlaceFn placeFn)
	{
		if (!track || track->sections.size() < 2 || at >= track->sections.size())
		{
			return false;
		}

		TrackSection const gone = track->sections[at];
		int32_t goneStart[3];

		TrackData_MarkerAt(*track, at, false, placeFn, goneStart);
		track->sections.erase(track->sections.begin() + (std::ptrdiff_t)at);

		size_t current = at;

		if (at < track->sections.size())
		{
			// the section after takes the deleted one's start place (the object's, as the marker FUN_004f11e0 copies) and turn (0x4f1246..0x4f126c)
			TrackSection &next = track->sections[at];
			int32_t pivot[3];

			for (int k = 0; k < 4; k++)
			{
				next.quatStart[k] = gone.quatStart[k];
			}

			if (PivotForMarker(*track, at, false, goneStart, placeFn, pivot))
			{
				for (int axis = 0; axis < 3; axis++)
				{
					next.pivotStart[axis] = pivot[axis];
				}
			}
			else
			{
				for (int axis = 0; axis < 3; axis++)
				{
					next.pivotStart[axis] = gone.pivotStart[axis];
				}
			}
		}
		else
		{
			current = at - 1;
		}

		if (newCurrent)
		{
			*newCurrent = current;
		}

		return true;
	}

	bool TrackData_SetPivotFromCursor(TrackData *track, size_t current, int32_t const cursor[3], TrackObjectPlaceFn placeFn)
	{
		if (!track || current >= track->sections.size())
		{
			return false;
		}

		int32_t offset[3];

		for (int axis = 0; axis < 3; axis++)
		{
			if (cursor[axis] == INT32_MIN)
			{
				return false;
			}

			offset[axis] = -cursor[axis];
		}

		if (!PlaceIsInside(offset))
		{
			return false;
		}

		// the object stays at the two places it had (FUN_004f2020 works the pivots out again from the markers), so the pivots move by the change of the turned offset
		int32_t startMarker[3], endMarker[3];

		TrackData_MarkerAt(*track, current, false, placeFn, startMarker);
		TrackData_MarkerAt(*track, current, true, placeFn, endMarker);

		TrackData made = *track;

		for (int axis = 0; axis < 3; axis++)
		{
			made.sections[current].offset[axis] = offset[axis];
		}

		int32_t startPivot[3], endPivot[3];

		if (!PivotForMarker(made, current, false, startMarker, placeFn, startPivot) || !PivotForMarker(made, current, true, endMarker, placeFn, endPivot))
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			made.sections[current].pivotStart[axis] = startPivot[axis];
			made.sections[current].pivotEnd[axis] = endPivot[axis];
		}

		*track = made;
		return true;
	}

	bool TrackData_CursorForPivot(TrackData const &track, size_t current, int32_t cursor[3])
	{
		if (current >= track.sections.size())
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			cursor[axis] = -track.sections[current].offset[axis];
		}

		return true;
	}

	bool TrackData_SetTime(TrackData *track, size_t current, int32_t time)
	{
		if (!track || current >= track->sections.size() || time < kTrackLeastSectionTime)
		{
			return false;
		}

		track->sections[current].time = time;
		return true;
	}

	bool TrackData_SetFlags(TrackData *track, uint32_t flagBits)
	{
		if (!track || (flagBits & ~0xfu) != 0)
		{
			return false;
		}

		track->flags = (track->flags & ~0xfu) | flagBits;
		return true;
	}

	bool TrackSound_Parse(uint8_t const *payload, size_t length, TrackSound *sound)
	{
		if (!payload || !sound || length < 25)
		{
			return false;
		}

		TrackSound made;

		memcpy(&made.innerRange, payload, 4);
		memcpy(&made.outerRange, payload + 4, 4);
		made.maxVolume = IntAt(payload + 8);
		made.pitch = IntAt(payload + 12);
		made.flags = IntAt(payload + 16);
		made.index = IntAt(payload + 20);

		if (made.index < 0 || made.index > 2)
		{
			return false;
		}

		uint8_t const *const zero = (uint8_t const *)memchr(payload + 24, 0, length - 24);

		if (!zero)
		{
			return false;
		}

		made.wav.assign((char const *)payload + 24, (size_t)(zero - (payload + 24)));
		*sound = made;
		return true;
	}

	void TrackSound_Bytes(TrackSound const &sound, std::vector<uint8_t> *payload)
	{
		payload->clear();
		payload->insert(payload->end(), (uint8_t const *)&sound.innerRange, (uint8_t const *)&sound.innerRange + 4);
		payload->insert(payload->end(), (uint8_t const *)&sound.outerRange, (uint8_t const *)&sound.outerRange + 4);
		PutInt(payload, sound.maxVolume);
		PutInt(payload, sound.pitch);
		PutInt(payload, sound.flags);
		PutInt(payload, sound.index);
		payload->insert(payload->end(), sound.wav.begin(), sound.wav.end());

		// the name and its zero, padded to a whole number of 4 bytes as the game's size_chunk does (win95/chunk_object.cpp:1142: (strlen + 4) & ~3)
		size_t const nameBytes = (sound.wav.size() + 4) & ~(size_t)3;

		payload->resize(24 + nameBytes, 0);
	}
}
