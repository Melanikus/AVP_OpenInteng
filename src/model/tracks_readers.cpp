// See tracks_readers.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "tracks_readers.h"

#include "field_bytes.h"

namespace model
{
	bool ReadTrackField(Field const &field, uint8_t const *payload, size_t length, double *value)
	{
		size_t place = 0;

		// The place functions read the payload's count, so a payload that isn't there is refused before they're asked
		if (!payload || !TrackFieldPlace(field, payload, length, &place))
		{
			return false;
		}

		return detail::ReadFieldAt(field, payload, length, place, value);
	}

	bool ReadTrackSectionField(Field const &field, uint8_t const *payload, size_t length, size_t section, double *value)
	{
		size_t place = 0;

		if (!payload || !TrackSectionFieldPlace(field, payload, length, section, &place))
		{
			return false;
		}

		return detail::ReadFieldAt(field, payload, length, place, value);
	}

	bool ReadTrackSectionFieldAxis(Field const &field, uint8_t const *payload, size_t length, size_t section, int axis, int32_t *value)
	{
		size_t place = 0;

		if (!payload || !TrackSectionFieldPlace(field, payload, length, section, &place))
		{
			return false;
		}

		return detail::ReadAxisAt(field, payload, length, place, axis, value);
	}

	bool ReadAlternateLocationField(Field const &field, uint8_t const *payload, size_t length, double *value)
	{
		size_t place = 0;

		if (!payload || !AlternateLocationFieldPlace(field, payload, length, &place))
		{
			return false;
		}

		return detail::ReadFieldAt(field, payload, length, place, value);
	}

	bool ReadAlternateLocationEntryField(Field const &field, uint8_t const *payload, size_t length, size_t location, double *value)
	{
		size_t place = 0;

		if (!payload || !AlternateLocationEntryFieldPlace(field, payload, length, location, &place))
		{
			return false;
		}

		return detail::ReadFieldAt(field, payload, length, place, value);
	}
}
