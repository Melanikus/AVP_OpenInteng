// smacker_dll.h: Barry's Smacker decoder (avpx/dependencies/libsmackerdec, LGPL 2.1), as the FMV preview calls it: a table of the library's functions.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// LINKED IN (2026-11-21, the owner's order through Bishop: "GET MY DECODER OUT, WORKING", "STATIC LINK, SEPARATE BARRY'S LIBS"): the library is a static
// library of its own, built from its own sources with its own settings (win32/VS2026/libsmackerdec_static.vcxproj), and the editor links it, so no
// libsmackerdec.dll is shipped, pinned or loaded. The table holds the linked functions themselves. The game's own film player, which the editor compiles
// (avpx/3dc/video/SmackerPlayback.cpp), calls the same library; none of its ways in (the intro, the menus, a level's film screens) is called by the editor.
// The names keep "Dll" from the DLL this replaced (11-18 to 11-21): renaming them is left to the change that deletes the DLL path (smacker_dll.cpp says when).
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

#if defined(SMACKER_DECODER_FROM_DLL)
	// The retired DLL path only (smacker_dll.cpp): the hash to load with, and where it came from: the ini's (when it names 64 hex digits) or the build's pin.
	std::string SmackerDll_ExpectedHash(char const *settingsFile, bool *fromIni);
#endif

	// The decoder's table, never null now it is linked in (settingsFile and *why are for the retired DLL path, which could refuse); *note says which decoder it is.
	SmackerDllCalls const *SmackerDll_Get(char const *settingsFile, std::string *why, std::string *note);
}
