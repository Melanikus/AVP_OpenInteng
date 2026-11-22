// host_media_parsers_test: WHOSE CODE READS A VIDEO (src/host/media_parsers.h, its use in video_import.cpp). Written 2026-11-22 by Melanikus (Vasquez) for
// AVP_OpenInteng, on Bishop 2's order (Vasquez's Video mode review (a)). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Part 1 asks media_parsers itself: the signer names, files (Windows' own, another publisher's, a changed copy, this test), the module an object's code is in,
// the list of formats, which offered decoder the reader's factory makes, and the check refusing a decoder of this test's own that the reader made. Part 2 reads
// videos through video_import, as Video mode and mp4-to-smk do, with three things planted for THIS PROCESS ONLY (nothing is written to the registry): a decoder
// of the test's own for H.264, a byte-stream handler of the test's own for video/mp4, and AVI files whose picture or sound is in a format no decoder of Windows'
// own reads.
// Built with RED_FIRST_OLD_IMPORT against the video_import.cpp before media_parsers (5a1f8da8), part 2 alone is built, and each of its cases fails there.
#include <stdio.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include "avi_test_file.h"
#include "fake_decoder.h"
#include "video_import.h"
#include "video_test_clip.h"
#ifndef RED_FIRST_OLD_IMPORT
#include "media_parsers.h"
#endif

using Microsoft::WRL::ComPtr;

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

static std::wstring ThisProgram()
{
	wchar_t path[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	return path;
}

static std::string ThisProgramName()
{
	std::wstring const path = ThisProgram();
	std::string name;
	for (wchar_t c : path.substr(path.find_last_of(L'\\') + 1)) name.push_back((char)c);
	return name;
}

// ---- A BYTE-STREAM HANDLER OF THE TEST'S OWN for video/mp4: it hands every call to Windows' own MP4 handler, and counts how often it was asked
static LONG plantedAsked = 0;
static CLSID const kMpeg4Handler = { 0x271C3902, 0x6095, 0x4c45, { 0xA2, 0x2F, 0x20, 0x09, 0x18, 0x16, 0xEE, 0x9E } };

class PlantedHandler : public IMFByteStreamHandler
{
public:
	PlantedHandler() { CoCreateInstance(kMpeg4Handler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&inner)); }
	virtual ~PlantedHandler() {}
	STDMETHODIMP QueryInterface(REFIID id, void **out) override
	{
		if (!out) return E_POINTER;
		if (id == __uuidof(IUnknown) || id == __uuidof(IMFByteStreamHandler)) { *out = static_cast<IMFByteStreamHandler *>(this); AddRef(); return S_OK; }
		*out = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&references); }
	STDMETHODIMP_(ULONG) Release() override { LONG const left = InterlockedDecrement(&references); if (left == 0) delete this; return (ULONG)left; }
	STDMETHODIMP BeginCreateObject(IMFByteStream *stream, LPCWSTR url, DWORD flags, IPropertyStore *props, IUnknown **cancel, IMFAsyncCallback *callback, IUnknown *state) override
	{
		InterlockedIncrement(&plantedAsked);
		return inner ? inner->BeginCreateObject(stream, url, flags, props, cancel, callback, state) : E_FAIL;
	}
	STDMETHODIMP EndCreateObject(IMFAsyncResult *result, MF_OBJECT_TYPE *type, IUnknown **object) override { return inner ? inner->EndCreateObject(result, type, object) : E_FAIL; }
	STDMETHODIMP CancelObjectCreation(IUnknown *cancel) override { return inner ? inner->CancelObjectCreation(cancel) : E_FAIL; }
	STDMETHODIMP GetMaxNumberOfBytesRequiredForResolution(QWORD *bytes) override { return inner ? inner->GetMaxNumberOfBytesRequiredForResolution(bytes) : E_FAIL; }
private:
	LONG references = 1;
	ComPtr<IMFByteStreamHandler> inner;
};

