// The test of Export map's films (src/host/export_films.cpp; design-vasquez-export-films.md). Written 2026-11-27 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_export_films_test.exe <Invasion .rif>. The level is only read. The test makes its own data folder, export_films_test_<process id>, in the
// folder it runs in (it stops if that is there already), and deletes it at the end. No word it's given becomes a path it writes to (Snyk Code's
// cpp/PT/test, 2026-11-27: a test folder taken from the command line and written to with fopen is a path traversal).
// (Its helper that writes a file is PutTestFile, never WriteFile: Snyk matched the Windows WriteFile calls in the editor's own code, the level's save among
// them, to a test function of that name, and flagged 80 other tests' saves as path traversals.)
// THE CASES:
//  1. the film name the game makes of a video screen's picture (FmvCutscenes.cpp ScanImagesForFMVs: from "FMVs" to the name's first '.', exact capitals);
//  2. Invasion's message numbers: some, ascending, 1 to 450, each once, and each one the depot-checked table says Invasion plays (message_numbers.h);
//     its one video screen, FMVs\triggered_plot.RIM;
//  3. the gathering over a data folder of the test's own, with no FMVs folder: every number is text only, nothing to export;
//  4. with films (real Smacker headers): one under a number with a stock film but other bytes (the mapper's: kept, and a WARNING that names Invasion),
//     one under a number with no stock film (kept, no warning), each with its size and SHA-256; a plain text named as a film is NOT kept, with a WARNING
//     (Dietrich's review MUST); a number with no file is text only; nothing counted as stock; a kept film read again for its write, and refused once
//     its bytes changed;
//  5. a screen film in the data folder (FMVs\triggered_plot.smk): a WARNING, and it is NOT exported;
//  6. a film that can't be read as a plain file (a folder of that name): the gathering stops, naming it.
// (Rebellion's own bytes, left out and counted, are the acceptance's to show: their bytes are only in the game's own folder.)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "export_films.h"
#include "message_numbers.h"
#include "smk_header.h"   // kMostMessageNumber, MessageFmvName

static int checksFailed = 0;
static int checksDone = 0;

