// The test for an imported texture saved as a new RIM file of the mapper's own (src/host/rim_save.h: the RIM import's step 6). Written 2026-11-23 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//   (1) the name: "Genshd1.RIF" and "Wall01" make Custom\Genshd1\Wall01.RIM; a whole path, a ".rif" in small letters and a texture named with ".rim" make
//       Custom\My Map\brick.RIM
//   (2) names refused, nothing given back: an empty texture, a slash in it, a ".." in it, a device's name, a letter the game refuses, an empty level part
//       (".rif"), and a whole name over the game's 200 bytes
//   (3) the name offered from a picture's file: the refused letters made "_", a second dot of two made "_", the ending dropped, at most 32 bytes, "tex_"
//       before a device, "texture" for nothing left; each one passes (1)
//   (4) saved: graphics, graphics\Custom and the level's folder made, the file there holding exactly the bytes, its whole path given back
//   (5) never over another file: the same name again is refused as taken, and a file already there (a stand-in for a stock one) is never written over,
//       its bytes unchanged
//   (6) plain folders only: graphics\Custom a junction (mklink /J, no privilege needed) is refused and nothing is made where it points; a file where a folder
//       should be is refused
//   (7) refused calls, nothing written: an entry not of RimSave_EntryName's shape (outside Custom, going up, an extra folder), no bytes, too many bytes, no
//       data folder, and a whole path longer than Windows opens
// RED FIRST: on a stand-in that refuses every name and every save (BadCall), 16 of the 33 checks fail: (1), (3), (4), (5), (6), and (7)'s long path.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "rim_save.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::vector<uint8_t> ReadAll(std::string const &path)
{
	std::vector<uint8_t> bytes;
	FILE *file = fopen(path.c_str(), "rb");

	if (file)
	{
		int c;

		while ((c = fgetc(file)) != EOF)
		{
			bytes.push_back((uint8_t)c);
		}

		fclose(file);
	}

	return bytes;
}

static bool WriteAll(std::string const &path, std::vector<uint8_t> const &bytes)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	return fclose(file) == 0 && written;
}