// Its activation object (MFRegisterLocalByteStreamHandler takes one): its attributes are Media Foundation's own, every call handed on
class PlantedActivate : public IMFActivate
{
public:
	PlantedActivate() { MFCreateAttributes(&attributes, 1); }
	STDMETHODIMP QueryInterface(REFIID id, void **out) override
	{
		if (!out) return E_POINTER;
		if (id == __uuidof(IUnknown) || id == __uuidof(IMFAttributes) || id == __uuidof(IMFActivate)) { *out = static_cast<IMFActivate *>(this); AddRef(); return S_OK; }
		*out = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() override { return 2; }   // (one, static, for the test's life)
	STDMETHODIMP_(ULONG) Release() override { return 1; }
	STDMETHODIMP ActivateObject(REFIID id, void **out) override
	{
		// (made owned, then handed to its COM count, as fake_decoder.h's factory does)
		std::unique_ptr<PlantedHandler> made = std::make_unique<PlantedHandler>();
		HRESULT const hr = made->QueryInterface(id, out);
		made.release()->Release();
		return hr;
	}
	STDMETHODIMP ShutdownObject() override { return S_OK; }
	STDMETHODIMP DetachObject() override { return S_OK; }
	STDMETHODIMP GetItem(REFGUID k, PROPVARIANT *v) override { return attributes->GetItem(k, v); }
	STDMETHODIMP GetItemType(REFGUID k, MF_ATTRIBUTE_TYPE *t) override { return attributes->GetItemType(k, t); }
	STDMETHODIMP CompareItem(REFGUID k, REFPROPVARIANT v, BOOL *r) override { return attributes->CompareItem(k, v, r); }
	STDMETHODIMP Compare(IMFAttributes *a, MF_ATTRIBUTES_MATCH_TYPE m, BOOL *r) override { return attributes->Compare(a, m, r); }
	STDMETHODIMP GetUINT32(REFGUID k, UINT32 *v) override { return attributes->GetUINT32(k, v); }
	STDMETHODIMP GetUINT64(REFGUID k, UINT64 *v) override { return attributes->GetUINT64(k, v); }
	STDMETHODIMP GetDouble(REFGUID k, double *v) override { return attributes->GetDouble(k, v); }
	STDMETHODIMP GetGUID(REFGUID k, GUID *v) override { return attributes->GetGUID(k, v); }
	STDMETHODIMP GetStringLength(REFGUID k, UINT32 *n) override { return attributes->GetStringLength(k, n); }
	STDMETHODIMP GetString(REFGUID k, LPWSTR v, UINT32 n, UINT32 *l) override { return attributes->GetString(k, v, n, l); }
	STDMETHODIMP GetAllocatedString(REFGUID k, LPWSTR *v, UINT32 *l) override { return attributes->GetAllocatedString(k, v, l); }
	STDMETHODIMP GetBlobSize(REFGUID k, UINT32 *n) override { return attributes->GetBlobSize(k, n); }
	STDMETHODIMP GetBlob(REFGUID k, UINT8 *b, UINT32 n, UINT32 *l) override { return attributes->GetBlob(k, b, n, l); }
	STDMETHODIMP GetAllocatedBlob(REFGUID k, UINT8 **b, UINT32 *n) override { return attributes->GetAllocatedBlob(k, b, n); }
	STDMETHODIMP GetUnknown(REFGUID k, REFIID i, LPVOID *p) override { return attributes->GetUnknown(k, i, p); }
	STDMETHODIMP SetItem(REFGUID k, REFPROPVARIANT v) override { return attributes->SetItem(k, v); }
	STDMETHODIMP DeleteItem(REFGUID k) override { return attributes->DeleteItem(k); }
	STDMETHODIMP DeleteAllItems() override { return attributes->DeleteAllItems(); }
	STDMETHODIMP SetUINT32(REFGUID k, UINT32 v) override { return attributes->SetUINT32(k, v); }
	STDMETHODIMP SetUINT64(REFGUID k, UINT64 v) override { return attributes->SetUINT64(k, v); }
	STDMETHODIMP SetDouble(REFGUID k, double v) override { return attributes->SetDouble(k, v); }
	STDMETHODIMP SetGUID(REFGUID k, REFGUID v) override { return attributes->SetGUID(k, v); }
	STDMETHODIMP SetString(REFGUID k, LPCWSTR v) override { return attributes->SetString(k, v); }
	STDMETHODIMP SetBlob(REFGUID k, UINT8 const *b, UINT32 n) override { return attributes->SetBlob(k, b, n); }
	STDMETHODIMP SetUnknown(REFGUID k, IUnknown *p) override { return attributes->SetUnknown(k, p); }
	STDMETHODIMP LockStore() override { return attributes->LockStore(); }
	STDMETHODIMP UnlockStore() override { return attributes->UnlockStore(); }
	STDMETHODIMP GetCount(UINT32 *n) override { return attributes->GetCount(n); }
	STDMETHODIMP GetItemByIndex(UINT32 i, GUID *k, PROPVARIANT *v) override { return attributes->GetItemByIndex(i, k, v); }
	STDMETHODIMP CopyAllItems(IMFAttributes *d) override { return attributes->CopyAllItems(d); }
private:
	ComPtr<IMFAttributes> attributes;
};

// A decoder that refuses every input type (registered first, it stands for "our decoder refusing", Bishop 2's one window left)
class RefusingDecoder : public FakeDecoder
{
public:
	STDMETHODIMP SetInputType(DWORD, IMFMediaType *, DWORD) override { return MF_E_INVALIDMEDIATYPE; }
};

class RefusingFactory : public FakeDecoderFactory
{
public:
	STDMETHODIMP CreateInstance(IUnknown *outer, REFIID id, void **out) override
	{
		if (outer) return CLASS_E_NOAGGREGATION;
		std::unique_ptr<RefusingDecoder> made = std::make_unique<RefusingDecoder>();   // (owned, then handed to its COM count)
		HRESULT const hr = made->QueryInterface(id, out);
		made.release()->Release();
		return hr;
	}
};

static std::vector<uint8_t> ReadAll(std::wstring const &path)
{
	std::vector<uint8_t> bytes;
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE) return bytes;
	LARGE_INTEGER size = {};
	GetFileSizeEx(file, &size);
	bytes.resize((size_t)size.QuadPart);
	DWORD got = 0;
	if (!bytes.empty()) ReadFile(file, bytes.data(), (DWORD)bytes.size(), &got, nullptr);
	CloseHandle(file);
	bytes.resize(got);
	return bytes;
}

