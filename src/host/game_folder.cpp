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

	// game_folder.h. (Added 2026-11-22 by Melanikus (Vasquez), F-d.)
	char const *GameFolder_WhyNotPlainBelow(std::string const &gameFolder, std::string const &path, std::string *which)
	{
		if (which)
		{
			which->clear();
		}

		std::string folder = gameFolder;

		while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
		{
			folder.pop_back();
		}

		// Below the data folder only: the folder's whole name, then a slash (compared as Windows compares names, whatever the capitals)
		if (folder.empty() || path.size() <= folder.size() + 1 || _strnicmp(path.c_str(), folder.c_str(), folder.size()) != 0 ||
			(path[folder.size()] != '\\' && path[folder.size()] != '/'))
		{
			return nullptr;
		}

		// Each folder part from there to the file's own name, the file left out: avp_rifs, then avp_rifs\Custom, ...
		std::string at = folder;
		size_t start = folder.size() + 1;

		for (;;)
		{
			size_t const slash = path.find_first_of("\\/", start);

			if (slash == std::string::npos)
			{
				return nullptr;   // the rest is the file's own name
			}

			std::string const part = path.substr(start, slash - start);

			at += "\\" + part;

			if (which)
			{
				*which = at.substr(folder.size() + 1);
			}

			// Windows drops a part's trailing dots and spaces (".. " is ".." to it), so a part like that, an empty one, "." or "..", is refused outright
			if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
			{
				return "a folder on the way has an empty name, a \".\" or \"..\", or a name ending in a dot or a space";
			}

			DWORD const kind = GetFileAttributesA(at.c_str());

			if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return "a folder on the way isn't there, or isn't a folder";
			}

			if (kind & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				return "a folder on the way is a link, or another reparse point, not a plain folder, so the save would go somewhere else";
			}

			start = slash + 1;
		}
	}
}
