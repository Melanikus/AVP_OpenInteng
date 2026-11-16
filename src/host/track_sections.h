// track_sections.h: a track's sections and sounds as numbers, and what inteng's Track Editing screen does to them (Edit Track 0x36 and page 2 of its menu:
// Add Section 0x518, Delete Section 0x51f, Finish End Pos Edit 0x517, Edit Pivot Point 0x51d, Properties 0x51b / 0x527, Sound 0x524..0x52b, Delete Track
// 0x520). Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// No window and no game types: plain numbers over the bytes of an OBJTRAK2 payload and a TRAKSOUN payload (src/model/tracks_fields.h has the layout), so a test
// can drive it. track_edit.h reads and writes the chunks in the open level (TrackCopy / TrackPasteAbsolute / TrackRemove); track_play_game.h plays a track with the
// game's own code.
//
// WHAT EACH OPERATION COPIES FROM INTENG (AvP-docs\engine\inteng\track-edit-facts.md has every line; functions are in dump-all-3.txt, inteng.exe):
// - ADD SECTION (FUN_004f0e40 with FUN_004f1ac0): a new section goes in AFTER the current one; it starts and ends at the current one's END place and turn, keeps
//   the current one's object offset, and takes 5 seconds (0x50000, FUN_004f1ac0 0x4f1ad7). The section after it keeps its start until the new end is finished.
// - FINISH END POSITION (FUN_004f0cd0): the section's end becomes the place chosen, and the NEXT section (if there is one: no wrapping round) starts there
//   (its start place and turn are the finished end's).
// - DELETE SECTION (FUN_004f11e0; only with two or more): the section after it takes the deleted section's START place and turn; the current section is then
//   the same number (or the last, if the deleted one was the last).
// - PIVOT POINT (FUN_004f3770 / FUN_004f3cf0): the section's object offset is the cursor with the sign turned (offset = -cursor), and it is edited from the
//   cursor (cursor = -offset).
// - SECTION PROPERTIES (FUN_004f1f30): "Time for section", in seconds, kept as 16.16 whole numbers (seconds * 65536); a time not above the smallest is not taken.
//
// MARKERS AND PIVOTS. What the file stores is the section's PIVOT at each end; the place the OBJECT takes there is the pivot plus the object's offset turned by
// the section's turn (the game: avp/track.cpp:352-378; inteng's markers: FUN_004f2020). A person places the OBJECT, so the operations below take a "marker" and work the
// pivot out: pivot = marker - (the object's place - the pivot), the object's place being asked of a TrackObjectPlaceFn, which the editor makes with the GAME's
// own track code (track_play_game.h). With no function (the tests) the pivot is the marker. 490 of the 1312 sections of the 63 stock levels have a non-zero offset,
// so this is not a corner case.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// One section, as the 76 bytes of OBJTRAK2 hold it (model/tracks_fields.h)
	struct TrackSection
	{
		float quatStart[4];       // x y z w, the file's form
		float quatEnd[4];
		int32_t pivotStart[3];
		int32_t pivotEnd[3];
		int32_t offset[3];        // the object's offset from the pivot
		int32_t time;             // 16.16 seconds
		int32_t spare;
	};

	struct TrackData
	{
		std::vector<TrackSection> sections;
		uint32_t flags = 0;       // bit 0 Loop, 1 Playing at start, 2 Loop back and forth, 3 Smoothing; bit 31 is the game's own (kept)
		int32_t timerStart = 0;
	};

	// The sections and flags of an OBJTRAK2 payload (the bytes after the chunk's 12-byte heading). False for a payload the game's reader would refuse.
	bool TrackData_Parse(uint8_t const *payload, size_t length, TrackData *track);

	// The payload for these sections (the count, each section, the flags with the game's bit 31 set, the timer's start)
	void TrackData_Bytes(TrackData const &track, std::vector<uint8_t> *payload);

	// A track of one section that stays at one place with the turn given (inteng's default for an object that has no track: FUN_004eff90 0x4f01f9..0x4f025e):
	// start and end at place, 5 seconds, no flags
	TrackData TrackData_Default(int32_t const place[3], float const quat[4]);

	// 5 seconds, as inteng makes a new section (0x50000)
	int32_t const kTrackNewSectionTime = 0x50000;
	// The smallest time the game's loader takes for a section (model/chunk_fields_tracks.cpp, win95/chunk_object.cpp:1018-1029)
	int32_t const kTrackLeastSectionTime = 65;
	// The most sections a track holds in the editor (a hostile file or a long edit cannot grow without end; the game reads a track of this many)
	size_t const kTrackMostSections = 400;

	// The place the object takes at the start or the end of section `section` of this track (file units), or false when it cannot be asked
	typedef bool (*TrackObjectPlaceFn)(TrackData const &track, size_t section, bool atEnd, int32_t place[3]);

	// The marker (the object's place) at an end of a section: asked of placeFn, or the pivot when there is none
	void TrackData_MarkerAt(TrackData const &track, size_t section, bool atEnd, TrackObjectPlaceFn placeFn, int32_t place[3]);

	// ADD SECTION after section `after`: gives the new section's number (after + 1), or false for no such section or a track at the most
	bool TrackData_AddSection(TrackData *track, size_t after, size_t *added);

	// FINISH END POSITION of section `current` at place (pivot): see above
	bool TrackData_FinishEnd(TrackData *track, size_t current, int32_t const place[3], TrackObjectPlaceFn placeFn);

	// TURN THE END (ours: inteng turns the end marker object with the placement keys): the end turn of the section becomes quat, and the next section (if any) starts with it; the OBJECT stays at
	// the places it has at the end of this section and the start of the next (the pivots move by the change of the turned offset), as inteng's FUN_004f2020 keeps the markers
	// keepObject false (inteng's P key: rotation about the PIVOT, not the centre): the pivots stay and the object swings round them, so the places it takes move
	bool TrackData_SetEndTurn(TrackData *track, size_t current, float const quat[4], TrackObjectPlaceFn placeFn, bool keepObject = true);

	// DELETE SECTION: only with two or more. *newCurrent is the section to show afterwards.
	bool TrackData_DeleteSection(TrackData *track, size_t at, size_t *newCurrent, TrackObjectPlaceFn placeFn);

	// EDIT PIVOT POINT, stopped: the section's object offset is minus the cursor
	// (the pivots of the section keep the object at the same two places, FUN_004f2020, so they move by the change of the turned offset)
	bool TrackData_SetPivotFromCursor(TrackData *track, size_t current, int32_t const cursor[3], TrackObjectPlaceFn placeFn);

	// The cursor to start a pivot edit with: minus the section's object offset
	bool TrackData_CursorForPivot(TrackData const &track, size_t current, int32_t cursor[3]);

	// SECTION PROPERTIES: the time of the section in 16.16 seconds (above kTrackLeastSectionTime)
	bool TrackData_SetTime(TrackData *track, size_t current, int32_t time);

	// TRACK PROPERTIES: the four flag bits a person may choose (mask 0xf); the game's bit 31 and every other bit stay as they were
	bool TrackData_SetFlags(TrackData *track, uint32_t flagBits);

	// ---- sounds: one TRAKSOUN chunk each; index 0 the track's own (while it moves), 1 the start sound, 2 the end sound ----

	struct TrackSound
	{
		uint32_t innerRange = 500000;   // inteng's defaults for a new one (FUN_004f2a60 0x4f2a9d..0x4f2ad0)
		uint32_t outerRange = 1000000;
		int32_t maxVolume = 127;
		int32_t pitch = 0;
		int32_t flags = 0;              // bit 0: loops
		int32_t index = 0;
		std::string wav = "Not Set";
	};

	size_t const kTrackSoundNameMost = 63;

	// A TRAKSOUN payload's numbers and name; false for one the game's reader would refuse (24 bytes of numbers, then the name ending in a zero)
	bool TrackSound_Parse(uint8_t const *payload, size_t length, TrackSound *sound);
	void TrackSound_Bytes(TrackSound const &sound, std::vector<uint8_t> *payload);
}
