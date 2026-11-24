// image_import.h: A USER'S PICTURE READ FOR A TEXTURE (the RIM import's steps 1 and 2: AvP-docs director/plan-rim-import.md; the owner: "we want it to drag and drop and
// create .rims"). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE FILE IS HOSTILE INPUT, read by exactly Video mode's rules (video_import.h VideoImport_ReadPlainFile): a plain path, every folder on the way checked, one
// handle opened without following a link, judged and read whole on it, at most kImageImportMostBytes. Its name must end in .png or .bmp (Stage 1), or .jpg or .jpeg
// (Drake 2026-11-24, the owner's "Add JPEG in beta.0"), and its first bytes must say the same (a PNG's eight, a BMP's "BM", a JPEG's start marker FF D8 FF): a name
// and its bytes that disagree are refused. A PNG must end with its IEND part and a JPEG with its end marker (FF D9): WIC decodes either one cut short without a
// word, filling in what is missing.
// DECODED BY WINDOWS' OWN WIC, FROM MEMORY, never by its path: the decoder is made by its own class (Microsoft's PNG, BMP or JPEG decoder, never one WIC would pick from
// whatever codecs a PC has), and the factory, the decoder and the converter are each traced to the module their code lives in, which must be Microsoft's own
// (media_parsers.h MediaParsers_CheckObject: a class registered for one user can name any DLL). One frame; each side at most kImageImportMostSide and at most
// kImageImportMostPixels in all, read from the file's header before any pixel is decoded; then the whole picture as 32-bit BGRA by WIC's own converter (a BMP's
// fourth byte is not taken as alpha: WIC reads a plain 32-bit BMP as BGR). Every refusal says why, for the status line, and gives nothing back.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	uint64_t const kImageImportMostBytes = 16ull * 1024 * 1024;   // BlendAvP's own bound on a picture's file
	uint32_t const kImageImportMostSide = 4096;
	uint64_t const kImageImportMostPixels = 16ull * 1024 * 1024;   // 64 MB of BGRA at most, before the fit makes it a texture's size

	enum class ImageImportResult
	{
		Done,
		BadCall,
		NotAPictureName,    // the name doesn't end in .png, .bmp, .jpg or .jpeg
		NotAFile,           // missing, a folder, a link, a device, a stream, a folder on the way refused (video_import.h's rules)
		Empty,
		TooBig,             // over kImageImportMostBytes
		CantRead,
		NotThatPicture,     // its bytes aren't what its name says (a PNG's signature, a BMP's "BM")
		NoWic,              // Windows' imaging (WIC) couldn't be made
		NotWindowsCode,     // the factory, the decoder or the converter isn't Microsoft's own code
		BadPicture,         // the decoder refused it, or it has no frame, or more than one
		TooLarge,           // a side of 0 or over kImageImportMostSide, or over kImageImportMostPixels
	};

	char const *ImageImport_Words(ImageImportResult result);

	struct ImagePixels
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> bgra;   // width x height pixels of 4 bytes (blue, green, red, alpha), row by row, the top row first
		uint32_t fileWidth = 0;      // the picture's own size, as its file says it (width and height are less when it was scaled down to mostPixels)
		uint32_t fileHeight = 0;
	};

	// THE READ AND THE DECODE (above). Done with the picture in *out; else why not in *why, and *out left empty.
	// mostPixels (0: none): THE WORK BOUNDED FROM THE PICTURE'S OWN SIZE, before a pixel is decoded (Dietrich's review of e20ccef6, MUST 1, Bishop 2's ruling:
	// a 4096 x 4096 picture, a few KB as a PNG, made the fit allocate one 512 MB plane in a 32-bit editor). A picture of more pixels is scaled down by WIC's
	// own scaler (Fant: every source pixel counted), its sides in the same proportion, to at most mostPixels, as it is decoded; the caller asks for what its
	// use needs (the texture import: twice the texture's side, squared).
	ImageImportResult ImageImport_Read(std::wstring const &path, ImagePixels *out, std::string *why, uint64_t mostPixels = 0);

	// The name's kind by its ending (capitals or not): 1 a PNG, 2 a BMP, 3 a JPEG (.jpg or .jpeg), 0 none of them
	int ImageImport_KindOfName(std::wstring const &path);
}
