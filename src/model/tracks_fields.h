// The tables for the two chunks that hold a LIST of items, each item with numbers of its own: OBJTRAK2, the track of a moving object
// (a door, a lift, a fan, a platform), and ALTLOCAT, the other places an object may start in. Written 2026-11-14 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". field_model.h says what each column of a
// table means.
//
// WHAT'S DIFFERENT HERE. The other chunks' tables (chunk_fields.cpp) are one row of numbers. These two are a few numbers of their own,
// then a list, and every item in the list has numbers of its own. So each chunk has two tables: the chunk's own numbers, and the numbers
// of ONE item (a track's section, or one alternate location). An item's offsets are counted from the start of THAT ITEM, never from the
// start of the chunk: a table of items is never used with a place of its own, only through the functions below, which turn "section 3's
// time" into a place in the payload and refuse any place the game wouldn't read.
//
// WHERE THE FACTS COME FROM. Every layout and limit was read from the game's own reader at the dev pin the editor builds against, AvPx
// 491deb22 (branch editor-dev-pin-491deb22 of the AvP repository); paths are under its 3dc folder (win95/chunk_object.cpp, which has the
// readers, and win95/chunk_object.hpp, which has the layouts). Every limit here is one that loader has, so each is Limit::Loader.
//
// A level can come from anyone, so every function here reads the payload as hostile: a count from the file is measured against the bytes
// that are really there before it sizes or places anything, and a place is worked out so it can't wrap round.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "field_model.h"

namespace model
{
	// ---- OBJTRAK2: a track ----

	// A track is: a count of sections (4 bytes), that many sections of TrackSectionBytes, then its flags (4 bytes) and the start of its
	// timer (4 bytes). The game reads nothing after the timer.
	unsigned const TrackSectionBytes = 76;

	// The track's own numbers: the count of its sections (counted from the start of the payload), then the flags and the timer's start
	// (counted from the end of the last section: Anchor::AfterFirstList).
	extern Field const TrackFields[];
	extern size_t const TrackFieldCount;

	// One section's numbers, counted from the start of that section: its two turns (each four floats), its three places (where the
	// pivot starts, where it ends, and where the object sits from it), how long it takes, and a spare number.
	extern Field const TrackSectionFields[];
	extern size_t const TrackSectionFieldCount;

	// The names of the flags a mapper may choose. The track's flags field says which bits those are (its mask). The game's
	// TrackFlag_QuatProblemSorted bit (0x80000000) isn't one of them: it's kept exactly as the file has it, and has no name here.
	extern ChunkBitName const TrackBits[];
	extern size_t const TrackBitCount;

	// The track's own field of that name, or 0 (also when name is 0).
	Field const *FindTrackField(char const *name);

	// The section field of that name, or 0 (also when name is 0).
	Field const *FindTrackSectionField(char const *name);

	// Checks a track's payload (the bytes after the chunk's 12-byte header) as the game's reader does (chunk_object.cpp:996-1016): the
	// count is there, isn't negative, and counts no more sections than the bytes after it could hold, and then the flags and the timer's
	// start are there after the last section. The game reads only what the count says and ignores any bytes after the timer, so a longer
	// payload is right. A count that's wrong is refused whatever it is, even one so big that its size would wrap round in 32 bits.
	// This checks the SIZE only: a section too quick or a timer too late is a field's limit (ValueIsAllowed), not a size.
	SizeCheck CheckTrackSize(uint8_t const *payload, size_t length);

	// How many sections a track of a right size holds. Gives back false, leaving *count alone, when CheckTrackSize doesn't find it right.
	bool TrackSectionCount(uint8_t const *payload, size_t length, size_t *count);

	// Where section number `section` (the first is 0) starts in the payload. Gives back false, leaving *place alone, when the payload isn't
	// a right size, or the track hasn't that many sections.
	bool TrackSectionPlace(uint8_t const *payload, size_t length, size_t section, size_t *place);

