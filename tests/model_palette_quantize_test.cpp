// The test for the palette quantizer (src/model/palette_quantize.h, RIM import step 4): pictures that fit keep their colours exactly, in the order they first
// appear; more are reduced by median cut to at most 256 (255 with a transparent colour), each colour on its nearest entry; the same pixels give the same bytes;
// the transparent colour is last and its red, green and blue are used by no other entry; alpha below 128 is transparent and partial alpha is counted; and
// every refusal. Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code (MIT): see License.txt.
//
// The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <set>
#include <string>
#include <vector>

#include "palette_quantize.h"

using namespace model;

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A picture of width x height, every pixel from a function (red, green, blue, alpha)
struct Picture
{
	uint32_t width = 0, height = 0;
	std::vector<uint8_t> bgra;

	Picture(uint32_t w, uint32_t h) : width(w), height(h), bgra((size_t)w * h * 4, 0) {}

	void Set(uint32_t x, uint32_t y, uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255)
	{
		uint8_t *const p = &bgra[((size_t)y * width + x) * 4];

		p[0] = (uint8_t)b;
		p[1] = (uint8_t)g;
		p[2] = (uint8_t)r;
		p[3] = (uint8_t)a;
	}

	uint32_t Key(uint32_t x, uint32_t y) const
	{
		uint8_t const *const p = &bgra[((size_t)y * width + x) * 4];

		return (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
	}
};

static uint32_t KeyOf(PaletteRgb const &e)
{
	return (uint32_t)e.r << 16 | (uint32_t)e.g << 8 | e.b;
}

static bool Quantize(Picture const &picture, QuantizedImage *out)
{
	std::string why;

	return PaletteQuantize(picture.bgra.data(), picture.width, picture.height, out, &why);
}

static bool SameBytes(QuantizedImage const &a, QuantizedImage const &b)
{
	if (a.indices != b.indices || a.palette.size() != b.palette.size() || a.transparent != b.transparent || a.coloursIn != b.coloursIn || a.reduced != b.reduced)
	{
		return false;
	}

	for (size_t k = 0; k < a.palette.size(); k++)
	{
		if (KeyOf(a.palette[k]) != KeyOf(b.palette[k]))
		{
			return false;
		}
	}

	return true;
}

// Every opaque pixel's entry is a NEAREST entry to its colour (no other opaque entry strictly closer)
static bool AllNearest(Picture const &picture, QuantizedImage const &q)
{
	for (uint32_t y = 0; y < picture.height; y++)
	{
		for (uint32_t x = 0; x < picture.width; x++)
		{
			if (picture.bgra[((size_t)y * picture.width + x) * 4 + 3] < 128)
			{
				continue;
			}

			uint32_t const key = picture.Key(x, y);
			auto distance = [key](PaletteRgb const &e)
			{
				int const dr = (int)(key >> 16 & 0xff) - e.r, dg = (int)(key >> 8 & 0xff) - e.g, db = (int)(key & 0xff) - e.b;
				return dr * dr + dg * dg + db * db;
			};
			int const mine = distance(q.palette[q.indices[(size_t)y * picture.width + x]]);

			for (size_t e = 0; e < q.palette.size(); e++)
			{
				if ((int32_t)e != q.transparent && distance(q.palette[e]) < mine)
				{
					return false;
				}
			}
		}
	}

	return true;
}

static void TheRefusals()
{
	Picture const one(1, 1);
	QuantizedImage kept;
	std::string why;

	// each call made BEFORE its Check: MSVC builds a Check's words before its condition, so words read from `why` inside one call would be the last call's
	kept.coloursIn = 77;
	bool const noPixels = !PaletteQuantize(nullptr, 1, 1, &kept, &why);
	std::string const whyNoPixels = why;
	Check(noPixels && whyNoPixels == "no picture was given" && kept.coloursIn == 77, "no pixels: refused, said, nothing changed (\"" + whyNoPixels + "\")");

	Check(!PaletteQuantize(one.bgra.data(), 1, 1, nullptr, &why), "nowhere to put the result: refused");

	bool const zero = !PaletteQuantize(one.bgra.data(), 0, 1, &kept, &why);
	std::string const whyZero = why;
	Check(zero && whyZero.find("each side must be 1 to 4096") != std::string::npos, "a side of 0: refused (\"" + whyZero + "\")");

	Check(!PaletteQuantize(one.bgra.data(), 4097, 1, &kept, &why) && !PaletteQuantize(one.bgra.data(), 1, 4097, &kept, &why), "a side of 4097: refused");

	bool const tooMany = !PaletteQuantize(one.bgra.data(), 1025, 1025, &kept, &why);
	std::string const whyTooMany = why;
	Check(tooMany && whyTooMany.find("at most 1048576") != std::string::npos && kept.coloursIn == 77, "1025 x 1025 pixels: refused before a pixel is read (\"" +
		whyTooMany + "\")");
}

static void TheColoursThatFit()
{
	// 256 distinct colours, each once, in a scrambled order: kept exactly, in the order they first appear
	Picture picture(16, 16);
	std::vector<uint32_t> order;

	for (uint32_t k = 0; k < 256; k++)
	{
		uint32_t const v = (k * 97 + 13) & 0xff;   // 97 is odd: every value once
		picture.Set(k % 16, k / 16, v, 255 - v, (v * 7) & 0xff);
		order.push_back(picture.Key(k % 16, k / 16));
	}

	QuantizedImage q;
	bool exact = Quantize(picture, &q) && q.palette.size() == 256 && !q.reduced && q.coloursIn == 256 && q.transparent == -1 && q.partialAlpha == 0;

	for (size_t k = 0; exact && k < 256; k++)
	{
		exact = KeyOf(q.palette[k]) == order[k] && KeyOf(q.palette[q.indices[k]]) == order[k];
	}

	Check(exact, "256 colours: the palette is exactly those colours, in the order they first appear, and every pixel names its own");

	Picture few(4, 2);
	few.Set(0, 0, 10, 20, 30); few.Set(1, 0, 200, 0, 0); few.Set(2, 0, 10, 20, 30); few.Set(3, 0, 0, 0, 0);
	few.Set(0, 1, 200, 0, 0); few.Set(1, 1, 0, 0, 0); few.Set(2, 1, 10, 20, 30); few.Set(3, 1, 0, 0, 0);
	QuantizedImage f;
	Check(Quantize(few, &f) && f.palette.size() == 3 && KeyOf(f.palette[0]) == 0x0a141e && KeyOf(f.palette[1]) == 0xc80000 && KeyOf(f.palette[2]) == 0 &&
		f.indices == std::vector<uint8_t>({ 0, 1, 0, 2, 1, 2, 0, 2 }), "3 colours: in first-seen order, black last though it is the smallest");

	Picture single(1, 1);
	single.Set(0, 0, 1, 2, 3);
	QuantizedImage s;
	Check(Quantize(single, &s) && s.palette.size() == 1 && s.indices == std::vector<uint8_t>({ 0 }) && s.transparent == -1, "one pixel: one entry");
}

static void TheColoursThatDont()
{
	// 300 distinct colours (a red ramp, then a green ramp), each on 4 pixels
	Picture picture(300, 4);

	for (uint32_t x = 0; x < 300; x++)
	{
		for (uint32_t y = 0; y < 4; y++)
		{
			uint32_t const r = x < 256 ? x : 255, g = x < 256 ? 0 : (x - 255) * 5;
			picture.Set(x, y, r, g, (x * 3) & 0xff);
		}
	}

	QuantizedImage a, b;
	bool const ran = Quantize(picture, &a) && Quantize(picture, &b);
	std::set<uint32_t> used;

	for (uint8_t const index : a.indices)
	{
		used.insert(index);
	}

	Check(ran && a.reduced && a.coloursIn == 300 && a.palette.size() == 256 && a.transparent == -1 && used.size() <= 256 && *used.rbegin() < 256,
		"300 colours: reduced to 256 entries, every pixel on one of them (" + std::to_string(a.palette.size()) + ")");
	Check(ran && SameBytes(a, b), "the same pixels twice: the same bytes");
	Check(ran && AllNearest(picture, a), "every pixel on a nearest entry to its own colour");

	// 256 opaque colours and a transparent pixel: 255 entries for the colours, and the transparent one last
	Picture withHole(257, 1);

	for (uint32_t x = 0; x < 256; x++)
	{
		withHole.Set(x, 0, x, x, x);
	}

	withHole.Set(256, 0, 9, 9, 9, 0);
	QuantizedImage h;
	bool const holeRan = Quantize(withHole, &h);
	bool unique = holeRan && h.palette.size() == 256 && h.reduced && h.coloursIn == 256 && h.transparent == 255 && h.indices[256] == 255;

	for (size_t e = 0; unique && e < 255; e++)
	{
		unique = KeyOf(h.palette[e]) != KeyOf(h.palette[255]);
	}

	Check(unique, "256 colours and a transparent pixel: 255 colour entries, the transparent one last (255), its colour used by no other");
	Check(holeRan && AllNearest(withHole, h), "... and every opaque pixel on a nearest entry");
}

static void TheTransparentColour()
{
	Picture hole(2, 1);
	hole.Set(0, 0, 1, 1, 1);
	hole.Set(1, 0, 255, 0, 255, 0);   // transparent, its own colour magenta: thrown away, not kept
	QuantizedImage q;
	Check(Quantize(hole, &q) && q.palette.size() == 2 && q.transparent == 1 && KeyOf(q.palette[1]) == 0xff00ff && q.indices[1] == 1,
		"a transparent pixel: magenta, the last entry");

	Picture magenta(2, 1);
	magenta.Set(0, 0, 255, 0, 255);
	magenta.Set(1, 0, 0, 0, 0, 0);
	Check(Quantize(magenta, &q) && q.transparent == 1 && KeyOf(q.palette[1]) == 0x00ffff, "an opaque magenta pixel: the transparent colour is cyan instead");

	Picture three(4, 1);
	three.Set(0, 0, 255, 0, 255);
	three.Set(1, 0, 0, 255, 255);
	three.Set(2, 0, 255, 255, 0);
	three.Set(3, 0, 0, 0, 0, 0);
	Check(Quantize(three, &q) && q.transparent == 3 && KeyOf(q.palette[3]) == 0x000000, "magenta, cyan and yellow all opaque: the first free colour from black");

	Picture four(5, 1);
	four.Set(0, 0, 255, 0, 255);
	four.Set(1, 0, 0, 255, 255);
	four.Set(2, 0, 255, 255, 0);
	four.Set(3, 0, 0, 0, 0);
	four.Set(4, 0, 0, 0, 0, 0);
	Check(Quantize(four, &q) && q.transparent == 4 && KeyOf(q.palette[4]) == 0x000001, "... and black too: the next one up, (0, 0, 1)");

	Picture empty(3, 2);   // every pixel alpha 0
	Check(Quantize(empty, &q) && q.palette.size() == 1 && q.transparent == 0 && q.coloursIn == 0 && q.indices == std::vector<uint8_t>(6, 0),
		"every pixel transparent: one entry, the transparent one");
}

static void TheAlpha()
{
	Picture alphas(6, 1);
	alphas.Set(0, 0, 50, 50, 50, 0);
	alphas.Set(1, 0, 50, 50, 50, 1);
	alphas.Set(2, 0, 50, 50, 50, 127);
	alphas.Set(3, 0, 50, 50, 50, 128);
	alphas.Set(4, 0, 50, 50, 50, 254);
	alphas.Set(5, 0, 50, 50, 50, 255);
	QuantizedImage q;
	bool const ran = Quantize(alphas, &q);

	Check(ran && q.indices[0] == q.transparent && q.indices[1] == q.transparent && q.indices[2] == q.transparent && q.indices[3] != q.transparent &&
		q.indices[4] != q.transparent && q.indices[5] != q.transparent, "alpha below 128 transparent, 128 and above opaque");
	Check(ran && q.partialAlpha == 4, "alpha neither 0 nor 255 counted (1, 127, 128, 254): " + std::to_string(q.partialAlpha));
}

static void TheTextureSize()
{
	// a 512 x 512 picture, the largest the fit makes, with tens of thousands of colours: reduced, and how long it took
	Picture big(512, 512);

	for (uint32_t y = 0; y < 512; y++)
	{
		for (uint32_t x = 0; x < 512; x++)
		{
			big.Set(x, y, x / 2, y / 2, (x * y) & 0xff);
		}
	}

	clock_t const start = clock();
	QuantizedImage q;
	bool const ran = Quantize(big, &q);
	long const ms = (long)((clock() - start) * 1000 / CLOCKS_PER_SEC);

	Check(ran && q.reduced && q.palette.size() == 256 && q.indices.size() == 512u * 512u, "512 x 512, " + std::to_string(q.coloursIn) + " colours: 256 entries, in " +
		std::to_string(ms) + " ms");
	Check(ran && ms < 5000, "... in under 5 seconds");
}

int main()
{
	TheRefusals();
	TheColoursThatFit();
	TheColoursThatDont();
	TheTransparentColour();
	TheAlpha();
	TheTextureSize();

	printf("checks %d; failed %d\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
