// game_track_check.h: a track played by the GAME's own track code (avpx 3dc/avp/track.cpp: Preprocess_Track_Controller and Update_Track_Position), for track-import's check.
// Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The controller is made from the numbers the game's loader reads, as setup_track_controller makes it (avp/win95/object_setup.cpp:253-360, line by line, the sounds left out: the
// proof's tracks have none), and every step runs the game's Update_Track_Position with NormalFrameTime set to the step, as the game's frame does for every track object at once.
// The game's types stay inside game_track_check.cpp, so the tool's own file never mixes them with the chunk library's.

#pragma once

#include <stdint.h>

#include "track_sections.h"   // host::TrackData: the sections and flags as the file holds them

struct GameTrack;

struct GameTrackPose
{
	int32_t position[3];   // the dynamics block's Position: the object's place in the game's units
	int32_t matrix[9];     // its OrientMat, row by row (mat11 mat12 mat13 mat21 ...), 16.16
	int32_t section;
	int32_t timer;
	bool playing;
};

// Makes the game's controller for the track at the level's scale and puts it at the timer start, playing as the file says. Null for a track the game's loader makes none of (no sections).
GameTrack *GameTrack_Make(host::TrackData const &track, double levelScale);

// One frame of the game: NormalFrameTime = frameTime (16.16 seconds), then Update_Track_Position. The pose after it.
void GameTrack_Step(GameTrack *track, int32_t frameTime, GameTrackPose *pose);

// The pose now, without a step
void GameTrack_Pose(GameTrack const *track, GameTrackPose *pose);

void GameTrack_Free(GameTrack *track);

// The game's own rotation of a vector by an OrientMat (RotateVector, as track.cpp turns the object's offset): out = the vector turned
void GameTrack_Rotate(int32_t const matrix[9], int32_t const in[3], int32_t out[3]);

// The game's QuatToMat of the game's quaternion made from a file turn (x y z w) as the loader makes it ((int)-(x*ONE_FIXED), ..., (int)(w*ONE_FIXED)): the OrientMat the game holds
void GameTrack_MatrixOfFileTurn(float const turn[4], int32_t matrix[9]);
