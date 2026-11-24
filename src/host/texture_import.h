// texture_import.h: a picture made a texture of the open level, from the user's file to the level's list (the RIM import, Stage 1, end to end:
// AvP-docs director/plan-rim-import.md). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// THE STEPS, each the module that does it, in this order:
//   1. the name the list will hold, Custom\<level>\<texture>.RIM, held to the game's rule      rim_save.h       RimSave_EntryName
//   2. the picture read as hostile input and decoded by Windows' own WIC (PNG, BMP)            image_import.h   ImageImport_Read
//   3. made a square of 128, 256 or 512 (Fit, Crop or Stretch)                                 image_fit.h      ImageFit_Square
//   4. its colours made a palette of at most 256                                               palette_quantize.h PaletteQuantize
//   5. written as a RIM of the stock levels' own form                                          rim_write.h      RimWrite_Stock
//   6. the level's bytes made with the new list entry, BEFORE anything is saved, so a level   bitmap_add.h     BitmapAdd_ToImage
//      whose list can't take it leaves no file behind
//   7. the RIM saved as a new file of the mapper's own, never over another (one there already
//      with these very bytes is taken as it is: rim_save.h AlreadyThere)                      rim_save.h       RimSave_New
//   8. the level swapped for the new bytes, read first by the game's own loader                level_file.h     OpenLevelFile::ReplaceLevel
// A step that refuses stops it there, with that step's words. When the game's loader refuses the new level (8), the level is as it was and the file made in 7
// is deleted again (it is this import's own, made a moment before), so nothing is left half done.
// AFTER IT, as after every ReplaceLevel: every Chunk * the editor held is gone, and the level shows the new list on its next build (Generation() is one more).
// The level isn't saved here: the editor's own Save writes it, as every other edit.
#pragma once

#include <stdint.h>

#include <string>

#include "image_fit.h"   // model::ImageFitMode

class OpenLevelFile;

namespace host
{
	enum class TextureImportResult
	{
		Imported,
		NoLevel,        // no level is open
		BadName,        // the texture's name, or the level's, fails the game's rule
		BadPicture,     // the picture was refused, or couldn't be made a texture
		ListRefused,    // the level's list can't take it (it names it already, has no room, or isn't one this adds to): nothing saved
		CantSave,       // the file couldn't be saved (its name taken, a folder not plain, the disk)
		LevelRefused,   // the game's own loader wouldn't take the new level: nothing changed, the file deleted again
	};

	struct TextureImportChoice
	{
		std::wstring picture;                               // the picture's file, as the user dropped or picked it
		std::string name;                                   // the texture's name (RimSave_SuggestedName offers one)
		uint32_t side = 128;                                // 128, 256 or 512
		model::ImageFitMode mode = model::ImageFitMode::Fit;
		bool transparentBars = false;                       // Fit's bars see-through instead of black
	};

	struct TextureImportDone
	{
		std::string entry;           // the name the list holds now: Custom\<level>\<texture>.RIM
		std::string savedPath;       // the file's whole path
		bool alreadyThere = false;   // the file was there already with these very bytes (rim_save.h AlreadyThere), so nothing was written: the list names it again
		int32_t index = -1;          // the list's new index, the one a polygon picks it by
		uint32_t pictureWidth = 0;   // the picture as it came
		uint32_t pictureHeight = 0;
		uint32_t side = 0;
		size_t coloursIn = 0;        // the picture's colours, and whether they had to be reduced to the palette
		size_t coloursOut = 0;
		bool reduced = false;
		bool transparent = false;    // a colour of the palette is see-through
		size_t partialAlpha = 0;     // pixels half see-through, each kept wholly one or the other
	};

	// level: the level the editor holds. levelFile: its file name or path (its last part, without ".rif", names its folder). dataFolder: the game's folder
	// (theGameFolder). Imported, with *done filled; else why not in *why, and the level and the folders as they were
	TextureImportResult TextureImport_Make(OpenLevelFile *level, std::string const &levelFile, std::string const &dataFolder, TextureImportChoice const &choice,
		TextureImportDone *done, std::string *why);

	// The status line's words for a done import: the name, the index, the size, the colours
	std::string TextureImport_DoneWords(TextureImportDone const &done);

	char const *TextureImportResultWords(TextureImportResult result);
}
