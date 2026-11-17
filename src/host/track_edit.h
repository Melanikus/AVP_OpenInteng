// track_edit.h: Copy Track and Paste Track Absolute (Object Placement, commands 0x52c and 0x52e; P10). Written 2026-11-15 by Melanikus (Dietrich)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// MATCHES inteng's FUN_004f2ff0 (Copy Track) and FUN_004f3330 (Paste Track), AvP-docs\engine\inteng, out\menu-reads-2.txt:8685 and :8814:
// - COPY: the selected object must hold an OBJTRAK2 chunk, or inteng says "Object doesn't have a track" and leaves its clipboard as it was. Else the
//   clipboard becomes that OBJTRAK2 chunk's bytes and every TRAKSOUN chunk's bytes of the object, in the order they are in.
// - PASTE: with an empty clipboard inteng says "Use copy track first". Else the selected object's own OBJTRAK2 chunk and every TRAKSOUN chunk are
//   taken out, and the clipboard's chunks are put in, the track first and then the sounds in order. That is Paste Track ABSOLUTE: the chunks as they
//   were copied. Paste Track RELATIVE also re-bases the track to the object's place and turn (FUN_004f0490, not read): it stays [wip].
// Not copied from inteng: its "Please use Mode->Save and try again" (an object with no record yet; every object of an open level has one here), and
// it doing the work on the object's in-memory track (FUN_004eff90): here the level's bytes are changed, through the game's own loader, which
// refuses a result it wouldn't take (then nothing is changed), as for Delete and Create.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "track_sections.h"

class OpenLevelFile;

namespace host
{
	enum class TrackEditResult
	{
		Done,
		NothingOpen,       // no level is open
		NoName,            // the ID is null or all 0
		NoSuchObject,      // no object of the level has that ID
		NoTrack,           // Copy: the object has no OBJTRAK2 ("Object doesn't have a track", inteng's words)
		ClipboardEmpty,    // Paste: nothing was copied ("Use copy track first", inteng's words)
		CantBuild,         // the level couldn't be put together from the bytes
		LoaderRefused,     // the game's loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, for the status line: static text, never null
	char const *TrackEditWords(TrackEditResult result);

	// The copied chunks, each whole (its 12-byte heading included, as it lies in the level)
	struct TrackClip
	{
		std::vector<char> track;                  // empty: nothing copied
		std::vector<std::vector<char> > sounds;   // TRAKSOUN chunks, in order
	};

	// Copy Track: of the object whose 8-byte ID is id, into *clip (changed only when Done)
	TrackEditResult TrackCopy(OpenLevelFile *file, uint8_t const *id, TrackClip *clip);

	// Delete Track (Track Editing 0x520 / the last section deleted, FUN_004f0f40): the object's OBJTRAK2 and every TRAKSOUN taken out
	TrackEditResult TrackRemove(OpenLevelFile *file, uint8_t const *id);

	// The track of the object as numbers (sections, flags, sounds): for an object that has none the default inteng makes (one section at the object's place and turn,
	// FUN_004eff90 0x4f01f9), with had false
	struct TrackLoaded
	{
		TrackData track;
		std::vector<TrackSound> sounds;
		bool had = false;
	};

	TrackEditResult TrackLoad(OpenLevelFile *file, uint8_t const *id, TrackLoaded *loaded);

	// A whole chunk (its 12-byte heading, then the payload) of this name, as it lies in the level
	std::vector<char> TrackChunkBytes(char const *name, std::vector<uint8_t> const &payload);

	// The level's sound directory (its SOUNDDIR chunk: the game finds a track's sounds under it, Rif_Sound_Directory), empty when it has none
	std::string TrackLevelSoundDirectory(OpenLevelFile *file);

	// The numbers written back as the object's OBJTRAK2 and TRAKSOUN chunks (inteng's FUN_004f1300): the game's loader checks the result, and nothing is changed if it refuses
	TrackEditResult TrackStore(OpenLevelFile *file, uint8_t const *id, TrackLoaded const &loaded);

	// The object's place and turn in the level (OBJHEAD1's location and orientation, x y z w), false for no such object
	bool TrackObjectPlace(OpenLevelFile *file, uint8_t const *id, int32_t place[3], float quat[4]);

	// The object's place and turn written into its OBJHEAD1 (place: three ints at the payload's byte 20, turn: four floats at byte 32, x y z w): what inteng's FUN_004eff90 does when a track is let go
	// (0x4effba: FUN_004f0870 puts the track's pose at its timer start into the object's place and matrix). The game's loader checks the result; nothing is changed if it refuses.
	TrackEditResult TrackSetObjectPose(OpenLevelFile *file, uint8_t const *id, int32_t const place[3], float const quat[4]);

	// The function that re-bases a track onto an object (the editor passes the game's: track_play_game.h TrackPlay_Rebase); with none Paste Track Relative says it cannot
	typedef bool (*TrackRebaseFn)(TrackData *track, int32_t const objectPlace[3], float const objectQuat[4]);
	void TrackEdit_UseRebase(TrackRebaseFn rebase);

	// The track turned and moved so that its pose at the timer start is the object's own place and turn (inteng's FUN_004f0490, which FUN_004eff90 runs on every track it loads for Edit Track, 0x4f01d2:
	// the track follows the object). False when there is no re-basing function, no such object or the track cannot be asked its pose.
	bool TrackRebasedOnObject(OpenLevelFile *file, uint8_t const *id, TrackData *track);

	// Paste Track Relative (0x52d, FUN_004f3330 with FUN_004f0490): as Paste Track Absolute, but the pasted track is first turned and moved so that where it starts (its pose at the timer
	// start) is the object's own place and turn
	TrackEditResult TrackPasteRelative(OpenLevelFile *file, uint8_t const *id, TrackClip const &clip);

	// Paste Track Absolute: onto the object whose ID is id
	TrackEditResult TrackPasteAbsolute(OpenLevelFile *file, uint8_t const *id, TrackClip const &clip);

	// Whether the object holds a track (an OBJTRAK2): what inteng asks of the object it pastes on (FUN_004f3330: the object's track pointer at +0x154 not 0)
	bool TrackExists(OpenLevelFile *file, uint8_t const *id);
}
