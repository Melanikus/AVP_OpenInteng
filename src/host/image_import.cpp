// image_import.cpp: a user's picture read as hostile input and decoded by Windows' own WIC (image_import.h says the rules and why). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "image_import.h"

#include <math.h>   // sqrt: the scale that brings a picture to its use's most pixels

#include <windows.h>
#include <wincodec.h>

#include "media_parsers.h"
#include "video_import.h"

namespace host
{
	namespace
	{
		char const kWhat[] = "Import texture";

		ImageImportResult Refuse(ImageImportResult result, std::string const &words, std::string *why)
		{
			if (why)
			{
				*why = std::string(kWhat) + ": " + words;
			}

			return result;
		}

		std::string Hex(HRESULT result)
		{
			char text[16];

			sprintf_s(text, "0x%08lx", (unsigned long)result);
			return text;
		}

		// A COM object let go when it goes out of scope
		template <typename T>
		struct Held
		{
			T *p = nullptr;
			~Held()
			{
				if (p)
				{
					p->Release();
				}
			}
		};

		// COM for this thread, stopped at the end only when it was started here (an apartment already there, of either kind, is used as it is)
		struct ComHere
		{
			bool started = false;
			HRESULT result = S_OK;

			ComHere()
			{
				result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
				started = SUCCEEDED(result);
			}

			~ComHere()
			{
				if (started)
				{
					CoUninitialize();
				}
			}

			bool Usable() const
			{
				return started || result == RPC_E_CHANGED_MODE;
			}
		};
	}

	char const *ImageImport_Words(ImageImportResult result)
	{
		switch (result)
		{
		case ImageImportResult::Done: return "read";
		case ImageImportResult::BadCall: return "nothing to read it into";
		case ImageImportResult::NotAPictureName: return "a picture's name ends in .png, .bmp, .jpg or .jpeg";
		case ImageImportResult::NotAFile: return "not a plain file";
		case ImageImportResult::Empty: return "the file is empty";
		case ImageImportResult::TooBig: return "the file is too big";
		case ImageImportResult::CantRead: return "Windows couldn't read it";
		case ImageImportResult::NotThatPicture: return "its bytes aren't the picture its name says";
		case ImageImportResult::NoWic: return "Windows' imaging couldn't be started";
		case ImageImportResult::NotWindowsCode: return "the code that would read it isn't Microsoft's own";
		case ImageImportResult::BadPicture: return "it couldn't be decoded";
		case ImageImportResult::TooLarge: return "the picture is too large";
		}

		return "not read";
	}

	int ImageImport_KindOfName(std::wstring const &path)
	{
		// the ending after the name's last dot, with a name before it (".png" alone is no name) and no folder after it ("C:\my.png\wall" is no picture)
		size_t const dot = path.find_last_of(L'.');

		if (dot == std::wstring::npos || dot == 0 || path.find_first_of(L"\\/", dot) != std::wstring::npos || path[dot - 1] == L'\\' || path[dot - 1] == L'/')
		{
			return 0;
		}

		wchar_t const *const ending = path.c_str() + dot;

		if (_wcsicmp(ending, L".png") == 0)
		{
			return 1;
		}

		if (_wcsicmp(ending, L".bmp") == 0)
		{
			return 2;
		}

		return _wcsicmp(ending, L".jpg") == 0 || _wcsicmp(ending, L".jpeg") == 0 ? 3 : 0;   // (Drake 2026-11-24: JPEG, the owner's "Add JPEG in beta.0")
	}

