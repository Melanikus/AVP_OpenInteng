// level_save.cpp: Save over the opened file with a backup (level_save.h says the rule and the order). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "level_save.h"

#include <ctype.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "level_file.h"

namespace host
{
	namespace
	{
		bool EndsWithRif(std::string const &path)
		{
			return path.size() > 4 && _stricmp(path.c_str() + path.size() - 4, ".rif") == 0;
		}

		std::string Reason(char const *what, std::string const &path)
		{
			DWORD const code = GetLastError();

			return std::string(what) + ": " + path + (code ? " (Windows error " + std::to_string((unsigned long)code) + ")" : std::string());
		}

		// The target is one the editor writes over: a .rif with room for ".saving" and ".bak" in its path, not a folder, not read-only. *exists: a file is there now.
		LevelSaveOver TargetIsUsable(OpenLevelFile *open, std::string const &target, bool *exists, std::string *why)
		{
			if (!open || !open->Level() || target.empty() || !EndsWithRif(target) || target.size() + 16 >= MAX_PATH)
			{
				*why = "no level, or the file to save over is not a .rif: " + target;
				return LevelSaveOver::BadTarget;
			}

			DWORD const attributes = GetFileAttributesA(target.c_str());

			*exists = attributes != INVALID_FILE_ATTRIBUTES;

			if (*exists && (attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				*why = "the level's path is a folder: " + target;
				return LevelSaveOver::BadTarget;
			}

			if (*exists && (attributes & FILE_ATTRIBUTE_READONLY))
			{
				*why = "the file is read-only: " + target;
				return LevelSaveOver::ReadOnly;
			}

			return LevelSaveOver::Saved;
		}

		// Bytes written to a file that must not be there yet (CREATE_NEW: a file or a link of that name is never written through), flushed to the disk
		bool WriteNewFile(std::string const &path, std::vector<char> const &bytes)
		{
			HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			bool good = true;
			size_t done = 0;

			while (good && done < bytes.size())
			{
				DWORD const want = (DWORD)(bytes.size() - done < (1u << 24) ? bytes.size() - done : (1u << 24));
				DWORD wrote = 0;

				good = WriteFile(file, bytes.data() + done, want, &wrote, nullptr) && wrote == want;
				done += wrote;
			}

			good = FlushFileBuffers(file) && good;
			good = CloseHandle(file) && good;
			return good;
		}

		// Steps 2 and 3 of a write over the target (level_save.h): the temporary file read back through the game's own loader, then put in place. On a failure the temporary file is removed
		// and the target is as it was.
		LevelSaveOver PutInPlace(std::string const &target, std::string const &temporary, bool exists, bool keepTheOneBefore, std::string *backupPath, std::string *why)
		{
			std::string const backup = target + ".bak";

			// 2. read back through the game's own loader
			{
				OpenLevelFile check;

				if (check.Open(temporary.c_str()) != LevelOpenResult::Opened || !check.Level())
				{
					*why = "the level written did not load back through the game's loader: " + temporary;
					DeleteFileA(temporary.c_str());
					return LevelSaveOver::ReadBackFailed;
				}
			}

			// 3. into place: the file that was there becomes the .bak in the same step (the sitting's first save), or is replaced with any .bak left alone (a later one)
			if (exists && !keepTheOneBefore)
			{
				if (!ReplaceFileA(target.c_str(), temporary.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
				{
					*why = Reason("the level file could not be replaced (it is as it was)", target);
					DeleteFileA(temporary.c_str());
					return LevelSaveOver::ReplaceFailed;
				}
			}
			else if (exists)
			{
				if (!ReplaceFileA(target.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
				{
					// an older .bak in the way: it goes, and the replace is tried once more
					DeleteFileA(backup.c_str());

					if (!ReplaceFileA(target.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
					{
						*why = Reason("the level file could not be replaced (it is as it was)", target);
						DeleteFileA(temporary.c_str());
						return LevelSaveOver::ReplaceFailed;
					}
				}

				if (backupPath)
				{
					*backupPath = backup;
				}
			}
			else if (!MoveFileExA(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
			{
				*why = Reason("the level file could not be made", target);
				DeleteFileA(temporary.c_str());
				return LevelSaveOver::ReplaceFailed;
			}

			why->clear();
			return LevelSaveOver::Saved;
		}
	}

	char const *LevelSaveOverWords(LevelSaveOver result)
	{
		switch (result)
		{
		case LevelSaveOver::Saved: return "saved";
		case LevelSaveOver::BadTarget: return "there is no level file to save over";
		case LevelSaveOver::ReadOnly: return "the level file is read-only";
		case LevelSaveOver::WriteFailed: return "the level could not be written";
		case LevelSaveOver::ReadBackFailed: return "the level written did not load back";
		case LevelSaveOver::ReplaceFailed: return "the level file could not be replaced";
		}

		return "not saved";
	}

	LevelSaveOver LevelSave_Over(OpenLevelFile *open, char const *targetPath, bool keepTheOneBefore, std::string *backupPath, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		if (backupPath)
		{
			backupPath->clear();
		}

		std::string const target = targetPath ? targetPath : "";
		bool exists = false;
		LevelSaveOver const usable = TargetIsUsable(open, target, &exists, why);

		if (usable != LevelSaveOver::Saved)
		{
			return usable;
		}

		// 1. the temporary file, in the target's own folder (so the last step is a rename on one disk)
		std::string const temporary = target + ".saving";

		DeleteFileA(temporary.c_str());

		// the Windows error said with the refusal is the save's own: the delete above leaves "file not found" (2) when there was no ".saving", and a refusal that sets
		// none (the level not held paired, the loader refusing the image) would carry it (Frost 11-17)
		SetLastError(0);

		LevelSaveResult const written = open->SaveAs(temporary.c_str());

		if (written != LevelSaveResult::Saved)
		{
			*why = Reason(written == LevelSaveResult::NothingOpen ? "the level could not be written (the level held is not paired with its bytes)" :
			              written == LevelSaveResult::GameWouldRefuse ? "the level could not be written (the game's loader refused it)" :
			              "the level could not be written", temporary);
			DeleteFileA(temporary.c_str());
			return LevelSaveOver::WriteFailed;
		}

		return PutInPlace(target, temporary, exists, keepTheOneBefore, backupPath, why);
	}

	LevelSaveOver LevelSave_ViewCopy(OpenLevelFile *open, char const *targetPath, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		std::string const target = targetPath ? targetPath : "";
		bool exists = false;
		LevelSaveOver const usable = TargetIsUsable(open, target, &exists, why);

		if (usable != LevelSaveOver::Saved)
		{
			return usable;
		}

		// 1. the working bytes, built without changing the open level, in a temporary file in the target's own folder
		std::string const temporary = target + ".saving";
		std::vector<char> bytes;

		DeleteFileA(temporary.c_str());

		if (!open->BuildLevel(bytes) || bytes.empty())
		{
			*why = "the level as edited could not be put together (its chunks are not paired with its bytes, or not as they were paired): " + target;
			return LevelSaveOver::WriteFailed;
		}

		// the Windows error said with a refused write is the write's own, as LevelSave_Over's (the delete above leaves "file not found" when there was no ".saving")
		SetLastError(0);

		if (!WriteNewFile(temporary, bytes))
		{
			*why = Reason("the level could not be written", temporary);
			DeleteFileA(temporary.c_str());
			return LevelSaveOver::WriteFailed;
		}

		return PutInPlace(target, temporary, exists, false, nullptr, why);
	}

	namespace
	{
		// the whole path, lower case: the sitting's key for a file
		std::string SittingKey(char const *path)
		{
			char whole[MAX_PATH * 2] = "";
			DWORD const got = path ? GetFullPathNameA(path, (DWORD)sizeof(whole), whole, nullptr) : 0;
			std::string key = got > 0 && got < sizeof(whole) ? whole : (path ? path : "");

			for (char &letter : key)
			{
				letter = letter == '/' ? '\\' : (char)tolower((unsigned char)letter);
			}

			return key;
		}
	}

	bool LevelSaveSitting::SavedBefore(char const *targetPath) const
	{
		return saved.count(SittingKey(targetPath)) > 0;
	}

	LevelSaveOver LevelSaveSitting::Save(OpenLevelFile *open, char const *targetPath, std::string *backupPath, std::string *why)
	{
		std::string const key = SittingKey(targetPath);
		bool const first = saved.count(key) == 0;
		std::string written;
		LevelSaveOver const result = LevelSave_Over(open, targetPath, first, &written, why);

		if (result == LevelSaveOver::Saved)
		{
			saved.insert(key);

			if (first && !written.empty())
			{
				kept.insert(key);
			}
		}

		if (backupPath)
		{
			*backupPath = result == LevelSaveOver::Saved && kept.count(key) ? std::string(targetPath) + ".bak" : std::string();
		}

		return result;
	}
}
