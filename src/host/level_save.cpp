// level_save.cpp: Save over the opened file with a backup (level_save.h says the rule and the order). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "level_save.h"

#include <ctype.h>
#include <string.h>

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

		if (!open || !open->Level() || target.empty() || !EndsWithRif(target) || target.size() + 16 >= MAX_PATH)
		{
			*why = "no level, or the file to save over is not a .rif: " + target;
			return LevelSaveOver::BadTarget;
		}

		DWORD const attributes = GetFileAttributesA(target.c_str());
		bool const exists = attributes != INVALID_FILE_ATTRIBUTES;

		if (exists && (attributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			*why = "the level's path is a folder: " + target;
			return LevelSaveOver::BadTarget;
		}

		if (exists && (attributes & FILE_ATTRIBUTE_READONLY))
		{
			*why = "the file is read-only: " + target;
			return LevelSaveOver::ReadOnly;
		}

		// 1. the temporary file, in the target's own folder (so the last step is a rename on one disk)
		std::string const temporary = target + ".saving";
		std::string const backup = target + ".bak";

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
