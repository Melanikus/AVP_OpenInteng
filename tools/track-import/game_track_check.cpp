// See game_track_check.h. Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "dynblock.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "track.h"
#include "psnd.h"
#include "psndplat.h"
#include "jsndsup.h"
#include "savegame.h"
#include "save_checks.h"

#include <math.h>
#include <string.h>

#include <new>

#include "game_track_check.h"

int NormalFrameTime = 0;   // the game's frame time (avp's main loop sets it each frame); here, each step's

// ---- what track.cpp names and the proof never reaches (its tracks have no sounds and are never saved): the program must link, and nothing here is called

DISPLAYBLOCK *Player = nullptr;
SOUNDSAMPLEDATA GameSounds[1];

void Load_SoundState(int *)
{
}

void Save_SoundState(int *)
{
}

void *GetPointerForSaveBlock(unsigned int)
{
	return nullptr;
}

int Save_PlaceInListOrNone(int, int)
{
	return -1;
}

bool Save_NumberIsInRange(int, int, int)
{
	return false;
}

void Sound_Play(SOUNDINDEX, char *, ...)
{
}

void Sound_Stop(int)
{
}

void Sound_UpdateNew3d(int, SOUND3DDATA *)
{
}

void LoseSound(LOADED_SOUND const *)
{
}

// ---- two of the game's own quaternion helpers, copied from avp/hmodel.cpp:159-175 and :208-216 (that file brings the whole model system with it)

void QNormalise(QUAT *q)
{
	float nw = q->quatw;
	float nx = q->quatx;
	float ny = q->quaty;
	float nz = q->quatz;
	float m = sqrt(nw * nw + nx * nx + ny * ny + nz * nz);

	if (!m) {
		return;
	}

	m = 65536.0f / m;
	f2i(q->quatw, nw * m);
	f2i(q->quatx, nx * m);
	f2i(q->quaty, ny * m);
	f2i(q->quatz, nz * m);
}

int QDot(QUAT *this_quat, QUAT *next_quat)
{
	int qdot;
	qdot = ((MUL_FIXED(this_quat->quatx, next_quat->quatx)) +
		    (MUL_FIXED(this_quat->quaty, next_quat->quaty)) +
		    (MUL_FIXED(this_quat->quatz, next_quat->quatz)) +
		    (MUL_FIXED(this_quat->quatw, next_quat->quatw)));
	return(qdot);
}

// and avp/hmodel.cpp:2566-2591 (the smooth track's control turns use it: track.cpp MakeControlQuat)
void MulQuat(QUAT *q1, QUAT *q2, QUAT *output)
{
	VECTORCH v1, v2, v3;
	int tw;
	/* Multiply quats... */
	v1.vx = q1->quatx;
	v1.vy = q1->quaty;
	v1.vz = q1->quatz;
	v2.vx = q2->quatx;
	v2.vy = q2->quaty;
	v2.vz = q2->quatz;
	CrossProduct(&v1, &v2, &v3);
	tw = MUL_FIXED(q1->quatw, q2->quatw);
	tw -= DotProduct(&v1, &v2);
	v3.vx += MUL_FIXED(q1->quatw, v2.vx);
	v3.vx += MUL_FIXED(q2->quatw, v1.vx);
	v3.vy += MUL_FIXED(q1->quatw, v2.vy);
	v3.vy += MUL_FIXED(q2->quatw, v1.vy);
	v3.vz += MUL_FIXED(q1->quatw, v2.vz);
	v3.vz += MUL_FIXED(q2->quatw, v1.vz);
	output->quatw = tw;
	output->quatx = v3.vx;
	output->quaty = v3.vy;
	output->quatz = v3.vz;
	QNormalise(output);
}

struct GameTrack
{
	STRATEGYBLOCK strategy;
	DYNAMICSBLOCK dynamics;
	TRACK_CONTROLLER controller;
	TRACK_SECTION_DATA *sections = nullptr;
};

namespace
{
	// the loader's file float to the game's 16.16 whole number, as it writes it: (int)(value * ONE_FIXED)
	int Fixed(float value)
	{
		return (int)(value * ONE_FIXED);
	}

	VECTORCH Scaled(int32_t const v[3], double scale)
	{
		// the loader: ChunkVectorInt * local_scale (a double), into a VECTORCH (each part cut toward 0 by the conversion)
		VECTORCH out;

		out.vx = (int)(v[0] * scale);
		out.vy = (int)(v[1] * scale);
		out.vz = (int)(v[2] * scale);
		return out;
	}

