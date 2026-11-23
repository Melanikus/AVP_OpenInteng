// The test for ShapeExtract_StockCheck (src/host/shape_extract_files.h): what a data folder's avp_rifs and avp_huds hold against the identity table's stock
// files, for Drake's FOUND form (Frost's shapes piece for Dietrich, 2026-11-23). Real files, in folders this test makes in the temp folder and takes away:
//   A: avp_rifs holds a derelict.rif of other bytes (NOT Steam's own), a MyMap.RIF the table has no row for (counted, not stock), a junction named
//      Genshd1.RIF, a folder named Folder.rif and an empty Empty.rif (each REFUSED in ShapeExtract_ReadMapFile's words, and not missing), a readme.txt and a
//      folder Custom (neither looked at); avp_huds is there and empty. Every other stock name is MISSING, by the table's name.
//   B: avp_rifs is a junction (refused, not looked in, so none of its stock names is called missing); avp_huds isn't there (all 23 missing)
//   C: avp_rifs holds 4097 .rif files: 4096 are read, and the rest refused in one line (a folder may hold anything)
// A stock-named file of Steam's own bytes can't be made here (no Rebellion data in a unit test): verify_shape_identities stock proves that on the stock copy.
// Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include "make_junction.h"   // a junction made without a shell (Vasquez)
#include "shape_extract_files.h"
#include "shape_identity.h"

// the game's memory code (our_mem.cpp, for the decoder shape_extract.cpp uses) calls these: here they only say what happened
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

static std::wstring Wide(std::string const &narrow)
{
	int const letters = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
	std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

	if (letters > 0)
	{
		MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
	}

	return wide;
}

