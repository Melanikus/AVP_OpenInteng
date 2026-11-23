// palette_quantize.cpp: a picture's colours as a RIM's palette, kept exactly or reduced by median cut (palette_quantize.h says what and why). Written 2026-11-23
// by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code (MIT): see License.txt, "AVP_OpenInteng's own code".
#include "palette_quantize.h"

#include <algorithm>

namespace model
{
	namespace
	{
		uint32_t const kOpaqueFrom = 128;   // alpha below this: transparent

		// One distinct opaque colour: its value (red << 16 | green << 8 | blue), how many pixels use it, the first pixel that does
		struct Colour
		{
			uint32_t key = 0;
			uint32_t count = 0;
			uint32_t first = 0;
		};

		uint32_t Channel(uint32_t key, int channel)
		{
			return (key >> (16 - 8 * channel)) & 0xff;   // 0 red, 1 green, 2 blue
		}

		// A box of colours for the median cut: colours [begin, end) of the working list, and its widest channel and that channel's span, measured once when the
		// box is made (a box's colours change only when it is split, and then it is two new boxes)
		struct Box
		{
			size_t begin = 0;
			size_t end = 0;
			int channel = 0;
			uint32_t span = 0;
		};

		// The box's widest channel and its span (0 when it holds one colour)
		void Widest(std::vector<Colour> const &colours, Box const &box, int *channel, uint32_t *span)
		{
			uint32_t low[3] = { 255, 255, 255 };
			uint32_t high[3] = { 0, 0, 0 };

			for (size_t k = box.begin; k < box.end; k++)
			{
				for (int c = 0; c < 3; c++)
				{
					uint32_t const v = Channel(colours[k].key, c);

					low[c] = v < low[c] ? v : low[c];
					high[c] = v > high[c] ? v : high[c];
				}
			}

			*channel = 0;
			*span = 0;

			for (int c = 0; c < 3; c++)
			{
				uint32_t const s = high[c] >= low[c] ? high[c] - low[c] : 0;

				if (s > *span)   // on a tie the earlier channel (red, then green, then blue)
				{
					*span = s;
					*channel = c;
				}
			}
		}

		uint32_t SquaredDistance(uint32_t key, PaletteRgb const &entry)
		{
			int32_t const dr = (int32_t)Channel(key, 0) - entry.r;
			int32_t const dg = (int32_t)Channel(key, 1) - entry.g;
			int32_t const db = (int32_t)Channel(key, 2) - entry.b;

			return (uint32_t)(dr * dr + dg * dg + db * db);
		}

		bool Used(std::vector<PaletteRgb> const &palette, uint32_t key)
		{
			for (PaletteRgb const &entry : palette)
			{
				if (((uint32_t)entry.r << 16 | (uint32_t)entry.g << 8 | entry.b) == key)
				{
					return true;
				}
			}

			return false;
		}

		PaletteRgb Rgb(uint32_t key)
		{
			PaletteRgb entry;

			entry.r = (uint8_t)Channel(key, 0);
			entry.g = (uint8_t)Channel(key, 1);
			entry.b = (uint8_t)Channel(key, 2);
			return entry;
		}
	}