// A refusal of code that isn't Windows' own, as video_import words it (the same words in the red build's code would mean the check was there)
static bool RefusedAsNotWindows(host::VideoImportResult result, std::string const &why)
{
	return result != host::VideoImportResult::Done && why.find("Windows' own") != std::string::npos;
}

int main()
{
	bool const com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	bool const on = com && SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));

	Check(on, "COM and Media Foundation start");

	// the H.264 + AAC clip every case below reads (made by Windows' own encoders, in the temp folder)
	wchar_t temp[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, temp);
	std::wstring const mp4 = std::wstring(temp) + L"host_media_parsers_test_" + std::to_wstring(GetCurrentProcessId()) + L".mp4";
	std::string made;
	bool const haveMp4 = on && TestClip_Make(mp4, MFVideoFormat_H264, 160, 120, 15, 15, true, [](uint32_t i, uint8_t *pixels) { TestClip_Fill(pixels, 160, 0, 120, (uint8_t)(i * 16), 64, 200); }, &made);
	std::vector<uint8_t> const mp4Bytes = haveMp4 ? ReadAll(mp4) : std::vector<uint8_t>();
	DeleteFileW(mp4.c_str());
	Check(haveMp4 && mp4Bytes.size() > 1000, "an H.264 + AAC clip is made (" + std::to_string((unsigned long long)mp4Bytes.size()) + " bytes" + (made.empty() ? "" : ", " + made) + ")");

