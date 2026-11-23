// palette_quantize.h: A PICTURE'S COLOURS AS A RIM'S PALETTE, at most 256 of them (RIM import, step 4 of Vasquez's plan, AvP-docs/director/plan-rim-import.md;
// the owner, 2026-11-23: drag and drop a picture and get a .rim, as avptools' small tool once did). Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code (MIT): see License.txt, "AVP_OpenInteng's own code". Not Smacker's quantizer, which is LGPL. Pure: no Windows, no files.
//
// WHAT IT DOES, with the picture's pixels as Windows' decoder gives them (4 bytes a pixel: blue, green, red, alpha), after the picture is fitted to its texture size:
//   1. ALPHA: a pixel with alpha below 128 is TRANSPARENT; 128 and above is opaque. Alpha between 0 and 255 can't be kept (a RIM has one transparent colour, no
//      partial see-through): such pixels are counted, so the editor can say so, as BlendAvP does.
//   2. THE COLOURS THAT FIT ARE KEPT EXACTLY: when the opaque pixels use at most 256 colours (255 when a transparent colour is needed too), the palette is exactly
//      those colours, in the order they first appear (row by row from the top).
//   3. MORE ARE REDUCED BY MEDIAN CUT: the colours, each weighted by how many pixels use it, start as one box; the box whose widest channel (red, green or blue)
//      spans the most splits first (on a tie, the earlier box), at the pixel-count median of that channel; until there are as many boxes as palette entries, or
//      no box holds two colours. Each entry is its box's pixel-weighted mean colour, rounded. Each colour then takes its NEAREST entry (squared distance in
//      red, green and blue; on a tie, the lower entry).
//   4. THE TRANSPARENT COLOUR, when any pixel is transparent, is the palette's last entry, with a red, green and blue that no opaque entry uses: the game finds
//      the transparent pixels by that colour. It is the first of magenta (255, 0, 255), cyan (0, 255, 255) and yellow (255, 255, 0) no opaque entry uses, else
//      the first free one counting up from black.
// DETERMINISTIC: integer arithmetic only, every order from a sort of the colours' own values or their first place; the same pixels give the same bytes on every
// machine. BOUNDED: at most kPaletteQuantizeMostSide a side and kPaletteQuantizeMostPixels pixels (the fit makes at most 512 x 512; the bound keeps the work and
// memory of a 32-bit editor small), refused with words otherwise.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	uint32_t const kPaletteQuantizeMostSide = 4096;
	uint32_t const kPaletteQuantizeMostPixels = 1024u * 1024u;

	struct PaletteRgb
	{
		uint8_t r = 0, g = 0, b = 0;
	};

	struct QuantizedImage
	{
		std::vector<uint8_t> indices;      // width x height, row by row from the top, as the pixels came in: each pixel's palette entry
		std::vector<PaletteRgb> palette;   // 1 to 256 entries, in the order a RIM's CMAP holds them
		int32_t transparent = -1;          // the entry every transparent pixel takes (the last one), or -1 when no pixel is transparent
		size_t coloursIn = 0;              // the distinct opaque colours the picture had
		bool reduced = false;              // they didn't fit, so median cut ran: "coloursIn colours to N"
		size_t partialAlpha = 0;           // pixels whose alpha was neither 0 nor 255 (each kept as fully opaque or fully transparent)
	};

	// bgra: width x height pixels of 4 bytes (blue, green, red, alpha), row by row, no gap between rows. False, with *why and *out untouched, for a null pointer,
	// a side of 0 or past kPaletteQuantizeMostSide, or more than kPaletteQuantizeMostPixels pixels
	bool PaletteQuantize(uint8_t const *bgra, uint32_t width, uint32_t height, QuantizedImage *out, std::string *why);
}