	void Copy(GameTrack const *track, GameTrackPose *pose)
	{
		MATRIXCH const &m = track->dynamics.OrientMat;
		int32_t const rows[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

		pose->position[0] = track->dynamics.Position.vx;
		pose->position[1] = track->dynamics.Position.vy;
		pose->position[2] = track->dynamics.Position.vz;
		memcpy(pose->matrix, rows, sizeof(rows));
		pose->section = track->controller.current_section;
		pose->timer = track->controller.timer;
		pose->playing = track->controller.playing != 0;
	}
}

GameTrack *GameTrack_Make(host::TrackData const &data, double levelScale)
{
	if (data.sections.empty())
	{
		return nullptr;
	}

	GameTrack *const track = new (std::nothrow) GameTrack();

	if (!track)
	{
		return nullptr;
	}

	memset(&track->strategy, 0, sizeof(track->strategy));
	memset(&track->dynamics, 0, sizeof(track->dynamics));
	memset(&track->controller, 0, sizeof(track->controller));
	track->strategy.DynPtr = &track->dynamics;
	track->dynamics.OrientMat.mat11 = track->dynamics.OrientMat.mat22 = track->dynamics.OrientMat.mat33 = ONE_FIXED;

	TRACK_CONTROLLER *const tc = &track->controller;
	size_t const count = data.sections.size();

	// object_setup.cpp:262-275
	tc->sbptr = &track->strategy;
	tc->playing = 0;
	tc->playing_start_sound = 0;
	tc->reverse = 0;
	tc->loop = 0;
	tc->loop_backandforth = 0;
	tc->no_rotation = 1;
	tc->timer = data.timerStart;
	tc->current_section = 0;
	tc->use_speed_mult = 0;
	tc->speed_mult = 0;
	tc->num_sections = (int)count;
	track->sections = new (std::nothrow) TRACK_SECTION_DATA[count];

	if (!track->sections)
	{
		delete track;
		return nullptr;
	}

	memset(track->sections, 0, sizeof(TRACK_SECTION_DATA) * count);
	tc->sections = track->sections;

	// :282-286, the game's own slip kept: its reference turn is the first section's x four times
	QUAT quat_start;
	quat_start.quatx = (int)-(data.sections[0].quatStart[0] * ONE_FIXED);
	quat_start.quaty = (int)-(data.sections[0].quatStart[0] * ONE_FIXED);
	quat_start.quatz = (int)-(data.sections[0].quatStart[0] * ONE_FIXED);
	quat_start.quatw = (int)(data.sections[0].quatStart[0] * ONE_FIXED);

	for (size_t i = 0; i < count; i++)
	{
		TRACK_SECTION_DATA *const tsd = &tc->sections[i];
		host::TrackSection const &cts = data.sections[i];

		tsd->quat_start.quatx = -Fixed(cts.quatStart[0]);
		tsd->quat_start.quaty = -Fixed(cts.quatStart[1]);
		tsd->quat_start.quatz = -Fixed(cts.quatStart[2]);
		tsd->quat_start.quatw = Fixed(cts.quatStart[3]);

		tsd->quat_end.quatx = -Fixed(cts.quatEnd[0]);
		tsd->quat_end.quaty = -Fixed(cts.quatEnd[1]);
		tsd->quat_end.quatz = -Fixed(cts.quatEnd[2]);
		tsd->quat_end.quatw = Fixed(cts.quatEnd[3]);

		if (tsd->quat_start.quatx != quat_start.quatx || tsd->quat_start.quaty != quat_start.quaty || tsd->quat_start.quatz != quat_start.quatz ||
			tsd->quat_start.quatw != quat_start.quatw)
		{
			tc->no_rotation = 0;
		}

		if (tsd->quat_end.quatx != quat_start.quatx || tsd->quat_end.quaty != quat_start.quaty || tsd->quat_end.quatz != quat_start.quatz ||
			tsd->quat_end.quatw != quat_start.quatw)
		{
			tc->no_rotation = 0;
		}

		int32_t const travel[3] = { cts.pivotEnd[0] - cts.pivotStart[0], cts.pivotEnd[1] - cts.pivotStart[1], cts.pivotEnd[2] - cts.pivotStart[2] };

		// :318-320: pivot_start, (pivot_end - pivot_start) and the offset, each times local_scale
		tsd->pivot_start = Scaled(cts.pivotStart, levelScale);
		tsd->pivot_travel = Scaled(travel, levelScale);
		tsd->object_offset = Scaled(cts.offset, levelScale);
		tsd->time_for_section = cts.time;
	}

	// :325-353: the flags (TrackFlag_Loop 1, _PlayingAtStart 2, _LoopBackAndForth 4, _UseTrackSmoothing 8)
	if (data.flags & 1)
	{
		tc->loop = 1;
	}
	else if (data.flags & 4)
	{
		tc->loop_backandforth = 1;
	}

	if (data.flags & 2)
	{
		tc->playing = 1;
	}

	tc->use_smoothing = (data.flags & 8) && tc->num_sections >= 3 ? 1 : 0;

	Preprocess_Track_Controller(tc);

	tc->initial_state_timer = tc->timer;
	tc->initial_state_playing = tc->playing;
	tc->initial_state_reverse = tc->reverse;

	// the place at the timer start (the object stands there until the first frame moves it)
	Update_Track_Position_Only(tc);
	return track;
}

void GameTrack_Step(GameTrack *track, int32_t frameTime, GameTrackPose *pose)
{
	NormalFrameTime = frameTime;
	Update_Track_Position(&track->controller);
	Copy(track, pose);
}

void GameTrack_Pose(GameTrack const *track, GameTrackPose *pose)
{
	Copy(track, pose);
}

void GameTrack_Free(GameTrack *track)
{
	if (track)
	{
		delete[] track->sections;
		delete track;
	}
}

void GameTrack_Rotate(int32_t const matrix[9], int32_t const in[3], int32_t out[3])
{
	MATRIXCH m;
	VECTORCH v;

	m.mat11 = matrix[0]; m.mat12 = matrix[1]; m.mat13 = matrix[2];
	m.mat21 = matrix[3]; m.mat22 = matrix[4]; m.mat23 = matrix[5];
	m.mat31 = matrix[6]; m.mat32 = matrix[7]; m.mat33 = matrix[8];
	v.vx = in[0];
	v.vy = in[1];
	v.vz = in[2];
	RotateVector(&v, &m);
	out[0] = v.vx;
	out[1] = v.vy;
	out[2] = v.vz;
}

void GameTrack_MatrixOfFileTurn(float const turn[4], int32_t matrix[9])
{
	QUAT q;
	MATRIXCH m;

	q.quatx = -Fixed(turn[0]);
	q.quaty = -Fixed(turn[1]);
	q.quatz = -Fixed(turn[2]);
	q.quatw = Fixed(turn[3]);
	QuatToMat(&q, &m);

	int32_t const rows[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

	memcpy(matrix, rows, sizeof(rows));
}