#ifndef RED_FIRST_OLD_IMPORT
	printf("Part 1: media_parsers\n");
	{
		Check(host::MediaParsers_SignerIsMicrosoft(L"Microsoft Windows") && host::MediaParsers_SignerIsMicrosoft(L"Microsoft Windows Publisher") &&
			host::MediaParsers_SignerIsMicrosoft(L"Microsoft Corporation"), "the three signers of Microsoft's own code are taken");
		Check(!host::MediaParsers_SignerIsMicrosoft(L"Microsoft Windows Hardware Compatibility Publisher") && !host::MediaParsers_SignerIsMicrosoft(L"Microsoft 3rd Party Application Component") &&
			!host::MediaParsers_SignerIsMicrosoft(L"microsoft windows") && !host::MediaParsers_SignerIsMicrosoft(L"") && !host::MediaParsers_SignerIsMicrosoft(L"Johannes Schindelin"),
			"Microsoft's stamp over another maker's code (a hardware driver, FFmpeg), other capitals, none, and another publisher are not");

		wchar_t system[MAX_PATH] = {};
		GetSystemDirectoryW(system, MAX_PATH);
		std::wstring const mfplat = std::wstring(system) + L"\\mfplat.dll";
		std::wstring signer;
		std::string why;
		Check(host::MediaParsers_FileIsMicrosoft(mfplat, &signer, &why) && signer == L"Microsoft Windows", "Windows' own mfplat.dll: Microsoft's (signed in Windows' catalogs)");
		// (each answer taken before its words are made: the words of a Check are built in no set order with its test)
		bool const self = host::MediaParsers_FileIsMicrosoft(ThisProgram(), &signer, &why);
		Check(!self && why.find("not signed") != std::string::npos, "this test program: not signed, not taken (" + why + ")");

		std::wstring const changed = std::wstring(temp) + L"host_media_parsers_test_" + std::to_wstring(GetCurrentProcessId()) + L"_changed.dll";
		std::vector<uint8_t> copy = ReadAll(mfplat);
		if (copy.size() > 4096) copy[copy.size() / 2] ^= 0xFF;
		HANDLE const out = CreateFileW(changed.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
		DWORD wrote = 0;
		bool const wroteCopy = out != INVALID_HANDLE_VALUE && WriteFile(out, copy.data(), (DWORD)copy.size(), &wrote, nullptr) && wrote == copy.size();
		if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
		bool const changedTaken = wroteCopy && host::MediaParsers_FileIsMicrosoft(changed, &signer, &why);
		Check(wroteCopy && !changedTaken, "a copy of mfplat.dll with one byte changed: not taken (" + why + ")");
		DeleteFileW(changed.c_str());

		std::wstring const git = L"C:\\Program Files\\Git\\cmd\\git.exe";
		bool const haveGit = GetFileAttributesW(git.c_str()) != INVALID_FILE_ATTRIBUTES;
		bool const gitTaken = haveGit && host::MediaParsers_FileIsMicrosoft(git, &signer, &why);
		Check(haveGit && !gitTaken && !signer.empty() && signer.find(L"Microsoft") == std::wstring::npos, "Git's git.exe, validly signed by another publisher: not taken (" + why + ")");
		Check(!host::MediaParsers_FileIsMicrosoft(L"\\\\127.0.0.1\\x\\y.dll", &signer, &why) && why.find("not a file on a drive") != std::string::npos,
			"a module path on another computer is refused from its letters, never opened");

		ComPtr<IMFByteStreamHandler> handler;
		std::wstring module;
		Check(SUCCEEDED(CoCreateInstance(kMpeg4Handler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&handler))) && host::MediaParsers_ModuleOf(handler.Get(), &module) &&
			module.size() > 15 && _wcsicmp(module.c_str() + module.size() - 15, L"mfmp4srcsnk.dll") == 0, "Windows' MP4 handler's code is in mfmp4srcsnk.dll");
		FakeDecoder *const own = new FakeDecoder();
		Check(host::MediaParsers_ModuleOf(own, &module) && _wcsicmp(module.c_str(), ThisProgram().c_str()) == 0 && !host::MediaParsers_CheckObject(own, "test's decoder", &why) &&
			why.find(ThisProgramName()) != std::string::npos, "an object of this test's own: its code is this program, and the check refuses it by name");
		own->Release();

		Check(host::MediaParsers_FormatTaken(MFMediaType_Video, MFVideoFormat_H264) && host::MediaParsers_FormatTaken(MFMediaType_Audio, MFAudioFormat_AAC) &&
			host::MediaParsers_FormatTaken(MFMediaType_Video, MFVideoFormat_RGB24) && host::MediaParsers_NeedsNoDecoder(MFVideoFormat_RGB24), "H.264, AAC and uncompressed RGB are taken");
		GUID const vorbis = { 0x8D2FD10B, 0x5841, 0x4A6B, { 0x89, 0x05, 0x58, 0x8F, 0xEC, 0x1A, 0xDE, 0xD9 } };
		GUID const xxxx = { AviTest_Fourcc("XXXX"), 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
		Check(!host::MediaParsers_FormatTaken(MFMediaType_Audio, vorbis) && host::MediaParsers_FormatWords(vorbis) == "Vorbis" && !host::MediaParsers_FormatTaken(MFMediaType_Video, xxxx) &&
			host::MediaParsers_FormatWords(xxxx) == "\"XXXX\"" && !host::MediaParsers_FormatTaken(MFMediaType_Audio, MFVideoFormat_H264),
			"Vorbis, a FOURCC no decoder knows, and a picture's format given as a sound are not, each named");

		// DIETRICH'S M1 (11-22): the factory the reader makes its decoder with makes the very decoder ChooseDecoder proved, not the first of its module. Say one
		// Microsoft module holds A then B for the format, A refuses this stream and B is chosen: A is never made in B's place, nor a codec pack's X listed
		// between them. A Store package's decoder has no class: it is known by its friendly name (its module only once made, and checked then). Names only:
		// nothing is opened or made here
		{
			GUID const a = { 0x11111111, 0x1111, 0x1111, { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 } };
			GUID const b = { 0x22222222, 0x2222, 0x2222, { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 } };
			GUID const x = { 0x33333333, 0x3333, 0x3333, { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33 } };
			std::wstring const windows = std::wstring(system) + L"\\decoders.dll";
			host::MediaParsersDecoderName const offeredA = { a, L"Decoder A", windows }, offeredB = { b, L"Decoder B", windows };
			host::MediaParsersDecoderName const offeredX = { x, L"A codec pack's decoder", L"C:\\Program Files\\Pack\\x.dll" }, chosenB = { b, L"Decoder B", windows };
			bool const aMade = host::MediaParsers_OfferIsChosen(offeredA, chosenB);
			Check(!aMade, "A, of the chosen B's own module, which refused this stream: never made in B's place");
			bool const xMade = host::MediaParsers_OfferIsChosen(offeredX, chosenB), bMade = host::MediaParsers_OfferIsChosen(offeredB, chosenB);
			Check(!xMade && bMade, "a codec pack's X listed between them: never made; B, the chosen one: made");
			host::MediaParsersDecoderName const storeA = { GUID_NULL, L"Store decoder A", L"" }, storeB = { GUID_NULL, L"Store decoder B", L"" };
			host::MediaParsersDecoderName const chosenStoreB = { GUID_NULL, L"Store decoder B", L"C:\\Program Files\\WindowsApps\\Pack_1.0.0.0_x64__0000000000000\\b.dll" };
			bool const storeAMade = host::MediaParsers_OfferIsChosen(storeA, chosenStoreB), storeBMade = host::MediaParsers_OfferIsChosen(storeB, chosenStoreB);
			bool const registeredMade = host::MediaParsers_OfferIsChosen(offeredB, chosenStoreB);
			Check(!storeAMade && storeBMade && !registeredMade, "a Store package's decoder (no class) chosen: only it, by its name; another Store one and a registered one never");
		}

		// BISHOP 2'S ONE WINDOW LEFT: "our" decoder refuses its type, the reader goes on to the next (a foreign one of this test's own), and the check refuses
		// that one by name before a sample (the reader made here the editor's way, the source from Windows' own handler)
		static RefusingFactory refusing;
		static FakeDecoderFactory foreign;
		FakeDecoder_Counts() = FakeDecoderCounts();
		bool const registered = SUCCEEDED(MFTRegisterLocal(&refusing, MFT_CATEGORY_VIDEO_DECODER, L"refuses", MFT_ENUM_FLAG_SYNCMFT, 0, nullptr, 0, nullptr)) && SUCCEEDED(FakeDecoder_Register(&foreign));
		std::vector<std::wstring> const before = host::MediaParsers_LoadedModules();
		HGLOBAL const global = GlobalAlloc(GMEM_MOVEABLE, mp4Bytes.size());
		if (global && !mp4Bytes.empty()) { memcpy(GlobalLock(global), mp4Bytes.data(), mp4Bytes.size()); GlobalUnlock(global); }
		ComPtr<IStream> stream;
		ComPtr<IMFByteStream> bytes;
		ComPtr<IMFAttributes> streamAttributes, readerAttributes;
		ComPtr<IMFMediaSource> source;
		ComPtr<IMFSourceReader> reader;
		HRESULT hr = global ? CreateStreamOnHGlobal(global, TRUE, &stream) : E_OUTOFMEMORY;
		if (SUCCEEDED(hr)) hr = MFCreateMFByteStreamOnStream(stream.Get(), &bytes);
		if (SUCCEEDED(hr) && SUCCEEDED(bytes.As(&streamAttributes))) streamAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, L"video/mp4");
		if (SUCCEEDED(hr)) hr = host::MediaParsers_CreateSource(bytes.Get(), L"video/mp4", &source, &why);
		if (SUCCEEDED(hr)) hr = MFCreateAttributes(&readerAttributes, 1);
		if (SUCCEEDED(hr)) { readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE); hr = MFCreateSourceReaderFromMediaSource(source.Get(), readerAttributes.Get(), &reader); }
		ComPtr<IMFMediaType> rgb;
		if (SUCCEEDED(hr)) hr = MFCreateMediaType(&rgb);
		if (SUCCEEDED(hr)) { rgb->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); rgb->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32); hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, rgb.Get()); }
		DWORD picture = 0;
		for (DWORD s = 0; s < 8; s++) { ComPtr<IMFMediaType> t; GUID m = GUID_NULL; if (reader && SUCCEEDED(reader->GetNativeMediaType(s, 0, &t)) && SUCCEEDED(t->GetGUID(MF_MT_MAJOR_TYPE, &m)) && m == MFMediaType_Video) picture = s; }
		bool const refused = SUCCEEDED(hr) && reader && !host::MediaParsers_CheckReader(reader.Get(), { picture }, before, &why);
		Check(registered && refused && why.find(ThisProgramName()) != std::string::npos && FakeDecoder_Counts().fed == 0,
			"our decoder refusing, the reader making the next (a foreign one): the check refuses it by name, before it is fed a sample (" + why + ")");
		reader.Reset();
		source.Reset();
		MFTUnregisterLocal(&refusing);
		FakeDecoder_Unregister(&foreign);
	}
