// See smk_header.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "smk_header.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace model
{
	namespace
	{
		uint32_t U32(uint8_t const *at)
		{
			return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
		}

		std::string Number(double value, int decimals)
		{
			char text[64];

			snprintf(text, sizeof(text), "%.*f", decimals, value);

			// "15.00" -> "15", "15.20" -> "15.2"
			std::string words = text;

			if (words.find('.') != std::string::npos)
			{
				while (!words.empty() && words.back() == '0')
				{
					words.pop_back();
				}

				if (!words.empty() && words.back() == '.')
				{
					words.pop_back();
				}
			}

			return words;
		}

		std::string TrackWords(SmkTrack const &track)
		{
			return std::to_string(track.rate) + " Hz " + std::to_string(track.bits) + "-bit " + (track.channels == 2 ? "stereo" : "mono");
		}
	}

	SmkRead SmkHeader_Read(uint8_t const *bytes, size_t size, SmkHeader *out)
	{
		if (!bytes || size < kSmkHeaderBytes)
		{
			return SmkRead::TooShort;
		}

		if (memcmp(bytes, "SMK2", 4) != 0 && memcmp(bytes, "SMK4", 4) != 0)
		{
			return SmkRead::NotSmacker;
		}

		SmkHeader header;

		memcpy(header.signature, bytes, 4);
		header.signature[4] = 0;
		header.width = U32(bytes + 4);
		header.height = U32(bytes + 8);
		header.frames = U32(bytes + 12);
		header.rateField = (int32_t)U32(bytes + 16);
		header.flags = U32(bytes + 20);

		if (header.width == 0 || header.height == 0 || header.width > kSmkMostSide || header.height > kSmkMostSide || header.frames == 0 || header.frames > kSmkMostFrames)
		{
			return SmkRead::Impossible;
		}

		// in 64 bits: the most negative rate has no positive in 32
		int64_t const rate = header.rateField;

		header.msPerFrame = rate > 0 ? (double)rate : (rate < 0 ? (double)(-rate) / 100.0 : 100.0);

		for (int t = 0; t < kSmkTracks; t++)
		{
			uint32_t const word = U32(bytes + 72 + 4 * t);
			SmkTrack &track = header.tracks[t];

			track.present = (word & 0x40000000u) != 0;
			track.compressed = (word & 0x80000000u) != 0;
			track.bits = (word & 0x20000000u) ? 16 : 8;
			track.channels = (word & 0x10000000u) ? 2 : 1;
			track.rate = word & 0xffffffu;
		}

		if (out)
		{
			*out = header;
		}

		return SmkRead::Read;
	}

	double SmkHeader_Fps(SmkHeader const &header)
	{
		return header.msPerFrame > 0.0 ? 1000.0 / header.msPerFrame : 0.0;
	}

	std::string SmkHeader_DiffersFromAvp(SmkHeader const &header)
	{
		std::string differs;
		auto const add = [&differs](std::string const &one) { differs += (differs.empty() ? "" : "; ") + one; };

		if (strcmp(header.signature, "SMK2") != 0)
		{
			add(std::string(header.signature) + ", not SMK2");
		}

		if (header.width != 128 || header.height != 96)
		{
			add(std::to_string(header.width) + " x " + std::to_string(header.height) + ", not 128 x 96");
		}

		double const fps = SmkHeader_Fps(header);

		if (fps < 14.995 || fps > 15.005)
		{
			add(Number(fps, 2) + " fps, not 15");
		}

		SmkTrack const &first = header.tracks[0];

		if (!first.present)
		{
			add("no sound, not 22050 Hz 8-bit mono");
		}
		else if (first.rate != 22050 || first.bits != 8 || first.channels != 1)
		{
			add("sound " + TrackWords(first) + ", not 22050 Hz 8-bit mono");
		}

		return differs;
	}

	std::string SmkHeader_SecondsWords(double seconds)
	{
		return Number(seconds, 1) + " s";
	}

	std::string SmkHeader_Words(SmkHeader const &header)
	{
		double const seconds = (double)header.frames * header.msPerFrame / 1000.0;
		std::string words = std::string(header.signature) + " " + std::to_string(header.width) + " x " + std::to_string(header.height) + ", " + std::to_string(header.frames) + " frames at " +
			Number(SmkHeader_Fps(header), 2) + " fps (" + SmkHeader_SecondsWords(seconds) + "), ";

		return words + (header.tracks[0].present ? TrackWords(header.tracks[0]) : std::string("no sound"));
	}

	bool MessageFmvNumber(std::string const &name, int32_t *number)
	{
		static char const head[] = "message";
		static char const tail[] = ".smk";
		size_t const headSize = sizeof(head) - 1, tailSize = sizeof(tail) - 1;

		if (name.size() <= headSize + tailSize || name.size() > headSize + 3 + tailSize)
		{
			return false;
		}

		for (size_t i = 0; i < headSize; i++)
		{
			if (tolower((unsigned char)name[i]) != head[i])
			{
				return false;
			}
		}

		for (size_t i = 0; i < tailSize; i++)
		{
			if (tolower((unsigned char)name[name.size() - tailSize + i]) != tail[i])
			{
				return false;
			}
		}

		std::string const digits = name.substr(headSize, name.size() - headSize - tailSize);
		int32_t value = 0;

		for (char ch : digits)
		{
			if (ch < '0' || ch > '9')
			{
				return false;
			}

			value = value * 10 + (ch - '0');
		}

		// %d writes no leading 0 (FMVs/message%d.smk), so "message07.smk" is a file the game never opens
		if ((digits.size() > 1 && digits[0] == '0') || value > kMostMessageNumber)
		{
			return false;
		}

		if (number)
		{
			*number = value;
		}

		return true;
	}

	std::string MessageFmvName(int32_t number)
	{
		return "message" + std::to_string((long long)number) + ".smk";
	}
}
