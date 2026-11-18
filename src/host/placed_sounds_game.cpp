// See placed_sounds_game.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "jsndsup.h"
#include "psnd.h"
#include "FastFile.h"

#include <math.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "placed_sounds_game.h"

extern int SoundSwitchedOn;                   // avp/psnd.cpp: 1 once the sound system started (there is a sound device)
extern int EffectsSoundVolume;                // avp/psnd.cpp: the volume SoundSys_Management puts the sound system back to every frame
extern void SoundSys_Start(void);             // avp/psnd.cpp
extern void SoundSys_ChangeVolume(int volume);
extern void SoundSys_Management(void);        // avp/psnd.cpp: the frame's work for the sounds: ended ones let go, the 3D ones and the listener followed
extern char *Rif_Sound_Directory;             // avp/win95/projload.cpp
LOADED_SOUND const *GetSoundForMainRif(const char *wavName);   // avp/win95/object_setup.cpp
extern std::vector<std::vector<FastFileHandle> > fastFileHandles;   // 3dc/FastFile.cpp: the files of every fastfile, by their names' hash

namespace host
{
	namespace
	{
		bool silent = false;
		std::string soundDirectory;

		struct Held
		{
			PlacedSoundIn sound;
			LOADED_SOUND const *loaded = nullptr;
			int active = SOUND_NOACTIVEINDEX;   // the game writes SOUND_NOACTIVEINDEX here when the sound ends: so a Held never moves (held by pointer)
		};

		std::vector<std::unique_ptr<Held> > held;

		LOADED_SOUND const *previewLoaded = nullptr;
		int previewActive = SOUND_NOACTIVEINDEX;

		// The sound system started (once), silent in a hidden run; false with no sound device
		bool Started()
		{
			if (!SoundSwitchedOn)
			{
				SoundSys_Start();
			}

			if (!SoundSwitchedOn)
			{
				return false;
			}

			if (silent)
			{
				EffectsSoundVolume = 0;
				SoundSys_ChangeVolume(0);   // VOLUME_MIN
			}

			return true;
		}

		// GetSoundForMainRif with the level's sound directory (the raw folder, else the fastfiles); a name the game refuses gives nothing (jsndsup.cpp's GetSound)
		LOADED_SOUND const *Load(std::string const &wav)
		{
			if (wav.empty() || wav.size() > 255 || wav == "Not Set" || !Started())
			{
				return nullptr;
			}

			char *const kept = Rif_Sound_Directory;

			Rif_Sound_Directory = soundDirectory.empty() ? nullptr : (char *)soundDirectory.c_str();

			LOADED_SOUND const *const loaded = GetSoundForMainRif(wav.c_str());

			Rif_Sound_Directory = kept;
			return loaded;
		}

		SOUND3DDATA ThreeD(PlacedSoundIn const &s)
		{
			SOUND3DDATA s3d;

			s3d.position.vx = s.place[0];
			s3d.position.vy = s.place[1];
			s3d.position.vz = s.place[2];
			s3d.velocity.vx = 0;   // DAT_008f4f20 / 24 / 28: nothing writes them
			s3d.velocity.vy = 0;
			s3d.velocity.vz = 0;
			s3d.inner_range = s.inner;
			s3d.outer_range = s.outer;
			return s3d;
		}

		int Clamp(int32_t v, int least, int most)
		{
			return v < least ? least : (v > most ? most : (int)v);
		}

		// Sound_Play(index, format, {place, 0, inner, outer}, &handle), then the volume and the pitch when it plays
		void Play(Held &h, char const *format)
		{
			if (!h.loaded)
			{
				return;
			}

			SOUND3DDATA s3d = ThreeD(h.sound);

			Sound_Play((SOUNDINDEX)h.loaded->sound_num, (char *)format, &s3d, &h.active);

			if (h.active != SOUND_NOACTIVEINDEX)
			{
				Sound_ChangeVolume(h.active, Clamp(h.sound.volume, VOLUME_MIN, VOLUME_MAX));
				Sound_ChangePitch(h.active, Clamp(h.sound.pitch, PITCH_MIN, PITCH_MAX));
			}
		}

		void Stop(Held &h)
		{
			if (h.active != SOUND_NOACTIVEINDEX)
			{
				Sound_Stop(h.active);
				h.active = SOUND_NOACTIVEINDEX;
			}
		}

		void LetGo(Held &h)
		{
			Stop(h);

			if (h.loaded)
			{
				LoseSound(h.loaded);
				h.loaded = nullptr;
			}
		}

