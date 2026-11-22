// mp4-to-smk.cpp: the owner's proof of concept (11-18: "a command line tool, resizes to default AVP video and audio, ask if resize or crop to fit aspect ratio, keep it simple"):
// a user's own video made into an AvP message video (.smk): 128 x 96, 15 frames a second, sound at 22050 Hz 8-bit mono. Its containers: video_import.h's ONE list
// (the owner's ruling 11-22: Media Foundation's common ones, .mp4 .m4v .mov .mkv .webm .avi .wmv .mpg .mpeg .vob .ts .m2ts .3gp; which decode depends on each Windows'
// codecs, and one it can't decode is named in words). Written 2026-11-18 by Melanikus (Vasquez)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
//   mp4-to-smk.exe <input video> <output.smk> [--fit | --crop] [--overwrite] [--level N | --level source] [--max-input-mb N] [--max-seconds N] [--dump <new folder>] [--encoder-sha256 HEX]
//
// The video is read by src/host/video_import (Windows' own Media Foundation; the file is hostile input, its checks are there), and its sound levelled by
// src/host/video_make, the same library the editor's Video mode links (videomake_static.vcxproj), so the two never make a film differently. A picture that isn't 4:3 makes the tool ASK: "[R]esize to
// fit (whole picture, black bars) or [C]rop to fill (edges cut)?"; --fit or --crop answers before it is asked. The sound is brought to AvP's own message videos' level (--level). The
// frames and sound go to Frost's Smacker encoder, libsmackerenc.dll (LGPL, Barry Duncan's libsmackerdec turned round), which must be beside this program: it is loaded from this
// program's own folder only, never from the current folder or PATH, and only the very file this build ships (its SHA-256 pinned at the build, dll_load.h / dll_pins.h), unless
// --encoder-sha256 names the user's own build of it (the owner's ruling 11-18; the LGPL's section 6). An
// output that exists is refused unless --overwrite is given (then the new file is written beside it first and put in its place whole). --dump writes what would be encoded into a new
// folder: frames.rgb (all frames, R G B, 128 x 96 each), sound.pcm (8-bit unsigned mono), sound.wav, and the first, middle and last frames as .bmp, to look at.
// At the end it says what it did: the picture's size, the frames, the length, and the output's header line as AvP-docs/tools/po/smk-info.js prints it.
//
// THE BUILD WITH THE ENCODER COMPILED IN (mp4-to-smk.vcxproj's EncoderBuiltIn=true, SMK_ENCODER_BUILT_IN; Bishop's antivirus experiment 11-19, Drake): the encoder's own sources
// (avpx dependencies/libsmackerenc at the pin, LGPL 2.1, with the same compiler settings as its DLL) are part of this program, so it loads no DLL of its own, has no pin to
// check and takes no --encoder-sha256. THAT PROGRAM AS A WHOLE IS UNDER THE GNU LESSER GENERAL PUBLIC LICENSE 2.1 (it holds libsmackerenc); this file stays AVP_OpenInteng's
// own code, MIT, and the editor is not changed by it.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <new>
#include <string>
#include <vector>

#include <windows.h>

#if !defined(SMK_ENCODER_BUILT_IN)
#include "dll_load.h"
#include "dll_pins.h"
#endif
#include "smk_encoder.h"
#include "video_import.h"
#include "video_make.h"

namespace
{

	int const kExitDone = 0;
	int const kExitRefused = 1;
	int const kExitUsage = 2;
#if !defined(SMK_ENCODER_BUILT_IN)
	int const kExitNoEncoder = 3;
#endif

	std::string Utf8(std::wstring const &wide)
	{
		if (wide.empty())
		{
			return std::string();
		}

		int const need = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
		std::string narrow(need > 0 ? (size_t)need : 0, '\0');

		if (need > 0)
		{
			WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &narrow[0], need, nullptr, nullptr);
		}

