// level_save.cpp: Save over the opened file with a backup (level_save.h says the rule and the order). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "level_save.h"

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

	LevelSaveOver LevelSave_Over(OpenLevelFile *open, char const *targetPath, std::string *backupPath, std::string *why)
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

		if (open->SaveAs(temporary.c_str()) != LevelSaveResult::Saved)
		{
			*why = Reason("the level could not be written", temporary);
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

		// 3. into place: the file that was there becomes the .bak in the same step
		if (exists)
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