		// FUN_004ecd40 0x4ecec2-0x4ecf13 (and the same after an Edit, 0x4ed295-0x4ed32b): loaded, and played at once when bit 0 is clear, looping unless bit 1 is set
		void Make(Held &h)
		{
			h.loaded = Load(h.sound.wav);

			if (h.loaded && (h.sound.flags & 1) == 0)
			{
				Play(h, (h.sound.flags & 2) == 0 ? "nel" : "ne");
			}
		}

		std::string Lower(std::string text)
		{
			for (char &letter : text)
			{
				letter = (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : (letter == '\\' ? '/' : letter);
			}

			return text;
		}
	}

	std::vector<std::string> SoundGame_FastFileNames(std::string const &prefix, size_t most)
	{
		std::string const under = Lower(prefix);
		std::vector<std::string> names;

		for (std::vector<FastFileHandle> const &bucket : fastFileHandles)
		{
			for (FastFileHandle const &file : bucket)
			{
				if (names.size() >= most)
				{
					return names;
				}

				if (file.fileName.size() > under.size() && file.fileName.compare(0, under.size(), under) == 0)
				{
					names.push_back(file.fileName);
				}
			}
		}

		return names;
	}

	void SoundGame_UseSilence(bool value)
	{
		silent = value;
	}

	void SoundGame_UseSoundDirectory(std::string const &directory)
	{
		soundDirectory = directory;
	}

	void SoundGame_PreviewStop()
	{
		if (previewActive != SOUND_NOACTIVEINDEX)
		{
			Sound_Stop(previewActive);
			previewActive = SOUND_NOACTIVEINDEX;
		}

		if (previewLoaded)
		{
			LoseSound(previewLoaded);
			previewLoaded = nullptr;
		}
	}

	bool SoundGame_PreviewPlay(std::string const &wav, int32_t volume, int32_t pitch, bool loops)
	{
		SoundGame_PreviewStop();
		previewLoaded = Load(wav);

		if (!previewLoaded)
		{
			return false;
		}

		Sound_Play((SOUNDINDEX)previewLoaded->sound_num, (char *)(loops ? "evpl" : "evp"), &previewActive, Clamp(volume, VOLUME_MIN, VOLUME_MAX), Clamp(pitch, PITCH_MIN, PITCH_MAX));
		return previewActive != SOUND_NOACTIVEINDEX;
	}

	bool SoundGame_PreviewSounding()
	{
		return previewActive != SOUND_NOACTIVEINDEX;
	}

	void PlacedSounds_Sync(std::vector<PlacedSoundIn> const &sounds)
	{
		for (size_t i = 0; i < sounds.size(); i++)
		{
			PlacedSoundIn const &now = sounds[i];

			if (i < held.size() && held[i]->sound.name == now.name && held[i]->sound.wav == now.wav)
			{
				held[i]->sound = now;   // the same sound: its numbers now (its place goes to the game in the frame)
				continue;
			}

			if (i < held.size())
			{
				LetGo(*held[i]);
			}
			else
			{
				held.push_back(std::unique_ptr<Held>(new Held()));
			}

			held[i]->sound = now;
			Make(*held[i]);
		}

		while (held.size() > sounds.size())
		{
			LetGo(*held.back());
			held.pop_back();
		}
	}

	void PlacedSounds_Frame(int32_t const camera[3])
	{
		if (held.empty() && previewActive == SOUND_NOACTIVEINDEX)
		{
			return;
		}

		for (std::unique_ptr<Held> const &one : held)
		{
			Held &h = *one;
			double const dx = (double)h.sound.place[0] - (double)camera[0];
			double const dy = (double)h.sound.place[1] - (double)camera[1];
			double const dz = (double)h.sound.place[2] - (double)camera[2];
			double const distance = sqrt(dx * dx + dy * dy + dz * dz);

			// 0x4ed805: the outer range (unsigned) below the distance: stopped
			if ((double)(uint32_t)h.sound.outer < distance)
			{
				Stop(h);
				continue;
			}

			// 0x4ed829: not playing and flags & 3 clear: played, looping
			if (h.active == SOUND_NOACTIVEINDEX && (h.sound.flags & 3) == 0)
			{
				Play(h, "nel");
			}
			else if (h.active != SOUND_NOACTIVEINDEX)
			{
				SOUND3DDATA s3d = ThreeD(h.sound);

				Sound_UpdateNew3d(h.active, &s3d);   // FUN_004ed770 on a move
			}
		}

		if (SoundSwitchedOn)
		{
			SoundSys_Management();
		}
	}

	void PlacedSounds_EditStart(size_t index)
	{
		if (index < held.size())
		{
			LetGo(*held[index]);
		}
	}

	void PlacedSounds_EditEnd(size_t index, PlacedSoundIn const &sound)
	{
		if (index >= held.size())
		{
			return;
		}

		LetGo(*held[index]);
		held[index]->sound = sound;
		Make(*held[index]);
	}