static bool Write(std::string const &path, std::string const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	DWORD wrote = 0;
	bool const good = file != INVALID_HANDLE_VALUE && (bytes.empty() || (WriteFile(file, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size()));

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	return good;
}

static bool Junction(std::string const &link, std::string const &to)
{

	return MakeJunction(Wide(link), Wide(to)) && (GetFileAttributesA(link.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

static bool Has(std::vector<std::string> const &list, std::string const &name)
{
	return std::find(list.begin(), list.end(), name) != list.end();
}

// the refused line that starts with these words, or ""
static std::string RefusedLine(std::vector<std::string> const &refused, std::string const &start)
{
	for (std::string const &line : refused)
	{
		if (line.compare(0, start.size(), start) == 0)
		{
			return line;
		}
	}

	return std::string();
}

// how many stock files the table has in that folder
static size_t StockCount(char const *folder)
{
	size_t count = 0;

	for (size_t i = 0; i < kShapeIdentityStockFileCount; i++)
	{
		count += strcmp(kShapeIdentityStockFiles[i].folder, folder) == 0 ? 1 : 0;
	}

	return count;
}

int main()
{
	puts("the stock check");

	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	size_t const stockLevels = StockCount("avp_rifs"), stockHuds = StockCount("avp_huds");

	Check(stockLevels == 63 && stockHuds == 23, "the table's stock files: 63 levels and 23 HUD files, as Steam ships them (" + std::to_string(stockLevels) + " and " +
		std::to_string(stockHuds) + ")");

	std::string const root = std::string(temp) + "shape_stock_check_test_" + std::to_string((unsigned long)GetCurrentProcessId());
	std::string const a = root + "\\a", aRifs = a + "\\avp_rifs", aHuds = a + "\\avp_huds", target = root + "\\target";
	std::string const b = root + "\\b", bRifs = b + "\\avp_rifs";
	std::string const c = root + "\\c", cRifs = c + "\\avp_rifs";

	bool made = CreateDirectoryA(root.c_str(), nullptr) && CreateDirectoryA(a.c_str(), nullptr) && CreateDirectoryA(aRifs.c_str(), nullptr) &&
		CreateDirectoryA(aHuds.c_str(), nullptr) && CreateDirectoryA(target.c_str(), nullptr) && CreateDirectoryA(b.c_str(), nullptr) &&
		CreateDirectoryA(c.c_str(), nullptr) && CreateDirectoryA(cRifs.c_str(), nullptr) &&
		Write(aRifs + "\\derelict.rif", "not Steam's Derelict") && Write(aRifs + "\\MyMap.RIF", "a mapper's own level") && Write(aRifs + "\\Empty.rif", "") &&
		Write(aRifs + "\\readme.txt", "not a level") && CreateDirectoryA((aRifs + "\\Folder.rif").c_str(), nullptr) &&
		CreateDirectoryA((aRifs + "\\Custom").c_str(), nullptr) && Write(aRifs + "\\Custom\\Inside.rif", "under a folder: not looked at");

	for (int i = 0; made && i < 4097; i++)
	{
		char name[32];
		_snprintf_s(name, sizeof(name), _TRUNCATE, "\\m%04d.rif", i);
		made = Write(cRifs + name, "m");
	}

	Check(made, "the test's folders and files are made in the temp folder: " + root);
	Check(Junction(aRifs + "\\Genshd1.RIF", target) && Junction(bRifs, aRifs), "A's Genshd1.RIF and B's avp_rifs are made junctions (make_junction.h, no shell)");

	// ---- A
	ShapeStockCheck const checkA = ShapeExtract_StockCheck(a);

	Check(checkA.levels == 2 && checkA.levelsOwn == 0, "A: 2 levels read (derelict.rif, MyMap.RIF), none Steam's own (" + std::to_string(checkA.levels) + ", " +
		std::to_string(checkA.levelsOwn) + ")");
	Check(checkA.notOwn.size() == 1 && Has(checkA.notOwn, "derelict.rif"), "A: derelict.rif, of other bytes, is NOT Steam's own, by its name as found (" +
		std::to_string(checkA.notOwn.size()) + " not own)");
	Check(!Has(checkA.notOwn, "MyMap.RIF") && !Has(checkA.missing, "mymap.rif"), "A: MyMap.RIF, which the table doesn't name, is counted and is no stock file");

	std::string const link = RefusedLine(checkA.refused, "avp_rifs\\Genshd1.RIF: "), folder = RefusedLine(checkA.refused, "avp_rifs\\Folder.rif: ");
	std::string const empty = RefusedLine(checkA.refused, "avp_rifs\\Empty.rif: ");

	Check(link.find("a link") != std::string::npos, "A: the junction named Genshd1.RIF is REFUSED: " + link);
	Check(folder.find("a folder") != std::string::npos, "A: the folder named Folder.rif is REFUSED: " + folder);
	Check(empty.find("empty") != std::string::npos, "A: the empty Empty.rif is REFUSED: " + empty);
	Check(checkA.refused.size() == 3, "A: and nothing else is refused: readme.txt and the folder Custom (and the .rif inside it) are not looked at (" +
		std::to_string(checkA.refused.size()) + " refused)");
	Check(!Has(checkA.missing, "genshd1.rif") && !Has(checkA.missing, "derelict.rif"), "A: genshd1.rif and derelict.rif are there, so not missing");
	Check(Has(checkA.missing, "hangar.rif"), "A: hangar.rif is MISSING, by the table's name");
	Check(checkA.missing.size() == stockLevels - 2 + stockHuds, "A: every other stock file is missing: " + std::to_string(stockLevels - 2) + " levels and the " +
		std::to_string(stockHuds) + " HUD files of an empty avp_huds (" + std::to_string(checkA.missing.size()) + ")");
	Check(checkA.huds == 0 && checkA.hudsOwn == 0, "A: no HUD file read");

	// ---- B
	ShapeStockCheck const checkB = ShapeExtract_StockCheck(b);
	std::string const bLink = RefusedLine(checkB.refused, "avp_rifs: ");

	Check(bLink.find("a link") != std::string::npos && checkB.levels == 0 && checkB.notOwn.empty(), "B: avp_rifs, a junction, is REFUSED and not looked in: " + bLink);
	Check(checkB.missing.size() == stockHuds, "B: none of avp_rifs' stock names is called missing (it wasn't looked in), and avp_huds, not there, has all " +
		std::to_string(stockHuds) + " missing (" + std::to_string(checkB.missing.size()) + ")");

	// ---- C
	ShapeStockCheck const checkC = ShapeExtract_StockCheck(c);
	std::string const more = RefusedLine(checkC.refused, "avp_rifs: ");

	Check(checkC.levels == kShapeStockMostFiles && more.find("more than 4096") != std::string::npos, "C: 4097 .rif files: " + std::to_string(checkC.levels) +
		" read, the rest refused in one line: " + more);

	// taken away again (RemoveDirectory on a junction takes the junction away, never what it points at)
	RemoveDirectoryA(bRifs.c_str());
	RemoveDirectoryA((aRifs + "\\Genshd1.RIF").c_str());

	for (char const *file : { "\\derelict.rif", "\\MyMap.RIF", "\\Empty.rif", "\\readme.txt", "\\Custom\\Inside.rif" })
	{
		DeleteFileA((aRifs + file).c_str());
	}

	for (int i = 0; i < 4097; i++)
	{
		char name[32];
		_snprintf_s(name, sizeof(name), _TRUNCATE, "\\m%04d.rif", i);
		DeleteFileA((cRifs + name).c_str());
	}

	for (std::string const &gone : { aRifs + "\\Folder.rif", aRifs + "\\Custom", aRifs, aHuds, a, b, cRifs, c, target })
	{
		RemoveDirectoryA(gone.c_str());
	}

	Check(RemoveDirectoryA(root.c_str()) != 0, "and the test's folders are taken away");

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
