// rim_save.h: an imported texture saved as a new RIM file of the mapper's own, beside the game (the RIM import's step 6: AvP-docs director/plan-rim-import.md).
// Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE IT GOES. The game opens a level's picture as "graphics/" + the name its BMPNAMES list holds (avpx/3dc/avp/win95/projload.cpp, load_rif_bitmap): a loose
// file first, then its fastfiles. So an imported texture is saved as <data folder>\graphics\Custom\<level>\<texture>.RIM, and the list names it
// Custom\<level>\<texture>.RIM: a folder of the mapper's own, apart from the game's Envrnmts, one folder for each level.
// THE NAME passes the game's own rule for a name from a level (LevelFileName_WhyRefused, avpx/3dc/level_file_name.h: only its letters, at most 200 bytes, no
// part that goes up a folder, ends in a dot or a space, or is a device's name). The level's part and the texture's part are one part each (no slash in either).
// The whole path, the data folder in front, fits what Windows opens (259 letters).
// NEVER OVER ANOTHER FILE: the file is made with CREATE_NEW, so a file there already (a stock one, an earlier import, a link) is never written over or through;
// the name is refused, naming the file. (The game keeps each picture by its name for as long as it runs, so a new picture under an old name wouldn't show anyway.)
// PLAIN FOLDERS ONLY: graphics, graphics\Custom and the level's folder are made when they're missing, one at a time, and each is held to
// GameFolder_WhyNotPlainBelow before anything is made in it: every folder from the data folder down is a plain folder, so a link among them can't send the file
// (or a folder) somewhere else.
// READ BACK: the bytes are flushed to the disk and the file closed, then read again by its name through the editor's one reader for a file someone could have
// touched (VideoImport_ReadPlainFile: no link, plain folders) and compared byte for byte; the save counts only when the file the game will open holds exactly
// what was written. A file this call made and couldn't finish writing is deleted through the very handle that made it (so nothing else can be deleted in its
// place). One that reads back wrong is left where it is and named: once closed it may no longer be this call's to delete. The game's own loader reads the new
// file in step 8, when Texture Map shows it.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	enum class RimSaveResult
	{
		Saved,
		BadCall,      // no bytes, too many, an entry RimSave_EntryName wouldn't give, or no data folder
		BadName,      // the whole path is longer than Windows opens
		NotPlain,     // a folder on the way is a link, a file, or can't be made
		NameTaken,    // a file of that name is there already: never written over
		CantWrite,    // the disk refused the bytes (the half-written file is deleted)
		NotReadBack,  // the disk doesn't hold exactly what was written
	};

	// The most bytes a texture file may have: a 512 x 512 one of rim_write.h is 263,048, so this leaves room and refuses a mistake
	uint64_t const kRimSaveMostBytes = 1024 * 1024;

	// The name the level's BMPNAMES list will hold, "Custom\<level>\<texture>.RIM". levelFile: the level's file name or its whole path (its last part, without a
	// ".rif" ending in any capitals, is the level's folder: "Genshd1.RIF" is "Genshd1"). texture: the mapper's name for it, with or without ".rim" in any capitals.
	// False, *entry empty and *why saying why, when either part is empty or holds a slash, or the whole fails the game's rule
	bool RimSave_EntryName(std::string const &levelFile, std::string const &texture, std::string *entry, std::string *why);

	// A texture name made from a picture's file name, for the form to offer first: its last part without its ending, each letter the game's rule refuses made "_",
	// the second dot of two together made "_", at most 32 bytes, dots and spaces at its end dropped, "tex_" in front of a device's name; "texture" when nothing is
	// left. It always passes RimSave_EntryName with a plain level name
	std::string RimSave_SuggestedName(std::string const &pictureFile);

	// bytes (a RIM: rim_write.h's) saved as dataFolder\graphics\<entry>, entry as RimSave_EntryName gives it (held to the same shape again here; the ending's
	// capitals aside). Saved, with the whole path in *savedPath; else why not in *why (for the status line) and *savedPath empty, nothing written over, and no
	// half-written file left behind (NotReadBack's file names itself in *why: see READ BACK)
	RimSaveResult RimSave_New(std::string const &dataFolder, std::string const &entry, std::vector<uint8_t> const &bytes, std::string *savedPath, std::string *why);

	char const *RimSaveResultWords(RimSaveResult result);
}
