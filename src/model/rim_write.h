// rim_write.h: a paletted picture written as a RIM, the game's own texture file, in the form every stock level texture has. Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng, the RIM import's step 5 (AvP-docs director/plan-rim-import.md; the owner: "we want it to drag and drop and create .rims").
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files; the bytes are given back for the caller to save.
//
// THE FORM, measured on the game's own files (2026-11-23: all 1,040 stock RIMs; 621 of the level textures exactly so, e.g. graphics\Envrnmts\GenShd1\Airvent01.RIM
// in fastfile\Tex15.FFL, 17,248 bytes), IFF ILBM, every number big-endian:
//   LIST <size> "ILBM"
//     PROP 20 "ILBM"  TRAN 8 <8 zero bytes>
//     FORM <size> "ILBM"
//       BMHD 20  width, height, x 0, y 0, planes 8, masking 0 or 2, compression 0, pad 0, transparent colour, aspect 1:1, page width, page height
//       CMAP 768  256 colours of 3 bytes (red, green, blue): the picture's own first, then black for the ones no pixel uses
//       BODY      for each row, top first: for each of the 8 planes, lowest first: one bit a pixel, the leftmost pixel in the byte's top bit
// The game's reader (avpx 3dc\RimLoader.cpp, compiled into the editor) reads each plane's row as (width + 7) / 8 bytes: byte-aligned, where ILBM's usual rule
// is 16-bit words. The two agree when the width is a multiple of 16, so only such widths are written (the retail game's reader isn't measured yet: plan R1).
// MASKING 2 makes the game draw every pixel whose colour (red, green, blue) equals the transparent colour's as see-through: it matches by colour, not by its
// number, so the transparent colour must be one no other colour a pixel uses has, or those pixels would vanish too. That is checked here.
// THE LIMITS, the stock-proven ones (plan R1): each side 16 to 512, the width a multiple of 16, 1 to 256 colours, every pixel's colour number in the palette.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	struct RimRgb
	{
		uint8_t red = 0;
		uint8_t green = 0;
		uint8_t blue = 0;
	};

	// A picture of colour numbers: width x height of them, row by row, the top row first; each a place in the palette
	struct RimPicture
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> indices;
		std::vector<RimRgb> palette;   // 1 to 256 colours
		int32_t transparent = -1;      // the colour number drawn see-through, or -1 for none
	};

	enum class RimWriteResult
	{
		Written,
		BadSize,          // a side outside 16..512, a width not a multiple of 16, or not width x height colour numbers
		BadPalette,       // no colours, or more than 256
		BadIndex,         // a pixel's colour number past the palette
		BadTransparent,   // the transparent number past the palette, or its colour shared with another colour a pixel uses
	};

	uint32_t const kRimLeastSide = 16;
	uint32_t const kRimMostSide = 512;

	// The RIM's bytes, in the form above, in *bytes (emptied first); Written, or why not (*why says which pixel or colour, when one is to blame)
	RimWriteResult RimWrite_Stock(RimPicture const &picture, std::vector<uint8_t> *bytes, std::string *why);

	char const *RimWriteResultWords(RimWriteResult result);
}