static void Check(bool good, std::string const &what)
{
	checksDone++;

	if (!good)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own (as host_acoustic_mode_test's)

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static bool PutTestFile(std::string const &path, std::string const &bytes)
{
	FILE *f = fopen(path.c_str(), "wb");

	if (!f)
	{
		return false;
	}

	bool const all = fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();

	fclose(f);
	return all;
}

static bool Has(std::vector<int32_t> const &list, int32_t n)
{
	return std::find(list.begin(), list.end(), n) != list.end();
}

// A Smacker header SmkHeader_Read takes (SMK2, 128 x 96, one frame), 300 bytes, its last one `mark` so each made with another is other bytes
static std::string SmackerBytes(char mark)
{
	std::string bytes(300, '\0');
	uint32_t const width = 128, height = 96, frames = 1;

	memcpy(&bytes[0], "SMK2", 4);
	memcpy(&bytes[4], &width, 4);
	memcpy(&bytes[8], &height, 4);
	memcpy(&bytes[12], &frames, 4);
	bytes[299] = mark;
	return bytes;
}

static bool AnyHolds(std::vector<std::string> const &lines, std::string const &words)
{
	for (std::string const &line : lines)
	{
		if (line.find(words) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// ---- 1. a screen's film name, as the game makes it
	Check(host::ExportFilms_ScreenFilm("FMVs\\triggered_plot.RIM") == "FMVs\\triggered_plot.smk", "FMVs\\triggered_plot.RIM: FMVs\\triggered_plot.smk");
	Check(host::ExportFilms_ScreenFilm("Custom\\x\\FMVs\\alien.RIM") == "FMVs\\alien.smk", "from \"FMVs\" on: Custom\\x\\FMVs\\alien.RIM gives FMVs\\alien.smk");
	Check(host::ExportFilms_ScreenFilm("FMVs\\final") == "FMVs\\final.smk", "a name with no '.': all of it from \"FMVs\", then .smk");
	Check(host::ExportFilms_ScreenFilm("a.b\\FMVs\\screen.RIM") == "FMVs\\screen.RIM.smk",
	      "a '.' BEFORE \"FMVs\": the game's count wraps, so it takes the rest of the name (FMVs\\screen.RIM.smk)");
	Check(host::ExportFilms_ScreenFilm("fmvs\\screen.RIM").empty() && host::ExportFilms_ScreenFilm("plain.RIM").empty(),
	      "no \"FMVs\" in those exact capitals: not a screen");

	if (argc < 2)
	{
		printf("  FAILED  usage: host_export_films_test <Invasion .rif>\n");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes Invasion");
		printf("%d checks, %d failed\n", checksDone, checksFailed);
		return 1;
	}

	// ---- 2. Invasion's numbers and screen
	std::vector<int32_t> const numbers = host::ExportFilms_MessageNumbers(level.Level());
	bool ordered = !numbers.empty(), invasions = !numbers.empty();

	for (size_t i = 0; i < numbers.size(); i++)
	{
		ordered = ordered && numbers[i] >= 1 && numbers[i] <= model::kMostMessageNumber && (i == 0 || numbers[i - 1] < numbers[i]);
		invasions = invasions && model::MessageNumbers_StockLevels(numbers[i]).find("Invasion.RIF") != std::string::npos;
	}

	Check(ordered, "Invasion's Message strategies play " + std::to_string(numbers.size()) + " numbers, ascending, 1 to 450, each once");
	Check(invasions, "and the depot-checked table says Invasion plays every one of them");

	std::vector<host::ExportFilmScreen> const screens = host::ExportFilms_Screens(level.Level());

	Check(screens.size() == 1 && screens[0].film == "FMVs\\triggered_plot.smk", "Invasion's one video screen is FMVs\\triggered_plot (the stock survey)");

	// the numbers the cases use: one with a stock film, three with none
	int32_t withStock = 0, without = 0, without2 = 0, without3 = 0;

	for (int32_t n : numbers)
	{
		if (model::MessageNumbers_HasStockFilm(n))
		{
			withStock = withStock ? withStock : n;
		}
		else if (!without)
		{
			without = n;
		}
		else if (!without2)
		{
			without2 = n;
		}
		else if (!without3)
		{
			without3 = n;
		}
	}

	Check(withStock && without && without2 && without3, "Invasion plays a number with a stock film (" + std::to_string(withStock) + ") and three with none (" +
	      std::to_string(without) + ", " + std::to_string(without2) + ", " + std::to_string(without3) + ")");

	// the folder it runs in, whole (the reader takes only a plain, whole path)
	char here[MAX_PATH] = "";
	DWORD const hereLength = GetCurrentDirectoryA(MAX_PATH, here);

	if (hereLength == 0 || hereLength >= MAX_PATH)
	{
		printf("  FAILED  the test couldn't read the folder it runs in\n");
		return 1;
	}

	std::string const data = std::string(here) + "\\export_films_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(data.c_str(), NULL))
	{
		printf("  FAILED  the test couldn't make its folder %s (is it there already?)\n", data.c_str());
		return 1;
	}

	std::string const fmvs = data + "\\FMVs";
	std::vector<std::string> files;
	std::string why;
	host::ExportFilms films;

	// ---- 3. no FMVs folder at all
	Check(host::ExportFilms_Gather(level.Level(), data, &films, &why) && films.own.empty() && films.textOnly.size() == numbers.size() && films.stock == 0,
	      "a data folder with no FMVs: every number is text only, nothing to export (" + why + ")");

	// ---- 4. the mapper's films: two real Smacker headers (each other bytes), and one plain text named as a film
	CreateDirectoryA(fmvs.c_str(), NULL);

	std::string const mine = fmvs + "\\" + model::MessageFmvName(withStock);
	std::string const mine2 = fmvs + "\\" + model::MessageFmvName(without);
	std::string const notAFilm = fmvs + "\\" + model::MessageFmvName(without2);

	files = { mine, mine2, notAFilm };
	Check(PutTestFile(mine, SmackerBytes('a')) && PutTestFile(mine2, SmackerBytes('b')) && PutTestFile(notAFilm, "plain words, no film"),
	      "two films of the mapper's and one plain text are put in the test's FMVs");

	bool const gathered = host::ExportFilms_Gather(level.Level(), data, &films, &why);

	Check(gathered, "the gathering runs (" + why + ")");

	int32_t const lower = withStock < without ? withStock : without;   // (no std::min: windows.h makes min and max macros)
	int32_t const higher = withStock < without ? without : withStock;

	Check(films.own.size() == 2 && films.own[0].path == "FMVs\\" + model::MessageFmvName(lower) &&
	      films.own[1].path == "FMVs\\" + model::MessageFmvName(higher), "the two films are the level's own, by number, as the game asks for them (FMVs\\message<N>.smk)");
	Check(films.own.size() == 2 && films.own[0].size == SmackerBytes('a').size() && films.own[0].sha256.size() == 64 && films.own[0].sha256 != films.own[1].sha256,
	      "each with its size and SHA-256, gathered without keeping its bytes");
	Check(films.stock == 0 && Has(films.textOnly, without3) && !Has(films.textOnly, withStock) && !Has(films.textOnly, without) && !Has(films.textOnly, without2) &&
	      films.textOnly.size() == numbers.size() - 3, "none is Rebellion's own bytes; every number with no file is text only");
	Check(films.unplayable == 1 && AnyHolds(films.warnings, "FMVs\\" + model::MessageFmvName(without2) + " is not exported"),
	      "the plain text is NOT exported, and a warning says why (Dietrich's review MUST: the players' decoder never gets it)");
	Check(films.replacing == 1 && AnyHolds(films.warnings, "FMVs\\" + model::MessageFmvName(withStock)) && AnyHolds(films.warnings, "Invasion.RIF") &&
	      AnyHolds(films.warnings, "replaces"), "one warning: the film under " + std::to_string(withStock) + " replaces Rebellion's, naming Invasion");
	Check(films.warnings.size() == 2 && films.screenFilms == 0, "and no other warning (no screen film is there yet)");

	// a film read again for its write: the very bytes, and refused once they change
	std::vector<uint8_t> again;

	Check(films.own.size() == 2 && host::ExportFilms_Read(films.own[0], &again, &why) && again.size() == films.own[0].size,
	      "a gathered film is read again whole for its write (" + why + ")");

	std::string const changedPath = fmvs + "\\" + model::MessageFmvName(lower);

	PutTestFile(changedPath, SmackerBytes('z'));
	why.clear();

	bool const refusedChanged = films.own.size() == 2 && !host::ExportFilms_Read(films.own[0], &again, &why);

	Check(refusedChanged && why.find("changed") != std::string::npos && again.empty(), "a film changed since the gathering is refused: " + why);
	PutTestFile(changedPath, SmackerBytes(lower == withStock ? 'a' : 'b'));

	// ---- 5. a screen film in the data folder
	std::string const screenFilm = fmvs + "\\triggered_plot.smk";

	files.push_back(screenFilm);
	Check(PutTestFile(screenFilm, "a screen film"), "a screen film is put in the test's FMVs");
	Check(host::ExportFilms_Gather(level.Level(), data, &films, &why) && films.own.size() == 2 && AnyHolds(films.warnings, "FMVs\\triggered_plot.smk") &&
	      films.screens.size() == 1 && films.screenFilms == 1, "it is NOT exported, and a warning names it (AvPx keeps that screen still while it's there)");

	// ---- 6. a film that isn't a plain file
	std::string const folderFilm = fmvs + "\\" + model::MessageFmvName(without3);

	CreateDirectoryA(folderFilm.c_str(), NULL);
	why.clear();

	bool const stopped = !host::ExportFilms_Gather(level.Level(), data, &films, &why);   // first, so the words below hold its reason

	Check(stopped && why.find(model::MessageFmvName(without3)) != std::string::npos, "a folder named as a film stops the gathering, naming it: " + why);
	RemoveDirectoryA(folderFilm.c_str());

	for (std::string const &one : files)
	{
		DeleteFileA(one.c_str());
	}

	RemoveDirectoryA(fmvs.c_str());
	RemoveDirectoryA(data.c_str());

	printf("%d checks, %d failed\n", checksDone, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
