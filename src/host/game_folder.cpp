// game_folder.cpp: the game's folder above the editor's (game_folder.h says why, and the rules it keeps).
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "game_folder.h"

#include <windows.h>

namespace host
{
	bool GameFolder_AboveProgram(std::string const &programFolder, std::string *gameFolder)
	{
		if (gameFolder == nullptr)
		{
			return false;
		}

		gameFolder->clear();

		// Only a plain path on a drive: a letter, a colon and a backslash ("C:\"). A share ("\\server\..."), a device form ("\\?\", "\\.\") or a path without
		// its drive is refused here, before any file is looked at
		bool const driveLetter = programFolder.size() >= 3 && ((programFolder[0] >= 'A' && programFolder[0] <= 'Z') || (programFolder[0] >= 'a' && programFolder[0] <= 'z'));

		if (!driveLetter || programFolder[1] != ':' || programFolder[2] != '\\')
		{
			return false;
		}

		// The program's folder without its last "\", then without its own name ("Tools"): what is left is the folder above it
		std::string above = programFolder;

		if (above.back() == '\\')
		{
			above.pop_back();
		}

		size_t const slash = above.find_last_of('\\');

		// The slash just after the drive's colon is at 2: a folder above that would be the whole drive ("C:")
		if (slash == std::string::npos || slash <= 2)
		{
			return false;
		}

		above.resize(slash);

		// It is the game's folder only when it holds a folder named avp_rifs, where the game keeps its levels
		DWORD const kind = GetFileAttributesA((above + "\\avp_rifs").c_str());

		if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			return false;
		}

		*gameFolder = above;
		return true;
	}
}