	ImageImportResult ImageImport_Read(std::wstring const &path, ImagePixels *out, std::string *why, uint64_t mostPixels)
	{
		if (!out)
		{
			return Refuse(ImageImportResult::BadCall, "no place for the picture", why);
		}

		*out = ImagePixels();

		int const kind = ImageImport_KindOfName(path);

		if (kind == 0)
		{
			return Refuse(ImageImportResult::NotAPictureName, "a picture's name ends in .png, .bmp, .jpg or .jpeg", why);
		}

		// ---- 1. THE FILE, by Video mode's rules (video_import.h), all of it into memory
		std::vector<uint8_t> bytes;
		std::string readWhy;
		VideoImportResult const read = VideoImport_ReadPlainFile(path, &bytes, &readWhy, kImageImportMostBytes, kWhat, "the picture");

		if (read != VideoImportResult::Done)
		{
			if (why)
			{
				*why = readWhy;
			}

			return read == VideoImportResult::Empty ? ImageImportResult::Empty : read == VideoImportResult::TooBig ? ImageImportResult::TooBig :
				read == VideoImportResult::CantRead ? ImageImportResult::CantRead : read == VideoImportResult::BadCall ? ImageImportResult::BadCall : ImageImportResult::NotAFile;
		}

		// its first bytes say what it is, and must agree with its name
		static uint8_t const kPng[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
		bool const isPng = bytes.size() >= 8 && memcmp(bytes.data(), kPng, 8) == 0;
		bool const isBmp = bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M';
		bool const isJpeg = bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF;   // its start marker, then the next marker's first byte

		if ((kind == 1 && !isPng) || (kind == 2 && !isBmp) || (kind == 3 && !isJpeg))
		{
			return Refuse(ImageImportResult::NotThatPicture, kind == 1 ? "its name ends in .png, but its bytes aren't a PNG's" : kind == 2 ? "its name ends in .bmp, but its "
				"bytes aren't a BMP's" : "its name ends in .jpg or .jpeg, but its bytes aren't a JPEG's", why);
		}

		// A JPEG ends with its end marker (FF D9). WIC's JPEG decoder, as its PNG one, decodes a JPEG cut short without a word, its missing blocks filled in, so a
		// JPEG that doesn't end so is refused as a PNG that doesn't end with its IEND is (below). (Drake 2026-11-24)
		if (kind == 3 && (bytes.size() < 4 || bytes[bytes.size() - 2] != 0xFF || bytes[bytes.size() - 1] != 0xD9))
		{
			return Refuse(ImageImportResult::BadPicture, "it doesn't end as a JPEG ends (its end marker): cut short, or something added after it", why);
		}

		// A PNG ends with its IEND chunk (no data, and its CRC: twelve fixed bytes). WIC decodes a PNG cut short without a word, its missing rows filled in (measured
		// 2026-11-23: half a PNG read back as "done"), which would make a texture of whatever was left: so a PNG that doesn't end so is refused
		static uint8_t const kIend[12] = { 0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82 };

		if (kind == 1 && (bytes.size() < 8 + 12 || memcmp(bytes.data() + bytes.size() - 12, kIend, 12) != 0))
		{
			return Refuse(ImageImportResult::BadPicture, "it doesn't end as a PNG ends (its IEND part): cut short, or something added after it", why);
		}

		if (bytes.size() > 0xffffffffull)
		{
			return Refuse(ImageImportResult::TooBig, "the file is too big", why);
		}

		// ---- 2. WIC, its factory, the decoder of that kind made by its own class, each Microsoft's own code
		ComHere com;

		if (!com.Usable())
		{
			return Refuse(ImageImportResult::NoWic, "COM couldn't be started (" + Hex(com.result) + ")", why);
		}

		Held<IWICImagingFactory> factory;
		HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory.p));

		if (FAILED(result) || !factory.p)
		{
			return Refuse(ImageImportResult::NoWic, "Windows' imaging (WIC) couldn't be made (" + Hex(result) + ")", why);
		}

		std::string codeWhy;

		if (!MediaParsers_CheckObject(factory.p, "Windows' imaging factory", &codeWhy))
		{
			return Refuse(ImageImportResult::NotWindowsCode, codeWhy, why);
		}

		// each kind's decoder made by its own class, Windows' own: PNG, BMP, JPEG (Drake 2026-11-24). NEVER WIC's any-decoder path (CreateDecoderFromStream and the
		// like pick from every codec the PC has registered, a stranger's among them)
		Held<IWICBitmapDecoder> decoder;
		CLSID const &decoderClass = kind == 1 ? CLSID_WICPngDecoder2 : kind == 2 ? CLSID_WICBmpDecoder : CLSID_WICJpegDecoder;
		char const *const kindName = kind == 1 ? "PNG" : kind == 2 ? "BMP" : "JPEG";

