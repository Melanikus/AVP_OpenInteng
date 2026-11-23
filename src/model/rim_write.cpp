// rim_write.cpp: a paletted picture written as a RIM in the stock form (rim_write.h says the form, its limits and why). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "rim_write.h"

namespace model
{
	namespace
	{
		uint32_t const kPlanes = 8;          // every stock level texture's (a colour number of 8 bits: 256 colours)
		uint32_t const kPaletteColours = 256;   // CMAP always holds 256, as the stock ones do

		void Put32(std::vector<uint8_t> &bytes, uint32_t value)
		{
			bytes.push_back((uint8_t)(value >> 24));
			bytes.push_back((uint8_t)(value >> 16));
			bytes.push_back((uint8_t)(value >> 8));
			bytes.push_back((uint8_t)value);
		}

		void Put16(std::vector<uint8_t> &bytes, uint32_t value)
		{
			bytes.push_back((uint8_t)(value >> 8));
			bytes.push_back((uint8_t)value);
		}

		void PutId(std::vector<uint8_t> &bytes, char const (&id)[5])
		{
			bytes.insert(bytes.end(), id, id + 4);
		}

		// A chunk's size, written where its heading put a 0 once what it holds is all there (at: the size's own place)
		void SetSizeAt(std::vector<uint8_t> &bytes, size_t at)
		{
			uint32_t const size = (uint32_t)(bytes.size() - (at + 4));

			bytes[at] = (uint8_t)(size >> 24);
			bytes[at + 1] = (uint8_t)(size >> 16);
			bytes[at + 2] = (uint8_t)(size >> 8);
			bytes[at + 3] = (uint8_t)size;
		}

		bool SameRgb(RimRgb const &a, RimRgb const &b)
		{
			return a.red == b.red && a.green == b.green && a.blue == b.blue;
		}

		RimWriteResult Refuse(RimWriteResult result, std::string const &words, std::vector<uint8_t> *bytes, std::string *why)
		{
			bytes->clear();
			*why = words;
			return result;
		}
	}

