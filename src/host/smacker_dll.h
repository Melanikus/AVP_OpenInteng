// smacker_dll.h: Barry's Smacker decoder (avpx/dependencies/libsmackerdec, LGPL 2.1) used by the editor through its own DLL, libsmackerdec.dll beside the editor, loaded only by
// dll_load.h's pinned loader and called through GetProcAddress, as the game links it as a separate DLL (its source never enters the editor). For the FMV preview, and reusable by
// Import video (Bishop 11-18). The editor's own build keeps its stand-ins for the game's players (edge_media.cpp): these pointers are a separate way in, used only here.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH FILE LOADS (Bishop's ruling 11-18): the hash the build pinned (dll_pins.h kDllPinSmackerDecoder), unless the user's AVP_OpenInteng.ini names his own build's
// (SmackerDecoderSha256=, 64 hex digits, in [Editor]: LGPL 2.1 section 6), which then loads instead and the words say so. A build with no pin and no ini hash loads none.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "SmackerDecoder.h"   // the library's interface only: its handle and audio structs, and its functions' types

namespace host
{
	struct SmackerDllCalls
	{
		decltype(&::Smacker_Open) Open = nullptr;
		decltype(&::Smacker_Close) Close = nullptr;
		decltype(&::Smacker_GetNumAudioTracks) GetNumAudioTracks = nullptr;
		decltype(&::Smacker_GetAudioTrackDetails) GetAudioTrackDetails = nullptr;
		decltype(&::Smacker_GetAudioData) GetAudioData = nullptr;
		decltype(&::Smacker_GetNumFrames) GetNumFrames = nullptr;
		decltype(&::Smacker_GetFrameSize) GetFrameSize = nullptr;
		decltype(&::Smacker_GetNextFrame) GetNextFrame = nullptr;
		decltype(&::Smacker_GetFrameRate) GetFrameRate = nullptr;
		decltype(&::Smacker_GetPalette) GetPalette = nullptr;
		decltype(&::Smacker_GetFrame) GetFrame = nullptr;
	};

	// The hash to load with, and where it came from: the ini's (when it names 64 hex digits) or the build's pin. *fromIni tells which.
	std::string SmackerDll_ExpectedHash(char const *settingsFile, bool *fromIni);

	// The decoder, loaded once (the first call loads it; later calls give the same, or the same refusal). Null with *why (words for the status line) when refused; *note says which
	// file loaded ("libsmackerdec.dll, the build's own" or "..., the ini's SmackerDecoderSha256").
	SmackerDllCalls const *SmackerDll_Get(char const *settingsFile, std::string *why, std::string *note);
}
