// image_fit.h: a picture of any size made a square texture's size (the RIM import's step 3: AvP-docs director/plan-rim-import.md). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files.
//
// THE SIDES a texture is made: 128, 256 or 512, the stock level textures' own squares (Stage 1; the width a multiple of 16, as rim_write.h needs).
// THE THREE WAYS, as Video mode names them:
//   Fit      the whole picture, its shape kept, in the middle; the bars beside it black, or see-through when asked (alpha 0)
//   Crop     the square filled, the picture's shape kept, its middle kept and its edges cut
//   Stretch  the whole picture over the whole square, its shape not kept
// THE RESAMPLE: each side on its own (rows, then columns). Shrinking, each new pixel is the average of the old ones it covers, a part of a pixel counting for its
// part; growing, each new pixel is drawn between its two nearest old ones. Colours are averaged weighted by their alpha (premultiplied), so a see-through pixel's
// colour never bleeds into its neighbours; alpha is averaged. Rounded to the nearest; done in doubles, so the same picture gives the same bytes on every machine.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	enum class ImageFitMode
	{
		Fit,
		Crop,
		Stretch,
	};

	// A side is one of these
	bool ImageFit_SideTaken(uint32_t side);

	// bgra: width x height pixels of 4 bytes (blue, green, red, alpha), row by row, top first (image_import.h's). *out: side x side such pixels. False, *out empty and
	// *why saying why, for no picture, a side of 0 or over 4096, more than 16 M pixels, or a square side not taken
	bool ImageFit_Square(uint8_t const *bgra, uint32_t width, uint32_t height, uint32_t side, ImageFitMode mode, bool transparentBars, std::vector<uint8_t> *out,
		std::string *why);
}
