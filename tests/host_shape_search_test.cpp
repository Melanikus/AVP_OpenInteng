// The test for the lookup order of a shape's file (src/host/shape_search.h). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code". It runs in a scratch folder of its own:   <a scratch folder>
//   (1) the order: ShapeFolder=, then the game's Tools\shape_rifs, then the game's shape_rifs, each with its label; without a setting or a game, fewer; a
//       ShapeFolder= that is one of the game's two (other capitals, the other slash) is there once
//   (2) the first folder holding <name>.rif wins: the game's shape_rifs alone; Tools\shape_rifs before it; ShapeFolder= before both; a ShapeFolder= without the
//       file, or that isn't there, passes to the next
//   (3) FAIL CLOSED, the lookup stops at: a ShapeFolder= that isn't a whole path (even with the game's folders holding the file), a \\server\share one, a
//       <name>.rif there that is a folder, the game's shape_rifs a junction, the game's Tools a junction
//   (4) nothing found: the words name every folder of the order by its label, and no folder of the user's; a name that isn't plain, and an empty order, are
//       refused before any folder is looked in
//   (5) the refresh's order (Frost's ruling 11-23): ShapeFolder= and the game's Tools\shape_rifs, never the game's shape_rifs, even when it holds the file;
//       a ShapeFolder= the user set to that very folder stays
// RED FIRST: on a stand-in that finds nothing (NotFound, an empty order), (1) to (3) fail; (5) on a refresh order that is the whole order.

#include <windows.h>

#include <stdio.h>

#include <string>
#include <vector>

#include "make_junction.h"   // a junction made without a shell
#include "shape_search.h"

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

