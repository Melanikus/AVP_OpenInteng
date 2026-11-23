// The test for a user's picture read for a texture (src/host/image_import.h: the RIM import's steps 1 and 2). Written 2026-11-23 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Pictures are made by the test itself, in a folder of its own in the folder it runs in: PNGs by Windows' own WIC encoder, BMPs byte by byte.
//   (1) a 32 x 16 PNG with see-through, half and whole alpha: read back exactly, every byte of BGRA
//   (2) a 20 x 10 24-bit BMP (its rows bottom first, each padded to 4 bytes): read back, every pixel's alpha 255
//   (3) a 16 x 8 32-bit BMP whose fourth bytes are 0: alpha 255 still (a plain BMP's fourth byte is not alpha, as BlendAvP reads it)
//   (3b) THE WORK BOUNDED (Dietrich's review of e20ccef6, MUST 1): a 400 x 200 BMP asked for at most 100 x 100 pixels is decoded 141 x 70 by WIC's own scaler, its
//       halves kept, its own size kept for the words; within the most, as it is (red on e20ccef6: the most wasn't there, the picture came whole)
//   (4) refused, and nothing given back: a .gif's name; a .png whose bytes are a BMP's; an empty .png; a .bmp over 16 MiB; a PNG 5000 x 8 (a side over 4096); half a BMP;
//       half a PNG; a folder named like a picture; a file that isn't there
//   (5) the name's kind, whatever its capitals
// RED FIRST: on a stand-in that refuses every picture, (1) to (3) fail.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <string>
#include <vector>

#include <windows.h>
#include <wincodec.h>

#include "image_import.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Save(std::wstring const &path, std::vector<uint8_t> const &bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);

	out.write((char const *)bytes.data(), (std::streamsize)bytes.size());
	return (bool)out;
}

// A PNG of these BGRA pixels, made by WIC's own encoder (alpha kept: 32-bit BGRA in, as WIC writes it)
static bool MakePng(std::wstring const &path, uint32_t width, uint32_t height, std::vector<uint8_t> const &bgra)
{
	IWICImagingFactory *factory = nullptr;
	IWICStream *stream = nullptr;
	IWICBitmapEncoder *encoder = nullptr;
	IWICBitmapFrameEncode *frame = nullptr;
	bool made = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
		SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
		SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) && SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
		SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(width, height));
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;

	made = made && SUCCEEDED(frame->SetPixelFormat(&format)) && IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) &&
		SUCCEEDED(frame->WritePixels(height, width * 4, (UINT)bgra.size(), (BYTE *)bgra.data())) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());

	if (frame) frame->Release();
	if (encoder) encoder->Release();
	if (stream) stream->Release();
	if (factory) factory->Release();
	return made;
}

static void Put16(std::vector<uint8_t> &b, uint32_t v) { b.push_back((uint8_t)v); b.push_back((uint8_t)(v >> 8)); }
static void Put32(std::vector<uint8_t> &b, uint32_t v) { Put16(b, v & 0xffff); Put16(b, v >> 16); }

// A BMP of 24 or 32 bits, byte by byte: BITMAPFILEHEADER, BITMAPINFOHEADER (40 bytes), then the rows bottom first, each padded to 4 bytes
static std::vector<uint8_t> MakeBmp(uint32_t width, uint32_t height, uint32_t bits, std::vector<uint8_t> const &bgra)
{
	uint32_t const pixelBytes = bits / 8;
	uint32_t const rowBytes = (width * pixelBytes + 3) & ~3u;
	std::vector<uint8_t> b;

	b.push_back('B');
	b.push_back('M');
	Put32(b, 14 + 40 + rowBytes * height);
	Put32(b, 0);
	Put32(b, 14 + 40);
	Put32(b, 40);
	Put32(b, width);
	Put32(b, height);
	Put16(b, 1);
	Put16(b, bits);
	Put32(b, 0);   // BI_RGB
	Put32(b, rowBytes * height);
	Put32(b, 2835);
	Put32(b, 2835);
	Put32(b, 0);
	Put32(b, 0);

	for (uint32_t y = 0; y < height; y++)
	{
		uint32_t const row = height - 1 - y;   // bottom first
		size_t const start = b.size();

		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t const *p = &bgra[((size_t)row * width + x) * 4];

			b.push_back(p[0]);
			b.push_back(p[1]);
			b.push_back(p[2]);

			if (pixelBytes == 4)
			{
				b.push_back(0);   // the fourth byte, 0: not alpha
			}
		}

		while (b.size() - start < rowBytes)
		{
			b.push_back(0);
		}
	}

	return b;
}

