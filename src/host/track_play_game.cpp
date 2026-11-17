// See track_play_game.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "dynblock.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "track.h"
#include "jsndsup.h"
#include "psnd.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <AL/al.h>   // alGetListenerf: the output volume the sound system set, read back for the proofs

#include "track_play_game.h"

extern int NormalFrameTime;
extern int SoundSwitchedOn;                   // avp/psnd.cpp: 1 once the sound system started (there is a sound device)
extern void SoundSys_Start(void);             // avp/psnd.cpp
extern void SoundSys_End(void);
extern void SoundSys_ChangeVolume(int volume);   // avp/psnd.cpp
extern void SoundSys_Management(void);        // avp/psnd.cpp: the frame's work for the sounds (the game's main loop calls it)
extern int EffectsSoundVolume;                // avp/psnd.cpp:35: the volume SoundSys_Management puts back on EVERY call (MUL_FIXED(EffectsSoundVolume, the fade), psnd.cpp:133-139)
extern double local_scale;
extern char *Rif_Sound_Directory;             // avp/win95/projload.cpp: set while the game loads a rif, so a track's sounds are found under it
LOADED_SOUND const *GetSoundForMainRif(const char *wavName);   // avp/win95/object_setup.cpp: the game's way to find a track's sound
extern void MatToQuat(MATRIXCH *m, QUAT *quat);   // avp/hmodel.cpp:2872

namespace host
{
	namespace
	{
		TRACK_CONTROLLER *controller = nullptr;
		STRATEGYBLOCK block;
		DYNAMICSBLOCK dynamics;
		double scale = 1.0;

		int soundsLoaded = 0;
		std::string soundDirectory;
		bool silent = false;
		int keptEffectsVolume = -1;   // the effects' volume before a hidden run made it 0, given back if the silence ends; -1: none kept

		// A HIDDEN RUN IS SILENT ON EVERY FRAME: the sound system's volume 0 AND the effects' volume 0. The volume alone (SoundSys_ChangeVolume(0)) lasted only to the next
		// SoundSys_Management, which sets it back to the effects' volume on every call (psnd.cpp:133-139): a playing track's first frame (Vasquez 11-16). Nothing when not silent.
		void Hush()
		{
			if (!silent)
			{
				return;
			}

			EffectsSoundVolume = 0;
			SoundSys_ChangeVolume(0);   // VOLUME_MIN
		}

		// the sounds the game's code was given are stopped and let go (Deallocate_Track_Sound, but freed as we made them)
		void LetGoOfSound(TRACK_SOUND *&ts)
		{
			if (!ts)
			{
				return;
			}

			Stop_Track_Sound(ts);

			if (ts->sound_loaded)
			{
				LoseSound(ts->sound_loaded);
			}

			free(ts);
			ts = nullptr;
		}

		void LetGo()
		{
			if (controller)
			{
				LetGoOfSound(controller->sound);
				LetGoOfSound(controller->start_sound);
				LetGoOfSound(controller->end_sound);
				soundsLoaded = 0;
				free(controller->sections);
				free(controller);
				controller = nullptr;
			}
		}

		void Report(TrackPose *pose)
		{
			if (!pose || !controller)
			{
				return;
			}

			double const use = scale > 0.0 ? scale : 1.0;
			int const axes[3] = { dynamics.Position.vx, dynamics.Position.vy, dynamics.Position.vz };

			for (int axis = 0; axis < 3; axis++)
			{
				double const file = (double)axes[axis] / use;

				pose->place[axis] = (int32_t)(file < 0.0 ? -floor(-file) : floor(file));
			}

			pose->section = controller->current_section;
			pose->timer = controller->timer;
			pose->playing = controller->playing != 0;
		}

		TRACK_CONTROLLER *Build(TrackData const &track, double use, bool *rotates);

		int FixedFromTurn(float part)
		{
			return (int)(part * ONE_FIXED);
		}
	}

