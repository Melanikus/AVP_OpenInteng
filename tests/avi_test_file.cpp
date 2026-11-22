// avi_test_file.cpp: a small AVI file made in memory (avi_test_file.h). The layout is AVI's own (RIFF "AVI ": the hdrl list with avih, a strl list for each
// stream with its strh and strf, the movi list with each frame's chunk, then idx1, the index). Written 2026-11-22 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "avi_test_file.h"

#include <math.h>
#include <string.h>

namespace
{
	void Put32(std::vector<uint8_t> *out, uint32_t v)
	{
		for (int i = 0; i < 4; i++)
		{
			out->push_back((uint8_t)(v >> (8 * i)));
		}
	}

	void Put16(std::vector<uint8_t> *out, uint16_t v)
	{
		out->push_back((uint8_t)v);
		out->push_back((uint8_t)(v >> 8));
	}

	void PutTag(std::vector<uint8_t> *out, char const tag[4])
	{
		out->insert(out->end(), tag, tag + 4);
	}

	// A chunk: its tag, its size, its bytes, and a byte more to make it even
	void PutChunk(std::vector<uint8_t> *out, char const tag[4], std::vector<uint8_t> const &bytes)
	{
		PutTag(out, tag);
		Put32(out, (uint32_t)bytes.size());
		out->insert(out->end(), bytes.begin(), bytes.end());

		if (bytes.size() & 1)
		{
			out->push_back(0);
		}
	}

	// A list: "LIST", its size, its kind, then what's in it
	void PutList(std::vector<uint8_t> *out, char const kind[4], std::vector<uint8_t> const &inside)
	{
		PutTag(out, "LIST");
		Put32(out, (uint32_t)(inside.size() + 4));
		PutTag(out, kind);
		out->insert(out->end(), inside.begin(), inside.end());
	}

	// A stream's header (AVISTREAMHEADER, 56 bytes)
	std::vector<uint8_t> StreamHeader(char const type[4], uint32_t handler, uint32_t scale, uint32_t rate, uint32_t length, uint32_t bufferSize, uint32_t sampleSize,
		uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> h;

		PutTag(&h, type);
		Put32(&h, handler);
		Put32(&h, 0);            // flags
		Put16(&h, 0);            // priority
		Put16(&h, 0);            // language
		Put32(&h, 0);            // initial frames
		Put32(&h, scale);
		Put32(&h, rate);
		Put32(&h, 0);            // start
		Put32(&h, length);
		Put32(&h, bufferSize);
		Put32(&h, 0xFFFFFFFFu);  // quality: the default
		Put32(&h, sampleSize);
		Put16(&h, 0);            // the frame's rectangle
		Put16(&h, 0);
		Put16(&h, (uint16_t)width);
		Put16(&h, (uint16_t)height);
		return h;
	}
}

uint32_t AviTest_Fourcc(char const letters[4])
{
	return (uint32_t)(uint8_t)letters[0] | ((uint32_t)(uint8_t)letters[1] << 8) | ((uint32_t)(uint8_t)letters[2] << 16) | ((uint32_t)(uint8_t)letters[3] << 24);
}

