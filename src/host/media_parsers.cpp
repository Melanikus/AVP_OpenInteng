// media_parsers.cpp: whose code parses a video (media_parsers.h says what and why). Written 2026-11-22 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// The order, for one video (video_import.cpp asks each step): the container's reader made from Windows' own handler (MediaParsers_CreateSource); each stream's format
// checked against the list (MediaParsers_FormatTaken), before any decoder exists; each decoder chosen and proved by the editor (MediaParsers_ChooseDecoder), and then
// DRIVEN BY THE EDITOR ITSELF on the stream's compressed samples, the reader reading each stream at its own type (card 2c, 2026-11-23: so the reader makes no decoder
// and Media Foundation never picks one; "the one window left" closed); then, before the first sample, everything the reader used and loaded checked
// (MediaParsers_CheckReader). host_media_parsers_test plants decoders for this process and shows a read making none of them.
#include "media_parsers.h"

#include <stdio.h>

#include <algorithm>
#include <map>
#include <set>

#include <bcrypt.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mscat.h>
#include <psapi.h>
#include <softpub.h>
#include <wintrust.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
	size_t const kMostPathLetters = 32768;
	size_t const kMostRegistryLetters = 1024;   // a module's path as the registry gives it (a longer one is not read, and that CLSID has no module here)

	std::string Narrow(std::wstring const &wide)
	{
		std::string narrow;

		for (wchar_t const c : wide)
		{
			narrow.push_back(c >= 0x20 && c < 0x7f ? (char)c : '?');
		}

		return narrow;
	}

	std::wstring Lower(std::wstring text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
		return text;
	}

	// The Store package a file is in (the folder under WindowsApps, its name starting with the publisher's: "DolbyLaboratories.DolbyDigitalPlusDecoderOEM_..."), or ""
	std::string StorePackageOf(std::wstring const &path)
	{
		size_t const apps = Lower(path).find(L"\\windowsapps\\");

		if (apps == std::wstring::npos)
		{
			return std::string();
		}

		size_t const from = apps + 13;
		size_t const to = path.find(L'\\', from);

		return Narrow(path.substr(from, to == std::wstring::npos ? std::wstring::npos : to - from));
	}

	// A plain path on a drive of this computer ("C:\..."): the only kind a module's file is ever opened by here, so this check never contacts another computer
	bool IsPlainDrivePath(std::wstring const &path)
	{
		return path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' && path[2] == L'\\';
	}

	// The signer of a checked signature: its first signer's certificate, by its simple name ("Microsoft Windows")
	std::wstring SignerOf(HANDLE state)
	{
		CRYPT_PROVIDER_DATA *const provider = WTHelperProvDataFromStateData(state);
		CRYPT_PROVIDER_SGNR *const signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;

		if (!signer || signer->csCertChain < 1 || !signer->pasCertChain || !signer->pasCertChain[0].pCert)
		{
			return std::wstring();
		}

		wchar_t name[256] = {};

		CertGetNameStringW(signer->pasCertChain[0].pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, (DWORD)(sizeof(name) / sizeof(name[0])));
		return name;
	}

	// One signature check, from what is on this PC only (no revocation list fetched, no address followed), and its signer when it holds
	LONG Verify(WINTRUST_DATA *data, std::wstring *signer)
	{
		GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;

		data->cbStruct = sizeof(*data);
		data->dwUIChoice = WTD_UI_NONE;
		data->fdwRevocationChecks = WTD_REVOKE_NONE;
		data->dwStateAction = WTD_STATEACTION_VERIFY;
		data->dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;

		LONG const result = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, data);

		if (result == ERROR_SUCCESS)
		{
			*signer = SignerOf(data->hWVTStateData);
		}

		data->dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, data);
		return result;
	}

	// A file signed in Windows' own catalogs (as System32's files are): its hash looked up in them, SHA-256 first, then SHA-1, and that catalog's signature checked
	bool CatalogSigned(std::wstring const &path, HANDLE file, std::wstring *signer)
	{
		for (wchar_t const *algorithm : { BCRYPT_SHA256_ALGORITHM, BCRYPT_SHA1_ALGORITHM })
		{
			HCATADMIN admin = nullptr;

			if (!CryptCATAdminAcquireContext2(&admin, nullptr, algorithm, nullptr, 0))
			{
				continue;
			}

			bool good = false;
			BYTE hash[64] = {};
			DWORD size = sizeof(hash);

			SetFilePointer(file, 0, nullptr, FILE_BEGIN);

			if (CryptCATAdminCalcHashFromFileHandle2(admin, file, &size, hash, 0) && size > 0 && size <= sizeof(hash))
			{
				HCATINFO const catalog = CryptCATAdminEnumCatalogFromHash(admin, hash, size, 0, nullptr);

				if (catalog)
				{
					CATALOG_INFO info = {};

					info.cbStruct = sizeof(info);

					if (CryptCATCatalogInfoFromContext(catalog, &info, 0))
					{
						std::wstring tag;
						wchar_t const digits[] = L"0123456789ABCDEF";

						for (DWORD i = 0; i < size; i++)
						{
							tag.push_back(digits[hash[i] >> 4]);
							tag.push_back(digits[hash[i] & 15]);
						}

						WINTRUST_CATALOG_INFO member = {};

						member.cbStruct = sizeof(member);
						member.pcwszCatalogFilePath = info.wszCatalogFile;
						member.pcwszMemberFilePath = path.c_str();
						member.pcwszMemberTag = tag.c_str();
						member.hMemberFile = file;
						member.pbCalculatedFileHash = hash;
						member.cbCalculatedFileHash = size;
						member.hCatAdmin = admin;

						WINTRUST_DATA data = {};

						data.dwUnionChoice = WTD_CHOICE_CATALOG;
						data.pCatalog = &member;
						good = Verify(&data, signer) == ERROR_SUCCESS;
					}

					CryptCATAdminReleaseCatalogContext(admin, catalog, 0);
				}
			}

			CryptCATAdminReleaseContext(admin, 0);

			if (good)
			{
				return true;
			}
		}

		return false;
	}

	// The same file, however its path is written (a 32-bit program's System32 is SysWOW64, but for DriverStore): the volume and file number Windows gives it.
	// Opened only by a plain drive path, for its identity only
	bool FileId(std::wstring const &path, std::pair<uint64_t, uint64_t> *id)
	{
		if (!IsPlainDrivePath(path))
		{
			return false;
		}

		HANDLE const file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		BY_HANDLE_FILE_INFORMATION info = {};
		bool const good = GetFileInformationByHandle(file, &info) != FALSE;

		CloseHandle(file);
		*id = std::make_pair((uint64_t)info.dwVolumeSerialNumber, ((uint64_t)info.nFileIndexHigh << 32) | info.nFileIndexLow);
		return good;
	}

	// A CLSID's module: InprocServer32's default value, the user's own registration first as COM reads it, %variables% filled in; a bare name is looked for in
	// the system folder, as LoadLibrary would find it there
	std::wstring ModuleOfClsid(std::wstring const &clsid)
	{
		for (HKEY const root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
		{
			wchar_t value[kMostRegistryLetters] = {};
			DWORD bytes = sizeof(value) - sizeof(wchar_t);
			std::wstring const key = L"SOFTWARE\\Classes\\CLSID\\" + clsid + L"\\InprocServer32";

			if (RegGetValueW(root, key.c_str(), nullptr, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, value, &bytes) != ERROR_SUCCESS)
			{
				continue;
			}

			wchar_t expanded[kMostRegistryLetters] = {};
			DWORD const letters = ExpandEnvironmentStringsW(value, expanded, (DWORD)(sizeof(expanded) / sizeof(expanded[0])));
			std::wstring path = letters > 0 && letters <= sizeof(expanded) / sizeof(expanded[0]) ? std::wstring(expanded) : std::wstring(value);

			if (!path.empty() && path.find_first_of(L"\\/") == std::wstring::npos)
			{
				wchar_t system[MAX_PATH] = {};
				UINT const got = GetSystemDirectoryW(system, MAX_PATH);

				if (got > 0 && got < MAX_PATH)
				{
					path = std::wstring(system) + L"\\" + path;
				}
			}

			return path;
		}

		return std::wstring();
	}

	// Every CLSID named under a key, braces added where the key leaves them off. Two forms: the byte-stream and scheme handlers' key holds a subkey for each file
	// ending or scheme, whose VALUES' names are the CLSIDs ({...} = "description"); the transforms' and the DirectX media objects' keys hold a SUBKEY named for each
	// CLSID (and a Categories subkey, which is no CLSID and is passed over)
	enum class ClsidForm { ValuesOfSubkeys, Subkeys };

	void ClsidsUnder(HKEY root, wchar_t const *path, ClsidForm form, std::set<std::wstring> *clsids)
	{
		HKEY key = nullptr;

		if (RegOpenKeyExW(root, path, 0, KEY_READ, &key) != ERROR_SUCCESS)
		{
			return;
		}

		auto const add = [clsids](std::wstring name) {
			if (name.size() == 36)
			{
				name = L"{" + name + L"}";
			}

			if (name.size() == 38 && name[0] == L'{' && name[37] == L'}')
			{
				clsids->insert(name);
			}
		};

		for (DWORD i = 0;; i++)
		{
			wchar_t name[256] = {};
			DWORD letters = (DWORD)(sizeof(name) / sizeof(name[0]));
			if (RegEnumKeyExW(key, i, name, &letters, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
			{
				break;
			}

			// a byte-stream or scheme handler's subkey is a file ending or a scheme, holding the CLSIDs as its values
			HKEY sub = nullptr;

			if (form == ClsidForm::ValuesOfSubkeys && RegOpenKeyExW(key, name, 0, KEY_READ, &sub) == ERROR_SUCCESS)
			{
				for (DWORD j = 0;; j++)
				{
					wchar_t value[256] = {};
					DWORD valueLetters = (DWORD)(sizeof(value) / sizeof(value[0]));

					if (RegEnumValueW(sub, j, value, &valueLetters, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
					{
						break;
					}

					add(value);
				}

				RegCloseKey(sub);
				continue;
			}

			add(name);
		}

		RegCloseKey(key);
	}

	struct Judged
	{
		bool microsoft = false;
		std::wstring signer;
		std::string why;
	};

	// One module, judged once per check
	Judged Judge(std::wstring const &path, std::map<std::wstring, Judged> *seen)
	{
		std::wstring const key = Lower(path);
		auto const found = seen->find(key);

		if (found != seen->end())
		{
			return found->second;
		}

		Judged judged;

		judged.microsoft = host::MediaParsers_FileIsMicrosoft(path, &judged.signer, &judged.why);
		(*seen)[key] = judged;
		return judged;
	}

	std::string Line(std::wstring const &path, Judged const &judged)
	{
		return Narrow(path) + " (" + (judged.signer.empty() ? std::string("no signature") : "signed by " + Narrow(judged.signer)) + ")";
	}
}

bool host::MediaParsers_SignerIsMicrosoft(std::wstring const &signer)
{
	return signer == L"Microsoft Windows" || signer == L"Microsoft Windows Publisher" || signer == L"Microsoft Corporation";
}

bool host::MediaParsers_FileIsMicrosoft(std::wstring const &path, std::wstring *signer, std::string *why)
{
	signer->clear();

	if (!IsPlainDrivePath(path))
	{
		*why = Narrow(path) + " is not a file on a drive of this computer";
		return false;
	}

	HANDLE const file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		*why = Narrow(path) + " can't be opened to check its signature (error " + std::to_string((unsigned long)GetLastError()) + ")";
		return false;
	}

	// its own signature first; with none, Windows' catalogs
	WINTRUST_FILE_INFO info = {};

	info.cbStruct = sizeof(info);
	info.pcwszFilePath = path.c_str();
	info.hFile = file;

	WINTRUST_DATA data = {};

	data.dwUnionChoice = WTD_CHOICE_FILE;
	data.pFile = &info;

	LONG const own = Verify(&data, signer);
	bool const signedAtAll = own == ERROR_SUCCESS || (own == TRUST_E_NOSIGNATURE && CatalogSigned(path, file, signer));

	CloseHandle(file);

	if (!signedAtAll)
	{
		signer->clear();
		char code[16];

		snprintf(code, sizeof(code), "0x%08lx", (unsigned long)own);
		std::string const reason = own == TRUST_E_NOSIGNATURE ? std::string("not signed, in itself or in Windows' catalogs") :
			own == CERT_E_CHAINING ? std::string("its own signature doesn't lead to a root this PC trusts, ") + code : std::string("its signature doesn't hold, ") + code;
		std::string const package = StorePackageOf(path);

		// A file of a Store package: whose package, then the reason (Bishop 2, 11-22, of Dolby's decoder: "Dolby's decoder from a Store package, not Microsoft's own
		// code". Measured 11-22: its DLL is signed, but its chain doesn't lead to a trusted root, 0x800b010a)
		*why = Narrow(path) + (package.empty() ? " is " + reason : " is from the Store package " + package + ", not Microsoft's own code (" + reason + ")");
		return false;
	}

	if (!MediaParsers_SignerIsMicrosoft(*signer))
	{
		std::string const package = StorePackageOf(path);

		*why = Narrow(path) + (package.empty() ? "" : " is from the Store package " + package + ", and") + " is signed by " + Narrow(*signer) + ", not by Microsoft";
		return false;
	}

	why->clear();
	return true;
}

bool host::MediaParsers_ModuleOf(void const *object, std::wstring *path)
{
	path->clear();

	if (!object)
	{
		return false;
	}

	void const *const table = *static_cast<void const *const *>(object);
	MEMORY_BASIC_INFORMATION place = {};

	if (!VirtualQuery(table, &place, sizeof(place)) || place.Type != MEM_IMAGE || !place.AllocationBase)
	{
		return false;
	}

	std::wstring name(kMostPathLetters, L'\0');
	DWORD const letters = GetModuleFileNameW(static_cast<HMODULE>(place.AllocationBase), &name[0], (DWORD)name.size());

	if (letters == 0 || letters >= name.size())
	{
		return false;
	}

	name.resize(letters);
	*path = name;
	return true;
}

std::vector<std::wstring> host::MediaParsers_LoadedModules()
{
	std::vector<HMODULE> modules(256);
	DWORD needed = 0;

	for (;;)
	{
		if (!K32EnumProcessModulesEx(GetCurrentProcess(), modules.data(), (DWORD)(modules.size() * sizeof(HMODULE)), &needed, LIST_MODULES_ALL))
		{
			return std::vector<std::wstring>();
		}

		if (needed <= modules.size() * sizeof(HMODULE))
		{
			break;
		}

		modules.resize(needed / sizeof(HMODULE) + 16);
	}

	std::vector<std::wstring> paths;

	for (size_t i = 0; i < needed / sizeof(HMODULE); i++)
	{
		std::wstring name(kMostPathLetters, L'\0');
		DWORD const letters = GetModuleFileNameW(modules[i], &name[0], (DWORD)name.size());

		if (letters > 0 && letters < name.size())
		{
			name.resize(letters);
			paths.push_back(name);
		}
	}

	return paths;
}

std::vector<std::wstring> host::MediaParsers_RegisteredModules()
{
	std::set<std::wstring> clsids;

	for (HKEY const root : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER })
	{
		ClsidsUnder(root, L"SOFTWARE\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers", ClsidForm::ValuesOfSubkeys, &clsids);
		ClsidsUnder(root, L"SOFTWARE\\Microsoft\\Windows Media Foundation\\SchemeHandlers", ClsidForm::ValuesOfSubkeys, &clsids);
		ClsidsUnder(root, L"SOFTWARE\\Classes\\MediaFoundation\\Transforms", ClsidForm::Subkeys, &clsids);
		ClsidsUnder(root, L"SOFTWARE\\Classes\\DirectShow\\MediaObjects", ClsidForm::Subkeys, &clsids);
	}

	std::set<std::wstring> modules;

	for (std::wstring const &clsid : clsids)
	{
		std::wstring const module = ModuleOfClsid(clsid);

		if (!module.empty())
		{
			modules.insert(Lower(module));
		}
	}

	return std::vector<std::wstring>(modules.begin(), modules.end());
}

bool host::MediaParsers_CheckObject(void const *object, char const *what, std::string *why)
{
	std::wstring path;
	std::wstring signer;
	std::string judged;

	if (!MediaParsers_ModuleOf(object, &path))
	{
		*why = std::string("the video's ") + what + " is code that lives in no module (made while the program ran), so whose it is can't be told";
		return false;
	}

	if (!MediaParsers_FileIsMicrosoft(path, &signer, &judged))
	{
		*why = std::string("the video's ") + what + " is read by code that isn't Windows' own or Microsoft's: " + judged;
		return false;
	}

	return true;
}

bool host::MediaParsers_CheckReader(IMFSourceReader *reader, std::vector<DWORD> const &streams, std::vector<std::wstring> const &before, std::string *why,
	std::string *said)
{
	std::map<std::wstring, Judged> seen;

	if (said)
	{
		said->clear();
	}

	if (!reader)
	{
		*why = "no reader to check";
		return false;
	}

	// what is loaded now, taken FIRST: checking signatures below loads Windows' own crypto modules, which are no part of the reader's setup
	std::vector<std::wstring> const loadedNow = MediaParsers_LoadedModules();

	auto const note = [said](std::string const &line) {
		if (said)
		{
			*said += line + "\n";
		}
	};

	// 1. THE MEDIA SOURCE: the code that reads the container
	ComPtr<IMFMediaSource> source;

	if (FAILED(reader->GetServiceForStream((DWORD)MF_SOURCE_READER_MEDIASOURCE, GUID_NULL, IID_PPV_ARGS(&source))) || !source)
	{
		*why = "Media Foundation won't say which code reads the video's container";
		return false;
	}

	std::wstring path;

	if (!MediaParsers_ModuleOf(source.Get(), &path))
	{
		*why = "the code reading the video's container lives in no module, so whose it is can't be told";
		return false;
	}

	Judged judged = Judge(path, &seen);

	note("container: " + Line(path, judged));

	if (!judged.microsoft)
	{
		*why = "the video's container is read by code that isn't Windows' own or Microsoft's: " + judged.why;
		return false;
	}

	// 2. EVERY TRANSFORM on each stream: the decoder, and anything after it (the converter to the picture or sound asked for)
	ComPtr<IMFSourceReaderEx> chosen;

	if (FAILED(reader->QueryInterface(IID_PPV_ARGS(&chosen))) || !chosen)
	{
		*why = "Media Foundation won't say which decoders it chose (no IMFSourceReaderEx)";
		return false;
	}

	for (DWORD const stream : streams)
	{
		for (DWORD i = 0;; i++)
		{
			GUID category = GUID_NULL;
			ComPtr<IMFTransform> transform;
			HRESULT const hr = chosen->GetTransformForStream(stream, i, &category, &transform);

			if (hr == MF_E_INVALIDINDEX)
			{
				break;
			}

			if (FAILED(hr) || !transform)
			{
				*why = "Media Foundation won't say which decoder it chose for stream " + std::to_string((unsigned long)stream);
				return false;
			}

			if (!MediaParsers_ModuleOf(transform.Get(), &path))
			{
				*why = "a decoder Media Foundation chose lives in no module, so whose it is can't be told";
				return false;
			}

			judged = Judge(path, &seen);
			note("stream " + std::to_string((unsigned long)stream) + " transform " + std::to_string((unsigned long)i) + ": " + Line(path, judged));

			if (!judged.microsoft)
			{
				*why = "the video is decoded by code that isn't Windows' own or Microsoft's: " + judged.why;
				return false;
			}
		}
	}

	// 3. EVERY MODULE LOADED while the reader was set up (Bishop 2, 11-22): one that is a registered parser (a handler, a transform, a DirectX media object, which a
	// Microsoft wrapper may have loaded inside itself) must be Microsoft's; one that is no parser (a graphics driver the picture converter's device brought) is only
	// said. One from anywhere but a drive of this computer is refused whatever it is
	std::set<std::wstring> had;

	for (std::wstring const &one : before)
	{
		had.insert(Lower(one));
	}

	std::set<std::pair<uint64_t, uint64_t> > parsers;

	for (std::wstring const &registered : MediaParsers_RegisteredModules())
	{
		std::pair<uint64_t, uint64_t> id;

		if (FileId(registered, &id))
		{
			parsers.insert(id);
		}
	}

	for (std::wstring const &now : loadedNow)
	{
		if (had.count(Lower(now)))
		{
			continue;
		}

		if (!IsPlainDrivePath(now))
		{
			*why = "a module loaded while the video was opened is not on a drive of this computer: " + Narrow(now);
			return false;
		}

		std::pair<uint64_t, uint64_t> id;
		bool const isParser = FileId(now, &id) && parsers.count(id) > 0;

		judged = Judge(now, &seen);
		note(std::string(isParser ? "loaded, a parser: " : "loaded: ") + Line(now, judged));

		if (isParser && !judged.microsoft)
		{
			*why = "a parser loaded while the video was opened isn't Windows' own or Microsoft's: " + judged.why;
			return false;
		}
	}

	why->clear();
	return true;
}

namespace
{
	// A WAVE format tag's audio subtype (the base GUID Windows gives every one), for the formats mfapi.h shows only to Windows 10 targets
	GUID AudioTag(uint16_t tag)
	{
		return GUID{ tag, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
	}

	struct Format
	{
		char const *words;
		bool video;
		GUID subtype;
	};

	// THE LIST, BY MEASURE on the owner's PC, 2026-11-22 (AvP-docs tools/vasquez/mf/decoders-2026-11-22.txt; Bishop 2: "the list by measure, not by guess"): each
	// format here has a decoder whose code is Microsoft's, and nothing foreign loads when it is made and given its input type. The decoder measured, for each:
	//   H.264 msmpeg2vdec; HEVC HEVCDECODER_STORE; VP9 and VP8 msvp9dec_store; AV1 av1decodermft_store; MPEG-2 and MPEG-1 msmpeg2vdec_store (Windows' own
	//   msmpeg2vdec isn't made there, a licence error 0xC004F011, so its Store extension decodes them); MPEG-4 part 2 mp4sdecd; MP43 mp43decd; WMV and VC-1
	//   wmvdecod; MJPEG mfmjpegdec; DV mfdvdec; AAC and MPEG audio MSAudDecMFT; MP3 mp3dmod; WMA WMADMOD; FLAC MSFlacDecoder; ALAC MSAlacDecoder; Opus
	//   MSOpusDecoder; Dolby DolbyDecMFT. The first Dolby decoder Windows offers there is Dolby's decoder from a Store package, not Microsoft's own code: passed over.
	// The Store packages taken here (HEVC, VP9, AV1, MPEG-2) are Microsoft's, and carry Microsoft's own signature on each file (a Store package's files often carry
	// none of their own: Windows checks the package). NOT taken: Vorbis (its only decoder, Web Media Extensions' FFmpegAudioDecoder, loads FFmpeg: avcodec_ms and
	// the rest, "Microsoft 3rd Party Application Component"), DV25, DV50 and DVH1 (no decoder there), and anything not measured
	Format const kFormats[] = {
		{ "H.264", true, MFVideoFormat_H264 }, { "H.264", true, MFVideoFormat_H264_ES }, { "HEVC", true, MFVideoFormat_HEVC }, { "HEVC", true, MFVideoFormat_HEVC_ES },
		{ "VP9", true, MFVideoFormat_VP90 }, { "VP8", true, MFVideoFormat_VP80 }, { "AV1", true, MFVideoFormat_AV1 }, { "MPEG-2", true, MFVideoFormat_MPEG2 },
		{ "MPEG-1", true, MFVideoFormat_MPG1 }, { "MPEG-4 part 2", true, MFVideoFormat_MP4V }, { "MPEG-4 part 2", true, MFVideoFormat_MP4S },
		{ "MPEG-4 part 2", true, MFVideoFormat_M4S2 }, { "MPEG-4 v3", true, MFVideoFormat_MP43 }, { "WMV", true, MFVideoFormat_WMV1 }, { "WMV", true, MFVideoFormat_WMV2 },
		{ "WMV", true, MFVideoFormat_WMV3 }, { "VC-1", true, MFVideoFormat_WVC1 }, { "MJPEG", true, MFVideoFormat_MJPG }, { "DV", true, MFVideoFormat_DVSD },
		{ "DV", true, MFVideoFormat_DVSL },
		{ "AAC", false, MFAudioFormat_AAC }, { "AAC", false, MFAudioFormat_ADTS }, { "MP3", false, MFAudioFormat_MP3 }, { "MPEG audio", false, MFAudioFormat_MPEG },
		{ "WMA", false, MFAudioFormat_WMAudioV8 }, { "WMA", false, MFAudioFormat_WMAudioV9 }, { "WMA lossless", false, MFAudioFormat_WMAudio_Lossless },
		{ "FLAC", false, AudioTag(0xF1AC) }, { "ALAC", false, AudioTag(0x6C61) }, { "Opus", false, AudioTag(0x704F) }, { "Dolby AC-3", false, MFAudioFormat_Dolby_AC3 },
		{ "Dolby Digital Plus", false, MFAudioFormat_Dolby_DDPlus },
	};

	// Uncompressed pictures and sound: no decoder at all, only Windows' own converter (the picture) or resampler (the sound)
	Format const kUncompressed[] = {
		{ "RGB24", true, MFVideoFormat_RGB24 }, { "RGB32", true, MFVideoFormat_RGB32 }, { "YUY2", true, MFVideoFormat_YUY2 }, { "NV12", true, MFVideoFormat_NV12 },
		{ "I420", true, MFVideoFormat_I420 }, { "IYUV", true, MFVideoFormat_IYUV }, { "UYVY", true, MFVideoFormat_UYVY }, { "YV12", true, MFVideoFormat_YV12 },
		{ "PCM", false, MFAudioFormat_PCM }, { "float PCM", false, MFAudioFormat_Float },
	};

	// Formats not taken that the words name (so the user hears which, not a GUID)
	Format const kNamedOnly[] = {
		{ "Vorbis", false, GUID{ 0x8D2FD10B, 0x5841, 0x4A6B, { 0x89, 0x05, 0x58, 0x8F, 0xEC, 0x1A, 0xDE, 0xD9 } } },
	};

	Format const *FindFormat(GUID const &subtype, Format const *list, size_t count)
	{
		for (size_t i = 0; i < count; i++)
		{
			if (list[i].subtype == subtype)
			{
				return &list[i];
			}
		}

		return nullptr;
	}

	// The source's answer, waited for on this thread (BeginCreateObject's callback)
	class Created : public IMFAsyncCallback
	{
	public:
		explicit Created(IMFByteStreamHandler *made) : handler(made) { done = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
		virtual ~Created() { if (done) CloseHandle(done); }

		STDMETHODIMP QueryInterface(REFIID id, void **out) override
		{
			if (!out)
			{
				return E_POINTER;
			}

			if (id == __uuidof(IUnknown) || id == __uuidof(IMFAsyncCallback))
			{
				*out = static_cast<IMFAsyncCallback *>(this);
				AddRef();
				return S_OK;
			}

			*out = nullptr;
			return E_NOINTERFACE;
		}

		STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&references); }

		STDMETHODIMP_(ULONG) Release() override
		{
			LONG const left = InterlockedDecrement(&references);

			if (left == 0)
			{
				delete this;
			}

			return (ULONG)left;
		}

		STDMETHODIMP GetParameters(DWORD *, DWORD *) override { return E_NOTIMPL; }

		STDMETHODIMP Invoke(IMFAsyncResult *result) override
		{
			MF_OBJECT_TYPE type = MF_OBJECT_INVALID;

			answer = handler->EndCreateObject(result, &type, &object);
			SetEvent(done);
			return S_OK;
		}

		HANDLE done = nullptr;
		HRESULT answer = E_PENDING;
		ComPtr<IUnknown> object;

	private:
		LONG references = 1;
		ComPtr<IMFByteStreamHandler> handler;
	};

	// The byte-stream handlers registered for a content type, the user's own registration first as Windows reads them
	std::vector<std::wstring> HandlerClsids(wchar_t const *contentType)
	{
		std::vector<std::wstring> clsids;

		for (HKEY const root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
		{
			HKEY key = nullptr;
			std::wstring const path = std::wstring(L"SOFTWARE\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers\\") + contentType;

			if (RegOpenKeyExW(root, path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
			{
				continue;
			}

			for (DWORD i = 0;; i++)
			{
				wchar_t name[256] = {};
				DWORD letters = (DWORD)(sizeof(name) / sizeof(name[0]));

				if (RegEnumValueW(key, i, name, &letters, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
				{
					break;
				}

				if (letters == 38 && name[0] == L'{' && name[37] == L'}')
				{
					clsids.push_back(name);
				}
			}

			RegCloseKey(key);
		}

		return clsids;
	}

	std::string Hex(HRESULT hr)
	{
		char code[16];

		snprintf(code, sizeof(code), "0x%08lx", (unsigned long)hr);
		return code;
	}
}

bool host::MediaParsers_FormatTaken(GUID const &major, GUID const &subtype)
{
	if (major != MFMediaType_Video && major != MFMediaType_Audio)
	{
		return false;
	}

	Format const *const format = FindFormat(subtype, kFormats, sizeof(kFormats) / sizeof(kFormats[0]));

	Format const *const plain = FindFormat(subtype, kUncompressed, sizeof(kUncompressed) / sizeof(kUncompressed[0]));

	return (format && format->video == (major == MFMediaType_Video)) || (plain && plain->video == (major == MFMediaType_Video));
}

bool host::MediaParsers_NeedsNoDecoder(GUID const &subtype)
{
	return FindFormat(subtype, kUncompressed, sizeof(kUncompressed) / sizeof(kUncompressed[0])) != nullptr;
}

std::string host::MediaParsers_FormatWords(GUID const &subtype)
{
	Format const *format = FindFormat(subtype, kFormats, sizeof(kFormats) / sizeof(kFormats[0]));

	format = format ? format : FindFormat(subtype, kUncompressed, sizeof(kUncompressed) / sizeof(kUncompressed[0]));
	format = format ? format : FindFormat(subtype, kNamedOnly, sizeof(kNamedOnly) / sizeof(kNamedOnly[0]));

	if (format)
	{
		return format->words;
	}

	// the base GUID every FOURCC and WAVE tag subtype has: say the FOURCC's letters, or the tag's number
	GUID const base = AudioTag(0);

	if (subtype.Data2 == base.Data2 && subtype.Data3 == base.Data3 && memcmp(subtype.Data4, base.Data4, 8) == 0)
	{
		uint32_t const value = subtype.Data1;
		char letters[5] = { (char)(value & 0xff), (char)((value >> 8) & 0xff), (char)((value >> 16) & 0xff), (char)((value >> 24) & 0xff), 0 };
		bool printable = value > 0xffff;

		for (int i = 0; i < 4 && printable; i++)
		{
			printable = letters[i] >= 0x20 && letters[i] < 0x7f;
		}

		if (printable)
		{
			return std::string("\"") + letters + "\"";
		}

		char text[32];

		snprintf(text, sizeof(text), "format tag 0x%04lx", (unsigned long)value);
		return text;
	}

	wchar_t text[64] = {};

	StringFromGUID2(subtype, text, 64);
	return Narrow(text);
}

HRESULT host::MediaParsers_CreateSource(IMFByteStream *stream, wchar_t const *contentType, IMFMediaSource **source, std::string *why)
{
	*source = nullptr;

	if (!stream || !contentType || !*contentType)
	{
		*why = "nothing to read";
		return E_INVALIDARG;
	}

	std::vector<std::wstring> const clsids = HandlerClsids(contentType);
	std::string passedOver;

	for (std::wstring const &clsid : clsids)
	{
		// its registered module, judged on its file before anything is made
		std::wstring const module = ModuleOfClsid(clsid);
		std::wstring signer;
		std::string judged;

		if (module.empty() || !MediaParsers_FileIsMicrosoft(module, &signer, &judged))
		{
			passedOver += (passedOver.empty() ? "" : "; ") + (module.empty() ? Narrow(clsid) + " has no module" : judged);
			continue;
		}

		CLSID id = GUID_NULL;
		ComPtr<IMFByteStreamHandler> handler;

		judged.clear();

		if (FAILED(CLSIDFromString(clsid.c_str(), &id)) || FAILED(CoCreateInstance(id, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&handler))) ||
			!MediaParsers_CheckObject(handler.Get(), "container's reader", &judged))
		{
			passedOver += (passedOver.empty() ? "" : "; ") + (judged.empty() ? Narrow(clsid) + " wasn't made" : judged);
			continue;
		}

		// the source, from THIS handler only, waited for here (a minute at most)
		Created *const created = new Created(handler.Get());
		ComPtr<IUnknown> cancel;
		HRESULT hr = created->done ? handler->BeginCreateObject(stream, nullptr, MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ, nullptr, &cancel, created, nullptr) : E_OUTOFMEMORY;

		if (SUCCEEDED(hr))
		{
			if (WaitForSingleObject(created->done, 60000) != WAIT_OBJECT_0)
			{
				handler->CancelObjectCreation(cancel.Get());
				hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
			}
			else
			{
				hr = created->answer;
			}
		}

		ComPtr<IMFMediaSource> made;
		bool const good = SUCCEEDED(hr) && created->object && SUCCEEDED(created->object.As(&made)) && made;

		created->Release();

		if (!good)
		{
			*why = "Windows' own reader of this kind of video can't read it (" + Hex(FAILED(hr) ? hr : E_FAIL) + ")";
			return FAILED(hr) ? hr : E_FAIL;
		}

		if (!MediaParsers_CheckObject(made.Get(), "container", why))
		{
			made->Shutdown();
			return E_ACCESSDENIED;
		}

		*source = made.Detach();
		why->clear();
		return S_OK;
	}

	*why = clsids.empty() ? "Windows has no reader for " + Narrow(contentType) + " videos" :
		"no reader of " + Narrow(contentType) + " videos on this PC is Windows' own or Microsoft's (" + passedOver + ")";
	return E_ACCESSDENIED;
}

namespace
{
	// A decoder's friendly name as Media Foundation lists it ("" when it has none)
	std::wstring FriendlyName(IMFActivate *one)
	{
		WCHAR *name = nullptr;
		UINT32 letters = 0;
		std::wstring friendly;

		if (SUCCEEDED(one->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &name, &letters)) && name)
		{
			friendly = name;
		}

		CoTaskMemFree(name);
		return friendly;
	}
}

bool host::MediaParsers_ChooseDecoder(IMFMediaType *native, IMFTransform **decoder, std::string *why, std::string *said, MediaParsersDecoderName *chosen)
{
	*decoder = nullptr;

	if (chosen)
	{
		*chosen = MediaParsersDecoderName();
	}

	GUID major = GUID_NULL, subtype = GUID_NULL;

	if (!native || FAILED(native->GetGUID(MF_MT_MAJOR_TYPE, &major)) || FAILED(native->GetGUID(MF_MT_SUBTYPE, &subtype)))
	{
		*why = "the stream says no format";
		return false;
	}

	if (!MediaParsers_FormatTaken(major, subtype) || MediaParsers_NeedsNoDecoder(subtype))
	{
		*why = MediaParsers_NeedsNoDecoder(subtype) ? "it needs no decoder" : MediaParsers_FormatWords(subtype) + " is not a format the editor takes";
		return false;
	}

	MFT_REGISTER_TYPE_INFO input = { major, subtype };
	IMFActivate **offered = nullptr;
	UINT32 count = 0;
	HRESULT const listed = MFTEnumEx(major == MFMediaType_Video ? MFT_CATEGORY_VIDEO_DECODER : MFT_CATEGORY_AUDIO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
		&input, nullptr, &offered, &count);
	std::string passedOver;
	std::map<std::wstring, Judged> seen;
	bool kept = false;

	for (UINT32 i = 0; SUCCEEDED(listed) && i < count; i++)
	{
		IMFActivate *const one = offered[i];

		if (kept)
		{
			one->Release();
			continue;
		}

		std::wstring const friendly = FriendlyName(one);
		GUID clsid = GUID_NULL;

		one->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &clsid);

		std::string const shown = friendly.empty() ? std::string("a decoder") : Narrow(friendly);

		// a registered module is judged on its file BEFORE the decoder is made; a Store decoder has none to look up (no CLSID), and is judged once made
		if (clsid != GUID_NULL)
		{
			wchar_t text[64] = {};

			StringFromGUID2(clsid, text, 64);

			std::wstring const module = ModuleOfClsid(text);

			if (!module.empty())
			{
				Judged const judged = Judge(module, &seen);

				if (!judged.microsoft)
				{
					passedOver += (passedOver.empty() ? "" : "; ") + shown + ": " + judged.why;

					if (said)
					{
						*said += "passed over, never made: " + shown + " " + Line(module, judged) + "\n";
					}

					one->Release();
					continue;
				}
			}
		}

		std::vector<std::wstring> const before = MediaParsers_LoadedModules();
		ComPtr<IMFTransform> made;

		if (FAILED(one->ActivateObject(IID_PPV_ARGS(&made))) || !made)
		{
			passedOver += (passedOver.empty() ? "" : "; ") + shown + " couldn't be made";
			one->Release();
			continue;
		}

		// its own code, then everything its making loaded; only then its input type (the codec's headers are read there), and what that loaded
		std::string foreign;
		std::wstring path;

		if (!MediaParsers_ModuleOf(made.Get(), &path))
		{
			foreign = shown + " lives in no module";
		}
		else if (!Judge(path, &seen).microsoft)
		{
			foreign = shown + ": " + Judge(path, &seen).why;
		}

		std::set<std::wstring> had;

		for (std::wstring const &m : before)
		{
			had.insert(Lower(m));
		}

		auto const judgeLoaded = [&]() {
			for (std::wstring const &now : MediaParsers_LoadedModules())
			{
				if (!foreign.empty() || had.count(Lower(now)))
				{
					continue;
				}

				had.insert(Lower(now));

				Judged const judged = Judge(now, &seen);

				if (said)
				{
					*said += "  loaded by " + shown + ": " + Line(now, judged) + "\n";
				}

				if (!judged.microsoft)
				{
					foreign = shown + " loaded " + judged.why;
				}
			}
		};

		judgeLoaded();

		HRESULT const typed = foreign.empty() ? made->SetInputType(0, native, 0) : E_ACCESSDENIED;

		judgeLoaded();

		if (!foreign.empty() || FAILED(typed))
		{
			passedOver += (passedOver.empty() ? "" : "; ") + (foreign.empty() ? shown + " won't take this stream (" + Hex(typed) + ")" : foreign);

			if (said)
			{
				*said += "shut down: " + shown + (foreign.empty() ? " (it won't take this stream)" : " (" + foreign + ")") + "\n";
			}

			one->ShutdownObject();
			one->Release();
			continue;
		}

		if (said)
		{
			*said += "chosen: " + shown + " " + Line(path, Judge(path, &seen)) + "\n";
		}

		*decoder = made.Detach();
		kept = true;

		// named for the factory, which makes only this one (its class; a Store decoder's name)
		if (chosen)
		{
			chosen->clsid = clsid;
			chosen->friendly = friendly;
			chosen->module = path;
		}

		one->Release();
	}

	CoTaskMemFree(offered);

	if (!kept)
	{
		*why = count == 0 ? "Windows has no decoder for its " + MediaParsers_FormatWords(subtype) :
			"no decoder of its " + MediaParsers_FormatWords(subtype) + " on this PC is Windows' own or Microsoft's (" + passedOver + ")";
		return false;
	}

	why->clear();
	return true;
}
