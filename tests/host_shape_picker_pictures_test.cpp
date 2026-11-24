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
//   (6) THE SITTING'S KEPT PICTURES (Frost, 2026-11-23, the owner's "cache every texture once"): a picture asked for again is given from the kept ones, the same
//       pixels, decoded once, nothing more counted on the allowance, also when its name is written in other letters and when the picker is opened again; a file
//       that isn't there is never kept; past the most kept the least recently given is let go and decoded again when next asked for; one larger than the most
//       alone is never kept. ((4) lets every kept picture go before each of its reads, so it still spends the allowance.)
//   (7) THE SAME PATH LEADING TO ANOTHER FILE (Vasquez's MUST on 578f59d9), in a scratch data folder of the test's own: a kept picture's file written again with
//       other pixels at another size, then the same bytes saved again with only the time last written moved: each time the new file is decoded, never the kept
//       pixels given. MUTATION-PROVED: with the kept picture given without comparing the files, (7) fails.
// RED FIRST: on the stand-in that read no picture (04c140f6's: "waiting for #293"), (1), (4) and (5) fail. On the stand-in that keeps nothing (the cache's red),
// (6)'s decoded-once, opened-again and most-kept checks fail.
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

// A stock picture's own bytes, as the game's archive holds them (read through the game's FileStream, from the data folder the test stands in)
static std::vector<uint8_t> StockBytes(char const *path)
{
	FileStream file;
	std::vector<uint8_t> bytes;

	if (file.Open(path, FileStream::FileRead) && file.IsGood())
	{
		bytes.assign(file.GetFileSize(), 0);

		if (bytes.empty() || file.ReadBytes(bytes.data(), bytes.size()) != bytes.size())
		{
			bytes.clear();
		}
	}

	return bytes;
}

// A file written whole, in place (an existing one is opened again and cut to nothing first, so it stays the same file on the disk)
static bool WriteWhole(std::string const &path, std::vector<uint8_t> const &bytes)
{
	FILE *file = fopen(path.c_str(), "wb");
	bool const written = file && !bytes.empty() && fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	if (file)
	{
		fclose(file);
	}

	return written;
}

