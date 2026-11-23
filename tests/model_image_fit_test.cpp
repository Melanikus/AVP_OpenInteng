// The test for a picture made a square texture's size (src/model/image_fit.h: the RIM import's step 3). Written 2026-11-23 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//   (1) Fit, 300 x 200 of one colour into 128: the picture 128 x 85 in the middle (rows 21 to 105), that colour exactly; the bars black and opaque, or
//       see-through when asked
//   (2) Crop, the same picture: the whole square that colour
//   (3) Stretch, 64 x 32 red on the left and blue on the right, into 128: the left columns red, the right ones blue, only the middle ones mixed
//   (4) shrinking averages: a 256 x 256 board of single black and white pixels into 128 is grey 128 everywhere (each new pixel the mean of four)
//   (5) a see-through pixel's colour never bleeds: red at alpha 0 beside green at 255, shrunk, gives green only (half alpha at the edge)
//   (6) the same picture twice gives the same bytes; 256 and 512 are taken
//   (7) refused, nothing given back: a side of 100, a picture of 0 x 0, a side over 4096, no picture
// RED FIRST: on a stand-in that refuses every picture, (1) to (6) fail.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "image_fit.h"

using namespace model;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::vector<uint8_t> Solid(uint32_t w, uint32_t h, uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
	std::vector<uint8_t> px;

	for (uint32_t i = 0; i < w * h; i++)
	{
		px.push_back(b);
		px.push_back(g);
		px.push_back(r);
		px.push_back(a);
	}

	return px;
}

static bool PixelIs(std::vector<uint8_t> const &px, uint32_t side, uint32_t x, uint32_t y, uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
	size_t const at = ((size_t)y * side + x) * 4;

	return px.size() >= at + 4 && px[at] == b && px[at + 1] == g && px[at + 2] == r && px[at + 3] == a;
}

// every pixel of the square's rows [y0, y1) and columns [x0, x1) is that one
static bool AreaIs(std::vector<uint8_t> const &px, uint32_t side, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1, uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
	for (uint32_t y = y0; y < y1; y++)
	{
		for (uint32_t x = x0; x < x1; x++)
		{
			if (!PixelIs(px, side, x, y, b, g, r, a))
			{
				return false;
			}
		}
	}

	return true;
}