static std::vector<uint8_t> Pattern(uint32_t width, uint32_t height, bool alphas)
{
	std::vector<uint8_t> bgra;

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			bgra.push_back((uint8_t)(x * 8));
			bgra.push_back((uint8_t)(y * 16));
			bgra.push_back((uint8_t)(x ^ y));
			bgra.push_back(alphas ? (uint8_t)(x % 3 == 0 ? 0 : (x % 3 == 1 ? 128 : 255)) : 255);
		}
	}

	return bgra;
}

int main()
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	std::wstring const folder = L"host_image_import_test_" + std::to_wstring(GetCurrentProcessId());

	if (!CreateDirectoryW(folder.c_str(), nullptr))
	{
		printf("  FAILED  the test's folder couldn't be made\n");
		return 1;
	}

	ImagePixels got;
	std::string why;

	// ---- (1) a PNG with alpha, exactly
	std::vector<uint8_t> const pngPixels = Pattern(32, 16, true);
	std::wstring const png = folder + L"\\picture.png";
	bool const pngMade = MakePng(png, 32, 16, pngPixels);
	ImageImportResult const pngRead = ImageImport_Read(png, &got, &why);

	Check(pngMade, "(1) a 32 x 16 PNG with see-through, half and whole alpha is made by WIC's encoder");
	Check(pngRead == ImageImportResult::Done && got.width == 32 && got.height == 16 && got.bgra == pngPixels,
		"(1) it is read back exactly, every byte of BGRA: " + std::string(ImageImport_Words(pngRead)) + " " + why);

	// ---- (2) a 24-bit BMP
	std::vector<uint8_t> const bmpPixels = Pattern(20, 10, false);
	std::wstring const bmp = folder + L"\\picture.bmp";
	bool const bmpSaved = Save(bmp, MakeBmp(20, 10, 24, bmpPixels));
	ImageImportResult const bmpRead = ImageImport_Read(bmp, &got, &why);

	Check(bmpSaved && bmpRead == ImageImportResult::Done && got.width == 20 && got.height == 10 && got.bgra == bmpPixels,
		"(2) a 20 x 10 24-bit BMP is read back, every pixel, alpha 255: " + std::string(ImageImport_Words(bmpRead)) + " " + why);

	// ---- (3) a 32-bit BMP whose fourth bytes are 0: still opaque
	std::vector<uint8_t> const bmp32Pixels = Pattern(16, 8, false);
	std::wstring const bmp32 = folder + L"\\thirty-two.bmp";
	bool const bmp32Saved = Save(bmp32, MakeBmp(16, 8, 32, bmp32Pixels));
	ImageImportResult const bmp32Read = ImageImport_Read(bmp32, &got, &why);

	Check(bmp32Saved && bmp32Read == ImageImportResult::Done && got.bgra == bmp32Pixels,
		"(3) a 32-bit BMP's fourth byte (0) isn't taken as alpha: every pixel opaque: " + std::string(ImageImport_Words(bmp32Read)) + " " + why);

	// ---- (3b) THE WORK BOUNDED FROM THE PICTURE'S OWN SIZE (Dietrich's review of e20ccef6, MUST 1): a picture of more pixels than its use asks for is scaled down
	// by WIC's own scaler as it is decoded, in the same proportion, its own size kept for the words; one within it, as it is
	{
		uint32_t const w = 400, h = 200;
		std::vector<uint8_t> halves((size_t)w * h * 4, 255);

		for (uint32_t y = 0; y < h; y++)
		{
			for (uint32_t x = 0; x < w; x++)
			{
				uint8_t *const p = &halves[((size_t)y * w + x) * 4];
				p[0] = x < w / 2 ? 0 : 255;    // blue on the right
				p[1] = 0;
				p[2] = x < w / 2 ? 255 : 0;    // red on the left
			}
		}

		std::wstring const big = folder + L"\\big.bmp";
		bool const bigSaved = Save(big, MakeBmp(w, h, 24, halves));
		ImageImportResult const scaled = ImageImport_Read(big, &got, &why, 100 * 100);
		size_t const at = ((size_t)(got.height / 2) * got.width + 5) * 4, atRight = ((size_t)(got.height / 2) * got.width + got.width - 5) * 4;

		Check(bigSaved && scaled == ImageImportResult::Done && (uint64_t)got.width * got.height <= 100 * 100 && got.width == 141 && got.height == 70 &&
			got.bgra.size() == (size_t)got.width * got.height * 4 && got.fileWidth == 400 && got.fileHeight == 200,
			"(3b) 400 x 200 asked for at most 100 x 100 pixels: decoded " + std::to_string(got.width) + " x " + std::to_string(got.height) + " (the same proportion), its own size " +
			std::to_string(got.fileWidth) + " x " + std::to_string(got.fileHeight) + " kept: " + std::string(ImageImport_Words(scaled)) + " " + why);
		Check(got.bgra.size() > atRight + 3 && got.bgra[at + 2] > 200 && got.bgra[at] < 50 && got.bgra[atRight] > 200 && got.bgra[atRight + 2] < 50,
			"(3b) and it is still red on the left and blue on the right");

		ImageImportResult const within = ImageImport_Read(big, &got, &why, 400 * 200);
		Check(within == ImageImportResult::Done && got.width == 400 && got.height == 200 && got.bgra == halves, "(3b) a picture within the most is read as it is, every pixel");
	}

	// ---- (4) refusals, with nothing given back
	auto refused = [&](std::wstring const &path, ImageImportResult want, std::string const &what)
	{
		got.width = 7;
		got.bgra.assign(3, 1);
		ImageImportResult const result = ImageImport_Read(path, &got, &why);

		Check(result == want && got.width == 0 && got.bgra.empty() && !why.empty(), "(4) " + what + ": " + ImageImport_Words(result) + " (" + why + ")");
	};

	Save(folder + L"\\picture.gif", MakeBmp(16, 8, 24, bmp32Pixels));
	refused(folder + L"\\picture.gif", ImageImportResult::NotAPictureName, "a .gif's name");

	Save(folder + L"\\lying.png", MakeBmp(16, 8, 24, bmp32Pixels));
	refused(folder + L"\\lying.png", ImageImportResult::NotThatPicture, "a .png whose bytes are a BMP's");

	Save(folder + L"\\empty.png", std::vector<uint8_t>());
	refused(folder + L"\\empty.png", ImageImportResult::Empty, "an empty .png");

	std::vector<uint8_t> huge(16 * 1024 * 1024 + 1, 0);

	huge[0] = 'B';
	huge[1] = 'M';
	Save(folder + L"\\huge.bmp", huge);
	refused(folder + L"\\huge.bmp", ImageImportResult::TooBig, "a .bmp of 16 MiB and a byte");
	huge.clear();

	std::wstring const wide = folder + L"\\wide.png";

	Check(MakePng(wide, 5000, 8, Pattern(5000, 8, false)), "(4) a PNG 5000 x 8 is made");
	refused(wide, ImageImportResult::TooLarge, "a PNG 5000 x 8 (a side over 4096), refused from its header");

	std::vector<uint8_t> half;
	{
		std::ifstream in(png, std::ios::binary);
		std::vector<uint8_t> whole((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		half.assign(whole.begin(), whole.begin() + (ptrdiff_t)(whole.size() / 2));
	}
	Save(folder + L"\\half.png", half);

	// half a BMP: its header says more pixels than the file holds
	std::vector<uint8_t> const wholeBmp = MakeBmp(20, 10, 24, bmpPixels);

	Save(folder + L"\\half.bmp", std::vector<uint8_t>(wholeBmp.begin(), wholeBmp.begin() + (ptrdiff_t)(wholeBmp.size() / 2)));
	refused(folder + L"\\half.bmp", ImageImportResult::BadPicture, "half a BMP (its header names more pixels than the file holds)");
	refused(folder + L"\\half.png", ImageImportResult::BadPicture, "half a PNG");

	CreateDirectoryW((folder + L"\\folder.png").c_str(), nullptr);
	refused(folder + L"\\folder.png", ImageImportResult::NotAFile, "a folder named like a picture");
	refused(folder + L"\\not-there.png", ImageImportResult::NotAFile, "a file that isn't there");

	// ---- (5) the name's kind
	Check(ImageImport_KindOfName(L"a.PNG") == 1 && ImageImport_KindOfName(L"b.Bmp") == 2 && ImageImport_KindOfName(L"c.jpg") == 0 && ImageImport_KindOfName(L".png") == 0,
		"(5) .PNG is a PNG, .Bmp a BMP, .jpg neither, and \".png\" alone has no name");

	// taken away again
	for (wchar_t const *name : { L"picture.png", L"picture.bmp", L"thirty-two.bmp", L"big.bmp", L"picture.gif", L"lying.png", L"empty.png", L"huge.bmp", L"wide.png", L"half.png",
		L"half.bmp" })
	{
		DeleteFileW((folder + L"\\" + name).c_str());
	}

	RemoveDirectoryW((folder + L"\\folder.png").c_str());
	RemoveDirectoryW(folder.c_str());
	CoUninitialize();
	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