	namespace
	{
		TRACK_CONTROLLER *Build(TrackData const &track, double use, bool *rotates)
		{
		if (track.sections.empty() || track.sections.size() > kTrackMostSections)
		{
			return nullptr;
		}

		TRACK_CONTROLLER *const tc = (TRACK_CONTROLLER *)calloc(1, sizeof(TRACK_CONTROLLER));

		if (!tc)
		{
			return nullptr;
		}

		tc->num_sections = (int)track.sections.size();
		tc->sections = (TRACK_SECTION_DATA *)calloc((size_t)tc->num_sections, sizeof(TRACK_SECTION_DATA));

		if (!tc->sections)
		{
			free(tc);
			return nullptr;
		}

		tc->no_rotation = 1;
		tc->timer = track.timerStart;
		tc->current_section = 0;

		// setup_track_controller, object_setup.cpp:278-331 (its comparison of every turn with the first section's x, for all four parts, is copied as it is)
		QUAT first;

		first.quatx = FixedFromTurn(-track.sections[0].quatStart[0]);
		first.quaty = FixedFromTurn(-track.sections[0].quatStart[0]);
		first.quatz = FixedFromTurn(-track.sections[0].quatStart[0]);
		first.quatw = FixedFromTurn(track.sections[0].quatStart[0]);

		for (int i = 0; i < tc->num_sections; i++)
		{
			TRACK_SECTION_DATA *const tsd = &tc->sections[i];
			TrackSection const &cts = track.sections[(size_t)i];

			tsd->quat_start.quatx = FixedFromTurn(-cts.quatStart[0]);
			tsd->quat_start.quaty = FixedFromTurn(-cts.quatStart[1]);
			tsd->quat_start.quatz = FixedFromTurn(-cts.quatStart[2]);
			tsd->quat_start.quatw = FixedFromTurn(cts.quatStart[3]);

			tsd->quat_end.quatx = FixedFromTurn(-cts.quatEnd[0]);
			tsd->quat_end.quaty = FixedFromTurn(-cts.quatEnd[1]);
			tsd->quat_end.quatz = FixedFromTurn(-cts.quatEnd[2]);
			tsd->quat_end.quatw = FixedFromTurn(cts.quatEnd[3]);

			if (tsd->quat_start.quatx != first.quatx || tsd->quat_start.quaty != first.quaty || tsd->quat_start.quatz != first.quatz || tsd->quat_start.quatw != first.quatw ||
				tsd->quat_end.quatx != first.quatx || tsd->quat_end.quaty != first.quaty || tsd->quat_end.quatz != first.quatz || tsd->quat_end.quatw != first.quatw)
			{
				tc->no_rotation = 0;
			}

			tsd->pivot_start.vx = (int)(cts.pivotStart[0] * use);
			tsd->pivot_start.vy = (int)(cts.pivotStart[1] * use);
			tsd->pivot_start.vz = (int)(cts.pivotStart[2] * use);
			tsd->pivot_travel.vx = (int)((cts.pivotEnd[0] - cts.pivotStart[0]) * use);
			tsd->pivot_travel.vy = (int)((cts.pivotEnd[1] - cts.pivotStart[1]) * use);
			tsd->pivot_travel.vz = (int)((cts.pivotEnd[2] - cts.pivotStart[2]) * use);
			tsd->object_offset.vx = (int)(cts.offset[0] * use);
			tsd->object_offset.vy = (int)(cts.offset[1] * use);
			tsd->object_offset.vz = (int)(cts.offset[2] * use);
			tsd->time_for_section = cts.time;
		}

		tc->loop = (track.flags & 0x1u) != 0;

		if (!tc->loop)
		{
			tc->loop_backandforth = (track.flags & 0x4u) != 0;
		}

		tc->use_smoothing = (track.flags & 0x8u) != 0 && tc->num_sections >= 3;
		Preprocess_Track_Controller(tc);
		tc->playing = 1;   // inteng's Play plays whatever the "Playing at start" flag says
		(void)rotates;
		return tc;
		}
	}

