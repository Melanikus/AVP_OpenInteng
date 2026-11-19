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

// paints frame `index` into `pixels` (B, G, R, unused; the top row first; width x height)
typedef std::function<void(uint32_t index, uint8_t *pixels)> Painter;

static void Fill(uint8_t *pixels, uint32_t width, uint32_t fromRow, uint32_t toRow, uint8_t r, uint8_t g, uint8_t b)
{
	for (uint32_t y = fromRow; y < toRow; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t *const p = pixels + ((size_t)y * width + x) * 4;

			p[0] = b;
			p[1] = g;
			p[2] = r;
			p[3] = 0;
		}
	}
}

// A video made with Media Foundation's sink writer (the container from the name's ending); false with the step that failed in *why
static bool MakeVideo(std::wstring const &path, GUID const &videoFormat, uint32_t width, uint32_t height, uint32_t fps, uint32_t frames, bool tone, Painter const &paint, std::string *why)
{
	ComPtr<IMFSinkWriter> writer;
	ComPtr<IMFMediaType> out, in;
	DWORD video = 0, sound = 0;
	HRESULT hr = MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, &writer);

	auto step = [&](char const *name) {
		if (FAILED(hr) && why->empty())
		{
			char words[16];

			snprintf(words, sizeof(words), "0x%08lx", (unsigned long)hr);
			*why = std::string(name) + " " + words;
		}

		return SUCCEEDED(hr);
	};

	if (!step("MFCreateSinkWriterFromURL"))
	{
		return false;
	}

	hr = MFCreateMediaType(&out);

	if (SUCCEEDED(hr))
	{
		out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		out->SetGUID(MF_MT_SUBTYPE, videoFormat);
		out->SetUINT32(MF_MT_AVG_BITRATE, 2000000);
		out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeSize(out.Get(), MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(out.Get(), MF_MT_FRAME_RATE, fps, 1);
		MFSetAttributeRatio(out.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = writer->AddStream(out.Get(), &video);
	}

	if (!step("the picture's AddStream"))
	{
		return false;
	}

	hr = MFCreateMediaType(&in);

	if (SUCCEEDED(hr))
	{
		in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
		in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		in->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);   // the top row first
		MFSetAttributeSize(in.Get(), MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(in.Get(), MF_MT_FRAME_RATE, fps, 1);
		MFSetAttributeRatio(in.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = writer->SetInputMediaType(video, in.Get(), nullptr);
	}

	if (!step("the picture's SetInputMediaType"))
	{
		return false;
	}

	if (tone)
	{
		ComPtr<IMFMediaType> aac, pcm;

		hr = MFCreateMediaType(&aac);

		if (SUCCEEDED(hr))
		{
			aac->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
			aac->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
			aac->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			aac->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
			aac->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
			aac->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
			hr = writer->AddStream(aac.Get(), &sound);
		}

		if (SUCCEEDED(hr))
		{
			hr = MFCreateMediaType(&pcm);
		}

		if (SUCCEEDED(hr))
		{
			pcm->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
			pcm->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
			pcm->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			pcm->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
			pcm->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
			pcm->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
			pcm->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 44100 * 4);
			hr = writer->SetInputMediaType(sound, pcm.Get(), nullptr);
		}

		if (!step("the sound's stream"))
		{
			return false;
		}
	}

	hr = writer->BeginWriting();

	if (!step("BeginWriting"))
	{
		return false;
	}

	LONGLONG const frameTime = 10000000LL / fps;

	for (uint32_t i = 0; i < frames && SUCCEEDED(hr); i++)
	{
		ComPtr<IMFMediaBuffer> buffer;
		ComPtr<IMFSample> sample;
		BYTE *pixels = nullptr;

		hr = MFCreateMemoryBuffer(width * height * 4, &buffer);

		if (SUCCEEDED(hr) && SUCCEEDED(hr = buffer->Lock(&pixels, nullptr, nullptr)))
		{
			paint(i, pixels);
			buffer->Unlock();
			buffer->SetCurrentLength(width * height * 4);
			hr = MFCreateSample(&sample);
		}

		if (SUCCEEDED(hr))
		{
			sample->AddBuffer(buffer.Get());
			sample->SetSampleTime((LONGLONG)i * frameTime);
			sample->SetSampleDuration(frameTime);
			hr = writer->WriteSample(video, sample.Get());
		}
	}

	if (!step("a picture's WriteSample"))
	{
		return false;
	}

	if (tone)
	{
		uint32_t const total = (uint32_t)((uint64_t)frames * 44100 / fps);
		double const pi = 3.14159265358979323846;

		for (uint32_t at = 0; at < total && SUCCEEDED(hr); at += 4410)
		{
			uint32_t const count = total - at < 4410 ? total - at : 4410;
			ComPtr<IMFMediaBuffer> buffer;
			ComPtr<IMFSample> sample;
			BYTE *bytes = nullptr;

			hr = MFCreateMemoryBuffer(count * 4, &buffer);

			if (SUCCEEDED(hr) && SUCCEEDED(hr = buffer->Lock(&bytes, nullptr, nullptr)))
			{
				int16_t *const samples = reinterpret_cast<int16_t *>(bytes);

				for (uint32_t i = 0; i < count; i++)
				{
					int16_t const value = (int16_t)(12000.0 * sin(2.0 * pi * 440.0 * (double)(at + i) / 44100.0));

					samples[i * 2] = value;
					samples[i * 2 + 1] = value;
				}

				buffer->Unlock();
				buffer->SetCurrentLength(count * 4);
				hr = MFCreateSample(&sample);
			}

			if (SUCCEEDED(hr))
			{
				sample->AddBuffer(buffer.Get());
				sample->SetSampleTime((LONGLONG)at * 10000000LL / 44100);
				sample->SetSampleDuration((LONGLONG)count * 10000000LL / 44100);
				hr = writer->WriteSample(sound, sample.Get());
			}
		}

		if (!step("a sound's WriteSample"))
		{
			return false;
		}
	}

	hr = writer->Finalize();
	return step("Finalize");
}

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

	bool const madeMp4 = on && MakeVideo(mp4, MFVideoFormat_H264, 320, 180, 30, 60, true, [](uint32_t i, uint8_t *pixels) {
		if (i < 30)
		{
			Fill(pixels, 320, 0, 180, 255, 0, 0);
		}
		else
		{
			Fill(pixels, 320, 0, 90, 0, 0, 255);
			Fill(pixels, 320, 90, 180, 0, 255, 0);
		}
	}, &why);

	Check(madeMp4, "the test's MP4 is made (" + why + ")");
	why.clear();

	bool const madeWmv = on && MakeVideo(wmv, MFVideoFormat_WMV3, 160, 160, 25, 25, false, [](uint32_t, uint8_t *pixels) { Fill(pixels, 160, 0, 160, 0, 255, 0); }, &why);

	Check(madeWmv, "the test's WMV is made (" + why + ")");
	why.clear();

	bool const madeLong = on && MakeVideo(longOne, MFVideoFormat_H264, 64, 64, 5, 910, false, [](uint32_t, uint8_t *pixels) { Fill(pixels, 64, 0, 64, 128, 128, 128); }, &why);

	Check(madeLong, "the test's 182-second MP4 is made (" + why + ")");

	if (on)
	{
		MFShutdown();
	}

	if (madeMp4)
	{
		TestMp4(mp4);
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
