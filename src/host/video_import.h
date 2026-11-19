// video_import.h: IMPORT VIDEO (the owner's addition 11-18, Bishop's order; inteng had none): a user's own MP4 / AVI / WMV made into what an AvP message FMV holds, frames of
// 128 x 96 RGB pixels at 15 a second (letterboxed, the picture's shape kept) and the sound as PCM at 22050 Hz, 8 bits, mono, for Frost's Smacker encoder. Read through Windows' own Media
// Foundation (its source reader, its audio resampler): no third-party code or binary. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE FILE IS HOSTILE INPUT. Only a regular file is read: not a folder, a link (a symbolic link or a junction), a device (NUL, CON, COM1, \\.\...), nor a file's own stream (name:stream).
// It is opened once without following a link, judged on that handle, and read into memory from it, so what was judged is what is read (Media Foundation never opens the path itself).
// Refused, with the reason in words for the status line: a file over kVideoImportMostBytes or empty, a length over kVideoImportMostSeconds, a picture wider or taller than
// kVideoImportMostSide, and a name whose ending is not one of .mp4 .m4v .avi .wmv.
//
// This header has the parts that need no Media Foundation, each tested on its own (tests/host_video_import_test.cpp): the file's check and read, the stream's bounds, the letterbox,
// the 15-a-second ticks, and the sound's 8 bits.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// What an AvP message FMV holds
	int32_t const kVideoImportWidth = 128;
	int32_t const kVideoImportHeight = 96;
	int32_t const kVideoImportFrameBytes = kVideoImportWidth * kVideoImportHeight * 3;   // R, G, B
	int32_t const kVideoImportFramesPerSecond = 15;
	int32_t const kVideoImportSoundRate = 22050;

	// The bounds on a user's file (the owner's or Bishop's to change): its size, its length, its picture's sides as the file has them
	uint64_t const kVideoImportMostBytes = 256ull * 1024 * 1024;
	int64_t const kVideoImportMostSeconds = 180;          // the default; a read may be given more (mp4-to-smk --max-seconds), up to:
	int64_t const kVideoImportLongestSeconds = 600;       // the most any read takes: 600 s of frames is 332 MB in a 32-bit program, and the encoder takes them whole
	uint32_t const kVideoImportMostSide = 4096;

	// Media Foundation's time: 100 ns
	int64_t const kVideoImportTimeUnitsPerSecond = 10000000;

	enum class VideoImportResult
	{
		Done,
		BadCall,
		NotAFile,      // missing, a folder, a link, a device, a file's own stream, or a name that isn't a plain path
		NotAVideoName, // its name doesn't end in .mp4, .m4v, .avi or .wmv
		Empty,
		TooBig,        // over kVideoImportMostBytes
		CantRead,      // Windows wouldn't open or read it
		TooLong,       // over the read's most seconds (kVideoImportMostSeconds unless given)
		TooLarge,      // a picture side over kVideoImportMostSide, or of 0
		NotAVideo,     // Media Foundation found no picture in it
		BadPicture,    // its picture can't be decoded to RGB
		BadSound,      // its sound can't be decoded or resampled
		NoMediaFoundation,
	};

	char const *VideoImport_Words(VideoImportResult result);

	// The name's container, as Media Foundation is told it (a MIME type), or null when the ending is none of .mp4 .m4v .avi .wmv (capitals or not)
	wchar_t const *VideoImport_ContentType(std::wstring const &path);

	// THE FILE: judged and read, all of it, into *bytes (see the top). *why says what was refused and why (for the status line).
	VideoImportResult VideoImport_ReadFile(std::wstring const &path, std::vector<uint8_t> *bytes, std::string *why);

	// THE STREAM'S BOUNDS: its picture's sides (as the file has them) and its length (100 ns; negative when the file doesn't say, which passes: the read stops at the bound itself)
	// mostSeconds: 1 to kVideoImportLongestSeconds (BadCall otherwise)
	VideoImportResult VideoImport_CheckStream(uint32_t width, uint32_t height, int64_t length, std::string *why, int64_t mostSeconds = kVideoImportMostSeconds);

	// A picture whose shape isn't 128 : 96 is either fitted whole (black bars) or cropped to fill (its edges cut): the owner's question, "[R]esize to fit or [C]rop to fill"
	enum class VideoImportFit
	{
		Fit,
		Crop,
	};

	// THE LETTERBOX: one picture of width x height pixels, 4 bytes each (B, G, R, unused: Media Foundation's RGB32), the top row at top and each next row pitch bytes on (negative for a
	// picture kept bottom row first), its pixels parWidth : parHeight in shape (1 : 1 square; 0 in either taken as 1 : 1), made into kVideoImportFrameBytes bytes at out (R, G, B, the top row
	// first). Fit: the whole picture into 128 x 96 with its shape kept, centred, the rest black. Crop: the middle of the picture of 128 : 96's shape (VideoImport_CropWindow) over all of
	// 128 x 96. Each pixel the average of the part of the picture it covers. False, out untouched, for a side of 0 or over kVideoImportMostSide, a pitch shorter than a row, or a pixel
	// shape past 1 : 16 or 16 : 1.
	bool VideoImport_Letterbox(uint8_t const *top, uint32_t width, uint32_t height, int64_t pitch, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit, uint8_t *out);

	// Fit's place in 128 x 96 (the rest is black): left, top, width, height. False as VideoImport_Letterbox's.
	bool VideoImport_LetterboxArea(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, int32_t area[4]);

	// Crop's part of the picture, in its own pixels (left, top, width, height; parts of a pixel at its edges): the largest middle part of 128 : 96's shape as seen. False as above.
	bool VideoImport_CropWindow(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, double window[4]);

	// Whether the picture is 128 : 96's shape already (Fit fills all of it), so there is nothing to ask
	bool VideoImport_IsAvpShape(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight);

	// THE TICKS: tick k is at k / 15 s and shows the latest picture that starts at or before it; ticks before the first picture show the first. How many ticks, from *next on, come
	// before time (100 ns): those the picture shown until then fills. *next moves past them. Time is held to 0 .. (kVideoImportLongestSeconds + 1) s, so no sum overflows.
	int64_t VideoImport_TicksBefore(int64_t *next, int64_t time);

	// THE SOUND: signed 16-bit samples to unsigned 8-bit ones (128 silence), each the nearest, appended to *out
	void VideoImport_SoundTo8(int16_t const *samples, size_t count, std::vector<uint8_t> *out);

	// THE LEVEL (the owner, 11-18: "the same level of the game's videos"): the gain that brings the sound's loudness (its RMS, counted in 8-bit steps: 0 .. 128) to targetRms, held
	// down so that its loudest sample still fits 8 bits (no clipping). 1 for an empty or silent sound, or a target of 0 or less (or not a number).
	double VideoImport_LevelGain(int16_t const *samples, size_t count, double targetRms);

	// The sound times gain (each the nearest, held to 16 bits), then 8 bits as VideoImport_SoundTo8, appended to *out
	void VideoImport_SoundTo8Gained(int16_t const *samples, size_t count, double gain, std::vector<uint8_t> *out);

	// The loudness (RMS) of 8-bit unsigned samples, in 8-bit steps (0 .. 128); 0 for none
	double VideoImport_Loudness8(std::vector<uint8_t> const &sound);

	// THE WHOLE READ, through Media Foundation (video_import.cpp's second half)
	int32_t const kVideoImportSamplesPerFrame = kVideoImportSoundRate / kVideoImportFramesPerSecond;   // 1470

	struct VideoImportClip
	{
		std::vector<uint8_t> frames;   // frameCount pictures of kVideoImportFrameBytes, one after another (R, G, B, the top row first; the letterbox's bars black)
		int32_t frameCount = 0;
		std::vector<uint8_t> sound;    // 8-bit unsigned mono at 22050 Hz, exactly frameCount x 1470 samples (silence put in or cut to fit); empty when the file has no sound
		std::vector<int16_t> sound16;  // the same sound before its 8 bits (16-bit signed), for a change of level (VideoImport_LevelGain) without the 8 bits' loss
		uint32_t sourceWidth = 0;      // the picture as the file has it
		uint32_t sourceHeight = 0;
		int64_t length = -1;           // the length the file gives (100 ns), -1 when it gives none
	};

	// What the file says of itself before anything is decoded (for the question)
	struct VideoImportInfo
	{
		uint32_t width = 0;        // its picture as decoded
		uint32_t height = 0;
		uint32_t parWidth = 1;     // its pixels' shape
		uint32_t parHeight = 1;
		int64_t length = -1;       // 100 ns; -1 when it gives none
		bool hasSound = false;
	};

	// The file's bytes (VideoImport_ReadFile's) opened by Media Foundation, its picture's size, pixel shape and length read, its bounds checked; nothing decoded
	VideoImportResult VideoImport_Probe(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportInfo *info, std::string *why, int64_t mostSeconds = kVideoImportMostSeconds);

	// The file's bytes decoded: its first picture stream to RGB, fitted or cropped, at 15 a second (VideoImport_TicksBefore); its first sound stream to 16-bit PCM, then Media
	// Foundation's own Audio Resampler to 22050 Hz mono, then 8 bits; the sound's own start kept (silence before a late one). contentType as VideoImport_ContentType gives it. *why says
	// what was refused and why.
	VideoImportResult VideoImport_ReadBytes(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportFit fit, VideoImportClip *clip, std::string *why,
		int64_t mostSeconds = kVideoImportMostSeconds);

	// VideoImport_ReadFile, then VideoImport_ReadBytes
	VideoImportResult VideoImport_Read(std::wstring const &path, VideoImportFit fit, VideoImportClip *clip, std::string *why, int64_t mostSeconds = kVideoImportMostSeconds);
}
