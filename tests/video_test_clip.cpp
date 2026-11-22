// video_test_clip.cpp: the test video maker (video_test_clip.h). Moved 2026-11-22 by Melanikus (Dietrich) from tests/host_video_import_read_test.cpp (Vasquez's, 11-18),
// unchanged but for its names, and (later that day, Frost's find) for writing the pictures and the sound in time order. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "video_test_clip.h"

#include <math.h>
#include <stdio.h>

#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

void TestClip_Fill(uint8_t *pixels, uint32_t width, uint32_t fromRow, uint32_t toRow, uint8_t r, uint8_t g, uint8_t b)
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

bool TestClip_Make(std::wstring const &path, GUID const &videoFormat, uint32_t width, uint32_t height, uint32_t fps, uint32_t frames, bool tone, TestClipPainter const &paint, std::string *why)
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

	// THE PICTURES AND THE SOUND IN TIME ORDER: each turn writes whichever stream is behind, so neither is ever far ahead of the other. Media Foundation's writer
	// stops and waits for a stream that falls far behind (Frost measured it 2026-11-22: with every picture written first, a clip with sound of 9 s or more never
	// ended), as a real recorder never does
	LONGLONG const frameTime = 10000000LL / fps;
	uint32_t const soundTotal = tone ? (uint32_t)((uint64_t)frames * 44100 / fps) : 0;   // the sound's samples, 44100 a second
	uint32_t const soundStep = 4410;                                                      // a tenth of a second of sound per write
	double const pi = 3.14159265358979323846;
	uint32_t picture = 0, soundAt = 0;
	char const *writing = "a picture's WriteSample";

	while (SUCCEEDED(hr) && (picture < frames || soundAt < soundTotal))
	{
		LONGLONG const pictureTime = (LONGLONG)picture * frameTime;
		LONGLONG const soundTime = (LONGLONG)soundAt * 10000000LL / 44100;
		bool const soundNext = soundAt < soundTotal && (picture >= frames || soundTime < pictureTime);
		ComPtr<IMFMediaBuffer> buffer;
		ComPtr<IMFSample> sample;
		BYTE *bytes = nullptr;

		if (!soundNext)
		{
			writing = "a picture's WriteSample";
			hr = MFCreateMemoryBuffer(width * height * 4, &buffer);

			if (SUCCEEDED(hr) && SUCCEEDED(hr = buffer->Lock(&bytes, nullptr, nullptr)))
			{
				paint(picture, bytes);
				buffer->Unlock();
				buffer->SetCurrentLength(width * height * 4);
				hr = MFCreateSample(&sample);
			}

			if (SUCCEEDED(hr))
			{
				sample->AddBuffer(buffer.Get());
				sample->SetSampleTime(pictureTime);
				sample->SetSampleDuration(frameTime);
				hr = writer->WriteSample(video, sample.Get());
			}

			picture++;
			continue;
		}

		// a 440 Hz tone, the same in both ears, 16-bit
		uint32_t const count = soundTotal - soundAt < soundStep ? soundTotal - soundAt : soundStep;

		writing = "a sound's WriteSample";
		hr = MFCreateMemoryBuffer(count * 4, &buffer);

		if (SUCCEEDED(hr) && SUCCEEDED(hr = buffer->Lock(&bytes, nullptr, nullptr)))
		{
			int16_t *const samples = reinterpret_cast<int16_t *>(bytes);

			for (uint32_t i = 0; i < count; i++)
			{
				int16_t const value = (int16_t)(12000.0 * sin(2.0 * pi * 440.0 * (double)(soundAt + i) / 44100.0));

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
			sample->SetSampleTime(soundTime);
			sample->SetSampleDuration((LONGLONG)count * 10000000LL / 44100);
			hr = writer->WriteSample(sound, sample.Get());
		}

		soundAt += count;
	}

	if (!step(writing))
	{
		return false;
	}

	hr = writer->Finalize();
	return step("Finalize");
}
