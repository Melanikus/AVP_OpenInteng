// verify_jpeg: MAKES the JPEG picture acceptance-texture-import-jpeg drops on the editor (Drake 2026-11-24, the owner's "Add JPEG in beta.0"). The repository
// holds no picture; Node, which makes the PNG of acceptance-texture-import, has no JPEG encoder; so this asks Windows' own (WIC's JPEG encoder) for one.
// Written 2026-11-24 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_jpeg.exe make <a new .jpg file> <side>
//   a <side> x <side> JPEG (8 to 2048) of a smooth blue-green ramp, 24-bit, baseline, written as a NEW file: a file already there is never written over
//   (refused), and its folder is made when it isn't there (one folder, as make-level's PNG does). Named verify_ so run-shards builds it from the tree it runs.
// Gives back 0 only when the file was made and read back as a JPEG of that size.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>
#include <wincodec.h>

namespace
{
	int Failed(std::string const &why)
	{
		printf("FAILED verify_jpeg: %s\n", why.c_str());
		return 1;
	}

	std::wstring Wide(char const *narrow)
	{
		int const length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, narrow, -1, nullptr, 0);

		if (length <= 0)
		{
			return std::wstring();
		}

		std::wstring wide((size_t)length - 1, L'\0');
		MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, narrow, -1, &wide[0], length);

		for (wchar_t &c : wide)
		{
			c = c == L'/' ? L'\\' : c;
		}

		return wide;
	}

	template <typename T> void LetGo(T *&thing)
	{
		if (thing)
		{
			thing->Release();
			thing = nullptr;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 4 || strcmp(argv[1], "make") != 0)
	{
		return Failed("use: make <a new .jpg file> <side>");
	}

	char *end = nullptr;
	long const side = strtol(argv[3], &end, 10);

	if (!end || *end != '\0' || end == argv[3] || side < 8 || side > 2048)
	{
		return Failed("\"" + std::string(argv[3]) + "\" isn't a side from 8 to 2048");
	}

	std::wstring const path = Wide(argv[2]);
	size_t const length = path.size();

	if (length < 5 || _wcsicmp(path.c_str() + length - 4, L".jpg") != 0)
	{
		return Failed("the file's name must end in .jpg");
	}

	if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		return Failed(std::string(argv[2]) + " is there already: never written over");
	}

	size_t const slash = path.find_last_of(L'\\');

	if (slash != std::wstring::npos)
	{
		std::wstring const folder = path.substr(0, slash);

		if (GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES && !CreateDirectoryW(folder.c_str(), nullptr))
		{
			return Failed("its folder couldn't be made (Windows error " + std::to_string((unsigned long)GetLastError()) + ")");
		}
	}

	// the picture: a smooth ramp, blue across, green down, red half, 24-bit BGR rows
	uint32_t const n = (uint32_t)side;
	std::vector<uint8_t> rows((size_t)n * n * 3);

	for (uint32_t y = 0; y < n; y++)
	{
		for (uint32_t x = 0; x < n; x++)
		{
			uint8_t *const p = &rows[((size_t)y * n + x) * 3];

			p[0] = (uint8_t)(x * 255 / (n - 1));
			p[1] = (uint8_t)(y * 255 / (n - 1));
			p[2] = 128;
		}
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
	{
		return Failed("COM couldn't be started");
	}

	IWICImagingFactory *factory = nullptr;
	IWICStream *stream = nullptr;
	IWICBitmapEncoder *encoder = nullptr;
	IWICBitmapFrameEncode *frame = nullptr;
	WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
	bool made = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
		SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
		SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder)) && SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
		SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(n, n)) &&
		SUCCEEDED(frame->SetPixelFormat(&format)) && IsEqualGUID(format, GUID_WICPixelFormat24bppBGR) &&
		SUCCEEDED(frame->WritePixels(n, n * 3, (UINT)rows.size(), rows.data())) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());

	LetGo(frame);
	LetGo(encoder);
	LetGo(stream);

	// read back: a JPEG of that size
	IWICBitmapDecoder *decoder = nullptr;
	IWICBitmapFrameDecode *first = nullptr;
	UINT width = 0, height = 0;
	GUID container = {};
	bool const readBack = made && factory && SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) &&
		SUCCEEDED(decoder->GetContainerFormat(&container)) && IsEqualGUID(container, GUID_ContainerFormatJpeg) && SUCCEEDED(decoder->GetFrame(0, &first)) &&
		SUCCEEDED(first->GetSize(&width, &height)) && width == n && height == n;

	LetGo(first);
	LetGo(decoder);
	LetGo(factory);
	CoUninitialize();

	if (!made)
	{
		return Failed("WIC's JPEG encoder couldn't make " + std::string(argv[2]));
	}

	if (!readBack)
	{
		return Failed(std::string(argv[2]) + " wasn't read back as a " + std::to_string(n) + " x " + std::to_string(n) + " JPEG");
	}

	printf("ok verify_jpeg make: %s, a %ld x %ld JPEG by WIC's own encoder\n", argv[2], side, side);
	return 0;
}
