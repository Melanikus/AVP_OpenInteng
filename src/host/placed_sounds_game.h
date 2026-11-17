// placed_sounds_game.h: Object Placement's SOUNDS played by the GAME's own sound code (avpx avp/psnd.cpp, avp/win95/openal_audio.cpp, the level's sounds loaded by GetSoundForMainRif, which reads
// a raw sound folder or the fastfiles): the placed sounds as inteng plays them in its view, and the Sound Edit window's Play. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (reassess/object-placement.md OP-133b/c, OP-92a/b; dump-full-3 FUN_004ecd40, FUN_004ed7e0, dump-orphans-3 FUN_004ed060, FUN_004ed770; the formats read in inteng.exe:
// 0x5e9094 "ne", 0x5e9098 "nel"):
// - A SOUND OBJECT MADE (FUN_004ecd40 from the file when Object Placement makes its objects, FUN_004d68e0 0x4d8c63; FUN_004ecb20 for a new one): its wav loaded (FUN_004a65c0) and, when
//   flag bit 0 ("not playing at start") is clear, played AT ONCE wherever the camera is: Sound_Play(its index, "nel" when bit 1 ("does not loop") is clear, else "ne", {its place, speed
//   0, inner, outer}, &its handle), then Sound_ChangeVolume(its volume) and Sound_ChangePitch(its pitch) when it plays (0x4ececd-0x4ecf13).
// - EVERY FRAME of the screen (FUN_004d9af0 0x4dc6e0 -> FUN_004dd3c0 -> FUN_004ed7e0 for each object of kind 3): the camera's distance to it (FUN_004b31b0); beyond the OUTER range a
//   playing one is stopped (0x4ed805-0x4ed81c); within it, one not playing whose flags & 3 are 0 is played with "nel" and given its volume and pitch (0x4ed829-0x4ed8ae). So a sound
//   that does not loop, or does not play at start, is only ever played when made or edited.
// - MOVED (FUN_004ed770, its method at 0x5ae77c): a playing one gets its new place (Sound_UpdateNew3d with the same numbers).
// - EDIT (0x4ed060): before the window it is stopped and its wav let go (0x4ed13e-0x4ed1ac); after OK or Cancel alike its wav is loaded by the (new) name and, when bit 0 is clear, played
//   as when made ("nel" / "ne", no range test), with its volume and pitch (0x4ed295-0x4ed32b).
// - Its numbers are inteng's memory units: place and ranges in the file's units x the unit; volume 0..127 and pitch in 128ths of a semitone, the game's own (psnd.h VOLUME_MAX 127,
//   PITCH_MAX 6144 = 48 x 128). Here the place and the ranges are in the GAME's units (the file's x the level's scale), as the game's sound code and the editor's camera
//   (Global_VDB_Ptr->VDB_World) use them; the distances and their test are the same.
// - Sound Edit's Play (edobtype 0x10007060) plays the picked wav in 2D through DirectSound: frequency x 2^(pitch / 12), SetVolume(2048 x ln(volume %)), looping when Sound Loops is ticked;
//   the next Play or the window's closing stops it (0x10007010).
// NAMED (Bishop's ruling 11-16): the wavs are loaded through the GAME's loader (GetSoundForMainRif: the raw sound folder, else the fastfiles): inteng read only the raw folder; an installed
// game has its sounds in the fastfiles. Play plays through the game's sound code (its volume 0..127 = volume % x 127 / 100 and its pitch in 128ths of a semitone), not DirectSound's curve.
// NAMED: a placed sound's place is given to the game every frame it plays (inteng: on each move; the same place either way). A sound list that changed (the level's objects read again) keeps
// a sound whose name and wav are the same at the same place in the list; any other is let go and made again (inteng's objects keep their own identity). Leaving the screen lets them all go,
// as Mode > Exit empties inteng's list (OP-6, FUN_004d9af0 0x4d9d58: each entry destroyed; a sound's destructor FUN_004ecf90 / FUN_004edd70 stops it and lets its wav go).
// A hidden run makes NO NOISE: the sounds are made and played, at the sound system's volume 0 (the effects volume too: SoundSys_Management puts the volume back from it every frame).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// The names of the files the game's loader holds in its fastfiles (FastFile.cpp's table: lowercase, '/' between) that begin with `prefix` (any case, either slash), at most `most`
	std::vector<std::string> SoundGame_FastFileNames(std::string const &prefix, size_t most = 65536);

	void SoundGame_UseSilence(bool silent);
	void SoundGame_UseSoundDirectory(std::string const &directory);   // the level's sound directory (TrackLevelSoundDirectory): the game's Rif_Sound_Directory while a wav is loaded

	// Sound Edit's Play: what the last Play started is stopped and let go; the wav is loaded and played in 2D at the volume (0..127) and pitch (128ths of a semitone), looping when asked.
	// False when nothing plays (no sound device, the wav not found or refused).
	bool SoundGame_PreviewPlay(std::string const &wav, int32_t volume, int32_t pitch, bool loops);
	void SoundGame_PreviewStop();
	bool SoundGame_PreviewSounding();

	// One placed sound as the level holds it
	struct PlacedSoundIn
	{
		std::string name;
		std::string wav;
		int32_t place[3] = { 0, 0, 0 };   // the game's units
		int32_t inner = 0, outer = 0;     // the game's units
		int32_t volume = 0, pitch = 0;    // 0..127; 128ths of a semitone
		uint32_t flags = 0;               // bit 0: not playing at start; bit 1: does not loop
	};

	// The level's sounds now, in their order (a sound new to the list is made: loaded, and played at once when bit 0 is clear)
	void PlacedSounds_Sync(std::vector<PlacedSoundIn> const &sounds);

	// FUN_004ed7e0 for each, the camera at `camera` (the game's units); then the sound system's frame
	void PlacedSounds_Frame(int32_t const camera[3]);

	// Edit on the sound at `index` of the last list: before the window (stopped, its wav let go) and after it (the sound's numbers now: loaded, and played when bit 0 is clear)
	void PlacedSounds_EditStart(size_t index);
	void PlacedSounds_EditEnd(size_t index, PlacedSoundIn const &sound);

	// All stopped and let go (the screen left, the editor closing: before the game's level goes, which lets go of every loaded sound)
	void PlacedSounds_LetGo();

	int PlacedSounds_Held();       // sounds in the list
	int PlacedSounds_Loaded();     // whose wav the game loaded
	int PlacedSounds_Sounding();   // playing now
	bool PlacedSounds_SoundingAt(size_t index);   // the sound at `index` of the last list is playing now
	bool SoundGame_DeviceOn();     // the sound system started (there is a sound device)
}
