// smk_header.h: a Smacker video's header read as HOSTILE input, before any decoder sees a byte, and what the editor says about a message FMV. Pure: bytes in, numbers and words out.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (the owner's FMV finder, through Bishop; facts: AvP-docs/engine/inteng/reassess/fmv-trace-dietrich.md). AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE HEADER (the first 104 bytes of a Smacker file, every number little-endian; the same reading as AvP-docs/tools/po/smk-info.js): +0 "SMK2" or "SMK4", +4 width, +8 height,
// +12 frames, +16 the frame rate (above 0: milliseconds a frame; below 0: hundredths of a millisecond a frame; 0: 100 ms), +20 flags (1 ring frame, 2 Y interlaced, 4 Y doubled),
// +72 .. +99 the seven audio tracks (bit 30 the track is there, bit 31 compressed, bit 29 16-bit, bit 28 stereo, the low 24 bits the sample rate).
//
// AvP's MESSAGE FMVs (52 of the 53 the game ships): SMK2, 128 x 96, 15 frames a second, track 0 at 22050 Hz, 8-bit, mono. The game plays other videos too (it turns any frame into
// its 128 x 128 screen, FmvCutscenes.cpp ConvertFrame; message238.smk is 11025 Hz 16-bit), so a difference is a note, never a refusal.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace model
{
	size_t const kSmkHeaderBytes = 104;
	uint32_t const kSmkMostSide = 4096;        // larger sides are refused as impossible (smk-info.js's bound)
	uint32_t const kSmkMostFrames = 1000000;
	int const kSmkTracks = 7;

	struct SmkTrack
	{
		bool present = false;
		bool compressed = false;
		int bits = 8;          // 8 or 16
		int channels = 1;      // 1 or 2
		uint32_t rate = 0;     // samples a second
	};

	struct SmkHeader
	{
		char signature[5] = {};   // "SMK2" or "SMK4"
		uint32_t width = 0, height = 0, frames = 0;
		int32_t rateField = 0;
		uint32_t flags = 0;
		double msPerFrame = 100.0;   // from rateField
		SmkTrack tracks[kSmkTracks];
	};

	enum class SmkRead
	{
		Read,         // a Smacker header with possible numbers
		TooShort,     // fewer than 104 bytes
		NotSmacker,   // it does not start with SMK2 or SMK4
		Impossible,   // a side of 0 or above 4096, or 0 frames or more than 1,000,000
	};

	// Reads the header from the first `size` bytes of a file (only the first 104 are looked at). *out is filled only when the answer is Read.
	SmkRead SmkHeader_Read(uint8_t const *bytes, size_t size, SmkHeader *out);

	// Frames a second (1000 / msPerFrame)
	double SmkHeader_Fps(SmkHeader const &header);

	// What differs from AvP's message format, as words ("320 x 240, not 128 x 96; track 0 11025 Hz 16-bit, not 22050 Hz 8-bit mono"); empty when nothing does
	std::string SmkHeader_DiffersFromAvp(SmkHeader const &header);

	// One line about the video: "SMK2 128 x 96, 228 frames at 15 fps (15.2 s), 22050 Hz 8-bit mono" (no audio: "no sound")
	std::string SmkHeader_Words(SmkHeader const &header);

	// The game's message FMV names (FmvCutscenes.cpp StartTriggerPlotFMV: "FMVs/message%d.smk"): true when `name` (a file name, no folder) is "message<n>.smk" in any capitals with
	// <n> written as %d writes it (no sign, no leading 0 but for "0"), from 0 to 450 (the numbers the game's reader lets through, strategy_chunk.cpp CheckMessageNumber); *number is n.
	int32_t const kMostMessageNumber = 450;
	bool MessageFmvNumber(std::string const &name, int32_t *number);
	std::string MessageFmvName(int32_t number);   // "message<n>.smk"
}
