// See smacker_dll.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng; the decoder linked in 2026-11-21 by Melanikus (Drake). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
#include "smacker_dll.h"

#if !defined(SMACKER_DECODER_FROM_DLL)

namespace host
{
	namespace
	{
		// The decoder's own functions, linked into the editor (libsmackerdec_static.lib): the same table GetProcAddress used to fill from the DLL, so the
		// FMV preview calls them exactly as before
		SmackerDllCalls Linked()
		{
			SmackerDllCalls calls;

			calls.Open = &::Smacker_Open;
			calls.Close = &::Smacker_Close;
			calls.GetNumAudioTracks = &::Smacker_GetNumAudioTracks;
			calls.GetAudioTrackDetails = &::Smacker_GetAudioTrackDetails;
			calls.GetAudioData = &::Smacker_GetAudioData;
			calls.GetNumFrames = &::Smacker_GetNumFrames;
			calls.GetFrameSize = &::Smacker_GetFrameSize;
			calls.GetNextFrame = &::Smacker_GetNextFrame;
			calls.GetFrameRate = &::Smacker_GetFrameRate;
			calls.GetPalette = &::Smacker_GetPalette;
			calls.GetFrame = &::Smacker_GetFrame;
			return calls;
		}
	}

	SmackerDllCalls const *SmackerDll_Get(char const *, std::string *, std::string *note)
	{
		static SmackerDllCalls const calls = Linked();

		if (note)
		{
			*note = "Barry's libsmackerdec, linked into the editor";
		}

		return &calls;
	}
}

#else

// THE DLL PATH, RETIRED 2026-11-21 (the owner's order through Bishop: the decoder is linked in). No project defines SMACKER_DECODER_FROM_DLL, so none of this
// is compiled; it is deleted, with dll_load and dll_pins, as a change of its own once the owner has ruled on the LGPL question (a user's own build in its
// place). It is kept until then so that change can be read on its own.
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

#endif