#endif

	printf("Part 2: reading videos through video_import\n");
	{
		host::VideoImportClip clip;
		std::string why;
		host::VideoImportResult result = host::VideoImport_ReadBytes(mp4Bytes, L"video/mp4", host::VideoImportFit::Fit, &clip, &why, 60);
		Check(result == host::VideoImportResult::Done && clip.frameCount > 0 && !clip.sound.empty(), "the H.264 + AAC clip is read, picture and sound, through Windows' own code (" + why + ")");

		// A DECODER OF THE TEST'S OWN for H.264, registered for this process: it must never be fed a sample, and the read is either refused by name or goes
		// through the decoder the editor chose (Microsoft's), never through the test's
		static FakeDecoderFactory own;
		FakeDecoder_Counts() = FakeDecoderCounts();
		FakeDecoder_Register(&own);
		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(mp4Bytes, L"video/mp4", host::VideoImportFit::Fit, &clip, &why, 60);
		bool const throughOurs = result == host::VideoImportResult::Done && clip.frameCount > 0;
		Check(FakeDecoder_Counts().fed == 0 && (throughOurs || (RefusedAsNotWindows(result, why) && why.find(ThisProgramName()) != std::string::npos)),
			"a decoder of the test's own, registered first: never fed a sample (" + std::to_string(FakeDecoder_Counts().fed) + "), and " +
			(throughOurs ? "the picture read through the editor's own choice" : "the read refused by name: " + why));
		FakeDecoder_Unregister(&own);

		// A BYTE-STREAM HANDLER OF THE TEST'S OWN for video/mp4: the editor never asks it (it makes the source from Windows' own handler, never the resolver's choice)
		static PlantedActivate planted;
		plantedAsked = 0;
		HRESULT const plantedOn = MFRegisterLocalByteStreamHandler(L".mp4", L"video/mp4", &planted);
		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(mp4Bytes, L"video/mp4", host::VideoImportFit::Fit, &clip, &why, 60);
		Check(SUCCEEDED(plantedOn) && plantedAsked == 0 && result == host::VideoImportResult::Done && clip.frameCount > 0,
			"a byte-stream handler of the test's own, planted for video/mp4: never asked (" + std::to_string(plantedAsked) + " times), the clip read through Windows' own");
		// (a handler registered so stays until MFShutdown, at the test's end: Media Foundation has no call to take it away. Every read after this is an AVI's)

		// A PICTURE IN A FORMAT NO DECODER OF WINDOWS' OWN READS ("XXXX"): refused, named, before any decoder
		std::vector<uint8_t> const offList = AviTest_Make(AviTest_Fourcc("XXXX"), 64, 48, 6, 15, 1);
		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(offList, L"video/avi", host::VideoImportFit::Fit, &clip, &why, 60);
		Check(RefusedAsNotWindows(result, why) && why.find("\"XXXX\"") != std::string::npos, "an AVI whose picture is \"XXXX\": refused, the format named (" + why + ")");

		// A SOUND IN A FORMAT NO DECODER OF WINDOWS' OWN READS (tag 0x1234): the video comes in silent, and says why
		std::vector<uint8_t> const silentOne = AviTest_Make(0, 64, 48, 6, 15, 0x1234);
		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(silentOne, L"video/avi", host::VideoImportFit::Fit, &clip, &why, 60);
		bool silent = result == host::VideoImportResult::Done && clip.frameCount > 0 && clip.sound.empty();
#ifndef RED_FIRST_OLD_IMPORT
		silent = silent && clip.soundLeftOut.find("format tag 0x1234") != std::string::npos && clip.soundLeftOut.find("left out") != std::string::npos;
		Check(silent, "an AVI whose sound is format tag 0x1234: read silent, and it says why (" + clip.soundLeftOut + ")");
#else
		Check(silent, "an AVI whose sound is format tag 0x1234: read silent (" + why + ")");
#endif

		// AND ONE WHOLLY TAKEN: uncompressed RGB (no decoder at all) and PCM sound
		std::vector<uint8_t> const plain = AviTest_Make(0, 64, 48, 6, 15, 1);
		clip = host::VideoImportClip();
		result = host::VideoImport_ReadBytes(plain, L"video/avi", host::VideoImportFit::Fit, &clip, &why, 60);
		Check(result == host::VideoImportResult::Done && clip.frameCount > 0 && !clip.sound.empty(), "an AVI of RGB pictures and PCM sound: read whole (" + why + ")");
	}

	if (on) MFShutdown();
	if (com) CoUninitialize();
	printf(failed ? "%d FAILED\n" : "all ok\n", failed);
	return failed ? 1 : 0;
}
