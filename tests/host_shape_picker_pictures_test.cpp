// The test for a placement picker tile's pictures read as the level's own are (src/host/shape_picker_pictures.cpp). Written 2026-11-23 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It runs on the stock copy's data folder (the folder above {LEVEL:Battle}'s
// avp_rifs), its pictures read from the game's own fastfile archives by the game's own FileStream and RimLoader, as the editor's level open reads them:
//   (1) a stock picture (Common\Pikup.rim, Tex1.FFL's graphics/common/pikup.rim) is read and decoded: its size, 4 bytes a pixel, its pixels not all one
//   (2) NEVER READ: a picture with no path (Frost on ec8a4d0e: not even "graphics/" alone), one whose path shape_thumb refused, and one the level open's rule refuses
//       even if the caller didn't say so; and none at all while the picker holds no allowance
//   (3) a file that isn't there: RimLoader's words, nothing given
//   (4) THE VISIT'S ONE ALLOWANCE (the game's RimLoaderToolAllowance, Melanikus/AvP #293): its pictures counted apart from the level's, and once used up the rest
//       refused with words; when the visit ends the level's own count is exactly as it was
//   (5) A TILE'S PICTURES TOGETHER at most kShapePickerMostTileBytes: one that would take it past is refused, the tile's count unchanged
// RED FIRST: on the stand-in that read no picture (04c140f6's: "waiting for #293"), (1), (4) and (5) fail.
#include <windows.h>

#include <algorithm>
#include <stdio.h>
#include <string>

#include "FastFile.h"
#include "RimLoader.h"
#include "shape_picker_screen.h"
#include "shape_thumb.h"
#include "shape_thumb_render.h"

// THE GAME'S UTILITIES AND CONSOLE the archive reader calls (avpx 3dc/utilities.cpp and console.cpp bring in the whole game), the three it needs as they are there
namespace Util
{
	void FtoBslash(std::string &theString)
	{
		std::replace(theString.begin(), theString.end(), '/', '\\');
	}

	int LowercaseAndChangeSlash(char c)
	{
		return (c >= 'A' && c <= 'Z') ? c + 32 : (c == '\\' ? '/' : c);
	}

	std::string IntToString(const int value)
	{
		return std::to_string(value);
	}
}

void Con_PrintError(const std::string &)
{
}

void Con_PrintMessage(const std::string &)
{
}

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static host::ShapeThumbPicture Picture(char const *name, char const *path, char const *refused = "")
{
	host::ShapeThumbPicture p;
	p.name = name;
	p.path = path;
	p.refused = refused;
	return p;
}