		result = CoCreateInstance(decoderClass, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&decoder.p));

		if (FAILED(result) || !decoder.p)
		{
			return Refuse(ImageImportResult::NoWic, std::string("Windows' ") + kindName + " decoder couldn't be made (" + Hex(result) + ")", why);
		}

		if (!MediaParsers_CheckObject(decoder.p, kind == 1 ? "the PNG decoder" : kind == 2 ? "the BMP decoder" : "the JPEG decoder", &codeWhy))
		{
			return Refuse(ImageImportResult::NotWindowsCode, codeWhy, why);
		}

		// ---- 3. THE PICTURE, from the bytes in memory
		Held<IWICStream> stream;

		result = factory.p->CreateStream(&stream.p);

		if (FAILED(result) || !stream.p || FAILED(result = stream.p->InitializeFromMemory(bytes.data(), (DWORD)bytes.size())))
		{
			return Refuse(ImageImportResult::NoWic, "Windows' imaging couldn't take the bytes (" + Hex(result) + ")", why);
		}

		result = decoder.p->Initialize(stream.p, WICDecodeMetadataCacheOnDemand);

		if (FAILED(result))
		{
			return Refuse(ImageImportResult::BadPicture, "the decoder refused it (" + Hex(result) + ")", why);
		}

		UINT frames = 0;

		if (FAILED(result = decoder.p->GetFrameCount(&frames)) || frames != 1)
		{
			return Refuse(ImageImportResult::BadPicture, "it has " + std::to_string(frames) + " pictures in it, not one", why);
		}

		Held<IWICBitmapFrameDecode> frame;
		UINT width = 0, height = 0;

		if (FAILED(result = decoder.p->GetFrame(0, &frame.p)) || !frame.p || FAILED(result = frame.p->GetSize(&width, &height)))
		{
			return Refuse(ImageImportResult::BadPicture, "its picture couldn't be read (" + Hex(result) + ")", why);
		}

		// the sides, from the header, before a pixel is decoded
		if (width == 0 || height == 0 || width > kImageImportMostSide || height > kImageImportMostSide || (uint64_t)width * height > kImageImportMostPixels)
		{
			return Refuse(ImageImportResult::TooLarge, "it is " + std::to_string(width) + " x " + std::to_string(height) + ": each side 1 to " +
				std::to_string(kImageImportMostSide) + ", at most " + std::to_string(kImageImportMostPixels) + " pixels", why);
		}

		// ---- 4. SCALED DOWN FIRST, when it has more pixels than its use needs (mostPixels): by WIC's own scaler, in the same proportion, as it is decoded, so
		// nothing after this holds more than mostPixels (Dietrich's review of e20ccef6, MUST 1: the fit's planes are of the decoded size)
		UINT const fileWidth = width, fileHeight = height;
		Held<IWICBitmapScaler> scaler;
		IWICBitmapSource *source = frame.p;

		if (mostPixels > 0 && (uint64_t)width * height > mostPixels)
		{
			double const scale = sqrt((double)mostPixels / ((double)width * (double)height));
			UINT const scaledWidth = (UINT)(width * scale) > 0 ? (UINT)(width * scale) : 1, scaledHeight = (UINT)(height * scale) > 0 ? (UINT)(height * scale) : 1;

			if (FAILED(result = factory.p->CreateBitmapScaler(&scaler.p)) || !scaler.p)
			{
				return Refuse(ImageImportResult::NoWic, "Windows' imaging couldn't make its scaler (" + Hex(result) + ")", why);
			}

			if (!MediaParsers_CheckObject(scaler.p, "the picture's scaler", &codeWhy))
			{
				return Refuse(ImageImportResult::NotWindowsCode, codeWhy, why);
			}

			if (FAILED(result = scaler.p->Initialize(frame.p, scaledWidth, scaledHeight, WICBitmapInterpolationModeFant)))
			{
				return Refuse(ImageImportResult::BadPicture, "it couldn't be scaled down to " + std::to_string(scaledWidth) + " x " + std::to_string(scaledHeight) + " (" + Hex(result) +
					")", why);
			}

			source = scaler.p;
			width = scaledWidth;
			height = scaledHeight;
		}

		// ---- 5. THE PIXELS, as 32-bit BGRA, by WIC's own converter
		Held<IWICFormatConverter> converter;

		if (FAILED(result = factory.p->CreateFormatConverter(&converter.p)) || !converter.p)
		{
			return Refuse(ImageImportResult::NoWic, "Windows' imaging couldn't make its converter (" + Hex(result) + ")", why);
		}

		if (!MediaParsers_CheckObject(converter.p, "the picture's converter", &codeWhy))
		{
			return Refuse(ImageImportResult::NotWindowsCode, codeWhy, why);
		}

		if (FAILED(result = converter.p->Initialize(source, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		{
			return Refuse(ImageImportResult::BadPicture, "its pixels couldn't be made BGRA (" + Hex(result) + ")", why);
		}

		ImagePixels picture;

		picture.width = width;
		picture.height = height;
		picture.fileWidth = fileWidth;
		picture.fileHeight = fileHeight;

		try
		{
			picture.bgra.resize((size_t)width * height * 4);
		}
		catch (std::bad_alloc const &)
		{
			return Refuse(ImageImportResult::TooLarge, "there is not the memory to hold it", why);
		}

		if (FAILED(result = converter.p->CopyPixels(nullptr, width * 4, (UINT)picture.bgra.size(), picture.bgra.data())))
		{
			return Refuse(ImageImportResult::BadPicture, "its pixels couldn't be decoded (" + Hex(result) + ")", why);
		}

		*out = std::move(picture);

		if (why)
		{
			why->clear();
		}

		return ImageImportResult::Done;
	}
}