	void TrackPlay_UseSilence(bool value)
	{
		if (value && !silent)
		{
			keptEffectsVolume = EffectsSoundVolume;
			EffectsSoundVolume = 0;
		}
		else if (!value && silent && keptEffectsVolume >= 0)
		{
			EffectsSoundVolume = keptEffectsVolume;
			keptEffectsVolume = -1;
		}

		silent = value;
	}

	float TrackPlay_OutputGain()
	{
		if (!SoundSwitchedOn)
		{
			return -1.0f;
		}

		ALfloat gain = -1.0f;

		alGetListenerf(AL_GAIN, &gain);
		return alGetError() == AL_NO_ERROR ? gain : -1.0f;
	}

	void TrackPlay_UseSoundDirectory(std::string const &directory)
	{
		soundDirectory = directory;
	}

	bool TrackPlay_SoundsAvailable()
	{
		return SoundSwitchedOn != 0;
	}

	int TrackPlay_SoundsLoaded()
	{
		return soundsLoaded;
	}

	int TrackPlay_SoundsSounding()
	{
		int sounding = 0;

		if (controller)
		{
			TRACK_SOUND *const all[3] = { controller->sound, controller->start_sound, controller->end_sound };

			for (TRACK_SOUND *const ts : all)
			{
				sounding += ts && ts->activ_no != SOUND_NOACTIVEINDEX ? 1 : 0;
			}
		}

		return sounding;
	}

	bool TrackPlay_Start(TrackData const &track, double levelScale, TrackPose *pose, std::vector<TrackSound> const *sounds)
	{
		LetGo();
		scale = levelScale > 0.0 ? levelScale : 1.0;
		memset(&block, 0, sizeof(block));
		memset(&dynamics, 0, sizeof(dynamics));
		block.DynPtr = &dynamics;
		controller = Build(track, scale, nullptr);

		if (!controller)
		{
			return false;
		}

		controller->sbptr = &block;
		Update_Track_Position_Only(controller);

		// the sounds, by the game's own way (setup_track_sound, object_setup.cpp:167-193: the sound found by its name, the ranges in the level's scale, the loop flag), then played as Start_Track_Playing plays them
		if (sounds && !sounds->empty())
		{
			if (!SoundSwitchedOn)
			{
				SoundSys_Start();
			}

			if (SoundSwitchedOn)
			{
				Hush();   // a hidden run is silent

				char *const keptDirectory = Rif_Sound_Directory;

				Rif_Sound_Directory = soundDirectory.empty() ? nullptr : (char *)soundDirectory.c_str();

				for (TrackSound const &s : *sounds)
				{
					TRACK_SOUND **const slot = s.index == 0 ? &controller->sound : (s.index == 1 ? &controller->start_sound : (s.index == 2 ? &controller->end_sound : nullptr));

					if (!slot || *slot || s.wav.empty() || s.wav == "Not Set")
					{
						continue;
					}

					LOADED_SOUND const *const loaded = GetSoundForMainRif(s.wav.c_str());

					if (!loaded)
					{
						continue;   // a sound that is not there is skipped, as the game skips it
					}

					TRACK_SOUND *const ts = (TRACK_SOUND *)calloc(1, sizeof(TRACK_SOUND));

					if (!ts)
					{
						LoseSound(loaded);
						continue;
					}

					ts->sound_loaded = loaded;
					ts->inner_range = (unsigned)(s.innerRange * local_scale);
					ts->outer_range = (unsigned)(s.outerRange * local_scale);
					ts->pitch = s.pitch;
					ts->max_volume = s.maxVolume;
					ts->activ_no = SOUND_NOACTIVEINDEX;
					ts->playing = 0;
					ts->loop = (s.flags & 1) != 0;
					*slot = ts;
					soundsLoaded++;
				}

				Rif_Sound_Directory = keptDirectory;

				// Start_Track_Playing: the start sound first (the middle one begins when it has finished), or the middle one
				if (controller->start_sound)
				{
					controller->playing_start_sound = 1;
					Start_Track_Sound(controller->start_sound, &dynamics.Position);
				}
				else if (controller->sound)
				{
					Start_Track_Sound(controller->sound, &dynamics.Position);
				}
			}
		}

		Report(pose);
		return true;
	}