	void PlacedSounds_LetGo()
	{
		for (std::unique_ptr<Held> const &one : held)
		{
			LetGo(*one);
		}

		held.clear();
	}

	int PlacedSounds_Held()
	{
		return (int)held.size();
	}

	int PlacedSounds_Loaded()
	{
		int loaded = 0;

		for (std::unique_ptr<Held> const &one : held)
		{
			loaded += one->loaded ? 1 : 0;
		}

		return loaded;
	}

	int PlacedSounds_Sounding()
	{
		int sounding = 0;

		for (std::unique_ptr<Held> const &one : held)
		{
			sounding += one->active != SOUND_NOACTIVEINDEX ? 1 : 0;
		}

		return sounding;
	}

	namespace
	{
		std::vector<std::unique_ptr<Held>> hierarchyHeld;   // the current placed hierarchy's sounds
		std::vector<HierarchySoundIn> hierarchyIn;          // what they were made from

		Held *HierarchyHeldOf(int32_t index)
		{
			for (size_t i = 0; i < hierarchyIn.size() && i < hierarchyHeld.size(); i++)
			{
				if (hierarchyIn[i].index == index)
				{
					return hierarchyHeld[i].get();
				}
			}

			return nullptr;
		}

		bool HierarchyLoops(int32_t index)
		{
			for (HierarchySoundIn const &one : hierarchyIn)
			{
				if (one.index == index)
				{
					return (one.flags & 1) != 0;
				}
			}

			return false;
		}
	}

	void HierarchySounds_Sync(std::vector<HierarchySoundIn> const &sounds)
	{
		bool same = sounds.size() == hierarchyIn.size();

		for (size_t i = 0; same && i < sounds.size(); i++)
		{
			HierarchySoundIn const &a = sounds[i], &b = hierarchyIn[i];

			same = a.index == b.index && a.wav == b.wav && a.inner == b.inner && a.outer == b.outer && a.volume == b.volume && a.pitch == b.pitch && a.flags == b.flags;
		}

		if (same)
		{
			return;
		}

		HierarchySounds_LetGo();
		hierarchyIn = sounds;

		for (HierarchySoundIn const &one : sounds)
		{
			std::unique_ptr<Held> h(new Held);

			h->sound.wav = one.wav;
			h->sound.inner = one.inner;
			h->sound.outer = one.outer;
			h->sound.volume = one.volume;
			h->sound.pitch = one.pitch;
			h->loaded = Load(one.wav);
			hierarchyHeld.push_back(std::move(h));
		}
	}

	void HierarchySounds_Start(int32_t index, int32_t const place[3])
	{
		Held *const h = HierarchyHeldOf(index);

		if (!h || !h->loaded)
		{
			return;
		}

		Stop(*h);
		h->sound.place[0] = place[0];
		h->sound.place[1] = place[1];
		h->sound.place[2] = place[2];
		Play(*h, HierarchyLoops(index) ? "nel" : "ne");   // FUN_004ebfe0: the looping format 0x5e9098 when bit 0 is set
	}

	void HierarchySounds_Move(int32_t index, int32_t const place[3])
	{
		Held *const h = HierarchyHeldOf(index);

		if (!h || h->active == SOUND_NOACTIVEINDEX)
		{
			return;
		}

		h->sound.place[0] = place[0];
		h->sound.place[1] = place[1];
		h->sound.place[2] = place[2];

		SOUND3DDATA s3d = ThreeD(h->sound);

		Sound_UpdateNew3d(h->active, &s3d);   // FUN_004ec120
	}

	void HierarchySounds_Stop(int32_t index)
	{
		Held *const h = HierarchyHeldOf(index);

		if (h)
		{
			Stop(*h);   // FUN_004ec190
		}
	}

	void HierarchySounds_StopAll()
	{
		for (std::unique_ptr<Held> &one : hierarchyHeld)
		{
			Stop(*one);
		}
	}

	void HierarchySounds_LetGo()
	{
		for (std::unique_ptr<Held> &one : hierarchyHeld)
		{
			LetGo(*one);
		}

		hierarchyHeld.clear();
		hierarchyIn.clear();
	}

	int HierarchySounds_Playing()
	{
		int playing = 0;

		for (std::unique_ptr<Held> const &one : hierarchyHeld)
		{
			playing += one->active != SOUND_NOACTIVEINDEX ? 1 : 0;
		}

		return playing;
	}

	bool PlacedSounds_SoundingAt(size_t index)
	{
		return index < held.size() && held[index]->active != SOUND_NOACTIVEINDEX;
	}

	bool SoundGame_DeviceOn()
	{
		return SoundSwitchedOn != 0;
	}
}