int main()
{
	std::vector<uint8_t> out;
	std::string why;

	// ---- (1) Fit: 300 x 200 of one colour (blue 10, green 200, red 30) into 128: 128 x 85 in the middle, rows 21..105
	std::vector<uint8_t> const wide = Solid(300, 200, 10, 200, 30, 255);
	bool const fitted = ImageFit_Square(wide.data(), 300, 200, 128, ImageFitMode::Fit, false, &out, &why);

	Check(fitted && out.size() == 128 * 128 * 4, "(1) Fit makes a 128 x 128 texture: " + why);
	Check(AreaIs(out, 128, 0, 128, 21, 106, 10, 200, 30, 255), "(1) rows 21 to 105 are the picture's colour, exactly");
	Check(AreaIs(out, 128, 0, 128, 0, 21, 0, 0, 0, 255) && AreaIs(out, 128, 0, 128, 106, 128, 0, 0, 0, 255), "(1) the bars above and below are black and opaque");

	bool const fittedClear = ImageFit_Square(wide.data(), 300, 200, 128, ImageFitMode::Fit, true, &out, &why);

	Check(fittedClear && AreaIs(out, 128, 0, 128, 0, 21, 0, 0, 0, 0) && AreaIs(out, 128, 0, 128, 21, 106, 10, 200, 30, 255),
		"(1) asked for see-through bars: the bars alpha 0, the picture as before");

	// ---- (2) Crop: the whole square the colour
	bool const cropped = ImageFit_Square(wide.data(), 300, 200, 128, ImageFitMode::Crop, false, &out, &why);

	Check(cropped && AreaIs(out, 128, 0, 128, 0, 128, 10, 200, 30, 255), "(2) Crop fills the whole square with the picture's colour: " + why);

	// ---- (3) Stretch: red left half, blue right half, 64 x 32 into 128 x 128
	std::vector<uint8_t> halves;

	for (uint32_t y = 0; y < 32; y++)
	{
		for (uint32_t x = 0; x < 64; x++)
		{
			std::vector<uint8_t> const p = x < 32 ? std::vector<uint8_t>{ 0, 0, 255, 255 } : std::vector<uint8_t>{ 255, 0, 0, 255 };

			halves.insert(halves.end(), p.begin(), p.end());
		}
	}

	bool const stretched = ImageFit_Square(halves.data(), 64, 32, 128, ImageFitMode::Stretch, false, &out, &why);

	Check(stretched && AreaIs(out, 128, 0, 62, 0, 128, 0, 0, 255, 255) && AreaIs(out, 128, 66, 128, 0, 128, 255, 0, 0, 255),
		"(3) Stretch: columns 0 to 61 red, 66 to 127 blue, over every row (only the middle ones mixed): " + why);

	// ---- (4) shrinking averages: a board of single black and white pixels, 256 into 128, is grey 128 (the mean of 0, 0, 255, 255 is 127.5, rounded up)
	std::vector<uint8_t> board;

	for (uint32_t y = 0; y < 256; y++)
	{
		for (uint32_t x = 0; x < 256; x++)
		{
			uint8_t const v = ((x + y) & 1) ? 255 : 0;

			board.insert(board.end(), { v, v, v, 255 });
		}
	}

	bool const shrunk = ImageFit_Square(board.data(), 256, 256, 128, ImageFitMode::Stretch, false, &out, &why);

	Check(shrunk && AreaIs(out, 128, 0, 128, 0, 128, 128, 128, 128, 255), "(4) a 1-pixel board shrunk 2:1 is grey 128 everywhere: " + why);

	// ---- (5) a see-through pixel's colour never bleeds: columns 0..127 red at alpha 0, 128..255 green opaque, 256 x 256 into 128
	std::vector<uint8_t> edge;

	for (uint32_t y = 0; y < 256; y++)
	{
		for (uint32_t x = 0; x < 256; x++)
		{
			if (x < 128) edge.insert(edge.end(), { 0, 0, 255, 0 }); else edge.insert(edge.end(), { 0, 255, 0, 255 });
		}
	}

	bool const edged = ImageFit_Square(edge.data(), 256, 256, 128, ImageFitMode::Stretch, false, &out, &why);
	bool noRed = edged;

	for (size_t i = 0; noRed && i < out.size(); i += 4)
	{
		noRed = out[i + 2] == 0 && (out[i + 3] == 0 || out[i + 1] == 255);
	}

	Check(noRed && AreaIs(out, 128, 64, 128, 0, 128, 0, 255, 0, 255) && AreaIs(out, 128, 0, 64, 0, 128, 0, 0, 0, 0),
		"(5) no red anywhere: the see-through half stays clear, the opaque half pure green: " + why);

	// ---- (6) the same bytes twice; the other sides
	std::vector<uint8_t> first, second;
	bool const once = ImageFit_Square(wide.data(), 300, 200, 256, ImageFitMode::Fit, false, &first, &why);
	bool const twice = ImageFit_Square(wide.data(), 300, 200, 256, ImageFitMode::Fit, false, &second, &why);
	bool const big = ImageFit_Square(wide.data(), 300, 200, 512, ImageFitMode::Crop, false, &out, &why);

	Check(once && twice && first == second && first.size() == 256 * 256 * 4, "(6) the same picture twice gives the same bytes (256)");
	Check(big && out.size() == 512 * 512 * 4 && AreaIs(out, 512, 0, 512, 0, 512, 10, 200, 30, 255), "(6) 512 is taken: " + why);

	// ---- (7) refusals
	auto refused = [&](uint8_t const *px, uint32_t w, uint32_t h, uint32_t side, std::string const &what)
	{
		out.assign(3, 1);
		bool const made = ImageFit_Square(px, w, h, side, ImageFitMode::Fit, false, &out, &why);

		Check(!made && out.empty() && !why.empty(), "(7) " + what + " (" + why + ")");
	};

	refused(wide.data(), 300, 200, 100, "a side of 100");
	refused(wide.data(), 0, 0, 128, "a picture of 0 x 0");
	refused(wide.data(), 5000, 2, 128, "a side over 4096");
	refused(nullptr, 300, 200, 128, "no picture");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
