// smk_encoder.h: a user's video (video_import.h's clip) made into an AvP message video through the Smacker encoder, libsmackerenc.dll (Frost's, LGPL, AvPx's
// dependencies/libsmackerenc: Barry Duncan's libsmackerdec turned round), and written out. Shared by mp4-to-smk.exe and the editor's Import video (the owner's
// additions, 11-18). Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The encoder is a DLL with three plain C calls (its SmackerEncoder.h); this file only takes their addresses from a module the caller loaded (pinned: the caller's
// business) and calls them. No part of the encoder is compiled in.
// EXCEPT in mp4-to-smk's EncoderBuiltIn build (SMK_ENCODER_BUILT_IN; Bishop's antivirus experiment 11-19, Drake): there the encoder's own sources (avpx
// dependencies/libsmackerenc, LGPL 2.1) are compiled into that program, SmkEncoder_BuiltIn hands their three calls over, and nothing is loaded, so
// SmkEncoder_From is left out. That program as a whole is then under the LGPL 2.1; the editor never defines it and stays as above.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "video_import.h"

namespace host
{
	// As SmackerEncoder.h declares it (AvPx dependencies/libsmackerenc/include): kept to its layout until avpx carries the header at the editor's pin, then included from there
	struct SmackerEncodeInput
	{
		uint32_t width;
		uint32_t height;
		uint32_t frameCount;
		uint32_t fps;
		uint8_t const *rgb;
		size_t rgbBytes;
		uint32_t sampleRate;
		uint8_t const *pcm;
		size_t pcmBytes;
	};

	typedef int(__cdecl *SmackerEncodeCall)(SmackerEncodeInput const *in, uint8_t **smk, size_t *smkBytes);
	typedef void(__cdecl *SmackerFreeCall)(uint8_t *smk);
	typedef char const *(__cdecl *SmackerWordsCall)(int result);

	// The encoder's three calls
	struct SmkEncoder
	{
		SmackerEncodeCall encode = nullptr;
		SmackerFreeCall freeEncoded = nullptr;
		SmackerWordsCall words = nullptr;
	};

	// The DLL's file name, beside the program that loads it
	wchar_t const kSmkEncoderDll[] = L"libsmackerenc.dll";

#if defined(SMK_ENCODER_BUILT_IN)
	// The three calls of the encoder compiled into this program; false only when given nowhere to put them
	bool SmkEncoder_BuiltIn(SmkEncoder *encoder);
#else
	// The three calls taken from a loaded libsmackerenc.dll (void const * is its HMODULE); false with words when one is missing
	bool SmkEncoder_From(void *module, SmkEncoder *encoder, std::string *why);
#endif

	// The clip (128 x 96 R G B frames at 15 a second, 22050 Hz 8-bit mono sound or none) encoded: *smk the .smk's bytes. False with the encoder's own words.
	bool SmkEncoder_Encode(SmkEncoder const &encoder, VideoImportClip const &clip, std::vector<uint8_t> *smk, std::string *why);

	// A .smk's header line, as AvP-docs/tools/po/smk-info.js prints it ("<name>: SMK2 128 x 96, 609 frames at 15.00 fps (40.6 s), flags none; track 0: 22050 Hz, 8-bit,
	// mono, compressed"); read as hostile input (the first 104 bytes, every field bounded)
	std::string Smk_HeaderLine(std::vector<uint8_t> const &smk, std::string const &name);

	// A NEW file with these bytes, never over one that is there (CREATE_NEW), all of them on the disk or none (a short write is deleted); false with words
	bool Smk_WriteNew(std::wstring const &path, std::vector<uint8_t> const &bytes, std::string *why);

	// The file at path replaced whole: the bytes written beside it first, then moved into its place; the old file stays if anything fails; false with words
	bool Smk_Replace(std::wstring const &path, std::vector<uint8_t> const &bytes, std::string *why);
}
