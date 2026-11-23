// video_import.h: IMPORT VIDEO (the owner's addition 11-18, Bishop's order; inteng had none): a user's own MP4 / AVI / WMV made into what an AvP message FMV holds, frames of
// 128 x 96 RGB pixels at 15 a second (letterboxed, the picture's shape kept) and the sound as PCM at 22050 Hz, 8 bits, mono, for Frost's Smacker encoder. Read through Windows' own Media
// Foundation (its source reader, its audio resampler): no third-party code or binary. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE FILE IS HOSTILE INPUT. Only a regular file is read: not a folder, a link (a symbolic link or a junction), a device (NUL, CON, COM1, \\.\...), nor a file's own stream (name:stream).
// It is opened once without following a link, judged on that handle, and read into memory from it, so what was judged is what is read (Media Foundation never opens the path itself).
// THE FOLDERS ON THE WAY TO IT (Vasquez's review, the MUST; Drake 11-22): that open does not follow the FILE if it is a link, but Windows still goes through a FOLDER on the way
// that is one, and a folder made with mklink /D can name another computer (\\server\share): opening through it hands that computer the user's login hash. So before the
// open, every folder from the drive's root down to the file's own is read ITSELF, in that order, each before anything goes through it (VideoImport_FoldersOnTheWayTaken). A
// junction or a mounted volume (only ever local) and OneDrive's placeholders are taken; a symbolic link and any other kind of link are refused, and nothing is opened.
// (A folder swapped between that walk and the open is not caught: making such a link needs Developer Mode or the symbolic-link privilege, so it would have to be planted.)
// Refused, with the reason in words for the status line: a file over kVideoImportMostBytes or empty, a length over kVideoImportMostSeconds, a picture wider or taller than
// kVideoImportMostSide, and a name whose ending is not one of the list (VideoImport_EndingsWords: the owner's ruling 11-22, Media Foundation's common containers, the ONE
// list the drop, Load > Browse..., mp4-to-smk and this read all take).
//
// This header has the parts that need no Media Foundation, each tested on its own (tests/host_video_import_test.cpp): the file's check and read, the stream's bounds, the letterbox,
// the 15-a-second ticks, and the sound's 8 bits.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include <guiddef.h>

namespace host
{
	// What an AvP message FMV holds
	int32_t const kVideoImportWidth = 128;
	int32_t const kVideoImportHeight = 96;
	int32_t const kVideoImportFrameBytes = kVideoImportWidth * kVideoImportHeight * 3;   // R, G, B
	int32_t const kVideoImportFramesPerSecond = 15;
	int32_t const kVideoImportSoundRate = 22050;

	// The bounds on a user's file (the owner's or Bishop's to change): its size, its length, its picture's sides as the file has them
	uint64_t const kVideoImportMostBytes = 256ull * 1024 * 1024;     // the default; a read may be given more (mp4-to-smk --max-input-mb), up to:
	uint64_t const kVideoImportLongestBytes = 2048ull * 1024 * 1024;  // the most any read takes (a 32-bit program may still not find the memory: refused, not a crash)
	int64_t const kVideoImportMostSeconds = 180;          // the default; a read may be given more (mp4-to-smk --max-seconds), up to:
#if defined(_WIN64)
	int64_t const kVideoImportLongestSeconds = 3600;      // the most any read takes in a 64-bit program: an hour of frames is 2 GB
#else
	int64_t const kVideoImportLongestSeconds = 600;       // the most any read takes: 600 s of frames is 332 MB in a 32-bit program, and the encoder takes them whole
#endif
	uint32_t const kVideoImportMostSide = 4096;

	// Media Foundation's time: 100 ns
	int64_t const kVideoImportTimeUnitsPerSecond = 10000000;

	enum class VideoImportResult
	{
		Done,
		BadCall,
		NotAFile,      // missing, a folder, a link, a device, a file's own stream, or a name that isn't a plain path
		NotAVideoName, // its name doesn't end in one of the list (VideoImport_EndingsWords)
		Empty,
		TooBig,        // over the read's most bytes (kVideoImportMostBytes unless given)
		CantRead,      // Windows wouldn't open or read it
		TooLong,       // over the read's most seconds (kVideoImportMostSeconds unless given)
		TooLarge,      // a picture side over kVideoImportMostSide, or of 0
		NotAVideo,     // Media Foundation found no picture in it
		BadPicture,    // its picture can't be decoded to RGB
		BadSound,      // its sound can't be decoded or resampled
		NoMediaFoundation,
		Stopped,       // the read's progress said stop (the Video mode's Cancel, or its leaving)
		NotWindowsCode, // its picture's format, or the code Windows would read it with (the container's reader, a decoder), isn't Windows' own or Microsoft's (media_parsers.h)
	};

