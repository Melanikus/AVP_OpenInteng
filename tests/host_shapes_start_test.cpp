// The test for src/host/shapes_start.h, the player's maps found and searched for object shapes (shapes part 3), on real folders made in the temp folder
// and taken away again:
//   - THE MAPS FOUND: avp_rifs, its Custom and OpenedByEditor, then avp_huds, each folder's .rif files sorted; never a folder named x.rif, another file, or a
//     link; a missing folder no refusal; a junction folder REFUSED and not looked in; a data folder written with a last "\" the same;
//   - THE STOCK SHAPES MISSING from shape_rifs by the table's names, in any capitals; THE FACTS (a map that isn't Steam's own counted); WHETHER TO ASK;
//   - THE READ AND THE SEARCH: the bytes searched are the bytes read and hashed, even when the file changes in between (no second read);
//   - the whole of it with Drake's trigger: every map found searched once, its shapes written, its SHA-256 on the list.
// Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "make_junction.h"
#include "shape_extract.h"
#include "shape_extract_files.h"
#include "shape_identity.h"
#include "shape_import.h"
#include "shape_extract_fixtures.h"   // PackageFile, LevelOf
#include "shapes_start.h"
#include "shapes_trigger.h"

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
	checksFailed += good ? 0 : 1;
}

static bool Write(std::string const &path, std::vector<char> const &bytes)
{
	FILE *file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") != 0 || !file)
	{
		return false;
	}

	bool const all = bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	return fclose(file) == 0 && all;
}

static std::vector<char> ReadAll(std::string const &path)
{
	std::vector<char> bytes;
	FILE *file = nullptr;

	if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
	{
		return bytes;
	}

	char piece[4096];
	size_t got;

	while ((got = fread(piece, 1, sizeof(piece), file)) > 0)
	{
		bytes.insert(bytes.end(), piece, piece + got);
	}

	fclose(file);
	return bytes;
}

static bool Exists(std::string const &path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static std::wstring Wide(std::string const &narrow)
{
	return std::wstring(narrow.begin(), narrow.end());   // (the test's own paths are plain ASCII)
}

static void TakeAway(std::string const &folder)
{
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string const name = found.cFileName;

			if (name == "." || name == "..")
			{
				continue;
			}

			std::string const path = folder + "\\" + name;

			if (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				RemoveDirectoryA(path.c_str());   // a junction: itself, never what it points at
			}
			else if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				TakeAway(path);
			}
			else
			{
				DeleteFileA(path.c_str());
			}
		} while (FindNextFileA(search, &found));

		FindClose(search);
	}

	RemoveDirectoryA(folder.c_str());
}

static std::string Joined(std::vector<std::string> const &names, std::string const &from)
{
	std::string out;

	for (std::string const &name : names)
	{
		out += (out.empty() ? "" : " ") + (name.compare(0, from.size(), from) == 0 ? name.substr(from.size()) : name);
	}

	return out;
}

