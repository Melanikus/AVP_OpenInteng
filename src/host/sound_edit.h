// sound_edit.h: the numbers of inteng's SOUND EDIT window (edobtype.dll's edit_sound, resource 132), as plain functions: the record inteng hands the window, what the window shows, how a
// typed field is read and checked, what the window hands back, what goes back into the sound, and the Play button's frequency and loudness. Written 2026-11-16 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (reassess/object-placement.md OP-133a..c; dump-orphans-3 0x4ed060, edobtype FUN_10006bd0, 0x10006830, 0x10007060, 0x10007420):
// - THE RECORD (0x4ed0e4-0x4ed193): the sound's numbers in inteng's memory (the file's x the level's unit: FUN_004ecd40 0x4ecddf, the save 0x4ed4f8) as FLOATS: inner and outer metres
//   (x 1e-5), pitch semitones (x 1/128), volume % (x 100 x 0.007874016); the flags; the probability; the mask 3 (both boxes live) and the sound folder.
// - THE WINDOW (FUN_10006bd0): it shows inner, outer and pitch as they are, the volume ROUNDED to a whole % (ftol(v + 0.5)); the boxes from the flags (Playing at Start ticked when bit 0 is
//   clear, Sound Loops when bit 1 is clear), each only when its mask bit is set; the probability 0 in AvP (the exe is neither "rainbow6" nor "gunlok", 0x10006c50).
// - Each field is MFC's DDX_Text for a float: shown with "%.*g", 6 digits; read back by MFC's simple float parse (spaces and tabs around one number, nothing else), then DDV_MinMaxFloat:
//   inner 0..1000, outer 0..2000, pitch -48..48, volume 0..100. No number: "Please enter a number."; out of range: "Please enter a number between <min> and <max>."; the window stays open.
// - OK (and only OK, 0x10006ca9): the record gets the fields, the volume + 50/127 (0x1001a998), the name, the probability, the boxes' bits under the mask. CANCEL leaves the record as it was.
// - BACK INTO THE SOUND, after OK OR Cancel alike (0x4ed20c-0x4ed290, no answer is tested): inner = ftol(f x 100000), outer the same, pitch = ftol(f x 128), volume = ftol(f x 0.01 x 127)
//   (ftol cuts toward 0), the flags, the probability. So a Cancel also writes the numbers back through the floats.
// - Play (0x10007060): the file at its own frequency x 2^(pitch / 12) when the pitch is not 0; its loudness SetVolume(ftol(2048 x ln(max(volume, 1) x 0.01))) in hundredths of a dB.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// The record inteng hands edit_sound (its numbers as the window keeps them)
	struct SoundEditRecord
	{
		std::string name;
		float inner = 0;
		float outer = 0;
		float pitch = 0;
		float volume = 0;
		uint32_t flags = 0;
		uint32_t mask = 3;
		int32_t probability = 0;
	};

	// What the window shows: the four numbers' texts, the boxes, whether each box is live
	struct SoundEditShown
	{
		std::string fields[4];   // inner, outer, pitch, volume
		bool playingAtStart = false;
		bool loops = false;
		bool playingAtStartLive = false;
		bool loopsLive = false;
	};

	// The record from inteng's memory numbers (inner and outer are unsigned there)
	SoundEditRecord SoundEdit_Record(std::string const &name, uint32_t inner, uint32_t outer, int32_t pitch, int32_t volume, uint32_t flags, int32_t probability);

	// What the window shows for the record (FUN_10006bd0 0x10006be5-0x10006c98)
	SoundEditShown SoundEdit_Shown(SoundEditRecord const &record);

	// MFC's text of a float field ("%.*g", 6 digits)
	std::string SoundEdit_FloatText(float value);

	// field: 0 inner, 1 outer, 2 pitch, 3 volume. Reads and checks one typed field as MFC's DDX / DDV do; false with *message (MFC's words) when it is no number or out of range.
	bool SoundEdit_ReadField(int field, std::string const &typed, float *value, std::string *message);

	// OK: the record gets the window's values (the four already read and checked; the name; the boxes under the mask; the probability 0)
	void SoundEdit_Ok(SoundEditRecord *record, std::string const &name, float const values[4], bool playingAtStart, bool loops);

	// Back into inteng's memory numbers, after OK or Cancel alike (0x4ed20c-0x4ed290)
	void SoundEdit_Back(SoundEditRecord const &record, int32_t *inner, int32_t *outer, int32_t *pitch, int32_t *volume, uint32_t *flags, int32_t *probability);

	// Play's frequency (the file's own x 2^(pitch / 12); the file's when the pitch is 0) and its loudness in hundredths of a dB (0 at 100%)
	int32_t SoundEdit_PlayFrequency(int32_t fileFrequency, float pitch);
	int32_t SoundEdit_PlayVolume(float volume);

	// The names of the folder's *.wav files ("%s\\*.wav", FindFirstFile / FindNextFile: their order, not sorted), at most `most`; a name of 256 letters or more is left out
	std::vector<std::string> SoundEdit_WavNames(std::string const &folder, size_t most = 4096);

	// THE SAME LIST WHEN THE RAW FOLDER IS NOT THERE (Bishop's ruling 11-16; NAMED: inteng read only the raw folder, an installed game has its sounds in the fastfiles): from the names of
	// the files the game's loader holds (FastFile.cpp: lowercase, '/' between), those under `prefix` (e.g. "sound/env/invasion/", any case, either slash) ending in ".wav", in the order the
	// finder would give them from such a folder: in each folder its files first, then its folders, each set as Windows orders an NTFS folder (the names compared in capitals), depth first.
	// Each name is the path under the prefix with '\' between ("sub\x.wav"). The names are HOSTILE (a fastfile can come from anyone): one with a letter outside ' '..'~' or one Windows
	// refuses in a file name (< > : " | ? * \), an empty part, or a folder part starting with '.' (the finder skips such folders) is left out, as is one of 256 letters or more or more than
	// 32 folders deep; the same name twice (a file in two fastfiles) is given once. At most `most`.
	std::vector<std::string> SoundEdit_WavNamesFromFiles(std::string const &prefix, std::vector<std::string> const &files, size_t most = 4096);
}
