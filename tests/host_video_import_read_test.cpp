// The test for src/host/video_import's whole read, through Windows' own Media Foundation (the owner's proof of concept, 11-18). Written 2026-11-18 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the read is Media Foundation's decoders, its converter to RGB and its resampler, glued to the letterbox and the ticks; a slip there shows only in the finished video (a picture
// upside down, a frame short, the decoder's padding shown as a green edge, the sound at the wrong speed). No sample video is committed: the test MAKES its own first, with Media
// Foundation's sink writer, in its own folder, and reads them back:
//   an MP4 (H.264 + AAC), 320 x 180 at 30 a second for 2 s: the first second red, the second blue above green; a 440 Hz tone, stereo, 44100 Hz;
//   a WMV (WMV 9), 160 x 160 at 25 a second for 1 s, green, no sound;
//   a 64 x 64 MP4 at 5 a second for 182 s (past the most seconds).
// It checks the frame count (15 a second over the length), the size and the bars (exactly black), the colours where they belong (the top above the bottom), cropping, the sound's
// sample count (exactly 1470 a frame) and its pitch (440 Hz still: the resampler kept the speed), the probe's numbers, and refusals: no sound where there is none, a file that is
// noise, half a file, and a file too long.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <string>
#include <vector>

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include "video_import.h"
#include "video_test_clip.h"

using Microsoft::WRL::ComPtr;

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

static std::wstring folderOfTest;

static bool ReadAll(std::wstring const &path, std::vector<uint8_t> *bytes)
{
	std::string why;

	return host::VideoImport_ReadFile(path, bytes, &why) == host::VideoImportResult::Done;
}

static uint8_t const *Pixel(host::VideoImportClip const &clip, int32_t frame, int32_t x, int32_t y)
{
	return clip.frames.data() + (size_t)frame * host::kVideoImportFrameBytes + ((size_t)y * host::kVideoImportWidth + x) * 3;
}

static bool Near(uint8_t const *p, int r, int g, int b, int tolerance)
{
	return abs(p[0] - r) <= tolerance && abs(p[1] - g) <= tolerance && abs(p[2] - b) <= tolerance;
}

static std::string PixelWords(uint8_t const *p)
{
	return "(" + std::to_string(p[0]) + ", " + std::to_string(p[1]) + ", " + std::to_string(p[2]) + ")";
}

// Every pixel of rows [fromRow, toRow) and columns [fromColumn, toColumn) of every frame exactly black
static bool BlackEverywhere(host::VideoImportClip const &clip, int32_t fromRow, int32_t toRow, int32_t fromColumn, int32_t toColumn)
{
	for (int32_t f = 0; f < clip.frameCount; f++)
	{
		for (int32_t y = fromRow; y < toRow; y++)
		{
			for (int32_t x = fromColumn; x < toColumn; x++)
			{
				uint8_t const *const p = Pixel(clip, f, x, y);

				if (p[0] || p[1] || p[2])
				{
					return false;
				}
			}
		}
	}

	return true;
}

