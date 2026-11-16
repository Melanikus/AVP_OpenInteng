// alt_locations.h: an object's alternate locations (ALTLOCAT) as inteng keeps them while it edits: a list of places with a CURRENT one, a group
// number, and the exact things inteng's Alt Locations items do to that list, with no screen and no game. Written 2026-11-15 by Melanikus (Frost)
// for AVP_OpenInteng (P9, the data side). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE EVERY RULE COMES FROM (out\ = re-tools\projects\vasquez-inteng\out):
// - THE FILE (LOAD): FUN_004d3950 (out\menu-reads-16.txt): the chunk's first dword goes to the editor object's +0x30 (the group number), the second to
//   +0x34 (a spare), the third is the count of entries, then that many entries of 9 ints = 36 bytes, copied as they are. Nothing in them is judged.
// - THE FILE (SAVE): FUN_004dfc70 (menu-reads-16.txt): it first destroys any ALTLOCAT the object has; when the list is EMPTY it makes none, so a
//   level with no locations has no ALTLOCAT chunk; otherwise it makes one, with the group number as dword 0, the spare as dword 1, the count,
//   and one entry per node: ints 0-2 the place, ints 3-6 the turn (four floats), ints 7 and 8 = 0.
// - DELETE CURRENT: FUN_004df6b0 (out\menu-reads-2.txt:8053-8121). Add new: FUN_004df820 (:8124-8165). Next: FUN_004df8c0 (:8167-8219). Prev:
//   FUN_004df990 (:8221-8273). Assign group number: FUN_004dfa60 (:8275-8303).
//
// The list holds the entries in FILE form (a place in file units and four floats for the turn). inteng's own list holds a turn as a matrix and turns it
// into four floats when it saves (FUN_004de070); here a caller that has the object's present place and turn in file form gives them where a function
// below needs "the object as it is now".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace model
{
	// One alternate location: 36 bytes in the file (win95/chunk_object.hpp: AltObjectLocation)
	struct AltLocation
	{
		AltLocation() { place[0] = place[1] = place[2] = 0; turn[0] = turn[1] = turn[2] = 0.0f; turn[3] = 1.0f; spare[0] = spare[1] = 0; }

		int32_t place[3];
		float turn[4];     // x, y, z, w
		int32_t spare[2];  // ints 7 and 8: as loaded; 0 in a new one (FUN_004dfc70)
	};

	// The editor's state for one object's alternate locations
	struct AltLocations
	{
		AltLocations() : group(0), spare(0), current(0) {}

		int32_t group;                   // dword 0: 0 means no group (the prompt of FUN_004dfa60 says so)
		int32_t spare;                   // dword 1: kept as it is
		std::vector<AltLocation> list;   // the entries, in order
		size_t current;                  // the editor's current one (inteng's +0x34): not in the file; 0 when a level is loaded
	};

	// LOAD (FUN_004d3950): the group, the spare and the entries from a chunk's payload (after its 12-byte heading). False for a payload of fewer than
	// 12 bytes, a count below 0, or a count that the bytes after it can't hold (the game's reader refuses the same: ReadListCount). A payload with
	// bytes after the last entry loads: the game reads nothing after it.
	bool AltLocations_Parse(uint8_t const *payload, size_t length, AltLocations *out);

	// SAVE (FUN_004dfc70): the chunk's payload for this state. Gives back false and leaves *payload empty when the list is EMPTY: then the object has no
	// ALTLOCAT at all. Otherwise true, and the payload is 12 + 36 x count bytes.
	bool AltLocations_Build(AltLocations const &state, std::vector<uint8_t> *payload);

	// Delete Current (FUN_004df6b0): does nothing for an empty list. Otherwise takes out the current location; when that leaves ONE location, it is taken
	// out too (so the list is empty and the object has no alternate locations); then, if the count is now not above the current number, the current
	// number is 0. The new current location is then put on the object by the caller (FUN_004dfae0): it is state.list[state.current], when there is one.
	void AltLocations_DeleteCurrent(AltLocations *state);

	// Add new (FUN_004df820): the object as it is now (`now`) is stored. With no location yet, the list becomes TWO copies of `now` (inteng stores
	// the object's own place as the first, then adds the new one); with some, `now` replaces the current location, and one more copy of `now` is added
	// at the END. The current number is then the last. Gives back false, changing nothing, when one more would make a chunk whose size is not a
	// 32-bit number (kMostAltLocations): inteng's own list has no limit, the file's size field is the one.
	bool AltLocations_Add(AltLocations *state, AltLocation const &now);

	// Next (FUN_004df8c0) and Prev (FUN_004df990): the object as it is now replaces the current location (FUN_004f73a0), then the current number moves
	// by one, round the end: Next goes to (current + 1) % count; Prev to current - 1, or the last when that is below 0. Nothing for an empty list.
	// The new current location is then put on the object by the caller.
	void AltLocations_Next(AltLocations *state, AltLocation const &now);
	void AltLocations_Prev(AltLocations *state, AltLocation const &now);

	// Change shown location (FUN_004dfb90, out/menu-reads-2.txt:8305-8359, for one object): the object as it is now replaces the current location
	// (FUN_004f73a0), then the current number is the typed number modulo the count. Nothing for an empty list. The location is then put on the object.
	void AltLocations_JumpTo(AltLocations *state, AltLocation const &now, int64_t typed);

	// Assign Group Number (FUN_004dfa60): the typed number replaces the group only when the prompt was confirmed and the number is 0 or more
	// (FUN_004dfa60: `if (-1 < value)`). Gives back whether it was taken.
	bool AltLocations_SetGroup(AltLocations *state, bool confirmed, int64_t typed);

	// How many locations a chunk can hold: its size (12 for the heading, 12 for the three numbers, 36 each) has to fit in 32 bits
	size_t const kMostAltLocations = (size_t)(0x7fffffff - 12 - 12) / 36;
}
