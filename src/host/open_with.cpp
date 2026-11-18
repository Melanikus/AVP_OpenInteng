// open_with.cpp: opening a level file given to the editor on its command line. The rules, and why, are in open_with.h.
// Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "open_with.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <vector>

#include "level_file_name.h"

namespace
{

const char *const kOpenedFolder = "OpenedByEditor";   // inside the data folder's avp_rifs: only the editor puts files there

void Say(char *why, size_t whySize, const char *text)
{
	if (why && whySize)
	{
		_snprintf_s(why, whySize, _TRUNCATE, "%s", text);
	}
}

// Reads a whole file of at most "most" bytes, through its own wide path. False if it can't be read whole, or is bigger.
bool ReadWhole(const wchar_t *path, size_t most, std::vector<char> &bytes)
{
	bytes.clear();
	HANDLE const file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool ok = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && (unsigned long long)size.QuadPart <= most;
	if (ok)
	{
		bytes.resize((size_t)size.QuadPart);
		DWORD got = 0;
		ok = bytes.empty() || (ReadFile(file, bytes.data(), (DWORD)bytes.size(), &got, NULL) && got == bytes.size());
	}

	CloseHandle(file);
	return ok;
}

}

bool OpenWith_TakeLevel(const wchar_t *path, const char *dataFolder, char *levelName, size_t levelNameSize, char *why, size_t whySize)
{
	if (!path || !path[0] || !dataFolder || !dataFolder[0] || !levelName || levelNameSize == 0)
	{
		Say(why, whySize, "no level file was named");
		return false;
	}
	levelName[0] = '\0';

	// Not a name in Windows' device space (it starts \\.\), such as a pipe: only a file on a disk is a level file
	if (wcsncmp(path, L"\\\\.\\", 4) == 0)
	{
		Say(why, whySize, "that's not a file on a disk");
		return false;
	}

	// The file's own name: everything after its last slash. The rest of the path is only ever handed to Windows to read the file.
	size_t const length = wcslen(path);
	size_t nameStart = 0;
	for (size_t i = 0; i < length; i++)
	{
		if (path[i] == L'\\' || path[i] == L'/')
		{
			nameStart = i + 1;
		}
	}
	size_t const nameLength = length - nameStart;

	// It has to end in .rif, in capitals or not, with something before it
	if (nameLength <= 4 || _wcsicmp(path + length - 4, L".rif") != 0)
	{
		Say(why, whySize, "the editor opens level files, whose names end in .rif");
		return false;
	}

	// The part before .rif becomes the level's name. Only plain letters can pass the game's rule below, so anything else is refused
	// here, before the wide text is ever turned narrow.
	std::string name;
	for (size_t i = nameStart; i < length - 4; i++)
	{
		wchar_t const letter = path[i];
		if (letter < 0x20 || letter > 0x7E)
		{
			Say(why, whySize, "the file's name has a letter no game file's name has: rename it with plain letters and digits");
			return false;
		}
		name += (char)letter;
	}

	const char *const refused = LevelFileName_WhyRefused((std::string(kOpenedFolder) + "/" + name).c_str());
	if (refused)
	{
		Say(why, whySize, (std::string("the file's name can't be a level's name (") + refused + "): rename it").c_str());
		return false;
	}
	std::string const gameName = std::string(kOpenedFolder) + "/" + name;

	if (gameName.size() >= levelNameSize)
	{
		Say(why, whySize, "the file's name is too long");
		return false;
	}

	// A regular file: not a folder, a link (or any other reparse point) or a device, and at most 16 MB
	WIN32_FILE_ATTRIBUTE_DATA facts;
	if (!GetFileAttributesExW(path, GetFileExInfoStandard, &facts))
	{
		Say(why, whySize, "the file can't be found, or can't be read");
		return false;
	}
	if (facts.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))
	{
		Say(why, whySize, "that's a folder or a link, not a level file");
		return false;
	}
	unsigned long long const bytes = ((unsigned long long)facts.nFileSizeHigh << 32) | facts.nFileSizeLow;
	if (bytes > (unsigned long long)kOpenWithMostBytes)
	{
		Say(why, whySize, "the file is bigger than 16 MB, which no level is");
		return false;
	}

	// Where it goes: the data folder's avp_rifs, under its own name. The data folder is the user's own setting, in the narrow text the
	// settings file gives, so it's made wide the same way Windows would.
	std::string const folder = std::string(dataFolder) + "\\avp_rifs\\" + kOpenedFolder;
	std::string const place = folder + "\\" + name + ".rif";
	int const wideLength = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, place.c_str(), -1, NULL, 0);
	if (wideLength <= 0 || wideLength > 32767)
	{
		Say(why, whySize, "the data folder's name can't be used");
		return false;
	}
	std::vector<wchar_t> placeWide((size_t)wideLength);
	MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, place.c_str(), -1, placeWide.data(), wideLength);

	// The editor's own folder for opened files is made when it isn't there; the copy replaces the editor's own earlier one (a file given from inside that folder is its own copy: nothing to do)
	CreateDirectoryA(folder.c_str(), NULL);

	wchar_t fullGiven[MAX_PATH * 2];
	wchar_t fullPlace[MAX_PATH * 2];
	DWORD const givenLength = GetFullPathNameW(path, MAX_PATH * 2, fullGiven, NULL);
	DWORD const placeLength = GetFullPathNameW(placeWide.data(), MAX_PATH * 2, fullPlace, NULL);
	bool const itself = givenLength > 0 && givenLength < MAX_PATH * 2 && placeLength > 0 && placeLength < MAX_PATH * 2 && _wcsicmp(fullGiven, fullPlace) == 0;

	// A READ-ONLY FILE OPENS (TB-C10, OUR BUG, FIXED: inteng opens it and says "This file is read only" at the hub's start): Windows' copy keeps the read-only mark, so the editor's own
	// copy would be read-only and the next copy over it refused. The copy is the editor's own file: an earlier one loses the mark before the copy, the new one after it.
	if (!itself)
	{
		DWORD const before = GetFileAttributesW(placeWide.data());

		if (before != INVALID_FILE_ATTRIBUTES && (before & FILE_ATTRIBUTE_READONLY) && !(before & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
		{
			SetFileAttributesW(placeWide.data(), before & ~FILE_ATTRIBUTE_READONLY);
		}
	}

	if (!itself && !CopyFileW(path, placeWide.data(), FALSE))
	{
		Say(why, whySize, "the file couldn't be copied into the editor's own OpenedByEditor folder");
		return false;
	}

	if (!itself)
	{
		DWORD const after = GetFileAttributesW(placeWide.data());

		if (after != INVALID_FILE_ATTRIBUTES && (after & FILE_ATTRIBUTE_READONLY))
		{
			SetFileAttributesW(placeWide.data(), after & ~FILE_ATTRIBUTE_READONLY);
		}
	}

	strcpy_s(levelName, levelNameSize, gameName.c_str());
	return true;
}