static void TestMp4(std::wstring const &path)
{
	puts("An MP4 the test made (H.264 + AAC, 320 x 180, 30 a second, 2 s; red, then blue above green; a 440 Hz tone):");

	std::vector<uint8_t> bytes;
	host::VideoImportInfo info;
	std::string why;

	Check(ReadAll(path, &bytes) && host::VideoImport_Probe(bytes, L"video/mp4", &info, &why) == host::VideoImportResult::Done, "the probe opens it (" + why + ")");
	Check(info.width == 320 && info.height == 180 && info.hasSound && info.length > 19000000 && info.length < 21000000,
		"the probe: 320 x 180, with sound, about 2 s (" + std::to_string(info.width) + " x " + std::to_string(info.height) + ", " + std::to_string((long long)info.length) + ")");
	Check(!host::VideoImport_IsAvpShape(info.width, info.height, info.parWidth, info.parHeight), "16:9: the tool asks fit or crop");

	host::VideoImportClip clip;
	host::VideoImportResult const result = host::VideoImport_Read(path, host::VideoImportFit::Fit, &clip, &why);

	Check(result == host::VideoImportResult::Done, std::string("read, fitted: done (") + host::VideoImport_Words(result) + "; " + why + ")");

	if (result != host::VideoImportResult::Done)
	{
		return;
	}

	Check(clip.frameCount == 30 && clip.frames.size() == (size_t)30 * host::kVideoImportFrameBytes, "30 frames (2 s at 15 a second), all their bytes (" + std::to_string(clip.frameCount) + ")");
	Check(clip.sourceWidth == 320 && clip.sourceHeight == 180, "the source's size kept: 320 x 180");
	Check(BlackEverywhere(clip, 0, 12, 0, 128) && BlackEverywhere(clip, 84, 96, 0, 128), "rows 0-11 and 84-95 exactly black in every frame (the bars of a 16:9 picture)");
	Check(Near(Pixel(clip, 5, 64, 48), 255, 0, 0, 40) && Near(Pixel(clip, 5, 1, 13), 255, 0, 0, 60) && Near(Pixel(clip, 5, 126, 82), 255, 0, 0, 60),
		"frame 5 (0.33 s): red in the middle and at the corners of the picture, no decoder's padding at its edges " + PixelWords(Pixel(clip, 5, 126, 82)));
	Check(Near(Pixel(clip, 22, 64, 20), 0, 0, 255, 40) && Near(Pixel(clip, 22, 64, 75), 0, 255, 0, 40),
		"frame 22 (1.47 s): blue above, green below (the top row first) " + PixelWords(Pixel(clip, 22, 64, 20)) + " " + PixelWords(Pixel(clip, 22, 64, 75)));
	Check(Near(Pixel(clip, 13, 64, 48), 255, 0, 0, 40) && Near(Pixel(clip, 16, 64, 20), 0, 0, 255, 40), "frames 13 and 16: red before the change at 1 s, blue after it (the ticks in time)");
	Check(clip.sound.size() == (size_t)30 * host::kVideoImportSamplesPerFrame, "the sound: exactly 1470 samples a frame (" + std::to_string((unsigned long long)clip.sound.size()) + ")");

	if (clip.sound.size() < 33075)
	{
		return;
	}

	int crossings = 0;
	double power = 0.0;

	for (size_t i = 11025; i < 33075; i++)
	{
		crossings += (clip.sound[i - 1] < 128) != (clip.sound[i] < 128) ? 1 : 0;
		power += (clip.sound[i] - 128.0) * (clip.sound[i] - 128.0);
	}

	double const loudness = sqrt(power / 22050.0);

	Check(crossings >= 860 && crossings <= 900, "the tone still 440 Hz: " + std::to_string(crossings) + " crossings in 1 s (880 is 440 Hz: the resampler kept the speed)");
	Check(loudness > 10.0, "the tone is there (loudness " + std::to_string(loudness) + " of 128)");

	host::VideoImportClip cropped;

	Check(host::VideoImport_Read(path, host::VideoImportFit::Crop, &cropped, &why) == host::VideoImportResult::Done && cropped.frameCount == 30, "read, cropped: done, 30 frames");

	if (cropped.frameCount == 30)
	{
		Check(Near(Pixel(cropped, 5, 64, 0), 255, 0, 0, 60) && Near(Pixel(cropped, 5, 0, 48), 255, 0, 0, 60) && Near(Pixel(cropped, 5, 127, 95), 255, 0, 0, 60),
			"cropped: no bars, red to every edge " + PixelWords(Pixel(cropped, 5, 64, 0)));
	}
}

