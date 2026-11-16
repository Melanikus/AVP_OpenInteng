// Reading the numbers of a track out of a payload, to show them. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// tracks_fields.h says where each of a track's fields is (the ...Place functions) and refuses a payload the game's reader would refuse.
// The functions here read a field at the place it gives, so a field is read only where the game reads it, from a payload of a size the
// game reads, and from a section the list holds: nothing is read past the end of the payload, whatever its count says. They are the
// inverses of the setters (plans/frost-tracks-setters.md), with the same rule: the field has to be one of the table's own, so a field of
// another table can't be read at its own offset by mistake.
//
// WHAT'S READ IS THE LEVEL'S, so it is hostile, and it's given back exactly as the file holds it: a fraction may be "not a number", and a
// number may be outside its limit. A field with a mask is given with its mask applied and not shifted. When anything is refused the
// value is left as it was.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "tracks_fields.h"

namespace model
{
	// One of the track's own fields (its flags, the start of its timer) as one number. Gives back false, leaving *value alone, when there's
	// no payload or no place for the value, the field isn't one of TrackFields, the payload isn't a size the game reads (CheckTrackSize), or
	// the field isn't one number (a field of three numbers has the Axis reader below).
	bool ReadTrackField(Field const &field, uint8_t const *payload, size_t length, double *value);

	// One of section number `section`'s fields (the first section is 0) as one number: its time, or a part of a turn. The same refusals, and
	// also a section the track doesn't hold.
	bool ReadTrackSectionField(Field const &field, uint8_t const *payload, size_t length, size_t section, double *value);

	// One number of a section's field of three numbers (where its pivot starts or ends, or where the object sits from it): axis 0, 1 or 2
	// is x, y or z. The same refusals, and also a field that isn't of three numbers, or an axis that isn't 0, 1 or 2.
	bool ReadTrackSectionFieldAxis(Field const &field, uint8_t const *payload, size_t length, size_t section, int axis, int32_t *value);

	// ---- ALTLOCAT: alternate locations ----
	//
	// The same readers for the chunk of an object's alternate locations (tracks_fields.h: AlternateLocationFields for the chunk's own
	// numbers, AlternateLocationEntryFields for one location's), with the same rules: the field has to be one of the table's own, the payload
	// a size the game reads (CheckAlternateLocationsSize), and the location one the chunk holds. A location's place (a field of three
	// numbers) has no reader yet: no field of a location that has a label is a place, so nothing asks for one, and the day one is added the
	// record builder refuses it, naming it. A location's turn is four fields of one number each.

	// One of the chunk's own fields (its group number, its spare, its count of locations) as one number.
	bool ReadAlternateLocationField(Field const &field, uint8_t const *payload, size_t length, double *value);

	// One of location number `location`'s fields (the first location is 0) as one number: a part of its turn, or a spare.
	bool ReadAlternateLocationEntryField(Field const &field, uint8_t const *payload, size_t length, size_t location, double *value);
}
