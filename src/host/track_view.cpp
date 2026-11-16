// See track_view.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P10). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "track_view.h"

#include <string.h>

namespace host
{
	namespace
	{
		size_t const SectionBytes = 76;        // TrackSectionBytes (model/tracks_fields.h)
		size_t const PivotStartAt = 32;
		size_t const PivotEndAt = 44;
		size_t const ObjectOffsetAt = 56;

		int32_t IntAt(uint8_t const *bytes)
		{
			int32_t value;
			memcpy(&value, bytes, sizeof(value));
			return value;
		}

		int32_t Abs(int32_t value)
		{
			return value == INT32_MIN ? INT32_MAX : (value < 0 ? -value : value);
		}

		// Held to a 31-bit sum: a place plus half a cross never wraps
		int32_t Plus(int32_t a, int32_t b)
		{
			int64_t const sum = (int64_t)a + b;
			return sum > INT32_MAX ? INT32_MAX : (sum < INT32_MIN ? INT32_MIN : (int32_t)sum);
		}
	}

	bool TrackView_Lines(uint8_t const *payload, size_t length, int32_t tickHalf, std::vector<TrackLine> *lines, int32_t maxAbs[3])
	{
		return TrackView_Section(payload, length, tickHalf, -1, lines, maxAbs, nullptr);
	}

	bool TrackView_Section(uint8_t const *payload, size_t length, int32_t tickHalf, int32_t onlySection, std::vector<TrackLine> *lines, int32_t maxAbs[3],
		size_t *sectionCount, bool hideStart)
	{
		if (sectionCount)
		{
			*sectionCount = 0;
		}

		if (!lines || !maxAbs)
		{
			return false;
		}

		lines->clear();
		maxAbs[0] = maxAbs[1] = maxAbs[2] = 0;

		// A count, that many sections, then the flags and the timer's start (12 bytes in all besides the sections): the size the game's reader takes
		if (!payload || length < 12)
		{
			return false;
		}

		int32_t const count = IntAt(payload);

		if (count < 0 || (uint64_t)count > (length - 12) / SectionBytes)
		{
			return false;
		}

		size_t const drawn = (size_t)count < kTrackViewMostSections ? (size_t)count : kTrackViewMostSections;

		if (sectionCount)
		{
			*sectionCount = (size_t)count;
		}

		for (size_t s = 0; s < drawn; s++)
		{
			uint8_t const *const section = payload + 4 + s * SectionBytes;
			int32_t start[3];
			int32_t end[3];
			int32_t offset[3];

			for (int axis = 0; axis < 3; axis++)
			{
				start[axis] = IntAt(section + PivotStartAt + 4 * axis);
				end[axis] = IntAt(section + PivotEndAt + 4 * axis);
				offset[axis] = IntAt(section + ObjectOffsetAt + 4 * axis);
				maxAbs[axis] = Abs(start[axis]) > maxAbs[axis] ? Abs(start[axis]) : maxAbs[axis];
				maxAbs[axis] = Abs(end[axis]) > maxAbs[axis] ? Abs(end[axis]) : maxAbs[axis];
				maxAbs[axis] = Abs(offset[axis]) > maxAbs[axis] ? Abs(offset[axis]) : maxAbs[axis];
			}

			if (onlySection >= 0 && (size_t)onlySection != s)
			{
				continue;
			}

			TrackLine pivot;

			memcpy(pivot.from, start, sizeof(start));
			memcpy(pivot.to, end, sizeof(end));
			pivot.colour = kTrackPivotColour;
			lines->push_back(pivot);

			// a cross at each pivot, and one at the object offset
			int32_t const *const crossAt[3] = { start, end, offset };
			uint32_t const crossColour[3] = { kTrackPivotColour, kTrackPivotColour, kTrackOffsetColour };

			// With one section shown, its markers are the start and the end only (FUN_004f0da0): no cross at the object offset
			for (int cross = 0; cross < (onlySection >= 0 ? 2 : 3) && tickHalf > 0; cross++)
			{
				if (cross == 0 && hideStart && onlySection >= 0)
				{
					continue;
				}

				for (int axis = 0; axis < 3; axis++)
				{
					TrackLine tick;

					memcpy(tick.from, crossAt[cross], sizeof(tick.from));
					memcpy(tick.to, crossAt[cross], sizeof(tick.to));
					tick.from[axis] = Plus(crossAt[cross][axis], -tickHalf);
					tick.to[axis] = Plus(crossAt[cross][axis], tickHalf);
					tick.colour = crossColour[cross];
					lines->push_back(tick);
				}
			}
		}

		return true;
	}
}
