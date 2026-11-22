// verify_videomake.cpp: the VIDEO EDITOR's acceptance tool (alpha.5 item 2, the owner 11-22: "let's do it, little video editor inside inteng";
// tests/scenarios/acceptance-strategy-make-video.txt). Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
//   verify_videomake.exe make <new .mp4> <seconds> [--again] [--size <width>x<height>] [--fps <n>] [--no-sound]
//       makes the test clip the scenario drops on the editor, with Windows' own Media Foundation (tests/video_test_clip.h: the repository holds no
//       video): by default 320 x 180, not 4:3, at 30 frames a second, the first half red, the second blue above green, with a 440 Hz tone. An existing
//       file is refused, so a run never writes over anything; with --again (a run done twice, or run-shards' two sides on one data folder) a file that
//       is itself an MP4 (its first box "ftyp") is made again in its place, and any other file there is still refused.
//       Other clips, for the edges (acceptance-video-mode-edges; Frost 2026-11-22, Dietrich's list): --size another picture (two even numbers, 16 to
//       1920 each: a 4:3 320x240, a tall 180x320); --fps another rate (1 to 30), so a clip past the editor's 180 s stays small (182 s at 2 a second
//       is 364 frames); --no-sound no sound track at all. The seconds are 1 to 600, and at most 9000 frames in all.
//   verify_videomake.exe check <film.smk> <width> <height> <frames a second> <frames> [--no-sound]
//       reads the made film back as the game would find it: its header, read as hostile input by the editor's own reader (model::SmkHeader_Read),
//       must say SMK2, that size, that rate and that many frames, with sound track 0 at 22050 Hz 8-bit mono (AvP's message format); with
//       --no-sound, with no sound track at all (a film made from a video that has none).
// Exit 0: done, or the film is as asked; 1: it isn't (the words say what); 2: not understood, or the clip couldn't be made.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include <mfapi.h>

#include "../video_test_clip.h"
#include "smk_header.h"

// The make's own choices, from its words after <seconds>
struct MakeOptions
{
	bool again = false;
	uint32_t width = 320;
	uint32_t height = 180;
	uint32_t fps = 30;
	bool tone = true;
};

// A whole number from `least` to `most`, the whole of `text`
static bool WholeNumber(wchar_t const *text, long least, long most, long *number)
{
	wchar_t *end = nullptr;
	long const n = wcstol(text, &end, 10);

	if (!end || end == text || *end || n < least || n > most)
	{
		return false;
	}

	*number = n;
	return true;
}

