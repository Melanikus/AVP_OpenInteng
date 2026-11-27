// wav_check.cpp: see wav_check.h. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "wav_check.h"

#include <string.h>

namespace model
{
	namespace
	{
		uint32_t Four(uint8_t const *at)
		{
			return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
		}

		uint16_t Two(uint8_t const *at)
		{
			return (uint16_t)(at[0] | (at[1] << 8));
		}

		std::string Number(uint64_t n)
		{
			return std::to_string((unsigned long long)n);
		}

		// A chunk's four letters, safe to show (they come from the file)
		std::string Letters(uint8_t const *at)
		{
			std::string name(reinterpret_cast<char const *>(at), 4);

			for (char &c : name)
			{
				c = c >= ' ' && c <= '~' ? c : '?';
			}

			return name;
		}

		bool Refuse(std::string const &words, std::string *why)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}
	}

	bool WavCheck(uint8_t const *bytes, size_t size, WavFacts *facts, std::string *why)
	{
		if (facts)
		{
			*facts = WavFacts();
		}

		if (!bytes)
		{
			size = 0;
		}

		// THE GAME'S RULES, 1 to 13: the header, as WavHeader_Read reads it (wav_chunks.h)
		// 1
		if (size < 4 || memcmp(bytes, "RIFF", 4) != 0)
		{
			return Refuse("it doesn't start with RIFF", why);
		}

		// 2
		if (size < 8)
		{
			return Refuse("it ends inside its first 8 bytes", why);
		}

		uint64_t const riffSize = Four(bytes + 4);
		uint64_t const riffEnd = 8 + riffSize;

		// 3
		if (riffEnd > size)
		{
			return Refuse("its RIFF chunk says it runs past the end of the file (it says " + Number(riffSize) + " bytes; " + Number(size - 8) + " follow)", why);
		}

		// 4
		if (size < 12 || memcmp(bytes + 8, "WAVE", 4) != 0)
		{
			return Refuse("it isn't a WAVE file", why);
		}

		// 5
		if (size < 16 || memcmp(bytes + 12, "fmt ", 4) != 0)
		{
			return Refuse("its first chunk isn't its format" + (size >= 16 ? " (it is \"" + Letters(bytes + 12) + "\")" : std::string()), why);
		}

		uint64_t const formatBytes = size >= 20 ? Four(bytes + 16) : 0;

		// 6, then 7 (an 18-byte format's last two bytes, read only when the file holds them, as the game's read gives 0 past the end)
		if (formatBytes == 18)
		{
			if (size >= 38 && Two(bytes + 36) != 0)
			{
				return Refuse("its format has extra bytes, which plain PCM never has (" + Number(Two(bytes + 36)) + " of them)", why);
			}
		}
		else if (formatBytes != 16)
		{
			return Refuse("its format isn't the 16 bytes of plain PCM" + (size >= 20 ? " (this one: " + Number(formatBytes) + " bytes)" : std::string()), why);
		}

		// 8
		uint64_t const formatEnd = 20 + formatBytes;

		if (formatEnd > size)
		{
			return Refuse("it ends inside its format", why);
		}

		uint8_t const *const format = bytes + 20;
		uint16_t const tag = Two(format);
		uint16_t const channels = Two(format + 2);
		uint32_t const rate = Four(format + 4);
		uint32_t const perSecond = Four(format + 8);
		uint16_t const block = Two(format + 12);
		uint16_t const bits = Two(format + 14);

		// 9, 10: the sound found by its name after the format, each chunk before it stepped over by its length alone, as FindWavDataChunk and Rebellion's
		// reader step (wav_check.h)
		uint64_t at = formatEnd;

		for (;;)
		{
			if (at + 4 > size)
			{
				return Refuse("it has no sound (no data chunk)", why);
			}

			if (memcmp(bytes + at, "data", 4) == 0)
			{
				break;
			}

			if (at + 8 > size)
			{
				return Refuse("it has no sound (no data chunk)", why);
			}

			at += 8 + (uint64_t)Four(bytes + at + 4);
		}

		// 10
		if (at + 8 > size)
		{
			return Refuse("it ends inside the size of its sound", why);
		}

		uint64_t const dataBytes = Four(bytes + at + 4);
		uint64_t const dataStart = at + 8;

		// 11
		if (dataBytes > kWavMostDataBytes)
		{
			return Refuse("it holds more sound than the game takes from one WAV (this one: " + Number(dataBytes) + " bytes; 64 MB at most)", why);
		}

		// 12
		if (dataBytes > size - dataStart)
		{
			return Refuse("it says it holds more sound than the file has (" + Number(dataBytes) + " bytes said, " + Number(size - dataStart) + " there)", why);
		}

		// 13
		if (dataStart + dataBytes > riffEnd)
		{
			return Refuse("its sound runs past the end of its RIFF chunk", why);
		}

		// THE GAME'S RULES, 14 to 21: the format and the sound's size, as WavFormat_Refusal checks them (wav_chunks.h)
		if (tag != 1)
		{
			return Refuse("it isn't plain PCM sound (its format tag isn't 1) (this one: format " + Number(tag) + ")", why);
		}

		if (channels != 1 && channels != 2)
		{
			return Refuse("it has neither 1 channel nor 2 (this one: " + Number(channels) + ")", why);
		}

		if (bits != 8 && bits != 16)
		{
			return Refuse("its samples are neither 8 nor 16 bits (this one: " + Number(bits) + " bits)", why);
		}

		if (rate < kWavSlowestRate || rate > kWavFastestRate)
		{
			return Refuse("its rate isn't from 100 to 100,000 samples a second (this one: " + Number(rate) + ")", why);
		}

		// after those, a block is 1 to 4 bytes and a second at most 400,000, so neither product can run over
		if (block != channels * (bits / 8))
		{
			return Refuse("its block size isn't its channels times its bytes a sample (this one: " + Number(block) + ")", why);
		}

		if (perSecond != rate * block)
		{
			return Refuse("its bytes a second aren't its rate times its block size (this one: " + Number(perSecond) + ")", why);
		}

		if (dataBytes == 0)
		{
			return Refuse("it holds no sound", why);
		}

		if (dataBytes % block != 0)
		{
			return Refuse("its sound isn't a whole number of blocks (" + Number(dataBytes) + " bytes, in blocks of " + Number(block) + ")", why);
		}

		if (facts)
		{
			facts->channels = channels;
			facts->bits = bits;
			facts->rate = rate;
			facts->dataBytes = (uint32_t)dataBytes;
			facts->overSteamLimit = dataBytes > kWavSteamMostDataBytes;   // Steam AvP's limit: a warning
		}

		return true;
	}
}