// THE OWNER'S VIDEO MODE (11-22): "while video loads you'll put a loading message with %", and the rectangle "drawn over the video": the read's progress, a read
// stopped part way, and the source's own picture at a time
static void TestProgressAndStill(std::wstring const &mp4)
{
	puts("The read's progress, and the still under the rectangle:");

	std::vector<uint8_t> bytes;
	std::string why;

	Check(host::VideoImport_ReadFile(mp4, &bytes, &why) == host::VideoImportResult::Done, "the MP4's bytes are read (" + why + ")");

	std::vector<int32_t> said;
	host::VideoImportClip clip;
	host::VideoImportResult const read = host::VideoImport_ReadBytes(bytes, L"video/mp4", host::VideoImportFit::Fit, &clip, &why, host::kVideoImportMostSeconds,
		[&said](int32_t percent) { said.push_back(percent); return true; });
	bool rising = !said.empty();

	for (size_t i = 1; i < said.size(); i++)
	{
		rising = rising && said[i] >= said[i - 1];
	}

	Check(read == host::VideoImportResult::Done && clip.frameCount == 30, "a read told its progress still reads all 30 frames (" + why + ")");
	Check(said.size() >= 3 && rising && said.front() >= 0 && said.back() <= 100 && said.back() >= 90, "it is told how far it is, " + std::to_string(said.size()) +
		" times, never going back, from " + (said.empty() ? std::string("-") : std::to_string(said.front())) + " to " + (said.empty() ? std::string("-") : std::to_string(said.back())) + "%");

	host::VideoImportClip stopped;
	host::VideoImportResult const stop = host::VideoImport_ReadBytes(bytes, L"video/mp4", host::VideoImportFit::Fit, &stopped, &why, host::kVideoImportMostSeconds,
		[](int32_t) { return false; });

	Check(stop == host::VideoImportResult::Stopped && stopped.frameCount == 0 && stopped.frames.empty(), "a read its progress stops ends as Stopped, its clip untouched (" + why + ")");

	std::vector<uint8_t> still;
	uint32_t width = 0, height = 0;
	host::VideoImportResult const red = host::VideoImport_Still(bytes, L"video/mp4", host::kVideoImportTimeUnitsPerSecond / 2, 512, &still, &width, &height, &why);

	Check(red == host::VideoImportResult::Done && width == 320 && height == 180 && still.size() == (size_t)320 * 180 * 3, "the still at 0.5 s is the picture at its own size, 320 x 180 (" + why + ")");
	Check(still.size() >= 3 && Near(&still[((size_t)90 * 320 + 160) * 3], 255, 0, 0, 40), "and it is the red first second");

	host::VideoImportResult const blue = host::VideoImport_Still(bytes, L"video/mp4", host::kVideoImportTimeUnitsPerSecond * 3 / 2, 512, &still, &width, &height, &why);

	Check(blue == host::VideoImportResult::Done && still.size() == (size_t)320 * 180 * 3 && Near(&still[((size_t)30 * 320 + 160) * 3], 0, 0, 255, 40) &&
		Near(&still[((size_t)150 * 320 + 160) * 3], 0, 255, 0, 40), "the still at 1.5 s is blue above green, the top row first (" + why + ")");

	host::VideoImportResult const smaller = host::VideoImport_Still(bytes, L"video/mp4", 0, 160, &still, &width, &height, &why);

	Check(smaller == host::VideoImportResult::Done && width == 160 && height == 90 && still.size() == (size_t)160 * 90 * 3, "a still asked at most 160 a side is 160 x 90, its shape kept");
	Check(host::VideoImport_Still(bytes, L"video/mp4", 0, 0, &still, &width, &height, &why) != host::VideoImportResult::Done &&
		host::VideoImport_Still(bytes, L"video/mp4", -1, 512, &still, &width, &height, &why) != host::VideoImportResult::Done &&
		host::VideoImport_Still(bytes, L"video/mp4", 0, 512, nullptr, &width, &height, &why) != host::VideoImportResult::Done, "a size of 0, a time before the start or no place: refused");
	Check(host::VideoImport_Still(bytes, L"video/mp4", host::kVideoImportTimeUnitsPerSecond * 60, 512, &still, &width, &height, &why) != host::VideoImportResult::Done,
		"a time past the video's end has no picture: refused (" + why + ")");
}