		return narrow;
	}

	void Usage()
	{
#if defined(SMK_ENCODER_BUILT_IN)
		puts("usage: mp4-to-smk.exe <input video> <output.smk> [--fit | --crop] [--overwrite] [--level N | --level source] [--max-input-mb N] [--max-seconds N] [--dump DIR]");
#else
		puts("usage: mp4-to-smk.exe <input video> <output.smk> [--fit | --crop] [--overwrite] [--level N | --level source] [--max-input-mb N] [--max-seconds N] [--dump DIR] [--encoder-sha256 HEX]");
#endif
		printf("  makes an AvP message video: 128 x 96, 15 fps, sound 22050 Hz 8-bit mono (%s in)\n", host::VideoImport_EndingsWords());
		puts("  --fit        a picture that isn't 4:3 is resized whole, with black bars (no question)");
		puts("  --crop       a picture that isn't 4:3 is cropped to fill, its edges cut (no question)");
		puts("  --overwrite  an output that exists is replaced");
		printf("  --max-seconds N  takes a video up to N seconds long (default %lld, at most %lld)\n", (long long)host::kVideoImportMostSeconds, (long long)host::kVideoImportLongestSeconds);
		printf("  --max-input-mb N  takes a file up to N MB (default %llu, at most %llu)\n", (unsigned long long)(host::kVideoImportMostBytes >> 20), (unsigned long long)(host::kVideoImportLongestBytes >> 20));
		puts("  --dump DIR   also writes what is encoded into the new folder DIR (frames.rgb, sound.pcm, sound.wav, three .bmp)");
#if !defined(SMK_ENCODER_BUILT_IN)
		puts("  --encoder-sha256 HEX  uses your own build of libsmackerenc.dll (beside this program), whose SHA-256 is HEX; otherwise only the one shipped with it loads");
#endif
		printf("  --level N    the sound's loudness, RMS in 8-bit steps (1 to 128; default %.1f, AvP's own message videos'); --level source keeps the source's\n", host::kVideoMakeStockLevel);
	}

	int Refused(std::string const &words)
	{
		printf("mp4-to-smk: refused: %s\n", words.c_str());
		return kExitRefused;
	}

	std::wstring FullPath(std::wstring const &path)
	{
		DWORD const need = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);

		if (need == 0 || need > 32768)
		{
			return std::wstring();
		}

		std::wstring full(need, L'\0');
		DWORD const got = GetFullPathNameW(path.c_str(), need, &full[0], nullptr);

		full.resize(got > 0 && got < need ? got : 0);
		return full;
	}

	bool EndsWith(std::wstring const &text, wchar_t const *ending)
	{
		size_t const length = wcslen(ending);

		return text.size() >= length && _wcsicmp(text.c_str() + text.size() - length, ending) == 0;
	}

	void Put16(std::vector<uint8_t> &out, uint32_t value)
	{
		out.push_back((uint8_t)value);
		out.push_back((uint8_t)(value >> 8));
	}

	void Put32(std::vector<uint8_t> &out, uint32_t value)
	{
		Put16(out, value & 0xffffu);
		Put16(out, value >> 16);
	}

	// One frame (R G B, top row first) as a 24-bit .bmp (B G R, bottom row first)
	std::vector<uint8_t> Bitmap(uint8_t const *frame)
	{
		uint32_t const rowBytes = host::kVideoImportWidth * 3;   // 384: already a multiple of 4
		uint32_t const pixels = rowBytes * host::kVideoImportHeight;
		std::vector<uint8_t> bmp;

		bmp.push_back('B');
		bmp.push_back('M');
		Put32(bmp, 54 + pixels);
		Put32(bmp, 0);
		Put32(bmp, 54);
		Put32(bmp, 40);
		Put32(bmp, (uint32_t)host::kVideoImportWidth);
		Put32(bmp, (uint32_t)host::kVideoImportHeight);
		Put16(bmp, 1);
		Put16(bmp, 24);
		Put32(bmp, 0);
		Put32(bmp, pixels);
		Put32(bmp, 2835);
		Put32(bmp, 2835);
		Put32(bmp, 0);
		Put32(bmp, 0);

		for (int32_t y = host::kVideoImportHeight - 1; y >= 0; y--)
		{
			uint8_t const *const row = frame + (size_t)y * rowBytes;

			for (int32_t x = 0; x < host::kVideoImportWidth; x++)
			{
				bmp.push_back(row[x * 3 + 2]);
				bmp.push_back(row[x * 3 + 1]);
				bmp.push_back(row[x * 3]);
			}
		}

		return bmp;
	}

	std::vector<uint8_t> Wave(std::vector<uint8_t> const &sound)
	{
		std::vector<uint8_t> wav;

		wav.insert(wav.end(), { 'R', 'I', 'F', 'F' });
		Put32(wav, 36 + (uint32_t)sound.size());
		wav.insert(wav.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
		Put32(wav, 16);
		Put16(wav, 1);   // PCM
		Put16(wav, 1);   // mono
		Put32(wav, (uint32_t)host::kVideoImportSoundRate);
		Put32(wav, (uint32_t)host::kVideoImportSoundRate);
		Put16(wav, 1);
		Put16(wav, 8);
		wav.insert(wav.end(), { 'd', 'a', 't', 'a' });
		Put32(wav, (uint32_t)sound.size());
		wav.insert(wav.end(), sound.begin(), sound.end());
		return wav;
	}

	bool Dump(std::wstring const &folder, host::VideoImportClip const &clip, std::string *why)
	{
		if (!CreateDirectoryW(folder.c_str(), nullptr))
		{
			*why = "the dump folder " + Utf8(folder) + " can't be made (it must not be there yet)";
			return false;
		}

		std::vector<uint8_t> const wav = Wave(clip.sound);
		int32_t const picks[3] = { 0, clip.frameCount / 2, clip.frameCount - 1 };
		wchar_t const *const names[3] = { L"\\frame-first.bmp", L"\\frame-middle.bmp", L"\\frame-last.bmp" };

		if (!host::Smk_WriteNew(folder + L"\\frames.rgb", clip.frames, why) || !host::Smk_WriteNew(folder + L"\\sound.pcm", clip.sound, why) ||
			!host::Smk_WriteNew(folder + L"\\sound.wav", wav, why))
		{
			return false;
		}

		for (int i = 0; i < 3; i++)
		{
			std::vector<uint8_t> const bmp = Bitmap(clip.frames.data() + (size_t)picks[i] * host::kVideoImportFrameBytes);

			if (!host::Smk_WriteNew(folder + names[i], bmp, why))
			{
				return false;
			}
		}

		return true;
	}

	// The owner's question; false when no answer comes (input ended)
	bool Ask(uint32_t width, uint32_t height, host::VideoImportFit *fit)
	{
		for (int tries = 0; tries < 5; tries++)
		{
			printf("This video is %u x %u. [R]esize to fit (whole picture, black bars) or [C]rop to fill (edges cut)? ", width, height);
			fflush(stdout);

			char answer[64];

			if (!fgets(answer, sizeof(answer), stdin))
			{
				puts("");
				return false;
			}

			char const first = answer[0] == ' ' ? answer[1] : answer[0];

			if (first == 'r' || first == 'R')
			{
				*fit = host::VideoImportFit::Fit;
				return true;
			}

			if (first == 'c' || first == 'C')
			{
				*fit = host::VideoImportFit::Crop;
				return true;
			}
		}

		return false;
	}
}