int main(int argc, char **argv)
{
	printf("host_shape_picker_pictures_test: a tile's pictures read as the level's own are\n");

	// the stock copy's data folder: the folder above the level's avp_rifs (the game's own paths, "graphics/...", are under it, its archives in fastfile)
	std::string const level = argc > 1 ? argv[1] : "";
	size_t const slash = level.find_last_of("\\/");
	size_t const above = slash == std::string::npos || slash == 0 ? std::string::npos : level.find_last_of("\\/", slash - 1);

	if (above == std::string::npos || !SetCurrentDirectoryA(level.substr(0, above).c_str()) || !FF_Init())
	{
		printf("  FAILED  give a stock level ({LEVEL:Battle}): its data folder and its fastfile archives are where the pictures are read from\n");
		return 1;
	}

	host::ShapeThumbTexture got;
	std::string why;
	uint64_t tile = 0;

	// THE OPEN LEVEL'S OWN COUNT, made something (one picture opened as the level opens its own), so "exactly as it was" means more than 0. (Each read below is made
	// BEFORE its Check: MSVC builds a check's words before its condition, so a call inside it would show the call before's words.)
	RimLoader_NewLevel();
	{
		RimLoader levelsOwn;
		levelsOwn.Open("graphics/Common/Pikup.rim");
	}

	uint64_t const levelBefore = RimLoader_BytesThisLevel();

	// ---- (2b) none at all while no allowance is held
	bool const closed = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &got, &why, &tile);
	Check(levelBefore > 0 && !closed && why.find("isn't open") != std::string::npos, "no picture is read while the picker holds no allowance (" + why + ")");

	ShapePickerPictures_Begin();

	// ---- (1) a stock picture, from the game's own archive
	bool const read = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &got, &why, &tile);
	bool varied = false;

	for (size_t i = 4; read && i < got.bgra.size(); i += 4)
	{
		varied = varied || got.bgra[i] != got.bgra[0] || got.bgra[i + 1] != got.bgra[1] || got.bgra[i + 2] != got.bgra[2];
	}

	Check(read && got.width > 0 && got.height > 0 && got.bgra.size() == (size_t)got.width * got.height * 4 && varied && tile == got.bgra.size(),
		"(1) Common\\Pikup.rim is read from the archive and decoded: " + std::to_string(got.width) + " x " + std::to_string(got.height) + ", its pixels not all one, the tile's count " +
		std::to_string((unsigned long long)tile) + " (" + why + ")");
	Check(RimLoader_BytesThisLevel() > 0, "and it is counted on the picker's own allowance");

	// ---- (2) never read
	tile = 0;
	uint64_t const spentBefore = RimLoader_BytesThisLevel();
	bool const noPath = ShapePickerScreen_ReadPicture(Picture("Nothing.rim", ""), &got, &why, &tile);
	Check(!noPath && why == "it names no file" && got.bgra.empty() && tile == 0, "(2) a picture with no path: never read, not even as \"graphics/\" alone (" + why + ")");
	bool const refusedBefore = ShapePickerScreen_ReadPicture(Picture("..\\x.rim", "", "a path may not go up a folder"), &got, &why, &tile);
	Check(!refusedBefore && why.find("refused") != std::string::npos, "(2) one whose path shape_thumb refused: never read (" + why + ")");
	bool const refusedHere = ShapePickerScreen_ReadPicture(Picture("x.rim", "..\\..\\Windows\\win.rim"), &got, &why, &tile);
	Check(!refusedHere && why.find("refused") != std::string::npos, "(2) one the level open's rule refuses, though its caller didn't say so: refused here again (" + why + ")");
	Check(RimLoader_BytesThisLevel() == spentBefore, "(2) and none of them opened a file: nothing was counted");

	// ---- (3) not there
	bool const missing = ShapePickerScreen_ReadPicture(Picture("Common\\NotThere.rim", "Common\\NotThere.rim"), &got, &why, &tile);
	Check(!missing && why == RimLoader_ProblemWords(kRimNoFile) && got.bgra.empty(), "(3) a file that isn't there: RimLoader's own words (" + why + ")");

	// ---- (5) a tile's pictures together
	tile = kShapePickerMostTileBytes - 100;
	bool const past = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &got, &why, &tile);
	Check(!past && why.find("more than one tile's pictures may hold") != std::string::npos && tile == kShapePickerMostTileBytes - 100 && got.bgra.empty(),
		"(5) a picture that would take its tile past " + std::to_string((unsigned long long)(kShapePickerMostTileBytes >> 20)) + " MB: refused, the tile's count unchanged (" + why + ")");

	// ---- (4) the visit's one allowance: read until it is used up (with a large stock picture, the loading screen's, so it takes a few hundred reads, not 4,096)
	int reads = 0;
	bool usedUp = false;
	uint32_t largeWidth = 0, largeHeight = 0;

	while (reads < 5000 && !usedUp)
	{
		uint64_t one = 0;

		if (ShapePickerScreen_ReadPicture(Picture("Menus\\Loading.rim", "Menus\\Loading.rim"), &got, &why, &one))
		{
			reads++;
			largeWidth = got.width;
			largeHeight = got.height;
		}
		else
		{
			usedUp = why.find("allowance is used up") != std::string::npos;
			break;
		}
	}

	uint64_t const largeBytes = (uint64_t)largeWidth * largeHeight * 4;
	uint64_t const fits = largeBytes ? (kMostRimBytesPerLevel - 2 * kLeastRimChargeBytes) / largeBytes : 0;   // after (1)'s picture and the refused ones' nothing

	Check(usedUp && reads > 10 && (uint64_t)reads >= fits - 1 && (uint64_t)reads <= fits + 1, "(4) the visit's allowance: " + std::to_string(reads) + " more pictures of " +
		std::to_string(largeWidth) + " x " + std::to_string(largeHeight) + " (about " + std::to_string((unsigned long long)fits) + " fit in 256 MB), then the rest refused, saying so (" + why + ")");

	ShapePickerPictures_End();
	Check(RimLoader_BytesThisLevel() == levelBefore, "(4) when the visit ends, the level's own count is exactly as it was (" + std::to_string((unsigned long long)levelBefore) + ")");

	// a new visit has its own allowance again
	ShapePickerPictures_Begin();

	uint64_t fresh = 0;
	bool const again = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &got, &why, &fresh);

	Check(again && RimLoader_BytesThisLevel() >= kLeastRimChargeBytes && RimLoader_BytesThisLevel() < 2 * kLeastRimChargeBytes + fresh,
		"(4) a new visit has its own allowance again: a picture is read, counted from nothing (" + std::to_string((unsigned long long)RimLoader_BytesThisLevel()) + ")");
	ShapePickerPictures_End();
	Check(RimLoader_BytesThisLevel() == levelBefore, "(4) and its end puts the level's count back too");

	printf(checksFailed ? "host_shape_picker_pictures_test: %d FAILED\n" : "host_shape_picker_pictures_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