	namespace
	{
		QUAT GameQuat(float const q[4])
		{
			QUAT game;

			game.quatx = FixedFromTurn(-q[0]);
			game.quaty = FixedFromTurn(-q[1]);
			game.quatz = FixedFromTurn(-q[2]);
			game.quatw = FixedFromTurn(q[3]);
			return game;
		}

		void FileQuat(QUAT const &game, float q[4])
		{
			q[0] = -(float)game.quatx / (float)ONE_FIXED;
			q[1] = -(float)game.quaty / (float)ONE_FIXED;
			q[2] = -(float)game.quatz / (float)ONE_FIXED;
			q[3] = (float)game.quatw / (float)ONE_FIXED;
		}
	}

	bool TrackPlay_PoseAtStart(TrackData const &track, int32_t place[3], int32_t matrix[9])
	{
		TRACK_CONTROLLER *const tc = Build(track, 1.0, nullptr);

		if (!tc)
		{
			return false;
		}

		STRATEGYBLOCK sb;
		DYNAMICSBLOCK dyn;

		memset(&sb, 0, sizeof(sb));
		memset(&dyn, 0, sizeof(dyn));
		sb.DynPtr = &dyn;
		tc->sbptr = &sb;
		tc->current_section = 0;
		tc->timer = track.timerStart;
		Update_Track_Position_Only(tc);
		place[0] = dyn.Position.vx;
		place[1] = dyn.Position.vy;
		place[2] = dyn.Position.vz;

		MATRIXCH m = dyn.OrientMat;

		if (tc->no_rotation)
		{
			QUAT q = GameQuat(track.sections[0].quatStart);

			QuatToMat(&q, &m);
		}

		matrix[0] = m.mat11; matrix[1] = m.mat12; matrix[2] = m.mat13;
		matrix[3] = m.mat21; matrix[4] = m.mat22; matrix[5] = m.mat23;
		matrix[6] = m.mat31; matrix[7] = m.mat32; matrix[8] = m.mat33;
		free(tc->sections);
		free(tc);
		return true;
	}

	bool TrackPlay_PoseAtStartAsFile(TrackData const &track, int32_t place[3], float quat[4])
	{
		int32_t matrix[9];

		if (!TrackPlay_PoseAtStart(track, place, matrix))
		{
			return false;
		}

		MATRIXCH m;

		m.mat11 = matrix[0]; m.mat12 = matrix[1]; m.mat13 = matrix[2];
		m.mat21 = matrix[3]; m.mat22 = matrix[4]; m.mat23 = matrix[5];
		m.mat31 = matrix[6]; m.mat32 = matrix[7]; m.mat33 = matrix[8];

		QUAT game;

		MatToQuat(&m, &game);
		FileQuat(game, quat);
		return true;
	}

