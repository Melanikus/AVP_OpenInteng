// host_fmv_files_test.cpp: the FMV finder on a folder of its own (src/host/fmv_files.h): which files it lists, the message numbers, the header read of good, odd and hostile
// files, and the words the status line gets. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "fmv_files.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static void Put(std::vector<uint8_t> *bytes, size_t at, uint32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		(*bytes)[at + (size_t)i] = (uint8_t)(value >> (8 * i));
	}
}

// message100.smk's header (rate -6666, track 0 0xc0005622) and some bytes after it
static std::vector<uint8_t> AvpMessage(uint32_t track0 = 0xc0005622u)
{
	std::vector<uint8_t> bytes(model::kSmkHeaderBytes + 40, 0x5a);

	memset(bytes.data(), 0, model::kSmkHeaderBytes);
	memcpy(bytes.data(), "SMK2", 4);
	Put(&bytes, 4, 128);
	Put(&bytes, 8, 96);
	Put(&bytes, 12, 228);
	Put(&bytes, 16, (uint32_t)-6666);
	Put(&bytes, 72, track0);
	return bytes;
}

static bool Write(std::string const &path, std::vector<uint8_t> const &bytes)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const ok = bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	return fclose(file) == 0 && ok;
}

static bool Has(std::vector<std::string> const &names, char const *name)
{
	return std::find(names.begin(), names.end(), std::string(name)) != names.end();
}

