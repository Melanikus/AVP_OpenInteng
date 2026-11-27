// new_file_save.cpp: see new_file_save.h. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "new_file_save.h"

#include <windows.h>

#include "game_folder.h"     // GameFolder_WhyNotPlainBelow: every folder on the way a plain one
#include "video_import.h"    // VideoImport_ReadPlainFile: the one reader for a file someone could have touched

namespace host
{
	namespace
	{
		std::wstring Wide(std::string const &narrow)
		{
			int const letters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.c_str(), (int)narrow.size(), nullptr, 0);
			std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

			if (letters > 0)
			{
				MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
			}

			return wide;
		}

		std::string Error(DWORD error)
		{
			return "Windows error " + std::to_string((unsigned long long)error);
		}
	}

	NewFileSaved NewFile_Save(std::string const &root, std::vector<std::string> const &folders, std::string const &name, std::vector<uint8_t> const &bytes,
		std::string const &shown, uint64_t mostBytes, char const *what, std::string *why)
	{
		std::string notSaid;

		why = why ? why : &notSaid;
		why->clear();

		std::string at = root;

		while (!at.empty() && (at.back() == '\\' || at.back() == '/'))
		{
			at.pop_back();
		}

		if (at.empty() || name.empty() || name.find_first_of("\\/") != std::string::npos)
		{
			*why = "no folder to save " + shown + " in, or a name that isn't one part";
			return NewFileSaved::Refused;
		}

		std::string const base = at;
		std::vector<std::string> made;

		for (std::string const &part : folders)
		{
			if (part.empty() || part.find_first_of("\\/") != std::string::npos)
			{
				*why = "a folder on the way to " + shown + " isn't one part";
				return NewFileSaved::Refused;
			}

			at += "\\" + part;
			made.push_back(at);
		}

		std::string const full = at + "\\" + name;

		if (full.size() >= MAX_PATH)
		{
			*why = shown + "'s whole path would be " + std::to_string((unsigned long long)full.size()) + " letters, more than the " +
				std::to_string((unsigned long long)(MAX_PATH - 1)) + " Windows opens";
			return NewFileSaved::Refused;
		}

		// THE FOLDERS, one at a time: each made when missing, then it and every folder above it down from the root held plain before anything is made in it
		for (std::string const &each : made)
		{
			if (!CreateDirectoryA(each.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
			{
				*why = "couldn't make the folder " + each.substr(base.size() + 1) + " (" + Error(GetLastError()) + ")";
				return NewFileSaved::Refused;
			}

			std::string which;
			char const *const notPlain = GameFolder_WhyNotPlainBelow(base, each + "\\" + name, &which);

			if (notPlain)
			{
				*why = which + ": " + notPlain;
				return NewFileSaved::Refused;
			}
		}

		std::wstring const wide = Wide(full);

		if (wide.empty())
		{
			*why = "the folder's name can't be read as a path";
			return NewFileSaved::Refused;
		}

		// THE FILE: CREATE_NEW, so one there already (or a link of that name) is never written over or through. DELETE, so a half-written one is deleted through
		// this very handle
		HANDLE const file = CreateFileW(wide.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
			{
				// ALREADY THERE: read back as a plain file (a link of that name is refused, never followed); the very same bytes are taken as they are
				std::vector<uint8_t> there;
				std::string thereWhy;
				VideoImportResult const read = VideoImport_ReadPlainFile(wide, &there, &thereWhy, mostBytes, what, "the file");

				if (read == VideoImportResult::Done && there == bytes)
				{
					return NewFileSaved::AlreadyThere;
				}

				*why = shown + " is there already, " + (read == VideoImportResult::Done ? std::string("with other bytes,") :
					"and can't be read as a plain file (" + thereWhy + "),") + " and is never written over: give the file another name (or move that one away first)";
				return NewFileSaved::Refused;
			}

			*why = "couldn't make " + shown + " (" + Error(error) + ")";
			return NewFileSaved::Refused;
		}

		size_t done = 0;
		bool written = true;

		while (written && done < bytes.size())
		{
			DWORD wrote = 0;
			size_t const left = bytes.size() - done;

			written = WriteFile(file, bytes.data() + done, (DWORD)(left < 65536 ? left : 65536), &wrote, nullptr) && wrote > 0;
			done += wrote;
		}

		written = written && FlushFileBuffers(file);

		if (!written)
		{
			DWORD const error = GetLastError();
			FILE_DISPOSITION_INFO gone = {};

			gone.DeleteFile = TRUE;
			SetFileInformationByHandle(file, FileDispositionInfo, &gone, sizeof(gone));
			CloseHandle(file);
			*why = "couldn't write " + shown + " (" + Error(error) + "); the half-written file is deleted";
			return NewFileSaved::Refused;
		}

		if (!CloseHandle(file))
		{
			*why = shown + " was written but couldn't be closed (" + Error(GetLastError()) + ")";
			return NewFileSaved::Refused;
		}

		// READ BACK by its name, as the game will open it
		std::vector<uint8_t> back;
		std::string readWhy;
		VideoImportResult const read = VideoImport_ReadPlainFile(wide, &back, &readWhy, bytes.size() + 1, what, "the file");

		if (read != VideoImportResult::Done || back != bytes)
		{
			*why = shown + " was written, but reading it back " + (read != VideoImportResult::Done ? "was refused (" + readWhy + ")" : std::string("gave other bytes")) +
				": it is left there to look at";
			return NewFileSaved::Refused;
		}

		return NewFileSaved::Made;
	}
}