	bool TrackPlay_QuatAxes(float const quat[4], double axes[9])
	{
		QUAT game = GameQuat(quat);
		MATRIXCH m;

		QuatToMat(&game, &m);

		int32_t const v[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

		for (int i = 0; i < 9; i++)
		{
			axes[i] = (double)v[i] / 65536.0;
		}

		return true;
	}

	bool TrackPlay_OrientationAt(TrackData const &track, size_t section, bool atEnd, int32_t matrix[9])
	{
		if (section >= track.sections.size())
		{
			return false;
		}

		TRACK_CONTROLLER *const tc = Build(track, 1.0, nullptr);

		if (!tc)
		{
			return false;
		}

		STRATEGYBLOCK sb;
		DYNAMICSBLOCK dyn;

		memset(&sb, 0, sizeof(sb));
		memset(&dyn, 0, sizeof(dyn));
		sb.DynPtr = &dyn;
		tc->sbptr = &sb;
		tc->current_section = (int)section;
		tc->timer = atEnd ? tc->sections[section].time_for_section : 0;
		Update_Track_Position_Only(tc);

		MATRIXCH m = dyn.OrientMat;

		if (tc->no_rotation)
		{
			QUAT q = GameQuat(atEnd ? track.sections[section].quatEnd : track.sections[section].quatStart);

			QuatToMat(&q, &m);
		}

		matrix[0] = m.mat11; matrix[1] = m.mat12; matrix[2] = m.mat13;
		matrix[3] = m.mat21; matrix[4] = m.mat22; matrix[5] = m.mat23;
		matrix[6] = m.mat31; matrix[7] = m.mat32; matrix[8] = m.mat33;
		free(tc->sections);
		free(tc);
		return true;
	}

	bool TrackPlay_TurnedBy(float const startQuat[4], double yaw, double pitch, double roll, float outQuat[4])
	{
		if (!(fabs(yaw) <= 3600.0 && fabs(pitch) <= 3600.0 && fabs(roll) <= 3600.0))
		{
			return false;
		}

		EULER e;

		e.EulerX = (int)floor(pitch * 4096.0 / 360.0 + 0.5) & 4095;
		e.EulerY = (int)floor(yaw * 4096.0 / 360.0 + 0.5) & 4095;
		e.EulerZ = (int)floor(roll * 4096.0 / 360.0 + 0.5) & 4095;

		MATRIXCH turn, start, composed;
		QUAT q = GameQuat(startQuat);

		CreateEulerMatrix(&e, &turn);
		QuatToMat(&q, &start);
		MatrixMultiply(&start, &turn, &composed);   // the game's MatrixMultiply(m1, m2, out) is m2 x m1: the turn applied after the start turn
		MatToQuat(&composed, &q);
		FileQuat(q, outQuat);
		return true;
	}

	bool TrackPlay_Rebase(TrackData *track, int32_t const objectPlace[3], float const objectQuat[4])
	{
		int32_t p0[3];
		int32_t m0[9];

		if (!track || !TrackPlay_PoseAtStart(*track, p0, m0))
		{
			return false;
		}

		MATRIXCH pose;

		pose.mat11 = m0[0]; pose.mat12 = m0[1]; pose.mat13 = m0[2];
		pose.mat21 = m0[3]; pose.mat22 = m0[4]; pose.mat23 = m0[5];
		pose.mat31 = m0[6]; pose.mat32 = m0[7]; pose.mat33 = m0[8];

		QUAT objectGame = GameQuat(objectQuat);
		MATRIXCH object, turn;

		QuatToMat(&objectGame, &object);
		TransposeMatrixCH(&pose);
		MatrixMultiply(&object, &pose, &turn);   // FUN_004f0490 0x4f04f7: FUN_004b34b0(the object's matrix, the pose's matrix turned about, out)

		TrackData made = *track;

		for (TrackSection &s : made.sections)
		{
			int32_t *const ends[2] = { s.pivotStart, s.pivotEnd };

			for (int32_t *end : ends)
			{
				VECTORCH v;

				v.vx = end[0] - p0[0];
				v.vy = end[1] - p0[1];
				v.vz = end[2] - p0[2];
				_RotateVector(&v, &turn);
				end[0] = v.vx + objectPlace[0];
				end[1] = v.vy + objectPlace[1];
				end[2] = v.vz + objectPlace[2];

				for (int axis = 0; axis < 3; axis++)
				{
					if (end[axis] < -16777216 || end[axis] > 16777216)
					{
						return false;
					}
				}
			}

			float *const turns[2] = { s.quatStart, s.quatEnd };

			for (float *file : turns)
			{
				QUAT q = GameQuat(file);
				MATRIXCH m, turned;

				QuatToMat(&q, &m);
				MatrixMultiply(&turn, &m, &turned);   // 0x4f054c: FUN_004b34b0(R, the section's matrix, the same)
				MatToQuat(&turned, &q);
				FileQuat(q, file);
			}
		}

		*track = made;
		return true;
	}

	void TrackPlay_UseLevelScale(double levelScale)
	{
		scale = levelScale > 0.0 ? levelScale : 1.0;
	}

	bool TrackPlay_ObjectPlace(TrackData const &track, size_t section, bool atEnd, int32_t place[3])
	{
		if (section >= track.sections.size())
		{
			return false;
		}

		TRACK_CONTROLLER *const tc = Build(track, scale, nullptr);

		if (!tc)
		{
			return false;
		}

		STRATEGYBLOCK sb;
		DYNAMICSBLOCK dyn;

		memset(&sb, 0, sizeof(sb));
		memset(&dyn, 0, sizeof(dyn));
		sb.DynPtr = &dyn;
		tc->sbptr = &sb;
		tc->current_section = (int)section;
		tc->timer = atEnd ? tc->sections[section].time_for_section : 0;
		Update_Track_Position_Only(tc);

		int const axes[3] = { dyn.Position.vx, dyn.Position.vy, dyn.Position.vz };

		for (int axis = 0; axis < 3; axis++)
		{
			double const file = (double)axes[axis] / scale;

			place[axis] = (int32_t)(file < 0.0 ? -floor(-file) : floor(file));
		}

		free(tc->sections);
		free(tc);
		return true;
	}

	bool TrackPlay_Step(int32_t frameTime, TrackPose *pose)
	{
		if (!controller)
		{
			return false;
		}

		int const kept = NormalFrameTime;

		NormalFrameTime = frameTime < 0 ? 0 : frameTime;
		Update_Track_Position(controller);
		NormalFrameTime = kept;

		if (SoundSwitchedOn && soundsLoaded > 0)
		{
			SoundSys_Management();   // the sound system's frame: sounds that have ended are let go, the listener is followed
		}
		Report(pose);
		return controller->playing != 0;
	}

	void TrackPlay_Stop()
	{
		LetGo();
	}

	void TrackPlay_EndSounds()
	{
		LetGo();

		if (SoundSwitchedOn)
		{
			SoundSys_End();
		}
	}

	bool TrackPlay_Playing()
	{
		return controller != nullptr && controller->playing != 0;
	}

	namespace
	{
		LOADED_SOUND const *listened = nullptr;   // the sound the last listen loaded (let go at the next listen)
		int listenedActive = SOUND_NOACTIVEINDEX;
	}

	bool TrackPlay_Listen(std::string const &wav, int32_t inner, int32_t outer, int32_t volume, int32_t pitch, int32_t const place[3])
	{
		// FUN_00501080 first stops what plays: here the sound the last listen started
		if (listenedActive != SOUND_NOACTIVEINDEX)
		{
			Sound_Stop(listenedActive);
			listenedActive = SOUND_NOACTIVEINDEX;
		}

		if (listened)
		{
			LoseSound(listened);
			listened = nullptr;
		}

		if (!place || wav.empty() || wav == "Not Set" || wav.size() > 255 || inner < 0 || outer < 0)
		{
			return false;
		}

		if (!SoundSwitchedOn)
		{
			SoundSys_Start();
		}

		if (!SoundSwitchedOn)
		{
			return false;
		}

		Hush();   // a hidden run is silent

		char *const keptDirectory = Rif_Sound_Directory;

		Rif_Sound_Directory = soundDirectory.empty() ? nullptr : (char *)soundDirectory.c_str();
		listened = GetSoundForMainRif(wav.c_str());
		Rif_Sound_Directory = keptDirectory;

		if (!listened)
		{
			return false;
		}

		// FUN_004ec0a0: Sound_Play(its index, "nvp" (0x5d0a88), the place with no speed and the two ranges, its volume, its pitch); "e" here as well, so the next listen can stop it
		SOUND3DDATA s3d;

		s3d.position.vx = place[0];
		s3d.position.vy = place[1];
		s3d.position.vz = place[2];
		s3d.velocity.vx = 0;
		s3d.velocity.vy = 0;
		s3d.velocity.vz = 0;
		s3d.inner_range = inner;
		s3d.outer_range = outer;
		Sound_Play((SOUNDINDEX)listened->sound_num, (char *)"nvpe", &s3d, (int)(volume < 0 ? 0 : (volume > 127 ? 127 : volume)), (int)pitch, &listenedActive);
		return listenedActive != SOUND_NOACTIVEINDEX;
	}
}
