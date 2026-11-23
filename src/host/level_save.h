// level_save.h: Save writes the level OVER the file it was opened from, keeping the one before as "<file>.bak" (THE OWNER'S RULING, Melanikus 2026-11-16 through Bishop: "Overwrite, with
// backup"; it replaces owner-intent 7's save-a-copy). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// SAFELY, in this order: the file to save over is checked (below); the level is written to a temporary file in the target's own folder ("<file>.saving"); that file is read back through the
// game's own loader; the file to save over is looked at once more; only then is the new one put in place, the file that was there becoming "<file>.bak" in the same step (ReplaceFileW; an
// older .bak is replaced). On ANY failure the target is untouched, the temporary file is removed, and *why says what and where.
//
// THE FILE SAVED OVER IS ITSELF CHECKED (Vasquez's find F-a, 2026-11-22; Bishop 2's order, Tao 1). The temporary file is written by OpenLevelFile::SaveAs, whose checks of the file it
// replaces therefore only ever saw the temporary name: until then the real file got only a .rif's name, not a folder, not read-only. Now OpenLevelFile::MayWriteOver asks SaveAs's own
// checks of the real file first: it is written over only when it is a plain file on a disk with one name only (never a link, any other reparse point, a device, or one of a hard link's
// names: writing through a link changes some other file, and over one hard link's name leaves the other as it was), and, when it is the file the level stands for, only when it is as
// it was when the level was opened or last saved (a change another program made is never lost without a word). Right before the replace, the file must still be exactly the bytes that
// check read (LevelFile_StillThese), so a change while the save was being made is caught too. After a save the level stands for the file saved over (OpenLevelFile::NowStandsFor). inteng writes over the level it opened with no backup (FUN_004719e0, asking only when the file is read-only or the write fails); the backup is the
// owner's addition, named.
//
// ONE .bak A SITTING (Bishop's ruling B, 2026-11-16 23:4x): the editor saves at inteng's own points with no question (Module Linking's Exit, AutoLink's end, ...), so a .bak that moved on
// with every save would lose the level as it was opened within one AutoLink. LevelSaveSitting (one run of the editor): the FIRST save of a file in the sitting keeps the file as it was as
// "<file>.bak" (an older .bak replaced); every later save of that file in the sitting writes over it and leaves that .bak alone. So the .bak is always "the level before this sitting".
#pragma once

#include <set>
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
		NotAPlainFile,   // the target is not a plain file on a disk with one name only (a link, another reparse point, a device, a hard link's name), so it is never written over
		ChangedOnDisk,   // the target was changed by another program since the level was opened or last saved, or while the save was being made: it is as that program left it
	};

	char const *LevelSaveOverWords(LevelSaveOver result);

	// The open level written over targetPath, as above. With keepTheOneBefore the file that was there becomes "<file>.bak" (an older .bak replaced); without, it is replaced and any .bak is
	// left as it is. *backupPath (when not null) gets the .bak written by this call (empty when none was).
	LevelSaveOver LevelSave_Over(OpenLevelFile *open, char const *targetPath, bool keepTheOneBefore, std::string *backupPath, std::string *why);

	// The view gap's copy (editor_main.cpp, ShowTheWorkingBytes): the open level's working bytes written over targetPath for the game's view to load, as the next save would build them
	// (OpenLevelFile::BuildLevel: unpacked, and not prepared, as inteng's FUN_0046e700 loads its level from the tree in memory as it is), with the same temporary file, read-back and put in
	// place as LevelSave_Over, and no .bak. It is NOT a save: the open level is left exactly as it was (still changed, still standing for the file it was opened from or last saved to, its
	// tree still paired with its bytes), where a SaveAs would have taken the copy for the level's file and paired the tree with the copy's bytes.
	LevelSaveOver LevelSave_ViewCopy(OpenLevelFile *open, char const *targetPath, std::string *why);

	// One sitting's saves, as above. Save: LevelSave_Over keeping the one before only at the file's first save in the sitting (a refused save does not count); *backupPath gets the .bak this
	// sitting kept for the file (written now or at its first save; empty when there was no file before the sitting's first save). SavedBefore: the file had a save in this sitting (the paths
	// compared whole, whichever way the capitals are written).
	class LevelSaveSitting
	{
	public:
		LevelSaveOver Save(OpenLevelFile *open, char const *targetPath, std::string *backupPath, std::string *why);
		bool SavedBefore(char const *targetPath) const;
		// ALL STOCK MAPS (all_stock_maps.h): the file was saved by a caller that wrote its own .bak first (CREATE_NEW, never replacing one) and saved
		// with no backup name: marked as saved, and its .bak as this sitting's, so a later Save by hand in the same sitting leaves that .bak alone (R11)
		void MarkSaved(char const *targetPath);

	private:
		std::set<std::string> saved;   // whole paths, lower case: files saved in this sitting
		std::set<std::string> kept;    // of them, the ones whose .bak this sitting wrote
	};
}
