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
//   (6) JPEG (Drake 2026-11-24, the owner's "Add JPEG in beta.0"), decoded by Windows' own JPEG decoder made by its class, never WIC's any-decoder path:
//       a 32 x 16 JPEG made by WIC's own encoder read back close (JPEG loses a little), every pixel opaque; the same named .JPEG; and THE HOSTILE ONES, built byte by
//       byte here: a CMYK one (WIC's encoder) and a progressive one (SOF2, DC only, 8 x 8) decoded or refused in words; half a JPEG and one with bytes after its end
//       refused (WIC fills a JPEG cut short without a word, as it does a PNG); a header saying 65535 x 65535 refused before a pixel is decoded; one saying 2048 x 2048
//       over data for one block (a size lie) decoded within the most pixels asked for, or refused in words; a .jpg whose bytes are a PNG's; a .jpg of junk.
// RED FIRST: on a stand-in that refuses every picture, (1) to (3) fail. (6) on 2143de03 (PNG and BMP only): every JPEG refused by its name, and (5)'s .jpg.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

// A JPEG made by WIC's own encoder: 24-bit BGR (from BGRA, its alpha dropped) or 32-bit CMYK (cmyk: four bytes a pixel, given as they are)
static bool MakeJpeg(std::wstring const &path, uint32_t width, uint32_t height, std::vector<uint8_t> const &pixels, bool cmyk)
{
	IWICImagingFactory *factory = nullptr;
	IWICStream *stream = nullptr;
	IWICBitmapEncoder *encoder = nullptr;
	IWICBitmapFrameEncode *frame = nullptr;
	std::vector<uint8_t> rows;

	if (cmyk)
	{
		rows = pixels;
	}
	else
	{
		for (size_t i = 0; i + 3 < pixels.size(); i += 4)
		{
			rows.push_back(pixels[i]);
			rows.push_back(pixels[i + 1]);
			rows.push_back(pixels[i + 2]);
		}
	}

	UINT const stride = width * (cmyk ? 4 : 3);
	bool made = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
		SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
		SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder)) && SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
		SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(width, height));
	WICPixelFormatGUID const wanted = cmyk ? GUID_WICPixelFormat32bppCMYK : GUID_WICPixelFormat24bppBGR;
	WICPixelFormatGUID format = wanted;

	made = made && SUCCEEDED(frame->SetPixelFormat(&format)) && IsEqualGUID(format, wanted) &&
		SUCCEEDED(frame->WritePixels(height, stride, (UINT)rows.size(), rows.data())) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());

	if (frame) frame->Release();
	if (encoder) encoder->Release();
	if (stream) stream->Release();
	if (factory) factory->Release();
	return made;
}