int main()
{
	puts("the player's maps, found and searched for shapes");

	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	std::string const root = std::string(temp) + "shapes_start_test_" + std::to_string((unsigned long)GetCurrentProcessId());
	std::string const data = root + "\\game";
	Check(CreateDirectoryA(root.c_str(), nullptr) && CreateDirectoryA(data.c_str(), nullptr), "the test's game folder is made in the temp folder: " + data);

	// THE MAPS: a level holding medbox01 (as the package holds it, imported), and a level with no shape at all
	Bytes placed;
	ShapeImport_Build(PackageFile(), "medbox01.rif", 12, &placed, nullptr);
	Bytes const withMedbox = LevelOf({ placed }), empty = LevelOf({});

	for (char const *folder : { "\\avp_rifs", "\\avp_rifs\\Custom", "\\avp_rifs\\OpenedByEditor", "\\avp_huds", "\\elsewhere" })
	{
		CreateDirectoryA((data + folder).c_str(), nullptr);
	}

	CreateDirectoryA((data + "\\avp_rifs\\folder.rif").c_str(), nullptr);   // a FOLDER named like a map
	bool const made = Write(data + "\\avp_rifs\\b.RIF", withMedbox) && Write(data + "\\avp_rifs\\A.rif", empty) && Write(data + "\\avp_rifs\\notes.txt", { 'x' }) &&
		Write(data + "\\avp_rifs\\Custom\\Mine.rif", withMedbox) && Write(data + "\\avp_rifs\\OpenedByEditor\\Copy.rif", empty) &&
		Write(data + "\\avp_huds\\Marine.rif", empty) && Write(data + "\\avp_rifs\\a.rifle", empty);
	Check(made, "the maps are made: avp_rifs b.RIF, A.rif (and notes.txt, a.rifle, a folder folder.rif), Custom Mine.rif, OpenedByEditor Copy.rif, avp_huds Marine.rif");

	// THE MAPS FOUND, in their fixed order
	{
		std::vector<std::string> refused;
		std::vector<std::string> const maps = ShapesStart_MapsFound(data, &refused);
		std::string const found = Joined(maps, data + "\\");
		Check(found == "avp_rifs\\A.rif avp_rifs\\b.RIF avp_rifs\\Custom\\Mine.rif avp_rifs\\OpenedByEditor\\Copy.rif avp_huds\\Marine.rif" && refused.empty(),
			"found: avp_rifs sorted in any case, then Custom, OpenedByEditor, avp_huds; no .txt, no .rifle, no folder named folder.rif (\"" + found + "\")");

		std::vector<std::string> refused2;
		Check(Joined(ShapesStart_MapsFound(data + "\\", &refused2), data + "\\") == found && refused2.empty(), "a data folder written with a last \"\\\": the same maps");

		std::vector<std::string> refused3;
		Check(ShapesStart_MapsFound(root + "\\nothing", &refused3).empty() && refused3.empty(), "a data folder with none of the four folders: no map, and no refusal");
	}

	// A JUNCTION among them is refused and never looked in: Custom made a junction to a folder outside the game
	{
		std::string const outside = root + "\\outside";
		CreateDirectoryA(outside.c_str(), nullptr);
		Write(outside + "\\Stranger.rif", withMedbox);
		TakeAway(data + "\\avp_rifs\\Custom");
		bool const linked = MakeJunction(Wide(data + "\\avp_rifs\\Custom"), Wide(outside));
		std::vector<std::string> refused;
		std::vector<std::string> const maps = ShapesStart_MapsFound(data, &refused);
		std::string const found = Joined(maps, data + "\\");
		Check(linked && found.find("Stranger") == std::string::npos && refused.size() == 1 && refused[0].find("Custom") != std::string::npos,
			"avp_rifs\\Custom a junction: REFUSED, nothing in it taken (\"" + (refused.empty() ? std::string() : refused[0]) + "\")");
		RemoveDirectoryA((data + "\\avp_rifs\\Custom").c_str());   // the junction itself
		CreateDirectoryA((data + "\\avp_rifs\\Custom").c_str(), nullptr);
		Write(data + "\\avp_rifs\\Custom\\Mine.rif", withMedbox);
	}

	// THE BOUNDS (Drake's review, SHOULD 3; Tao 3): a folder holding more than kShapesStartMostAFolder .rif files gives that many and says the rest in one
	// line; a name Windows' code page can't say is counted and never taken (a letter it lacks is never "best fitted" onto another). (A .rif that is itself
	// a symbolic link: host_shape_extract_files_symlink_test, as making one needs the symbolic-link privilege.)
	{
		std::string const many = root + "\\many";
		CreateDirectoryA(many.c_str(), nullptr);
		CreateDirectoryA((many + "\\avp_rifs").c_str(), nullptr);
		bool allMade = true;

		for (size_t k = 0; k <= kShapesStartMostAFolder; k++)
		{
			char name[32];
			sprintf_s(name, "\\avp_rifs\\m%05u.rif", (unsigned)k);
			HANDLE const made = CreateFileA((many + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			allMade = allMade && made != INVALID_HANDLE_VALUE;

			if (made != INVALID_HANDLE_VALUE)
			{
				CloseHandle(made);
			}
		}

		std::wstring const unsayable = Wide(many) + L"\\avp_rifs\\\x4E2D.rif";   // a CJK letter: no single-byte code page has it
		HANDLE const odd = CreateFileW(unsayable.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		allMade = allMade && odd != INVALID_HANDLE_VALUE;

		if (odd != INVALID_HANDLE_VALUE)
		{
			CloseHandle(odd);
		}

		std::vector<std::string> refused;
		std::vector<std::string> const maps = ShapesStart_MapsFound(many, &refused);
		std::string const said = refused.size() == 2 ? refused[0] + " | " + refused[1] : std::string("(" + std::to_string(refused.size()) + " refusals)");
		Check(allMade && maps.size() == kShapesStartMostAFolder && maps.back() == many + "\\avp_rifs\\m04095.rif", "4,097 .rif files in one folder: the first 4,096 "
			"taken, in order (" + std::to_string(maps.size()) + ")");
		Check(refused.size() == 2 && refused[0] == "avp_rifs: 1 more .rif files not taken (at most 4096 a folder)" &&
			refused[1] == "avp_rifs: 1 file names Windows' code page can't say, not taken", "the one past the most said in one line, and the name the code page can't "
			"say counted, never taken (\"" + said + "\")");
		DeleteFileW(unsayable.c_str());   // by its wide name: the code page's (TakeAway's) can't name it
		TakeAway(many);
	}

	// THE STOCK SHAPES MISSING, by the table's names, in any capitals
	{
		Check(ShapesStart_StockShapesMissing(data) == kShapeIdentityRowCount, "no shape_rifs: every one of the table's " + std::to_string(kShapeIdentityRowCount) +
			" names missing");
		CreateDirectoryA((data + "\\shape_rifs").c_str(), nullptr);
		Write(data + "\\shape_rifs\\MEDBOX01.RIF", { 'x' });
		Write(data + "\\shape_rifs\\not-a-stock-name.rif", { 'x' });
		Check(ShapesStart_StockShapesMissing(data) == kShapeIdentityRowCount - 1, "MEDBOX01.RIF there (any capitals): one fewer missing; a name not in the table "
			"changes nothing (" + std::to_string(ShapesStart_StockShapesMissing(data)) + ")");
		DeleteFileA((data + "\\shape_rifs\\MEDBOX01.RIF").c_str());
		DeleteFileA((data + "\\shape_rifs\\not-a-stock-name.rif").c_str());
	}

	// THE FACTS and WHETHER TO ASK: none of these maps is Steam's own, so each is something to search
	{
		std::vector<std::string> refused;
		std::vector<std::string> const maps = ShapesStart_MapsFound(data, &refused);
		FirstLaunchFacts const facts = ShapesStart_Facts("C:\\Tools", data, true, maps);
		Check(facts.found && facts.fromIni && facts.gameFolder == data && facts.programFolder == "C:\\Tools" && facts.levels == 2 && facts.huds == 1,
			"the facts: found, from DataFolder=, the game's folder, 2 levels and 1 HUD file at the folders' tops (" + std::to_string(facts.levels) + ", " +
			std::to_string(facts.huds) + ")");
		Check(facts.shapesToWrite == kShapeIdentityRowCount + maps.size(), "something to search: every stock shape missing plus the " + std::to_string(maps.size()) +
			" maps that aren't Steam's own (" + std::to_string(facts.shapesToWrite) + ")");
		Check(ShapesStart_ShouldAsk(false, facts) && !ShapesStart_ShouldAsk(true, facts), "no list kept yet: the form asks; a list kept: never");

		FirstLaunchFacts nothing = facts;
		nothing.shapesToWrite = 0;
		Check(!ShapesStart_ShouldAsk(false, nothing), "nothing to search (every stock shape there by name, every map Steam's own): no form");
	}

	// THE READ AND THE SEARCH: the bytes searched are the bytes read, even when the file is changed in between (here, to a level with no shape at all)
	{
		std::string const shapeFolder = root + "\\shapes-one";
		std::string const map = data + "\\avp_rifs\\b.RIF";
		ShapesStartSearch search(shapeFolder);
		ShapesTriggerHooks const hooks = search.Hooks();
		std::string sha256;
		std::string const why = hooks.read(map, &sha256);
		Check(why.empty() && sha256 == ShapeExtract_Sha256(withMedbox), "read: b.RIF's bytes, its SHA-256 the bytes' (" + why + ")");

		Write(map, empty);   // changed on the disk after it was read
		ShapesMapSearch const found = hooks.search(map);
		Check(!found.failed && found.written == 1 && ReadAll(shapeFolder + "\\medbox01.rif").size() > 0, "searched: the bytes READ (medbox01 written), not the file as it "
			"is now (no shape) (" + std::to_string(found.written) + " written; " + found.why + ")");
		Write(map, withMedbox);

		ShapesMapSearch const other = hooks.search(data + "\\avp_rifs\\A.rif");
		Check(other.failed && other.written == 0 && other.why.find("wasn't the map just read") != std::string::npos, "a search of a map not just read: refused, nothing "
			"read again (\"" + other.why + "\")");

		std::string unread;
		std::string const missing = hooks.read(data + "\\avp_rifs\\Gone.rif", &unread);
		Check(!missing.empty() && unread.empty() && hooks.search(data + "\\avp_rifs\\Gone.rif").failed, "a map that can't be read: why given, no SHA-256, and no search");
	}

	// THE WHOLE OF IT with Drake's trigger: every map found searched once (Mine.rif holds b.RIF's very bytes: a copy, met once), the shapes written, the list
	{
		std::string const shapeFolder = data + "\\shape_rifs";
		std::vector<std::string> refused;
		std::vector<std::string> const maps = ShapesStart_MapsFound(data, &refused);
		ShapesStartSearch search(shapeFolder);
		ShapesTrigger trigger;
		trigger.Begin(ShapesScannedList(), maps);
		ShapesTriggerHooks const hooks = search.Hooks();
		size_t steps = 0;

		while (trigger.Step(hooks))
		{
			steps++;
		}

		Check(steps == maps.size() && trigger.Done() && trigger.Changed(), "one Step a map: " + std::to_string(steps) + " of " + std::to_string(maps.size()));
		Check(trigger.List().size() == 2 && trigger.Scanned() == 2 && trigger.Written() == 1 && Exists(shapeFolder + "\\medbox01.rif") &&
			Exists(shapeFolder + "\\shapes.txt"), "two maps searched (the empty level's bytes and b.RIF's: every copy met once), medbox01.rif written with its "
			"shapes.txt, both SHA-256s on the list (" + trigger.Words() + ")");
	}

	TakeAway(root);
	Check(!Exists(root), "and the test's folder is taken away");
	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