static int Run(int argc, wchar_t **argv)
{
	// no DLL from the current folder or PATH: Windows' own (Media Foundation's), and the encoder by its full path beside this program (the build with it compiled in loads none)
	SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);

	std::vector<std::wstring> named;
	bool fitGiven = false, cropGiven = false, overwrite = false;
	std::wstring dumpFolder;
#if !defined(SMK_ENCODER_BUILT_IN)
	std::string encoderSha256;   // the user's own encoder build's SHA-256 (64 hex digits), or "" for the one this build pins
#endif
	uint64_t mostBytes = host::kVideoImportMostBytes;      // the biggest file taken (--max-input-mb; a safety bound, raised only when asked)
	int64_t mostSeconds = host::kVideoImportMostSeconds;   // the longest video taken (--max-seconds; a safety bound, raised only when asked)
	double level = host::kVideoMakeStockLevel;

	for (int i = 1; i < argc; i++)
	{
		std::wstring const arg = argv[i];

		if (arg == L"--fit")
		{
			fitGiven = true;
		}
		else if (arg == L"--crop")
		{
			cropGiven = true;
		}
		else if (arg == L"--overwrite")
		{
			overwrite = true;
		}
		else if (arg == L"--dump" && i + 1 < argc)
		{
			dumpFolder = argv[++i];
		}
#if !defined(SMK_ENCODER_BUILT_IN)
		else if (arg == L"--encoder-sha256" && i + 1 < argc)
		{
			std::wstring const value = argv[++i];
			std::string hex;

			for (wchar_t const letter : value)
			{
				hex.push_back(letter < 128 ? (char)letter : '?');
			}

			if (!host::DllLoad_IsSha256Hex(hex.c_str()))
			{
				Usage();
				return kExitUsage;
			}

			encoderSha256 = hex;
		}
#endif
		else if (arg == L"--max-input-mb" && i + 1 < argc)
		{
			wchar_t *end = nullptr;
			long long const number = wcstoll(argv[++i], &end, 10);

			if (!end || *end != L'\0' || number < 1 || (uint64_t)number > (host::kVideoImportLongestBytes >> 20))
			{
				Usage();
				return kExitUsage;
			}

			mostBytes = (uint64_t)number << 20;
		}
		else if (arg == L"--max-seconds" && i + 1 < argc)
		{
			wchar_t *end = nullptr;
			long long const number = wcstoll(argv[++i], &end, 10);

			if (!end || *end != L'\0' || number < 1 || number > host::kVideoImportLongestSeconds)
			{
				Usage();
				return kExitUsage;
			}

			mostSeconds = (int64_t)number;
		}
		else if (arg == L"--level" && i + 1 < argc)
		{
			std::wstring const value = argv[++i];
			wchar_t *end = nullptr;
			double const number = value == L"source" ? 0.0 : wcstod(value.c_str(), &end);

			if (value != L"source" && (!end || *end != L'\0' || !(number > 0.0) || !(number <= 128.0)))
			{
				Usage();
				return kExitUsage;
			}

			level = number;   // 0: the source's own level
		}
		else if (arg.size() >= 2 && arg[0] == L'-' && arg[1] == L'-')
		{
			Usage();
			return kExitUsage;
		}
		else
		{
			named.push_back(arg);
		}
	}

	if (named.size() != 2 || (fitGiven && cropGiven))
	{
		Usage();
		return kExitUsage;
	}

	std::wstring const input = named[0];
	std::wstring const output = named[1];
	std::wstring const fullInput = FullPath(input);
	std::wstring const fullOutput = FullPath(output);

	if (!EndsWith(output, L".smk") || fullOutput.empty())
	{
		return Refused("the output's name must end in .smk");
	}

	if (!fullInput.empty() && _wcsicmp(fullInput.c_str(), fullOutput.c_str()) == 0)
	{
		return Refused("the output is the input");
	}

	if (GetFileAttributesW(fullOutput.c_str()) != INVALID_FILE_ATTRIBUTES && !overwrite)
	{
		return Refused(Utf8(fullOutput) + " already exists (--overwrite replaces it)");
	}

	std::vector<uint8_t> bytes;
	std::string why;
	host::VideoImportResult result = host::VideoImport_ReadFile(input, &bytes, &why, mostBytes);

	if (result != host::VideoImportResult::Done)
	{
		return Refused(why.empty() ? host::VideoImport_Words(result) : why);
	}

	wchar_t const *const contentType = host::VideoImport_ContentType(input);
	host::VideoImportInfo info;

	result = host::VideoImport_Probe(bytes, contentType, &info, &why, mostSeconds);

	if (result != host::VideoImportResult::Done)
	{
		return Refused(why.empty() ? host::VideoImport_Words(result) : why);
	}

	printf("mp4-to-smk: %s: %u x %u", Utf8(input).c_str(), info.width, info.height);

	if (info.parWidth != info.parHeight)
	{
		printf(" (pixels %u:%u)", info.parWidth, info.parHeight);
	}

	if (info.length >= 0)
	{
		printf(", %.1f s", (double)info.length / (double)host::kVideoImportTimeUnitsPerSecond);
	}

	printf(", %s\n", info.hasSound ? "with sound" : "no sound");

	host::VideoImportFit fit = host::VideoImportFit::Fit;
	bool const avpShape = host::VideoImport_IsAvpShape(info.width, info.height, info.parWidth, info.parHeight);

	if (!avpShape)
	{
		if (fitGiven || cropGiven)
		{
			fit = cropGiven ? host::VideoImportFit::Crop : host::VideoImportFit::Fit;
		}
		else if (!Ask(info.width, info.height, &fit))
		{
			return Refused("no answer to the question (give --fit or --crop)");
		}
	}

	host::VideoImportClip clip;

	result = host::VideoImport_ReadBytes(bytes, contentType, fit, &clip, &why, mostSeconds);

	if (result != host::VideoImportResult::Done)
	{
		return Refused(why.empty() ? host::VideoImport_Words(result) : why);
	}

	bytes.clear();
	bytes.shrink_to_fit();

	// the sound's level: brought to the target loudness, its loudest sample kept inside 8 bits (the 16-bit sound scaled, then its 8 bits), or the source's own (level 0):
	// video_make's, the editor's Video mode's own (with no volume asked here)
	if (!clip.sound16.empty())
	{
		std::string levelWords;

		host::VideoMake_Level(&clip, level, 0.0, &levelWords);
		printf("mp4-to-smk: %s\n", levelWords.c_str());
	}

	double const seconds = (double)clip.frameCount / host::kVideoImportFramesPerSecond;

	printf("mp4-to-smk: made %d frames of %d x %d (%s from %u x %u), %.2f s at %d fps; sound %s\n", clip.frameCount, host::kVideoImportWidth, host::kVideoImportHeight,
		avpShape ? "resized" : (fit == host::VideoImportFit::Crop ? "cropped to fill" : "resized to fit, black bars"), clip.sourceWidth, clip.sourceHeight, seconds, host::kVideoImportFramesPerSecond,
		clip.sound.empty() ? "none" : (std::to_string((unsigned long long)clip.sound.size()) + " samples at 22050 Hz 8-bit mono").c_str());

	if (!dumpFolder.empty())
	{
		if (!Dump(dumpFolder, clip, &why))
		{
			return Refused(why);
		}

		printf("mp4-to-smk: dumped into %s\n", Utf8(dumpFolder).c_str());
	}

	host::SmkEncoder calls;
	std::vector<uint8_t> smk;