	char const *VideoImport_Words(VideoImportResult result);

	// THE ONE LIST of the endings taken (the owner's ruling 11-22, through Bishop 2: "widen the accepted video types to Windows Media Foundation's common ones"), in words:
	// ".mp4, .m4v, .mov, .mkv, .webm, .avi, .wmv, .mpg, .mpeg, .vob, .ts, .m2ts, .3gp". Which ones decode depends on the codecs each Windows has (VideoImport_CodecWords)
	char const *VideoImport_EndingsWords();

	// What a stream Windows can't decode is, in words for the status line, with the Microsoft extension that adds its decoder where there is one (the owner's ruling
	// 11-22: never a bare error code): "HEVC picture: Microsoft's HEVC Video Extension adds it"; one not known: "ABCD picture (Windows has no decoder for it)".
	// subtype: the stream's own MF_MT_SUBTYPE; picture: a picture stream (else a sound stream)
	std::string VideoImport_CodecWords(GUID const &subtype, bool picture);

	// The name's container, as Media Foundation is told it (a MIME type), or null when the ending is none of the list (capitals or not)
	wchar_t const *VideoImport_ContentType(std::wstring const &path);

	// A folder's link kind (its reparse tag, FILE_ATTRIBUTE_TAG_INFO::ReparseTag) that a video's path may go through: a junction or a mounted volume (IO_REPARSE_TAG_MOUNT_POINT, only
	// ever local) and OneDrive's placeholders (IO_REPARSE_TAG_CLOUD and its CLOUD_1 to CLOUD_F, filled by its sync client, not by Windows opening a share). Every other kind is
	// refused: a symbolic link (IO_REPARSE_TAG_SYMLINK, which can name another computer) above all, and any kind this list doesn't know
	bool VideoImport_FolderTagTaken(unsigned long tag);

	// Each folder of `full` (a whole path, as GetFullPathNameW gives it: "C:\a\b\name.mp4"), from the first one under the drive's root down to the file's own, read itself in that
	// order (GetFileAttributesW reads the last part of what it is given without going through it, and everything before it was taken already); a folder that is a link has its
	// kind read on a handle to the link itself. True when every folder is there, is a folder, and is no link of a refused kind; false, with the reason in *why
	bool VideoImport_FoldersOnTheWayTaken(std::wstring const &full, std::string *why);

	// THE FILE: judged and read, all of it, into *bytes (see the top). *why says what was refused and why (for the status line).
	// mostBytes: 1 to kVideoImportLongestBytes (BadCall otherwise); a file the memory can't hold is refused (CantRead), never a crash
	VideoImportResult VideoImport_ReadFile(std::wstring const &path, std::vector<uint8_t> *bytes, std::string *why, uint64_t mostBytes = kVideoImportMostBytes);

	// THE SAME READ for a file that isn't a video (the RIM import's picture, Vasquez 2026-11-23: one set of rules for every file a user hands the editor): every
	// check of VideoImport_ReadFile but the name's ending, which the caller makes itself first. what: the words' start ("Import texture"); noun: what the file is
	// in them ("the picture", as in "is a link ..., not the picture itself"). VideoImport_ReadFile is this with its ending check, "Import video" and "the video"
	VideoImportResult VideoImport_ReadPlainFile(std::wstring const &path, std::vector<uint8_t> *bytes, std::string *why, uint64_t mostBytes, char const *what, char const *noun);

	// THE STREAM'S BOUNDS: its picture's sides (as the file has them) and its length (100 ns; negative when the file doesn't say, which passes: the read stops at the bound itself)
	// mostSeconds: 1 to kVideoImportLongestSeconds (BadCall otherwise)
	VideoImportResult VideoImport_CheckStream(uint32_t width, uint32_t height, int64_t length, std::string *why, int64_t mostSeconds = kVideoImportMostSeconds);

	// A picture whose shape isn't 128 : 96 is either fitted whole (black bars) or cropped to fill (its edges cut): the owner's question, "[R]esize to fit or [C]rop to fill";
	// or, in his Video mode (11-22, "let them fit/crop/expand"), expanded: the whole picture stretched over all of 128 x 96, its shape not kept
	enum class VideoImportFit
	{
		Fit,
		Crop,
		Expand,
	};