// A JPEG BUILT BYTE BY BYTE: one grey component, its header saying width x height, a table of quantisation 1s, a DC Huffman table of one code ("0": a difference of
// 0), and ONE block's data (a single 0 bit, padded with 1s), then its end marker. marker 0xC2 makes it progressive (its first scan, DC only, is a whole picture's
// first scan), 0xC0 baseline. With 8 x 8 it is a whole, mid-grey picture; with a larger size it LIES (its data is one block)
static std::vector<uint8_t> MakeDcOnlyJpeg(uint32_t width, uint32_t height, uint8_t marker)
{
	std::vector<uint8_t> b = { 0xFF, 0xD8 };

	b.insert(b.end(), { 0xFF, 0xDB, 0x00, 0x43, 0x00 });                     // DQT: 67 bytes, table 0, 8-bit
	b.insert(b.end(), 64, (uint8_t)1);
	b.insert(b.end(), { 0xFF, marker, 0x00, 0x0B, 0x08, (uint8_t)(height >> 8), (uint8_t)height, (uint8_t)(width >> 8), (uint8_t)width, 0x01, 0x01, 0x11, 0x00 });
	b.insert(b.end(), { 0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01 });               // DHT: 20 bytes, DC table 0, one code of length 1
	b.insert(b.end(), 15, (uint8_t)0);
	b.push_back(0x00);                                                       // its one symbol: category 0, a difference of 0
	b.insert(b.end(), { 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 });   // SOS: one component, DC only (Ss 0, Se 0, Ah Al 0)
	b.push_back(0x7F);                                                       // one block: the bit 0, then 1s
	b.insert(b.end(), { 0xFF, 0xD9 });
	return b;
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
	Check(ImageImport_KindOfName(L"a.PNG") == 1 && ImageImport_KindOfName(L"b.Bmp") == 2 && ImageImport_KindOfName(L".png") == 0 && ImageImport_KindOfName(L"c.gif") == 0,
		"(5) .PNG is a PNG, .Bmp a BMP, .gif neither, and \".png\" alone has no name");
	Check(ImageImport_KindOfName(L"c.jpg") == 3 && ImageImport_KindOfName(L"d.JPEG") == 3 && ImageImport_KindOfName(L"C:\\pics\\e.Jpeg") == 3 && ImageImport_KindOfName(L".jpeg") == 0 &&
		ImageImport_KindOfName(L".jpg") == 0 && ImageImport_KindOfName(L"f.jpe") == 0 && ImageImport_KindOfName(L"C:\\my.jpg\\wall") == 0,
		"(5) .jpg and .jpeg are a JPEG, any capitals; \".jpg\" and \".jpeg\" alone have no name; .jpe isn't one; a dot only in a folder's name isn't one");

	// ---- (6) JPEG, by Windows' own JPEG decoder (the owner, 2026-11-24: "Add JPEG in beta.0")
	{
		// a smooth picture (JPEG keeps smooth pictures close; a pattern of sharp steps it doesn't)
		uint32_t const w = 32, h = 16;
		std::vector<uint8_t> smooth;

		for (uint32_t y = 0; y < h; y++)
		{
			for (uint32_t x = 0; x < w; x++)
			{
				smooth.push_back((uint8_t)(64 + x * 4));
				smooth.push_back((uint8_t)(64 + y * 8));
				smooth.push_back(128);
				smooth.push_back(255);
			}
		}

		std::wstring const jpg = folder + L"\\photo.jpg";
		bool const jpgMade = MakeJpeg(jpg, w, h, smooth, false);
		ImageImportResult const jpgRead = ImageImport_Read(jpg, &got, &why);
		uint64_t difference = 0;
		bool opaque = got.bgra.size() == smooth.size();

		for (size_t i = 0; opaque && i < smooth.size(); i++)
		{
			if (i % 4 == 3)
			{
				opaque = got.bgra[i] == 255;
			}
			else
			{
				difference += (uint64_t)abs((int)got.bgra[i] - (int)smooth[i]);
			}
		}

		double const meanDifference = smooth.empty() ? 0.0 : (double)difference / (double)(smooth.size() / 4 * 3);

		Check(jpgMade, "(6) a 32 x 16 JPEG is made by WIC's own encoder");
		Check(jpgRead == ImageImportResult::Done && got.width == w && got.height == h && opaque && meanDifference < 8.0,
			"(6) it is read back 32 x 16, every pixel opaque, each colour within " + std::to_string(meanDifference) + " of the original on average (under 8): " +
			std::string(ImageImport_Words(jpgRead)) + " " + why);

		// the same bytes named .JPEG
		std::wstring const jpegName = folder + L"\\photo-again.JPEG";
		bool const copied = CopyFileW(jpg.c_str(), jpegName.c_str(), TRUE) != 0;
		ImageImportResult const jpegRead = ImageImport_Read(jpegName, &got, &why);

		Check(copied && jpegRead == ImageImportResult::Done && got.width == w && got.height == h, "(6) the same named .JPEG: read (" + std::string(ImageImport_Words(jpegRead)) + " " + why + ")");

		// decoded or refused in words, never more: a CMYK JPEG and a progressive one
		auto decodedOrRefused = [&](std::wstring const &path, uint32_t wantW, uint32_t wantH, std::string const &what)
		{
			got.width = 7;
			got.bgra.assign(3, 1);
			ImageImportResult const result = ImageImport_Read(path, &got, &why);
			bool opaqueAll = true;

			for (size_t i = 3; i < got.bgra.size(); i += 4)
			{
				opaqueAll = opaqueAll && got.bgra[i] == 255;
			}

			bool const decoded = result == ImageImportResult::Done && got.width == wantW && got.height == wantH && got.bgra.size() == (size_t)wantW * wantH * 4 && opaqueAll;
			bool const refusedInWords = result != ImageImportResult::Done && got.width == 0 && got.bgra.empty() && !why.empty();

			Check(decoded || refusedInWords, "(6) " + what + ": " + (decoded ? std::string("decoded, ") + std::to_string(wantW) + " x " + std::to_string(wantH) + ", every pixel opaque" :
				std::string("refused in words: ") + ImageImport_Words(result) + " (" + why + ")"));
			return result;
		};

		std::vector<uint8_t> cmykPixels;

		for (uint32_t i = 0; i < 16 * 16; i++)
		{
			cmykPixels.insert(cmykPixels.end(), { (uint8_t)(i % 200), 40, (uint8_t)(200 - i % 200), 20 });
		}

		std::wstring const cmyk = folder + L"\\cmyk.jpg";

		Check(MakeJpeg(cmyk, 16, 16, cmykPixels, true), "(6) a 16 x 16 CMYK JPEG is made by WIC's own encoder");
		decodedOrRefused(cmyk, 16, 16, "a CMYK JPEG");

		std::wstring const progressive = folder + L"\\progressive.jpg";

		Save(progressive, MakeDcOnlyJpeg(8, 8, 0xC2));
		ImageImportResult const progressiveRead = decodedOrRefused(progressive, 8, 8, "a progressive JPEG (SOF2, DC only, 8 x 8, built byte by byte)");

		if (progressiveRead == ImageImportResult::Done)
		{
			Check(got.bgra.size() == 8 * 8 * 4 && abs((int)got.bgra[0] - 128) <= 2 && abs((int)got.bgra[1] - 128) <= 2 && abs((int)got.bgra[2] - 128) <= 2,
				"(6) and it is mid grey, as its one block says (" + std::to_string(got.bgra.empty() ? 0 : got.bgra[0]) + ")");
		}

		// refused, with nothing given back ((4)'s check, under (6))
		auto refusedJpeg = [&](std::wstring const &path, ImageImportResult want, std::string const &what)
		{
			got.width = 7;
			got.bgra.assign(3, 1);
			ImageImportResult const result = ImageImport_Read(path, &got, &why);

			Check(result == want && got.width == 0 && got.bgra.empty() && !why.empty(), "(6) " + what + ": " + ImageImport_Words(result) + " (" + why + ")");
		};

		// refused: cut short, bytes after its end, a header past the most, a .jpg of a BMP's bytes, a .jpg of junk
		std::vector<uint8_t> whole;
		{
			std::ifstream in(jpg, std::ios::binary);
			whole.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		}

		Check(whole.size() > 100 && whole[whole.size() - 2] == 0xFF && whole[whole.size() - 1] == 0xD9, "(6) WIC's JPEG ends with its end marker (FF D9)");
		Save(folder + L"\\half.jpg", std::vector<uint8_t>(whole.begin(), whole.begin() + (ptrdiff_t)(whole.size() / 2)));
		refusedJpeg(folder + L"\\half.jpg", ImageImportResult::BadPicture, "half a JPEG (WIC would fill the rest without a word)");

		std::vector<uint8_t> after = whole;

		after.insert(after.end(), { 'x', 'y', 'z' });
		Save(folder + L"\\after.jpg", after);
		refusedJpeg(folder + L"\\after.jpg", ImageImportResult::BadPicture, "a JPEG with bytes after its end marker");

		// headers past the most, refused before a pixel is decoded: 8192 x 8 by the import's own check (a side over 4096, read from the header); 65535 x 65535 by
		// Windows' JPEG decoder itself, as it reads the header (measured 2026-11-24: WINCODEC_ERR_BADHEADER 0x88982f61, past its own most), either way in words
		Save(folder + L"\\wide.jpg", MakeDcOnlyJpeg(8192, 8, 0xC2));
		refusedJpeg(folder + L"\\wide.jpg", ImageImportResult::TooLarge, "a JPEG whose header says 8192 x 8 (a side over 4096), refused from its header before a pixel is decoded");
		Save(folder + L"\\vast.jpg", MakeDcOnlyJpeg(65535, 65535, 0xC2));
		got.width = 7;
		got.bgra.assign(3, 1);
		ImageImportResult const vast = ImageImport_Read(folder + L"\\vast.jpg", &got, &why);
		Check((vast == ImageImportResult::TooLarge || vast == ImageImportResult::BadPicture) && got.width == 0 && got.bgra.empty() && !why.empty(),
			"(6) a JPEG whose header says 65535 x 65535, refused as its header is read, before a pixel is decoded: " + std::string(ImageImport_Words(vast)) + " (" + why + ")");

		// THE SIZE LIE: its header says 2048 x 2048 (RimLoader's own most, within the import's 4096), its data one block. Asked, as the import asks, for at most
		// twice a 128 texture's side squared: decoded within that, or refused in words; never the 2048 x 2048 planes
		Save(folder + L"\\lie.jpg", MakeDcOnlyJpeg(2048, 2048, 0xC2));
		got.width = 7;
		got.bgra.assign(3, 1);
		ImageImportResult const lie = ImageImport_Read(folder + L"\\lie.jpg", &got, &why, 256 * 256);
		bool const lieDecoded = lie == ImageImportResult::Done && (uint64_t)got.width * got.height <= 256 * 256 && got.bgra.size() == (size_t)got.width * got.height * 4 &&
			got.fileWidth == 2048 && got.fileHeight == 2048;
		bool const lieRefused = lie != ImageImportResult::Done && got.width == 0 && got.bgra.empty() && !why.empty();

		Check(lieDecoded || lieRefused, "(6) a JPEG whose header says 2048 x 2048 over one block's data, asked for at most 256 x 256 pixels: " +
			(lieDecoded ? "decoded " + std::to_string(got.width) + " x " + std::to_string(got.height) + ", within the most" : "refused in words: " +
			std::string(ImageImport_Words(lie)) + " (" + why + ")"));

		Save(folder + L"\\bmp-inside.jpg", MakeBmp(4, 4, 24, Pattern(4, 4, false)));
		refusedJpeg(folder + L"\\bmp-inside.jpg", ImageImportResult::NotThatPicture, "a .jpg whose bytes are a BMP's");

		std::vector<uint8_t> junk = { 0xFF, 0xD8, 0xFF };

		for (int i = 0; i < 200; i++)
		{
			junk.push_back((uint8_t)(i * 37 + 11));
		}

		junk.insert(junk.end(), { 0xFF, 0xD9 });
		Save(folder + L"\\junk.jpg", junk);
		got.width = 7;
		got.bgra.assign(3, 1);
		ImageImportResult const junkRead = ImageImport_Read(folder + L"\\junk.jpg", &got, &why);
		Check(junkRead != ImageImportResult::Done && got.width == 0 && got.bgra.empty() && !why.empty(), "(6) a .jpg of junk between a JPEG's first and last bytes: refused in words: " +
			std::string(ImageImport_Words(junkRead)) + " (" + why + ")");
	}

	// taken away again
	for (wchar_t const *name : { L"picture.png", L"picture.bmp", L"thirty-two.bmp", L"big.bmp", L"picture.gif", L"lying.png", L"empty.png", L"huge.bmp", L"wide.png", L"half.png",
		L"half.bmp", L"photo.jpg", L"photo-again.JPEG", L"cmyk.jpg", L"progressive.jpg", L"half.jpg", L"after.jpg", L"wide.jpg", L"vast.jpg", L"lie.jpg", L"bmp-inside.jpg", L"junk.jpg" })
	{
		DeleteFileW((folder + L"\\" + name).c_str());
	}

	RemoveDirectoryW((folder + L"\\folder.png").c_str());
	RemoveDirectoryW(folder.c_str());
	CoUninitialize();
	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