static bool Folder(std::string const &path)
{
	return CreateDirectoryA(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool Shape(std::string const &path)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const written = fwrite("REBINFF2", 1, 8, file) == 8;

	return fclose(file) == 0 && written;
}

// a junction, made without a shell (make_junction.h: no command line, so the scratch folder from argv reaches none); one there already is taken as it is
static bool Junction(std::string const &link, std::string const &target)
{
	if (GetFileAttributesA(link.c_str()) == INVALID_FILE_ATTRIBUTES && !MakeJunction(link, target))
	{
		return false;
	}

	DWORD const kind = GetFileAttributesA(link.c_str());

	return kind != INVALID_FILE_ATTRIBUTES && (kind & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

static ShapeSearchResult Find(std::string const &setting, std::string const &game, std::string const &name, ShapeSearchFolder *found, std::string *why)
{
	return ShapeSearch_Find(ShapeSearch_Order(setting, game), name, found, why);
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("  FAILED  give a scratch folder: nothing was tested\n");
		return 1;
	}

	std::string const root = std::string(argv[1]) + "\\shape-search";
	std::string const game = root + "\\game";
	std::string const own = root + "\\own";
	bool const made = Folder(argv[1]) && Folder(root) && Folder(game) && Folder(own);

	Check(made, "the scratch folders are made: " + root);

	// ---- (1) the order
	puts("The order:");
	std::vector<ShapeSearchFolder> const all = ShapeSearch_Order("C:\\mine\\shapes\\", "C:\\Games\\AvP\\");

	Check(all.size() == 3 && all[0].path == "C:\\mine\\shapes" && all[0].label == "ShapeFolder=" && all[0].under.empty() &&
		all[1].path == "C:\\Games\\AvP\\Tools\\shape_rifs" && all[1].label == "the game's Tools\\shape_rifs" && all[1].under == "C:\\Games\\AvP" &&
		all[2].path == "C:\\Games\\AvP\\shape_rifs" && all[2].label == "the game's shape_rifs" && all[2].under == "C:\\Games\\AvP",
		"(1) ShapeFolder=, the game's Tools\\shape_rifs, the game's shape_rifs: " + ShapeSearch_Labels(all));
	Check(ShapeSearch_Order("", "C:\\Games\\AvP").size() == 2 && ShapeSearch_Order("C:\\mine", "").size() == 1 && ShapeSearch_Order("", "").empty(),
		"(1) no ShapeFolder=: the game's two; no game: ShapeFolder= alone; neither: none");
	Check(ShapeSearch_Order("c:/games/avp/SHAPE_RIFS", "C:\\Games\\AvP").size() == 2, "(1) a ShapeFolder= that is the game's shape_rifs (other capitals and slashes): once");

	// ---- (5) the refresh's order (Frost's ruling): never the game's shape_rifs
	std::vector<ShapeSearchFolder> const refresh = ShapeSearch_RefreshOrder("C:\\mine\\shapes\\", "C:\\Games\\AvP\\");
	std::vector<ShapeSearchFolder> const chosen = ShapeSearch_RefreshOrder("c:/games/avp/shape_rifs", "C:\\Games\\AvP");

	Check(refresh.size() == 2 && refresh[0].label == "ShapeFolder=" && refresh[1].label == "the game's Tools\\shape_rifs",
		"(5) the refresh's order: ShapeFolder= and the game's Tools\\shape_rifs, not the game's shape_rifs: " + ShapeSearch_Labels(refresh));
	Check(chosen.size() == 2 && chosen[0].label == "ShapeFolder=" && chosen[0].path == "c:/games/avp/shape_rifs",
		"(5) a ShapeFolder= the user set to the game's shape_rifs stays (the user's own choice): " + ShapeSearch_Labels(chosen));

	// ---- (2) the first folder holding it wins
	puts("The first folder holding it wins:");
	ShapeSearchFolder found;
	std::string why;

	// (each look made first, then checked: MSVC builds a check's words before its condition, so words from inside the condition would be the last look's)
	bool const inGame = Folder(game + "\\shape_rifs") && Shape(game + "\\shape_rifs\\sadar.rif");
	ShapeSearchResult got = Find("", game, "sadar", &found, &why);

	Check(inGame && got == ShapeSearchResult::Found && found.label == "the game's shape_rifs" && found.path == game + "\\shape_rifs",
		"(2) only the game's shape_rifs: found there (" + found.label + ")");
	got = ShapeSearch_Find(ShapeSearch_RefreshOrder("", game), "sadar", &found, &why);
	Check(got == ShapeSearchResult::NotFound, "(5) the refresh's order, the game's shape_rifs holding the file: never read there, so not found (" + why + ")");

	bool const inTools = Folder(game + "\\Tools") && Folder(game + "\\Tools\\shape_rifs") && Shape(game + "\\Tools\\shape_rifs\\sadar.rif");

	got = Find("", game, "sadar", &found, &why);
	Check(inTools && got == ShapeSearchResult::Found && found.label == "the game's Tools\\shape_rifs",
		"(2) Tools\\shape_rifs holds it too: found there, before the game's shape_rifs (" + found.label + ")");

	bool const inOwn = Shape(own + "\\sadar.rif");

	got = Find(own, game, "sadar", &found, &why);
	Check(inOwn && got == ShapeSearchResult::Found && found.label == "ShapeFolder=" && found.path == own, "(2) ShapeFolder= holds it too: found there, before both (" +
		found.label + ")");
	got = Find(own, game, "grenade", &found, &why);
	Check(got == ShapeSearchResult::NotFound, "(2) a name none holds: not found (" + why + ")");
	DeleteFileA((own + "\\sadar.rif").c_str());
	got = Find(own, game, "sadar", &found, &why);
	Check(got == ShapeSearchResult::Found && found.label == "the game's Tools\\shape_rifs", "(2) ShapeFolder= without the file: on to the next (" + found.label + ")");
	got = Find(root + "\\not-there", game, "sadar", &found, &why);
	Check(got == ShapeSearchResult::Found && found.label == "the game's Tools\\shape_rifs" && why.empty(),
		"(2) a ShapeFolder= that isn't there: passed over without a word (" + found.label + ")");

	// ---- (3) fail closed
	puts("Fail closed:");
	auto stopped = [&](std::string const &setting, std::string const &at, ShapeSearchResult want, std::string const &label, std::string const &what)
	{
		ShapeSearchResult const got = Find(setting, at, "sadar", &found, &why);

		Check(got == want && found.label == label && why.find(label) != std::string::npos, "(3) " + what + ": stopped at " + found.label + " (" + why + ")");
	};

	stopped("shapes", game, ShapeSearchResult::FolderRefused, "ShapeFolder=", "a ShapeFolder= that isn't a whole path, the game's folders holding the file");
	stopped("\\\\server\\share\\shapes", game, ShapeSearchResult::FolderRefused, "ShapeFolder=", "a ShapeFolder= on another computer (\\\\server\\share)");
	Check(Folder(own + "\\sadar.rif"), "(3) a folder named sadar.rif made in ShapeFolder=");
	stopped(own, game, ShapeSearchResult::FileRefused, "ShapeFolder=", "sadar.rif in ShapeFolder= a folder, not a file");
	RemoveDirectoryA((own + "\\sadar.rif").c_str());

	std::string const game2 = root + "\\game2";
	std::string const elsewhere = root + "\\elsewhere";
	bool const joined = Folder(game2) && Folder(elsewhere) && Shape(elsewhere + "\\sadar.rif") && Junction(game2 + "\\shape_rifs", elsewhere);

	Check(joined, "(3) game2's shape_rifs made a junction to a folder holding sadar.rif");
	stopped("", game2, ShapeSearchResult::FolderRefused, "the game's shape_rifs", "the game's shape_rifs a junction");

	std::string const game3 = root + "\\game3";
	std::string const tools3 = root + "\\tools3";
	bool const toolsJoined = Folder(game3) && Folder(tools3) && Folder(tools3 + "\\shape_rifs") && Shape(tools3 + "\\shape_rifs\\sadar.rif") &&
		Junction(game3 + "\\Tools", tools3);

	Check(toolsJoined, "(3) game3's Tools made a junction to a folder whose shape_rifs holds sadar.rif");
	stopped("", game3, ShapeSearchResult::FolderRefused, "the game's Tools\\shape_rifs", "the game's Tools a junction (a folder on the way)");

	// ---- (4) nothing found, and refused before looking
	puts("Nothing found:");
	ShapeSearchResult const none = Find(own, game, "grenade", &found, &why);

	Check(none == ShapeSearchResult::NotFound && why.find("ShapeFolder=") != std::string::npos && why.find("Tools\\shape_rifs") != std::string::npos &&
		why.find("the game's shape_rifs") != std::string::npos && why.find(root) == std::string::npos, "(4) the words name the three by label, no folder of the user's: " + why);
	bool const dotted = Find(own, game, "a.b", &found, &why) == ShapeSearchResult::BadName;
	bool const device = Find(own, game, "nul", &found, &why) == ShapeSearchResult::BadName;

	Check(dotted && device, "(4) a name with a dot, a device's name: refused before any folder (" + why + ")");
	got = ShapeSearch_Find({}, "sadar", &found, &why);
	Check(got == ShapeSearchResult::NothingToLook, "(4) an empty order: nothing to look in (" + why + ")");

	// ---- our own files away (RemoveDirectory on a junction takes the junction away, never what it points at)
	DeleteFileA((game + "\\shape_rifs\\sadar.rif").c_str());
	DeleteFileA((game + "\\Tools\\shape_rifs\\sadar.rif").c_str());
	DeleteFileA((elsewhere + "\\sadar.rif").c_str());
	DeleteFileA((tools3 + "\\shape_rifs\\sadar.rif").c_str());
	RemoveDirectoryA((game2 + "\\shape_rifs").c_str());
	RemoveDirectoryA((game3 + "\\Tools").c_str());

	for (std::string const &folder : { game + "\\shape_rifs", game + "\\Tools\\shape_rifs", game + "\\Tools", game, game2, game3, tools3 + "\\shape_rifs", tools3, elsewhere,
		own, root })
	{
		RemoveDirectoryA(folder.c_str());
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
