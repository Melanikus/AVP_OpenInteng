// image_fit.cpp: a picture made a square texture's size (image_fit.h says the ways and the resample). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "image_fit.h"

#include <math.h>

namespace model
{
	namespace
	{
		uint32_t const kMostSide = 4096;
		uint64_t const kMostPixels = 16ull * 1024 * 1024;

		// A picture in premultiplied doubles: blue, green, red (each times alpha / 255) and alpha
		struct Plane
		{
			uint32_t width = 0;
			uint32_t height = 0;
			std::vector<double> px;   // 4 a pixel
		};

		// One row's (or column's) weights: for each new pixel, the old pixels it takes and how much of each
		struct Tap
		{
			uint32_t from = 0;
			std::vector<double> weight;
		};

		// The weights that make `outLength` new pixels of the old pixels [begin, begin + span) of a line, the span a part of the old line (Crop cuts it)
		std::vector<Tap> Taps(double begin, double span, uint32_t oldLength, uint32_t outLength)
		{
			std::vector<Tap> taps(outLength);
			double const scale = span / outLength;   // old pixels for each new one

			for (uint32_t i = 0; i < outLength; i++)
			{
				Tap &tap = taps[i];

				if (scale >= 1.0)
				{
					// SHRINKING: the old pixels the new one covers, a part of one counting for its part
					double const a = begin + i * scale, b = a + scale;
					uint32_t first = (uint32_t)floor(a);
					uint32_t last = (uint32_t)ceil(b);

					last = last > oldLength ? oldLength : last;
					first = first >= last ? (last ? last - 1 : 0) : first;
					tap.from = first;

					for (uint32_t k = first; k < last; k++)
					{
						double const lo = k > a ? k : a, hi = k + 1 < b ? k + 1 : b;

						tap.weight.push_back(hi > lo ? (hi - lo) / scale : 0.0);
					}
				}
				else
				{
					// GROWING: between the two nearest old pixels (their centres at k + 0.5)
					double const centre = begin + (i + 0.5) * scale - 0.5;
					double const at = centre < 0 ? 0 : (centre > oldLength - 1 ? oldLength - 1 : centre);
					uint32_t const k = (uint32_t)floor(at);
					double const t = at - k;

					tap.from = k;
					tap.weight.push_back(1.0 - t);

					if (k + 1 < oldLength)
					{
						tap.weight.push_back(t);
					}
				}
			}

			return taps;
		}

		// The plane's columns [x0, x0 + spanX) and rows [y0, y0 + spanY) made outW x outH
		Plane Resample(Plane const &in, double x0, double spanX, double y0, double spanY, uint32_t outW, uint32_t outH)
		{
			std::vector<Tap> const across = Taps(x0, spanX, in.width, outW);
			std::vector<Tap> const down = Taps(y0, spanY, in.height, outH);
			Plane rows;

			// rows first: each old row made outW wide
			rows.width = outW;
			rows.height = in.height;
			rows.px.assign((size_t)outW * in.height * 4, 0.0);

			for (uint32_t y = 0; y < in.height; y++)
			{
				for (uint32_t x = 0; x < outW; x++)
				{
					Tap const &tap = across[x];
					double *const to = &rows.px[((size_t)y * outW + x) * 4];

					for (size_t k = 0; k < tap.weight.size(); k++)
					{
						double const *const from = &in.px[((size_t)y * in.width + tap.from + k) * 4];

						for (int c = 0; c < 4; c++)
						{
							to[c] += from[c] * tap.weight[k];
						}
					}
				}
			}

			// then the columns
			Plane out;

			out.width = outW;
			out.height = outH;
			out.px.assign((size_t)outW * outH * 4, 0.0);

			for (uint32_t y = 0; y < outH; y++)
			{
				Tap const &tap = down[y];

				for (uint32_t x = 0; x < outW; x++)
				{
					double *const to = &out.px[((size_t)y * outW + x) * 4];

					for (size_t k = 0; k < tap.weight.size(); k++)
					{
						double const *const from = &rows.px[((size_t)(tap.from + k) * outW + x) * 4];

						for (int c = 0; c < 4; c++)
						{
							to[c] += from[c] * tap.weight[k];
						}
					}
				}
			}

			return out;
		}

