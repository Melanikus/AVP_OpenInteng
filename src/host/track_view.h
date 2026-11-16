// track_view.h: the lines of a track for the three views of inteng's track and point screens (P10). Plain numbers, no window: a test can drive it. Written
// 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IS READ (run 18, p11-shape-handlers.md 27; menu-reads-18.txt:2-193): inteng's 3-view screen hides every object of the level but the two track
// objects, and the three views show the TRACK in the object's own space, with the centre object at (0, 0, 0). The track's data is the selected object's
// OBJTRAK2 (model/tracks_fields.h: a count of sections, each 76 bytes: two turns, then the places "pivot start" (+32), "pivot end" (+44) and "object
// offset" (+56), three whole numbers each, then a time and a spare number).
//
// WHAT FUN_004f0da0 SHOWS (menu-reads-15.txt:3424-3479; p11-shape-handlers.md 21): the track editor has a CURRENT section, and shows only that section's two
// MARKER OBJECTS: the END marker at the section's end place, the START marker at its start place. So the views draw only the current section: its start and end
// as small crosses (orange) and a line between them. The markers' own shapes, the colours, and the keys that step the current section are NOT read: ours, named.
// (The section's object offset is not part of what FUN_004f0da0 shows: it is not drawn.)

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace host
{
	struct TrackLine
	{
		int32_t from[3];
		int32_t to[3];
		uint32_t colour;   // ARGB
	};

	// The most sections drawn (a section makes up to ten lines); a track with more is cut there
	size_t const kTrackViewMostSections = 400;

	uint32_t const kTrackPivotColour = 0xFFFFA000u;    // ours
	uint32_t const kTrackOffsetColour = 0xFF40E0FFu;   // ours

	// With onlySection of 0 or more, only that section's lines are made (the current section); with -1 every section's (a context drawing, ours). maxAbs and
	// *sectionCount are always about the whole track. sectionCount may be null.
	bool TrackView_Section(uint8_t const *payload, size_t length, int32_t tickHalf, int32_t onlySection, std::vector<TrackLine> *lines, int32_t maxAbs[3], size_t *sectionCount,
		bool hideStart = false);   // hideStart: with one section shown, its start cross is left out (inteng's O key, "Toggle display of section start position")

	// The lines of the track whose OBJTRAK2 payload (the bytes after the chunk's 12-byte heading) is payload, length bytes. tickHalf is half the length of
	// the crosses at each section's pivots and object offset, in the game's units (0: no cross). maxAbs gets the largest distance from the centre on each axis, over every
	// place the track has. Gives back false, with nothing in *lines, for a payload that isn't a size the game's reader takes (a count of sections that the
	// bytes can't hold, or too few bytes) or no payload.
	bool TrackView_Lines(uint8_t const *payload, size_t length, int32_t tickHalf, std::vector<TrackLine> *lines, int32_t maxAbs[3]);
}
