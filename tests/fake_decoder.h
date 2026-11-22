// fake_decoder.h: A DECODER THAT ISN'T MICROSOFT'S, for the tests of whose code reads a video (media_parsers.h). Its code is the test program's own (not signed), it
// says it decodes H.264 to NV12, and it counts how often it was given its input type and how often it was fed a sample (it decodes nothing: a sample fed to it is
// a sample the check should have stopped). Registered for this process only (MFTRegisterLocal), never in the registry. Written 2026-11-22 by Melanikus (Vasquez)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <memory>

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <wrl/client.h>

struct FakeDecoderCounts
{
	LONG made = 0;        // how many were made
	LONG typed = 0;       // SetInputType calls (the codec's headers would be read there)
	LONG fed = 0;         // ProcessInput / ProcessOutput calls (samples)
};

inline FakeDecoderCounts &FakeDecoder_Counts()
{
	static FakeDecoderCounts counts;
	return counts;
}

class FakeDecoder : public IMFTransform
{
public:
	FakeDecoder() { InterlockedIncrement(&FakeDecoder_Counts().made); }
	virtual ~FakeDecoder() {}

	STDMETHODIMP QueryInterface(REFIID id, void **out) override
	{
		if (!out) return E_POINTER;
		if (id == __uuidof(IUnknown) || id == __uuidof(IMFTransform)) { *out = static_cast<IMFTransform *>(this); AddRef(); return S_OK; }
		*out = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&references); }
	STDMETHODIMP_(ULONG) Release() override { LONG const left = InterlockedDecrement(&references); if (left == 0) delete this; return (ULONG)left; }

	STDMETHODIMP GetStreamLimits(DWORD *a, DWORD *b, DWORD *c, DWORD *d) override { *a = *b = *c = *d = 1; return S_OK; }
	STDMETHODIMP GetStreamCount(DWORD *in, DWORD *out) override { *in = *out = 1; return S_OK; }
	STDMETHODIMP GetStreamIDs(DWORD, DWORD *, DWORD, DWORD *) override { return E_NOTIMPL; }
	STDMETHODIMP GetInputStreamInfo(DWORD, MFT_INPUT_STREAM_INFO *info) override { *info = {}; return S_OK; }
	STDMETHODIMP GetOutputStreamInfo(DWORD, MFT_OUTPUT_STREAM_INFO *info) override { *info = {}; info->cbSize = 1920 * 1088 * 3 / 2; return S_OK; }
	STDMETHODIMP GetAttributes(IMFAttributes **) override { return E_NOTIMPL; }
	STDMETHODIMP GetInputStreamAttributes(DWORD, IMFAttributes **) override { return E_NOTIMPL; }
	STDMETHODIMP GetOutputStreamAttributes(DWORD, IMFAttributes **) override { return E_NOTIMPL; }
	STDMETHODIMP DeleteInputStream(DWORD) override { return E_NOTIMPL; }
	STDMETHODIMP AddInputStreams(DWORD, DWORD *) override { return E_NOTIMPL; }
	STDMETHODIMP GetInputAvailableType(DWORD, DWORD, IMFMediaType **) override { return MF_E_NO_MORE_TYPES; }

	STDMETHODIMP GetOutputAvailableType(DWORD, DWORD index, IMFMediaType **type) override
	{
		if (!in) return MF_E_TRANSFORM_TYPE_NOT_SET;
		if (index > 0) return MF_E_NO_MORE_TYPES;
		Microsoft::WRL::ComPtr<IMFMediaType> nv12;
		HRESULT hr = MFCreateMediaType(&nv12);
		UINT32 width = 0, height = 0;
		MFGetAttributeSize(in.Get(), MF_MT_FRAME_SIZE, &width, &height);
		if (SUCCEEDED(hr))
		{
			nv12->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			nv12->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
			MFSetAttributeSize(nv12.Get(), MF_MT_FRAME_SIZE, width ? width : 320, height ? height : 240);
			MFSetAttributeRatio(nv12.Get(), MF_MT_FRAME_RATE, 15, 1);
			MFSetAttributeRatio(nv12.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
			nv12->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
			nv12->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
			*type = nv12.Detach();
		}
		return hr;
	}

	STDMETHODIMP SetInputType(DWORD, IMFMediaType *type, DWORD flags) override
	{
		InterlockedIncrement(&FakeDecoder_Counts().typed);
		if (!type) { in.Reset(); return S_OK; }
		GUID subtype = GUID_NULL;
		if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) || (subtype != MFVideoFormat_H264 && subtype != MFVideoFormat_H264_ES)) return MF_E_INVALIDMEDIATYPE;
		if (!(flags & MFT_SET_TYPE_TEST_ONLY)) in = type;
		return S_OK;
	}

	STDMETHODIMP SetOutputType(DWORD, IMFMediaType *type, DWORD flags) override
	{
		if (!type) { out.Reset(); return S_OK; }
		GUID subtype = GUID_NULL;
		if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) || subtype != MFVideoFormat_NV12) return MF_E_INVALIDMEDIATYPE;
		if (!(flags & MFT_SET_TYPE_TEST_ONLY)) out = type;
		return S_OK;
	}

	STDMETHODIMP GetInputCurrentType(DWORD, IMFMediaType **type) override { if (!in) return MF_E_TRANSFORM_TYPE_NOT_SET; *type = in.Get(); (*type)->AddRef(); return S_OK; }
	STDMETHODIMP GetOutputCurrentType(DWORD, IMFMediaType **type) override { if (!out) return MF_E_TRANSFORM_TYPE_NOT_SET; *type = out.Get(); (*type)->AddRef(); return S_OK; }
	STDMETHODIMP GetInputStatus(DWORD, DWORD *flags) override { *flags = MFT_INPUT_STATUS_ACCEPT_DATA; return S_OK; }
	STDMETHODIMP GetOutputStatus(DWORD *flags) override { *flags = 0; return S_OK; }
	STDMETHODIMP SetOutputBounds(LONGLONG, LONGLONG) override { return E_NOTIMPL; }
	STDMETHODIMP ProcessEvent(DWORD, IMFMediaEvent *) override { return E_NOTIMPL; }
	STDMETHODIMP ProcessMessage(MFT_MESSAGE_TYPE, ULONG_PTR) override { return S_OK; }
	STDMETHODIMP ProcessInput(DWORD, IMFSample *, DWORD) override { InterlockedIncrement(&FakeDecoder_Counts().fed); return E_FAIL; }
	STDMETHODIMP ProcessOutput(DWORD, DWORD, MFT_OUTPUT_DATA_BUFFER *, DWORD *) override { InterlockedIncrement(&FakeDecoder_Counts().fed); return MF_E_TRANSFORM_NEED_MORE_INPUT; }