	bool PaletteQuantize(uint8_t const *bgra, uint32_t width, uint32_t height, QuantizedImage *out, std::string *why)
	{
		std::string refused;

		if (!bgra || !out)
		{
			refused = "no picture was given";
		}
		else if (width == 0 || height == 0 || width > kPaletteQuantizeMostSide || height > kPaletteQuantizeMostSide)
		{
			refused = "the picture is " + std::to_string(width) + " x " + std::to_string(height) + ": each side must be 1 to " +
				std::to_string(kPaletteQuantizeMostSide);
		}
		else if ((uint64_t)width * height > kPaletteQuantizeMostPixels)
		{
			refused = "the picture has " + std::to_string((uint64_t)width * height) + " pixels: at most " + std::to_string(kPaletteQuantizeMostPixels) +
				" can be given a palette";
		}

		if (!refused.empty())
		{
			if (why)
			{
				*why = refused;
			}

			return false;
		}

		uint32_t const pixels = width * height;   // at most kPaletteQuantizeMostPixels: no overflow
		QuantizedImage result;

		// 1. EVERY PIXEL: transparent or not, its alpha counted when partial; each opaque one's colour and place, sorted, so the distinct colours, their counts
		// and their first places come out of one sort (no hash order anywhere)
		std::vector<uint64_t> seen;   // colour << 32 | pixel's place
		bool anyTransparent = false;

		seen.reserve(pixels);

		for (uint32_t p = 0; p < pixels; p++)
		{
			uint8_t const *const pixel = bgra + (size_t)p * 4;
			uint32_t const alpha = pixel[3];

			result.partialAlpha += alpha != 0 && alpha != 255;

			if (alpha < kOpaqueFrom)
			{
				anyTransparent = true;
				continue;
			}

			uint32_t const key = (uint32_t)pixel[2] << 16 | (uint32_t)pixel[1] << 8 | pixel[0];

			seen.push_back((uint64_t)key << 32 | p);
		}

		std::sort(seen.begin(), seen.end());

		std::vector<Colour> colours;   // distinct, in the colours' own order (by value)

		for (size_t k = 0; k < seen.size();)
		{
			Colour colour;

			colour.key = (uint32_t)(seen[k] >> 32);
			colour.first = (uint32_t)seen[k];   // the smallest place, as the places sort within a colour

			while (k < seen.size() && (uint32_t)(seen[k] >> 32) == colour.key)
			{
				colour.count++;
				k++;
			}

			colours.push_back(colour);
		}

		seen.clear();
		seen.shrink_to_fit();

		size_t const room = anyTransparent ? 255 : 256;
		std::vector<int32_t> entryOf(colours.size(), 0);   // each distinct colour's palette entry, in `colours`' order

		result.coloursIn = colours.size();
		result.reduced = colours.size() > room;

		if (!result.reduced)
		{
			// 2. THEY FIT: exactly these colours, in the order they first appear
			std::vector<size_t> order(colours.size());

			for (size_t k = 0; k < order.size(); k++)
			{
				order[k] = k;
			}

			std::sort(order.begin(), order.end(), [&colours](size_t a, size_t b) { return colours[a].first < colours[b].first; });

			for (size_t place = 0; place < order.size(); place++)
			{
				result.palette.push_back(Rgb(colours[order[place]].key));
				entryOf[order[place]] = (int32_t)place;
			}
		}
		else
		{
			// 3. MEDIAN CUT, on a working copy whose boxes are kept as ranges of it
			std::vector<Colour> work = colours;
			std::vector<Box> boxes(1);

			boxes[0].end = work.size();
			Widest(work, boxes[0], &boxes[0].channel, &boxes[0].span);

			while (boxes.size() < room)
			{
				size_t widest = boxes.size();
				uint32_t widestSpan = 0;

				for (size_t k = 0; k < boxes.size(); k++)
				{
					if (boxes[k].span > widestSpan)   // on a tie the earlier box; a box of one colour spans 0 and never splits
					{
						widest = k;
						widestSpan = boxes[k].span;
					}
				}

				if (widest == boxes.size())
				{
					break;   // no box holds two colours
				}

				Box const box = boxes[widest];
				int const channel = box.channel;

				// the box's colours by that channel, then by value, so equal channels keep one order everywhere
				std::sort(work.begin() + (ptrdiff_t)box.begin, work.begin() + (ptrdiff_t)box.end, [channel](Colour const &a, Colour const &b)
				{
					uint32_t const ca = Channel(a.key, channel);
					uint32_t const cb = Channel(b.key, channel);

					return ca != cb ? ca < cb : a.key < b.key;
				});

				uint64_t total = 0;

				for (size_t k = box.begin; k < box.end; k++)
				{
					total += work[k].count;
				}

				// the pixel-count median: the first place where the colours before it hold half the box's pixels; both halves keep at least one colour
				uint64_t before = 0;
				size_t split = box.begin + 1;

				for (size_t k = box.begin; k < box.end; k++)
				{
					before += work[k].count;

					if (before * 2 >= total)
					{
						split = k + 1;
						break;
					}
				}

				split = split <= box.begin ? box.begin + 1 : (split >= box.end ? box.end - 1 : split);

				Box left = box;
				Box right = box;

				left.end = split;
				right.begin = split;
				Widest(work, left, &left.channel, &left.span);
				Widest(work, right, &right.channel, &right.span);
				boxes[widest] = left;
				boxes.insert(boxes.begin() + (ptrdiff_t)widest + 1, right);
			}

			// each box's pixel-weighted mean, rounded
			for (Box const &box : boxes)
			{
				uint64_t sum[3] = { 0, 0, 0 };
				uint64_t total = 0;

				for (size_t k = box.begin; k < box.end; k++)
				{
					for (int c = 0; c < 3; c++)
					{
						sum[c] += (uint64_t)Channel(work[k].key, c) * work[k].count;
					}

					total += work[k].count;
				}

				PaletteRgb entry;

				entry.r = (uint8_t)((sum[0] + total / 2) / total);
				entry.g = (uint8_t)((sum[1] + total / 2) / total);
				entry.b = (uint8_t)((sum[2] + total / 2) / total);
				result.palette.push_back(entry);
			}

			// each colour to its nearest entry (on a tie, the lower one)
			for (size_t k = 0; k < colours.size(); k++)
			{
				uint32_t best = UINT32_MAX;

				for (size_t e = 0; e < result.palette.size(); e++)
				{
					uint32_t const distance = SquaredDistance(colours[k].key, result.palette[e]);

					if (distance < best)
					{
						best = distance;
						entryOf[k] = (int32_t)e;
					}
				}
			}
		}

		// 4. THE TRANSPARENT COLOUR, last, by a red, green and blue no opaque entry uses (at most 255 opaque entries, so one of 256 tries is always free)
		if (anyTransparent)
		{
			uint32_t const named[] = { 0xff00ffu, 0x00ffffu, 0xffff00u };
			uint32_t pick = UINT32_MAX;

			for (uint32_t const key : named)
			{
				if (!Used(result.palette, key))
				{
					pick = key;
					break;
				}
			}

			for (uint32_t key = 0; pick == UINT32_MAX && key <= 0xffffffu; key++)
			{
				pick = Used(result.palette, key) ? UINT32_MAX : key;
			}

			result.transparent = (int32_t)result.palette.size();
			result.palette.push_back(Rgb(pick));
		}

		// every pixel's entry: a transparent one the transparent entry; an opaque one its colour's (found by the colour's value in the sorted list)
		result.indices.resize(pixels);

		for (uint32_t p = 0; p < pixels; p++)
		{
			uint8_t const *const pixel = bgra + (size_t)p * 4;

			if (pixel[3] < kOpaqueFrom)
			{
				result.indices[p] = (uint8_t)result.transparent;
				continue;
			}

			uint32_t const key = (uint32_t)pixel[2] << 16 | (uint32_t)pixel[1] << 8 | pixel[0];
			auto const at = std::lower_bound(colours.begin(), colours.end(), key, [](Colour const &c, uint32_t value) { return c.key < value; });

			result.indices[p] = (uint8_t)entryOf[(size_t)(at - colours.begin())];
		}

		*out = std::move(result);
		return true;
	}
}