int main()
{
	printf("THE FMV FINDER\n");

	Check(host::FmvFiles_Folder("C:\\Game") == "C:\\Game\\FMVs\\", "the FMVs folder of C:\\Game");
	Check(host::FmvFiles_Folder("C:\\Game\\") == "C:\\Game\\FMVs\\", "and of C:\\Game\\ (its backslash kept once)");
	Check(host::FmvFiles_Folder("") == "" && host::FmvFiles_Folder(nullptr) == "", "no game folder: no FMVs folder");

	char temp[MAX_PATH];
	DWORD const tempLength = GetTempPathA(MAX_PATH, temp);

	if (tempLength == 0 || tempLength >= MAX_PATH)
	{
		printf("FAILED host_fmv_files_test: no temporary folder\n");
		return 1;
	}

	std::string const folder = std::string(temp) + "host_fmv_files_test_" + std::to_string((unsigned long long)GetCurrentProcessId()) + "\\";

	CreateDirectoryA(folder.c_str(), nullptr);

	std::vector<uint8_t> bik = AvpMessage();

	memcpy(bik.data(), "BIKi", 4);

	std::vector<uint8_t> wide = AvpMessage();

	Put(&wide, 4, 5000);

	bool const made = Write(folder + "message100.smk", AvpMessage()) && Write(folder + "message238.smk", AvpMessage(0xe0002b11u)) && Write(folder + "Message7.smk", AvpMessage()) &&
		Write(folder + "message101.smk", bik) && Write(folder + "message102.smk", std::vector<uint8_t>(50, 7)) && Write(folder + "message104.smk", wide) &&
		Write(folder + "message105.smk", std::vector<uint8_t>()) && Write(folder + "message07.smk", AvpMessage()) && Write(folder + "intro.SMK", AvpMessage()) &&
		Write(folder + "notes.txt", AvpMessage()) && CreateDirectoryA((folder + "message103.smk").c_str(), nullptr) != 0;

	Check(made, "the test folder made: " + folder);

	std::vector<std::string> const names = host::FmvFiles_Names(folder);

	Check(names.size() == 9, "9 .smk files listed (" + std::to_string(names.size()) + ")");
	Check(Has(names, "message100.smk") && Has(names, "Message7.smk") && Has(names, "intro.SMK") && Has(names, "message07.smk") && Has(names, "message105.smk"), "the files, their capitals as they are");
	Check(!Has(names, "message103.smk"), "a FOLDER named message103.smk is not listed");
	Check(!Has(names, "notes.txt"), "a .txt is not listed");
	Check(host::FmvFiles_Names(folder, 3).size() == 3, "at most `most`");
	Check(host::FmvFiles_Names(folder.substr(0, folder.size() - 1)) .size() == 9, "the folder without its backslash: the same");

	std::vector<int32_t> const numbers = host::FmvFiles_MessageNumbers(folder);
	std::vector<int32_t> const expected = { 7, 100, 101, 102, 104, 105, 238 };

	Check(numbers == expected, "the message numbers 7, 100, 101, 102, 104, 105, 238: not 07 (the game's %d never writes it), not the folder 103, not intro");

	printf("THE WORDS FOR A MESSAGE STRATEGY'S NUMBER\n");

	struct Said { int32_t number; char const *words; };
	Said const said[] = {
		{ 100, "FMVs\\message100.smk: SMK2 128 x 96, 228 frames at 15 fps (15.2 s), 22050 Hz 8-bit mono" },
		{ 7, "FMVs\\message7.smk: SMK2 128 x 96, 228 frames at 15 fps (15.2 s), 22050 Hz 8-bit mono" },
		{ 238, "FMVs\\message238.smk: SMK2 128 x 96, 228 frames at 15 fps (15.2 s), 11025 Hz 16-bit mono; not AvP's message format: sound 11025 Hz 16-bit mono, not 22050 Hz 8-bit mono" },
		{ 101, "FMVs\\message101.smk is not a Smacker video (no SMK2 or SMK4 at its start): the game can't play it" },
		{ 102, "FMVs\\message102.smk is too short for a Smacker video: the game can't play it" },
		{ 105, "FMVs\\message105.smk is too short for a Smacker video: the game can't play it" },
		{ 104, "FMVs\\message104.smk is a Smacker file with impossible numbers (a side of 0 or above 4096, or no frames): the game can't play it" },
		{ 103, "FMVs\\message103.smk can't be read (it is a folder): the game can't play it" },
		{ 5, "No FMVs\\message5.smk: the game shows message 5 as text only" },
		{ 451, "Message 451: the game refuses a message number outside 0 to 450, so it has no FMV" },
		{ -1, "Message -1: the game refuses a message number outside 0 to 450, so it has no FMV" },
	};

	for (Said const &one : said)
	{
		std::string const words = host::FmvFiles_MessageWords(folder, one.number);

		Check(words == one.words, std::to_string((long long)one.number) + ": " + words);
	}

	model::SmkHeader header;
	std::string why;

	Check(host::FmvFiles_Header(folder, "message100.smk", &header, &why) == host::FmvFileRead::Read && header.frames == 228, "the header of message100.smk");
	Check(host::FmvFiles_Header(folder, "", &header, &why) == host::FmvFileRead::Missing, "no name: missing");
	Check(host::FmvFiles_Header(std::string(), "message100.smk", &header, &why) == host::FmvFileRead::Missing, "no folder: missing");
	Check(host::FmvFiles_Header(folder, std::string(400, 'a') + ".smk", &header, &why) == host::FmvFileRead::Missing, "a path past MAX_PATH: missing, never opened");

	// a file another program holds open for writing is still read (shared)
	HANDLE const held = CreateFileA((folder + "message100.smk").c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	Check(held != INVALID_HANDLE_VALUE && host::FmvFiles_Header(folder, "message100.smk", &header, &why) == host::FmvFileRead::Read, "a file held open by a writer is read");

	if (held != INVALID_HANDLE_VALUE)
	{
		CloseHandle(held);
	}

	std::string const none = folder + "no-such-folder\\";

	Check(host::FmvFiles_Names(none).empty() && host::FmvFiles_MessageNumbers(none).empty(), "a folder that isn't there: nothing listed");
	Check(host::FmvFiles_MessageWords(none, 100) == "No FMVs\\message100.smk: the game shows message 100 as text only", "and no FMV for message 100");

	// the test folder goes
	for (char const *name : { "message100.smk", "message238.smk", "Message7.smk", "message101.smk", "message102.smk", "message104.smk", "message105.smk", "message07.smk", "intro.SMK", "notes.txt" })
	{
		DeleteFileA((folder + name).c_str());
	}

	RemoveDirectoryA((folder + "message103.smk").c_str());
	RemoveDirectoryA(folder.c_str());

	printf("THE VIDEO SCREENS (ScanImagesForFMVs line for line)\n");

	std::vector<size_t> asked;
	uint32_t side = 128;
	auto const sized = [&asked, &side](size_t i, uint32_t *w, uint32_t *h) { asked.push_back(i); *w = side; *h = side; };
	auto const noSmk = [](std::string const &) { return false; };

	std::vector<host::FmvScreenRow> rows = host::FmvFiles_JudgeScreens({ "graphics\\Common\\x.RIM", "FMVs\\triggered_plot.RIM", "fmvs\\lower.RIM", "Fmvscreen.RIM" }, sized, noSmk);

	Check(rows.size() == 1 && rows[0].picture == "FMVs\\triggered_plot.RIM" && rows[0].smk == "FMVs\\triggered_plot.smk" && rows[0].verdict == host::FmvScreen::Screen,
		"FMVs\\triggered_plot.RIM is a screen; fmvs\\ and Fmvscreen are not (the game's find is exact capitals)");
	Check(asked.size() == 1 && asked[0] == 1, "its size asked by its place in the list (1), as the game's Tex_GetDimensions(i)");
	Check(host::FmvFiles_ScreensWords(rows).empty(), "a level whose screens all work: nothing said");

	rows = host::FmvFiles_JudgeScreens({ "FMVs\\triggered_plot.RIM" }, sized, [](std::string const &smk) { return smk == "FMVs\\triggered_plot.smk"; });
	Check(rows.size() == 1 && rows[0].verdict == host::FmvScreen::StillItsSmkIsThere, "its .smk THERE: the game skips it, it stays still");
	Check(host::FmvFiles_ScreensWords(rows) == "Video screen FMVs\\triggered_plot.RIM stays still: FMVs\\triggered_plot.smk is in the game folder, and the game plays messages only on a screen whose own .smk is missing",
		"and says why: " + host::FmvFiles_ScreensWords(rows));

	side = 64;
	rows = host::FmvFiles_JudgeScreens({ "FMVs\\small.RIM" }, sized, noSmk);
	Check(rows.size() == 1 && rows[0].verdict == host::FmvScreen::StillWrongSize && rows[0].width == 64, "64 x 64: stays still");
	Check(host::FmvFiles_ScreensWords(rows) == "Video screen FMVs\\small.RIM stays still: it is 64 x 64, and a video screen must be 128 x 128", "and says why: " + host::FmvFiles_ScreensWords(rows));
	side = 128;

	rows = host::FmvFiles_JudgeScreens({ "graphics.old\\FMVs\\a.RIM", "FMVs\\nodot" }, sized, noSmk);
	Check(rows.size() == 2 && rows[0].smk == "FMVs\\a.RIM.smk" && rows[1].smk == "FMVs\\nodot.smk", "a '.' before FMVs takes the rest of the name (the game's substr): " + (rows.empty() ? std::string() : rows[0].smk) + "; no '.' at all: FMVs\\nodot.smk");

	std::vector<std::string> twelve;

	twelve.push_back("FMVs\\small.RIM");

	for (int k = 0; k < 12; k++)
	{
		twelve.push_back("FMVs\\screen" + std::to_string(k) + ".RIM");
	}

	asked.clear();
	rows = host::FmvFiles_JudgeScreens(twelve, [&asked](size_t i, uint32_t *w, uint32_t *h) { asked.push_back(i); *w = *h = i == 0 ? 64 : 128; }, noSmk);

	int working = 0, past = 0;

	for (host::FmvScreenRow const &row : rows)
	{
		working += row.verdict == host::FmvScreen::Screen ? 1 : 0;
		past += row.verdict == host::FmvScreen::StillPastTheTenth ? 1 : 0;
	}

	Check(rows.size() == 13 && working == 10 && past == 2 && rows[11].verdict == host::FmvScreen::StillPastTheTenth && rows[0].verdict == host::FmvScreen::StillWrongSize,
		"a wrong-size one counts for nothing; 10 screens, then the game stops looking: the last 2 stay still");
	Check(asked.size() == 11, "the pictures past the tenth are never measured (the game breaks out)");
	Check(host::FmvFiles_ScreensWords(rows) == "3 video screens stay still; the first, FMVs\\small.RIM: it is 64 x 64, and a video screen must be 128 x 128", "several: how many, and the first: " + host::FmvFiles_ScreensWords(rows));

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_fmv_files_test\n");
		return 1;
	}

	printf("passed host_fmv_files_test\n");
	return 0;
}
