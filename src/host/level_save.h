// level_save.h: Save writes the level OVER the file it was opened from, keeping the one before as "<file>.bak" (THE OWNER'S RULING, Melanikus 2026-11-16 through Bishop: "Overwrite, with
// backup"; it replaces owner-intent 7's save-a-copy). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// SAFELY, in this order: the level is written to a temporary file in the target's own folder ("<file>.saving"); that file is read back through the game's own loader; only when it loads is
// it put in place, the file that was there becoming "<file>.bak" in the same step (ReplaceFileW; an older .bak is replaced). On ANY failure the target is untouched, the temporary file is
// removed, and *why says what and where. inteng writes over the level it opened with no backup (FUN_004719e0, asking only when the file is read-only or the write fails); the backup is the
// owner's addition, named.
#pragma once

#include <string>

class OpenLevelFile;

namespace host
{
	enum class LevelSaveOver
	{
		Saved,           // the target holds the level; *backupPath the one before (empty when there was none)
		BadTarget,       // no target, or not a ".rif" file
		ReadOnly,        // the target is read-only (inteng: "%s is read-only\nWould you like to try again?")
		WriteFailed,     // the temporary file could not be written (the game's loader refused the image, or the disk)
		ReadBackFailed,  // the temporary file did not load back through the game's loader
		ReplaceFailed,   // the temporary file could not be put in place (the target is as it was)
	};

	char const *LevelSaveOverWords(LevelSaveOver result);

	// The open level written over targetPath, as above. *backupPath (when not null) gets the backup's path when there was a file to keep.
	LevelSaveOver LevelSave_Over(OpenLevelFile *open, char const *targetPath, std::string *backupPath, std::string *why);
}
