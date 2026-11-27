// See smk_encoder.h. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "smk_encoder.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>

#if defined(SMK_ENCODER_BUILT_IN)
#include "SmackerEncoder.h"   // the encoder compiled into this program: avpx dependencies/libsmackerenc/include (the editor's, SMK_ENCODER_BUILT_IN)
#endif

#if defined(_M_IX86)
static_assert(sizeof(host::SmackerEncodeInput) == 36, "SmackerEncodeInput must match SmackerEncoder.h's layout (x86)");
#endif

namespace host
{
	namespace
	{
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

		uint32_t Read32(uint8_t const *at)
		{
			return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
		}
	}

#if defined(SMK_ENCODER_BUILT_IN)
	namespace
	{
		// The input handed on as the encoder's own type, field by field: the two types have one layout (the static_assert above) but are two types, so no cast
		int __cdecl EncodeBuiltIn(SmackerEncodeInput const *in, uint8_t **smk, size_t *smkBytes)
		{
			if (!in)
			{
				return Smacker_Encode(nullptr, smk, smkBytes);
			}

			::SmackerEncodeInput own = {};

			own.width = in->width;
			own.height = in->height;
			own.frameCount = in->frameCount;
			own.fps = in->fps;
			own.rgb = in->rgb;
			own.rgbBytes = in->rgbBytes;
			own.sampleRate = in->sampleRate;
			own.pcm = in->pcm;
			own.pcmBytes = in->pcmBytes;
			return Smacker_Encode(&own, smk, smkBytes);
		}
	}

	bool SmkEncoder_BuiltIn(SmkEncoder *encoder)
	{
		if (!encoder)
		{
			return false;
		}

		encoder->encode = EncodeBuiltIn;
		encoder->freeEncoded = Smacker_FreeEncoded;
		encoder->words = Smacker_EncodeWords;
		return true;
	}
#endif

	bool SmkEncoder_Encode(SmkEncoder const &encoder, VideoImportClip const &clip, std::vector<uint8_t> *smk, std::string *why)
	{
		if (!encoder.encode || !encoder.freeEncoded || !encoder.words || !smk || clip.frameCount <= 0 ||
			clip.frames.size() != (size_t)clip.frameCount * kVideoImportFrameBytes)
		{
			*why = "nothing to encode";
			return false;
		}

		SmackerEncodeInput in = {};

		in.width = (uint32_t)kVideoImportWidth;
		in.height = (uint32_t)kVideoImportHeight;
		in.frameCount = (uint32_t)clip.frameCount;
		in.fps = (uint32_t)kVideoImportFramesPerSecond;
		in.rgb = clip.frames.data();
		in.rgbBytes = clip.frames.size();
		in.sampleRate = (uint32_t)kVideoImportSoundRate;
		in.pcm = clip.sound.empty() ? nullptr : clip.sound.data();
		in.pcmBytes = clip.sound.size();

		uint8_t *encoded = nullptr;
		size_t encodedBytes = 0;
		int const result = encoder.encode(&in, &encoded, &encodedBytes);

		if (result != 0 || !encoded || encodedBytes == 0)
		{
			char const *const reason = encoder.words(result);

			encoder.freeEncoded(encoded);
			*why = std::string("the encoder says: ") + (reason ? reason : "?") + " (" + std::to_string(result) + ")";
			return false;
		}

		smk->assign(encoded, encoded + encodedBytes);
		encoder.freeEncoded(encoded);
		return true;
	}

	std::string Smk_HeaderLine(std::vector<uint8_t> const &smk, std::string const &name)
	{
		if (smk.size() < 104)
		{
			return name + ": too short for a Smacker header (" + std::to_string((unsigned long long)smk.size()) + " bytes)";
		}

		std::string const signature(reinterpret_cast<char const *>(smk.data()), 4);

		if (signature != "SMK2" && signature != "SMK4")
		{
			std::string shown;

			for (char const letter : signature)
			{
				shown.push_back(letter >= ' ' && letter <= '~' ? letter : '?');
			}

			return name + ": not a Smacker file (starts \"" + shown + "\")";
		}

		uint32_t const width = Read32(&smk[4]), height = Read32(&smk[8]), frames = Read32(&smk[12]), flags = Read32(&smk[20]);
		int32_t const rate = (int32_t)Read32(&smk[16]);

		if (width == 0 || height == 0 || width > 4096 || height > 4096 || frames == 0 || frames > 1000000)
		{
			return name + ": " + signature + " with impossible numbers (" + std::to_string(width) + " x " + std::to_string(height) + ", " + std::to_string(frames) + " frames)";
		}

		// the frame rate: positive = milliseconds a frame, negative = hundredths of a millisecond, 0 = 10 frames a second (smk-info.js's reading)
		double const msPerFrame = rate > 0 ? (double)rate : rate < 0 ? -(double)rate / 100.0 : 100.0;
		std::string tracks;

		for (int t = 0; t < 7; t++)
		{
			uint32_t const a = Read32(&smk[72 + t * 4]);

			if (!(a & 0x40000000u))
			{
				continue;
			}

			char words[96];

			snprintf(words, sizeof(words), "track %d: %u Hz, %d-bit, %s, %s", t, a & 0xffffffu, a & 0x20000000u ? 16 : 8, a & 0x10000000u ? "stereo" : "mono", a & 0x80000000u ? "compressed" : "raw");
			tracks += (tracks.empty() ? "" : "; ") + std::string(words);
		}

		std::string flagWords;
		char const *const flagNames[3] = { "ring frame", "Y interlaced", "Y doubled" };

		for (int bit = 0; bit < 3; bit++)
		{
			if (flags & (1u << bit))
			{
				flagWords += (flagWords.empty() ? "" : ", ") + std::string(flagNames[bit]);
			}
		}

		char line[256];

		snprintf(line, sizeof(line), "%s %u x %u, %u frames at %.2f fps (%.1f s), flags %s; %s", signature.c_str(), width, height, frames, 1000.0 / msPerFrame, frames * msPerFrame / 1000.0,
			flagWords.empty() ? "none" : flagWords.c_str(), tracks.empty() ? "no audio" : tracks.c_str());
		return name + ": " + line;
	}

	bool Smk_WriteNew(std::wstring const &path, std::vector<uint8_t> const &bytes, std::string *why)
	{
		HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			*why = error == ERROR_FILE_EXISTS ? Utf8(path) + " already exists" : "Windows won't make " + Utf8(path) + " (error " + std::to_string((unsigned long)error) + ")";
			return false;
		}

		size_t done = 0;
		bool good = true;

		while (good && done < bytes.size())
		{
			DWORD const ask = (DWORD)(bytes.size() - done > (1u << 24) ? (1u << 24) : bytes.size() - done);
			DWORD wrote = 0;

			good = WriteFile(file, bytes.data() + done, ask, &wrote, nullptr) && wrote == ask;
			done += wrote;
		}

		good = FlushFileBuffers(file) && good;
		CloseHandle(file);

		if (!good)
		{
			DeleteFileW(path.c_str());
			*why = "Windows stopped writing " + Utf8(path);
		}

		return good;
	}

	bool Smk_Replace(std::wstring const &path, std::vector<uint8_t> const &bytes, std::string *why)
	{
		std::wstring const part = path + L".part-" + std::to_wstring(GetCurrentProcessId());

		if (!Smk_WriteNew(part, bytes, why))
		{
			return false;
		}

		if (!MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			*why = "Windows won't put the new file in place of " + Utf8(path) + " (error " + std::to_string((unsigned long)GetLastError()) + ")";
			DeleteFileW(part.c_str());
			return false;
		}

		return true;
	}
}