// The time a file was last written, set an hour earlier than it says now (so a check never rests on how fast the clock ticks)
static bool TimeAnHourBack(std::string const &path)
{
	HANDLE const touch = CreateFileA(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	FILETIME written = {};
	bool set = false;

	if (touch != INVALID_HANDLE_VALUE)
	{
		if (GetFileTime(touch, NULL, NULL, &written))
		{
			uint64_t const earlier = ((uint64_t)written.dwHighDateTime << 32 | written.dwLowDateTime) - 36000000000ull;   // 100-nanosecond steps
			written.dwLowDateTime = (DWORD)earlier;
			written.dwHighDateTime = (DWORD)(earlier >> 32);
			set = SetFileTime(touch, NULL, NULL, &written) != 0;
		}

		CloseHandle(touch);
	}

	return set;
}

static std::string CountWords(ShapePickerPictureCounts const &c)
{
	return "decoded " + std::to_string((unsigned long long)c.decoded) + ", from the kept " + std::to_string((unsigned long long)c.fromKept) + ", kept " +
		std::to_string((unsigned long long)c.kept) + " of " + std::to_string((unsigned long long)c.keptBytes) + " bytes, let go " + std::to_string((unsigned long long)c.letGo);
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

	host::ShapeThumbTexture const pickup = got;   // (6) compares with it

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

		ShapePickerPictures_Forget();   // (the kept one let go, so each read is a whole read again, counted on the allowance)

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
	ShapePickerPictures_Forget();   // (as in the loop: a whole read)
	bool const again = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &got, &why, &fresh);

	Check(again && RimLoader_BytesThisLevel() >= kLeastRimChargeBytes && RimLoader_BytesThisLevel() < 2 * kLeastRimChargeBytes + fresh,
		"(4) a new visit has its own allowance again: a picture is read, counted from nothing (" + std::to_string((unsigned long long)RimLoader_BytesThisLevel()) + ")");
	ShapePickerPictures_End();
	Check(RimLoader_BytesThisLevel() == levelBefore, "(4) and its end puts the level's count back too");

	// ---- (6) the sitting's kept pictures: decoded once, then given from the kept ones while the path leads to the very same file
	ShapePickerPictures_Begin();
	ShapePickerPictures_Forget();
	why.clear();   // (a read that is given says no why: else (4)'s last words would show here)

	host::ShapeThumbTexture once, twice, otherLetters;
	uint64_t tileOnce = 0, tileTwice = 0, tileOther = 0;
	bool const readOnce = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &once, &why, &tileOnce);
	uint64_t const spentOnce = RimLoader_BytesThisLevel();
	bool const readTwice = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &twice, &why, &tileTwice);
	bool const readOther = ShapePickerScreen_ReadPicture(Picture("common\\PIKUP.RIM", "common\\PIKUP.RIM"), &otherLetters, &why, &tileOther);
	ShapePickerPictureCounts const asked = ShapePickerPictures_Counts();

	Check(readOnce && readTwice && readOther && once.bgra == pickup.bgra && twice.width == once.width && twice.height == once.height && twice.bgra == once.bgra &&
		otherLetters.bgra == once.bgra && tileTwice == tileOnce && tileOther == tileOnce && tileOnce == once.bgra.size(),
		"(6) a picture asked for again, and under its name in other letters: the same pixels, and its tile counts the same bytes (" + why + ")");
	Check(asked.decoded == 1 && asked.fromKept == 2 && asked.kept == 1 && asked.keptBytes == once.bgra.size() && RimLoader_BytesThisLevel() == spentOnce,
		"(6) decoded ONCE, given from the kept ones the other two times, nothing more counted on the visit's allowance (" + CountWords(asked) + ")");

	ShapePickerPictures_End();
	ShapePickerPictures_Begin();

	host::ShapeThumbTexture nextVisit;
	uint64_t tileNext = 0;
	bool const readNext = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &nextVisit, &why, &tileNext);
	ShapePickerPictureCounts const later = ShapePickerPictures_Counts();

	Check(readNext && nextVisit.bgra == once.bgra && later.decoded == 1 && later.fromKept == 3 && RimLoader_BytesThisLevel() == 0,
		"(6) kept for the sitting: with the picker opened again it is given from the kept ones, the new visit's allowance untouched (" + CountWords(later) + ", " +
		std::to_string((unsigned long long)RimLoader_BytesThisLevel()) + " bytes counted)");

	host::ShapeThumbTexture absent;
	uint64_t tileAbsent = 0;
	bool const readAbsent = ShapePickerScreen_ReadPicture(Picture("Common\\NotThere.rim", "Common\\NotThere.rim"), &absent, &why, &tileAbsent);
	ShapePickerPictureCounts const afterAbsent = ShapePickerPictures_Counts();

	Check(!readAbsent && why == RimLoader_ProblemWords(kRimNoFile) && afterAbsent.kept == 1 && afterAbsent.decoded == 1 && tileAbsent == 0,
		"(6) a file that isn't there: RimLoader's own words, and nothing kept (" + why + "; " + CountWords(afterAbsent) + ")");

	// THE MOST KEPT, made room for the loading screen's picture but not the pickup's too: each read lets the other go, the least recently given first
	uint64_t const twoLess = largeBytes + once.bgra.size() - 1;
	ShapePickerPictures_Forget(twoLess);

	host::ShapeThumbTexture a, b, c;
	uint64_t ta = 0, tb = 0, tc = 0;
	bool const ra = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &a, &why, &ta);
	bool const rb = ShapePickerScreen_ReadPicture(Picture("Menus\\Loading.rim", "Menus\\Loading.rim"), &b, &why, &tb);
	bool const rc = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &c, &why, &tc);
	ShapePickerPictureCounts const most = ShapePickerPictures_Counts();

	Check(ra && rb && rc && c.bgra == once.bgra && most.decoded == 3 && most.fromKept == 0 && most.letGo == 2 && most.kept == 1 && most.keptBytes == once.bgra.size() &&
		most.keptBytes <= twoLess, "(6) past the most kept (" + std::to_string((unsigned long long)twoLess) + " bytes here): the least recently given let go, and decoded again when asked for (" +
		CountWords(most) + ")");

	// one larger than the most on its own is never kept
	ShapePickerPictures_Forget(once.bgra.size() - 1);

	host::ShapeThumbTexture d, e;
	uint64_t td = 0, te = 0;
	bool const rd = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &d, &why, &td);
	bool const re = ShapePickerScreen_ReadPicture(Picture("Common\\Pikup.rim", "Common\\Pikup.rim"), &e, &why, &te);
	ShapePickerPictureCounts const tooLarge = ShapePickerPictures_Counts();

	Check(rd && re && e.bgra == once.bgra && tooLarge.decoded == 2 && tooLarge.fromKept == 0 && tooLarge.kept == 0 && tooLarge.keptBytes == 0,
		"(6) a picture larger than the most on its own: given, and never kept (" + CountWords(tooLarge) + ")");

	ShapePickerPictures_Forget();
	ShapePickerPictures_End();
	Check(RimLoader_BytesThisLevel() == levelBefore, "(6) and the visit's end puts the level's count back");

	// ---- (7) THE SAME PATH LEADING TO ANOTHER FILE (Vasquez's MUST on 578f59d9: nothing failed when the kept picture was given without comparing the files). In a
	// scratch data folder of the test's own (never the stock copy, which other runs share), a picture is kept, then the file its path leads to is written again with
	// another picture's bytes (other pixels, another size), then written again with those same bytes and only its time last written moved: each time the NEW file
	// is decoded, never the kept pixels given.
	std::vector<uint8_t> const pickupBytes = StockBytes("graphics/Common/Pikup.rim");
	std::vector<uint8_t> const loadingBytes = StockBytes("graphics/Menus/Loading.rim");
	char stockFolder[MAX_PATH] = {};
	char temp[MAX_PATH] = {};
	DWORD const stockLength = GetCurrentDirectoryA(MAX_PATH, stockFolder);
	DWORD const tempLength = GetTempPathA(MAX_PATH, temp);
	std::string const scratch = std::string(temp, tempLength) + "host_shape_picker_pictures_test-" + std::to_string((unsigned long)GetCurrentProcessId());
	std::string const graphics = scratch + "\\graphics";
	std::string const folder = graphics + "\\picker7";
	std::string const picture = folder + "\\same.rim";
	bool const madeScratch = stockLength > 0 && stockLength < MAX_PATH && tempLength > 0 && tempLength < MAX_PATH && !pickupBytes.empty() && !loadingBytes.empty() &&
		CreateDirectoryA(scratch.c_str(), NULL) && CreateDirectoryA(graphics.c_str(), NULL) && CreateDirectoryA(folder.c_str(), NULL) && WriteWhole(picture, pickupBytes) &&
		SetCurrentDirectoryA(scratch.c_str());

	Check(madeScratch, "(7) a scratch data folder of the test's own, its graphics\\picker7\\same.rim the pickup's bytes (" + scratch + ")");

	if (madeScratch)
	{
		ShapePickerPictures_Begin();
		ShapePickerPictures_Forget();

		host::ShapeThumbTexture first, again, other, saved;
		bool const readFirst = ShapePickerScreen_ReadPicture(Picture("picker7\\same.rim", "picker7\\same.rim"), &first, &why, nullptr);
		bool const readAgain = ShapePickerScreen_ReadPicture(Picture("picker7\\same.rim", "picker7\\same.rim"), &again, &why, nullptr);
		ShapePickerPictureCounts const keptOnce = ShapePickerPictures_Counts();

		Check(readFirst && readAgain && first.bgra == pickup.bgra && again.bgra == pickup.bgra && keptOnce.decoded == 1 && keptOnce.fromKept == 1,
			"(7) the loose picture decoded once, then given from the kept ones (" + CountWords(keptOnce) + ")");

		bool const replaced = WriteWhole(picture, loadingBytes);
		bool const readOther = ShapePickerScreen_ReadPicture(Picture("picker7\\same.rim", "picker7\\same.rim"), &other, &why, nullptr);
		ShapePickerPictureCounts const afterOther = ShapePickerPictures_Counts();

		Check(replaced && readOther && other.width == largeWidth && other.height == largeHeight && other.bgra != pickup.bgra && afterOther.decoded == 2 &&
			afterOther.fromKept == 1 && afterOther.kept == 1 && afterOther.keptBytes == other.bgra.size(), "(7) the same path now leading to other pixels at another size: the new file "
			"decoded (" + std::to_string(other.width) + " x " + std::to_string(other.height) + "), never the kept pixels given, and kept in their place (" + CountWords(afterOther) + ")");

		bool const savedAgain = WriteWhole(picture, loadingBytes) && TimeAnHourBack(picture);
		bool const readSaved = ShapePickerScreen_ReadPicture(Picture("picker7\\same.rim", "picker7\\same.rim"), &saved, &why, nullptr);
		ShapePickerPictureCounts const afterSaved = ShapePickerPictures_Counts();

		Check(savedAgain && readSaved && saved.bgra == other.bgra && afterSaved.decoded == 3 && afterSaved.fromKept == 1 && afterSaved.kept == 1,
			"(7) the same bytes saved again, only the time last written moved: decoded again, never given from the kept ones (" + CountWords(afterSaved) + ")");

		ShapePickerPictures_Forget();
		ShapePickerPictures_End();
	}

	SetCurrentDirectoryA(stockFolder);
	DeleteFileA(picture.c_str());
	RemoveDirectoryA(folder.c_str());
	RemoveDirectoryA(graphics.c_str());
	RemoveDirectoryA(scratch.c_str());

	printf(checksFailed ? "host_shape_picker_pictures_test: %d FAILED\n" : "host_shape_picker_pictures_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
