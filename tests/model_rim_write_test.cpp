// The test for the RIM writer (src/model/rim_write.h): a paletted picture written in the stock form and read back through the GAME'S OWN reader (avpx's
// RimLoader, the one the editor and the game load pictures with), pixel for pixel. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng, the RIM
// import's step 5 (AvP-docs director/plan-rim-import.md). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//   (1) 128 x 128, 256 colours, no transparency: the first 88 bytes are a stock level texture's own, byte for byte (graphics\Envrnmts\GenShd1\Airvent01.RIM
//       in fastfile\Tex15.FFL: LIST, PROP with TRAN, FORM, BMHD, CMAP's heading), the size is its size (17,248), and RimLoader reads every pixel back as its
//       palette colour, fully opaque.
//   (2) 256 x 256 and 512 x 512, and the smallest, 16 x 16: read back the same way.
//   (3) a transparent colour: BMHD's masking 2 and its number; RimLoader reads exactly those pixels see-through.
//   (4) every refusal: a side under 16 or over 512, a width not a multiple of 16, the wrong count of colour numbers, no colours, 257 colours, a colour number
//       past the palette, a transparent number past it, a transparent colour another used colour shares; and nothing is given back.
// RED FIRST: on the stand-in writer (it writes nothing) every case fails.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <string>
#include <vector>

#include <windows.h>

#include "rim_write.h"
#include "RimLoader.h"
#include "FastFile.h"

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

// ---- What the game's FileStream asks of the rest of the game: these pictures are plain files, never in a fastfile
FastFileHandle *FF_Find(const std::string &)
{
	return 0;
}

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string const none;
	return none;
}

namespace Util
{
	void FtoBslash(std::string &text)
	{
		for (char &letter : text)
		{
			letter = letter == '/' ? '\\' : letter;
		}
	}
}

// A picture whose every pixel shows which pixel it is, from a palette of `colours` distinct colours
static RimPicture Pattern(uint32_t width, uint32_t height, size_t colours)
{
	RimPicture picture;

	picture.width = width;
	picture.height = height;

	for (size_t i = 0; i < colours; i++)
	{
		RimRgb rgb;

		rgb.red = (uint8_t)i;
		rgb.green = (uint8_t)(255 - i);
		rgb.blue = (uint8_t)((i * 7) & 255);
		picture.palette.push_back(rgb);
	}

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			picture.indices.push_back((uint8_t)((x * 3 + y * 5) % colours));
		}
	}

	return picture;
}

// The bytes saved to a file of the test's own, then opened and decoded by the game's RimLoader: true when every pixel is its palette colour, see-through
// (alpha 0) exactly where its number is the transparent one
static bool ReadsBack(RimPicture const &picture, std::vector<uint8_t> const &bytes, std::string const &file, std::string *said)
{
	{
		std::ofstream out(file, std::ios::binary | std::ios::trunc);

		out.write((char const *)bytes.data(), (std::streamsize)bytes.size());

		if (!out)
		{
			*said = "the test couldn't save the bytes";
			return false;
		}
	}

	RimLoader_NewLevel();

	RimLoader loader;

	if (!loader.Open(file))
	{
		*said = std::string("RimLoader refused it: ") + RimLoader_ProblemWords(loader.Problem());
		return false;
	}

	uint32_t width = 0, height = 0;

	loader.GetDimensions(width, height);

	if (width != picture.width || height != picture.height)
	{
		*said = "RimLoader read " + std::to_string(width) + " x " + std::to_string(height);
		return false;
	}

	std::vector<uint8_t> bgra((size_t)width * height * 4, 0xcd);

	if (!loader.Decode(bgra.data(), width * 4))
	{
		*said = std::string("RimLoader couldn't decode it: ") + RimLoader_ProblemWords(loader.Problem());
		return false;
	}

	for (uint32_t i = 0; i < width * height; i++)
	{
		RimRgb const &want = picture.palette[picture.indices[i]];
		uint8_t const alpha = (picture.transparent >= 0 && picture.indices[i] == (uint8_t)picture.transparent) ? 0 : 255;
		uint8_t const *got = &bgra[(size_t)i * 4];

		if (got[0] != want.blue || got[1] != want.green || got[2] != want.red || got[3] != alpha)
		{
			*said = "pixel " + std::to_string(i % width) + "," + std::to_string(i / width) + " differs";
			return false;
		}
	}

	*said = "every pixel its colour";
	return true;
}

static uint32_t Big32(std::vector<uint8_t> const &bytes, size_t at)
{
	return at + 4 <= bytes.size() ? ((uint32_t)bytes[at] << 24) | ((uint32_t)bytes[at + 1] << 16) | ((uint32_t)bytes[at + 2] << 8) | bytes[at + 3] : 0;
}

