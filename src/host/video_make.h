// video_make.h: WHAT IS DONE TO A READ VIDEO before it is encoded, for the editor's Video mode (the owner's, 11-22: "cut start and ending, turn
// volume up and down"), in its own library (videomake_static.vcxproj). Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng, its level code
// Vasquez's (11-18). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The clip comes from video_import.h (frames of 128 x 96 at 15 a second, its sound at 22050 Hz, 1470 samples a frame, in 8 bits and in 16); it goes to smk_encoder.h.
// THE CUT keeps a run of whole frames and exactly their sound. THE LEVEL brings the sound to AvP's own message videos' loudness (or keeps the source's), then the
// volume the user asked, in steps of kVideoMakeVolumeStepDb, its loudest sample still inside 8 bits. THE SAVE never writes over a film without being asked, and keeps
// the film it replaces in FMVs\backup\ first (the game reads only FMVs\message<n>.smk, never a folder under it).
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "video_import.h"

namespace host
{
	// The sound's loudness by default (the owner, 11-18: "the same level of the game's videos"): AvP's own message videos' median RMS, in 8-bit steps. Measured 11-18 by
	// Dietrich over all 53 stock FMVs\message*.smk, decoded by Barry's libsmackerdec (AvP-docs\tools\dietrich\smk-levels): median 22.18 (-15.2 dBFS), lowest 16.47,
	// highest 53.95; message101 21.97.
	double const kVideoMakeStockLevel = 22.18;

	// The Video mode's volume: a step of 3 dB (half again as loud, near enough), from -24 dB to +24 dB around the level
	double const kVideoMakeVolumeStepDb = 3.0;
	double const kVideoMakeMostVolumeDb = 24.0;

	// The frame nearest `seconds` into the clip (15 a second), held to 0 .. frameCount
	int32_t VideoMake_FrameAt(double seconds, int32_t frameCount);

	// THE CUT: only frames fromFrame .. toFrame - 1 kept, with exactly their sound (8-bit and 16-bit alike). False, the clip untouched, unless
	// 0 <= fromFrame < toFrame <= frameCount
	bool VideoMake_Cut(VideoImportClip *clip, int32_t fromFrame, int32_t toFrame);

	// THE LEVEL: the sound brought to `level` (RMS in 8-bit steps, 1 .. 128; 0 or less keeps the source's own), then volumeDb more or less (held to
	// +-kVideoMakeMostVolumeDb), never so loud that its loudest sample leaves 8 bits; *words (when given) says what was done, as the Video mode says it
	// ("sound level 9.4 -> 22.2 of 128 (RMS in 8-bit steps; target 22.2, gain x2.36, held so nothing clips)"). A clip with no sound is left as it is.
	void VideoMake_Level(VideoImportClip *clip, double level, double volumeDb, std::string *words);

	// The volume in words for the screen and the state line: "AvP's level", "AvP's level +3 dB", "AvP's level -6 dB"; with the source's level, "the video's own level ..."
	std::string VideoMake_VolumeWords(bool sourceLevel, double volumeDb);

	// THE SAVE of a made film at `path`: a file already there is kept first as backupFolder\<its name>.<stamp> (a new file there; never written over), then replaced
	// whole (Smk_Replace: the old one stays if the new one can't be put in its place); no file there: written new (Smk_WriteNew). The caller asked the user before a
	// replace. stamp: the time in the name, e.g. "20261122-131500". False with the reason in *why; *keptAs (when given) the backup's full name, empty when none was made.
	bool VideoMake_Save(std::wstring const &path, std::vector<uint8_t> const &smk, std::wstring const &backupFolder, std::wstring const &stamp, std::wstring *keptAs,
		std::string *why);
}
