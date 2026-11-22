// The test for the game's folder above the editor's (src/host/game_folder.h): the data folder a first start takes when AVP_OpenInteng.ini names none. A program
// folder <the game>\Tools\ whose folder above holds avp_rifs gives that folder; every other one gives nothing: no avp_rifs above, an avp_rifs that is a file, the
// folder above being a whole drive, and the paths that are not a plain one on a drive (a share, Windows' device forms, a relative path), which are refused before
// anything is read. Folders are made in the temp folder and taken away again. Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>

#include <string>

#include <windows.h>

#include "game_folder.h"

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

// The answer for programFolder: true and the folder, or false; gameFolder starts holding words, so an answer of false must leave it empty
static bool Ask(std::string const &programFolder, std::string *gameFolder)
{
	*gameFolder = "left over from before";
	return GameFolder_AboveProgram(programFolder, gameFolder);
}

int main()
{
	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	// Our own folder in the temp folder, named after this run, with a space in the game's name as "Aliens versus Predator Classic" has
	std::string const root = std::string(temp) + "game_folder_test_" + std::to_string(GetCurrentProcessId());
	std::string const game = root + "\\the game";
	std::string const noRifs = root + "\\no rifs";
	std::string const rifsFile = root + "\\rifs a file";
	std::string const capitals = root + "\\capitals";
	bool const made = CreateDirectoryA(root.c_str(), NULL) && CreateDirectoryA(game.c_str(), NULL) && CreateDirectoryA((game + "\\avp_rifs").c_str(), NULL) &&
		CreateDirectoryA((game + "\\Tools").c_str(), NULL) && CreateDirectoryA(noRifs.c_str(), NULL) && CreateDirectoryA((noRifs + "\\Tools").c_str(), NULL) &&
		CreateDirectoryA(rifsFile.c_str(), NULL) && CreateDirectoryA((rifsFile + "\\Tools").c_str(), NULL) && CreateDirectoryA(capitals.c_str(), NULL) &&
		CreateDirectoryA((capitals + "\\AVP_RIFS").c_str(), NULL) && CreateDirectoryA((capitals + "\\Tools").c_str(), NULL);
	HANDLE const file = made ? CreateFileA((rifsFile + "\\avp_rifs").c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL) : INVALID_HANDLE_VALUE;

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	Check(made && file != INVALID_HANDLE_VALUE, "the test's folders are made in the temp folder: " + root);

	std::string found;

	// The editor in the game's Tools folder: the folder above it, without its last "\"
	bool const inTools = Ask(game + "\\Tools\\", &found);
	Check(inTools && found == game, "<the game>\\Tools\\ with <the game>\\avp_rifs: the game's folder, " + game + " (got " + found + ")");
	Check(Ask(game + "\\Tools", &found) && found == game, "and the same without the program folder's last \\");
	Check(Ask(capitals + "\\Tools\\", &found) && found == capitals, "an avp_rifs in capitals (AVP_RIFS) is the same folder to Windows: found");

	// Nothing when the folder above isn't the game's
	Check(!Ask(noRifs + "\\Tools\\", &found) && found.empty(), "no avp_rifs in the folder above: nothing, and the answer is emptied");
	Check(!Ask(rifsFile + "\\Tools\\", &found) && found.empty(), "an avp_rifs that is a file, not a folder: nothing");
	Check(!Ask(game + "\\", &found) && found.empty(), "the program in the game's folder itself, not its Tools: the folder above (the test's own) holds no avp_rifs, so nothing");

	// Nothing when the folder above is a whole drive, or there is none
	Check(!Ask("C:\\Tools\\", &found) && found.empty(), "C:\\Tools\\: the folder above is the whole drive, which the data folder's rule refuses: nothing");
	Check(!Ask("C:\\", &found) && found.empty(), "C:\\: no folder above: nothing");
	Check(!Ask("C:", &found) && found.empty(), "C: alone: nothing");
	Check(!Ask("", &found) && found.empty(), "an empty path: nothing");

	// Hostile or odd forms: refused before anything is read (no share is ever asked, no device opened)
	Check(!Ask("\\\\server\\share\\the game\\Tools\\", &found) && found.empty(), "another computer's share, \\\\server\\share\\...: nothing");
	Check(!Ask("\\\\?\\" + game + "\\Tools\\", &found) && found.empty(), "Windows' \\\\?\\ form of the same folder: nothing");
	Check(!Ask("\\\\.\\" + game + "\\Tools\\", &found) && found.empty(), "Windows' \\\\.\\ device form: nothing");
	Check(!Ask("the game\\Tools\\", &found) && found.empty(), "a relative path, the game\\Tools\\: nothing");
	Check(!Ask("\\the game\\Tools\\", &found) && found.empty(), "a path from the top of the current drive, \\the game\\Tools\\: nothing");
	Check(!Ask("C:the game\\Tools\\", &found) && found.empty(), "a drive's own current folder, C:the game\\Tools\\: nothing");
	Check(!GameFolder_AboveProgram(game + "\\Tools\\", nullptr), "no room for the answer: false");

	// Taken away again
	DeleteFileA((rifsFile + "\\avp_rifs").c_str());
	RemoveDirectoryA((capitals + "\\Tools").c_str());
	RemoveDirectoryA((capitals + "\\AVP_RIFS").c_str());
	RemoveDirectoryA(capitals.c_str());
	RemoveDirectoryA((rifsFile + "\\Tools").c_str());
	RemoveDirectoryA(rifsFile.c_str());
	RemoveDirectoryA((noRifs + "\\Tools").c_str());
	RemoveDirectoryA(noRifs.c_str());
	RemoveDirectoryA((game + "\\Tools").c_str());
	RemoveDirectoryA((game + "\\avp_rifs").c_str());
	RemoveDirectoryA(game.c_str());
	Check(RemoveDirectoryA(root.c_str()) != 0, "and the test's folders are taken away");

	printf("%s: %d failed\n", checksFailed ? "FAILED" : "ok", checksFailed);
	return checksFailed ? 1 : 0;
}