static bool Exists(std::string const &path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// The name, or "" with *why said, for one level and one texture
static std::string Entry(std::string const &level, std::string const &texture, std::string *why)
{
	std::string entry = "left over";
	bool const made = RimSave_EntryName(level, texture, &entry, why);

	return made ? entry : (entry.empty() ? std::string() : std::string("(false, but entry not emptied)"));
}

int main()
{
	std::string why;

	// ---- (1) the name
	puts("The name the level's list holds:");
	std::string const plain = Entry("Genshd1.RIF", "Wall01", &why);

	Check(plain == "Custom\\Genshd1\\Wall01.RIM", "Genshd1.RIF and Wall01: " + plain + " " + why);

	std::string const whole = Entry("C:\\Games\\AvP\\avp_rifs\\Custom\\My Map.rif", "brick.rim", &why);

	Check(whole == "Custom\\My Map\\brick.RIM", "a whole path, \".rif\" in small letters, \"brick.rim\": " + whole + " " + why);

	// ---- (2) names refused
	puts("Names refused:");
	auto refusedName = [&](std::string const &level, std::string const &texture, std::string const &what)
	{
		std::string const got = Entry(level, texture, &why);

		Check(got.empty() && !why.empty(), what + " (" + why + ")");
	};

	refusedName("Genshd1.RIF", "", "an empty texture");
	refusedName("Genshd1.RIF", "walls\\one", "a backslash in the texture");
	refusedName("Genshd1.RIF", "walls/one", "a slash in the texture");
	refusedName("Genshd1.RIF", "a..b", "a \"..\" in the texture");
	refusedName("Genshd1.RIF", "con", "a device's name");
	refusedName("Genshd1.RIF", "wall(1)", "a letter the game refuses");
	refusedName("avp_rifs\\.rif", "Wall01", "an empty level part");
	refusedName("Genshd1.RIF", std::string(190, 'w'), "a whole name over 200 bytes");

	// ---- (3) the name offered from a picture's file
	puts("The name offered from a picture:");
	auto offered = [&](std::string const &picture, std::string const &want)
	{
		std::string const got = RimSave_SuggestedName(picture);
		std::string const entry = Entry("Genshd1.RIF", got, &why);

		Check(got == want && !entry.empty(), picture + " offers \"" + got + "\" (want \"" + want + "\"), and it passes the name's rule " + why);
	};

	offered("C:\\Pictures\\my wall (1).png", "my wall _1_");
	offered("D:/scans/Brick.BMP", "Brick");
	offered("C:\\Pictures\\" + std::string(40, 'b') + ".png", std::string(32, 'b'));
	offered("C:\\Pictures\\con.png", "tex_con");
	offered("C:\\Pictures\\...png", "texture");
	offered("C:\\Pictures\\wall. .png", "wall");
	offered("C:\\Pictures\\a..b.png", "a._b");

	// ---- the data folder: our own, in the folder the test runs in
	char here[MAX_PATH];
	DWORD const length = GetCurrentDirectoryA(MAX_PATH, here);

	if (length == 0 || length >= MAX_PATH - 80)
	{
		Check(false, "no folder to run in: nothing saved was tested");
		printf("%d checks failed\n", checksFailed);
		return 1;
	}

	std::string const data = std::string(here) + "\\host_rim_save_test_" + std::to_string(GetCurrentProcessId());
	std::string const elsewhere = data + "-elsewhere";
	bool const made = CreateDirectoryA(data.c_str(), nullptr) && CreateDirectoryA(elsewhere.c_str(), nullptr);

	Check(made, "the test's own data folder and a folder elsewhere are made: " + data);

	std::vector<uint8_t> rim(17248);

	for (size_t i = 0; i < rim.size(); i++)
	{
		rim[i] = (uint8_t)(i * 7 + 3);
	}

	// ---- (4) saved
	puts("Saved:");
	std::string saved;
	RimSaveResult const first = RimSave_New(data, "Custom\\Genshd1\\Wall01.RIM", rim, &saved, &why);
	std::string const file = data + "\\graphics\\Custom\\Genshd1\\Wall01.RIM";

	Check(first == RimSaveResult::Saved && saved == file, std::string("saved as ") + saved + " (" + RimSaveResultWords(first) + " " + why + ")");
	Check(ReadAll(file) == rim, "the file holds exactly the bytes (17,248)");

	// ---- (5) never over another file
	puts("Never over another file:");
	std::vector<uint8_t> other(rim.rbegin(), rim.rend());
	RimSaveResult const again = RimSave_New(data, "Custom\\Genshd1\\Wall01.RIM", other, &saved, &why);

	Check(again == RimSaveResult::NameTaken && ReadAll(file) == rim && why.find("Wall01.RIM") != std::string::npos,
		std::string("the same name again: taken, naming it, the first file as it was (") + RimSaveResultWords(again) + ": " + why + ")");

	std::string const stockLike = data + "\\graphics\\Custom\\Genshd1\\Stock.RIM";
	std::vector<uint8_t> const stockBytes = { 'F', 'O', 'R', 'M', 1, 2, 3 };
	bool const stockMade = WriteAll(stockLike, stockBytes);
	RimSaveResult const over = RimSave_New(data, "Custom\\Genshd1\\stock.rim", rim, &saved, &why);

	Check(stockMade && over == RimSaveResult::NameTaken && ReadAll(stockLike) == stockBytes,
		std::string("a file already there, named in other capitals: never written over, its 7 bytes unchanged (") + RimSaveResultWords(over) + ")");

	// ---- (6) plain folders only
	puts("Plain folders only:");
	std::string const joinedData = data + "-joined";
	bool const joinedBase = CreateDirectoryA(joinedData.c_str(), nullptr) && CreateDirectoryA((joinedData + "\\graphics").c_str(), nullptr);
	std::string const joined = joinedData + "\\graphics\\Custom";
	std::string const command = "mklink /J \"" + joined + "\" \"" + elsewhere + "\" >nul";
	bool const junction = joinedBase && system(command.c_str()) == 0 && (GetFileAttributesA(joined.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	RimSaveResult const through = RimSave_New(joinedData, "Custom\\Genshd1\\Wall01.RIM", rim, &saved, &why);

	Check(junction && through == RimSaveResult::NotPlain && !Exists(elsewhere + "\\Genshd1"),
		std::string("graphics\\Custom a junction: refused, and nothing made where it points (") + RimSaveResultWords(through) + ": " + why + ")");

	std::string const fileData = data + "-file";
	bool const fileBase = CreateDirectoryA(fileData.c_str(), nullptr) && WriteAll(fileData + "\\graphics", { 1 });
	RimSaveResult const onFile = RimSave_New(fileData, "Custom\\Genshd1\\Wall01.RIM", rim, &saved, &why);

	Check(fileBase && onFile == RimSaveResult::NotPlain, std::string("a file named graphics where the folder should be: refused (") + RimSaveResultWords(onFile) + ": " + why + ")");

	// ---- (7) refused calls
	puts("Refused calls:");
	auto refusedCall = [&](std::string const &folder, std::string const &entry, std::vector<uint8_t> const &bytes, RimSaveResult want, std::string const &what)
	{
		saved = "left over";
		RimSaveResult const got = RimSave_New(folder, entry, bytes, &saved, &why);

		Check(got == want && saved.empty() && !why.empty(), what + " (" + RimSaveResultWords(got) + ": " + why + ")");
	};

	refusedCall(data, "Envrnmts\\Genshd1\\Wall02.RIM", rim, RimSaveResult::BadCall, "outside Custom");
	refusedCall(data, "Custom\\..\\Genshd1\\Wall02.RIM", rim, RimSaveResult::BadCall, "a part going up");
	refusedCall(data, "Custom\\Genshd1\\more\\Wall02.RIM", rim, RimSaveResult::BadCall, "a folder more");
	refusedCall(data, "Custom\\Genshd1\\Wall02.bmp", rim, RimSaveResult::BadCall, "not a .RIM");
	refusedCall(data, "Custom\\Genshd1\\Wall02.RIM", {}, RimSaveResult::BadCall, "no bytes");
	refusedCall(data, "Custom\\Genshd1\\Wall02.RIM", std::vector<uint8_t>((size_t)kRimSaveMostBytes + 1, 0), RimSaveResult::BadCall, "a byte over the most");
	refusedCall("", "Custom\\Genshd1\\Wall02.RIM", rim, RimSaveResult::BadCall, "no data folder");
	// the entry the game's most (200 bytes), under a data folder 61 letters deeper than ours: over 259 whatever folder the test runs in (never made: the length
	// is refused before any folder is)
	refusedCall(data + "\\" + std::string(60, 'd'), "Custom\\Genshd1\\" + std::string(181, 'w') + ".RIM", rim, RimSaveResult::BadName,
		"a whole path longer than Windows opens");
	Check(!Exists(data + "\\graphics\\Custom\\Genshd1\\Wall02.RIM") && !Exists(data + "\\graphics\\Envrnmts"), "none of them wrote anything");

	// ---- our own folders away (RemoveDirectory on a junction takes the junction away, never what it points at)
	DeleteFileA(file.c_str());
	DeleteFileA(stockLike.c_str());
	RemoveDirectoryA((data + "\\graphics\\Custom\\Genshd1").c_str());
	RemoveDirectoryA((data + "\\graphics\\Custom").c_str());
	RemoveDirectoryA((data + "\\graphics").c_str());
	RemoveDirectoryA(data.c_str());
	RemoveDirectoryA(joined.c_str());
	RemoveDirectoryA((joinedData + "\\graphics").c_str());
	RemoveDirectoryA(joinedData.c_str());
	RemoveDirectoryA(elsewhere.c_str());
	DeleteFileA((fileData + "\\graphics").c_str());
	RemoveDirectoryA(fileData.c_str());

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