	// THE LETTERBOX: one picture of width x height pixels, 4 bytes each (B, G, R, unused: Media Foundation's RGB32), the top row at top and each next row pitch bytes on (negative for a
	// picture kept bottom row first), its pixels parWidth : parHeight in shape (1 : 1 square; 0 in either taken as 1 : 1), made into kVideoImportFrameBytes bytes at out (R, G, B, the top row
	// first). Fit: the whole picture into 128 x 96 with its shape kept, centred, the rest black. Crop: the middle of the picture of 128 : 96's shape (VideoImport_CropWindow) over all of
	// 128 x 96. Expand: the whole picture over all of 128 x 96. Each pixel the average of the part of the picture it covers. False, out untouched, for a side of 0 or over kVideoImportMostSide, a pitch shorter than a row, or a pixel
	// shape past 1 : 16 or 16 : 1.
	bool VideoImport_Letterbox(uint8_t const *top, uint32_t width, uint32_t height, int64_t pitch, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit, uint8_t *out);

	// Fit's place in 128 x 96 (the rest is black): left, top, width, height. False as VideoImport_Letterbox's.
	bool VideoImport_LetterboxArea(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, int32_t area[4]);

	// Crop's part of the picture, in its own pixels (left, top, width, height; parts of a pixel at its edges): the largest middle part of 128 : 96's shape as seen. False as above.
	bool VideoImport_CropWindow(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, double window[4]);

	// THE RECTANGLE (the owner's Video mode, 11-22: "drawn a rectangle over the video to show how it'll display down"): the part of the picture, in its own pixels (left, top, width,
	// height), that becomes the 128 x 96 picture. Fit: the rectangle of 128 : 96's shape round the whole picture (its bars lie outside the picture: a negative left or top);
	// Crop: VideoImport_CropWindow's; Expand: the whole picture. False as VideoImport_Letterbox's, or for no rect
	bool VideoImport_SourceRect(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit, double rect[4]);

	// The same in words: "fit: all of 320 x 180 into 128 x 72, black bars above and below" (or "at the sides"; a picture of 128 : 96's shape "fills 128 x 96"),
	// "crop: the middle 240 x 180 of 320 x 180 becomes 128 x 96", "expand: all of 320 x 180 stretched to 128 x 96" (sizes in whole pixels, rounded); "" when refused
	std::string VideoImport_FitWords(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit);

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
		std::string soundLeftOut;      // a sound in the file that isn't taken (media_parsers.h): why, in words; the clip is then silent. Empty otherwise
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
		std::string soundLeftOut;  // a sound there that isn't taken: why, in words (hasSound is then false)
	};

	// The file's bytes (VideoImport_ReadFile's) opened by Media Foundation, its picture's size, pixel shape and length read, its bounds checked; nothing decoded
	VideoImportResult VideoImport_Probe(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportInfo *info, std::string *why, int64_t mostSeconds = kVideoImportMostSeconds);

	// The file's bytes decoded: its first picture stream to RGB, fitted or cropped, at 15 a second (VideoImport_TicksBefore); its first sound stream to 16-bit PCM, then Media
	// Foundation's own Audio Resampler to 22050 Hz mono, then 8 bits; the sound's own start kept (silence before a late one). contentType as VideoImport_ContentType gives it. *why says
	// what was refused and why.
	// HOW FAR A READ IS (the owner's Video mode, 11-22: "while video loads you'll put a loading message with %"): told 0 .. 100 as the pictures come, by their time over
	// the file's length (0 throughout when the file gives no length), never going back; it gives back false to stop the read (VideoImportResult::Stopped, the clip
	// untouched). Called on the reading thread
	typedef std::function<bool(int32_t percent)> VideoImportProgress;

	VideoImportResult VideoImport_ReadBytes(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportFit fit, VideoImportClip *clip, std::string *why,
		int64_t mostSeconds = kVideoImportMostSeconds, VideoImportProgress const &progress = VideoImportProgress());

	// THE STILL (the owner's Video mode: the picture the rectangle is drawn over): the picture shown at the time asked (at, 100 ns from the start: the first picture at or after it), as
	// it is seen (its pixels' shape counted), at most mostSide pixels a side (never made larger), R G B a pixel, the top row first, into *rgb (*width x *height). Refused:
	// a size of 0, a time before the start, no place, or no picture at or after the time (past the end)
	VideoImportResult VideoImport_Still(std::vector<uint8_t> const &bytes, wchar_t const *contentType, int64_t at, uint32_t mostSide, std::vector<uint8_t> *rgb, uint32_t *width,
		uint32_t *height, std::string *why);

	// VideoImport_ReadFile, then VideoImport_ReadBytes
	VideoImportResult VideoImport_Read(std::wstring const &path, VideoImportFit fit, VideoImportClip *clip, std::string *why, int64_t mostSeconds = kVideoImportMostSeconds);
}