static void TestWmv(std::wstring const &path)
{
	puts("A WMV the test made (WMV 9, 160 x 160, 25 a second, 1 s, green, no sound):");

	host::VideoImportClip clip;
	std::string why;
	host::VideoImportResult const result = host::VideoImport_Read(path, host::VideoImportFit::Fit, &clip, &why);

	Check(result == host::VideoImportResult::Done, std::string("read: done (") + host::VideoImport_Words(result) + "; " + why + ")");

	if (result != host::VideoImportResult::Done)
	{
		return;
	}

	Check(clip.frameCount == 15, "15 frames (1 s at 15 a second, from 25) (" + std::to_string(clip.frameCount) + ")");
	Check(clip.sound.empty(), "no sound: none made up");
	Check(BlackEverywhere(clip, 0, 96, 0, 16) && BlackEverywhere(clip, 0, 96, 112, 128), "columns 0-15 and 112-127 exactly black (the bars of a square picture)");
	Check(Near(Pixel(clip, 7, 64, 48), 0, 255, 0, 40) && Near(Pixel(clip, 7, 16, 0), 0, 255, 0, 60) && Near(Pixel(clip, 7, 111, 95), 0, 255, 0, 60),
		"green in the middle and at the picture's corners " + PixelWords(Pixel(clip, 7, 111, 95)));
}

static void TestRefused(std::wstring const &mp4, std::wstring const &longPath)
{
	puts("Refused, or read no further than it holds:");

	std::string why;
	host::VideoImportClip clip;
	std::vector<uint8_t> noise(4000);

	for (size_t i = 0; i < noise.size(); i++)
	{
		noise[i] = (uint8_t)((i * 2654435761u) >> 13);
	}

	host::VideoImportResult result = host::VideoImport_ReadBytes(noise, L"video/mp4", host::VideoImportFit::Fit, &clip, &why);

	Check(result != host::VideoImportResult::Done, std::string("4000 bytes of noise named .mp4: refused (") + host::VideoImport_Words(result) + ")");

	std::vector<uint8_t> bytes;

	if (ReadAll(mp4, &bytes))
	{
		std::vector<uint8_t> const half(bytes.begin(), bytes.begin() + bytes.size() / 2);

		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(half, L"video/mp4", host::VideoImportFit::Fit, &clip, &why);
		Check(result != host::VideoImportResult::Done || clip.frameCount <= 30, std::string("half of the MP4: no more than it holds (") + host::VideoImport_Words(result) + ", " + std::to_string(clip.frameCount) + " frames)");

		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(bytes, L"video/x-ms-wmv", host::VideoImportFit::Fit, &clip, &why);
		Check(result != host::VideoImportResult::Done || clip.frameCount == 30, std::string("the MP4 named .wmv: refused, or read as what it is (") + host::VideoImport_Words(result) + ")");
	}

	result = host::VideoImport_Read(longPath, host::VideoImportFit::Fit, &clip, &why);
	Check(result == host::VideoImportResult::TooLong && !why.empty(), std::string("182 s: refused as too long under the default 180 (") + host::VideoImport_Words(result) + "; " + why + ")");

	host::VideoImportClip longer;

	result = host::VideoImport_Read(longPath, host::VideoImportFit::Fit, &longer, &why, 200);
	Check(result == host::VideoImportResult::Done && longer.frameCount == 182 * 15, std::string("the same with the most raised to 200 s: read, 2730 frames (") + host::VideoImport_Words(result) + ", " +
		std::to_string(longer.frameCount) + ")");

	Check(host::VideoImport_ReadBytes(std::vector<uint8_t>(), L"video/mp4", host::VideoImportFit::Fit, &clip, &why) == host::VideoImportResult::BadCall, "no bytes: BadCall");
	Check(host::VideoImport_ReadBytes(noise, nullptr, host::VideoImportFit::Fit, &clip, &why) == host::VideoImportResult::BadCall, "no content type: BadCall");
}

