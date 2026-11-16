// track_play_game.h: a track played with the GAME's own track code (avpx 3dc/avp/track.cpp: Preprocess_Track_Controller, Update_Track_Position). Written
// 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng's Play Track (0x514, FUN_004f09e0) moves a marker along the track with code of its own; the game is what plays a track in a level, so the editor
// builds the game's TRACK_CONTROLLER from the same numbers the game's loader reads (setup_track_controller, avp/win95/object_setup.cpp:252-351, copied
// line by line apart from the sounds, which are not played here) and runs the game's Update_Track_Position on it. The place the game's code gives is the
// position of the track's OBJECT (the pivot plus the object's offset turned by the section's turn, track.cpp:352-378), in the level's file units.
//
// Start (inteng's Play, 0x514): from section 0 at the track's timer start (FUN_004f09e0 0x4f0a0d), always playing whatever the "Playing at start" flag says.

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "track_sections.h"

namespace host
{
	struct TrackPose
	{
		int32_t place[3];     // the object's place, in file units (the game's millimetres divided by the level's scale)
		int32_t section;      // the section the game is in
		int32_t timer;        // 16.16 seconds into that section
		bool playing;         // false once a track that does not loop has reached its end
	};

	// Makes the game's controller for these sections and puts it at its start. False for no sections. Any controller made before is let go.
	bool TrackPlay_Start(TrackData const &track, double levelScale, TrackPose *pose, std::vector<TrackSound> const *sounds = nullptr);

	// THE SOUNDS (inteng plays the track's start, middle and end sounds as the track plays): given to Start, they are loaded and played by the GAME's own sound code (psnd.cpp, openal_audio.cpp: Start_Track_Sound,
	// Update_Track_Sound of track.cpp, run by the game's Update_Track_Position). The sound system is started when the first track with a sound plays; with no sound device the track plays silently and this says false.
	// A hidden run makes NO NOISE (somebody may be asleep at the machine): the game still makes and starts the sounds, but the sound system's volume is 0
	void TrackPlay_UseSilence(bool silent);

	bool TrackPlay_SoundsAvailable();

	// The level's sound directory for the sounds Start is given (the game's Rif_Sound_Directory while it loads them); empty: none
	void TrackPlay_UseSoundDirectory(std::string const &directory);

	// How many of the playing track's sounds are sounding now (a handle of the sound system's in use), and how many it has loaded
	int TrackPlay_SoundsSounding();
	int TrackPlay_SoundsLoaded();

	// Runs the game's Update_Track_Position for frameTime (16.16 seconds, as NormalFrameTime is: 65536 is one second). False when nothing is playing.
	bool TrackPlay_Step(int32_t frameTime, TrackPose *pose);

	// The level's scale for TrackPlay_ObjectPlace (the editor sets it from the level, as for Start)
	void TrackPlay_UseLevelScale(double levelScale);

	// The place the OBJECT takes at the start or the end of a section, by the game's own code (Update_Track_Position_Only on a controller of its own, timer 0 or the
	// section's whole time): the marker inteng's Finish End Pos Edit and Edit Pivot Point work with. A TrackObjectPlaceFn (track_sections.h).
	bool TrackPlay_ObjectPlace(TrackData const &track, size_t section, bool atEnd, int32_t place[3]);

	// The pose of the object at the track's timer start, by the game's own code (a controller of its own, scale 1: file units): its place, and its orientation as the game's 16.16 matrix
	// (mat11 mat12 mat13 mat21 mat22 mat23 mat31 mat32 mat33). A track that turns nowhere keeps the first section's start turn, as the game's loader leaves the object's own.
	bool TrackPlay_PoseAtStart(TrackData const &track, int32_t place[3], int32_t matrix[9]);

	// The same pose with the turn as a file turn (x y z w), for the object's record: inteng's FUN_004f0870 puts this pose into the object when its track is let go
	bool TrackPlay_PoseAtStartAsFile(TrackData const &track, int32_t place[3], float quat[4]);

	// PASTE TRACK RELATIVE (inteng FUN_004f0490 with FUN_004f0870): the whole track is turned and moved so that its pose at the timer start is the object's own place and turn:
	// R = (the object's matrix) x (the pose's matrix, transposed), every pivot becomes R (pivot - P0) + objectPlace (P0 = the pose's place), every turn is R x turn. objectQuat is the
	// object's orientation as the file has it (x y z w).
	bool TrackPlay_Rebase(TrackData *track, int32_t const objectPlace[3], float const objectQuat[4]);

	// The object's orientation at the start or the end of a section, by the game's own code, as the game's 16.16 matrix (nine numbers, as TrackPlay_PoseAtStart). False when it cannot be asked.
	bool TrackPlay_OrientationAt(TrackData const &track, size_t section, bool atEnd, int32_t matrix[9]);

	// TURN THE END: the start turn (a file turn, x y z w) turned by yaw (about the vertical axis), pitch and roll, in degrees, with the game's own Euler matrix (4096 to a turn): the new end turn, a file turn
	bool TrackPlay_TurnedBy(float const startQuat[4], double yaw, double pitch, double roll, float outQuat[4]);

	// The world directions of an object's three local axes (x, y, z: rows of the game's matrix, as PositionPlayersWeapon uses its row 3 for the way it points), unit lengths, from a file turn (x y z w): the end marker's arrows
	bool TrackPlay_QuatAxes(float const quat[4], double axes[9]);

	// The sound system ends with the editor (only if a track started it)
	void TrackPlay_EndSounds();

	void TrackPlay_Stop();
	bool TrackPlay_Playing();
}
