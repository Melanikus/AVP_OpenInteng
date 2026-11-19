// See smacker_dll.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "smacker_dll.h"

#include <windows.h>

#include "dll_load.h"
#include "dll_pins.h"

namespace host
{
	namespace
	{
		bool tried = false;
		bool loaded = false;
		SmackerDllCalls calls;
		std::string refusal;
		std::string loadedNote;

		template <typename T> bool Find(HMODULE module, char const *name, T *out)
		{
			*out = reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(module, name)));
			return *out != nullptr;
		}
	}

	std::string SmackerDll_ExpectedHash(char const *settingsFile, bool *fromIni)
	{
		char named[80] = "";

		if (fromIni)
		{
			*fromIni = false;
		}

		if (settingsFile && settingsFile[0])
		{
			DWORD const length = GetPrivateProfileStringA("Editor", "SmackerDecoderSha256", "", named, (DWORD)sizeof(named), settingsFile);

			if (length == 64 && DllLoad_IsSha256Hex(named))
			{
				if (fromIni)
				{
					*fromIni = true;
				}

				return named;
			}
		}

		return kDllPinSmackerDecoder;
	}

	SmackerDllCalls const *SmackerDll_Get(char const *settingsFile, std::string *why, std::string *note)
	{
		if (!tried)
		{
			tried = true;

			bool fromIni = false;
			std::string const expected = SmackerDll_ExpectedHash(settingsFile, &fromIni);

			if (!DllLoad_IsSha256Hex(expected.c_str()))
			{
				refusal = "No FMV preview: this build pins no libsmackerdec.dll, and AVP_OpenInteng.ini names no SmackerDecoderSha256";
			}
			else
			{
				std::string loadWhy;
				HMODULE const module = DllLoad_Pinned(L"libsmackerdec.dll", expected.c_str(), &loadWhy);

				if (!module)
				{
					refusal = "No FMV preview: " + loadWhy + (fromIni ? " (the hash AVP_OpenInteng.ini names)" : "");
				}
				else if (Find(module, "Smacker_Open", &calls.Open) && Find(module, "Smacker_Close", &calls.Close) && Find(module, "Smacker_GetNumAudioTracks", &calls.GetNumAudioTracks) &&
					Find(module, "Smacker_GetAudioTrackDetails", &calls.GetAudioTrackDetails) && Find(module, "Smacker_GetAudioData", &calls.GetAudioData) &&
					Find(module, "Smacker_GetNumFrames", &calls.GetNumFrames) && Find(module, "Smacker_GetFrameSize", &calls.GetFrameSize) &&
					Find(module, "Smacker_GetNextFrame", &calls.GetNextFrame) && Find(module, "Smacker_GetFrameRate", &calls.GetFrameRate) &&
					Find(module, "Smacker_GetPalette", &calls.GetPalette) && Find(module, "Smacker_GetFrame", &calls.GetFrame))
				{
					loaded = true;
					loadedNote = fromIni ? "libsmackerdec.dll, the build AVP_OpenInteng.ini's SmackerDecoderSha256 names" : "libsmackerdec.dll, the one this editor was built with";
				}
				else
				{
					calls = SmackerDllCalls();
					FreeLibrary(module);
					refusal = "No FMV preview: libsmackerdec.dll lacks a function the editor calls";
				}
			}
		}

		if (!loaded)
		{
			if (why)
			{
				*why = refusal;
			}

			return nullptr;
		}

		if (note)
		{
			*note = loadedNote;
		}

		return &calls;
	}
}