int main()
{
	wchar_t temp[MAX_PATH];
	DWORD const length = GetTempPathW(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		puts("  FAILED  no folder for the test's files");
		return 1;
	}

	folderOfTest = std::wstring(temp) + L"host_video_import_read_test_" + std::to_wstring(GetCurrentProcessId());

	if (!CreateDirectoryW(folderOfTest.c_str(), nullptr))
	{
		puts("  FAILED  the test's folder can't be made");
		return 1;
	}

	// the sink writer needs Media Foundation on; the reads start and stop their own
	bool const com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	bool const on = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));

	Check(on, "Windows' Media Foundation starts (an N edition of Windows needs its Media Feature Pack)");

	std::wstring const mp4 = folderOfTest + L"\\made.mp4";
	std::wstring const wmv = folderOfTest + L"\\made.wmv";
	std::wstring const longOne = folderOfTest + L"\\long.mp4";
	std::string why;

	bool const madeMp4 = on && TestClip_Make(mp4, MFVideoFormat_H264, 320, 180, 30, 60, true, [](uint32_t i, uint8_t *pixels) {
		if (i < 30)
		{
			TestClip_Fill(pixels, 320, 0, 180, 255, 0, 0);
		}
		else
		{
			TestClip_Fill(pixels, 320, 0, 90, 0, 0, 255);
			TestClip_Fill(pixels, 320, 90, 180, 0, 255, 0);
		}
	}, &why);

	Check(madeMp4, "the test's MP4 is made (" + why + ")");
	why.clear();

	bool const madeWmv = on && TestClip_Make(wmv, MFVideoFormat_WMV3, 160, 160, 25, 25, false, [](uint32_t, uint8_t *pixels) { TestClip_Fill(pixels, 160, 0, 160, 0, 255, 0); }, &why);

	Check(madeWmv, "the test's WMV is made (" + why + ")");
	why.clear();

	bool const madeLong = on && TestClip_Make(longOne, MFVideoFormat_H264, 64, 64, 5, 910, false, [](uint32_t, uint8_t *pixels) { TestClip_Fill(pixels, 64, 0, 64, 128, 128, 128); }, &why);

	Check(madeLong, "the test's 182-second MP4 is made (" + why + ")");

	// THE OTHER CONTAINERS OF THE ONE LIST (the owner's ruling 11-22): each one Media Foundation's sink writer can make on this Windows is made (H.264 with an AAC tone,
	// 1 s, 160 x 120) and read back below through the read's own content type; one it can't make is said, not failed (which ones a sink writer has differs between
	// Windows builds; Matroska, WebM, QuickTime and MPEG program streams it never makes, and their decode depends on each user's codecs)
	struct Other { wchar_t const *ending; char const *name; };
	Other const others[] = { { L".m4v", ".m4v" }, { L".3gp", ".3gp" }, { L".mov", ".mov" }, { L".avi", ".avi" }, { L".ts", ".ts" }, { L".m2ts", ".m2ts" } };
	std::vector<std::wstring> madeOthers;
	std::vector<std::string> madeNames;

	for (Other const &other : others)
	{
		std::wstring const made = folderOfTest + L"\\made" + other.ending;
		std::string notMade;

		if (on && TestClip_Make(made, MFVideoFormat_H264, 160, 120, 15, 15, true, [](uint32_t, uint8_t *pixels) { TestClip_Fill(pixels, 160, 0, 120, 0, 0, 255); }, &notMade))
		{
			madeOthers.push_back(made);
			madeNames.push_back(other.name);
		}
		else
		{
			printf("  note  Media Foundation's sink writer makes no %s here (%s)\n", other.name, notMade.c_str());
			DeleteFileW(made.c_str());
		}
	}

	if (on)
	{
		MFShutdown();
	}

	for (size_t i = 0; i < madeOthers.size(); i++)
	{
		host::VideoImportClip clip;
		std::string readWhy;
		host::VideoImportResult const read = host::VideoImport_Read(madeOthers[i], host::VideoImportFit::Fit, &clip, &readWhy);

		Check(read == host::VideoImportResult::Done && clip.frameCount >= 14 && clip.frameCount <= 16 && clip.sourceWidth == 160 && clip.sourceHeight == 120 && !clip.sound.empty(),
			"a " + madeNames[i] + " made here reads back, as its content type in the one list says: " + std::to_string((long long)clip.frameCount) + " frames of 160 x 120, with sound (" + readWhy + ")");
	}

	if (madeMp4)
	{
		TestMp4(mp4);
		TestProgressAndStill(mp4);
	}

	if (madeWmv)
	{
		TestWmv(wmv);
	}

	if (madeMp4 && madeLong)
	{
		TestRefused(mp4, longOne);
	}

	DeleteFileW(mp4.c_str());
	DeleteFileW(wmv.c_str());
	DeleteFileW(longOne.c_str());
	RemoveDirectoryW(folderOfTest.c_str());

	if (com)
	{
		CoUninitialize();
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