	RimWriteResult RimWrite_Stock(RimPicture const &picture, std::vector<uint8_t> *bytes, std::string *why)
	{
		std::vector<uint8_t> notWanted;
		std::string notSaid;

		bytes = bytes ? bytes : &notWanted;
		why = why ? why : &notSaid;
		bytes->clear();
		why->clear();

		uint32_t const width = picture.width, height = picture.height;

		// ---- the picture is held to the limits first: nothing is made of one that isn't
		if (width < kRimLeastSide || height < kRimLeastSide || width > kRimMostSide || height > kRimMostSide || width % 16 != 0)
		{
			return Refuse(RimWriteResult::BadSize, std::to_string(width) + " x " + std::to_string(height) + ": each side 16 to 512, the width a multiple of 16",
				bytes, why);
		}

		if (picture.indices.size() != (size_t)width * height)
		{
			return Refuse(RimWriteResult::BadSize, std::to_string(picture.indices.size()) + " colour numbers for " + std::to_string((size_t)width * height) +
				" pixels", bytes, why);
		}

		size_t const colours = picture.palette.size();

		if (colours == 0 || colours > kPaletteColours)
		{
			return Refuse(RimWriteResult::BadPalette, std::to_string(colours) + " colours: 1 to 256", bytes, why);
		}

		// which colours a pixel uses (the transparent colour must differ from every one of them but itself)
		std::vector<bool> used(colours, false);

		for (size_t i = 0; i < picture.indices.size(); i++)
		{
			if (picture.indices[i] >= colours)
			{
				return Refuse(RimWriteResult::BadIndex, "pixel " + std::to_string(i % width) + "," + std::to_string(i / width) + " uses colour " +
					std::to_string(picture.indices[i]) + " of " + std::to_string(colours), bytes, why);
			}

			used[picture.indices[i]] = true;
		}

		if (picture.transparent >= 0)
		{
			if ((size_t)picture.transparent >= colours)
			{
				return Refuse(RimWriteResult::BadTransparent, "the transparent colour " + std::to_string(picture.transparent) + " of " + std::to_string(colours),
					bytes, why);
			}

			for (size_t i = 0; i < colours; i++)
			{
				if ((int32_t)i != picture.transparent && used[i] && SameRgb(picture.palette[i], picture.palette[(size_t)picture.transparent]))
				{
					return Refuse(RimWriteResult::BadTransparent, "the transparent colour " + std::to_string(picture.transparent) + " has the same red, green and "
						"blue as colour " + std::to_string(i) + ", which pixels use: the game would draw those see-through too", bytes, why);
				}
			}
		}
		else if (picture.transparent != -1)
		{
			return Refuse(RimWriteResult::BadTransparent, "the transparent colour " + std::to_string(picture.transparent), bytes, why);
		}

		// ---- the bytes, in the stock form
		uint32_t const rowBytes = width / 8;   // one plane's row: 1 bit a pixel (a multiple of 2, as the width is of 16)

		bytes->reserve(100 + kPaletteColours * 3 + (size_t)rowBytes * kPlanes * height);
		PutId(*bytes, "LIST");
		size_t const listSize = bytes->size();
		Put32(*bytes, 0);
		PutId(*bytes, "ILBM");

		// PROP ILBM { TRAN 8 <zeros> }: every stock level texture's, as the game's own tools wrote them (the game's reader steps over it)
		PutId(*bytes, "PROP");
		Put32(*bytes, 20);
		PutId(*bytes, "ILBM");
		PutId(*bytes, "TRAN");
		Put32(*bytes, 8);
		Put32(*bytes, 0);
		Put32(*bytes, 0);

		PutId(*bytes, "FORM");
		size_t const formSize = bytes->size();
		Put32(*bytes, 0);
		PutId(*bytes, "ILBM");

		bool const seeThrough = picture.transparent >= 0;

		PutId(*bytes, "BMHD");
		Put32(*bytes, 20);
		Put16(*bytes, width);
		Put16(*bytes, height);
		Put16(*bytes, 0);                       // x
		Put16(*bytes, 0);                       // y
		bytes->push_back((uint8_t)kPlanes);
		bytes->push_back(seeThrough ? 2 : 0);   // masking: 2 = one colour drawn see-through
		bytes->push_back(0);                    // compression: none
		bytes->push_back(0);                    // (pad)
		Put16(*bytes, seeThrough ? (uint32_t)picture.transparent : 0);
		bytes->push_back(1);                    // the pixels' aspect, 1:1
		bytes->push_back(1);
		Put16(*bytes, width);                   // the page: the picture's own size
		Put16(*bytes, height);

		PutId(*bytes, "CMAP");
		Put32(*bytes, kPaletteColours * 3);

		for (size_t i = 0; i < kPaletteColours; i++)
		{
			RimRgb const rgb = i < colours ? picture.palette[i] : RimRgb();   // the unused ones black: no pixel uses them

			bytes->push_back(rgb.red);
			bytes->push_back(rgb.green);
			bytes->push_back(rgb.blue);
		}

		// BODY: row by row, top first; in each row the 8 planes, lowest first; in each plane's row a bit a pixel, the leftmost in a byte's top bit, the bit
		// being the plane's bit of the pixel's colour number (as the game's RimLoader reads it: rows of (width + 7) / 8 bytes, 128 >> (x % 8))
		PutId(*bytes, "BODY");
		Put32(*bytes, rowBytes * kPlanes * height);

		for (uint32_t y = 0; y < height; y++)
		{
			uint8_t const *const row = &picture.indices[(size_t)y * width];

			for (uint32_t plane = 0; plane < kPlanes; plane++)
			{
				for (uint32_t byte = 0; byte < rowBytes; byte++)
				{
					uint8_t bits = 0;

					for (uint32_t bit = 0; bit < 8; bit++)
					{
						bits |= (uint8_t)(((row[byte * 8 + bit] >> plane) & 1) << (7 - bit));
					}

					bytes->push_back(bits);
				}
			}
		}

		// (BODY's size is even: the width is a multiple of 16. So no pad byte, and the sizes are now known)
		SetSizeAt(*bytes, formSize);
		SetSizeAt(*bytes, listSize);
		return RimWriteResult::Written;
	}

	char const *RimWriteResultWords(RimWriteResult result)
	{
		switch (result)
		{
		case RimWriteResult::Written: return "written";
		case RimWriteResult::BadSize: return "the picture's size can't be a texture";
		case RimWriteResult::BadPalette: return "the picture's colours can't be a texture's palette";
		case RimWriteResult::BadIndex: return "a pixel's colour isn't in the palette";
		case RimWriteResult::BadTransparent: return "the transparent colour can't be used";
		}

		return "not written";
	}
}