// --again, --size <w>x<h>, --fps <n>, --no-sound, each at most once; anything else is not understood
static bool ReadMakeOptions(int count, wchar_t **words, MakeOptions *options)
{
	bool seen[4] = {};

	for (int i = 0; i < count; i++)
	{
		if (wcscmp(words[i], L"--again") == 0 && !seen[0])
		{
			seen[0] = options->again = true;
		}
		else if (wcscmp(words[i], L"--size") == 0 && !seen[1] && i + 1 < count)
		{
			wchar_t const *const size = words[++i];
			wchar_t const *const x = wcschr(size, L'x');
			long w = 0, h = 0;

			if (!x || x == size || wcslen(size) > 16)
			{
				return false;
			}

			std::wstring const left(size, x);

			if (!WholeNumber(left.c_str(), 16, 1920, &w) || !WholeNumber(x + 1, 16, 1920, &h) || (w % 2) != 0 || (h % 2) != 0)
			{
				return false;
			}

			options->width = (uint32_t)w;
			options->height = (uint32_t)h;
			seen[1] = true;
		}
		else if (wcscmp(words[i], L"--fps") == 0 && !seen[2] && i + 1 < count)
		{
			long fps = 0;

			if (!WholeNumber(words[++i], 1, 30, &fps))
			{
				return false;
			}

			options->fps = (uint32_t)fps;
			seen[2] = true;
		}
		else if (wcscmp(words[i], L"--no-sound") == 0 && !seen[3])
		{
			options->tone = false;
			seen[3] = true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

static int Make(wchar_t const *out, wchar_t const *secondsText, MakeOptions const &options)
{
	long seconds = 0;

	// With sound or without, 1 to 600 s. (Until 2026-11-22 a clip with sound was at most 8 s: tests/video_test_clip.cpp wrote every picture first and the
	// sound after, and Media Foundation's writer, waiting for the sound to catch up, never ended a clip of 9 s or more, as Frost measured: 9, 10, 20, 30,
	// 45 and 60 s at 30 a second each stayed a 79-byte file until stopped. It now writes the two in time order.)
	if (!WholeNumber(secondsText, 1, 600, &seconds) || (uint64_t)seconds * options.fps > 9000)
	{
		printf("verify_videomake: the seconds are a whole number from 1 to 600, at most 9000 frames in all\n");
		return 2;
	}

	bool const again = options.again;

	if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES)
	{
		// --again: only a file that is an MP4 (bytes 4 to 7 "ftyp", the first box of every MP4) goes; it is deleted here, and made again below
		char head[8] = {};
		DWORD got = 0;
		HANDLE const there = again ? CreateFileW(out, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
		bool const isMp4 = there != INVALID_HANDLE_VALUE && ReadFile(there, head, sizeof(head), &got, nullptr) && got == sizeof(head) && memcmp(head + 4, "ftyp", 4) == 0;

		if (there != INVALID_HANDLE_VALUE)
		{
			CloseHandle(there);
		}

		if (!isMp4 || !DeleteFileW(out))
		{
			printf("verify_videomake: %ls is there already: nothing is written over%s\n", out, again ? " (it is not an MP4, or it couldn't be deleted)" : "");
			return 2;
		}
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION)))
	{
		printf("verify_videomake: Media Foundation would not start\n");
		return 2;
	}

	uint32_t const width = options.width, height = options.height, fps = options.fps, frames = (uint32_t)seconds * fps;
	std::string why;
	bool const made = TestClip_Make(out, MFVideoFormat_H264, width, height, fps, frames, options.tone, [frames, width, height](uint32_t i, uint8_t *pixels)
	{
		if (i < frames / 2)
		{
			TestClip_Fill(pixels, width, 0, height, 255, 0, 0);
		}
		else
		{
			TestClip_Fill(pixels, width, 0, height / 2, 0, 0, 255);
			TestClip_Fill(pixels, width, height / 2, height, 0, 255, 0);
		}
	}, &why);

	MFShutdown();

	if (!made)
	{
		printf("verify_videomake: the clip couldn't be made: %s\n", why.c_str());
		return 2;
	}

	printf("verify_videomake: made %ls: %u x %u, %u frames at %u a second (%ld s), %s\n", out, width, height, frames, fps, seconds, options.tone ? "a 440 Hz tone" : "no sound");
	return 0;
}

static int Check(wchar_t const *film, int argc, wchar_t **argv, bool noSound)
{
	long wanted[4];

	for (int i = 0; i < 4; i++)
	{
		wchar_t *end = nullptr;

		wanted[i] = wcstol(argv[i], &end, 10);

		if (!end || *end || wanted[i] < 1 || wanted[i] > 100000)
		{
			printf("verify_videomake: the width, height, rate and frames are whole numbers from 1\n");
			return 2;
		}
	}

	(void)argc;
	HANDLE const file = CreateFileW(film, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		printf("verify_videomake: %ls can't be opened\n", film);
		return 1;
	}

	uint8_t bytes[model::kSmkHeaderBytes] = {};
	DWORD got = 0;
	BOOL const read = ReadFile(file, bytes, sizeof(bytes), &got, nullptr);

	CloseHandle(file);

	model::SmkHeader header;

	if (!read || model::SmkHeader_Read(bytes, got, &header) != model::SmkRead::Read)
	{
		printf("verify_videomake: %ls has no Smacker header the editor's reader takes\n", film);
		return 1;
	}

	double const fps = model::SmkHeader_Fps(header);
	model::SmkTrack const &sound = header.tracks[0];
	bool const soundAsAsked = noSound ? !sound.present : (sound.present && sound.rate == 22050 && sound.bits == 8 && sound.channels == 1);
	bool const good = strcmp(header.signature, "SMK2") == 0 && header.width == (uint32_t)wanted[0] && header.height == (uint32_t)wanted[1] &&
		fps > wanted[2] - 0.01 && fps < wanted[2] + 0.01 && header.frames == (uint32_t)wanted[3] && soundAsAsked;

	printf("verify_videomake: %ls: %s; asked SMK2 %ld x %ld, %ld frames at %ld fps, %s: %s\n", film, model::SmkHeader_Words(header).c_str(), wanted[0],
		wanted[1], wanted[3], wanted[2], noSound ? "no sound track" : "22050 Hz 8-bit mono", good ? "the same" : "NOT THE SAME");
	return good ? 0 : 1;
}

int wmain(int argc, wchar_t **argv)
{
	MakeOptions options;

	if (argc >= 4 && wcscmp(argv[1], L"make") == 0 && ReadMakeOptions(argc - 4, argv + 4, &options))
	{
		return Make(argv[2], argv[3], options);
	}

	if ((argc == 7 || (argc == 8 && wcscmp(argv[7], L"--no-sound") == 0)) && wcscmp(argv[1], L"check") == 0)
	{
		return Check(argv[2], 4, argv + 3, argc == 8);
	}

	printf("usage: verify_videomake.exe make <new .mp4> <seconds> [--again] [--size <width>x<height>] [--fps <n>] [--no-sound] | check <film.smk> <width> <height> <frames a second> <frames> [--no-sound]\n");
	return 2;
}