	// Where one of the track's own fields starts. The field has to be one of TrackFields, or this gives back false: a field of another
	// table can't be found at its own offset by mistake. Also false when the payload isn't a right size.
	bool TrackFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t *place);

	// Where one of a section's fields starts, in section number `section`. The field has to be one of TrackSectionFields.
	bool TrackSectionFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t section, size_t *place);

	// ---- ALTLOCAT: an object's alternate locations ----

	// The chunk is: a group number (4 bytes), a spare number (4 bytes), a count of locations (4 bytes), then that many locations of
	// AlternateLocationBytes each. The game reads nothing after the last one. It refuses a chunk of fewer than 8 bytes.
	unsigned const AlternateLocationBytes = 36;

	// The chunk's own numbers, all counted from the start of the payload: the group, the spare, and the count of locations.
	extern Field const AlternateLocationFields[];
	extern size_t const AlternateLocationFieldCount;

	// One location's numbers, counted from the start of that location: a place, a turn (four floats) and two spare numbers.
	extern Field const AlternateLocationEntryFields[];
	extern size_t const AlternateLocationEntryFieldCount;

	// The chunk's own field of that name, or 0 (also when name is 0).
	Field const *FindAlternateLocationField(char const *name);

	// The location field of that name, or 0 (also when name is 0).
	Field const *FindAlternateLocationEntryField(char const *name);

	// Checks the chunk's payload as the game's reader does (chunk_object.cpp:1147-1173): at least 8 bytes, the count there, not negative,
	// and no more locations than the bytes after it could hold. Bytes after the last location are ignored. Like CheckTrackSize, it checks
	// the size only.
	SizeCheck CheckAlternateLocationsSize(uint8_t const *payload, size_t length);

	// How many locations a chunk of a right size holds. Gives back false, leaving *count alone, when the size isn't right.
	bool AlternateLocationCount(uint8_t const *payload, size_t length, size_t *count);

	// Where location number `location` (the first is 0) starts in the payload. Gives back false when the payload isn't a right size or
	// there's no such location.
	bool AlternateLocationPlace(uint8_t const *payload, size_t length, size_t location, size_t *place);

	// Where one of the chunk's own fields starts. The field has to be one of AlternateLocationFields.
	bool AlternateLocationFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t *place);

	// Where one of a location's fields starts, in location number `location`. The field has to be one of AlternateLocationEntryFields.
	bool AlternateLocationEntryFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t location, size_t *place);

	// ---- Setting one number, and the upgrade of an old track ----

	// The reason the setters below give for a field that isn't one of the table's own: at its own offset it would land in a part of the
	// payload it doesn't belong to (a section's field among the track's own numbers, say, a copy of a field, or another chunk's field).
	extern char const NotThisTablesReason[];

	// Each setter puts one checked value into a payload, at exactly the place the matching place function above gives, and gives back
	// true; or it gives back false and changes nothing. They keep the rules of the editor's other setters (SetField, SetChunkField and
	// SetAiField), in this order:
	// - a field that isn't one of the table's own is refused first, with NotThisTablesReason;
	// - then the value is checked with ValueIsAllowed (the field's storage and its limit). One that isn't allowed is refused with that
	//   reason, even where the next rule would refuse the place too;
	// - a list's count is refused with ListCountReason: it changes only when items are added or taken away;
	// - no payload, a payload whose size the game's reader would refuse (CheckTrackSize, CheckAlternateLocationsSize), or a section or
	//   location the payload doesn't hold, is refused with no reason of its own, as the other setters refuse a field with no place;
	// - a place (three numbers) can't be set from one number: the ...Axis setters set one of its numbers (axis 0, 1 or 2: x, y or z),
	//   leave the other two as they were, and refuse any field that isn't a place.
	// A reason is put in *why only when why isn't 0. A part of a turn is a field of its own (quat_start_x and the rest), so the plain
	// setters set it. A field labelled "" is set like any other when it's asked for (the track and location modes will move places and
	// turns); the panel just doesn't offer it. The track's flags change only in the four bits a mapper chooses: bit 31 and the bits the
	// game doesn't use keep what the payload has. Nothing is ever written outside the field's own bytes. The other values already in the
	// payload aren't checked here: the game's loader checks every one when a level is read, and each setter checks the one it writes.
	bool SetTrackField(Field const &field, uint8_t *payload, size_t length, double value, char const **why);
	bool SetTrackSectionField(Field const &field, uint8_t *payload, size_t length, size_t section, double value, char const **why);
	bool SetTrackSectionFieldAxis(Field const &field, uint8_t *payload, size_t length, size_t section, int axis, int32_t value,
	                              char const **why);
	bool SetAlternateLocationField(Field const &field, uint8_t *payload, size_t length, double value, char const **why);
	bool SetAlternateLocationEntryField(Field const &field, uint8_t *payload, size_t length, size_t location, double value,
	                                    char const **why);
	bool SetAlternateLocationEntryFieldAxis(Field const &field, uint8_t *payload, size_t length, size_t location, int axis,
	                                        int32_t value, char const **why);

	// The game's upgrade of an old track, done the same way when a level is read. A track whose flags lack bit 31
	// (TrackFlag_QuatProblemSorted, win95/chunk_object.hpp:396) holds its turns in an older form: the game's reader rewrites both turns of
	// every section and then sets the bit (win95/chunk_object.cpp:1045-1062), and this makes the same bytes. A track with the bit set, no
	// payload, or a payload whose size CheckTrackSize refuses, is left as it is. Gives back whether it changed anything.
	bool UpgradeOldTrack(uint8_t *payload, size_t length);
}