#if defined(SMK_ENCODER_BUILT_IN)
	// the encoder compiled into this program: nothing to load or check
	if (!host::SmkEncoder_BuiltIn(&calls) || !host::SmkEncoder_Encode(calls, clip, &smk, &why))
	{
		return Refused(why);
	}
#else
	// the encoder: from this program's own folder only, and only the very file this build ships (its SHA-256 pinned when it was built: dll_pins.h), unless --encoder-sha256 names
	// the user's own build of it (the owner's ruling 11-18; the LGPL's section 6). dll_load.h holds the file from its hashing to its loading.
	bool const ownBuild = !encoderSha256.empty();

	if (!ownBuild && !host::DllLoad_IsSha256Hex(host::kDllPinSmackerEncoder))
	{
		printf("mp4-to-smk: no encoder: this build pins no libsmackerenc.dll (--encoder-sha256 names the one to use); nothing written to %s\n", Utf8(fullOutput).c_str());
		return kExitNoEncoder;
	}

	HMODULE const encoder = host::DllLoad_Pinned(host::kSmkEncoderDll, ownBuild ? encoderSha256.c_str() : host::kDllPinSmackerEncoder, &why);

	if (!encoder)
	{
		printf("mp4-to-smk: no encoder: %s%s; nothing written to %s\n", why.c_str(), ownBuild ? "" : " (a build of your own: --encoder-sha256 <its SHA-256>)", Utf8(fullOutput).c_str());
		return kExitNoEncoder;
	}

	if (ownBuild)
	{
		puts("mp4-to-smk: the encoder is your own build of libsmackerenc.dll (its SHA-256 as --encoder-sha256 gives it)");
	}

	if (!host::SmkEncoder_From(encoder, &calls, &why) || !host::SmkEncoder_Encode(calls, clip, &smk, &why))
	{
		return Refused(why);
	}
#endif

	// an output that is there was refused at the start unless --overwrite; a new one is never written over another (made meanwhile)
	if (!(overwrite ? host::Smk_Replace(fullOutput, smk, &why) : host::Smk_WriteNew(fullOutput, smk, &why)))
	{
		return Refused(why + (overwrite ? "" : " (--overwrite replaces it)"));
	}

	size_t const slash = fullOutput.find_last_of(L'\\');

	printf("mp4-to-smk: wrote %s, %llu bytes\n", Utf8(fullOutput).c_str(), (unsigned long long)smk.size());
	printf("%s\n", host::Smk_HeaderLine(smk, Utf8(slash == std::wstring::npos ? fullOutput : fullOutput.substr(slash + 1))).c_str());
	return kExitDone;
}

int wmain(int argc, wchar_t **argv)
{
	try
	{
		return Run(argc, argv);
	}
	catch (std::bad_alloc const &)
	{
		return Refused(std::string("there is not the memory for this video in a ") + (sizeof(void *) == 8 ? "64" : "32") + "-bit program (a shorter --max-seconds, or the 64-bit build)");
	}
}