std::vector<uint8_t> AviTest_Make(uint32_t fourcc, uint32_t width, uint32_t height, uint32_t frames, uint32_t fps, uint16_t soundTag)
{
	bool const rgb = fourcc == 0;
	uint32_t const rowBytes = (width * 3 + 3) & ~3u;
	uint32_t const frameBytes = rgb ? rowBytes * height : width * height;
	uint32_t const rate = 22050;
	uint32_t const samplesPerFrame = rate / (fps ? fps : 1);
	uint32_t const soundBytesPerFrame = samplesPerFrame * 2;
	bool const sound = soundTag != 0;

	// THE HEADERS: avih, then the picture's strl, then the sound's
	std::vector<uint8_t> avih;

	Put32(&avih, 1000000 / (fps ? fps : 1));                                       // microseconds a frame
	Put32(&avih, (frameBytes + (sound ? soundBytesPerFrame : 0)) * fps);           // most bytes a second
	Put32(&avih, 0);                                                               // padding granularity
	Put32(&avih, 0x10);                                                            // AVIF_HASINDEX
	Put32(&avih, frames);
	Put32(&avih, 0);                                                               // initial frames
	Put32(&avih, sound ? 2 : 1);                                                   // streams
	Put32(&avih, frameBytes);                                                      // suggested buffer
	Put32(&avih, width);
	Put32(&avih, height);

	for (int i = 0; i < 4; i++)
	{
		Put32(&avih, 0);
	}

	std::vector<uint8_t> bitmap;   // BITMAPINFOHEADER

	Put32(&bitmap, 40);
	Put32(&bitmap, width);
	Put32(&bitmap, height);
	Put16(&bitmap, 1);
	Put16(&bitmap, rgb ? 24 : 12);
	Put32(&bitmap, rgb ? 0 : fourcc);
	Put32(&bitmap, frameBytes);

	for (int i = 0; i < 4; i++)
	{
		Put32(&bitmap, 0);
	}

	std::vector<uint8_t> videoList;

	PutChunk(&videoList, "strh", StreamHeader("vids", rgb ? 0 : fourcc, 1, fps, frames, frameBytes, 0, width, height));
	PutChunk(&videoList, "strf", bitmap);

	std::vector<uint8_t> headers;

	PutChunk(&headers, "avih", avih);
	PutList(&headers, "strl", videoList);

	if (sound)
	{
		std::vector<uint8_t> wave;   // WAVEFORMATEX, its 18 bytes

		Put16(&wave, soundTag);
		Put16(&wave, 1);
		Put32(&wave, rate);
		Put32(&wave, rate * 2);
		Put16(&wave, 2);
		Put16(&wave, 16);
		Put16(&wave, 0);

		std::vector<uint8_t> soundList;

		PutChunk(&soundList, "strh", StreamHeader("auds", 0, 2, rate * 2, frames * samplesPerFrame, soundBytesPerFrame, 2, 0, 0));
		PutChunk(&soundList, "strf", wave);
		PutList(&headers, "strl", soundList);
	}

	// THE FRAMES, each picture then its sound, and the index of them (each chunk's place from the "movi" word)
	std::vector<uint8_t> movi;
	std::vector<uint8_t> index;

	for (uint32_t f = 0; f < frames; f++)
	{
		std::vector<uint8_t> picture(frameBytes, 0);

		if (rgb)
		{
			for (uint32_t y = 0; y < height; y++)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					uint8_t *const p = &picture[(size_t)y * rowBytes + (size_t)x * 3];

					p[0] = (uint8_t)(f * 16);
					p[1] = (uint8_t)(x * 4);
					p[2] = (uint8_t)(y * 4);
				}
			}
		}

		char const *const pictureTag = rgb ? "00db" : "00dc";

		PutTag(&index, pictureTag);
		Put32(&index, 0x10);   // AVIIF_KEYFRAME
		Put32(&index, (uint32_t)movi.size() + 4);
		Put32(&index, frameBytes);
		PutChunk(&movi, pictureTag, picture);

		if (sound)
		{
			std::vector<uint8_t> tone;

			for (uint32_t s = 0; s < samplesPerFrame; s++)
			{
				int16_t const v = (int16_t)(8000.0 * sin(2.0 * 3.14159265358979 * 440.0 * (double)(f * samplesPerFrame + s) / rate));

				Put16(&tone, (uint16_t)v);
			}

			PutTag(&index, "01wb");
			Put32(&index, 0x10);
			Put32(&index, (uint32_t)movi.size() + 4);
			Put32(&index, (uint32_t)tone.size());
			PutChunk(&movi, "01wb", tone);
		}
	}

	std::vector<uint8_t> body;

	PutTag(&body, "AVI ");
	PutList(&body, "hdrl", headers);
	PutList(&body, "movi", movi);
	PutChunk(&body, "idx1", index);

	std::vector<uint8_t> file;

	PutTag(&file, "RIFF");
	Put32(&file, (uint32_t)body.size());
	file.insert(file.end(), body.begin(), body.end());
	return file;
}