		uint8_t Byte(double value)
		{
			double const rounded = floor(value + 0.5);

			return (uint8_t)(rounded < 0 ? 0 : (rounded > 255 ? 255 : rounded));
		}
	}

	bool ImageFit_SideTaken(uint32_t side)
	{
		return side == 128 || side == 256 || side == 512;
	}

	bool ImageFit_Square(uint8_t const *bgra, uint32_t width, uint32_t height, uint32_t side, ImageFitMode mode, bool transparentBars, std::vector<uint8_t> *out,
		std::string *why)
	{
		std::string notSaid;

		why = why ? why : &notSaid;
		why->clear();

		if (!out)
		{
			*why = "no place for the texture";
			return false;
		}

		out->clear();

		if (!bgra || width == 0 || height == 0 || width > kMostSide || height > kMostSide || (uint64_t)width * height > kMostPixels)
		{
			*why = "the picture is " + std::to_string(width) + " x " + std::to_string(height) + ": each side 1 to 4096, at most 16777216 pixels";
			return false;
		}

		if (!ImageFit_SideTaken(side))
		{
			*why = "a texture is 128, 256 or 512 on a side, not " + std::to_string(side);
			return false;
		}

		// the picture in premultiplied doubles
		Plane in;

		in.width = width;
		in.height = height;
		in.px.resize((size_t)width * height * 4);

		for (size_t i = 0; i < (size_t)width * height; i++)
		{
			double const alpha = bgra[i * 4 + 3];

			for (int c = 0; c < 3; c++)
			{
				in.px[i * 4 + c] = bgra[i * 4 + c] * alpha / 255.0;
			}

			in.px[i * 4 + 3] = alpha;
		}

		// where the picture goes in the square, and which part of it
		uint32_t placeX = 0, placeY = 0, placeW = side, placeH = side;
		double fromX = 0, fromY = 0, spanX = width, spanY = height;

		if (mode == ImageFitMode::Fit)
		{
			// the longer side fills the square; the other keeps the shape, at least one pixel, in the middle
			if (width >= height)
			{
				placeH = (uint32_t)floor((double)side * height / width + 0.5);
				placeH = placeH < 1 ? 1 : placeH;
				placeY = (side - placeH) / 2;
			}
			else
			{
				placeW = (uint32_t)floor((double)side * width / height + 0.5);
				placeW = placeW < 1 ? 1 : placeW;
				placeX = (side - placeW) / 2;
			}
		}
		else if (mode == ImageFitMode::Crop)
		{
			// the shorter side fills the square; the longer one's middle is kept
			if (width >= height)
			{
				spanX = height;
				fromX = (width - (double)height) / 2;
			}
			else
			{
				spanY = width;
				fromY = (height - (double)width) / 2;
			}
		}

		Plane const placed = Resample(in, fromX, spanX, fromY, spanY, placeW, placeH);

		// the square: the bars (black and opaque, or see-through), then the picture over them
		out->assign((size_t)side * side * 4, 0);

		if (!transparentBars)
		{
			for (size_t i = 0; i < (size_t)side * side; i++)
			{
				(*out)[i * 4 + 3] = 255;
			}
		}

		for (uint32_t y = 0; y < placeH; y++)
		{
			for (uint32_t x = 0; x < placeW; x++)
			{
				double const *const p = &placed.px[((size_t)y * placeW + x) * 4];
				uint8_t *const to = &(*out)[((size_t)(placeY + y) * side + placeX + x) * 4];
				double const alpha = p[3];

				for (int c = 0; c < 3; c++)
				{
					to[c] = alpha > 0 ? Byte(p[c] * 255.0 / alpha) : 0;   // back from premultiplied
				}

				to[3] = Byte(alpha);
			}
		}

		return true;
	}
}
