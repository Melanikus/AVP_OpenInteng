// See save_copy.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "save_copy.h"

#include <string.h>

#include <windows.h>   // GetFullPathNameA, MAX_PATH, CompareStringA

namespace host
{
	namespace
	{
		size_t const kMostNameLetters = 64;

		char const *const kNoOpened = "there is no opened file to name the copy after";
		char const *const kNoOwnFolder = "the folder for copies (Documents\\AVP_OpenInteng) isn't known";
		char const *const kNoGameFolder = "the game's folder isn't known";
		char const *const kPathTooLong = "the copy's path is too long";
		char const *const kSameAsOpened = "the copy would be the opened file itself, which a save never replaces";
		char const *const kInGameFolder = "the copy would be inside the game's folder, where a save never writes";
		char const *const kOwnInGameFolder = "the folder for copies (Documents\\AVP_OpenInteng) is inside the game's folder, so nothing is saved there";

		// A path as a whole path, with its slashes as '\' and no ".." or "." parts left: what Windows says it is. False when it can't be made
		// whole in MAX_PATH letters.
		bool WholePath(char const *path, std::string *whole)
		{
			char full[MAX_PATH];
			DWORD const length = GetFullPathNameA(path, MAX_PATH, full, nullptr);

			if (length == 0 || length >= MAX_PATH)
			{
				return false;
			}

			*whole = std::string(full, length);
			return true;
		}

		// Whether two whole paths are the same one, the way Windows compares file names: by letter, capitals and small letters alike
		bool SamePath(std::string const &a, std::string const &b)
		{
			return a.size() == b.size() && CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, a.c_str(), (int)a.size(), b.c_str(), (int)b.size()) == CSTR_EQUAL;
		}

		// Whether whole path inside lies inside folder (folder ends in one '\'): its first letters are the folder's, so the two never meet
		// by only part of a name ("C:\game2\x" is not inside "C:\game\")
		bool IsInside(std::string const &inside, std::string const &folder)
		{
			return inside.size() > folder.size() && SamePath(inside.substr(0, folder.size()), folder);
		}

		std::string WithEndSlash(std::string folder)
		{
			if (folder.empty() || folder.back() != '\\')
			{
				folder += '\\';
			}

			return folder;
		}
	}

	std::string SaveCopyBase(char const *documentsFolder)
	{
		std::string documents;

		if (!documentsFolder || !documentsFolder[0] || !WholePath(documentsFolder, &documents))
		{
			return std::string();
		}

		std::string const base = WithEndSlash(documents) + "AVP_OpenInteng\\";

		// room after it for "edited\", the longest name (64 letters) and "-edited.rif"
		return base.size() + 7 + kMostNameLetters + 11 < MAX_PATH ? base : std::string();
	}

	std::string SaveCopyName(char const *openedPath)
	{
		std::string const path = openedPath ? std::string(openedPath, strnlen(openedPath, MAX_PATH)) : std::string();

		// Only the file's own name: after the last slash of either kind
		size_t const slash = path.find_last_of("\\/");
		std::string name = slash == std::string::npos ? path : path.substr(slash + 1);

		// Without its extension, the last dot's and what follows
		size_t const dot = name.find_last_of('.');

		if (dot != std::string::npos)
		{
			name.erase(dot);
		}

		std::string safe;

		for (char const letter : name)
		{
			if (safe.size() >= kMostNameLetters)
			{
				break;
			}

			bool const plain = (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') ||
				letter == '_' || letter == '-';

			safe += plain ? letter : '_';
		}

		return (safe.empty() ? std::string("level") : safe) + "-edited.rif";
	}

	namespace
	{
		// SaveCopyPlace and SaveExportPlace: the same rules, for a file name already made
		bool PlaceFor(char const *openedPath, std::string const &fileName, char const *ownFolder, char const *gameFolder, std::string *copyPath,
			std::string *copyFolder, char const **why);
	}

	bool SaveCopyPlace(char const *openedPath, char const *ownFolder, char const *gameFolder, std::string *copyPath, std::string *copyFolder,
		char const **why)
	{
		return PlaceFor(openedPath, SaveCopyName(openedPath), ownFolder, gameFolder, copyPath, copyFolder, why);
	}

	std::string SaveExportName(char const *typedName, char const *extension)
	{
		// The same letters as a copy's name: letters, digits, _ and -, at most 64, never empty
		std::string safe;

		for (char const *letter = typedName; letter && *letter && safe.size() < kMostNameLetters; letter++)
		{
			bool const plain = (*letter >= 'A' && *letter <= 'Z') || (*letter >= 'a' && *letter <= 'z') || (*letter >= '0' && *letter <= '9') ||
				*letter == '_' || *letter == '-';

			safe += plain ? *letter : '_';
		}

		return (safe.empty() ? std::string("export") : safe) + (extension ? extension : "");
	}

	bool SaveExportPlace(char const *openedPath, char const *typedName, char const *extension, char const *ownFolder, char const *gameFolder,
		std::string *path, std::string *folder, char const **why)
	{
		return PlaceFor(openedPath, SaveExportName(typedName, extension), ownFolder, gameFolder, path, folder, why);
	}

	namespace
	{
	bool PlaceFor(char const *openedPath, std::string const &fileName, char const *ownFolder, char const *gameFolder, std::string *copyPath,
		std::string *copyFolder, char const **why)
	{
		char const *ignored = nullptr;

		if (!why)
		{
			why = &ignored;
		}

		*why = nullptr;

		if (!copyPath || !copyFolder)
		{
			*why = kNoOpened;
			return false;
		}

		copyPath->clear();
		copyFolder->clear();

		if (!openedPath || !openedPath[0])
		{
			*why = kNoOpened;
			return false;
		}

		if (!ownFolder || !ownFolder[0])
		{
			*why = kNoOwnFolder;
			return false;
		}

		if (!gameFolder || !gameFolder[0])
		{
			*why = kNoGameFolder;
			return false;
		}

		std::string own;
		std::string game;
		std::string opened;

		if (!WholePath(ownFolder, &own) || !WholePath(gameFolder, &game) || !WholePath(openedPath, &opened))
		{
			*why = kPathTooLong;
			return false;
		}

		own = WithEndSlash(own);
		game = WithEndSlash(game);

		// The editor's own folder is not the game's, and not inside it: the copy would land there
		if (SamePath(own, game) || IsInside(own, game))
		{
			*why = kOwnInGameFolder;
			return false;
		}

		std::string const folder = own + "edited\\";
		std::string const path = folder + fileName;
		std::string wholeFolder;
		std::string wholePath;

		if (path.size() >= MAX_PATH || !WholePath(folder.c_str(), &wholeFolder) || !WholePath(path.c_str(), &wholePath))
		{
			*why = kPathTooLong;
			return false;
		}

		// The opened file is never the copy, and nothing is written in the game's folder
		if (SamePath(wholePath, opened))
		{
			*why = kSameAsOpened;
			return false;
		}

		if (IsInside(wholePath, game))
		{
			*why = kInGameFolder;
			return false;
		}

		*copyPath = wholePath;
		*copyFolder = WithEndSlash(wholeFolder);
		return true;
	}
	}
}