private:
	LONG references = 1;
	Microsoft::WRL::ComPtr<IMFMediaType> in, out;
};

class FakeDecoderFactory : public IClassFactory
{
public:
	STDMETHODIMP QueryInterface(REFIID id, void **out) override
	{
		if (!out) return E_POINTER;
		if (id == __uuidof(IUnknown) || id == __uuidof(IClassFactory)) { *out = static_cast<IClassFactory *>(this); AddRef(); return S_OK; }
		*out = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() override { return 2; }   // (one static factory for the test's life)
	STDMETHODIMP_(ULONG) Release() override { return 1; }
	STDMETHODIMP CreateInstance(IUnknown *outer, REFIID id, void **out) override
	{
		if (outer) return CLASS_E_NOAGGREGATION;
		// made owned (make_unique), then handed to its COM count: the caller's reference keeps it, and its Release deletes it at 0 (Snyk, 11-22: a bare
		// new handed out through QueryInterface reads as a leak)
		std::unique_ptr<FakeDecoder> made = std::make_unique<FakeDecoder>();
		HRESULT const hr = made->QueryInterface(id, out);
		made.release()->Release();
		return hr;
	}
	STDMETHODIMP LockServer(BOOL) override { return S_OK; }
};

// The fake registered for this process only, as an H.264 video decoder; and taken away again
inline HRESULT FakeDecoder_Register(FakeDecoderFactory *factory)
{
	MFT_REGISTER_TYPE_INFO input[2] = { { MFMediaType_Video, MFVideoFormat_H264 }, { MFMediaType_Video, MFVideoFormat_H264_ES } };
	MFT_REGISTER_TYPE_INFO output = { MFMediaType_Video, MFVideoFormat_NV12 };
	return MFTRegisterLocal(factory, MFT_CATEGORY_VIDEO_DECODER, L"A decoder that isn't Microsoft's (the test's own)", MFT_ENUM_FLAG_SYNCMFT, 2, input, 1, &output);
}

inline HRESULT FakeDecoder_Unregister(FakeDecoderFactory *factory)
{
	return MFTUnregisterLocal(factory);
}