int main()
{
	std::string const folder = "model_rim_write_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), nullptr))
	{
		printf("  FAILED  the test's folder %s couldn't be made\n", folder.c_str());
		return 1;
	}

	std::string const file = folder + "\\picture.rim";
	std::vector<uint8_t> bytes;
	std::string why, said;

	// ---- (1) 128 x 128, 256 colours, against a stock level texture's own first 88 bytes (Airvent01.RIM, measured 2026-11-23)
	static uint8_t const kStockHead[88] = {
		'L', 'I', 'S', 'T', 0x00, 0x00, 0x43, 0x58, 'I', 'L', 'B', 'M',
		'P', 'R', 'O', 'P', 0x00, 0x00, 0x00, 0x14, 'I', 'L', 'B', 'M',
		'T', 'R', 'A', 'N', 0x00, 0x00, 0x00, 0x08, 0, 0, 0, 0, 0, 0, 0, 0,
		'F', 'O', 'R', 'M', 0x00, 0x00, 0x43, 0x30, 'I', 'L', 'B', 'M',
		'B', 'M', 'H', 'D', 0x00, 0x00, 0x00, 0x14, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x80, 0x00, 0x80,
		'C', 'M', 'A', 'P', 0x00, 0x00, 0x03, 0x00 };
	RimPicture const stock = Pattern(128, 128, 256);
	RimWriteResult const written = RimWrite_Stock(stock, &bytes, &why);

	Check(written == RimWriteResult::Written, "(1) 128 x 128 with 256 colours is written: " + why);
	Check(bytes.size() == 17248, "(1) its size is the stock texture's, 17,248 bytes (" + std::to_string(bytes.size()) + ")");
	Check(bytes.size() >= sizeof(kStockHead) && memcmp(bytes.data(), kStockHead, sizeof(kStockHead)) == 0,
		"(1) its first 88 bytes are Airvent01.RIM's own: LIST, PROP with TRAN, FORM, BMHD (128 x 128, 8 planes, masking 0), CMAP's heading");
	Check(Big32(bytes, 856) == 0x424f4459 && Big32(bytes, 860) == 16384, "(1) BODY at 856, 16,384 bytes, as the stock texture's");
	bool const stockBack = ReadsBack(stock, bytes, file, &said);   // (called before Check: MSVC builds the message before the condition)

	Check(stockBack, "(1) the game's RimLoader reads it back: " + said);

	// ---- (2) the other stock sizes, and the smallest
	for (uint32_t side : { 256u, 512u, 16u })
	{
		RimPicture const picture = Pattern(side, side, side == 16 ? 7 : 200);

		bool const back = RimWrite_Stock(picture, &bytes, &why) == RimWriteResult::Written && ReadsBack(picture, bytes, file, &said);

		Check(back, "(2) " + std::to_string(side) + " x " + std::to_string(side) + " written and read back: " + (why.empty() ? said : why));
	}

	// ---- (3) a transparent colour: masking 2, its number in BMHD, and exactly its pixels see-through
	RimPicture seeThrough = Pattern(128, 64, 10);

	seeThrough.palette[9] = RimRgb{ 255, 0, 255 };
	seeThrough.transparent = 9;
	RimWriteResult const seeThroughWritten = RimWrite_Stock(seeThrough, &bytes, &why);

	Check(seeThroughWritten == RimWriteResult::Written, "(3) a picture with a transparent colour is written: " + why);
	Check(bytes.size() > 76 && bytes[69] == 2 && bytes[72] == 0 && bytes[73] == 9, "(3) BMHD says masking 2 and transparent colour 9");
	bool const seeThroughBack = ReadsBack(seeThrough, bytes, file, &said);

	Check(seeThroughBack, "(3) RimLoader reads exactly colour 9's pixels see-through: " + said);

	// ---- (4) every refusal, with nothing given back
	auto refused = [&](RimPicture const &picture, RimWriteResult want, std::string const &what)
	{
		bytes.assign(5, 1);
		RimWriteResult const got = RimWrite_Stock(picture, &bytes, &why);

		Check(got == want && bytes.empty() && !why.empty(), "(4) " + what + ": " + RimWriteResultWords(got) + " (" + why + ")");
	};

	refused(Pattern(8, 8, 4), RimWriteResult::BadSize, "a side under 16");
	refused(Pattern(528, 16, 4), RimWriteResult::BadSize, "a side over 512");
	refused(Pattern(24, 16, 4), RimWriteResult::BadSize, "a width not a multiple of 16");

	RimPicture missing = Pattern(16, 16, 4);

	missing.indices.pop_back();
	refused(missing, RimWriteResult::BadSize, "one colour number short of width x height");

	RimPicture noColours = Pattern(16, 16, 4);

	noColours.palette.clear();
	refused(noColours, RimWriteResult::BadPalette, "no colours");

	RimPicture tooMany = Pattern(16, 16, 4);

	tooMany.palette.resize(257);
	refused(tooMany, RimWriteResult::BadPalette, "257 colours");

	RimPicture past = Pattern(16, 16, 4);

	past.indices[37] = 4;
	refused(past, RimWriteResult::BadIndex, "a pixel's colour number past the palette");

	RimPicture farTransparent = Pattern(16, 16, 4);

	farTransparent.transparent = 4;
	refused(farTransparent, RimWriteResult::BadTransparent, "a transparent number past the palette");

	RimPicture shared = Pattern(16, 16, 4);

	shared.palette[3] = shared.palette[1];
	shared.transparent = 3;
	refused(shared, RimWriteResult::BadTransparent, "a transparent colour that colour 1, used by pixels, shares (the game would make those see-through too)");

	DeleteFileA(file.c_str());
	RemoveDirectoryA(folder.c_str());
	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
